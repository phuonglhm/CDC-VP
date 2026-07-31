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

Status snapshot date: **2026-07-30**.

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
- **Eleven cross-checks are signed** against frozen revision `9a6972a`: the XY
  route selector, the input FIFO wrap, the wormhole arbiter, the five-port
  router (at `OutFifoDepth` 2 and 0), the AXI sizing arithmetic, chimney request
  content, chimney response content, the `NoRoB` ordering rule, chimney request
  timing, chimney response/subordinate timing, and inter-node mesh timing.
  Section 10.11 tabulates them; `docs/STATUS.md` holds the evidence and the
  negative controls.
- Every RTL timing block selected for the v0 vertical slice has an isolated
  cycle cross-check, including the chimney's request path, its subordinate
  side, and the inter-node mesh. The manager-side response unpacker is the one
  block that is implemented but **not** yet cross-checked; that is Step A-1.
  Statements that the mesh or link timing itself is still only an estimate
  predate Step 9.2 and are wrong.
- This does **not** make the current TLM-manager-port to TLM-subordinate-port
  integration one composed RTL-equivalent timing path. `axi_noc.hpp`
  instantiates the two meshes, while `noc_interconnect` uses the abstract
  endpoint transactors in `axi_endpoint.hpp`; it does not instantiate the timed
  chimney classes from `axi_chimney.hpp`.
- Not signed, and **not signable against a direct RTL counterpart**: the endpoint
  transactors (`axi_endpoint.hpp`) and the TLM wrapper (`noc_interconnect`).
  They are a driver and collector built on individually signed rules. Their
  functional behavior and composed TLM-boundary timing need model-level
  integration tests; this is now the highest-risk correctness layer, see Step
  10.3.

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
parallel. This component is the Direction-2 workstream, and no FlooNoC-specific
H1 implementation exists yet. **That decision is still open and is now the
largest unscheduled item**: the detailed cycle-stepped model is explicitly
described as the calibration reference for a future approximately-timed mode
(`platforms/noc_soc/README.md`, "Why it is slow"), but nothing calibrates
against it yet. Step 11 records that work.

| Phase | FlooNoC interpretation | Current state |
|---|---|---|
| P0 | Freeze RTL/configuration, supported traffic, timing target, metrics, and integration role | Completed for vertical slice v0; the frozen parameter set in section 6.1 was corrected at Step 9.2 (`OutFifoDepth = 2`) |
| P1 | Timing-independent address decode, routes, transaction/flit reference behavior, and vectors | Implemented: address map, XY path, and AXI reference behaviour through the endpoint transactors |
| P2 | Signal-safe coordinate/header/flit/AXI types and address map | Implemented: full `FLOO_TYPEDEF_HDR_T` field set, AXI channel types, and RTL-signed sizing arithmetic. Coordinate widths remain a model choice |
| P3 | FIFO, route selection, arbitration, router, links, chimney/meta/RoB blocks | Signed: FIFO wrap, XY selector, arbiter, five-port router with output FIFO, chimney **request** timing (141 cyc), chimney **subordinate** side (221 cyc), meta buffer, `NoRoB` gate. **Not signed:** the manager-side response unpacker — implemented and unit-tested, cross-check is Step A-1 |
| P4 | Router/link topology and endpoint wiring | Implemented: separate `req` and `rsp` meshes (`axi_noc.hpp`) with AXI endpoint transactors, mesh timing signed |
| P5 | NoC-specific measured counters | Implemented for the router boundary over RTL-signed signals; latency/utilization counters still deliberately deferred, see section 13.6 |
| P6 | Optional configuration translation | NPU-style operation driver is not applicable; `noc_soc` takes mesh geometry and placement as construction parameters |
| P7 | Unit, router, mesh, stress, and RTL equivalence tests | 32 SystemC tests pass; eleven RTL cross-checks signed. **Randomized multi-initiator stress is still missing** — see Step 10.3 |
| P8 | Standalone build | Implemented with CMake and Make |
| P9 | SW/platform-visible contract | No register map by design. The platform-visible contract is the address map plus the placement rule that no target may share a node with a manager |
| P10 | TLM integration | Implemented: `noc_interconnect` is a fabric adapter with M:N tagged sockets and explicit placement, not a target+worker wrapper. Sign-off pending, Steps 10.1 to 10.3 |
| P11 | CDC-VP CMake component/install/export | Header-only interface target plus the compiled `cdc::components::noc_interconnect`. Clean-prefix install unproven, Step 10.4 |
| P12 | Platform assembly and packaging | `platforms/noc_soc` assembled and running; packaging unproven, Step 10.4 |
| P13 | RISC-V/SoC traffic validation | Real firmware (`fw/dma_riscv`) reaches `DMA PASS` over the mesh at ~52 ns of modeled target time per instruction. This is a target-timing result, not a host-performance measurement. Automated as `noc_soc_firmware_regression` — Step 10.2 DONE |
| P14 | Coexistence and maintenance | Open. Clock gating is implemented but its quiescence condition is unproven (section 13.6b); the H1 coexistence mode is unstarted (Step 11) |

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

**What is actually implemented today does not meet that bar.**
`noc_interconnect::impl::network_idle()` in `src/noc_interconnect.cpp` skips the
clock when its own bookkeeping is empty — `in_flight`, each node's `outbox` and
`serving`, and each manager's `has_request()`. It observes none of the six
conditions above directly, because it cannot: `floo_mesh` keeps its `routers_`
vector private and exports no occupancy or lock state upward, and `axi_noc`
exposes only the per-node `network_port`. The gate is therefore an *inference
from the wrapper*, which is exactly the shortcut the paragraph above warns
against.

It is probably sound — `in_flight` counts every transaction between injection
and completion, and both the route lock and the arbiter lock release on an
accepted `last` flit — but "probably sound" is not a proof, and a stuck lock is
precisely the failure this check should catch and currently cannot. Closing it
is part of Step 10.3; see section 13.6b.

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

### 6.3 What the datapath now is — closed at Step 9

