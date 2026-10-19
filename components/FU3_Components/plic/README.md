# FU3 AP_PLIC - SystemC/TLM

Functional model of the FU3 platform-level interrupt controller (`ftel_plic`):
32 interrupt sources, one target (CVA6 `irq_i[0]`, M-mode external, cause 11), 8 priority levels, per-source level/edge gateways and claim/complete. See [MODEL_CONTRACT.md](docs/MODEL_CONTRACT.md) for exact behavior and the points the HDS leaves open.

## Layout

- `include/fu3_plic/registers.h`: C-compatible offsets, bit helpers and FU3 Source IDs.
- `include/fu3_plic/plic.h`: the `fu3_plic::Plic` model.
- `include/fu3_plic/intmap.h`: the `fu3_plic::Intmap` Ibex interrupt map (HAS v2.0 10.2).
- `tests/`: direct behavior and actual FU3 bus integration (shared `bench.h`), `run_all.sh`.
- `tests/consumer/`: validation through the installed CMake package.

## Connect to the bus

```cpp
#include <fu3_plic/plic.h>
fu3_plic::Plic plic{"plic"};
bus.target("AP_PLIC").bind(plic.socket);          // PERIBUS_1, 0x4010_0000, local offsets
plic.reset_n(reset_n);
plic.irq_sources[FU3_PLIC_ID_TIMER1 - 1](timer1_irq);   // ID n <- irq_sources[n-1]
// ... bind all 32 sources and tie unused ones to a constant-low signal
plic.irq(plic_irq);                                // -> cva6.set_irq(fu3::kCauseExternal, level)
```

Firmware sequence: set PRIORITY (nonzero) and EL per source, set IE_0, set THRESHOLD_0.
In the handler read ID_0 (claim), service the device, write same ID to ID_0 (complete).

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

It also contains `fu3_plic::Intmap`, the stateless Ibex interrupt map from FU3 HAS v2.0 section 10.2.