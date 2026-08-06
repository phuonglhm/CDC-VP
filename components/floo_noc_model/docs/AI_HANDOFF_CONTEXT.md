# FlooNoC SystemC/TLM Model — AI Handoff Context

## 1. Purpose of this document

This is the single handoff document for continuing the FlooNoC Direction-2
SystemC/TLM model in CDC-VP. It consolidates the project intent, frozen source,
engineering decisions, current implementation, verification evidence, known
gaps, build environment, and recommended next steps.

An AI or engineer taking over this work should read this document before making
changes. The shorter files in this directory remain useful references, but this
file is intended to provide enough context to resume the work without the
original chat history.

Status snapshot date: **2026-08-05**.

Sections 2 to 13 describe the state as it is now, after Steps 1 to 10. Section
14 keeps the per-step history, including what each step found. If the two ever
disagree, section 14 is the record of *when* something happened and sections 2
to 13 are the record of *what is true*; fix whichever is stale rather than
leaving the reader to guess.

---

## 2. Non-negotiable project intent

### 2.1 This is a FlooNoC model, not an NPU model

The Direction-2 playbook was introduced through a SAURIA/NPU worked example.
That example is only a process, packaging, and verification pattern. Do not copy
the NPU architecture into this project.

In particular, do not introduce these accelerator-specific concepts unless a
future FlooNoC requirement independently calls for them:

- `START`/`DONE` operation semantics;
- tensor staging;
- a GEMM or accelerator FSM;
- an NPU register map;
- an autonomous DMA worker copied from the NPU wrapper;
- golden output overwrite;
- a single MMIO-peripheral view of the NoC.

FlooNoC is a traffic-driven interconnect. Its natural activity is caused by AXI
transactions and flit handshakes, not by a synthetic accelerator start command.

The worked example the playbook refers to lives at
`components/npu_tlm_v4_model` in this repository. It is listed in section 3.1
so a reader is not left guessing, but its scope is strictly limited:

| Reusable from the NPU component | Not reusable |
|---|---|
| CMake component shape and `cdc::components::<name>` export naming | The register map and any MMIO contract |
| Test registration and standalone build layout | The target socket plus worker structure |
| Platform assembly and SDK packaging patterns (P11/P12) | `START`/`DONE`, tensor staging, GEMM FSM, DMA worker |

Nothing before Step 10 needs it. Steps 5 to 9 are anchored entirely in the
FlooNoC IP. Do not open it for architecture questions.

### 2.2 Integration role in CDC-VP

FlooNoC should replace, compose, or sit hierarchically within the CDC-VP fabric.
It must not be attached as one ordinary MMIO target peripheral.

The implemented CDC-VP integration therefore uses a fabric-oriented M:N TLM
adapter with tagged upstream ports and explicit endpoint placement. Any future
change to that structure must be based on CDC-VP bus topology and endpoint
requirements, not on the NPU wrapper.

### 2.3 Source-of-truth priority

The reference architecture is the FlooNoC IP implementation itself, upstream at
`https://github.com/pulp-platform/FlooNoC.git`, frozen locally at
`/home/duyptt_HW/Documents/work/Study_FlooNoC/FlooNoC`. Every modelling
decision must be traceable to a file in that tree.

Use sources in this order:

1. Frozen FlooNoC RTL.
2. FlooGen configuration and generated topology.
3. FlooNoC RTL testbenches and assertions.
4. FlooNoC documentation for architectural intent.
5. The SystemC model only as the implementation under verification.

If the SystemC behavior disagrees with the frozen RTL, investigate the RTL
configuration and testbench first, then fix the SystemC model. Do not alter the
reference RTL merely to make a comparison pass.

### 2.4 Output and timing policy

- The SystemC datapath/network state is authoritative.
- Golden/reference logic is comparison-only and must never overwrite model
  output.
- A block is cycle-approximate until it passes an RTL per-cycle cross-check.
- **Twelve cross-checks are signed** against frozen revision `9a6972a`: the XY
  route selector, the input FIFO wrap, the wormhole arbiter, the five-port
  router (at `OutFifoDepth` 2 and 0), the AXI sizing arithmetic, chimney request
  content, chimney response content, the `NoRoB` ordering rule, chimney request
  timing, chimney response/subordinate timing, chimney manager-side response
  timing, and inter-node mesh timing.
  Section 10.11 tabulates them; `docs/STATUS.md` holds the evidence and the
  negative controls.
- Every RTL timing block selected for the v0 vertical slice has an isolated
  cycle cross-check, including the chimney's request path, its subordinate
  side, its manager-side response unpacker, and the inter-node mesh. As of Step
  A-1 there is no selected v0 block left uncrossed-checked.
  Statements that the mesh or link timing itself is still only an estimate
  predate Step 9.2 and are wrong.
- Step A-2 provides a signal-driven `axi_noc`: one complete timed chimney at
  every coordinate over the two meshes. Step A-3 now makes it the integrated
  `noc_interconnect` datapath: the wrapper drives manager AW/W/AR and
  subordinate B/R cycle by cycle. This means the complete TLM path traverses
  the individually signed timing blocks; it still does **not** turn their
  composition into one monolithic RTL cross-check.
- Not signed, and **not signable against a direct RTL counterpart**: the
  TLM-to-AXI adapters and wrapper logic in `noc_interconnect`. They are a driver
  and collector above real timed AXI signal boundaries. Their functional and
  composed timing behavior is covered by model-level integration tests. Step
  10.3 adds bounded three-manager deterministic-random stress and seven
  mutation controls specifically for this unsigned layer.
- `axi_endpoint.hpp` is now legacy reference/test code only. It has no RTL
  counterpart and is no longer in the production datapath.

---

## 3. Important paths and frozen versions

### 3.1 Project paths

| Item | Path |
|---|---|
| CDC-VP model component | `/home/duyptt_HW/Desktop/VP_INTER/upgit/CDC-VP/components/floo_noc_model` |
| Frozen FlooNoC repository | `/home/duyptt_HW/Documents/work/Study_FlooNoC/FlooNoC` |
| Direction-2 playbook | `/home/duyptt_HW/Documents/work/tlm_model/ip/doc/TLM_IP_H2_BUILD_PLAYBOOK.en.md` |
| Playbook's worked example, **process reference only** | `/home/duyptt_HW/Desktop/VP_INTER/upgit/CDC-VP/components/npu_tlm_v4_model` |
| Local direction guide copy | `docs/TLM_IP_DIRECTION_GUIDE.en.md` |
| Local Direction-2 playbook copy | `docs/TLM_IP_H2_BUILD_PLAYBOOK.en.md` |
| SystemC installation | `/opt/systemc-2.3.4` |
| Locked RTL dependency checkout (leaf path) | `/home/duyptt_HW/Documents/work/Study_FlooNoC/floo_rtl_deps` |
| Bender dependency checkout (full path) | `<FlooNoC>/.bender/git/checkouts` |
| Bender binary | `/home/duyptt_HW/.local/bin/bender` |
| Default route cross-check output | `/tmp/floo_noc_route_crosscheck` |
| Default FIFO cross-check output | `/tmp/floo_noc_stream_fifo_crosscheck` |
| Default arbiter cross-check output | `/tmp/floo_noc_wormhole_arbiter_crosscheck` |
| Default RTL file-list output | `/tmp/floo_noc_rtl_filelist` |
| Default AXI sizing cross-check output | `/tmp/floo_noc_axi_sizing_crosscheck` |
| Default router cross-check output | `/tmp/floo_noc_router_crosscheck` |
| Default chimney request-content output | `/tmp/floo_noc_chimney_req_crosscheck` |
| Default chimney response-content output | `/tmp/floo_noc_chimney_rsp_crosscheck` |
| Default `NoRoB` ordering output | `/tmp/floo_noc_rob_crosscheck` |
| Default chimney request-timing output | `/tmp/floo_noc_chimney_timing_crosscheck` |
| Default chimney response-timing output | `/tmp/floo_noc_chimney_rsp_timing_crosscheck` |
| Default mesh cross-check output | `/tmp/floo_noc_mesh_crosscheck` |
| TLM integration platform | `/home/duyptt_HW/Desktop/VP_INTER/upgit/CDC-VP/platforms/noc_soc` |
| DMA firmware used by that platform | `/home/duyptt_HW/Desktop/VP_INTER/upgit/CDC-VP/fw/dma_riscv` |
| RISC-V toolchain | `/opt/toolchains/riscv-none-elf/bin` (GCC 15.2.0) |

### 3.2 Frozen source

- FlooNoC upstream: `https://github.com/pulp-platform/FlooNoC.git`
- FlooNoC Git revision:
  `9a6972a5f9b8117506d1df8a6505ce1da2bc9084`
- Short revision used in documentation: `9a6972a`
- `git describe --tags`: `v0.8.4-10-g9a6972a`
- Provenance verified 2026-07-28: the local tree's `origin` is the upstream URL
  above, and the frozen revision is reachable from `origin/main`. Upstream
  `main` has since advanced to `0a08447`; the working tree is intentionally
  held behind it.
- FlooGen package version: `0.8.4`
- Route-selector RTL:
  `hw/floo_route_select.sv`
- Frozen route-selector SHA-256:
  `234fefcb0cee853ee93299b16caee0811e7f142237f205ee399d3f9c53073f1e`
- Wormhole-arbiter RTL:
  `hw/floo_wormhole_arbiter.sv`
- Frozen wormhole-arbiter SHA-256:
  `ab4c2272e069cdf611b909b1dfbca4c55b13906c1bd4c12d6ae9ff12db1445b0`
- Router RTL:
  `hw/floo_router.sv`
- Frozen router SHA-256:
  `fb291e78a0344dc40bfa19a4a0cebef9f6042088a39e4de06fb8f77739b0deaa`
- The frozen v0 router parameter set is recorded in `docs/P0_SCOPE.md`
- Locked dependency in use: `common_cells` `1.39.0` @
  `9ca8a7655f741e7dd5736669a20a301325194c28`
- Frozen `common_cells` file SHA-256 values are recorded in
  `rtl_crosscheck/fetch_rtl_deps.sh`
- Frozen `Bender.lock` SHA-256:
  `73eb4c72134b7fa51639e254e66ea211e0101496f64161add4f61b2aaa7d86bc`
- Pinned Bender version: `0.32.1`
  (artifact `bender-x86_64-unknown-linux-gnu.tar.xz`, SHA-256
  `59a36723b056a06b266dc68d4ceedcd0aa17a1c096e8c2ea512af264ff6a13f6`;
  installed binary SHA-256
  `37f68104525495733bec4c89baec24f3a01f8995a6c80137f469cef42c57953e`)
- SystemC version: `2.3.4`
- Language level: C++17
- Verified host compiler: GCC/G++ `11.5.0`
- Verified Verilator: `5.022`

The model must not silently follow a different FlooNoC revision. A revision
change requires:

1. an explicit scope update;
2. review of RTL/package/type changes;
3. regeneration or review of all trace shims;
4. rerunning every available RTL cross-check;
5. updating the frozen hashes and this document.

### 3.3 FlooNoC repository character

The FlooNoC repository is not merely a prose specification. It contains:

- synthesizable SystemVerilog RTL;
- FlooGen topology generation;
- generated NoC packages/topologies;
- RTL testbenches and assertions;
- Bender dependency metadata;
- documentation and architectural intent.

FlooNoC is a configurable, non-coherent NoC with simple transport routers and
protocol complexity moved toward endpoint network interfaces, called
“chimneys.” It supports end-to-end AXI4, wide physical channels, traffic-class
separation, outstanding transactions, and generated topologies. The current
SystemC vertical slice intentionally implements only a small subset.

---

## 4. Mandatory build environment

Before every build, use this environment exactly:

```bash
export CC=/usr/bin/gcc
export CXX=/usr/bin/g++
export PATH=/usr/bin:/bin:$PATH

# sanity
$CC -dumpfullversion
$CXX --version | head
```

Reason: a broken or incompatible Synopsys compiler wrapper can appear earlier
in the host `PATH`. Do not trust the inherited compiler environment.

The component Makefile already exports these values and prints the sanity
information. New build scripts must do the same before invoking CMake, Make,
Ninja, Verilator-generated builds, or another C/C++ compilation flow.

Do not use a stale environment `SYSTEMC_HOME`. The component intentionally
defaults to:

```text
/opt/systemc-2.3.4
```

---

## 5. Direction-2 playbook interpretation for FlooNoC

The general phase structure remains useful, but the contents must be adapted to
an interconnect.

The direction guide explicitly lists an interconnect/NoC as an H2 candidate.
This project has already selected the full Direction-2 path because RTL is
available and the behavior of interest is inherently timing-dependent:
contention, arbitration, back-pressure, buffering, routing, and traffic mix all
change latency and throughput. Do not reopen that decision merely because a
simple functional fabric is faster.

The guide also says that a fast Direction-1 baseline normally exists in
parallel. Step 11 now provides that coexistence path inside the same
`noc_interconnect` class: detailed mode remains the Direction-2 calibration
reference, while fast mode reuses its functional replay and annotates a
calibrated no-contention estimate for long runs.

| Phase | FlooNoC interpretation | Current state |
|---|---|---|
| P0 | Freeze RTL/configuration, supported traffic, timing target, metrics, and integration role | Completed for vertical slice v0; the frozen parameter set in section 6.1 was corrected at Step 9.2 (`OutFifoDepth = 2`) |
| P1 | Timing-independent address decode, routes, transaction/flit reference behavior, and vectors | Implemented: address map, XY path, AXI reference behaviour and legacy endpoint reference tests |
| P2 | Signal-safe coordinate/header/flit/AXI types and address map | Implemented: full `FLOO_TYPEDEF_HDR_T` field set, AXI channel types, and RTL-signed sizing arithmetic. Coordinate widths remain a model choice |
| P3 | FIFO, route selection, arbitration, router, links, chimney/meta/RoB blocks | Signed: FIFO wrap, XY selector, arbiter, five-port router with output FIFO, chimney **request** timing (141 cyc), chimney **subordinate** side (221 cyc), chimney **manager-side response** (78 cyc, Step A-1), meta buffer, `NoRoB` gate. Every selected v0 block is signed; A-2 composes them and model-tests the wiring, but there is no composed RTL harness |
| P4 | Router/link topology and endpoint wiring | Implemented: `axi_mesh_noc` preserves the raw `req`/`rsp` fabric; `axi_noc` adds one complete timed chimney per node (A-2), and `noc_interconnect` drives it cycle by cycle (A-3). Mesh timing is signed |
| P5 | NoC-specific measured counters | Complete for D0-D6 v1: passive production counters on every request/response router, input/output occupancy, exact transaction histograms, manager/target/flow attribution, JSON/dashboard and drainable DSE; see section 13.6 |
| P6 | Optional configuration translation | NPU-style operation driver is not applicable; `noc_soc` takes mesh geometry and placement as construction parameters |
| P7 | Unit, router, mesh, stress, and RTL equivalence tests | 41 SystemC tests pass; twelve RTL cross-checks signed; 51/51 component mutation controls and 10/10 D3-D6 platform metrics controls pass. Step 10.3 adds bounded deterministic-random stress with three concurrent initiators; Step 10.5 adds bounded same-port concurrency; Step 11 adds detailed/fast calibration; metrics D3-D6 adds deterministic drain/conservation and winner-selection gates |
| P8 | Standalone build | Implemented with CMake and Make |
| P9 | SW/platform-visible contract | No NoC register map by design. The platform-visible contract is the address map, the placement rule that no target may share a node with a manager, and `noc_soc`'s explicit survey/firmware ownership mode plus reserved scratch page |
| P10 | TLM integration | Implemented and stress-tested: `noc_interconnect` is a fabric adapter with M:N tagged sockets, explicit placement, bounded concurrent detailed calls, and construction-selected detailed/fast timing. A-3 and Steps 10.1, 10.3, 10.5 and 11 are done |
| P11 | CDC-VP CMake component/install/export | Header-only interface target plus the compiled `cdc::components::noc_interconnect`. Step 10.4's registered packaging regression proves clean-prefix installation and independent downstream consumption of the compiled target |
| P12 | Platform assembly and packaging | `platforms/noc_soc` assembled and running. `noc_soc_packaging_regression` rebuilds the self-contained package under a private root and checks exact `$ORIGIN` RPATH, local SystemC resolution, runtime execution, licences and provenance |
| P13 | RISC-V/SoC traffic validation | Real firmware (`fw/dma_riscv`) reaches `DMA PASS` in detailed and fast timing; survey matches all bytes in both. Step 10.1 makes traffic ownership explicit and rejects an ELF claiming the reserved page. Automated as `noc_soc_firmware_regression`. Step 11 separates ~53.14 modeled ns/instruction from the recorded 8.12 s versus 0.08 s host measurements |
| P14 | Coexistence and maintenance | Implemented. Step 10.3 proves detailed-model clock gating; Step 11 adds the construction-selected fast backend and calibration/firmware gates without weakening the detailed path |

### 5.1 Clock-gating interpretation

The playbook requires idle cycle-level models to avoid consuming simulation
time. For FlooNoC, stopping the clock is legal only when the whole modeled
network is quiescent:

- all ingress queues are empty;
- all router FIFOs are empty;
- no route or arbitration lock is active;
- all links and egress queues are empty;
- no AXI transaction or response is outstanding;
- no chimney, metadata buffer, or RoB entry is pending.

Do not gate the network merely because no new TLM request arrived in the current
delta cycle.

**Step 10.3 now meets that bar.** `floo_router` exports passive input/output
FIFO occupancy plus route and arbiter lock state; `floo_mesh` aggregates those
with live injection/ejection boundaries; `axi_noc` additionally checks every
chimney's metadata, RoB, arbitration and AXI valid state. The resulting
`mesh_quiescent()` is separate from `network_idle()`, which still represents
wrapper bookkeeping. The production thread waits with its clock stopped only
when both predicates are true and asserts the one-way invariant
`network_idle() => mesh_quiescent()`. Equality is deliberately not required:
the mesh can be empty while a wrapper request or target delay remains pending.
See section 13.6b for the directed and mutation evidence.

---

## 6. Frozen vertical slice v0

### 6.1 Included

| Parameter | Locked value |
|---|---|
| Network class | Single-AXI vertical slice |
| Routing | Deterministic XY |
| Traffic | Unicast |
| Physical channels | `req` and `rsp`, two separate meshes (`axi_noc.hpp`) |
| Flow control | Ready/valid |
| Virtual channels | No modeled VC dimension or credit protocol |
| Router ports | North, East, South, West, Eject |
| Local ports | One Eject port per router |
| `InFifoDepth` | 2, which selects the spill-register branch of the RTL wrap |
| `OutFifoDepth` | **2**, matching every FlooGen router template |
| `MaxUniqueIds` | 1, so chimney response metadata is a plain in-order FIFO |
| `NoLoopback` | 1, so a self-addressed flit is undeliverable |
| Topology | Rectangular 2-D mesh |
| Output source | SystemC state/datapath |
| Timing target | Cycle-accurate, manager AXI port to subordinate AXI port |

**`OutFifoDepth = 2` is not optional.** Every FlooGen router template hardcodes
`.OutFifoDepth (2)`. An earlier revision of this table said "Output FIFO |
Disabled", which came from a Step-4 testbench choice rather than from the RTL,
and the model was built without an output FIFO for five steps because of it.
Step 9.2 found it and corrected the end-to-end latencies from 7 and 16 cycles to
11 and 30. Anything quoting 7 or 16 predates that fix. `test_floo_router` is
deliberately pinned to `OutFifoDepth = 0` so the other branch stays covered, and
the router cross-check runs at both depths.

### 6.2 Deferred

- narrow-wide AXI and the `wide` physical channel;
- YX routing;
- source routing;
- ID/table-based routing;
- virtual channels and credit-based flow control;
- multicast;
- synchronization and reduction collectives;
- ATOP support;
- configurable reorder-buffer modes;
- multiple unique downstream IDs;
- CDC links and asynchronous clock domains;
- multiple local Eject ports;
- generated irregular/tree topologies;
- explicit link pipeline/cut configuration.

Deferred features must be added only after lower-level equivalence tests stay
green.

### 6.3 What the datapath now is — A-3 integration complete

`floo_mesh<FlitT, ...>` is still the generic single-stream mesh. `axi_noc.hpp`
now exposes two deliberately different compositions:

- `axi_mesh_noc` is the raw pair of meshes: one carrying `axi_req_flit`, one
  carrying `axi_rsp_flit`. Mesh cross-checks and legacy endpoint tests use it
  directly.
- `axi_noc` is the A-2 signal-driven datapath. It wraps `axi_mesh_noc` with one
  `axi_chimney_node` per coordinate and exposes AXI manager/subordinate signals.
  Since A-3, this is the production datapath inside `noc_interconnect`.

`axi_chimney_node` connects `axi_chimney_request` and
`axi_chimney_manager_response` on the manager side, and
`axi_chimney_response` on the subordinate side. The B/R pop, RLAST and RoB
ready loops are internal; an external testbench cannot drive them by hand.

So the model now contains a complete single-AXI FlooNoC network: AXI signals,
timed chimneys and two physical meshes. `test_axi_noc_chimney` verifies one
two-beat write and one three-beat read across a 2x2 composition in 56 cycles,
including back-pressure, downstream ID `3'b111`, ID restoration, metadata/RoB
release and no leakage into unused nodes.

**It may not be described as one composed RTL cross-check.** Every block is
signed individually, A-2's test verifies their SystemC wiring, and A-3's
wrapper tests exercise the complete TLM path. The TLM adapters have no direct
RTL counterpart.

| Header/type | What it is | Where it is instantiated |
|---|---|---|
| `axi_mesh_noc` | raw `req` and `rsp` meshes | RTL mesh runner and legacy endpoint tests |
| `axi_noc` | one complete timed chimney per node over `axi_mesh_noc` | `test_axi_noc_chimney` and production `noc_interconnect` |

The wrapper's manager adapter holds AW/W/AR stable to handshake, the
subordinate adapter reconstructs writes and reads from accepted AXI channels,
and B/R are held until the timed chimney accepts them. Passive observation of
the accepted request-link AW/AR header supplies the requester coordinate only
for target-delay attribution, because the subordinate AXI ID is intentionally
rewritten to `3'b111`; it does not bypass or drive the datapath.

