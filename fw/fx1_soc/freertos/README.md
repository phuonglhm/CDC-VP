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
   - NPU: `VP_ID`, ABI major 1, the features the CLI uses, quiescent with
     nothing accepted, `VP_PUSH_WAIT_NS` read/write, a byte access faults.
     Without an NPU the aperture faults: `NPU: absent (fx1_soc --no-npu, or
     not built), skipped`, not a failure.
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
   only; hart 1 keeps crt0's). Then it installs the ISP, SYS_DMA, Ethernet and
   UART interrupt services, creates the NPU task (which probes the NPU and
   installs its interrupt once the scheduler runs) and the CLI task, and starts
   the scheduler.

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
| `regdump isp\|dma\|plic\|clint\|uart\|eth` | Key registers of one IP (`eth`: no RMON counter, so a dump never clears one) |
| `fpu test` | FP context-switch check (above) |
| `dma test [BYTES]` | SYS_DMA M2M copy: completion IRQ → semaphore, ownership calls, data verified |
| `isp selftest` | The 8-frame 64×48 fixture through the driver from a task, ISR → semaphore; every NV12 CRC must match the reference |
| `isp selftest profile` | The same fixture in CSR v1.07 profile mode: the basic configuration built in bank 0 (INIT), sealed as profile 1 and applied; the same CRCs, every output carrying profile ID 1 |
| `isp run` | Process the RAW job that the host loaded into DDR (see `platforms/fx1_soc/demo/`); the NV12 stays in DDR for `--dump`. In the job's mode: CSR v1.07 profile mode by default (the profile built into bank 0, sealed and applied; a refused profile is reported, never run in legacy mode instead), legacy for a version-1 job; the job's output channels (CH0, CH1 NV12, CH2 RGB888) |
| `isp run legacy` | The same job in legacy mode, for comparison |
| `isp stats` | AEC/AWB/AF publications of the last frame |
| `isp stream start` | VP-only RAW-sequence stream (`platforms/fx1_soc/docs/ISP_STREAM_CONTRACT.md`): runs the session the host prepared (`isp_stream_prep.py`, `fx1_soc --load`) in one ISP session, every frame copied from the source pool by SYS_DMA channel 1 and published to the host bridge (`fx1_soc --isp-stream-spool`). Refused without an attached bridge (or one that did not attach this session), with an invalid descriptor (the word is named), when the ISP is owned (camera, `isp run`), when the session already ran or when the bridge's spool budget holds no frame. Submits only the frames the spool budget holds, then stops (LIMIT). Returns at once; the stream task runs it |
| `isp stream status`, `isp stream stop`, `isp stream wait [MS]` | The status block; a stop request (the frames in flight complete); wait until STOPPED or ERROR (MS: simulated-time limit), then the summary as a check (ERROR fails) |
| `isp stream idle [MS]` | Wait until no session runs and the stream no longer holds the ISP, then the summary as a check (passes once the ISP is free). An ended session whose ISP stop still drains (a recovery waiting for a transaction the bus holds: status `ISP stop draining`) keeps the ISP claimed, its interrupt masked at the PLIC, and refuses a new start, until the stop has finished; use it before another ISP command after an ERROR session. A PASS does not clear that session's error; without MS it waits as long as the stop drains (give scripts a deadline); it says nothing about other owners (the next command still claims the ISP) |
| `eth ...` | Ethernet MAC, see "Ethernet" below |
| `camera ...` | Camera build only (CSI-2 M6b), see "Camera" below |
| `npu ...` | NPU (SAURIA v4.7), see "NPU" below |
| `selftest` | hwcheck report + `fpu test` + `dma test` + `isp selftest` + `eth selftest` + `npu selftest` |
| `poweroff` | End the simulation: PASS when no check failed so far, otherwise FAIL `0x400 + failures` |

A command that checks something prints `name: PASS|FAIL`. Failures are counted
for `poweroff`.

## Ethernet (plan M4)

