# FlooNoC SoC FreeRTOS Bring-Up Roadmap

**Status:** Active post-Step-11 roadmap, explicitly requested on 2026-08-01;
Gate V0-CLOSE and Steps 12.0-12.7 completed on 2026-08-03; Step 12.8 UART CLI
completed on 2026-08-05
**Scope:** Close the last original v0 verification criterion, then bring up
FreeRTOS on `platforms/noc_soc` without importing the NPU  
**Primary platform:** `platforms/noc_soc`  
**Firmware profile:** `fw/freertos_noc_soc`, reusing the proven
`fw/freertos_fx1` port/BSP with `NPU=0`

This document opens new work after the numbered FlooNoC model roadmap through
Step 11. It does not change the frozen FlooNoC v0 behavior, enable
`MaxUniqueIds > 1`, add a reorder buffer, or add the NPU.

## 1. Current baseline

The current `noc_soc` is ready for RTOS integration at Virtual Platform level:

- the RISC-V CPU's unified instruction/data port crosses FlooNoC;
- CPU, DMA and the latency probe are independent AXI managers on the mesh;
- RAM, boot ROM, CLINT, PLIC and the platform peripherals are mapped through
  the NoC;
- all 31 PLIC inputs are explicitly connected or tied low;
- bare-metal firmware programs the real DMA manager and reaches `DMA PASS`;
- the detailed cycle-stepped path and calibrated fast LT path have automated
  firmware regressions;
- the selected FlooNoC timing blocks are individually RTL cross-checked, while
  the unsigned composed TLM integration layer is covered by scoreboards,
  watchdogs, stress tests and mutation controls.

This is sufficient to begin software bring-up. It is not a claim of full RTL
SoC sign-off: there is no monolithic RTL harness equivalent to the complete
TLM-adapter-to-mesh-to-adapter platform path, and fast mode does not model
contention or back-pressure.

## 2. Mandatory build environment

Run these commands in the same shell before every host build:

```bash
export CC=/usr/bin/gcc
export CXX=/usr/bin/g++
export PATH=/usr/bin:/bin:$PATH

# sanity
$CC -dumpfullversion
$CXX --version | head
```

Use `/tmp` or another fresh build directory. Do not reuse a CMake cache that
was configured from a different source tree.

## 3. Gate V0-CLOSE — DONE (2026-08-03)

### 3.1 Closed gap

The finding was valid. Section 15 of `docs/AI_HANDOFF_CONTEXT.md` previously
marked this criterion as unmet:

> every included leaf has a direct standalone SystemC test

The two previously missing direct tests were for:

- `include/floo_noc_model/rr_arb_tree.hpp`;
- `include/floo_noc_model/meta_buffer.hpp`.

Both leaves already have meaningful indirect RTL coverage:

- `rr_arb_tree.hpp` is exercised by the 152-cycle wormhole-arbiter
  cross-check at 5, 4 and 2 routes;
- `meta_buffer.hpp` is exercised by the 221-cycle chimney subordinate-side
  cross-check, including multi-entry FIFO ordering controls.

Therefore this was not evidence of an untested datapath or an unknown RTL
mismatch. It was a modular-testability and v0 definition-of-done gap. The two
direct tests and their executable mutation controls below closed it before
Step 12 began.

### 3.2 Implemented direct tests

Added:

```text
components/floo_noc_model/tests/test_rr_arb_tree.cpp
components/floo_noc_model/tests/test_meta_buffer.cpp
```

Both are registered through `add_floo_model_test(...)` in
`components/floo_noc_model/tests/CMakeLists.txt`, so they inherit the component
warning policy and run under CTest.

`test_rr_arb_tree` must directly cover:

- `NumIn = 1`, power-of-two and non-power-of-two route counts;
- no requests, one request and competing requests;
- priority selection for representative `rr_q` values;
- wrap-around in `next_rr()`;
- the locked `req_q` path versus live `req_i`;
- invalid request bits outside `NumIn` not affecting a valid decision, where
  applicable to the public contract.

`test_meta_buffer` must directly cover:

- downstream all-ones IDs for representative output-ID widths;
- independent read and write FIFO state;
- FIFO ordering with at least three entries;
- exact full/empty and outstanding-count transitions;
- read and write overflow rejection;
- response-without-request underflow rejection;
- invalid constructor arguments.

Each test must be sensitive to a targeted behavioral defect. A test that only
constructs the class or repeats an assertion already guaranteed by the C++
type system does not close the gate. Existing RTL cross-checks and negative
controls remain required and must continue to pass.

### 3.3 Executable controls — PASS

The prose requirement alone was not sufficient evidence. These two controls
are registered in `rtl_crosscheck/run_negative_controls.sh`:

| Control name | Mutation | Required observing test | Expected reason |
|---|---|---|---|
| `rr-arb-tree-wrap-ignored` | break the lower-mask wrap path in `rr_arb_tree::next_rr()` while keeping the source buildable | `test_rr_arb_tree` | the wrap-around decision is wrong |
| `meta-buffer-overflow-accepted` | remove or bypass the full check before a metadata push while keeping the source buildable | `test_meta_buffer` | the configured FIFO capacity is exceeded without the required overflow rejection |

Before this gate, the component registry contained 39 executable controls.
Closing it added the two controls above. The observed result on 2026-08-03 was:

```text
negative controls: 41 detected, 0 missed
negative controls PASS
```

As with every existing registry entry, detection requires all of the following:

1. the exact production needle is found once in a private copy;
2. the mutated source still builds;
3. the named direct test runs;
4. that test fails;
5. its output contains the mutation-specific expected message.

A compile failure, an unrelated assertion or a generic nonzero exit does not
count as detection.

### 3.4 V0-CLOSE sign-off evidence

All acceptance items passed on 2026-08-03:

1. both executables build with the existing warning-as-error policy;
2. both appear in CTest and pass directly;
3. the complete component suite passes 39/39;
4. the negative-control runner reports 41 detected and zero missed;
5. all twelve existing RTL cross-check runners remain green;
6. `AI_HANDOFF_CONTEXT.md` and `STATUS.md` record the criterion as met without
   claiming new RTL equivalence.

Suggested verification:

```bash
cd /home/duyptt_HW/Desktop/VP_INTER/upgit/CDC-VP/components/floo_noc_model

export CC=/usr/bin/gcc
export CXX=/usr/bin/g++
export PATH=/usr/bin:/bin:$PATH

# sanity
$CC -dumpfullversion
$CXX --version | head

BUILD_DIR=/tmp/floo_noc_model_v0_close
cmake -S . -B "$BUILD_DIR" \
  -DSYSTEMC_HOME=/opt/systemc-2.3.4 \
  -DFLOO_NOC_MODEL_BUILD_TESTS=ON
cmake --build "$BUILD_DIR" -j"$(nproc)"
ctest --test-dir "$BUILD_DIR" --output-on-failure

./rtl_crosscheck/run_wormhole_arbiter_crosscheck.sh
./rtl_crosscheck/run_chimney_rsp_crosscheck.sh
./rtl_crosscheck/run_negative_controls.sh
```

## 4. Step 12.0 — DONE (2026-08-03)

