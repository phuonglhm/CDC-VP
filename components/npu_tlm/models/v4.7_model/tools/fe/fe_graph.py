"""Frontend graph reader: exported YOLOv8m ONNX -> hardware-level IR.

Scope: only nodes that are ancestors of the cut tensors (the 6 raw head convs). Detect decode
(DFL/Softmax/anchors) is outside the cone and is host float work.

Rules:
- Nodes whose inputs are all compile-time known (initializers, Constant, Shape of a static tensor)
  are folded with a small numpy evaluator. An unsupported op there is an error.
- Every remaining node must match one IR op. Anything else is an error. No silent aliasing.

IR ops (tensors are NCHW, N=1):
  conv             Conv (+ Sigmoid/Mul SiLU fused when the pattern is exact)
  slice_ch         Slice along axis 1 with constant bounds, step 1
  concat           Concat along axis 1
  add              elementwise Add of two data tensors with equal shape
  maxpool          MaxPool (kernel, stride, symmetric pad)
  upsample_nearest Resize mode=nearest, integer scale on H/W only
"""
import numpy as np
import onnx
from onnx import numpy_helper

CUT_OUTPUTS = ["head_box0", "head_box1", "head_box2", "head_cls0", "head_cls1", "head_cls2"]
GRAPH_INPUT = "images"
DETECT_LAYER = 22


class FrontendError(RuntimeError):
    pass


def module_name_from_scope(node_name, suffixes):
    """'/model.22/cv2.0/cv2.0.0/conv/Conv' -> 'model.22.cv2.0.0'; head convs called directly by
    TappedYolo have scope 'cv2.N/...' and get the 'model.22.' prefix."""
    for suffix in suffixes:
        if node_name.endswith(suffix):
            base = node_name[: -len(suffix)]
            break
    else:
        return None
    segs = [s for s in base.split("/") if s]
    full = []
    for seg in segs:
        if full and seg.startswith(full[-1] + "."):
            full[-1] = seg
        else:
            full.append(seg)
    name = ".".join(full)
    if name.startswith("cv2.") or name.startswith("cv3."):
        name = "model.%d.%s" % (DETECT_LAYER, name)
    return name


_C2F = {2: "dark2.c2f", 4: "dark3.c2f", 6: "dark4.c2f", 8: "dark5.c2f",
        12: "neck.p4.c2f", 15: "neck.p3.c2f", 18: "neck.d4.c2f", 21: "neck.d5.c2f"}
_SINGLE = {0: "stem", 1: "dark2.conv", 3: "dark3.conv", 5: "dark4.conv", 7: "dark5.conv",
           16: "neck.d4.conv", 19: "neck.d5.conv"}


def job_name(module_name):
    """ultralytics module name -> sauria_model gen_yolo_jobs.py layer name (None if unknown)."""
    parts = module_name.split(".")
    if len(parts) < 2 or parts[0] != "model":
        return None
    idx, rest = int(parts[1]), parts[2:]
    if idx in _SINGLE and not rest:
        return _SINGLE[idx]
    if idx in _C2F:
        if rest in (["cv1"], ["cv2"]):
            return "%s.%s" % (_C2F[idx], rest[0])
        if len(rest) == 3 and rest[0] == "m" and rest[2] in ("cv1", "cv2"):
            return "%s.b%s.%s" % (_C2F[idx], rest[1], "1" if rest[2] == "cv1" else "2")
    if idx == 9 and rest in (["cv1"], ["cv2"]):
        return "sppf.%s" % rest[0]
    if idx == DETECT_LAYER and len(rest) == 3 and rest[0] in ("cv2", "cv3"):
        branch = "box" if rest[0] == "cv2" else "cls"
        stage = {"0": "1", "1": "2", "2": "_out"}[rest[2]]
        return "det.p%d.%s%s" % (3 + int(rest[1]), branch, stage)
    return None


# --------------------------------------------------------------------------- constant folding
def _attr(node, name, default=None):
    for a in node.attribute:
        if a.name == name:
            return onnx.helper.get_attribute_value(a)
    return default


def _slice_bounds(dim, start, end, step):
    if step != 1:
        raise FrontendError("Slice step %d not supported" % step)
    if start < 0:
        start += dim
    if end < 0:
        end += dim
    return max(0, min(start, dim)), max(0, min(end, dim))


