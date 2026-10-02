"""Golden for the GVU (generic_vector_unit.docx): LUT_RCE (§5.8, Figure 12) and SOFTMAX (§5.4, Figure 6).

Pure-Python integer model, written from the GVU drawings + has/HAS_IFACE_RCE.md; vectors are for a bit-exact SystemC check.
FUSED_ATTN (§5.5, Figure 7): --only attn. LAYERNORM (§5.6): --only ln.

LUT_RCE, log-scale mode (identical to has/gvu_lut.h and tools/fe/fe_ref_has_rce.py at default knobs, asserted at run time):
  E = 31 - clz(x); y = (x << (31 - E)) mod 2^32; i = y[30:23]; frac = y[22:15]
  recip: a, b = T[i], T[i+1]            rsqrt: a, b = T[p*257 + i], T[p*257 + i + 1], p = E[0]
  r = a + ((b - a) * frac >> 8)         E_shift = E (recip) | E >> 1 (rsqrt);  value = r * 2^-(Q + E_shift)
LUT_RCE, PWL mode (Figure 12 lower path; Base, W from the instruction; E_shift comes from the instruction, not the LUT):
  pwl_form "seg" (default, follows the drawing: Sub(E, Base) -> MSB, Fine bit extract (W bits) -> LSB, Concat -> index):
      seg = E - Base, sub = y[30:31-W], frac = y[30-W:23-W]; idx = seg << W | sub  (9-bit index, 2^(9-W) segments)
      E < Base -> idx 0, frac 0;  seg beyond the table -> last idx, frac 255 (saturate)
  pwl_form "uniform" (alternative reading): off = x - Base; idx = off >> W; frac = 8 bits below; saturate the same way
  r = a + ((b - a) * frac >> 8) as in log-scale mode.
SOFTMAX (per row, 32 lanes = 32 rows; the golden works one row at a time):
  stage 1: m = max_j x_j
  stage 2: d_j = m - x_j (knob H4) ; e_j = LUT_exp[d_j] (256 x 1 B, knob R6) ; sigma = sum_j e_j (int32)
  stage 3: (r, E) = LUT_recip(sigma) [log-scale]  |  r = LUT_recip_pwl(sigma), E = E_shift_instr [PWL]   (the mux)
           p_j = r * e_j (int16 x int8) ; t_j = RoundShift(p_j, E + Q - P) (knob R5) ; a_j = Requant(t_j, ATTN_Scale,
           ATTN_Shift, Z_ATTN)  (same Requant as has/gvu_quant.h: x*S -> round-shift -> sat16 -> +zp -> clamp int8)

Knobs (each one an open HW question; defaults marked *):
  H1 round        : *half_up | floor | half_away | half_even  (Round-Shift and Requant)
  H4 smx_sub      : *unsigned (m - x as uint8 0..255, never overflows) | sat (m - x saturated to int8 0..127)
  R2 tab_fmt      : *q14s (int16 Q14, as gvu_lut.h) | q15u (uint16 Q15)
  R3 interp_round : *floor (>> 8 as drawn) | half_up (+128 before >> 8)
  R4 zero_in      : *sat (x = 0 -> r = INT16_MAX (q14s) / UINT16_MAX (q15u), E_shift 0, as gvu_lut.h) | error
  R5 P            : *15 (probability t = p * 2^P before requant)
  R6 exp_fmt      : *u8 (LUT_exp[d] = round(255 e^-(d sx)), read unsigned) | s8 (round(127 e^-(d sx)), int8)

Usage: python tools/fe/fe_ref_gvu.py [--n-lut 40000] [--rows 1500] [--out-dir DIR]   -> DIR (default FE_WORK/has/vectors/gvu)
"""
import argparse
import json
import math
import os
import random
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))


DEFAULT_KNOBS = {"round": "half_up", "smx_sub": "unsigned", "tab_fmt": "q14s", "interp_round": "floor",
                 "zero_in": "sat", "P": 15, "exp_fmt": "u8", "pwl_form": "seg"}
QUESTION = {"round": "H1", "smx_sub": "H4", "tab_fmt": "R2", "interp_round": "R3", "zero_in": "R4", "P": "R5",
            "exp_fmt": "R6", "pwl_form": "R1 (PWL: meaning of Base / W)"}


def rnd(v):
    return int(math.floor(v + 0.5))


def q_of(k):
    return 14 if k["tab_fmt"] == "q14s" else 15


def rshift(p, s, mode):
    """Round-shift of an arbitrary Python int (gvu_quant.h rshift); s < 0 = left shift."""
    if s <= 0:
        return p << -s
    half = 1 << (s - 1)
    if mode == "floor":
        return p >> s
    if mode == "half_up":
        return (p + half) >> s
    if mode == "half_away":
        q = (abs(p) + half) >> s
        return -q if p < 0 else q
    if mode == "half_even":
        q = p >> s
        rem = p - (q << s)
        return q + 1 if rem > half or (rem == half and q & 1) else q
    raise ValueError(mode)


def requant(x, S, s, zp, k):
    r = rshift(x * S, s, k["round"])
    return max(-128, min(127, max(-32768, min(32767, r)) + zp))


# ---------------------------------------------------------------- LUT_RCE
def log_table(func, k):
    q = q_of(k)
    if func == "recip":
        T = [rnd(2.0 ** q / (1.0 + i / 256.0)) for i in range(257)]
    else:
        T = [rnd(2.0 ** q / math.sqrt((1.0 + i / 256.0) * 2.0 ** p)) for p in (0, 1) for i in range(257)]
    lim = 32767 if k["tab_fmt"] == "q14s" else 65535
    assert max(T) <= lim, "table does not fit %s" % k["tab_fmt"]
    return T


def interp(a, b, frac, k):
    d = b - a
    if not -256 <= d <= 255:
        raise OverflowError("table delta %d does not fit int9" % d)
    return a + ((d * frac + (128 if k["interp_round"] == "half_up" else 0)) >> 8)


def zero_result(k):
    if k["zero_in"] == "error":
        raise ZeroDivisionError("LUT input 0 (R4)")
    return (32767 if k["tab_fmt"] == "q14s" else 65535), 0


