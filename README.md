# CDC-VP — Virtual SoC Platform

A SystemC / TLM-2.0 (C++17) **virtual SoC** for bringing up firmware, drivers, and
system architecture early — before silicon. Firmware-driven, with a fixed SoC-level
memory map and IRQ map so firmware, drivers, and TLM IP models develop independently.

- **RISC-V CPU backend:** `riscv_vp` (Bremen RISC-V VP, application-class, boots an RTOS).
- **Infrastructure:** TLM bus router, RAM, CLINT, PLIC.
- **Peripherals:** UART×2, I2C×2, SPI×2, Timer×2, WDT, PWM, DMA, TRNG, DMIC, OTP, QSPI
  (+ NOR flash), CMU, PMU, RTC, ADC.
- **Planned:** media/AI accelerator pipeline `RAW → ISP → VPU → NPU`.

> Full documentation: [`docs/CDC-VP_VIRTUAL_SoC_PLATFORM.md`](docs/CDC-VP_VIRTUAL_SoC_PLATFORM.md)

## Quick start

```bash
source tools/third_party/setup_env.sh        # CC/CXX + RISC-V toolchain
tools/third_party/setup_third_party.sh       # fetch external CPU cores

cmake -S . -B build \
  -DCDC_CPU_BACKEND=riscv_vp \
  -DCDC_BUILD_CUSTOM_SOC=ON -DCDC_BUILD_TESTS=ON
cmake --build build -j"$(nproc)"
ctest --test-dir build --output-on-failure
```

Requires: SystemC 2.3.4 (`/opt/systemc-2.3.4`), a C++17 compiler, CMake, and the
`riscv-none-elf` bare-metal toolchain for firmware.

## Documentation

| Topic | File |
|---|---|
| Platform overview (full) | `docs/CDC-VP_VIRTUAL_SoC_PLATFORM.md` |
| Memory & IRQ map | `docs/peripheral_memory_map.md` |
| Clock / reset / bus protocol | `docs/clock_reset_and_bus_protocol.md` |
| CPU features & block diagram | `docs/CDC-VP_VIRTUAL_SoC_PLATFORM.md` §7, `docs/cpu_block_diagram.svg` |
| CPU benchmark | `docs/cpu_benchmark_results.md` |
| Block / pipeline diagrams | `docs/virtual_soc_block_diagram.svg`, `docs/virtual_soc_flow_diagrams.svg` |

## License

CDC-VP's own source is licensed under **Apache-2.0** (see [`LICENSE`](LICENSE) and
[`NOTICE`](NOTICE)).

The external CPU core (**Bremen riscv-vp, MIT**) is **not** bundled; it is fetched
into `third_party/` (gitignored). All dependencies are permissive — see
[`THIRD_PARTY.md`](THIRD_PARTY.md).
