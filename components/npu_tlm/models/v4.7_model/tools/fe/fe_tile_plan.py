"""Step 6a: tile planning for the SAURIA RTL core layout (raw [Cin, H, W]).

Per conv, output tiles (H_t, W_t, Cout_t, Cin_t = Cin) are chosen so one tile fits one lane's buffers:
  A  input window (host-padded)  A_H * A_W * Cin            <= BANK_A_BYTES   (int8)
  B  weights                     kh * kw * Cin * Cout_t     <= BANK_B_BYTES   (int8)
  C  psums                       H_t * W_t * Cout_t         <= BANK_C_PSUMS   (int32 words)
  OBP lanes                      Cout_t                     <= OBP_LANES      (bias/scale/shift/LUT RAM)
  skip (residual, if fused)      H_t * W_t * Cout_t         <= BANK_SKIP_BYTES (int8, scratch bank 5 -- S9-FE)
Cin is never split (Cin_t = Cin): every YOLOv8m conv fits without it, and it keeps the K-folding/preload
path out of the first integration.

A shortcut Add whose one input is a SiLU conv output used only by that Add is fused into the conv as
OBP stage 4: q_out = sat8(lut[q_pre + 128] + skip).
"""
import math
import os

import numpy as np

import fe_quant as fq
import fe_ref_int8 as fr

BANK_A_BYTES = 79 * 1024
BANK_B_BYTES = 81 * 1024
BANK_C_PSUMS = (96 * 1024) // 4
OBP_LANES = 32
# The skip (residual) tile lives in the 24 KB scratch (sram/sram_top.h bank 5, see tb_fe_core_net.cpp
# SKIP_SCRATCH_*), not in the second act bank (BANK_A_BYTES). This is the hardware limit.
BANK_SKIP_BYTES = 24 * 1024


class PlanError(RuntimeError):
    pass


# det.p3.box_out / det.p4.box_out deadlock or time out on the RTL-ref core with h_t 24 (a 1x1 layer that keeps
# Bank C 100 % busy triggers it); h_t 4 runs both cleanly (120/120 and 40/40 tiles, 0 deadlocks). The minimum
# working h_t was not searched. Explicit list: every other conv keeps the planner's choice.
_DEFAULT_HT_OVERRIDE = {"det.p3.box_out": 4, "det.p4.box_out": 4}


def _parse_ht_override():
    """FE_TILE_HT_OVERRIDE="job1:ht1,job2:ht2" extends / overrides _DEFAULT_HT_OVERRIDE for experiments; with the
    variable unset _DEFAULT_HT_OVERRIDE still applies (unlike WIDE_WEIGHT_JOBS, this is a confirmed fix, not an
    opt-in)."""
    out = dict(_DEFAULT_HT_OVERRIDE)
    raw = os.environ.get("FE_TILE_HT_OVERRIDE", "")
    for part in raw.split(","):
        part = part.strip()
        if not part:
            continue
        name, val = part.split(":")
        out[name] = int(val)
    return out


HT_OVERRIDE = _parse_ht_override()


def fuse_residuals(ir):
    """conv op id -> {'skip': tensor, 'add_id': id, 'output': add output}."""
    consumers = {}
    for o in ir["ops"]:
        for i in o["inputs"]:
            consumers.setdefault(i, []).append(o)
    by_output = {o["output"]: o for o in ir["ops"]}
    cut = set(ir["cut_outputs"])
    fused = {}
    for o in ir["ops"]:
        if o["op"] != "add":
            continue
        for k, t in enumerate(o["inputs"]):
            p = by_output.get(t)
            if (p is not None and p["op"] == "conv" and p["act"] == "silu" and t not in cut
                    and [c["id"] for c in consumers.get(t, [])] == [o["id"]] and p["id"] not in fused):
                fused[p["id"]] = {"skip": o["inputs"][1 - k], "add_id": o["id"], "output": o["output"]}
                break
    return fused


