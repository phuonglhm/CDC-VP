# FX1 SoC virtual platform

Functional SystemC/TLM-2.0 platform for FX1 firmware and driver bring-up.
It is not cycle accurate and does not model silicon timing or bandwidth.

Plan and decisions: `VP_BK/ISP/plan/FX1_PLATFORM_FIX_PLAN.md` section 7 (outside this repo).

**New to the platform? Start with [docs/FW_GETTING_STARTED.md](docs/FW_GETTING_STARTED.md)**
(build, run, write firmware, get RAW/YUV out, debug). This README is the reference.

## Status — stage G5 (handoff)

| Block | Model | State |
|---|---|---|
| CPU | 2 × RISC-V VP++ RV32 harts (`cpu_models/riscv_vp_plusplus`), hart ID 0/1, CVA6 extension set RV32IMAFC (`misa` D and V removed), M-mode | in |
| Bus | `components/FX1_Components/bus`, `BusConfig::fx1()` | in |
| DDR | 512 MiB sparse RAM (`FX1_Components/sparse_ram`) | in |
| BootROM | 2 MiB read-only sparse RAM, unused by the one-image boot | in |
| CLINT | N-hart SiFive layout (`FX1_Components/clint`), `mtime` 20 MHz | in |
| UART | `components/uart2_tlm` (PL011-style), TX to stdout / log | in |
| sim-control | VP-only PASS/FAIL finisher plus 4 test interrupt lines (`FX1_Components/sim_control`) | in |
| PLIC | `FX1_Components/plic`: 7 sources, one M-mode context per hart, MEIP per hart | in (G2) |
| CPU port | `FX1_Components/cpu_port`: CPU payload contract check, completion, and bus-error → access-fault mapping; integration errors are kept loud | in (G2) |
| SYS_DMA | `FX1_Components/dma` (8 channels): CSR at `SYS_DMA_CSR`, master port `SYS_DMA`, IRQ = PLIC source 2, 20 ns power-on reset; request inputs tied to 0 (M2M only on the platform) | in (G3) |
| ISP | `FX1_Components/isp` TLM model: CSR at `ISP_CSR` (PERIBUS_1), masters `ISP_IDMA`/`ISP_ODMA`, IRQ = PLIC source 3, 20 ns power-on reset; driven by the reference C99 driver from firmware | in (G4a) |
| Exclusive monitor | `FX1_Components/exclusive_monitor`: real LR/SC reservations (64 B granule) shared by both harts, SYS_DMA and ISP ODMA; AMO/SC brackets exclude every writer. CPU side through `riscv_vp_plusplus_options::exclusive_monitor`, device side through a `WriteGuard` in front of the `SYS_DMA` and `ISP_ODMA` ports | in (G4b) |

## Memory and interrupt map

Single source: `components/FX1_Components/fx1_map/include/fx1/fx1_memory_map.h`.
It is included by the platform, the bus defaults, the firmware and the firmware
linker script. **All values are VP placeholders pending the FX1 HAS.** The map
differs from `platforms/VP_FX1_Full_SoC`, where 0x1001_0000 is I2C0 and
0x1002_0000 is SPI0.

| Region | Base | Size | Path |
|---|---|---|---|
| BootROM | 0x0000_0000 | 2 MiB | SYSBUS_1 |
| CLINT | 0x0200_0000 | 64 KiB | PERIBUS_0 (APB) |
| PLIC | 0x0C00_0000 | 16 MiB | PERIBUS_0 (APB) |
| UART | 0x1000_0000 | 64 KiB | PERIBUS_0 (APB) |
| SYS_DMA_CSR | 0x1001_0000 | 64 KiB | PERIBUS_0 (APB); DMA registers occupy 0x0000–0x10FF |
| SIM_CTRL (VP only) | 0x1002_0000 | 64 KiB | PERIBUS_0 (APB) |
| ISP_CSR | 0x1100_0000 | 64 KiB | PERIBUS_1 (APB) |
| DDR | 0x8000_0000 | 512 MiB | SYSBUS_1 |

PLIC source IDs:
- UART = 1, SYS_DMA = 2, ISP = 3.
- IDs 4–7 are reserved for NPU/H.264/ETH/MIPI. Until those IPs exist, they are
  driven by the four **VP-only test lines** of sim-control (`FX1_SIM_CTRL_IRQ_OFF`,
  bit i → source 4 + i).