What else may **not** be claimed is anything in section 6.2.

One structural caveat survives: the inter-router links are combinational
`SC_METHOD` connections, and all storage lives in the router input and output
FIFOs. There is no separate configurable link cut/pipeline module, so do not
claim the model implements FlooNoC's explicit link pipelining. The frozen
configuration uses none, which is why mesh timing signs off anyway.

---

## 7. Current component layout

```text
floo_noc_model/
  CMakeLists.txt
  Makefile
  README.md
  LICENSES/
    SHL-0.51.txt
  docs/
    AI_HANDOFF_CONTEXT.md        <- this file
    P0_SCOPE.md
    RTL_MAPPING.md
    STATUS.md
    FLOONOC_MODEL_IMPLEMENTATION_REPORT.vi.md
    Finish_Task.txt              <- progress reports, Vietnamese
    Finish_Task_2026-07-28.txt
    TLM_IP_DIRECTION_GUIDE.en.md
    TLM_IP_H2_BUILD_PLAYBOOK.en.md
    NOC_METRICS_DASHBOARD_ROADMAP.md
    NOC_METRICS_DASHBOARD_IMPLEMENTATION.md
  include/floo_noc_model/
    floo_types.hpp               <- header/flit/coordinate types
    reference_model.hpp          <- timing-independent decode and XY path
    stream_fifo.hpp              <- input/output buffer wrap
    xy_route_select.hpp
    rr_arb_tree.hpp
    wormhole_arbiter.hpp
    floo_router.hpp              <- five-port router, In/OutFifoDepth
    floo_mesh.hpp                <- generic single-stream mesh
    noc_counters.hpp
    noc_metrics.hpp              <- exact histograms and traffic buckets
    axi_types.hpp                <- AXI channel types and flit sizing
    axi_chimney_pack.hpp         <- flit assembly rules
    meta_buffer.hpp
    rob_order_gate.hpp           <- NoRoB ordering gate
    axi_chimney.hpp              <- timed chimney, request and response
    axi_endpoint.hpp             <- legacy manager/subordinate reference tests
    axi_noc.hpp                  <- raw mesh pair + per-node chimney composition
    axi_lanes.hpp                <- TLM byte range -> AxSIZE/AxLEN/WSTRB
    noc_interconnect.h           <- TLM wrapper for CDC-VP
  src/
    noc_interconnect.cpp         <- A-3 TLM-to-signal adapters; only compiled TU
  tests/
    CMakeLists.txt               <- registers 41 tests
    test_reference_model.cpp
    test_stream_fifo.cpp
    test_xy_route_select.cpp
    test_wormhole_arbiter.cpp
    test_floo_router.cpp
    test_floo_mesh.cpp
    test_noc_counters.cpp
    test_noc_metrics.cpp
    test_axi_types.cpp
    test_axi_chimney_pack.cpp
    test_rob_order_gate.cpp
    test_axi_endpoint.cpp
    test_noc_interconnect_stress.cpp <- Step 10.3 three-manager scoreboard
    test_noc_interconnect_concurrency.cpp <- Step 10.5 same-port queue/slots
    test_noc_interconnect_fast.cpp <- Step 11 detailed/fast calibration
    test_noc_interconnect_observer.cpp <- Step 12.7 completion observer
    test_axi_noc.cpp
    test_axi_noc_chimney.cpp     <- A-2 signal-driven composition
    test_noc_interconnect.cpp    <- A-3 TLM wrapper + 1/6-hop calibration
    test_noc_interconnect_bad_config.cpp <- configs refused before traffic
    test_axi_lanes_odr.cpp               <- and _odr_second.cpp: one link
    test_axi_lanes.cpp           <- AxSIZE/AxLEN/WSTRB, checked on the fields
    test_axi_chimney_manager_response.cpp
    route_trace_sc.cpp           <- trace runners, one per cross-check
    fifo_trace_sc.cpp
    arbiter_trace_sc.cpp
    router_trace_sc.cpp
    axi_sizing_trace.cpp
    chimney_req_trace_sc.cpp
    chimney_rsp_trace_sc.cpp
    chimney_timing_trace_sc.cpp
    chimney_rsp_timing_trace_sc.cpp
    rob_trace_sc.cpp
    mesh_trace_sc.cpp
    data/
      gen_*.py                   <- stimulus generators, 7 of them
      *_stimulus.csv             <- shared stimulus, both sides read it
      *_expected.csv             <- RTL-captured goldens, never model-captured
  rtl_crosscheck/
    compare_traces.py            <- the only comparator; exact line match
    fetch_rtl_deps.sh            <- leaf path: revision + per-file SHA-256
    gen_rtl_filelist.sh          <- full path: Bender against the frozen lock
    install_floogen.sh
    run_route_select_crosscheck.sh
    run_stream_fifo_crosscheck.sh
    run_wormhole_arbiter_crosscheck.sh
    run_router_crosscheck.sh
    run_axi_sizing_crosscheck.sh
    run_chimney_req_crosscheck.sh
    run_chimney_rsp_crosscheck.sh
    run_rob_crosscheck.sh
    run_chimney_timing_crosscheck.sh
    run_chimney_rsp_timing_crosscheck.sh
    run_mesh_crosscheck.sh
    run_negative_controls.sh     <- re-injects each fixed defect; all must fail
    route_select/
      floo_pkg.sv
      tb_route_select_trace.sv
      shim/common_cells/registers.svh
    stream_fifo/
      tb_stream_fifo_trace.sv
    wormhole_arbiter/
      floo_pkg_empty.sv
      tb_wormhole_arbiter_trace.sv
    floo_router/
      tb_floo_router_trace.sv
    axi_sizing/
      tb_axi_sizing_trace.sv
    axi_chimney/
      tb_floo_axi_chimney_req_trace.sv
      tb_floo_axi_chimney_rsp_trace.sv
      tb_floo_axi_chimney_timing_trace.sv
      tb_floo_axi_chimney_rsp_timing_trace.sv
    rob/
      tb_floo_rob_wrapper_trace.sv
    mesh/
      tb_floo_mesh_trace.sv
```

Every `*_expected.csv` under `tests/data/` is captured from the frozen RTL by the
matching cross-check runner. They exist so the standalone regression can detect
a regression without Verilator; they are never a substitute for the RTL
comparison and **must be regenerated from RTL, never from the model**. A golden
refreshed from the model turns the whole test suite into a tautology.

Two CMake targets are exported:

```text
cdc::components::floo_noc_model    # header-only: the model itself
cdc::components::noc_interconnect  # compiled: the TLM wrapper, links the above
```

The split matters. Anything using only the network model links the interface
target; a platform links `noc_interconnect`, which is why
`test_noc_interconnect` is registered separately in `tests/CMakeLists.txt`
rather than through `add_floo_model_test`.

The consumer platform lives outside this component, at
`platforms/noc_soc` — see its `README.md` for the floorplan, the measured
latency table, and the placement rule.

---

## 8. RTL-to-SystemC block mapping

| SystemC file | Frozen RTL/architecture source | Modeled responsibility |
|---|---|---|
| `floo_types.hpp` | `hw/floo_pkg.sv`, `hw/include/floo_noc/typedef.svh` | Directions, coordinates, basic header/flit identity |
| `reference_model.hpp` | FlooGen address/routing concepts | Timing-independent address decode and XY route |
| `stream_fifo.hpp` | `stream_fifo_optimal_wrap`, `spill_register_flushable`, `stream_fifo`, `fifo_v3` (common_cells 1.39.0) | Input buffering; mirrors the RTL wrap hierarchy including the depth-2 spill-register branch |
| `xy_route_select.hpp` | `hw/floo_route_select.sv` | XY next-hop and packet route lock |
| `rr_arb_tree.hpp` | `rr_arb_tree`, `lzc`, `cf_math_pkg` (common_cells 1.39.0) | Round-robin tree, trailing-zero counters, fair next-index and lock state |
| `wormhole_arbiter.hpp` | `hw/floo_wormhole_arbiter.sv` | Request snapshot, requester selection, and lock through `last` |
| `floo_router.hpp` | `hw/floo_router.sv`, `hw/floo_output_arbiter.sv` | Five input FIFOs, route selection, optimized crossbar, per-output arbitration |
| `floo_mesh.hpp` | FlooGen generated `floo_*_noc.sv` topology concept | Rectangular router composition, one flit stream, abstract endpoints |
| `axi_types.hpp` | `hw/floo_pkg.sv` sizing functions over locked `axi_pkg` | AXI channel payload widths, channel-to-link mapping, reserved bits |
| `axi_chimney_pack.hpp` | `hw/floo_axi_chimney.sv` `always_comb` blocks, `hw/floo_id_translation.sv` | Flit assembly per channel, both destination-decode modes, `aw_w_sel_q` |
| `meta_buffer.hpp` | `hw/floo_meta_buffer.sv`, `MaxUniqueIds = 1` branch | Request metadata retention as a plain in-order `fifo_v3`, no ID matching |
| `rob_order_gate.hpp` | `hw/floo_rob_wrapper.sv`, `NoRoB` branch, over `axi_demux_id_counters` | The same-ID/different-destination stall rule and its per-ID capacity |
| `axi_chimney.hpp` | `hw/floo_axi_chimney.sv` | Timed AXI/flit conversion: AW/W coupling and request arbiter (signed, 141 cyc); subordinate side and response arbiter (signed, 221 cyc); manager-side response unpacker (signed, 78 cyc, Step A-1) |
| `axi_endpoint.hpp` | **no RTL counterpart** | Legacy AXI manager/subordinate transactors used only by isolated reference tests after A-3 |
| `axi_noc.hpp` | FlooGen generated `floo_axi_mesh_noc.sv`, `hw/floo_axi_chimney.sv` | `axi_mesh_noc`: raw `req`/`rsp` mesh pair. `axi_noc`: one complete timed chimney per coordinate over that pair (A-2), now the integrated datapath |
| `noc_interconnect.h`, `src/noc_interconnect.cpp` | **no RTL counterpart**; CDC-VP `bus_router` for the socket contract | TLM generic payload to cycle-driven manager AW/W/AR and subordinate B/R, M:N tagged sockets, placement and clock gating (A-3) |

The final row remains the layer named in section 2.4: signed timing blocks
driven by unsigned TLM adaptation code. Treat behavior that lives only there as
model-level integration behavior; Step 10.3 supplies its stress evidence.

External `common_cells`, `axi`, and other Bender dependencies are not copied
into the model. Their exact frozen behavior must be used for RTL cross-checks.

---

## 9. Implemented model semantics

### 9.1 `floo_types.hpp`

Namespace:

```cpp
floo::model
```

Direction encoding matches `floo_pkg::route_direction_e`:

| Direction | Numeric port |
|---|---:|
| North | 0 |
| East | 1 |
| South | 2 |
| West | 3 |
| Eject | 4 |

Other current types:

- `axi_channel`: AW=0, W=1, AR=2, B=3, R=4.
- `physical_channel`: req=0, rsp=1.
- `coordinate`:
  - `x`: 16-bit `sc_uint`;
  - `y`: 16-bit `sc_uint`;
  - `port_id`: 8-bit `sc_uint`.
- `flit_header`:
  - destination/source coordinates;
  - `last`;
  - three-bit AXI-channel field;
  - `rob_req`;
  - 16-bit `rob_idx`;
  - `atop`.
- `basic_flit<PayloadBits>`: header plus `sc_bv` payload.
- `test_flit`: `basic_flit<64>`.

Custom types implement equality, stream output, and `sc_trace` support so they
can be used safely in `sc_signal`.

**Header field set: closed 2026-07-28.** The model's `flit_header` now carries
the full field set of `FLOO_TYPEDEF_HDR_T`, in the macro's declaration order:

```systemverilog
rob_req, rob_idx, dst_id, collective_mask, src_id, last, atop, axi_ch, collective_op
```

`collective_mask` and `collective_op` stay inert inside frozen v0, which is
Unicast with `EnMultiCast = 0`, because `hw/floo_route_select.sv` only reads
`hdr.collective_op` when `EnMultiCast` is set. Adding them changed no
cross-check result. Field widths are still a model choice rather than a proven
packed representation.

What was verified to match the frozen package exactly:

| Model | Frozen source | Result |
|---|---|---|
| `direction` North/East/South/West/Eject = 0..4 | `floo_pkg::route_direction_e` | exact |
| `axi_channel` aw/w/ar/b/r = 0..4, 3-bit | `floo_pkg::axi_ch_e` (`logic [2:0]`) | exact |

Coordinate widths remain a model choice, not a generated-configuration value.

### 9.2 `reference_model.hpp`

`reference_address_map`:

- owns a list of `{base, size, destination}` regions;
- uses half-open address ranges `[base, base + size)`;
- rejects zero-sized regions;
- rejects wrapping 64-bit regions;
- rejects overlaps;
- returns `std::nullopt` for unmapped addresses.

`xy_next_hop`:

1. Eject if current X/Y equals destination X/Y.
2. Move East or West until X matches.
3. Move North or South until Y matches.

Only destination `port_id == 0` is accepted in vertical slice v0.

`xy_path` validates nonzero mesh dimensions and in-range endpoints, then returns
the deterministic hop sequence including the final Eject.

### 9.3 `stream_fifo.hpp`

Templates:

```cpp
spill_register<T>                    // spill_register_flushable, Bypass = 0
stream_fifo<T, Depth>                // stream_fifo over fifo_v3, FALL_THROUGH = 0
stream_fifo_optimal_wrap<T, Depth>   // Depth == 2 -> spill, Depth > 2 -> FIFO
```

This replaces the superseded `ready_valid_fifo`, which modeled an "optimal"
FIFO that accepted a push while full whenever its head was popped in the same
cycle. **The frozen RTL does not do this in either branch.** In both branches
`ready_o` is a function of registers only:

- `spill_register_flushable`: `ready_o = !a_full_q || !b_full_q`;
- `stream_fifo`/`fifo_v3`: `ready_o = ~full`, `push = valid_i & ~full`.

A full buffer therefore refuses a push even in a cycle where it pops. The old
model overstated input-buffer acceptance and created a combinational
`ready_i -> ready_o` path the RTL does not have.

Behavior now modeled:

- asynchronous active-low reset that also clears the FIFO memory, matching
  `fifo_v3`;
- spill branch: A/B registers, `valid_o = a_full | b_full`,
  `data_o = b_full ? b_data : a_data`, drain of A into B under back-pressure;
- FIFO branch: circular memory, registered read/write pointers and
  `status_cnt`, `data_o = mem_q[read_pointer_q]` even when empty;
- push and pop in the same cycle only while not full;
- `o_occupancy` is model-only debug output.

`flush_i` is not modeled because `hw/floo_router.sv` ties it low, and `usage_o`
is not modeled because the router leaves it unconnected and the depth-2 branch
drives it to `'x`.

Depth 0 and 1 are rejected by `static_assert`, matching the RTL `$fatal`.
The frozen router configuration uses `InFifoDepth = 2`, so the modeled input
buffer is a spill register, not a circular FIFO.

This block passes a 133-cycle RTL cross-check at depth 2 and depth 4.

### 9.4 `xy_route_select.hpp`

Template:

```cpp
xy_route_select<FlitT>
```

Behavior:

- combinational flit pass-through;
- deterministic XY route calculation;
- route state locks after an accepted non-last flit;
- while locked, the stored route is used even if the presented header changes;
- an accepted last flit releases the lock;
- route and lock state reset asynchronously.

This block passes a 12-cycle RTL cross-check. It was the first signed block;
section 10.11 lists the complete current set of twelve cross-checks.

### 9.5 `rr_arb_tree.hpp` and `wormhole_arbiter.hpp`

Templates:

```cpp
rr_arb_tree<NumIn>                   // stateless mirror of the common_cells tree
wormhole_arbiter<FlitT, NumRoutes>   // mirror of floo_wormhole_arbiter.sv
```

The earlier hand-written arbiter was wrong in four separate ways. It searched
the **live** `valid_i` inputs from an explicit `rr_next_q` register and advanced
that register to `selected + 1`. The frozen RTL instead arbitrates through
`rr_arb_tree` with `ExtPrio = 0`, `AxiVldRdy = 1`, `LockIn = 1`, `FairArb = 1`,
granted only by `ready_i & last_out`:

| Aspect | Superseded model | Frozen RTL |
|---|---|---|
| Round-robin advance | `selected + 1` | next *requesting* index above `rr_q`, from two `lzc` over masked requests |
| Arbitrated request set | live `valid_i` | the `valid_q` snapshot, held by the tree's `LockIn` |
| `ready_o` | only if the selected input is itself valid | on the selected index whenever any input is valid |
| `data_o` when invalid | zeroed | always `data_i[valid_selected_idx]` |

`rr_arb_tree.hpp` reproduces the arbitration tree, `lzc` with `MODE = 0`, and
`cf_math_pkg::idx_width` structurally. The tree's data multiplexer and
per-input grant decode are not modeled: the frozen instantiation drives
`data_i` with `'0` and leaves `data_o` and `gnt_o` unconnected. `flush_i` is
tied low and is not modeled. The tree is a stateless helper; its registers
(`rr_q`, `lock_q`, `req_q`) live in the enclosing `sc_module` so they take part
in normal SystemC sensitivity and can be traced.

`wormhole_arbiter.hpp` reproduces the FlooNoC wrapper: the `valid_q` snapshot
refreshed only when it is empty or the previous cycle accepted a `last` flit,
`valid_selected_idx`, the handshake decode, and `last_q`.

Two debug outputs are model-defined and not part of the signed contract:
`o_selected` reports `valid_selected_idx`, and `o_locked` reports that
`valid_q` is being held rather than refreshed.

Note a structural redundancy in the RTL: the wrapper's snapshot and the tree's
`LockIn` implement the same packet hold, and either alone reproduces the frozen
behavior. See section 10.4.

### 9.6 `floo_router.hpp`

Template:

```cpp
floo_router<FlitT, InFifoDepth = 2, OutFifoDepth = 2>
```

Current fixed structure:

- five inputs and five outputs;
- one ready/valid stream per port;
- one input FIFO per port;
- one XY route selector per input;
- a combinational crossbar;
- one five-requester wormhole arbiter per output;
- **one output FIFO per port**, depth 2 in the frozen configuration;
- no VC layer;
- no collective/reduction path;
- no credit path.

`OutFifoDepth` selects between two structurally different bodies,
`gen_out_fifo` and `gen_no_out_fifo`, mirroring the RTL's own generate branches.
Depth 0 removes the buffer rather than degenerating into a wire with different
timing. Both branches are cross-checked, 214 cycles each.

Crossbar optimization mirrors the relevant frozen RTL configuration:

- no input-to-same-output loopback (`NoLoopback = 1`);
- in XY mode, a flit entering from North/South cannot return to East/West after
  the X dimension should already have been resolved (`XYRouteOpt = 1`).

Both the handshake **and the data** of an illegal pair are tied off. The data
tie-off is not cosmetic: `floo_wormhole_arbiter` drives `data_o` from the
selected index even when that index is not valid, so a missing tie-off is
observable. That was the defect the router cross-check found.

Debug outputs expose:

- input FIFO occupancy per port;
- selected input per output;
- output lock state per output.

They are model-defined and not part of the signed contract, and **they stop at
the router boundary**: `floo_mesh` does not propagate them upward, which is why
the clock gate described in section 5.1 cannot observe mesh state.

This block passes a 214-cycle RTL cross-check at `OutFifoDepth` 2 and 0, against
the real `floo_pkg`, `floo_route_select`, `floo_wormhole_arbiter`, and
`common_cells` sources from the Bender-generated file list. No shim is involved.

### 9.7 `floo_mesh.hpp` and `axi_noc.hpp`

Templates:

```cpp
floo_mesh<FlitT, Width, Height, InFifoDepth, OutFifoDepth>
axi_mesh_noc<Width, Height, InFifoDepth, OutFifoDepth> // raw pair
axi_noc<Width, Height, InFifoDepth, OutFifoDepth,      // pair + chimneys
        AxiIdBits, OutIdWidth, MaxTxns, MaxTxnsPerId>
```

`floo_mesh` behavior:

- creates `Width * Height` routers;
- assigns coordinates in row-major order:
  `node_index(x, y) = y * Width + x`;
- connects East/West and North/South neighbors;
- ties absent boundary inputs invalid and boundary outputs not-ready;
- uses each router’s Eject input for endpoint injection;
- uses each router’s Eject output for endpoint ejection;
- exposes one abstract inject/eject ready/valid endpoint per node.

`axi_mesh_noc` puts two of them side by side, one carrying `axi_req_flit` and
one carrying `axi_rsp_flit`, and exposes a `network_port<FlitT>` per node per
network. `axi_noc` owns one `axi_chimney_node` per coordinate and connects every
raw inject/eject signal internally. Its public boundary is
`manager(node)`/`subordinate(node)`, with `chimney(node)` for observation.

**Timing status: signed.** The inter-node cross-check compares a 3x3 model mesh
against a grid of the frozen `floo_axi_router` for 1872 node-cycles. What that
signs is the composition — coordinates, neighbour wiring, boundary tie-offs, and
the resulting per-hop cost.

Structural caveat that survives: the inter-router links are combinational
`SC_METHOD` connections and all storage lives in the router input and output
FIFOs. There is no separate link pipeline/cut module. The frozen configuration
uses no link cuts, which is why the timing signs off anyway, but the model has
no configurable link latency to turn on.

Two properties learned while signing this, recorded because they will recur:

- **`NoLoopback = 1` makes a self-addressed flit undeliverable.** It is not
  dropped; it wedges the injecting node's input FIFO permanently.
  `noc_interconnect` refuses such a placement at construction time.
- **A passing mesh cross-check proves nothing if the stimulus stopped moving.**
  Open-loop injection of truncated wormhole packets deadlocked all nine nodes by
  cycle 139 of 208, and both sides agreed on the deadlock. Count the last cycle
  each node accepted an injection before trusting a pass.

---

## 10. Verification evidence

### 10.1 Standalone SystemC regression

