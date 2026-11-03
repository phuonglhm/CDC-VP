# VP_FX1_V3.0 firmware: FreeRTOS CLI

This directory is the source of `freertos_cli.elf`, the firmware delivered
with the FX1 SoC VP. It is the starting point for FX1 drivers on FreeRTOS:
- BSP;
- interrupt handling through the PLIC;
- the ISP and SYS_DMA drivers called from tasks;
- the Ethernet MAC through its FX1 manager, owned by a service task;
- a UART command line.

It builds with GNU Make and the xPack `riscv-none-elf-gcc` 15.2. It needs
neither CDC-VP nor CMake.

```bash
make -C sw/bootloader/sources/VP_FX1_V3.0 -j"$(nproc)"            # toolchain on PATH
make -C sw/bootloader/sources/VP_FX1_V3.0 TOOLDIR=/opt/xpack-riscv-none-elf-gcc-15.2.0-1/bin
sw/bootloader/test/VP_FX1_V3.0/run_vp.sh smoke
```

Outputs go to `sw/bootloader/build/VP_FX1_V3.0/`: `freertos_cli.elf`, `.dis`,
`.map` and `.sections.txt`.

## Layout

| Directory | Content |
|---|---|
| `app/` | the application: `main.c` (boot, hart roles), `cli.c` (console, commands), `cmd_isp.c`, `cmd_dma.c`, `cmd_fpu.c`, `cmd_eth.c` (Ethernet owner task and `eth` commands), `hwcheck.c` (hart 1 boot IP check), `rtos_bsp.c` (trap/IRQ glue, UART RX), `probe.S` (fault-tolerant `md`/`mw`), `FreeRTOSConfig.h`, the FP context extension |
| `bsp/` | bare-metal support: `crt0.S` (both harts), `fx1_fw.c/.h` (MMIO, console, CLINT, PLIC, hart release, locks, traps, sim-control verdict), `fx1.ld.S` (linker script, preprocessed with the map) |
| `include/fx1/fx1_memory_map.h` | **the** address/IRQ/clock map (VP placeholders pending the HAS) |
| `include/dma/fx1_dma_regs.h` | SYS_DMA register map |
| `include/fx1_ddr3/` | DDR3 controller register map (`fx1_ddr3_regs.h`; field codes the specification leaves open are in `provisional_encodings.h`: use the macros). The VP initializes the DDR3 before the harts start |
| `include/fx1/fx1_ddr_sysctrl.h` | DDR_SYSCTRL, the VP-only glue of the DDR3 sideband: readiness, power requests, QoS, resets |
| `ddr3/`, `bsp/crt0_rom.S`, `bsp/fx1_rom.ld.S` | DDR3 reference bring-up code and firmware-init examples that run from BootROM/SRAM (`make ddr3`, then `run_vp.sh ddr3`); guide in `vp/doc/VP_FX1_V3.0/ddr3/README.md` |
| `include/fx1_isp/` | ISP register map (`fx1_isp_csr.h`, generated from the ISP CSR specification) and the ISP driver API (`fx1_isp_drv.h`) |
| `drivers/isp/fx1_isp_drv.c` | the reference ISP driver (C99, HAL-based, no OS dependency) |
| `drivers/eth/` | Ethernet: register map `eth_regs.h`, portable driver `eth_drv.[ch]`, FX1 manager `fx1_eth.[ch]` (start here); guide in `vp/doc/VP_FX1_V3.0/eth/README.md` |
| `eth/` | bare-metal Ethernet example `eth_wiring.c` (`make eth`, then `run_vp.sh eth`) |
| `isp_selftest/` | the 8-frame synthetic fixture behind `isp selftest`, with its reference CRCs |
| `demo/isp_demo_job.h` | the host ↔ firmware job contract of `isp run` (job, result block, profile in shared DDR) |
| `third_party/FreeRTOS-Kernel/` | the subset of FreeRTOS-Kernel V11.2.0 (`0adc196`) this build compiles, MIT |

## ABI and build contract

- **Arch:** `-march=rv32imafc_zicsr_zifencei -mabi=ilp32f -mcmodel=medany`. The
  harts have hardware single-precision FP. Every object, including libraries
  you add, must be `ilp32f`. A mismatch can link and still fail at run time.
- **Image:** one ELF linked at `FX1_FW_IMAGE_BASE` (0x8000_0000, 4 MiB). Both
  harts start at its entry.
  - Hart 0 runs `main()`.
  - Hart 1 waits in `crt0` until hart 0 calls `fx1_release_secondaries()`,
    then runs `secondary_main()`.
- **Stacks:** 64 KiB per hart at `FX1_FW_STACK_BASE`. FreeRTOS task stacks
  come from its heap.
