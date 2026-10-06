# FX1 ISP VP — M4 standalone qualification report

Status as of 2026-10-01. This is revision 2: it answers review findings
M4-R1..R3 (§8). Model at CDC-VP `dev`, uncommitted working tree on top of
`e611ed22`. Host: Intel Core Ultra 9 285K, AlmaLinux 9, GCC 11.5,
SystemC 2.3.4, build type RelWithDebInfo (`-O2`).

**Evidence level.** Every "bit-exact" result below is an agreement between two
independent implementations of the same reading of the HAS/CSR: the C++ model
and the project's numpy reference (`reference/fx1_isp_ref`). No owner golden,
RTL or tuning exists (DEC-05). Two implementations that share a misreading of
the specification pass together. The formula reviews (`plan/alg/ALG_*.md`) and
the hand-computed checks in the unit tests mitigate that risk; they do not
remove it. Camera PNGs are not compared (DEC-10, DEC-13).

## 1. Scope decisions (2026-10-01)

| ID | Decision |
|---|---|
| DEC-34 | Real-RAW regression with four test profiles (`tools/make_profile.py --preset`). The profiles are formulas, not calibrations. `full` enables every pipeline block (BLC, LSC, BPC, WB, DG, CCM, Gamma, GTM, 2DNR, EE, CNF, Resizer FHD) and AEC/AWB/AF; Demosaic, CSC and the formatters always run. The other presets: `basic`; `full_vga_709` (Resizer VGA through the decimator, BT.709, Gamma 2.2, LSC profile 1 at 0.75, DG attenuation 0.9375, maximum AWB grid); `gtm_manual_qhd` (GTM manual LUT, Resizer qHD, EE radial and non-unity tables, illegal AEC grid, AWB global only). |
| DEC-35 | The regression is `tools/run_regression.py`. It writes the plan §10 artefacts to a directory outside the tree. CTest `fx1_isp.regression.raw` runs it when `-DFX1_ISP_RAW_INVENTORY=<ISP_RAW_INVENTORY.json>` is given. The dataset is never copied into the repository. |
| DEC-36 | G-CONT has two parts: (a) the real sequence: all 28 RAW frames back to back in one VP session, for each preset (§3.2); (b) 100+ synthetic frames from a fixed seed, every completed frame compared with the reference, with soft resets, AXI errors, underrun and overflow injected (`tools/gen_sequence.py`, `fx1_isp.integration.cont_tlm`). |
| DEC-37 | 4K: a deterministic input pattern implemented in both C++ and Python. Only the NV12 SHA-256 and the statistics readout are committed (`tools/gen_4k_vectors.py`, `fx1_isp.integration.e2e_4k.*`). |

## 2. How to reproduce

```bash
export CC=/usr/bin/gcc CXX=/usr/bin/g++ PATH=/usr/bin:/bin:$PATH
cmake -S . -B build -G Ninja -DCMAKE_CXX_COMPILER=$CXX \
      -DFX1_ISP_CSR_XLSX=/path/FTEL_IP_ISP_CSR_v1.0.xlsx \
      -DFX1_ISP_RAW_INVENTORY=/path/plan/ISP_RAW_INVENTORY.json -DFX1_ISP_RAW_OUT=/path/out
cmake --build build && ctest --test-dir build --output-on-failure
# or the regression alone (about 6 minutes with 8 jobs):
python3 tools/run_regression.py --inventory /path/plan/ISP_RAW_INVENTORY.json \
        --run-raw build/fx1_isp_run_raw --out /path/out --jobs 8 [--mode image,sequence]
```

The regression needs Python 3 with numpy and PIL. Inventory paths are taken
relative to the inventory's grandparent directory, unless `--root` is given.

## 3. G-E2E and the real sequence — 28 RAW frames × 4 profiles

Each run takes this path: RAW files → `raw_fixture` (`le16_raw10_msb`,
`>> 4`, checked against the inventory SHA-256) → test RAM → CSR profile →
IDMA → pipeline → ODMA → test RAM → NV12. The model is driven like a driver
would drive it, through the 4-buffer rotation with distinct input and output
buffers. Each output is read out after its DONE, and the buffer is recycled.
**The NV12 of every frame is compared with the reference** (`--per-frame`).
The statistics are compared through every readout mux after the last frame.