Forty tests pass with GCC 11.5.0 and SystemC 2.3.4:

| Test | Verified behavior |
|---|---|
| `test_reference_model` | Region base/end behavior, overlap rejection, out-of-mesh rejection, X-before-Y path |
| `test_stream_fifo` | Depth-2 spill and depth-4 FIFO branches: reset, fill, refused push at full (with and without a simultaneous pop), pointer wrap, drain order, mid-stream reset |
| `test_xy_route_select` | X-before-Y result, lock acquisition, locked route retention, release on last, local Eject |
| `test_wormhole_arbiter` | Reset priority, selected-ready behavior, packet lock, no interleave, round-robin advancement at two routes |
| `test_floo_router` | Two contenders for East, stalled output, FIFO occupancy, two-flit packet continuity, waiting requester service. Pinned to `OutFifoDepth = 0` so that branch stays covered |
| `test_floo_mesh` | 2x2 injection from `(0,0)` to `(1,1)`, unique correct ejection, stable data/valid under destination stall |
| `test_axi_noc` | Legacy abstract endpoints over `axi_mesh_noc`: two separate `req`/`rsp` meshes, independent traffic and structural latency |
| `test_axi_noc_chimney` | A-2 signal composition: timed source and target chimneys over a 2x2 mesh; two-beat write, three-beat read, B/R back-pressure, downstream-ID rewrite/restoration, metadata/RoB release and no wrong-node leakage |
| `test_noc_interconnect` | The TLM wrapper contract through A-3's timed chimney path: payload validation; byte-enable/`WSTRB` and lane placement; error classification; region guards; delay rounding; reset and idle wake-up; bounded concurrent second-manager scoreboard; sparse and maximum-length bursts; widened-read policy; top-of-address-space accesses; all four AXI response mappings; and target-delay exclusion. Its 4x4 no-contention calibration is pinned at **10 network cycles for one hop and 30 for six** |
| `test_noc_interconnect_stress` | Step 10.3 bounded deterministic-random stress: three staggered managers contend for one delayed RAM with 1/2/4/6/8/13/24-byte and sparse multi-beat transfers. Per-requester byte/completion scoreboards, target records, per-transaction deadlines and a global watchdog cover requester attribution, response/order, the half-cycle arrival window, odd tails, error routing, whole-network quiescence and idle wake-up |
| `test_noc_interconnect_concurrency` | Step 10.5 launches seven calls from independent SystemC threads through one upstream socket. With capacity set to two it proves saturation/back-pressure, separate read/write FIFO order, simultaneous B/R ownership, data/error ownership, slot release, per-transaction target-delay exclusion and final quiescence |
| `test_noc_interconnect_observer` | Step 12.7's passive completion observer: one record per completion in both timing modes, correct requester/address/length/direction, records summing to the interconnect's own latency total, concurrent managers keeping their own attribution, and identical traffic with and without an observer producing the same counts |
| `test_noc_interconnect_fast` | Step 11 drives identical traffic through detailed and fast instances over one through six hops, 1/2/4/8-byte widths and 32-byte bursts. The hard tolerance is one cycle. It also checks incoming annotation, rounded target delay, zero internal time advance, error/data/target-effect equivalence, sparse unaligned writes, exception-time slot cleanup and mesh bypass. This is model-to-model calibration, not RTL equivalence |
| `test_axi_lanes` | `AxSIZE`, `AxLEN`, lane offset and per-beat `WSTRB` for 13 address/length shapes, plus byte-enable holes and short repeating enable arrays. Checked on the fields themselves, not through a target, because a packing error and a matching unpacking error cancel |
| `test_noc_interconnect_bad_config` | Configurations refused before any traffic: zero or more than eight upstream ports for the frozen 3-bit ID; per-port outstanding capacity outside 1..32; unknown timing backend; a non-positive clock period; self-node target/manager placements; invalid/overlapping regions; and proof rejected calls preserve slots and placement |
| `test_axi_lanes_odr` | Two translation units including `axi_lanes.hpp` in one link. The only test that can catch a missing `inline` |
| `test_axi_chimney_manager_response` | The manager-side response unpacker: channel decode, per-channel back-pressure, and the AW→B / AR→multi-beat-R counter release loop |
| `test_noc_counters` | Hand-derived accept/stall/high-water counts, per-port identities, conservation across a drained router |
| `test_axi_types` | Hand-computed AXI channel widths, channel-to-link mapping, reserved-bit padding, `OutIdWidth` independence |
| `test_axi_sizing_trace` | 8-configuration sizing table against the RTL-captured golden |
| `test_axi_chimney_pack` | Flit assembly per channel, both destination-decode modes, AW/W select FSM |
| `test_rob_order_gate` | The `NoRoB` same-ID/different-destination stall rule and its per-ID capacity |
| `test_axi_endpoint` | Manager and subordinate transactors: AW/W coupling, response routing, ID restoration, ordering |
| `test_chimney_req_trace_sc` | 16-flit request trace against the RTL-captured golden |
| `test_chimney_rsp_trace_sc` | 8-flit response trace against the RTL-captured golden |
| `test_route_trace_sc` | CSV-driven expected route/lock trace |
| `test_fifo_trace_sc_d2` | 133-cycle FIFO trace against the RTL-captured depth-2 golden |
| `test_fifo_trace_sc_d4` | 133-cycle FIFO trace against the RTL-captured depth-4 golden |
| `test_arbiter_trace_sc_n2` | 152-cycle arbiter trace against the RTL-captured 2-route golden |
| `test_arbiter_trace_sc_n4` | 152-cycle arbiter trace against the RTL-captured 4-route golden |
| `test_arbiter_trace_sc_n5` | 152-cycle arbiter trace against the RTL-captured 5-route golden |
| `test_router_trace_sc_d2` | 214-cycle router trace, `OutFifoDepth = 2`, against the RTL-captured golden |
| `test_router_trace_sc_d0` | 214-cycle router trace, `OutFifoDepth = 0`, against the RTL-captured golden |
| `test_rob_trace_sc` | 127-cycle `NoRoB` ordering trace against the RTL-captured golden |
| `test_chimney_timing_trace_sc` | 141-cycle chimney request-timing trace against the RTL-captured golden |
| `test_chimney_rsp_timing_trace_sc` | 221-cycle chimney response/subordinate-timing trace against the RTL-captured golden |
| `test_mesh_trace_sc` | 1872 node-cycles of 3x3 mesh trace against the RTL-captured golden |

Most tests are directed and small; Step 10.3 adds one bounded,
deterministic-random multi-initiator scenario. This is not a substitute for a
full AXI protocol checker or a long multi-seed stress campaign. Fifteen tests
replay an RTL-captured golden, so they detect a model regression offline, but a
golden replay is not the cross-check — the cross-check is section 10.11, and it
needs Verilator.

**The gap this level cannot close:** none of these tests caught the platform bug
in section 13.10, because that bug lived in `platforms/noc_soc` rather than in
the component, and a component test cannot cover a platform. Step 10.2 added
that missing level as `noc_soc_firmware_regression`.

### 10.2 Route-selector RTL cross-check

The cross-check runs the same 12-cycle CSV stimulus through:

1. `xy_route_select<test_flit>` in SystemC;
2. the unmodified frozen `hw/floo_route_select.sv` in Verilator.

Compared trace columns:

```text
cycle,pre_route,pre_locked,post_route,post_locked
```

Both phases as of 2026-07-30. This harness recorded post-edge only until then
and was the single exception to rule 9b. The pre-edge column is not decoration:
it separates the cycle in which the route lock is *acquired* from the cycles in
which it is merely held, and shows the release happening on the same edge as the
route change.

Coverage:

- asynchronous reset observation;
- East;
- West;
- North;
- South;
- local Eject;
- valid without ready;
- first accepted non-last flit;
- locked route under back-pressure;
- accepted last flit and lock release.

Result:

```text
route-select cross-check PASS: 12 cycles match
```

The expected trace, captured from the frozen RTL:

```text
cycle,pre_route,pre_locked,post_route,post_locked
0,4,0,4,0
1,4,0,4,0
2,1,0,1,0
3,1,0,1,0
4,1,0,1,1     <- the lock is taken on this edge
5,1,1,1,1     <- held
6,1,1,3,0     <- released, and the route changes, on the same edge
7,4,0,4,0
8,0,0,0,0
9,2,0,2,0
10,1,0,1,0
11,3,0,3,0
```

### 10.3 Input-FIFO RTL cross-check

`rtl_crosscheck/run_stream_fifo_crosscheck.sh` runs one 133-cycle CSV stimulus
through:

1. `stream_fifo_optimal_wrap<sc_uint<64>, Depth>` in SystemC;
2. the unmodified `common_cells` RTL at the locked revision, with the same
   parameters `hw/floo_router.sv` uses (`flush_i` low, `testmode_i` low,
   `usage_o` unconnected).

Both wrap branches are covered: `Depth = 2` (spill register) and `Depth = 4`
(`stream_fifo` over `fifo_v3`).

Compared trace columns:

```text
cycle,pre_ready,pre_valid,pre_data,post_ready,post_valid,post_data
```

The pre-edge sample is the handshake view the environment acts on within the
cycle; the post-edge sample is the resulting state. Recording both matters:
post-edge-only sampling was demonstrated to hide a `ready_o` that wrongly
depends on the current `ready_i`.

Result:

```text
stream-fifo depth 2 cross-check PASS: 133 cycles match
stream-fifo depth 4 cross-check PASS: 133 cycles match
```

Stimulus coverage: reset, idle, fill past full under stall, push refused while
full, push attempted while full and popping, drain to empty, full-rate
streaming, pointer wrap, mid-stream reset, single-cycle valid/ready pulses, and
80 cycles of deterministic pseudo-random `valid_i`/`ready_i` toggling.
`tests/data/gen_stream_fifo_stimulus.py` regenerates the CSV.

The harness was validated with two negative controls, each of which must fail:

| Injected defect | Observed |
|---|---|
| `ready_o` made dependent on `ready_i` | FAIL, first at the cycle-9 pre-edge column |
| Superseded accept-at-full push rule | FAIL, `data_o` divergence from cycle 9 |

Rerun those injections before trusting any future change to this harness.

### 10.4 Wormhole-arbiter RTL cross-check

`rtl_crosscheck/run_wormhole_arbiter_crosscheck.sh` runs one 152-cycle CSV
stimulus through:

1. `wormhole_arbiter<test_flit, NumRoutes>` in SystemC;
2. the unmodified frozen `hw/floo_wormhole_arbiter.sv` over the unmodified
   locked `cf_math_pkg`, `lzc`, and `rr_arb_tree`.

Configurations covered: `NumRoutes` 5 (the router configuration, non
power-of-two tree), 4 (full binary tree), and 2 (single-level tree, matching
the standalone unit test).

Compared trace columns:

```text
cycle,pre_ready,pre_valid,pre_data,pre_selected,
post_ready,post_valid,post_data,post_selected,
valid_q,last_q,rr_q,lock_q,req_q
```

The registered state is read from the RTL through hierarchical references, so
the comparison pins the arbiter's internal state and not only its outputs.

Result:

```text
wormhole-arbiter routes 5 cross-check PASS: 152 cycles match
wormhole-arbiter routes 4 cross-check PASS: 152 cycles match
wormhole-arbiter routes 2 cross-check PASS: 152 cycles match
```

Stimulus coverage: reset, idle, single requester with and without ready, each
input served alone in turn, a multi-flit packet while other inputs request,
all inputs requesting at once, contention under back-pressure, two interleaved
multi-flit packets, a requester withdrawing while selected, `ready_i` with no
requester, mid-packet reset, and 100 cycles of deterministic pseudo-random
`valid_i`/`ready_i`/`last` traffic. `tests/data/gen_wormhole_arbiter_stimulus.py`
regenerates the CSV.

The only local SystemVerilog in this compile is `floo_pkg_empty.sv`, an
intentionally empty package. `floo_wormhole_arbiter.sv` imports `floo_pkg` but
uses no symbol from it, and compiling against an empty package proves that
instead of asserting it. The frozen `hw/floo_pkg.sv` is not used because it
depends on `axi_pkg`, which no current cross-check needs.

Six defects were injected to validate the harness:

| Injected defect | Result |
|---|---|
| Round-robin advance replaced by `selected + 1` | FAIL on `rr_q` from cycle 4 |
| `ready_o` gated by the selected input's own valid | FAIL on `ready_o` from cycle 40 |
| Snapshot never refreshed on `last_q` | FAIL from cycle 5 |
| Both packet holds removed at once | FAIL on selection from cycle 13 |
| Tree fed live `valid_i` instead of the snapshot | PASS — equivalent, see below |
| Tree `LockIn` disabled | PASS — equivalent, see below |

**Redundant hold mechanisms.** The last two injections pass because the
wrapper's `valid_q` snapshot and the tree's `LockIn` implement the same packet
hold. Whenever `lock_q` is low, the reachable state guarantees
`valid_d == valid_i`; whenever `lock_q` is high, the tree ignores its request
input entirely. Either mechanism alone reproduces the frozen behavior. Removing
both does break the comparison, which is what the fourth injection shows. This
is a property of the RTL, not a weakness of the stimulus, and it should not be
"fixed" by adding stimulus.

### 10.5 Router RTL cross-check

`rtl_crosscheck/run_router_crosscheck.sh` runs one 214-cycle CSV stimulus
through:

1. `floo_router<test_flit, 2>` in SystemC;
2. the unmodified frozen `hw/floo_router.sv` in the parameter set frozen in
   `docs/P0_SCOPE.md`.

**No shim of any kind is used.** The RTL compile comes from the
Bender-generated file list, so `floo_pkg`, `floo_route_select`,
`floo_wormhole_arbiter`, `floo_vc_arbiter`, and every `common_cells`
dependency are the real frozen sources. Types are built from the frozen
`floo_noc/typedef.svh` macros, so the header layout is the real one.

Compared trace columns:

```text
cycle,pre_ready,pre_valid,pre_d0..pre_d4,
post_ready,post_valid,post_d0..post_d4,
mask0..mask4
```

`maskN` is the one-hot `route_mask[N]`, which the router uses internally
instead of the encoded index. The route-selector cross-check had only ever
compared `route_sel_id_o`, so this closes that gap empirically.

Result:

```text
floo-router cross-check PASS: 214 cycles match
```

`floo_router.sv` keeps its `StableValidIn` and `StableValidOut` assertions
active under Verilator, because `INC_ASSERT` is gated on `SYNTHESIS` while the
generated file list only defines `TARGET_SYNTHESIS`. The runner greps the
simulation log and fails if either fires, so a stimulus that violates the
router's handshake contract is reported as a stimulus defect rather than
silently tolerated. The current stimulus fires neither.

#### The defect it found

The model tied off only the *handshake* of an illegal input/output pair. The
RTL also ties off the *data*:

```systemverilog
assign masked_data[out][v][in] = '0;
```

This is observable because `floo_wormhole_arbiter` drives `data_o` from the
selected index even when that index is not valid. Fixed in
`floo_router.hpp::connect_crossbar`.

Three defects were injected to validate the harness:

| Injected defect | Result |
|---|---|
| Crossbar data tie-off reverted | FAIL from cycle 15 |
| `XYRouteOpt` Y-to-X restriction removed | FAIL from cycle 15 |
| `NoLoopback` restriction removed | FAIL from cycle 15 |

### 10.6 AXI sizing cross-check

`rtl_crosscheck/run_axi_sizing_crosscheck.sh` compares the model's mirror of
the FlooNoC flit sizing arithmetic against the real `floo_pkg` functions, which
call the locked `axi_pkg`. It is not a cycle comparison: the functions are
pure, so both sides evaluate the same configuration list and the tables are
compared exactly. The RTL compile comes from the Bender-generated file list, so
no shim is involved.

Compared per configuration: each AXI channel's payload width, its
channel-to-link mapping, both physical channel widths, and each channel's
reserved-bit count.

Result:

```text
axi-sizing cross-check PASS: 8 configurations match
```

The harness exists because two details would otherwise be silently wrong
everywhere downstream:

- `get_max_axi_payload_bits` adds one spare bit, so a physical channel is
  always at least one bit wider than its widest payload;
- the channel widths use `cfg.InIdWidth`, never `OutIdWidth`.

Three defects were injected to validate it:

| Injected defect | Result |
|---|---|
| Spare bit dropped from the physical channel width | FAIL on every configuration |
| `OutIdWidth` used for the B channel width | FAIL on every configuration |
| Wrong W-channel strobe divisor | FAIL on every configuration |

Note one RTL edge case the model reproduces as written rather than smoothing
over: `get_axi_chan_width` uses the raw `cfg.UserWidth`, while
`FLOO_TYPEDEF_AXI_FROM_CFG` declares the user type as
`logic [floo_iomsb(UserWidth):0]`. At `UserWidth == 0` those disagree by one
bit. No configuration in scope uses zero user width.

### 10.7 Chimney — all four quadrants signed

`include/floo_noc_model/axi_chimney_pack.hpp` mirrors the flit-assembly
`always_comb` blocks of `hw/floo_axi_chimney.sv`, its `gen_route` destination
rules over `hw/floo_id_translation.sv`, and its `aw_w_sel_q` state.
`include/floo_noc_model/axi_chimney.hpp` adds the timed wrapper: the request
arbiter, the response arbiter, metadata FIFOs and manager-side response
unpacker.

`tests/test_axi_chimney_pack.cpp` remains a **contract test against the RTL
text**, not an equivalence proof; keep reading it that way. The equivalence
proof is five separate cross-checks against the unmodified chimney:

| Cross-check | What it compares |
|---|---|
| `run_chimney_req_crosscheck.sh` | 16 request flits, content |
| `run_chimney_rsp_crosscheck.sh` | 8 response flits, content |
| `run_chimney_timing_crosscheck.sh` | 141 cycles, request-path timing |
| `run_chimney_rsp_timing_crosscheck.sh` | 221 cycles, response and subordinate side |
| `run_chimney_mgr_rsp_crosscheck.sh` | 78 cycles, manager-side B/R unpacking, back-pressure and RLAST-qualified counter release |

Isolating the packing was impossible — it is inline `always_comb` — so the
content/timing harnesses instantiate the whole chimney, which also brings in
the meta buffer and the `NoRoB` gate. That turned out to be the right thing to
do anyway.

**`ChimneyDefaultCfg` sets `CutAx = CutOup = CutRsp = 0`.** An earlier plan for
Step 8 was to "model the chimney's cuts"; all three are bypassed in the frozen
configuration, so there is nothing to model. Check the config before modelling a
pipeline stage.

**The defect the request-timing cross-check found.** `floo_req_arb_in
[AxiW:AxiAr]` is an **ascending** packed range, `[1:2]`, so the *first* index in
the declaration is the most significant element and arbiter index 0 is **AR, not
W** — the reverse of the declaration's reading order. The model had W at index 0.
The same trap applies to `[AxiB:AxiR]` = `[3:4]`, where index 0 is R.

The upstream `hw/tb/tb_floo_axi_chimney.sv` cannot be reused under Verilator:
it depends on the class-based `axi_test` package, which Verilator does not
support. It remains usable under VCS, installed at
`/opt/synopsys/vcs/X-2025.06/bin/vcs`, if a class-based driver is ever wanted.
The five harnesses here are hand-written synchronous BFMs instead; see section 16
rules 9f to 9i for the four defects that idiom cost before it worked.

Rules captured from the RTL, all now confirmed by the cross-checks above:

| Rule | Source |
|---|---|
| AW has `hdr.last = 0`; W has `hdr.last = w.last` | AW/W form one wormhole packet |
| W carries the AW's reorder tag | the W flits belong to the AW's transaction |
| AR, B, R have `hdr.last = 1` | single-flit packets; R bursts deliberately not wormholed |
| `hdr.atop = (aw.atop != ATOP_NONE)` | a flag, not the ATOP code |
| B and R restore the original AXI id from retained metadata | the downstream id is a chimney-local reissue |
| W's destination is the id latched at AW acceptance | W never decodes its own address |

Destination decode has two modes under XY routing and the frozen tree uses
both: `UseIdTable = 1` is a system-address-map lookup, as
`floogen/examples/axi_mesh_xy.yml` selects; `UseIdTable = 0` extracts the
coordinate from address bit fields, as `hw/test/floo_test_pkg.sv` selects. The
model implements both.

### 10.8 Cross-check harness design

`run_route_select_crosscheck.sh`:

- enforces the required GCC/G++/PATH environment;
- checks the frozen RTL file SHA-256;
- builds the SystemC trace runner with a fresh CMake cache;
- compiles the original RTL with Verilator;
- runs both sides;
- compares normalized CSV lines exactly.

The leaf RTL needs declarations from `floo_pkg` and register macros from
`common_cells`. The local shim contains only:

- frozen enum values;
- `floo_iomsb`;
- `FF` and `FFL` register macro behavior.

It does not reimplement routing or lock logic.

`TARGET_SYNTHESIS` is defined only to suppress the RTL’s simulation-only warning
that intentionally fires when the directed test changes the destination header
while a route is locked. The route and state logic are unchanged.

`run_stream_fifo_crosscheck.sh` and `run_wormhole_arbiter_crosscheck.sh`
follow the same pattern but resolve their RTL through `fetch_rtl_deps.sh`
instead of a shim. That script reads the
`common_cells` revision from the frozen `Bender.lock`, refuses to continue if
the lock no longer matches the recorded frozen value, checks out that exact
revision, and verifies the SHA-256 of every compiled dependency file. No
behavioural SystemVerilog replacement is involved anywhere in this flow.

### 10.9 Known RTL lint observation

Verilator reports `WIDTHEXPAND` at:

```systemverilog
Eject + channel_i.hdr.dst_id.port_id
```

This does not affect the tested single local Eject port because `port_id` is
zero. Multiple local ports are not signed off and remain outside v0 scope.

### 10.10 What the RTL cross-checks do not prove

They do not compare:

- multiple local Eject ports;
- routing algorithms other than XY;
- multicast or collective routing;
- FIFO depths other than 2 and 4, and `usage_o` at any depth;
- FIFO `flush_i` and `testmode_i` behavior;
- explicit link cuts, because the frozen configuration has none;
- ATOP behaviour beyond carrying the header flag;
- `MaxUniqueIds > 1`, i.e. the `id_queue` branch of `floo_meta_buffer.sv` and
  the reorder-buffer modes it enables;
