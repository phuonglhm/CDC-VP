# FU3 I2C behavioral VP

Common Spec v2.0 takes precedence over older endpoint restrictions. See the
[updated model contract](docs/MODEL_CONTRACT.md) for byte strobes and remaining VP decisions.

SystemC/TLM model of the FU3 I2C controller. It supports firmware MMIO, master byte transfers, basic 7-bit slave operation, interrupts, and resolved open-drain serial connections. It is intended for behavioral and data-flow verification, not RTL cycle or electrical qualification.

The model implements the I2C V1.1 interface. See [MODEL_CONTRACT.md](docs/MODEL_CONTRACT.md) for explicit decisions at specification gaps and [TRACEABILITY.md](docs/TRACEABILITY.md) for verification coverage. Private specification files are not part of this component.

## Build and test

Use a compiler and C++ ABI compatible with your installed SystemC library:

```sh
cmake -S components/FU3_Components/i2c -B build/fu3-i2c \
  -DCMAKE_PREFIX_PATH=/path/to/systemc -DCMAKE_BUILD_TYPE=Release
cmake --build build/fu3-i2c --parallel
ctest --test-dir build/fu3-i2c --output-on-failure
cmake --install build/fu3-i2c --prefix /path/to/fu3-i2c-install
```

Set `BUILD_SHARED_LIBS=ON` for a shared library or `BUILD_TESTING=OFF` for a standalone library build. Test builds use the adjacent FU3 bus source. Consumers use `find_package(fu3-i2c CONFIG REQUIRED)` and link `fu3::i2c`. Executables must provide `sc_main`; the reusable library has no entry point.

## Platform connection

Instantiate `fu3_i2c::I2c` and bind its `socket` to `bus.target("I2C")`. The FU3 window starts at `0x50080000`, on PERIBUS_1. The bus supplies local offsets; direct socket callers must also use local offsets. Route `irq` to PLIC source ID 15 (source array index 14) and Ibex fast interrupt index 2 in the platform. No DMA channel is assigned by this component.

Bind `reset_n`, `scl_i`, `sda_i`, `scl_oe_n`, `sda_oe_n`, and `irq`. A false output-enable value drives zero; true releases the line. Resolve all participants with logical AND, including a released-high pull-up, then feed the resulting bus values back to both inputs. Never connect the enable directly to an active-high pad enable without inversion. Output data is always zero and is not a separate port.

`Config::clock_period` defaults to 10 ns and must be set to the platform's HCLK period. Clock gating and runtime frequency changes are not modeled by this interface. Supply the appropriate fixed configuration for each run.

Firmware should use 32-bit full-lane MMIO. Sparse writes and narrow payloads are rejected at the TLM adapter rather than silently becoming full writes. This adapter error is not a claim that the physical IP asserts PSLVERR.

The tests use fake serial peers and driver-style MMIO through the real bus. They do not execute CPU firmware or verify a complete SoC boot.
