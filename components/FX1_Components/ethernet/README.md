# FX1 Ethernet component (FTEL VP v1.0.6 import)

Source: FTEL_Ethernet_MAC_VP_v1.0.6/virtual_platform, imported 2026-11-02.
The import reorganized the delivery and adapted build paths. Plan milestone M1
(2026-11-02) then changed the MAC model for review findings F01 and F04 to F07:
RX FIFO reset mid-frame, absent unicast entries, parameter-dependent reset
values and parameter validation, the MAS CSR decode, and same-edge
hardware/software collisions (review MR01 included). Milestones M2 to M5 wired it
into `platforms/fx1_soc` (CSR 0x1103_0000, PLIC source 6, DMA through a write
guard to DDR3, `--eth-peer`), gave the portable driver a lifecycle
(`eth_cfg_check`, `eth_quiesce`, `eth_restart`, `eth_recover`, `eth_irq_take`,
`eth_*_inflight`) and an FX1 manager (`fw/fx1_soc/drivers/fx1_eth.[ch]`) with
FreeRTOS `eth` commands. The PHY, the legacy firmware and the original vectors
are unchanged. It does not claim full MAS v1.0.7 compliance; see
[docs/MODEL_LIMITATIONS.md](docs/MODEL_LIMITATIONS.md).

## Layout

| Directory | Content |
| --- | --- |
| `include/icsl_eth` | Public MAC, PHY, frame and core interfaces |
| `src` | MAC/DMA/datapath, CSR table and frame core |
| `driver/include`, `driver/src` | Portable C99 driver and register definitions |
| `tests` | MAC, core, scenario and driver tests |
| `tests/support/include/vp` | Example/test bus, RAM, UART and simulation helpers |
| `examples/eth_soc` | Two-node host example |
| `examples/cdc_vp` | Optional legacy Bremen RV32 example; not the FX1 VP++ integration |
| `fw/eth_bringup` | Legacy example firmware |
| `vectors` | Original frame and scenario regression inputs |
| `scripts` | SystemC installer and regression helper |
| `docs/upstream` | Original release README, validation, hashes and replaced build files |
| `docs/source-map.json` | Every original file, source hash and relocated destination |

## Build and test on AlmaLinux

Run from this directory, using system GCC rather than an EDA compiler wrapper.
`PATH` matters as much as `CC`/`CXX`: g++ finds `as` through `PATH`, and an EDA
`as` wrapper earlier in `PATH` can recurse without end.

```bash
export CC=/usr/bin/gcc
export CXX=/usr/bin/g++
export PATH=/usr/bin:/bin:$PATH
$CC -dumpfullversion
$CXX --version | head
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DSYSTEMC_HOME=/opt/systemc-2.3.4
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure --no-tests=error
build/examples/eth_soc/vp_eth_echo --frames 64
```

Alternatively: `make test` or
`SYSTEMC_HOME=/opt/systemc-2.3.4 bash scripts/run_regression.sh /tmp/eth-build`.
Build directories are caller-owned; the Makefile does not delete them.

Seven original host CTest cases cover MAC/DMA/CSR, frame vectors, scenarios,
driver RX, 100/10 Mbps echo and a 2,000-frame burst. Twenty-three more (label
`m1`) cover the M1 findings and the DMA bus contract:
- `tests/test_eth_mac_m1.cpp`: the MAC through its sockets.
- `tests/test_eth_m1_units.cpp`: the same-edge rules of `CsrFile` and
  `core::Rmon` on their own.

`ctest -L m1` runs them alone. Five more (label `m3`,
`tests/test_eth_drv_lifecycle.cpp`) run the driver lifecycle on the MAC model:
configuration check, quiesce/restart, recovery, write-backs in flight, the
ISR split and MDIO. Inside CDC-VP every test also carries `fx1;fx1_unit` and
is required by the FX1 gate (`platforms/fx1_soc/ci/required_tests.txt`). The `eth_mac_tlm_unit` expectations for CSR holes
and unaligned offsets changed with F06 (MAS Table 6-47 rows 1, 2, 9). The original `VALIDATION.txt` is a
vendor report, not evidence of FX1 integration. The optional RV32/Docker path
has not been executed during this import; it requires external dependencies.

## Consuming the component

The directory supports `add_subdirectory()`. Link `eth_mac_tlm` (alias
`cdc::components::fx1_ethernet`) and `eth_phy_tlm`; `eth_drv` is the host-built
reference driver. Firmware must compile `driver/src/eth_drv.c` with its own
cross compiler. A library-only build sets `ICSL_ETH_VP_TESTS=OFF` and
`FX1_ETH_BUILD_EXAMPLES=OFF`. `ICSL_ETH_VP_CDC` is OFF by default.
The parent build must enable C and C++; an existing `SystemC::systemc` is reused.
Headers retain the `icsl_eth` namespace and original filenames.

`EthMacParams::validate()` returns why a parameter set is illegal (MAS Table
5-1); the constructor reports it as `SC_FATAL` before any port exists.

Ports: `csr` receives local register offsets below `csr_window` (64 KiB by
default; address bits [1:0] are ignored and a hole answers SLVERR as
`TLM_GENERIC_ERROR_RESPONSE`);
`axi` accesses descriptor/payload memory; `phy_tx`, `phy_rx` carry complete
frames; `mdio` carries Clause 22 register transactions; `irq` is active-high
level; `rst_n` and `rmii_rst_n` are active-low resets. `EthMacParams::dram_base`
is added to DMA offsets. See `examples/eth_soc/eth_node.h` for original wiring.

This is a functional, loosely timed VP. RMII pin cycles, electrical MDIO,
CDC and exact FIFO/bus timing are not modeled. The FX1 review must resolve
address translation, bus/PLIC ownership, atomic write guards, lifecycle and
FreeRTOS driver contracts before integration. Review plan location:
`/home/duyptt_HW/Documents/work/VP_BK/Ethernet/plan/ETHERNET_REVIEW_PLAN.md`.
