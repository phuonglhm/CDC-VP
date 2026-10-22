# M5b status — firmware camera path on fx1_soc, CI gate, SDK package

Date: 2026-10-21. Not committed. Second half of M5 (M5a closed by the
reviewer, plan/M5A_REVIEW_CLOSURE_20261021.md), one review round. Scope: the
acceptance M5a left open — **FW → I2C sensor → detailed PHY → CSI → DMA →
DDR → PLIC → release / recovery** on fx1_soc — plus the CI manifest and the
SDK package. Decisions added or changed: P12 (stop request), P16 (clock lane
between frames) ([spec_decisions.md](spec_decisions.md) §2.4).

Choices made by the user (2026-10-21):
- firmware drivers as a BSP in `fw/fx1_soc/drivers` (reference C99 CSI-2
  driver + MMIO HAL, new I2C0 and sensor drivers); camera interrupt through a
  trap handler on PLIC source 7;
- a stop (mode_select 0, software_reset) is a latched request: the frame in
  progress ends, the sensor stops physically even if streaming is requested
  again before FE, then restarts with a fresh latch (closure carry-over);
- smokes 64x16, three frames, RAW10 4 lanes and RAW12 2 lanes;
- recovery: firmware-driven (drop, sensor reset, DMA stop/start), SLVERR via
  `--ddr3-fault write-result` (CPU only reading during the capture, fixed
  time), DECERR via an unmapped buffer side;
- CI: camera smokes and the CSI-2 component tests without PHY traffic
  required; PHY-level tests stay out of the gate;
- SDK package with the camera; THIRD_PARTY.md records `mipirxphy` as
  converted from RTL/netlists/analog models, supplier, owner and original
  licence unconfirmed, no licence asserted; `check_package.sh` reports
  "provenance pending" without failing;
- FreeRTOS CLI / hwcheck camera support deferred to M6.

## What changed

| Part | Files | Notes |
|---|---|---|
| Sensor model | `sensor/include/fx1_csi2_sensor/ccs_sensor.h`, `sensor/src/ccs_sensor.cpp` | latched stop request (`stop_req_`), at least 1 µs of LP-11 before a restart (`standby_min()`, a function: no static `sc_time`, P02); `CcsSensorConfig::continuous_clock` (P16) |
| fx1_soc | `platforms/fx1_soc/src/fx1_soc_top.cpp` | the sensor with `continuous_clock = false` |
| BSP | `fw/fx1_soc/drivers/fx1_i2c.[ch]`, `fx1_ccs_sensor.[ch]`, `fx1_csi2_hal_mmio.[ch]` (new) | I2C0: one transaction per call, polled with an mtime budget or WFI on source 9 (claim/complete by the driver), NACK → `FX1_I2C_ENACK`, timeout flushes the FIFOs. Sensor: probe, set_mode (standby only, size read back), stream, soft_reset, frame_count, `fx1_ccs_pattern()`. HAL for `fx1_csi2_drv` |
| Camera smokes | `fw/fx1_soc/tests/camera/` (new): `camera.[ch]`, `raw.c`, `recover.c`, `decerr.c`, `slverr.c`, `i2c_irq.c`; `fw/fx1_soc/CMakeLists.txt` | shared bring-up, trap handler (claim, `fx1_csi2_irq_handle`, complete), word-wise frame check against the pattern, frame number recovered from pixel (0,0) |
| Component tests | `tests/test_csi2_sensor.cpp`, `CMakeLists.txt` | `restart_before_fe`, `stream_noncontinuous`; custom target `fx1_csi2_unit_tests` |
| CI | `platforms/fx1_soc/ci/run_ci.sh`, `ci/required_tests.txt` | `FX1_MIPI_CSI2_BUILD_TESTS=ON`, build `fx1_all fx1_csi2_unit_tests`, smoke selection `fx1_unit\|fx1_smoke\|fx1_csi2_unit`; manifest + 11 camera smokes + 101 CSI-2 unit tests |
| Package | `package/package_sdk.sh`, `check_package.sh`, `write_manifest.py`, `run_packaging_regression.sh`, `sdk/Makefile`, `test/run_vp.sh`, `docs/CAMERA_FW_GUIDE.md` (new), `docs/README.md`, `docs/MEMORY_MAP.md`; `THIRD_PARTY.md` | camera build required; CSI-2 driver, BSP and examples shipped; `make camera`; `run_vp.sh camera`; camera guide; manifest (map with I2C0 / MIPI_CSR, camera block, provenance pending); packaging gate builds the examples from the shipped sources and runs raw10 + i2c_irq |
| Docs | `platforms/fx1_soc/README.md`, `docs/FW_GETTING_STARTED.md`, `spec_decisions.md` | tests table, drivers, P12, P16 |

## Exit-gate items

