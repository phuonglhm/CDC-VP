#!/usr/bin/env python3
"""Evidence sheets for the CPU-direct checks of the model (tools/has/cpu_direct/run_cpu_direct.sh).

For every test whose CPU program ran, one Markdown sheet: what is tested, the input (initial DRAM image, the run of the data flow
controller the program was taken from, the generator line), the CPU program (register writes per block, polls, checked regions, an
excerpt), the output against the golden region by region (bytes, bad bytes as the testbench counted them during the run and again on
the DRAM image the CPU left, MD5 of both sides, the first bytes of both sides, the first differing offsets) and the counters of the log.
INDEX.md lists all checks with their verdict from summary.txt and links the sheets.

usage: evidence_cpu_direct.py <out dir of run_cpu_direct.sh>
Needs <test dir>/dram_snapshot.bin (FE_SNAPSHOT_DIR, set by run_cpu_direct.sh). Exit 1 when a sheet cannot be made or when a region
count recomputed on the image differs from the testbench count of a region that is not written again after its check.
"""
import hashlib
import os
import re
import sys

# test -> (title, what is tested, where the input / golden come from, source dump dir or None)
TESTS = {
    "opgen_e2e": ("DMA tensor operations programmed through the host port",
                  "One DMA_TENSOR COPY instruction and 7 descriptors of the DMA tensor register file (copy with loops, fill, 2-D pad, "
                  "upsample) written by the host; every written DRAM region is compared with the golden image.",
                  "Program and golden from make_opgen_e2e.py: the expected bytes are computed with independent loops in Python.", None),
    "cpu_dma": ("One-dimensional DMA driven by the CPU",
                "For every local memory (SRAM A / B / C, vbank0 / 1 / 2, sbank) the CPU copies a DRAM block into the memory and back to "
                "another DRAM block, with unaligned addresses, write-zero and the earlier register address; one command that points "
                "outside every memory must not be executed.",
                "Program and golden from mk_cpu_dma.py: the golden is the source block copied to its destination (Python).", None),
    "cpu_core": ("One SAURIA core run driven by the CPU",
                 "The CPU loads SRAM A / B / C-preload by DMA, writes the 29 packed configuration words of config_regs.sv, starts the "
                 "core, polls for done, switches memory and copies SRAM C out by DMA. The core cycles must equal the run of the data "
                 "flow controller.",
                 "Taken from the first core run of the data flow controller of the model on the reference program ref_gate "
                 "(HAS_CPU_DIRECT_DUMP); golden = SRAM C after that run.", "cpu_dump_core"),
    "cpu_pool": ("Max pool of the vector unit driven by the CPU",
                 "The CPU loads the input rows into vbank0 by DMA, writes the same register values the data flow controller wrote, "
                 "enables the unit, polls the done bit, swaps the buffer and copies vbank2 out. The unit cycles must equal the "
                 "controller run.",
                 "Taken from the first max pool of the data flow controller on ref_gate_mp; golden = vbank2 after that run.",
                 "cpu_dump_pool"),
    "cpu_res": ("RESIDUAL add of the vector unit driven by the CPU",
                "64 rows of 32 int8 lanes: A in vbank0, B in vbank1, add with scales, bias, shift and output zero point; result read "
                "from vbank2. 80 unit cycles (16 + N, as the RTL).",
                "Program and golden from mk_cpu_res.py: the golden is the formula of gvu_top.sv computed in Python.", None),
    "cpu_res_sub": ("RESIDUAL subtract of the vector unit driven by the CPU",
                    "As cpu_res with the subtract flag.",
                    "Program and golden from mk_cpu_res.py --sub 1 (formula of gvu_top.sv in Python).", None),
    "cpu_gemm_3t": ("GEMM_FUSED layer driven by the CPU (core + epilogue of the vector unit)",
                    "1 x 1 convolution, 24 -> 96 channels, 32 positions, 3 core calls of one context. The CPU loads the activation "
                    "table and Out_Scale / Out_Shift into the sbank, configures the GEMM_FUSED operation, runs 3 core calls with the "
                    "output stream into the unit (soft reset before the 2nd and 3rd START), and copies the 96 result rows of vbank2 out.",
                    "Taken from the run of the data flow controller on the same layer (mk_gemm_gate.py, HAS_CPU_DIRECT_DUMP); golden "
                    "= the result rows of that run.", "gemm_dump32/gemm_L0"),
    "cpu_gemm_mc": ("GEMM_FUSED layer driven by the CPU, two contexts per core call",
                    "As cpu_gemm_3t with 64 positions: 3 core calls of 2 contexts, 192 result rows.",
                    "Taken from the controller run of the same layer; golden = its result rows.", "gemm_dump64/gemm_L0"),
    "cpu_gemm_nosoft": ("FPGA rule: START without a soft reset",
                        "cpu_gemm_3t with every soft-reset write removed. The hardware stalls on such a START; the model runs on but "
                        "must count the 2 STARTs and report once, so firmware that breaks the rule is caught on the model.",
                        "Same input and golden as cpu_gemm_3t.", "gemm_dump32/gemm_L0"),
    "cpu_core_st": ("SAURIA status registers read by the CPU",
                    "cpu_core with reads of the status, cycle counter, stall counter and version registers: version 0x00772020, "
                    "cycle counter equal to the controller run.",
                    "Same input and golden as cpu_core.", "cpu_dump_core"),
    "cpu_core_soft": ("Soft reset right after a START, then the run again",
                      "cpu_core with a soft reset (CONTROL bit 23) right after the START, then a second START on the same memory "
                      "halves: the result must still be exact.",
                      "Same input and golden as cpu_core.", "cpu_dump_core"),
    "cpu_chain": ("Two layers chained through DRAM, then RESIDUAL",
                  "Firmware as for a whole network: GEMM_FUSED layer 0 writes its result to DRAM, layer 1 reads it from there with its "
                  "own activation table (table reset between the layers) and sbank, then RESIDUAL of the two results. A soft reset "
                  "before every START after the first.",
                  "Taken from the run of the data flow controller on the same two layers (mk_cpu_chain.py gate); RESIDUAL golden "
                  "from the RTL formula in Python.", "chain_dump"),
    "cpu_chain_nr": ("NEGATIVE CONTROL: cpu_chain without the table reset",
                     "cpu_chain with the write that resets the activation table removed. Layer 1 then keeps the table of layer 0, so "
                     "layer 1 and the RESIDUAL must differ: the check passes only when exactly 2 regions / 4 096 bytes are wrong. It "
                     "shows that the comparison catches a real firmware error.",
                     "Same input and golden as cpu_chain.", "chain_dump"),
}
MAIN = ["cpu_dma", "cpu_core", "cpu_pool", "cpu_res", "cpu_gemm_3t", "cpu_gemm_nosoft", "cpu_chain", "cpu_chain_nr", "opgen_e2e"]
BLOCKS = [(0x11000, 0x12000, "SAURIA (0x11000)"), (0x12000, 0x13000, "GVU (0x12000)"), (0x13000, 0x14000, "DMA (0x13000)"),
          (0x14000, 0x15000, "SYNC (0x14000)")]


