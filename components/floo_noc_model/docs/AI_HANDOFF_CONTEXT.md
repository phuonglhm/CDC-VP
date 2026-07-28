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

Status snapshot date: **2026-07-28**.

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

Nothing before Step 8 needs it. Steps 5 to 7 are anchored entirely in the
FlooNoC IP. Do not open it for architecture questions.

### 2.2 Integration role in CDC-VP

FlooNoC should replace, compose, or sit hierarchically within the CDC-VP fabric.
It must not be attached as one ordinary MMIO target peripheral.

The eventual CDC-VP integration will therefore need a fabric-oriented M:N TLM
adapter or a composition of TLM-to-AXI endpoint transactors, depending on the
chosen platform architecture. That decision must be based on CDC-VP bus
topology and endpoint requirements, not on the NPU wrapper.

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
- The XY route selector, the input FIFO, the wormhole arbiter, and the
  five-port router are currently RTL-signed.
- Mesh, links, and end-to-end latency are still estimates.

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
parallel. This component is the Direction-2 workstream; it does not establish
that a separate FlooNoC-specific H1 implementation is already complete. During
CDC-VP integration, inspect the existing fast platform fabric and decide how it
serves as the H1/coexistence path.

| Phase | FlooNoC interpretation | Current state |
|---|---|---|
| P0 | Freeze RTL/configuration, supported traffic, timing target, metrics, and integration role | Completed for vertical slice v0 |
| P1 | Timing-independent address decode, routes, transaction/flit reference behavior, and vectors | Address map and XY path implemented; AXI reference behavior remains pending |
| P2 | Signal-safe coordinate/header/flit/AXI types and address map | Basic flit foundation implemented; full AXI types pending |
| P3 | FIFO, route selection, arbitration, router, links, chimney/meta/RoB blocks | FIFO wrap, XY selector, arbiter, simplified router implemented; chimney/meta/RoB pending |
| P4 | Router/link topology and endpoint wiring | Abstract rectangular mesh implemented; generated topology and AXI endpoints pending |
| P5 | NoC-specific measured counters | Implemented for the router boundary over RTL-signed signals; link and end-to-end latency counters deliberately deferred |
| P6 | Optional configuration translation | NPU-style operation driver is not applicable; FlooGen/platform configuration mapping may be needed later |
| P7 | Unit, router, mesh, stress, and RTL equivalence tests | Thirteen SystemC tests pass; route selector, input FIFO, wormhole arbiter, and router have RTL cross-checks |
| P8 | Standalone build | Implemented with CMake and Make |
| P9 | SW/platform-visible contract | Do not copy an accelerator register map; define only actual fabric/config/debug contracts if required |
| P10 | TLM integration | Must be a fabric/endpoint adapter, not an accelerator target+worker wrapper |
| P11 | CDC-VP CMake component/install/export | Interface target exists; final integration library shape is pending |
| P12 | Platform assembly and packaging | Pending |
| P13 | RISC-V/SoC traffic validation | Pending |
| P14 | Coexistence and maintenance | Pending; future clock gating only at whole-network quiescence |

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

---

## 6. Frozen vertical slice v0

### 6.1 Included

| Parameter | Locked value |
|---|---|
| Network class | Single-AXI vertical slice |
| Routing | Deterministic XY |
| Traffic | Unicast |
| Intended physical channels | `req` and `rsp` |
| Flow control | Ready/valid |
| Virtual channels | No modeled VC dimension or credit protocol |
| Router ports | North, East, South, West, Eject |
| Local ports | One Eject port per router |
| Input FIFO | Enabled; template depth, currently depth 2 in router/mesh tests, which selects the spill-register branch of the RTL wrap |
| Output FIFO | Disabled |
| Topology | Rectangular 2-D mesh |
| Output source | SystemC state/datapath |
| Timing target | Approximate until block-level RTL cross-check |

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

### 6.3 Important present-vs-intended distinction

The current generic mesh carries one `FlitT` stream. Although
`physical_channel::{req,rsp}` and `axi_channel` identifiers exist in the type
foundation, the model does **not yet instantiate two physically separate req
and rsp networks**, and it does not yet implement channel-specific AXI payload
structures.

