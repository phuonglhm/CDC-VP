"""Golden for ONE ViT-B/16 encoder block, every step in the integer form the DFC runs it:
  LN1 (LAYERNORM 0x14, GVU bit-exact fe_ref_gvu) -> QKV GEMM 2304 (0x12, per-channel requant) -> 12 heads FUSED_ATTN (0x13,
  fe_ref_gvu.fused_attn, same knobs as has/gvu_fused_attn.h) -> proj GEMM -> ADD residual (0x15, fe_ref_has.elem_add)
  -> LN2 -> fc1 GEMM + GELU LUT (act 3, LUT direct) -> fc2 GEMM -> ADD residual.
Scales: fe_work/has/vit/vit_scales.json (fe_vit_quant.py calibration); weights / image as fe_ref_gvu.vit_heads().
Output fe_work/has/vit_block/golden/: golden.npz (int tensors, token-major [L][C]; heads [12][L][64]) + golden.json (params,
cos vs the float block). The instruction stream (dram_init/dram_golden/mmio.txt) is written from these by --emit (layout:
every activation CHANNEL-MAJOR [C][tokens], FLAGS bit 1 CHANNEL_MAJOR on 0x13 / 0x14, docs/SW_INTEGRATION_GUIDE.md).
block_golden() and Emitter are reused by fe_vit_full.py (all 12 blocks + embedding + head).
Usage (repository root, FE_WORK set): python3 tools/fe/fe_vit_block.py [--block 0]   then   python3 tools/fe/fe_vit_block.py --emit
Run: tools/has/tb_has_npu_top $FE_WORK/has/vit_block --c-bcast   (recommended profile + bias broadcast)
"""
import argparse
import glob
import struct
import json
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fe_common as fc  # noqa: E402
import fe_ref_gvu as gv  # noqa: E402
import fe_ref_has as fh  # noqa: E402
import fe_vit_float as vf  # noqa: E402
import fe_vit_quant as vq  # noqa: E402

import numpy as np  # noqa: E402

K = gv.DEFAULT_KNOBS
AK = gv.ATTN_KNOBS
LK = gv.LN_KNOBS


def ln(xq, sx, g, b, s_out):
    """LAYERNORM rows through the GVU integer path; params calibrated on these rows (per-tensor, as the GVU LAYERNORM vectors)."""
    rows = xq.tolist()
    p = gv.ln_params(rows, sx, g.tolist(), b.tolist(), s_out, LK)
    cnt = dict.fromkeys(("d_int32", "v_int64", "vshift_int32", "s3_int16", "s3_floor", "p_int48", "s5_sat"), 0)
    y = np.array([gv.layernorm_row(r, p, K, LK, cnt)["y"] for r in rows], dtype=np.int64)
    return y, p, cnt


PSUM_MAX = (1 << 31) - 1
BIAS_MAX = 1 << 30  # |q_bias| <= 2^30 keeps bias + sum(x*w) inside int32


def gemm(xq, sx, Wf, bf, s_out):
    """GEMM_FUSED: int8 x per-channel int8 W + int32 bias (PSUM preload) -> requant; also returns the DFC parameters.
    The HW PSUM is int32 (bias preload + accumulate) and wraps: a near-dead channel (tiny weight scale) made
    bias / (sx * sw) huge and fc1 wrapped on 1 033 elements. Per-channel weight-scale floor sw >= |b| / (sx * 2^30),
    then every psum is checked to lie inside int32."""
    Wf = np.asarray(Wf, np.float64)
    sw = np.maximum(np.abs(Wf).max(1) / 127.0, 1e-12)
    floor = np.abs(np.asarray(bf, np.float64)) / (sx * BIAS_MAX)
    raised = int((floor > sw).sum())
    sw = np.maximum(sw, floor)
    wq = np.clip(np.floor(Wf / sw[:, None] + 0.5), -127, 127).astype(np.int64)
    qb = np.floor(np.asarray(bf, np.float64) / (sx * sw) + 0.5).astype(np.int64)
    acc = np.asarray(xq, np.int64) @ wq.T + qb
    ovf = int((np.abs(acc) > PSUM_MAX).sum())
    if ovf or np.abs(qb).max() > PSUM_MAX:
        raise SystemExit("GEMM psum outside int32: %d elements (max |bias| %d)" % (ovf, np.abs(qb).max()))
    S, s = vq.req_params(sx * sw / np.atleast_1d(s_out))
    y = fh.requant(acc, S, s, 0, vq.K_HAS).astype(np.int64)
    return y, {"w": wq, "bias": qb, "S": S, "s": s, "raised": raised, "acc_max": int(np.abs(acc).max())}


