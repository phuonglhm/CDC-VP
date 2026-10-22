# M6b status — camera in the FreeRTOS CLI, boot check, SDK

**Closed by the reviewer 2026-10-22** (plan/M6B_REVIEW_CLOSURE_20261022.md),
after two review rounds (review F1/F2, re-review F2 follow-up below). M6 complete.

Date: 2026-10-22. Not committed. Second half of M6 (plan §9); M6a closed by
the reviewer (plan/M6A_REVIEW_CLOSURE_20261022.md). Scope: the CLI part of
the M6 exit gate — **sensor → DDR → ISP → NV12 driven from the FreeRTOS
CLI**, the camera in hart 1's boot IP check, and the SDK package with the
camera manager. Decision added: P18 ([spec_decisions.md](spec_decisions.md)).

Choices made by the user (2026-10-22):
- one `freertos_cli` image; the camera is compiled in with the camera build
  (`FX1_FW_HAS_CAMERA`); without `--camera` probe says absent and the boot
  check reports the CSI-2 RX absent (skipped), not failed;
- the camera commands run in the CLI task (manager polled with a one-tick
  delay); the ISP is shared with the `isp` commands, one owner at a time, the
  ISP interrupt going to the owner;
- commands: probe, mode [W H BITS LANES] (default 64 16 12 4), source
  sensor|tpg, start, capture N [crc], status, stop [US] (default 24 ms),
  recover (stop, sensor software reset, configure, start; on error report the
  step, keep a valid state, no automatic retry);
- camera buffers in a DDR region of their own, frames up to 3840x2160; CRC
  and golden for NV12 up to 1 MiB unless `crc`; RAW spot check on every
  frame;
- boot check: CSI-2 RX registers, I2C0, the sensor's model_id;
- tests: nominal, fault and absent CLI sessions in the smoke stage
  (required); TPG session in the regression stage — at 4K without a CRC
  (a 12 MiB CRC on the simulated CPU, ~0.3 MIPS from DDR, takes about ten
  minutes of host time, not the 30–60 s estimated when this was decided), with
  a new 640x480 TPG golden instead; a negative session (RAW file instead of
  the pattern) in the regression stage, added after the mutation check;
- SDK: manager, ISP HAL, camera → ISP examples (`make camera`, `run_vp.sh
  camera-isp`), CLI with the camera (`run_vp.sh camera-cli`), guide;
  packaging regression runs `camera_isp_raw12`.

## What changed

| Part | Files | Notes |
|---|---|---|
| CLI | `fw/fx1_soc/freertos/cmd_camera.c` (new), `cli.c`, `rtos_app.h`, `main.c` | `camera` command, PLIC 7 service, MIPI_CSR and I2C0 in the `md`/`mw` regions |
| ISP sharing | `fw/fx1_soc/freertos/cmd_isp.c` | `isp_claim` / `isp_release`; ISP ISR forwarded to the camera while it owns the ISP; `isp selftest` / `run` refused meanwhile |
| Boot check | `fw/fx1_soc/freertos/hwcheck.[ch]` | CSI-2 RX (identity, 2 reset values, 3 read/write registers), I2C0 (idle, CTRL), sensor model_id; `skipped` field and report line |
| Golden | `fw/fx1_soc/tests/camera_isp/gen_golden.py`, `camera_isp_golden.h` | new case `tpg640` (640x480 RAW12 colour bars); the other CRCs unchanged |
| Build / CI | `fw/fx1_soc/CMakeLists.txt`, `platforms/fx1_soc/ci/required_tests.txt` | camera sources and `FX1_FW_HAS_CAMERA` in the RTOS images; 3 smoke + 2 regression sessions |
| Tests | `fw/fx1_soc/freertos/tests/cli_camera.txt`, `cli_camera_fault.txt`, `cli_camera_absent.txt`, `cli_camera_tpg.txt`, `cli_camera_mismatch.txt` + `.raw` | scripted sessions |
| Package | `package_sdk.sh`, `sdk/Makefile`, `test/run_vp.sh`, `check_package.sh`, `run_packaging_regression.sh`, `write_manifest.py`, `docs/CAMERA_FW_GUIDE.md` | manager + ISP HAL + `camera_isp/` shipped; the SDK-built CLI has the camera; `make camera` builds `camera_isp_raw12/raw10`; `run_vp.sh camera-isp`, `camera-cli`; manifest notes the zero-copy open item |
| Docs | `fw/fx1_soc/freertos/README.md`, `platforms/fx1_soc/README.md`, `spec_decisions.md` (P18) | |

## Exit-gate items (M6b part of plan §9)