Do not begin with scheduler debugging until the following contracts are
written down and checked against the implementation:

- CPU reset and firmware entry address;
- RV32 ISA/privilege mode expected by the FreeRTOS port;
- RAM, CLINT, PLIC, UART0, TIMER0 and DMA addresses;
- PLIC source 4 for TIMER0 and source 7 for DMA completion;
- CLINT `mtime`/`mtimecmp` units and the intended FreeRTOS tick rate;
- ownership of `mtvec` and the trap entry;
- firmware versus survey traffic ownership;
- maximum supported simulation quantum and timing mode.

The last RAM page, `[0x80ff_f000, 0x8100_0000)`, is reserved by `noc_soc`.
The `fw/freertos_noc_soc` linker profile restricts usable RAM to:

```text
ORIGIN = 0x80000000
LENGTH = 0x00fff000
END    = 0x80fff000
```

The firmware stack, heap, BSS and every ELF32 `PT_LOAD` range must remain below
`0x80fff000`.

The profile reuses the proven FreeRTOS 11.2.0 port and BSP from
`fw/freertos_fx1` where the contracts match. Its wrapper build forces `NPU=0`
and `TFLM=0`; PLIC source 17 remains tied low and is not accessed. Parameterising
the FX1 Makefile's target, linker and disassembly paths left its default build
unchanged.

The frozen contract and reproducible commands are in
`fw/freertos_noc_soc/README.md`. `check_elf_contract.sh` verifies ELF32,
little-endian RISC-V, entry coverage, every `PT_LOAD` using `p_memsz`, and the
exact stack/linker end before simulation.

Observed sign-off:

- host GCC/G++ 11.5.0 and xPack RISC-V GCC 15.2.0;
- FreeRTOS Kernel V11.2.0 at
  `0adc196d4bd52a2d91102b525b0aafc1e14a2386`;
- entry `_start = 0x80000000`;
- one `PT_LOAD` at `[0x80000000, 0x80015730)`;
- `_stack_top = __firmware_ram_end = 0x80fff000`;
- a 700 ms fast-mode compatibility probe reached the legacy
  `FreeRTOS FX1 boot`, `PLIC TIMER0 OK irqs=3`, `CLINT tick OK`, and
  `FreeRTOS FX1 PASS` markers;
- firmware mode reported zero synthetic transactions, RAM writes and DMA
  register writes.

The legacy marker names were expected at Step 12.0 because that step
intentionally reused the existing application as a compatibility probe. They
did not close Step 12.1; the completed Step 12.1 below replaced that probe with
the dedicated `FreeRTOS NoC boot` application and its task-level acceptance.

## 5. Step 12.1 — DONE (2026-08-03): minimum FreeRTOS boot

Bring up the smallest observable RTOS image:

1. start at the firmware entry point;
2. initialise BSS and the boot stack;
3. print `FreeRTOS NoC boot` through polled UART0;
4. start the scheduler;
5. run at least two tasks at different priorities;
6. prove repeated context switches without unexpected traps or stalls.

Do not add the interactive CLI, DMA or optional peripheral tests at this stage.
The goal is to isolate startup, trap entry, task stacks and scheduler state.

Implemented in `fw/freertos_noc_soc/src/main.c`:

- the profile overrides the inherited FX1 application source while retaining
  the pinned FreeRTOS port, startup and minimal shared BSP;
- CLI, TIMER0, DMA and NPU application paths are absent;
- task `noc-low` runs at priority 1 and task `noc-high` at priority 2;
- the tasks exchange direct notifications for eight bounded rounds;
- firmware checks the current task handle, priority, counters and strict turn
  state before every handoff;
- the UART trace contains exactly 16 alternating markers,
  `NoC low round=N` then `NoC high round=N`;
- the final markers are
  `FreeRTOS NoC context switching OK rounds=8` and
  `FreeRTOS NoC PASS`.

`fw/freertos_noc_soc/run_step_12_1.sh` builds a private ELF, runs the platform
with a 120-second host timeout, validates all markers in strict order, rejects
legacy and later-step markers, rejects failures/unexpected traps, and requires
zero synthetic transactions, RAM writes and DMA-register writes.

Observed evidence on 2026-08-03:

- ELF entry `0x80000000`, one `PT_LOAD` at
  `[0x80000000, 0x80013298)`, stack/end `0x80fff000`;
- the self-checking 20 ms fast-mode run passed all 16 ordered handoffs;
- a separate 6 ms detailed-mode smoke produced the same 16 handoffs and PASS;
- both runs reported zero synthetic traffic;
- no `FAIL`, `FATAL`, unexpected-trap diagnostic, watchdog expiry or terminal
  PC zero appeared.

The detailed smoke is functional supporting evidence only. It is not a timing
threshold, contention measurement or complete RTL-equivalence claim. Full
CTest automation and platform-level mutation controls remain Steps 12.5 and
12.6. At this sign-off boundary Step 12.2 was next; it is now completed below.

## 6. Step 12.2 — DONE (2026-08-03): CLINT tick and PLIC interrupt

Add interrupt functionality in two independently diagnosable stages:

### 6.1 CLINT tick

- program `mtimecmp`;
- receive machine timer interrupts;
- increment the FreeRTOS tick;
- prove that `vTaskDelay()` blocks and wakes tasks;
- print `CLINT tick OK`.

### 6.2 TIMER0 through PLIC

- register TIMER0 as PLIC source 4;
- configure priority, threshold and enable state;
- perform claim, dispatch and complete;
- acknowledge the TIMER0 source correctly;
- wake a task from the ISR;
- print `PLIC TIMER0 OK irqs=3` after at least three verified interrupts.

These stages must distinguish a CLINT failure from a PLIC/TIMER0 failure.

The application is compile-staged so the completed Step 12.1 remains directly
reproducible:

- `NOC_STEP=121` excludes the Step 12.2 task and markers;
- `NOC_STEP=122` retains the scheduler proof and adds the two interrupt paths;
- `run_step_12_1.sh` and `run_step_12_2.sh` call a shared checker with
  different marker contracts.

### 6.3 Implemented CLINT proof

- the shared FreeRTOS port owns `mtvec` and programs `mtimecmp`;
- compile-time assertions pin `mtime = 0x0200bff8` and
  `mtimecmp = 0x02004000` against the platform headers;
- a priority-1 task performs three `vTaskDelay(1 ms)` calls;
- every wake verifies that `xTaskGetTickCount()` advanced by at least one;
- the task emits three ordered `NoC tick wake=N` markers, then
  `CLINT tick OK`.

This proves block/wake behavior driven by the 1 MHz CLINT time base and 1 kHz
FreeRTOS tick. It does not yet claim a tick-jitter measurement.

### 6.4 Implemented TIMER0/PLIC proof

- a compile-time assertion pins TIMER0 to PLIC source 4;
- the task registers source 4, assigns priority 1 and enables it for hart 0
  M-mode;
- TIMER0 is programmed for a periodic 5000-count interval;
- the ISR accepts only an asserted device status, clears the level cause before
  returning to the common PLIC claim/complete dispatcher, and notifies the
  priority-3 task;
- the task uses a tick-bounded wait, requires exactly three ISR notifications,
  disables and clears TIMER0, disables source 4, and prints
  `PLIC TIMER0 OK irqs=3`.

