# M6a status — bare-metal camera → ISP on fx1_soc

**Closed by the reviewer 2026-10-22** (plan/M6A_REVIEW_CLOSURE_20261022.md),
after four review rounds (F1/F2, R1, R2, R3 below). M6b continues.

Date: 2026-10-21. Not committed. First half of M6 (plan §9). M5 closed by the
reviewer (plan/M5B_REVIEW_CLOSURE_20261021.md). Scope: **sensor → detailed
D-PHY → CSI-2 RX DMA → DDR → ISP → NV12** under firmware control, through a
camera manager separate from the hardware drivers, for RAW12 and RAW10, with
golden NV12, multi-frame stress, the fault cases and host-checked artifacts.
M6b (FreeRTOS CLI `camera ...`, hwcheck, SDK package / docs) follows.
Decision added: P17 ([spec_decisions.md](spec_decisions.md) §2.4), open item
for the ISP owner (§4).

Choices made by the user (2026-10-21):
- M6 split into M6a (bare metal) and M6b (CLI, hwcheck, SDK);
- camera manager separate from the drivers (`fw/fx1_soc/drivers/fx1_camera`);
- RAW12 zero-copy: the manager writes `IDMA_BUF_ADDR` of the next ISP input
  slot (outside the ISP driver; ISP driver and model unchanged; to be
  confirmed by the ISP owner); RAW10: the CSI-2 DMA writes RAW16, the
  firmware stages `sample << 2`;
- golden: build-time `fx1_isp_run_raw` CRC checked by the firmware, plus
  artifact dumps checked on the host (SHA-256);
- 64x16 RGGB, ISP profile "basic"; large frames up to 3840x2160 through the
  CSI-2 RX TPG (no PHY) as a separate regression test;
- fault cases: CSI-2 error not sent to the ISP, ISP back-pressure → CSI-2
  drop, ISP ODMA error recovery, manager timeout (plus `idma_hold`, added
  after the mutation check, see below).

## What changed

| Part | Files | Notes |
|---|---|---|
| Camera manager | `fw/fx1_soc/drivers/fx1_camera.[ch]` (new) | probe / configure / start / stop / poll / dequeue / release / set_output, ISR hooks for PLIC 7 and 3, capture and ISP-error hooks, statistics; lifecycle in P17 and the header comment |
| ISP HAL | `fw/fx1_soc/drivers/fx1_isp_hal_mmio.[ch]` (new) | MMIO HAL of the reference ISP driver at `FX1_ISP_CSR_BASE`, delays from mtime |
| Golden | `fw/fx1_soc/tests/camera_isp/gen_golden.py`, `camera_isp_golden.h` (new, generated, committed) | `fx1_isp_run_raw` on the exact ISP input (RAW12 pattern; RAW10 pattern << 2; TPG colour bars 3840x2160), profile basic from `make_profile.py`; CRC-32 per frame number 1..32; `--check` |
| Tests | `fw/fx1_soc/tests/camera_isp/` (new): `cam_isp.[ch]`, `stream.c`, `faults.c`, `artifacts.c`, `tpg4k.c`, `check_artifacts.py`, `check_tpg4k.py`; `fw/fx1_soc/CMakeLists.txt` | shared bring-up, trap handler, table CRC-32, frame number from the golden CRC |
| CI | `platforms/fx1_soc/ci/required_tests.txt` | 12 smokes (7 + 4 after the review + 1 after the re-review) + 4 regression tests |
| Docs | `platforms/fx1_soc/README.md`, `spec_decisions.md` | tests table, P17, open item |

## Exit-gate items (M6a part of plan §9)

