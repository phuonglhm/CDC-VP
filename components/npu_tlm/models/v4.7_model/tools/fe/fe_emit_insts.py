"""YOLOv8m HAS program -> v4.5-style MMIO write stream (1 instruction = 1 layer)
plus a new DRAM layout. Reference compiler backend (docs/SW_INTEGRATION_GUIDE.md).

Input : FE_WORK/has/program_has_has.json, FE_WORK/step2/graph_ir.json, FE_WORK/step3/<config>/params.npz,
        FE_WORK/has/net_has/{manifest.json, dram_golden.bin} (tensor values for the self-check only)
Output: FE_WORK/has/insts_has/{mmio.txt, dram_init.bin, dram_golden.bin, manifest.json}   (refuses any other directory)

mmio.txt: "W <addr hex> <value hex>" in the order a CPU writes them, "H <host op> ..." for host steps left, "# ..." comments.
  Registers and meaning: docs/SW_INTEGRATION_GUIDE.md, section 3; extension index order = enum has::mmio::Ext (has/has_mmio_compat.h), EXT_BASE 0x4000046C.
  Extension registers are one-shot (cleared after each push) so every instruction writes its own; old registers are sticky.
  GEMM_FUSED: IN/W/OUT/BIAS_ADDR, KH, KW, STRIDE, PAD, ACT_TYPE (2 SiLU / 0), HAS_SKIP 0, ext IN_C/H/W, OUT_C/H/W,
              TILE_COUT/H/W, SCALE_ADDR, SHIFT_ADDR, [LUT_ADDR if SiLU], ZP_OUT 0, FLAGS 0; push 0x12.
  ELEM_WISE ADD: A/B/OUT_ADDR, LEN, MODE 0, A_LEN, B_LEN, ext ZP_A/B/O, SA, sA, SB, sB, SO, sO; push 0x15.
  ELEM_WISE MAX_POOL: A/OUT_ADDR, MODE 1, STRIDE, ext IN_C/H/W, POOL_K, POOL_P, POOL_MODE 0; push 0x15.
DRAM layout (new):
  - tensors are [C][H][W] int8; slice_ch and concat are turned into address aliases (weighted union-find on
    "tensor A lives at byte offset o inside tensor B"). A concat whose inputs cannot all be placed stays a host copy
    ("H copy src dst bytes" per input) and the reason is listed in the manifest.
  - per conv: weights already in SAURIA order, one block per output-channel range c0..c1 of the tile plan at
    W_ADDR + c0*K (K = cin*kh*kw); the block is sauria_weight_order(w[c0:c1], c_til=cin, k_til=c1-c0) = w[c0:c1]
    transposed [k][c][kh][kw] -> [c][kh][kw][k] (driver/libsauria_mem.h:143, as tb_has_net.cpp calls it per tile).
  - LUT int8[256], scale u32[cout], shift u32[cout], bias i32[cout] (PSUM preload), unchanged values.
  - upsample x2 stays a host step "H upsample2x in=.. out=.. c h w" (HW question H15).
Self-check: every tensor of net_has is compared by name, value for value, against net_has/dram_golden.bin.
Usage (repository root, FE_WORK set): python3 tools/fe/fe_emit_insts.py [--plan pe3] [--config pc_p9999_has]
Default plan: pe3 (the program of the recommended testbench profile, tools/has/tb_has_npu_top). --plan has = HAS default
tiling (no PE optimisation).

Plans pe / pe2 / pe3 / pe4 need --pe-csv (default $FE_WORK/has/insts_pe/measured_layers.csv): the per-layer tiling table
(columns layer, tiler_recommendation, one row per conv). It is INPUT DATA of the data package (under $FE_WORK); it was
produced by the internal measurement tooling (tools/metrics/pe_opt/pe_measured.py, not shipped) and is not regenerated
by this script.

--plan pe (PE-utilisation tiling from the per-layer table measured_layers.csv):
  S2  1x1 s1 p0 layers marked "flatten HxW -> 1xN row": the instruction carries IN_H = OUT_H = 1, IN_W = OUT_W = H*W
      ([C][H][W] is contiguous, so this is a rename, no data moves); h_t 1, cout_t 32, w_t from the csv clipped to
      min(768, floor(80896 / cin)) rounded down to a multiple of 32 (SRAM-A capacity rule).
  S3  stem: cout_t 32 (+16 tail), h_t 4, w_t 160.
  S1  "Cin split" layers: cout_t 32, h_t/w_t unchanged, TILE_CIN = cin_t (ext index 26, 0x400004D4; the DFC walks Cin
      parts per tile). Weight block [c0,c1) = Cin parts back to back: part j at W_ADDR + c0*K + j*nch*Kt. Tiles follow has/has_tile_iter.h (same grid as fe_tile_plan.py).
  Output FE_WORK/has/insts_pe/ + FE_WORK/has/net_pe/prog.bin (= net_has/prog.bin with the new tiles: the reference for
  tools/has/tb_has_mmio_replay). Tensor values are unchanged, only the tiling.
"""
import argparse
import csv
import json
import math
import os
import re
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fe_common as fc  # noqa: E402

import numpy as np  # noqa: E402

