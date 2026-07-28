# FlooNoC cycle-level SystemC model

This component is a FlooNoC-native Direction-2 model. The SAURIA/NPU model is
used only as a process and packaging reference; no accelerator-specific
`START`/`DONE`, tensor staging, DMA worker, or register contract is reused.

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
bash rtl_crosscheck/run_route_select_crosscheck.sh   # hw/floo_route_select.sv
bash rtl_crosscheck/run_stream_fifo_crosscheck.sh    # common_cells FIFO wrap
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

The FIFO harness uses no shim at all. `rtl_crosscheck/fetch_rtl_deps.sh` checks
out `common_cells` at the revision locked in the frozen `Bender.lock` and
verifies every compiled file by SHA-256. Router-level cross-checking still
requires the complete Bender dependency tree.

## Current deliverables

- P0: frozen scope, constraints, metrics, and RTL-to-SystemC mapping.
- P1: timing-independent address decode and XY-path reference.
- P2: signal-safe flit/header/coordinate types.
- P3/P4: `stream_fifo_optimal_wrap` mirror, locked XY route selector, wormhole
  arbiter, five-port XY router, and rectangular mesh top, each with a standalone
  SystemC test.
- P7.6 (partial): common SystemC/SV trace harness with a passing 12-cycle
  equivalence check for the XY route selector and a passing 133-cycle
  equivalence check for the input FIFO at depths 2 and 4.

RTL-signed so far: XY route selection with lock state, and the input FIFO wrap.
Everything else is cycle-approximate.

CDC-VP fabric integration is deliberately deferred until the standalone blocks
and router-level model have RTL cross-check coverage.

See `docs/STATUS.md` for verified coverage and the next implementation step.