| Item | Status | Evidence |
|---|---|---|
| Bare-metal sensor → DDR → ISP → NV12, RAW12 and RAW10 | PASS | `fx1_smoke_camera_isp_raw12` (4 lanes, zero-copy), `_raw10` (2 lanes, staged): 8 consecutive frames each, every NV12 = the ISP model's for its frame number, geometry from the ISP driver; no drop, no CSI-2 or ISP error; 8 CSI-2 and 8 ISP interrupts through the PLIC; both sides back after stop |
| Only valid CSI-2 buffers go to the ISP | PASS | `_csi_err`: side 1 unmapped, its frames (2, 4, 6) end with an error and are released, never queued (`isp_queued` = `csi_ok`); frames 1, 3, 5, 7 exact, all from side 0 |
| Multi-frame buffer stress / back-pressure | PASS | `_backpressure`: four outputs held by the application, the ISP feed stops, 3 CSI-2 drops counted; after the release the held frames 5 and 6 come out intact and the next is 10 = 7 + 3 drops |
| Recovery | PASS | `_odma_err`: ODMA error on output slot 2 (DMA_ERR 0x10), repaired from the error hook, `fx1_isp_recover_ex`; one frame lost (frame 3), frames 1, 2, 4–7 exact and in order |
| Zero-copy side held until the ISP has read it | PASS | `_idma_hold`: the ISP's IDMA paused by the test (`DMA_CTRL.IDMA_EN` = 0, white-box poke, no driver change): frames 2 and 3 handed over and unread, both sides stay with the ISP, 3 drops, `isp_queued` = 3; IDMA resumed: frames 2 and 3 intact, then 7 = 4 + 3 drops |
| Timeout | PASS | `_timeout`: sensor stopped, `fx1_camera_dequeue` returns ETIMEOUT after 6006 µs for a 6000 µs budget; stop still works |
| Artifacts: original RAW, staging RAW, NV12, metadata, links | PASS | `fx1_regression_camera_isp_artifacts_raw12/_raw10`, 6 frames each: RAW = sensor pattern; RAW10 staging = RAW << 2 (0x3FF → 0xFFC in every frame); RAW12 zero-copy from the recorded side, 0xFFF reaching the ISP; NV12 CRC = golden and SHA-256 = `fx1_isp_run_raw` on the dumped input; capture, FRAME_ID, ISP sequence consecutive, sides alternate, output slot = seq mod 4 |
| Large frames (address / size) | PASS | `fx1_regression_camera_isp_tpg4k`: two 3840x2160 RAW12 frames from the TPG, 16 MiB sides, zero-copy, RAW spot-checked by the firmware (every bar and Bayer phase, first and last lines), both NV12 = ISP model (CRC 0xDD6DA3BB) |
| Golden up to date | PASS | `fx1_regression_camera_isp_golden_check` |
| ISP-owner confirmation of the zero-copy CSR write | OPEN | spec_decisions §4 |

## Findings

1. **A slow consumer made the CSI-2 RX drop frames in RAW12.** Zero-copy
   sides were first released only from `fx1_camera_poll`, and the firmware's
   bit-wise CRC of a 64x16 NV12 took longer than a frame period, so both sides
   were held and frame 4 was dropped. The ISP ISR now returns read sides
   (`release_read_sides`), side-state changes happen with interrupts masked,
   and the test CRC is table-driven.
2. **Frames left in flight at stop are not consumed**, so the drop counter
   may move during stop: tests read it before stopping.
3. **The 4K TPG path is cheap**: ~77 ms simulated per frame, 33 s host for
   the whole test, 540 MB RSS. The detailed-PHY 64x16 tests cost 85–200 s
   host each (blanking at 3 ms frame period dominates, P16).
4. Decisions taken by me within the approved scope, for review: an optional
   `isp_error_hook` in the manager configuration (the application repairs the
   faulty slot before the manager recovers); ISP frames reported lost are
   counted, not resubmitted.

## Gaps (not covered by M6a)

- 1-lane configurations run only in the component tests (fx1_soc: RAW12 4
  lanes, RAW10 2 lanes; M5b: RAW10 4 lanes, RAW12 2 lanes).
- Statistics with tag validation: profile basic has no statistics; the
  frame tag / timestamp linkage is checked instead.
- RAW10 through the TPG at 4K is not run (RAW12 only).

