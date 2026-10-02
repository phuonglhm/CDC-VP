"""Host-side YOLOv8 head decode + NMS in numpy, and int8-vs-FP32 comparison metrics.

The decode mirrors ultralytics Detect._inference for the legacy YOLOv8 head:
  DFL: box logits [4*16, H, W] -> softmax over the 16 bins -> expectation -> (l, t, r, b) distances
  anchors: cell centers (x + 0.5, y + 0.5), levels p3/p4/p5 concatenated, row-major flatten
  output: xywh * stride and sigmoid(class logits)
fe_step5_quality.py checks this decode against the model's own 'final' output before using it.
"""
import numpy as np

STRIDES = (8, 16, 32)
REG_MAX = 16


def decode(head_box, head_cls, strides=STRIDES):
    """head_box: list of [1, 64, H, W], head_cls: list of [1, nc, H, W] (float) -> [4 + nc, A]."""
    xywh, cls = [], []
    for b, c, s in zip(head_box, head_cls, strides):
        _, _, h, w = b.shape
        logits = b[0].reshape(4, REG_MAX, h * w).astype(np.float64)
        logits -= logits.max(axis=1, keepdims=True)
        p = np.exp(logits)
        p /= p.sum(axis=1, keepdims=True)
        dist = (p * np.arange(REG_MAX)[None, :, None]).sum(axis=1)  # [4, A]
        gy, gx = np.meshgrid(np.arange(h) + 0.5, np.arange(w) + 0.5, indexing="ij")
        anchor = np.stack([gx.reshape(-1), gy.reshape(-1)])  # [2, A]
        x1y1 = anchor - dist[:2]
        x2y2 = anchor + dist[2:]
        xywh.append(np.concatenate([(x1y1 + x2y2) / 2, x2y2 - x1y1]) * s)
        cls.append(1.0 / (1.0 + np.exp(-c[0].reshape(c.shape[1], -1).astype(np.float64))))
    return np.concatenate([np.concatenate(xywh, axis=1), np.concatenate(cls, axis=1)])


def _iou(box, boxes):
    x1 = np.maximum(box[0], boxes[:, 0])
    y1 = np.maximum(box[1], boxes[:, 1])
    x2 = np.minimum(box[2], boxes[:, 2])
    y2 = np.minimum(box[3], boxes[:, 3])
    inter = np.clip(x2 - x1, 0, None) * np.clip(y2 - y1, 0, None)
    area = lambda b: (b[..., 2] - b[..., 0]) * (b[..., 3] - b[..., 1])  # noqa: E731
    return inter / (area(box) + area(boxes) - inter + 1e-9)


def detections(pred, conf_thres=0.25, iou_thres=0.7, max_det=300):  # ultralytics predict defaults
    """pred [4 + nc, A] -> array [N, 6]: x1, y1, x2, y2, conf, cls (per-class greedy NMS)."""
    scores = pred[4:]
    cls = scores.argmax(axis=0)
    conf = scores.max(axis=0)
    keep = conf > conf_thres
    if not keep.any():
        return np.zeros((0, 6))
    xywh = pred[:4, keep].T
    boxes = np.concatenate([xywh[:, :2] - xywh[:, 2:] / 2, xywh[:, :2] + xywh[:, 2:] / 2], axis=1)
    conf, cls = conf[keep], cls[keep]
    out = []
    for k in np.unique(cls):
        idx = np.where(cls == k)[0]
        idx = idx[np.argsort(-conf[idx], kind="stable")]
        while idx.size:
            i = idx[0]
            out.append([*boxes[i], conf[i], k])
            if idx.size == 1:
                break
            rest = idx[1:]
            idx = rest[_iou(boxes[i], boxes[rest]) <= iou_thres]
    out = np.array(out)
    return out[np.argsort(-out[:, 4], kind="stable")][:max_det]


def match(ref, got, iou_thres=0.5):
    """Greedy match of got detections to ref detections (same class, IoU >= iou_thres)."""
    used = np.zeros(len(got), dtype=bool)
    ious, dconf = [], []
    for r in ref:
        cand = np.where((~used) & (got[:, 5] == r[5]))[0] if len(got) else np.array([], dtype=int)
        if cand.size == 0:
            continue
        iou = _iou(r[:4], got[cand, :4])
        j = int(np.argmax(iou))
        if iou[j] >= iou_thres:
            used[cand[j]] = True
            ious.append(float(iou[j]))
            dconf.append(abs(float(got[cand[j], 4] - r[4])))
    return {"tp": len(ious), "n_ref": len(ref), "n_got": len(got), "ious": ious, "dconf": dconf}


def cosine(a, b):
    a = a.reshape(-1).astype(np.float64)
    b = b.reshape(-1).astype(np.float64)
    return float(a @ b / (np.linalg.norm(a) * np.linalg.norm(b) + 1e-30))


def sqnr_db(ref, approx):
    ref = ref.reshape(-1).astype(np.float64)
    err = ref - approx.reshape(-1).astype(np.float64)
    return float(10.0 * np.log10((ref @ ref) / (err @ err + 1e-30)))
