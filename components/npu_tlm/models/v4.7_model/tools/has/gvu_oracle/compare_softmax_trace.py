#!/usr/bin/env python3
"""Compares the state-change cycles of the RTL report bench (tb_top_softmax.sv, lines "TOP cycle N: state a -> b", RTL state codes of the delivered enum with
GEMM_FUSED_OBP at 5: 0 IDLE, 2 LUT_EXP_INIT, 3 LUT_RECIP_INIT, 6 SOFTMAX_ST1, 7 SOFTMAX_ST2, 8 SOFTMAX_ST3) with those of tb_gvu_top_softmax_trace (names).
usage: python3 gvu_oracle_compare_softmax_trace.py <rtl trace file> <token length> <model trace file>   (exit 0 = same transitions and done cycle)"""
import re, sys
names = {0: "IDLE", 1: "LUT_ACT_INIT", 2: "LUT_EXP_INIT", 3: "LUT_RECIP_INIT", 4: "LUT_RSQRT_INIT", 6: "SOFTMAX_ST1", 7: "SOFTMAX_ST2", 8: "SOFTMAX_ST3"}
def rtl(path, L):
    out, on = [], False
    for l in open(path):
        if l.startswith("== token length"): on = l.split()[-1] == str(L); continue
        if not on: continue
        m = re.match(r"TOP cycle (\d+): state (\d+) -> (\d+);", l)
        if m: out.append((int(m.group(1)), names.get(int(m.group(2)), "?"), names.get(int(m.group(3)), "?")))
        m = re.match(r"TOP cycle (\d+): o_pipe_done = 1", l)
        if m: out.append((int(m.group(1)), "done", ""))
        # the bench leaves its loop when the state is IDLE again and does not print that last change: "TOP end at cycle N: state 0"
        m = re.match(r"TOP end at cycle (\d+): state 0,", l)
        if m: out.append((int(m.group(1)), "SOFTMAX_ST3", "IDLE"))
    return out
def model(path):
    out = []
    for l in open(path):
        m = re.match(r"TOP cycle (\d+): (\w+) -> (\w+)", l)
        if m: out.append((int(m.group(1)), m.group(2), m.group(3)))
        m = re.match(r"TOP cycle (\d+): done output = 1", l)
        if m: out.append((int(m.group(1)), "done", ""))
    return out
a, b = rtl(sys.argv[1], sys.argv[2]), model(sys.argv[3])
a, b = sorted(a), sorted(b)
for x in a: print("RTL  ", x, "OK" if x in b else "NOT IN MODEL")
for x in b: print("MODEL", x, "OK" if x in a else "NOT IN RTL")
print("COMPARE_SOFTMAX_TRACE", "SAME" if a == b else "DIFFERENT", len(a), len(b))
sys.exit(0 if a == b else 1)
