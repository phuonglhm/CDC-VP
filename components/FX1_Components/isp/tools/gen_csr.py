#!/usr/bin/env python3
"""Generate the FX1 ISP CSR artefacts from FTEL_IP_ISP_CSR_v1.0.xlsx.

The spreadsheet is the ABI source (decision DEC-09: CSR wins over the HAS).
It is confidential and is NOT stored in this repository; pass its path on the
command line. The generated files carry the ABI only -- names, offsets, bit
positions, access types, reset values, enum names and the worksheet row of
each item -- and never the spreadsheet's descriptive text. The generated files are kept in the source tree so that the C++
build never needs Python or the spreadsheet:

  docs/csr/fx1_isp_csr_schema.json   machine-readable schema, cell traceability
  include/fx1_isp/fx1_isp_csr.h      C/C++ register header for FW/SW
  src/registers/csr_table_gen.cpp    register table consumed by the model

Every deviation from the literal spreadsheet content is an explicit entry in
OVERRIDES below, carries a decision/issue ID, and is asserted against the
spreadsheet text so that a new revision of the map fails loudly instead of
silently inheriting a stale override.

Usage:
  gen_csr.py --xlsx <path/to/FTEL_IP_ISP_CSR_v1.0.xlsx> [--out <isp dir>] [--check]
"""

import argparse
import hashlib
import json
import os
import re
import sys
import tempfile

try:
    import openpyxl
except ImportError:  # pragma: no cover - environment problem, not a bug
    sys.exit("gen_csr.py: openpyxl is required (pip install openpyxl)")

GENERATOR_VERSION = "1.0"
EXPECTED_SHA256 = "0980b61ded06a386f0cc64e0445f93fd85b0b1f967d37f0cd41869c0753cd6bf"
EXPECTED_COUNTS = {"registers": 226, "fields": 298,
                   "RW": 192, "RO": 75, "W1C": 20, "W1S": 9, "W1SC": 2}
ACCESS_TYPES = ("RW", "RO", "W1C", "W1S", "W1SC")
APERTURE_BYTES = 0x10000

# Overrides of the literal spreadsheet content. Each one is checked against the
# text it overrides (`expect_*`), so it cannot outlive the defect it corrects.
OVERRIDES = [
    {
        "id": "DEC-11",
        "register": "COMMON_BAYER", "field": "pattern", "kind": "enum",
        "reason": "Field description defines [V,H] encoding 0=RGGB 1=GRBG 2=GBRG "
                  "3=BGGR and states the trailing enum list is the obsolete order. "
                  "User decision DEC-11 adopts the description.",
        "expect_enum": {0: "RGGB", 1: "BGGR", 2: "GRBG", 3: "GBRG"},
        "enum": {0: "RGGB", 1: "GRBG", 2: "GBRG", 3: "BGGR"},
    },
    {
        "id": "SPEC-05",
        "register": "DMA_IRQ_EN", "field": "irq_en", "kind": "tied_zero",
        "reason": "Bit 0 is reserved and tied to zero (no IRQ_CFG_ERROR, SPEC-05).",
        "expect_text": "Bit 0 is reserved and tied to zero",
        "bits": [0],
    },
    {
        "id": "SPEC-05",
        "register": "DMA_IRQ_STAT", "field": "irq", "kind": "tied_zero",
        "reason": "Bit 0 is reserved and tied to zero (no IRQ_CFG_ERROR, SPEC-05).",
        "expect_text": "Bit 0 is reserved and tied to zero",
        "bits": [0],
    },
    {
        "id": "CSR-DMA-ERR-5",
        "register": "DMA_ERR", "field": "err", "kind": "tied_zero",
        "reason": "Cause list marks bit 5 as reserved, tied to zero.",
        "expect_text": "5 (reserved)         tied to zero",
        "bits": [5],
    },
]

