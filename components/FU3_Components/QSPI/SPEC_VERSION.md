# QSPI specification baseline

| Item | Pinned value |
| --- | --- |
| Document | `FTEL_CDC_IP_QSPI_Specification.pdf` |
| Title | FU3 QSPI Controller Hardware Design Specification |
| Spec version | **HDS V1.0** |
| Spec date | **2026-10-20 (20/10/2026)** |
| Physical PDF pages | 63 |
| SHA-256 | `27705ded65c0317ede7da3645c7edefbceff181d5709383e0f0a63f06d625da6` |
| Model scope | Firmware-visible SystemC/TLM controller model |

The source version is the document's version, not a model software release
number. This baseline supersedes the old register/wrapper PDF baseline.
Closed HDS DEC decisions take precedence; the future CSR workbook is DEP-001.

[spec-baseline.json](spec-baseline.json) is the machine-readable identity and
feature-to-regression map. [Spec decisions](docs/spec-decisions.md) record the
behavioral mapping and explicit VP policies. [Testing](docs/testing.md) lists
functional checks and the RTL/pad verification boundary.

The regression recorder embeds this identity, source hashes, per-feature test
results and overall suite results into `test-summary.json`. With `--spec-pdf`
it also checks the supplied PDF hash before executing tests. A different PDF
with the same file name is rejected. To move to a new spec, update the baseline,
review behavioral changes and their mapped tests, and regenerate evidence.

All 13 functional groups must have their mapped tests registered and passing;
the recorder fails on missing, unknown or unmapped scenarios. The generic NOR
backend is a separate support group. The default suite has 57 CTest cases,
including phase/rate, divider and cache-parameter sweeps. These checks do not
claim exhaustive coverage of every possible input combination or RTL/pad
waveform equivalence.