### 6.5 Sign-off evidence

`fw/freertos_noc_soc/run_step_12_2.sh` builds a private level-122 image,
validates its ELF, applies a 120-second host timeout and accepts final
`FreeRTOS NoC PASS` only after the scheduler, CLINT and PLIC prerequisites. It
also rejects legacy/DMA markers, failures, unexpected traps, PC zero and
nonzero synthetic ownership.

Observed on 2026-08-03:

- host GCC/G++ 11.5.0 and RISC-V GCC 15.2.0;
- ELF entry `0x80000000`, one `PT_LOAD` at
  `[0x80000000, 0x8001392c)`, stack/end `0x80fff000`;
- 50 ms fast-mode regression: PASS, 16 scheduler handoffs, three CLINT wakes
  and exactly three TIMER0/PLIC notifications;
- 9 ms detailed-mode smoke: the same functional acceptance markers passed;
- both timing modes reported zero synthetic transactions, RAM writes and
  DMA-register writes;
- fast instrumentation recorded 171598 completed firmware transactions and
  1796144 estimated no-contention network cycles;
- detailed instrumentation recorded 160036 completed firmware transactions
  and 1676793 measured network cycles.

Those NoC counters are observations for these fixed simulation windows, not
performance thresholds or congestion measurements. Full CTest registration
and platform mutations remain Steps 12.5 and 12.6. At this sign-off boundary
Step 12.3 was next; it is now completed below.

## 7. Step 12.3 — DONE (2026-08-03): DMA under FreeRTOS

Move the existing bare-metal DMA proof into an RTOS task:

1. prepare source and destination buffers below `0x80fff000`;
2. program DMA through its MMIO target;
3. let the DMA manager issue descriptor/data traffic over FlooNoC;
4. receive completion through PLIC source 7;
5. notify or unblock the waiting task from the ISR;
6. compare every destination byte with the expected data;
7. print `DMA NoC PASS`.

Use timeouts for both interrupt completion and task-level waits. Polling alone
does not satisfy this milestone.

Implemented as the default `NOC_STEP=123` application level:

- level 121 and 122 remain separately buildable and their runners still pass;
- the level-123 DMA task initially blocks, with a 100 ms tick timeout, until
  the Step 12.2 scheduler/CLINT/TIMER0 flags are complete;
- the 25-byte PL330-style channel program and aligned 32-byte source and
  destination buffers are static firmware objects;
- runtime checks bound each complete object by `CDC_RAM0_BASE` and the linker
  symbol `__firmware_ram_end = 0x80fff000`;
- source and destination use independent expected-pattern comparisons, so
  equal corruption cannot pass;
- the task enables event 3, launches channel 0 through the debug interface and
  waits on a direct notification with a separate 100 ms timeout;
- the DMA manager fetches its program and transfers two 16-byte bursts through
  its independent NoC manager port;
- the level-sensitive completion vector reaches PLIC source 7;
- the ISR checks `INTMIS`, clears event 3 through `INTCLR` before PLIC
  completion, counts exactly one IRQ and wakes the task;
- the task disables source 7 and `INTEN`, requires `INTMIS`/raw event/FSRC
  clear, channel `STOPPED`, exact final SAR/DAR and all 32 expected bytes;
- PLIC source 8 is not enabled in the positive path, and the runner rejects any
  platform `dma0_abort` assertion;
- the accepted markers are `DMA NoC IRQ source=7 count=1`,
  `DMA NoC bytes match=32` and `DMA NoC PASS`.

`fw/freertos_noc_soc/run_step_12_3.sh` builds a private level-123 image, checks
its ELF, applies a 120-second host timeout, requires the firmware markers and
the platform's `[IRQ] dma0 asserted` observation, rejects abort/failure/trap
diagnostics, and requires zero synthetic traffic. Final `FreeRTOS NoC PASS`
must follow the scheduler, CLINT, TIMER0/PLIC and DMA PASS markers.

Observed on 2026-08-03:

- ELF entry `0x80000000`, one `PT_LOAD` at
  `[0x80000000, 0x80014074)`, stack/end `0x80fff000`;
- level-123 image size: 13638 text, 12 data and 68372 BSS bytes;
- 100 ms fast regression: PASS, DMA IRQ asserted at about 7.73 ms modeled
  time, exactly one source-7 notification, all 32 bytes matched;
- 12 ms detailed smoke: the same DMA/IRQ/data markers passed;
- both modes reported zero synthetic transactions, RAM writes and DMA-register
  writes and no DMA abort;
- fast instrumentation recorded 200211 completed firmware transactions and
  2070765 estimated no-contention network cycles;
- detailed instrumentation recorded 175395 completed firmware transactions
  and 1814686 measured network cycles.

The fixed-window counters are labelled observations, not pass thresholds or
contention claims. The expanded firmware DMA register definitions are
compile-time checked against `dma_tlm.h` with:

```bash
CHECK_REGS_SKIP_NPU=1 ./tools/check_regs_drift.sh
```

The explicit flag skips only the optional private NPU scope that is absent from
this workspace; the default drift command remains strict when that model is
available. Full CTest registration and platform negative controls remain Steps
12.5 and 12.6. At this sign-off boundary Step 12.4 was next; it is now
completed below.

## 8. Step 12.4 — DONE (2026-08-03): concurrent SoC workload

Run a bounded workload in which:

- two or more CPU tasks execute and access RAM/MMIO;
- the CLINT tick remains active;
- TIMER0 continues generating PLIC interrupts;
- DMA transfers run concurrently with CPU traffic;
- UART0 reports progress without becoming the synchronisation mechanism.

Required checks:

- no unexpected trap;
- no PLIC source is left claimed;
- no lost DMA completion;
- no data corruption;
- no deadlock or watchdog expiry;
- forward progress for every task;
- zero synthetic survey transactions in firmware mode.

Use `--noc-timing fast` for long RTOS runs. Detailed mode is the cycle-stepped
calibration/reference path; it intentionally spends simulated time and is not
the supported Bremen temporal-decoupling path. Run only a short, bounded
detailed-mode comparison where its quantum-keeper limitation is respected.

Fast mode proves functional long-run integration and no-contention timing. It
must not be used to make claims about congestion, router back-pressure, packet
locks, FIFO occupancy or clock-gating behavior.

### 8.1 Implemented phase

Implemented as level 124, which was the default at this step and remains
separately buildable after level 128 became the Step 12.8 default. Levels 121,
122 and 123 also remain reproducible and their runners still pass unchanged.

After `DMA NoC PASS` a supervisor task at priority 2 opens a bounded concurrent
phase. Its participants are:

- two CPU tasks at priority 1, each owning a 64-word RAM buffer. Every
  iteration reads the previous generation back before writing the next, so
  corruption from the other worker, from DMA or from the network is caught on
  the following pass. When the phase stops, each worker also reads back its
  final generation explicitly; without that teardown check the last writes
  have no following iteration to verify them. Each worker also reads one
  NoC-crossing MMIO register per iteration and checks the value: CLINT `mtime`
  must not go backwards for one worker, the PLIC priority register of source 4
  must read the programmed value for the other;