The MAC (`ETH_CSR` 0x1103_0000, PLIC source 6) through the FX1 manager
`fw/fx1_soc/drivers/fx1_eth.[ch]` on the portable driver
(`components/FX1_Components/ethernet/driver`). One owner: the service task
`eth` (priority 3) runs the manager's service under a mutex that every `eth`
command also takes; the ISR only takes and acknowledges `ETH_INT_STATUS` (DONE
cleared, level error sources masked so the line falls before the PLIC
completion) and notifies the task. A descriptor whose DES0 write-back is still
in flight after DONE (MAS 9.3) is polled every 1 ms, every 10 ms once past a
2 ms deadline (counted once in `wb-timeouts`), for as long as it stays owned:
never left for an interrupt that will not come, never reclaimed while OWN reads
1. A DMA error is classified by its ERRINFO class: class 4 (TX_TOO_LONG,
RX_TRUNC) is a soft error, every other class halts that direction from the
moment ERR is seen, before HALTED (submissions refused at once); `eth recover`
waits up to 2 ms for HALTED, else the device goes FAULTED. A halting error that
came while a class-4 one still held ERR shows only as HALTED (no new ERR, no
interrupt): after a class-4 clear the direction is watched (1 ms, then 10 ms,
up to 100 ms or until TX is suspended), every service pass checks HALTED and
the task runs at least every 100 ms while the MAC is running; `eth send`
reads TX HALTED before queuing. MDIO completion is polled (MDIO_DONE masked);
RMON counters are read without READ_CLEAR_EN, so `eth stats` never clears them.
Hart 1's boot check covers the MAC (identity, BUILDCFG, a hash register,
byte/hole faults, the PHY ID over MDIO) and restores what it changed.

| Command | Behaviour |
|---|---|
| `eth probe` | ID, CAP, BUILDCFG (MAC not the expected IP → FAIL), PHY ID/BMCR/BMSR over MDIO, link and speed; a PHY that does not answer is told apart from a bad MAC |
| `eth status` | State (uninitialized, stopped, running, stopping, FAULTED), speed, TX_EN/RX_EN, ETH_STATUS, INT_EN/RAW/STATUS, ring indices and CUR_DESC, the halted direction and the last error snapshot (ERRINFO, CUR_DESC), the last stop |
| `eth stats [clear]` | RMON (non-destructive), DMA no-descriptor drops, FIFO_STAT, overflow flags, driver counters (interrupts, halts, soft errors, recoveries and their timeouts, write-backs found in flight and past their deadline, stop outcomes with stops that flushed written-back frames); `clear`: RMON CLEAR_ALL, RX_DROP_CNT, every driver counter (all count from the clear) |
| `eth start` | Cold init from uninitialized, SRST + rebuilt rings from stopped; nothing to do when running; refused when FAULTED |
| `eth stop [DRAIN_US]` | Drain the queued frames (default 20000 µs), HALT_REQ, a second HALT_REQ when needed, MAC enables cleared only after HALTED, both MACs idle, FIFOs flushed, interrupts masked: `OK` (stopped, frames dropped and forced printed; `eth status` tells whether frames already written back, so not yet confirmed on the line, were flushed with the TX FIFO) or `ETIMEOUT` (FAULTED: the DMA may still own the rings; `eth recover`). `eth stop 0` skips only the drain: queued frames are dropped, but the command still waits for HALTED (with the forced second HALT_REQ, up to its deadline) and idle MACs before it reports STOPPED, so it does not return at once. A flushed TX FIFO means frames may have been lost; how many is not known. Repeating it is harmless |
| `eth recover` | A direction halted by a DMA error (or with ERR pending): HALTED awaited (≤ 2 ms, else `ETIMEOUT` and FAULTED), snapshot, SRST, that ring rebuilt, the other direction untouched. FAULTED: quiesce again (5 ms per HALT_REQ); both directions HALTED means every write of the old session landed, then SRST, rings rebuilt, running. Otherwise `ETIMEOUT`, still FAULTED, nothing reset or rebuilt (no global reset: MAS 9.1 needs both directions HALTED first, and issued writes would still land); repeat later |
| `eth selftest` | PHY loopback (independent of `--eth-peer`): 60/61/512/1514-byte frames compared at 100 and 10 Mbps, then one frame through the interrupt path; the previous state restored |
| `eth mdio read P R`, `eth mdio write P R V` | Clause 22, validated (0..31, 0..0xffff); a BMCR write is refused while running (speed, loopback, reset only with the MAC stopped) |
| `eth send N [LEN] [PATTERN]` | N frames (≤ 1000, 60..1514 bytes) to ourselves, one per hold of the device; prints how many came back (loopback or partner echo) |
| `eth dump rx\|tx` | The ring's descriptors with CUR_DESC, the software head and the transmit tail |
| `eth cycle N` | N start/frame/stop cycles with one frame each (ends stopped); heap before/after, the service task's stack headroom; PASS without stale frames or heap change |
| `eth load start\|stop` | Background 1 KiB frames with a sequence number and a payload check, for concurrency with SYS_DMA and the ISP; `stop` reports sent, received and payload errors |
| `eth mdiopoll start\|stop` | A second task reads the PHY ID over MDIO every tick, under the owner's mutex; `stop` reports reads, wrong or failed ones, and how many came while the MAC was not running or a direction was halted (MDIO concurrent with the lifecycle) |
| `eth wait MS` | Scripts: let MS milliseconds (≤ 10000) pass without holding the device |

