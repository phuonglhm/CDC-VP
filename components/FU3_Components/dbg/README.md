# FU3 DBG - SystemC/TLM

Functional model of the FU3 debug subsystem, phase 1: JTAG DTM (IDCODE, dtmcs, dmi) at scan level, a RISC-V Debug Spec 0.13.2 Debug Module for two harts (halt/resume, abstract Access Register, ndmreset, haltsum) and system bus access through the DBG master port of the FU3 bus.
Behavior follows FU3 HAS v2.0 and the RISC-V debug specification.
See [MODEL_CONTRACT.md](docs/MODEL_CONTRACT.md) for exact behavior and the open points.

## Layout

- `include/fu3_dbg/registers.h`: JTAG IR, DMI addresses and fields.
- `include/fu3_dbg/dm.h`: `fu3_dbg::DebugModule` and the `fu3_dbg::DebugHart` interface.
- `include/fu3_dbg/dtm.h`: `fu3_dbg::Dtm`.
- `tests/`: direct behavior and actual FU3 bus integration (shared `bench.h`), `run_all.sh`.
- `tests/consumer/`: validation through the installed CMake package.

## Connect to the platform

```cpp
#include <fu3_dbg/dtm.h>
fu3_dbg::DebugModule dm{"dm", {&cva6_debug, &ibex_debug}};   // DebugHart adapters (phase 2)
fu3_dbg::Dtm dtm{dm};
dm.sba.bind(bus.initiator("DBG"));                            // SYSBUS_1, 32-bit beats
dm.ndmreset(ndmreset);                                        // -> crm.dbg_reset_req
// A JTAG front end (test, remote bitbang server, ...) calls dtm.scan_ir() / dtm.scan_dr() from an SC_THREAD
```

## Build and test

Run everything (static/shared builds, both benches, installed consumers):

```bash
tests/run_all.sh        # SYSTEMC_HOME defaults to /usr/local/systemc-2.3.4
```

The library uses the header-only `fu3_bus/access.h` from the sibling `bus` directory at build time.
Set `BUILD_TESTING=OFF` for a standalone library build without the bus tests.
Public target: `fu3::dbg` and installed package: `fu3-dbg`.

## Verification evidence

See [validation results](docs/VALIDATION.md) and [specification traceability](docs/TRACEABILITY.md).