- TIMER0, left free-running through PLIC source 4;
- the DMA manager, repeatedly transferring 4 KiB with completion through PLIC
  source 7;
- the supervisor, which is the only task that prints.

### 8.2 Two design points that were not optional

**The concurrent transfer is not the Step 12.3 transfer.** A 32-byte transfer
completes in about 0.5 us of modeled time, which is shorter than one CPU-task
loop iteration. A first implementation that reused it passed while the CPU
tasks made *zero* progress during the transfers — the acceptance was measuring
coexistence, not concurrency. The concurrent phase therefore uses its own
program built from a DMALP loop over 16-byte bursts, giving a window the
lower-priority tasks demonstrably execute in. The Step 12.3 objects are
untouched so that acceptance stays exactly as signed off.

**TIMER0 was slowed for the phase.** At the Step 12.2 rate of 5000 counts it
interrupts every 50 us, and an ISR entry plus PLIC claim/complete plus a
context switch costs a comparable amount of modeled time. The phase was
interrupt saturated: the CPU tasks barely advanced and what was being measured
was trap handling. The phase reprograms TIMER0 to 200 us. It remains a real
periodic interrupt load; it stops being the only load.

### 8.3 How each required check is met

| Requirement | Evidence |
|---|---|
| forward progress for every task | the supervisor requires both workers and the tick to advance at **every** checkpoint, not only end to end |
| DMA concurrent with CPU traffic | worker words counted strictly between a DMA launch and its completion interrupt, a window in which the DMA task is blocked |
| CLINT tick active | the supervisor's own bounded `vTaskDelay` checkpoints plus a strict tick-advance check |
| TIMER0 keeps interrupting | a phase-total floor, plus a loop-scoped overlap count |
| no lost DMA completion | `dma_irq_count` must equal exactly one per sequential plus one per concurrent transfer |
| no data corruption | worker generation check per word, including an explicit final-generation readback after phase stop; per-transfer DMA patterns with both source and destination compared |
| no PLIC source left claimed | sources 4 and 7 kept re-delivering, impossible under the model's gateway unless every claim was completed, plus a final claim/pending probe that must read idle |
| no deadlock or watchdog expiry | every wait is timeout-bounded and the checkpoint count is capped at 60 |
| no unexpected trap | the runner rejects `FAIL`, `FATAL`, trap diagnostics and terminal PC zero |
| zero synthetic traffic | the runner requires zero survey transactions, RAM writes and DMA register writes |

The TIMER0 overlap figure is deliberately weaker than the CPU one: it is scoped
to the whole transfer loop rather than to one transfer, because a 200 us period
cannot be required to fall inside a 50 us window without asserting a ratio of
periods instead of concurrency.

### 8.4 Sign-off evidence

`fw/freertos_noc_soc/run_step_12_4.sh` builds a private level-124 image,
validates its ELF, applies a 120-second host timeout, requires the concurrent
markers in strict order after the Step 12.3 acceptance, and enforces the
counters as floors rather than fixed values — iteration and interrupt counts
depend on the scheduling of a genuinely concurrent phase and must not become
timing thresholds.

Observed on 2026-08-03:

- ELF entry `0x80000000`, one `PT_LOAD` at
  `[0x80000000, 0x80017678)`, stack/end `0x80fff000`;
- 500 ms fast regression PASS in about 9 s of host time, with 4 checkpoints,
  4 concurrent DMA transfers and 16384 concurrent bytes;
- per-worker phase totals 39 and 40 iterations (2496 and 2560 words), 100
  TIMER0 interrupts;
- in-flight overlap 24 and 10 worker words, and 30 TIMER0 interrupts across the
  transfer loop;
- zero synthetic transactions, RAM writes and DMA register writes; no DMA
  abort, failure, trap diagnostic or PC zero;
- byte-identical counters across repeated runs.

A single bounded detailed-mode comparison was then run on the same image, and
reached the same acceptance rather than only a partial smoke:

- 45 ms modeled window, 404 s of host time;
- the same 4 checkpoints, 4 transfers and 16384 concurrent bytes, and the same
  100 TIMER0 interrupts and 2496 worker words for cpu-a;
- in-flight overlap 32 and 32 worker words, and 31 TIMER0 interrupts across the
  transfer loop — higher and better balanced than in fast mode;
- 779680 completed firmware transactions, 8124026 measured network cycles and
  15788268 mesh clock cycles;
- zero synthetic traffic and no abort, failure or trap diagnostic.

That is roughly 45x the host cost of the fast run for the same acceptance,
which is why fast mode owns the regression window and detailed mode is run
once. The detailed figures are measured rather than estimated cycles, but they
remain observations for a fixed window: this workload was not constructed to
create contention, and none of these numbers is a threshold.

The phase counters are labelled observations for a fixed window, not
performance thresholds or contention measurements.

Five of the checks were shown to be load-bearing while developing the step,
which is the reason they are worded the way they are:

| Check | What it caught |
|---|---|
| in-flight worker overlap | a first version in which all four transfers completed with the CPU tasks at zero progress |
| per-checkpoint progress | a worker that had not completed a whole iteration inside one checkpoint interval, which is why progress is counted in words |
| loop-scoped TIMER0 overlap | a per-transfer version that failed because a 200 us period does not fit a 50 us window |
| final worker readback | corruption of the final generation after the iterative check, at a boundary with no next iteration |
| overlap evidence | losing one worker's in-flight progress turns concurrency back into mere coexistence |

The first three are development observations. The last two are registered
mutation controls in the executable Step 12 registry in section 10.

## 9. Step 12.5 — DONE (2026-08-03): automated regression and packaging

Add a self-checking runner and register it in CTest. It must:

- build the firmware with `NPU=0`;
- validate the ELF32 load ranges before simulation;
- run `noc_soc` in explicit firmware and fast-timing modes;
- enforce a host timeout;
- retain the full log on failure;
- require all acceptance markers;
- reject any `FAIL`, unexpected trap, watchdog expiry or synthetic traffic;
- run against the development binary and the installed/package consumer where
  applicable.

The current `noc_soc` CLI accepts `--sim-us`, not the FX1 platform's
`--sim-ms` and `--quantum` options. A 700 ms modeled window is therefore
expressed as:

```text
--mode firmware --noc-timing fast --sim-us 700000
```

Do not copy the existing FX1 runner unchanged. Adapt it to the actual
`noc_soc` command-line and timing contracts.

The required final markers are:

```text
FreeRTOS NoC boot
CLINT tick OK
PLIC TIMER0 OK irqs=3
DMA NoC PASS
FreeRTOS NoC Step 12.4 concurrent
FreeRTOS NoC concurrent OK checkpoints=<n>
FreeRTOS NoC PASS
```

`fw/freertos_noc_soc/run_step_regression.sh` implements this contract for
levels 121, 122, 123 and 124. Step 12.8 later extended the same oracle with
level 128 rather than creating a second copy.

### 9.1 Implementation

