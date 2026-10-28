# VP_FU3_V1.0 — FU3 SoC virtual platform, release R1

SystemC/TLM virtual platform of the FU3 SoC (CVA6 application CPU + Ibex
boot/control CPU, Common Spec v2.1 IP set) for firmware and software bring-up
before silicon. This package is release **R1** of the assembly plan: every IP
delivered for R1 is integrated; EDHOC/OSCORE, PWM0/1 and CAN (R2) are not.

What the VP is and is not:

- It models the firmware-visible behaviour of the FU3 blocks as the specs and
  the decisions recorded with them define it (`FU3_VP_ABI_1.md`). Timing is a
  TLM loosely-timed model, not cycle accurate; it is **not** an RTL equivalence
  or silicon-timing reference.
- The images it runs are preloaded test images. The product Boot ROM and
  firmware belong to FW/SW; the small programs in
  `sw/bootloader/sources/VP_FU3_V1.0/` are VP verification firmware, examples only.

## Package layout

| Path | Content |
| --- | --- |
| `vp/bin/VP_FU3_V1.0/vp_fu3_soc` | the VP executable |
| `vp/bin/VP_FU3_V1.0/libsystemc.so*` | SystemC 2.3.4 runtime (found through RPATH `$ORIGIN`) |
| `vp/configs/VP_FU3_V1.0/` | prebuilt example images (`fw/`), stimulus files (`data/`), `run_package_tests.sh` |
| `sw/bootloader/sources/VP_FU3_V1.0/` | example firmware sources, `Makefile` (`make` builds into `sw/bootloader/build/VP_FU3_V1.0/`), `README.md`; `include/`: `fu3_vp_abi.h` and `csr/` (register headers of every IP) |
| `sw/bootloader/test/VP_FU3_V1.0/run_vp.sh` | demos of the example firmware on the VP (`smoke`, `console`, `handoff`, `wake`, `gdb`, `run`) |
| `vp/doc/VP_FU3_V1.0/` | this guide, firmware baseline, maps, ISA, clock/reset contract, R1 gaps, test results summary, `MANIFEST.txt` |
| `vp/licenses/` | notices of the bundled third-party code |

## Requirements

AlmaLinux 9 (x86_64) with its base C/C++ runtime (`libstdc++.so.6`,
`libgcc_s`, `libm`, `libc`). No compiler, SystemC installation or build tree is
needed to run. To build firmware: a RISC-V GCC (the examples use xPack
`riscv-none-elf-gcc` 15.2); for source-level debugging: `riscv-none-elf-gdb`.

## Quick start

```sh
VP=vp/bin/VP_FU3_V1.0/vp_fu3_soc
CFG=vp/configs/VP_FU3_V1.0
$VP --version                 # package, CDC-VP revision, map, FU3_VP_ABI_1
$VP --self-test               # platform self-check, exit 0
$VP --rom $CFG/fw/fu3_t20_console.elf --mode 10 --boot-pin 1 \
    --uart-rx-file $CFG/data/t20_rx.txt --uart-rx-delay-us 400 \
    --flash $CFG/data/t20_flash.bin --expect-uart PASS --timeout-ms 20
$CFG/run_package_tests.sh     # the package checks (exit 0 = all pass)
make -C sw/bootloader/sources/VP_FU3_V1.0      # build the example firmware
sw/bootloader/test/VP_FU3_V1.0/run_vp.sh smoke  # run its demos
```

## Running firmware

Ibex resets at `0x0000_0080` in the 4 KiB Ibex-only ROM, so every run needs ROM
content: `--rom FILE` (an ELF whose other segments load too, or a raw image at
0), or an `--elf` with a ROM segment. CVA6 starts at `0x0001_0000` once Ibex
releases it through CRM `FCLK_SW[0]` (`FU3_VP_ABI_1.md` §5).

| Option | Use |
| --- | --- |
| `--rom FILE`, `--elf FILE`, `--load ADDR:FILE` | preload (ROM, CVA6_ISRAM, IBEX_ISRAM, DSRAM, QSPI XIP window) |
| `--flash FILE[@OFF]`, `--flash-out FILE` | QSPI NOR content before / after the run |
| `--dump ADDR:LEN:FILE` | on-chip memory after the run |
| `--mode M1M0`, `--boot-pin`, `--test-mode`, `--test-mode-sel` | straps from power-on |
| `--debug-en 0\|1`, `--debug-en-release-us N` | DEBUG_EN pad |
| `--pad DIE=SEL`, `--pad-level DIE=0\|1` | pad select (host side), board level |
| `--console N\|none`, `--uart-log`, `--uart-rx-file`, `--uart-rx-delay-us` | console UART |
| `--adc CH=V`, `--rng-seed N`, `--spi-loopback N` | stimulus |
| `--timeout-ms N`, `--expect-uart TEXT`, `--fail-uart TEXT` | run length and verdict |
| `--gdb-cva6 PORT`, `--gdb-ibex PORT`, `--gdb-no-wait` | GDB servers |
| `--quantum-ns N`, `--trace`, `--verbose`, `--list-instances`, `--help` | other |