- coordinate values above 3, because the route-selector cross-check ran with a
  2-bit `x`/`y` id type.

And they cannot compare, by construction:

- the legacy `axi_endpoint.hpp` transactors or the TLM adapters in
  `noc_interconnect`, which have no RTL counterpart.

What *is* proven: twelve blocks, each in isolation, including the chimney's
request path, its subordinate side, its manager-side response unpacker, and the
mesh between them. Section 10.11 is the index.

**A block signed in isolation does not sign its parent.** The FIFO result did
not make the router cycle-equivalent, and the router result did not make the
mesh cycle-equivalent — each level needed its own comparison.

That rule applies to the integrated path too, and it is the reason the twelve
results must not be added up into an end-to-end claim. There is no cross-check
at the level above them. A-3 now makes the integrated path instantiate the same
timed chimney and mesh blocks, but the TLM adapters around their AXI signal
boundaries remain model-only. An earlier revision of this section asserted the
end-to-end claim two paragraphs above this rule, which is exactly the mistake
the rule exists to prevent.

### 10.11 The twelve signed cross-checks

Each runner lives in `rtl_crosscheck/`, compares a SystemC trace against the
unmodified frozen RTL through `compare_traces.py`, and refuses to run if the RTL
SHA-256 no longer matches revision `9a6972a`. `docs/STATUS.md` holds the detail
and the negative controls; this table is the index.

| Runner | Frozen RTL under test | Result |
|---|---|---|
| `run_route_select_crosscheck.sh` | `floo_route_select.sv` | 12 cycles |
| `run_stream_fifo_crosscheck.sh` | `common_cells` wrap, depths 2 and 4 | 133 cycles each |
| `run_wormhole_arbiter_crosscheck.sh` | `floo_wormhole_arbiter.sv` over the real `rr_arb_tree` | 152 cycles at 5, 4, 2 routes |
| `run_router_crosscheck.sh` | `floo_router.sv` | 214 cycles at `OutFifoDepth` 2 and 0 |
| `run_axi_sizing_crosscheck.sh` | `floo_pkg` sizing functions over locked `axi_pkg` | 8 configurations |
| `run_chimney_req_crosscheck.sh` | `floo_axi_chimney.sv`, request content | 16 flits |
| `run_chimney_rsp_crosscheck.sh` | `floo_axi_chimney.sv`, response content | 8 flits |
| `run_rob_crosscheck.sh` | `floo_rob_wrapper.sv`, `NoRoB` branch | 127 cycles |
| `run_chimney_timing_crosscheck.sh` | `floo_axi_chimney.sv`, request timing | 141 cycles |
| `run_chimney_rsp_timing_crosscheck.sh` | `floo_axi_chimney.sv`, response and subordinate side | 221 cycles |
| `run_chimney_mgr_rsp_crosscheck.sh` | `floo_axi_chimney.sv` + `floo_rob_wrapper.sv`, manager-side response | 78 cycles |
| `run_mesh_crosscheck.sh` | a grid of `floo_axi_router` | 1872 node-cycles |

Every one of them was validated by injecting defects that must fail; the counts
and the reasoning for the injections that legitimately *passed* are in
`docs/STATUS.md`. Rule 9e in section 16 exists because of those.

---

## 11. Build and run commands

### 11.1 Full standalone regression

```bash
cd /home/duyptt_HW/Desktop/VP_INTER/upgit/CDC-VP/components/floo_noc_model

export CC=/usr/bin/gcc
export CXX=/usr/bin/g++
export PATH=/usr/bin:/bin:$PATH
$CC -dumpfullversion
$CXX --version | head

make BUILD_DIR=/tmp/floo_noc_model_build test
```

Expected result:

```text
100% tests passed, 0 tests failed out of 34
```

Using a `/tmp` build directory avoids adding build artifacts to the component.

### 11.2 Equivalent direct CMake flow

```bash
export CC=/usr/bin/gcc
export CXX=/usr/bin/g++
export PATH=/usr/bin:/bin:$PATH
$CC -dumpfullversion
$CXX --version | head

cmake \
  -S /home/duyptt_HW/Desktop/VP_INTER/upgit/CDC-VP/components/floo_noc_model \
  -B /tmp/floo_noc_model_build \
  --fresh \
  -DCMAKE_BUILD_TYPE=Debug \
  -DSYSTEMC_HOME=/opt/systemc-2.3.4 \
  -DFLOO_NOC_MODEL_BUILD_TESTS=ON

cmake --build /tmp/floo_noc_model_build --parallel
ctest --test-dir /tmp/floo_noc_model_build --output-on-failure
```

### 11.3 Route-selector RTL cross-check

```bash
cd /home/duyptt_HW/Desktop/VP_INTER/upgit/CDC-VP/components/floo_noc_model
./rtl_crosscheck/run_route_select_crosscheck.sh
```

Optional path overrides:

```bash
FLOONOC_RTL_ROOT=/path/to/FlooNoC \
BUILD_ROOT=/tmp/my_floo_crosscheck \
./rtl_crosscheck/run_route_select_crosscheck.sh
```

The alternative RTL root must contain the exact frozen
`hw/floo_route_select.sv`; otherwise the hash guard fails.

### 11.4 Input-FIFO RTL cross-check

```bash
cd /home/duyptt_HW/Desktop/VP_INTER/upgit/CDC-VP/components/floo_noc_model
./rtl_crosscheck/run_stream_fifo_crosscheck.sh
```

Expected result:

```text
common_cells 1.39.0 @ 9ca8a76 verified
stream-fifo depth 2 cross-check PASS: 133 cycles match
stream-fifo depth 4 cross-check PASS: 133 cycles match
```

Path overrides:

```bash
FLOONOC_RTL_ROOT=/path/to/FlooNoC \
FLOO_RTL_DEPS_ROOT=/path/to/deps \
BUILD_ROOT=/tmp/my_fifo_crosscheck \
./rtl_crosscheck/run_stream_fifo_crosscheck.sh
```

### 11.5 Wormhole-arbiter RTL cross-check

```bash
cd /home/duyptt_HW/Desktop/VP_INTER/upgit/CDC-VP/components/floo_noc_model
./rtl_crosscheck/run_wormhole_arbiter_crosscheck.sh
```

Expected result:

```text
common_cells 1.39.0 @ 9ca8a76 verified
wormhole-arbiter routes 5 cross-check PASS: 152 cycles match
wormhole-arbiter routes 4 cross-check PASS: 152 cycles match
wormhole-arbiter routes 2 cross-check PASS: 152 cycles match
```

### 11.6 Router RTL cross-check

```bash
cd /home/duyptt_HW/Desktop/VP_INTER/upgit/CDC-VP/components/floo_noc_model
./rtl_crosscheck/run_router_crosscheck.sh
```

Expected result:

```text
floo-router cross-check PASS: 214 cycles match
```

This runner invokes `gen_rtl_filelist.sh` itself, so Bender must be installed.

### 11.7 AXI sizing cross-check

```bash
cd /home/duyptt_HW/Desktop/VP_INTER/upgit/CDC-VP/components/floo_noc_model
./rtl_crosscheck/run_axi_sizing_crosscheck.sh
```

Expected result:

```text
axi-sizing cross-check PASS: 8 configurations match
```

### 11.8 Full RTL compile flow and tool file lists

```bash
cd /home/duyptt_HW/Desktop/VP_INTER/upgit/CDC-VP/components/floo_noc_model
./rtl_crosscheck/gen_rtl_filelist.sh
```

Expected result:

```text
bender 0.32.1
FlooNoC @ 9a6972a clean, Bender.lock unchanged
common_cells @ 9ca8a76 agrees with fetch_rtl_deps.sh
verilator file list: /tmp/floo_noc_rtl_filelist/floo_verilator.f
vcs script:          /tmp/floo_noc_rtl_filelist/floo_vcs.sh
flist-plus:          /tmp/floo_noc_rtl_filelist/floo_flist_plus.f
resolved deps:       /tmp/floo_noc_rtl_filelist/resolved_deps.txt
floo_router elaborates from the generated list (0 errors)
```

If `bender` is missing, the script prints the exact pinned-artifact install
commands. Do not use the upstream `https://pulp-platform.github.io/bender/init`
installer: for v0.32.0 and later it is a cargo-dist wrapper that installs into
`$CARGO_HOME/bin` and edits shell profile files.

### 11.9 Dependency resolution only

```bash
./rtl_crosscheck/fetch_rtl_deps.sh
```

This is idempotent, needs network access on first run only, and prints
`COMMON_CELLS_ROOT=<path>` for downstream scripts.

### 11.10 The remaining six cross-checks

All follow the same pattern and honour `FLOONOC_RTL_ROOT` and `BUILD_ROOT`. Each
needs Bender and Verilator.

```bash
cd /home/duyptt_HW/Desktop/VP_INTER/upgit/CDC-VP/components/floo_noc_model
./rtl_crosscheck/run_chimney_req_crosscheck.sh
./rtl_crosscheck/run_chimney_rsp_crosscheck.sh
./rtl_crosscheck/run_rob_crosscheck.sh
./rtl_crosscheck/run_chimney_timing_crosscheck.sh
./rtl_crosscheck/run_chimney_rsp_timing_crosscheck.sh
./rtl_crosscheck/run_mesh_crosscheck.sh
```

Expected results:

```text
chimney-request cross-check PASS: 16 flits match
chimney-response cross-check PASS: 8 flits match
norob-ordering cross-check PASS: 127 cycles match
chimney-timing cross-check PASS: 141 cycles match
chimney-rsp-timing cross-check PASS: 221 cycles match
mesh cross-check PASS: 1872 node-cycles match
```

`rtl_crosscheck/install_floogen.sh` installs the pinned FlooGen 0.8.4 if a
generated topology is ever needed; no current cross-check requires it, because
the mesh harness builds its router grid directly.

### 11.11 The integration platform

```bash
export CC=/usr/bin/gcc CXX=/usr/bin/g++ PATH=/usr/bin:/bin:$PATH
cd /home/duyptt_HW/Desktop/VP_INTER/upgit/CDC-VP
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCDC_BUILD_NOC_SOC=ON
cmake --build build --target noc_soc --parallel

# synthetic survey
./build/platforms/noc_soc/noc_soc --mode survey --sim-us 200

# real firmware over the mesh
export PATH=/opt/toolchains/riscv-none-elf/bin:$PATH
make -C fw/dma_riscv clean
make -C fw/dma_riscv EXTRA_CFLAGS=-DDMA_BASE=0x10060000u
./build/platforms/noc_soc/noc_soc \
  --mode firmware --fw fw/dma_riscv/dma_test.elf --sim-us 2000
```

Those are the **manual** commands. The automated regression builds the firmware
in a private copy under its own log directory, so it needs no in-source
`make clean` and leaves the working tree untouched:

```bash
LOG_DIR=/tmp/my_logs ./platforms/noc_soc/tests/run_firmware_regression.sh
```

The firmware run must print `DMA PASS`. The DMA base override is required
because `fw/dma_riscv` defaults to the `VP_FX1_Full_SoC` address; `noc_soc`
follows `docs/peripheral_memory_map.md`, where DMA0 is at `0x1006_0000`.
`make clean` first, because the Makefile's only dependency is on the sources and
a changed `EXTRA_CFLAGS` alone will not retrigger the link.

The firmware run is automated as `noc_soc_firmware_regression`; the commands
above are the manual equivalent. See Step 10.2.

---

## 12. Current tools and dependency state

Available:

- `/usr/bin/verilator`, version 5.022;
- `/opt/synopsys/vcs/X-2025.06/bin/vcs`;
- FlooNoC `Bender.yml`;
- FlooNoC `Bender.lock`.

Also installed and currently resolvable in `PATH`:

- `/home/duyptt_HW/.local/bin/bender`, version 0.32.1.

No shell profile was modified by the install. If a clean or non-interactive
environment does not include `~/.local/bin`, prepend that directory for the RTL
file-list and cross-check scripts; those scripts invoke `bender` by command
name, not by absolute path.

The full dependency tree is now resolved. Two paths exist and they
cross-validate each other:

**Leaf path** — `rtl_crosscheck/fetch_rtl_deps.sh` pins single repositories by
revision and per-file SHA-256, without Bender. It is what the FIFO and arbiter
cross-checks use: fast, offline after the first run, and hash-guarded. It is
not a Bender replacement: it resolves no transitive tree and emits no source
order.

**Full path** — `rtl_crosscheck/gen_rtl_filelist.sh` runs Bender 0.32.1 against
the frozen `Bender.lock` and emits ordered tool file lists. All 13 packages
resolve; `Bender.lock` is byte-identical before and after; and the
Bender-resolved `common_cells` revision is asserted to equal the leaf path's
independent pin. `floo_router.sv` elaborates from the generated list with zero
Verilator errors.

Resolved set (from `Bender.lock`): `apb` 0.2.4, `axi` 0.39.9,
`axi_riscv_atomics` 0.8.3, `axi_stream` 0.1.1, `common_cells` 1.39.0,
`common_verification` 0.2.5, `fpnew` @ `e5aa6a0`, `fpu_div_sqrt_mvp` 1.0.4,
`idma` 0.6.5, `obi` 0.1.7, `register_interface` 0.4.7, `tech_cells_generic`
0.2.13, and the in-tree `floo_noc_pd` at `./pd`.

These are the resolved versions in `Bender.lock`; they intentionally take
precedence over looser or older version constraints shown in `Bender.yml`.

Two facts worth remembering:

- `Bender.lock` **is tracked** by git in the FlooNoC repository. The
  `Bender.lock` entry in that repository's `.gitignore` is inert because the
  file was committed upstream, so a lock change does show up in
  `git status`.
- Only `bender checkout` may be run against the frozen tree. `bender update`
  re-resolves and rewrites the lock.

The route-selector cross-check avoids external dependencies entirely because
that leaf requires package declarations and register macros, not external
behavioral modules.

Do not create a handwritten behavioral SV “shim” for FIFO or arbitration and
call the result an RTL cross-check. That would compare two models rather than
the SystemC model against the frozen implementation. FIFO/arbiter/router
cross-checking must use the exact locked `common_cells` behavior.

---

## 13. Known limitations and engineering risks

### 13.1 Accuracy boundaries

- The selected RTL datapath blocks and inter-node mesh are individually
  cycle-signed; the twelve cross-checks are indexed in section 10.11.
- The current integrated TLM path instantiates those timed chimney classes
  through `axi_noc` and drives both AXI signal boundaries cycle by cycle.
  That composition is still not one monolithic RTL-equivalence harness.
- **The remaining TLM integration layer is not signed and has no direct RTL
  counterpart**: the manager/subordinate adapters and payload mapping in
  `noc_interconnect`. `axi_endpoint.hpp` is legacy reference/test code after
  A-3. Step 10.3 signs this layer at model-integration level with bounded
  three-manager scoreboarding and mutation evidence; it still cannot make an
  RTL-equivalence claim for code that has no RTL counterpart.
- Combinational SystemC delta-cycle settling is not hardware latency.
- No timing number should be marketed as cycle-accurate until its path passes
  RTL comparison. Conversely, do not describe a path as an estimate once it has
  passed one — an earlier revision of this document kept calling mesh latency an
  estimate after Step 9.2 had signed it.
- The current signal-driven SystemC integration measures **10 cycles at one hop
  and 30 at six** on a 4x4 mesh, excluding target delay. These are model-level
  integration calibration values over individually signed blocks, not a
  composed full-path RTL golden. The legacy raw-mesh endpoint test measured
  11 and 30; 7 and 16 predate the output-FIFO correction.

### 13.2 Type completeness

- The route-selector cross-check ran with a 2-bit `x`, 2-bit `y`, 1-bit
  `port_id` id type, so coordinates 0..3 are signed off. The 4x4 platform uses
  exactly that `x`/`y` range and is therefore within the cross-checked coordinate
  range. The model's wider 16-bit `x`/`y` and 8-bit `port_id` representation
  beyond those values has no RTL evidence.
- `test_flit` uses a generic 64-bit payload; the AXI networks use
  `axi_req_flit`/`axi_rsp_flit`.
- Coordinate and header field widths are model choices, not a proven packed
  representation from the generated package. Field *sizing arithmetic* is signed
  (`axi_types.hpp`, 8 configurations); the packing is not.
- `collective_mask` and `collective_op` exist in `flit_header` but stay inert:
  frozen v0 is Unicast with `EnMultiCast = 0`, and
  `hw/floo_route_select.sv` only reads `hdr.collective_op` when `EnMultiCast` is
  set.

### 13.2b FIFO coverage boundaries

- Only depths 2 and 4 are cross-checked; other depths share the model code but
  have no trace evidence.
- `usage_o`, `flush_i`, and `testmode_i` are neither modeled nor compared.
- The cross-check uses a flat 64-bit data type, not a packed flit struct, so
  field packing is not exercised there.

### 13.3 Router completeness

Implemented and signed: five ports, input FIFO, XY route selection with lock,
optimized crossbar with data tie-off, per-output wormhole arbitration, and the
output FIFO at depth 2 and 0.

Still absent:

- No virtual channels.
- No credit protocol.
- No explicit VC arbiter.
- No collectives/reduction path.
- No multicast handshake history.
- No configurable route table.
- No source routing or YX routing.
- No explicit link cut/pipeline model — the frozen configuration has none.
- No full RTL assertions mirrored in SystemC. The router cross-check instead
  greps the RTL's own `StableValidIn`/`StableValidOut` assertions out of the
  simulation log and fails on either, so a handshake-contract violation is
  reported as a stimulus defect rather than silently tolerated.

### 13.4 AXI/network-interface completeness

Modeled **and RTL cross-checked** (section 10.7):

- system-address-map and address-offset destination decode;
- AW/W destination coupling through the latched AW id;
- AW followed by W packet route continuity via `hdr.last`;
- AW, W, AR request packing and B/R response packing;
- the AW/W channel-select FSM;
- restoring the manager's AXI id into responses;
- request and response flit arbitration onto the physical channels;
- metadata buffering (`floo_meta_buffer.sv`) in its `MaxUniqueIds = 1` branch;
- the `NoRoB` ordering rule and its per-ID capacity;
- burst semantics: multi-beat W with strobes, multi-beat R.

Still not modeled:

- the `id_queue` branch of `floo_meta_buffer.sv`, i.e. `MaxUniqueIds > 1`, which
  supports multiple downstream AXI IDs and response matching by ID;
- downstream AXI ID management beyond the single-ID reissue;
- configurable reorder-buffer modes (`floo_rob.sv` proper, as opposed to the
  `NoRoB` branch);
- ATOP behaviour beyond carrying the header flag.

`MaxUniqueIds = 1` does **not** limit a manager to one outstanding transaction.
The selected RTL branch reissues all non-atomic traffic under one downstream AXI
ID and retains read and write metadata in independent in-order FIFOs of depth
`MaxTxns = 32`. Multiple requests may therefore be outstanding when their
responses preserve the required order; the `NoRoB` gate separately allows
multiple requests of the same input ID to the same destination, up to its
counter capacity.

Step 10.5 removes the former wrapper-only one-in-flight limit. Each upstream
TLM port now admits a bounded number of calls (default and hard maximum 32),
queues manager requests, and retains one completion record per call.
Independent B and R waiter FIFOs preserve the selected one-ID ordering rule.
The constructor may lower the bound to apply earlier back-pressure.

This does **not** enable the `MaxUniqueIds > 1` branch. There is no current SoC
requirement for downstream out-of-order matching, and enabling it would require
new RTL evidence for `floo_meta_buffer.sv`'s `id_queue`, allocation, matching
and back-pressure. The previously measured **+9-cycle** contention remains a
historical baseline for the old single-waiter wrapper and its exact workload;
it is not the Step 10.5 throughput result.

Step 11 adds a second timing backend without adding a second functional
interconnect. `timing_mode::fast` uses the same validation, decode, lane/byte-
enable shaping, mapped target call and response classification as detailed
mode. It bypasses mesh evaluation and annotates:

```text
read  = 4 * Manhattan_hops + 6 + (beats - 1) cycles
write = 4 * Manhattan_hops + 6 + beats       cycles
```

The target's own delay is rounded up per access in both modes. Fast mode's
interconnect code preserves incoming local time and never waits; its downstream
targets must also annotate latency rather than call `wait()` in their own
`b_transport`. Detailed mode spends the incoming annotation and returns zero.
The fast estimate is explicitly
**no-contention**: it does not claim link back-pressure, packet locks, FIFO/
metadata occupancy, contention or clock-gating activity. Blocking-call order
and functional target effects remain preserved.

### 13.5 Verification gaps

- Step 10.3 provides bounded deterministic-random stress **at the TLM layer**:
  three concurrent initiators, per-requester byte/completion scoreboards,
  per-transaction deadlines and a global watchdog. It is one fixed-seed,
  bounded scenario, not a long-duration or multi-seed campaign.
- Lost-flit/deadlock behavior is bounded in that scenario; there is no formal
  or exhaustive liveness proof.
- No throughput/latency regression thresholds.
- No AXI protocol checker.
- Assert-enabled detailed `noc_soc` still exposes its intentional contract
  mismatch with Bremen: detailed mode spends incoming delay and returns zero,
  while the CPU requires nondecreasing local annotation. Step 11 closes the
  platform need with fast mode, which preserves/increases the annotation and
  reaches `DMA PASS` in an assert-enabled build. Do not use detailed mode with a
  caller whose quantum keeper requires LT semantics.
- ~~No automated platform-level test.~~ Closed by Step 10.2. Until then the
  firmware run was manual, which is why the component tests — 28 of them at the
  time — did not catch the bug in section 13.10.

Step 10.3 closes the original TLM-layer stress, scoreboarding and bounded-hang
requirements. Throughput/latency thresholds, multi-seed statistical coverage,
formal liveness and an AXI protocol checker remain separate gaps; none should
be inferred from the bounded stress pass.

### 13.6 Instrumentation and metrics