def _best_spatial(o, cout_t, has_skip, bank_a, bank_c):
    _, cin, _, _ = o["in_shapes"][0]
    _, _, ho, wo = o["out_shape"]
    kh, kw = o["kh"], o["kw"]
    sh, sw = o["stride"]
    best = None
    for ht in range(1, ho + 1):
        a_h = (ht - 1) * sh + kh
        lim_a = bank_a // (a_h * cin)
        if lim_a < kw:
            break
        wt = min(wo, (lim_a - kw) // sw + 1, bank_c // (ht * cout_t))
        if has_skip:
            wt = min(wt, bank_a // (ht * cout_t))
        if wt < 1:
            break
        key = (ht * wt, -abs(ht - wt))
        if best is None or key > best[0]:
            best = (key, ht, wt)
    if best is None:
        return None
    return best[1], best[2]


SA_DIM = 32


def active_divisor(tile_dim, hw_dim=SA_DIM):
    """Largest active PE count that divides both the tile dimension and the SA dimension
    (sauria_model tools/gen_yolo_jobs.py largest_active_divisor)."""
    for a in range(min(tile_dim, hw_dim), 0, -1):
        if tile_dim % a == 0 and hw_dim % a == 0:
            return a
    return 1


def _chunk_contexts(total, step):
    """Contexts needed along one dimension split into chunks of `step`: sum of ceil(chunk / active_divisor(chunk))."""
    return sum(math.ceil(min(step, total - s) / active_divisor(min(step, total - s))) for s in range(0, total, step))


def _splits(total, step):
    out, s = [], 0
    while s < total:
        out.append(min(step, total - s))
        s += step
    return out


def _predicted_total(model, job, cout, ho, wo, cout_t, ht, wt):
    """S9-FE: total predicted cycles (busy+ld+st) to cover the WHOLE conv with this (cout_t, ht, wt)
    geometry -- summed over every chunk (remainder chunks included), matching
    tools/eval_sw/tile_sweep.py's eval_seq()/seq_total exactly (a single representative tile's cost is
    NOT comparable across candidates with different tile counts). Returns None if any chunk can't be
    predicted (flag == 'N')."""
    total = 0.0
    for nch in _splits(cout, cout_t):
        for hh in _splits(ho, ht):
            for ww in _splits(wo, wt):
                t = model.tile(job, nch, hh, ww)
                if t["busy"] is None:
                    return None
                total += t["busy"] + t["ld"] + t["st"]
    return total


def plan_conv_core(o, has_skip, bank_a=BANK_A_BYTES, bank_b=BANK_B_BYTES, bank_c=BANK_C_PSUMS, lanes=OBP_LANES,
                   psm_span_rows=None, max_wt=1 << 16, model=None, bank_skip=BANK_SKIP_BYTES):
    """Tiles aligned with the SAURIA core mapping (libsauria_cfg.h): output channels -> SA columns (X_used),
    output width -> SA rows (Y_used); one context = Y_used positions of ONE output row x X_used channels.
    W_t and Cout_t are chosen among divisors of 32 (32, 16, ...) so that X_used = Cout_t and Y_used = W_t for
    every full tile and remainder tiles still get a power-of-two active geometry; H_t is free.

    psm_span_rows: optional W_t * Cout_t limit (sauria_model gen_yolo_jobs.py PSM_span proxy). Off by default:
    the PSM SRAM-C address depends on W_t*H_t and Cout_t only, and tiles with W_t*Cout_t = 1024/2048 at
    W_t*H_t*Cout_t = 24576 pass on sauria_model; the capacity rule below is the real one.

    model: optional tools/eval_sw/tile_model.ShapeModel instance (S9-FE). When given, candidates are ranked
    by PREDICTED CYCLES (busy+ld+st, tile_model's calibrated cost) instead of minimum tile count, among
    candidates the model can predict (flag != 'N'). Candidates the model can't predict still fall back to
    the tile-count objective so the search never comes up empty. model=None keeps the old behavior exactly."""
    _, cin, _, _ = o["in_shapes"][0]
    _, cout, ho, wo = o["out_shape"]
    kh, kw = o["kh"], o["kw"]
    sh, sw = o["stride"]
    pows = [p for p in (32, 16, 8, 4, 2, 1)]
    # W_t > 32 only as multiples of 32: Y_used stays 32 and one output row spans W_t/32 full contexts
    # (W_t = 64 was verified on sauria_model); set max_wt=32 for the previous planner.
    wide = [SA_DIM * m for m in range(math.ceil(wo / SA_DIM), 1, -1) if SA_DIM * m <= max_wt]
    best = None        # tile-count objective (old default, always tracked as fallback)
    best_cost = None   # S9-FE: predicted-cycles objective (busy+ld+st via tile_model), when model given
    for cout_t in pows:
        if cout_t > lanes or kh * kw * cin * cout_t > bank_b:
            continue
        for wt in wide + pows:
            if (wt > max(wo, 1) and wt != 1) or (psm_span_rows is not None and wt * cout_t > psm_span_rows):
                continue
            a_w = (wt - 1) * sw + kw
            ht_a = ((bank_a // (a_w * cin)) - kh) // sh + 1 if bank_a // (a_w * cin) >= kh else 0
            ht_c = bank_c // (wt * cout_t)
            # skip -> 24 KB scratch (bank_skip), no longer bounded by the second act bank (bank_a).
            ht = min(ho, ht_a, ht_c, bank_skip // (wt * cout_t) if has_skip else ho)
            if ht < 1:
                continue
            n = math.ceil(cout / cout_t) * math.ceil(wo / wt) * math.ceil(ho / ht)
            # contexts (same count as n_contexts below): every output row x column chunks x channel chunks
            n_ctx = ho * _chunk_contexts(wo, wt) * _chunk_contexts(cout, cout_t)
            key = (-n, -n_ctx, wt * cout_t, ht)
            if best is None or key > best[0]:
                best = (key, cout_t, ht, wt, n)
            if model is not None:
                # S9-FE: `ht` above is the MAX feasible row count (minimizes tile count, matches the old
                # objective) -- but the cycle-optimal H_t is not always the max one (tile_sweep.py's own
                # validated search varies H_t too, tools/eval_sw/tile_sweep.py candidates()/ht_for()).
                # Mirror that candidate set here so this reproduces tile_sweep's measured numbers instead
                # of silently under-searching. Cost = TOTAL over every chunk this geometry needs to cover
                # the whole conv (_predicted_total), never a single representative tile's cost -- candidates
                # have different tile counts, so per-tile cost alone isn't comparable.
                m0 = math.ceil(ho / ht)
                for ht2 in sorted({math.ceil(ho / m) for m in (m0, m0 + 1, m0 + 2, 2 * m0, 4 * m0, 8 * m0)
                                    if m <= ho and math.ceil(ho / m) <= ht} | {ht}, reverse=True):
                    total = _predicted_total(model, o["job"], cout, ho, wo, cout_t, ht2, wt)
                    if total is None:
                        continue
                    if best_cost is None or total < best_cost[0]:
                        n2 = math.ceil(cout / cout_t) * math.ceil(wo / wt) * math.ceil(ho / ht2)
                        best_cost = (total, cout_t, ht2, wt, n2)
    if best_cost is not None:
        _, cout_t, ht, wt, n = best_cost
    elif best is not None:
        _, cout_t, ht, wt, n = best
    else:
        raise PlanError("%s: no core-aligned tile fits (Cin=%d k=%dx%d)" % (o["job"], cin, kh, kw))
    # Per-job h_t override (HT_OVERRIDE, see _parse_ht_override): only ever shrinks h_t below the planner's
    # choice; fewer consecutive contexts per tile keep the 1x1 box_out layers from deadlocking the core.
    if o["job"] in HT_OVERRIDE:
        ht = min(ht, HT_OVERRIDE[o["job"]])
        n = math.ceil(cout / cout_t) * math.ceil(wo / wt) * math.ceil(ho / ht)
    pt, pl = o["pad"][0], o["pad"][1]
    tiles = []
    for c0 in range(0, cout, cout_t):
        c1 = min(c0 + cout_t, cout)
        for oy0 in range(0, ho, ht):
            oy1 = min(oy0 + ht, ho)
            for ox0 in range(0, wo, wt):
                ox1 = min(ox0 + wt, wo)
                tiles.append({"c": [c0, c1], "oy": [oy0, oy1], "ox": [ox0, ox1],
                              "iy": [oy0 * sh - pt, (oy1 - 1) * sh - pt + kh],
                              "ix": [ox0 * sw - pl, (ox1 - 1) * sw - pl + kw],
                              "x_used": active_divisor(c1 - c0), "y_used": active_divisor(ox1 - ox0)})
    usage = {"A": max((t["iy"][1] - t["iy"][0]) * (t["ix"][1] - t["ix"][0]) * cin for t in tiles),
             "B": kh * kw * cin * cout_t,
             "C": max((t["oy"][1] - t["oy"][0]) * (t["ox"][1] - t["ox"][0]) * (t["c"][1] - t["c"][0]) for t in tiles)}
    bad_geo = [t for t in tiles if t["x_used"] != t["c"][1] - t["c"][0] and (t["c"][1] - t["c"][0]) % t["x_used"]]
    if (usage["A"] > bank_a or usage["B"] > bank_b or usage["C"] > bank_c or cout_t > lanes
            or (has_skip and usage["C"] > bank_skip) or bad_geo):
        raise PlanError("%s: tile exceeds limits %s" % (o["job"], usage))
    n_ctx = sum((t["oy"][1] - t["oy"][0]) * math.ceil((t["ox"][1] - t["ox"][0]) / t["y_used"])
                * math.ceil((t["c"][1] - t["c"][0]) / t["x_used"]) for t in tiles)
    # Mark tiles that use both weight banks (only when bank_b > BANK_B_BYTES is allowed for the layers listed in
    # WIDE_WEIGHT_JOBS, see build_program). tb_fe_core_net.cpp reads the flag to load two halves and switch
    # i_select between them.
    return {"cout_t": cout_t, "h_t": ht, "w_t": wt, "n_tiles": n, "usage": usage, "tiles": tiles,
            "n_contexts": n_ctx, "mapping": "core", "wide_weight": usage["B"] > BANK_B_BYTES}


def plan_conv(o, has_skip, bank_a=BANK_A_BYTES, bank_b=BANK_B_BYTES, bank_c=BANK_C_PSUMS, lanes=OBP_LANES):
    _, cin, _, _ = o["in_shapes"][0]
    _, cout, ho, wo = o["out_shape"]
    kh, kw = o["kh"], o["kw"]
    best = None
    for cout_t in range(1, min(lanes, cout, bank_b // (kh * kw * cin)) + 1):
        sp = _best_spatial(o, cout_t, has_skip, bank_a, bank_c)
        if sp is None:
            continue
        ht, wt = sp
        n = math.ceil(cout / cout_t) * math.ceil(ho / ht) * math.ceil(wo / wt)
        key = (-n, cout_t)
        if best is None or key > best[0]:
            best = (key, cout_t, ht, wt, n)
    if best is None:
        raise PlanError("%s: no tile fits (Cin=%d k=%dx%d) without splitting Cin" % (o["job"], cin, kh, kw))
    _, cout_t, ht, wt, n = best
    pt, pl = o["pad"][0], o["pad"][1]
    sh, sw = o["stride"]
    tiles = []
    for c0 in range(0, cout, cout_t):
        c1 = min(c0 + cout_t, cout)
        for oy0 in range(0, ho, ht):
            oy1 = min(oy0 + ht, ho)
            for ox0 in range(0, wo, wt):
                ox1 = min(ox0 + wt, wo)
                tiles.append({"c": [c0, c1], "oy": [oy0, oy1], "ox": [ox0, ox1],
                              "iy": [oy0 * sh - pt, (oy1 - 1) * sh - pt + kh],
                              "ix": [ox0 * sw - pl, (ox1 - 1) * sw - pl + kw]})
    usage = {"A": max((t["iy"][1] - t["iy"][0]) * (t["ix"][1] - t["ix"][0]) * cin for t in tiles),
             "B": kh * kw * cin * cout_t,
             "C": max((t["oy"][1] - t["oy"][0]) * (t["ox"][1] - t["ox"][0]) * (t["c"][1] - t["c"][0]) for t in tiles)}
    if (usage["A"] > bank_a or usage["B"] > bank_b or usage["C"] > bank_c or cout_t > lanes
            or (has_skip and usage["C"] > bank_a)):
        raise PlanError("%s: tile exceeds bank limits %s" % (o["job"], usage))
    return {"cout_t": cout_t, "h_t": ht, "w_t": wt, "n_tiles": n, "usage": usage, "tiles": tiles}


# Layers listed here may exceed BANK_B_BYTES (up to 2x, using both weight banks). Disabled (empty): the
# half-split + i_select switch in tb_fe_core_net.cpp is not byte-exact on dark5.conv (root cause open).
# Empty = every layer reads one weight bank; the list and the C++ mechanism are kept for re-enabling.
WIDE_WEIGHT_JOBS = frozenset()


def build_program(ir, mapping="core", model=None):
    """mapping='core': tiles aligned with the SAURIA core geometry (plan_conv_core, default);
    mapping='area': the first 6a planner (minimum tiles, any W_t).

    model: optional tools/eval_sw/tile_model.ShapeModel (S9-FE) forwarded to plan_conv_core to rank tile
    candidates by predicted cycles instead of tile count. Ignored by mapping='area' (plan_conv doesn't
    take it -- unchanged on purpose, area mapping is the older/unused planner)."""
    planner = plan_conv_core if mapping == "core" else plan_conv
    fused = fuse_residuals(ir)
    fused_adds = {f["add_id"] for f in fused.values()}
    steps = []
    for o in ir["ops"]:
        if o["id"] in fused_adds:
            continue
        if o["op"] == "conv":
            f = fused.get(o["id"])
            if mapping == "core":
                bank_b = 2 * BANK_B_BYTES if o["job"] in WIDE_WEIGHT_JOBS else BANK_B_BYTES
                p = planner(o, has_skip=f is not None, model=model, bank_b=bank_b)
            else:
                p = planner(o, has_skip=f is not None)
            steps.append({"kind": "conv", "op_id": o["id"], "job": o["job"], "input": o["inputs"][0],
                          "output": f["output"] if f else o["output"], "skip": f["skip"] if f else None,
                          "conv_output_elided": o["output"] if f else None, **p})
        else:
            steps.append({"kind": "host", "op_id": o["id"], "op": o["op"], "inputs": o["inputs"], "output": o["output"]})
    return {"steps": steps, "fused_residuals": len(fused), "mapping": mapping,
            "limits": {"BANK_A_BYTES": BANK_A_BYTES, "BANK_B_BYTES": BANK_B_BYTES,
                       "BANK_C_PSUMS": BANK_C_PSUMS, "OBP_LANES": OBP_LANES}}


# --------------------------------------------------------------------------- numpy execution of the program
def padded_crop(x, iy, ix):
    """x int8 [1,C,H,W]; zero outside the tensor (symmetric int8, zero point 0)."""
    _, c, h, w = x.shape
    out = np.zeros((1, c, iy[1] - iy[0], ix[1] - ix[0]), dtype=np.int8)
    sy0, sy1 = max(iy[0], 0), min(iy[1], h)
    sx0, sx1 = max(ix[0], 0), min(ix[1], w)
    if sy1 > sy0 and sx1 > sx0:
        out[:, :, sy0 - iy[0]:sy1 - iy[0], sx0 - ix[0]:sx1 - ix[0]] = x[:, :, sy0:sy1, sx0:sx1]
    return out


def run_program(ir, program, params, x):
    ops = {o["id"]: o for o in ir["ops"]}
    t = {ir["graph_input"]: fr.quantize_input(x)}
    for st in program["steps"]:
        o = ops[st["op_id"]]
        if st["kind"] == "conv":
            j = st["job"]
            src = t[st["input"]]
            skip = t[st["skip"]] if st["skip"] else None
            out = np.zeros(o["out_shape"], dtype=np.int8)
            for tl in st["tiles"]:
                c0, c1 = tl["c"]
                oy0, oy1 = tl["oy"]
                ox0, ox1 = tl["ox"]
                crop = padded_crop(src, tl["iy"], tl["ix"])
                psum = fr.conv_psum(crop, params[j + "/q_w"][c0:c1], o["stride"], [0, 0, 0, 0])
                q = fr.requant(psum, params[j + "/q_b"][c0:c1], params[j + "/scale"][c0:c1], params[j + "/shift"][c0:c1])
                if o["act"] == "silu":
                    q = params[j + "/lut"][q.astype(np.int16) + 128]
                if skip is not None:
                    q = fq.sat8(q.astype(np.int16) + skip[:, c0:c1, oy0:oy1, ox0:ox1].astype(np.int16)).astype(np.int8)
                if list(q.shape) != [1, c1 - c0, oy1 - oy0, ox1 - ox0]:
                    raise PlanError("%s tile %s: shape %s" % (j, tl, list(q.shape)))
                out[:, c0:c1, oy0:oy1, ox0:ox1] = q
            t[st["output"]] = out
        else:
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
                raise PlanError("host op %s" % o["op"])
            t[st["output"]] = np.ascontiguousarray(y)
    return t