def _fold(node, vals):
    op = node.op_type
    x = [vals[i] if i else None for i in node.input]
    if op == "Gather":
        return np.take(x[0], x[1], axis=_attr(node, "axis", 0))
    if op in ("Add", "Sub", "Mul"):
        return {"Add": np.add, "Sub": np.subtract, "Mul": np.multiply}[op](x[0], x[1])
    if op == "Div":
        if np.issubdtype(x[0].dtype, np.integer):
            return np.trunc(x[0] / x[1]).astype(x[0].dtype)  # ONNX integer Div truncates
        return x[0] / x[1]
    if op == "Unsqueeze":
        out = x[0]
        for ax in sorted(int(a) for a in x[1].reshape(-1)):
            out = np.expand_dims(out, ax)
        return out
    if op == "Concat":
        return np.concatenate(x, axis=_attr(node, "axis"))
    if op == "Cast":
        return x[0].astype(onnx.helper.tensor_dtype_to_np_dtype(_attr(node, "to")))
    if op == "Reshape":
        shape = [x[0].shape[i] if s == 0 else int(s) for i, s in enumerate(x[1].reshape(-1))]
        return x[0].reshape(shape)
    raise FrontendError("constant folding: op %s (%s) not supported" % (op, node.name))


def _out_shape(rec):
    """Output NCHW shape from IR semantics (checked against ONNX shape inference when available)."""
    op, ins = rec["op"], rec["in_shapes"]
    n, c, h, w = ins[0]
    if op in ("conv", "maxpool"):
        kh, kw = (rec["kh"], rec["kw"]) if op == "conv" else rec["kernel"]
        dh, dw = rec["dilation"] if op == "conv" else (1, 1)
        sh, sw = rec["stride"]
        pt, pl, pb, pr = rec["pad"]
        oh = (h + pt + pb - dh * (kh - 1) - 1) // sh + 1
        ow = (w + pl + pr - dw * (kw - 1) - 1) // sw + 1
        if op == "conv" and c != rec["cin"]:
            raise FrontendError("conv %s: input C %d != weight Cin %d" % (rec.get("module"), c, rec["cin"]))
        return [n, rec["cout"] if op == "conv" else c, oh, ow]
    if op == "slice_ch":
        return [n, rec["end"] - rec["start"], h, w]
    if op == "concat":
        if any(s[0] != n or s[2:] != [h, w] for s in ins):
            raise FrontendError("concat input shapes differ: %s" % ins)
        return [n, sum(s[1] for s in ins), h, w]
    if op == "add":
        return list(ins[0])
    if op == "upsample_nearest":
        return [n, c, h * rec["factor"], w * rec["factor"]]
    raise FrontendError("unknown IR op %s" % op)