`floo_mesh<FlitT, ...>` is still the generic single-stream mesh, and it remains
useful on its own for router-level work. **The AXI datapath is `axi_noc.hpp`**,
which instantiates two of them — one carrying `axi_req_flit`, one carrying
`axi_rsp_flit` — over the same coordinates, matching the FlooGen-generated
`floo_axi_mesh_noc.sv` at the frozen revision. Channel-specific AXI payload
types exist in `axi_types.hpp` with RTL-signed sizing arithmetic.

So the vertical slice may be described as a single-AXI FlooNoC network: two
physical channels carrying real AXI flit types, with the mesh between them
cycle-signed.

**It may not be described as a signed manager-port-to-subordinate-port path**,
and the reason is structural rather than a documentation nicety. Two different
things model the network interface:

| Header | What it is | Where it is instantiated |
|---|---|---|
| `axi_chimney_pack.hpp` | the combinational flit-assembly rules | `axi_noc.hpp`, i.e. the integrated path |
| `axi_chimney.hpp` | the **timed** chimney: request arbiter, response arbiter, metadata FIFOs | only `tests/chimney_timing_trace_sc.cpp` and `tests/chimney_rsp_timing_trace_sc.cpp` |

So the 141-cycle and 221-cycle chimney timing results are real, and they are
proved on a module the integrated platform does not contain. Between the AXI
endpoint transactors and the mesh sits `axi_chimney_pack.hpp` plus
`axi_endpoint.hpp`, not the signed timed chimney. Composing them is unfinished
work, not an accomplished fact — see sections 2.4 and 13.1.

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
    axi_types.hpp                <- AXI channel types and flit sizing
    axi_chimney_pack.hpp         <- flit assembly rules
    meta_buffer.hpp
    rob_order_gate.hpp           <- NoRoB ordering gate
    axi_chimney.hpp              <- timed chimney, request and response
    axi_endpoint.hpp             <- AXI manager/subordinate transactors
    axi_noc.hpp                  <- the AXI datapath: req + rsp meshes
    axi_lanes.hpp                <- TLM byte range -> AxSIZE/AxLEN/WSTRB
    noc_interconnect.h           <- TLM wrapper for CDC-VP
  src/
    noc_interconnect.cpp         <- the only compiled translation unit
  tests/
    CMakeLists.txt               <- registers 32 tests
    test_reference_model.cpp
    test_stream_fifo.cpp
    test_xy_route_select.cpp
    test_wormhole_arbiter.cpp
    test_floo_router.cpp
    test_floo_mesh.cpp
    test_noc_counters.cpp
    test_axi_types.cpp
    test_axi_chimney_pack.cpp
    test_rob_order_gate.cpp
    test_axi_endpoint.cpp
    test_axi_noc.cpp
    test_noc_interconnect.cpp    <- the TLM wrapper contract
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
| `axi_chimney.hpp` | `hw/floo_axi_chimney.sv` | Timed AXI/flit conversion: AW/W coupling and request arbiter (signed, 141 cyc); subordinate side and response arbiter (signed, 221 cyc); manager-side response unpacker (**not signed**, Step A-1) |
| `axi_endpoint.hpp` | **no RTL counterpart** | AXI manager and subordinate transactors: burst assembly, strobes, response routing, ID restoration |
| `axi_noc.hpp` | FlooGen generated `floo_axi_mesh_noc.sv` | Two meshes, `req` and `rsp`, over shared coordinates |
| `noc_interconnect.h`, `src/noc_interconnect.cpp` | **no RTL counterpart**; CDC-VP `bus_router` for the socket contract | TLM generic payload to/from the AXI endpoints, M:N tagged sockets, placement, clock gating |

The last two rows are the layer named in section 2.4: signed rules assembled by
unsigned code. Treat any behaviour that lives only there as unverified until
Step 10.3.

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
section 10.11 lists the complete current set of eleven cross-checks.

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
axi_noc<Width, Height, InFifoDepth, OutFifoDepth>   // two of the above
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

`axi_noc` puts two of them side by side, one carrying `axi_req_flit` and one
carrying `axi_rsp_flit`, and exposes a `network_port<FlitT>` per node per
network. That is the AXI datapath; see section 6.3.

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

Thirty-two tests pass with GCC 11.5.0 and SystemC 2.3.4:

| Test | Verified behavior |
|---|---|
| `test_reference_model` | Region base/end behavior, overlap rejection, out-of-mesh rejection, X-before-Y path |
| `test_stream_fifo` | Depth-2 spill and depth-4 FIFO branches: reset, fill, refused push at full (with and without a simultaneous pop), pointer wrap, drain order, mid-stream reset |
| `test_xy_route_select` | X-before-Y result, lock acquisition, locked route retention, release on last, local Eject |
| `test_wormhole_arbiter` | Reset priority, selected-ready behavior, packet lock, no interleave, round-robin advancement at two routes |
| `test_floo_router` | Two contenders for East, stalled output, FIFO occupancy, two-flit packet continuity, waiting requester service. Pinned to `OutFifoDepth = 0` so that branch stays covered |
| `test_floo_mesh` | 2x2 injection from `(0,0)` to `(1,1)`, unique correct ejection, stable data/valid under destination stall |
| `test_axi_noc` | Two separate `req`/`rsp` meshes over shared coordinates, independent traffic, per-node port identity |
| `test_noc_interconnect` | The TLM wrapper contract, each item a concrete assertion: payload validation (command, zero length, null pointer, wrapped streaming width, zero-length byte enables); byte-enable to `WSTRB` translation with a disabled byte proven unchanged in target memory; lane placement at `+4` and `+6` and across a beat boundary, checked by direct memory inspection; `DECERR` for an unmapped address distinguished from `SLVERR` for a target that refuses; region-crossing refused; delay contract (incoming delay spent, sub-cycle target latency rounded up, 1.5 and 2.0 cycles both costing 2, measured on four targets sharing one node); reset-time submission and idle-to-active wake-up; a scoreboard over a second initiator's writes; `last_latency_cycles()` excluding the target hold-off; bounded waits and a global watchdog. Round 3 added: the 256/257-beat `AxLEN` boundary at aligned and offset addresses; sparse multi-beat writes with fully disabled leading, trailing and middle beats; the widened-read policy against a spy target that records every downstream access; a region ending at `UINT64_MAX` written and read back; and the exact AXI-to-TLM response mapping for all four codes |
| `test_axi_lanes` | `AxSIZE`, `AxLEN`, lane offset and per-beat `WSTRB` for 13 address/length shapes, plus byte-enable holes and short repeating enable arrays. Checked on the fields themselves, not through a target, because a packing error and a matching unpacking error cancel |
| `test_noc_interconnect_bad_config` | Configurations refused before any traffic, each rejected while still an ordinary function call so teardown is normal: a non-positive clock period; a target on an initiator's node, including the documented default `(0,0)` of a port never placed; a manager moved onto an existing target; zero-sized, address-space-wrapping and overlapping regions; and proof that a refused call consumes no target slot and leaves a port's position intact. A legal layout is still accepted |
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

