# FlooNoC detailed and approximately-timed SystemC model

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

## Installed package

The compiled wrapper is exported as
`cdc::components::noc_interconnect`; `cdc::components::floo_noc_model` is the
header-only target beneath it. Step 10.4 verifies the compiled target from an
independent source tree:

```bash
export CC=/usr/bin/gcc
export CXX=/usr/bin/g++
export PATH=/usr/bin:/bin:$PATH
$CC -dumpfullversion
$CXX --version | head

cd /home/duyptt_HW/Desktop/VP_INTER/upgit/CDC-VP
cmake -S . -B /tmp/cdc-vp-install-build \
  -DCMAKE_BUILD_TYPE=Release \
  -DCDC_BUILD_NOC_SOC=ON \
  -DCDC_BUILD_TESTS=OFF \
  -DCDC_BUILD_MINI_TLM=OFF \
  -DCDC_BUILD_CPU_EVAL=ON \
  -DCDC_BUILD_CUSTOM_SOC=OFF
cmake --build /tmp/cdc-vp-install-build --parallel
cmake --install /tmp/cdc-vp-install-build \
  --prefix /tmp/cdc-vp-install-prefix

cmake -S components/floo_noc_model/tests/installed_consumer \
  -B /tmp/floo-installed-consumer \
  -DCMAKE_PREFIX_PATH=/tmp/cdc-vp-install-prefix \
  -DSYSTEMC_HOME=/opt/systemc-2.3.4
cmake --build /tmp/floo-installed-consumer --parallel
/tmp/floo-installed-consumer/floo_noc_installed_consumer
```

The prefix includes the Apache-2.0 and SHL-0.51 texts, CDC-VP NOTICE and
`PROVENANCE.md` under
`share/licenses/cdc-components/floo_noc_model`.

The commands above are useful for inspecting one stage manually. The complete
Step 10.4 gate is automated and registered with the parent CTest suite:

```bash
cd /home/duyptt_HW/Desktop/VP_INTER/upgit/CDC-VP
./platforms/noc_soc/tests/run_packaging_regression.sh
```

It rebuilds into a private temporary directory, runs this installed consumer,
checks the installed header manifest/SPDX identifiers and byte-compares every
development-package licence/provenance record. It then continues with the
binary-package checks documented by `platforms/noc_soc`.

## Wrapper concurrency policy

Step 10.5 keeps the frozen FlooNoC `MaxUniqueIds = 1` configuration. No current
SoC requirement needs out-of-order downstream response matching, so the
unverified `id_queue` branch is not enabled.

`noc_interconnect` nevertheless supports concurrent `b_transport` calls through
one upstream port in detailed mode. The outstanding bound defaults to 32,
matching `ChimneyDefaultCfg.MaxTxns`:

```cpp
cdc::components::noc_interconnect noc{
    "noc", 4, 4, num_targets, num_initiators,
    sc_core::sc_time(1, sc_core::SC_NS), 32,
    cdc::components::noc_interconnect::timing_mode::detailed};
```

The bound must be 1..32. Calls beyond it wait for a slot. Requests and
completions use independent FIFO queues for reads and writes, so the constant
downstream ID remains sufficient as long as the frozen FIFO-order contract is
preserved. `outstanding_transactions(port)` and
`peak_outstanding_transactions(port)` expose admission diagnostics.

## Timing backends

Step 11 keeps one socket/address-map class and selects its timing backend at
construction:

```cpp
cdc::components::noc_interconnect fast_noc{
    "noc", 4, 4, num_targets, num_initiators,
    sc_core::sc_time(1, sc_core::SC_NS),
    cdc::components::noc_interconnect::default_max_outstanding_per_port,
    cdc::components::noc_interconnect::timing_mode::fast};
```

- `detailed` is the default and the calibration reference. It drives the
  chimney/mesh signals cycle by cycle, spends incoming and network time, and
  returns `delay == SC_ZERO_TIME`.