`platforms/noc_soc/tests/run_freertos_regression.sh` is a deliberately thin
platform-level wrapper. It does **not** restate the acceptance contract: a
second oracle would drift from the first, and the drift would be discovered by
one of them passing something the other rejects. What the wrapper adds is the
platform level — it now drives all five acceptance levels (121, 122, 123, 124
and 128), so earlier stages stay independently reproducible rather than merely
being asserted. It accepts an externally supplied `NOC_SOC_BIN` so the
packaging gate can point it at the installed package consumer, and it keeps
every log on failure with the path reported.

Registered as:

```text
noc_soc_freertos_regression   labels: firmware;freertos
                              SKIP_RETURN_CODE 77, TIMEOUT 900
```

The skip is deliberate and matches the existing platform tests: a machine
without the RISC-V cross-toolchain cannot build the firmware and must not
report a red test it could never have run. The toolchain check happens in the
wrapper as well as in the step runner, so a skip is never reported after a
level has already passed.

`run_packaging_regression.sh` now runs level 128 against the packaged executable
with `LD_LIBRARY_PATH` unset. Level 128 retains every level-124 marker and adds
UART host replay/CLI acceptance. Only the strongest level is run there: that
gate asks whether the packaged binary boots and runs the shipping FreeRTOS
profile, not whether earlier stages are separately reproducible.

### 9.2 What was already satisfied, and by what

| Requirement | Where it is met |
|---|---|
| build the firmware with `NPU=0` | `fw/freertos_noc_soc/Makefile` forces `NPU=0 TFLM=0` |
| validate the ELF32 load ranges before simulation | `check_elf_contract.sh`, run by the `check` target the step runner invokes |
| explicit firmware and fast-timing modes | `--mode firmware --noc-timing fast`, both named on the command line |
| enforce a host timeout | `timeout --kill-after=10 120` per run |
| retain the full log on failure | private log directory per step, path printed on failure |
| require all acceptance markers | ordered marker contract per level, including the Step 12.4 concurrent set and Step 12.8 CLI |
| reject `FAIL`, unexpected trap, watchdog, synthetic traffic | forbidden-text scan plus the zero-ownership checks |
| development and packaged consumer | `noc_soc_freertos_regression` and the packaging gate respectively |

### 9.3 Sign-off evidence

Observed on 2026-08-03, in a private `-DCDC_BUILD_TESTS=ON` build so the
developer's build cache was not modified:

- `ctest -N -R noc_soc` lists `noc_soc_firmware_regression`,
  `noc_soc_freertos_regression` and `noc_soc_packaging_regression`;
- the original four-level regression passed in about 13.6 s; the current
  five-level regression also includes level 128, with the `firmware` and
  `freertos` labels applied;
- the platform target builds with zero warnings;
- the packaging gate passes with level 128 against the packaged executable,
  including deterministic UART replay and CLI responses.

Step 12.5 does not register the detailed-mode comparison. Detailed mode costs
roughly 45x the host time for the same acceptance, which belongs in a one-off
measurement rather than in a gate that must stay cheap enough to run often.

## 10. Step 12.6 — DONE (2026-08-03): qualify the platform regression with negative controls

The positive FreeRTOS regression must be demonstrated to fail on defects that
break each major acceptance path. Acceptance markers alone are not evidence:
firmware that always prints PASS is the easiest test in the world to satisfy.

Create a platform-level runner:

```text
platforms/noc_soc/tests/run_freertos_negative_controls.sh
```

Do not add these controls directly to
`components/floo_noc_model/rtl_crosscheck/run_negative_controls.sh`. That
runner intentionally copies and builds only the component. The Step 12 controls
span platform code and firmware, so their runner must create a private copy of
the required repository scope under `/tmp`, build there and leave the working
tree untouched.

The unmodified positive regression must pass first. Each control is then
detected only when:

1. the exact mutation needle is found once;
2. the mutated platform and firmware still build;
3. the model runs under the normal modeled-time and host-time bounds;
4. the positive regression returns nonzero;
5. it emits the expected control-specific failure diagnostic.

A compilation error, unrelated trap, arbitrary nonzero exit or generic host
timeout does not count as detection. A missing marker may count only when the
self-checking runner names that exact missing marker and the clean run has
already proved that it is normally produced.

The minimum Step 12 registry is:

| Control | Injected defect | Required detection |
|---|---|---|
| `freertos-dma-destination-corrupted` | program a valid but wrong DMA destination below the reserved page | byte comparison fails and `DMA NoC PASS` is absent |
| `freertos-plic-timer-source-wrong` | register or enable TIMER0 under the wrong PLIC source | bounded TIMER0 notification timeout and no `PLIC TIMER0 OK irqs=3` |
| `freertos-plic-complete-omitted` | omit or corrupt the claim completion write while leaving claim and device acknowledgement intact | the named PLIC progress/marker check fails; an unclassified host hang is not sufficient |
| `freertos-clint-tick-disabled` | prevent `mtimecmp` from producing the scheduler tick | the runner reports the missing `CLINT tick OK` marker within an independent platform/host bound |
| `firmware-survey-ownership-broken` | allow at least one synthetic survey access in firmware mode | the zero-transaction/RAM-write/DMA-write ownership check fails |
| `freertos-concurrent-final-buffer-corrupted` | corrupt the final worker generation after the iterative checks have stopped | the explicit teardown readback reports the exact corrupted word |
| `freertos-concurrent-overlap-lost` | erase one worker's measured progress inside the DMA-active window | the overlap acceptance rejects coexistence presented as concurrency |
| `freertos-cli-uart-rx-disabled` | leave the CLI task present but do not arm UART0 RX or PLIC source 1 | Step 12.8 rejects the missing RX-arm marker and no host command reaches the parser |
| `freertos-hw-scan-identity-wrong` | change one expected peripheral identity while retaining the real MMIO read | `hw_scan` prints `MISMATCH` and cannot produce aggregate PASS |
| `freertos-reg-test-rw-write-bypassed` | restore the old value instead of writing the requested RW test pattern | `reg_test` reports failed RW rows and cannot produce aggregate PASS |

The PLIC controls are deliberately separate. A wrong source proves that the
claim/dispatch path is observed; a missing completion proves that the
claim/complete contract is observed. Combining them into one mutation would
leave one half unqualified.

Register the runner in CTest with an extended/negative-control label. The final
Step 12 sign-off must record the number detected and missed; every registered
control must build, run and fail for its own expected reason.

### 10.1 Implementation

`platforms/noc_soc/tests/run_freertos_negative_controls.sh`, registered as:

```text
noc_soc_freertos_negative_controls
    labels: negative-controls;extended;freertos
    RUN_SERIAL, SKIP_RETURN_CODE 77, TIMEOUT 1800
```

Every control is now judged against Step 12.8 — the default build and the level
the packaging gate runs. Level 128 retains the complete level-124 workload and
adds the UART CLI, so the registry qualifies the strongest image that actually
ships rather than a cheaper stand-in.

Two private scopes, because the cost differs by an order of magnitude:

- `firmware` copies `fw/` plus the pinned FreeRTOS kernel and rebuilds only the
  firmware against the platform binary under test;
- `platform` copies the build scope and configures and builds `noc_soc`
  privately. `third_party/tflite-micro` is 300 MB and unreachable from this
  platform or from an `NPU=0 TFLM=0` firmware, so it is excluded rather than
  copied.

