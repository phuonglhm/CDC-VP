# FX1 ISP VP — Source manifest (P00)

The specification documents are FPT Telecom confidential and are **not** stored
in this repository. They are pinned by path and SHA-256 so every artefact in
this component can be traced to an exact revision.

| Document | Location used during development | SHA-256 | Revision marker |
|---|---|---|---|
| HAS (PDF, 255 pages) | `~/Documents/work/VP_BK/ISP/doc/FTEL_IP_ISP_HAS_v1.0.pdf` | `239a0f104e8c55d22f4593ef7bd415bb6aeb3c624f1e590b434e4ba429100b93` | Cover date September 24, 2026; PDF CreationDate 2026-09-24 12:19 +07 |
| HAS (DOCX) | `~/Documents/work/VP_BK/ISP/doc/FTEL_IP_ISP_HAS_v1.0.docx` | `ff82fd03b67abcbadafc21660cf19868d384bbbaaf80aa74989611c8e9a5134f` | Same title; not compared line by line against the PDF |
| CSR map (XLSX) | `~/Documents/work/VP_BK/ISP/doc/FTEL_IP_ISP_CSR_v1.0.xlsx` | `0980b61ded06a386f0cc64e0445f93fd85b0b1f967d37f0cd41869c0753cd6bf` | Summary: module `ftel_isp_csr`, generated `2026-09-23T02:11:27.964Z` |

## Priority

1. **CSR XLSX** for ABI and for any behaviour the CSR describes (DEC-09).
2. **HAS** for algorithms and for behaviour the CSR does not describe.
3. Contradictions inside one document, and requirements that have no register,
   are tracked in [ISP_DECISIONS_AND_DISCREPANCIES.md](ISP_DECISIONS_AND_DISCREPANCIES.md).

## How the sources were read

- CSR: parsed with `openpyxl` by [tools/gen_csr.py](../tools/gen_csr.py). The
  generator refuses a spreadsheet whose SHA-256 differs from the pinned value
  unless `--allow-new-revision` is given, checks that the `Registers` and
  `Flat Registers` sheets agree, and asserts the expected counts
  (226 registers, 298 fields; 192 RW, 75 RO, 20 W1C, 9 W1S, 2 W1SC).
- HAS: text extracted from the PDF with `pdftotext -layout`. Figures and
  diagrams are not machine-readable; where a figure carries information that
  is not in the text, the relevant block review (M3) must consult the PDF
  directly. The DOCX has not been diffed against the PDF (open item P00-1).

## Regenerating the CSR artefacts

```bash
python3 tools/gen_csr.py --xlsx <path>/FTEL_IP_ISP_CSR_v1.0.xlsx          # write
python3 tools/gen_csr.py --xlsx <path>/FTEL_IP_ISP_CSR_v1.0.xlsx --check  # verify tree
```

The outputs carry the ABI only: names, offsets, bit positions, access types,
reset values, enum/bit names, and the worksheet row of every register and
field. They contain no descriptive text from the spreadsheet. Anyone with
access to the source can follow a row number back to its description.

Outputs (kept in the tree; the C++ build does not need Python):

- `docs/csr/fx1_isp_csr_schema.json` — every register/field with its worksheet row
- `include/fx1_isp/fx1_isp_csr.h` — C header for FW/SW
- `src/registers/csr_table_gen.cpp` — register table used by the model

## Other inputs

- RAW test images: `~/Documents/work/VP_BK/ISP/Image Source/` (28 RAW, BGGR,
  RAW10 at bits [15:6] of LE uint16, 2688×1520). Inventory and checksums:
  `~/Documents/work/VP_BK/ISP/plan/ISP_RAW_INVENTORY.json`. Not copied into
  the repository (CONTRIBUTING: no media datasets).
- Plan: `~/Documents/work/VP_BK/ISP/plan/ISP_VP_PLAN.md`.

## Open items

- **P00-1** DOCX vs PDF not compared in full. Low risk for M1 (register
  behaviour comes from the XLSX); to be spot-checked per block in M3.