def lut_log(x, func, T, k):
    """Log-scale LUT_indirect: (r, E_shift)."""
    if x == 0:
        return zero_result(k)
    E = x.bit_length() - 1
    y = (x << (31 - E)) & 0xFFFFFFFF
    i, frac = (y >> 23) & 0xFF, (y >> 15) & 0xFF
    base = 0 if func == "recip" else (E & 1) * 257
    return interp(T[base + i], T[base + i + 1], frac, k), (E if func == "recip" else E >> 1)


def pwl_index(x, base, w, k, n_idx=512):
    """PWL index / fraction (see module doc). Returns (idx, frac); table must have n_idx + 1 entries."""
    if k["pwl_form"] == "seg":
        if x == 0:
            return 0, 0
        E = x.bit_length() - 1
        y = (x << (31 - E)) & 0xFFFFFFFF
        seg = E - base
        if seg < 0:
            return 0, 0
        idx = (seg << w) | ((y >> (31 - w)) & ((1 << w) - 1) if w else 0)
        frac = (y >> (23 - w)) & 0xFF
    else:
        off = x - base
        if off < 0:
            return 0, 0
        idx = off >> w
        frac = ((off >> (w - 8)) if w >= 8 else (off << (8 - w))) & 0xFF
    if idx >= n_idx:
        return n_idx - 1, 255
    return idx, frac


def pwl_recip_table(base, w, e_shift, k, n_idx=512):
    """Host-side PWL recip table: T[i] ~ 2^(Q + e_shift) / x at the segment start of index i (saturated to the format)."""
    q, lim = q_of(k), (32767 if k["tab_fmt"] == "q14s" else 65535)
    T = []
    for i in range(n_idx + 1):
        if k["pwl_form"] == "seg":
            seg, sub = i >> w, i & ((1 << w) - 1)
            x0 = 2.0 ** (seg + base) * (1.0 + sub / 2.0 ** w)
        else:
            x0 = base + i * 2.0 ** w
        T.append(min(lim, rnd(2.0 ** (q + e_shift) / max(x0, 1.0))))
    d = max(abs(b - a) for a, b in zip(T, T[1:]))
    if d > 255:  # the drawing interpolates with an int9 delta: the host must choose Base/W/E_shift so the table fits
        raise OverflowError("PWL table delta %d does not fit int9 (Base=%d W=%d E_shift=%d)" % (d, base, w, e_shift))
    return T


def lut_pwl(x, T, base, w, e_shift, k):
    if x == 0 and k["zero_in"] == "error":
        raise ZeroDivisionError("LUT input 0 (R4)")
    idx, frac = pwl_index(x, base, w, k, len(T) - 1)
    return interp(T[idx], T[idx + 1], frac, k), e_shift


# ---------------------------------------------------------------- SOFTMAX
def exp_table(sx, k):
    one = 255 if k["exp_fmt"] == "u8" else 127
    return [rnd(one * math.exp(-d * sx)) for d in range(256)]


def softmax_row(xs, k, cfg, zero=None):
    """Returns (m, sigma, r, E, [a_j]) for one row of int8 inputs; zero[j] forces e_j = 0 (FUSED_ATTN mask_e=zero)."""
    m = max(xs)
    ds = []
    for x in xs:
        d = m - x
        if k["smx_sub"] == "sat":
            d = min(d, 127)
        ds.append(d)
    es = [cfg["exp"][d] for d in ds]
    if zero is not None:
        es = [0 if z else e for e, z in zip(es, zero)]
    sigma = sum(es)
    if cfg["mode"] == "log":
        r, E = lut_log(sigma, "recip", cfg["recip"], k)
    else:
        r, E = lut_pwl(sigma, cfg["recip"], cfg["base"], cfg["w"], cfg["e_shift"], k)
    sh = E + q_of(k) - k["P"]
    out = [requant(rshift(r * e, sh, k["round"]), cfg["S"], cfg["s"], cfg["zp"], k) for e in es]
    return m, sigma, r, E, out


def attn_requant(k):
    """ATTN_Scale/Shift/Z so that a = t * 256 / 2^P - 128 (probability on the grid 1/256, zero point -128)."""
    return {"S": 1 << 30, "s": 30 + k["P"] - 8, "zp": -128}


def rows(rng, n):
    lens = [1, 2, 3, 7, 32, 49, 64, 197, 256]
    for i in range(n):
        L = lens[i % len(lens)] if i < 3 * len(lens) else rng.choice([rng.randint(1, 64), rng.randint(64, 256), 197])
        kind = i % 4
        if kind == 0:
            xs = [rng.randint(-128, 127) for _ in range(L)]
        elif kind == 1:
            c = rng.randint(-100, 100)
            xs = [max(-128, min(127, c + int(rng.gauss(0, 12)))) for _ in range(L)]
        elif kind == 2:
            xs = [rng.choice([-128, 127]) for _ in range(L)]
        else:
            xs = [rng.randint(-128, -100) for _ in range(L)]
            xs[rng.randrange(L)] = 127
        yield xs


# ---------------------------------------------------------------- vector writers
def lut_inputs(rng, n):
    xs = list(range(0, 2049))
    for kk in range(32):
        for d in (-1, 0, 1):
            if 0 < (1 << kk) + d < (1 << 32):
                xs.append((1 << kk) + d)
    xs += [(1 << 32) - 1, 0x80008000, 0x7F800000, 0x00FF8001]
    while len(xs) < n:
        xs.append(int(2.0 ** rng.uniform(0, 32)) & 0xFFFFFFFF)
    return xs