The working tree is never touched, and the mandatory clean run happens first: a
registry qualifying a regression that does not currently pass proves nothing.
That clean run also verifies every marker a control expects to lose is
currently produced.

### 10.2 Two expectations per control, not one

Each control records what the **runner** must say and what the **platform log**
must contain. Checking only the exit code would score a mutation that merely
broke the build the same as one that broke behaviour.

Some defects produce no firmware diagnostic at all — a scheduler tick that
never arrives simply leaves tasks blocked — so a log expectation may be written
`!text`, meaning that text must be absent, and the clean run is required to have
produced it. The evidence stays positive in both directions rather than
degenerating into "the run failed somehow".

A related fix went into the acceptance runner itself: `FreeRTOS NoC PASS` is now
checked **after** the specific prerequisites. The final PASS is the least
informative marker in the log — it is absent whenever anything upstream failed —
so reporting it first named the symptom for every possible defect. Requiring the
specific markers first means a failing run says which stage broke, which is what
makes `freertos-clint-tick-disabled` able to name `CLINT tick OK`.

### 10.3 Registry as implemented

| Control | Scope | Mutation | Runner names | Log evidence |
|---|---|---|---|---|
| `freertos-dma-destination-corrupted` | firmware | DMA destination moved 8 MiB into RAM, still mapped and below the reserved page | `DMA NoC IRQ source=7 count=1` | `DMA NoC FAIL: mismatch byte=` |
| `freertos-plic-timer-source-wrong` | firmware | TIMER0 enabled under PLIC source 5, handler left on 4 | `PLIC TIMER0 OK irqs=3` | `PLIC TIMER0 FAIL: timeout after 0 irqs` |
| `freertos-plic-complete-omitted` | firmware | the claim completion write removed; claim and device W1C intact | `PLIC TIMER0 OK irqs=3` | `PLIC TIMER0 FAIL: timeout after 1 irqs` |
| `freertos-clint-tick-disabled` | firmware | the `mtimecmp` write removed from the port | `CLINT tick OK` | `!NoC tick wake=` |
| `firmware-survey-ownership-broken` | platform | one synthetic probe write in firmware mode | `  transactions 0` | `  RAM writes 1` |
| `freertos-concurrent-final-buffer-corrupted` | firmware | final worker generation corrupted after the normal loop stops | `NoC concurrent DMA transfers=4` | `NoC concurrent FAIL: final RAM word=0` |
| `freertos-concurrent-overlap-lost` | firmware | cpu-a's recorded in-flight progress forced to zero | `NoC concurrent DMA transfers=4` | `NoC concurrent FAIL: overlap cpu-a=0` |
| `freertos-cli-uart-rx-disabled` | firmware | `uart_rx_start()` removed while the CLI task and release notification remain | `FreeRTOS NoC CLI RX armed source=1` | `FreeRTOS NoC CLI RX deliberately disabled` |
| `freertos-hw-scan-identity-wrong` | firmware | WDT PID expectation changed while its real MMIO value is unchanged | `HW_SCAN PASS implemented=22 reserved=3 absent=2` | `MISMATCH` |
| `freertos-reg-test-rw-write-bypassed` | firmware | the restoring RW helper writes the old value instead of the pattern | `REG_TEST PASS 37/37` | `REG_TEST FAIL` |

The two PLIC controls stay separate deliberately: `after 0 irqs` versus
`after 1 irqs` is exactly the difference between a claim/dispatch fault and a
claim/complete fault, and one mutation covering both would leave half of it
unqualified.

### 10.4 Two findings from writing them

Both came from a control failing rather than from reasoning, and both are worth
keeping:

- **The first DMA mutation qualified the wrong check.** `dma_destination + 4`
  ran off the end of the destination array into the *source* buffer, so the run
  failed on `DMA NoC FAIL: source corruption byte=0`. It was a detection of a
  real defect, but not of the one the control exists for. A wrong destination
  has to land where nothing else is verified.
- **The CLINT control cannot be written on the firmware side.** Every
  firmware-owned way of breaking it — the CLINT base, the `mtimecmp` offset,
  the FreeRTOS config macro — is already rejected at compile time by the
  `_Static_assert` pair in `main.c`. The misconfiguration class of this defect
  cannot reach a run at all, which is a real strength of the existing contract;
  the only way to exercise the runtime path is to remove the `mtimecmp` write
  itself, in the private copy.

### 10.5 Sign-off evidence

Current result, re-run on 2026-08-05 against Step 12.8:

```text
clean run PASS; every expected marker is present
detected freertos-dma-destination-corrupted (firmware scope)
detected freertos-plic-timer-source-wrong (firmware scope)
detected freertos-plic-complete-omitted (firmware scope)
detected freertos-clint-tick-disabled (firmware scope)
detected firmware-survey-ownership-broken (platform scope)
detected freertos-concurrent-final-buffer-corrupted (firmware scope)
detected freertos-concurrent-overlap-lost (firmware scope)
detected freertos-cli-uart-rx-disabled (firmware scope)
detected freertos-cli-dashboard-marker-omitted (firmware scope)
detected freertos-hw-scan-identity-wrong (firmware scope)
detected freertos-reg-test-rw-write-bypassed (firmware scope)

FreeRTOS negative controls: 11 detected, 0 missed
```

The clean regression and all eleven controls were re-run after the
host-assisted dashboard command was added to Step 12.8. The
first version of the original five-control registry scored 3 detected and 2
missed; both misses were wrong expectations, not weak checks, and are recorded
in section 10.4.

## 11. Step 12.7 — DONE (2026-08-03): record a NoC and RTOS measurement baseline

Step 12 must leave at least a small, reproducible measurement artifact. This is
diagnostic evidence, not a timing-closure claim, and the first version has no
hard performance threshold.

Run a short deterministic workload in `--noc-timing detailed`. Record:

- CPU-port network latency for PLIC claim and PLIC complete as separately
  classified MMIO operations;
- CPU RAM-access network latency before DMA starts and while DMA is active,
  including the contention delta;
- modeled time from DMA start to its completion interrupt;
- completed transactions, total NoC latency and
  `peak_outstanding_transactions(port)` for every active manager;
- the modeled window, network clock, floorplan, timing mode, build type and
  firmware revision accompanying every number.

`last_latency_cycles(unsigned port)` is already exposed by
`noc_interconnect`, but it holds only the most recent completion for that
manager. Under concurrent CPU traffic it may be overwritten by another CPU
transaction before a platform monitor reads it. PLIC claim/complete numbers
must therefore be captured at the classified transaction-completion point or
through new per-port/per-target aggregate counters; polling the global
`last_latency_cycles()` later is not acceptable attribution.

`router_counters::output_utilisation()` and `mean_occupancy()` exist in
`noc_counters.hpp`, but they are not currently exposed through
`noc_interconnect`. If Step 12 exposes them, keep the observer passive and
report at least:

- accepted flits and packets;
- stall cycles;
- input occupancy mean and high-water mark;
- output utilisation.

Those router metrics are meaningful only when the cycle-stepped mesh is active.
Fast mode bypasses that datapath and must not be used to report utilisation,
FIFO occupancy, stalls or contention. Fast remains the functional long-run
FreeRTOS mode; detailed mode owns the short measurement baseline.

