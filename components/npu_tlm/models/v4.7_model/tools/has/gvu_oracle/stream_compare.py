#!/usr/bin/env python3
"""Compares the stream_tb_b_lut.txt files of two RTL revisions (written by the STREAM phase of tb_b_lut in tb_oracle_b.sv).
usage: python3 stream_compare.py <file revision A> <file revision B>
Prints, per category (0 direct, 1 index mode 0, 2 index mode 1), the number of requests, the number whose data differ,
the number whose E_shift at the output valid differs, and the first differences. Category and request fields must be equal."""
import sys, collections
def load(p):
    rows = []
    for l in open(p):
        a, b = l.split("|")
        rows.append((a.split(), b.split()))
    return rows
A, B = load(sys.argv[1]), load(sys.argv[2])
assert len(A) == len(B), (len(A), len(B))
n = collections.Counter(); dd = collections.Counter(); de = collections.Counter(); dv = collections.Counter(); shown = 0
for (ra, oa), (rb, ob) in zip(A, B):
    assert ra == rb, (ra, rb)
    c = ra[0]; n[c] += 1
    # outputs: valid latency o8 o16 es; the data is o8 for act / exp (category 0), o16 otherwise
    if oa[0] != ob[0]: dv[c] += 1
    data_a, data_b = (oa[2], ob[2]) if c == "0" else (oa[3], ob[3])
    if data_a != data_b:
        dd[c] += 1
        if shown < 12:
            shown += 1
            print("DATA", "cat", c, "sel", ra[1], "mode", ra[2], "comp", ra[3], "i8", ra[7], "x", ra[8], "A", data_a, "B", data_b)
    if c != "0" and oa[4] != ob[4]: de[c] += 1
for c in sorted(n):
    print("COMPARE category", c, "requests", n[c], "valid_differs", dv[c], "data_differs", dd[c], "e_shift_differs", de[c])
print("COMPARE total requests", sum(n.values()), "data_differs", sum(dd.values()), "e_shift_differs", sum(de.values()))