def gemm_into(G, P, pre, xq, sx, Wf, bf, s_out):
    """gemm() + store its DFC parameters under G[pre_*] and its int32 audit under P[gemm_pre]."""
    y, gp = gemm(xq, sx, Wf, bf, s_out)
    G.update({pre + "_w": gp["w"], pre + "_bias": gp["bias"], pre + "_S": gp["S"], pre + "_s": gp["s"]})
    P["gemm_" + pre] = {"raised_channels": gp["raised"], "psum_max": gp["acc_max"]}
    return y


def block_golden(W, bi, xq, sx, s, log=None):
    """One encoder block on the int8 residual stream xq [L][768] (scale sx) -> (G tensors, P parameters); G["y"] has scale s["x"]."""
    t0 = time.time()
    b = "blocks.%d." % bi
    L = xq.shape[0]
    G, P = {"x": xq}, {"block": bi, "sx": sx, "scales": s}
    G["ln1"], P["ln1"], P["ln1_cnt"] = ln(xq, sx, W[b + "norm1.weight"], W[b + "norm1.bias"], s["ln1"])
    so = np.repeat([s["q"], s["k"], s["v"]], 768)
    G["qkv"] = gemm_into(G, P, "qkv", G["ln1"], s["ln1"], W[b + "attn.qkv.weight"], W[b + "attn.qkv.bias"], so)
    q, k, v = (G["qkv"][:, j * 768:(j + 1) * 768].reshape(L, 12, 64).transpose(1, 0, 2) for j in range(3))
    Mqk, TSqk = gv.req_params(s["q"] * s["k"] / 8.0 / s["s"])
    Mav, TSav = gv.req_params((1.0 / 256) * s["v"] / s["av"])
    O, heads = np.zeros((12, L, 64), np.int64), []
    for hd in range(12):
        h = {"Q": q[hd].tolist(), "K": k[hd].tolist(), "V": v[hd].tolist(), "Zq": 0, "Zk": 0, "Zv": 0, "Zqk": 0, "Zav": 0,
             "Mqk": Mqk, "TSqk": TSqk, "Mav": Mav, "TSav": TSav, "sx": s["s"], "mask": [[0] * L for _ in range(L)]}
        r = gv.fused_attn(h, K, AK)
        O[hd] = np.array(r["O"])
        heads.append({"ovf16": r["ovf16"]})
    if log:
        log("block %d: 12 heads %.0fs" % (bi, time.time() - t0))
    G["attn_o"] = O
    P["attn"] = {"Mqk": Mqk, "TSqk": TSqk, "Mav": Mav, "TSav": TSav, "exp": gv.exp_table(s["s"], K), "heads": heads}
    av = O.transpose(1, 0, 2).reshape(L, 768)
    G["proj"] = gemm_into(G, P, "proj", av, s["av"], W[b + "attn.proj.weight"], W[b + "attn.proj.bias"], s["proj"])
    P["add1"] = fh.elem_add_params("has", sx, s["proj"], s["x1"])
    G["x1"] = fh.elem_add(xq, G["proj"], P["add1"], vq.K_HAS).astype(np.int64)
    G["ln2"], P["ln2"], P["ln2_cnt"] = ln(G["x1"], s["x1"], W[b + "norm2.weight"], W[b + "norm2.bias"], s["ln2"])
    G["h"] = gemm_into(G, P, "fc1", G["ln2"], s["ln2"], W[b + "mlp.fc1.weight"], W[b + "mlp.fc1.bias"], s["h"])
    lv = np.arange(-128, 128) * s["h"]
    G["gelu_lut"] = np.clip(np.floor(0.5 * lv * (1 + vq.erf(lv / np.sqrt(2.0))) / s["g"] + 0.5), -128, 127).astype(np.int64)
    G["g"] = G["gelu_lut"][G["h"] + 128]
    G["mlp"] = gemm_into(G, P, "fc2", G["g"], s["g"], W[b + "mlp.fc2.weight"], W[b + "mlp.fc2.bias"], s["mlp"])
    P["add2"] = fh.elem_add_params("has", s["x1"], s["mlp"], s["x"])
    G["y"] = fh.elem_add(G["x1"], G["mlp"], P["add2"], vq.K_HAS).astype(np.int64)
    return G, P