def md5(data):
    return hashlib.md5(data).hexdigest()


def read(path):
    with open(path, "rb") as f:
        return f.read()


def hexs(b):
    return " ".join("%02x" % x for x in b)


def program(path):
    """W / P / K / H lines of a program; K = (address, bytes, index of the line)."""
    w, p, h, k, comments, lines = {}, 0, 0, [], 0, []
    with open(path) as f:
        lines = f.read().splitlines()
    for i, ln in enumerate(lines):
        t = ln.split()
        if not t:
            continue
        if t[0].startswith("#"):
            comments += 1
        elif t[0] == "W":
            a = int(t[1], 16)
            name = next((n for lo, hi, n in BLOCKS if lo <= a < hi), "other (0x%x page)" % (a & ~0xFFF))
            w[name] = w.get(name, 0) + 1
        elif t[0] == "P":
            p += 1
        elif t[0] == "K":
            k.append((int(t[1], 16), int(t[2]), i))
        elif t[0] == "H":
            h += 1
    return w, p, h, k, comments, lines


def log_regions(path):
    """Bad counts of the K regions as the testbench printed them, in order."""
    out = []
    with open(path, errors="replace") as f:
        for ln in f:
            m = re.search(r"region 0x([0-9a-f]+) (\d+) B -> bad=(\d+) (PASS|FAIL)", ln)
            if m:
                out.append((int(m.group(1), 16), int(m.group(2)), int(m.group(3))))
    return out


def summary_rows(out):
    rows = {}
    p = os.path.join(out, "summary.txt")
    if os.path.exists(p):
        with open(p) as f:
            for ln in f:
                m = re.match(r"\s*(\d+)\s+(PASS|FAIL)\s+(\S+)\s+(.*)", ln)
                if m:
                    rows[m.group(3)] = (int(m.group(1)), m.group(2), m.group(4).strip())
    return rows


