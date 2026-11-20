#!/usr/bin/env python3
"""GEMM_FUSED layer driven by the CPU (HAS_CPU_DIRECT=1: SAURIA, GVU and DMA blocks of has/has_cpu_direct.h) for tools/has/tb_has_npu_top.cpp, and the same
program for the neo_top bench of the RTL (tb_neo_top_cpu.sv, +CASE=cpu).

Input: the files that HAS_CPU_DIRECT_DUMP=<dir> writes for a GEMM_FUSED layer of the data flow controller on the clocked stream (<dir>/gemm_L<k>/: layer.txt,
lut.bin, sbank.bin, packets.txt, beats.txt, call<i>_{a,b,c}.bin, call<i>_words.txt). The golden is the result of the data flow controller of the model itself
(beats.txt), not a second computation.

The CPU program (one layer = one enable of the vector unit, one START per core call):
  1. DMA the 256-byte activation table into vbank1, the Out_Scale words (sbank byte 0 ...) and the Out_Shift bytes (sbank byte SBANK_SHIFT_INT8_BASE ...)
     into the sbank, in the order of the beats (the scale index of the unit counts the beats since it entered GEMM_FUSED, not the channels).
  2. GVU registers: GEMM_FUSED (zero point, skip activation), OPCODE, then the enable bit of CONTROL last. The buffers of the scratchpad flip at the enable.
  3. For every core call: DMA SRAM A / B / C-preload into the host half, the 29 packed words (with psum_stream_en, cin_tile_last, oc_tile_last), global and
     done interrupt enable, wait for ready (CONTROL bit 3: done and the stream empty), from the second call a soft reset (CONTROL bit 23: the RTL
     does not run a later invocation without it), START, polling of the interrupt status, clear it.
  4. Polling of the done bit of the GVU CONTROL, write 0xC (clear done, force the buffer swap: the result goes to the DMA side), DMA vbank2 to DRAM (row i of
     the result at byte 32 * i), check ("K" line) against the beats of the data flow controller. The 32 lanes of a row are written by the unit whatever the
     lane mask (the RTL writes all lanes): a lane without a mask holds the activation of the zero point, the golden says so.

Limits (the generator refuses): at most 614 beats between the enable and the done (sbank), 1024 rows in vbank2; a layer the dump marks unfit (feeder padding, vertical
scan, beats that the unit does not take). RTL file: the RTL puts oc_tile_last on EVERY packet of the last call, so the unit leaves after its first context:
the last call must run one context, else the RTL program is not written (the model and its per-context flag do not need it).

usage: mk_cpu_gemm.py <dump layer dir> <out dir> [--dram-base 0x80000000] [--name NAME] [--dfc-order]
  --dfc-order: the GVU registers and its enable after the START of the first call, and the START word of the data flow controller (0x10003)
  out dir: dram_init.bin, dram_golden.bin, mmio.txt (model), rtl/NAME.prog, rtl/NAME_dram.hex, rtl/NAME_gold.hex (RTL bench)
"""
import os
import sys

SAURIA, GVU, DMA = 0x11000, 0x12000, 0x13000
REGION = [(0x200, 2), (0x400, 17), (0x600, 5), (0x800, 5)]   # CON, ACT, WEI, OUT words of config_regs.sv
BUS_A, BUS_B, BUS_C = 0x00000000, 0x00040000, 0x00080000
BUS_VBANK1, BUS_VBANK2, BUS_SBANK = 0x00120000, 0x00140000, 0x00180000
SHIFT_BASE, MAX_BEATS, OP_GEMM_FUSED = 2456, 614, 2
NEO, DRAM_RTL, DRAM_MODEL = 0x80000000, 0x40000000, 0x80000000


def read_layer(d):
    kv, calls = {}, []
    for l in open(os.path.join(d, "layer.txt")):
        t = l.split()
        if not t: continue
        if t[0] == "call":
            calls.append({t[i]: int(t[i + 1]) for i in range(2, len(t) - 1, 2)} | {"i": int(t[1])})
        else:
            kv[t[0]] = int(t[1])
    return kv, calls


def rtl_addr(a):
    if 0x11000 <= a < 0x15000: return NEO + (((a >> 12) & 0xF) << 16) + (a & 0xFFF)
    raise SystemExit("address 0x%x has no block of neo_top" % a)