`noc_counters.hpp` now covers every production router on both physical meshes.
Implemented and measured:

- accepted flits and packets per input and per output;
- stall and busy cycles per port;
- input and output FIFO occupancy high-water and sum;
- exact transaction count, useful payload bytes and integer-cycle histogram;
- P50/P95/P99 and manager/target/flow attribution;
- modeled-window throughput, link utilisation, stall ratio, clock-active ratio,
  average hop count and Jain fairness;
- deterministic JSON, terminal dashboard and topology/load DSE.

`noc_metrics.hpp` and the JSON contract keep `[M]` measured, `[D]` derived,
`[A]` analytic and `[S]` static/spec values separate. Area, power and energy are
explicitly unavailable; no placeholder is permitted.

Still absent from v1: per-flit latency tags, metadata/RoB occupancy in the
production report, MMIO/uniform/permutation synthetic traffic and calibrated
area/power. These are optional extensions, not implied by the D0-D6 pass.

Two properties to keep in mind when using the counters:

- flit conservation holds only within a reset-free, fully drained window; a
  reset discards buffered flits;
- the counter block is passive by construction, and the router cross-check runs
  with it attached to keep that true.

The firmware fixed-window JSON is diagnostic and normally not drained because
the CPU remains active. `noc_benchmark` owns the signed D3 sequence
warm-up/reset/measure/stop/drain/report and is the only source of selectable DSE
rows. Throughput and link utilisation use total modeled window cycles, not only
mesh-active cycles; the latter would make a clock-gated quiet run look as busy
as a zero-gap hotspot.

One measurement trap, found while reporting platform numbers: a per-instruction
cost divided by total simulated time is **not** a per-instruction cost, because
firmware ends in `wfi` and `instret` stops advancing. The same workload appeared
to cost 29.7, 93.3 and 279.8 ns depending only on `--sim-us`. Charge only the
window in which the CPU was actually retiring.

That corrected value is still **modeled target time per instruction**, not
simulator performance. It says how much simulated time the platform assigns to
the workload. It does not say how many host seconds the simulator needed.
Performance work must additionally report a wall-clock metric such as retired
instructions per host second, host seconds per simulated microsecond, or
real-time factor.

### 13.6b Clock gating uses proven whole-network quiescence — CLOSED

Step 10.3 exports passive input/output FIFO occupancy and route/arbiter lock
state through `floo_router` and `floo_mesh`. `axi_noc::mesh_quiescent()` combines
both physical meshes with every chimney's request/response arbitration,
metadata, RoB and live AXI/link valid state.

`network_idle()` remains a separate wrapper predicate. The production network
thread stops its clock only for
`network_idle() && mesh_quiescent()` and explicitly asserts the one-way
invariant `network_idle() => mesh_quiescent()`. Equality is intentionally not
required: the stress test records cycles where the mesh is quiescent while the
wrapper still owns an un-injected request or target service.

Three independent controls keep the proof meaningful:

- changing the gate from AND to OR makes `test_noc_interconnect_stress` hit its
  bounded deadline;
- omitting output-FIFO occupancy makes `test_floo_mesh` misclassify a stalled
  ejection;
- omitting route/arbiter locks makes `test_floo_router` misclassify an open
  packet whose FIFOs are empty.

### 13.7 Integration risks

- A peripheral-style TLM wrapper would be architecturally wrong. `noc_interconnect`
  is a fabric adapter; keep it one.
- Gating the clock before whole-network quiescence can lose traffic. Step 10.3
  mitigates this with the production AND predicate, explicit invariant and
  three mutation controls; preserve all three layers of evidence.
- A-3 no longer translates a blocking TLM transaction into one atomic NoC
  action: AW, each W beat, AR, B and R handshake independently. Step 10.3 now
  stresses those adapters under three-initiator contention.
- **The timing backend must match the caller's synchronization contract.**
  Detailed `b_transport` spends simulated time and returns zero, so a caller
  requiring nondecreasing quantum-keeper annotation must not use it. Step 11
  closes the long-run platform requirement with fast mode: the interconnect
  preserves/increases `delay` and does not wait. Fast-mode downstream targets
  must follow the same LT convention and annotate their latency; a target that
  calls `wait()` still advances global time and violates that contract.
- Address ownership and the M:N socket topology are implemented from the CDC-VP
  platform map. `noc_soc` additionally requires exactly one execution mode:
  survey owns its reserved RAM page, peripheral walk and DMA0 experiment;
  firmware owns all firmware RAM contents and DMA0, while the synthetic probe
  emits zero transactions. Every firmware ELF `PT_LOAD` range is checked with
  `p_memsz` against `[0x80ff_f000, 0x8100_0000)` before the internal SoC is
  constructed. Preserve and review both contracts when the bus map or endpoint
  set changes.
- **No target may share a node with a manager.** `NoLoopback = 1` makes such a
  placement hang rather than fail. The wrapper refuses it at construction; do not
  remove that guard.
- The frozen 3-bit manager ID limits `num_initiators` to 1..8. The constructor
  refuses anything else before instantiating the mesh; do not widen that limit
  without changing and verifying the chimney configuration.

### 13.8 Licensing/provenance audit — CLOSED (2026-08-01)

Hardware-derived files use SPDX `SHL-0.51`; some reference/test files use
SPDX `Apache-2.0`. All 18 installed FlooNoC headers have one of those two
identifiers: 15 SHL-0.51 and 3 Apache-2.0.

The clean-prefix development package installs both licence texts, the CDC-VP
NOTICE and `PROVENANCE.md` under
`share/licenses/cdc-components/floo_noc_model`. The provenance record pins
FlooNoC, common_cells and axi to the exact revisions used by the model and
distinguishes RTL-derived headers from original CDC-VP integration code.

The `noc_soc` binary bundle additionally contains the Bremen RISC-V VP MIT
licence and `THIRD_PARTY.md`, because the CPU is linked statically, plus the
Apache-2.0 text applicable to CDC-VP and the bundled SystemC runtime. Preserve
these package commands whenever a new static or copied runtime dependency is
added. `noc_soc_packaging_regression` is the executable audit for this section;
do not replace it with an unrecorded manual sign-off.

### 13.9 Repository hygiene

Repository state can change independently of this document. Before editing,
run `git status --short` in both the CDC-VP and FlooNoC repositories. Treat all
pre-existing modifications and untracked files as user work, preserve unrelated
changes, and never assume the component has already been staged or committed.

---

## 13.10 RESOLVED — CPU firmware trap was caused by the synthetic survey

**Status: fixed and verified on 2026-07-29. This was not a
`noc_interconnect`, mesh, or back-to-back-store bug.**

### Symptom

Under the pre-Step 10.1 CLI, `platforms/noc_soc` with `--fw` loaded and started
the firmware correctly — its UART output appeared — and then the Bremen ISS
took a trap to `mtvec = 0`. The CPU spun at address 0 for the rest of the run.

```text
DMA platform start

[1] Prepare source/destination buffers
[ISS] Warn: Taking trap handler in machine mode to 0x0, this is probably an error.
[PC] trapped to 0 at 9660 ns, last pc before that 0x80000102
```

The sampled PC initially made `0x8000_0112` (`sb a3,0(a5)`) look suspicious,
but the actual exception was later in the instruction stream.

### Root cause

Running with `CDC_ISS_TRACE=1` exposed the actual trap:

```text
core  0: prv 3: pc 80000100: ADD zero (x0), zero (x0), zero (x0)
core  0: prv 3: pc 80000102: ZERO-INVALID
take trap 2, mtval=0
```

`mcause = 2` is an illegal instruction, not a store fault. The ELF contains a
valid 32-bit instruction at `0x8000_0100`, but `traffic_stub::run()` was
concurrently executing its RAM correctness test at the same addresses:

```cpp
access(true, kRamBase, ...);
access(true, kRamBase + 0x100, burst, sizeof(burst));
```

The second access wrote `{1, 2, 3, 4}` over live firmware `.text`. At
`0x8000_0100` the resulting bytes started with `01 00 00 00`: the ISS decoded
`0x0001` as a compressed NOP, advanced to `0x8000_0102`, then trapped on
`0x0000`. This exactly explains the observed PC pair. The apparent correlation
with a tight store loop was coincidental.

### Initial containment and final fix

The initial containment moved survey RAM writes into the final 4 KiB RAM page:

```cpp
constexpr std::uint64_t kSurveyScratch =
    kRamBase + kRamSize - 0x1000;
```

All synthetic RAM warm-up, single-beat correctness, and burst correctness
writes use that page. That stopped the current DMA firmware from being
overwritten, but relying only on its present link layout was not a complete
ownership contract.

Step 10.1 closes the issue: `--mode survey|firmware` is mandatory and the two
modes are mutually exclusive. Survey mode owns the scratch page, peripheral
walk, and synthetic DMA0 experiment. Firmware mode returns before every probe
access, so firmware owns RAM contents and DMA0 without competition. Before any
internal platform composition is constructed, each little-endian RISC-V ELF32
`PT_LOAD` range is checked using `p_memsz`; overlap with
`[0x80ff_f000, 0x8100_0000)` is rejected with both exact ranges.
ELF64 is rejected because the platform CPU is RV32.

The similarly observed `riscv_cpu_eval` trap was unrelated. Trace showed
`mcause = 15`, `mtval = 0x800f_fffc`: that platform maps only 64 KiB of RAM,
while the probe linker placed its stack at the top of a 1 MiB region.

### Verification

- Alignment/readback probe reached `DONE`; no `Taking trap` or `[PC]` report.
- SoC-map DMA firmware reached `DMA PASS`, including the 32-byte copy,
  `DMAKILL`, and the intentional undefined-opcode fault on channel 1.
- The fixed run retired 10,726 instructions in 500,704 ns and ended at
  `PC = 0x8000_047a`, not zero.
- Standalone model regression: **28/28 CTest tests passed** from a clean build
  directory.

The old `port_busy`/`port_free` serialization and `MaxUniqueIds` were not causal
for this incident. Step 10.5 later replaced the serialization with a complete
per-transaction design; it did not change the ownership split or
`MaxUniqueIds`.

### Three wrapper bugs already found and fixed during this work

Listed because they show the shape of what tends to go wrong here, and none of
them were visible with a single manager on the mesh:

- **Injection race.** `step_once` drove `inject_valid` from `has_request()` and
  then re-read it after the half-cycle wait. A `b_transport` running in another
  process could push a request into a manager during that wait, and the flit
  was popped without ever being driven — it vanished. Fixed by remembering what
  was actually driven.
- **Hold-off underflow.** The target's access latency was once discounted from
  every waiting transaction. A-3 first keyed it by requesting node; Step 10.5
  completed the fix for same-port concurrency by retaining the delay with each
  direction's FIFO-ordered completion.
- **Front-versus-back mix-up.** `axi_subordinate_endpoint::pending_*()` returned
  the *oldest* outstanding request while `absorb_request` used them to describe
  the request that had just **arrived**. With one manager the two are the same
  entry; with two managers hitting one node the DMA fetched from the wrong
  address and faulted on an undefined instruction. Fixed by splitting the two:
  `pending_*()` reports the newest arrival, responses still pop oldest-first.

Also fixed: the wrapper refused any access whose length was neither a power of
two below the bus width nor a multiple of it. A PL330 fetching a six-byte
`DMAMOV` is ordinary AXI traffic; it now uses full-width beats with the tail
marked by the final beat's byte strobes.

### Guard added

`noc_interconnect` now **refuses at construction** to place a target on a node
that already hosts an upstream port. `floo_router` defaults to
`NoLoopback = 1`, so a flit addressed to the node that injected it is
undeliverable and wedges that port for good — the platform hung silently the
first time the boot ROM was put on the CPU's node.

## 14. Step history and continuation policy

Steps 1 through 11 are done; each entry records what it produced and, where it
applies, what defect it found. Read those before touching the block they signed —
several of them exist because an earlier assumption was wrong, and the reasoning
matters more than the result.

Step 10's sub-steps did not run in numeric order; the execution order and the
reason for it remain in the table as history. Step 11 is complete; there is no
later numbered roadmap item.

### Step 1 — RTL cross-check the FIFO — DONE (2026-07-28)

Outcome:

- `common_cells` 1.39.0 resolved reproducibly at the locked revision by
  `rtl_crosscheck/fetch_rtl_deps.sh`, with a lock-file guard and per-file
  SHA-256 guards. No behavioural SV replacement was written.
- The RTL was found to disagree with the model. `ready_valid_fifo` was
  superseded by `include/floo_noc_model/stream_fifo.hpp`, which mirrors the
  `stream_fifo_optimal_wrap` hierarchy including the depth-2 spill-register
  branch used by the frozen router.
- 133-cycle exact match at depth 2 and depth 4 on pre-edge and post-edge
  `ready_o`/`valid_o`/`data_o`.
- Harness strength was demonstrated with two negative controls (section 10.3).

Remaining FIFO gaps are listed in section 13.2b.

### Step 2 — RTL cross-check the wormhole arbiter — DONE (2026-07-28)

Outcome:

- `hw/floo_wormhole_arbiter.sv` compiled unmodified over the locked
  `cf_math_pkg`, `lzc`, and `rr_arb_tree`, with the arbiter RTL and every
  dependency file guarded by SHA-256.
- The RTL disagreed with the model in four places, listed in section 9.5. The
  hand-written arbiter was replaced by a structural mirror:
  `include/floo_noc_model/rr_arb_tree.hpp` plus a rewritten
  `wormhole_arbiter.hpp`.
- 152-cycle exact match at 5, 4, and 2 routes, on the handshake outputs and on
  every arbiter register.
- Harness strength was demonstrated with six negative controls, including the
  finding that two of them are equivalent rewrites rather than defects
  (section 10.4).

Remaining arbiter gaps: `NumRoutes` values other than 2, 4, and 5 share the
model code but have no trace evidence; the tree's data and grant outputs are
neither modeled nor compared, because the frozen instantiation leaves them
unconnected.

### Step 3 — Resolve the full frozen RTL compile flow — DONE (2026-07-28)

Outcome:

- Bender 0.32.1 installed at `~/.local/bin/bender` from the pinned release
  artifact, without running the upstream cargo-dist installer and without
  touching any shell profile. Artifact and binary hashes are in section 3.2.
- `bender checkout` resolved all 13 locked packages against the frozen
  `Bender.lock`. `bender update` was never run.
- `Bender.lock` is byte-identical before and after, and the FlooNoC working
  tree stays clean.
- The Bender-resolved `common_cells` revision and file hashes match the
  independent pins in `fetch_rtl_deps.sh` exactly, so the FIFO and arbiter
  cross-checks were already compiling the same sources Bender resolves.
- `rtl_crosscheck/gen_rtl_filelist.sh` emits `floo_verilator.f`,
  `floo_vcs.sh`, `floo_flist_plus.f`, and `resolved_deps.txt`.
- `floo_router.sv` elaborates from the generated Verilator list with **0
  errors** under Verilator 5.022.
- The script's guards were tested: a dirty FlooNoC tree is rejected, a modified
  `Bender.lock` is rejected, and the lock-hash backstop fires when exercised in
  isolation.

Carry-over facts for Step 4:

- `bender script verilator` emits `+define+TARGET_SYNTHESIS` and
  `+define+TARGET_VERILATOR` by default. The route-selector harness already
  defines `TARGET_SYNTHESIS` for its own reason; a router harness built on the
  generated list inherits it, which suppresses simulation-only warnings. Decide
  deliberately whether that is wanted.
- The generated list carries the whole package, including chimneys, RoB, and
  the `axi`/`idma`/`fpnew` trees. That is fine for elaboration but means a
  router testbench should set `--top-module` explicitly rather than relying on
  automatic top detection.

### Step 4 — RTL cross-check the router — DONE (2026-07-28)

Outcome:

- The v0 parameter set was derived from `hw/floo_router.sv` rather than
  assumed, and is recorded in `docs/P0_SCOPE.md` together with what each
  parameter reduces the RTL to.
- The harness compiles the real RTL from the Bender-generated file list. No
  shim is involved anywhere, and the flit types come from the frozen
  `floo_noc/typedef.svh` macros.
- One real model defect was found and fixed: the crossbar tied off only the
  handshake of an illegal input/output pair, while the RTL also ties off the
  data (section 10.5).
- 214-cycle exact match on per-port `ready_o`/`valid_o` masks, per-output
  `data_o`, and the one-hot route mask per input.
- The router's own `StableValidIn`/`StableValidOut` assertions are live and do
  not fire on this stimulus; the runner fails the run if they ever do.
- Harness strength was demonstrated with three negative controls.

Two things this establishes that were previously unverified assumptions:

- `route_sel_o == 1 << route_sel_id_o` for unicast, so the model's use of the
  encoded index is equivalent to the RTL's one-hot mask;
- the model's loopback and Y-to-X crossbar rules correspond to the real
  `NoLoopback` and `XYRouteOpt` parameters.

Remaining router gaps: only `NumRoutes = 5`, `NumVirtChannels = 1`,
`InFifoDepth = 2`, `OutFifoDepth = 2` are signed off (and `0` too, though no
generated NoC uses it — see Step 9.2); virtual channels, credit
flow control, output FIFOs, collectives, multicast, and reduction are neither
modeled nor compared.

### Step 5 — Add measured NoC counters — DONE (2026-07-28)

Outcome:

- `include/floo_noc_model/noc_counters.hpp` adds `router_counters<FlitT,
  NumPorts>`, observing only signals whose timing is RTL-signed: the router
  boundary handshake and the input buffer occupancy.
- Counting happens exclusively on accepted transfers (`valid && ready`) or on a
  directly sampled state. Asserted-but-refused cycles are counted separately as
  stalls.
- The block is passive: `sc_in` ports only, driving nothing. This is checked,
  not asserted. `tests/router_trace_sc.cpp` instantiates the counters alongside
  the router, so the 214-cycle router cross-check runs with them attached and
  still matches the RTL exactly.
- `tests/test_noc_counters.cpp` pins absolute expectations hand-derived from
  the RTL-signed spill register: drained, it accepts one flit per cycle and
  never back-pressures; stalled, it accepts exactly two and then refuses three.
  It also asserts the per-port identity `busy == accepted + stall`, the depth
  bound on high-water, and conservation across a drained router.
- Reporting keeps measured counts, derived arithmetic, and analytic estimates
  separate, and emits none of the last kind.

Caveat found during validation: flit conservation holds only within a
reset-free, fully drained window, because a reset discards buffered flits. The
214-cycle router stimulus contains two reset events and legitimately shows 179
accepted in against 165 out.

Deliberately not added, because each needs an unsigned or unmodeled path:
transaction and flit latency, hop count, payload bytes per cycle, link
utilization, outstanding responses, RoB occupancy. See section 13.6.

This last paragraph is historical. Metrics D0-D6 later added transaction
latency, payload bandwidth, derived hop count and link utilisation through
passive production counters and the completion observer. Per-flit tags and
production metadata/RoB occupancy remain deferred.

### Step 6 — Add single-AXI channel and endpoint modeling — DONE (2026-07-28)

All eight items are complete:

1. **AXI channel types and sizing.** `axi_types.hpp`, cross-checked against the
   real `floo_pkg` functions over eight configurations.
2. **Endpoint transactors.** `axi_endpoint.hpp`: manager and subordinate,
   composing the signed rules. Model-side abstraction, not RTL-signable.
3. **Address-to-destination mapping.** Both `UseIdTable` modes, signed via the
   request-path cross-check.
4. **Request and response flit packing.** Both signed: 16 request flits and 8
   response flits match the unmodified chimney exactly.
5. **AW/W coupling and route continuity.** Signed with the request path.
6. **Response metadata.** `meta_buffer.hpp` models the `MaxUniqueIds == 1`
   branch; signed with the response path.
7. **Outstanding transaction tracking.** Covered by issuing three transactions
   per batch before answering, which is what gives the metadata FIFOs any
   coverage at all.
8. **Ordering.** `rob_order_gate.hpp` models the `NoRoB` admission rule. Both
   reorder buffers are `NoRoB` in the frozen configuration, so this is v0's
   actual ordering behaviour; the reordering RoB types need a different frozen
   configuration.

Two things deliberately **not** signed, and marked as such in `docs/STATUS.md`:
the ordering gate, because it is an admission decision that needs a harness
observing the request-side `ready`; and the transactors, which have no RTL
counterpart.

Also unverified **at the end of Step 6** — this is a dated record, not the
current state: chimney *timing* and arbitration in both directions, multi-beat R
bursts, ATOPs, back-pressure on either link, and the `MaxUniqueIds > 1` metadata
path. Steps 8 and 9 later signed chimney request timing (141 cyc) and the
subordinate side (221 cyc); for what is still unsigned today see section 14.

### Step 7 — Sign the `NoRoB` ordering rule — DONE (2026-07-29)

`rtl_crosscheck/run_rob_crosscheck.sh`: **127 cycles match**, twelve negative
controls all detected. See `docs/STATUS.md` for the full control table.

**The plan was wrong about where to put the harness.** It said to extend the
chimney request harness. `floo_rob_wrapper.sv` is a leaf module, so it is
instantiated directly instead. That keeps the comparison free of the chimney's
arbitration and cuts — which is exactly the timing divergence the plan
predicted would swamp the ordering signal. Prefer the leaf whenever the RTL
offers one.

**Three counter-bank rules the earlier model got wrong**, all from
`axi_demux_id_counters` in axi `src/axi_demux_simple.sv`:

- `full_o = |cnt_full` is a **global** OR across all `2**AxiIdBits` counters.
  One saturated ID stalls *every* ID. The old model had a per-ID full.
- `cnt_full[i] = overflow | (&in_flight)` saturates at
  `2**$clog2(MaxRoTxnsPerId) - 1`, so the default `MaxRoTxnsPerId = 32` admits
  **31**. The old model admitted 32.
- the counter pops by `rsp_i.id`, the ID on the *response*.

`delta_counter` holds `WIDTH + 1` bits with `q_o` the low `WIDTH` and
`overflow_o` the top bit; `id_counter_bank` reproduces that shape rather than
collapsing it into a saturating counter.

