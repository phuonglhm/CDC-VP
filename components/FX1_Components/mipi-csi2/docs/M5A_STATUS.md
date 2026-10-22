# M5a status — I2C host, camera sensor model, fx1_soc integration

Date: 2026-10-21. Not committed. First half of M5 (plan §8; M4 closed RAW
only). M5b follows: bare-metal FW drivers (I2C, sensor, CSI-2), camera smokes
RAW10 / RAW12 / recovery through the real PLIC, CI manifest, docs/package.
Decisions added or changed: P02, P06, P12–P15
([spec_decisions.md](spec_decisions.md) §2.4).

Choices made by the user (2026-10-20/21):
- sensor control over I2C: `components/i2c_tlm` completed as a functional
  host, extended backward-compatibly (legacy behaviour stays the default for
  mini_soc, riscv_custom_soc, noc_soc, i2c_platform); sim-control kept for
  fault injection and debug (P06, P12);
- sensor registers: subset of MIPI CCS; pixels: deterministic pattern plus an
  optional RAW file;
- I2C0 at 0x1003_0000 on PERIBUS_0, PLIC source 9 (`FX1_PLIC_NUM_SOURCES`
  9 → 10); sim-control test lines 4 → 3 (sources 4–6), source 7 driven only
  by csi2_irq; `plic_irq` firmware test adapted;
- camera = compile-time gate (`-DFX1_BUILD_MIPI_CSI2=ON`, set by `run_ci.sh`)
  plus run-time `--camera`; without `--camera` the platform is unchanged
  except for I2C0; time resolution 1 fs on every fx1_soc run;
- PLL gating in the D-PHY wrapper, no PHY model source change (P13);
- VP++ patch 0005, an approved exception to "no CPU model change" (P15).

## What changed

| Part | Files | Notes |
|---|---|---|
| I2C host | `components/i2c_tlm/include/i2c.h`, `i2c_device.h` (new), `src/i2c.cpp`, `tests/test_i2c_functional.cpp` (new), README | `i2c_config{functional_host, scl_hz, verbose}`; `attach(addr7, i2c_device*)`; FMT FIFO executed with bit timing, repeated START, READB with RCONT, RX-full clock stretching, NACK halt (`INTR_NAK`) and NAKOK; one `irq` driver; at most 4 bytes copied per access |
| Sensor model | `sensor/include/fx1_csi2_sensor/ccs_sensor.h`, `sensor/src/ccs_sensor.cpp` (new) | `CcsSensor`: CCS subset at I2C 0x10, mode latched per frame, standby at the end of the frame in progress, RAW10/RAW12 on 1/2/4 lanes through `DphyTxBfm`, pattern or RAW file (P12) |
| D-PHY wrapper | `phy/include/fx1_csi2_phy/dphy_rx_wrapper.h`, `phy/src/dphy_rx_wrapper.cpp` | PLL core processes suspended while the PLL is off (P13) |
| CMake | `CMakeLists.txt` | `fx1_csi2_sensor`; standalone build compiles `i2c_tlm` itself when `cdc::components::i2c_tlm` is absent; tests `test_fx1_csi2_sensor`, `test_fx1_dphy_pll_gate` |
| Memory map | `fx1_map/include/fx1/fx1_memory_map.h` | `FX1_I2C0_BASE`, `FX1_IRQ_I2C0` 9, `FX1_PLIC_NUM_SOURCES` 10, `FX1_SIM_CTRL_IRQ_LINES` 3, map assertions |
| Bus | `bus/src/config.cpp`, `bus/tests/bus_config_tests.cpp`, README, WORKBOOK_MAPPING | initiator `MIPI_DMA`; `MIPI_CSR` with base `FX1_MIPI_CSR_BASE`, `I2C0` on PERIBUS_0, both disabled until a platform instantiates them |
| fx1_soc | `platforms/fx1_soc/src/fx1_soc_top.{h,cpp}`, `main.cpp`, `ddr3_subsystem.cpp`, `ddr_sysctrl.cpp`, `CMakeLists.txt`, `ci/run_ci.sh`, README, `docs/FW_GETTING_STARTED.md` | I2C0 always (word-access guard); `--camera` / `--camera-raw`; camera = WriteGuard (master 0x102) + D-PHY + controller + sensor; CSI reset from the power-on pulse; `sc_set_time_resolution(1 fs)` first in `sc_main`; namespace-scope `sc_time` constants turned into functions (P02); `--version` and the start banner report the camera |
| VP++ | `cpu_models/riscv_vp_plusplus/patches/0005-resolution-independent-instr-time.patch` (new), `fetch_riscv_vp_plusplus.sh`, `CMakeLists.txt` | instruction time independent of the time resolution (P15) |
| Firmware | `fw/fx1_soc/tests/plic_irq/main.c`, `tests/camera_absent/main.c` (new), `tests/camera_wiring/main.c` (new), `fw/fx1_soc/CMakeLists.txt` | see below |