def main():
    d, out = sys.argv[1], sys.argv[2]
    opt = dict(zip(sys.argv[3::2], sys.argv[4::2]))
    dram_base = int(opt.get("--dram-base", "0x80000000"), 0)
    name = opt.get("--name", os.path.basename(os.path.normpath(out)))
    dfc_order = "--dfc-order" in sys.argv[3:]
    if dfc_order: sys.argv.remove("--dfc-order"); opt = dict(zip(sys.argv[3::2], sys.argv[4::2]))
    kv, calls = read_layer(d)
    rd = lambda n: open(os.path.join(d, n), "rb").read()
    if kv.get("unfit") or kv.get("bypassed"):
        raise SystemExit("layer not expressible by the CPU path: unfit %s bypassed %s" % (kv.get("unfit"), kv.get("bypassed")))
    beats = [l.split() for l in open(os.path.join(d, "beats.txt")) if l.strip()]
    nb = len(beats)
    if nb != kv["beats"] or nb != kv["scale_entries"]: raise SystemExit("beats %d scale entries %d layer.txt %d" % (nb, kv["scale_entries"], kv["beats"]))
    if nb > MAX_BEATS: raise SystemExit("%d beats: more than the %d the sbank holds between two enables; split the layer into several enables" % (nb, MAX_BEATS))
    if (nb + 3) // 4 > 256: raise SystemExit("%d beats do not fit vbank2" % nb)
    if len(calls) != kv["calls"] or not calls[-1]["oc_tile_last"] or any(c["oc_tile_last"] for c in calls[:-1]):
        raise SystemExit("oc_tile_last must be set in the last call only")
    lut = rd("lut.bin") if kv["layer_act"] else bytes(256)
    sb = rd("sbank.bin")
    zp, act = kv["layer_zp"], kv["layer_act"]
    z8 = ((zp & 0xFF) ^ 0x80) - 0x80
    # the result image: row i, lane l; a lane without a mask is the zero point through the activation (a zero psum is requantised to 0, + zp, clamp, table)
    zero_lane = (lut[max(-128, min(127, z8)) + 128] if act else max(-128, min(127, z8))) & 0xFF
    res = bytearray()
    for b in beats:
        mask = int(b[2], 16)
        for l in range(32):
            res.append(int(b[3 + l], 16) if (mask >> l) & 1 else zero_lane)
    al = lambda x: (x + 255) & ~255
    off, ptr = {}, 0x1000

    def put(key, size):
        nonlocal ptr
        off[key] = ptr; ptr = al(ptr + size)

    put("lut", 256); put("scale", 4 * nb); put("shift", nb)
    cd = []
    for c in calls:
        i = c["i"]
        blobs = [rd("call%03d_%s.bin" % (i, k)) for k in "abc"]
        words = [int(l.split()[1], 16) for l in open(os.path.join(d, "call%03d_words.txt" % i)) if l.startswith("word")]
        assert len(words) == 29
        cd.append((blobs, words))
        for k, bl in zip("abc", blobs): put("c%d%s" % (i, k), len(bl))
    put("out", len(res))
    size = ptr + 0x1000
    init = bytearray(size)
    init[off["lut"]:off["lut"] + 256] = lut
    init[off["scale"]:off["scale"] + 4 * nb] = sb[0:4 * nb]
    init[off["shift"]:off["shift"] + nb] = sb[SHIFT_BASE:SHIFT_BASE + nb]
    for i, (blobs, _) in enumerate(cd):
        for k, bl in zip("abc", blobs): init[off["c%d%s" % (i, k)]:off["c%d%s" % (i, k)] + len(bl)] = bl
    for i in range(len(res)): init[off["out"] + i] = 0xA5
    gold = bytearray(init); gold[off["out"]:off["out"] + len(res)] = res

    L = ["# GEMM_FUSED layer driven by the CPU: %d core calls, %d beats, zero point %d, activation %d" % (len(calls), nb, zp, act), "W 0x%08x 0x3" % (DMA + 0x04)]

    def dma(r, w, n):
        L.extend(["W 0x%08x 0x%08x" % (DMA + 0x10, r), "W 0x%08x 0x%08x" % (DMA + 0x20, w), "W 0x%08x 0x%08x" % (DMA + 0x30, n),
                  "W 0x%08x 0x3" % DMA, "P 0x%08x 0x2 0x2" % (DMA + 0x0C), "W 0x%08x 0x3" % (DMA + 0x0C)])

    L.append("# activation table, Out_Scale words, Out_Shift bytes (before the enable: the buffers flip at the enable)")
    dma(dram_base + off["lut"], BUS_VBANK1, 256)
    dma(dram_base + off["scale"], BUS_SBANK, 4 * nb)
    dma(dram_base + off["shift"], BUS_SBANK + SHIFT_BASE, nb)
    gvu_cfg = ["W 0x%08x 0x%08x" % (GVU + 0x0C, ((zp & 0xFF) << 8) | (0 if act else 1)), "W 0x%08x 0x%08x" % (GVU + 0x04, OP_GEMM_FUSED)]
    if dfc_order:   # the order of the data flow controller: SAURIA words and START first, the GVU registers and its enable bit after the START
        L += ["W 0x%08x 0x1" % (SAURIA + 0x04), "W 0x%08x 0x1" % (SAURIA + 0x08)]
    else:
        L.append("# GVU: GEMM_FUSED zero point / skip activation, opcode, enable")
        L += gvu_cfg + ["W 0x%08x 0x1" % GVU, "W 0x%08x 0x1" % (SAURIA + 0x04), "W 0x%08x 0x1" % (SAURIA + 0x08)]
    for i, (blobs, words) in enumerate(cd):
        L.append("# core call %d" % i)
        for bus, k, bl in ((BUS_A, "a", blobs[0]), (BUS_B, "b", blobs[1]), (BUS_C, "c", blobs[2])): dma(dram_base + off["c%d%s" % (i, k)], bus, len(bl))
        k = 0
        for base, n in REGION:
            for q in range(n):
                L.append("W 0x%08x 0x%08x" % (SAURIA + base + 4 * q, words[k])); k += 1
        # the hardware does not run a second invocation unless the core is soft reset (CONTROL bit 23) before its START (measured on the neo_top simulation)
        L += ["P 0x%08x 0x8 0x8" % SAURIA]
        if i > 0: L += ["W 0x%08x 0x800000" % SAURIA, "P 0x%08x 0x8 0x8" % SAURIA]
        # --dfc-order: the START word of the data flow controller (start, clear done, memory switch: neo_interface.sv SAURIA_SEND_START_DATA)
        L += ["W 0x%08x 0x%x" % (SAURIA, 0x10003 if dfc_order else 0x1)]
        if dfc_order and i == 0: L += gvu_cfg + ["W 0x%08x 0x1" % GVU]
        L += ["P 0x%08x 0x1 0x1" % (SAURIA + 0x0C), "W 0x%08x 0x1" % (SAURIA + 0x0C)]
    L += ["# the layer ends with the beat that carries oc_last and oc_tile_last: polling of the done bit, clear it and swap the buffers",
          "P 0x%08x 0x2 0x2" % GVU, "W 0x%08x 0xc" % GVU]
    dma(BUS_VBANK2, dram_base + off["out"], len(res))
    L.append("K %x %d" % (off["out"], len(res)))
    os.makedirs(out, exist_ok=True)
    open(os.path.join(out, "dram_init.bin"), "wb").write(init)
    open(os.path.join(out, "dram_golden.bin"), "wb").write(gold)
    open(os.path.join(out, "mmio.txt"), "w", newline="\n").write("\n".join(L) + "\n")
    msg = "calls %d beats %d out_bytes %d lut %d" % (len(calls), nb, len(res), act)

    # program of the RTL bench (same sequence; model page 0x1X000 + off -> NEO + (X << 16) + off; DRAM window 0x80000000 -> 0x40000000)
    if calls[-1]["ncontexts"] != 1:
        msg += " | RTL program NOT written: the last call runs %d contexts (the RTL flags every packet of it with oc_tile_last)" % calls[-1]["ncontexts"]
    else:
        os.makedirs(os.path.join(out, "rtl"), exist_ok=True)
        P = []
        for l in L:
            t = l.split()
            if not t or t[0] == "#": continue
            if t[0] == "W":
                a, v = int(t[1], 16), int(t[2], 16)
                if (a & 0xFF) in (0x10, 0x20) and 0x13000 <= a < 0x14000 and v >= DRAM_MODEL: v = DRAM_RTL + (v - DRAM_MODEL)
                P.append("W %08x %08x" % (rtl_addr(a), v))
            elif t[0] == "P": P.append("P %08x %08x %08x" % (rtl_addr(int(t[1], 16)), int(t[2], 16), int(t[3], 16)))
            elif t[0] == "K": P.append("K %08x %d" % (int(t[1], 16), int(t[2])))
        P.append("E 0 0")
        base = os.path.join(out, "rtl", name)
        open(base + ".prog", "w", newline="\n").write("\n".join(P) + "\n")
        for src, dst in ((init, "_dram.hex"), (gold, "_gold.hex")):
            open(base + dst, "w", newline="\n").write("@0\n" + "\n".join("%02x" % x for x in src) + "\n")
        msg += " | RTL program written: %s.prog, %d lines, %d bytes" % (base, len(P), len(init))
    print(msg)


if __name__ == "__main__":
    main()