PLIC context n is hart n in M-mode. CLINT and PLIC use the SiFive register layout.

## Build

Run from the CDC-VP root. Use a host compiler, not an EDA wrapper:

One command (configure, build, smoke, bus tests, regression; JUnit XML and
logs in `<build-dir>/fx1-ci-artifacts`, exit 0 only when every stage passed).
It sets every test option the gate needs (a reused build directory whose
cache turned one off is corrected) and refuses to run when a test listed in
`ci/required_tests.txt` for a stage that runs is not registered, or when that
manifest is missing, unreadable or empty; add new required tests there. The
full run needs `python3` with numpy (the regression stage's fixture check);
`--smoke-only` does not:

```bash
platforms/fx1_soc/ci/run_ci.sh [--build-dir DIR] [--jobs N] [--smoke-only]
```

By hand:

```bash
export CC=/usr/bin/gcc CXX=/usr/bin/g++ PATH=/usr/bin:/bin:$PATH
cmake -S . -B build-fx1 -G Ninja -DCMAKE_BUILD_TYPE=Release -DSYSTEMC_HOME=/opt/systemc-2.3.4 \
      -DCDC_BUILD_FX1_SOC=ON -DFX1_SOC_BUILD_FIRMWARE=ON -DCDC_BUILD_TESTS=ON
cmake --build build-fx1 -j1 --target fx1_all
ctest --test-dir build-fx1 -L "fx1_unit|fx1_smoke" --output-on-failure --no-tests=error
ctest --test-dir build-fx1 -R '^(bus_behavior|bus_config|bus_arbitration)$' --no-tests=error
ctest --test-dir build-fx1 -L fx1_regression --output-on-failure --no-tests=error
```

Build `fx1_all`, not the default target: it collects every FX1 target
(simulator, harnesses, FX1 component and VP++ backend tests, firmware) and
nothing else. The default target also builds `components/isp_tlm`, which
does not link today (`isp_register_bank_test`: no `sc_main`), unrelated to FX1.
Release matters for speed: an unoptimised build runs the simulator 2–3×
slower (spinlock test: 2.0 s vs 5.1 s).

Turning the other platforms off is optional. An FX1-only configuration needs
neither the Bremen `riscv-vp` checkout nor `cpu_models/cva6_vp`.

It does need:
- the pinned VP++ checkout in `third_party/riscv-vp-plusplus`; see `cpu_models/riscv_vp_plusplus/fetch_riscv_vp_plusplus.sh`;
- Boost headers.

### Firmware toolchain policy

`FX1_SOC_BUILD_FIRMWARE` (default ON) is the acceptance configuration.
Firmware and the `fx1_smoke` tests are mandatory, and configure **fails** when:
- no `riscv-none-elf-gcc` is found in `$RISCV_HOME/bin` or `/opt/toolchains/riscv-none-elf/bin`; or
- `FX1_RISCV_PREFIX` (e.g. `/opt/toolchains/riscv-none-elf/bin/riscv-none-elf-`)
  does not name a compiler that runs.

`-DFX1_SOC_BUILD_FIRMWARE=OFF` builds only the simulator and the unit tests. It
prints a warning and is **not** an acceptance configuration. Acceptance runs
use `ctest --no-tests=error`. The host compiler never builds firmware.

## Run

```bash
build-fx1/platforms/fx1_soc/fx1_soc --fw build-fx1/fw/fx1_soc/hello_2hart.elf \
    [--timeout-ms 1000] [--quantum-ns 1000] [--uart-log uart.txt] [--quiet-uart] [--trace] \
    [--no-exclusive-monitor] \
    [--dump ADDR:LEN:FILE ...]   # e.g. --dump 0x88006000:3072:y.bin (DDR bytes after the run)
```

Every run prints one line of exclusive-monitor counters (`[fx1_soc] atomics:`).
`--no-exclusive-monitor` falls back to upstream VP++'s bus-lock model, where no
device takes part in atomics; it exists as a negative control, not as a mode
to develop against.

Exit status:

| Code | Meaning |
|---|---|
| 0 | PASS (firmware wrote PASS to sim-control) |
| 1 | FAIL (firmware reported a code) |
| 2 | Timeout: no verdict within the simulated-time budget |
| 3 | Simulator or integration error |
| 4 | Usage error |

## Firmware contract (`fw/fx1_soc`)

- **ABI:** `-march=rv32imafc_zicsr_zifencei -mabi=ilp32f`. The xPack GCC 15
  toolchain has no rv32imafc/ilp32 multilib.
- **Boot:**
  - One ELF is linked at 0x8000_0000 and loaded into DDR through the bus
    backdoor; every hart resets at its entry.
  - `crt0.S` gives each hart a 64 KiB stack from `FX1_FW_STACK_BASE`.
  - Hart 0 clears `.bss` and calls `main()`; its return value becomes the verdict.
  - Other harts wait in WFI until hart 0 calls `fx1_release_secondaries()`,
    then enter `secondary_main(hartid)`.
- **Ordering (RVWMO):** `volatile` is not a synchronisation primitive. Use the
  HAL barriers:
  - `fx1_mb` = `fence iorw,iorw`;
  - `fx1_wmb_io` = `fence w,o` (memory writes before a device write);
  - `fx1_release` = `fence rw,w`;
  - `fx1_acquire` = `fence r,rw`.

  The release sequence is: shared data → `fence rw,w` → flag →
  `fence iorw,iorw` → MSIP write. The released hart issues `fence iorw,iorw`
  after it observes the flag. `fx1_secondary_release` is forced into `.data`.
- **Console:**
  - `fx1_log*` print one whole line under a console lock with this hart's
    interrupts masked (saved and restored), so lines never interleave and
    interrupt handlers may log.
  - A hart that faults while printing gets nested, unlocked access.
  - `fx1_putc` and `fx1_puts` are raw and unserialised.
- **Fatal path:** `fx1_fail()`, failing `fx1_check()` and the default trap
  handler mask interrupts and print best effort, waiting a bounded time for the
  console. They always write the FAIL verdict. The default trap code is
  0xE000 | mcause.
- **SYS_DMA:** include `dma/fx1_dma_regs.h`. See
  `components/FX1_Components/dma/docs/PROGRAMMING_GUIDE.md`.
- **ISP:** compile `components/FX1_Components/isp/driver/src/fx1_isp_drv.c` into
  the image with `isp/driver/include` and `isp/include` on the include path.
  - Provide an `fx1_isp_hal` with `read32`/`write32` at `FX1_ISP_CSR_BASE + offset`
    and a `delay_cycles` for the 2 ns ISP core clock.
  - Serialise driver calls against the ISR with `fx1_irq_save()`.
  - See `tests/isp_stream/` (`isp_common.c` is the HAL) and `isp/docs/ISP_PROGRAMMING_GUIDE.md`.
  - `fx1_isp_events.frames_done` is the number of DONE outputs still waiting in
    the driver queue when the ISR ran, not the number of new completions. Do not
    sum it across interrupts. Mark the sequences of the oldest `frames_done`
    queue entries instead (`dev.queue[k].seq`), as `isp_stream/main.c` does.
  - The statistics FRAME_ID is the hardware's per-accepted-SOF count (1, 2, ...
    after reset), published at EOF. It is not the driver's `seq` (0-based).
    Read it through the driver's `fx1_isp_read_*` accessors, which re-read the
    tag around the payload.
  - `IDMA_UNDERRUN` (a level while no next input is valid) and `ODMA_OVERFLOW`
    are lossless stalls, not errors.