The tests are directed and small. They are not a substitute for randomized
stress or full protocol checking. Fifteen of them replay an RTL-captured golden,
so they detect a model regression offline, but a golden replay is not the
cross-check — the cross-check is section 10.11, and it needs Verilator.

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

### 10.7 Chimney — request path and subordinate side signed; unpacker not

`include/floo_noc_model/axi_chimney_pack.hpp` mirrors the flit-assembly
`always_comb` blocks of `hw/floo_axi_chimney.sv`, its `gen_route` destination
rules over `hw/floo_id_translation.sv`, and its `aw_w_sel_q` state.
`include/floo_noc_model/axi_chimney.hpp` adds the timed wrapper: the request
arbiter, the response arbiter, and the metadata FIFOs.

`tests/test_axi_chimney_pack.cpp` remains a **contract test against the RTL
text**, not an equivalence proof; keep reading it that way. The equivalence
proof is four separate cross-checks against the unmodified chimney:

| Cross-check | What it compares |
|---|---|
| `run_chimney_req_crosscheck.sh` | 16 request flits, content |
| `run_chimney_rsp_crosscheck.sh` | 8 response flits, content |
| `run_chimney_timing_crosscheck.sh` | 141 cycles, request-path timing |
| `run_chimney_rsp_timing_crosscheck.sh` | 221 cycles, response and subordinate side |

Isolating the packing was impossible — it is inline `always_comb` — so all four
instantiate the whole chimney, which also brings in the meta buffer and the
`NoRoB` gate. That turned out to be the right thing to do anyway.

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
The four harnesses here are hand-written synchronous BFMs instead; see section 16
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

- `axi_endpoint.hpp` and `noc_interconnect`, which have no RTL counterpart.

What *is* proven: eleven blocks, each in isolation, including the chimney's
request path, its subordinate side, and the mesh between them. The manager-side
response unpacker is not among them. Section 10.11 is the index.

**A block signed in isolation does not sign its parent.** The FIFO result did
not make the router cycle-equivalent, and the router result did not make the
mesh cycle-equivalent — each level needed its own comparison.

That rule applies to the integrated path too, and it is the reason the eleven
results must not be added up into an end-to-end claim. There is no cross-check
at the level above them, and the integrated path does not even instantiate the
same modules: the signed timed chimney (`axi_chimney.hpp`) appears only in its
two trace runners, while `noc_interconnect` composes `axi_chimney_pack.hpp`
with `axi_endpoint.hpp`. An earlier revision of this section asserted the
end-to-end claim two paragraphs above this rule, which is exactly the mistake
the rule exists to prevent.

### 10.11 The eleven signed cross-checks

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
100% tests passed, 0 tests failed out of 32
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

# synthetic survey, no firmware
./build/platforms/noc_soc/noc_soc --sim-us 200

# real firmware over the mesh
export PATH=/opt/toolchains/riscv-none-elf/bin:$PATH
make -C fw/dma_riscv clean
make -C fw/dma_riscv EXTRA_CFLAGS=-DDMA_BASE=0x10060000u
./build/platforms/noc_soc/noc_soc --fw fw/dma_riscv/dma_test.elf --sim-us 2000
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
  cycle-signed; the eleven cross-checks are indexed in section 10.11.
- The current integrated TLM path does not instantiate those timed chimney
  classes around the mesh, so the composed TLM-manager-port to
  TLM-subordinate-port latency is not one RTL-equivalent path.
- **The two integration layers are not signed and have no direct RTL
  counterpart**: `axi_endpoint.hpp` and `noc_interconnect`. Every integration
  defect found so far has been in one of them. See Step 10.3.
- Combinational SystemC delta-cycle settling is not hardware latency.
- No timing number should be marketed as cycle-accurate until its path passes
  RTL comparison. Conversely, do not describe a path as an estimate once it has
  passed one — an earlier revision of this document kept calling mesh latency an
  estimate after Step 9.2 had signed it.
- The current SystemC integration measures **11 cycles at one hop and 30 at
  six** on a 4x4 mesh. Those figures contain the signed mesh timing but use the
  abstract endpoint transactors; they are model calibration values, not a
  composed full-chimney RTL golden. Anything quoting 7 and 16 predates the
  output-FIFO correction.

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

The present CDC-VP wrapper nevertheless allows at most one transaction in flight
per upstream TLM port because `noc_interconnect` has one waiter per port and
holds `port_busy` until that transaction completes. The observed **+9-cycle**
worst contention is consequently a result for the current wrapper, three-master
platform and workload, not a limitation of the frozen RTL architecture. Step
10.5 must distinguish wrapper concurrency from downstream-ID configuration.

### 13.5 Verification gaps

- No randomized ready/valid stress **at the TLM layer**. The signed blocks each
  carry 80 to 100 cycles of deterministic pseudo-random traffic; the wrapper and
  the transactors above them have only directed tests.
- No scoreboarding across multiple concurrent initiators.
- No deadlock/livelock tests.
- No throughput/latency regression thresholds.
- No AXI protocol checker.
- ~~No automated platform-level test.~~ Closed by Step 10.2. Until then the
  firmware run was manual, which is why the component tests — 28 of them at the
  time — did not catch the bug in section 13.10.

