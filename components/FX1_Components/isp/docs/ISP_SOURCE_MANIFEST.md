# FX1 ISP VP — Source manifest (P00)

The specification documents are FPT Telecom confidential and are **not** stored
in this repository. They are pinned by path and SHA-256 so every artefact in
this component can be traced to an exact revision.

| Document | Location used during development | SHA-256 | Revision marker |
|---|---|---|---|
| HAS (PDF, 255 pages) | `~/Documents/work/VP_BK/ISP/doc/FTEL_IP_ISP_HAS_v1.0.pdf` | `239a0f104e8c55d22f4593ef7bd415bb6aeb3c624f1e590b434e4ba429100b93` | Cover date September 24, 2026; PDF CreationDate 2026-09-24 12:19 +07 |
| HAS (DOCX) | `~/Documents/work/VP_BK/ISP/doc/FTEL_IP_ISP_HAS_v1.0.docx` | `ff82fd03b67abcbadafc21660cf19868d384bbbaaf80aa74989611c8e9a5134f` | Same title; not compared line by line against the PDF |
| **CSR map (XLSM), pinned** | `~/Documents/work/VP_BK/ISP/doc/doc_update_ver_107/FTEL_IP_ISP_CSR_v1.07.xlsm` | `444e72f8d7fd3f35d83a7ce47d455b87dc6df6cda833b11302c6a45fb8b721fa` | v1.07, `COMMON_VER_DATE` 2026-10-18; 573 registers, 819 fields |
| **MAS (DOCX), pinned** | `~/Documents/work/VP_BK/ISP/doc/doc_update_ver_107/FTEL_IP_ISP_MAS_v1.07.docx` | `3fa4097d8f000b4aaf5319ea4ba0b57ebd750db26f9841b746701e138495254b` | v1.07 |
| MAS (PDF conversion, 421 pages) | `~/Documents/work/VP_BK/ISP/output/v107_0_docs/FTEL_IP_ISP_MAS_v1.07.pdf` | `d1c5521a28ecbd234c1cde6ee0fb6ef1995c61951275386e04d68cce5aa34a6a` | Derived from the DOCX in V107-0; MAS page references are pages of this file (PDF index = printed page) |
| **Tuning parameters (XLSX), pinned** | `~/Documents/work/VP_BK/ISP/doc/doc_update_ver_107/FTEL_IP_ISP_Tuning_Parameters_v1.07.xlsx` | `9119eb1236bf0d12d7226990ba47d98ad25843f5d555711c897d3e65589e1d5b` | v1.07; 178 parameters |
| CSR map (XLSX), 07/10 | `~/Documents/work/VP_BK/ISP/doc/doc_update_07_10/FTEL_IP_ISP_CSR_v1.0.xlsx` | `bda077b63d5ffe92c99e856170f4770509934ff900a270e954601b6124750db4` | Revision of 2026-10-07; 254 registers, 336 fields. Superseded, see [ISP_CSR_REVISION_V107.md](ISP_CSR_REVISION_V107.md) |
| MAS (PDF, 336 pages), 07/10 | `~/Documents/work/VP_BK/ISP/doc/doc_update_07_10/FTEL_IP_ISP_MAS_v1.0.pdf` | `7627f38dda45004f3dce327ff4ff5b9e7d1329a6b1f684922b0fd878c1a76bf5` | Dated 2026-10-07. Superseded by v1.07 |
| CSR map (XLSX), previous | `~/Documents/work/VP_BK/ISP/doc/FTEL_IP_ISP_CSR_v1.0.xlsx` | `0980b61ded06a386f0cc64e0445f93fd85b0b1f967d37f0cd41869c0753cd6bf` | Summary: module `ftel_isp_csr`, generated `2026-09-23T02:11:27.964Z`; 226 registers. Superseded, see [ISP_CSR_REVISION_0710.md](ISP_CSR_REVISION_0710.md) |

## Priority

1. **CSR** (v1.07) for ABI and for any behaviour the CSR describes (DEC-09).
2. **MAS** (v1.07) for protocol, activation and block
   behaviour; it replaces the HAS where both describe the same behaviour.
   Block-specific contracts win over general summary paragraphs.
   The **Tuning** workbook (v1.07) gives parameter ranges, defaults and
   update classes; the generated outputs do not use it (V107-0 matrix only).
3. **HAS** for algorithms the MAS does not restate.
4. Contradictions inside one document, and requirements that have no register,
   are tracked in [ISP_DECISIONS_AND_DISCREPANCIES.md](ISP_DECISIONS_AND_DISCREPANCIES.md).

## How the sources were read

- CSR: parsed with `openpyxl` by [tools/gen_csr.py](../tools/gen_csr.py). The
  generator refuses a spreadsheet whose SHA-256 differs from the pinned value
  unless `--allow-new-revision` is given, checks that the `Registers` and
  `Flat Registers` sheets agree, and asserts the expected counts
  (573 registers, 819 fields; 464 RW, 288 RO, 35 W1C, 11 W1S, 2 W1SC, 19 W1P).
- HAS: text extracted from the PDF with `pdftotext -layout`. Figures and
  diagrams are not machine-readable; where a figure carries information that
  is not in the text, the relevant block review (M3) must consult the PDF
  directly. The DOCX has not been diffed against the PDF (open item P00-1).
- MAS: text extracted the same way for the U0 review of the 07/10 revision;
  section and page references in the docs are PDF pages.
- MAS v1.07: delivered as DOCX only. V107-0 converted it to PDF (LibreOffice)
  and extracted the text with `pdftotext -layout`; references to v1.07 pages
  are pages of that PDF (pinned above).
- Tuning v1.07 and the CSR v1.07 matrix: parsed by the V107-0 matrix tools
  (workspace `output/v107_0_matrix/`), outside this repository.

## Regenerating the CSR artefacts

```bash
python3 tools/gen_csr.py --xlsx <path>/FTEL_IP_ISP_CSR_v1.07.xlsm          # write
python3 tools/gen_csr.py --xlsx <path>/FTEL_IP_ISP_CSR_v1.07.xlsm --check  # verify tree
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