## Mutation check

`$S/mut/mutate_m6a.py` (sources restored and checked by hash after each
case), 11/11 detected:

| Mutation | Detected by |
|---|---|
| RAW10 staged without `<< 2` | `camera_isp_raw10` |
| CSI-2 error side handed to the ISP | `_csi_err` |
| newest waiting side handed over first | `_backpressure` |
| held output slot ignored at hand-over | `_backpressure` |
| ISP DMA error not recovered | `_odma_err` |
| dequeue budget doubled | `_timeout` |
| RAW12 `IDMA_BUF_ADDR` not rewritten | `_csi_err` |
| zero-copy side released at hand-over | `_idma_hold` (check 0x120: 0 drops, 5 queued) |
| capture hook given the other side | `artifacts_raw12` |
| ready list returned newest first | `_backpressure` |
| golden header CRC stale | `golden_check` |

The zero-copy early release was first **missed** by raw12, backpressure and
artifacts_raw12: at 64x16 the IDMA reads its input within microseconds, long
before the CSI-2 DMA reuses the side. `fx1_smoke_camera_isp_idma_hold` was
added for it.

Host checker negative checks (corrupted copies of real dumps): one staging
sample → "staging = RAW << 2"; one NV12 byte → CRC ≠ golden; a record's
FRAME_ID → "FRAME_ID consecutive". All rejected.

## Test run

| Suite | Result |
|---|---|
| fx1_soc `run_ci.sh`, fresh directory, camera build, 1 fs, -j8 | PASS: 326 required tests registered; smoke stage 310/310 (83 smoke + 126 unit + 101 `fx1_csi2_unit`), 323 s wall; components 6/6; regression 10/10, 125 s wall |
| camera → ISP tests in that run (host time) | raw12 169 s, raw10 138 s, csi_err 152 s, backpressure 219 s, odma_err 155 s, timeout 87 s, idma_hold 164 s; artifacts raw12 125 s, raw10 101 s; tpg4k 34 s; golden_check 1 s |
| mipi-csi2 component regression | not re-run: M6a changed no component source (only `fw/fx1_soc`, fx1_soc README / CI manifest and these docs); last run 152/152 at M5b closure |
| mutation | 11/11 detected (one after adding `idma_hold`) |

Smoke-stage cost: the seven camera → ISP smokes add ~1100 s of CPU time
(~2–3.5 min each, detailed PHY); at -j8 the smoke stage wall time is 323 s.

## Review 2026-10-21 (plan/M6A_REVIEW_20261021.md) — resolution

Policies chosen by the user (2026-10-21): F1 — while an IDMA read error is
pending, hold every zero-copy side; F2 — stop cleans up the CSI-2 RX and the
ISP anyway, returns the new `FX1_CAMERA_ESENSOR` and stays in state 3 (stop
again retries the standby only, configure refused); the reviewer's note on
stop with application-held outputs settled now: stop cancels what is left in
the pipeline.