Step 10.3 targets randomized TLM-layer stress, multi-initiator scoreboarding and
deadlock/livelock detection. Throughput/latency thresholds and an AXI protocol
checker remain separate gaps; neither should be claimed as closed by any step
currently on the list.

### 13.6 Instrumentation gaps

`noc_counters.hpp` covers the router boundary. Implemented and measured:

- accepted flits and packets per input and per output;
- stall and busy cycles per port;
- input buffer occupancy high-water and sum.

Still absent. The blocking reason for the first three has now gone away — the
mesh is signed and the AXI payload types exist — so these are simply unwritten
rather than unwritable:

- transaction and flit latency, and hop count: needs per-flit tagging. The
  platform measures end-to-end latency at the TLM boundary instead
  (`noc_interconnect::last_latency_cycles()`), which is coarser: it sees a
  transaction, not a flit;
- payload bytes per cycle;
- link utilization;
- outstanding responses and metadata-FIFO occupancy. Note the chimney harnesses
  already trace both metadata FIFO occupancies through hierarchical references,
  so the quantity is observable in RTL; it is the model-side counter that is
  missing.

Measured and analytic numbers must remain separate. The header distinguishes
measured counts, derived arithmetic over them, and analytic estimates, and
provides none of the last kind. **Keep it that way** — the moment an analytic
tier appears here, no consumer can tell which of the two they are reading.

Two properties to keep in mind when using the counters:

- flit conservation holds only within a reset-free, fully drained window; a
  reset discards buffered flits;
- the counter block is passive by construction, and the router cross-check runs
  with it attached to keep that true.

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

### 13.6b Clock gating rests on an unproven condition

Section 5.1 lists six conditions under which stopping the network clock is legal.
`network_idle()` in `src/noc_interconnect.cpp` checks none of them directly. It
checks `in_flight`, each node's `outbox` and `serving`, and each manager's
`has_request()` — all wrapper bookkeeping.

It cannot do better as the code stands: `floo_mesh` keeps `routers_` private and
exports no occupancy or lock state, and `axi_noc` exposes only the per-node
`network_port`. The router *does* expose occupancy and lock debug outputs
(section 9.6); they simply are not routed upward.

Why this matters rather than being pedantic: the argument that the wrapper's view
is sufficient depends on the route lock and the arbiter lock always releasing on
an accepted `last` flit. That is true of the signed RTL, and it is exactly the
invariant a future change could break — at which point the clock would stop with
a flit still held, and the symptom would be a hang with no diagnostic.

The fix belongs in Step 10.3: export occupancy and lock state from `floo_mesh`
and `axi_noc`, add a `mesh_quiescent()` that reads them, and require both wrapper
idle and mesh quiescence before waiting with the network clock stopped. Assert
the implication `network_idle() => mesh_quiescent()` at every clock-gating
decision.

Do not require equality on every cycle. The mesh can legitimately be quiescent
while the wrapper still owns an un-injected request or a target service is
pending, so `mesh_quiescent() == network_idle()` is too strong. Until the gate
itself consumes the mesh predicate and the implication is tested, treat "the
network clock stops only at proven quiescence" in section 15 as **not met**.

### 13.7 Integration risks

- A peripheral-style TLM wrapper would be architecturally wrong. `noc_interconnect`
  is a fabric adapter; keep it one.
- Gating the clock before whole-network quiescence can lose traffic. See 13.6b —
  this is a live risk, not a hypothetical one.
- Directly translating a blocking TLM transaction into one atomic NoC action
  can hide AXI channel ordering and back-pressure.
- **`b_transport` spends simulated time** rather than annotating `delay`. A
  caller relying on temporal decoupling will find its quantum consumed. No step
  currently resolves this; a platform that needs a global quantum has to decide
  whether the NoC opts out of it, and that decision is unmade.
- Address ownership and the M:N socket topology are now implemented from the
  CDC-VP platform map. Preserve that platform-derived contract and review it
  explicitly when the bus map or endpoint set changes.
- **No target may share a node with a manager.** `NoLoopback = 1` makes such a
  placement hang rather than fail. The wrapper refuses it at construction; do not
  remove that guard.

### 13.8 Licensing/provenance audit

Hardware-derived files use SPDX `SHL-0.51`; some reference/test files use
SPDX `Apache-2.0`. The component currently ships `LICENSES/SHL-0.51.txt`.

Before packaging, audit whether an Apache-2.0 license text and a provenance file
must also be installed. Do not remove existing SPDX headers. The final SDK must
ship all required license and provenance material for code compiled into it.

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

`platforms/noc_soc` with `--fw` loads and starts the firmware correctly — its
UART output appears — and then the Bremen ISS takes a trap to `mtvec = 0`. The
CPU spins at address 0 for the rest of the run.

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

### Fix

`platforms/noc_soc/src/noc_soc_top.cpp` now reserves the final 4 KiB RAM page:

```cpp
constexpr std::uint64_t kSurveyScratch =
    kRamBase + kRamSize - 0x1000;
```

All synthetic RAM warm-up, single-beat correctness, and burst correctness
writes use that page. The current DMA firmware is linked into the first 1 MiB,
including its stack and DMA buffers, so the survey no longer aliases it.

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

Do not remove `port_busy`/`port_free` or change `MaxUniqueIds` as a fix for this
incident; neither was causal.

### Three wrapper bugs already found and fixed during this work

Listed because they show the shape of what tends to go wrong here, and none of
them were visible with a single manager on the mesh:

- **Injection race.** `step_once` drove `inject_valid` from `has_request()` and
  then re-read it after the half-cycle wait. A `b_transport` running in another
  process could push a request into a manager during that wait, and the flit
  was popped without ever being driven — it vanished. Fixed by remembering what
  was actually driven.
- **Hold-off underflow.** The target's access latency was discounted from every
  waiting transaction, which underflows as soon as more than one is in flight.
  Fixed by charging it to the requesting node, keyed by the request's `src_id`.
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

