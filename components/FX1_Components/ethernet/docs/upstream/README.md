# Ethernet MAC Virtual Prototype v1.0.6

Delivery companion to Ethernet MAC v1.0.6 MAS and CSR documents.
Release date: 25 October 2026.

A loosely timed SystemC/TLM-2.0 model of a 10/100 Mbps Ethernet MAC, with
frame-level PHY, DMA, register access, interrupts, MDIO, checksum offload and
RMON counters. Includes a C99 driver, a two-node host example, an optional
RV32 firmware platform, and fixed reference vectors for offline regression.

## Build and run

Requires CMake 3.21+, a C++17/C99 compiler, and SystemC 2.3.4 built with a
compatible C++ ABI. Ninja is used below. Linux and Windows with MinGW-w64
are supported by the host build. Python is not required.

Run these commands from the directory containing `virtual_platform/`:

```bash
bash virtual_platform/scripts/install_systemc.sh "$HOME/opt/systemc-2.3.4"
export SYSTEMC_HOME="$HOME/opt/systemc-2.3.4"
cmake -S virtual_platform -B build -G Ninja -DSYSTEMC_HOME="$SYSTEMC_HOME"
cmake --build build
ctest --test-dir build --output-on-failure
```

The installer requires internet access to download Accellera SystemC. Skip
it if SystemC is already installed. Subsequent host builds and tests work
offline with the included sources and vectors.

On Windows PowerShell, set the installation prefix using
`$env:SYSTEMC_HOME = 'C:/path/to/systemc-2.3.4'` and pass
`-DSYSTEMC_HOME="$env:SYSTEMC_HOME"` to CMake. Use a MinGW-w64 compiler
matching the SystemC installation. The `.sh` helpers require Bash.

```bash
build/platforms/eth_soc/vp_eth_echo --frames 64
build/platforms/eth_soc/vp_eth_echo --frames 26 --10m
build/platforms/eth_soc/vp_eth_echo --burst 2000
build/platforms/eth_soc/vp_eth_echo --frames 4 --trace
```

Executables have a `.exe` suffix on Windows. A combined build-and-test helper
is available as `bash virtual_platform/scripts/run_regression.sh`.

## Package contents

| Path | Contents |
|---|---|
| `components/eth_mac_tlm` | MAC model, frame functions, register table and replay tests |
| `components/eth_phy_tlm` | Clause 22 PHY, loopback and link partner |
| `components/common` | Bus router, memory, UART and simulation control |
| `sw` | C99 driver and register definitions |
| `platforms/eth_soc` | Two-node Ethernet example |
| `fw/eth_bringup` | RV32 bring-up firmware |
| `cdc_vp` | Optional RV32 platform and Docker build helper |
| `vectors` | 3,069 frame records (3,073 checks) and 200 full-duplex scenarios |
| `tests` | Driver and platform regressions |
| `scripts` | SystemC installer and regression helper |

## Integration

Link `eth_mac_tlm` and bind the `icsl_eth::EthMacTlm` ports:

| Port | Interface |
|---|---|
| `csr` | 32-bit TLM target; local register offsets in a 64 KiB window |
| `axi` | TLM initiator for descriptor and payload memory access |
| `phy_tx`, `phy_rx` | Frame-level TLM transport, transmit and receive |
| `mdio` | Clause 22 management transport |
| `irq` | Active-high level interrupt |
| `rst_n`, `rmii_rst_n` | Active-low system and interface resets |

Use `EthPhyTlm` for the PHY and `EthLinkPartner` for scripted network traffic.
`EthFrameExt` carries line rate, receive error, dribble and preamble metadata.
Frame bytes run from destination address through FCS; preamble and SFD are
represented separately. See `platforms/eth_soc/eth_node.h` for complete wiring.

`EthMacParams` controls clock period, CSR latency, bus width, maximum burst
length, FIFO depths, frame size, unicast entries, MDIO divider, polling and
DRAM base. Defaults use a 10 ns system clock and two-cycle CSR latency.
Memory transactions split at buffer and 4 KiB boundaries. A descriptor is
16 bytes; descriptor status write-back updates the first word.

Register transactions require four bytes at a four-byte aligned offset.
Byte enables are honored. Invalid addresses return ADDRESS_ERROR; unsupported
sizes return GENERIC_ERROR. DMI is unavailable for registers; debug access
is read-only and does not clear counters. Writes outside a permitted write
window return an error. Interrupt status is registered with one-clock latency.

The driver routes all memory and register accesses through `eth_hal_t`, so
it can run on the host example or as freestanding firmware. Publish descriptor
ownership last, acknowledge interrupt sources, and ring the receive doorbell
after returning a descriptor. `TX_BUF_UNAVAIL` remains asserted while transmit
is suspended; enabling it continuously can keep the interrupt asserted. DMA
errors must be cleared or masked before completing interrupt service.

## Optional RV32 platform

```bash
bash virtual_platform/cdc_vp/run_in_docker.sh "unit|scenario|platform|riscv"
```

Requires Docker and internet access. The helper downloads the public CDC-VP
and Bremen riscv-vp dependencies at the commits pinned in the script, builds
the firmware, and runs the regression. Dependency source and licenses are
provided by their respective upstream projects and are not bundled here.

For an existing Linux dependency checkout, configure with
`-DICSL_ETH_VP_CDC=ON -DCDC_VP_ROOT=/path/to/CDC-VP`. The platform also needs
Boost and a RISC-V bare-metal GCC toolchain. Firmware success is reported as
`FW: ALL PASS`.

## Model scope

This model supports software development and transaction-level integration.
It does not model individual RMII di-bits, MDIO electrical timing, every bus
handshake cycle, false-carrier detection, or the interface bit-order options.
Frames arrive as complete frame transactions; FIFO timing is approximate.
EEE and PAUSE are unsupported. Destination addresses shorter than six bytes
are outside the address-filter model. Software must not rely on received
`LENGTH` after `RX_DATA_WR_ERR`. Reset cancels in-flight activity according to
the selected reset group.

## Regression

CTest covers register and DMA behavior, reset cancellation, write strobes,
write windows, frame checksums and framing, full-duplex scenarios, driver
receive behavior, 100/10 Mbps echo and a 2,000-frame line-rate burst.
Reference inputs and expected results are bundled under `vectors/`; no
external test generator is needed. Optional RV32 testing adds PHY management,
interrupt-driven loopback, a memory error and recovery.

See `VALIDATION.txt` for the checks performed on this delivery.