| Finding | Fix | Regression |
|---|---|---|
| F1 (P2) a zero-copy side went back to the CSI-2 DMA on an IDMA read error (VALID clears on the error too), before `fx1_isp_recover_ex` re-armed the same input: the retry read a later frame | `fx1_camera.c` `release_read_sides`: VALID read first, then `DMA_ERR`; no side released while `AXI_RD` is pending in the hardware or in the ISR's snapshot; after recovery VALID is set again, so the side returns only after a successful read | `fx1_smoke_camera_isp_idma_err`: retried output = capture 2 with sensor frame 2's pixels, side held through 7 ms of ISR-only time with the sensor streaming, then frame 3, then 4 + drops; reviewer repros mode 0 (ownership) and 3 (retry pixels) PASS from a copy |
| F2 (P2) stop returned OK although the sensor standby failed; the budget restarted after I2C | `fx1_camera_stop`: one deadline for all steps; standby written and read back (`fx1_ccs_stream_state`, new); on failure CSI-2 RX and ISP still stopped, `FX1_CAMERA_ESENSOR`, state 3, `stats.sensor_errors`; stop in state 3 retries the standby only; configure refused in state 3 | `fx1_smoke_camera_isp_sensor_stop_timeout` (I2C host disabled: ESENSOR after 2084 µs), `_nack` (wrong address: after 94 µs); both: sensor still streaming, stop again OK, mode_select 0, frame count stops, configure + start again with exact frames; reviewer repro mode 2 PASS |
| Note: stop with held outputs → ETIMEOUT | stop cancels waiting captures, frames in the ISP and completed-not-dequeued frames (`stats.stop_cancelled`); outputs the application holds stay readable and owned until released (ownership by capture number) | `fx1_smoke_camera_isp_stop_held` (stop in ~0.3 ms, 2 cancelled, held NV12 exact after stop, release OK, second release EINVAL, restart exact); reviewer probe mode 1 PASS |
| Header / P17 said the ISRs only collect events | corrected: what each ISR does is described | — |
| Found while fixing: `fx1_isp_stop` returns the number of frames discarded, the old stop reported EDEV whenever one was in flight | `dropped < 0` is the only error | `_stop_held`, every stop |

Not covered by a test (at the first review round; the deadline part is
covered since the re-review, below): the shared deadline with a budget
tighter than the I2C failure; the mode_select read-back on its own (a write that is ACKed but not applied cannot be made
in the model); a stale release of a frame from an earlier session.

Mutation (`$S/mut/mutate_m6a.py "R1:"`): 6/6 detected — read error not
checked before release (`_idma_err`), standby result ignored (`_sensor_stop_timeout`,
`_nack`), no standby retry in state 3, configure allowed in state 3
(`_nack`), stop dropping held-output ownership, stop waiting for the ISP to
drain as before (`_stop_held`).

Reviewer repros (`repro_manager.c` modes 0–3, from a copy in `$S/m6a_review`,
built against this source): all PASS — mode 0 side held after the read-error
ISR (state 2, CSI buffer SW-owned), mode 1 stop with held outputs OK in
316 µs and configure accepted, mode 2 stop returns ESENSOR (-24), mode 3
retried capture 2 = sensor frame 2.

Test run after the fixes: fx1_soc `run_ci.sh`, fresh directory, PASS — 330
required tests registered; smoke stage 314/314 (87 smoke + 126 unit + 101
`fx1_csi2_unit`), 406 s wall at -j8; components 6/6; regression 10/10, 121 s.
New smokes (host time): idma_err 133 s, sensor_stop_timeout 175 s,
sensor_stop_nack 165 s, stop_held 193 s; earlier ones unchanged in verdict.

## Re-review 2026-10-21 (plan/M6A_REREVIEW_20261021.md) — resolution

F1 closed by the reviewer; F2's error propagation, state 3, cancel, stale
handle and positive ISP discard accepted. R1 (P2): the stop deadline did not
reach the sensor transfers (2000 µs each from the sensor context) and the
state-3 retry computed none: stop(100 µs) took 2068 µs from running and
2017 µs from state 3.

Policies chosen by the user (2026-10-22): bound = `timeout_us +
FX1_CAMERA_STOP_SLACK_US`; the remaining budget reaches each transfer
through a copy of the sensor context (no BSP API change); a self-calibrating
read-back-cut case.

| Fix | Regression |
|---|---|
| `fx1_camera_stop`: deadline computed on entry, before the state-3 branch; `sensor_standby(cam, deadline)` copies the sensor context and sets its budget to what is left before the write and again before the read-back, starts neither once the deadline has passed; the CSI-2 DMA abort gets at least `FX1_CAMERA_STOP_ABORT_US` (50 µs) after the deadline; `FX1_CAMERA_STOP_SLACK_US` = 2 × `FX1_I2C_ABORT_US` + 100 = 300 µs documented in `fx1_camera.h`; stale prototype comment fixed | `fx1_smoke_camera_isp_sensor_stop_budget`: I2C disabled, stop(100) from running → ESENSOR after 177 µs, from state 3 → after 122 µs; I2C back, stop(1000) → OK after 247 µs; read-back cut: one standby write timed at 105 µs, stop(135) → ESENSOR after 230 µs (bound 435) with the sensor really in standby, stop again → OK; restarts exact |