## 14. Step history and required next work

Steps 1 to 9.2 are done; each entry records what it produced and, where it
applies, what defect it found. Read those before touching the block they signed —
several of them exist because an earlier assumption was wrong, and the reasoning
matters more than the result.

Step 10 is implemented but unsigned. **Its sub-steps do not run in numeric
order**; the execution order and the reason for it are in the table under Step 10.
Step 11 is unstarted.

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
arbitration, no shared buffering, no ordering between them. So `axi_noc` is two
`floo_mesh` instantiations — `floo_mesh` was already templated on `FlitT`, so
this needed no change to the mesh at all.

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
out of order — the current wrapper's single input ID plus its one-in-flight
policy does so — or freeze a configuration with multiple downstream IDs and
ID-based response matching. Derived from the RTL text, not demonstrated against
a full-system RTL simulation.

Later clarification: this constrains response ordering and downstream-ID use; it
does **not** mean only one request may be outstanding. See the corrected current
state in section 13.4. The CDC-VP wrapper's one-in-flight limit is imposed by
`port_busy` and its single waiter, not by this metadata FIFO.

**Still not signed:** inter-node timing (needs the generated top elaborated
against the model — the harness does not exist), the chimney's response-path
timing and subordinate side, and the endpoint transactors themselves, which
have no RTL counterpart.

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

### Step 10 — CDC-VP TLM integration — IMPLEMENTED; SIGN-OFF NEXT

The handoff previously called the integration design the next step, but the
code has moved past that point:

- `noc_interconnect` presents a `bus_router`-like TLM target interface plus
  multiple tagged upstream ports and explicit mesh placement;
- `platforms/noc_soc` instantiates a 4x4 network with CPU, DMA, and survey
  managers, and routes RAM plus the SoC peripheral map through it;
- blocking TLM accesses spend simulated time while traversing the network;
- `network_idle()` stops mesh clock evaluation when there is no in-flight
  work, and the `work` event restarts it;
- `test_noc_interconnect` covers the wrapper contract at component level;
- real SoC-map DMA firmware reaches `DMA PASS` through the NoC.

The integration is therefore a working proof of implementation, not a design
task. The next work is to turn that proof into a stable, automated contract.
Do not restart the socket-topology design from scratch.

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
| 4 | **pre-A1 cleanup round 3** — `docs/PRE_A1_REVIEW_ROUND3_FIX_PLAN.md` | **current**; A-1 waits on its reviewer sign-off |
| 5 | **A-1** — cross-check the manager-side response unpacker | next, after the round-3 reviewer box is checked |
| 6 | **A-2** — assemble a per-node chimney into `axi_noc` | |
| 7 | **A-3** — drive it from `noc_interconnect` | |
| 8 | **10.3** — stress the TLM layer that remains above the chimney | |
| 9 | **10.1** — separate survey and firmware ownership | |
| 10 | **10.4** — install, packaging, licensing | |
| 11 | **10.5** — wrapper concurrency, and separately `MaxUniqueIds > 1` | |
| 12 | **11** — the fast approximately-timed mode | |

**Why 10.3 sits after A-3.** It was second on an earlier list, on the reasoning
that the TLM layer was the only unsigned correctness layer. That reasoning no
longer holds: two cleanup rounds fixed twelve defects in that layer and gave it
directed coverage, and Step A-3 *replaces* part of what 10.3 would stress.
Stressing code that is about to be swapped out is wasted work. What remains for
10.3 afterwards — randomized multi-initiator stress, deadlock detection — is
still needed and still not done.

**A-1, A-2 and A-3 are part of the v0 completion gate**, not work beyond it.
See section 15.

### Step 10.2 — Add an automated real-firmware regression — DONE (2026-07-30)

The 28 standalone tests that existed at the time did not catch the platform bug
in section 13.10. They could not: it lived in `platforms/noc_soc`, and a
component test cannot cover a platform. What was built is a bounded CTest that:

1. builds `fw/dma_riscv` with `EXTRA_CFLAGS=-DDMA_BASE=0x10060000u`;
2. runs `noc_soc --fw <elf> --sim-us 2000`, the ELF built in a private copy
   of the firmware sources rather than in the working tree;
3. requires `DMA PASS`;
4. rejects `Taking trap`, `[PC] trapped`, SystemC errors, timeouts, and a final
   CPU PC of zero;
5. preserves the full simulation log on failure.

**Deviation from the original acceptance criteria, recorded rather than
quietly dropped.** This step originally asked for a second *image* — a small
CPU alignment/readback ELF reaching `DONE`. That image does not exist.

What runs instead is the platform's own synthetic survey, with no `--fw`,
requiring `result   all bytes match`. Describe it accurately:

- it is **not** a second firmware image, and not equivalent to one;
- it is a **substitute smoke path with reduced and different coverage**: no CPU
  workload, no DMA programming, no interrupt path;
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

### Step 10.3 — Stress the TLM wrapper

The routers, FIFOs, arbiters, chimney paths, ordering rule, and mesh timing are
RTL-signed. The endpoint transactors and TLM wrapper have no RTL counterpart,
so this is the highest-risk correctness layer in the project.

Extend `test_noc_interconnect` with a bounded, scoreboard-driven scenario:

- three initiators concurrently access the same RAM target;
- mix reads and writes of 1, 2, 4, 6, and 8 bytes plus multi-beat bursts;
- include a target with annotated latency;
- verify address, data, response, requester ownership, and completion order;
- cover reset-time submission, idle-to-active wake-up, whole-network
  quiescence, unmapped accesses, and the self-node placement guard;
- use per-transaction timeouts and a global watchdog so lost flits fail rather
  than hang.

This test must directly cover the integration defects already found:
the half-cycle injection race, requester hold-off attribution, newest-versus-
oldest request metadata, and odd-length burst tails. Firmware/survey address
ownership is a platform concern and belongs to the Step 10.2 regression and the
Step 10.1 mode split, not to `test_noc_interconnect`.