**Result: 116 / 116 runs, 308 / 308 frames bit-exact.** `DMA_ERR` = 0x2 at
the end of every run: `IDMA_UNDERRUN` after the last buffer, which is
expected (M3-A9).

### 3.1 One RAW frame per VP session (112 runs, 196 frames)

Presets with temporal state submit the frame twice. The first frame runs with
the reset state: the GTM curve is not yet valid, so GTM passes through, and
the 2DNR variance is 0. The second frame uses the state the first one left.
In 77 of the 84 two-frame runs the two outputs differ. The 7 identical pairs
are all `gtm_manual_qhd` (a static GTM table) on frames whose measured noise
variance is 0, so the second frame runs with the same variance as the first.
That is the expected result, checked against the reference.

| Preset | Output | Frames/run | Statistics reads | Model host s/run (mean) | Reference host s/run (mean) | Simulated ms/run |
|---|---|---|---|---|---|---|
| basic | 2686×1518 | 1 | 109 | 0.39 | 2.1 | 8.18 |
| full | 1920×1080 | 2 | 898 | 3.7 | 17.6 | 16.37 |
| full_vga_709 | 640×480 | 2 | 951 | 3.5 | 17.3 | 16.37 |
| gtm_manual_qhd | 960×540 | 2 | 190 | 2.8 | 9.9 | 16.37 |

### 3.2 The 28 real frames back to back in one VP session (4 runs, 112 frames)

For each preset, the 28 frames go through one VP session in inventory order,
up to 4 in flight (queue 4). The profile is applied once and computed from
the first frame, so the GTM curve, the 2DNR variance, the AEC/AWB/AF
configuration and the FRAME_IDs carry from one real image to the next. The
reference processes the same 28 frames in the same order on one register
state. The comparison checks each frame against its position in the
sequence, which verifies the order. In the events log every frame completes
in buffer (index mod 4), and in the presets with statistics enabled the
FRAME_IDs read 1..28 in order. `basic` has AEC/AWB/AF off, so its FRAME_IDs
stay 0 (ALG-STAT-06).

| Preset | Frames | Result | Simulated (28 frames, back to back) | Model host s | Reference host s |
|---|---|---|---|---|---|
| basic | 28 | 28 / 28 bit-exact, 28 distinct outputs | 228.8 ms (8.17 ms/frame) | 8.4 | 58 |
| full | 28 | 28 / 28 | 228.8 ms | 48.5 | 261 |
| full_vga_709 | 28 | 28 / 28 | 228.8 ms | 47.4 | 260 |
| gtm_manual_qhd | 28 | 28 / 28 | 228.8 ms | 37.1 | 149 |

### 3.3 Artefacts and visual check

Each run writes its artefacts to `<out>/<raw>/<preset>/`, or to
`<out>/sequence/<preset>/` for a sequence:
- `run_manifest.json`: inputs, containers, profile and LUT checksums, frame
  order, geometry, strides, tool versions;
- `output.nv12` and `output_preview.png` of the last frame;
- `statistics.json`;
- `events.log`: interrupt edges and the driver's view of each frame,
  including its SHA-256;
- `comparison.json`: one record per frame, with the first mismatch if any.

`summary.md` and `summary.json` sit in `<out>`.

A visual check of the previews shows the expected scenes. The synthetic LSC
mesh (gain up to 1.6) and GTM auto make `full*` brighter, and give low-light
frames a slight magenta cast. That comes from an uncalibrated test profile,
not from the model, and it is not a pass/fail criterion.

NV12 SHA-256 of the last frame of each run (first 16 hex digits), the
regression baseline:

| RAW | basic (2686×1518) | full (1920×1080) | full_vga_709 (640×480) | gtm_manual_qhd (960×540) |
|---|---|---|---|---|
| khoid_ground_1 | `272ff227ae010a6c` | `a9cc194fe67e8be1` | `922f9b4d643c554a` | `e9d18e84b7559fa0` |
| khoid_ground | `fe02aaeff883beb4` | `20efd004610ff5b0` | `ff2cd731cf2e942e` | `a12932a842597f19` |
| khoid_indoor_led | `5baf97df8dea4ab3` | `de3fa726dc695150` | `c707e1fd8f1b0bde` | `04997bc867403d05` |
| khoid_outdoor1 | `a785773b7fabbe0c` | `b277e5cebb912502` | `9bd90c571c1a0aed` | `d3198e88a278ea1d` |
| khoid_outdoor2 | `959832490cbe5796` | `c2faf7ce8ed8000f` | `7156e5c5d216c161` | `6def6b8f050fefd4` |
| khoid_outdoor3 | `e352621cfebab37a` | `5ae70875cc4df576` | `416dbdf4b28eaf3c` | `9d5f4e4fee0423b2` |
| khoid_outdoor4 | `ae2ac05821b62eb1` | `ba4ecce5509d5dc0` | `bead857ad165f279` | `b4ce11db5c030fb0` |
| khoid_outdoor5 | `54f98da5d3eb5021` | `a5123279aa4361cd` | `1fd3f00acbd1c90a` | `22bed7a0b9611e7a` |
| khoid_outdoor6 | `dd8d0d5833f19dee` | `7e12b8fee972cd13` | `44b9a0d4d362ee8b` | `13b70f5f3cf537e6` |
| khoid_outdoor7 | `5c15c889f091c669` | `39180863ccbba5c9` | `365814e4f9b50bfb` | `3d963656cdf7e120` |
| khoid_outdoor8 | `1c05d2fce725faa9` | `0f88b5d02e29530b` | `87567f3b5734ea47` | `f5ef42aad0ec94f7` |
| khoid_outdoor | `c4f8b214dda77b4c` | `64716f209e4549a6` | `c4e3aab9f3c13982` | `34913385f0c2242a` |
| khoipd_basement1 | `f1cdc84f9b73b1f0` | `c5218b9b2cc556d9` | `e1f26a27604a5706` | `a7cc64cfcd48e0f2` |
| khoipd_basement2 | `96bfef1e046c9b97` | `e72da10a8a611299` | `7385d9be63ab0158` | `83bc51a67dcdf862` |
| khoipd_basement | `6fdf354a46b385be` | `a4d35efbbaf4683c` | `0af5170df4c13716` | `f6af5b621ddb54d3` |
| khoipd_indoor1 | `b9e2d98ee6bc282b` | `f3852bbb9c224f96` | `62849b891e9bcefb` | `48e90815bd0f5b24` |
| khoipd_indoor2 | `c1367dd92963066a` | `12b271c12ace3fe6` | `015b554168ee4a05` | `8531c3dd51e934c2` |
| khoipd_indoor_figure1 | `f256e44c2c9f8e95` | `e5cc1eb222bbc99f` | `d0d76ea23041f409` | `42cf890300cab25d` |
| khoipd_indoor_figure2 | `29b6b1664b46b910` | `1089f4b9ad8ecac2` | `1e0af1d3dd537e2f` | `c5e78fbe4b651a90` |
| khoipd_indoor_figure | `93845d0bc26776ee` | `3efb26ecf9a36254` | `0441f9514a3272e3` | `1cd2f9bc9eeee44f` |
| khoipd_indoor | `a8e19ddb66ff4a39` | `52edc062557b3526` | `df01da6f5f95e4d3` | `1044ba331f6d19ad` |
| khoipd_night1 | `06492195856ea395` | `6fd1603154ffd2a9` | `17247c8606227070` | `e6401c475b539f69` |
| khoipd_night2 | `c0e015255d563c3f` | `60a553133300b7a9` | `04613c3143216295` | `664d9347c39bdec8` |
| khoipd_night3 | `7bafac4395d12bb6` | `cc5d1276f1134c2c` | `bea3ebfcda2feb6e` | `97c10bdc97b6e708` |
| khoipd_night4 | `aaa9d8696fa35c88` | `3e28e45e4a56d11b` | `c8a28efd22ce8fac` | `25cd35957b899bc0` |
| khoipd_night5 | `edbc52e63597dabb` | `257dc6bc88497573` | `c0ba24be744008bf` | `1164530a2b8c3b96` |
| khoipd_night6 | `ffcd389e1bdf9394` | `bb8c39a1d1613c99` | `a955426ef19c62b3` | `b6e4badcc40f4292` |
| khoipd_night | `4a68adda77fd93f3` | `0f3177fe3e178117` | `627c7a1035eabd24` | `a14c56b64ec7808a` |
| *sequence, frame 28* | `ca18efa337d01d41` | `07087cdddb69f3a2` | `30c09313746a5b26` | `5e63039c57aaa3d6` |

