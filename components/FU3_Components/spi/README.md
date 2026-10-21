# FU3 SPI0/1/2/3 - SystemC/TLM

Common Spec v2.0 takes precedence over older endpoint restrictions. See the
[updated model contract](docs/MODEL_CONTRACT.md) for byte strobes and remaining VP decisions.

Functional SPI controller for VP register, serial-data, interrupt and DMA-flow
verification, based on SPI m_spi_top HDS V1.0. The model provides master and slave
operation, four clock modes, 4..32-bit frames, FIFO buffering, automatic/manual
chip select, sticky errors, interrupt thresholds and DMA request/acknowledge.

`fu3_spi::Spi` is reusable; `SpiBank` creates four independent controllers. It
models actual serial bit values on SystemC signals, with TLM register access.
It does not reproduce every RTL pipeline or CDC synchronizer cycle. See the
[model contract](docs/MODEL_CONTRACT.md) for precise scope and adapter choices.

| Instance | Base | Bus | PLIC source |
|---|---|---|---|
| SPI0 | 0x440C0000 | PERIBUS_0 | 9 |
| SPI1 | 0x440B0000 | PERIBUS_0 | 16 |
| SPI2 | 0x50090000 | PERIBUS_1 | 22 |
| SPI3 | 0x500C0000 | PERIBUS_1 | 23 |

## Integration

```cpp
#include <fu3_spi/spi_bank.h>
fu3_spi::SpiBank spi{"spi"};
for (unsigned i = 0; i < 4; ++i)
    bus.target("SPI" + std::to_string(i)).bind(spi.controllers[i]->socket);
```

Bind all ports of each controller before simulation, including unused inputs and
outputs. Inputs: `reset_n`, `apb_reset_n`, `sck_in`, `ss_n_in`, `mosi_in`, `miso_in`,
`dma_tx_ack`, `dma_rx_ack`. Outputs: `sck`, `ss_n` (four active-low selects), `mosi`,
`miso`, the four output enables, `irq`, `dma_tx_req` and `dma_rx_req`. Unused slave
select input should be high; unused acknowledge inputs low. A platform pad mux
must honor output enables. The component does not implement IOSEL or PLIC.

The bank accepts FIFO depth 16/32/64 and four system periods. A standalone `Spi`
also accepts its APB period. Defaults are 16 entries and 10 ns per period; they
are VP parameters, not asserted chip frequencies. Disable absent bus targets
before constructing BusSystem. See [integration.cpp](tests/integration.cpp) for
complete wiring through the real FU3 bus, fake IRQ routes and a DMA stand-in.

Firmware configures DIV, CS, DLY and CTRL while disabled, then enables the block
and writes one word to TXDR per frame. Drain RXDR to prevent master backpressure.
Clear sticky ISR bits by writing ones with lane zero enabled. TXDR pushes one masked, zero-filled word for nonzero write strobes and does
not push for zero strobes. RXDR still pops, including debug reads. The public
[register header](include/fu3_spi/registers.h) is usable from C firmware.

## Build and validate

Run from this directory in Linux/WSL with compatible C++17 and SystemC 2.3.4:

```bash
set -e
export SYSTEMC_HOME=/absolute/path/to/systemc-install
for mode in static shared; do
    shared=OFF
    if [ "$mode" = shared ]; then shared=ON; fi
    cmake -S . -B "build/$mode" -DSYSTEMC_HOME="$SYSTEMC_HOME" -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS="$shared"
    cmake --build "build/$mode" --parallel 4
    ctest --test-dir "build/$mode" --output-on-failure
    cmake --install "build/$mode" --prefix "$PWD/build/$mode/install"
    cmake -S tests/consumer -B "build/$mode/consumer" -DCMAKE_PREFIX_PATH="$PWD/build/$mode/install;$SYSTEMC_HOME"
    cmake --build "build/$mode/consumer" --parallel 4
    ctest --test-dir "build/$mode/consumer" --output-on-failure
done
```

Each component build runs 18 tests, including named corner scenarios in
`tests/corners.cpp`; each installed consumer build runs eight tests. Corner tests
cover slave formats, concurrent FIFO flush, event priority, active-state abort/reset,
DMA boundaries, functional clock-ratio profiles and extreme master delays.
The integration test also exercises concurrent CPU/DMA-stand-in FIFO writes while
another SPI progresses through the other peripheral bridge.

Installed consumers use `find_package(fu3-spi CONFIG REQUIRED)` and link
`fu3::spi`. SystemC is an exported PUBLIC dependency. Applications define
`sc_main`; the library has no entry point. The production library does not depend
on the FU3 bus; only its integration tests do. Private PDFs, extracted text and
generated build outputs are not distributed with this component.

See [traceability](docs/TRACEABILITY.md) and [validation](docs/VALIDATION.md).