**Also close the quiescence gap here** (section 13.6b). Export input/output FIFO
occupancy and route/arbiter lock state from `floo_mesh` and `axi_noc`, add a
`mesh_quiescent()` that reads them, and make the clock-gating decision require
both wrapper idle and mesh quiescence. Assert
`network_idle() => mesh_quiescent()` whenever the thread is about to wait with
the clock stopped. Do not assert equality across all cycles: an empty mesh can
coexist legitimately with wrapper-side work that has not yet been injected.
Until the production gate consumes this predicate, the definition-of-done
bullet "the network clock stops only at proven quiescence" is unmet, and a
future change to the lock-release invariant could show up as an undiagnosable
hang.

Acceptance criteria:

- every listed defect has a test that fails when the fix is reverted;
- every transition into the clock-gated wait satisfies
  `network_idle() && mesh_quiescent()`;
- directed tests exercise states where the mesh is quiescent but wrapper work is
  still pending, proving that equality is not incorrectly required;
- no test can hang: every wait is bounded and a global watchdog exists.

### Step 10.1 — Separate synthetic-survey and firmware modes

Make the two ownership modes explicit and mutually exclusive:

- **survey mode:** the synthetic manager may write its reserved RAM scratch
  page, walk the peripheral map, and own DMA0;
- **firmware mode:** firmware owns RAM contents and DMA0; synthetic accesses
  that can modify firmware state or trigger peripheral side effects must not
  run.

The final 4 KiB RAM page currently avoids the known ELF layout, but it is a
documented convention rather than an enforced memory-map reservation. A future
ELF could legitimately use it and recreate the same corruption. Prefer
disabling destructive synthetic traffic in firmware mode. If a scratch page
is retained, reserve it in the platform memory contract and validate every ELF
`PT_LOAD` range against it.

Acceptance criteria:

- survey mode still reports RAM, peripheral, and DMA measurements;
- firmware mode performs no synthetic RAM write and does not program DMA0;
- the current DMA firmware still reaches `DMA PASS` — via the Step 10.2
  regression, not by hand;
- loading an ELF whose segment overlaps a reserved scratch region fails before
  simulation starts with the exact conflicting range.

### Step 10.4 — Prove install, packaging, and licensing

Once integration regressions pass:

- install into a clean prefix with `cmake --install`;
- build a minimal external consumer using only the installed package, linking
  `cdc::components::noc_interconnect` rather than the header-only target;
- run the packaged `noc_soc` outside the build tree and verify SystemC RPATH;
- exercise `cdc_make_portable` and `cdc_package_platform`;
- audit Apache-2.0 and SHL-0.51 license texts plus source provenance;
- update `STATUS.md`, the verification matrix, and this handoff.

Sign-off requires a clean-prefix consumer and packaged-platform run, not only
a successful in-tree link.

### Step 10.5 — Decide the required outstanding concurrency and downstream-ID policy

Do this only after Steps 10.2, 10.3, 10.1 and 10.4 pass.

Separate two decisions that the earlier roadmap incorrectly combined:

1. **Concurrent outstanding transactions at one TLM upstream port.** The frozen
   `MaxUniqueIds = 1` branch has read and write metadata FIFOs of depth
   `MaxTxns = 32`; it does not impose a one-outstanding limit. The current limit
   comes from `noc_interconnect` using one waiter and one `port_busy` bit per
   port. A concurrency implementation needs a per-transaction queue or slot,
   unambiguous completion association, bounded capacity, and ordering tests. It
   can initially retain `MaxUniqueIds = 1` when responses are guaranteed to
   preserve FIFO order.
2. **Multiple downstream AXI IDs and out-of-order response matching.** If the
   requirement needs these, freeze a new configuration with
   `MaxUniqueIds > 1` and cross-check the `id_queue` branch of
   `floo_meta_buffer.sv`, ID allocation/matching and back-pressure against RTL.

`MaxUniqueIds` and `BRoBType`/`RRoBType` are independent configuration fields.
Raising `MaxUniqueIds` does not by itself replace `NoRoB`. Re-sign
`rob_order_gate.hpp` or model another RoB branch only if the frozen RoB type also
changes.

Do **not** obtain more traffic by merely deleting `port_busy`. The existing
wrapper has only one waiter and one completion record per port, so that shortcut
would lose transaction association. Redesign the wrapper state and tests first.

The current three-manager platform's observed worst contention of **+9 cycles**
is a useful baseline for that exact implementation and workload. It is not proof
that the frozen configuration or a 4x4 FlooNoC cannot congest.

Virtual channels, ATOPs, collectives, multicast, reduction, and the
narrow-wide network remain deferred until an SoC requirement explicitly needs
them.

### Step A — Compose the signed chimney into the datapath — IN PROGRESS

Chosen over the alternative of leaving the abstract transactors in place and
only testing them. The goal: the platform's network interface should *be* the
RTL-signed chimney, not code that follows its rules.

**The finding that set the scope.** The chimney has four quadrants and the model
had three:

| Quadrant | Module | Status |
|---|---|---|
| Manager: AXI request → `req` link | `axi_chimney_request` | signed, 141 cycles |
| Subordinate: `req` → AXI out → B/R → `rsp` | `axi_chimney_response` | signed, 221 cycles |
| Manager: `rsp` link → B/R to the manager | `axi_chimney_manager_response` | **new**, not yet signed |

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

#### A-1 — cross-check the unpacker against RTL — NEXT

The unpacker's rules are read off the RTL text, which is the same standing this
project refuses to call "signed". Drive `floo_rsp_in` on the unmodified chimney,
trace `axi_in_rsp_o` plus `floo_rsp_out_ready` and the R counter, and compare per
cycle. It would be the twelfth cross-check. Include a multi-beat R burst — the
RLAST defect above is exactly what such a harness exists to catch.

Do not start A-2 before this passes.

#### A-2 — assemble a per-node chimney and wire it into `axi_noc`

Manager side is `axi_chimney_request` + `axi_chimney_manager_response`;
subordinate side is `axi_chimney_response`. Add a composition test.

#### A-3 — switch `noc_interconnect` to the signal-driven chimney