| Item | Status | Evidence |
|---|---|---|
| CLI: sensor → DDR → ISP → NV12 for both RAW depths | PASS | `fx1_smoke_freertos_cli_camera`: probe (IP_VERSION 0x00010100, model_id 0xF1C5), 64x16 RAW12 4 lanes: 4 frames zero-copy = golden 1..4, RAW spot check ok, no drop; RAW10 2 lanes: 2 frames staged = golden 6, 7; status counters consistent |
| CLI recovery, stop while the sensor does not answer | PASS | `_camera_fault`: `mw` disables the I2C host → `camera stop` ESENSOR in ~2.2 ms, state "stopped, sensor not confirmed", `camera start` refused; I2C back → stop OK; `camera recover` → frames 1, 2 again = golden |
| Without `--camera` | PASS | `_camera_absent`: boot check `CSI-2 RX: absent, skipped`, `camera: absent`, refused commands, bad mode reported, `isp selftest` PASS, 0 failures |
| ISP shared with the `isp` commands | PASS | `_camera`: `isp selftest` refused while the camera runs, PASS after `camera stop` |
| Large frames from the CLI | PASS | `fx1_regression_freertos_cli_camera_tpg`: TPG 640x480 RAW12 2 frames = golden; 3840x2160 RAW12 1 frame, metadata and RAW colour bars, CRC skipped by size |
| Capture failure path | PASS | `fx1_regression_freertos_cli_camera_mismatch`: `--camera-raw` frames of 0x123: both frames `raw BAD` and `GOLDEN MISMATCH`, capture FAIL, poweroff FAIL |
| hwcheck | PASS | the two sessions with `--camera`: `ok CSI-2 RX, checks 9`, `ok I2C0 + sensor, checks 4` |
| SDK package | see Test run | `run_packaging_regression.sh` |

## Findings

1. **ISS speed from DDR is ~0.3 MIPS, not ~1 MIPS**: the 4K NV12 CRC in the
   CLI would take ~10 minutes of host time (85 M instructions in 313 s before
   the first TPG session timed out). The CLI computes the CRC up to 1 MiB by
   default; the TPG regression uses a 640x480 golden; the bare-metal tpg4k
   regression (M6a) keeps checking the 4K NV12 on the host.
2. **Console output during a capture would make the receiver drop frames**:
   the UART TX is polled at the character time; the capture collects all
   frames first, then prints.