## Exit-gate items (M5a)

| Item | Status | Evidence |
|---|---|---|
| I2C functional host: order and bit timing, ACK/NACK, repeated START, multi-byte read, stretching, NACK recovery | PASS | `i2c_tlm_functional` (3-byte write = 140.0 µs at 400 kHz, read with repeated START and last byte NACKed, NACK of an absent address and of a data byte, NAKOK, 100-byte read through a 64-entry RX FIFO) |
| Legacy I2C behaviour unchanged | PASS | `i2c_tlm` (legacy test) in the fx1 build and in a default build; mini_soc, riscv_custom_soc, noc_soc, i2c_platform build; noc_soc tests (see Test run) |
| Sensor over I2C → BFM → D-PHY → controller → DDR | PASS | `fx1_csi2_sensor_stream` (probe, RAW10 64x4 4 lanes, 3 frames verified against `CcsSensor::pattern` and released from the interrupt, stream off inside frame 4: that frame completes, none follows, clock lane back to LP-11, FRAME_COUNT = frames sent, no drop, CCS frame_count), `_raw12_2lane`, `_raw_file` (frames from a file, wrap), `_controls` (NACK at 0x22, RO / unknown registers, auto-increment across registers, unsupported format → no frame and error count, software_reset) |
| PLL gating does not change the PHY's behaviour | PASS | whole mipi-csi2 regression with gating: 143/143 (bench excluded) + `fx1_dphy_pll_gate_on/off`; idle cost 1.06·10^6 → 1.05·10^4 delta cycles per 100 µs |
| fx1_soc without `--camera`: MIPI_CSR DECERR, source 7 low, I2C0 present | PASS | `fx1_smoke_camera_absent` (+`_q10us`) |
| fx1_soc with `--camera`: CSR, I2C0 on source 9, MIPI_DMA to DDR, csi2_irq on source 7 | PASS | `fx1_smoke_camera_wiring` (+`_q10us`): driver probe, CCS model_id read over I2C with completion claimed at source 9, one TPG frame 32x8 RAW10 through MIPI_DMA, BUF_DONE_OK claimed at source 7, DDR image = colour bars, padding untouched |
| Build without the camera | PASS | `FX1_BUILD_MIPI_CSI2=OFF`: builds; `--version` says "not built"; `--camera` exit 4 with a message; `camera_wiring` not registered; camera_absent, plic_irq, hello pass |
| fx1_soc at 1 fs: full CI | PASS | `run_ci.sh` (camera build): 202 required tests registered; smoke 68 + unit 126, components 6, regression 6 — PASS |
| VP++ unchanged at 1 ps | PASS | VP++ test suite (label `cpu`, TPU_V3 build): 16/16 |

## Findings