def write_lut(out, k, n, seed, meta):
    for func in ("recip", "rsqrt"):
        T = log_table(func, k)
        name = "lut_log_%s.txt" % func
        worst = 0.0
        with open(os.path.join(out, name), "w") as f:
            f.write("# LUT_RCE log-scale %s, knobs in gvu_knobs.json; table in gvu_tables.json\n# x r E_shift\n" % func)
            for x in lut_inputs(random.Random("%d/log/%s" % (seed, func)), n):
                r, e = lut_log(x, func, T, k)
                f.write("%d %d %d\n" % (x, r, e))
                if x:
                    ex = 1.0 / x if func == "recip" else 1.0 / math.sqrt(x)
                    worst = max(worst, abs(r * 2.0 ** -(q_of(k) + e) - ex) / ex)
        meta["tables"]["log_" + func] = T
        meta["files"][name] = {"max_rel_err": worst}
    for form, base, w, es in (("seg", 7, 6, 7), ("uniform", 1024, 3, 10)):
        kf = dict(k, pwl_form=form)
        T = pwl_recip_table(base, w, es, kf)
        name = "lut_pwl_%s_recip.txt" % form
        with open(os.path.join(out, name), "w") as f:
            f.write("# LUT_RCE PWL recip, pwl_form=%s Base=%d W=%d E_shift(instr)=%d; table gvu_tables.json\n"
                    "# x idx frac r E_shift\n" % (form, base, w, es))
            for x in lut_inputs(random.Random("%d/pwl/%s" % (seed, form)), n):
                idx, frac = pwl_index(x, base, w, kf, len(T) - 1)
                r, e = lut_pwl(x, T, base, w, es, kf)
                f.write("%d %d %d %d %d\n" % (x, idx, frac, r, e))
        meta["tables"]["pwl_%s_recip" % form] = T
        meta["files"][name] = {"pwl_form": form, "Base": base, "W": w, "E_shift": es}


def write_softmax(out, k, n_rows, seed, meta):
    rq = attn_requant(k)
    for mode in ("log", "pwl"):
        name = "softmax_%s.txt" % mode
        groups = []
        for sx in (1 / 16.0, 1 / 8.0, 1 / 4.0):
            cfg = dict(rq, mode=mode, exp=exp_table(sx, k), sx=sx)
            if mode == "log":
                cfg["recip"] = log_table("recip", k)
            else:  # sigma 2^7..2^15: 8 octaves x 64 steps (9-bit index), E_shift 7; wider range breaks the int9 delta
                cfg.update(base=7, w=6, e_shift=7)
                cfg["recip"] = pwl_recip_table(7, 6, 7, dict(k, pwl_form="seg"))
            groups.append(cfg)
        kk = dict(k, pwl_form="seg")
        n_el, worst, sum_err = 0, 0.0, 0.0
        with open(os.path.join(out, name), "w") as f:
            f.write("# SOFTMAX %s; groups (exp table, recip table, requant) in gvu_tables.json / gvu_knobs.json\n"
                    "# group L | x_0..x_L-1 | m sigma r E | a_0..a_L-1\n" % mode)
            for i, xs in enumerate(rows(random.Random("%d/smx/%s" % (seed, mode)), n_rows)):
                g = i % len(groups)
                m, sg, r, E, a = softmax_row(xs, kk, groups[g])
                f.write("%d %d | %s | %d %d %d %d | %s\n" % (g, len(xs), " ".join(map(str, xs)), m, sg, r, E,
                                                             " ".join(map(str, a))))
                ref = [math.exp((x - m) * groups[g]["sx"]) for x in xs]
                tot = sum(ref)
                for aj, pj in zip(a, ref):
                    err = abs((aj + 128) / 256.0 - pj / tot)
                    worst, sum_err, n_el = max(worst, err), sum_err + err, n_el + 1
        for gi, cfg in enumerate(groups):
            meta["tables"]["softmax_%s_g%d_exp" % (mode, gi)] = cfg["exp"]
            if mode == "pwl":
                meta["tables"]["softmax_pwl_g%d_recip" % gi] = cfg["recip"]
        meta["files"][name] = {
            "rows": n_rows, "elements": n_el,
            "groups": [{"sx": c["sx"], "recip": ("log_recip" if mode == "log" else
                                                   {"pwl_form": "seg", "Base": c["base"], "W": c["w"],
                                                    "E_shift": c["e_shift"]})} for c in groups],
            "ATTN_Scale": rq["S"], "ATTN_Shift": rq["s"], "Z_ATTN": rq["zp"],
            "vs_float_prob_err": {"max": worst, "mean": sum_err / max(n_el, 1), "grid": "1/256"}}


# ---------------------------------------------------------------- FUSED_ATTN (§5.5, Figure 7)
# Phase 1: acc = Q.K^T + Qrow_i * (-Zk) + Kcol_j * (-Zq) + D*Zq*Zk ; x = Requant(acc, Mqk, TS_qk, Zqk) (1/sqrt(d) inside Mqk)
#          masked (mask rule) -> x = -128 ; row_max running over OC tiles (= max of the finished row)
# Phase 2: SOFTMAX stages 2-3 = softmax_row() above, exp table built from the scale of x
# Phase 3: acc = A.V + Vcol_n * (-Za) [+ Arow_i * (-Zv) + L*Za*Zv only with av_zv_corr: NOT in the drawing]
#          out = Requant(acc, Mav, TS_av, Zav)
# Qrow / Kcol / Vcol are v32int16 accumulators in the drawing: overflow is counted, not modelled (d, L <= 256 never do).
ATTN_KNOBS = {"asym_zp": True, "mask_rule": "eq_neg128", "inv_sqrt_d": "in_Mqk", "av_zv_corr": False,
              "mask_e": "drawing"}
ATTN_QUESTION = {"asym_zp": "Figure 7 zero-point path (always computed; Z = 0 when the frontend is symmetric)",
                 "mask_rule": "which mask value masks (eq_neg128: mask == -128 | nonzero)",
                 "inv_sqrt_d": "1/sqrt(d) folded into Mqk (H7, drawing has no separate multiplier)",
                 "mask_e": "drawing: masked x = -128 still gets e = LUT_exp[m + 128] > 0 | zero: masked e forced to 0 "
                           "(proposal; needs a mask bit carried to stage 2)",
                 "av_zv_corr": "phase 3 corrects only Za (V col-sum x -Za); Zv / L*Za*Zv terms absent in the drawing"}