- **Atomics (plan C7):**
  - LR/SC is a real reservation over the naturally aligned
    `FX1_RESERVATION_GRANULE` (64 B, VP placeholder pending HAS) containing the
    LR address. A write by any other master into that granule cancels it: the
    other hart's store, SC or AMO, a SYS_DMA write, an ISP ODMA write. The
    hart's own stores do not. An SC succeeds only with a surviving reservation
    for the same granule and consumes it either way; a reset drops it.
  - An LR does not stall the other hart. An SC or AMO excludes the other hart
    for its duration (VP++ bus lock) and every device write to its granule
    (the write waits, and the SC/AMO waits for an overlapping write already in
    flight).
  - Keep lock words and other LR/SC targets out of any granule a device writes
    (DMA buffers), or SCs fail spuriously — correct, but no forward progress
    while the device streams. Write retry loops; do not count on the granule
    size.
  - Not covered: debug (backdoor) writes, i.e. ELF loading and `--dump`.
- **Buffer ownership (plan C12):** CPU harts, SYS_DMA and the ISP masters see
  one DDR. The VP models no cache. Every buffer a device reads or writes
  changes owner twice, each time through a HAL call with the direction of the
  device's access (`FX1_DMA_TO_DEVICE`, `FX1_DMA_FROM_DEVICE`,
  `FX1_DMA_BIDIRECTIONAL`):
  - `fx1_dma_prepare(addr, bytes, dir)`: CPU → device, after the CPU's last
    access and before the device is started on or handed the buffer. This
    includes buffers the device only **writes** (DMA destination, ISP Y/UV:
    all four before `fx1_isp_start`, and each again once consumed, since the
    driver may re-queue it).
  - `fx1_dma_complete(addr, bytes, dir)`: device → CPU, after the completion
    and before the CPU's first access.
  In between the device owns the buffer: the CPU neither reads nor writes it.
  On the VP both calls are a full fence. On silicon the work depends on the
  FX1 cache/coherency, which the HAS has not fixed; for a non-coherent
  write-back cache the usual mapping is in `fx1_fw.h` (prepare: clean for
  TO_DEVICE, clean + invalidate for FROM_DEVICE/BIDIRECTIONAL; complete:
  invalidate for FROM_DEVICE/BIDIRECTIONAL), and device-written buffers must
  then cover whole cache lines. Porting means confirming that mapping against
  the HAS, not only replacing the two function bodies. The VP cannot detect a
  missing or misdirected call. Memory that the CPU and a device access
  concurrently (the C7 atomic test words) is outside this contract and needs
  coherent or uncached memory on silicon.
  - Buffers live in the regions of `fx1_memory_map.h` (`FX1_FW_SHARED_BASE`,
    `FX1_FW_RAW_BUF_BASE`, `FX1_FW_YUV_BUF_BASE`, `FX1_FW_TEST_AREA_BASE`),
    never in the image or the stacks; the linker script asserts that the
    regions are disjoint. Device-side alignment and strides follow each IP's
    guide (ISP: 16-byte AXI beats; DMA: any byte alignment).
  - Drive each IP from one hart only. Moving an IP between harts needs a lock
    and an ownership hand-over that this VP does not provide.