1. **D-PHY PLL VCO runs while the PLL is off** (P13): `pllcoremodel.cpp`
   sets `half_period` = 1.8 when `on` = 0 and the VCO loop toggles whenever
   `half_period` > 0. With nothing driven, ~1.06·10^7 delta cycles per
   simulated ms (~860 s of host time per simulated second): the 0.67 s DDR3
   preinit alone took ~10 min with `--camera`. Not a functional defect (the
   RX path does not use the PLL, contract §3.3); handled in the wrapper.
   Residual idle cost ~28 s per simulated second: a `--camera` run spends
   ~19 s in the preinit.
2. **VP++ assumed 1 ps** (P15): instruction time `cycle_time.value() / 1000`
   read back as ns; at 1 fs each instruction cost 10 µs and 43/192 fx1 smoke
   and unit tests failed on firmware timeouts. Fixed by patch 0005. The P02
   audit had missed it (the source is under `third_party/`).
3. **Namespace-scope `sc_time` in the platform** (P02): six constants in the
   DDR3 glue and one in `fx1_soc_top.cpp` fixed the resolution at 1 ps before
   `sc_main` (E514). Now functions.
4. **Bench hazard found while writing the sensor test** (not a model bug): a
   loop on `irq.read()` without advancing time spun at one time stamp and
   flooded the log until ctest was OOM-killed (59 GB). The test now bounds
   every loop and exits after 50 failures; reading `irq` right after a W1C
   clear can see the old level for a few sys_clk, as firmware does.

## Mutation check

Sensor (`$S/mut/mutate_sensor.py`, each mutant built and run, sources
restored by hash): 11/11 detected — stream off at once, frame number off by
one, lane mode ignored, no read auto-increment, no write auto-increment,
software_reset keeps x_size, invalid format accepted, RAW file offset, CCS
frame_count + 1, RAW12 sent with the RAW10 DT, clock lane not stopped in
standby (detected after adding the PHY_STATUS clock-lane check).

PLL gating: gate never applied → `fx1_dphy_pll_gate_on` FAIL (1.07·10^6
delta cycles), restored → PASS. VP++ patch: its absence is the 43-test
failure of finding 2.

## Test run

| Suite | Result |
|---|---|
| mipi-csi2 (`ctest -E bench`, gating on) | 143/143, 102 s at -j6 |
| `fx1_dphy_pll_gate_on/off` | 2/2 |
| `i2c_tlm`, `i2c_tlm_functional` | 2/2 (fx1 build and default build) |
| fx1_soc `run_ci.sh`, camera build, 1 fs | PASS (202 required; smoke 68 + unit 126; components 6; regression 6) |
| fx1_soc without the camera (`FX1_BUILD_MIPI_CSI2=OFF`) | build OK; camera_absent, plic_irq, hello 7/7 |
| VP++ `cpu` suite at 1 ps | 16/16 |
| noc_soc | see the M5a report |

Host cost: `fx1_soc --camera` adds ~19 s per run (DDR3 preinit with the PHY
idle); `camera_wiring` 24 s. Builds that already have a cache must be
reconfigured with `-U "RISCV_VP_PLUSPLUS_PATCH*"` (P15).

## Open for M5b / later

- FW drivers (I2C, sensor, CSI-2 BSP), camera smokes RAW10 / RAW12 / recovery
  with frames from the sensor through the PLIC, `ci/required_tests.txt`,
  docs/package. The camera tests of M5a are registered (label `fx1_smoke`)
  but not yet in the manifest.
- The TPU_V3 decision log (owner of the VP++ patch series) does not record
  0005; recorded here (P15) and in the series' `why` field.
- Large frames through the detailed PHY stay slow (M3b benchmark: 640x480
  ≈ 150 s per 1.255 ms simulated); a PPI-level source for fx1_soc remains an
  option for M6 workloads.

## Review 2026-10-21 (plan/M5A_REVIEW_20261021.md) — resolution

Policies chosen by the user (2026-10-21): F1 complete the frame in
progress; F2 pause between transactions; F3 refuse a bad file at load and
refuse a frame the file cannot fill, never fall back to the pattern.