**A control that passes is not automatically a harness gap.** Rewriting the
simultaneous push/pop arm as an unconditional `+1` then `-1` passed, because on
a wrapping counter those cancel exactly — it injects no defect. It was
discarded and replaced, not recorded as a gap. Check for equivalence before
concluding the harness is blind.

**Stimulus must be directed here.** A same-ID/different-destination stall needs
the ID outstanding when the second destination is offered, and the global
`full_o` only shows when a second *completely idle* ID is offered while the
first is saturated. Neither happens by chance.
`tests/data/gen_rob_stimulus.py` carries a shadow of the rule used **only** to
keep the response stream legal — the bank has an underflow assertion, and
popping an unadmitted transaction corrupts both sides rather than comparing
them. The generator asserts its own phase invariants so a stimulus that stops
reaching the interesting states fails loudly.

### Step 8 — Cross-check chimney timing — DONE (2026-07-29)

`rtl_crosscheck/run_chimney_timing_crosscheck.sh`: **141 cycles match**, eleven
negative controls all detected, two discarded as equivalent rewrites. See
`docs/STATUS.md` for the tables.

**The plan's premise was wrong: the frozen chimney has no cuts.**
`ChimneyDefaultCfg` sets `CutAx = CutOup = CutRsp = 0`, and `floo_test_pkg`
takes the defaults. `i_req_out_cut` is instantiated with `Bypass = !CutOup = 1`
and is fully transparent. So "model the chimney's cuts" was not the work; the
request path's only state is the `aw_w_sel_q` FSM, the arbiter registers, and
the reorder-buffer counters. Check a generate condition's *value in the frozen
config* before planning around it.

**The defect it found: the request arbiter's index order is reversed.**

```systemverilog
floo_req_chan_t [AxiW:AxiAr] floo_req_arb_in;   // AxiW = 1, AxiAr = 2
```

That range is **ascending**, so the first index is the most significant
element: connected to `data_i[NumRoutes-1:0]`, the `AxiW` slot lands on bit 1
and `AxiAr` on bit **0**. Arbiter index 0 is AR. The model had W there, which
inverts round-robin priority whenever AW and AR contend. It showed at the first
traced cycle, because with `valid_i == 0` the arbiter drives
`data_o = data_i[0]` and the RTL was presenting an AR flit where the model
presented an AW.

Verilator's `ASCRANGE` warning is the only signal the RTL gives. The same
applies to `floo_rsp_arb_in [AxiB:AxiR]` — index 0 is R, index 1 is B — which
matters for Step 9 if the response path is composed next.

**Verilator 5.022 crashes** with "internal fault" on a hierarchical reference
to an enum *member* (`dut.SelAw`). Compare against the literal instead; the
enum's declaration order gives the value.

**Two controls passed and were discarded, not recorded as gaps.** Swapping the
two `aw_w_sel_d` assignments is unobservable because an AW acceptance needs
`sel == SelAw` and a W acceptance needs `sel == SelW`, so they can never both
fire. Building the AW flit with the AR reorder tag is unobservable because
under `NoRoB` both reorder buffers drive the same constant `{rob_req = 1,
rob_idx = 0}`. Always check for equivalence before concluding the harness is
blind — this is the third time a passing control turned out to be a
non-defect.

**Still unsigned after this step:** the response path's timing, the subordinate
side (`i_aw_out_queue`, the meta buffer), multi-beat R bursts, ATOPs, and the
reorder-buffer saturation corner. The response link is held idle here, and the
stimulus generator bounds offers per ID below the counter capacity.

### Step 9 — Replace abstract mesh endpoints — DONE (2026-07-29)

`include/floo_noc_model/axi_noc.hpp` plus `test_axi_noc`: AXI runs end to end
over separate `req` and `rsp` meshes on a 4x4 grid. 24/24 tests pass.

**The two-network shape is the IP's, not a choice.** `hw/floo_axi_router.sv` is
literally two `floo_router` instances with identical parameters, one per flit
type, with two independent `[NumRoutes-1:0]` port arrays. No shared
arbitration, no shared buffering, no ordering between them. So the raw fabric
is two `floo_mesh` instantiations — it was named `axi_noc` at Step 9 and was
renamed `axi_mesh_noc` when A-2 made `axi_noc` the chimney-backed composition.
`floo_mesh` was already templated on `FlitT`, so this needed no change to the
mesh at all.

**FlooGen 0.8.4 is installed and reproducible** via
`rtl_crosscheck/install_floogen.sh`. Two environment facts it works around:
FlooGen needs Python >= 3.10 and this host's default `python3` is 3.9, so it
uses `python3.11`; and it writes only into a build directory and then fails if
`git status` in the frozen tree is non-empty.

The generated `floo_axi_mesh_noc.sv` **confirms the port index order** the model
already used (North 0, East 1, South 2, West 3, Eject 4). Note FlooGen puts the
extra HBM endpoints in the *West* slot of the x=0 column, which is why a corner
router's index 3 is an endpoint rather than a neighbour.

**Latency is checked structurally, not against a golden.** Each router registers
its input (`InFifoDepth = 2`), so a round trip cannot beat one cycle per hop
each way. Measured 1 hop 11 cycles, 6 hops 30 cycles (corrected in Step 9.2;
the 7 and 16 first recorded here predate the output-FIFO fix), and the test
asserts the
bound plus far > near. That is what separates "the mesh was traversed" from "a
test that passes because nothing moved". Two controls confirm it: swapping the
address regions, and ejecting from the wrong network.

**A constraint found in the frozen configuration.** `MaxUniqueIds = 1` makes
`hw/floo_meta_buffer.sv` store metadata in a plain `fifo_v3` popped in order
with **no ID matching**; the `id_queue` keyed by AXI ID is the
`MaxUniqueIds > 1` branch. So the chimney assumes responses return in request
order per direction. One destination gives that; two do not, because `NoRoB`
only serialises the *same* AXI ID to a different destination. The v0 integration
must therefore prevent transactions sharing one metadata FIFO from returning
out of order or freeze a configuration with multiple downstream IDs and
ID-based response matching. Step 10.5 chooses the former: one input/downstream
ID, FIFO manager-request queues, and separate FIFO B/R completion owners.
`NoRoB` prevents the same input ID changing destination while requests remain
outstanding, and same-destination traffic retains channel order.

Later clarification: this constrains response ordering and downstream-ID use; it
does **not** mean only one request may be outstanding. See the implemented
bounded concurrency state in section 13.4. The FIFO/NoRoB behavior is
RTL-signed at leaf level; the wrapper request/completion queues are model-level
integration code with no RTL counterpart and are covered by
`test_noc_interconnect_concurrency` plus mutation controls.

### Step 9.1 — Sign the chimney response path and subordinate side — DONE (2026-07-29)

`rtl_crosscheck/run_chimney_rsp_timing_crosscheck.sh`: **221 cycles match**,
eleven negative controls all detected. `axi_chimney_response` in
`include/floo_noc_model/axi_chimney.hpp`. The chimney's request path and its
subordinate side are now
signed for timing.

**Not everything is bypassed on this side.** The request path had
`CutAx = CutOup = CutRsp = 0` and therefore no cuts at all; here
`i_aw_out_queue` is an **unconditional** `spill_register` (no `Cut*` parameter
gates it — AW and W share a link, so a subordinate may refuse the AW until its
W is valid), and the metadata FIFOs' `full` back-pressures the inbound link.

**The ascending-range trap, second instance.** `floo_rsp_arb_in [AxiB:AxiR]`
with `AxiB = 3`, `AxiR = 4` puts **R at index 0 and B at index 1**. Modelled
right first time only because the request-side defect had already taught the
pattern. Assume every `[Lo:Hi]` port array in this IP is ascending.

Also: `floo_req_out_ready = axi_ready_out[hdr.axi_ch]` — the inbound link's
`ready` is *selected by the channel the arriving flit names*.

**Two stimulus gaps, found by controls and closed.** The first run detected
only nine of eleven, and neither miss was an equivalence:

- the metadata-`full` control passed because `MaxTxns = 32` and the stimulus
  never had 32 outstanding writes;
- the `r.last` control passed because every R beat carried `last = 1`.

Both were unreachable states, not equivalent rewrites. **A passing control is
one or the other, and they need different fixes** — argue equivalence from the
RTL, or extend the stimulus. Three earlier passes in this project were
equivalences; these two were coverage.

The meta buffer is instantiated inside the `gen_mgr_port` generate block
despite `ChimneyCfg.EnSbrPort` being what selects it, so its hierarchical path
is `dut.gen_mgr_port.i_floo_meta_buffer.gen_no_atop_fifos.*`.

### Step 9.2 — Sign inter-node timing — DONE (2026-07-29)

`rtl_crosscheck/run_mesh_crosscheck.sh`: **1872 node-cycles match** on a 3x3
grid of the unmodified `floo_axi_router`. Six of seven negative controls
detected, one discarded as an equivalence. This was the last unsigned
**isolatable RTL timing block in the planned v0 slice**, not a composed
TLM-boundary timing proof; the abstract endpoint transactors remain outside RTL
equivalence.

**Do not cross-check against the FlooGen top.** `floo_axi_mesh_noc.sv` exposes
only AXI ports per endpoint, so it would need a full chimney at every node and
would fold chimney behaviour into a mesh measurement. `floo_axi_router` has
flit-level ports and is the isolatable unit. Use the generated netlist as the
authority for the *wiring rule* only.

**The defect it found: the model had no output FIFO.** Every FlooGen router
template hardcodes `.OutFifoDepth (2)`; `hw/test/floo_test_pkg.sv` defines no
router FIFO depths at all. The `OutFifoDepth = 0` that `docs/P0_SCOPE.md`
recorded as "the frozen v0 parameter set" was a choice made in the router
testbench back at Step 4, and **no generated NoC uses it**. End-to-end latency
went from 7 to 11 cycles at one hop and 16 to 30 at six once the buffer was
added. `floo_router` now takes `OutFifoDepth` (default 2) and both branches are
signed.

The lesson generalises: **a "frozen parameter set" is only frozen if it came
from the IP.** Check the generator templates, not just the test package.

**A second defect: `flit_header::last` defaulted to `true`.** The RTL ties
unconnected inputs to `'0`, and the mesh writes `FlitT{}` into edge inputs, so
every idle cycle diverged. Earlier cross-checks compared payloads rather than
`last` while idle and never saw it.

**Two stimulus defects that had silently killed the run**, neither visible from
the comparison because both sides agreed on a dead network:

- multi-flit packets injected open loop can be *truncated* when the closing
  `last` lands on a refused cycle; the half-open packet then holds its route
  forever. Keep multi-flit packets in lightly-loaded directed phases.
- `NoLoopback` defaults to 1, so a **self-addressed** flit is undeliverable and
  wedges its node's input FIFO permanently.

Together they had all nine nodes deadlocked by cycle 139 of 208. **Check the
stimulus is still live before trusting a pass** — count the last cycle each
node accepted an injection.

### Step 10 — CDC-VP TLM integration — IMPLEMENTED AND SIGNED OFF

This is a historical checkpoint. The handoff previously called the integration
design the next step, but the code has moved past that point:

- `noc_interconnect` presents a `bus_router`-like TLM target interface plus
  multiple tagged upstream ports and explicit mesh placement;
- `platforms/noc_soc` instantiates a 4x4 network with CPU, DMA, and survey
  managers, and routes RAM plus the SoC peripheral map through it;
- detailed blocking TLM accesses spend simulated time while traversing the
  network; Step 11 later added the fast annotated backend;
- `network_idle()` stops mesh clock evaluation when there is no in-flight
  work, and the `work` event restarts it;
- `test_noc_interconnect` covers the wrapper contract at component level;
- real SoC-map DMA firmware reaches `DMA PASS` through the NoC.

The integration was therefore a working proof of implementation, not a design
task. Steps 10.1 through 10.5 and Step 11 subsequently turned that proof into
the current automated contract. Do not restart the socket-topology design from
scratch.

#### Execution order — the single authoritative list

Everything else that names an order defers to this table. If two places
disagree, this one is right and the other is a bug. Individual step headings
carry **no ordinal words** — they went stale twice as the order changed, and a
heading that says "DO SECOND" while the table says otherwise is worse than one
that says nothing.

| # | Step | State |
|---|---|---|
| 1 | **10.2** — automated firmware regression | **done** (2026-07-30) |
| 2 | **pre-A1 cleanup round 1** — `docs/PRE_A1_REVIEW_FIX_PLAN.md` | **done** (2026-07-31), see its checklist for per-item evidence |
| 3 | **pre-A1 cleanup round 2** — `docs/PRE_A1_REVIEW_ROUND2_FIX_PLAN.md` | implementation complete; round-3 review found further gaps |
| 4 | **pre-A1 cleanup rounds 3 to 5** — `docs/PRE_A1_REVIEW_ROUND3_FIX_PLAN.md` | **done** (2026-07-31); reviewer signed at the end of round 5 |
| 5 | **A-1** — cross-check the manager-side response unpacker | **done** (2026-07-31): 78 cycles exact, six negative controls detected. The chimney's four quadrants are all signed |
| 6 | **A-2** — assemble a per-node chimney into `axi_noc` | **done** (2026-07-31): `test_axi_noc_chimney`, 56-cycle model-level composition test |
| 7 | **A-3** — drive it from `noc_interconnect` | **done** (2026-07-31): cycle-driven AW/W/AR/B/R, 34/34 tests, relevant RTL cross-checks and firmware regression pass; baseline 10/30 cycles |
| 8 | **10.3** — stress the TLM layer that remains above the chimney | **done** (2026-08-01): 35/35 tests, 32/32 controls, all RTL cross-checks and rebuilt platform regression pass |
| 9 | **10.1** — separate survey and firmware ownership | **done** (2026-08-01): mandatory mutually exclusive modes, zero synthetic firmware traffic, enforced ELF scratch reservation |
| 10 | **10.4** — install, packaging, licensing | **done** (2026-08-01): clean-prefix consumer, portable bundle run, `$ORIGIN` RPATH, licences and provenance |
| 11 | **10.5** — wrapper concurrency, and separately `MaxUniqueIds > 1` | **done** (2026-08-01): bounded same-port queues/slots, FIFO completion ownership; one downstream ID retained |
| 12 | **11** — the fast approximately-timed mode | **done** (2026-08-01): calibrated LT backend, both firmware modes, 37/37 tests and 39/39 controls |

**Why 10.3 sat after A-3.** A-3 removed the abstract endpoints from the
production path, so stressing them first would have targeted code scheduled for
removal. Step 10.3 therefore targeted the actual signal-driven integration:
bounded multi-initiator stress, lost-flit detection and proof of the quiescence
gate.

**A-1, A-2 and A-3 are part of the v0 completion gate**, not work beyond it.
See section 15.

### Step 10.2 — Add an automated real-firmware regression — DONE (2026-07-30)

The 28 standalone tests that existed at the time did not catch the platform bug
in section 13.10. They could not: it lived in `platforms/noc_soc`, and a
component test cannot cover a platform. What was built is a bounded CTest that:

1. builds `fw/dma_riscv` with `EXTRA_CFLAGS=-DDMA_BASE=0x10060000u`;
2. runs `noc_soc --mode firmware --fw <elf> --sim-us 2000`, the ELF built in a
   private copy of the firmware sources rather than in the working tree;
3. requires `DMA PASS`;
4. rejects `Taking trap`, `[PC] trapped`, SystemC errors, timeouts, and a final
   CPU PC of zero;
5. preserves the full simulation log on failure.

**Deviation from the original acceptance criteria, recorded rather than
quietly dropped.** This step originally asked for a second *image* — a small
CPU alignment/readback ELF reaching `DONE`. That image does not exist.

What runs instead is the platform's own synthetic survey,
`--mode survey`, requiring `result   all bytes match`. Describe it accurately:

- it is **not** a second firmware image, and not equivalent to one;
- it is a **substitute smoke path with reduced and different coverage**: it
  programs DMA0 from the synthetic probe, but has no CPU firmware workload or
  CPU interrupt-handling path;
- it does **not** uniquely separate a DMA fault from a CPU/NoC fault. The two
  runs share almost all of the interconnect, so the survey passing does not
  clear it. A failure in the firmware run but not the survey is *evidence
  towards* the CPU/DMA/firmware side — a hint about where to look first, not a
  decision procedure;
- a dedicated alignment/readback ELF remains genuinely worth writing if that
  separation is ever needed.

The automated regression builds the firmware in a **private copy** of its
sources, so it needs no in-source `make clean` and leaves the working tree
untouched. The `make clean` in section 11.11 is part of the **manual**
instructions, where it is still required: the Makefile depends only on its
sources, so a changed `EXTRA_CFLAGS` alone will not retrigger the link.

Acceptance criteria:

- the test fails with a preserved log if the synthetic survey again overwrites
  executable firmware state, if the firmware traps, or if the DMA path no longer
  reaches `DMA PASS`;
- it completes in bounded time on an unattended run;
- it is invoked by the same command that runs the rest of the CDC-VP tests.

Do not require this one firmware image to deterministically expose every wrapper
defect in section 13.10. Each wrapper defect needs its own focused regression or
negative control under Step 10.3; Step 10.2 owns platform-level firmware and
address-ownership failures.

### Step 10.3 — Stress the TLM wrapper — DONE (2026-08-01)

The routers, FIFOs, arbiters, chimney paths, ordering rule, and mesh timing are
RTL-signed. The endpoint transactors and TLM wrapper have no RTL counterpart,
so this is the highest-risk correctness layer in the project.

The implementation adds a separate `test_noc_interconnect_stress` so the
directed calibration contract in `test_noc_interconnect` stays focused. Its
bounded, scoreboard-driven scenario provides:

- three initiators concurrently access the same RAM target;
- mix reads and writes of 1, 2, 4, 6, 8, 13 and 24 bytes plus a sparse
  multi-beat transfer;
- use a target with 3.5 ns annotated latency and verify per-requester hold-off
  accounting;
- verify address, data, response, requester ownership, and completion order;
- cover idle-to-active wake-up, whole-network quiescence and unmapped accesses;
- use per-transaction timeouts and a global watchdog so lost flits fail rather
  than hang.

Reset-time submission remains in `test_noc_interconnect`, and the self-node
placement guard remains in `test_noc_interconnect_bad_config`; together the
35-test suite covers the full Step 10.3 list without duplicating those focused
cases in the stress scenario.

The stress test directly covers the integration defects already found: the
half-cycle injection race, requester hold-off attribution, oldest-request
metadata/order under newer arrivals, and odd-length burst tails.
Firmware/survey address ownership is a platform concern and belongs to the Step
10.2 regression and the Step 10.1 mode split, not to
`test_noc_interconnect_stress`.

The quiescence gap is also closed (section 13.6b). Passive input/output FIFO
occupancy and route/arbiter locks flow from `floo_router` through `floo_mesh`;
`axi_noc` combines them with both physical networks and chimney metadata, RoB,
arbiter and valid state. The clock gate requires wrapper idle **and**
`mesh_quiescent()`, and the thread asserts
`network_idle() => mesh_quiescent()`. The stress run also observes the legal
opposite state — mesh quiescent while wrapper work remains — so no equality
assumption is hidden in the gate.

Acceptance criteria:

- every listed defect has a test that fails when the fix is reverted;
- every transition into the clock-gated wait satisfies
  `network_idle() && mesh_quiescent()`;
- directed tests exercise states where the mesh is quiescent but wrapper work is
  still pending, proving that equality is not incorrectly required;
- no test can hang: every wait is bounded and a global watchdog exists.

All acceptance criteria pass. Verification evidence:

- fresh standalone build: **35/35 pass** with GCC/G++ 11.5.0 and SystemC 2.3.4;
- automated mutation runner: **32 detected, 0 missed**, including seven new
  Step 10.3 controls for half-cycle sampling, requester/response ownership,
  odd tails, the two-predicate gate, output-FIFO occupancy and packet locks;
- all twelve SystemC-to-RTL cross-check runners pass unchanged;
- after rebuilding the parent `noc_soc`, real DMA firmware and the synthetic
  survey both pass.

### Step 10.1 — Separate synthetic-survey and firmware modes — DONE (2026-08-01)

The two ownership modes are explicit and mutually exclusive:

- `--mode survey` rejects `--fw`; the synthetic manager writes only its
  reserved RAM scratch page, walks the peripheral map, and owns DMA0;
- `--mode firmware` requires `--fw <image.elf>`; firmware owns RAM contents and
  DMA0, and the synthetic probe returns before issuing any TLM transaction;
- omitting `--mode` or naming any other mode is an error rather than an
  ownership inference.

The final 4 KiB RAM page is now an enforced platform reservation:
`[0x80ff_f000, 0x8100_0000)`. Before `sc_start()` and before the CPU ELF loader
touches RAM, the constructor parses every little-endian RISC-V ELF32 `PT_LOAD`,
uses `p_memsz` so a BSS-only claim is included, and rejects an overlap with the
exact segment and reservation ranges. ELF64 is rejected because `noc_soc` uses
an RV32 CPU.

Focused coverage lives in
`platforms/noc_soc/tests/run_firmware_regression.sh`. It checks missing,
invalid and conflicting mode selections; relocates the real DMA ELF so a
`PT_LOAD` enters the scratch page and requires rejection before the
`noc_soc config:` construction marker; and checks the ownership counters and
mode-specific reports in both successful runs.

All acceptance criteria pass:

- survey mode still reports RAM, peripheral, and DMA measurements and nonzero
  synthetic counts: 2,366 transactions, 1,112 RAM writes, and 4 DMA-register
  writes in the signed-off run;
- firmware mode reports zero synthetic transactions, RAM writes, and DMA
  register writes;
- the current DMA firmware reaches `DMA PASS` through the automated regression;
- the relocated ELF is rejected before internal SoC construction with the
  exact conflicting range
  `[0x80fff000, 0x80fff7a9)` versus
  `[0x80fff000, 0x81000000)`;
- a fresh parent build with `CDC_BUILD_TESTS=ON` passes the registered
  `noc_soc_firmware_regression` CTest (1/1), and the fresh standalone component
  suite remains 35/35.

### Step 10.4 — Prove install, packaging, and licensing — DONE (2026-08-01)

All sign-off requirements pass and are reproduced by one registered runner:

