"""Roadmap R2: check one frontend-tile core job (tools/fe/fe_core_jobs.py, tools/fe/fe_core_run.sh).

Hop (b), after stimulus generation: the sauria_model golden equals the frontend expectation.
  stimuli/tstcfg.txt = [A offset, C offset, region length] (file_helper.generate_test_files); DRAM text files hold one
  hex byte per line; regions are packed by data_helper.assign_dram_values: A int8 [Cin, AH, AW], C int32 little-endian
  [k, h, w] (initial_dram: preload, gold_dram: result).
  checks: A region == A.npy; gold == preload + expect (mod 2^32).
Hop (a), after tb_iso (--sim): run.log of tools/run_eval.sh, parsed like sauria_model tools/run_yolo.sh
  (Mismatches, Expected elements, 'core timeout' count, [STAGE] busy).
Prints one TSV line: name, hop_b, gold_bad, preload nonzero count, hop_a, mismatches, expected, timeouts, busy, W_t*Cout_t.
Usage: python3 tools/fe/fe_core_check.py <job dir> [--sim]
"""
import json
import os
import re
import sys

import numpy as np


def read_hex_bytes(path):
    with open(path) as f:
        return np.array([int(x, 16) for x in f.read().split()], dtype=np.uint8)


def main():
    d = sys.argv[1]
    sim = "--sim" in sys.argv[2:]
    name = os.path.basename(d.rstrip("/"))
    with open(os.path.join(d, "job.json")) as f:
        meta = json.load(f)
    stim = os.path.join(d, "sm", "stimuli")
    a = np.load(os.path.join(d, "A.npy"))
    expect = np.load(os.path.join(d, "expect.npy")).astype(np.int64)
    hop_b, gold_bad, pre_nz = "NO_STIM", -1, -1
    if os.path.exists(os.path.join(stim, "gold_dram.txt")):
        a_off, c_off, _ = [int(x, 16) for x in open(os.path.join(stim, "tstcfg.txt")).read().split()]
        init = read_hex_bytes(os.path.join(stim, "initial_dram.txt"))
        gold = read_hex_bytes(os.path.join(stim, "gold_dram.txt"))
        n = expect.size
        a_ok = np.array_equal(init[a_off:a_off + a.size], a.reshape(-1).view(np.uint8))
        pre = init[c_off:c_off + 4 * n].view("<u4").astype(np.uint64)
        got = gold[c_off:c_off + 4 * n].view("<u4").astype(np.uint64)
        want = (pre + (expect.reshape(-1) % (1 << 32)).astype(np.uint64)) % (1 << 32)
        gold_bad = int(np.count_nonzero(got != want))
        pre_nz = int(np.count_nonzero(pre))
        hop_b = "PASS" if (a_ok and gold_bad == 0) else ("A_REGION_DIFF" if not a_ok else "FAIL")
    hop_a, mism, expected, timeouts, busy = "-", "-", "-", "-", "-"
    if sim:
        log = os.path.join(d, "sim", "run.log")
        if not os.path.exists(log):
            hop_a = "NO_RUN_LOG"
        else:
            text = open(log, errors="replace").read()
            m = re.findall(r"Mismatches\s*:\s*(\d+)", text)
            e = re.findall(r"Expected elements\s*:\s*(\d+)", text)
            b = re.findall(r"^\[STAGE\] busy\s*=\s*(\d+)", text, re.M)
            timeouts = str(text.count("core timeout"))
            mism = m[-1] if m else "-"
            expected = e[-1] if e else str(expect.size)
            busy = b[-1] if b else "-"
            if not m:
                hop_a = "INCOMPLETE"
            else:
                hop_a = "PASS" if (int(mism) == 0 and timeouts == "0") else "FAIL"
    print("\t".join(map(str, [name, hop_b, gold_bad, pre_nz, hop_a, mism, expected, timeouts, busy, meta["W_t*Cout_t"]])))


if __name__ == "__main__":
    main()