def req_params(m):
    """Real multiplier m > 0 -> (S int32 in [2^30, 2^31), s)  (same rule as fe_vit_quant.req_params)."""
    e = math.floor(math.log2(m))
    s = 30 - e
    S = rnd(m * 2.0 ** s)
    if S >= 1 << 31:
        S, s = S >> 1, s - 1
    if not 0 <= s <= 63:
        raise ValueError("requant shift %d out of range" % s)
    return S, s


def matmul_t(A, B):
    """A [n][d] . B[m][d]^T -> [n][m] exact ints."""
    return [[sum(a * b for a, b in zip(ra, rb)) for rb in B] for ra in A]


def fused_attn(h, k, ak):
    """h: head dict (Q, K, V int8 codes, zero points, requant params, mask). Returns intermediates + output."""
    Q, K, V, D, L = h["Q"], h["K"], h["V"], len(h["Q"][0]), len(h["K"])
    masked = [[((mv == -128) if ak["mask_rule"] == "eq_neg128" else (mv != 0)) for mv in r] for r in h["mask"]]
    Zq, Zk, Zv = (h["Zq"], h["Zk"], h["Zv"]) if ak["asym_zp"] else (0, 0, 0)
    ovf16 = 0
    qrow = [sum(r) for r in Q]
    kcol = [sum(r) for r in K]
    ovf16 += sum(abs(v) > 32767 for v in qrow + kcol)
    S = matmul_t(Q, K)
    X = []
    for i, row in enumerate(S):
        xr = []
        for j, acc in enumerate(row):
            acc = acc - Zk * qrow[i] - Zq * kcol[j] + D * Zq * Zk
            x = requant(acc, h["Mqk"], h["TSqk"], h["Zqk"], k)
            if masked[i][j]:
                x = -128
            xr.append(x)
        X.append(xr)
    cfg = dict(attn_requant(k), mode="log", exp=exp_table(h["sx"], k), recip=log_table("recip", k))
    A, stats = [], []
    for xr, mr in zip(X, masked):
        m, sg, r, E, a = softmax_row(xr, k, cfg, mr if ak["mask_e"] == "zero" else None)
        A.append(a)
        stats.append((m, sg, r, E))
    Za = cfg["zp"]
    Vt = [[V[j][n] for j in range(L)] for n in range(D)]
    vcol = [sum(c) for c in Vt]
    arow = [sum(r) for r in A]
    ovf16 += sum(abs(v) > 32767 for v in vcol)
    O = []
    for i, prow in enumerate(matmul_t(A, Vt)):
        orow = []
        for n, acc in enumerate(prow):
            acc -= Za * vcol[n]
            if ak["av_zv_corr"]:
                acc += -Zv * arow[i] + L * Za * Zv
            orow.append(requant(acc, h["Mav"], h["TSav"], h["Zav"], k))
        O.append(orow)
    return {"X": X, "A": A, "O": O, "stats": stats, "exp": cfg["exp"], "ovf16": ovf16}


def qz(x, s, z):
    return max(-128, min(127, rnd(x / s) + z))


def make_head(name, qf, kf, vf_, mask_f, s_out=None, s_av=None, zq=0, zk=0, zv=0):
    """Float Q/K/V -> quantised head dict with calibrated per-tensor scales (max / 127, zero points given)."""
    D = len(qf[0])
    mx = lambda M: max(abs(v) for r in M for v in r) or 1.0
    sq, sk, sv = mx(qf) / 127.0, mx(kf) / 127.0, mx(vf_) / 127.0
    Sf = [[sum(a * b for a, b in zip(r, c)) / math.sqrt(D) for c in kf] for r in qf]
    s_s = s_out or mx(Sf) / 127.0
    Af = [softmax_f(r, mr) for r, mr in zip(Sf, mask_f)]
    Of = [[sum(Af[i][j] * vf_[j][n] for j in range(len(vf_))) for n in range(D)] for i in range(len(qf))]
    s_o = s_av or mx(Of) / 127.0
    Mqk, TSqk = req_params(sq * sk / math.sqrt(D) / s_s)
    Mav, TSav = req_params((1.0 / 256) * sv / s_o)
    h = {"name": name, "Q": [[qz(v, sq, zq) for v in r] for r in qf], "K": [[qz(v, sk, zk) for v in r] for r in kf],
         "V": [[qz(v, sv, zv) for v in r] for r in vf_], "Zq": zq, "Zk": zk, "Zv": zv, "Zqk": 0, "Zav": 0,
         "Mqk": Mqk, "TSqk": TSqk, "Mav": Mav, "TSav": TSav, "sx": s_s, "s_out": s_o,
         "sq": sq, "sk": sk, "sv": sv,
         "mask": [[-128 if mm else 0 for mm in r] for r in mask_f], "float_out": Of}
    return h


def softmax_f(row, mrow):
    m = max(v for v, mm in zip(row, mrow) if not mm)
    e = [0.0 if mm else math.exp(v - m) for v, mm in zip(row, mrow)]
    t = sum(e)
    return [v / t for v in e]


def dequant_ref(h):
    """Float attention on the dequantised int8 Q/K/V (isolates the GVU integer path from input quantisation)."""
    dq = lambda M, s, z: [[(v - z) * s for v in r] for r in M]
    Q, K, V = dq(h["Q"], h["sq"], h["Zq"]), dq(h["K"], h["sk"], h["Zk"]), dq(h["V"], h["sv"], h["Zv"])
    D = len(Q[0])
    mrow = [[mv == -128 for mv in r] for r in h["mask"]]
    Sf = [[sum(a * b for a, b in zip(r, c)) / math.sqrt(D) for c in K] for r in Q]
    Af = [softmax_f(r, m) for r, m in zip(Sf, mrow)]
    return [[sum(Af[i][j] * V[j][n] for j in range(len(V))) for n in range(D)] for i in range(len(Q))]


