# FreeRTOS profile for `noc_soc`

This directory is the Step 12 firmware profile for the FlooNoC-backed
`platforms/noc_soc`. It reuses the proven FreeRTOS 11.2.0 Bremen RV32 port and
minimal UART/PLIC support from `fw/freertos_fx1`, but always builds with
`NPU=0`, supplies its own application, and uses a platform-specific linker
contract.

This is deliberate reuse of the RTOS port, not reuse of the NPU. NPU source 17
remains tied low in `noc_soc`.

## Frozen Step 12.0 contract

| Contract | `noc_soc` value |
|---|---|
| CPU | Bremen RV32, `rv32imac_zicsr`, ILP32, single hart, M-mode |
| Firmware entry | ELF `_start`; current linker places it at `0x80000000` |
| Image load | Backdoor ELF load through the bound CPU bus before ISS init; not a boot-ROM flow and not charged as NoC traffic |
| Usable firmware RAM | `[0x80000000, 0x80fff000)` |
| Reserved RAM | `[0x80fff000, 0x81000000)`, never claimed by a firmware `PT_LOAD`, stack, heap or BSS |
| Initial/boot stack | `_stack_top = 0x80fff000`; FreeRTOS task stacks come from its heap |
| Trap ownership | `_start` writes `mtvec = freertos_risc_v_trap_handler`; application handlers must not replace it |
| CLINT | base `0x02000000`; `mtimecmp = 0x02004000`; `mtime = 0x0200bff8`; units are microseconds |
| FreeRTOS tick | 1 MHz CLINT time base, 1 kHz scheduler tick |
| PLIC | base `0x0c000000`, hart 0 M-mode context, sources 1 through 31 |
| TIMER0 interrupt | PLIC source 4 |
| DMA interrupts | completion source 7, abort source 8 |
| UART0 | MMIO `0x10000000`, PLIC source 1; TX is connected to the host log and the optional TCP client; host RX enters the pin-side FIFO |
| NPU/ISP/VPU | absent/reserved; NPU source 17 is tied low |
| NoC timing | `fast` for long RTOS runs; short `detailed` runs only for calibrated NoC measurements |
| Traffic ownership | `--mode firmware` only; synthetic survey transactions must remain zero |

The firmware-facing shared headers describe the larger FX1 RAM0 allocation.
This profile intentionally narrows only the linker region; it does not change
the shared peripheral or IRQ ABI.

## Build and validate

Run the mandated host-compiler sanity first, then expose the RISC-V cross
toolchain:

```bash
cd /home/duyptt_HW/Desktop/VP_INTER/upgit/CDC-VP

export CC=/usr/bin/gcc
export CXX=/usr/bin/g++
export PATH=/usr/bin:/bin:$PATH

# sanity
$CC -dumpfullversion
$CXX --version | head

source tools/third_party/setup_env.sh
make -C fw/freertos_noc_soc
make -C fw/freertos_noc_soc check
```

The checker requires an ELF32 little-endian RISC-V image, verifies `_start` is
inside firmware RAM, and checks every `PT_LOAD` using `p_memsz`. It rejects any
claim on the reserved final page.

## Staged scheduler and interrupt proofs

`src/main.c` is the dedicated minimum `noc_soc` application. It deliberately
does not link the FX1 CLI, DMA demo or NPU code. Its own small CLI is linked
only at level 128. `NOC_STEP=121` builds the
isolated scheduler proof: two tasks at priorities 1 and 2 exchange direct
notifications for eight rounds. The firmware checks the active task handle,
priority, round counters and strict handoff state before printing:

```text
FreeRTOS NoC boot
FreeRTOS NoC tasks priorities low=1 high=2
FreeRTOS NoC context switching OK rounds=8
FreeRTOS NoC PASS
```

Run the bounded, self-checking fast-mode regression with:

```bash
./fw/freertos_noc_soc/run_step_12_1.sh
```

It builds the ELF into a private `/tmp` log directory, validates the ELF
contract, requires all 16 low/high handoff markers in exact order, enforces a
host timeout, rejects later-step markers and errors, and verifies zero synthetic
platform traffic.

`NOC_STEP=122` retains that proof and adds two independently observable
interrupt paths:

- three `vTaskDelay(1 ms)` block/wake rounds driven by the CLINT machine timer;
- exactly three level-sensitive TIMER0 interrupts through PLIC source 4, with
  device W1C before PLIC claim completion.

Run its self-checking regression with:

```bash
./fw/freertos_noc_soc/run_step_12_2.sh
```

The final PASS is accepted only after the scheduler, `CLINT tick OK` and
`PLIC TIMER0 OK irqs=3` markers have all appeared. Full CTest registration and
platform mutation controls remain owned by roadmap Steps 12.5 and 12.6.

`NOC_STEP=123` then sequences a DMA task after Step 12.2:

- the DMA channel program and 32-byte source/destination buffers are aligned
  static objects checked against the linker's `__firmware_ram_end`;
- event 3 raises the DMA completion vector routed to PLIC source 7;
- the ISR clears `INTCLR` before the common dispatcher completes the claim;
- the task is unblocked by exactly one ISR notification, not register polling;
- both the prerequisite task wait and completion wait are tick-bounded;
- source and destination are independently compared against the expected
  32-byte pattern before `DMA NoC PASS`.

Run:

```bash
./fw/freertos_noc_soc/run_step_12_3.sh
```

The runner also requires the platform's `[IRQ] dma0 asserted` observation,
rejects any DMA abort assertion and accepts final PASS only after DMA PASS.

## Concurrent SoC workload

`NOC_STEP=124` keeps every proof above and then runs them together. After
`DMA NoC PASS` a supervisor task opens a bounded concurrent
phase in which:

- two CPU tasks at priority 1 read-modify-write their own RAM buffers and read
  one NoC-crossing MMIO register per iteration — CLINT `mtime` for one, the
  PLIC priority register of source 4 for the other — checking the value rather
  than merely issuing the access;
- TIMER0 stays free-running through PLIC source 4 at a 200 us period;
- the DMA manager repeatedly transfers 4 KiB over its own NoC port, each
  completion arriving through PLIC source 7;
- only the supervisor prints, so UART is never the synchronisation mechanism.

Run:

```bash
./fw/freertos_noc_soc/run_step_12_4.sh
```

What the phase actually checks:

| Requirement | How it is checked |
|---|---|
| forward progress for every task | the supervisor requires both workers and the tick to advance at *every* checkpoint, not only end to end |
| DMA runs concurrently with CPU traffic | worker words counted strictly between a DMA launch and its completion interrupt, a window in which the DMA task is blocked |
| no lost DMA completion | `dma_irq_count` must equal one per sequential plus one per concurrent transfer, exactly |
| no data corruption | worker buffers verify the previous generation before writing the next and explicitly read back the final generation after phase stop; each DMA transfer uses its own pattern and both source and destination are compared |
| no PLIC source left claimed | sources 4 and 7 kept re-delivering, which the gateway forbids unless every claim was completed; plus a final claim/pending probe that must read idle |
| no deadlock or watchdog expiry | every wait is timeout-bounded and the checkpoint count itself is capped |
| zero synthetic traffic | the runner requires zero survey transactions, RAM writes and DMA register writes |

Two limits are deliberate rather than accidental. The concurrent transfer is
built from a DMALP loop because the 32-byte Step 12.3 transfer is shorter than
one CPU-task iteration, so nothing could be shown to run while it is in flight.
And TIMER0 overlap is scoped to the whole transfer loop, not to one transfer:
its period is far longer than a single transfer, so a per-transfer requirement
would assert a ratio of periods rather than concurrency.

The counters the runner enforces are floors, never fixed values. Iteration and
interrupt counts depend on the scheduling of a genuinely concurrent phase, and
turning them into exact expectations would make the regression a timing
threshold.

## Step 12.8 UART CLI

The default level is now `NOC_STEP=128`. It retains the complete level-124
workload unchanged and adds a dedicated FreeRTOS CLI. The CLI arms the existing
interrupt-driven UART BSP early, but a direct task notification keeps its parser
closed until the supervisor has printed the full `FreeRTOS NoC PASS`.

The input path is architectural, not a firmware backdoor:

```text
host file/TCP -> uart_host_bridge -> UART0 RX FIFO
              -> PLIC source 1 -> FreeRTOS ISR queue -> CLI task
```

Every UART register access and PLIC claim/complete issued by the CPU crosses
FlooNoC. The command set is intentionally limited to:

| Command | Result |
|---|---|
| `help` | list the available CLI commands |
| `soc` | fixed 4x4 topology, manager placement and key addresses |
| `noc_dashboard` | request a live metrics snapshot and full host-rendered dashboard |
| `hw_scan` | enumerate the mapped hardware and check stable identities/state |
| `reg_test` | run restoring RO, RW and W1C register tests |

`hw_scan` reports 22 implemented blocks, three architecturally reserved
accelerator windows (ISP/VPU/NPU), and two blocks present in the shared FX1
memory map but absent from `noc_soc` (IFLASH/PMU). It never probes the two
unmapped addresses, because a CPU decode miss would become a synchronous
FreeRTOS trap. `reg_test` runs 37 checks and restores every RW register after
readback. Its TIMER1, ADC and QSPI W1C checks keep their interrupt sources
masked and clear the generated cause before returning.

The deterministic acceptance is:

```bash
cmake --build build --target noc_soc --parallel
./fw/freertos_noc_soc/run_step_12_8.sh
```