Exit status: 0 pass (console printed the `--expect-uart` text, or the run ended
without a verdict asked), 1 fail (`--fail-uart` text, which wins over the
expected one), 2 timeout, 3 image or simulator error, 4 usage error.

Preload rules: only ROM, CVA6_ISRAM `0x0001_0000-0x0003_FFFF`, IBEX_ISRAM
`0x0005_0000-0x0005_FFFF`, DSRAM and the QSPI NOR may be loaded. The debug
partitions (`0x0004_0000`, `0x0006_0000`; the Boot ROM writes them), HyperRAM
(needs its controller initialised), MMIO and unmapped space are refused, as is
an ELF whose code does not match the ISA of the CPU owning its region
(`isa.md`). A refused image writes nothing.

## Debugging

**GDB.** One remote-protocol server per CPU on 127.0.0.1, no authentication
(local use only):

```sh
$VP --rom $CFG/fw/fu3_t19_ibex.elf --elf $CFG/fw/fu3_t19_cva6.elf \
    --gdb-cva6 3333 --gdb-ibex 3334 --expect-uart PASS
riscv-none-elf-gdb $CFG/fw/fu3_t19_cva6.elf -ex 'target remote :3333'
```

The run waits at time 0 until every server has its client. Supported: halt,
continue, `stepi`, `break` (EBREAK patched into memory), `hbreak` (execute
triggers: CVA6 2, Ibex 4), ^C, registers (x, pc, CVA6 f0-f31 and fflags/frm/
fcsr, main machine CSRs), memory (through the hart's bus port, as a debug
access: no access policy, no side effects), detach and kill. A hart held in
reset halts on its first instruction after its release. Not supported:
watchpoints. While every attached hart is stopped, the whole SoC stands still
(timers and watchdogs included).

**JTAG / Debug Module.** The SoC model has the DBG JTAG2AXI bridge and one
Debug Module (0x1A11_0000, hartsel 0 = CVA6, 1 = Ibex, abstract register
access, no program buffer, RISC-V Debug Spec 0.13.2). The JTAG side is the VP's
IR/DR profile (`FU3_VP_ABI_1.md` §6), reachable from SystemC test benches; the
executable has no external JTAG port (no OpenOCD). GDB and the DM share each
hart with one owner at a time.

## Firmware interface (R1 baseline)

- `FU3_VP_ABI_1.md` and `fu3_vp_abi.h` (in `sw/bootloader/sources/VP_FU3_V1.0/include/`,
  `SWINC` below): register interfaces, the SYSCSR
  extension (boot status, reset cause, INTMAP enables, software interrupts),
  boot contract, Ibex PMP profile, debug, access profile, pads, VP/RTL
  differences with their owners, agreements still needed.
- `memory_map.md`: the bus map of this build (Common Spec v2.1).
- `irq_dma_map.md`: interrupt sources, PLIC IDs, INTMAP routes, CPU inputs, DMA
  channels.
- `SWINC/csr/`: register definitions of every IP (C or C++ headers), see its
  `README.md`.
- `isa.md`: both CPUs (extensions, PMP, triggers, reset).
- `clock_reset.md`: clock, reset and gate contract.
- `r1_gaps.md`: what R1 does not cover, open spec decisions, IP not received.

## Tests

`run_package_tests.sh` checks this package as delivered (self-test, console
demo with straps, console input, NOR and memory dump, wake and watchdog-reset
firmware on both CPUs, GDB-pair firmware, error cases). `test_results.md`
summarises the full regression of the build this package comes from (FU3:
platform and component tests), these checks and the example firmware build and
demos (their logs: `sw/bootloader/build/VP_FU3_V1.0/`).

## Responsibilities

The VP team owns this package and the VP behaviour it documents. FW/SW owns the
product Boot ROM, firmware and their image formats; the open agreements are in
`FU3_VP_ABI_1.md` §10 and `r1_gaps.md`. Report VP issues with the command line,
the image and the `--version` output.
