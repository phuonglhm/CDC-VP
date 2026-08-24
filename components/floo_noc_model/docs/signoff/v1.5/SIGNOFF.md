# FlooNoC model D1/D26 v1.5 — technical sign-off record

## Decision

**PASS — baseline v1.5 signed on 2026-08-24 (Asia/Ho_Chi_Minh).**

The component owner approved D1 and D26 as the FlooNoC model v1.5 baseline in
the 2026-08-24 review session. This record supersedes v1.4 for the exact source
snapshot bound by `tested_source_manifest.sha256`. It does not rewrite the
historical v1.4 evidence or extend this decision to files outside the signed
component boundary.

Changing a file covered by the source manifest, the frozen RTL revision, or
the dependency lock invalidates the affected evidence and requires the
corresponding gates to be rerun.

## Signed scope and boundary

| Item | Signed statement |
|---|---|
| Carried v0 architecture | The frozen 4x4, single-AXI, deterministic-XY, `NoLoopback`, `NoRoB`, `MaxUniqueIds = 1` architecture remains the reference |
| D1 local bypass | A manager may bypass the mesh only to a target at the same node for which it is the declared `local_owner`; the bypass creates no flit, routed transaction, network latency sample, or routed admission-slot use, while preserving per-port completion order |
| D26 admission ownership | A routed call owns its admission slot from admission until its ordered `b_transport()` completion; the slot is released only after `await_earlier_bypass()` and `mark_completed()` |
| Idle semantics | `network_idle()` represents physical mesh/adapter work and excludes admission slots; `wrapper_idle()` represents caller-return obligations and includes admission slots and bypass work |
| SystemC component | All 42 registered component tests pass on the recorded snapshot |
| Test sensitivity | All 57 injected component defects are detected; zero mutation is missed, including both D26 controls |
| RTL-derived blocks | All 12 isolated runners pass against unmodified FlooNoC `9a6972a`; 9 compare cycle traces and 3 compare content/configuration |
| TLM-to-AXI wrapper | Model-level verified only; it has no direct RTL counterpart and is not claimed as monolithic RTL-equivalent |

The sign-off does **not** claim monolithic manager-to-subordinate RTL
equivalence, a full RTL SoC sign-off, silicon timing/PPA, or support for the
deferred features (`MaxUniqueIds > 1`, virtual channels, wide channel, ATOP,
CDC links, non-XY routing and irregular topologies). TPU_V3 architecture and
integration are consumers of this component and are outside this approval.

## Snapshot identity

| Field | Value |
|---|---|
| Evidence window | 2026-08-24 10:08:04 to 10:14:36 +07:00 |
| CDC-VP base Git HEAD | `963466e1f09b9bb17061a45228df232eaf921a37` |
| CDC-VP tree state | Dirty; exact tested component files are bound by the manifest below |
| Tested source manifest | `tested_source_manifest.sha256`, 133 files, SHA-256 `ccfc12dc0c5db08d2c823013102afdf1727023fbe62bb3731f4f402245e94c43` |
| Frozen FlooNoC RTL | `9a6972a5f9b8117506d1df8a6505ce1da2bc9084`, clean working tree |
| Frozen `Bender.lock` | SHA-256 `73eb4c72134b7fa51639e254e66ea211e0101496f64161add4f61b2aaa7d86bc` |
| Toolchain | SystemC 2.3.4; `/usr/bin/gcc` and `/usr/bin/g++` 11.5.0; Verilator 5.022; Bender 0.32.1 |

The dirty-tree qualifier is intentional and material. The base commit alone is
not the tested identity. `tested_source_manifest.sha256` is authoritative and
includes the new `tests/test_noc_interconnect_local_bypass.cpp`.

## Executed gates

Every transcript was captured with the required host compiler setup:

```bash
export CC=/usr/bin/gcc
export CXX=/usr/bin/g++
export PATH=/usr/bin:/bin:$PATH
$CC -dumpfullversion
$CXX --version | head
```

| Gate | Result | Durable log | SHA-256 |
|---|---:|---|---|
| Component regression | **PASS — 42/42**, 0 failed | `systemc_tests.log` | `56c98019abc48a934db58428643fcf16ea7d4277da6f3a9e0ad544d35d58e619` |
| Directed D1/D26 scenario | **PASS** | `d1_d26_directed.log` | `691b7242d59f5e180dcfd3ddd3769dba11d83b56eecd9960eb69fb2540a22299` |
| Mutation controls | **PASS — 57/57 detected**, 0 missed | `mutation_controls.log` | `c8216d033eecb5d4062ef3ca8dcb39557a4251fa6c393cedc8f0427a7d719f8e` |
| RTL cross-check aggregate | **PASS — 12/12** | `rtl_crosschecks.log` | `f755ae7d91e3b0d27a07a7d1ec2d8b498d39b8ba822f7bd4754c9d701908ecfd` |

Each `script(1)` transcript ends with `COMMAND_EXIT_CODE="0"`. The RTL log
retains warnings from the frozen RTL/dependency sources; none caused a trace
mismatch or non-zero runner exit.

## D1/D26 load-bearing evidence

The directed scenario holds one routed response behind an older slow local
bypass while the configured routed bound is one:

```text
R-P9-2 while routed A is parked: outstanding=1 peak=1 remote_accesses=1 mesh_quiescent=1 wrapper_idle=0 cycles 24->24
same-port completion order: 0 then 1
slow-bypass release order: 0 1 2
```

`remote_accesses=1` proves that the second routed call was not admitted or
injected. `cycles 24->24` proves that retaining the logical admission slot did
not keep an empty mesh clocking. The mutation registry independently detects:

- `admission-slot-released-before-ordering-wait`;
- `network-idle-consults-admission-slots`.

Both fail `test_noc_interconnect_local_bypass` as expected.

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

## Document-artifact qualification

`docs/NOC_MODEL_ARCHITECTURE.vi.md`, `docs/STATUS.md`, the Phase 9 audit and
decision record are updated to identify v1.5 and the D1/D26 semantics. The
existing `NOC_MODEL_ARCHITECTURE.vi.docx` remains the historical v1.4 render;
it is not silently re-labelled or claimed as a v1.5 signed artifact. A new
DOCX/PDF requires regeneration and its own artifact validation.

## Approval record

| Role | Disposition | Date |
|---|---|---|
| Evidence executor / document maintainer | PASS — all mandatory gates executed, transcripts retained and manifests verified | 2026-08-24 |
| Component owner | **Duy, SoC Design Engineer — APPROVED D1/D26 and selected baseline v1.5 in the review session** | 2026-08-24 |

Conditions or exceptions: none beyond the signed boundary and exclusions
stated above.

**Final decision: APPROVED — signed v1.5.**

This is a technical evidence record, not a substitute for an
organization-specific cryptographic signature, release tag, independent
approver, or commit policy when any of those are required.
