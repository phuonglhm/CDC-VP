# FlooNoC cycle-level SystemC model

This component is a FlooNoC-native Direction-2 model. The SAURIA/NPU model is
used only as a process and packaging reference; no accelerator-specific
`START`/`DONE`, tensor staging, DMA worker, or register contract is reused.

The reference architecture is the FlooNoC IP implementation itself, upstream at
`https://github.com/pulp-platform/FlooNoC.git` and frozen locally at the
revision recorded in `docs/P0_SCOPE.md`. Every modelling decision must be
traceable to a file in that tree.

The source of truth is, in order:

1. FlooNoC RTL at the frozen revision recorded in `docs/P0_SCOPE.md`.
2. FlooGen configuration and generated topology.
3. FlooNoC RTL testbenches and assertions.
4. FlooNoC documentation for architectural intent.

The model is developed bottom-up. The initial vertical slice intentionally
covers only single-AXI, deterministic XY-routed, unicast, ready/valid traffic.
Unsupported FlooNoC features are listed explicitly in `docs/P0_SCOPE.md`.

## Standalone build

The default compiler is `/usr/bin/g++`; this avoids a broken Synopsys compiler
wrapper that can appear first in `PATH` on the development host.

```bash
make test
```

The Makefile exports the required build environment and prints compiler sanity:

```bash
export CC=/usr/bin/gcc
export CXX=/usr/bin/g++
export PATH=/usr/bin:/bin:$PATH
$CC -dumpfullversion
$CXX --version | head
```

Equivalent CMake commands:

```bash
cmake -S . -B build \
  -DCMAKE_CXX_COMPILER=/usr/bin/g++ \
  -DSYSTEMC_HOME=/opt/systemc-2.3.4
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

## RTL cross-check

Each harness runs one shared CSV stimulus through both the SystemC model and the
unmodified frozen RTL, then compares the cycle traces exactly.

```bash
bash rtl_crosscheck/run_route_select_crosscheck.sh       # hw/floo_route_select.sv
bash rtl_crosscheck/run_stream_fifo_crosscheck.sh        # common_cells FIFO wrap
bash rtl_crosscheck/run_wormhole_arbiter_crosscheck.sh   # hw/floo_wormhole_arbiter.sv
bash rtl_crosscheck/run_router_crosscheck.sh             # hw/floo_router.sv
bash rtl_crosscheck/run_axi_sizing_crosscheck.sh         # floo_pkg flit sizing
bash rtl_crosscheck/run_chimney_req_crosscheck.sh        # chimney request content
bash rtl_crosscheck/run_chimney_rsp_crosscheck.sh        # chimney response content
bash rtl_crosscheck/run_rob_crosscheck.sh                # hw/floo_rob_wrapper.sv, NoRoB
bash rtl_crosscheck/run_chimney_timing_crosscheck.sh     # chimney request timing
bash rtl_crosscheck/run_chimney_rsp_timing_crosscheck.sh # chimney subordinate side
bash rtl_crosscheck/run_chimney_mgr_rsp_crosscheck.sh    # chimney manager response
bash rtl_crosscheck/run_mesh_crosscheck.sh               # a grid of floo_axi_router
```

Twelve runners. The twelfth,
`run_chimney_mgr_rsp_crosscheck.sh`, closed the chimney at Step A-1: it drives
`floo_rsp_i`, which the other three chimney runners pin low, and compares the
manager's B/R channels, `floo_rsp_o.ready` and both reorder-buffer counters for
97 cycles.

The tests are also expected to be seen failing. `run_negative_controls.sh`
re-injects each defect the model-level tests exist to catch and requires every
one of them to be detected:

```bash
bash rtl_crosscheck/run_negative_controls.sh
```

Both default to the frozen local FlooNoC tree and write generated files under
`/tmp`. Override the locations when needed:

```bash
FLOONOC_RTL_ROOT=/path/to/FlooNoC \
FLOO_RTL_DEPS_ROOT=/path/to/deps \
BUILD_ROOT=/tmp/my_floo_crosscheck \
bash rtl_crosscheck/run_route_select_crosscheck.sh
```

The route-selector harness supplies only a minimal `floo_pkg`/`common_cells`
compile shim; route computation and locking are compiled from the original
FlooNoC RTL, and the runner rejects an RTL file whose SHA-256 does not match
frozen revision `9a6972a`.

### Full RTL compile flow

Leaf cross-checks pin single dependencies themselves. For anything that needs
the whole transitive tree, such as the upcoming router cross-check, generate an
ordered file list with Bender:

```bash
bash rtl_crosscheck/gen_rtl_filelist.sh
```

It refuses to run unless Bender is the pinned version, the FlooNoC tree is at
the frozen revision and clean, and `Bender.lock` hashes identically before and
after. It emits Verilator, VCS, and generic file lists plus the resolved
dependency table, and verifies that `floo_router.sv` elaborates from the result.
If Bender is missing, the script prints the exact pinned-artifact install
commands.

The FIFO and arbiter harnesses use no behavioural shim.
`rtl_crosscheck/fetch_rtl_deps.sh` checks out `common_cells` at the revision
locked in the frozen `Bender.lock` and verifies every compiled file by SHA-256.
The arbiter harness compiles against an intentionally empty `floo_pkg`, which
proves the arbiter references no symbol from that package. Router-level
cross-checking still requires the complete Bender dependency tree.

## Current deliverables

- P0: frozen scope, constraints, metrics, and RTL-to-SystemC mapping.
- P1: timing-independent address decode and XY-path reference.
- P2: signal-safe flit/header/coordinate types.
- P3/P4: `stream_fifo_optimal_wrap` mirror, locked XY route selector, wormhole
  arbiter over an `rr_arb_tree` mirror, five-port XY router, and rectangular
  mesh top, each with a standalone SystemC test.
- P7.6 (partial): common SystemC/SV trace harness with passing equivalence
  checks of 12 cycles for the XY route selector, 133 cycles for the input FIFO
  at depths 2 and 4, and 152 cycles for the wormhole arbiter at 5, 4, and 2
  routes, and 214 cycles for the five-port router; plus a reproducible
  full-tree RTL compile flow.

- P5: measured router counters, observing only RTL-signed signals and driving
  nothing.
- P6: single-AXI channel types and sizing, chimney flit assembly and
  destination decode, metadata retention, and abstract AXI endpoint
  transactors.
- P7: the `NoRoB` ordering rule, cycle cross-checked against the unmodified
  reorder-buffer wrapper over the locked `axi_demux_id_counters` (127 cycles,
  twelve negative controls all detected).
- P8: the chimney request path composed at cycle granularity and cross-checked
  under AW/W/AR contention and link back-pressure (141 cycles, eleven negative
  controls all detected).
- P9: separate `req` and `rsp` meshes, matching `floo_axi_router`'s two-router
  structure, with the AXI transactors attached and AXI running end to end
  across a 4x4 mesh.
- P9.1: the chimney's response path and subordinate side cross-checked for
  timing (221 cycles, eleven negative controls all detected).
- P9.2: inter-node timing cross-checked against a grid of the real router
  (1872 node-cycles). It found the model missing the output FIFO that every
  generated FlooNoC router has, which cost one cycle per hop.

RTL-signed so far: XY route selection with lock state, the input FIFO wrap, the
wormhole arbiter, the five-port router at both output-FIFO depths, the AXI flit
sizing, both chimney flit *content* paths, the `NoRoB` ordering rule, the chimney's
**request-path timing** (141 cycles) and its **subordinate side** (221 cycles),
and **inter-node mesh timing** (1872 node-cycles).

The **manager-side response unpacker** (`axi_chimney_manager_response`) is
signed too, 97 cycles, which completes all four chimney quadrants.

Still not signed: the composed **manager-AXI-to-subordinate-AXI** path. Every
block on it is signed individually, but `noc_interconnect` does not yet
instantiate the timed chimney — Steps A-2 and A-3.

Each of those is signed in isolation. That is deliberately not a claim that the
integrated path is signed end to end: the timed chimney (`axi_chimney.hpp`) is
instantiated only by its two trace runners, while `noc_interconnect` composes
the combinational `axi_chimney_pack.hpp` with the `axi_endpoint.hpp`
transactors. Those transactors and the TLM wrapper have no RTL counterpart and
cannot be signed against one — they are a driver and collector built on signed
rules.

The router harness uses no shim at all: it compiles the real RTL from the
Bender-generated file list and keeps the router's own protocol assertions
enabled.

CDC-VP integration is in `include/floo_noc_model/noc_interconnect.h`, a TLM-2.0
wrapper presenting the same interface as `cdc::components::bus_router` plus mesh
placement, and `platforms/noc_soc`, a small SoC that uses it. Both are separate
from the header-only model so a standalone user pays for neither.

See `docs/STATUS.md` for verified coverage and the next implementation step.