def sheet(out, test, rows):
    title, what, source, dump = TESTS[test]
    d = os.path.join(out, test)
    init, gold, snap = (os.path.join(d, n) for n in ("dram_init.bin", "dram_golden.bin", "dram_snapshot.bin"))
    log = os.path.join(out, test + ".log")
    for p in (init, gold, snap, log, os.path.join(d, "mmio.txt")):
        if not os.path.exists(p):
            raise RuntimeError("%s: missing %s" % (test, p))
    bi, bg, bs = read(init), read(gold), read(snap)
    w, p, h, k, comments, lines = program(os.path.join(d, "mmio.txt"))
    lr = log_regions(log)
    no, verdict, check = rows.get(test, (0, "?", ""))
    neg = test.endswith("_nr")
    L = ["# %s: %s" % (test, title), ""]
    L.append("**Verdict: %s** (check %d of %d in summary.txt%s): %s" % (
        verdict, no, len(rows), "; negative control, the run itself must FAIL as stated" if neg else "", check))
    L += ["", "## 1. What is tested", "", what, ""]
    L += ["## 2. Input", "", "| Item | Value |", "|---|---|",
          "| Initial DRAM image | `%s/dram_init.bin`, %d bytes, MD5 `%s` |" % (test, len(bi), md5(bi)),
          "| Golden DRAM image | `%s/dram_golden.bin`, %d bytes, MD5 `%s` |" % (test, len(bg), md5(bg)),
          "| Where input and golden come from | %s |" % source]
    gen = os.path.join(out, test + ".gen.log")
    if os.path.exists(gen):
        g = open(gen, errors="replace").read().strip().splitlines()
        if g:
            L.append("| Generator line | `%s` |" % g[-1][:160].replace("|", "/"))
    L.append("")
    if dump and os.path.isdir(os.path.join(out, dump)):
        L += ["Files the generator read (written by the data flow controller run, HAS_CPU_DIRECT_DUMP), `%s/`:" % dump, ""]
        names = sorted(os.listdir(os.path.join(out, dump)))
        L.append(", ".join("`%s` (%s)" % (n, ("%d B" % os.path.getsize(os.path.join(out, dump, n))) if os.path.isfile(os.path.join(out, dump, n)) else "dir") for n in names[:24])
                 + (", ..." if len(names) > 24 else ""))
        for desc in ("layer.txt", "core.txt", "pool.txt"):
            dp = os.path.join(out, dump, desc)
            if os.path.exists(dp):
                L += ["", "First lines of `%s`:" % desc, "", "```"] + open(dp, errors="replace").read().splitlines()[:8] + ["```"]
        L.append("")
    L += ["## 3. CPU program (`%s/mmio.txt`, replayed by the testbench as the CPU)" % test, "",
          "| Lines | Register writes (W) | Polls (P) | Checked regions (K) | Host steps (H) | Comment lines |", "|---|---|---|---|---|---|",
          "| %d | %d (%s) | %d | %d | %d | %d |" % (len(lines), sum(w.values()), ", ".join("%s %d" % (n, c) for n, c in w.items()), p, len(k), h,
                                                comments), "",
          "W = the CPU writes a register, P = the CPU reads a register until (value & mask) = expected, K = the testbench compares a DRAM "
          "region with the golden image (not part of firmware). First lines:", "", "```"]
    L += [ln for ln in lines if ln.strip()][:24] + ["```", ""]
    L += ["## 4. Output against the golden", "",
          "`bad (run)` = bytes the testbench found wrong when it reached the K line; `bad (end)` = recomputed by this script on the DRAM "
          "image the CPU left at the end (`dram_snapshot.bin`). A region written again after its check can differ at the end; that is "
          "noted.", "",
          "| # | Region | Bytes | bad (run) | bad (end) | MD5 output | MD5 golden | First 16 bytes, output | First 16 bytes, golden |",
          "|---|---|---|---|---|---|---|---|---|"]
    problems = []
    for j, (a, n, li) in enumerate(k):
        o, gg = bs[a:a + n], bg[a:a + n]
        end_bad = sum(1 for x, y in zip(o, gg) if x != y) + abs(len(o) - len(gg))
        run_bad = lr[j][2] if j < len(lr) and lr[j][0] == a else None
        rewritten = any(lj > li for (aa, nn, lj) in k[j + 1:] if aa < a + n and a < aa + nn)
        if run_bad is not None and run_bad != end_bad and not rewritten:
            problems.append("region 0x%x: run %d, end %d" % (a, run_bad, end_bad))
        L.append("| %d | 0x%x | %d | %s | %d | `%s` | `%s` | `%s` | `%s` |" % (
            j, a, n, "-" if run_bad is None else run_bad, end_bad, md5(o)[:12], md5(gg)[:12], hexs(o[:16]), hexs(gg[:16])))
    total_bad = sum(r[2] for r in lr)
    total = sum(n for _, n, _ in k)
    L += ["", "Total: %d regions, %d bytes compared, %d bytes wrong during the run." % (len(k), total, total_bad), ""]
    diffs = []
    for a, n, _ in k:
        for off in range(n):
            if a + off < len(bs) and bs[a + off] != bg[a + off]:
                diffs.append((a + off, bs[a + off], bg[a + off]))
                if len(diffs) >= 8:
                    break
        if len(diffs) >= 8:
            break
    if diffs:
        L += ["First differing bytes (address: output / golden): " + ", ".join("0x%x: %02x / %02x" % x for x in diffs), ""]
    L += ["## 5. Counters and result lines of the log (`%s.log`)" % test, "", "```"]
    with open(log, errors="replace") as f:
        for ln in f:
            if re.search(r"RESULT:|sim cycles|HAS_CPU_DIRECT|START\(s\) without|region 0x", ln):
                L.append(ln.rstrip()[:240])
    L += ["```", ""]
    err = os.path.join(out, test + ".err")
    if os.path.exists(err) and os.path.getsize(err):
        L += ["Messages on stderr (`%s.err`):" % test, "", "```"] + open(err, errors="replace").read().splitlines()[:6] + ["```", ""]
    with open(os.path.join(out, "evidence", test + ".md"), "w") as f:
        f.write("\n".join(L) + "\n")
    return verdict, len(k), total, total_bad, problems