## 4. 4K and boundary geometry

| Case | Input | Output | Check | Result |
|---|---|---|---|---|
| `full_rggb_3840x2160` | 3840×2160 RGGB pattern, every block, Resizer off | 3840×2160 | NV12 SHA-256, 898 statistics reads, 2 frames | PASS |
| `full_gbrg_3840x2160_fhd` | 3840×2160 GBRG → 3840×2158 after the crop, Resizer FHD (the DEC-23 / owner query A1 case) | 1920×1080 | as above | PASS |
| `edge_min_4x4_full_rggb` | 4×4, the smallest frame the pipeline supports (ALG-DMS-03); every grid and the FHD mode are larger than the frame | 4×4 | pipeline vectors + E2E | PASS |
| `edge_crop_6x6_full_bggr` | 6×6 BGGR, cropped on both axes to 4×4 | 4×4 | as above | PASS |
| `edge_strip_3840x4_full_rggb` | maximum width, minimum height | 3840×4 | as above | PASS |
| `edge_strip_6x2160_full_grbg` | minimum width after the GRBG crop, maximum height | 4×2160 | as above | PASS |

All six cases use the `full` preset, DG included.

## 5. G-CONT, synthetic part — continuous operation with injected events

`tests/data/sequences/cont_100` (seed 20261001) holds 53 bursts and 102
submitted frames, all compared. The frames use five geometries covering all
four Bayer orders:

| Geometry | Bayer | Bursts |
|---|---|---|
| 64×48 | RGGB | 7 |
| 66×34 | BGGR | 17 |
| 96×64 | GRBG | 6 |
| 130×74 | GBRG | 22 |
| 644×484 → VGA | RGGB | 1 |

Each geometry is placed at fixed bursts, not left to the random generator.
Before each burst, while the pipeline is idle, 1–3 random configuration
deltas change one of: every pixel block (DG included), a commit gate written
with or without `updated`, LUT reloads, an LSC load and profile switch, an
AEC grid with commit, without commit or with commit of `en` = 0, legal and
illegal AWB/AEC grids, AF, Resizer, CSC, geometry.

| Event | Bursts | What the bench checks |
|---|---|---|
| none | 25 | every frame |
| soft reset, COMMON / DMA | 4 / 4 | reset inside the first frame (no DONE yet); ownership cleared, counters kept, rotation back to 0; the resubmitted burst completes; statistics reflect the reset |
| IDMA AXI read error | 5 | error flagged, input buffer released, rotation kept; re-arming the same index completes the burst; the aborted frame's SOF effects (FRAME_ID, AEC commit, GTM swap) are visible afterwards |
| ODMA AXI write error | 5 | not DONE, not FREE, rotation kept; the next frames stall on that buffer until it is freed again; the pipeline's EOF effects of the failed frame are visible (statistics, GTM curve, 2DNR variance); the frame is resubmitted |
| overflow | 5 | lossless stall after exactly one frame, overflow flag and IRQ, completion after the buffers are freed |
| underrun | 5 | level held against W1C while idle; a new IRQ edge after the burst |
| counter wrap | 1 | FRAME_ID counter and `IDMA/ODMA_FRAME_COUNT` set to 2^32 − 2 (debug fixture) and wrapped inside the burst |

The events fall on all five geometries. After every burst the bench checks:
- `IDMA_FRAME_COUNT` and `ODMA_FRAME_COUNT` against the reference's count;
- the rotation positions;
- that no ALIGN or AXI error remains;
- the full statistics readout (28 563 reads over the sequence).

Any wait without progress fails as a deadlock.

**Result: 53 bursts, 102 frames compared, 95 155 checks, 0 failures, 0
deadlocks.**