| Finding | Fix | Regression |
|---|---|---|
| F1 (P1) software_reset mid-frame mixed frame numbers and pixels but was delivered as a valid buffer | `ccs_sensor.cpp`: per frame the mode, the CSI-2 frame number (`fn`, used by every line and by FE) and a reset epoch are latched at FS; `reset_registers()` bumps the epoch, a frame that a reset hit is sent whole and not counted; registers at reset values at once; frame numbers restart at 1 | `fx1_csi2_sensor_reset_mid_frame`: reset lands inside frame 2 (after frame 1 verified); frame 2 exact against `pattern(2)`, no frame level / sync / number-sequence error, FS/FE numbers on the link 1,1,2,2 then 1,1 after the restart (new `CcsSensor::packet_observer`, decoded from the header bytes sent), no frame after the reset, CCS frame_count 0, FRAME_COUNT 2, restart delivers `pattern(1)` |
| F2 (P2) disable/re-enable left the FMT FIFO stuck | `i2c.cpp`: CTRL write notifies the host; the host pauses only between transactions (`!enable && !host_busy_ && !discarding_`) | `i2c_tlm_functional` cases 7 (disable before the first command is taken: FIFO 4, no bus event, no completion for 200 µs; enable → written, CMD_COMPLETE) and 8 (disable inside a 3-byte write: it completes with STOP, the queued next transaction waits 500 µs untouched, enable → runs); legacy `i2c_tlm` unchanged |
| F3 (P2) a bad RAW file silently became the pattern | load: empty or odd byte count → `SC_REPORT_ERROR` (fx1_soc exit 3); `latch_mode`: with a file, samples must be a whole multiple of W×H of the mode latched at FS, else no frame (standby, error_count, warning); `line_pixels` uses the file whenever one is configured | `fx1_csi2_sensor_raw_load_errors` (0, 1 and 129 bytes refused), `_raw_short` (10 samples), `_raw_partial` (1.5 frames): no frame, PACKET_COUNT 0, error_count, mode_select stays 1; `_raw_mode_change` (32x3 while streaming a 32x2 file: stops at the frame end; 32x2 again resumes with the next file frame); `_raw_file` (two frames, wrap) still passes; `fx1_smoke_camera_raw_invalid` (`--camera-raw` of 1 byte: exit 3 with the message) |
| housekeeping | trailing whitespace at `i2c.h:59,121` removed | `git diff --check` clean |

Reviewer repros (run from a copy, `$S/m5a_review`): `review_i2c_reenable`
PASS, `review_reset_mid_frame` PASS, `review_raw_short` now stops at load
with "RAW file … has 1 bytes" — the agreed policy refuses an odd-length file
before any stream, stricter than the repro's oracle; the same repro with a
2-byte (even, < one frame) file PASSES its oracle (frames 0, errors 1, CSI
frames 0).

Mutation (`$S/mut/mutate_m5a_review.py`): 9 of 10 mutants detected — line
pixels or FE number from the live counter, frame counted across a reset,
odd/empty file accepted, per-mode file check removed (raw_short, raw_partial,
raw_mode_change), enable not waking the host, disable pausing inside a
transaction. Not detected: restoring the pattern fallback in `line_pixels`,
an equivalent mutant while `latch_mode` refuses every file that is not a
whole number of frames (removing that check is detected).

Test run after the fixes: mipi-csi2 150/150 (`ctest -E bench`, 145 + 5 new
sensor cases); `i2c_tlm` + `i2c_tlm_functional` 2/2; fx1_soc `run_ci.sh` on a
fresh directory PASS (202 required; smoke 69 incl. `camera_raw_invalid` +
unit 126; components 6; regression 6).

Remaining limitations: a byte-wise register update that a frame boundary
splits (e.g. x_output_size high then low byte) is latched as written, as on a
sensor without grouped parameter hold (CCS 0x0104, not modelled); FW should
change the mode in standby. FDATA writes are ignored while the host is
disabled, so the STOP of a started transaction must be queued before
disabling.