| Item | Status | Evidence |
|---|---|---|
| Bare-metal capture pad → PHY → CSI DMA → DDR with FW controlling the sensor over I2C, interrupts through the real PLIC | PASS | `fx1_smoke_camera_raw10` (RAW10 4 lanes, packed), `_raw12` (RAW12 2 lanes, RAW16): 3 frames each, every pixel equal to `fx1_ccs_pattern(f)` with f = 1, 2, 3, released from the source-7 interrupt; stream off at a frame boundary; FRAME_COUNT = sensor frame_count = frames handled; no drop, CRC or ECC error; no resolution mismatch |
| Release / recovery | PASS | `fx1_smoke_camera_recover`: A no free side → frame 3 dropped (FRAME_DROPPED, count 1), release → frame 4 in the released side; B software_reset inside frame 6 (timed) → frame 6 delivered whole and exact, numbers restart at 1; C DMA stop while streaming → no capture, a frame finished by the stop checked, restart → exact frames, FRAME_COUNT + 2 |
| AXI errors | PASS | `_slverr` (SLVERR at line 8 of frame 2, fault address inside that side, last valid line 8, frame 1 exact, next frame exact); `_decerr` (side at 0x6000_0000: DECERR at its base, no valid line, recovery by reprogramming) |
| I2C driver interrupt mode | PASS | `_i2c_irq` (8 transactions through WFI on source 9, NACK → ENACK, source low afterwards) |
| Stop request (closure carry-over) | PASS | `fx1_csi2_sensor_restart_before_fe`: stop+start inside frame 2 and reset+reconfigure+start inside frame 4: each frame whole, one physical stop (clock lane LP-11) each, FS/FE numbers 1,1 2,2 3,3 4,4 1,1, FRAME_COUNT 5, no drop |
| Non-continuous clock does not change reception | PASS | `fx1_csi2_sensor_stream_noncontinuous` (clock lane LP-11 in blanking, frames exact); all camera smokes run with it |
| CI manifest | see Test run | `run_ci.sh` on a fresh directory |
| Package | see Test run | `run_packaging_regression.sh` |

## Findings

1. **Continuous HS clock dominates the host cost** of a camera run (~50 s
   per simulated ms with the clock lane in HS). With the sensor's clock lane
   in LP-11 between frames (P16) a 64x16 frame costs ~10 s and the vertical
   blanking almost nothing; first raw10 attempt took 471 s, then 82 s.
2. **Firmware frame check is slower than a short frame period**: a
   byte-wise check over the bus took > 2 ms per 64x16 frame and frames were
   dropped. Word compares against a pre-built expected line: ~1.5 ms
   including the refill; frame period set to 3 ms (`CAM_FRAME_LENGTH` 300).
3. **SLVERR injection is global** (`write-result` hits the next write of any
   master) and with ECC on it also latches a DDR fatal ECC hold on the
   granule. `camera_slverr` runs with `--ddr3-ecc off`, the CPU only reads
   during frame 2's capture, and the fault time is calibrated to the image
   (6200 µs after the CPU release ≈ line 7 of frame 2; documented in
   `slverr.c`).