Do not describe the current mesh as a complete single-AXI FlooNoC datapath.

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
    P0_SCOPE.md
    RTL_MAPPING.md
    STATUS.md
    TLM_IP_DIRECTION_GUIDE.en.md
    TLM_IP_H2_BUILD_PLAYBOOK.en.md
    AI_HANDOFF_CONTEXT.md
  include/floo_noc_model/
    floo_types.hpp
    reference_model.hpp
    stream_fifo.hpp
    xy_route_select.hpp
    rr_arb_tree.hpp
    wormhole_arbiter.hpp
    floo_router.hpp
    floo_mesh.hpp
    noc_counters.hpp
    axi_types.hpp
    axi_chimney_pack.hpp
    meta_buffer.hpp
    rob_order_gate.hpp
    axi_endpoint.hpp
  tests/
    CMakeLists.txt
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
    route_trace_sc.cpp
    fifo_trace_sc.cpp
    arbiter_trace_sc.cpp
    router_trace_sc.cpp
    axi_sizing_trace.cpp
    data/
      route_select_stimulus.csv
      route_select_expected.csv
      gen_stream_fifo_stimulus.py
      stream_fifo_stimulus.csv
      stream_fifo_expected_d2.csv
      stream_fifo_expected_d4.csv
      gen_wormhole_arbiter_stimulus.py
      wormhole_arbiter_stimulus.csv
      wormhole_arbiter_expected_n2.csv
      wormhole_arbiter_expected_n4.csv
      wormhole_arbiter_expected_n5.csv
      gen_router_stimulus.py
      router_stimulus.csv
      router_expected.csv
      axi_sizing_configs.csv
      axi_sizing_expected.csv
  rtl_crosscheck/
    compare_traces.py
    fetch_rtl_deps.sh
    gen_rtl_filelist.sh
    run_axi_sizing_crosscheck.sh
    run_chimney_req_crosscheck.sh
    run_chimney_rsp_crosscheck.sh
    run_route_select_crosscheck.sh
    run_stream_fifo_crosscheck.sh
    run_router_crosscheck.sh
    run_wormhole_arbiter_crosscheck.sh
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
      tb_floo_axi_chimney_elab.sv
```

`stream_fifo_expected_d*.csv` and `wormhole_arbiter_expected_n*.csv` are
captured from the frozen RTL by the matching cross-check runner. They exist so the standalone regression can
detect a FIFO regression without Verilator; they are never a substitute for the
RTL comparison and must be regenerated from RTL, never from the model.

The model is currently header-only and exported through the CMake interface
target:

```text
cdc::components::floo_noc_model
```

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
| `floo_mesh.hpp` | FlooGen generated `floo_*_noc.sv` topology concept | Rectangular router composition with abstract endpoints |
| Future `axi_chimney.hpp` | `hw/floo_axi_chimney.sv` | AXI/flit conversion, AW/W coupling, request/response arbitration |
| Future `meta_buffer.hpp` | `hw/floo_meta_buffer.sv` | Request metadata retention and downstream ID mapping |
| Future RoB model | `hw/floo_rob*.sv` | AXI response ordering |
| Future TLM adapters | CDC-VP bus and AXI semantics | TLM generic payload to/from modeled AXI endpoints |

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

This is the only block with a passing RTL cycle cross-check.

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
floo_router<FlitT, InFifoDepth>
```

Current fixed structure:

- five inputs and five outputs;
- one ready/valid stream per port;
- one input FIFO per port;
- one XY route selector per input;
- a combinational crossbar;
- one five-requester wormhole arbiter per output;
- no output FIFO;
- no VC layer;
- no collective/reduction path;
- no credit path.

Crossbar optimization mirrors the relevant frozen RTL configuration:

- no input-to-same-output loopback;
- in XY mode, a flit entering from North/South cannot return to East/West after
  the X dimension should already have been resolved.

Debug outputs expose:

- input FIFO occupancy per port;
- selected input per output;
- output lock state per output.

The current router test demonstrates contention and packet non-interleaving,
but the router has not yet been compared to the RTL.

### 9.7 `floo_mesh.hpp`

Template:

```cpp
floo_mesh<FlitT, Width, Height, InFifoDepth>
```

Behavior:

- creates `Width * Height` routers;
- assigns coordinates in row-major order:
  `node_index(x, y) = y * Width + x`;
- connects East/West and North/South neighbors;
- ties absent boundary inputs invalid and boundary outputs not-ready;
- uses each router’s Eject input for endpoint injection;
- uses each router’s Eject output for endpoint ejection;
- exposes one abstract inject/eject ready/valid endpoint per node.