Reviewer repros (`m6a_rereview_20261021/repro_manager.c` modes 0–5, from a
copy in `$S/m6a_rereview`): all PASS; mode 4 (deadline) 160 µs from running,
119 µs from state 3, for a 100 µs budget.

Mutation (`"R2:"`): 3/3 detected — standby write with the sensor's own
budget, state-3 retry without the caller's deadline, read-back with the
sensor's own budget (the last one first missed, before the read-back-cut
case was added).

Follow-up (user decisions 2026-10-22), replacing parts of the fix above:
each transfer gets `transfer_timeout = min(remaining_us,
cam->sensor.timeout_us)` (a 24 ms stop budget no longer makes one
transaction with an unresponsive sensor wait 24 ms); the slack is **200 µs
for the whole call** (`FX1_CAMERA_STOP_SLACK_US` = 200), guaranteed by the
BSP I2C driver: `begin()`'s idle wait is now min(`FX1_I2C_ABORT_US`, the
call's budget) and counts against that budget, so a call returns within its
budget + `FX1_I2C_ABORT_US`; the CSI-2 DMA abort allowance is 30 µs. The
first version above had 300 µs (2 × `FX1_I2C_ABORT_US` + 100, the idle wait
outside the budget) although 200 µs had been asked for.

Measured after the follow-up (`sensor_stop_budget`, `sensor_stop_timeout`):
stop(100) from running 169 µs, from state 3 115 µs, stop(1000) OK 251 µs,
read-back cut (write timed 107 µs, budget 137) ESENSOR after 212 µs — at most
75 µs over the budget; stop(24 ms) with the I2C host disabled → ESENSOR after
2084 µs (the sensor's 2000 µs cap; `sensor_stop_timeout` checks it, 0x123).
`camera_i2c_timeout`, `_i2c_irq`, `camera_raw10/12`, `_wiring`, `_recover`
PASS after the I2C driver change.

Mutation after the follow-up (whole set re-run on the current source): R1
6/6, R2 3/3, R3 transfer not capped at the sensor timeout detected
(`sensor_stop_timeout` 0x123, stop 24086 µs); R3 idle wait of `begin()`
outside the budget **not detected**: the host is busy at the start of a call
only after a call that returned `FX1_I2C_ERECOVER`, which the model at
400 kHz cannot produce (M5b), so the bound it closes is not observable here.

Test run after the follow-up: fx1_soc `run_ci.sh`, fresh directory, PASS —
331 required tests; smoke stage 315/315, 410 s wall at -j8 (497 s before the
cap); components 6/6; regression 10/10, 121 s. `sensor_stop_timeout` 177 s
host again. Reviewer repros modes 0–5 PASS; mode 4: stop(100) 154 µs from
running, 113 µs from state 3.

## Follow-up review 2026-10-22 (plan/M6A_FOLLOWUP_REVIEW_20261022.md) — resolution

R1 (deadline, bound `timeout + 200 µs`) accepted. R2 (P2): in the BSP I2C
driver, `fx1_i2c_write()` and `fx1_i2c_write_read()` queued START right after
`begin()` without checking the budget again: when the idle wait and the
interrupt acknowledge used it up (`wait_idle()` may also see idle on the poll
past the deadline), a new transaction started after the deadline (reviewer
harness: deadline 64 µs, START queued at 66 µs).

Policies chosen by the user (2026-10-22): `FX1_I2C_ETIMEOUT` with nothing
sent; a host regression test in the repository.

| Fix | Regression |
|---|---|
| `fw/fx1_soc/drivers/fx1_i2c.c`: `budget_left(start, timeout_us)` checked right before the first FDATA write of both APIs, from the call's start (no budget reset, the cleanup allowance is not budget for a new transfer); `FX1_I2C_ETIMEOUT` with nothing on the bus; contract in `fx1_i2c.h` | `fx1_i2c_bsp_budget` (new, label `fx1_unit`, required): `platforms/fx1_soc/tests/test_i2c_bsp_budget.cpp` builds the BSP driver for the host (`tests/hostshim/fx1_fw.h`) against the real `i2c_tlm` model in functional mode at 400 kHz, 1 µs per register access, mtime from `sc_time_stamp`; before each call an earlier transaction is left aborting (address byte on the bus, FMT reset `phase` µs later); sweep phase 0–30 µs × budget 5–200 µs, write and write_read (112 calls each); invariants: (a) no START queued after the deadline, (b) return within budget + `FX1_I2C_ABORT_US` + 10 µs bench allowance, (c) OK ⇒ the device took the transaction, nothing queued ⇒ no START at the device, (d) nothing queued ⇒ return within budget + 10 µs; the sweep must reach the gate and complete OK (write: 23 OK, 32 gated, 57 started and timed out; write_read: 16 / 32 / 64). Design after the reviewer's harness |

Mutation (`$S/mut/mutate_i2c_gate.py`): 4/4 detected by `fx1_i2c_bsp_budget`
— gate removed in write (a), gate removed in write_read (a), gate with the
budget doubled (a), and the R3 idle wait outside the budget (d: budget 5 µs,
returned after 28 µs), which no fx1_soc test could observe before.

Reviewer harness (`repro_i2c_begin.cpp`, from a copy in `$S/m6a_followup`,
against the fixed driver): write and write_read "begin deadline invariant:
PASS".

Found while writing the test: `sc_assert` is compiled out with `NDEBUG`
(Release); a call placed inside it (`fx1_i2c_open`) never ran and every
transfer timed out. The bench checks results explicitly.

Test run after the fix: fx1_soc `run_ci.sh`, fresh directory, PASS — 332
required tests; smoke stage 316/316 (88 smoke + 127 unit + 101
`fx1_csi2_unit`), 415 s wall at -j8; components 6/6; regression 10/10, 127 s.
Reviewer FW repros (`m6a_followup_20261022/repro_manager.c` modes 0–6, from a
copy in `$S/m6a_followup_fw`): 7/7 PASS; mode 4 stop(100) 154 µs from
running, 112 µs from state 3; mode 6 stop(0) with the CSI-2 DMA busy 76 µs,
from state 3 6 µs.

## R2 review 2026-10-22 (plan/M6A_R2_REVIEW_20261022.md) — resolution

R2 accepted. R3 (P2): `test_i2c_bsp_budget` returned 0 when `sc_start(1 s)`
ended before the sweep did (reviewer: a 2 s stall at the start of the sweep,
no case run, exit 0, CTest PASS).

| Fix | Check |
|---|---|
| `platforms/fx1_soc/tests/test_i2c_bsp_budget.cpp`: `completed` set only at the end of the sweep (after the coverage checks); `sc_main` returns nonzero when `!completed \|\| failures` and prints "FAIL sweep did not complete" (explicit check, not `sc_assert`) | normal run PASS; with a 2 s stall inserted at the start of the sweep (temporary, restored by hash): CTest FAIL "sweep did not complete (stopped at 1000000.000 us)" |
| Same defect class, found by grep over the SystemC tests: `components/i2c_tlm/tests/test_i2c_functional.cpp` (written in M5a/M5b) had `sc_start(1 s); return g_fail;` — same `completed` flag | normal run PASS; 2 s stall in its bench thread: CTest FAIL "did not complete". The mipi-csi2 tests already require `SC_STOPPED` after `sc_start` |

Runs: `fx1_unit` 127/127, `i2c_tlm` + `i2c_tlm_functional` PASS. Production
source unchanged in this round; full CI not repeated (last full run above,
332 required PASS).
