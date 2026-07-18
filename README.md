# CDC-VP — Virtual SoC Platform

A SystemC / TLM-2.0 (C++17) **virtual SoC** for bringing up firmware, drivers, and
system architecture early — before silicon. Firmware-driven, with a fixed SoC-level
memory map and IRQ map so firmware, drivers, and TLM IP models develop independently.

- **RISC-V CPU backend:** `riscv_vp` (Bremen RISC-V VP, application-class, boots an RTOS).
- **Infrastructure:** TLM bus router, RAM, CLINT, PLIC.
- **Peripherals:** UART×2, I2C×2, SPI×2, Timer×2, WDT, PWM, DMA, TRNG, DMIC, OTP, QSPI
  (+ NOR flash), CMU, PMU, RTC, ADC.
- **Optional accelerator adapter:** SAURIA NPU v4 MMIO, physical-RAM master,
  PLIC IRQ17, and fixed `INT8 32xK * Kx32 -> INT32 32x32` GEMM. The private
  SystemC core is not part of this public repository.

> Full documentation: [`docs/CDC-VP_VIRTUAL_SoC_PLATFORM.md`](docs/CDC-VP_VIRTUAL_SoC_PLATFORM.md)

## Quick start

```bash
export CC=/usr/bin/gcc
export CXX=/usr/bin/g++
export PATH=/usr/bin:/bin:$PATH
$CC -dumpfullversion
$CXX --version | head -n 1

source tools/third_party/setup_env.sh        # RISC-V toolchain
tools/third_party/setup_third_party.sh       # fetch external CPU cores

cmake -S . -B build \
  -DCDC_CPU_BACKEND=riscv_vp \
  -DCDC_BUILD_CUSTOM_SOC=ON -DCDC_BUILD_TESTS=ON
cmake --build build -j"$(nproc)"
ctest --test-dir build --output-on-failure
```

Requires: SystemC 2.3.4 (`/opt/systemc-2.3.4`), a C++17 compiler, CMake, and the
`riscv-none-elf` bare-metal toolchain for firmware.

Authorized internal builds enable the external NPU model explicitly:

```bash
cmake -S . -B build-soc \
  -DCDC_ENABLE_SAURIA_NPU_V4=ON \
  -DSAURIA_NPU_ROOT=/private/path/to/v4_model
```

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

The external CPU core (**Bremen riscv-vp, MIT**) is **not** bundled; it is
fetched into `third_party/` (gitignored). The external SAURIA source is also not
bundled. Its upstream Solderpad v2.1/Apache-2.0 license and provenance are kept
under [`licenses/`](licenses/). The public CDC-VP source release does not
contain that private model or an NPU-enabled binary. See
[`THIRD_PARTY.md`](THIRD_PARTY.md).
