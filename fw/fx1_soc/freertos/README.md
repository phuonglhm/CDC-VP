# FX1 FreeRTOS + CLI firmware

This firmware runs FreeRTOS V11.2.0 on the FX1 SoC VP, with a UART command
line, a boot-time IP check on the second hart, and commands that drive the ISP
and SYS_DMA from tasks. It is the starting point for RTOS drivers on FX1.

| | |
|---|---|
| Kernel | `third_party/FreeRTOS-Kernel` V11.2.0, GCC RISC-V port, `heap_4`, 256 KiB heap, 1 kHz tick |
| ABI | `rv32imafc_zicsr_zifencei` / `ilp32f`, the same as the bare-metal SDK |
| Harts | AMP. FreeRTOS runs on hart 0; hart 1 runs the bare-metal IP check at boot and then parks in WFI |
| Console | UART (PL011 model), RX by interrupt (PLIC source 1) into a queue, TX by polling; lines end with CR LF (BSP `fx1_putc`), like a real serial console |
| Image | `build-fx1/fw/fx1_soc/freertos_cli.elf` (target `fx1_all`) |

## Boot sequence

1. **Hart 0 releases hart 1.** It prints the banner, then calls
   `fx1_release_secondaries()`.
2. **Hart 1 runs `hwcheck_run()`.** Hart 0 touches no IP during this step,
   which keeps the platform's one-owner-per-IP rule.
   - BootROM: a store faults and leaves the ROM unchanged.
   - DDR: on the last 4 KiB, patterns, address-in-address, walking one, and a
     byte/halfword merge.
   - Bus: an unmapped load and store both fault.
   - CLINT: `mtime` advances; `mtimecmp[1]` is read/write.
   - PLIC: priority, enable, threshold and claim on context 1.
   - UART: `IFLS` and `CR` reset values; `IFLS`, `LCR_H` and `IBRD` are
     read/write; a byte access faults.
   - SYS_DMA: identity, idle state, channel-7 command registers are
     read/write, a byte access faults.
   - ISP: `COMMON_VER_ID`, `DMA_ERR` clear, buffer address registers are
     read/write.
   - Camera build only (`-DFX1_BUILD_MIPI_CSI2=ON`, CSI-2 M6b): CSI-2 RX
     `IP_VERSION`, `CORE_CONFIG` and `DMA_CONTROL` reset values, three
     read/write registers; I2C0 host idle and `CTRL` read/write; the sensor's
     `model_id` (0xF1C5) over I2C0. Without `--camera` the CSI-2 RX faults on
     the bus: the report prints `CSI-2 RX: absent (fx1_soc without --camera),
     skipped`, which is not a failure, and the sensor is not addressed.

   Every read/write check restores the register's original value. Hart 1
   writes the report and the `done` flag (release order), then parks.
3. **Hart 0 prints the report.** If any check failed it prints
   `boot halted: hwcheck FAIL` and ends with FAIL code `0x50`. The scheduler
   does not start.