# has/has_mmio_compat.h (namespace mmio)
IN_ADDR, W_ADDR, OUT_ADDR, BIAS_ADDR = 0x40000400, 0x40000404, 0x40000408, 0x4000040C
KH, KW, STRIDE, PAD, ACT_TYPE, HAS_SKIP = 0x4000041C, 0x40000420, 0x40000424, 0x40000428, 0x4000042C, 0x40000430
A_ADDR, B_ADDR, LEN, MODE_PACK, A_LEN, B_LEN = 0x40000444, 0x40000448, 0x40000450, 0x40000454, 0x40000464, 0x40000468
PUSH_A = 0x40000310
EXT_BASE = 0x4000046C
EXT = ["IN_C", "IN_H", "IN_W", "OUT_C", "OUT_H", "OUT_W", "TILE_COUT", "TILE_H", "TILE_W", "SCALE_ADDR", "SHIFT_ADDR",
       "LUT_ADDR", "ZP_OUT", "FLAGS", "ZP_A", "ZP_B", "ZP_O", "SA", "SHA", "SB", "SHB", "SO", "SHO", "POOL_K", "POOL_P",
       "POOL_MODE", "TILE_CIN", "Y_USED"]
X = {n: EXT_BASE + 4 * i for i, n in enumerate(EXT)}
ALIGN = 64


def align(n, a=ALIGN):
    return (n + a - 1) // a * a


PE3_SKIP = ("stem", "det.p3.cls_out", "det.p4.cls_out")
SRAM_A_BYTES, SRAM_B_BYTES, SRAM_C_PSUMS, WT_MAX = 80896, 82944, 24576, 768