```bash
./platforms/noc_soc/tests/run_packaging_regression.sh
```

`noc_soc_packaging_regression` is also registered in the parent CTest suite
with labels `packaging;distribution`. It configures a fresh Release child build
with its own install prefix and `CDC_PACKAGE_ROOT`; the inner build disables
tests, so running this gate cannot recursively register itself. Temporary roots
also prevent two jobs from deleting or overwriting the same `out/noc_soc`.
Verified after automation on 2026-08-01: the registered CTest passes 1/1 in
29.30 seconds; retained direct-run evidence is under
`/tmp/floo_noc_packaging_regression.IkFfCD`.

- a Release parent build installs successfully into the empty prefix
  created for that run;
- the independently configured fixture
  `tests/installed_consumer` uses only
  `find_package(cdc-components CONFIG REQUIRED)` and links the compiled
  `cdc::components::noc_interconnect`; it prints
  `installed noc_interconnect consumer PASS`;
- `noc_soc_package` exercises both `cdc_make_portable` and
  `cdc_package_platform`, producing `out/noc_soc` outside the build tree;
- the packaged executable has exactly `RPATH=$ORIGIN`, with no build directory
  or `/opt/systemc` fallback, and `ldd` resolves `libsystemc.so.2.3` beside the
  executable;
- with `LD_LIBRARY_PATH` removed, the packaged survey reports all bytes match,
  and the complete packaged-binary firmware regression also reaches
  `DMA PASS`;
- all 18 installed headers carry an expected SPDX identifier; the installed
  package and binary bundle contain byte-identical Apache-2.0, SHL-0.51 and
  FlooNoC provenance records. The binary bundle additionally ships the static
  CPU's MIT licence, CDC-VP NOTICE and third-party inventory.

The runner compares the installed header manifest to the source manifest before
counting SPDX identifiers, so a newly added header cannot silently disappear
from the install set. It scopes the 18-header claim to
`include/floo_noc_model`; unrelated component headers in the parent prefix are
not incorrectly charged to this gate. It reuses
`run_firmware_regression.sh` through `NOC_SOC_BIN` rather than maintaining a
second firmware oracle.

The original `cdc_make_portable` implementation left
`$ORIGIN:/opt/systemc-2.3.4/lib64` in the build executable copied into the
bundle. `BUILD_WITH_INSTALL_RPATH=TRUE` now makes the packaged build-tree
binary use the intended install RPATH, while
`INSTALL_RPATH_USE_LINK_PATH=FALSE` prevents absolute host link paths from
being appended.

### Step 10.5 — Outstanding concurrency and downstream-ID policy — DONE (2026-08-01)

The two decisions are closed separately.

1. **Concurrent calls at one TLM upstream port: implemented.**
   `noc_interconnect` now admits a bounded number of `b_transport` calls per
   port, queues their manager requests, and keeps one caller-owned waiter until
   the matching completion. B and R have independent FIFO waiter queues because
   the frozen metadata has independent write/read FIFOs. The constructor bound
   defaults to 32, may be lowered, and is rejected outside 1..32.
2. **Multiple downstream AXI IDs: not required and not enabled.**
   `MaxUniqueIds = 1`, `NoRoB`, and the all-ones downstream reissue ID remain
   frozen. No current CDC-VP/noc_soc requirement needs out-of-order downstream
   matching. Enabling `MaxUniqueIds > 1` would be a separate future feature
   requiring a new configuration and an RTL cross-check of
   `floo_meta_buffer.sv`'s `id_queue`, ID allocation/matching and
   back-pressure.

The former `port_busy` deletion is therefore not the implementation. It is
replaced by:

- a per-node FIFO of manager requests, each pointing to its suspended caller;
- a per-port admission count and event, with current/peak diagnostics;
- independent FIFO B/R completion-owner queues;
- independent per-port B/R target-delay FIFOs, so one completion cannot consume
  another outstanding call's peripheral delay;
- completion-time slot release and whole-wrapper quiescence checks that include
  every queue and sideband.

`test_noc_interconnect_concurrency` uses seven SystemC traffic threads on one socket and
a configured bound of two. It proves the bound saturates but is never exceeded,
reads and writes complete in FIFO issue order, simultaneous B/R traffic retains
separate owners, data and error status return to the correct callers, each call
excludes only its own target delay, all slots release, every access completes
once, and wrapper plus mesh drain to quiescence. Invalid bounds 0 and 33 are covered by
`test_noc_interconnect_bad_config`.

Verification evidence at the Step 10.5 close (Step 11 later raises the totals):

- focused `test_noc_interconnect`, stress, concurrency and bad-config tests:
  4/4 pass;
- automated negative controls then: **35 detected, 0 missed**, including the four
  new same-port mutations for response owner, capacity off-by-one, reversed
  request order and dropped target delay;
- fresh full component regression then: **36/36 pass**;
- a fresh **Release** parent build passes
  `noc_soc_firmware_regression` 1/1 (`DMA PASS` plus survey). The supported
  platform command has always selected Release.

An assert-enabled parent build exposes a pre-existing timing-contract mismatch,
not a Step 10.5 queue failure: `noc_interconnect` spends an incoming annotated
delay and returns zero, while Bremen's `CombinedMemoryInterface` asserts that a
target never decreases its quantum-keeper local delay (`mem.h:65`). Release
disables that upstream `assert`, which is why the signed firmware regression
passes. Do not hide this as concurrency evidence. Step 11 later resolved the
long-run need with a separate fast backend that preserves/increases annotation;
the detailed backend deliberately retains its spending contract.

`MaxUniqueIds` and `BRoBType`/`RRoBType` remain independent configuration
fields. Raising `MaxUniqueIds` would not itself replace `NoRoB`; change and
re-sign the RoB only if a future frozen configuration changes that field too.
Virtual channels, ATOPs, collectives, multicast, reduction, and the narrow-wide
network remain deferred until an SoC requirement explicitly needs them.

### Step A — Compose the signed chimney into the datapath — DONE (2026-07-31)

Chosen over the alternative of leaving the abstract transactors in place and
only testing them. The goal: the platform's network interface should *be* the
RTL-signed chimney, not code that follows its rules.

**The finding that set the scope.** The chimney has four quadrants and the model
had three:

| Quadrant | Module | Status |
|---|---|---|
| Manager: AXI request → `req` link | `axi_chimney_request` | signed, 141 cycles |
| Subordinate: `req` → AXI out → B/R → `rsp` | `axi_chimney_response` | signed, 221 cycles |
| Manager: `rsp` link → B/R to the manager | `axi_chimney_manager_response` | signed, 78 cycles (A-1) |

`axi_chimney.hpp` had said so all along — "the unpacker are not modelled here" —
and left `i_b_pop`/`i_r_pop` as inputs for the harness to drive. So Step A is not
"wire up modules that exist"; the fourth had to be written first.

#### A-0 — the unpacker — DONE (2026-07-30)

`axi_chimney_manager_response`, purely combinational, because the `NoRoB` branch
of `floo_rob_wrapper.sv` really is a pass-through on the response side
(`rsp_ready_o = rsp_ready_i`) with all state in the counter bank that belongs to
`axi_chimney_request`. Covered by `test_axi_chimney_manager_response`, whose
useful half closes the loop: an AW takes a counter and the answering B releases
it, with nothing in the test driving `i_b_pop` by hand.

Exposing the reorder buffers' response handshake meant adding ports to the
already-signed `axi_chimney_request`. The 141-cycle cross-check was re-run
against the RTL afterwards and still matches, which is the proof the addition
changed nothing.

**A real defect this uncovered, present since Step 8.** The model tied
`r_rob_.i_rsp_last` high, so *every* beat of a read burst released a counter. The
RTL differentiates:

```systemverilog
i_b_rob: .rsp_last_i ( 1'b1 )                            // B is single-beat
i_r_rob: .rsp_last_i ( floo_rsp_in.axi_r.payload.last )
```

RLAST is now routed through `i_r_pop_last`. Note **the request-timing
cross-check could not have caught this**: it holds the response link idle, so the
counters only ever fill. A signed block is signed for the stimulus that reached
it.

#### A-1 — cross-check the unpacker against RTL — DONE (2026-07-31)

`rtl_crosscheck/run_chimney_mgr_rsp_crosscheck.sh`, the twelfth cross-check.
**78 cycles exact.**

It drives `floo_rsp_i` on the unmodified frozen chimney and compares, per cycle
and both pre-edge and post-edge: `axi_in_rsp_o`'s B and R channels qualified by
their valid, `floo_rsp_o.ready`, the manager-side `aw_ready`/`ar_ready`, and the
`NoRoB` counter `in_flight` for two read ids and one write id. The model side is
`axi_chimney_request` composed with `axi_chimney_manager_response`, so what is
signed is the wiring between them — the counter bank alone was already signed by
`run_rob_crosscheck.sh`.

The stimulus includes the multi-beat R burst this step existed for, plus
back-pressure held across a valid response and each channel's ready driven low
while the *other* channel's stays high, which pins the per-channel ready select.

Three things worth carrying forward:

* **The RLAST defect is now caught by RTL.** Reinjecting it
  (`rlast-ignored-vs-rtl`) makes the model's R counter fall on every beat while
  the RTL's holds, diverging at the first beat of the burst. Three signed
  cross-checks could not see this, because all three pin
  `floo_rsp_in.valid = 1'b0`.
* **Payload fields are traced qualified by their valid**, because
  `floo_rsp_chan_t` is a union: B and R payloads alias, so an R flit leaves the
  RTL's B output holding reinterpreted bits. Comparing those unqualified would
  compare a SystemVerilog struct layout against a C++ one. Flit content is
  already signed by `run_chimney_rsp_crosscheck.sh`.
* **The runner guards `floo_rob_wrapper.sv` as well as `floo_axi_chimney.sv`.**
  The counter release under test lives in the `NoRoB` branch of the wrapper, so
  guarding only the chimney file would let the behaviour change unnoticed.

The runner also checks the transaction budget directly: the observed peak
occupancies must be exactly `(2, 1, 1)` and the final values `(0, 0, 0)`. Both
sides replay the same vector, so a stimulus issuing more transactions than it
documents still compares exactly while proving a different scenario — an earlier
version held `AxVALID` for six cycles, which is six transactions rather than
one.

An earlier form of this check tested `counter >= MaxTxnsPerId` and called it a
wrap detector. It was not one: `CounterWidth = $clog2(MaxTxnsPerId) = 5`, so
`in_flight` is five bits and cannot reach 32; an underflow from zero lands on
31. The test was dead code. Checking the budget catches an underflow, an extra
push and a missed pop alike.

#### A-2 — assemble a per-node chimney and wire it into `axi_noc` — DONE (2026-07-31)

`axi_noc.hpp` now has three explicit levels:

1. `axi_mesh_noc` preserves the old raw-flit API and exact signed mesh
   behaviour. `mesh_trace_sc` and the legacy endpoint tests use this type
   directly.
2. `axi_chimney_node` composes `axi_chimney_request` plus
   `axi_chimney_manager_response` on the manager side and
   `axi_chimney_response` on the subordinate side. It owns every internal
   response-pop/RLAST/RoB-ready signal.
3. `axi_noc` instantiates one such node at every coordinate and wires all
   request/response inject/eject links to `axi_mesh_noc`.

`test_axi_noc_chimney` drives a 2x2 network through AXI signals. It checks a
two-beat AW/W write and three-beat AR/R read, manager-side B/R back-pressure,
stable stalled payloads, `OutIdWidth=3` reissue ID 7 at the subordinate,
restoration of manager IDs 5 and 3, exact payload fields, RoB and metadata
release only on accepted B/RLAST, and no traffic at unused nodes. It passes in
56 cycles. The full standalone regression is 34/34. The automated
`chimney-node-rob-ready-swapped` negative control crosses the node's private
B/R RoB-ready bindings; the mutated source still builds and
`test_axi_noc_chimney` fails on the stalled R payload. This proves that the
composition test observes these internal links rather than only the two mesh
boundaries.

After the change, five directly affected RTL checks were re-run unchanged:
request content 16 flits, request timing 141 cycles, subordinate/response
timing 221 cycles, manager response 78 cycles, and mesh 1872 node-cycles all
match. This preserves the individual block signs; A-2 itself is a SystemC
composition test, **not** an RTL end-to-end cross-check.

#### A-3 — switch `noc_interconnect` to the signal-driven chimney — DONE (2026-07-31)

`noc_interconnect` now instantiates `axi_noc` rather than `axi_mesh_noc` plus
method-driven endpoints. Its manager adapter holds AW, each W beat, or AR stable
until the timed chimney handshakes it. Its subordinate adapter accepts AW/W/AR,
reconstructs the downstream TLM access, then holds B or each R beat until
accepted. The chimney performs downstream-ID rewrite to 7, metadata retention,
response routing, manager-ID restoration and `NoRoB` counter release.

Because the subordinate-side ID is intentionally rewritten, target-delay
attribution passively observes accepted request-link AW/AR headers and pairs
their source coordinate with the subordinate AXI handshake. That observation
drives no ready/valid signal and does not bypass the datapath.

The frozen 3-bit manager ID supports ports 0..7, so the constructor now refuses
zero or more than eight upstream ports before building a mesh.

Verification:

- full standalone regression: **34/34 pass**;
- `test_noc_interconnect` uses a 4x4 topology, retains the two-manager
  scoreboard and pins the no-contention integrated baseline at **10 cycles for
  one hop and 30 for six**, excluding target delay;
- request content 16 flits, request timing 141 cycles, response content 8
  flits, subordinate/response timing 221 cycles, manager response 78 cycles and
  mesh timing 1872 node-cycles all still match RTL;
- after rebuilding the parent `noc_soc` target against A-3,
  `run_firmware_regression.sh` passes both real firmware (`DMA PASS`) and the
  synthetic survey; the firmware retires 10,726 instructions in 570,683 ns of
  modeled time (~53.2 ns/instruction);
- the automated `a3-manager-aw-address-shifted` negative control changes only
  the AW payload driven by the per-cycle adapter. The transaction still routes
  and completes, but `test_noc_interconnect` catches the bytes at the wrong
  target addresses.

The model-level negative-control gate was re-run after A-3 with **25 detected,
0 missed**. The existing `sparse-beat-renumbering` mutation was updated for
both A-3 changes at its injection point: the new indentation and the current
`capture.strb` storage. A build failure is not counted as detection.

This completes the timed datapath integration, not a new monolithic RTL
cross-check. The TLM adapters remain model-only code; Step 10.3 subsequently
covered them with bounded three-manager stress and mutation controls.

### Step 11 — Build the fast approximately-timed mode — DONE (2026-08-01)

The H1/H2 coexistence decision is closed with **one interconnect class and two
construction-selected timing backends**:

- `timing_mode::detailed` is unchanged as the calibration reference. It drives
  AW/W/AR/B/R through the timed chimneys and mesh, spends incoming and network
  time, and returns zero annotation.
- `timing_mode::fast` shares validation, address decode, AXI lane/byte-enable
  shaping, target replay, target-error mapping and placement. It terminates the
  private mesh clock process, never injects a flit, preserves incoming local
  time and returns its estimate in `delay`.

The fast estimate is pinned in code and test:

```text
read  cycles = 4 * Manhattan_hops + 6 + (beats - 1)
write cycles = 4 * Manhattan_hops + 6 + beats
```

The request/response split presents estimated request arrival time to the
target. The target's incremental annotation is then rounded up to the next
network cycle, matching detailed mode, before the response estimate is added.
A target that decreases the annotation is rejected. The interconnect itself
does not wait; downstream targets selected for fast mode must likewise annotate
latency rather than call `wait()` inside `b_transport`.

`test_noc_interconnect_fast` instantiates both backends and applies the same
stimulus over one through six hops, 1/2/4/8-byte accesses and 32-byte bursts.
The declared calibration tolerance is **one cycle**. It separately verifies
zero fast-mode internal time advance, incoming-delay preservation, rounded
1.5-cycle target delay, read/write data and target effects, subordinate errors,
sparse unaligned byte enables, target-exception slot cleanup, quiescence and
mesh bypass. This is model-to-model calibration; it is not a thirteenth RTL
cross-check.

The approximation boundary is explicit: fast mode does not estimate
contention, link back-pressure, router/packet locks, FIFO or metadata occupancy,
or clock-gating activity. It preserves blocking-call invocation order and
functional effects. Use detailed mode for congestion and cycle-accurate
questions.

Verification evidence:

- full component regression: **37/37 pass** with GCC/G++ 11.5.0 and SystemC
  2.3.4;
- mutation gate: **39 detected, 0 missed**. Four Step 11 mutations independently
  remove hop cost, incoming delay, target-delay ceiling and functional target
  replay;
- Release `noc_soc_firmware_regression`: 1/1 pass. The script runs firmware and
  survey in both timing modes; firmware reaches `DMA PASS` in each;
- assert-enabled fast firmware: `DMA PASS`, so Bremen's nondecreasing quantum-
  keeper assertion is satisfied without relying on `NDEBUG`;
- detailed RTL-facing blocks remain unchanged; the twelve existing RTL
  cross-checks remain the hardware evidence.

Host performance and modeled accuracy were measured separately on the same
AlmaLinux host, Release binary, ELF and 2 ms firmware window:

```text
timing     host wall   retired   modeled active time   ns/instruction
detailed   8.12 s      10726     570 us                53.1419
fast       0.08 s      10726     570 us                53.1419
```

This single sample is about **101x** faster. It is not a performance guarantee;
host load, compiler/build type and window must accompany any future number.
Modeled time is the calibration result, while host wall time is the speed
result.

Detailed mode still intentionally violates Bremen's LT expectation by spending
incoming local time. That is no longer hidden: `noc_soc --noc-timing fast` is
the temporal-decoupling/long-run path, and `--noc-timing detailed` is the
cycle-stepped reference.

---

## 15. Definition of done for the v0 vertical slice

The v0 slice is not complete until all of the following are true.

| Criterion | State |
|---|---|
| every included leaf has a standalone SystemC test | **met** (2026-08-03) — `test_rr_arb_tree` and `test_meta_buffer` closed the last two; Gate V0-CLOSE |
| FIFO, route selector, arbiter, and router have RTL trace comparisons | **met**, plus seven more; section 10.11 |
| the router passes routing, contention, back-pressure, and wormhole tests | **met** |
| a small req/rsp mesh delivers every flit exactly once | **met** — `test_axi_noc`, and mesh timing signed at 1872 node-cycles |
| AXI AW/W/AR/B/R transactions survive the integrated signal-driven path | **met** — A-3 `test_noc_interconnect`, A-2 `test_axi_noc_chimney`, and real firmware reaches `DMA PASS` |
| ordering rules for the selected configuration are verified | **met** — `NoRoB`, 127 cycles |
| measured timing is separated from analytic estimates | **met** — schema v1 requires explicit `[M]/[D]/[A]/[S]` source tags and leaves area/power/energy unavailable |
| CDC-VP integration reflects a fabric, not a fake accelerator peripheral | **met** — `noc_interconnect` is an M:N fabric adapter |
| the network clock stops only at proven quiescence | **met** — production gating requires wrapper idle and exported mesh/chimney quiescence; section 13.6b |
| licensing and provenance are complete | **met** — `noc_soc_packaging_regression` byte-compares the clean-prefix and binary-package records: Apache-2.0, SHL-0.51, NOTICE, pinned FlooNoC provenance, CPU MIT and third-party inventory |
| **the datapath uses the cycle-accurate blocks from AXI manager port to AXI subordinate port** | **met at model integration level** — A-3 drives the timed `axi_noc` cycle by cycle; every selected block is individually RTL-signed, but there is no monolithic full-path RTL harness |

Two further criteria that were implicit and should be explicit:

| Added criterion | State |
|---|---|
| the unsigned integration layer has scoreboard-driven multi-initiator stress | **met** — Step 10.3 adds bounded deterministic-random three-manager stress, per-transaction deadlines, a global watchdog and seven mutation controls |
| a platform-level regression runs real firmware unattended | **met** — `noc_soc_firmware_regression`, Step 10.2 |

**Every criterion is now met.** Counted here explicitly so the prose and the
tables remain mechanically consistent.

The last one closed on 2026-08-03, under Gate V0-CLOSE of
`docs/NOC_SOC_FREERTOS_ROADMAP.md`: `rr_arb_tree.hpp` and `meta_buffer.hpp` were
the two included leaves without a direct standalone test. Both now have one —
`test_rr_arb_tree` and `test_meta_buffer` — registered through
`add_floo_model_test()` so they inherit the component warning policy.

This was a modular-testability gap, not an untested datapath: `rr_arb_tree` was
already covered by the 152-cycle wormhole-arbiter cross-check at 5, 4 and 2
routes, and `meta_buffer` by the 221-cycle chimney subordinate-side cross-check.
What the direct tests add is isolation — rule 4 of section 16 asks for a leaf
test to run before a cross-check so a failure can be located rather than
bisected.

Two registered mutations stop the new tests closing the criterion vacuously:
`rr-arb-tree-wrap-ignored` breaks the `FairArb` lower-mask wrap, and
`meta-buffer-overflow-accepted` removes the `MaxTxns` push guard. Both build and
are detected, taking the registry from 39 to **41 detected, 0 missed**.

Step 10.5 and Step 11 were beyond this original v0 gate; both are also
complete.

The explicitly authorised post-v0 roadmap is
`docs/NOC_SOC_FREERTOS_ROADMAP.md`. **Step 12.0 is complete** (2026-08-03):
`fw/freertos_noc_soc` reuses the proven FreeRTOS 11.2.0 Bremen RV32 port/BSP
with `NPU=0`, links only `[0x80000000, 0x80fff000)`, fixes `_stack_top` at
`0x80fff000`, and validates every ELF `PT_LOAD` by `p_memsz`. The built image
has entry `0x80000000` and one load range
`[0x80000000, 0x80015730)`.

A 700 ms `noc_soc --mode firmware --noc-timing fast` compatibility probe
reached the reused application's scheduler, TIMER0/PLIC and CLINT markers and
reported zero synthetic transactions, RAM writes and DMA-register writes.
Those legacy `FreeRTOS FX1` markers proved the frozen Step 12.0 contract.

