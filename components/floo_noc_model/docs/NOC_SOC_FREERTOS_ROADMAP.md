# FlooNoC SoC FreeRTOS Bring-Up Roadmap

**Status:** Proposed post-Step-11 roadmap, explicitly requested on 2026-08-01  
**Scope:** Close the last original v0 verification criterion, then bring up
FreeRTOS on `platforms/noc_soc` without importing the NPU  
**Primary platform:** `platforms/noc_soc`  
**Firmware starting point:** `fw/freertos_fx1`, built with `NPU=0`

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

## 3. Gate V0-CLOSE — close the last original v0 criterion

### 3.1 Confirmed gap

The finding is valid. Section 15 of `docs/AI_HANDOFF_CONTEXT.md` still marks
this criterion as unmet:

> every included leaf has a direct standalone SystemC test

The two missing direct tests are:

- `include/floo_noc_model/rr_arb_tree.hpp`;
- `include/floo_noc_model/meta_buffer.hpp`.

Both leaves already have meaningful indirect RTL coverage:

- `rr_arb_tree.hpp` is exercised by the 152-cycle wormhole-arbiter
  cross-check at 5, 4 and 2 routes;
- `meta_buffer.hpp` is exercised by the 221-cycle chimney subordinate-side
  cross-check, including multi-entry FIFO ordering controls.

Therefore this is not evidence of an untested datapath or an unknown RTL
mismatch. It is a modular-testability and v0 definition-of-done gap. It must
still be closed before starting Step 12.

### 3.2 Required implementation

Add:

```text
components/floo_noc_model/tests/test_rr_arb_tree.cpp
components/floo_noc_model/tests/test_meta_buffer.cpp
```

Register both through `add_floo_model_test(...)` in
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

### 3.3 Mandatory executable controls for the two new leaf tests

The prose requirement above is not sufficient evidence. Register these two
controls in `rtl_crosscheck/run_negative_controls.sh` after the direct tests
exist:

| Control name | Mutation | Required observing test | Expected reason |
|---|---|---|---|
| `rr-arb-tree-wrap-ignored` | break the lower-mask wrap path in `rr_arb_tree::next_rr()` while keeping the source buildable | `test_rr_arb_tree` | the wrap-around decision is wrong |
| `meta-buffer-overflow-accepted` | remove or bypass the full check before a metadata push while keeping the source buildable | `test_meta_buffer` | the configured FIFO capacity is exceeded without the required overflow rejection |

At the time this roadmap was written, the component registry contained 39
executable controls. Closing this gate adds the two controls above, so the
expected result is:

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

### 3.4 V0-CLOSE acceptance

The gate closes only when:

1. both new executables build with the existing warning-as-error policy;
2. both appear in `ctest -N`;
3. both pass directly;
4. the complete component suite passes;
5. the negative-control runner reports all 41 registered controls detected and
   zero missed;
6. the existing RTL cross-check suites remain green;
7. `AI_HANDOFF_CONTEXT.md` and `STATUS.md` are updated from `NOT met` to `met`
   with the observed test totals, without claiming new RTL equivalence.

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

## 4. Step 12.0 — freeze the RTOS/platform contract

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
Create a `noc_soc` linker profile whose usable RAM is:

```text
ORIGIN = 0x80000000
LENGTH = 0x00fff000
END    = 0x80fff000
```

The firmware stack, heap, BSS and every ELF32 `PT_LOAD` range must remain below
`0x80fff000`.

Reuse the proven FreeRTOS 11.2.0 port and BSP from `fw/freertos_fx1` where the
contracts match, but create a separate `noc_soc` firmware target or linker
profile. Build it with `NPU=0`; PLIC source 17 remains tied low and must not be
accessed.

## 5. Step 12.1 — minimum FreeRTOS boot

Bring up the smallest observable RTOS image:

1. start at the firmware entry point;
2. initialise BSS and the boot stack;
3. print `FreeRTOS NoC boot` through polled UART0;
4. start the scheduler;
5. run at least two tasks at different priorities;
6. prove repeated context switches without traps or stalls.

Do not add the interactive CLI, DMA or optional peripheral tests at this stage.
The goal is to isolate startup, trap entry, task stacks and scheduler state.

## 6. Step 12.2 — CLINT tick and PLIC interrupt

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

## 7. Step 12.3 — DMA under FreeRTOS

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

## 8. Step 12.4 — concurrent SoC workload

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

## 9. Step 12.5 — automated regression and packaging

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
FreeRTOS NoC PASS
```

## 10. Step 12.6 — qualify the platform regression with negative controls

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

The PLIC controls are deliberately separate. A wrong source proves that the
claim/dispatch path is observed; a missing completion proves that the
claim/complete contract is observed. Combining them into one mutation would
leave one half unqualified.

Register the runner in CTest with an extended/negative-control label. The final
Step 12 sign-off must record the number detected and missed; every registered
control must build, run and fail for its own expected reason.

## 11. Step 12.7 — record a NoC and RTOS measurement baseline

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

## 12. Deferred work after Step 12

The following are useful later, but are not part of the first FreeRTOS gate:

- UART RX injection and an interactive FreeRTOS CLI;
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

## 13. Final definition of done

The roadmap is complete only when:

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
full list above establishes the next platform milestone: **SoC software
bring-up complete on the FlooNoC-backed Virtual Platform**.
