# VP_FU3_V1.0 — example images and checks

VP verification firmware: small programs that exercise the VP. They are
**not** a product Boot ROM or firmware (that belongs to FW/SW); use them as
examples of the boot contract (`../../doc/VP_FU3_V1.0/FU3_VP_ABI_1.md` §5) and
of the CLI.

| Image (`fw/`) | Runs on | What it does | Command |
| --- | --- | --- | --- |
| `fu3_t20_console.elf` | Ibex (ROM stub + IBEX_ISRAM) | console banner, straps and reset cause from BOOT_STATUS/RESET_CAUSE, echoes one console line, reads the NOR through XIP, writes `0xC0FFEE20` at `0x0103_F000`, prints PASS | `--rom fw/fu3_t20_console.elf --mode 10 --boot-pin 1 --uart-rx-file data/t20_rx.txt --uart-rx-delay-us 400 --flash data/t20_flash.bin --expect-uart PASS` |
| `fu3_t19_ibex.elf` + `fu3_t19_cva6.elf` | both | Ibex boots twice around a software reset, releases CVA6; CVA6 computes single-precision values (fa0-fa4) and stops at `t19_fp_stop`; Ibex prints `cva6 result` (fa2) and PASS | `--rom fw/fu3_t19_ibex.elf --elf fw/fu3_t19_cva6.elf --expect-uart PASS` (add `--gdb-cva6 3333 --gdb-ibex 3334` to debug) |
| `fu3_t21_ibex.elf` + `fu3_t21_cva6.elf` | both | WFI wakes from TIMER1/TIMER2 (Ibex, INTMAP), TIMER0, PLIC and SWIRQ (CVA6), a gated timer that does not wake, SWIRQ both ways, then a WDT0 watchdog reset during the traffic of both CPUs; boot 2 prints RESET_CAUSE and PASS | `--rom fw/fu3_t21_ibex.elf --elf fw/fu3_t21_cva6.elf --expect-uart PASS --timeout-ms 50` |
| `vp_debug_blob.bin` | — | VP-only placeholder of the debug-partition blob (`j .` at +0 and +8), FU3_VP_ABI_1 §6 | (included by a Boot ROM; not run directly) |

`data/`: `t20_rx.txt` (console line), `t20_rx_pass_fail.txt` (a line that
prints both verdict texts: FAIL wins), `t20_flash.bin` (NOR content).

`fw-src/`: the sources of these images (ROM stub `ibex_rom.S`, start-up files,
linker scripts `ibex.ld`/`cva6.ld`, trap runtime, the test programs) and
`fu3_vp_abi.h`. Build (xPack riscv-none-elf-gcc 15.2):

```sh
COMMON="-O1 -g -ffreestanding -nostdlib -nostartfiles -msmall-data-limit=0 -Ifw-src fw-src/fw_trap.S fw-src/fw_runtime.c"
riscv-none-elf-gcc -march=rv32imc_zcb_zicsr_zifencei -mabi=ilp32 $COMMON \
    fw-src/ibex_rom.S fw-src/ibex_start.S fw-src/t20_console.c -T fw-src/ibex.ld -o t20_console.elf
riscv-none-elf-gcc -march=rv32imfc_zicsr_zifencei -mabi=ilp32f $COMMON \
    fw-src/cva6_start.S fw-src/t19_cva6.c -T fw-src/cva6.ld -o t19_cva6.elf
```

`run_package_tests.sh`: the package checks (exit 0 = all pass); the GDB check
runs when `riscv-none-elf-gdb` is on PATH or given as `GDB=...`.