ENUM_VALUE_RE = re.compile(r"^\s*0x([0-9A-Fa-f]+)\s*=\s*([A-Za-z][A-Za-z0-9_]*)")
ENUM_BIT_RE = re.compile(r"^\s*bit\s+(\d+)\s*=\s*([A-Za-z][A-Za-z0-9_]*)")
NOT_IMPL_RE = re.compile(r"not implemented|no hardware", re.IGNORECASE)


def fail(msg):
    sys.exit("gen_csr.py: ERROR: " + msg)


def sha256_of(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def parse_int(text, what):
    try:
        return int(str(text).strip(), 0)
    except ValueError:
        fail("cannot parse %s %r" % (what, text))


def parse_bits(text, what):
    m = re.fullmatch(r"\[(\d+)(?::(\d+))?\]", str(text).strip())
    if not m:
        fail("cannot parse bit range %r of %s" % (text, what))
    msb = int(m.group(1))
    lsb = int(m.group(2)) if m.group(2) is not None else msb
    if lsb > msb:
        fail("inverted bit range %r of %s" % (text, what))
    return msb, lsb


def read_sheet(ws):
    """Return the register list of one worksheet, with source row numbers."""
    header = [c.value for c in ws[1]]
    want = ["Address", "Register Name", "Field Name", "Bit Range", "Width",
            "Type", "Reset Value", "Description"]
    if header[:8] != want:
        fail("unexpected header in sheet %r: %r" % (ws.title, header))
    regs = []
    for row_idx, row in enumerate(ws.iter_rows(min_row=2, values_only=True), start=2):
        addr, rname, fname, bits, width, acc, rst, desc = row[:8]
        if all(v is None for v in row[:8]):
            continue
        desc = (desc or "").strip()
        if rname is not None:
            regs.append({"offset": parse_int(addr, "address"), "name": str(rname).strip(),
                         "description": desc, "row": row_idx, "fields": []})
            continue
        if not regs:
            fail("field row %d before any register" % row_idx)
        reg = regs[-1]
        what = "%s.%s" % (reg["name"], fname)
        msb, lsb = parse_bits(bits, what)
        if width is None or int(width) != msb - lsb + 1:
            fail("width %r disagrees with bit range %r for %s" % (width, bits, what))
        acc = str(acc).strip()
        if acc not in ACCESS_TYPES:
            fail("unknown access type %r for %s" % (acc, what))
        enums, bit_names = {}, {}
        for line in desc.splitlines():
            m = ENUM_VALUE_RE.match(line)
            if m:
                enums[int(m.group(1), 16)] = m.group(2)
                continue
            m = ENUM_BIT_RE.match(line)
            if m:
                bit_names[int(m.group(1))] = m.group(2)
        reg["fields"].append({
            "name": str(fname).strip(), "msb": msb, "lsb": lsb, "width": msb - lsb + 1,
            "access": acc, "reset": parse_int(rst, "reset of " + what),
            "description": desc, "row": row_idx, "enums": enums, "bits": bit_names,
            "implemented": not (NOT_IMPL_RE.search(desc) or
                                "NOT IMPLEMENTED" in reg["description"]),
            "tied_zero_bits": [],
        })
    return regs


def normalise(regs):
    return [(r["offset"], r["name"], r["description"],
             [(f["name"], f["msb"], f["lsb"], f["access"], f["reset"], f["description"])
              for f in r["fields"]]) for r in regs]


def apply_overrides(regs):
    by_name = {r["name"]: r for r in regs}
    applied = []
    for ov in OVERRIDES:
        reg = by_name.get(ov["register"])
        if reg is None:
            fail("override %s: register %s missing" % (ov["id"], ov["register"]))
        fld = next((f for f in reg["fields"] if f["name"] == ov["field"]), None)
        if fld is None:
            fail("override %s: field %s.%s missing" % (ov["id"], ov["register"], ov["field"]))
        if ov["kind"] == "enum":
            if fld["enums"] != ov["expect_enum"]:
                fail("override %s: %s.%s enum is %r, expected %r -- re-review the map"
                     % (ov["id"], ov["register"], ov["field"], fld["enums"], ov["expect_enum"]))
            fld["enums"] = dict(ov["enum"])
        elif ov["kind"] == "tied_zero":
            if ov["expect_text"] not in fld["description"]:
                fail("override %s: text %r not found in %s.%s -- re-review the map"
                     % (ov["id"], ov["expect_text"], ov["register"], ov["field"]))
            for b in ov["bits"]:
                if not fld["lsb"] <= b <= fld["msb"]:
                    fail("override %s: bit %d outside field" % (ov["id"], b))
            fld["tied_zero_bits"] = sorted(set(fld["tied_zero_bits"]) | set(ov["bits"]))
        else:
            fail("unknown override kind %r" % ov["kind"])
        applied.append({k: v for k, v in ov.items() if not k.startswith("expect_")})
    return applied


def field_mask(f):
    mask = ((1 << f["width"]) - 1) << f["lsb"]
    for b in f["tied_zero_bits"]:
        mask &= ~(1 << b)
    return mask


def validate(regs):
    names, offsets = set(), set()
    counts = {a: 0 for a in ACCESS_TYPES}
    prev = -1
    for r in regs:
        off = r["offset"]
        if off % 4 or off >= APERTURE_BYTES:
            fail("register %s offset 0x%X misaligned or outside aperture" % (r["name"], off))
        if off <= prev:
            fail("register %s offset 0x%X not ascending" % (r["name"], off))
        prev = off
        if r["name"] in names or off in offsets:
            fail("duplicate register name/offset %s 0x%X" % (r["name"], off))
        names.add(r["name"])
        offsets.add(off)
        if not r["fields"]:
            fail("register %s has no fields" % r["name"])
        used, fnames = 0, set()
        for f in r["fields"]:
            if f["msb"] > 31:
                fail("%s.%s exceeds 32 bits" % (r["name"], f["name"]))
            if f["name"] in fnames:
                fail("duplicate field %s.%s" % (r["name"], f["name"]))
            fnames.add(f["name"])
            full = ((1 << f["width"]) - 1) << f["lsb"]
            if used & full:
                fail("overlapping field %s.%s" % (r["name"], f["name"]))
            used |= full
            if f["reset"] >> f["width"]:
                fail("reset 0x%X does not fit %s.%s" % (f["reset"], r["name"], f["name"]))
            counts[f["access"]] += 1
    got = dict(counts, registers=len(regs), fields=sum(len(r["fields"]) for r in regs))
    for k, v in EXPECTED_COUNTS.items():
        if got[k] != v:
            fail("count %s = %d, expected %d" % (k, got[k], v))
    return got


def register_summary(r):
    masks = {a: 0 for a in ACCESS_TYPES}
    reset = 0
    for f in r["fields"]:
        m = field_mask(f)
        masks[f["access"]] |= m
        reset |= (f["reset"] << f["lsb"]) & m
    return masks, reset


def c_ident(text):
    return re.sub(r"[^A-Za-z0-9]", "_", text).upper()


def gen_header(regs, src):
    out = []
    w = out.append
    w("/* SPDX-License-Identifier: Apache-2.0 */")
    w("/*")
    w(" * FX1 ISP register map -- GENERATED FILE, DO NOT EDIT.")
    w(" * Generator : tools/gen_csr.py v%s" % GENERATOR_VERSION)
    w(" * Source    : %s" % src["file"])
    w(" * SHA-256   : %s" % src["sha256"])
    w(" * Map stamp : %s" % src["generated_at"])
    w(" *")
    w(" * Offsets are byte offsets relative to the ISP base address, which the")
    w(" * platform assigns. All registers are 32-bit; address bits [1:0] are ignored.")
    w(" * Deviations from the literal spreadsheet are listed in")
    w(" * docs/csr/fx1_isp_csr_schema.json (\"overrides\").")
    w(" */")
    w("#ifndef FX1_ISP_CSR_H")
    w("#define FX1_ISP_CSR_H")
    w("")
    w("#define FX1_ISP_CSR_APERTURE_BYTES 0x%05Xu" % APERTURE_BYTES)
    w("#define FX1_ISP_CSR_NUM_REGISTERS %du" % len(regs))
    w("")
    for r in regs:
        rn = "FX1_ISP_" + c_ident(r["name"])
        masks, reset = register_summary(r)
        # No spreadsheet prose here: the source is confidential (see the module
        # docstring); the worksheet row is the pointer back to the description.
        w("/* 0x%04X %s (CSR row %d) */" % (r["offset"], r["name"], r["row"]))
        w("#define %s_OFFSET 0x%04Xu" % (rn, r["offset"]))
        w("#define %s_RESET 0x%08Xu" % (rn, reset))
        for f in r["fields"]:
            fn = rn + "_" + c_ident(f["name"])
            note = "" if f["implemented"] else " (no hardware behind this field)"
            w("/*   %s [%d:%d] %s%s */" % (f["name"], f["msb"], f["lsb"], f["access"], note))
            w("#define %s_SHIFT %du" % (fn, f["lsb"]))
            w("#define %s_WIDTH %du" % (fn, f["width"]))
            w("#define %s_MASK 0x%08Xu" % (fn, field_mask(f)))
            w("#define %s_RESET 0x%Xu" % (fn, f["reset"]))
            for val, name in sorted(f["enums"].items()):
                w("#define %s_%s 0x%Xu" % (fn, c_ident(name), val))
            for bit, name in sorted(f["bits"].items()):
                if bit not in f["tied_zero_bits"]:
                    w("#define %s_%s_BIT (1u << %du)" % (fn, c_ident(name), bit))
        w("")
    w("#endif /* FX1_ISP_CSR_H */")
    return "\n".join(out) + "\n"


def c_str(text):
    return '"' + text.replace("\\", "\\\\").replace('"', '\\"') + '"'


def gen_table(regs, src):
    out = []
    w = out.append
    w("// SPDX-License-Identifier: Apache-2.0")
    w("// FX1 ISP register table -- GENERATED FILE, DO NOT EDIT.")
    w("// Generator: tools/gen_csr.py v%s, source SHA-256 %s" % (GENERATOR_VERSION, src["sha256"]))
    w("")
    w('#include "registers/csr_desc.h"')
    w("")
    w("namespace cdc::components::fx1_isp::csr {")
    w("namespace {")
    w("")
    for i, r in enumerate(regs):
        w("constexpr field_desc fields_%d[] = {  // %s" % (i, r["name"]))
        for f in r["fields"]:
            w("   {%s, %d, %d, 0x%08Xu, access::%s, 0x%Xu, %s, %d}," % (
                c_str(f["name"]), f["lsb"], f["width"], field_mask(f), f["access"].lower(),
                f["reset"], "true" if f["implemented"] else "false", f["row"]))
        w("};")
    w("")
    w("}  // namespace")
    w("")
    w("const reg_desc registers[] = {")
    for i, r in enumerate(regs):
        masks, reset = register_summary(r)
        w("   {0x%04Xu, %s, 0x%08Xu, 0x%08Xu, 0x%08Xu, 0x%08Xu, 0x%08Xu, 0x%08Xu, fields_%d, %d, %d}," % (
            r["offset"], c_str(r["name"]), reset, masks["RW"], masks["RO"], masks["W1C"],
            masks["W1S"], masks["W1SC"], i, len(r["fields"]), r["row"]))
    w("};")
    w("")
    w("const std::size_t num_registers = sizeof(registers) / sizeof(registers[0]);")
    w("")
    w("}  // namespace cdc::components::fx1_isp::csr")
    return "\n".join(out) + "\n"


def gen_schema(regs, src, applied, counts):
    doc = {
        "generator": "tools/gen_csr.py", "generator_version": GENERATOR_VERSION,
        "source": src, "counts": counts, "overrides": applied,
        "access_semantics": "HAS Table 8-1 / Table 7-10; see docs/ISP_CSR_CONTRACT.md",
        "registers": [],
    }
    for r in regs:
        masks, reset = register_summary(r)
        doc["registers"].append({
            "offset": "0x%04X" % r["offset"], "name": r["name"], "row": r["row"],
            "reset": "0x%08X" % reset,
            "masks": {a: "0x%08X" % m for a, m in masks.items()},
            "fields": [{
                "name": f["name"], "bits": "[%d:%d]" % (f["msb"], f["lsb"]),
                "access": f["access"], "reset": "0x%X" % f["reset"], "row": f["row"],
                "implemented": f["implemented"], "tied_zero_bits": f["tied_zero_bits"],
                "enums": {str(k): v for k, v in sorted(f["enums"].items())},
                "bit_names": {str(k): v for k, v in sorted(f["bits"].items())},
            } for f in r["fields"]],
        })
    return json.dumps(doc, indent=2, ensure_ascii=False) + "\n"


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--xlsx", required=True, help="path to FTEL_IP_ISP_CSR_v1.0.xlsx")
    ap.add_argument("--out", default=os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."),
                    help="component root (default: parent of tools/)")
    ap.add_argument("--check", action="store_true",
                    help="verify the files in the tree match a fresh generation")
    ap.add_argument("--allow-new-revision", action="store_true",
                    help="accept a spreadsheet whose SHA-256 differs from the pinned one")
    args = ap.parse_args()

    digest = sha256_of(args.xlsx)
    if digest != EXPECTED_SHA256 and not args.allow_new_revision:
        fail("spreadsheet SHA-256 %s differs from pinned %s; review the new revision and "
             "rerun with --allow-new-revision" % (digest, EXPECTED_SHA256))

    wb = openpyxl.load_workbook(args.xlsx, data_only=True)
    summary = {str(r[0]).strip(): r[1] for r in wb["Summary"].iter_rows(values_only=True)
               if r and r[0] is not None}
    regs = read_sheet(wb["Registers"])
    if normalise(regs) != normalise(read_sheet(wb["Flat Registers"])):
        fail("sheets 'Registers' and 'Flat Registers' disagree")
    applied = apply_overrides(regs)
    counts = validate(regs)
    src = {"file": os.path.basename(args.xlsx), "sha256": digest, "sheet": "Registers",
           "generated_at": str(summary.get("Generated At", "")),
           "module": str(summary.get("Module Name", ""))}

    outputs = {
        os.path.join("docs", "csr", "fx1_isp_csr_schema.json"): gen_schema(regs, src, applied, counts),
        os.path.join("include", "fx1_isp", "fx1_isp_csr.h"): gen_header(regs, src),
        os.path.join("src", "registers", "csr_table_gen.cpp"): gen_table(regs, src),
    }
    stale = []
    for rel, text in outputs.items():
        path = os.path.join(args.out, rel)
        if args.check:
            try:
                with open(path, encoding="utf-8") as f:
                    if f.read() != text:
                        stale.append(rel)
            except FileNotFoundError:
                stale.append(rel)
            continue
        os.makedirs(os.path.dirname(path), exist_ok=True)
        fd, tmp = tempfile.mkstemp(dir=os.path.dirname(path))
        with os.fdopen(fd, "w", encoding="utf-8") as f:
            f.write(text)
        os.chmod(tmp, 0o644)
        os.replace(tmp, path)
        print("wrote", rel)
    if stale:
        fail("generated files out of date: " + ", ".join(stale))
    print("registers=%(registers)d fields=%(fields)d RW=%(RW)d RO=%(RO)d W1C=%(W1C)d "
          "W1S=%(W1S)d W1SC=%(W1SC)d" % counts)


if __name__ == "__main__":
    main()