def sauria_block(w):
    """w [n, cin] int8 (1x1) -> SAURIA order c-major, k fastest (fe_emit_insts.sauria_block)."""
    return np.ascontiguousarray(w.T).reshape(-1)


EXT = ["IN_C", "IN_H", "IN_W", "OUT_C", "OUT_H", "OUT_W", "TILE_COUT", "TILE_H", "TILE_W", "SCALE_ADDR", "SHIFT_ADDR",
       "LUT_ADDR", "ZP_OUT", "FLAGS", "ZP_A", "ZP_B", "ZP_O", "SA", "SHA", "SB", "SHB", "SO", "SHO", "POOL_K", "POOL_P",
       "POOL_MODE", "TILE_CIN", "Y_USED", "ROWS", "PARAM_ADDR", "MASK_ADDR"]
X = {n: 0x4000046C + 4 * i for i, n in enumerate(EXT)}  # FLAGS 0x4A0, ROWS 0x4DC, PARAM_ADDR 0x4E0 (docs/SW_INTEGRATION_GUIDE.md, section 3.3)


class Emitter:
    """DRAM images + MMIO stream (format of fe_emit_insts.py / make_rce_gate.py); every activation channel-major [C][L]."""

    def __init__(self, title):
        self.init, self.gold, self.mm, self.n = bytearray(), bytearray(), ["# " + title], 0

    def alloc(self, di, dg=None):
        while len(self.init) % 64:
            self.init.append(0)
            self.gold.append(0)
        a = len(self.init)
        self.init.extend(di)
        self.gold.extend(dg if dg is not None else di)
        return a

    @staticmethod
    def cm(t):  # [L][C] -> [C][L] int8 bytes
        return np.ascontiguousarray(np.asarray(t).T).astype(np.int8).tobytes()

    def data(self, t):  # input tensor: same bytes in init and golden
        return self.alloc(self.cm(t))

    def act(self, t):  # output region: zeros in init, golden in gold
        return self.alloc(bytes(np.asarray(t).size), self.cm(t))

    def w(self, a, v):
        self.mm.append("W %08x %x" % (a, int(v) & 0xFFFFFFFF))

    def gemm(self, name, xa, oa, G, pre, L, lut=None):
        """1x1 GEMM over L tokens; cout padded to a multiple of 32 (PAD_TAIL computes whole channel tiles)."""
        wq = G[pre + "_w"].astype(np.int8)
        cout, cin = wq.shape
        cpad = -(-cout // 32) * 32
        pad = cpad - cout
        if pad:
            wq = np.concatenate([wq, np.zeros((pad, cin), np.int8)])
        cin_t = 768 if cin > 768 else cin  # S1 Cin split: SRAM-A holds cin_t x w_t (80 896 B)
        blob = np.zeros(cpad * cin, np.int8)
        for c0 in range(0, cpad, 32):  # part j at c0*K + j*32*cin_t (fe_emit_insts S1 layout)
            for jp in range(cin // cin_t):
                a = c0 * cin + jp * 32 * cin_t
                blob[a:a + 32 * cin_t] = sauria_block(wq[c0:c0 + 32, jp * cin_t:(jp + 1) * cin_t])
        padv = lambda arr, v: np.concatenate([arr, np.full(pad, v, arr.dtype)]) if pad else arr  # noqa: E731
        wa = self.alloc(blob.tobytes())
        ba = self.alloc(padv(G[pre + "_bias"], 0).astype("<i4").tobytes())
        sa = self.alloc(padv(G[pre + "_S"], 1 << 30).astype("<u4").tobytes())
        ha = self.alloc(padv(G[pre + "_s"], 30).astype("<u4").tobytes())
        la = self.alloc(np.asarray(lut).astype(np.int8).tobytes()) if lut is not None else 0
        self.mm.append("# [%d] GEMM_FUSED %s cin %d cout %d tokens %d%s" % (self.n, name, cin, cout, L, " + LUT" if la else ""))
        for a, v in ((0x40000400, xa), (0x40000404, wa), (0x40000408, oa), (0x4000040C, ba), (0x4000041C, 1), (0x40000420, 1),
                     (0x40000424, 1), (0x40000428, 0), (0x4000042C, 3 if la else 0), (0x40000430, 0)):
            self.w(a, v)
        for k, v in (("IN_C", cin), ("IN_H", 1), ("IN_W", L), ("OUT_C", cout), ("OUT_H", 1), ("OUT_W", L), ("TILE_COUT", 32),
                     ("TILE_H", 1), ("TILE_W", 96), ("SCALE_ADDR", sa), ("SHIFT_ADDR", ha), ("ZP_OUT", 0), ("FLAGS", 1)):
            self.w(X[k], v)
        if la:
            self.w(X["LUT_ADDR"], la)
        if cin_t != cin:
            self.w(X["TILE_CIN"], cin_t)
        self.w(0x40000310, 0x12)
        self.n += 1

    def lnorm(self, name, xa, oa, p, L):
        H = p["H"]
        per = lambda v: v if isinstance(v, list) else [v] * H  # noqa: E731
        scal = [H, p["Pre_Shift"], p["M0_var"], p["TS_var"], p["Z_var"], p["M0_7"], p["TS_7"], p["E_bias"], p["Z_7"],
                p["M0_div"], p["TS_div"], p["Z_div"], p["Z_mul"], p["Z_out"], int(LK["r8_floor"] == "z_out"), LK["r8_z_out"],
                int(LK["r9_split"] == "exact"), LK["r9_lsb_bits"], int(LK["out_fmt"] == "int16")]
        blk = struct.pack("<Ii", 0x31504E4C, H) + struct.pack("<19q", *scal) + struct.pack("<%di" % H, *per(p["gamma_q"]))
        blk += struct.pack("<%dq" % H, *per(p["beta_q"])) + struct.pack("<%dq" % H, *per(p["M0_mul"]))
        blk += struct.pack("<%di" % H, *per(p["TS_mul"]))
        pa = self.alloc(blk)
        self.mm.append("# [%d] LAYERNORM %s rows %d H %d channel-major" % (self.n, name, L, H))
        for a, v in ((0x40000400, xa), (0x40000408, oa), (0x40000450, H), (X["ROWS"], L), (X["PARAM_ADDR"], pa), (X["FLAGS"], 2)):
            self.w(a, v)
        self.w(0x40000310, 0x14)
        self.n += 1

    def add(self, name, aa, ba, oa, p, e):
        self.mm.append("# [%d] ELEM_WISE ADD %s, %d elements" % (self.n, name, e))
        for a, v in ((0x40000444, aa), (0x40000448, ba), (0x40000408, oa), (0x40000450, e), (0x40000454, 0), (0x40000464, e),
                     (0x40000468, e)):
            self.w(a, v)
        for k, key in (("ZP_A", "zpA"), ("ZP_B", "zpB"), ("ZP_O", "zpO"), ("SA", "SA"), ("SHA", "sA"), ("SB", "SB"),
                       ("SHB", "sB"), ("SO", "SO"), ("SHO", "sO")):
            self.w(X[k], p[key])
        self.w(0x40000310, 0x15)
        self.n += 1

    def attn(self, qkva, oa, at, L, tag=""):
        flags = int(AK["asym_zp"]) | (int(AK["mask_e"] == "zero") << 1) | (int(AK["av_zv_corr"]) << 2)
        blk = struct.pack("<I5i", 0x314E5441, 0, 0, 0, 0, 0) + struct.pack("<qi", at["Mqk"], at["TSqk"])
        blk += struct.pack("<qi", at["Mav"], at["TSav"]) + struct.pack("<II", flags, 0) + struct.pack("<256i", *at["exp"])
        pa = self.alloc(blk)
        for hd in range(12):  # head h: rows hd*64 .. hd*64+63 of the [C][L] tensors (channel-major)
            self.mm.append("# [%d] FUSED_ATTN %shead %d NQ %d L %d D 64 channel-major" % (self.n, tag, hd, L, L))
            for a, v in ((0x40000444, qkva + hd * 64 * L), (0x40000448, qkva + (768 + hd * 64) * L),
                         (0x4000044C, qkva + (1536 + hd * 64) * L), (0x40000450, L), (0x40000458, 64),
                         (0x40000408, oa + hd * 64 * L), (X["ROWS"], L), (X["PARAM_ADDR"], pa), (X["FLAGS"], 2)):
                self.w(a, v)
            self.w(0x40000310, 0x13)
            self.n += 1

    def block(self, G, P, xa):
        """The 20 instructions of one encoder block reading the residual stream at xa; returns the address of its output y."""
        L, e, t = G["x"].shape[0], G["x"].size, "b%d " % P["block"]
        ln1a, qkva = self.act(G["ln1"]), self.act(G["qkv"])
        oa = self.act(G["attn_o"].transpose(1, 0, 2).reshape(L, 768))
        proja, x1a, ln2a = self.act(G["proj"]), self.act(G["x1"]), self.act(G["ln2"])
        ga, mlpa, ya = self.act(G["g"]), self.act(G["mlp"]), self.act(G["y"])
        self.lnorm(t + "ln1", xa, ln1a, P["ln1"], L)
        self.gemm(t + "qkv", ln1a, qkva, G, "qkv", L)
        self.attn(qkva, oa, P["attn"], L, t)
        self.gemm(t + "proj", oa, proja, G, "proj", L)
        self.add(t + "x1 = x + proj", xa, proja, x1a, P["add1"], e)
        self.lnorm(t + "ln2", x1a, ln2a, P["ln2"], L)
        self.gemm(t + "fc1+gelu", ln2a, ga, G, "fc1", L, lut=G["gelu_lut"])
        self.gemm(t + "fc2", ga, mlpa, G, "fc2", L)
        self.add(t + "y = x1 + mlp", x1a, mlpa, ya, P["add2"], e)
        return ya

    def write(self, out):
        while len(self.init) % 4096:
            self.init.append(0)
            self.gold.append(0)
        os.makedirs(out, exist_ok=True)
        open(os.path.join(out, "dram_init.bin"), "wb").write(self.init)
        open(os.path.join(out, "dram_golden.bin"), "wb").write(self.gold)
        open(os.path.join(out, "mmio.txt"), "w").write("\n".join(self.mm) + "\n")
        return len(self.init)


def emit(gdir, out):
    """golden.npz/json -> dram_init.bin, dram_golden.bin, mmio.txt."""
    G = dict(np.load(os.path.join(gdir, "golden.npz")))
    P = json.load(open(os.path.join(gdir, "golden.json")))
    em = Emitter("fe_vit_block.py --emit: ViT-B/16 block %d, L %d, channel-major [C][L]" % (P["block"], G["x"].shape[0]))
    em.block(G, P, em.data(G["x"]))
    size = em.write(out)
    print("[vit-block] emit: %d instructions, DRAM %d B -> %s" % (em.n, size, out))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--block", type=int, default=0)
    ap.add_argument("--emit", action="store_true", help="write the instruction stream from an existing golden")
    args = ap.parse_args()
    t0 = time.time()
    out = os.path.join(fc.FE_WORK, "has", "vit_block", "golden")
    if args.emit:
        return emit(out, os.path.dirname(out))
    os.makedirs(out, exist_ok=True)
    W = vf.load_safetensors(vf.WEIGHTS)
    with open(os.path.join(fc.FE_WORK, "has", "vit", "vit_scales.json")) as f:
        sc = json.load(f)
    imgs = sorted(glob.glob(os.path.join(fc.FE_WORK, "datasets", "coco128", "images", "train2017", "*.jpg")))
    img = imgs[16]  # first image after the 16 calibration images (as fe_ref_gvu.vit_heads)
    tp = vq.forward_taps(W, vf.preprocess(img))
    bi, s = args.block, sc[str(args.block)]
    x_f = tp["x_in"] if bi == 0 else tp[bi - 1]["x"]  # float block input
    sx = sc["x_in"] if bi == 0 else sc[str(bi - 1)]["x"]
    G, P = block_golden(W, bi, vq.q8(x_f, sx), sx, s, log=lambda m: print("[vit-block] " + m, flush=True))
    P["image"] = os.path.basename(img)

    # quality vs the float block (same image, float input of this block)
    ft = tp[bi]
    av = G["attn_o"].transpose(1, 0, 2).reshape(-1, 768)
    P["cos"] = {n: vf.cos(a * sa, f) for n, a, sa, f in (
        ("ln1", G["ln1"], s["ln1"], ft["ln1"]), ("attn_o", av, s["av"], ft["av"]), ("proj", G["proj"], s["proj"], ft["proj"]),
        ("x1", G["x1"], s["x1"], x_f + ft["proj"]), ("ln2", G["ln2"], s["ln2"], ft["ln2"]), ("g", G["g"], s["g"], ft["g"]),
        ("y", G["y"], s["x"], ft["x"]))}
    np.savez_compressed(os.path.join(out, "golden.npz"), **G)
    with open(os.path.join(out, "golden.json"), "w") as f:
        json.dump(P, f, indent=1, default=lambda o: o.tolist() if hasattr(o, "tolist") else int(o))
    print("[vit-block] block %d: cos vs float %s | %.0fs -> %s" % (
        bi, " ".join("%s %.4f" % kv for kv in P["cos"].items()), time.time() - t0, out))


if __name__ == "__main__":
    main()
