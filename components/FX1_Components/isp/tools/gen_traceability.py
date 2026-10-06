#!/usr/bin/env python3
"""Per-register CSR traceability report (M5): docs/ISP_CSR_TRACEABILITY.md.

For each of the registers of the generated schema
(docs/csr/fx1_isp_csr_schema.json) the report gives:
  - offset, field access types and source row of the CSR map;
  - the model class:
      model      the model reads or drives it (macro reference in src/, or an
                 indexed register group whose base is referenced)
      constant   read-only identification value
      storage    no hardware behind it, with the reason (CSR text, DEC/M5 id)
  - the specification sections and decisions that define its behaviour;
  - the dedicated tests that name it (tests/**/*.cpp, tools/run_raw.cpp) and
    the vector data that writes or reads it (profiles, between-frame writes,
    G-CONT script, expected statistics readouts).
Every register is also covered by the generic access sweep (unit test
test_access_sweep) and by the XLSX cross-check (tools/check_csr_header.py).

The generator fails if a register falls in no class, or if a `model`
register has no dedicated test and no vector use: a new register or a lost
reference cannot pass unnoticed. --check verifies the committed report.
"""

import argparse
import glob
import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
OUT = os.path.join(ROOT, "docs", "ISP_CSR_TRACEABILITY.md")

# Register groups the model accesses by base offset plus index (no per-register macro).
# The third element names the base registers; a test that addresses
# `FX1_ISP_<base>_OFFSET + ...` exercises every member of the group.
INDEXED = [
    (r"BLC_OFS_G[0-3]_(DFT|R|GR|GB|B)$", "BLC profile g at BLC_OFS_G0_DFT + g * profile stride", ()),
    (r"BLC_SCALE_G[0-3]$", "BLC_SCALE_G0 + 4 * g", ()),
    (r"CCM_C[RGB][RGB]$", "CCM_CRR + 4 * index", ("CCM_CRR",)),
    (r"CCM_OFS_[RGB]$", "CCM_OFS_R + 4 * index", ("CCM_OFS_R",)),
    (r"IDMA_BUF_ADDR_[LH][0-3]$", "64-bit buffer address, IDMA_BUF_ADDR_L0 + 8 * k", ("IDMA_BUF_ADDR_L0",)),
    (r"ODMA_(Y|UV)_ADDR_[LH][0-3]$", "64-bit buffer address, ODMA_Y_ADDR_L0 + 16 * k",
     ("ODMA_Y_ADDR_L0", "ODMA_UV_ADDR_L0")),
]
CONSTANT = {"COMMON_VER_DATE", "COMMON_VER_ID"}
STORAGE = {
    "COMMON_SCRATCH": "software scratch register by definition",
    "BPC_TEMPORAL_VAR": "BPC dynamic detection not modelled (DEC-28)",
    "BPC_PIXEL_AGE": "BPC dynamic detection not modelled (DEC-28)",
    "D_WDR_CTRL": "WDR unsupported (HAS §3.1); enable warns (M5-A1)",
    "D_WDR_STRENGTH": "WDR unsupported (HAS §3.1) (M5-A1)",
    "TNR_3D_CTRL": "3DNR unsupported (HAS §3.1); enable warns (M5-A1)",
    "TNR_3D_MTF_SEL": "3DNR unsupported (HAS §3.1) (M5-A1)",
    "TNR_3D_HIST_BASE": "3DNR unsupported (HAS §3.1) (M5-A1)",
    "CNF_STRENGTH": "no hardware behind it (CSR)",
    "AWB_X_BOUND_ADDR": "no hardware: equal zones only (CSR, ALG-AWB-05)",
    "AWB_Y_BOUND_ADDR": "no hardware: equal zones only (CSR, ALG-AWB-05)",
    "AWB_X_BOUND_DATA": "no hardware: equal zones only (CSR, ALG-AWB-05)",
    "AWB_Y_BOUND_DATA": "no hardware: equal zones only (CSR, ALG-AWB-05)",
    "AF_ZONE_EN": "no hardware (CSR, ALG-F §3.2)",
    "AF_METRIC_CFG": "no hardware (CSR, ALG-F §3.2)",
    "AF_LUT_ADDR": "no hardware (CSR, ALG-F §3.2)",
    "AF_LUT_DATA": "no hardware (CSR, ALG-F §3.2)",
    "AF_VCM_CUR_POS": "no hardware (CSR, ALG-F §3.2)",
    "AF_VCM_BEST_POS": "no hardware (CSR, ALG-F §3.2)",
    "AF_PEAK_SCORE": "no hardware (CSR, ALG-F §3.2)",
    "AF_FOCUS_RANGE": "no hardware (CSR, ALG-F §3.2)",
    "OFMT_CTRL": "stride padding not modelled; stride_en warns (DEC-39, owner query B14)",
    "OFMT_Y_STRIDE": "stride padding not modelled (DEC-39)",
    "OFMT_UV_STRIDE": "stride padding not modelled (DEC-39)",
}
# Block prefix -> specification and decisions (first match wins).
BLOCKS = [
    ("COMMON_", "HAS §9.1-9.2, §6.5-6.6; DEC-14..17, DEC-20, DEC-38, CSR-01..10"),
    ("BLC_", "HAS §6.7; ALG-BLC"),
    ("LSC_", "HAS §6.8; ALG-LSC, DEC-31"),
    ("BPC_", "HAS §6.9; ALG-BPC, DEC-28"),
    ("WB_", "HAS §6.10; ALG-WB"),
    ("DG_", "HAS §6.11; ALG-DG"),
    ("DEMOSAIC_", "HAS §6.12; ALG-DMS, DEC-38"),
    ("D_WDR_", "HAS §3.1 (unsupported); M5-A1"),
    ("CCM_", "HAS §6.13; DEC-24/26, DEC-38"),
    ("GAMMA_", "HAS §6.14; DEC-25, CSR-16/21, DEC-38"),
    ("CSC_", "HAS §6.15; SPEC-03"),
    ("GTM_", "HAS §6.16; ALG-GTM, DEC-38"),
    ("NR_2D_", "HAS §6.17; ALG-2DNR, DEC-38"),
    ("TNR_3D_", "HAS §3.1 (unsupported); M5-A1"),
    ("EE_", "HAS §6.18; DEC-25, M3-A2, DEC-38"),
    ("CNF_", "HAS §6.19; DEC-27, ALG-CNF, DEC-38"),
    ("RESIZER_", "HAS §6.20; DEC-23/24/26, CSR-09, DEC-38"),
    ("AEC_", "HAS §6.21, §9.5; ALG-AEC, DEC-29/30/32"),
    ("AWB_", "HAS §6.22, §9.5; ALG-AWB, DEC-29/30"),
    ("AF_", "HAS §6.23, §9.5; ALG-AF, CSR-07/15"),
    ("OFMT_", "HAS §6.24; DEC-39, owner query B14"),
    ("DMA_", "HAS §6.25; DEC-14/15/18/19, CSR-06"),
    ("IDMA_", "HAS §6.25.8, §6.25.10; DEC-19, M2-A1..A3"),
    ("ODMA_", "HAS §6.25.9-6.25.10; DEC-18/19, SPEC-07"),
]


