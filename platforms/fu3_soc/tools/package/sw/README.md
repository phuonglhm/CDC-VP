# VP_FU3_V1.0 — example firmware

Firmware for the FU3 SoC virtual platform `VP_FU3_V1.0` (release R1): small
programs that exercise the VP and show the boot contract of FU3_VP_ABI_1. They
are **VP verification firmware, not a product Boot ROM or firmware** (those
belong to FW/SW); copy what helps, the interfaces are in
`vp/doc/VP_FU3_V1.0/`.

| Path | Content |
| --- | --- |
| `common/ibex_rom.S` | Ibex ROM stub at `0x80` (boot_addr + 0x80): records the reset state, opens IBEX_ISRAM for execution (PMP entry 4 while RLB = 1), jumps to the image |
| `common/ibex_start.S`, `common/cva6_start.S` | start-up of each CPU (gp, sp, mtvec, `main`) |
| `common/fw_trap.S`, `common/fw_runtime.c`, `common/fw_common.h` | trap entry and the small test runtime |
| `common/ibex.ld`, `common/cva6.ld` | linker scripts: ROM + IBEX_ISRAM + DSRAM; CVA6_ISRAM + DSRAM |
| `include/fu3_vp_abi.h` | FU3_VP_ABI_1 register definitions (SYSCSR extension) |
| `include/csr/` | register definitions of every IP, one `registers.h` per IP (`csr/README.md`); not used by the examples |
| `examples/t20_console.c` | Ibex: console, straps and reset cause, one console input line, NOR via XIP, PASS |
| `examples/t19_ibex.c`, `examples/t19_cva6.c` | Ibex boots twice around a software reset and releases CVA6 (CRM `FCLK_SW`); CVA6 computes single-precision values and stops at `t19_fp_stop` (GDB demo) |
| `examples/t21_ibex.c`, `examples/t21_cva6.c` | WFI wakes from real interrupt sources on both CPUs, a gated timer that does not wake, SWIRQ both ways, then a watchdog reset during the traffic of both CPUs |
| `examples/vp_debug_blob.S` | VP-only placeholder of the debug-partition blob (FU3_VP_ABI_1 §6) |

## Build

```sh
make                       # into ../../build/VP_FU3_V1.0 (ELF, .map, .dis, vp_debug_blob.bin)
make TOOLDIR=<riscv-none-elf toolchain bin directory>
```

Toolchain: xPack `riscv-none-elf-gcc` 15.2 (default
`/opt/toolchains/xpack-riscv-none-elf-gcc-15.2.0-1/bin`). ISA flags per CPU:
Ibex `-march=rv32imc_zcb_zicsr_zifencei -mabi=ilp32`, CVA6
`-march=rv32imfc_zicsr_zifencei -mabi=ilp32f`.

## Run

`../../test/VP_FU3_V1.0/run_vp.sh` runs these images on `vp/bin/VP_FU3_V1.0`
(`smoke` checks the three demos; `gdb` waits for GDB on ports 3333/3334).
Prebuilt copies of the same images are in `vp/configs/VP_FU3_V1.0/fw/`, used by
the package checks (`run_package_tests.sh`) on machines without a toolchain.