Also record FreeRTOS tick interval minimum, maximum and maximum deviation from
the configured period. Label this as a **platform/RTOS metric**, not a pure NoC
metric: CLINT drives MTIP directly into the CPU, while the PLIC claim/complete
MMIO transactions actually cross the NoC.

Archive the baseline log in the regression evidence directory. Changes may be
reviewed against it, but no pass/fail threshold should be introduced until
repeatability across clean runs and host-load variation has been measured.

### 11.1 Attribution: a new observer, because polling could not work

The warning above was correct and it forced the design. PLIC claim and PLIC
complete are the **same target at the same address**, `0x0c20_0004`, separated
only by direction. No per-target counter can tell them apart, and polling
`last_latency_cycles()` afterwards may read a different manager's transaction
entirely.

`noc_interconnect` therefore gained a passive completion observer:

```cpp
struct completion { port; address; length; is_write; latency_cycles; at_cycle; };
void set_completion_observer(std::function<void(const completion&)>);
```

It fires where each transaction's latency becomes final, carrying enough to
classify without the interconnect knowing what any register means. The contract
is strict: cheap, must not throw, must not `wait()`, and may call nothing on the
interconnect but a `const` accessor — consuming time there would change the
numbers being measured.

`test_noc_interconnect_observer` pins it: one record per completion in both
timing modes, correct requester/address/length/direction, per-record latencies
summing to the interconnect's own total, and — running identical traffic with
and without an observer — no change to transaction count or measured latency.
The registered control `completion-observer-attribution-lost` reports every
completion as port 0 and must be detected, which is exactly the attribution loss
the observer exists to prevent.

### 11.2 What is measured, and how each classification is defined

`--noc-baseline` (firmware mode only) installs the observer and prints the
report. Classification:

| Bucket | Rule |
|---|---|
| PLIC claim | CPU port, read of `0x0c200004` |
| PLIC complete | CPU port, write of `0x0c200004` |
| RAM, DMA idle | CPU port, RAM range, DMA port had **no** transaction admitted at completion |
| RAM, DMA active | CPU port, RAM range, DMA port had **≥1** transaction admitted at completion |
| other CPU MMIO | CPU port, everything else |
| DMA manager | DMA port |

"While DMA is active" is a classification at the completion instant, not a
claim that the two overlapped for their whole flight. Stating the rule is better
than implying a stronger one.

DMA channel-start-to-interrupt uses two real platform observables. A passive
`dma_tlm` observer fires synchronously at the channel's architectural
`STOPPED -> EXECUTING` transition, before its worker is released, and an
`SC_METHOD` observes the DMA completion line rising. Timing from completion of
the CPU's `DBGCMD` transaction is wrong: by then the DMAGO instruction has
already started and scheduled the channel, so that timestamp silently omits
the trailing NoC response path. The DMA unit test pins both the channel
identity and the required callback-before-`b_transport`-return ordering.

### 11.3 Firmware level 125

Tick jitter needs a tick hook, and a hook that runs every tick and reads CLINT
`mtime` over the NoC is instrumentation. Folding it into level 124 would change
the very counters that image is signed on, so level 125 is level 124 plus the
hook, selected by the measurement runner alone. `configUSE_TICK_HOOK` became
overridable so every existing image keeps its default of 0. The first three
intervals are discarded as warm-up: the first is measured against a
zero-initialised timestamp and the scheduler is still starting tasks.

### 11.4 Router counters: deliberately not exposed

Section 11 makes this conditional — "**If** Step 12 exposes them". It does not.
`router_counters` is a standalone `sc_module` that must be bound to a router's
signals; `floo_router`, `floo_mesh` and `axi_noc` neither instantiate nor
expose it. Wiring one per router would mean adding 2 × W × H modules and their
bindings inside the RTL-signed composition, for an explicitly optional item. The
baseline says so in its own text rather than leaving a reader to wonder, and
reports instead what the cycle-stepped path already exposes: mesh clocked
cycles, clock-gate transitions and mesh-quiescent-while-wrapper-busy cycles.

### 11.5 Sign-off evidence

`platforms/noc_soc/tests/run_freertos_baseline.sh`, registered as
`noc_soc_measurement_baseline` with labels `baseline;extended;freertos`,
`RUN_SERIAL`, skip 77, timeout 2400. It is a **completeness check, not a
threshold test**: it verifies every required number was produced and is well
formed, that the workload actually completed, and that detailed mode was used —
then archives the artifact with its provenance to
`platforms/noc_soc/evidence/noc_baseline.txt`. A manual sign-off run promotes
the artifact there; the CTest registration instead sets `BASELINE_DIR` to its
binary-tree `evidence/` directory so a test never writes into the source tree.

The completeness schema covers the modeled window, network clock, floorplan,
build type and firmware identity; every classified latency bucket; the peak
outstanding count of each manager; aggregate and mesh/gating counters; and the
configured RTOS tick period, sample count, interval extrema and deviation. The
repository state is derived from `git status --porcelain`, including untracked
files, so an artifact cannot be labelled `clean` merely because its new source
files have not been added to Git yet. For a dirty checkout the runner also
stores `noc_baseline.source.patch` and its SHA-256, covering tracked and
untracked source while excluding generated evidence. Together with the
recorded revision, firmware hash and platform-binary hash, that patch identifies
the exact build input rather than merely saying `dirty`.

The authoritative numeric record is the archived artifact rather than a copy
in this roadmap. This avoids letting documentation drift when the diagnostic
run is deliberately regenerated. Its DMA interval is explicitly labelled
`channel start to completion interrupt`; artifacts with the older
`dispatch to completion interrupt` label used the late DBGCMD-completion
timestamp and are superseded.

One result is worth keeping for its own sake. The same workload in **fast** mode
reports `RAM, DMA active: n=0` — the bucket is empty by construction, because
fast mode completes each call inside its own `b_transport` and never has two
managers in flight at one instant. That is a concrete demonstration, not an
assertion, that fast mode cannot answer a contention question.

No threshold is attached to any number above, and none should be added until
repeatability across clean runs and host-load variation has been measured.

## 12. Step 12.8 — DONE (2026-08-05): UART RX and interactive FreeRTOS CLI

Step 12.8 closes the deferred UART-console item without weakening the signed
level-124 workload.

### 12.1 Platform host bridge

`noc_soc` now links the existing `components/uart_host_tlm` bridge to UART0's
pin-side RX/TX signals. It is idle unless one of two firmware-only options is
selected:

- `--uart0-rx-file FILE [--uart0-rx-delay-us N]` for deterministic CI replay;
- `--uart0-socket PORT [--uart0-wait]` for a bidirectional loopback TCP
  console.

Host bytes enter `UartTLM` rather than a firmware queue. The architectural path
is therefore:

```text
host -> UART0 RX FIFO -> PLIC source 1 -> FreeRTOS ISR queue -> CLI task
```

The CPU's UART register accesses and PLIC claim/complete operations cross
FlooNoC. Invalid option combinations are rejected before SoC construction:
wait without a socket, replay delay without a file, non-decimal/out-of-range
ports, and any host input in survey mode.

### 12.2 Firmware level 128