`fx1_soc --eth-peer` chooses the line: `loopback` (default), `partner` (an
echo station) or `none` (link down). Tests: `fx1_smoke_freertos_cli_eth_*`
(see `platforms/fx1_soc/README.md`).

## NPU (plan FX1_NPU_V47 N3)

The NPU (`FX1_NPU_VP_BASE` 0x1300_0000, PLIC source 4) through the reference
driver `fw/fx1_soc/drivers/fx1_npu.h` and the job format
`fw/fx1_soc/drivers/fx1_npu_job.h` (guide:
`platforms/fx1_soc/docs/NPU_JOB_GUIDE.md`). One owner: the task `npu`
(priority 2, as the CLI: time slicing shares the CPU between a job's checks
and CRCs and the console, so neither waits for the other's CPU work to end;
review N4) is the only caller of the driver; the commands post it one request. The
interrupt handler acknowledges `IRQ_STATUS` and notifies the task, nothing
else; a claim that finds `IRQ_STATUS` 0 is counted as stale. The task never
sleeps until a DONE interrupt: it polls with a deadline and blocks at most one
tick between polls, because a batch that latched an error ends without DONE
(review N2). Hart 1's boot check covers the NPU (VP_ID, ABI 1.x, features, idle
at boot, a read/write register, a byte access faults); without an NPU
(`fx1_soc --no-npu`, or a platform built without it) it is reported as absent
and every `npu` command says so without a failure.

