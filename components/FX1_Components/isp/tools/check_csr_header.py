#!/usr/bin/env python3
"""Independent cross-check of include/fx1_isp/fx1_isp_csr.h against the workbook.

Deliberately shares no code with gen_csr.py: it re-reads the spreadsheet
(.xlsx or .xlsm, data only) with a minimal parser and compares every register
offset, register reset value, field mask/shift/reset and field access type
(the header's per-field comment) with the generated header. The only knowledge
it duplicates is the list of reserved tied-zero bits (CSR-DMA-ERR-5, and since
v1.07 the buffer-ownership bits at or above the build buffer count of 4,
CSR107-BUFCOUNT), the register count of the pinned revision and the VP CSR
extension of the consolidated update (ISP-UPD-001/002/007,
docs/ISP_CSR_EXTENSION_UPD.md), restated here on purpose.
"""

import argparse
import re
import sys

import openpyxl

TIED_ZERO = {"DMA_ERR": 0x20, "IDMA_BUF_VALID": 0xF0, "ODMA_BUF_FREE": 0xF0, "ODMA_BUF_DONE": 0xF0,
             "CH1_ODMA_BUF_FREE": 0xF0, "CH1_ODMA_BUF_DONE": 0xF0, "CH2_ODMA_BUF_FREE": 0xF0,
             "CH2_ODMA_BUF_DONE": 0xF0}
NUM_REGISTERS = 573  # CSR revision v1.07

# VP CSR extension 0x20261109, restated from the design table (not from gen_csr.py).
EXTENSION_ID = 0x20261109
EXT_REMOVED = {"BLC_SCALE_G%d" % g for g in range(4)}            # ISP-UPD-002
EXT_WIDENED = {("LSC_COEF_DATA", "coef_data"): 21,                # ISP-UPD-001: UQ4.18
               ("BLC_GAIN_SEL", "gain_level"): 2}                 # ISP-UPD-002: G0..G7


def extension_added():
    """ISP-UPD-002: G4..G7 DFT/trims after BLC_SCALE_G3's old slot, then the
    per-channel scales of G0..G7; one RW field per register at bit 0."""
    added = {}
    for g in range(4, 8):
        for i, c in enumerate(("DFT", "R", "GR", "GB", "B")):
            added["BLC_OFS_G%d_%s" % (g, c)] = (0x268 + 20 * (g - 4) + 4 * i, "ofs_g%d_%s" % (g, c.lower()), 12,
                                                200 if c == "DFT" else 0)
    for g in range(8):
        for i, c in enumerate(("R", "GR", "GB", "B")):
            added["BLC_SCALE_G%d_%s" % (g, c)] = (0x2B8 + 16 * g + 4 * i, "scale_g%d_%s" % (g, c.lower()), 18,
                                                  0x10000)
    return added


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
        m = re.match(r"/\* 0x[0-9A-F]{4} (\w+) \((?:CSR row|extension)", line)
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
    if len(regs) != NUM_REGISTERS:
        errors.append("expected %d registers in the workbook, found %d" % (NUM_REGISTERS, len(regs)))
    # Apply the extension to the workbook view.
    for rname in EXT_REMOVED:
        if rname not in regs:
            errors.append("extension: %s not in the workbook" % rname)
        regs.pop(rname, None)
        p = "FX1_ISP_" + rname
        if p + "_OFFSET" in macros or p + "_RESET" in macros or any(
                k.startswith("%s_SCALE_G%s_" % (p, rname[-1])) for k in macros):
            errors.append("extension: removed register %s still has macros in the header" % rname)
    for (rname, fname), hi in EXT_WIDENED.items():
        lo, _mask, rst, acc, _hi = regs[rname]["fields"][fname]
        regs[rname]["fields"][fname] = (lo, ((1 << (hi - lo + 1)) - 1) << lo, rst, acc, hi)
    for rname, (off, fname, width, rst) in extension_added().items():
        if rname in regs:
            errors.append("extension: %s already in the workbook" % rname)
        regs[rname] = {"offset": off, "fields": {fname: (0, (1 << width) - 1, rst, "RW", width - 1)}}

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
    if macros.get("FX1_ISP_CSR_NUM_REGISTERS") != len(regs):
        errors.append("FX1_ISP_CSR_NUM_REGISTERS %r, expected %d" % (macros.get("FX1_ISP_CSR_NUM_REGISTERS"),
                                                                     len(regs)))
    if macros.get("FX1_ISP_CSR_EXTENSION") != EXTENSION_ID:
        errors.append("FX1_ISP_CSR_EXTENSION %r, expected 0x%08X" % (macros.get("FX1_ISP_CSR_EXTENSION"),
                                                                     EXTENSION_ID))
    offsets = sorted(r["offset"] for r in regs.values())
    if len(set(offsets)) != len(offsets):
        errors.append("duplicate register offsets after the extension")
    for e in errors:
        print("MISMATCH", e)
    print("check_csr_header: %d registers, %d values compared, %d mismatches"
          % (len(regs), compared, len(errors)))
    sys.exit(1 if errors else 0)


if __name__ == "__main__":
    main()
