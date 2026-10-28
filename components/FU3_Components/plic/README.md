# FU3 AP_PLIC - SystemC/TLM

Functional model of the FU3 platform-level interrupt controller as defined by FTEL_FU3_Common_Spec v2.1 (pulp `rv_plic` `plic_top`):
31 level-triggered interrupt sources, one target (CPU0 = CVA6 `irq_i[0]`, M-mode external, cause 11), priorities 1-31 and claim/complete. See [MODEL_CONTRACT.md](docs/MODEL_CONTRACT.md) for exact behavior and the points the specification leaves open.

It also contains `fu3_plic::Intmap`, the interrupt map of CPU1 (Ibex) from the Common Spec v2.1 Interrupt Mapping (proposed Ibex allocation, decision Q03-b).

## Layout

- `include/fu3_plic/registers.h`: C-compatible offsets, bit helpers and FU3 Source IDs.
- `include/fu3_plic/plic.h`: the `fu3_plic::Plic` model.
- `include/fu3_plic/intmap.h`: the `fu3_plic::Intmap` Ibex interrupt map (Common v2.1).
- `tests/`: direct behavior and actual FU3 bus integration (shared `bench.h`), `run_all.sh`.
- `tests/consumer/`: validation through the installed CMake package.

## Connect to the bus

```cpp
#include <fu3_plic/plic.h>
fu3_plic::Plic plic{"plic"};
bus.target("AP_PLIC").bind(plic.socket);          // PERIBUS_1, 0x4010_0000, local offsets
plic.reset_n(reset_n);
plic.irq_sources[FU3_PLIC_ID_TIMER1 - 1](timer1_irq);   // ID n <- irq_sources[n-1]
// ... bind all 31 sources and tie unused ones to a constant-low signal
plic.irq(plic_irq);                                // -> cva6.set_irq(fu3::kCauseExternal, level)
```

Firmware sequence: write a nonzero priority at `FU3_PLIC_PRIORITY_OFFSET(id)`, set the source bit in ENABLE, set THRESHOLD.
In the handler read CLAIM, service and clear the device, then write the same ID to CLAIM (complete).

## Build and test

Run everything (static/shared builds, both benches, installed consumers):

```bash
tests/run_all.sh        # SYSTEMC_HOME defaults to /usr/local/systemc-2.3.4
```

Or step by step from this directory in Linux/WSL:

```bash
cmake -S . -B build/local -DSYSTEMC_HOME=/path/to/systemc-install -DBUILD_SHARED_LIBS=ON
cmake --build build/local --parallel 4
ctest --test-dir build/local --output-on-failure
cmake --install build/local --prefix "$PWD/build/local/install"
cmake -S tests/consumer -B build/local/consumer -DCMAKE_PREFIX_PATH="$PWD/build/local/install;/path/to/systemc-install"
cmake --build build/local/consumer --parallel 4
ctest --test-dir build/local/consumer --output-on-failure
```

Set `BUILD_TESTING=OFF` for a standalone library build without the sibling bus.
Public target: `fu3::plic`; installed package: `fu3-plic`.

## Verification evidence

See [validation results](docs/VALIDATION.md) and [specification traceability](docs/TRACEABILITY.md).
