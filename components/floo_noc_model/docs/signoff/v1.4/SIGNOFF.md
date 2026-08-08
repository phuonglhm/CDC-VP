# FlooNoC model architecture v1.4 — technical sign-off record

## Decision

**PASS — technical sign-off closed on 2026-08-07 (Asia/Ho_Chi_Minh).**

This decision applies to the v0 architecture and parameter set described in
`docs/NOC_MODEL_ARCHITECTURE.vi.md`, the tested SystemC source snapshot, and
the twelve isolated SystemC↔RTL cross-check runners. It is a snapshot-bound
decision: changing any file covered by `tested_source_manifest.sha256`, the
frozen RTL revision, or the locked dependency set invalidates this record and
requires the affected gates to be rerun.

## Signed scope and boundary

| Item | Signed statement |
|---|---|
| SystemC component | All 41 registered component tests pass on the recorded snapshot |
| Test sensitivity | All 51 injected model defects are detected; zero mutation is missed |
| RTL-derived blocks | All 12 runners pass against unmodified FlooNoC `9a6972a`; 9 compare cycle traces and 3 compare content/configuration |
| Architecture document | The overview, 4x4 REQ/RSP block diagram, RTL-derived micro-architecture, verification boundary, findings and reproduction flow are consistent with the signed snapshot |
| TLM-to-AXI wrapper | Model-level verified only; it has no direct RTL counterpart and is not claimed as RTL-equivalent |

The sign-off does **not** claim monolithic end-to-end RTL equivalence from a
manager AXI port through the mesh to a subordinate AXI port. It also excludes
deferred features (`MaxUniqueIds > 1`, virtual channels, wide channel, ATOP,
CDC links, non-XY routing and irregular topologies), silicon timing/PPA, and
the D7 area/power calibration gate. Previously reported metrics/dashboard and
FreeRTOS/platform controls were not rerun for this document-only closure.

## Snapshot identity

| Field | Value |
|---|---|
| Evidence window | 2026-08-07 11:02:44 to 11:07:14 +07:00 |
| CDC-VP base Git HEAD | `4a5c00f9cb35b161cf43fdffa0e881748672837d` |
| CDC-VP tree state | `4a5c00f-dirty`; exact tested files are bound by the manifest below |
| Tested source manifest | `tested_source_manifest.sha256`, 132 files, SHA-256 `2f5a17aa955566b48801c6577da39f75d9b292cbb905f3a7871f078c96314fa4` |
| Frozen FlooNoC RTL | `9a6972a5f9b8117506d1df8a6505ce1da2bc9084`, clean working tree |
| Frozen `Bender.lock` | SHA-256 `73eb4c72134b7fa51639e254e66ea211e0101496f64161add4f61b2aaa7d86bc` |
| Toolchain | SystemC 2.3.4; `/usr/bin/gcc` and `/usr/bin/g++` 11.5.0; Verilator 5.022; Bender 0.32.1 |

The dirty-tree qualifier is intentional and material. The base commit alone
is insufficient to reproduce the result; use the tested-source manifest as
the authoritative binding for this sign-off.

## Executed gates

| Gate | Command | Result | Durable log and SHA-256 |
|---|---|---|---|
| Component regression | `make test` | **PASS — 41/41**, 0 failed | `systemc_tests.log` — `bc6c4d7b8e0553bf117d75798bd334bb41c9ded6dbadcd917e59d5e237f84f9f` |
| Mutation controls | `bash rtl_crosscheck/run_negative_controls.sh` | **PASS — 51/51 detected**, 0 missed | `mutation_controls.log` — `3db02b6d...7998764` |
| RTL cross-check aggregate | `bash rtl_crosscheck/run_all_crosschecks.sh` | **PASS — 12/12** | `rtl_crosschecks.log` — `9107a268...51dec2` |

Full hashes for the last two logs are:

```text
3db02b6ee5fc2b8822775965fc0fd6a8a59a1a00b6b207781e183d4337998764  mutation_controls.log
9107a268c98990fb81fdc3fb8a0bb715e363cc93d0bf3e4da2a995611f51dec2  rtl_crosschecks.log
```

Each `script(1)` transcript ends with `COMMAND_EXIT_CODE="0"`. The RTL log
also contains the per-runner heading, individual PASS diagnostic and final
`RTL cross-check summary: 12/12 passed`. Verilator warnings emitted by frozen
RTL/dependencies remain in the transcript; no warning produced a trace mismatch
or non-zero runner exit.

## Signed document artifacts

| Artifact | Validation | SHA-256 |
|---|---|---|
| `docs/NOC_MODEL_ARCHITECTURE.vi.md` | Canonical source, revision v1.4 | `c76901185ff0f669b4802a7e1d7b1e51fc3bf916de65c78a5d9256c4968332e4` |
| `docs/NOC_MODEL_ARCHITECTURE.vi.docx` | Valid OOXML; v1.4 header; 50 headings, 32 tables, 12/12 diagrams embedded, 0 fallback, TOC update field present | `cf480ba5a363c80db0243f8996291d8d7012e599d99768324bf015e35929d9cd` |

No LibreOffice/Word executable was available on the build host, so validation
is structural and content-based rather than an application-specific pagination
review. The DOCX requests TOC/PAGE field refresh when opened; the document owner
should let the target Word application perform that refresh before exporting a
release PDF.

## RTL cross-check inventory

| # | Runner | Comparison | Result |
|---:|---|---|---|
| 1 | `run_route_select_crosscheck.sh` | cycle trace | PASS, 12 cycles |
| 2 | `run_stream_fifo_crosscheck.sh` | cycle trace | PASS, depth 2 and 4, 133 cycles each |
| 3 | `run_wormhole_arbiter_crosscheck.sh` | cycle trace | PASS, 5/4/2 routes, 152 cycles each |
| 4 | `run_router_crosscheck.sh` | cycle trace | PASS, output FIFO 2 and 0, 214 cycles each |
| 5 | `run_rob_crosscheck.sh` | cycle trace | PASS, 127 cycles |
| 6 | `run_chimney_req_crosscheck.sh` | ordered content | PASS, 16 flits |
| 7 | `run_chimney_timing_crosscheck.sh` | cycle trace | PASS, 141 cycles |
| 8 | `run_chimney_rsp_crosscheck.sh` | ordered content | PASS, 8 flits |
| 9 | `run_chimney_rsp_timing_crosscheck.sh` | cycle trace | PASS, 221 cycles |
| 10 | `run_chimney_mgr_rsp_crosscheck.sh` | cycle trace | PASS, 78 cycles |
| 11 | `run_mesh_crosscheck.sh` | cycle trace | PASS, 1872 node-cycles |
| 12 | `run_axi_sizing_crosscheck.sh` | configuration/content | PASS, 8 configurations |

## Approval record

| Role | Disposition | Date |
|---|---|---|
| Evidence executor / document maintainer | PASS — all mandatory gates executed and artifacts recorded | 2026-08-07 |
| Document owner / SoC Design Engineer | Approved closure in the review session; identity recorded by role because no personal signatory name was supplied | 2026-08-07 |

This record is technical evidence, not a substitute for an organization-specific
release form, independent approver identity, or commit/tag policy when those are
required by the project.
