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
      deferred   defined by the pinned CSR revision but not modelled yet: the
                 register file stores it (RO words read their reset value) and
                 the entry names the milestone that implements it
  - the specification sections and decisions that define its behaviour;
  - the dedicated tests that name it (tests/**/*.cpp, tools/run_raw.cpp) and
    the vector data that writes or reads it (profiles, between-frame writes,
    G-CONT script, expected statistics readouts).
Every register is also covered by the generic access sweep (unit test
test_access_sweep) and by the XLSX cross-check (tools/check_csr_header.py).

The generator fails if a register falls in no class, if a `model` register
has no dedicated test and no vector use, or if a `deferred` register gains a
model reference (move it to `model` with its tests): a new register or a lost
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
    (r"EE_(ALPHA|BETA|CLAMP|FEATURE_EN|RADIAL_CENTER|RADIAL_R2_TH[0-2]|RADIAL_GAIN_01|RADIAL_GAIN_23)$",
     "EE profile scalar k at EE_ALPHA + 4 * k, banked per profile (MAS §6.18.4.1)", ("EE_ALPHA",)),
    (r"IDMA_BUF_ADDR_[LH][0-3]$", "64-bit buffer address, IDMA_BUF_ADDR_L0 + 8 * k", ("IDMA_BUF_ADDR_L0",)),
    (r"ODMA_(Y|UV)_ADDR_[LH][0-3]$", "64-bit buffer address, ODMA_Y_ADDR_L0 + 16 * k",
     ("ODMA_Y_ADDR_L0", "ODMA_UV_ADDR_L0")),
    (r"IDMA_BUF_(TAG|TS)[0-3]$", "input buffer metadata, IDMA_BUF_TAG0/TS0 + 4 * k, captured at frame start (U5)",
     ("IDMA_BUF_TAG0", "IDMA_BUF_TS0")),
    (r"ODMA_BUF_(TAG|TS|SOF_TS|EOF_TS)[0-3]$",
     "output buffer metadata, ODMA_BUF_TAG0/TS0/SOF_TS0/EOF_TS0 + 4 * k, published at completion (U5)",
     ("ODMA_BUF_TAG0", "ODMA_BUF_TS0", "ODMA_BUF_SOF_TS0", "ODMA_BUF_EOF_TS0")),
    (r"CH[12]_ODMA_(Y|UV)_ADDR_[LH][0-3]$", "64-bit buffer address of CH1/CH2, CHc_ODMA_Y/UV_ADDR_L0 + 16 * k "
     "(MAS v1.07 §6.25.3.8)", ("CH1_ODMA_Y_ADDR_L0", "CH1_ODMA_UV_ADDR_L0", "CH2_ODMA_Y_ADDR_L0")),
    (r"CH[12]_ODMA_BUF_(TAG|TS|SOF_TS|EOF_TS)[0-3]$", "CH1/CH2 output buffer metadata, + 4 * k, published at "
     "DONE (MAS v1.07 Table 6-159)", ("CH1_ODMA_BUF_TAG0", "CH1_ODMA_BUF_TS0", "CH1_ODMA_BUF_SOF_TS0",
                                      "CH1_ODMA_BUF_EOF_TS0", "CH2_ODMA_BUF_TAG0", "CH2_ODMA_BUF_TS0",
                                      "CH2_ODMA_BUF_SOF_TS0", "CH2_ODMA_BUF_EOF_TS0")),
    (r"TUNE_CH[0-2]_BUF[0-3]_(FRAME|PROFILE)$", "ingress frame ID / profile ID of channel c buffer k, + 4 * k, "
     "published at DONE (MAS v1.07 §9.9.9)", tuple("TUNE_CH%d_BUF0_%s" % (c, f) for c in range(3)
                                                    for f in ("FRAME", "PROFILE"))),
    (r"TUNE_HIST_WEIGHT([0-9]|1[0-9]|2[0-4])$", "weighted-histogram cell weight k at TUNE_HIST_WEIGHT0 + 4 * k "
     "(MAS v1.07 §6.21.3.4)", ("TUNE_HIST_WEIGHT0",)),
    (r"TUNE_AF_W([0-9]|1[0-5])_(START|END)$", "AF rectangle k at TUNE_AF_W0_START/END + k * window stride "
     "(MAS v1.07 §6.23)", ("TUNE_AF_W0_START", "TUNE_AF_W0_END")),
    (r"TUNE_AWB_E[0-7]_(CENTER|A|B|C|RADIUS2)$", "AWB ellipse k at TUNE_AWB_E0_* + k * ellipse stride "
     "(MAS v1.07 §6.22.3.9)",
     ("TUNE_AWB_E0_CENTER", "TUNE_AWB_E0_A", "TUNE_AWB_E0_B", "TUNE_AWB_E0_C", "TUNE_AWB_E0_RADIUS2")),
]
CONSTANT = {"COMMON_VER_DATE", "COMMON_VER_ID", "TUNE_ID", "TUNE_REV"}
STORAGE = {
    "BPC_STATUS": "W1C status with no source in the VP (the candidate list never overflows in this model); "
                  "not an ERROR_IRQ source (MAS v1.07 Table 9-188, review V107-7..10 R7)",
    "RESIZER_CUSTOM_W": "reserved (CSR v1.07): reads zero, writes ignored; no custom targets (MAS §6.20.4.2)",
    "RESIZER_CUSTOM_H": "reserved (CSR v1.07): reads zero, writes ignored; no custom targets (MAS §6.20.4.2)",
    "CH1_RESIZER_CUSTOM_W": "reserved (CSR v1.07): reads zero, writes ignored; no custom targets (MAS §6.20.4.2)",
    "CH1_RESIZER_CUSTOM_H": "reserved (CSR v1.07): reads zero, writes ignored; no custom targets (MAS §6.20.4.2)",
    "CH2_RESIZER_CUSTOM_W": "reserved (CSR v1.07): reads zero, writes ignored; no custom targets (MAS §6.20.4.2)",
    "CH2_RESIZER_CUSTOM_H": "reserved (CSR v1.07): reads zero, writes ignored; no custom targets (MAS §6.20.4.2)",
    "COMMON_SCRATCH": "software scratch register by definition",
    "BPC_MODE": "DYNAMIC_DET_EN has no effect: spatial BPC only (MAS §6.9; DEC-28 superseded)",
    "BPC_TEMPORAL_VAR": "no effect: spatial BPC only (MAS §6.9; DEC-28 superseded)",
    "BPC_PIXEL_AGE": "no effect: spatial BPC only (MAS §6.9; DEC-28 superseded)",
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
# Registers of CSR v1.07 that the model does not implement yet (plan
# ISP_V107_UPDATE_PLAN_2026-10-28; milestone per register from the V107-0 field
# matrix). Regex -> reason. An entry must disappear in the milestone that models
# the register; first match wins. RO words read their reset value, TUNE_CAPS
# reports none of their capabilities (V107-A1).
# Since V107-3 every TUNE_* extension control is bank storage (profile
# controller); "deferred" means its datapath function is not modelled yet.
DEFERRED = []
# Block prefix -> specification and decisions (first match wins).
BLOCKS = [
    ("COMMON_RECOVERY_", "MAS v1.07 §9.9.8; V107-D62..D67"),
    ("COMMON_ABORT", "MAS v1.07 §9.9.8; V107-D62, V107-D64, V107-D67"),
    ("COMMON_", "HAS §9.1-9.2, §6.5-6.6; DEC-14..17, DEC-20, DEC-38, CSR-01..10"),
    ("BLC_", "HAS §6.7; ALG-BLC"),
    ("LSC_", "HAS §6.8; ALG-LSC, DEC-31"),
    ("BPC_", "MAS §6.9; ALG-BPC, DEC-28 superseded (U2)"),
    ("WB_", "HAS §6.10; ALG-WB"),
    ("DG_", "HAS §6.11; ALG-DG"),
    ("DEMOSAIC_", "HAS §6.12; ALG-DMS, DEC-38"),
    ("D_WDR_", "HAS §3.1 (unsupported); M5-A1"),
    ("CCM_", "HAS §6.13; DEC-24/26, DEC-38"),
    ("GAMMA_", "HAS §6.14; DEC-25, CSR-16/21, DEC-38"),
    ("CSC_", "HAS §6.15; SPEC-03"),
    ("GTM_", "HAS §6.16, MAS p146 (LUT writes only while EN = 0, U3); ALG-GTM, DEC-38"),
    ("NR_2D_", "HAS §6.17; ALG-2DNR, DEC-38"),
    ("TNR_3D_", "HAS §3.1 (unsupported); M5-A1"),
    ("EE_", "MAS §6.18.4.1 (three resident profiles, U3); DEC-25 superseded, M3-A2, DEC-38"),
    ("CNF_", "HAS §6.19; DEC-27, ALG-CNF, DEC-38"),
    ("RESIZER_", "HAS §6.20; DEC-23/24/26, CSR-09, DEC-38"),
    ("AEC_", "HAS §6.21, §9.5, MAS §6.21.4; ALG-AEC, DEC-29/30, DEC-32 superseded"),
    ("AWB_", "MAS §6.22.4-6.22.5 (result banks, U2); ALG-AWB, DEC-29, DEC-30 superseded for AWB"),
    ("AF_", "MAS §6.23 (EN sampled at SOF, U2); ALG-AF-01/02 superseded, CSR-07/15"),
    ("OFMT_", "HAS §6.24; DEC-39, owner query B14"),
    ("DMA_", "HAS §6.25, MAS Table 161 (ODMA_OVERFLOW held while stalled, U5); DEC-14/15/19, DEC-18 status superseded, CSR-06"),
    ("IDMA_BUF_TAG", "MAS §6.25.4.1.0.3 Table 160 (U5)"),
    ("IDMA_BUF_TS", "MAS §6.25.4.1.0.3 Table 160 (U5)"),
    ("ODMA_BUF_TAG", "MAS §6.25.4.1.0.3 Table 160 (U5)"),
    ("ODMA_BUF_TS", "MAS §6.25.4.1.0.3 Table 160 (U5)"),
    ("ODMA_BUF_SOF_TS", "MAS §6.25.4.1.0.3 Table 160 (U5)"),
    ("ODMA_BUF_EOF_TS", "MAS §6.25.4.1.0.3 Table 160 (U5)"),
    ("ODMA_BUF_FREE", "MAS Table 156/159 (cleared at frame start, U5); SPEC-07 superseded"),
    ("IDMA_", "HAS §6.25.8, §6.25.10; V107-D62..D65, M2-A1..A3"),
    ("ODMA_", "HAS §6.25.9-6.25.10; DEC-18/19"),
    ("TUNE_", "MAS §8.2 (discovery, V107-1; TUNE_CAPS V107-A1), §9.7-9.8; CSR v1.07 TUNE page"),
    ("CH0_", "MAS §6.20 (CH0 crop and geometry, CSR v1.07)"),
    ("CH1_", "MAS §6.20, §6.25 (CH1 output, CSR v1.07)"),
    ("CH2_", "MAS §6.20, §6.25 (CH2 RGB888 output, CSR v1.07)"),
    ("OUTPUT_", "MAS §6.24, §9.5 (output enable and interrupts, CSR v1.07)"),
    ("FRAME_COUNTER", "MAS §6.25.4.1.0.3 Table 160 (core-clock time base, U5-A1)"),
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
    # profile_fields.cpp only classifies the bank-owned words (V107-3): bank
    # storage is not a model of the field's function.
    src = {k: v for k, v in src.items() if "csr_table_gen" not in k and "profile_fields" not in k}
    tests = read_all(["tests/**/*.cpp", "tools/run_raw.cpp"])
    data = read_all(["tests/data/vectors/*/profile.csrw", "tests/data/vectors/*/frames.csrw",
                     "tests/data/vectors/*/expected_stats.txt", "tests/data/sequences/*/sequence.txt",
                     "tests/data/sequences/*/stats_*.txt", "tests/data/4k/*/profile.csrw",
                     "tests/data/4k/*/expected_stats.txt"])
    rows, errors = [], []
    counts = {"model": 0, "model (indexed)": 0, "constant": 0, "storage": 0, "deferred": 0}
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
        deferred = next((why for pat, why in DEFERRED if re.match(pat, name)), None)
        if deferred and model_files == ["control_unit_profile.cpp"]:
            # The profile controller validates the field at SEAL or rejects the
            # command by mode (V107-3); its datapath function is still deferred.
            deferred += "; bank storage, SEAL/mode checks since V107-3"
        elif deferred and model_files:
            errors.append("%s: listed as deferred but the model references it (%s) -- classify it as model"
                          % (name, ", ".join(model_files)))
        if name in CONSTANT:
            cls, note = "constant", "read-only identification value"
        elif deferred:
            cls, note = "deferred", deferred
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
        "hardware behind it, readback only, with the reason; **deferred** = defined by the pinned CSR revision,",
        "not modelled yet, with the milestone that implements it. *Vector uses* = number of vector directories",
        "or G-CONT/4K data files that write or read the register. For indexed groups, *dedicated tests*",
        "lists tests that address the group by base plus index: coverage at group level, which does not",
        "show that every member was written with a non-zero value.",
        "",
        "| Class | Registers |",
        "|---|---|",
    ]
    for k in ("model", "model (indexed)", "constant", "storage", "deferred"):
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
