"""Step 6b preparation: conv tile execution in the order the HAS defines.

HAS mapping (§6.7, §6.8, Figure 7-2), C[M, N] with M = output positions, N = output channels:
  - one context = up to 32 output positions (SA rows) x up to 32 output channels (SA columns).
    Core mapping (driver/libsauria_cfg.h, tiles with 'y_used'): the positions of a context are y_used
    consecutive positions of ONE output row (output width -> SA rows, N_cswitch = ceil(w/Y_used)*h*ceil(k/X_used)).
    Tiles without 'y_used' (first 6a planner) fall back to row-major chunks of 32.
  - bias is not added in the OBP: the PSUM SRAM is preloaded with bias[c] for every (position, c) and the
    PSM preload initializes the PE accumulators with it before the reduction (HAS §6.8/§6.9).
  - PSM scan vector x = C[rows of the context, x] = ONE output channel over the context's positions;
    the active-row mask covers partial contexts.
  - OBP (per-channel requant): the vector's channel selects scale/shift/LUT. With the v4.5 `Obp` this is
    `vec_channel_mode` (index = vector counter % 32, counter reset when the pipeline is idle), so each
    context must start from an idle OBP and a tile programs at most 32 channels.
  - residual: skip[position, x] added after the LUT, saturated to INT8.
The arithmetic equals T2; this module fixes the data arrangement the SystemC testbench must reproduce.
"""
import numpy as np

import fe_quant as fq
import fe_ref_int8 as fr
import fe_tile_plan as tp

N = 32
INT32_MIN, INT32_MAX = -(1 << 31), (1 << 31) - 1


def conv_tile_hw(o, tl, params, job, src, skip, stats):
    c0, c1 = tl["c"]
    oy0, oy1 = tl["oy"]
    ox0, ox1 = tl["ox"]
    nch, ht, wt = c1 - c0, oy1 - oy0, ox1 - ox0
    npos = ht * wt
    if nch > N:
        raise tp.PlanError("%s: %d channels in one tile exceed %d OBP vectors" % (job, nch, N))
    crop = tp.padded_crop(src, tl["iy"], tl["ix"])
    prod = fr.conv_psum(crop, params[job + "/q_w"][c0:c1], o["stride"], [0, 0, 0, 0])  # [1, nch, ht, wt]
    prod = prod[0].reshape(nch, npos).T                                                 # [npos, nch]
    bias = params[job + "/q_b"][c0:c1].astype(np.int64)
    preload = np.broadcast_to(bias[None, :], (npos, nch))                               # PSUM SRAM initial content
    scale = params[job + "/scale"][c0:c1].astype(np.int64)
    shift = params[job + "/shift"][c0:c1].astype(np.int64)
    lut = params.get(job + "/lut")
    sk = None
    if skip is not None:
        sk = skip[0, c0:c1, oy0:oy1, ox0:ox1].reshape(nch, npos).T.astype(np.int16)
    out = np.zeros((npos, nch), dtype=np.int8)
    if "y_used" in tl:
        # core mapping (libsauria_cfg.h): a context is y_used positions of ONE output row x x_used channels
        yu, xu = tl["y_used"], tl["x_used"]
        if xu != nch or wt % yu:
            raise tp.PlanError("%s: unsupported geometry x_used=%d nch=%d y_used=%d w_t=%d" % (job, xu, nch, yu, wt))
        ctx_rows = [slice(oy * wt + cx0, oy * wt + cx0 + yu) for oy in range(ht) for cx0 in range(0, wt, yu)]
    else:
        ctx_rows = [slice(c, min(c + N, npos)) for c in range(0, npos, N)]
    for rows in ctx_rows:
        acc = preload[rows] + prod[rows]                    # PE accumulators after the reduction
        stats["acc_absmax"] = max(stats["acc_absmax"], int(np.abs(acc).max()))
        if acc.min() < INT32_MIN or acc.max() > INT32_MAX:
            raise OverflowError("%s: accumulator exceeds INT32" % job)
        stats["contexts"] += 1
        stats["partial_contexts"] += int(rows.stop - rows.start < N)
        vec_cnt = 0                                         # OBP idle before each context
        for x in range(nch):
            ch = vec_cnt % N
            v = fq.sat8(np.right_shift(acc[:, x] * scale[ch], shift[ch]))
            if lut is not None:
                v = lut[v.astype(np.int16) + 128].astype(np.int16)
            if sk is not None:
                v = fq.sat8(v.astype(np.int16) + sk[rows, x])
            out[rows, x] = v
            vec_cnt += 1
    stats["tiles"] += 1
    stats["tiles_lt32_channels"] += int(nch < N)
    return out.T.reshape(nch, ht, wt)


def run_program_hw(ir, program, params, x, stats):
    ops = {o["id"]: o for o in ir["ops"]}
    t = {ir["graph_input"]: fr.quantize_input(x)}
    for st in program["steps"]:
        o = ops[st["op_id"]]
        if st["kind"] != "conv":
            a = [t[i] for i in o["inputs"]]
            if o["op"] == "slice_ch":
                y = a[0][:, o["start"]:o["end"]]
            elif o["op"] == "concat":
                y = np.concatenate(a, axis=1)
            elif o["op"] == "add":
                y = fq.sat8(a[0].astype(np.int16) + a[1].astype(np.int16)).astype(np.int8)
            elif o["op"] == "maxpool":
                y = fr.maxpool_int8(a[0], o["kernel"], o["stride"], o["pad"])
            elif o["op"] == "upsample_nearest":
                y = a[0].repeat(o["factor"], axis=2).repeat(o["factor"], axis=3)
            else:
                raise tp.PlanError("host op %s" % o["op"])
            t[st["output"]] = np.ascontiguousarray(y)
            continue
        src = t[st["input"]]
        skip = t[st["skip"]] if st["skip"] else None
        out = np.zeros(o["out_shape"], dtype=np.int8)
        for tl in st["tiles"]:
            c0, c1 = tl["c"]
            out[0, c0:c1, tl["oy"][0]:tl["oy"][1], tl["ox"][0]:tl["ox"][1]] = conv_tile_hw(o, tl, params, st["job"], src, skip, stats)
        t[st["output"]] = out
    return t
