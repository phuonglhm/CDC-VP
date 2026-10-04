"""Shared helpers for the real YOLOv8m frontend/golden flow.

Runs on a Linux host with torch / onnxruntime / ultralytics, not on Windows.
Heavy data lives in the directory named by the FE_WORK environment variable (required), never in the source tree.
"""
import os

# Must be set before ultralytics is imported: no auto pip installs, no online checks.
os.environ.setdefault("YOLO_AUTOINSTALL", "False")
os.environ.setdefault("YOLO_OFFLINE", "True")

import numpy as np
import torch

FE_WORK = os.environ.get("FE_WORK", "")
if not FE_WORK:
    raise SystemExit("FE_WORK is not set: export FE_WORK=<work directory holding weights/, datasets/, onnx/, step*/>")
WEIGHTS = os.path.join(FE_WORK, "weights", "yolov8m.pt")
COCO8_DIR = os.path.join(FE_WORK, "datasets", "coco8", "images")
IMGSZ = 640


def load_fused_model():
    """YOLOv8m DetectionModel, float32, eval, BN fused into Conv (what the frontend quantizes)."""
    # ultralytics 8.4.x: attempt_load_one_weight no longer exists
    from ultralytics.nn.tasks import load_checkpoint

    model, _ = load_checkpoint(WEIGHTS)
    model = model.float().eval()
    model = model.fuse(verbose=False)
    for p in model.parameters():
        p.requires_grad_(False)
    return model


def list_coco8_images(split="all"):
    """coco8 images; split = 'all' (train then val), 'train' or 'val'."""
    paths = []
    for split_name in ("train", "val"):
        if split not in ("all", split_name):
            continue
        d = os.path.join(COCO8_DIR, split_name)
        paths += [os.path.join(d, f) for f in sorted(os.listdir(d)) if f.lower().endswith(".jpg")]
    return paths


def letterbox_input(path, imgsz=IMGSZ):
    """BGR file -> float32 NCHW RGB in [0,1], ultralytics LetterBox (pad 114, centered, no auto)."""
    import cv2
    from ultralytics.data.augment import LetterBox

    img = cv2.imread(path)
    if img is None:
        raise RuntimeError("cannot read image: %s" % path)
    img = LetterBox(new_shape=(imgsz, imgsz), auto=False, stride=32)(image=img)
    img = img[..., ::-1].transpose(2, 0, 1)
    return np.ascontiguousarray(img, dtype=np.float32)[None] / 255.0


class TappedYolo(torch.nn.Module):
    """Same computation as DetectionModel._predict_once + Detect._inference, but returns every
    top-level layer output (0..21), the 6 raw head convs (cv2[i], cv3[i]) and the final decode.

    The head convs are run once and fed to _inference, so the exported graph has no duplicate
    head convs.
    """

    def __init__(self, det_model):
        super().__init__()
        self.det = det_model

    def tap_names(self):
        names = ["layer%02d" % i for i in range(len(self.det.model) - 1)]
        nl = self.det.model[-1].nl
        names += ["head_box%d" % i for i in range(nl)]
        names += ["head_cls%d" % i for i in range(nl)]
        names += ["final"]
        return names

    def forward(self, x):
        layers = self.det.model
        y = []
        for layer in layers[:-1]:
            if layer.f != -1:
                x = y[layer.f] if isinstance(layer.f, int) else [x if j == -1 else y[j] for j in layer.f]
            x = layer(x)
            y.append(x)
        d = layers[-1]
        feats = [y[j] for j in d.f]
        box = [d.cv2[i](feats[i]) for i in range(d.nl)]
        cls = [d.cv3[i](feats[i]) for i in range(d.nl)]
        bs = feats[0].shape[0]
        preds = {
            "boxes": torch.cat([b.view(bs, 4 * d.reg_max, -1) for b in box], dim=-1),
            "scores": torch.cat([c.view(bs, d.nc, -1) for c in cls], dim=-1),
            "feats": feats,
        }
        final = d._inference(preds)
        return tuple(y) + tuple(box) + tuple(cls) + (final,)