def tiles_of(cout, oh, ow, kh, kw, s, pad, cout_t, h_t, w_t, sa_y=32, y_reg=0, pad_tail=False):
    """has/has_tile_iter.h TileIter in Python: channel tile outermost, then oy, then ox. y_reg = Y_USED register
    (ext 27): used when it divides the tile width and is <= 32, else gcd(width, 32).
    pad_tail = FLAGS bit 0 PAD_TAIL: every tile COMPUTES c1 = c0 + cout_t and a width rounded up to y (y = Y_USED or
    32), y_used = y; only c < OUT_C, ox < OUT_W are written. Tiles here carry the computed geometry."""
    out = []
    y = y_reg if 0 < y_reg <= sa_y else sa_y
    for c0 in range(0, cout, cout_t):
        for oy0 in range(0, oh, h_t):
            for ox0 in range(0, ow, w_t):
                c1, oy1, ox1 = min(c0 + cout_t, cout), min(oy0 + h_t, oh), min(ox0 + w_t, ow)
                if pad_tail:
                    c1, ox1, yu = c0 + cout_t, ox0 + -(-(ox1 - ox0) // y) * y, y
                else:
                    yu = y_reg if 0 < y_reg <= sa_y and (ox1 - ox0) % y_reg == 0 else math.gcd(ox1 - ox0, sa_y)
                out.append({"c": [c0, c1], "oy": [oy0, oy1], "ox": [ox0, ox1],
                            "iy": [oy0 * s - pad, (oy1 - 1) * s - pad + kh], "ix": [ox0 * s - pad, (ox1 - 1) * s - pad + kw],
                            "x_used": c1 - c0, "y_used": yu})
    return out


def pe_plan(csv_path, prog, ops, shapes):
    """Per job: (cout_t, h_t, w_t, flatten) for S2 + S3; every other layer keeps its old tiling."""
    plan, why = {}, {}
    with open(csv_path) as f:
        rows = {r["layer"]: r["tiler_recommendation"] for r in csv.DictReader(f)}
    conv = {st["job"]: st for st in prog["steps"] if st["kind"] == "conv"}
    for job, rec in rows.items():
        if job not in conv:
            raise SystemExit("measured_layers.csv: unknown layer %s" % job)
        st = conv[job]
        o = ops[st["op_id"]]
        _, cin, _, _ = shapes[st["input"]]
        _, co, oh, ow = shapes[st["output"]]
        if rec.startswith("flatten"):
            if not (o["kh"] == o["kw"] == 1 and o["stride"][0] == 1 and max(o["pad"]) == 0):
                raise SystemExit("%s: S2 needs 1x1 s1 p0" % job)
            n = int(re.search(r"1x(\d+) row", rec).group(1))
            if n != oh * ow:
                raise SystemExit("%s: csv row length %d != H*W %d" % (job, n, oh * ow))
            w_csv = int(re.search(r"w_t (\d+)", rec).group(1))
            w_lim = min(WT_MAX, SRAM_A_BYTES // cin) // 32 * 32
            w_t = min(w_csv, w_lim)
            if w_t < 32:
                raise SystemExit("%s: w_t limit %d < 32" % (job, w_lim))
            plan[job] = (32, 1, w_t, True)
            why[job] = "S2 w_t csv %d, limit %d" % (w_csv, w_lim)
        elif job == "stem":
            plan[job] = (32, 4, 160, False)
            why[job] = "S3"
        elif "Cin split" in rec:  # S1: DFC walks Cin in cin_t parts per tile, psum stays in SRAM-C (TILE_CIN)
            cin_t = int(re.search(r"cin_t (\d+)", rec).group(1))
            h_t, w_t = (int(v) for v in re.search(r"h_t (\d+) w_t (\d+) unchanged", rec).groups())
            if (h_t, w_t) != (st["h_t"], st["w_t"]):
                raise SystemExit("%s: csv h_t/w_t %d/%d != program %d/%d" % (job, h_t, w_t, st["h_t"], st["w_t"]))
            if cin % cin_t or 32 * cin_t * o["kh"] * o["kw"] > SRAM_B_BYTES:
                raise SystemExit("%s: cin_t %d does not divide %d or weights over SRAM-B" % (job, cin_t, cin))
            plan[job] = (32, h_t, w_t, False)
            st["cin_t"] = cin_t
            why[job] = "S1 cin_t %d x %d" % (cin_t, cin // cin_t)
    for job, (ct, ht, wt, flat) in plan.items():
        st = conv[job]
        o = ops[st["op_id"]]
        _, cin, ih, iw = shapes[st["input"]]
        _, co, oh, ow = shapes[st["output"]]
        if flat:
            ih, iw, oh, ow = 1, ih * iw, 1, oh * ow
        tl = tiles_of(co, oh, ow, o["kh"], o["kw"], o["stride"][0], o["pad"][0], ct, ht, wt)
        for t in tl:
            c = (t["c"][1] - t["c"][0]) * (t["oy"][1] - t["oy"][0]) * (t["ox"][1] - t["ox"][0])
            a = st.get("cin_t", cin) * (min(t["iy"][1], ih) - max(t["iy"][0], 0)) * (min(t["ix"][1], iw) - max(t["ix"][0], 0))
            if c > SRAM_C_PSUMS or a > SRAM_A_BYTES:
                raise SystemExit("%s: tile %s over SRAM (C %d, A %d)" % (job, t, c, a))
        st.update(cout_t=ct, h_t=ht, w_t=wt, tiles=tl, n_tiles=len(tl), flat=flat, pe_note=why[job])
    return plan


def core_cost(tiles, K, parts, kh, s2=False):
    """Measured core law per tile (fitted on RTL-accurate runs): n_ctx * max(K, 230) + 281, n_ctx = computed positions / y_used;
    x_used < 32, or 1x1 with y_used < 32 -> x2 (stall, RTL-confirmed); 3x3 stride 2 with y_used 32 -> context 1.32 K."""
    tot = 0
    for t in tiles:
        y = t["y_used"]
        n = (t["oy"][1] - t["oy"][0]) * (t["ox"][1] - t["ox"][0]) // y
        c = n * max(K, 230) * (1.32 if s2 and y == 32 else 1) + 281
        tot += parts * (2 * c if t["x_used"] < 32 or (kh == 1 and y < 32) else c)
    return tot


def dma_bytes(tiles, cin, ih, iw, co, kh, kw, oh, ow, s=1, halo=False):
    """DMA bytes per layer (simple model): ifmap window per spatial tile (halo included, clipped
    to the image), weights, output; order = min(C-outer, spatial-outer) (S4).
    halo (tb --halo): layers without Cin split, kh > s, >= 2 tile rows, C-outer order only; a tile
    below the first row of its column reads only its new rows (the kh - s overlap is copied inside SRAM-A)."""
    sp, sp_h = {}, {}
    for t in tiles:
        k = (tuple(t["oy"]), tuple(t["ox"]))
        cols = min(t["ix"][1], iw) - max(t["ix"][0], 0)
        sp[k] = cin * (min(t["iy"][1], ih) - max(t["iy"][0], 0)) * cols
        y0 = t["iy"][0] + (kh - s if t["oy"][0] > 0 else 0)
        sp_h[k] = cin * (min(t["iy"][1], ih) - max(y0, 0)) * cols
    G = len({tuple(t["c"]) for t in tiles})
    ifm, T, wt, out = sum(sp.values()), len(sp), co * cin * kh * kw, co * oh * ow
    rows = len({tuple(t["oy"]) for t in tiles})
    c_outer = (sum(sp_h.values()) if halo and kh > s and rows >= 2 else ifm) * G + wt + out
    return min(c_outer, ifm + wt * T + out)


def total_cost(core, byt, F=1.22):
    """Simple total-cycle model per conv instruction: max(core*1.022, F*bytes/16), x1.26 when 0.6 < DMA/core < 1.0 (F = 1.19..1.22)."""
    d, c = F * byt / 16, 1.022 * core
    return max(c, d) * (1.26 if 0.6 < d / c < 1.0 else 1)


try:  # optional calibrated layer cost (core + DMA, tools/metrics/pe_opt/dma_cost.py); falls back to total_cost()
    sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "metrics", "pe_opt"))
    from dma_cost import cost as DMA_COST  # noqa: E402
except ImportError:
    DMA_COST = None


def fits(tiles, cin_t):
    """SRAM-A (input window incl. padding) and SRAM-C (computed psums) for the computed tile geometry."""
    for t in tiles:
        if cin_t * (t["iy"][1] - t["iy"][0]) * (t["ix"][1] - t["ix"][0]) > SRAM_A_BYTES:
            return False
        if t["x_used"] * (t["oy"][1] - t["oy"][0]) * (t["ox"][1] - t["ox"][0]) > SRAM_C_PSUMS:
            return False
    return True


def opt_plan(prog, ops, shapes, skip=(), total=False, table=None, measured=None, halo=False, c_bcast=False):
    """Per-layer tile optimisation on top of the S1/S2/S3 plan (cout_t, cin_t fixed):
    L2 = PAD_TAIL (FLAGS bit 0) on/off, L3 = (h_t, w_t) search for 3x3 layers, L5 = Y_USED 31 for 3x3 stride 2 (not the
    stem). S2 flattened layers keep their measured w_t (h_t 1); the stem and other 1x1 layers keep h_t / w_t.
    Lowest measured-law core cycles wins; a layer changes only if >= 1 % better than its current plan.
    total=True (plan pe4): the objective is total_cost(core, DMA) instead of core only, every layer incl. the stem.
    measured: {(job, h_t, w_t, y_used_reg, pad_tail): tb instr cycles} of configurations already measured; these
    replace the simple model (it mis-ranks DMA-bound layers such as stem PAD and cls_out PAD). halo: see dma_bytes.
    table: list to receive (job, h, w, y_reg, pad_tail, core, dma16, total) of the chosen plan per layer."""
    out = {}
    for st in prog["steps"]:
        if st["kind"] != "conv":
            continue
        o = ops[st["op_id"]]
        kh, kw, s, pad = o["kh"], o["kw"], o["stride"][0], o["pad"][0]
        _, cin, ih, iw = shapes[st["input"]]
        _, co, oh, ow = shapes[st["output"]]
        layer = dict(k=kh, s=s, p=pad, cin=cin, cout=co, H=oh, W=ow)  # dma_cost: real output size, flatten via tile_cfg
        if st.get("flat"):
            oh, ow = 1, oh * ow
            ih, iw = 1, ih * iw  # 1x1 s1 p0: the input is renamed the same way (DMA window clip)
        ct, cin_t = st["cout_t"], st.get("cin_t", cin)
        parts, K = cin // cin_t, cin_t * kh * kw
        s2 = kh == 3 and s == 2
        hl = halo and parts == 1
        dm = lambda tl: dma_bytes(tl, cin, ih, iw, co, kh, kw, oh, ow, s, hl)
        meas = measured or {}

        bkey = (st["job"], st["h_t"], st["w_t"], st.get("y_used_reg") or 0, bool(st.get("pad_tail")))
        base_core = core_cost(st["tiles"], K, parts, kh, s2)
        # per-layer calibration: measured / model of the current (measured) plan scales the model for unmeasured candidates
        r = meas[bkey] / total_cost(base_core, dm(st["tiles"])) if total and bkey in meas and not DMA_COST else 1.0

        def cost(tl, c, key=None):
            if total and DMA_COST and key:  # calibrated model: same yardstick for every candidate
                _, h_, w_, yr_, pt_ = key
                return DMA_COST(layer, dict(cout_t=ct, h_t=h_, w_t=w_, pad_tail=pt_, y_used=yr_,
                                            cin_t=cin_t if parts > 1 else None, flatten=bool(st.get("flat")), halo=halo, c_bcast=c_bcast))[2]
            if total and key in meas:
                return meas[key]
            return r * total_cost(c, dm(tl)) if total else c
        base = cost(st["tiles"], base_core, bkey)
        row = bkey + (base_core, dm(st["tiles"]) / 16, base, "M" if total and bkey in meas and not DMA_COST else "")
        if st["job"] in skip:  # skip: keep the current plan (measured as a loss on total time); still costed into table
            if table is not None:
                table.append(row)
            continue
        if kh == 3 and st["job"] != "stem":
            ws = sorted({w for w in [4, 8, 16] + list(range(32, ow + 1, 32)) + [ow] if w <= ow})
            if s2:
                ws = sorted(set(ws) | set(range(31, ow + 31, 31)))
            shapes_hw = [(None, w) for w in ws]
            yregs = (0, 31) if s2 else (0,)
        else:
            shapes_hw = [(st["h_t"], st["w_t"])]
            yregs = (0,)
        best = None
        for h_fix, w in shapes_hw:
            for yr in yregs:
                if yr and w % yr:
                    continue
                for pt in (False, True):
                    if yr and not pt and w > ow:
                        continue
                    hs = [h_fix] if h_fix else range(1, oh + 1)
                    for h in hs:
                        tl = tiles_of(co, oh, ow, kh, kw, s, pad, ct, h, w, y_reg=yr, pad_tail=pt)
                        if not fits(tl, cin_t):
                            break
                        cc = core_cost(tl, K, parts, kh, s2)
                        if total and best is not None and 0.98 * cc > best[0]:
                            continue  # total >= core: cannot win, skip the (slow) DMA simulation
                        c = cost(tl, cc, (st["job"], h, w, yr, pt))
                        if best is None or (c, len(tl)) < (best[0], len(best[5])):
                            best = (c, h, w, yr, pt, tl)
        if best and best[0] < 0.99 * base:
            c, h, w, yr, pt, tl = best
            if table is not None:
                cc = core_cost(tl, K, parts, kh, s2)
                key = (st["job"], h, w, yr, pt)
                table.append(key + (cc, dm(tl) / 16, cost(tl, cc, key), "M" if key in meas and not DMA_COST else ""))
            tags = [n for n, on in (("L2", pt), ("L3", (h, w) != (st["h_t"], st["w_t"])), ("L5", yr)) if on]
            st.update(h_t=h, w_t=w, tiles=tl, n_tiles=len(tl),
                      pe_note=(st.get("pe_note", "") + " " if st.get("pe_note") else "") + "%s h_t %d w_t %d%s%s core %d -> %d"
                      % ("+".join(tags), h, w, " Y_USED %d" % yr if yr else "", " PAD_TAIL" if pt else "", base, c))
            st.pop("y_used_reg", None)  # re-plan of an already optimised layer (pe4 on pe3): drop stale flags
            st.pop("pad_tail", None)
            if yr:
                st["y_used_reg"] = yr
            if pt:
                st["pad_tail"] = True
            out[st["job"]] = (ct, h, w, bool(st.get("flat")))
        elif table is not None:
            table.append(row)
    return out


def write_prog_pe(src, dst, prog):
    """Copy net_has/prog.bin (FEHP) with the conv tiles replaced by prog's (step order = conv order of prog)."""
    b = open(src, "rb").read()
    if b[:4] != b"FEHP":
        raise SystemExit("%s: not FEHP" % src)
    u32 = lambda at: struct.unpack_from("<I", b, at)[0]
    pos = 20 + 16
    nt = u32(pos)
    pos += 4 + 16 * nt
    ns = u32(pos)
    pos += 4
    out = bytearray(b[:pos])
    convs = iter([st for st in prog["steps"] if st["kind"] == "conv"])
    for _ in range(ns):
        kind = b[pos]
        if kind == 0:
            hdr = 1 + 4 * 3 + 1 + 4 * 11
            out += b[pos:pos + hdr]
            pos += hdr
            pos += 4 + 44 * u32(pos)
            st = next(convs)
            out += struct.pack("<I", len(st["tiles"]))
            for t in st["tiles"]:
                out += struct.pack("<11i", *(t["c"] + t["oy"] + t["ox"] + t["iy"] + t["ix"] + [t["y_used"]]))
            continue
        if kind == 2:
            size = 1 + 8 + 4 * u32(pos + 5)
        else:
            size = {1: 1 + 16, 4: 1 + 12, 5: 1 + 12 + 36, 6: 1 + 20}.get(kind)
        if size is None:
            raise SystemExit("prog.bin: unknown step kind %d" % kind)
        out += b[pos:pos + size]
        pos += size
    if pos != len(b) or next(convs, None) is not None:
        raise SystemExit("prog.bin rewrite: size / conv count mismatch")
    os.makedirs(os.path.dirname(dst), exist_ok=True)
    with open(dst, "wb") as f:
        f.write(out)


def sauria_block(w):
    """w [n, cin, kh, kw] int8 -> SAURIA order for c_til = cin, k_til = n: index ((c*kh + y)*kw + x)*n + k."""
    return np.ascontiguousarray(w.transpose(1, 2, 3, 0)).reshape(-1)


class Placement:
    """Weighted union-find: parent[t], off[t] = byte offset of t inside parent[t]."""

    def __init__(self):
        self.parent, self.off = {}, {}

    def find(self, t):
        if self.parent.get(t, t) == t:
            return t, 0
        r, o = self.find(self.parent[t])
        self.parent[t], self.off[t] = r, self.off[t] + o
        return r, self.off[t]

    def check(self, a, b, o):
        """Would 'a lives at offset o inside b' be consistent? Returns None if fine, else a reason."""
        ra, oa = self.find(a)
        rb, ob = self.find(b)
        if ra == rb and oa != ob + o:
            return "%s already at offset %d of %s, needs %d" % (a, oa, ra, ob + o)
        return None

    def union(self, a, b, o):
        ra, oa = self.find(a)
        rb, ob = self.find(b)
        if ra != rb:
            self.parent[ra], self.off[ra] = rb, ob + o - oa


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--config", default="pc_p9999_has")
    ap.add_argument("--plan", choices=["has", "pe", "pe2", "pe3", "pe4"], default="pe3")
    ap.add_argument("--pe-csv", default=os.path.join(fc.FE_WORK, "has", "insts_pe", "measured_layers.csv"))
    ap.add_argument("--out-dir", default=None)
    ap.add_argument("--halo", action="store_true", help="pe4: cost DMA with the SRAM-A halo reuse (tb --halo)")
    ap.add_argument("--c-bcast", action="store_true", help="pe4: cost CH3 bias as 4 B/channel (tb --c-bcast)")
    args = ap.parse_args()
    want = os.path.join(fc.FE_WORK, "has", {"has": "insts_has", "pe": "insts_pe", "pe2": "insts_pe2", "pe3": "insts_pe3", "pe4": "insts_pe4"}[args.plan])
    out_dir = os.path.abspath(args.out_dir or want)
    if out_dir != os.path.abspath(want):
        raise SystemExit("refusing out-dir other than %s for --plan %s: %s" % (want, args.plan, out_dir))

    with open(os.path.join(fc.FE_WORK, "has", "program_has_has.json")) as f:
        prog = json.load(f)
    with open(os.path.join(fc.FE_WORK, "step2", "graph_ir.json")) as f:
        ir = json.load(f)
    net_dir = os.path.join(fc.FE_WORK, "has", "net_has")
    with open(os.path.join(net_dir, "manifest.json")) as f:
        old = json.load(f)
    if old["config"] != args.config:
        raise SystemExit("net_has was exported with %s, not %s" % (old["config"], args.config))
    old_gold = np.fromfile(os.path.join(net_dir, "dram_golden.bin"), dtype=np.uint8)
    params = dict(np.load(os.path.join(fc.FE_WORK, "step3", args.config, "params.npz")))
    ops = {o["id"]: o for o in ir["ops"]}
    shapes = {t: v["shape"] for t, v in old["tensors"].items()}
    nbytes = {t: int(np.prod(s[1:])) for t, s in shapes.items()}
    conv_steps = {st["job"]: st for st in prog["steps"] if st["kind"] == "conv"}
    old_tiles = sum(len(st["tiles"]) for st in prog["steps"] if st["kind"] == "conv")
    plan = pe_plan(args.pe_csv, prog, ops, shapes) if args.plan != "has" else {}
    if args.plan == "pe3":  # pe2 minus the layers measured as a loss on total time
        plan.update(opt_plan(prog, ops, shapes, skip=PE3_SKIP))
    if args.plan == "pe4":  # PAD/L3/L5 per layer on total core + DMA, pe3 costed alongside
        import copy
        mdir = os.path.join(fc.FE_WORK, "has", "pe4_measured")  # measured per-layer tables of the pe / pe2 runs
        instr = lambda run: {r["layer"]: float(r["instr"]) for r in csv.DictReader(open(os.path.join(mdir, run, "layers.csv")))}
        key = lambda st: (st["job"], st["h_t"], st["w_t"], st.get("y_used_reg") or 0, bool(st.get("pad_tail")))
        conv = lambda p: [st for st in p["steps"] if st["kind"] == "conv"]
        m5, m5b = instr("m5_post"), instr("m5b_post")
        meas = {key(st): m5[st["job"]] for st in conv(prog) if st["job"] in m5}   # measured run of insts_pe = this base plan
        p2 = copy.deepcopy(prog)
        opt_plan(p2, ops, shapes)                                                   # measured run of insts_pe2
        meas.update({key(st): m5b[st["job"]] for st in conv(p2) if st["job"] in m5b})
        t3, t4 = [], []
        plan.update(opt_plan(prog, ops, shapes, skip=PE3_SKIP))                  # start from pe3 (gate: no layer worse)
        p3 = copy.deepcopy(prog)
        opt_plan(p3, ops, shapes, skip=[st["job"] for st in conv(p3)], total=True, table=t3, measured=meas, halo=args.halo, c_bcast=args.c_bcast)
        # stem kept as pe3: the tile core law mis-signs PAD for cin 3 (measured busy 1 466 560 -> 1 547 200 with PAD)
        plan.update(opt_plan(prog, ops, shapes, skip=("stem",), total=True, table=t4, measured=meas, halo=args.halo,
                             c_bcast=args.c_bcast))
        os.makedirs(out_dir, exist_ok=True)
        with open(os.path.join(out_dir, "pe4_decisions.csv"), "w", newline="") as f:
            w = csv.writer(f)
            w.writerow(["layer", "pe3_cfg", "pe4_cfg", "pe3_core", "pe3_dma16", "pe3_total", "pe4_core", "pe4_dma16", "pe4_total", "delta_pct"])
            cfg = lambda r: "h%d w%d%s%s%s" % (r[1], r[2], " Y%d" % r[3] if r[3] else "", " PT" if r[4] else "", " [M]" if r[8] else "")
            for a, b in zip(sorted(t3), sorted(t4)):
                w.writerow([a[0], cfg(a), cfg(b), round(a[5]), round(a[6]), round(a[7]), round(b[5]), round(b[6]), round(b[7]),
                            round(100 * (b[7] / a[7] - 1), 2)])
            w.writerow(["TOTAL", "", "", "", "", round(sum(r[7] for r in t3)), "", "", round(sum(r[7] for r in t4)),
                        round(100 * (sum(r[7] for r in t4) / sum(r[7] for r in t3) - 1), 2)])
    if args.plan == "pe2":  # + L2 (PAD_TAIL) + L3 (larger tiles) + L5 (3x3 s2 Y_USED 31)
        plan.update(opt_plan(prog, ops, shapes))

    # ---- 1. placement: slices first (always a view of their input), then concats in program order ----
    pl = Placement()
    host_left, notes = [], []
    for st in prog["steps"]:
        if st["kind"] == "host" and st["op"] == "slice_ch":
            o = ops[st["op_id"]]
            _, _, h, w = shapes[o["inputs"][0]]
            pl.union(o["output"], o["inputs"][0], o["start"] * h * w)
    for st in prog["steps"]:
        if st["kind"] == "host" and st["op"] == "concat":
            o = ops[st["op_id"]]
            offs, off = [], 0
            for t in o["inputs"]:
                offs.append(off)
                off += nbytes[t]
            reasons = [r for t, of in zip(o["inputs"], offs) for r in [pl.check(t, o["output"], of)] if r]
            if reasons:
                host_left.append(st)
                notes.append({"op_id": st["op_id"], "op": "concat", "output": o["output"], "why": reasons})
                continue
            for t, of in zip(o["inputs"], offs):
                pl.union(t, o["output"], of)

    # ---- 2. addresses: one region per placement root, sized to cover every member ----
    roots = {}
    for t in shapes:
        r, o = pl.find(t)
        lo, hi = roots.get(r, (0, 0))
        roots[r] = (min(lo, o), max(hi, o + nbytes[t]))
    addr, root_addr = 0, {}
    for t in shapes:  # manifest order = old tensor-table order
        r, _ = pl.find(t)
        if r not in root_addr:
            lo, hi = roots[r]
            root_addr[r] = addr - lo
            addr = align(addr + hi - lo)
    taddr = {t: root_addr[pl.find(t)[0]] + pl.find(t)[1] for t in shapes}
    tensor_bytes = addr

    # ---- 3. conv parameters ----
    conv = {}
    for st in prog["steps"]:
        if st["kind"] != "conv":
            continue
        j = st["job"]
        q_w = params[j + "/q_w"].astype(np.int8)
        cout, cin, kh, kw = q_w.shape
        K = cin * kh * kw
        ranges = sorted({(t["c"][0], t["c"][1]) for t in st["tiles"]})
        cpad = ranges[-1][1]  # > cout only with PAD_TAIL: the last block is padded with zero weights to cout_t
        q_w = np.concatenate([q_w, np.zeros((cpad - cout, cin, kh, kw), np.int8)]) if cpad > cout else q_w
        blob = np.zeros(cpad * K, dtype=np.int8)
        cov = 0
        ct = st.get("cin_t", cin)
        Kt = ct * kh * kw
        for c0, c1 in ranges:  # S1: Cin parts back to back inside the channel block, part j at c0*K + j*nch*Kt
            for jp in range(cin // ct):
                a = c0 * K + jp * (c1 - c0) * Kt
                blob[a:a + (c1 - c0) * Kt] = sauria_block(q_w[c0:c1, jp * ct:(jp + 1) * ct])
            cov += c1 - c0
        if cov != cpad or ranges[0][0] != 0 or any(a[1] != b[0] for a, b in zip(ranges, ranges[1:])):
            raise SystemExit("%s: tile channel ranges do not tile [0, %d): %s" % (j, cout, ranges))
        rec = {}
        for key, arr in (("w", blob), ("lut", params.get(j + "/lut", np.zeros(256, np.int8)).astype(np.int8)),
                         ("scale", params[j + "/scale"].astype("<u4")), ("shift", params[j + "/shift"].astype("<u4")),
                         ("bias", params[j + "/q_b"].astype("<i4"))):
            rec[key] = (addr, arr)
            addr = align(addr + arr.nbytes)
        conv[j] = rec
    total = addr

    # ---- 4. DRAM images + value self-check ----
    def tval(t):
        a = old["tensors"][t]["addr"]
        return old_gold[a:a + nbytes[t]]

    init = np.zeros(total, dtype=np.uint8)
    for rec in conv.values():
        for a, arr in rec.values():
            init[a:a + arr.nbytes] = np.frombuffer(arr.tobytes(), dtype=np.uint8)
    gin = ir["graph_input"]
    init[taddr[gin]:taddr[gin] + nbytes[gin]] = tval(gin)
    gold = init.copy()
    for t in shapes:
        gold[taddr[t]:taddr[t] + nbytes[t]] = tval(t)
    bad = [t for t in shapes if not np.array_equal(gold[taddr[t]:taddr[t] + nbytes[t]], tval(t))]
    if bad:
        raise SystemExit("self-check FAILED: %d tensors differ after aliasing, first %s" % (len(bad), bad[0]))

    # ---- 5. MMIO stream ----
    lines, n_w, n_push, n_h = [], 0, 0, 0

    def W(a, v, c=""):
        nonlocal n_w
        lines.append("W %08x %x%s" % (a, v & 0xFFFFFFFF, ("  # " + c) if c else ""))
        n_w += 1

    def H(s):
        nonlocal n_h
        lines.append("H " + s)
        n_h += 1

    lines.append("# fe_emit_insts.py: %s, %d program steps, DRAM %d B" % (args.config, len(prog["steps"]), total))
    host_ids = {id(s) for s in host_left}
    for si, st in enumerate(prog["steps"]):
        k = st["kind"]
        if k == "conv":
            o = ops[st["op_id"]]
            rec = conv[st["job"]]
            if len(set(o["pad"])) != 1 or o["stride"][0] != o["stride"][1]:
                raise SystemExit("%s: pad/stride not symmetric" % st["job"])
            _, cin, ih, iw = shapes[st["input"]]
            _, co, oh, ow = shapes[st["output"]]
            if st.get("flat"):
                ih, iw, oh, ow = 1, ih * iw, 1, oh * ow
            silu = o["act"] == "silu"
            lines.append("# [%d] step %d GEMM_FUSED %s %dx%d s%d p%d cin %d cout %d, %d tiles%s"
                         % (n_push, si, st["job"], o["kh"], o["kw"], o["stride"][0], o["pad"][0], cin, co, len(st["tiles"]),
                            (" (" + st["pe_note"] + ")") if "pe_note" in st else ""))
            W(IN_ADDR, taddr[st["input"]]); W(W_ADDR, rec["w"][0]); W(OUT_ADDR, taddr[st["output"]]); W(BIAS_ADDR, rec["bias"][0])
            W(KH, o["kh"]); W(KW, o["kw"]); W(STRIDE, o["stride"][0]); W(PAD, o["pad"][0])
            W(ACT_TYPE, 2 if silu else 0); W(HAS_SKIP, 0)
            for n, v in (("IN_C", cin), ("IN_H", ih), ("IN_W", iw), ("OUT_C", co), ("OUT_H", oh), ("OUT_W", ow),
                         ("TILE_COUT", st["cout_t"]), ("TILE_H", st["h_t"]), ("TILE_W", st["w_t"]),
                         ("SCALE_ADDR", rec["scale"][0]), ("SHIFT_ADDR", rec["shift"][0]), ("LUT_ADDR", rec["lut"][0]),
                         ("ZP_OUT", 0), ("FLAGS", 1 if st.get("pad_tail") else 0)):
                if n != "LUT_ADDR" or silu:
                    W(X[n], v)
            if "cin_t" in st:  # only S1 layers; unset = whole Cin
                W(X["TILE_CIN"], st["cin_t"])
            if "y_used_reg" in st:  # only L5 layers; unset = gcd(tile width, 32)
                W(X["Y_USED"], st["y_used_reg"])
            W(PUSH_A, 0x12)
            n_push += 1
        elif k == "elem_add":
            n = nbytes[st["out"]]
            lines.append("# [%d] step %d ELEM_WISE ADD from %s, %d elements" % (n_push, si, st["from_job"], n))
            W(A_ADDR, taddr[st["a"]]); W(B_ADDR, taddr[st["b"]]); W(OUT_ADDR, taddr[st["out"]])
            W(LEN, n); W(MODE_PACK, 0); W(A_LEN, n); W(B_LEN, n)
            for nm, key in (("ZP_A", "zpA"), ("ZP_B", "zpB"), ("ZP_O", "zpO"), ("SA", "SA"), ("SHA", "sA"), ("SB", "SB"),
                            ("SHB", "sB"), ("SO", "SO"), ("SHO", "sO")):
                W(X[nm], st[key])
            W(PUSH_A, 0x15)
            n_push += 1
        elif k == "elem_max":
            _, c, h, w = shapes[st["in"]]
            lines.append("# [%d] step %d ELEM_WISE MAX_POOL %dx%d s%d p%d" % (n_push, si, st["k"], st["k"], st["s"], st["p"]))
            W(A_ADDR, taddr[st["in"]]); W(OUT_ADDR, taddr[st["out"]]); W(MODE_PACK, 1); W(STRIDE, st["s"])
            W(X["IN_C"], c); W(X["IN_H"], h); W(X["IN_W"], w)
            W(X["POOL_K"], st["k"]); W(X["POOL_P"], st["p"]); W(X["POOL_MODE"], 0)
            W(PUSH_A, 0x15)
            n_push += 1
        else:
            o = ops[st["op_id"]]
            if o["op"] == "slice_ch":
                lines.append("# step %d slice_ch %s = %s[%d:%d] -> address alias 0x%x"
                             % (si, o["output"], o["inputs"][0], o["start"], o["end"], taddr[o["output"]]))
            elif o["op"] == "concat" and id(st) not in host_ids:
                lines.append("# step %d concat %s -> inputs already written in place at 0x%x"
                             % (si, o["output"], taddr[o["output"]]))
            elif o["op"] == "concat":
                off = 0
                for t in o["inputs"]:
                    if taddr[t] != taddr[o["output"]] + off:
                        H("copy src=0x%x dst=0x%x bytes=%d  # concat %s <- %s" % (taddr[t], taddr[o["output"]] + off, nbytes[t],
                                                                              o["output"], t))
                    off += nbytes[t]
            elif o["op"] == "upsample_nearest":
                _, c, h, w = shapes[o["inputs"][0]]
                H("upsample%dx in=0x%x out=0x%x c=%d h=%d w=%d" % (o["factor"], taddr[o["inputs"][0]], taddr[o["output"]], c, h, w))
            else:
                raise SystemExit("unsupported host op %s" % o["op"])

    os.makedirs(out_dir, exist_ok=True)
    with open(os.path.join(out_dir, "mmio.txt"), "w") as f:
        f.write("\n".join(lines) + "\n")
    init.tofile(os.path.join(out_dir, "dram_init.bin"))
    gold.tofile(os.path.join(out_dir, "dram_golden.bin"))
    manifest = {
        "config": args.config, "image": old["image"], "dram_bytes": total, "tensor_region_bytes": tensor_bytes,
        "old_dram_bytes": old["dram_bytes"], "instructions": n_push, "mmio_writes": n_w, "host_lines": n_h,
        "tensors": {t: {"addr": taddr[t], "shape": shapes[t], "storage_root": pl.find(t)[0], "offset_in_root": pl.find(t)[1]}
                    for t in shapes},
        "conv_params": {j: {k: {"addr": a, "bytes": int(arr.nbytes)} for k, (a, arr) in rec.items()} for j, rec in conv.items()},
        "host_steps_kept": notes, "graph_input": gin, "cut_outputs": ir["cut_outputs"],
        "weight_layout": "SAURIA order per tile channel range: w[c0:c1] as [c][kh][kw][k] at W_ADDR + c0*K",
        "plan": args.plan, "tiles": sum(len(st["tiles"]) for st in prog["steps"] if st["kind"] == "conv"),
        "tiles_has_plan": old_tiles,
        "pe_layers": {j: {"cout_t": v[0], "h_t": v[1], "w_t": v[2], "flatten": v[3], "cin_t": conv_steps[j].get("cin_t"),
                          "y_used_reg": conv_steps[j].get("y_used_reg"), "pad_tail": bool(conv_steps[j].get("pad_tail"))}
                      for j, v in plan.items()},
    }
    if args.plan != "has":
        write_prog_pe(os.path.join(net_dir, "prog.bin"),
                      os.path.join(fc.FE_WORK, "has", "net_" + args.plan, "prog.bin"), prog)
    with open(os.path.join(out_dir, "manifest.json"), "w") as f:
        json.dump(manifest, f, indent=1)
    print("[emit] %d instructions (%d GEMM_FUSED + %d ELEM_WISE), %d MMIO writes, %d H lines, concats kept on host %d"
          % (n_push, len(conv), n_push - len(conv), n_w, n_h, len(notes)))
    print("[emit] plan %s: %d tiles (has plan %d), %d layers retiled" % (args.plan, manifest["tiles"], old_tiles, len(plan)))
    print("[emit] DRAM %d B (tensors %d B) vs net_has %d B; self-check: %d/%d tensors equal by name"
          % (total, tensor_bytes, old["dram_bytes"], len(shapes), len(shapes)))


if __name__ == "__main__":
    main()