| Command | Behaviour |
|---|---|
| `npu info` | Identity (VP_ID, ABI, model hash), features, queue depth, window limit, arenas, job and result addresses, whether a job image is loaded |
| `npu status` | The task's state (idle, running with the job, operation and time, closed), VP_STATUS, ACCEPTED, RETIRED, QUEUE_FREE, interrupts (stale), the last job's outcome. Answers while a job runs (register reads without side effects) |
| `npu selftest` | GEMM bank K=32, the rich chain (conv 3x3, conv 1x1 + residual, MAX_POOL, AVG_POOL, ADD) and the DFC cases (neo MAX_POOL, RESIDUAL) against the C oracle, in the small arena from +3 MiB; a check |
| `npu run` | The job the host loaded at `FX1_NPU_JOB_BASE` (`fx1_soc --load`, `platforms/fx1_soc/demo/npu_run.py`): copied, checked (nothing reaches the NPU from an invalid job), its input CRCs, then its operations one at a time; the result block at `FX1_NPU_RESULT_BASE`. Returns at once |
| `npu wait [MS]` | Until the job has ended (its execution budget plus the firmware's CRC time), then its summary (outcome, operations, ACCEPTED / RETIRED, outputs matching, model cycles, MAC operations, interrupts, NPU time) as a check (OK passes). With MS it returns after MS ms if the job still runs: not a failure, the job goes on, so a script must wait for (or check) the outcome before `poweroff` |
| `npu wait start [MS]` | Until the job posted last has passed its checks and its first operation was accepted by the NPU (pushed or started), so what follows in a script runs while the NPU executes (MS: limit). The state is reset when `npu run` is accepted, so it never answers from an earlier job; not a check |
| `npu reset` | Idle: recovery (ABORT, every engine idle) + soft reset + reopen; after an UNAVAILABLE recovery only the reopen, once the NPU is idle. While a job runs: the job is stopped first (ABORTED, recovered), then the same. A check; the command waits at most 2 s |

## Camera (CSI-2 M6b)

With the camera build, `fx1_soc --camera` runs the sensor → D-PHY → CSI-2 RX →
DDR → ISP → NV12 path through the camera manager
(`fw/fx1_soc/drivers/fx1_camera`, guide in the SDK's camera README):

| Command | Does |
|---|---|
| `camera probe` | CSI-2 RX `IP_VERSION`, the sensor's `model_id` over I2C0; without `--camera`: `camera: absent` |
| `camera mode [W H BITS LANES]` | Set (W 8..3840, multiple of 8; H 2..2160, even; RAW10 / RAW12; 1, 2 or 4 lanes; default 64 32 12 4) or show the mode, and whether a golden exists for it |
| `camera source [sensor\|tpg]` | The sensor through the detailed D-PHY, or the CSI-2 RX test pattern generator (no D-PHY: fast at any size); probe again after a change |
| `camera start` | Configure (ISP profile basic of the committed golden header) and start; takes the ISP. A start that fails after configure is cleaned up at once (`cleanup stop ...`; with the sensor not answering: state "stopped, sensor not confirmed", `camera stop` again later) |
| `camera capture N [crc]` | N frames (≤ 32): collected first, then one line each: capture, CSI-2 FRAME_ID and side, ISP sequence, zero-copy / staged, output slot, RAW spot check (sensor pattern with the frame number, or TPG colour bars), NV12 CRC-32 and golden match. The CRC is computed for NV12 up to 1 MiB, or with `crc` (about 0.3 MIPS from DDR: ten minutes of host time at 4K); without it a golden mode is spot-checked (TPG 4K: 32 Y + 32 UV bytes, positions poisoned before each frame), any other mode prints `NV12 not checked (no golden)` |
| `camera status` | State, counters (CSI-2, ISP, held, timeouts, stop-cancelled, sensor errors), drops, sensor `frame_count` |
| `camera stop [US]` | Stop within US (default 24000) + 200 µs: `OK`, `ESENSOR` (the sensor did not confirm standby: CSI-2 RX and ISP stopped, state "stopped, sensor not confirmed"; `camera stop` again once I2C answers), `ETIMEOUT` |
| `camera recover` | Stop, sensor software reset (frame numbers from 1), configure, start; on an error it prints the step and leaves a valid state, no automatic retry |

Golden (`fw/fx1_soc/tests/camera_isp/camera_isp_golden.h`): sensor 64x32
RAW12 and RAW10, frame numbers 1..64 since the last sensor reset, any lane
count; TPG 640x480 and 3840x2160 RAW12. Other modes print `no golden`.

The camera and the `isp` commands share the ISP: `camera start` is refused
while it is in use, `isp selftest` / `isp run` while the camera is
configured or running (`camera stop` first); the ISP interrupt goes to the
owner. Captures poll the manager with a one-tick delay, so the other tasks
keep running. `camera stop` results are printed, not counted as failures;
refused commands (not probed, running, absent, ISP busy) are not counted
either; probe and capture checks and start / recover failures are.

Large sensor frames go through the netlist-level D-PHY: a 64x32 frame costs
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
| `fx1_smoke_freertos_cli_camera` | `tests/cli_camera.txt` with `--camera`: hwcheck of CSI-2 RX and I2C0 + sensor, probe, 64x32 RAW12 4 lanes (4 frames = golden 1..4, zero-copy) and RAW10 2 lanes (staged, golden), `isp selftest` refused while the camera runs and PASS after `camera stop` |
| `fx1_smoke_freertos_cli_camera_fault` | `tests/cli_camera_fault.txt`: I2C host disabled with `mw` → `camera stop` ESENSOR, state "stopped, sensor not confirmed", `camera start` refused; I2C back → stop OK; `camera recover` → frames 1 and 2 again (golden) |
| `fx1_smoke_freertos_cli_camera_absent` | `tests/cli_camera_absent.txt` without `--camera`: hwcheck `CSI-2 RX: absent, skipped`, `camera: absent`, refused commands, bad mode reported, nothing counted as failed |
| `fx1_regression_freertos_cli_camera_startfault` | Test image `freertos_cli_camfault` (I2C host lost once between configure and stream-on): `camera start` EDEV, cleanup stop ESENSOR, ISP free again (`isp selftest` PASS); I2C back → stop OK; start and capture against the golden |
| `fx1_regression_freertos_cli_camera_nv12_fault` | TPG 4K, CRC skipped: a good frame (Y and UV spot ok), then the ODMA Y address of slot 0 moved to scratch DDR (Y spot BAD), then UV (UV spot BAD) |
| `fx1_regression_freertos_cli_camera_tpg` | `tests/cli_camera_tpg.txt`: TPG 640x480 RAW12 (2 frames = golden) and 3840x2160 RAW12 (metadata, RAW spot check, CRC skipped by size); about one minute of host time |
| `fx1_smoke_freertos_cli_npu` (+ `_absent`) | `tests/cli_npu.txt`: hwcheck covers the NPU, `version` names it, `npu info/status/selftest/run/wait/reset` (no job loaded); with `--no-npu` every command reports the NPU absent, no failure |
| `fx1_smoke_freertos_cli_npu_job` | The `chain` fixture loaded with `--load`: `npu run` twice (buffers reused), every output CRC as expected, ACCEPTED / RETIRED +5 each time |
| `fx1_smoke_freertos_cli_npu_reset` | The `long` fixture: `npu status` while it runs, `npu wait 1` returns, `npu reset` stops it (ABORTED, recovered, soft reset), then the job again passes |
| `fx1_smoke_freertos_cli_npu_reset_exec` | Ethernet load, the `busy` fixture: `npu reset` after `npu wait start`, while the NPU executes (ACCEPTED 1..7), stops it (ABORTED, recovered), then the job again passes |
| `fx1_smoke_freertos_cli_npu_coex` | Ethernet load, the `busy` job, then (after `npu wait start`) ISP legacy + profile fixtures and a SYS_DMA copy while the NPU executes; every IP against its own golden |
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