def cmp(o_int, h, ref):
    a = [(v - h["Zav"]) * h["s_out"] for r in o_int for v in r]
    b = [v for r in ref for v in r]
    dot = sum(x * y for x, y in zip(a, b))
    na, nb = math.sqrt(sum(x * x for x in a)), math.sqrt(sum(y * y for y in b))
    return {"cos": dot / (na * nb) if na and nb else 0.0, "max_abs_err_lsb": max(abs(x - y) for x, y in zip(a, b)) / h["s_out"]}


def random_heads(seed):
    rng = random.Random("%d/attn" % seed)
    g = lambda n, d, s: [[rng.gauss(0, s) for _ in range(d)] for _ in range(n)]
    heads = []
    for name, L, D, mask, zp in (("r32", 32, 64, "none", 0), ("r64_causal", 64, 64, "causal", 0),
                                 ("r197", 197, 64, "none", 0), ("r197_pad", 197, 64, "pad20", 0),
                                 ("r64_asym", 64, 64, "none", 1), ("r197_asym_causal", 197, 64, "causal", 1)):
        qf, kf, vf_ = g(L, D, 1.0), g(L, D, 1.0), g(L, D, 1.0)
        if zp:  # shift to make asymmetric ranges meaningful
            qf = [[v + 0.8 for v in r] for r in qf]
            kf = [[v - 0.5 for v in r] for r in kf]
        mask_f = [[(mask == "causal" and j > i) or (mask == "pad20" and j >= L - 20) for j in range(L)] for i in range(L)]
        heads.append(make_head(name, qf, kf, vf_, mask_f, zq=(-20 if zp else 0), zk=(15 if zp else 0), zv=0))
    return heads


def vit_heads(blocks=((0, 0), (6, 3), (11, 7))):
    """Real heads from ViT-B/16 (timm weights + calibrated scales vit_scales.json), first eval image; VM only."""
    import glob
    import fe_common as fc
    import fe_vit_float as vf
    import fe_vit_quant as vq
    W = vf.load_safetensors(vf.WEIGHTS)
    with open(os.path.join(fc.FE_WORK, "has", "vit", "vit_scales.json")) as f:
        sc = json.load(f)
    imgs = sorted(glob.glob(os.path.join(fc.FE_WORK, "datasets", "coco128", "images", "train2017", "*.jpg")))
    img = imgs[16]  # first image after the 16 calibration images
    tp = vq.forward_taps(W, vf.preprocess(img))
    heads = []
    for b, hd in blocks:
        s = sc[str(b)]
        q, kk, v = (tp[b][n][hd].tolist() for n in ("q", "k", "v"))
        L = len(q)
        h = make_head("vit_b%d_h%d" % (b, hd), q, kk, v, [[False] * L for _ in range(L)], s_out=s["s"], s_av=s["av"])
        h["image"] = os.path.basename(img)
        heads.append(h)
    return heads


def write_attn(out, k, ak, seed, with_vit):
    heads = random_heads(seed) + (vit_heads() if with_vit else [])
    meta = {"knobs": k, "attn_knobs": ak, "open_question": dict(QUESTION, **ATTN_QUESTION), "heads": {}}
    exp_tabs = {}
    for h in heads:
        r = fused_attn(h, k, ak)
        L, D = len(h["Q"]), len(h["Q"][0])
        path = os.path.join(out, "attn_%s.txt" % h["name"])
        with open(path, "w") as f:
            f.write("# FUSED_ATTN head %s L=%d d=%d; params in attn_knobs.json, exp table in attn_tables.json\n"
                    "# sections: Q K V (L x d int8) | M mask (L x L) | X phase-1 x (L x L) | S row stats m sigma r E |"
                    " A attention (L x L) | O output (L x d)\n" % (h["name"], L, D))
            for tag, M in (("Q", h["Q"]), ("K", h["K"]), ("V", h["V"]), ("M", h["mask"]), ("X", r["X"]),
                           ("S", r["stats"]), ("A", r["A"]), ("O", r["O"])):
                for row in M:
                    f.write("%s %s\n" % (tag, " ".join(map(str, row))))
        c_deq = cmp(r["O"], h, dequant_ref(h))
        c_flt = cmp(r["O"], h, h["float_out"])
        c_alt = None
        if any(v == -128 for row in h["mask"] for v in row):  # diagnostic: the other mask_e reading, not written out
            alt = dict(ak, mask_e="zero" if ak["mask_e"] == "drawing" else "drawing")
            c_alt = dict(cmp(fused_attn(h, k, alt)["O"], h, dequant_ref(h)), mask_e=alt["mask_e"])
        exp_tabs[h["name"]] = r["exp"]
        info = {key: h[key] for key in ("Zq", "Zk", "Zv", "Zqk", "Mqk", "TSqk", "Mav", "TSav", "Zav", "sx", "s_out",
                                         "sq", "sk", "sv")}
        info.update(L=L, d=D, ATTN=attn_requant(k), ovf16=r["ovf16"], vs_float_dequant_inputs=c_deq,
                    vs_float_inputs=c_flt, other_mask_e_vs_float_dequant=c_alt, image=h.get("image"))
        meta["heads"][h["name"]] = info
        print("[gvu-attn] %-18s L=%3d cos(deq) %.5f maxerr %.1f LSB | cos(float) %.5f maxerr %.1f LSB | ovf16 %d%s"
              % (h["name"], L, c_deq["cos"], c_deq["max_abs_err_lsb"], c_flt["cos"], c_flt["max_abs_err_lsb"],
                 r["ovf16"], " | mask_e=%s cos %.5f" % (c_alt["mask_e"], c_alt["cos"]) if c_alt else ""))
    with open(os.path.join(out, "attn_tables.json"), "w") as f:
        json.dump(exp_tabs, f)
    with open(os.path.join(out, "attn_knobs.json"), "w") as f:
        json.dump(meta, f, indent=1)