def main():
    if len(sys.argv) != 2:
        print(__doc__)
        return 2
    out = sys.argv[1]
    os.makedirs(os.path.join(out, "evidence"), exist_ok=True)
    rows = summary_rows(out)
    done, problems = {}, []
    for t in TESTS:
        try:
            done[t] = sheet(out, t, rows)
            problems += ["%s: %s" % (t, p) for p in done[t][4]]
        except Exception as e:  # a missing file: reported, the other sheets are still written
            problems.append("%s: %s" % (t, e))
    L = ["# Evidence of the CPU-direct checks of the model", "",
         "Made by `tools/has/cpu_direct/evidence_cpu_direct.py` from the output of `tools/has/cpu_direct/run_cpu_direct.sh` (`%s`)." % out, "",
         "How every check works: the data flow controller (DFC) of the model runs a small piece of work and its result is the golden; "
         "what the DFC gave the hardware (SRAM contents, the 29 core words, the registers of the vector unit) is written out; a generator "
         "turns it into the register accesses a CPU must make (`mmio.txt`); the testbench replays them as the CPU while the DFC stays idle; "
         "every checked DRAM region is compared byte by byte with the golden, and the counts in the log (STARTs, soft resets, cycles) "
         "with the expected values. A negative control removes one required step and must fail.", "",
         "## Main checks", "", "| Check | Verdict | Regions | Bytes compared | Bytes wrong | Sheet |", "|---|---|---|---|---|---|"]
    for t in MAIN:
        if t in done:
            v, nk, tot, tb, _ = done[t]
            L.append("| %s | %s%s | %d | %d | %d | [%s.md](%s.md) |" % (t, v, " (expected FAIL seen)" if t.endswith("_nr") else "", nk, tot, tb, t, t))
    L += ["", "## All checks (summary.txt)", "", "| No | Verdict | Check | What is checked | Sheet |", "|---|---|---|---|---|"]
    for t, (no, v, what) in sorted(rows.items(), key=lambda x: x[1][0]):
        L.append("| %d | %s | %s | %s | %s |" % (no, v, t, what.replace("|", "/"), "[sheet](%s.md)" % t if t in done else "-"))
    if problems:
        L += ["", "## Problems found while making the sheets", ""] + ["- " + p for p in problems]
    with open(os.path.join(out, "evidence", "INDEX.md"), "w") as f:
        f.write("\n".join(L) + "\n")
    print("sheets %d, problems %d" % (len(done), len(problems)))
    for p in problems:
        print("PROBLEM " + p)
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