- **Flags:** `-O2 -g -std=c11 -ffreestanding -fno-builtin -Werror` and
  `--param=min-pagesize=0`. The last one keeps GCC from treating MMIO at low
  addresses as null-pointer dereferences.
- **No C library:** `app/rtos_libc.c` provides the few functions GCC and the
  kernel need: `memset`, `memcpy`, `memmove`, `memcmp`, `strlen`, `strcmp`.

## Boot sequence

1. **Hart 0 releases hart 1.** It prints the banner and calls
   `fx1_release_secondaries()`.
2. **Hart 1 runs `hwcheck_run()`.** Hart 0 touches no IP meanwhile (one owner
   per IP).
   - BootROM: a store faults and leaves the ROM unchanged.
   - DDR: the last 4 KiB pass patterns, address-in-address, walking one, and
     byte/halfword merge.
   - Bus: an unmapped load and store both fault.
   - CLINT: `mtime` advances, and `mtimecmp[1]` is read/write.
   - PLIC: priority, enable, threshold and claim on context 1.
   - UART: reset values, the read/write control registers, and a byte access
     faults.
   - SYS_DMA: identity, idle state, command registers, and a byte access
     faults.
   - ISP: `COMMON_VER_ID`, `DMA_ERR` clear, and the buffer-address registers.
   - Ethernet MAC: identity, BUILDCFG, a hash register, byte and hole
     faults, and the PHY ID over MDIO.

   Every read/write probe restores the original value.
3. **Hart 0 prints the report.** On any failure it prints
   `boot halted: hwcheck FAIL` and ends with FAIL code 0x50; the scheduler
   never starts.
4. **Hart 0 starts FreeRTOS.** It sets `mtvec` to the kernel trap entry (hart 0
   only). Then it installs the UART, SYS_DMA, ISP and Ethernet interrupt services,
   creates the CLI task and starts the scheduler.

## FP registers in task context

The stock FreeRTOS RISC-V port saves only the integer registers.
`app/freertos_risc_v_chip_specific_extensions.h` adds f0–f31 and `fcsr` to
every task context (34 words), through the port's
`portasmSAVE/RESTORE_ADDITIONAL_REGISTERS` hooks. Tasks may therefore use
`float` freely. `fpu test` checks this: two tasks interleave the same FP
recurrence, and each result must match the switch-free value bit for bit.

## Interrupts and drivers from tasks

`rtos_bsp.c` dispatches machine external interrupts:

1. claim the PLIC source;
2. call the handler registered with `bsp_irq_register()`;
3. complete the source;
4. `portYIELD_FROM_ISR()` if the handler woke a task.

The pattern used by `cmd_isp.c` and `cmd_dma.c`:

- **ISR:** acknowledge the source at the device, then give a semaphore.
- **Task:** wait on the semaphore with a timeout.
- **Shared driver state:** a driver that both the ISR and the task call into
  is called from the task inside `taskENTER_CRITICAL()` /
  `taskEXIT_CRITICAL()`.
- **A device with its own owner task:** Ethernet (`cmd_eth.c`) uses a
  service task instead of a semaphore. The ISR only acknowledges and notifies;
  the task runs `fx1_eth_service()` under a mutex that every `eth` command
  takes, and waits for the next notification at most as long as the service
  asked for (DMA write-backs can commit after their interrupt).
- **Buffer ownership:** hand buffers to and from a DMA master with
  `fx1_dma_prepare()` / `fx1_dma_complete()`. These are no-ops on the VP
  (no caches). Silicon will need them, so keep the calls.

## Adding a command

1. Write `int cmd_x(int argc, char** argv)` that returns nonzero on a usage
   error, and declare it in `app/rtos_app.h`.
2. Add a row to `commands[]` in `app/cli.c`.
3. If the command uses an IP with an interrupt, register the handler with
   `bsp_irq_register()` before the scheduler starts (see
   `isp_service_init()`).
4. Print results with `out*()`, and report checks with `report(name, ok)`.
   Failed checks are counted for `poweroff`.
5. Add the new `.c` file to `SOURCES` in the Makefile.

The scripted session `sw/bootloader/test/VP_FX1_V3.0/cli_smoke.txt` is plain
text with one command per line, ending in `poweroff`. Copy it to script your
own regression, and run it with `fx1_soc --uart0-rx-file`.

## Limitations

- **Single-core FreeRTOS on hart 0.** Hart 1 is used only at boot. To use it,
  run bare-metal code with your own mailbox in shared DDR.
- **No tickless idle.** The idle task executes `wfi`.
- **No serialised console output.** Output is not serialised between tasks;
  only the CLI task prints after boot.
- **The kernel build disables `-Wint-to-pointer-cast`.** The upstream RV32 port
  casts the 64-bit `mtimecmp` base to a pointer.