**Step 12.1 is also complete** (2026-08-03). The profile now overrides the FX1
demo with `fw/freertos_noc_soc/src/main.c`: two tasks at priorities 1 and 2
exchange direct notifications for eight rounds while checking their current
handles, priorities, counters and strict turn state. The self-checking runner
observed all 16 `low/high` handoffs in order and `FreeRTOS NoC PASS` during a
20 ms fast-mode run. A separate 6 ms detailed smoke produced the same sequence.
Both reported zero synthetic traffic and no failure or unexpected-trap
diagnostic. The current ELF has entry `0x80000000`, one `PT_LOAD` at
`[0x80000000, 0x80013298)`, and stack/end `0x80fff000`.

**Step 12.2 is complete** (2026-08-03). `NOC_STEP=121` still builds the
isolated scheduler proof, while level 122 adds:

- three checked `vTaskDelay(1 ms)` block/wake rounds followed by
  `CLINT tick OK`;
- exactly three TIMER0 notifications through PLIC source 4, with the
  level-sensitive device cause cleared before claim completion, followed by
  `PLIC TIMER0 OK irqs=3`.

`run_step_12_2.sh` accepts final PASS only after the scheduler, CLINT and PLIC
markers. Its 50 ms fast regression and the supporting 9 ms detailed smoke both
pass with zero synthetic traffic. The current level-122 ELF has entry
`0x80000000`, one `PT_LOAD` at `[0x80000000, 0x8001392c)`, and stack/end
`0x80fff000`. The fixed fast window observed 171598 completed firmware
transactions and 1796144 estimated no-contention cycles; the detailed window
observed 160036 transactions and 1676793 measured cycles. These are labelled
observations, not pass thresholds.

**Step 12.3 is complete** (2026-08-03). Default `NOC_STEP=123` sequences a
FreeRTOS DMA task after the Step 12.2 flags. The PL330-style program and
32-byte buffers are aligned firmware objects bounded by
`__firmware_ram_end = 0x80fff000`. Event 3 reaches PLIC source 7; the ISR checks
`INTMIS`, clears `INTCLR` before claim completion and supplies exactly one
direct task notification. The task has separate prerequisite/completion
timeouts and verifies interrupt cleanup, `STOPPED`, FSRC, final SAR/DAR and
both buffers against the independent expected pattern.

`run_step_12_3.sh` passed a 100 ms fast window and the final level-123 binary
passed a supporting 12 ms detailed smoke. Both observed `DMA NoC PASS`, zero
synthetic traffic and no source-8 abort. The current ELF has one `PT_LOAD` at
`[0x80000000, 0x80014074)`. Fast instrumentation recorded 200211 firmware
transactions and 2070765 estimated no-contention cycles; detailed recorded
175395 transactions and 1814686 measured cycles. These are observations, not
thresholds.

The public-tree DMA register scope passes
`CHECK_REGS_SKIP_NPU=1 ./tools/check_regs_drift.sh`; the explicit skip is needed
because the optional private NPU header is absent and does not skip DMA checks.

**Step 12.4 is complete** (2026-08-03). Level 124 keeps every earlier proof
and then overlaps them: two priority-1 CPU tasks
read-modify-write their own RAM buffers and check one NoC-crossing MMIO
register each per iteration, TIMER0 stays free-running through PLIC source 4,
and the DMA manager repeatedly transfers 4 KiB completing through source 7. A
supervisor task owns the bounded phase and is the only task that prints, so
UART is never the synchronisation mechanism.

Two things about that step are worth carrying forward, because both were found
by the checks failing rather than by reasoning:

- **A 32-byte DMA transfer cannot demonstrate concurrency here.** It completes
  in about 0.5 us of modeled time, shorter than one CPU-task iteration. The
  first implementation passed with the CPU tasks at *zero* progress during the
  transfers. The phase now uses its own DMALP-looped 4 KiB transfer and counts
  worker words strictly between a launch and its completion interrupt — a
  window in which the DMA task is blocked, so the count can only come from work
  done while a transfer was in flight.
- **TIMER0 at the Step 12.2 rate saturates the phase.** At 50 us an ISR entry
  plus PLIC claim/complete plus a context switch costs a comparable amount of
  modeled time, and the workload measures trap handling. The phase reprograms
  TIMER0 to 200 us; it stays a real interrupt load without being the only one.

`run_step_12_4.sh` passed a 500 ms fast window in about 9 s of host time: 4
checkpoints, 4 concurrent transfers, 16384 concurrent bytes, per-worker totals
of 39 and 40 iterations, 100 TIMER0 interrupts, in-flight overlap of 24 and 10
worker words, and 30 TIMER0 interrupts across the transfer loop. Counters are
enforced as floors, never fixed values, so the regression does not become a
timing threshold. Repeated runs are byte-identical. Levels 121, 122 and 123
still pass unchanged.

**Step 12.5 is complete** (2026-08-03). `platforms/noc_soc/tests/run_freertos_regression.sh`
is a deliberately thin wrapper over the existing acceptance contract rather than
a second oracle. After Step 12.8 it drives levels 121, 122, 123, 124 and 128,
so earlier-stage reproducibility is checked rather than asserted. It accepts an external
`NOC_SOC_BIN`, and keeps every log on failure. It is registered as
`noc_soc_freertos_regression` with labels `firmware;freertos`,
`SKIP_RETURN_CODE 77` and a 900 s timeout. The original four-level form passed
in about 13.6 s; the current five-level form also passes.
`run_packaging_regression.sh` runs the strongest level 128 image against the
packaged executable with `LD_LIBRARY_PATH` unset. Verified in a private
`-DCDC_BUILD_TESTS=ON` build so the developer's cache was untouched; the
platform target builds with zero warnings.

The detailed-mode comparison is deliberately **not** registered: it costs about
45x the host time for the same acceptance, so it stays a one-off measurement.

**Step 12.6 is complete** (2026-08-03).
`platforms/noc_soc/tests/run_freertos_negative_controls.sh` holds the
platform/firmware registry, registered as
`noc_soc_freertos_negative_controls` with labels
`negative-controls;extended;freertos`, `RUN_SERIAL`, skip 77 and a 1800 s
timeout. It is deliberately separate from the component's
`run_negative_controls.sh`, which intentionally copies and builds only the
component. The registry includes two Step 12.4-specific controls (corrupting
the final worker generation and erasing in-flight progress) plus the Step 12.8
UART RX/PLIC, host-dashboard marker, hardware-scan identity and register-RW
controls. Current result: **11 detected, 0 missed**.

Design points worth preserving:

- **Two expectations per control**, not one: what the runner must name and what
  the platform log must contain. Checking only the exit code would score a
  mutation that merely broke the build the same as one that broke behaviour.
- **Absence expectations** are written `!text`, and the clean run must have
  produced that text. A scheduler tick that never arrives leaves no firmware
  diagnostic at all, so without this the evidence would degrade to "the run
  failed somehow".
- **The acceptance runner now checks `FreeRTOS NoC PASS` last.** It is the
  least informative marker in the log — absent whenever anything upstream
  failed — so checking it first named the symptom for every possible defect.
- **Two scopes**: `firmware` rebuilds only the firmware; `platform` configures
  and builds `noc_soc` privately, excluding the 300 MB `tflite-micro` tree that
  neither this platform nor an `NPU=0 TFLM=0` firmware can reach.

Two findings came out of controls failing rather than out of reasoning:

- the first DMA mutation (`dma_destination + 4`) overran into the *source*
  buffer and so qualified the source comparison instead of the destination one.
  A wrong destination has to land where nothing else is verified;
- the CLINT control **cannot** be written on the firmware side at all. Every
  firmware-owned way of breaking it is already rejected at compile time by the
  `_Static_assert` pair in `main.c`, so the misconfiguration class of that
  defect cannot reach a run. Only removing the `mtimecmp` write itself, in the
  private copy, exercises the runtime path.

**Step 12.7 is complete** (2026-08-03); at that point it closed the original
FreeRTOS roadmap. Step 12.8 was subsequently authorised to close the deferred
UART console.

The attribution warning in the roadmap was right and forced a design change.
PLIC claim and PLIC complete are the **same target at the same address**,
`0x0c20_0004`, separated only by direction, so no per-target counter can split
them and polling `last_latency_cycles()` afterwards may read another manager's
transaction. `noc_interconnect` therefore gained a passive completion observer
(`set_completion_observer`), firing where each latency becomes final and
carrying port/address/length/direction. Its contract: cheap, no throw, no
`wait()`, and nothing but a `const` accessor on the interconnect — spending time
there would change the numbers being measured.
`test_noc_interconnect_observer` and the control
`completion-observer-attribution-lost` are its gate; the registry is now
**42 detected, 0 missed** and the suite 40/40.

Firmware **level 125** is level 124 plus an RTOS tick hook, kept separate
because a hook running every tick and reading CLINT `mtime` over the NoC is
instrumentation, and folding it into 124 would move the counters that image is
signed on. `configUSE_TICK_HOOK` became overridable so every existing image
keeps its 0 default.

`noc_soc_measurement_baseline` archives
`platforms/noc_soc/evidence/noc_baseline.txt` with its own provenance. It is a
**completeness check, not a threshold test**. The DMA interval now starts at
the `dma_tlm` channel's architectural `STOPPED -> EXECUTING` transition and
ends when the completion IRQ rises. The older DBGCMD-completion timestamp was
late because the channel had already been started and scheduled before the NoC
response returned. A direct DMA unit test pins the callback-before-return
ordering.

Continuation review on the same date rebuilt the parent and standalone
component from fresh `/tmp` trees with GCC/G++ 11.5, passed the 40/40 component
suite, the four-level FreeRTOS regression and all 42/42 component negative
controls, then reproduced the detailed baseline from the fresh Release binary.
That review also closed the remaining runner-only gaps: the completeness schema
uses anchored numeric records for all required provenance, manager, mesh and
tick fields; a dirty checkout is accompanied by a reproducible binary source
patch plus revision, firmware hash and platform-binary hash; and CTest writes
evidence under its binary tree rather than mutating the source tree. Manual
sign-off still promotes the authoritative artifact to the platform evidence
directory.

Two things to preserve. The same workload in **fast** mode leaves the
  DMA-active RAM bucket empty — a demonstration, not an assertion, that fast mode
  cannot answer a contention question. The older statement that router
  utilisation/FIFO occupancy were deliberately absent is now superseded by the
  D1 metrics work: passive counters are bound to every production router in both
  request and response meshes, including explicit input/output FIFO occupancy.
  The archival baseline runner now requires those records and stores metrics
  JSON plus the rendered dashboard beside the text artifact.

**Step 12.8 is complete** (2026-08-05). It closes the UART RX/interactive CLI
item that was previously deferred. `noc_soc` now binds
`components/uart_host_tlm` to UART0 and exposes two firmware-only host paths:
deterministic `--uart0-rx-file` replay and a bidirectional loopback
`--uart0-socket` console. Both feed the pin-side RX FIFO; PLIC source 1 and the
shared FreeRTOS UART ISR queue deliver bytes to firmware. There is no direct
parser or firmware-queue injection.

Firmware level 128 is the new default. It retains the complete level-124
workload, while level 125 remains the separate tick-instrumented measurement
image. The CLI task arms UART RX early but waits for the supervisor's direct
notification, issued only after `FreeRTOS NoC PASS`. Its final command set is
`help`, `soc`, `noc_dashboard`, `hw_scan` and `reg_test`; `status`, `irq`,
`uptime` and `exit` were explicitly removed. `noc_dashboard` is host-assisted:
firmware emits a private UART record, detailed `noc_soc` atomically publishes a
live metrics JSON file, and `tools/noc_cli.py` invokes the same
`tools/noc_dashboard.py` renderer as the offline flow. It requires
`--noc-timing detailed`, `--noc-metrics FILE` and the UART TCP client; the live
firmware snapshot is diagnostic and remains `drained: false`.

Acceptance is owned by `fw/freertos_noc_soc/run_step_12_8.sh`: deterministic
file input, a real UART0 interrupt assertion, all level-124 markers, the
dashboard request/host acknowledgement, a 22-block hardware scan and 37/37
restoring register test in strict order after CLI ready. The fast regression
must report the dashboard as unavailable rather than fabricate detailed
counters. IFLASH and PMU are named `absent` without MMIO because `noc_soc` does
not instantiate them; ISP/VPU/NPU are named `reserved`. The five-level
FreeRTOS regression and 11/11 controls pass. The existing bridge unit passes
file replay, TCP RX and TCP TX; `noc_soc_cli_dashboard_protocol` covers marker
fragmentation, atomic publish and renderer ordering with a real loopback TCP
session.

For interactive use, `--uart0-wait` only waits for a client connection at
simulation time zero. The client must wait for `FreeRTOS NoC CLI ready` before
sending a burst; otherwise bytes can arrive before RX is armed and overflow
the UART model's 16-byte hardware FIFO. Fast mode also needs a deliberately
long `--sim-us` bound for a human session.

**FlooNoC metrics/dashboard D0-D6 v1 is complete** (2026-08-05). The definitive
implementation report is `docs/NOC_METRICS_DASHBOARD_IMPLEMENTATION.md`.
`noc_soc --noc-metrics` produces diagnostic firmware JSON; the drainable
`noc_benchmark` uses the production detailed `noc_interconnect` and owns
warm-up/reset/measure/stop/drain, offered/delivered reconciliation and boundary
flit conservation. `noc_dashboard.py` renders the versioned JSON, and
`noc_sweep.py` writes per-run evidence plus aggregate JSON/CSV and refuses any
failed, undrained or constraint-rejected winner.

The denominator correction is part of the contract: throughput and directed
link utilisation divide by all modeled cycles in the measurement window, while
router `counted_cycles` remain mesh-clock-active cycles for FIFO sampling and
clock-active ratio. Full verification is 41/41 component tests, 51/51 component
mutation controls, 10/10 D3-D6 metrics mutation controls and the five platform
metrics/dashboard CTests. D7 calibrated area/power remains deferred.

---

## 16. Rules for future changes

1. Read the frozen RTL and its assertions before changing behavior.
2. Keep each new leaf independently testable.
3. Add a common trace format before writing two separate testbenches.
4. Run SystemC tests before an RTL cross-check to isolate failures.
5. Run the exact compiler sanity commands before every build.
6. Put generated build artifacts under `/tmp` or an ignored build directory.
7. Never call an unverified timing number cycle-accurate.
8. Do not add deferred features into a lower-level equivalence patch.
9. Do not use an SV behavioral replacement as the RTL reference.
9b. **Every cycle cross-check samples both pre-edge and post-edge**, and every
    cross-check of any kind must be shown to fail on an injected defect before
    a PASS is recorded.

    The phase half applies to cycle comparisons, which is what all eight of
    them are: FIFO, arbiter, router, `NoRoB`, chimney request timing, chimney
    subordinate-side timing, mesh,
    and route selection. Pre-edge is the handshake the environment acts on
    within the cycle; post-edge is the state that resulted. Recording only one
    hides real behaviour — the route-select harness recorded post-edge alone
    until 2026-07-30, and adding the pre-edge column immediately distinguished
    the cycle where the route lock is *acquired* (`pre_locked 0`, `post_locked
    1`) from the cycles where it is merely held, and the cycle where it is
    released together with a route change.

    It does **not** apply to the three cross-checks that have no clock edge to
    sample around, and demanding it there would be cargo cult:

    | Cross-check | Why the phase rule does not apply |
    |---|---|
    | `run_axi_sizing_crosscheck.sh` | compares pure functions over a configuration list; no clock exists in the compile at all |
    | `run_chimney_req_crosscheck.sh` | compares an ordered list of emitted flits, not per-cycle state |
    | `run_chimney_rsp_crosscheck.sh` | same |

    Those three still owe the second half of this rule, and they have it: each
    is validated by injected defects recorded in `docs/STATUS.md`.

    Do not weaken this rule to accommodate a new harness. If a clocked harness
    cannot sample both phases, that is a fact about the harness, and it needs
    fixing or an entry in the table above with a technical reason.
9c. Pin every RTL dependency by revision and per-file hash in
    `rtl_crosscheck/fetch_rtl_deps.sh`.
9d. Export a block's internal registers into the trace when they are reachable
    by hierarchical reference; output-only comparison is weaker.
9e. When a negative control unexpectedly passes, decide whether the injection
    was an equivalent rewrite before adding stimulus. Record the reasoning
    either way.
9f. **Use the synchronous BFM idiom in any new cycle harness: drive at
    `clk = 0`, sample pre-edge, raise the clock, sample post-edge, with no phase
    offsets at all.** Do not mix explicit `#delay` phase arithmetic with
    sequential driving. An earlier revision of this rule prescribed the opposite
    — `ApplTime`/`TestTime` offsets — which is what the two chimney *content*
    harnesses still use, and four separate defects came out of it. Those two are
    left alone because they pass and are signed; the idiom is not to be copied.
    See the header comment of
    `rtl_crosscheck/axi_chimney/tb_floo_axi_chimney_timing_trace.sv`.
9g. Every wait in a testbench needs a bound and a global watchdog. A harness
    must fail with a message, never hang.
9h. A testbench must never gate the DUT's ability to make progress on its own
    sequencing. Keep the far side of every interface permanently ready and
    observe what the DUT did with a concurrent monitor. The chimney response
    harness deadlocked because the downstream subordinate only became ready
    inside a task that ran after the inbound flit had to be accepted.
9i. Compute how many outputs a stimulus step expects *before* running it. A
    counter derived from the live capture count races the capture itself: the
    response harness waited for a second flit because the first had already
    been recorded when the wait was entered.

Verilator limitations hit so far, all worked around in the harnesses:

- `%[^,]` scan sets in `$fscanf` are not implemented — use numeric CSV columns;
- a comment whose first word is "Verilator" is parsed as a directive;
- `ref` arguments to tasks must be simple variables, not struct fields;
- the class-based `axi_test` package is unsupported, so the upstream
  `hw/tb/*` testbenches cannot be reused. VCS is available for those.
10. Update this document, `STATUS.md`, and the verification matrix after every
    signed-off milestone.
10b. **When a step changes what is true, fix sections 2 to 13, not only
    section 14.** This was violated for Steps 7 through 9.2: the step entries
    were written, and the front of the document was left describing the
    pre-Step-7 state for two days. The result was a document that told a new
    reader the output FIFO was disabled, `req`/`rsp` were not separate networks,
    the router was uncompared, and mesh latency was an estimate — every one of
    which had already been fixed and signed. A stale handoff is worse than a
    short one: it is confidently wrong. The checklist after any milestone is
    sections 2.4, 5, 6.1, 6.3, 7, 8, 9, 10.1, 10.10, 10.11, 11, 13, and 15.
11. Preserve unrelated user changes in the CDC-VP and FlooNoC repositories.
12. Keep FlooNoC-derived license headers and package required licenses.
13. Do not let a golden CSV be refreshed from the model. Every
    `tests/data/*_expected.csv` comes from RTL via its runner. A model-captured
    golden turns the suite into a tautology that passes forever.

---

## 17. Recommended immediate continuation prompt

The next AI can be given this task:

> Read `docs/AI_HANDOFF_CONTEXT.md` and
> `docs/NOC_SOC_FREERTOS_ROADMAP.md`. The original numbered roadmap through
> **Step 11** and Gate V0-CLOSE are complete. Step 12 has now been explicitly
> authorised: Steps 12.0 through 12.8 are all complete. FlooNoC
> metrics/dashboard D0-D6 v1 is also complete; read
> `docs/NOC_METRICS_DASHBOARD_IMPLEMENTATION.md` before extending it. Continue
> only from the explicitly deferred list there or in the FreeRTOS roadmap.
> Anchor every NoC behavioural
> decision in the FlooNoC IP at
> `/home/duyptt_HW/Documents/work/Study_FlooNoC/FlooNoC` (upstream
> `https://github.com/pulp-platform/FlooNoC.git`), and in the CDC-VP platform for
> the socket side.
>
> Preserve the Step 10.5 decision and evidence. One upstream TLM port supports
> bounded concurrent calls through per-transaction request slots and separate
> FIFO B/R completion owners. The bound defaults to 32 and is restricted to
> 1..32. `MaxUniqueIds = 1`, `NoRoB`, and the constant downstream reissue ID
> remain frozen; no current requirement justifies enabling the unverified
> `id_queue` branch. `test_noc_interconnect_concurrency` and its four mutation
> controls are the regression gate for that policy.
>
> Preserve Step 11's one-class/two-backend contract. Detailed mode spends time
> and remains the cycle-stepped calibration reference. Fast mode preserves
> incoming annotation and uses the no-contention formulas `4*hops+6+(beats-1)`
> for reads and `4*hops+6+beats` for writes. It shares functional target replay
> but does not model contention, back-pressure, locks, occupancy or clock
> gating. The interconnect and every fast-mode downstream target must annotate
> rather than call `wait()`. `test_noc_interconnect_fast` owns the hard one-cycle calibration
> tolerance; widening it requires explicit technical justification.
>
> Twelve isolated RTL cross-checks cover the selected timing blocks, while the
> TLM adapters have no direct RTL counterpart. Do not call a fast-mode
> calibration or a model-to-model comparison RTL equivalence. Preserve the
> full 41-test component regression, 51 component mutation controls, 10 D3-D6
> metrics mutation controls, dual-timing firmware regression and the registered
> `noc_soc_packaging_regression`; the latter owns
> the clean-prefix consumer, portable package/RPATH and licence/provenance
> gates.
>
> Do not open the NPU component for architecture questions. Its CMake shape,
> test registration, and platform/SDK packaging patterns are reusable; its
> register map, socket structure, and worker model are not.
>
> Use the mandated GCC/G++/PATH environment before every build, keep generated
> output under `/tmp` or ignored directories, preserve unrelated dirty files,
> and update sections 2 to 13 as well as section 14 and `STATUS.md` after the
> milestone.

If a dependency cannot be resolved without network access or a tool install,
record the precise missing artifact and proceed with other safe, local,
read-only preparation such as the trace schema and harness structure. Do not
mislabel a model-to-model comparison as RTL equivalence.
