#!/usr/bin/env python3
"""Independent cross-check of include/fx1_isp/fx1_isp_csr.h against the XLSX.

Deliberately shares no code with gen_csr.py: it re-reads the spreadsheet with
a minimal parser and compares every register offset, register reset value and
field mask/shift/reset with the macros in the generated header. The only
knowledge it duplicates is the list of reserved tied-zero bits (SPEC-05,
CSR-DMA-ERR-5), restated here on purpose.
"""

import argparse
import re
import sys

import openpyxl

TIED_ZERO = {"DMA_IRQ_EN": 0x1, "DMA_IRQ_STAT": 0x1, "DMA_ERR": 0x20}


def parse_xlsx(path):
    ws = openpyxl.load_workbook(path, data_only=True)["Registers"]
    regs, cur = {}, None
    for row in ws.iter_rows(min_row=2, values_only=True):
        addr, rname, fname, bits, _w, _acc, rst, _d = row[:8]
        if rname:
            cur = str(rname).strip()
            regs[cur] = {"offset": int(str(addr), 16), "fields": {}}
        elif fname:
            m = re.match(r"\[(\d+)(?::(\d+))?\]", str(bits).strip())
            hi = int(m.group(1))
            lo = int(m.group(2)) if m.group(2) else hi
            mask = ((1 << (hi - lo + 1)) - 1) << lo
            mask &= ~TIED_ZERO.get(cur, 0)
            regs[cur]["fields"][str(fname).strip()] = (lo, mask, int(str(rst), 16))
    return regs


def parse_header(path):
    macros = {}
    for line in open(path, encoding="utf-8"):
        m = re.match(r"#define (FX1_ISP_\w+) (?:\(1u << )?(0x[0-9A-Fa-f]+|\d+)u", line)
        if m:
            macros[m.group(1)] = int(m.group(2), 0)
    return macros


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--xlsx", required=True)
    ap.add_argument("--header", required=True)
    a = ap.parse_args()
    regs, macros = parse_xlsx(a.xlsx), parse_header(a.header)
    errors, compared = [], 0

    def expect(name, value):
        nonlocal compared
        compared += 1
        if macros.get(name) != value:
            errors.append("%s: header %r, spreadsheet 0x%X" % (name, macros.get(name), value))

    for rname, r in regs.items():
        p = "FX1_ISP_" + re.sub(r"\W", "_", rname).upper()
        expect(p + "_OFFSET", r["offset"])
        reset = 0
        for fname, (lo, mask, rst) in r["fields"].items():
            f = p + "_" + re.sub(r"\W", "_", fname).upper()
            expect(f + "_SHIFT", lo)
            expect(f + "_MASK", mask)
            expect(f + "_RESET", rst)
            reset |= (rst << lo) & mask
        expect(p + "_RESET", reset)
    if len(regs) != 226:
        errors.append("expected 226 registers, found %d" % len(regs))
    for e in errors:
        print("MISMATCH", e)
    print("check_csr_header: %d registers, %d values compared, %d mismatches"
          % (len(regs), compared, len(errors)))
    sys.exit(1 if errors else 0)


if __name__ == "__main__":
    main()