# --------------------------------------------------------------------------- IR build
def build_ir(model):
    model = onnx.shape_inference.infer_shapes(model)
    g = model.graph
    shapes = {}
    for vi in list(g.input) + list(g.value_info) + list(g.output):
        dims = [d.dim_value if d.HasField("dim_value") else None for d in vi.type.tensor_type.shape.dim]
        if dims and all(d is not None for d in dims):
            shapes[vi.name] = dims

    producer = {}
    consumers = {}
    for n in g.node:
        for o in n.output:
            producer[o] = n
        for i in n.input:
            if i:
                consumers.setdefault(i, []).append(n)

    missing_cut = [c for c in CUT_OUTPUTS if c not in producer]
    if missing_cut:
        raise FrontendError("cut tensors not found in graph: %s" % missing_cut)

    # cone of the cut tensors
    cone = set()
    stack = list(CUT_OUTPUTS)
    seen = set()
    while stack:
        t = stack.pop()
        if t in seen:
            continue
        seen.add(t)
        n = producer.get(t)
        if n is None:
            continue
        cone.add(id(n))
        stack.extend(i for i in n.input if i)
    nodes = [n for n in g.node if id(n) in cone]

    known = {init.name: numpy_helper.to_array(init) for init in g.initializer}
    weights = dict(known)
    ops, unrecognized, folded = [], [], 0
    consumed = set()
    data_tensors = {GRAPH_INPUT}

    def data_inputs(n):
        return [i for i in n.input if i and i not in known]

    for n in nodes:
        if id(n) in consumed:
            continue
        op = n.op_type
        if op == "Constant":
            known[n.output[0]] = numpy_helper.to_array(_attr(n, "value"))
            folded += 1
            continue
        if op == "Shape":
            if n.input[0] not in shapes:
                raise FrontendError("Shape of non-static tensor %s" % n.input[0])
            known[n.output[0]] = np.array(shapes[n.input[0]], dtype=np.int64)
            folded += 1
            continue
        if all((not i) or i in known for i in n.input):
            known[n.output[0]] = _fold(n, known)
            folded += 1
            continue

        din = data_inputs(n)
        bad = [i for i in din if i not in data_tensors]
        if bad:
            unrecognized.append((op, n.name, "inputs not produced by recognized ops: %s" % bad))
            continue
        out = n.output[0]
        rec = None

        if op == "Conv":
            w = known.get(n.input[1])
            b = known.get(n.input[2]) if len(n.input) > 2 and n.input[2] else None
            if w is None or len(din) != 1:
                unrecognized.append((op, n.name, "Conv weight not constant"))
                continue
            cout, cin_g, kh, kw = w.shape
            group = _attr(n, "group", 1)
            pads = list(_attr(n, "pads", [0, 0, 0, 0]))
            rec = {"op": "conv", "inputs": [din[0]], "weight": n.input[1],
                   "bias": n.input[2] if b is not None else None,
                   "cin": cin_g * group, "cout": cout, "kh": kh, "kw": kw,
                   "stride": list(_attr(n, "strides", [1, 1])), "pad": pads,
                   "dilation": list(_attr(n, "dilations", [1, 1])), "group": group, "act": "none",
                   "module": module_name_from_scope(n.name, ["/conv/Conv", "/Conv"])}
            # SiLU: Conv -> {Sigmoid, Mul}, Sigmoid -> Mul, Mul(conv, sigmoid)
            cons = consumers.get(out, [])
            sig = [c for c in cons if c.op_type == "Sigmoid"]
            mul = [c for c in cons if c.op_type == "Mul"]
            if len(cons) == 2 and len(sig) == 1 and len(mul) == 1:
                s, m = sig[0], mul[0]
                if (consumers.get(s.output[0], []) == [m] and sorted(m.input) == sorted([out, s.output[0]])
                        and id(s) in cone and id(m) in cone):
                    consumed.update([id(s), id(m)])
                    rec["act"] = "silu"
                    out = m.output[0]
        elif op == "Slice":
            starts, ends = known.get(n.input[1]), known.get(n.input[2])
            axes = known.get(n.input[3]) if len(n.input) > 3 and n.input[3] else None
            steps = known.get(n.input[4]) if len(n.input) > 4 and n.input[4] else None
            shp = shapes.get(din[0])
            if starts is None or ends is None or shp is None or len(din) != 1:
                unrecognized.append((op, n.name, "Slice bounds not constant"))
                continue
            axes = [int(a) for a in (axes.reshape(-1) if axes is not None else range(len(starts)))]
            steps = [int(s) for s in (steps.reshape(-1) if steps is not None else [1] * len(axes))]
            if axes != [1] or len(shp) != 4:
                unrecognized.append((op, n.name, "Slice axes %s not channel-only" % axes))
                continue
            s0, e0 = _slice_bounds(shp[1], int(starts.reshape(-1)[0]), int(ends.reshape(-1)[0]), steps[0])
            rec = {"op": "slice_ch", "inputs": [din[0]], "start": s0, "end": e0}
        elif op == "Concat":
            if _attr(n, "axis") != 1 or len(din) != len([i for i in n.input if i]):
                unrecognized.append((op, n.name, "Concat not channel-only on data"))
                continue
            rec = {"op": "concat", "inputs": din}
        elif op == "Add":
            if len(din) != 2 or shapes.get(din[0]) != shapes.get(din[1]):
                unrecognized.append((op, n.name, "Add not two equal-shape data tensors"))
                continue
            rec = {"op": "add", "inputs": din}
        elif op == "MaxPool":
            pads = list(_attr(n, "pads", [0, 0, 0, 0]))
            if (_attr(n, "ceil_mode", 0) != 0 or list(_attr(n, "dilations", [1, 1])) != [1, 1]
                    or pads[:2] != pads[2:] or len(din) != 1):
                unrecognized.append((op, n.name, "MaxPool attributes not supported"))
                continue
            rec = {"op": "maxpool", "inputs": din, "kernel": list(_attr(n, "kernel_shape")),
                   "stride": list(_attr(n, "strides", [1, 1])), "pad": pads}
        elif op == "Resize":
            scales = known.get(n.input[2]) if len(n.input) > 2 and n.input[2] else None
            mode = _attr(n, "mode", b"nearest")
            ctm = _attr(n, "coordinate_transformation_mode", b"half_pixel")
            nmode = _attr(n, "nearest_mode", b"round_prefer_floor")
            as_str = [v.decode() if isinstance(v, bytes) else v for v in (mode, ctm, nmode)]
            ok = (scales is not None and len(din) == 1 and as_str[0] == "nearest"
                  and list(scales[:2]) == [1.0, 1.0] and scales[2] == scales[3]
                  and float(scales[2]).is_integer() and as_str[1] == "asymmetric" and as_str[2] == "floor")
            if not ok:
                unrecognized.append((op, n.name, "Resize not integer nearest (mode/ctm/nearest=%s, scales=%s)"
                                     % (as_str, None if scales is None else scales.tolist())))
                continue
            rec = {"op": "upsample_nearest", "inputs": din, "factor": int(scales[2])}
        else:
            unrecognized.append((op, n.name, "no IR pattern"))
            continue

        rec["onnx_node"] = n.name
        rec["output"] = out
        rec["in_shapes"] = [shapes.get(i) for i in rec["inputs"]]
        if any(s is None for s in rec["in_shapes"]):
            raise FrontendError("op %s: input shape unknown %s" % (n.name, rec["in_shapes"]))
        own = _out_shape(rec)
        onnx_shape = shapes.get(out)
        if onnx_shape is not None and onnx_shape != own:
            raise FrontendError("op %s: computed shape %s != ONNX inferred %s" % (n.name, own, onnx_shape))
        shapes[out] = own
        rec["out_shape"] = own
        rec["id"] = len(ops)
        if rec["op"] == "conv":
            rec["job"] = job_name(rec["module"]) if rec["module"] else None
        ops.append(rec)
        data_tensors.add(out)

    not_reached = [c for c in CUT_OUTPUTS if c not in data_tensors]
    ir = {"graph_input": GRAPH_INPUT, "input_shape": shapes[GRAPH_INPUT], "cut_outputs": CUT_OUTPUTS,
          "ops": ops}
    stats = {"graph_nodes": len(g.node), "cone_nodes": len(nodes), "folded": folded,
             "fused_silu_nodes": len(consumed), "ir_ops": len(ops), "unrecognized": unrecognized,
             "cut_not_reached": not_reached}
    return ir, stats, weights