3. **A CSI-2 interrupt after a stop must still be acknowledged** (the
   controller's interrupt enables survive the stop): the CLI's PLIC 7
   service calls the manager's ISR whenever the CSI-2 driver instance is
   valid.
4. A mutation that leaves the ISP interrupt unacknowledged (not forwarded to
   the camera) makes the session crawl (interrupt storm) instead of failing
   quickly: the CTest timeout is the bound (detected, ~30 min of host time).

## Mutation check

`$S/mut/mutate_m6b.py` (sources restored and checked by hash after each
case), 7/8 detected, 1 equivalent:

| Mutation | Detected by |
|---|---|
| ISP interrupt not forwarded to the camera | `_camera` (stalls; killed by the run's watchdog after 10 min) |
| ISP ownership not enforced | `_camera` |
| hwcheck: absent camera counted as a failure | `_camera_absent` |
| start accepted in state 3 | `_camera_fault` |
| recover without the sensor software reset | `_camera_fault` |
| golden comparison always passes | `_camera_mismatch` (first missed by `_camera` and `_camera_tpg`: only good frames) |
| RAW spot check ignored | `_camera_mismatch` (same) |
| TPG 640x480 profile not used | equivalent: `cam_isp_profile_tpg640` equals `cam_isp_profile_tpg4k` write for write (both from colour bars of the same proportions) |

## Gaps

- 1-lane sensor modes run in the component tests only (the CLI accepts them;
  the golden holds for any lane count).
- Interactive use over `--uart0-socket` is not part of CI (scripted sessions
  only).
- The zero-copy `IDMA_BUF_ADDR` write still waits for the ISP owner (P17,
  spec_decisions §4); it is listed in the guide and the build manifest.

## Test run

| Suite | Result |
|---|---|
| fx1_soc `run_ci.sh`, fresh directory, camera build, 1 fs, -j8 | PASS: 337 required tests registered; smoke stage 319/319, 570 s wall; components 6/6; regression 12/12, 122 s |
| `run_packaging_regression.sh` (JOBS=8) | PASS: 93 required files, provenance-pending note, camera reported by the binary, SHA256SUMS 131 files, smoke, ISP d65, firmware rebuilt from the shipped sources byte-identical (the CLI with the camera), DDR3 examples 4/4, camera examples 2/2, `run_vp.sh camera-isp` camera_isp_raw12 PASS, clone check, interactive flow |
| mipi-csi2 component regression | not re-run: M6b changed no component source |

`run_vp.sh camera-cli` is shipped but not run by the packaging gate (the
same image and script run in CI as `fx1_smoke_freertos_cli_camera`).

| Session (host time) | Result |
|---|---|
| `fx1_smoke_freertos_cli_camera` | PASS, 232 s |
| `_camera_fault` | PASS, 133 s |
| `_camera_absent` | PASS, 2 s |
| `fx1_regression_freertos_cli_camera_tpg` | PASS, 66 s |
| `fx1_regression_freertos_cli_camera_mismatch` | PASS, 58 s |
| existing FreeRTOS tests (`freertos_cli_q*`, `_bad_input`, `_hwcheck_fail`, `ddr3_boot_freertos_cli`) | PASS |

## Review 2026-10-22 (plan/M6B_REVIEW_20261022.md) — resolution

Policies chosen by the user (2026-10-22): F1 — the manager's stop takes the
configured state; F2 — a golden spot table with poisoned positions; tests:
a bare-metal start-failure case, a CLI session on a fault-injection image, one
TPG 4K session with a Y and a UV fault.

| Finding | Fix | Regression |
|---|---|---|
| F1 (P2) `camera start` failing after configure (stream-on write lost) left the manager in state 1 with the ISP owned by the camera: stop/recover refused, `isp` refused until reboot | `fx1_camera_stop` takes state 1 (sensor standby, CSI-2 DMA stop/abort, ISP stop, sides back, one deadline; ESENSOR → state 3); `fx1_camera_start` documents "on error state 1, call stop". CLI: a failed start is cleaned up at once with `fx1_camera_stop` (24 ms), the ISP given back only in state 0 or 3; `camera stop` and `recover` take state 1 | `fx1_smoke_camera_isp_start_fail` (bare metal: I2C host lost after configure → start EDEV, state 1; I2C back → stop OK, idle, sides back, sensor in standby and not streaming; restart exact); `fx1_regression_freertos_cli_camera_startfault` (test image `freertos_cli_camfault`, `-DCAMERA_TEST_STREAM_ON_FAULT`: start EDEV, cleanup stop ESENSOR, `isp selftest` PASS, I2C back → stop OK, start, 2 frames = golden) |
| F2 (P2) with the CRC skipped (NV12 > 1 MiB) no Y/UV byte was checked: the reviewer's ODMA UV-over-Y redirect still gave `capture: PASS` | `gen_golden.py` emits `cam_isp_spot_tpg4k_y/uv` (32 + 32 bytes: corners, every colour bar, first / middle / last lines; 11 distinct values each); the CLI checks them before release when the CRC is skipped, mismatch = capture FAIL; the positions are poisoned (complement of the expected byte) before configure and after every check, so a slot the ISP did not write cannot pass with an earlier frame; output `crc skipped (size): full golden not compared, Y spot ok UV spot ok (64 bytes)`; modes without a golden print `NV12 not checked (no golden)` | `fx1_regression_freertos_cli_camera_nv12_fault`: a good 4K frame (spot ok), ODMA Y of slot 0 redirected to scratch DDR → `Y spot BAD UV spot ok`, then UV → `Y spot ok UV spot BAD`, poweroff 2 failed; `_camera_tpg` now requires the spot-ok line |
| Note: `camera stop ... in N us` measured after printing | elapsed taken right after the call | — |

Mutation (`"R6b:"`): 6/6 detected — manager stop refusing state 1
(`start_fail`, `startfault`), CLI start failure not cleaned up
(`startfault`), Y check ignored, UV check ignored, no poison (the second and
third 4K frames then pass on the first frame's data), spot check skipped
(`nv12_fault`, `tpg`).

Test run after the fixes: fx1_soc `run_ci.sh`, fresh directory, PASS — 340
required tests; smoke stage 320/320, 587 s wall at -j8; components 6/6;
regression 14/14, 291 s (the start-fault session ~290 s host).
`run_packaging_regression.sh` (JOBS=8) PASS: 93 required files, firmware
rebuilt from the shipped sources byte-identical, camera examples 2/2,
`camera-isp` camera_isp_raw12 PASS, every other step as before.

## Re-review 2026-10-22 (plan/M6B_REREVIEW_20261022.md) — resolution

F1 closed by the reviewer. F2 follow-up (P2): the poisoning ran only after a
spot check; a frame checked by CRC (`camera capture N crc`) went back to the
ring with the golden bytes in place, so after a switch to spot-checked
captures a plane the ISP did not write could pass on that frame's data.

Policy chosen by the user (2026-10-22): the regression uses the F1 test image
with a fast-CRC fixture (a TPG 4K frame's CRC taken as the golden's; the real
CRC comparison runs at 64x16 and 640x480 in the other sessions).

| Fix | Regression |
|---|---|
| `cmd_camera.c`: `spot_poison` moved to the common path before `fx1_camera_release`, for every frame of a spot-checked mode, CRC or not; the stream-on fault of the test image applies to the sensor source only; header / P17: ETIMEOUT leaves the state unchanged (1 or 2) | `fx1_regression_freertos_cli_camera_mixed_crc` (`freertos_cli_camfault`, `-DCAMERA_TEST_FAST_CRC`): `capture 1 crc` (slot 0, = golden), `capture 3` (slots 1–3, spot ok), UV of slot 0 redirected → `capture 1` on slot 0: `UV spot BAD`; stop, start, the same with Y → `Y spot BAD`; poweroff 2 failed |

Mutation: poison only after a spot check (the old code) → detected by
`_mixed_crc`.

Test run after the fix: fx1_soc `run_ci.sh`, fresh directory, PASS — 341
required tests; smoke stage 320/320, 589 s wall at -j8; components 6/6;
regression 15/15, 290 s. `run_packaging_regression.sh` (JOBS=8) PASS: 93
required files, firmware rebuilt from the shipped sources byte-identical,
camera examples 2/2, `camera-isp` camera_isp_raw12 PASS.