- `fast` uses the same validation, address decode, AXI lane/byte-enable shaping,
  mapped target and response mapping. The interconnect does not tick the mesh
  or wait; it preserves incoming local time and returns target plus estimated
  network time in `delay`. Downstream targets used with this backend must also
  annotate latency rather than call `wait()` in their own `b_transport`.

The fast no-contention estimate is:

```text
read  cycles = 4 * Manhattan_hops + 6 + (beats - 1)
write cycles = 4 * Manhattan_hops + 6 + beats
```

Target delay is conservatively rounded up to the next network clock exactly as
in detailed mode. The fixed/hop terms are calibrated to the detailed 10-cycle
one-hop and 30-cycle six-hop read baselines. `test_noc_interconnect_fast`
compares both backends over 1..6 hops, 1/2/4/8-byte accesses and 32-byte bursts
with a hard one-cycle tolerance.

Fast mode does **not** estimate contention, link back-pressure, router locks,
metadata occupancy or clock-gating activity. Blocking calls retain invocation
order and functional target effects, but use detailed mode for any congestion
or cycle-accuracy question.

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
`floo_rsp_i`, which the other four chimney runners pin low, and compares the
manager's B/R channels, `floo_rsp_o.ready` and both reorder-buffer counters for
78 cycles.

The tests are also expected to be seen failing. `run_negative_controls.sh`
re-injects each defect the model-level tests exist to catch and requires every
one of them to be detected. The current gate detects all 39 mutations, including
Step 10.5 controls for same-port capacity/ownership and Step 11 controls for
hop cost, incoming-delay preservation, target rounding and functional replay:

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
- Step 10.3: bounded deterministic-random stress of the unsigned TLM adapters
  with three concurrent initiators, per-requester scoreboards and timeouts.
  Router/mesh/chimney occupancy and packet locks now form an explicit
  `mesh_quiescent()` predicate; the network clock stops only when both wrapper
  bookkeeping and the complete signal-driven NoC are idle.

RTL-signed so far: XY route selection with lock state, the input FIFO wrap, the
wormhole arbiter, the five-port router at both output-FIFO depths, the AXI flit
sizing, both chimney flit *content* paths, the `NoRoB` ordering rule, the chimney's
**request-path timing** (141 cycles) and its **subordinate side** (221 cycles),
and **inter-node mesh timing** (1872 node-cycles).

The **manager-side response unpacker** (`axi_chimney_manager_response`) is
signed too, 78 cycles, which completes all four chimney quadrants.

There is still no one-piece RTL cross-check for the composed
**manager-AXI-to-subordinate-AXI** path. Every block on it is signed
individually, and A-2's `test_axi_noc_chimney` now verifies their SystemC
composition. Since A-3, `noc_interconnect` uses that path and drives AW/W/AR
and B/R handshakes cycle by cycle.

Each of those is signed in isolation. That is deliberately not a claim that the
integrated path is signed end to end: the timed chimney (`axi_chimney.hpp`) is
instantiated by the trace runners and by `axi_noc`, which is now the wrapper's
datapath. The TLM-to-AXI manager/subordinate adapters that remain above those
signal boundaries have no RTL counterpart and cannot be signed against one;
they are covered by model-level integration tests, including the three-manager
`test_noc_interconnect_stress`. The legacy
`axi_endpoint.hpp` transactors remain only for isolated reference tests.

The router harness uses no shim at all: it compiles the real RTL from the
Bender-generated file list and keeps the router's own protocol assertions
enabled.

CDC-VP integration is in `include/floo_noc_model/noc_interconnect.h`, a TLM-2.0
wrapper presenting the same interface as `cdc::components::bus_router` plus mesh
placement, and `platforms/noc_soc`, a small SoC that uses it. Both are separate
from the header-only model so a standalone user pays for neither.

See `docs/STATUS.md` for verified coverage. Step 11 completed the planned
detailed/fast coexistence path. The explicitly authorised post-Step-11
FreeRTOS work is specified in `docs/NOC_SOC_FREERTOS_ROADMAP.md`.