- **Interrupts:**
  - `fx1_irq_save()` and `fx1_irq_restore()` mask this hart's interrupts around
    any lock that a handler on the same hart can also take.
  - PLIC helpers: `fx1_plic_*`. VP test lines: `fx1_sim_irq_set` and `fx1_sim_irq_get`.
  - Handlers clear the source at the device before `fx1_plic_complete()`,
    because sources are level-triggered.
- **Bus errors:** these become access faults: fetch → mcause 1, load → 5,
  store/AMO → 7, with `mtval` set to the address. Covered responses:
  - unmapped addresses;
  - slave errors, e.g. a write to BootROM;
  - unsupported access forms (a target's BURST/COMMAND/BYTE_ENABLE error).
  A target that never answers, or a malformed CPU payload, stops the simulator
  with an integration error (exit 3) naming the address, because it is a model
  defect, not a guest fault.
- **Traps:** `fx1_set_trap_handler()` installs a handler per hart. Handlers must
  not use floating point, because FP registers are not saved.
- **MMIO:** use 32-bit, naturally aligned accesses for every register.
  - The UART sits behind `fx1::WordAccessGuard`. `lb/lh/sb/sh` to the UART
    raise a load (5) or store (7) access fault and never reach the model.
- **Verdict:** `fx1_pass()`, `fx1_fail(code)`, or return from `main()`.

## Tests

| ctest | Checks |
|---|---|
| `fx1_sparse_ram` | Sparse paging, page crossing, byte enables, range errors, ROM write error, backdoor preload |
| `fx1_clint` | 20 MHz `mtime`, MTIP at exactly `mtimecmp × tick`, deassert on rewrite, MSIP, hart independence, `mtime` re-base, error responses |
| `fx1_sim_control` | PASS/FAIL codes, rejected writes, stop time |
| `fx1_access_guard` | Only aligned full 32-bit accesses pass; short, misaligned, partial byte-enable and null accesses are rejected without reaching the model, and a canary beyond a 1-byte payload stays intact (also with the real `uart2_tlm`) |
| `fx1_smoke_hello_2hart` | Two-hart boot, release over MSIP, separate stacks, shared data |
| `fx1_smoke_clint_irq` | On each hart, 3 timer IRQs with exactly one IRQ per deadline and none early; 3 IPIs with exactly one software IRQ each; `rdtime` agrees with MMIO `mtime` |
| `fx1_smoke_isa_misa` | `misa` = RV32IMAFC+NSU; `fld` and `vsetvli` raise illegal instruction; `fadd.s` executes |
| `fx1_smoke_irq_in_log` | A timer IRQ expiring in the middle of `fx1_log` is deferred to the end of the line, and its handler logs |
| `fx1_smoke_uart_access` | `lb`/`lh` raise mcause 5 and `sb`/`sh` raise mcause 7 at the UART address; word output still works |
| `fx1_smoke_trap_in_log` | Negative test: a load fault inside `fx1_log` ends with exit 1 and `FAIL code=0xe005`, not a timeout |
| `fx1_plic` | Priority order and lowest-ID tie-break, enable mask, claim clears pending, level re-pend after complete, latched request survives a deassert, one claim per request across two contexts, complete from a non-enabled context ignored, side-effect-free debug claim, error responses. The threshold gates notification only: a source below or equal to it is still claimable, priority 0 is neither notified nor claimable, and two contexts with different thresholds still compete for one request |
| `fx1_cpu_port` | Streaming width completed (a set width is kept). A valid access that the target refuses with BURST/COMMAND becomes GENERIC (a guest fault); ADDRESS/GENERIC pass through. Integration errors: a silent target (payload pre-set to OK, as VP++ does), INCOMPLETE, BYTE_ENABLE on a payload without byte enables, and malformed CPU payloads (command, null data, length, byte enables, streaming width), which never reach the target |
| `fx1_smoke_silent_target` | Negative test on a real VP++ hart: a target that never answers stops the run with an integration error, and the guest never reaches its PASS write |
| `fx1_smoke_plic_irq` | On hart 0: delivery with one claim and no re-entry; priority 6 → 7 → 5; threshold gating MEIP, and a polled claim that succeeds at threshold 7; mask; level re-pend. On hart 1: woken from WFI. A shared source is claimed exactly once. The UART TX interrupt is claimed and cleared at the device |
| `fx1_dma_*` | DMA model unit tests: registers, memory, peripheral, chain, pending, arbitration, errors, reset, timing, and `idle` (an M2P channel waits 10 ms with no scheduler wakeups, then completes). `fx1_dma_bus` runs the DMA through the FX1 bus with the named-port API. `fx1_dma_regs_header` checks the C header against `registers.h` |
| `fx1_smoke_dma_m2m` | SYS_DMA on the platform: 256-byte and 1023-byte unaligned M2M copies verified byte for byte; completion through PLIC source 2 and cleared at the DMA; an unmapped destination gives WRITE_DECERR; sub-word CSR access gives load/store access faults |
| `fx1_smoke_isp_stream_basic`, `fx1_smoke_isp_stream_gtm_auto` | The ISP driven by the reference C99 driver (`isp/driver`) through an MMIO HAL. Eight 64x48 RGGB frames go through the 4-buffer rotation (each buffer used twice), with up to two frames in flight; the CPU fills every input. The ISP interrupt goes through PLIC source 3 and is acknowledged by the driver at the device. Every NV12 frame must match the CRC-32 of the pinned Python reference (`tests/isp_stream/isp_fixture.h`). At the end: IDMA/ODMA frame counters are 8 and no fatal DMA error is set. The GTM-auto run checks the temporal effect on frames 1–7. IDMA underrun is a lossless level and is only counted |
| `fx1_smoke_isp_stream_backlog` | Same stream and profile as basic, but with a lagging consumer: the oldest output is taken only once the next one is DONE too. The ISR therefore sees two outputs DONE at once and reports one output in several snapshots. Every completion must be marked exactly once, by sequence; the run also checks that a backlog of 2 occurred and that the sum of the snapshots exceeds 8, so a counter that sums snapshots would fail here |
| `fx1_smoke_isp_frame_id` | Basic + AEC (4x3 zones of 16x16) + AWB (global) + AF, with distinct context IDs. One frame at a time; after each completion the driver reads the AEC/AWB/AF publications. FRAME_ID must be 1..8, and the context IDs and the AEC global sums and counts per channel must match the reference. The NV12 still matches basic's CRC |
| `fx1_exclusive_monitor` | Monitor and `WriteGuard` unit test with hand-driven threads: SC without/with a consumed or mismatched reservation; cancellation by another master's write to the same word, the same granule or a straddling range, not by the next granule or the hart's own write; reset drops reservation and bracket; an open bracket refuses another master's overlapping write; `atomic_begin` waits for an overlapping write in flight; a guarded DMA write waits for an open bracket while a read does not; a guarded write cancels a reservation |
| `riscv_vp_plusplus_exclusive_monitor_hooks` | A real VP++ hart with a scripted monitor: exact hook sequence of `sw`, `lr.w`, a successful and a failed `sc.w`, and `amoadd.w`; LR takes no bus lock; the guest sees the monitor's SC answer and a failed SC writes nothing; `reset_cpu()` calls `reset_hart` |
| `fx1_smoke_lrsc_masters` | LR/SC against every writer. SC without a reservation, consumed and mismatched reservations. Between LR and SC (the hart waits in WFI): SYS_DMA writes the reserved word or another word of its granule → SC fails and the DMA data stays; the DMA writes the next granule → SC succeeds; hart 1 stores to the word → SC fails; to the next granule → SC succeeds; the ISP ODMA writes its output over the word → SC fails and the NV12 matches the reference. The DMA write is chained behind a padding command so it lands after the LR (checked) |
| `fx1_smoke_amo_dma_q*ns` | 128 trials of `amoor.w` on a word while SYS_DMA writes bit 31 into it, the DMA write's phase swept by the padding size: the DMA bit must survive. The test also requires `device writes held` > 0 in the platform output, i.e. DMA writes did arrive inside an AMO and waited |
| `fx1_smoke_amo_dma_no_monitor` | Negative control: the same image under `--no-exclusive-monitor` must FAIL with code 0x20 (a DMA write lost inside an AMO) |
| `fx1_regression_spinlock_2hart_q*ns` | Both harts take an LR/SC spin lock 50 000 times each around a plain `lw`/`sw` increment, plus an `amoadd.w` counter: both end at 100 000. A varying few-instruction delay between LR and SC makes both harts see the lock free at once (without it the deterministic schedule never does, and a broken monitor still counted 100 000). Requires `cancelled by hart` > 0. About 5 s host per run |
| `fx1_smoke_shared_mem_concurrent` | Plan C12: hart 0 streams the eight ISP frames (as `isp_stream_basic`) while hart 1 runs SYS_DMA copies in its own DDR region, CPU-written source to CPU-checked destination with the ownership calls; both paths are checked. Overlap is measured: hart 1's copy counter is sampled when the first frame is queued and at each of the 8 completions; hart 1 must progress in every interval, copy at least 4 times in the window and still be running when the stream ends |
| `fx1_faulting_lr` | Plan C7: a real VP++ hart and the real monitor on a RAM that faults the first LR's load: after the trap the SC fails and writes nothing; a following LR/SC pair succeeds |
| `fx1_smoke_isp_recovery` | Plan P5, real bus errors through an unmapped buffer address. A: ODMA AXI error on output 1 → firmware repairs the address, `fx1_isp_recover_ex` frees it again, the lost frame is reported by `fx1_isp_next_lost` and resubmitted; all 8 frames complete exactly once with the reference CRC, exactly one loss, a second recover with the same snapshot is a no-op. B: IDMA AXI error on input 2 → repair, recover re-arms it, the frame is retried, no loss. C: soft reset while the first input is still being read (both frame counters unchanged at the abort) → no completion, no ODMA progress and no DONE report afterwards; restart from buffer 0, 8 frames correct. D: `fx1_isp_stop` at the same point discards exactly one frame, nothing completes afterwards; start again, 8 frames correct. Every phase ends with nothing in flight, no fatal DMA_ERR and the ISP line low |
| `fx1_smoke_dma_recovery` | SYS_DMA: WRITE_DECERR (unmapped destination) and READ_DECERR (unmapped source) with no completion and the channel inactive, then a re-armed copy on the same channel succeeds; a chain whose second command fails completes the first (data checked) and reports the error; an error on channel 0 leaves a concurrent channel-1 copy intact; a 16-command chain paused with `CH_ENABLE = 0` after 2 commands stops progressing and resumes to exactly 16 completed commands with correct data; the IRQ line is low at the end |
| `fx1_smoke_example_quickstart` | The handoff example (`fw/fx1_soc/examples/quickstart`): two-hart hello, a CLINT timer interrupt, a SYS_DMA copy with its PLIC interrupt and the ownership calls |
| `fx1_regression_isp_fixture_check` | `gen_fixture.py --check`: the committed fixture still matches the reference. Registered only when `python3` on PATH has numpy; the smoke tests never need Python |
| `fx1_smoke_bus_fault` | Fetch, load and store faults with exact `mcause`/`mepc`/`mtval`; a store to BootROM faults and leaves the ROM unchanged |
| `*_q10us` | Every smoke image is run again with a 10 µs quantum and must give the same verdict (plan C11) |
| `fx1_regression_perf_loop_q*ns` | Two-hart integer and memory loop. Checksums must equal independently computed references (0x1762a79a, 0x124d31b2). Prints host MIPS: about 3.5 MIPS with both harts in a Release build (about half that unoptimised), either quantum, depending on host load |

## Time model (plan C11)

The FX1 bus consumes annotated delay with `wait()` at every hop and returns a
delay of 0. VP++ hands its quantum-keeper local time to `b_transport` and takes
back what the bus returns. So **every CPU access, instruction fetches included,
synchronises the hart with SystemC time**:
- device side effects, interrupts and `mtime` are observed in program order;
- an interrupt raised by a device write is visible before the next instruction;
- the TLM quantum has no functional effect.

The `*_q10us` runs confirm this: the same verdicts, the same checksums, and
simulated time within a few ns. The cost is speed: the perf loop runs at about
3.5 MIPS on two harts in a Release build (about 1.5 MIPS unoptimised). Temporal decoupling (accumulating delay instead of
`wait`, with syncs at MMIO) and DMI for DDR are possible later optimisations.

## Limitations

- **Streaming width:** VP++ never sets the payload streaming width.
  `fx1::CpuPortAdapter` completes it for every CPU access. The shared wrapper is
  left unchanged for TPU_V3.
- **Performance:** about 3.5 MIPS with both harts in a Release build (about
  1.5 unoptimised), because every access syncs (see Time model). A CPU memory
  access costs about 100 ns of simulated time; the ISP processes a 64x48
  frame in a few microseconds. `mcycle` uses the VP++ 10 ns cycle on both harts and does
  not reflect the silicon frequencies (600/300 MHz in the HAS).
- **Atomics:** the exclusive monitor covers both harts, SYS_DMA and ISP ODMA
  (ISP IDMA only reads). A future writing master needs its own `WriteGuard`.
  The granule (64 B) is a placeholder. An SC or AMO stalls the other hart
  entirely for its duration (VP++ bus lock), which is stronger than silicon.
  No cache, so no cache-line reservation semantics beyond the granule.
- **Shared memory:** no cache is modelled; a driver that omits
  `fx1_dma_prepare`/`fx1_dma_complete` still passes here. Passing on the
  VP says nothing about cache maintenance on silicon.
- **ISP:** this stage covers only the basic flow: one geometry (64x48), two profiles,
  statistics checked only for the FRAME_ID/context tags and the AEC global sums
  (no zone, histogram or AWB/AF value checks), and no error injection or
  recovery on the platform. The
  model's own suites cover those, and the platform fault/recovery cases are
  stage G5.
- **PLIC:** level-triggered gateways only. A request latched before the line
  drops stays pending until claimed (RISC-V PLIC rule); the driver sees a
  spurious interrupt.
- **Scope:**
  - Not modelled: caches, PMP enforcement, S-mode/Sv32, and B and Zicbom.
  - DDR is a functional RAM, not a DDR3 controller (no CSRs, init or training).
- **Reset and abort:** the map has no reset controller (CRG) yet, so the
  platform tests recover with the drivers' soft reset / stop / recover only.
  SYS_DMA has no channel abort: `CH_ENABLE = 0` pauses a command (accepted
  transactions drain, re-enabling continues); only the IP reset, at power-on
  here, cancels one. ISP AXI errors are produced with unmapped buffer
  addresses and made transient by the firmware repairing the address; error
  injection inside DDR (ECC-like, mid-burst) is not modelled.