# ---------------------------------------------------------------- LAYERNORM (§5.6, Figure 8)
# Stage 1: S = sum_k x_k (int32)
# Stage 2: D_k = H*x_k - S (int32, stored) ; V = sum D_k^2 (int64) ; v = Requant(V >> Pre_Shift, M0_var, TS_var, Z_var) int8
# Stage 3: f = clamp8(RoundShift((v - Z_var) * M0_7 + E_bias, TS_7) + Z_7) ; w = f - Z_7 if f > Z_7 else Z_out3  (R8)
# Stage 4: (r, E) = LUT_rsqrt(w) ; p = r * D_k (int48, split MSB/LSB, R9) ; n = clamp8(RoundShift(p * M0_div, TS_div + E) + Z_div)
# Stage 5: t = Requant((n - Z_div) * gamma_k, M0_mul, TS_mul, Z_mul) ; y = clamp(t - Z_mul + beta_k + Z_out)
# Host-side parameter choice (the "compiler" part, not HW): var on a per-tensor grid s_v with Z_var = -128, var + eps on
# s_w with Z_7 = -128, n on s_n = n_range / 127, output scale s_out (per tensor, per channel, or int16 by knob out_fmt).
LN_KNOBS = {"r8_floor": "z_out", "r8_z_out": 1, "r9_split": "exact", "r9_lsb_bits": 32, "out_fmt": "int8_tensor",
            "eps": 1e-6, "pre_shift": "fit_int32_worst", "n_range": 8.0}
LN_QUESTION = {"r8_floor": "R8 stage 3 comparator: f <= Z_7 -> write Z_out3 (z_out) | max(f - Z_7, 1) (max1)",
               "r9_split": "R9 stage 4 split of r*D: exact (MSB and LSB partial products both kept) | msb_only (LSB dropped)",
               "out_fmt": "H13 LN output: int8_tensor (drawing) | int8_channel | int16 (stage-5 clamps widened)",
               "eps": "epsilon (float, folded into E_bias on the s_w grid)",
               "pre_shift": "fit_int32_worst (smallest shift with H*(255H)^2 >> P < 2^31) | calib (fit the calibration max)",
               "n_range": "|normalised value| range mapped to int8 at stage 4 (s_n = n_range / 127)"}


def requant_r(x, S, s, zp, k, lo=-128, hi=127):
    r = rshift(x * S, s, k["round"])
    return max(lo, min(hi, r + zp))


def ln_params(rows_q, sx, gamma, beta, s_out, lk):
    """Host: per-tensor parameters for a set of int8 rows (per-tensor calibration on these rows)."""
    H = len(rows_q[0])
    Vs = []
    for x in rows_q:
        S = sum(x)
        Vs.append(sum((H * v - S) ** 2 for v in x))
    if lk["pre_shift"] == "fit_int32_worst":
        P = max(0, (H * (255 * H) ** 2).bit_length() - 31)
    else:
        P = max(0, max(Vs).bit_length() - 31)
    var_max = max(Vs) * sx * sx / H ** 3
    s_v = max(var_max, 1e-12) / 255.0
    M0_var, TS_var = req_params(2.0 ** P * sx * sx / H ** 3 / s_v)
    s_w = (var_max + lk["eps"]) / 255.0
    M0_7, TS_7 = req_params(s_v / s_w)
    E_bias = rnd(lk["eps"] / s_w * 2.0 ** TS_7)
    s_n = lk["n_range"] / 127.0
    Q = 14
    M0_div, TS_div = req_params(sx / (H * math.sqrt(s_w) * s_n) * 2.0 ** -Q)
    s_g = max(max(abs(g) for g in gamma), 1e-12) / 127.0
    gq = [max(-127, min(127, rnd(g / s_g))) for g in gamma]
    so = list(s_out) if isinstance(s_out, (list, tuple)) else [s_out] * H
    mul = [req_params(s_n * s_g / o) for o in so]
    bq = [max(-(1 << 31), min((1 << 31) - 1, rnd(b / o))) for b, o in zip(beta, so)]
    return {"H": H, "Pre_Shift": P, "M0_var": M0_var, "TS_var": TS_var, "Z_var": -128, "M0_7": M0_7, "TS_7": TS_7,
            "E_bias": E_bias, "Z_7": -128, "M0_div": M0_div, "TS_div": TS_div, "Z_div": 0,
            "M0_mul": [m[0] for m in mul], "TS_mul": [m[1] for m in mul], "Z_mul": 0, "Z_out": 0,
            "gamma_q": gq, "beta_q": bq, "sx": sx, "s_v": s_v, "s_w": s_w, "s_n": s_n, "s_g": s_g, "s_out": so}


def layernorm_row(x, p, k, lk, cnt):
    H = p["H"]
    S = sum(x)
    D = [H * v - S for v in x]
    V = sum(d * d for d in D)
    cnt["d_int32"] += sum(abs(d) >= 1 << 31 for d in D)
    cnt["v_int64"] += V >= 1 << 63
    Vs = V >> p["Pre_Shift"]
    cnt["vshift_int32"] += Vs >= 1 << 31
    v = requant_r(Vs, p["M0_var"], p["TS_var"], p["Z_var"], k)
    u = rshift((v - p["Z_var"]) * p["M0_7"] + p["E_bias"], p["TS_7"], k["round"])
    cnt["s3_int16"] += abs(u) > 32767
    f = max(-128, min(127, u + p["Z_7"]))
    if f > p["Z_7"]:
        w = f - p["Z_7"]
    else:
        w = lk["r8_z_out"] if lk["r8_floor"] == "z_out" else 1
        cnt["s3_floor"] += 1
    r, E = lut_log(w, "rsqrt", log_table("rsqrt", k), k)
    lo8, hi8 = (-32768, 32767) if lk["out_fmt"] == "int16" else (-128, 127)
    n = []
    for d in D:
        prod = r * d
        cnt["p_int48"] += abs(prod) >= 1 << 47
        if lk["r9_split"] == "exact":
            acc = prod * p["M0_div"]
        else:  # keep only the MSB part (bits above r9_lsb_bits) times M0_div
            acc = ((prod >> lk["r9_lsb_bits"]) * p["M0_div"]) << lk["r9_lsb_bits"]
        n.append(max(-128, min(127, rshift(acc, p["TS_div"] + E, k["round"]) + p["Z_div"])))
    y = []
    for j, nv in enumerate(n):
        t = requant_r((nv - p["Z_div"]) * p["gamma_q"][j], p["M0_mul"][j], p["TS_mul"][j], p["Z_mul"], k, lo8, hi8)
        cnt["s5_sat"] += t in (lo8, hi8)
        y.append(max(lo8, min(hi8, t - p["Z_mul"] + p["beta_q"][j] + p["Z_out"])))
    return {"S": S, "V": V, "v": v, "f": f, "w": w, "r": r, "E": E, "n": n, "y": y}