Level 128 is the new default. It keeps all scheduler, CLINT, TIMER0/PLIC,
sequential DMA and concurrent level-124 checks. A low-priority CLI task arms
the existing interrupt-driven UART0 BSP early, then blocks on a direct task
notification. The supervisor sends that notification only after
`FreeRTOS NoC PASS`, so command output cannot substitute for incomplete
bring-up.

The final command set is deliberately narrow:

- `help` lists the available CLI commands;
- `soc` reports the fixed 4x4 topology, manager placement and key addresses;
- `noc_dashboard` requests a live metrics snapshot and full dashboard from the
  host-assisted SystemC/Python path;
- `hw_scan` checks stable identities/state for 22 mapped blocks, reports
  ISP/VPU/NPU as reserved and reports IFLASH/PMU as absent without touching
  their unmapped addresses;
- `reg_test` runs 37 restoring RO/RW/W1C checks. RW state is restored and
  verified; functional TIMER1/ADC/QSPI causes stay masked and are cleared.

The earlier `status`, `irq`, `uptime` and `exit` commands were removed at the
user's request. `help` and the banner both list the five remaining commands,
and a manual session ends through the VP time bound or `Ctrl-C`.

The dashboard command intentionally does not format the report in firmware.
Its private UART TX record asks detailed `noc_soc` to atomically publish a live
`floo-noc-metrics-v1` file. `tools/noc_cli.py` removes the private record from
the terminal stream and invokes the canonical `tools/noc_dashboard.py`
renderer. This requires `--noc-timing detailed`, `--noc-metrics FILE` and a
TCP UART connection. The result is the complete dashboard, but the active
firmware window remains `drained: false` and cannot be selected as a DSE
winner.

Level 125 remains separate and unchanged: it is level 124 plus the Step 12.7
tick hook. Level 128 does not enable that measurement instrumentation.

### 12.3 Acceptance and evidence

`fw/freertos_noc_soc/run_step_12_8.sh` replays `help`, `soc`,
`noc_dashboard`, `hw_scan` and `reg_test` after a 10 ms modeled delay. The
runner requires every level-124 prerequisite, the platform's real UART0
interrupt assertion, the dashboard command plus the expected fast-mode
unavailable diagnostic, the complete scan/register results, absence of removed
command text, and this strict order:

```text
FreeRTOS NoC PASS
FreeRTOS NoC CLI ready
Commands:
CLI soc mesh=4x4 managers=cpu,dma,probe
CLI noc_dashboard host request
noc_soc live dashboard unavailable: live dashboard requires detailed mode and --noc-metrics FILE
HW_SCAN PASS implemented=22 reserved=3 absent=2
REG_TEST PASS 37/37
```

The platform regression now runs levels 121, 122, 123, 124 and 128. The
packaging regression runs level 128 against the self-contained binary with
`LD_LIBRARY_PATH` unset. Both passed from fresh `/tmp` trees with GCC/G++ 11.5.

The Step 12 control registry is now 11 detected, 0 missed. Its four CLI/diag
mutations remove `uart_rx_start()`, omit the dashboard's private UART request,
corrupt a scan identity, and bypass the RW pattern write. They prove acceptance
is sensitive to the UART RX/PLIC path, the host dashboard handshake and both
diagnostic result paths rather than merely to a command banner.

The pre-existing `uart_host_bridge` unit test passed deterministic replay,
TCP RX and TCP TX. `noc_soc_cli_dashboard_protocol` additionally passes every
marker split, back-to-back markers, atomic metrics replacement and exact
UART/dashboard ordering over a real loopback TCP session. The scripted
platform-level run now exercises all four non-help commands over UART0 and
produces the complete 22-block scan and 37/37 restoring register result.

Interactive use must wait for `FreeRTOS NoC CLI ready` before sending data.
`--uart0-wait` only waits for a TCP connection at simulation time zero; bytes
sent before firmware arms RX can overflow the UART model's 16-byte hardware
FIFO. Fast mode also advances modeled time much faster than wall time, so a
human session needs a deliberately long `--sim-us` bound.

## 13. Deferred work after Step 12

The following are useful later, but are not part of the first FreeRTOS gate:

- a real reset-to-boot-ROM-to-RAM boot chain instead of backdoor ELF loading;
- additional RTOS drivers for one serial and one stateful peripheral;
- sustained contention experiments in detailed mode;
- an end-to-end RTL harness for the complete composed datapath;
- enabling `MaxUniqueIds > 1` or a reorder buffer;
- ISP, VPU, NPU or PMU integration;
- Zephyr or Linux bring-up.

Linux is not the immediate target. It would add MMU, privilege, device-tree,
bootloader and driver-stack dependencies before the existing hardware/software
contract has been proven under a small RTOS.

## 14. Final definition of done — MET, EXTENDED (2026-08-05)

Every item below is now satisfied. The evidence for each is in the step section
named beside it.

| Requirement | Where |
|---|---|
| Gate V0-CLOSE met and documented | section 3; the registry has since grown to 51/51 |
| FreeRTOS boots on `noc_soc` with the NPU absent | section 4 |
| scheduler context switching demonstrated | section 5 |
| CLINT drives the RTOS tick | section 6.3 |
| TIMER0 handled through PLIC source 4 | section 6.4 |
| DMA completes through PLIC source 7, data verified | section 7 |
| combined workload makes bounded forward progress | section 8 |
| fast-mode regression passes from a clean build and package | section 9 |
| every Step 12 control builds, runs and fails for its own reason, zero missed | sections 10 and 12: 11 detected, 0 missed |
| detailed NoC baseline and separately labelled tick-jitter baseline archived | section 11 |
| UART0 RX and interactive FreeRTOS CLI work through PLIC source 1 | section 12 |
| detailed and fast timing limitations stated accurately | sections 8.2, 11.4, 11.5 |

The original wording of the list, kept because a definition of done that gets
edited after the fact is not one:

- Gate V0-CLOSE is met and documented, including both direct leaf tests and
  the 41/41 component negative-control result;
- FreeRTOS boots on `noc_soc` with the NPU absent;
- scheduler context switching is demonstrated;
- CLINT drives the RTOS tick;
- TIMER0 is handled through PLIC source 4;
- DMA completes through PLIC source 7 and its data is verified;
- the combined workload makes bounded forward progress;
- the fast-mode automated regression passes from a clean build and package;
- every registered Step 12 platform/firmware negative control builds, runs and
  fails for its own expected reason, with zero missed;
- a short detailed-mode NoC baseline and a separately labelled RTOS tick-jitter
  baseline are archived without unsupported timing-closure claims;
- limitations of detailed and fast timing remain stated accurately.

Passing only the boot banner is not FreeRTOS bring-up completion. Passing the
full list above establishes the next platform milestone, and it is now reached:
**SoC software bring-up complete on the FlooNoC-backed Virtual Platform**.

What this milestone does **not** claim, restated so the next reader does not
have to infer it: there is still no monolithic RTL harness for the composed
TLM-adapter-to-mesh-to-adapter path, fast mode still models no contention, the
router utilisation and FIFO-occupancy counters are still not wired through the
mesh, and no measured number here carries a pass/fail threshold. The remaining
deferred list is in section 13.