It replays `help`, `soc`, `noc_dashboard`, `hw_scan` and `reg_test` from a
private file, requires the platform to observe UART0 asserting, and enforces
`full SoC PASS -> CLI ready -> help -> soc -> dashboard request -> HW_SCAN PASS
-> REG_TEST PASS`. The normal fast-mode run must reject the dashboard request
with an explicit detailed-mode diagnostic. The platform/firmware mutation
registry disables UART RX, omits the private dashboard request record, corrupts
one scan identity and bypasses the RW pattern write; all four defects must be
rejected.
The runner builds firmware but intentionally does not rebuild an externally
selected platform binary; its preflight reports a stale binary that lacks the
UART options.

## Manual platform command

The `noc_soc` CLI uses microseconds and does not expose the FX1 platform's
`--quantum` option:

```bash
./build/platforms/noc_soc/noc_soc \
  --mode firmware \
  --noc-timing fast \
  --fw fw/freertos_noc_soc/freertos_noc_soc.elf \
  --sim-us 700000
```

A bare `make -C fw/freertos_noc_soc` builds level 128, so the ELF runs the full
concurrent workload and then waits for CLI input. Pass `NOC_STEP=121`, `122`,
`123` or `124` to reproduce an earlier stage; level 125 remains the separate
Step 12.7 measurement image.

For an interactive TCP console, start the VP in one terminal:

```bash
./build/platforms/noc_soc/noc_soc \
  --mode firmware \
  --noc-timing fast \
  --fw fw/freertos_noc_soc/freertos_noc_soc.elf \
  --sim-us 60000000 \
  --uart0-socket 5555 \
  --uart0-wait
```

Then connect from another terminal:

```bash
nc 127.0.0.1 5555
```

Wait for `FreeRTOS NoC CLI ready` before typing. `--uart0-wait` waits for the
TCP client at simulation start, not for firmware to arm RX; sending a burst
before the ready banner can fill the UART's 16-byte hardware FIFO. A long
`--sim-us` value is also required because simulation time advances much faster
than wall time in fast mode. There is no `exit` command; end a manual VP
session with `Ctrl-C` or let its `--sim-us` bound expire.

`nc` is sufficient for `help`, `soc`, `hw_scan` and `reg_test`. The full
`noc_dashboard` command is host-assisted and must use `tools/noc_cli.py`
instead. Start the detailed platform in terminal 1:

```bash
export CC=/usr/bin/gcc
export CXX=/usr/bin/g++
export PATH=/usr/bin:/bin:$PATH
$CC -dumpfullversion
$CXX --version | head -n 1

cmake --build build --target noc_soc --parallel

source tools/third_party/setup_env.sh
make -C fw/freertos_noc_soc NOC_STEP=128 check

./build/platforms/noc_soc/noc_soc \
  --mode firmware \
  --noc-timing detailed \
  --fw fw/freertos_noc_soc/freertos_noc_soc.elf \
  --sim-us 100000 \
  --noc-metrics /tmp/noc_cli_live.json \
  --uart0-socket 5555 \
  --uart0-wait
```

Connect with the host-aware client in terminal 2:

```bash
python3 tools/noc_cli.py \
  --port 5555 \
  --metrics /tmp/noc_cli_live.json
```

Wait for `FreeRTOS NoC CLI ready`, then type `noc_dashboard`. Firmware sends a
private UART control record; `noc_soc` suppresses that record from its normal
stdout, atomically publishes a live `floo-noc-metrics-v1` JSON file, and the
host client invokes the same `tools/noc_dashboard.py` renderer used by the
standalone flow. The terminal therefore gets the complete dashboard, not a
short firmware summary.

This is a live, fixed-window firmware snapshot: it is intentionally marked
`drained: false`, so it is diagnostic and cannot become a DSE winner. Detailed
mode is mandatory because fast mode does not maintain measured router
utilisation, stalls or FIFO occupancy. A detailed FreeRTOS run is much slower
than the normal fast regression. Send one dashboard request at a time and wait
for the report before sending the next.

For deterministic non-interactive input, use:

```bash
printf 'help\nsoc\nnoc_dashboard\nhw_scan\nreg_test\n' >/tmp/noc_cli_input.txt
./build/platforms/noc_soc/noc_soc \
  --mode firmware \
  --noc-timing fast \
  --fw fw/freertos_noc_soc/freertos_noc_soc.elf \
  --sim-us 550000 \
  --uart0-rx-file /tmp/noc_cli_input.txt \
  --uart0-rx-delay-us 10000
```

Step 12 acceptance uses the self-checking runners, not this manual command.
Short detailed-mode smokes were also run to show the scheduler, interrupt and
DMA markers through the cycle-stepped reference path. They are supporting
functional evidence, not performance or full-RTL-equivalence claims. For level
124 the detailed run reached the same acceptance as the fast one, at 45 ms of
modeled time for 404 s of host time - about 45x the fast run's cost - which is
why fast mode owns the regression window and detailed mode is run once.