def ln_float(x, gamma, beta, eps):
    H = len(x)
    m = sum(x) / H
    var = sum((v - m) ** 2 for v in x) / H
    return [(v - m) / math.sqrt(var + eps) * g + b for v, g, b in zip(x, gamma, beta)]


def run_ln_case(name, xf_rows, sx, gamma, beta, s_out_f, k, lk, out):
    """xf_rows: float inputs (quantised here with sx, symmetric). s_out_f: output scale (float) of LN."""
    xq = [[max(-128, min(127, rnd(v / sx))) for v in r] for r in xf_rows]
    H = len(xq[0])
    if lk["out_fmt"] == "int8_channel":
        ref_all = [ln_float([v * sx for v in r], gamma, beta, lk["eps"]) for r in xq]
        s_out = [max(max(abs(r[j]) for r in ref_all), 1e-12) / 127.0 for j in range(H)]
    elif lk["out_fmt"] == "int16":
        s_out = s_out_f * 127.0 / 32767.0
    else:
        s_out = s_out_f
    p = ln_params(xq, sx, gamma, beta, s_out, lk)
    cnt = dict.fromkeys(("d_int32", "v_int64", "vshift_int32", "s3_int16", "s3_floor", "p_int48", "s5_sat"), 0)
    res = [layernorm_row(x, p, k, lk, cnt) for x in xq]
    so = p["s_out"]
    num = den_a = den_b = 0.0
    err = err_f = 0.0
    for x, xr, rr in zip(xq, xf_rows, res):
        ref = ln_float([v * sx for v in x], gamma, beta, lk["eps"])
        reff = ln_float(xr, gamma, beta, lk["eps"])
        for j, yv in enumerate(rr["y"]):
            a = (yv - p["Z_out"]) * so[j]
            num += a * ref[j]
            den_a += a * a
            den_b += ref[j] * ref[j]
            err = max(err, abs(a - ref[j]) / so[j])
            err_f = max(err_f, abs(a - reff[j]) / so[j])
    cos_v = num / math.sqrt(den_a * den_b) if den_a and den_b else 0.0
    path = os.path.join(out, "ln_%s.txt" % name)
    with open(path, "w") as f:
        f.write("# LAYERNORM %s rows=%d H=%d; params (gamma_q, beta_q, M0/TS, ...) in ln_knobs.json\n"
                "# per row: X x_0..x_H-1 | R S V v f w r E | N n_0..n_H-1 | Y y_0..y_H-1\n" % (name, len(xq), H))
        for x, rr in zip(xq, res):
            f.write("X %s\n" % " ".join(map(str, x)))
            f.write("R %d %d %d %d %d %d %d\n" % (rr["S"], rr["V"], rr["v"], rr["f"], rr["w"], rr["r"], rr["E"]))
            f.write("N %s\n" % " ".join(map(str, rr["n"])))
            f.write("Y %s\n" % " ".join(map(str, rr["y"])))
    pj = {kk: vv for kk, vv in p.items() if kk not in ("s_out",)}
    pj["s_out"] = so if lk["out_fmt"] == "int8_channel" else so[0]
    if lk["out_fmt"] != "int8_channel":
        pj["M0_mul"], pj["TS_mul"] = pj["M0_mul"][0], pj["TS_mul"][0]
    info = {"rows": len(xq), "H": H, "params": pj, "counters": cnt,
            "vs_float_dequant_inputs": {"cos": cos_v, "max_abs_err_lsb": err}, "vs_float_inputs_max_err_lsb": err_f}
    print("[gvu-ln] %-14s rows=%3d H=%3d cos %.5f maxerr %.1f LSB | floor %d s5_sat %d | ovf d32 %d v64 %d vs32 %d s3_16 %d p48 %d"
          % (name, len(xq), H, cos_v, err, cnt["s3_floor"], cnt["s5_sat"], cnt["d_int32"], cnt["v_int64"],
             cnt["vshift_int32"], cnt["s3_int16"], cnt["p_int48"]))
    return info