4. **Hart 0 starts FreeRTOS.** `mtvec` is set to the kernel trap entry (hart 0
   only; hart 1 keeps crt0's). Then it installs the ISP, SYS_DMA and UART
   interrupt services, creates the CLI task, and starts the scheduler.

## FP registers in task context

The stock RISC-V port saves only the integer registers. Because this firmware
uses the F extension under `ilp32f`, `freertos_risc_v_chip_specific_extensions.h`
adds f0–f31 and `fcsr` to every context through the port's
`portasmSAVE/RESTORE_ADDITIONAL_REGISTERS` hooks (34 words; see the comment in
that file for the frame layout). New tasks start with FP registers and `fcsr`
at 0.

The `fpu test` command checks this. Two tasks run the same FP recurrence and
yield after every step; each result must equal, bit for bit, the value
computed without any task switch.

## Commands

| Command | Does |
|---|---|
| `help`, `version` | Command list; firmware identity, ISP `COMMON_VER_ID`, SYS_DMA capability |
| `uptime`, `tasks`, `heap` | Tick time and `mtime`; FreeRTOS tasks with state, priority and stack headroom; heap use |
| `hwcheck` | Reprint hart 1's boot report |
| `md ADDR [WORDS]`, `mw ADDR VALUE` | 32-bit reads and writes. Refused outside the mapped regions, for writes to BootROM, and for numbers above 32 bits. An address the device rejects inside a region (such as a CLINT or PLIC register hole) prints `bus error` instead of trapping (`probe.S`) |
| `regdump isp\|dma\|plic\|clint\|uart` | Key registers of one IP |
| `fpu test` | FP context-switch check (above) |
| `dma test [BYTES]` | SYS_DMA M2M copy: completion IRQ → semaphore, ownership calls, data verified |
| `isp selftest` | The 8-frame 64×48 fixture through the driver from a task, ISR → semaphore; every NV12 CRC must match the reference |
| `isp run` | Process the RAW job that the host loaded into DDR (see `platforms/fx1_soc/demo/`); the NV12 stays in DDR for `--dump` |
| `isp stats` | AEC/AWB/AF publications of the last frame |
| `camera ...` | Camera build only (CSI-2 M6b), see "Camera" below |
| `selftest` | hwcheck report + `fpu test` + `dma test` + `isp selftest` |
| `poweroff` | End the simulation: PASS when no check failed so far, otherwise FAIL `0x400 + failures` |

A command that checks something prints `name: PASS|FAIL`. Failures are counted
for `poweroff`.

## Camera (CSI-2 M6b)

With the camera build, `fx1_soc --camera` runs the sensor → D-PHY → CSI-2 RX →
DDR → ISP → NV12 path through the camera manager
(`fw/fx1_soc/drivers/fx1_camera`, guide in the SDK's camera README):

| Command | Does |
|---|---|
| `camera probe` | CSI-2 RX `IP_VERSION`, the sensor's `model_id` over I2C0; without `--camera`: `camera: absent` |
| `camera mode [W H BITS LANES]` | Set (W 8..3840, multiple of 8; H 2..2160, even; RAW10 / RAW12; 1, 2 or 4 lanes; default 64 16 12 4) or show the mode, and whether a golden exists for it |
| `camera source [sensor\|tpg]` | The sensor through the detailed D-PHY, or the CSI-2 RX test pattern generator (no D-PHY: fast at any size); probe again after a change |
| `camera start` | Configure (ISP profile basic of the committed golden header) and start; takes the ISP. A start that fails after configure is cleaned up at once (`cleanup stop ...`; with the sensor not answering: state "stopped, sensor not confirmed", `camera stop` again later) |
| `camera capture N [crc]` | N frames (≤ 32): collected first, then one line each: capture, CSI-2 FRAME_ID and side, ISP sequence, zero-copy / staged, output slot, RAW spot check (sensor pattern with the frame number, or TPG colour bars), NV12 CRC-32 and golden match. The CRC is computed for NV12 up to 1 MiB, or with `crc` (about 0.3 MIPS from DDR: ten minutes of host time at 4K); without it a golden mode is spot-checked (TPG 4K: 32 Y + 32 UV bytes, positions poisoned before each frame), any other mode prints `NV12 not checked (no golden)` |
| `camera status` | State, counters (CSI-2, ISP, held, timeouts, stop-cancelled, sensor errors), drops, sensor `frame_count` |
| `camera stop [US]` | Stop within US (default 24000) + 200 µs: `OK`, `ESENSOR` (the sensor did not confirm standby: CSI-2 RX and ISP stopped, state "stopped, sensor not confirmed"; `camera stop` again once I2C answers), `ETIMEOUT` |
| `camera recover` | Stop, sensor software reset (frame numbers from 1), configure, start; on an error it prints the step and leaves a valid state, no automatic retry |

Golden (`fw/fx1_soc/tests/camera_isp/camera_isp_golden.h`): sensor 64x16
RAW12 and RAW10, frame numbers 1..32 since the last sensor reset, any lane
count; TPG 640x480 and 3840x2160 RAW12. Other modes print `no golden`.

The camera and the `isp` commands share the ISP: `camera start` is refused
while it is in use, `isp selftest` / `isp run` while the camera is
configured or running (`camera stop` first); the ISP interrupt goes to the
owner. Captures poll the manager with a one-tick delay, so the other tasks
keep running. `camera stop` results are printed, not counted as failures;
refused commands (not probed, running, absent, ISP busy) are not counted
either; probe and capture checks and start / recover failures are.

Large sensor frames go through the netlist-level D-PHY: a 64x16 frame costs
seconds of host time, 640x480 minutes. Use `camera source tpg` for large
frames.

## Run it

Interactive, over TCP. Use two terminals:

```bash
build-fx1/platforms/fx1_soc/fx1_soc --fw build-fx1/fw/fx1_soc/freertos_cli.elf \
    --uart0-socket 5000 --uart0-wait --timeout-ms 0 --realtime
platforms/fx1_soc/tools/fx1_console.sh 5000     # second terminal; type `help`
```

- `--uart0-wait`: the simulator waits for the client before it starts. The
  console output goes to both stdout and the client.
- `--timeout-ms 0`: no simulated-time limit. The run ends with `poweroff` (or
  Ctrl-C on the simulator).
- `--realtime`: simulated time never runs ahead of wall-clock time. Without it
  an idle system races through simulated time, measured at about 7× wall
  time. Any `--timeout-ms` budget then runs out within minutes and the client
  sees "Connection closed"; the timeout message prints only on the
  simulator's terminal. With `--realtime`, `uptime` matches wall time and the
  host barely loads the CPU while idle.
- `fx1_console.sh`: puts the terminal in raw mode. It uses socat when
  installed (Ctrl-] quits), else bash's `/dev/tcp`, which needs nothing
  installed (ends with `poweroff`). The VP listens on 127.0.0.1 only: run the
  client on the same host, or in the same container. The firmware echoes like
  a real UART. Plain `nc` or `telnet`
  in line mode echo locally as well, so every command appears twice.

Scripted (deterministic, what CI does). The script is
`fw/fx1_soc/freertos/tests/cli_smoke.txt`:

```bash
build-fx1/platforms/fx1_soc/fx1_soc --fw build-fx1/fw/fx1_soc/freertos_cli.elf \
    --uart0-rx-file fw/fx1_soc/freertos/tests/cli_smoke.txt --uart0-rx-delay-us 60000 \
    --uart0-rx-byte-us 87 --timeout-ms 3000
```

The script ends with `poweroff`, so the exit status is the verdict.
`--uart0-rx-delay-us` starts the input after the boot (about 26 ms with the
DDR3 controller), once the console is listening; `--uart0-rx-byte-us 87` types
at the 115200 8N1 character time. Faster input overruns the 16-byte RX FIFO
while the CLI prints a line with interrupts masked.

The same image also boots from BootROM through the DDR3 firmware-init loader
(`boot_freertos.elf`, run with `--ddr-boot firmware` and a replay delay of
1 s, test `fx1_smoke_ddr3_boot_freertos_cli`).

To process a real camera frame, use `isp run`. The job must already be loaded
when the VP starts. `run_isp_demo.py --raw FILE --out DIR --interactive` does
it in one command:

1. loads the job and starts this image;
2. opens the console;
3. after `poweroff`, builds the PNG and compares the output with the
   stand-alone ISP model.

See "Run it from the FreeRTOS CLI" in `platforms/fx1_soc/demo/README.md`.

`isp selftest` needs nothing loaded. It is a quick synthetic check: 8 frames
of 64×48, each CRC compared with the reference.

## Tests

| Test | Checks |
|---|---|
| `fx1_smoke_freertos_cli_q*ns` | The scripted session at both quanta. hwcheck must report 0 failures; the `fpu`, `dma`, `isp` and `selftest` lines must say PASS; an unknown command must be reported; `poweroff` must end with PASS |
| `fx1_smoke_freertos_cli_camera` | `tests/cli_camera.txt` with `--camera`: hwcheck of CSI-2 RX and I2C0 + sensor, probe, 64x16 RAW12 4 lanes (4 frames = golden 1..4, zero-copy) and RAW10 2 lanes (staged, golden), `isp selftest` refused while the camera runs and PASS after `camera stop` |
| `fx1_smoke_freertos_cli_camera_fault` | `tests/cli_camera_fault.txt`: I2C host disabled with `mw` → `camera stop` ESENSOR, state "stopped, sensor not confirmed", `camera start` refused; I2C back → stop OK; `camera recover` → frames 1 and 2 again (golden) |
| `fx1_smoke_freertos_cli_camera_absent` | `tests/cli_camera_absent.txt` without `--camera`: hwcheck `CSI-2 RX: absent, skipped`, `camera: absent`, refused commands, bad mode reported, nothing counted as failed |
| `fx1_regression_freertos_cli_camera_startfault` | Test image `freertos_cli_camfault` (I2C host lost once between configure and stream-on): `camera start` EDEV, cleanup stop ESENSOR, ISP free again (`isp selftest` PASS); I2C back → stop OK; start and capture against the golden |
| `fx1_regression_freertos_cli_camera_nv12_fault` | TPG 4K, CRC skipped: a good frame (Y and UV spot ok), then the ODMA Y address of slot 0 moved to scratch DDR (Y spot BAD), then UV (UV spot BAD) |
| `fx1_regression_freertos_cli_camera_tpg` | `tests/cli_camera_tpg.txt`: TPG 640x480 RAW12 (2 frames = golden) and 3840x2160 RAW12 (metadata, RAW spot check, CRC skipped by size); about one minute of host time |
| `fx1_smoke_freertos_cli_bad_input` | `tests/cli_bad_input.txt`: out-of-range numbers, wrapping ranges and faulting register holes are reported, and the CLI keeps working through `poweroff` PASS |
| `fx1_smoke_freertos_cli_hwcheck_fail` | An image built with `HWCHECK_INJECT_FAILURE` must halt the boot with `FAIL ISP` and code 0x50 |
| `fx1_smoke_uart_rx_q*ns` | UART RX from the host, polled and interrupt-driven (bare metal) |

## Adding a command

1. Write `int cmd_x(int argc, char** argv)` that returns nonzero on a usage
   error, and declare it in `rtos_app.h`.
2. Add a row to `commands[]` in `cli.c`.
3. If the command uses an IP with an interrupt, register the handler with
   `bsp_irq_register()` before the scheduler starts. Follow the pattern in
   `cmd_dma.c`: the ISR clears the source at the device and gives a semaphore.
4. Call the driver from the task inside `taskENTER_CRITICAL()` /
   `taskEXIT_CRITICAL()` when its ISR also calls into it (see `cmd_isp.c`).
5. Hand buffers to and from devices with `fx1_dma_prepare()` /
   `fx1_dma_complete()`.

## Limitations

- **Single-core FreeRTOS.** Hart 1 is not scheduled. Use it bare-metal with
  your own mailbox if needed; the platform rule is one hart per IP.
- **No tickless idle.** The idle task executes `wfi`.
- **Console output is not serialised between tasks.** Only the CLI task prints
  after boot; `fx1_log` lines remain atomic.
- **The kernel build disables `-Wint-to-pointer-cast`.** The upstream RV32 port
  casts the 64-bit `mtimecmp` base to a pointer.