# --------------------------------------------------------------------------- float execution
PREACT_SUFFIX = "@pre"


def run_ir_float(ir, weights, x, keep_preact=False):
    """Execute the IR with torch.nn.functional (independent of the ONNX graph structure).

    keep_preact: also return each SiLU conv's value before the activation as '<output>@pre'.
    """
    import torch
    import torch.nn.functional as F

    t = {ir["graph_input"]: torch.from_numpy(x)}
    for o in ir["ops"]:
        op = o["op"]
        a = [t[i] for i in o["inputs"]]
        if op == "conv":
            p = o["pad"]
            if p[:2] != p[2:]:
                raise FrontendError("asymmetric conv pad %s" % p)
            w = torch.from_numpy(weights[o["weight"]])
            b = torch.from_numpy(weights[o["bias"]]) if o["bias"] else None
            y = F.conv2d(a[0], w, b, stride=tuple(o["stride"]), padding=(p[0], p[1]),
                         dilation=tuple(o["dilation"]), groups=o["group"])
            if o["act"] == "silu":
                if keep_preact:
                    t[o["output"] + PREACT_SUFFIX] = y
                y = y * torch.sigmoid(y)
        elif op == "slice_ch":
            y = a[0][:, o["start"]:o["end"]]
        elif op == "concat":
            y = torch.cat(a, dim=1)
        elif op == "add":
            y = a[0] + a[1]
        elif op == "maxpool":
            p = o["pad"]
            y = F.max_pool2d(a[0], tuple(o["kernel"]), tuple(o["stride"]), padding=(p[0], p[1]))
        elif op == "upsample_nearest":
            f = o["factor"]
            y = a[0].repeat_interleave(f, dim=2).repeat_interleave(f, dim=3)
        else:
            raise FrontendError("unknown IR op %s" % op)
        if list(y.shape) != o["out_shape"]:
            raise FrontendError("op %d %s: shape %s != %s" % (o["id"], op, list(y.shape), o["out_shape"]))
        t[o["output"]] = y
    return {k: v.numpy() for k, v in t.items()}