The largest and riskiest piece: the wrapper currently calls methods on
`axi_endpoint.hpp` and must instead drive AW/W/AR cycle by cycle. It changes code
that currently runs firmware to `DMA PASS`, which is why Step 10.2 was built
first. Expect the measured latencies to move — 11 cycles at one hop and 30 at six
come from the abstract transactors, and a timed chimney has its own cost.

### Step 11 — Build the fast approximately-timed mode — THE POINT OF ALL THIS

Not started, and the largest unscheduled item in the project. Section 5 requires
a decision on the H1/coexistence path; `platforms/noc_soc/README.md` closes with
"this one to calibrate, and an approximately-timed model for long runs. This
platform is the calibration reference." Nothing calibrates against it yet, so
the roadmap and the README currently disagree.

The platform reports about **52 ns of modeled target time per retired
instruction** while fetching through the mesh. That is an accuracy/calibration
number, not evidence of simulator speed: it says nothing about host wall-clock
time. Per-cycle evaluation is expected to be more expensive than an
approximately-timed calculation, but Step 11 must first measure that cost on the
same host and workload. Report retired instructions per host second, host
seconds per simulated microsecond, or real-time factor before claiming that a
fast mode is required for OS boot.

Why an approximately-timed backend is now tractable: the no-contention platform
samples follow approximately **4 cycles per hop plus a fixed cost**, consistent
with the input and output spill registers in each traversed router. Treat this as
a calibration hypothesis, not a universal exact law. Verify it over more hop
counts, access widths and bursts; contention and back-pressure require either an
explicit approximation or a declared out-of-scope condition.

Suggested shape:

- one interconnect class, two timing back-ends chosen at construction;
- the fast back-end annotates `delay` instead of spending simulated time, which
  also resolves the temporal-decoupling risk in section 13.7;
- a host-performance baseline for the detailed cycle-stepped backend before the fast
  backend is implemented;
- a calibration test that runs the same stimulus through both back-ends and
  requires the fast one to stay within a declared tolerance of the signed one,
  per hop count and per access width;
- the tolerance is a number in the test, not a comment. If it has to be widened,
  that is a visible diff.

Acceptance criteria:

- the fast mode reproduces the **end-to-end latency figures in section 13.1**
  within the declared tolerance. Not section 10.11: that table lists cross-check
  *trace lengths* — 141 cycles, 1872 node-cycles — which are how much stimulus
  each harness ran, not latency targets. Confusing the two would have the fast
  mode calibrated against a number that means nothing about latency;
- and the calibration baseline is re-measured after Step A-3. The current 11
  cycles at one hop and 30 at six, and the ~52 ns per retired instruction, were
  all produced with the abstract endpoint transactors in the path. Once the timed chimney replaces them those figures
  will move, and silently keeping the old ones would calibrate the fast mode
  against a configuration that no longer exists;
- `fw/dma_riscv` reaches `DMA PASS` in both modes;
- accuracy is compared in modeled target time, while speedup is measured
  separately in host wall-clock metrics on the same workload, build type,
  machine and measurement window;
- the detailed cycle-stepped mode remains the calibration reference, and every
  published timing number states whether its complete path is RTL-signed or
  includes the abstract TLM integration layer.

Do not delete or weaken the detailed path to make the fast one look good. Its
signed blocks and model-level integration measurements are the references that
give the fast mode its claim to accuracy.

---

## 15. Definition of done for the v0 vertical slice

The v0 slice is not complete until all of the following are true.

| Criterion | State |
|---|---|
| every included leaf has a standalone SystemC test | **NOT met** — see below |
| FIFO, route selector, arbiter, and router have RTL trace comparisons | **met**, plus seven more; section 10.11 |
| the router passes routing, contention, back-pressure, and wormhole tests | **met** |
| a small req/rsp mesh delivers every flit exactly once | **met** — `test_axi_noc`, and mesh timing signed at 1872 node-cycles |
| AXI AW/W/AR/B/R transactions survive endpoint-to-endpoint conversion | **met** — `test_axi_endpoint`, and real firmware reaches `DMA PASS` |
| ordering rules for the selected configuration are verified | **met** — `NoRoB`, 127 cycles |
| measured timing is separated from analytic estimates | **met** — `noc_counters.hpp` has no analytic tier; keep it that way |
| CDC-VP integration reflects a fabric, not a fake accelerator peripheral | **met** — `noc_interconnect` is an M:N fabric adapter |
| the network clock stops only at proven quiescence | **NOT met** — see 13.6b |
| licensing and provenance are complete | **NOT met** — see 13.8; closes in Step 10.4 |
| **the datapath is cycle-accurate from AXI manager port to AXI subordinate port** | **NOT met** — Steps A-1, A-2, A-3 |

Two further criteria that were implicit and should be explicit:

| Added criterion | State |
|---|---|
| the unsigned integration layer has scoreboard-driven multi-initiator stress | **NOT met** — a scoreboard and directed coverage exist; randomized stress and deadlock detection do not, Step 10.3 |
| a platform-level regression runs real firmware unattended | **met** — `noc_soc_firmware_regression`, Step 10.2 |

**Five criteria are unmet.** Counted here explicitly, because an earlier
revision said "three criteria short" while the tables above marked five — the
kind of drift that makes a completion gate meaningless:

1. every included leaf has a direct standalone test — `rr_arb_tree.hpp` and
   `meta_buffer.hpp` do not have one;
2. the network clock stops only at proven mesh quiescence — section 13.6b;
3. licensing and provenance are complete — section 13.8, Step 10.4;
4. the datapath is cycle-accurate from AXI manager port to AXI subordinate
   port — Steps A-1, A-2, A-3;
5. scoreboard-driven wrapper stress is complete — Step 10.3.

Nothing in this list requires Step 10.5 or Step 11; those are beyond v0.

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