def random_ln_cases(seed):
    rng = random.Random("%d/ln" % seed)
    cases = []
    for H in (64, 768):
        rows = []
        for i in range(32):
            kind = i % 4
            mu, sd = rng.uniform(-1, 1), (rng.uniform(0.2, 2.0) if kind != 1 else rng.uniform(0.005, 0.05))
            r = [rng.gauss(mu, sd) for _ in range(H)]
            if kind == 2:  # a few outlier channels, as ViT residuals have
                for j in rng.sample(range(H), max(1, H // 64)):
                    r[j] *= 20
            if kind == 3 and i == 3:
                r = [mu] * H  # constant row: var = 0 -> eps / floor path
            rows.append(r)
        sx = max(abs(v) for r in rows for v in r) / 127.0
        gamma = [rng.uniform(0.5, 1.5) * rng.choice([1, 1, 1, -1]) for _ in range(H)]
        beta = [rng.gauss(0, 0.3) for _ in range(H)]
        cases.append(("r_h%d" % H, rows, sx, gamma, beta, 6.0 / 127.0))
    return cases


def vit_ln_cases(blocks=(0, 6, 11), n_rows=64):
    """norm1 of ViT-B blocks: input = residual stream at block entry quantised with the calibrated scale, output scale =
    calibrated ln1 scale. First eval image; first n_rows tokens (keeps the pure-Python run short)."""
    import glob
    import fe_common as fc
    import fe_vit_float as vf
    import fe_vit_quant as vq
    W = vf.load_safetensors(vf.WEIGHTS)
    with open(os.path.join(fc.FE_WORK, "has", "vit", "vit_scales.json")) as f:
        sc = json.load(f)
    imgs = sorted(glob.glob(os.path.join(fc.FE_WORK, "datasets", "coco128", "images", "train2017", "*.jpg")))
    tp = vq.forward_taps(W, vf.preprocess(imgs[16]))
    cases = []
    for b in blocks:
        xin = tp["x_in"] if b == 0 else tp[b - 1]["x"]
        sx = sc["x_in"] if b == 0 else sc[str(b - 1)]["x"]
        g, bt = W["blocks.%d.norm1.weight" % b].tolist(), W["blocks.%d.norm1.bias" % b].tolist()
        cases.append(("vit_b%d_ln1" % b, xin[:n_rows].tolist(), sx, g, bt, sc[str(b)]["ln1"]))
    return cases


def write_ln(out, k, lk, seed, with_vit):
    cases = random_ln_cases(seed) + (vit_ln_cases() if with_vit else [])
    meta = {"knobs": k, "ln_knobs": lk, "open_question": dict(QUESTION, **LN_QUESTION), "cases": {}}
    for name, rows, sx, g, b, so in cases:
        meta["cases"][name] = run_ln_case(name, rows, sx, g, b, so, k, lk, out)
    # H8: Scratchpad bytes for D[i] (int32) per row-parallel group, and the recompute alternative (keep x int8 + S int32)
    meta["h8_scratchpad"] = {
        "D_int32_bytes": {"%d_rows_H%d" % (r, H): r * H * 4 for r in (8, 16, 32) for H in (768, 1024)},
        "recompute_D_bytes": {"%d_rows_H%d" % (r, H): r * H + r * 4 for r in (8, 16, 32) for H in (768, 1024)},
        "note": "D_k = H*x_k - S is one int16 x int8 multiply + one int32 subtract (the stage-2 datapath); keeping x "
                "int8 + S per row and recomputing D in stage 4 needs 32 x 768 + 128 = 24.7 KB instead of 96 KB",
    }
    with open(os.path.join(out, "ln_knobs.json"), "w") as f:
        json.dump(meta, f, indent=1)


def self_check(k):
    """Default log-scale path must equal fe_ref_has_rce (validated against has/gvu_lut.h)."""
    import fe_ref_has_rce as rce  # imports fe_common (torch) -> VM only
    if k["tab_fmt"] != "q14s" or k["interp_round"] != "floor":
        return "skipped (non-default table/interp knobs)"
    rng = random.Random(7)
    for func in ("recip", "rsqrt"):
        T = log_table(func, k)
        assert T == rce.table(func)
        for x in lut_inputs(rng, 20000)[1:]:
            assert lut_log(x, func, T, k) == rce.lut_indirect(x, func, T), (func, x)
    return "ok (log-scale == fe_ref_has_rce on 2 x 20000 inputs)"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--n-lut", type=int, default=40000)
    ap.add_argument("--rows", type=int, default=1500)
    ap.add_argument("--seed", type=int, default=1)
    ap.add_argument("--out-dir", default=None)
    ap.add_argument("--only", choices=["lut_smx", "attn", "ln"], default="lut_smx")
    ap.add_argument("--r8-floor", default=LN_KNOBS["r8_floor"], choices=["z_out", "max1"])
    ap.add_argument("--r9-split", default=LN_KNOBS["r9_split"], choices=["exact", "msb_only"])
    ap.add_argument("--out-fmt", default=LN_KNOBS["out_fmt"], choices=["int8_tensor", "int8_channel", "int16"])
    ap.add_argument("--eps", type=float, default=LN_KNOBS["eps"])
    ap.add_argument("--pre-shift", default=LN_KNOBS["pre_shift"], choices=["fit_int32_worst", "calib"])
    ap.add_argument("--no-vit", action="store_true", help="attn: skip the real ViT-B heads (needs weights, VM)")
    ap.add_argument("--mask-rule", default=ATTN_KNOBS["mask_rule"], choices=["eq_neg128", "nonzero"])
    ap.add_argument("--av-zv-corr", action="store_true")
    ap.add_argument("--mask-e", default=ATTN_KNOBS["mask_e"], choices=["drawing", "zero"])
    for key, val in DEFAULT_KNOBS.items():
        ap.add_argument("--" + key.replace("_", "-"), dest=key, type=type(val), default=val)
    args = ap.parse_args()
    if args.out_dir is None:
        import fe_common as fc
        args.out_dir = os.path.join(fc.FE_WORK, "has", "vectors", "gvu")
    k = {key: getattr(args, key) for key in DEFAULT_KNOBS}
    os.makedirs(args.out_dir, exist_ok=True)
    if args.only == "attn":
        ak = dict(ATTN_KNOBS, mask_rule=args.mask_rule, av_zv_corr=args.av_zv_corr, mask_e=args.mask_e)
        write_attn(args.out_dir, k, ak, args.seed, not args.no_vit)
        return
    if args.only == "ln":
        lk = dict(LN_KNOBS, r8_floor=args.r8_floor, r9_split=args.r9_split, out_fmt=args.out_fmt, eps=args.eps,
                  pre_shift=args.pre_shift)
        write_ln(args.out_dir, k, lk, args.seed, not args.no_vit)
        return
    meta = {"knobs": k, "open_question": QUESTION, "tables": {}, "files": {}, "seed": args.seed,
            "self_check": self_check(k)}
    write_lut(args.out_dir, k, args.n_lut, args.seed, meta)
    write_softmax(args.out_dir, k, args.rows, args.seed, meta)
    tables = meta.pop("tables")
    with open(os.path.join(args.out_dir, "gvu_tables.json"), "w") as f:
        json.dump(tables, f)
    with open(os.path.join(args.out_dir, "gvu_knobs.json"), "w") as f:
        json.dump(meta, f, indent=1)
    print("[gvu] self_check:", meta["self_check"])
    for name, info in meta["files"].items():
        print("[gvu] %-26s %s" % (name, json.dumps(info.get("vs_float_prob_err", info.get("max_rel_err", "")))))


if __name__ == "__main__":
    main()