Important timing caveat:

The present inter-router links are combinational `SC_METHOD` connections. There
is no explicit link pipeline/cut module yet. Storage comes from router input
FIFOs. Therefore, do not claim that the model already implements a separately
configurable one-cycle link latency.

---

## 10. Verification evidence

### 10.1 Standalone SystemC regression

Twenty-one tests pass with GCC 11.5.0 and SystemC 2.3.4:

| Test | Verified behavior |
|---|---|
| `test_reference_model` | Region base/end behavior, overlap rejection, out-of-mesh rejection, X-before-Y path |
| `test_stream_fifo` | Depth-2 spill and depth-4 FIFO branches: reset, fill, refused push at full (with and without a simultaneous pop), pointer wrap, drain order, mid-stream reset |
| `test_xy_route_select` | X-before-Y result, lock acquisition, locked route retention, release on last, local Eject |
| `test_wormhole_arbiter` | Reset priority, selected-ready behavior, packet lock, no interleave, round-robin advancement at two routes |
| `test_floo_router` | Two contenders for East, stalled output, FIFO occupancy, two-flit packet continuity, waiting requester service |
| `test_floo_mesh` | 2x2 injection from `(0,0)` to `(1,1)`, unique correct ejection, stable data/valid under destination stall |
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
| `test_router_trace_sc` | 214-cycle router trace against the RTL-captured golden |

The tests are directed and small. They are not a substitute for randomized
stress, full protocol checking, or RTL equivalence.

### 10.2 Route-selector RTL cross-check

The cross-check runs the same 12-cycle CSV stimulus through:

1. `xy_route_select<test_flit>` in SystemC;
2. the unmodified frozen `hw/floo_route_select.sv` in Verilator.

Compared trace columns:

```text
cycle,route,locked
```

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

The expected trace is:

```text
cycle,route,locked
0,4,0
1,4,0
2,1,0
3,1,0
4,1,1
5,1,1
6,3,0
7,4,0
8,0,0
9,2,0
10,1,0
11,3,0
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

### 10.7 Chimney flit assembly — modeled, not yet cross-checked

`include/floo_noc_model/axi_chimney_pack.hpp` mirrors the flit-assembly
`always_comb` blocks of `hw/floo_axi_chimney.sv`, its `gen_route` destination
rules over `hw/floo_id_translation.sv`, and its `aw_w_sel_q` state.
`tests/test_axi_chimney_pack.cpp` is a **contract test against the RTL text**,
not an equivalence proof. A pass means the model still says what the RTL says;
it certifies nothing about timing or about behaviour under back-pressure.

Why there is no cross-check yet: the packing is inline `always_comb` inside the
chimney rather than a separate module, so it cannot be isolated. A real
cross-check must instantiate the whole chimney, which also brings in the meta
buffer and both reorder buffers.

What has been established is feasibility.
`rtl_crosscheck/axi_chimney/tb_floo_axi_chimney_elab.sv` instantiates the
unmodified chimney with the upstream test parameter set, using the real
`axi/typedef.svh` and `floo_noc/typedef.svh` macros, and lints with **zero
errors** against the Bender-generated file list. Only 13 parameters are needed;
the rest take defaults. So the remaining work is stimulus and tracing, not type
plumbing.

The upstream `hw/tb/tb_floo_axi_chimney.sv` cannot be reused under Verilator:
it depends on the class-based `axi_test` package, which Verilator does not
support. It remains usable under VCS, installed at
`/opt/synopsys/vcs/X-2025.06/bin/vcs`, if a class-based driver is ever wanted.

Rules captured from the RTL that the future harness must confirm:

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

### 10.10 What the current RTL cross-checks do not prove

They do not yet compare:

- full flit pass-through fields beyond the payload and the routed header;
- multiple local ports;
- other routing algorithms;
- multicast routing;
- FIFO depths other than 2 and 4, and `usage_o` at any depth;
- FIFO `flush_i` and `testmode_i` behavior;
- link timing;
- mesh end-to-end cycle latency;
- AXI chimney behaviour: modeled but unverified, see section 10.7.

The FIFO result signs off the leaf block in isolation. It does not by itself
make the router cycle-equivalent, because routing, crossbar, and arbitration
around the buffers are still unverified.

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
100% tests passed, 0 tests failed out of 21
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

---

## 12. Current tools and dependency state

Available:

- `/usr/bin/verilator`, version 5.022;
- `/opt/synopsys/vcs/X-2025.06/bin/vcs`;
- FlooNoC `Bender.yml`;
- FlooNoC `Bender.lock`.

Not available in `PATH`:

- `bender`.

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

- Only route selection with lock state, the input FIFO wrap, the wormhole
  arbiter, and the five-port router are RTL-signed.
- Existing router and mesh latency are model estimates.
- Combinational SystemC delta-cycle settling is not hardware latency.
- No timing number should be marketed as cycle-accurate until its path passes
  RTL comparison.

### 13.2 Type completeness

- The route-selector cross-check ran with a 2-bit `x`, 2-bit `y`, 1-bit
  `port_id` id type, so only coordinates 0..3 are signed off. The model's
  `coordinate` is 16-bit `x`/`y` with an 8-bit `port_id`; that wider range has
  no RTL evidence.
- `test_flit` uses a generic 64-bit payload.
- Full AXI AW/W/AR/B/R payload types are absent.
- Reserved-bit sizing from the RTL macros is absent.
- Generated AXI parameter sets are absent.
- Current coordinate widths are generic model widths, not frozen generated
  package widths.
- `req` and `rsp` are identifiers, not yet separate modeled networks.

### 13.2b FIFO coverage boundaries

- Only depths 2 and 4 are cross-checked; other depths share the model code but
  have no trace evidence.
- `usage_o`, `flush_i`, and `testmode_i` are neither modeled nor compared.
- The cross-check uses a flat 64-bit data type, not a packed flit struct, so
  field packing is not exercised there.

### 13.3 Router completeness

- No virtual channels.
- No credit protocol.
- No output FIFO.
- No explicit VC arbiter.
- No collectives/reduction path.
- No multicast handshake history.
- No configurable route table.
- No source routing or YX routing.
- No explicit link cut/pipeline model.
- No full RTL assertions mirrored in SystemC.

### 13.4 AXI/network-interface completeness

Modeled but **not RTL cross-checked** (see section 10.7):

- system-address-map and address-offset destination decode;
- AW/W destination coupling through the latched AW id;
- AW followed by W packet route continuity via `hdr.last`;
- AW, W, AR request packing and B/R response packing;
- the AW/W channel-select FSM;
- restoring the manager's AXI id into responses.

Still not implemented at all:

- request and response flit arbitration onto the physical channels;
- metadata buffering (`floo_meta_buffer.sv`);
- downstream AXI ID management;
- same-ID response ordering and RoB behaviour;
- burst semantics beyond `w.last`;
- outstanding transaction accounting;
- ATOP behaviour beyond carrying the header flag.

### 13.5 Verification gaps

- No randomized ready/valid stress.
- No scoreboarding across multiple sources/destinations.
- No deadlock/livelock tests.
- No throughput/latency regression thresholds.
- No AXI protocol checker.
- No CDC-VP end-to-end test.

### 13.6 Instrumentation gaps

`noc_counters.hpp` covers the router boundary. Implemented and measured:

- accepted flits and packets per input and per output;
- stall and busy cycles per port;
- input buffer occupancy high-water and sum.

Still absent, and deliberately so, because each needs a path that is not yet
RTL-signed or not yet modeled:

- transaction and flit latency, and hop count: need per-flit tagging and a
  signed mesh;
- payload bytes per cycle: needs real AXI payload types;
- link utilization: a link is a mesh construct, and the mesh is still
  cycle-approximate;
- outstanding responses and RoB occupancy: need the chimney.

Measured and analytic numbers must remain separate. The header distinguishes
measured counts, derived arithmetic over them, and analytic estimates, and
provides none of the last kind.

Two properties to keep in mind when using the counters:

- flit conservation holds only within a reset-free, fully drained window; a
  reset discards buffered flits;
- the counter block is passive by construction, and the router cross-check runs
  with it attached to keep that true.

### 13.7 Integration risks

- A peripheral-style TLM wrapper would be architecturally wrong.
- Gating the clock before whole-network quiescence can lose traffic.
- Directly translating a blocking TLM transaction into one atomic NoC action
  can hide AXI channel ordering and back-pressure.
- Address ownership and M:N socket topology must be designed with the CDC-VP
  platform bus, not assumed.
- The platform’s temporal decoupling/global quantum may interact with
  cycle-level handshakes and must be defined explicitly.

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

## 14. Required next work, in order

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
`InFifoDepth = 2`, `OutFifoDepth = 0` are signed off; virtual channels, credit
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

Also unverified: chimney *timing* and arbitration in both directions,
multi-beat R bursts, ATOPs, back-pressure on either link, and the
`MaxUniqueIds > 1` metadata path.

### Step 7 — Replace abstract mesh endpoints (next)

Connect the verified AXI chimney/transactors to req/rsp network instances.
Only then claim an end-to-end single-AXI FlooNoC vertical slice.

### Step 8 — Design CDC-VP TLM integration

After standalone AXI traffic passes:

- inspect the CDC-VP bus socket topology;
- define M:N ownership and routing;
- decide whether each endpoint needs target and/or initiator sockets;
- define TLM-to-AXI phase handling and back-pressure;
- define temporal decoupling policy;
- implement whole-network quiescence and safe clock gating;
- add CDC-VP component tests before platform assembly.

---

## 15. Definition of done for the v0 vertical slice

The v0 slice is not complete until all of the following are true:

- every included leaf has a standalone SystemC test;
- FIFO, route selector, arbiter, and router have RTL trace comparisons;
- the router passes routing, contention, back-pressure, and wormhole tests;
- a small req/rsp mesh delivers every flit exactly once;
- AXI AW/W/AR/B/R transactions survive endpoint-to-endpoint conversion;
- ordering rules for the selected configuration are verified;
- measured timing is separated from analytic estimates;
- CDC-VP integration reflects a fabric, not a fake accelerator peripheral;
- the network clock stops only at proven quiescence;
- licensing and provenance are complete.

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
9b. Sample every cross-check trace both pre-edge and post-edge, and prove the
    harness fails on an injected defect before recording a PASS.
9c. Pin every RTL dependency by revision and per-file hash in
    `rtl_crosscheck/fetch_rtl_deps.sh`.
9d. Export a block's internal registers into the trace when they are reachable
    by hierarchical reference; output-only comparison is weaker.
9e. When a negative control unexpectedly passes, decide whether the injection
    was an equivalent rewrite before adding stimulus. Record the reasoning
    either way.
9f. Drive stimulus at `ApplTime` after the clock edge and sample handshakes at
    `TestTime`, never at the edge itself. Driving and sampling at the edge
    races the DUT and silently misses handshakes; the chimney harness deadlocked
    exactly that way.
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
11. Preserve unrelated user changes in the CDC-VP and FlooNoC repositories.
12. Keep FlooNoC-derived license headers and package required licenses.

---

## 17. Recommended immediate continuation prompt

The next AI can be given this task:

> Read `docs/AI_HANDOFF_CONTEXT.md`, then start Step 7: connect the AXI
> endpoint transactors in `include/floo_noc_model/axi_endpoint.hpp` to the
> mesh, replacing its abstract endpoints, so AXI traffic runs end to end
> through the modelled network. Anchor every decision in the FlooNoC IP at
> `/home/duyptt_HW/Documents/work/Study_FlooNoC/FlooNoC` (upstream
> `https://github.com/pulp-platform/FlooNoC.git`).
>
> Two things to settle first. The mesh currently carries one generic `FlitT`
> stream, but AXI needs separate `req` and `rsp` physical channels; decide that
> before wiring anything. And a mesh cross-check needs the FlooGen-generated
> topology, which is absent: `generated/` is gitignored and FlooGen 0.8.4 is
> not installed. Install it from the frozen tree's `pyproject.toml`, not from
> PyPI latest.
>
> Harness discipline, learned the hard way in the chimney work: use the
> synchronous BFM idiom, assert with non-blocking assignments and sample
> handshakes at the clock edge. Do not mix explicit `#delay` phase arithmetic
> with sequential driving; four separate defects in the chimney harnesses came
> from that. Keep the far side of every interface permanently ready, observe
> the DUT with concurrent monitors, and compute expected output counts before
> running a step. See section 16 rules 9f to 9i.
>
> Re-run the full regression and all seven cross-checks after any model change.
> Use the mandated GCC/G++/PATH environment before every build.

If a dependency cannot be resolved without network access or a tool install,
record the precise missing artifact and proceed with other safe, local,
read-only preparation such as the trace schema and harness structure. Do not
mislabel a model-to-model comparison as RTL equivalence.