def read_all(patterns):
    out = {}
    for pat in patterns:
        for f in glob.glob(os.path.join(ROOT, pat), recursive=True):
            out[os.path.relpath(f, ROOT)] = open(f, encoding="utf-8", errors="replace").read()
    return out


def access_summary(reg):
    seen = []
    for f in reg["fields"]:
        if f["access"] not in seen:
            seen.append(f["access"])
    return ", ".join(seen) or "—"


def build():
    schema = json.load(open(os.path.join(ROOT, "docs", "csr", "fx1_isp_csr_schema.json")))
    regs = schema["registers"]
    src = read_all(["src/**/*.h", "src/**/*.cpp"])
    src = {k: v for k, v in src.items() if "csr_table_gen" not in k}
    tests = read_all(["tests/**/*.cpp", "tools/run_raw.cpp"])
    data = read_all(["tests/data/vectors/*/profile.csrw", "tests/data/vectors/*/frames.csrw",
                     "tests/data/vectors/*/expected_stats.txt", "tests/data/sequences/*/sequence.txt",
                     "tests/data/sequences/*/stats_*.txt", "tests/data/4k/*/profile.csrw",
                     "tests/data/4k/*/expected_stats.txt"])
    rows, errors = [], []
    counts = {"model": 0, "model (indexed)": 0, "constant": 0, "storage": 0}
    for r in regs:
        name = r["name"]
        macro = re.compile(r"\bFX1_ISP_%s_[A-Z0-9_]*(OFFSET|MASK|SHIFT|BIT)\b" % re.escape(name))
        word = re.compile(r"\b%s\b" % re.escape(name))
        model_files = sorted({os.path.basename(f) for f, t in src.items() if macro.search(t)})
        test_files = sorted({os.path.splitext(os.path.basename(f))[0] for f, t in tests.items()
                             if macro.search(t) or re.search(r'"%s"' % re.escape(name), t)})
        uses = sorted({f.split(os.sep)[3] if f.startswith(os.path.join("tests", "data")) else f
                       for f, t in data.items() if word.search(t)})
        group = next((g for g in INDEXED if re.match(g[0], name)), None)
        indexed = group[1] if group else None
        if group:  # tests that address the group by base + index
            for base in group[2]:
                arith = re.compile(r"\bFX1_ISP_%s_OFFSET\s*\+" % base)
                test_files = sorted(set(test_files) | {os.path.splitext(os.path.basename(f))[0]
                                                       for f, t in tests.items() if arith.search(t)})
        if name in CONSTANT:
            cls, note = "constant", "read-only identification value"
        elif name in STORAGE:
            cls, note = "storage", STORAGE[name]
        elif model_files:
            cls, note = "model", ", ".join(model_files)
        elif indexed:
            cls, note = "model (indexed)", indexed
        else:
            cls, note = None, ""
            errors.append("%s: no model reference and no storage/constant classification" % name)
        if cls in ("model", "model (indexed)") and not test_files and not uses:
            errors.append("%s: modelled but no dedicated test or vector use" % name)
        if cls:
            counts[cls] += 1
        spec = next((s for p, s in BLOCKS if name.startswith(p)), "")
        if not spec:
            errors.append("%s: no block mapping" % name)
        rows.append((r["offset"], name, access_summary(r), cls or "UNCLASSIFIED", note, spec,
                     ", ".join(test_files) or "—", len(uses)))
    return regs, rows, counts, errors


