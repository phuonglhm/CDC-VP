#!/usr/bin/env python3
"""Independent cross-check of include/fx1_isp/fx1_isp_csr.h against the workbook.

Deliberately shares no code with gen_csr.py: it re-reads the spreadsheet
(.xlsx or .xlsm, data only) with a minimal parser and compares every register
offset, register reset value, field mask/shift/reset and field access type
(the header's per-field comment) with the generated header. The only knowledge
it duplicates is the list of reserved tied-zero bits (CSR-DMA-ERR-5, and since
v1.07 the buffer-ownership bits at or above the build buffer count of 4,
CSR107-BUFCOUNT) and the register count of the pinned revision, restated here
on purpose.
"""

import argparse
import re
import sys

import openpyxl

TIED_ZERO = {"DMA_ERR": 0x20, "IDMA_BUF_VALID": 0xF0, "ODMA_BUF_FREE": 0xF0, "ODMA_BUF_DONE": 0xF0,
             "CH1_ODMA_BUF_FREE": 0xF0, "CH1_ODMA_BUF_DONE": 0xF0, "CH2_ODMA_BUF_FREE": 0xF0,
             "CH2_ODMA_BUF_DONE": 0xF0}
NUM_REGISTERS = 573  # CSR revision v1.07


def parse_xlsx(path):
    ws = openpyxl.load_workbook(path, data_only=True)["Registers"]
    regs, cur = {}, None
    for row in ws.iter_rows(min_row=2, values_only=True):
        addr, rname, fname, bits, _w, acc, rst, _d = row[:8]
        if rname:
            cur = str(rname).strip()
            regs[cur] = {"offset": int(str(addr), 16), "fields": {}}
        elif fname:
            m = re.match(r"\[(\d+)(?::(\d+))?\]", str(bits).strip())
            hi = int(m.group(1))
            lo = int(m.group(2)) if m.group(2) else hi
            mask = ((1 << (hi - lo + 1)) - 1) << lo
            mask &= ~TIED_ZERO.get(cur, 0)
            regs[cur]["fields"][str(fname).strip()] = (lo, mask, int(str(rst), 16), str(acc).strip(), hi)
    return regs


def parse_header(path):
    macros, access, reg = {}, {}, None
    for line in open(path, encoding="utf-8"):
        m = re.match(r"#define (FX1_ISP_\w+) (?:\(1u << )?(0x[0-9A-Fa-f]+|\d+)u", line)
        if m:
            macros[m.group(1)] = int(m.group(2), 0)
        m = re.match(r"/\* 0x[0-9A-F]{4} (\w+) \(CSR row", line)
        if m:
            reg = m.group(1)
        m = re.match(r"/\*   (\w+) \[(\d+):(\d+)\] (\w+)", line)
        if m and reg:
            access[(reg, m.group(1))] = (m.group(4), int(m.group(2)))
    return macros, access


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--xlsx", required=True)
    ap.add_argument("--header", required=True)
    a = ap.parse_args()
    regs = parse_xlsx(a.xlsx)
    macros, access = parse_header(a.header)
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
        for fname, (lo, mask, rst, acc, hi) in r["fields"].items():
            f = p + "_" + re.sub(r"\W", "_", fname).upper()
            compared += 1
            if access.get((rname, fname)) != (acc, hi):
                errors.append("%s.%s: header access/msb %r, spreadsheet %r" % (rname, fname,
                              access.get((rname, fname)), (acc, hi)))
            expect(f + "_SHIFT", lo)
            expect(f + "_MASK", mask)
            expect(f + "_RESET", rst)
            reset |= (rst << lo) & mask
        expect(p + "_RESET", reset)
    if len(regs) != NUM_REGISTERS:
        errors.append("expected %d registers, found %d" % (NUM_REGISTERS, len(regs)))
    for e in errors:
        print("MISMATCH", e)
    print("check_csr_header: %d registers, %d values compared, %d mismatches"
          % (len(regs), compared, len(errors)))
    sys.exit(1 if errors else 0)


if __name__ == "__main__":
    main()