4. The CSI-2 RX exposes no frame number to software: the firmware recovers
   it from the image (the pattern's first pixel is unique per frame number).

## Mutation check

`$S/mut/mutate_m5b.py`, 5/5 detected: I2C driver reporting a NACK as success
(`camera_i2c_irq`), sensor driver not writing the lane mode (`camera_raw12`),
x/y size bytes swapped (`camera_raw10`), sensor stop request ignored
(`fx1_csi2_sensor_restart_before_fe`), clock lane never stopped between
frames (`_stream_noncontinuous`).

## Test run

| Suite | Result |
|---|---|
| mipi-csi2 (`ctest -j6 -E bench`) | 152/152, 263 s (150 + `restart_before_fe`, `stream_noncontinuous`) |
| fx1_soc `run_ci.sh`, fresh directory, camera build, 1 fs | PASS: 314 required tests registered; smoke stage 302 (75 smoke + 126 unit + 101 `fx1_csi2_unit`), 0 failures, 279 s wall at -j8; components 6/6; regression 6/6 |
| camera smokes (host time, -j6) | raw10 93 s, raw12 83 s, recover 245 s, decerr 108 s, slverr 75 s, i2c_irq 23 s, wiring 24 s, raw_invalid 5 s, absent < 1 s |
| fx1_soc without the camera (`FX1_BUILD_MIPI_CSI2=OFF`) | build OK; no camera test registered; camera_absent, plic_irq, hello 7/7 |
| `run_packaging_regression.sh` (JOBS=8) | PASS: 82 required files, provenance-pending note, camera reported by the binary, SHA256SUMS 120 files, smoke, ISP d65, firmware byte-identical rebuild, DDR3 examples 4/4, camera examples built from the shipped sources and run (raw10, i2c_irq) 2/2, clone check, interactive flow |
| mutation | 5/5 detected |

## Open for M6 / later

- FreeRTOS CLI camera commands and hwcheck probe; CSI → ISP hand-off.
- PHY model provenance (THIRD_PARTY.md): pending information from the user.
- Camera frames larger than 64x16 stay expensive through the detailed PHY.

## Review 2026-10-21 (plan/M5B_REVIEW_20261021.md) — resolution

Policies chosen by the user (2026-10-21): F2 — FIFO_CTRL FMT reset aborts an
open transaction in the I2C model (functional mode only); F1 — the driver
waits by polling within its budget, no WFI (interrupt mode takes the
completion from PLIC source 9).

| Finding | Fix | Regression |
|---|---|---|
| F1 (P2) interrupt mode returned OK after 124 µs for a 20 µs budget (WFI without a deadline wake, completion checked before the deadline) | `fw/fx1_soc/drivers/fx1_i2c.c`: no WFI; one polling loop bounded by the budget in both modes; interrupt mode: end = PLIC source 9 pending + CMD_COMPLETE + HOSTIDLE, claimed and completed by the driver (`irq_claims` counts them); a completion seen on the poll that crosses the deadline is OK, otherwise the transaction is aborted | `fx1_smoke_camera_i2c_timeout` (both modes: 20 µs write → ETIMEOUT after 58–60 µs, 300 µs read → ETIMEOUT after ~332 µs, i.e. budget + the byte on the bus and STOP); `_i2c_irq` now also requires every completion through source 9 (≥ 10 claims) |
| F2 (P2) a timeout's cleanup removed the STOP and left the host busy (EBUSY forever) | `components/i2c_tlm/src/i2c.cpp`: FMT reset with a transaction open sets an abort: the byte on the bus completes (a READB stops after its current byte, also when clock-stretched), then STOP, host idle, no NAK / CMD_COMPLETE. Driver: at the budget FMT+RX reset, wait up to `FX1_I2C_ABORT_US` (100 µs: a byte and STOP at 100 kHz) for idle → `FX1_I2C_ETIMEOUT`, else the new `FX1_I2C_ERECOVER`; `begin()` waits up to the same bound for idle before `EBUSY` | `i2c_tlm_functional` cases 9 (FMT reset inside a write: STOP within one byte + STOP, data bytes not written, no interrupt, next transaction OK) and 10 (FMT + RX reset inside a clock-stretched 100-byte read: STOP, at most 65 bytes read from the device); `fx1_smoke_camera_i2c_timeout`: next transfer at once and a register write / read-back after each abort; legacy `i2c_tlm` unchanged |

Reviewer repros (from a copy, `$S/m5b_review`, against the new binary):
polling mode — ETIMEOUT after 36 µs, the next transfer OK, STATUS idle;
interrupt mode — ETIMEOUT after 60 µs for the 20 µs budget. Both exit 0.

Mutation (`$S/mut/mutate_m5b_review.py`): 4/4 detected — model FMT reset not
aborting (unit test and FW), driver not aborting at the budget, completion
awaited past the budget (100x budget), interrupt completion not claimed.

Test run after the fixes: mipi-csi2 152/152; `i2c_tlm` + `i2c_tlm_functional`
PASS; fx1_soc `run_ci.sh` fresh PASS (315 required; smoke stage 303, 0
failures; components 6; regression 6); packaging regression PASS (83 required
files, camera examples built from the shipped sources, raw10 + i2c_irq run).

Note: `FX1_I2C_ERECOVER` (abort not finished within 100 µs) is not reachable
at the model's 400 kHz; it exists for slower buses and is documented in
`fx1_i2c.h`.

## Re-review 2026-10-21 (plan/M5B_REREVIEW_20261021.md) — resolution

F1 closed by the reviewer. F2 race: `end_transaction()` took `discard_rest` /
`complete` before its STOP wait, so an FMT reset during the STOP bit (after a
NACK) restored `discarding_` and the next transaction was dropped; after a
normal write it reported the aborted transaction's CMD_COMPLETE.

Fix (`components/i2c_tlm/src/i2c.cpp`): the abort request is read again after
the STOP bit and wins — `discarding_ = discard_rest && !aborted`, no
CMD_COMPLETE when aborted, STOP sent once, the request cleared last. Same
defect class, found by review of every decision taken before a wait in
`host_thread`: a READB byte completing after the abort was pushed into the RX
FIFO just reset; it is now dropped (the device still sees the byte).

Regression: `i2c_tlm_functional` cases 11 (abort at 26 µs, inside the STOP
after an address NACK: the next write runs, only its CMD_COMPLETE), 12 (abort
inside the STOP of a normal write: bytes written, no CMD_COMPLETE, idle), 13
(abort inside a read byte: the byte not kept, RX empty, STOP);
`fx1_smoke_camera_i2c_timeout` sweeps budgets 8..35 µs to an absent address in
both modes, each followed by a sensor write + read-back that must succeed.

Reviewer repros (from a copy, `$S/m5b_rereview`): `review_abort_during_nack_stop`
PASS (value 0x77, irq 0x200), `review_abort_during_normal_stop` PASS, FW NACK
sweep PASS ("all following transactions succeeded"), the two earlier repros
PASS. Mutation: 3/3 detected (abort during STOP ignored for discard — unit and
FW; completion of an aborted STOP reported; aborted read byte kept).

Test run after the fix: mipi-csi2 152/152; `i2c_tlm` + `i2c_tlm_functional`
PASS; fx1_soc `run_ci.sh` fresh PASS (315 required; smoke stage 303, 0
failures; components 6; regression 6); packaging regression PASS (camera
examples raw10 + i2c_irq from the shipped sources).