> Read `docs/AI_HANDOFF_CONTEXT.md`, then continue **Direction A** (see Step 10
> and Step A in section 14); Step 10.2 is done. Anchor every NoC behavioural
> decision in the FlooNoC IP at
> `/home/duyptt_HW/Documents/work/Study_FlooNoC/FlooNoC` (upstream
> `https://github.com/pulp-platform/FlooNoC.git`), and in the CDC-VP platform for
> the socket side.
>
> Steps 1 to 9.2 produced eleven isolated RTL cross-checks, indexed in section
> 10.11, covering the selected router, FIFO, arbitration, chimney and mesh timing
> blocks. Do not overstate this as one composed TLM-port-to-TLM-port RTL timing
> proof: `noc_interconnect` uses abstract endpoint transactors rather than the
> timed chimney classes. Step 10 has already implemented `noc_interconnect` and
> `platforms/noc_soc`: CPU, DMA and survey traffic cross a 4x4 mesh, and real
> SoC-map DMA firmware reaches `DMA PASS` at about 52 ns of modeled target time
> per retired instruction. Do not redesign the socket topology and do not replace
> it with an NPU-style worker.
>
> **Follow the ordered table in section 14, not numeric order.** With 10.2 done,
> what remains runs:
>
> ```text
> A-1  ->  A-2  ->  A-3  ->  10.3  ->  10.1  ->  10.4  ->  10.5  ->  11
> ```
>
> A-1 to A-3 come first because they are part of the v0 completion gate and
> because **Step A-3 replaces part of what 10.3 would stress** — stressing the
> abstract transactors that A-3 is about to delete buys directed coverage of code
> with a scheduled removal date. 10.3 then outranks 10.1, and 10.1 cannot be
> verified without 10.2. Section 14 gives the reasoning in full; if this prompt
> and section 14 ever disagree, section 14 is authoritative.
>
> **A-1 first**, and do not skip past it: the manager-side response unpacker
> `axi_chimney_manager_response` is written and unit-tested but has no RTL
> cross-check, so it is not signed. Drive `floo_rsp_in` on the unmodified
> chimney, trace `axi_in_rsp_o`, `floo_rsp_out_ready` and the R counter, and
> compare per cycle. Include a multi-beat R burst: the RLAST defect recorded
> under Step A is exactly what such a harness exists to catch, and the
> request-timing cross-check provably could not see it because it holds the
> response link idle. Only then A-2 and A-3.
>
> Step 10.3, when its turn comes: extend `test_noc_interconnect` with three
> concurrent initiators on one RAM target, mixed widths including 6-byte and
> odd-length tails, bursts, target latency, a scoreboard, per-transaction bounds
> and a global watchdog — and close the quiescence gap in section 13.6b by
> exporting mesh occupancy and lock state. Require `mesh_quiescent()` as well as
> wrapper idle before clock gating, and assert
> `network_idle() => mesh_quiescent()` at every gating decision; do not require
> the two predicates to be equal on all cycles.
>
> Four constraints to carry in, all established and documented:
>
> 1. **`MaxUniqueIds = 1`** makes the chimney's metadata a plain in-order FIFO
>    with no ID matching, so responses must preserve FIFO order. It does not
>    impose a one-outstanding limit: the RTL metadata FIFO has depth
>    `MaxTxns = 32`. The current one-transaction-per-port limit comes from the
>    wrapper's single waiter and `port_busy`. The measured +9-cycle contention is
>    a baseline for that wrapper and workload, not proof that a 4x4 FlooNoC cannot
>    congest.
> 2. **`OutFifoDepth = 2`**, hardcoded by every FlooGen router template. The
>    measured end-to-end figures are 11 cycles at one hop and 30 at six; anything
>    quoting 7 and 16 predates that correction.
> 3. **The RTL timing blocks are signed individually**: chimney request timing,
>    chimney subordinate-side timing, and the mesh. The manager-side response
>    unpacker is implemented and unit-tested but **not signed** — A-1. The currently integrated TLM path does not compose
>    the timed chimney classes. `noc_counters.hpp` separates measured from derived
>    and has no analytic tier; keep it that way.
> 4. **The endpoint transactors and the TLM wrapper have no RTL counterpart** and
>    are not RTL-signable. They are a driver and a collector built on signed
>    rules. Every integration defect found so far has been in that layer, which
>    is why Step 10.3 outranks Step 10.1.
>
> After correctness is automated, prove clean-prefix install, external consumer
> linking, packaged-platform execution, RPATH, licenses and provenance (10.4).
> Only then decide separately whether the wrapper needs concurrent outstanding
> transactions and whether the RTL configuration needs `MaxUniqueIds > 1`
> (10.5). Never remove `port_busy` without replacing the single-waiter completion
> state with a tested per-transaction design. **Step 11, the fast
> approximately-timed mode calibrated against this model, is unstarted and is
> the largest item left. Before claiming that it makes OS boot faster, record a
> host wall-clock baseline; 30 ns/instruction is target timing, not simulator
> throughput.**
>
> Do not open the NPU component for architecture questions. Its CMake shape,
> test registration, and platform/SDK packaging patterns are reusable; its
> register map, socket structure, and worker model are not.
>
> Harness discipline for any new cycle comparison: synchronous BFM idiom — drive
> at `clk = 0`, sample pre-edge, raise the clock, sample post-edge, no phase
> offsets. Do not copy the `ApplTime`/`TestTime` arithmetic of the two chimney
> content harnesses; four defects came from it. Keep the far side of every
> interface permanently ready, observe the DUT with concurrent monitors, compute
> expected output counts before running a step, and confirm the stimulus is still
> moving before trusting a pass. See section 16 rules 9e to 9i.
>
> Re-run the full standalone regression (32 tests) and every registered RTL
> cross-check after any model change. Use the mandated GCC/G++/PATH environment
> before every build, preserve unrelated dirty files, and when a milestone is
> signed off update **sections 2 to 13 as well as section 14** plus `STATUS.md` —
> see rule 10b for why that is called out explicitly.

If a dependency cannot be resolved without network access or a tool install,
record the precise missing artifact and proceed with other safe, local,
read-only preparation such as the trace schema and harness structure. Do not
mislabel a model-to-model comparison as RTL equivalence.