def render(regs, rows, counts):
    lines = [
        "# FX1 ISP VP — CSR traceability (per register)",
        "",
        "Generated by `tools/gen_traceability.py` from the CSR schema, the model sources and the tests;",
        "`--check` (CTest `fx1_isp.reference.traceability_up_to_date`) keeps it current. Do not edit by hand.",
        "",
        "Every register is also covered by the generic access test `test_access_sweep` (all access types,",
        "reset values, reserved bits; the registers with side effects have the dedicated tests named below)",
        "and by the independent XLSX cross-check `tools/check_csr_header.py` (1346 values).",
        "",
        "Classes: **model** = the model reads or drives the register (files listed); **model (indexed)** =",
        "accessed by base offset plus index; **constant** = read-only identification; **storage** = no",
        "hardware behind it, readback only, with the reason. *Vector uses* = number of vector directories",
        "or G-CONT/4K data files that write or read the register. For indexed groups, *dedicated tests*",
        "lists tests that address the group by base plus index: coverage at group level, which does not",
        "show that every member was written with a non-zero value.",
        "",
        "| Class | Registers |",
        "|---|---|",
    ]
    for k in ("model", "model (indexed)", "constant", "storage"):
        lines.append("| %s | %d |" % (k, counts[k]))
    lines.append("| total | %d |" % len(regs))
    lines += ["", "| Offset | Register | Access | Class | Model / reason | Specification, decisions | Dedicated tests | Vector uses |",
              "|---|---|---|---|---|---|---|---|"]
    for off, name, acc, cls, note, spec, tests, uses in rows:
        lines.append("| %s | `%s` | %s | %s | %s | %s | %s | %d |" % (off, name, acc, cls, note, spec, tests, uses))
    return "\n".join(lines) + "\n"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--check", action="store_true")
    a = ap.parse_args()
    regs, rows, counts, errors = build()
    if errors:
        sys.exit("gen_traceability.py: coverage gate failed:\n  " + "\n  ".join(errors))
    text = render(regs, rows, counts)
    if a.check:
        if not os.path.exists(OUT) or open(OUT, encoding="utf-8").read() != text:
            sys.exit("gen_traceability.py: %s is out of date" % OUT)
        print("checked %s: %d registers" % (os.path.relpath(OUT, ROOT), len(regs)))
    else:
        open(OUT, "w", encoding="utf-8").write(text)
        print("wrote %s: %s" % (os.path.relpath(OUT, ROOT), ", ".join("%s %d" % kv for kv in counts.items())))


if __name__ == "__main__":
    main()
