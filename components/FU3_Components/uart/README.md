# FU3 UART0/1/2 SystemC/TLM

Reusable UART models for functional serial data flow, register-driven software,
interrupt handling and DMA integration. The model serializes and samples actual
SystemC TX/RX signals; a THR write is not an instantaneous receive injection.
It targets behavior, not cycle-identical RTL or electrical sign-off.

The supplied `FTEL_CDC_IP_UART_Specification.pdf` defines the local IP behavior;
FU3 HAS v2.0 defines the platform windows and routes. Missing or contradictory
requirements are identified in [MODEL_CONTRACT.md](docs/MODEL_CONTRACT.md) and
[REVIEW.md](docs/REVIEW.md). The private PDFs are not distributed with this package.

## Integration

```cpp
#include <fu3_uart/uart_bank.h>
fu3_uart::UartBank uarts{"uarts"};
bus.target("UART0").bind(uarts.controllers[0]->socket);
bus.target("UART1").bind(uarts.controllers[1]->socket);
bus.target("UART2").bind(uarts.controllers[2]->socket);
```

Bind all reset, serial, modem, IRQ and DMA ports. Tie unused active-low modem
inputs high and DMA clear inputs low. Drive RX high when disconnected. UART state
reset is synchronous and must cover at least one configured clock edge; MSR delta
bits clear asynchronously on reset assertion. Use each instance's `Config` to set
its clock period, maximum FIFO depth (32/64), and oversampling (8/16). Firmware
enables automatic flow control through MCR.AFE (bit 5); MCR.RTS (bit 1) also
enables automatic RTS behavior.

| Instance | Base | Bus | PLIC source ID | Ibex route |
|---|---|---|---|---|
| UART0 | 0x440D0000 | PERIBUS_0 | 2 | fast[7] |
| UART1 | 0x440E0000 | PERIBUS_0 | 10 | fast[4], OR with UART2 |
| UART2 | 0x50050000 | PERIBUS_1 | 11 | fast[4], OR with UART1 |

Offsets are **bytes 0..7**, as stated by the HDS programming sequence. Use byte
MMIO through the existing FU3 bus, with `fu3_bus::ScopedAccess(tx,1)` for a TLM
initiator. Do not multiply register offsets by four. A direct four-byte target
access operates on one register using the low data byte; it is not four successive
register operations. Sparse byte enables are rejected because the documented
APB3 endpoint has no PSTRB. See the contract for the explicit adapter boundary.

Configure DLAB, DLL/DLM, FIFO mode while DLAB is set, then clear DLAB and configure
LCR. Set MCR.AFE for auto-CTS; set MCR.RTS with AFE to enable auto-RTS as well.
Poll LSR.THRE/TEMT for TX; poll LSR.DR and read RBR for RX. Public byte offsets
and status masks are in [registers.h](include/fu3_uart/registers.h). Oversampling
8x/16x remains a model configuration because the HDS defines no firmware selector.
RS485 control is pending a register/pin definition; MCR bits 7:6 remain reserved.

See [integration.cpp](tests/integration.cpp) for real bus wiring with fake interrupt
receivers and a DMA initiator. These tests do not execute CPU firmware or prove a
complete boot flow. UART1 DMA RX/TX map to platform channels 2/3, UART2 to 4/5;
the component does not own that routing. UART0 has no HAS DMA channel assignment.

## Build and validate

Requires CMake, a C++17 compiler and a compatible installed SystemC package.
Run from this directory, in Linux/WSL:

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

Consumers call `find_package(fu3-uart CONFIG REQUIRED)` and link `fu3::uart`.
Executables supply `sc_main`. SystemC is a public imported dependency; the
production UART library does not depend on the bus library. Generated builds
and local scripts stay ignored. Native Windows DLL behavior is not established
by WSL testing.