Mutation check of the paths only G-CONT reaches (re-run on this sequence,
2026-10-01):
- **Killed:** soft reset keeping the 2DNR variance (3 failures); soft reset
  keeping the statistics results (848); an aborted frame not consuming a
  FRAME_ID (159); DG gain ignored (91).
- **Not observable at burst boundaries:** AF busy left set after an abort,
  and a pending AEC commit kept through a soft reset. `test_stats` covers
  both.

## 6. Host benchmark (record only; there is no host real-time requirement)

`fx1_isp_run_raw`, one process per run, queue 4. The per-frame cost comes from
the 4-frame minus 1-frame difference. Peak RSS includes the 128 MB test RAM.

| Geometry | Profile | Host s per frame | Peak RSS | Simulated per frame | Host / simulated |
|---|---|---|---|---|---|
| 2688×1520 → 2686×1518 | basic | 0.27 | 156 MB | 8.17 ms | ≈ 33× |
| 2688×1520 → 1920×1080 | full | 1.65 | 156 MB | 8.17 ms | ≈ 202× |
| 2688×1520 → 640×480 | full_vga_709 | 1.61 | 156 MB | 8.17 ms | ≈ 197× |
| 2688×1520 → 960×540 | gtm_manual_qhd | 1.23 | 156 MB | 8.17 ms | ≈ 151× |
| 3840×2160 | full, Resizer off | 3.20 | 156 MB | 16.6 ms | ≈ 193× |
| 3840×2160 → 1920×1080 | full | 3.18 | 156 MB | 16.6 ms | ≈ 192× |

Simulated time is `H × (W + pipeline_line_overhead_cycles) × core_period`
(M2-A11), here with no line overhead and no memory latency (test RAM).
Frames queued back to back follow each other with no gap: 28 frames take
228.8 ms. That gives 16.6 ms for a 4K frame, about 60 fps at 500 MHz and one
pixel per clock. It is a model parameter, not a silicon measurement
(DEC-06). On the host, the full chain at 4K runs at 0.31 frames/s. The
Python reference needs about 2 GB per real-RAW frame (frame-based numpy).

## 7. Limitations carried into M5

- No owner golden, RTL or tuning (DEC-05). Owner queries are listed in
  `plan/alg/OWNER_QUERIES.md`; B17 and B20 still await the owner.
- LT model: no outstanding AXI transactions and no Y/UV interleave at burst
  granularity (M2-A10); timing is parameterised, not measured.
- Real-frame NV12 depends on test profiles. Calibration is outside the scope.
- In the real sequence, statistics are compared after the last frame only.
  With frames back to back, the single zone memory and the globals of frame
  N are overwritten by N+1 before a driver could read them (DEC-30), so
  checking them per frame would require a non-streaming driver. The
  per-frame statistics paths are covered by `test_stats` and G-CONT (after
  every burst).
- SPEC-04 (buffer tags and timestamps) and CSR-14 (Input Formatter crop
  window) remain GAPs: the CSR map has no registers for them.

## 8. Review findings on M4 (2026-10-01)

| ID | Finding | Fix |
|---|---|---|
| M4-R1 | G-CONT lacked the plan's sequence of the 28 real frames in one VP session; the regression started every frame in its own session | `fx1_isp_run_raw --input-list --queue 4`; regression mode `sequence`, one per preset (§3.2) |
| M4-R2 | In two-frame runs only the last output was compared: 196 frames processed, 112 compared | `--per-frame`: every frame's NV12 is compared (§3.1, 308 / 308) |
| M4-R3 | `full` was described as "every block" but did not enable DG (reset `en` = 0); G-CONT had no DG delta either | DG enabled in `full` (gain 1.0625) and `full_vga_709` (0.9375); DG delta added to G-CONT; vectors, 4K checksums and baseline regenerated |
| M4-R4 | Found while fixing M4-R3: after regeneration, the synthetic sequence no longer contained the BGGR geometry, so "all four Bayer orders" would have been false | Each geometry is placed at fixed bursts |
| M4-R5 | Found while fixing M4-R1: the driver started the DMA before queuing the first buffers, which reported a start-up underrun | The first buffers are queued before `DMA_CTRL` enables the engines |
