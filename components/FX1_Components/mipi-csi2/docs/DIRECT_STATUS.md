# Direct CSI-2 → ISP status

Plan: `plan/DIRECT_CSI_ISP_IMPLEMENTATION_PLAN_20261118.md`, checklist
`plan/DIRECT_CSI_ISP_CHECKLIST_20261118.csv` (D0 → D5, then the CSR migration
C0–C7). Not committed; one review round per milestone.

## D0 — contract and baseline (2026-11-20, closed by review: plan/DIRECT_D0_REVIEW_CLOSURE_20261120.md)

Documents only: no model, driver or firmware source changed.

### Deliverables

| Item | Where |
|---|---|
| Integration contract | [DIRECT_CSI_ISP_CONTRACT.md](DIRECT_CSI_ISP_CONTRACT.md) |
| Decisions | [spec_decisions.md](spec_decisions.md) P19–P28 (D0; P29 added in D1a) |
| Expected AXI4-Stream vectors (independent of the model) | `examples/axis_source_demo/gen_expected.py`, `examples/axis_source_demo/expected/*.csv` (TPG RAW12 / RAW10 32×8, sensor BFM RAW12 34×8 with a TKEEP 0x0f tail, RAW10 32×8); `--check` regenerates and compares |
| Information for the ISP owner | [AXIS_STREAM_FOR_ISP_OWNER.md](AXIS_STREAM_FOR_ISP_OWNER.md) + data (contract §13, D2.1); the harness is internal |
| Baseline manifest, DDR intervals, test inventory | below |

### Decisions taken with the user (2026-11-20)

| Topic | Decision |
|---|---|
| Baseline | CDC-VP `36ad26a4` (the plan names `59c39ec`; two later NPU commits, `fab28cb9`, `36ad26a4`); same working rules as M0–M6 |
| Edits allowed | ISP model and ISP driver; minimal read-only observer hooks in the CSI controller (no CSR, IRQ, counter or behaviour change); D-PHY, CPU and DDR3 models untouched |
| Source select | ISP v1.07 has no pixel-source CSR (CSR workbook and MAS checked) and is memory-to-memory by specification → VP-only block `ISP_INGRESS` at 0x1105_0000, no new PLIC source (P20) |
| Insertion point | the ISP's internal IDMA → Input Formatter stream boundary (ISP MAS v1.07 Table 6-15), `in_fifo` in the model |
| Memory mode with the stream bound | ingress always ready, beats discarded (P21) |
| Publish gate | camera manager: ODMA completion + CSI terminal success, same capture tag (P26) |
| No credit at SOF | drop the whole frame (P23) |
| Buffering | 16-beat FIFO + 1 line + the 2-line `in_fifo` (P23) |
| Invalid frames | discard / abort + count, never published (P24) |
| Terminal outcome | CSI observer hook with IDs: OK / DROPPED / MISSING_FE / RESET (P25); per-beat `vp_frame_id` sideband for the SOF binding (review F1) |
| CSI reset | hook makes the ingress abort (P27) |
| ISP-owner sample | full `Csi2RxController` (TLM CSR programming, `set_stream_sink()`, D-PHY object required); TPG sessions plus sensor-BFM sessions through the D-PHY for the partial tail, CRC drop and WC error; delivered right after D0 (before D1); **information, data and traces only**, no repository/source/build for the owner (clarified after review) |
| Package (D5) | replace `VP_FX1_V3.0` (name and paths kept), rollback archive, tuning compatibility gate (plan §9) |

### Facts found while freezing the contract

1. `sys_capture_done` fires at the FE packet, **before** the last stream beats
   leave; neither done nor dropped carries a FRAME_ID.
2. A capture whose FE is missing is closed by the next FS and counted
   **delivered** by the CSI.
3. CRC/ECC/lane-abort errors never mark data beats: the line may leave whole
   and a null abort beat follows. TUSER[1] on data beats comes only from the
   WC-alignment error.
4. STREAM_CONFIG resets to 0x3: enabled, **packed**. Direct needs RAW16.
5. TID is always 0 in the model (`supported_vc_mask` = 0x1); unsupported
   exposures can be tested only with synthetic beats (D1).
6. The TPG truncates the width to a multiple of 4: no partial tail beat; tails
   and packet errors exist only on the sensor path.
7. A CSI reset drops TVALID with no TLAST and ends captures silently.
8. The ISP host-stream slots overlap the camera RAW/NV12 regions; safe because
   the ISP has one owner at a time (contract §12).
9. The ISP component's own unit tests are built only in a stand-alone ISP
   build (`FX1_ISP_BUILD_TESTS` defaults to `PROJECT_IS_TOP_LEVEL`); the fx1_soc
   CI does not run them. D1 adds that build to its regression.

### Baseline manifest

- CDC-VP `36ad26a4b200232ff49570c2b31f1120ccec5bd3` (2026-11-19, "NPU Docs VP
  FX1"), working tree clean before D0.
- Source hashes of the 27 files the direct path touches or depends on, and
  document hashes: `plan/evidence/direct_d0_20261120/source_hashes.txt`,
  `doc_hashes.txt`.
- Toolchain: host GCC/G++ 11.5.0, RISC-V GCC 15.2.0 (xPack), SystemC 2.3.4,
  CMake/Ninja; environment prefix `CC=/usr/bin/gcc CXX=/usr/bin/g++
  PATH=/usr/bin:/bin:$PATH`.
- Build/test commands used:
  - fx1_soc CI: `platforms/fx1_soc/ci/run_ci.sh --build-dir <dir> --jobs 8`
  - ISP stand-alone: `cmake -S components/FX1_Components/isp -B <dir> -DCMAKE_BUILD_TYPE=Release -G Ninja && cmake --build <dir> -j8 && ctest --test-dir <dir> -j8`

### Baseline test results

| Suite (baseline `36ad26a4`) | Result |
|---|---|
| fx1_soc `run_ci.sh`, fresh directory, camera build, 1 fs, -j8 | PASS: 536 required tests registered; smoke stage 472/472 (1018 s); components 6/6; regression 59/59 (378 s); NPU-off stage PASS |
| ISP component, stand-alone Release build | 36/36 PASS (152 s) |

### Affected tests (to stay green, and extended in D1–D4)

| Group | Tests at the baseline |
|---|---|
| ISP component (stand-alone build) | 36 tests: `fx1_isp.unit.control`, `fx1_isp.unit.dma_rules`, `fx1_isp.unit.row_stage`, `fx1_isp.unit.kernels`, `fx1_isp.unit.stats`, `fx1_isp.unit.ee_profiles`, `fx1_isp.unit.register_engine`, `fx1_isp.unit.profile`, `fx1_isp.unit.pipeline_vectors`, `fx1_isp.unit.block_vectors`, `fx1_isp.integration.dma_tlm`, `fx1_isp.integration.e2e_tlm`, `fx1_isp.integration.outputs_tlm`, `fx1_isp.integration.demosaic_tlm`, `fx1_isp.integration.csr_tlm`, `fx1_isp.integration.driver_tlm`, `fx1_isp.integration.driver_tlm_fast_csr`, `fx1_isp.integration.cont_tlm`, `fx1_isp.package.consumer`, `fx1_isp.integration.e2e_4k.full_gbrg_3840x2160_fhd`, `fx1_isp.integration.e2e_4k.full_rggb_3840x2160`, `fx1_isp.reference.traceability_up_to_date`, `fx1_isp.tools.migrate_profile`, `fx1_isp.reference.vectors_up_to_date`, `fx1_isp.reference.block_vectors_up_to_date`, `fx1_isp.reference.sequence_up_to_date`, `fx1_isp.tools.nv12_preview`, `fx1_isp.tools.run_raw_colorimetry`, `fx1_isp.tools.run_raw_profile_replay`, `fx1_isp.tools.csrw_codec`, `fx1_isp.tools.make_profile_defaults`, `fx1_isp.tools.input_depth`, `fx1_isp.tools.demosaic_capture`, `fx1_isp.tools.demosaic_select`, `fx1_isp.tools.demosaic_reference_self_test`, `fx1_isp.reference.4k_vectors_up_to_date` |
| fx1_soc ISP | `fx1_smoke_isp_stream_*`, `fx1_smoke_isp_frame_id*`, `fx1_smoke_isp_mas0710*`, `fx1_smoke_isp_recovery*`, `fx1_smoke_isp_demosaic_dir_refused`, `fx1_regression_isp_*` |
| Camera memory path | `fx1_smoke_camera_*` (40 including M5/M6), `fx1_regression_camera_isp_*`, `fx1_regression_freertos_cli_camera_*`, `fx1_smoke_freertos_cli_camera*` |
| CSI component | `fx1_csi2_*` (137 registered in the fx1_soc build; non-PHY ones in the CI smoke stage, PHY ones in the component regression) |
| FreeRTOS / host stream / NPU | `fx1_smoke_freertos_*` (34), ISP host-stream platform/launcher tests, NPU (115) — unaffected by design, run in the full CI |

### Sample for the ISP owner (D2.1 source side, delivered with D0)

`examples/axis_source_demo/` (README, harness, expected vectors), target
`fx1_csi2_axis_source_demo`, tests `fx1_csi2_axisdemo_*` (label `fx1_csi2`;
sensor sessions also `fx1_phy_slow`; not in the fx1_soc CI smoke selection).
The harness uses the real `Csi2RxController` (CSR over TLM, `set_stream_sink()`)
and needs no ISP, firmware or CSI DMA. No CSI source changed.

| Session | Result (component build, Release) |
|---|---|
| `tpg_raw12`, `tpg_raw10` | PASS: 64 beats each = independent expected vectors (~5 s) |
| `tpg_stall` | PASS: 20 refused cycles at the SOF beat, mid-line, an EOL beat and on line 6; payload unchanged, refused beat held stable (checked in the trace) |
| `tpg_abort` | PASS: 29 pixel beats (28 accepted + held beats drained), then `KEEP 0x00, LAST 1, TUSER 0b10` |
| `bfm_tail` | PASS: RAW12 34×8 through the D-PHY, 9 beats per line, tail `TKEEP 0x0f` (~7 s) |
| `bfm_crc` | PASS: frame 1 36 pixel beats then one abort beat; frame 2 whole (72 beats) |
| `bfm_crc_last` (added after re-review R3) | PASS: frame 1 all 72 payload beats = expected, then the abort beat; frame 2 whole (145 beats) |
| `bfm_wc` | PASS: `TUSER[1]` with `TKEEP 0x0f, TLAST` on line 3's last beat |
| `expected_check` | PASS: `gen_expected.py --check` |

Sensitivity: one altered byte in the expected TPG vector, or the tail TKEEP of
the BFM vector, makes the session FAIL; restored afterwards.

Found while building it: from reset (STREAM_CONFIG 0x3) a single write that
disables the stream and switches to RAW16 is a format change while enabled
(model warning), and `fx1_csi2_stream_config(dev, 1, 1)` alone returns
`FX1_CSI2_ESTATE` without writing. The sequence is 0x2 → 0x0 → 0x1, i.e.
`stream_config(dev, 0, 0)` then `stream_config(dev, 1, 1)` (contract §3,
README).

Internal evidence (traces, accepted beats, expected vectors, README,
sources, revision): `plan/evidence/direct_d0_20261120/axis_sample/`, built
from `36ad26a4` plus the uncommitted D0 files listed in its `REVISION.txt`.

**Delivery to the ISP owner** (information and data only, scope clarified
2026-11-20): `plan/evidence/direct_d0_20261120/isp_owner_delivery/`:
`CSI2_AXIS_STREAM_FOR_ISP.md` (copy of
[AXIS_STREAM_FOR_ISP_OWNER.md](AXIS_STREAM_FOR_ISP_OWNER.md): data path,
signal mapping, decoding of TDATA/TKEEP/SOF/EOL/TID for RAW10/RAW12,
handshake, errors/abort, configuration, sessions, trace excerpts), `data/expected/`
(TPG RAW12/RAW10 32×8, sensor RAW12 34×8), `data/traces/` (8 sessions, trace +
accepted beats), `MANIFEST.sha256`. No source, CMake or build instructions.
Checked against its own data: accepted payloads equal the expected frames,
the sensor formula holds for every sample, 20 refused cycles in `tpg_stall`,
the CRC/WC/abort excerpts match the files. Sending it is the user's step; not
recorded as sent.

### Review round 1 (`plan/DIRECT_D0_REVIEW_20261120.md`)

| Finding | Disposition |
|---|---|
| F1 (P1): a CSI terminal before the accepted SOF does not mean the frame has no stream (reviewer repro: capture delivered with 0 beats accepted, then 72/72 valid beats) | Accepted. Binding by ID instead of event order (user decision 2026-11-20): the stream master forwards the existing per-beat FRAME_ID sideband (M18 `tag`) as `AxisBeat::vp_frame_id` (VP sideband, not a Table 6-67 signal, not compared, not in the owner delivery); the accepted SOF binds to the context entry with that FRAME_ID; an early terminal stays in its entry; captures with no SOF are retired NO_STREAM only when a newer capture's SOF is accepted (FRAME_ID ring order) or on reset / source switch / stop. Contract §7, §9, §9.1 rules 2, 3, 5; P25 amended. Regressions listed in contract §9.3 (D1 synthetic, D2 with the CSI) |
| F2 (P2): no reservation rule for admitted frames' terminals | Accepted (user decision 2026-11-20): one table of 8 entries shared by PENDING / ACTIVE / READY, reserved at `capture_start`; no free entry → capture untracked, frame dropped at its SOF (NO_CONTEXT), nothing evicted; frames not admitted are counters only (no readable record); accounting `free + PENDING + ACTIVE + READY = 8`, ACTIVE ≤ 4; firmware not popping blocks new admissions only. Contract §6, §8, §9.1 rules 1, 4, 6–8; new P28; regressions §9.3 |
| F3 (P2): artifact could not be rebuilt from the base revision | Withdrawn by the reviewer: the user clarified that the ISP owner receives information, data and traces only, no repository, source or build. The delivery is now a self-contained document plus data (below); the harness stays internal |
| Latency note | Fixed: contract §4 separates the capacity bound (storage) from latency; states rates, the no-stall latency (own transfer + ≤ 2 pipeline line times, ≤ 15.4 µs at 3842 samples) and that there is no time bound under ISP/ODMA stall (ended by ISP resume, CSI drop, or the manager's deadline); P23 amended |

Still documents only: no model, driver or firmware source changed in this
round. `vp_frame_id` and the ingress are implemented in D1/D2.

### Re-review (`plan/DIRECT_D0_REREVIEW_20261120.md`)

| Finding | Disposition |
|---|---|
| R1 (P2): the 14-bit FRAME_ID ring is ambiguous: untracked captures advance it, a full reset clears it, "8 tracked" does not bound the distance | Accepted (user decision 2026-11-20): the key is a **64-bit capture sequence** allocated by the VP observer with the FRAME_ID at every capture start, never reset by any CSI reset, carried on each beat next to the M18 FRAME_ID tag and presented as `AxisBeat::vp_capture_seq` (with `vp_frame_id` for diagnostics / RAW dump); hooks `capture_start(seq, id, ts)`, `capture_end(seq, outcome)`; lookup, binding and NO_STREAM retirement by plain integer order of the sequence; FRAME_ID never a key. Contract §7, §9, §9.1 rules 2, 3, 5; P25; regression across a FRAME_ID wrap / half range with untracked captures and no POP (§9.3) |
| R2 (P2): reset made ACTIVE entries READY before the ISP/ODMA had drained | Accepted (user decision 2026-11-20): new state CANCELLING (outcome RESET latched, reservation, TAG and output slot held). CSI reset: abort marker down the pipeline, READY only after the ISP retirement on every output; the CSI reset does not reset the ISP. ISP reset: READY only after the reset release with no admitted frame unretired and no AXI transaction outstanding (ISP MAS §9.9.3 IDLE; model `axi_outstanding`). Found while checking the model: the ISP reset clears `odma_busy` at once, so `DMA_STAT.ODMA_BUSY = 0` does not prove the drain and is not used. PENDING released at reset. Accounting `free + PENDING + ACTIVE + CANCELLING + READY = 8`, `ACTIVE + CANCELLING ≤ 4`. Contract §7, §9.1 rule 8, §10; P27, P28; reset regressions §9.3 |
| R3 (P2): the owner document called a frame good at its H-th line and claimed the CSI status reports a missing FE | Fixed: pseudocode says "payload complete, not yet validated", an abort beat after the last line still invalidates the frame, the in-band status is final only at the next SOF; a missing FE is not visible on the stream and is counted delivered by the CSI; validation is a separate per-capture outcome. New internal session `bfm_crc_last` (CRC of the last line): 72 payload beats equal the expected frame, then the abort beat, frame 2 whole; added to the document (§5, §7, §8) and the bundle |

Verification of this round (component build, Release, GCC 11.5.0, SystemC
2.3.4): `fx1_csi2_axisdemo_*` **9/9 PASS** (16 s wall, the 8 earlier tests
plus `bfm_crc_last`); mutant without the CRC injection → `bfm_crc_last` FAIL
(144 beats), restored → PASS (145 beats); the 14 earlier trace files are
byte-identical to the bundle. Logs: `plan/evidence/direct_d0_20261120/rereview_fix_r2/`.
Bundle: 20 files in `MANIFEST.sha256`, document copy identical to
`AXIS_STREAM_FOR_ISP_OWNER.md`, every excerpt checked against the data; the
previous bundle kept as `isp_owner_delivery_r1_20261120/`. Contract changes
remain documents only; `vp_capture_seq`, CANCELLING and the ingress are
implemented in D1/D2.

### D0 gate

| Gate item | Status |
|---|---|
| Concrete decisions; RAW10/12 vectors bit-exact | done: P19–P28, `gen_expected.py` vectors (independent of the model) |
| Source select controllable from firmware | specified (`ISP_INGRESS`, P20); implemented in D1/D2 |
| Lifecycle handles late error / abort / credits | specified (contract §7–§10); association by `vp_frame_id` and the 8-entry accounting after review F1/F2 (§9.1, P25, P28) |
| Baseline and CSR encoding locked | done (P19, manifest above) |
| CSI output confirmed by the owner ≠ ISP consumer compatibility | stated (contract §1): the direct path is a VP extension of both IPs |

## D1a — ISP stream ingress, admission, IDMA bypass (2026-11-20, closed by review: plan/DIRECT_D1A_REVIEW_CLOSURE_20261123.md)

Scope (user decisions 2026-11-20): D1 split into **D1a** (D1.1 + D1.2: ingress
handshake and geometry, common admission, IDMA bypass, NV12 against the
independent reference, RAW10) and **D1b** (D1.3: context table limits,
CANCELLING / resets, late ISP errors, FRAME_ID wrap, no-POP); review after
each. The ingress is part of the ISP model; the ISP component suite joins the
fx1_soc CI (stage `isp`).

### Changes (uncommitted)

| Area | Files |
|---|---|
| Public interface | `isp/include/fx1_isp/fx1_isp_ingress.h` (`stream_ingress`: `axis_cycle`, capture observer, `source_reset`, `set_bound`), `fx1_isp_ingress_regs.h` (ISP_INGRESS map, frozen: contract §6) |
| Model | `isp/src/engine/ingress.{h,cpp}` (new); `engines.{h,cpp}`: `line.direct`, frame consumers carry direct/TAG, retirement and per-channel ODMA-end hooks, `ingress_reserved` credits, IDMA gated while direct, ISP resets clear the ingress; `fx1_isp_tlm.{h,cpp}`: `ingress_csr_socket` (optional), `ingress()`, drain thread, `i_rst_n` resets the block |
| Driver | `isp/driver/include/fx1_isp/fx1_isp_ingress_drv.h`, `driver/src/fx1_isp_ingress.c` (in `fx1_isp_drv`) |
| Tests | `isp/tests/common/axis_source.h` (synthetic AXIS source, 4 ns grid, held beats, bounded waits); `integration/test_ingress_tlm.cpp`, `test_direct_e2e_tlm.cpp`, `test_direct_raw10_tlm.cpp`; `tests/CMakeLists.txt` |
| Build / CI | `isp/CMakeLists.txt` (sources, installed headers); `platforms/fx1_soc/ci/run_ci.sh` stage `isp` (stand-alone ISP build, every ctest test, the three direct tests required); `fw/fx1_soc/tests/camera_isp/camera_isp_golden.h` (provenance comment regenerated) |
| Docs | ISP `docs/ISP_MODEL_INTERFACE.md` §7, `ISP_DECISIONS_AND_DISCREPANCIES.md` §19, `ISP_CSR_TRACEABILITY.md` (regenerated: new tests and `ingress.cpp` references only); contract §6 (map), §7 (late events), §8 (buffer look-ahead), §9.1 (BUF_TS); P22 note, P29 |

No change to the CSI-2 RX, D-PHY, CPU, DDR3 or fx1_soc platform sources
(the binding is D2). With SOURCE = memory none of the new paths is active.

### Behaviour implemented

- Handshake: `axis_cycle()` never waits; TREADY = FIFO (16 beats) not full;
  a refused beat is not consumed; the null abort beat and the beats of a
  dropped / invalid frame are always taken (TREADY 1).
- Admission at the accepted SOF beat: context entry of its capture (by
  64-bit capture sequence), ISP_EN, ODMA_EN, `control_unit::admission_state()`
  with `frames_in_flight + ingress_reserved`, and the ODMA buffer each enabled
  channel will take FREE (rotation head + admitted frames whose ODMA of that
  channel has not ended). Not admitted → NO_CREDIT / NO_CONTEXT / CONFIG,
  counters and LAST_DROP only.
- Validation per beat / line: KEEP prefix and even bytes, capture sequence,
  TUSER[1], sample above depth, line length, missing lines (SOF inside a
  frame), extra lines and late abort after the last line (unless the capture
  ended OK). Invalid frames are abandoned with the existing abort marker (or
  never reach the ISP); the next frame is processed normally.
- Lines into `in_fifo` with `tag` = ingress TAG (→ BUF_TAG) and `ts` = CSI
  FRAME_ID (→ BUF_TS, P29); RAW10 native.
- Context: 8 entries reserved at `capture_start`; READY in admission order
  only when CSI outcome + stream outcome + ISP retirement are all known
  (publish gate input); POP frees. NO_STREAM retirement by capture-sequence
  order. CANCELLING / reset paths are implemented but tested in D1b.
- IDMA: no read, no IDMA_* counter or interrupt in direct; the switch to
  direct needs IDMA_EN = 0 and an idle, drained ISP (SWITCH_REFUSED).

### Found while implementing

1. **ODMA buffer look-ahead**: the ODMA advances its rotation only at DONE,
   so a frame that has claimed the head buffer still holds it. A first
   implementation counted only unclaimed frames and dropped the second of
   two back-to-back frames with two FREE buffers (test `credit_lookahead`);
   fixed by counting admitted frames whose ODMA of that channel has not ended
   (per-channel ODMA-end hook). Contract §8 updated.
2. **RAW10 depth in profile mode**: `COMMON_INPUT_FORMAT` is an input-domain
   word, refused (TUNE_ERROR.MODE) in profile mode (ISP MAS §9.9.6). Firmware
   must set the sensor depth before applying a profile (D3). P22 note.
3. **P22 confirmed**: RAW10 native ≡ memory path RAW12 `<< 2` on 51 vectors,
   planes and statistics bit-exact; only `TUNE_AEC_RESULT_META.NATIVE_DEPTH`
   reports the configured depth (by design).

### Tests

| Test (ISP stand-alone build) | Content | Result |
|---|---|---|
| `fx1_isp.integration.ingress_tlm` | 23 cases (30 after review round 1), datapath stub, expected computed in the test: identity/capability, memory-mode discard with an IDMA frame (P21), switch rules (IDMA_EN, mid-frame, NOT_BOUND), IDMA silent in direct, nominal RAW12/RAW10 with a TKEEP 0x0F tail with and without stalls, slow-ODMA back-pressure, NO_CREDIT / ODMA off / NO_CONTEXT / CONFIG drops with TREADY 1, credit look-ahead, 10 invalid-frame causes each followed by a good frame, late abort / abort after OK / extra line, end before SOF, NO_STREAM retirement, two PENDING captures with inverted slots, publish gate (ISP done, CSI end late: DROPPED / MISSING_FE), TID | PASS, 1252 checks |
| `fx1_isp.integration.direct_e2e_tlm` | all 57 committed vectors (67 frames) through the ingress vs the Python reference: NV12 / CH1 / CH2 planes and statistics bit-exact; context OK, TAG = BUF_TAG, FRAME_ID = BUF_TS; 0 IDMA reads, IDMA_FRAME_COUNT 0, no IDMA interrupt; 5 refused geometries dropped at the SOF | PASS, 42614 checks |
| `fx1_isp.integration.direct_raw10_tlm` | 51 RAW12 vectors: memory RAW12 `(s>>2)<<2` vs direct RAW10 `s>>2`, planes and statistics bit-exact, AEC NATIVE_DEPTH checked separately | PASS, 77378 checks |
| ISP suite (39 tests) | the 36 baseline tests + the 3 above | PASS 39/39 (CI stage `isp`, 272 s) |

Mutations (`plan/evidence/direct_d1a_20261120/mutation/`): 14 faults in the
ingress / IDMA gate, each built and run against the three tests. Round 1:
10 killed by assertions, M06 (bind to any PENDING entry) and M10 (READY
before the CSI end) survived, M08 (no FREE check) and M14 (SOF inside a frame
kept) only hung. Tests added (inverted-slot binding, publish gate) and the
source's waits bounded (failure instead of hang). Round 2: **14/14 killed by
assertions**; sources restored and rebuilt.

| Mutation | Killed by |
|---|---|
| M01 IDMA not gated in direct | ingress_tlm (IDMA reads / IRQ) |
| M02 TREADY always 1 | ingress_tlm (no stall) |
| M03 refused beat consumed | ingress_tlm (beat count / output) |
| M04 RAW10 shifted in the ingress | all three |
| M05 abort beat ignored | ingress_tlm (cause) |
| M06 SOF bound to any PENDING entry | ingress_tlm (inverted slots) |
| M07 BUF_TS = TAG | direct_e2e_tlm |
| M08 no ODMA FREE check | ingress_tlm (stream stuck: bounded wait) |
| M09 look-ahead = head only | ingress_tlm (credit look-ahead) |
| M10 READY before the CSI end | ingress_tlm (publish gate) |
| M11 no late events | ingress_tlm (late abort / extra line) |
| M12 TKEEP unchecked | ingress_tlm (KEEP causes) |
| M13 TID unchecked | ingress_tlm (TID) |
| M14 SOF inside a frame kept | ingress_tlm (missing lines) |

### fx1_soc CI (fresh build directory, `run_ci.sh --jobs 8`, Release)

| Stage | Result |
|---|---|
| configure, manifest (536 required), build | ok |
| smoke | 472/472 PASS (1009 s) |
| components | 6/6 PASS |
| regression | first run 58/59: `fx1_regression_camera_isp_golden_check` reported `camera_isp_golden.h` out of date. Cause: the header's provenance hash covers `isp/src/engine/engines.cpp`, changed in D1a. Regenerated with `gen_golden.py`: **only the provenance line changed, every CRC and sample byte identical** (the memory path's NV12 is unchanged). After an incremental rebuild: the 17 `camera_isp` tests 17/17 PASS, the whole regression stage again **59/59 PASS** (365 s) |
| isp (new) | 39/39 PASS (272 s) |
| npu_off | ok |

Evidence: `plan/evidence/direct_d1a_20261120/` (`ci/` summary, JUnit, stage
logs, `gen_golden.log`, regression rerun; `mutation/`; `ingress_tlm.log`).
Additional file changed for this reason: `fw/fx1_soc/tests/camera_isp/camera_isp_golden.h`
(comment line only).

### Review round 1 (`plan/DIRECT_D1A_REVIEW_20261120.md`)

| Finding | Disposition |
|---|---|
| F1 (P2): after the accepted SOF beat and before the first line reached the pipeline, the ISP looked idle: an IDLE-class write (DEPTH RAW12 → RAW10) was accepted; 2112 wrong bytes, outcome OK | Fixed (user decision 2026-11-20: the ingress credit makes the ISP busy). The quiescence probe of the IDLE condition (ISP MAS §9.9.3) counts `ingress_reserved`; `COMMON_STATUS.BUSY` is set at the ingress admission and cleared only with no frame admitted in the ISP or the ingress. IDLE-class writes in that window are refused (`TUNE_ERROR.MODE`), as on the memory path. Profile / tuning commits keep following the pipeline's accepted SOF (same window as the memory path between IDMA arm and accepted SOF). Contract §7 |
| F2 (P2): a terminal OK before the SOF made an extra line of the same capture ignored; outcome OK | Fixed (user decision 2026-11-20: CSI observer event `capture_stream_end(seq)`). Extra data of the same capture is always EXTRA_LINES, whatever the CSI outcome; only a null abort after a capture that ended OK keeps its exception. An entry is READY only after its stream boundary: `capture_stream_end(seq)`, a beat of a newer capture, a reset or a source switch; a capture without SOF is retired NO_STREAM at once on its boundary. The CSI side of the event (observer-only, like `vp_capture_seq`) is implemented in D2. Contract §7, §9 table, §9.1 rules 5–6; P25 |
| F3 (P2): a null abort of TID 1 invalidated a valid TID 0 frame | Fixed: the TID filter runs before any frame state is touched, null abort beats included |

Regression added to `fx1_isp.integration.ingress_tlm` (now 30 cases, 1563
checks): `config_window` (depth, width, Bayer written after the SOF beat:
refused, BUSY, MODE error, value kept, outcome OK, output exact; accepted
again once idle), `extra_line_after_ok` (end OK before the extra line, with
fast ODMA, slow ODMA and "near READY": ISP retired, entry not READY before the
boundary; all INVALID / EXTRA_LINES; end before SOF without extra data OK),
`boundary_by_newer_capture`, `foreign_tid_abort` (mid-line and after the
payload: frame OK, 2 beats discarded). The synthetic source sends
`capture_stream_end` after each capture.

Mutations (`plan/evidence/direct_d1a_r1_20261120/mutation/`): the 14 earlier
plus M15 (probe ignores the ingress credit, F1), M16 (CSI OK skips extra
data, F2), M17 (READY without boundary, F2), M18 (TID filter after the abort
handling, F3): **18/18 killed by assertions** (M11's anchor re-pointed after
the `tail_event` change and run on its own).

Reviewer's benches (`plan/evidence/direct_d1a_review_20261120/repro_*.cpp`,
copies in `plan/evidence/direct_d1a_r1_20261120/reviewer_repros/`): run
unchanged, the fixed behaviour is visible (depth write refused, mismatches 0;
EXTRA_LINES counted; foreign abort ignored) but their READY waits fail,
because they predate `capture_stream_end`. With the event added after each
capture's last beat (diffs saved next to them) **all three PASS**: outcome OK
with 0 mismatched bytes, outcome 0x205 (INVALID / EXTRA_LINES), outcome OK
with abort count 0.

Regenerated again for the `engines.cpp` change: `camera_isp_golden.h`
(provenance line only, every CRC unchanged) and `ISP_CSR_TRACEABILITY.md`.

fx1_soc CI after the fixes (fresh build directory, `--jobs 8`): **PASS** at the
first run — smoke 472/472 (999 s), components 6/6, regression 59/59 (386 s),
isp 39/39 (347 s), npu_off ok. Evidence `plan/evidence/direct_d1a_r1_20261120/`
(`ci/`, `manifest.txt`, `ingress_tlm.log`, `camera_golden_comment_only.diff`).

### Re-review (`plan/DIRECT_D1A_REREVIEW_20261120.md`)

F1/F2/F3 closed by the reviewer. New finding:

| Finding | Disposition |
|---|---|
| R1 (P2): `capture_stream_end()` did not close a frame still open at the boundary: 8/32 lines or 20/66 samples of the first line left READY 0, BUSY 1, no cause, waiting for input that never comes | Fixed: at the boundary of the open frame's own capture, a partial line → LINE_LENGTH, fewer than HEIGHT lines → MISSING_LINES; `invalidate()` abandons it through the existing path (abort marker if lines reached the ISP, else the not-started branch returns the credit and clears BUSY). READY only after that retirement. Contract §7 |

Regression `short_capture_at_boundary` in `ingress_tlm` (34 cases, 1807
checks): 8/32 lines with the boundary after and before the CSI end, 20/66
samples of the first line (never reached the ISP), 20/66 samples of the last
line (abort marker after 31 lines); the source stops after each (no next
SOF): INVALID with the cause, cause counter, ISP and ingress idle (BUSY 0),
no entry left, nothing published, no DMA error; then a good frame in output
buffer 0 (no slot leaked), exact. The reviewer's two benches
(`plan/evidence/direct_d1a_rereview_20261120/repro_*_boundary.cpp`) PASS
unchanged: READY 1, BUSY 0, MISSING_LINES 1 / invalid 1 (logs in
`plan/evidence/direct_d1a_r2_20261120/reviewer_repros/`).

Mutations: the 18 earlier plus M19 (the boundary keeps the open frame):
**19/19 killed by assertions** (`plan/evidence/direct_d1a_r2_20261120/mutation/`).

Verification of this round: ISP suite from a fresh build directory **39/39
PASS** (297 s). The change touches only `capture_stream_end()` in
`ingress.cpp` and the ingress test, which fx1_soc does not reach before D2
(SOURCE stays memory), so the full fx1_soc CI was not repeated (last full run
on the previous revision: PASS). On that CI tree, rebuilt incrementally, the
tests matching `isp|camera` ran **59/59 PASS**; 8 more matched the pattern
but are not built by the CI targets and not in the manifest ("Not Run": the
7 `isp_*_test` of the separate `components/isp_tlm` and
`fx1_csi2_rxphy_drv_camera`). `ISP_CSR_TRACEABILITY.md` regenerated (new test
references only). Evidence `plan/evidence/direct_d1a_r2_20261120/`.

### Not in D1a (D1b / D2)

Context full / untracked / no-POP, FRAME_ID wrap and half range, CSI reset
and ISP reset (CANCELLING, slow write responses, `axi_outstanding`), late
ISP errors (AXI error → ISP_FAILED), CSI binding in fx1_soc (D2).

## D1b — context limits, resets, late ISP errors (2026-11-23, closed by review: plan/DIRECT_D1B_REVIEW_CLOSURE_20261123.md)

Scope: checklist D1.3 and contract §9.3: context table full / untracked /
firmware not popping, capture sequence vs FRAME_ID, TAG wrap, CSI reset and
ISP soft reset / error recovery with outputs being written (CANCELLING),
`i_rst_n`, statistics ↔ capture outcome, and the C driver (not tested in
D1a). Decision (user, 2026-11-23): the context entry reports the ISP frame ID
(`CTX_ISP_FRAME`, P30).

### Changes (uncommitted, on top of D1a)

| Area | Change |
|---|---|
| ISP_INGRESS | `CTX_ISP_FRAME` (0x06C): ISP frame ID assigned at the pipeline's accepted SOF (statistics `*_FRAME_ID`, `TUNE_CHx_BUFn_FRAME`); 0 if the frame never reached the ISP. Driver `fx1_isp_ingress_ctx.isp_frame` |
| Outcomes on ISP reset | a recovery for an error (AXI read / write response, malformed frame) → **ISP_FAILED**; a software soft / common reset → RESET (`control_unit::soft_reset_hook_cause()`, set while the reset hook runs). Only entries the ISP has **not** retired are cancelled: frames with outputs DONE keep waiting for their CSI terminal / boundary and complete normally (was: every ACTIVE entry cancelled) |
| Drain wake-up | **bug found by the new tests**: the end of a soft-reset / recovery window is a time, not an event, so a CANCELLING entry waiting for the drain was never re-checked (READY never came). The drain thread now waits until that time while an entry is draining |
| Cause field | reported only with outcome INVALID (a RESET of a frame cut mid-line no longer shows MISSING_LINES) |
| Test backdoor | `fx1_isp_tlm::debug_set_ingress_tag()` (VP-only, like `debug_set_frame_counter`) for the TAG wrap test |
| Engines | `direct_accepted(tag, isp_frame)` hook at the pipeline's accepted SOF of a direct frame |
| Docs | contract §6 (0x06C), §9.1 rule 8, §10; P27 amended, **P30**; ISP MODEL_INTERFACE §7; `camera_isp_golden.h` provenance and `ISP_CSR_TRACEABILITY.md` regenerated |

Correction of an earlier statement (contract §9.1 rule 8, R2 text): measured
in D1b, the model's `DMA_STAT.ODMA_BUSY` register is **not** cleared while a
write is outstanding (0 of 10 samples during the drain); only the engines'
internal busy count is reset at once, the register is reset by the reset
pulse that follows the drain. The ingress uses the §9.9.3 IDLE condition
(`axi_outstanding`), which is unchanged.

### Tests

`fx1_isp.integration.ingress_lifecycle_tlm` (new, datapath stub, 10 cases):

| Case | Checks |
|---|---|
| `context_full_no_pop` | 4 frames admitted and still written (slow ODMA) + 1 NO_CREDIT + 4 captures waiting for their SOF = 8 entries; the next capture untracked (CTX_FULL, counter); no deadlock: the 4 admitted complete with their own seq / FRAME_ID / ascending TAG = BUF_TAG, outputs exact; after the POPs the waiting captures are processed in order (OK, exact); the untracked capture's frame, sent after entries freed, is dropped NO_CONTEXT with TREADY 1; a capture started after the POPs is tracked and exact |
| `untracked_stays_untracked` | 1 READY + 7 PENDING + 1 untracked; a POP frees an entry before the untracked SOF: still NO_CONTEXT; the 7 older captures retired NO_STREAM at that SOF and released at their terminals; table empty |
| `sequence_and_frame_id` | two captures with FRAME_ID 5: the newer (no SOF, DROPPED) does not touch the older READY entry; FRAME_ID 16383 then 1; sequence 0xFFFFFFFF (no SOF) retired by 0x1'0000'0000 (64-bit order), CTX_SEQ_HI = 1 |
| `tag_wrap` | TAG 0xFFFFFFFF then 1 (0 skipped), = BUF_TAG |
| `csi_reset_cancelling` ×2 | CSI reset mid-frame and after all lines, slow blocking ODMA: entry not READY while a write is outstanding, POP ignored, PENDING released; READY (RESET) only after the last write; no write completing after READY, no stale write in the next 100 µs; next frame exact |
| `isp_soft_reset_drain` | soft reset with a write outstanding: entry CANCELLING, READY (RESET) only after the drain; a frame already retired (waiting for its terminal) completes OK; next frame exact in buffer 0 |
| `late_isp_error` | ODMA write error: recovery, outcome **ISP_FAILED**, DMA_ERR, READY after the drain, table empty; next frame exact |
| `hard_reset_block` | `i_rst_n`: SOURCE memory, empty table, counters 0 |
| `driver` | `fx1_isp_ingress_init` (EINVAL, ENOTSUP on a non-'ISPI' page, OK), `_bound`, `_set_source` (EINVAL, EBUSY with IDMA_EN, OK, ENOTSUP when not bound), `_ctx_peek` / `_ctx_pop` (ENODATA, every field incl. 64-bit seq and ISP_FRAME), `_ctx_ready` / `_used`, `_read_counters`, `_clear_status` |

`fx1_isp.integration.direct_e2e_tlm`: every frame of the 57 vectors checks
`CTX_ISP_FRAME = TUNE_CH0_BUFn_FRAME`; new statistics link on
`stats_flat_channels_64x48`: frame 1 CSI DROPPED after its last line (AEC
still published, outcome CSI_DROPPED), frame 2 OK: `AEC_FRAME_ID` and
`TUNE_AEC_FRAME` under a lease equal the entry's `CTX_ISP_FRAME` (43014
checks).

Mutations (`plan/evidence/direct_d1b_20261123/mutation/`): the 19 of D1a
plus M20 (CANCELLING READY without drain), M21 (error recovery reported
RESET), M22 (ISP reset cancels retired frames), M23 (CSI reset READY at once),
M24 (full table overwrites an entry), M25 (ISP frame ID not recorded), M26
(TAG 0 allowed), M27 (no timed drain wake-up), M28 (32-bit sequence order),
each run against the 4 direct tests: **28/28 killed by assertions**. M21's
first form was ineffective (operator precedence left the error cause true);
diagnosed with a temporary trace (`m21_diagnosis_run.log`), rewritten and
then killed.

### fx1_soc CI (fresh build directory, `run_ci.sh --jobs 8`)

**PASS** at the first run: smoke 472/472 (957 s), components 6/6,
regression 59/59 (357 s), isp **40/40** (the 39 earlier +
`ingress_lifecycle_tlm`; ingress_tlm 1807, lifecycle 1078, direct_e2e 43014,
direct_raw10 77378 checks), npu_off ok. `camera_isp_golden.h` differs from
HEAD only in its provenance line (every CRC unchanged). Evidence
`plan/evidence/direct_d1b_20261123/` (`ci/`, `manifest.txt`, test logs,
`mutation/`, `camera_golden_comment_only.diff`).

### Not in D1b (D2 / D3)

The CSI side of the hooks (`vp_capture_seq` per beat, `capture_start/end`,
`capture_stream_end`, `source_reset`) and the fx1_soc binding at 0x1105_0000
(D2), a real FRAME_ID wrap through the CSI (D2), the camera manager's use of
the context, publish gate and statistics link (D3).

### Review round 1 (`plan/DIRECT_D1B_REVIEW_20261123.md`)

| Finding | Disposition |
|---|---|
| F1 (P2): an ISP soft reset cleared the stream checks (`tail_`) of a frame the ISP had retired; an extra line of the same capture after the reset was accepted, outcome OK | Fixed: `isp_reset()` keeps the tail when its entry is kept (retired, still ACTIVE); it is cleared only if that entry was cancelled. Contract §9.1 rule 8 |
| F2 (P2): an ISP soft reset kept the PENDING entries; with a full table the first capture after the reset was untracked and dropped NO_CONTEXT | Fixed: `isp_reset()` (soft / common reset, error recovery) releases PENDING entries, as the CSI reset does; a late SOF of a released capture is dropped NO_CONTEXT, its terminal ignored (64-bit sequence keys: nothing can match a newer entry) |

Regression added to `ingress_lifecycle_tlm` (14 cases, 1287 checks):
`retired_frame_checked_after_isp_reset` (frame retired by the ISP, no
terminal / boundary yet, soft reset, then an extra line of the same capture →
INVALID / EXTRA_LINES; without extra data → OK), `isp_reset_releases_pending`
(8 PENDING with terminals, soft reset → USED 0 at once, the released
capture's late terminal ignored, the first capture afterwards tracked and
exact, a late SOF of a released capture → NO_CONTEXT without effect; error
recovery with 5 PENDING + the faulty frame → ISP_FAILED, PENDING released,
next capture tracked and exact). The reviewer's two benches
(`plan/evidence/direct_d1b_review_20261123/repro_*.cpp`) PASS unchanged:
USED 0 after the reset and the next capture admitted / READY; outcome 5 cause
2 with and without the reset.

Mutations: the 28 earlier plus M29 (ISP reset drops the retired frame's
stream checks, F1) and M30 (ISP reset keeps PENDING, F2): **30/30 killed by
assertions**. fx1_soc CI (fresh directory): **PASS** — smoke 472/472,
components 6/6, regression 59/59, isp 40/40 (lifecycle 1287 checks),
npu_off ok. The fix is in `ingress.cpp` only (`camera_isp_golden.h`
provenance unaffected); `ISP_CSR_TRACEABILITY.md` regenerated (test
references). Evidence `plan/evidence/direct_d1b_r1_20261123/`.

## D2a — CSI hooks, fx1_soc binding, direct-only on the platform (2026-11-23, closed by review: plan/DIRECT_D2A_REVIEW_CLOSURE_20261125.md)

Scope (user decisions 2026-11-23, P31): D2a = D2.1 binding + D2.2 context
on fx1_soc; D2b = concurrent RAW dump (D2.3), FRAME_ID wrap on the platform,
CSI reset / fault / abort recovery. ISP_INGRESS always mapped; adapter in the
platform; traffic counters behind `--traffic-report`; bare-metal tests on the
drivers; test-only FRAME_ID preset.

### Changes (uncommitted)

| Area | Change |
|---|---|
| CSI-2 RX (observer only; no CSR, IRQ, counter or timing change) | `FrameContext`: 64-bit capture sequence per capture (never reset), `capture_missing_fe` (closed by the next FS), test-only `debug_set_frame_id_counter`; `PxBeat::seq` / `OutBeat::seq` next to the M18 tag through the Line Buffer, Format Bridge (`seq_hold_`) and the master's output / skid registers; `AxisBeat::vp_capture_seq` / `vp_frame_id` (not compared; abort beats carry the last accepted beat's); `CaptureObserver` (`capture_start` with seq / FRAME_ID / FS timestamp / epoch, `capture_end` Ok / Dropped / MissingFe from the ledger, `capture_stream_end` when no beat of the capture is left in the stream path, `source_reset` on resetn / soft / full reset); `Csi2RxController::set_capture_observer()`; datapath not quiescent while a boundary is pending |
| fx1_soc | `FX1_ISP_INGRESS_BASE` 0x1105_0000 (map asserts widened to the 6 PERIBUS_1 slots), target `ISP_INGRESS` always mapped and bound to `isp.ingress_csr_socket`; with `--camera` `CsiIspDirect` (`csi_isp_direct.h`) is the CSI's stream sink and capture observer, `ingress().set_bound(true)`; `TrafficCounter` (`traffic_counter.h`) on MIPI_DMA / ISP_IDMA / ISP_ODMA with `--traffic-report FILE`; `MEMORY_MAP.md` |
| Firmware tests | `fw/fx1_soc/tests/camera_direct/direct.c` (bare metal, CSI / ISP / ingress drivers, no camera manager), `check_direct_traffic.py`; CMake `fx1_camera_direct_test`; manifest `required_tests.txt` +8 |
| CSI tests | `tests/test_csi2_capture_observer.cpp` (`fx1_csi2_observer_*`) |
| Docs | contract §3.1 (flush finding), §9 (implementation); P31; owner document §5 (flush) and its delivery copy refreshed (D0 copy kept as `isp_owner_delivery_d0_closed/`) |

With SOURCE = memory (reset) the ingress answers TREADY 1 and discards every
beat, as the unbound sink did: the memory path is unchanged.

### Found while integrating

1. **A CSI drop flushes the tail of an earlier capture.** Under
   back-pressure the Line Buffer can still hold the last lines of a capture
   the CSI has already ended OK; the drop of the next capture flushes the
   Line Buffer and Format Bridge whole (`lb_flush` / `fb_flush`). That
   earlier frame reaches the stream with 6 of 32 lines and the abort beat:
   the ingress marks it INVALID (ABORT) and it is not published, although
   the CSI counted it delivered. Contract §3.1, owner document §5.
2. **Publish gate by TAG, not by rotation.** An abandoned frame hands its
   ODMA buffer back free, and the next frame can complete in it before the
   firmware has read the abandoned frame's context: the firmware must match
   the ODMA buffer by `BUF_TAG == context TAG` (P26), never by assumed
   rotation order (the first version of the test did and failed).
3. `soft_reset` keeps STREAM_CONFIG: rewriting the RAW16 sequence after it
   would be a format change while enabled (the observer test checks it).


### Tests

| Test | What it proves | Result |
|---|---|---|
| `fx1_csi2_observer_tpg` | seq per capture, start / end / stream_end order, every beat carries its capture's seq and FRAME_ID, stream_end after the last beat leaves the master | PASS |
| `fx1_csi2_observer_tpg_stall` | same with the sink stalling (stream_end waits for the held beats, never early) | PASS |
| `fx1_csi2_observer_wrap` | FRAME_ID preset to 16382 (test-only): 16383, 1, 2 (0 skipped), the 64-bit seq keeps counting; soft_reset -> `source_reset` with the new epoch, seq survives, STREAM_CONFIG kept | PASS |
| `fx1_csi2_observer_bfm_missing_fe` | detailed PHY, missing FE -> `MissingFe` closed by the next FS; the next capture Ok | PASS |
| `fx1_csi2_observer_bfm_crc` | detailed PHY, CRC error -> `Dropped`, the abort beat carries the capture's seq, stream_end after it; the next capture Ok | PASS |
| `fx1_smoke_camera_direct_raw12` / `_raw10` | fx1_soc, detailed PHY, direct only, 64x32, 6 frames each: outcome OK, lines / samples, BUF_TAG = TAG, BUF_TS = FRAME_ID, CTX_ISP_FRAME = BUFn_FRAME, NV12 CRC = memory-path golden; traffic check | PASS |
| `fx1_smoke_camera_direct_tpg640` | TPG 640x480, 2 frames, NV12 CRC = golden | PASS |
| `fx1_regression_camera_direct_held_outputs` | no ODMA buffer free at SOF -> NO_CREDIT drops, never published; later frames OK | PASS |
| `fx1_regression_camera_direct_isp_pause` | ISP paused 5 frame periods -> real CSI drops (DROPPED / NO_STREAM), truncated frame INVALID and not published, stall cycles > 0, recovery OK | PASS |

Traffic (`--traffic-report`, checked by `check_direct_traffic.py`):

| Run | MIPI_DMA R / W | ISP_IDMA R | ISP_ODMA W (bytes), window |
|---|---|---|---|
| direct raw12 / raw10 (6 frames checked; 7 x 3072 B NV12 written: the capture in flight when the sensor stops is processed during the drain) | 0 / 0 | 0 | 336 (21504), 0x8800_0000..0x8803_83FF |
| direct tpg640 (2 frames 640x480) | 0 / 0 | 0 | 4320 (921600), 0x8800_0000..0x880F_57FF |
| negative control: memory path `camera_isp_raw12` | 0 / 288 | 256 | 384 (24576): the check exits 1 as expected |

### CI (2026-11-23, fresh build, GCC 11.5.0)

`platforms/fx1_soc/ci/run_ci.sh`: **PASS**. manifest 544/544 registered;
smoke 478/478 (472 + 3 camera_direct + 3 observer), components 6/6,
regression 61/61 (59 + held_outputs + isp_pause), isp 40/40, npu_off ok.
Evidence: `plan/evidence/direct_d2a_20261123/` (ci/, traffic/,
negative_control/).

### Mutations (7; source restored and byte-compared after each run)

Each mutation rebuilt `fx1_all fx1_csi2_unit_tests` and ran the five
observer tests, `fx1_smoke_camera_direct_raw12` / `_tpg640` and (D06)
`fx1_regression_camera_direct_isp_pause`. Evidence:
`plan/evidence/direct_d2a_20261123/mutations/`.

| Mutation | Killed by |
|---|---|
| D01 beat sideband seq = 0 (master input) | all observer tests, fw raw12, tpg640 |
| D02 stream_end ignores the beats held in the master | observer tpg / tpg_stall / wrap, fw tpg640 |
| D03 stream_end at once at capture end | observer tpg / tpg_stall / wrap, fw tpg640 |
| D04 MissingFe reported Ok | observer bfm_missing_fe |
| D05 no `source_reset` on CSI reset | observer wrap |
| D06 adapter maps Dropped to ok | **survives** (see below) |
| D07 adapter passes FRAME_ID 0 at capture_start | fw raw12 (BUF_TS = FRAME_ID) |

**D06 is not observable in the D2a scenarios.** With the CSI RAW DMA off,
a CSI drop is a Control-FSM drop: it flushes the stream path, so the dropped
capture never reaches the ingress as a frame (it is retired NO_STREAM, a
counter, no context entry to read), and a frame cut by the flush is INVALID
on its own. The unmutated `isp_pause` run gives the same context outcomes
as the mutated one (one 0x705 INVALID / ABORT, 6 frames admitted). The case
where the mapping decides publication is a CSI drop **with a complete stream**:
the RAW DMA refusing or rewinding a capture while the stream path delivers
it (D2.3, concurrent RAW dump). D2b must include that case and kill D06.

### Open for D2b

- D2.3 concurrent RAW dump: CSI DMA no buffer / rewound with a complete
  stream -> CSI_DROPPED, never published (kills D06).
- FRAME_ID wrap through the platform (preset hook exists; observer test
  covers the CSI side only).
- CSI reset / soft reset / missing FE / CRC on fx1_soc (D04 and D05 are
  killed by the CSI observer tests only, no platform test yet).

### Review round 1 (plan/DIRECT_D2A_REVIEW_20261123.md) — answered 2026-11-23

User decisions (P32): F1 hold + release to 0, F2 per-capture boundary,
F3 issue / return / outstanding counting.

| Finding | Root cause | Fix |
|---|---|---|
| F1 `capture_stream_end` before the abort beat of the same capture | the boundary looked only at beats present in the Line Buffer, Format Bridge and master registers; the abort a drop / stop sequence is about to inject (HOLD -> FLUSH -> DRAIN -> INJECT) is not a beat yet, so between two lines the capture looked finished. Second variant: an abort after the boundary of the capture it carries (drop between a FS and the first line of the next capture) | `SysDatapath::stream_resident(seq)`: also resident while `ControlFsm::abort_pending()` (new read-only accessor: HOLD, FLUSH or stream stop latched) or the master is not in PASSTHROUGH, and `seq` = `AxisMaster::last_seq()`. When the boundary is reported, `AxisMaster::release_seq(seq)`: a later abort beat carries 0. Only VP sideband / observer state changes |
| F2 boundary queue grows with the frames sent during a stall | `stream_wait_` was in order and blocked at its front: one capture held in the master kept every later capture (even with no beat in the path) waiting | the boundary is per capture: every non-resident ended capture is reported at once; `stream_wait_` keeps only resident captures, bound `line_buffer_depth + stream_fifo_depth + 4` (`stream_wait_size()` / `stream_wait_bound()` for tests) |
| F3 traffic counter misses a transaction blocked in its target | counted after `b_transport` returned | count on entry (issued, bytes, address range from a snapshot of the command), count return and errors after; `outstanding` in the report. Checker: any MIPI_DMA / ISP_IDMA transaction issued, any outstanding transaction, an ODMA write not returned or a report without the new keys -> exit 1 |

New tests (all in the CI manifest):

| Test | Checks |
|---|---|
| `fx1_csi2_sys_obs_stop_between_lines` | SysDatapath + sensor packets, core disable between two lines, ready / sparse TREADY: one abort beat carrying the capture, boundary after it, no beat after a boundary |
| `fx1_csi2_sys_obs_stop_midline` | the same inside a line (master `line_open`) |
| `fx1_csi2_sys_obs_stop_after_fs` | capture 1 complete with its boundary, capture 2 opened by its FS, core disable: the abort carries 0, never capture 1 |
| `fx1_csi2_sys_obs_pending_bound` | TREADY 0 for 256 frames: pending set stops growing once the line buffer is flushed (max 18 entries, the captures resident in the line buffer), 255 of 256 boundaries during the stall, the last capture's boundary out, drain afterwards; within `stream_wait_bound()` |
| `fx1_csi2_observer_stop_line` / `_stop_line_stall` / `_stop_midline_stall` | controller through TLM CSR, TPG 64x32, CORE_CONFIG = 0 between two lines / inside a held line: the reviewer's case |
| `fx1_unit_traffic_counter` | blocking targets: MIPI write and IDMA read counted at issue, outstanding at 1 us, returned later; ODMA error counted on return |
| `fx1_unit_traffic_checker_pending` / `_done` / `_old_format` / `_clean` | the product checker exits 1 on the pending, completed-forbidden and old-format reports (message checked), 0 on a direct-only report |

Bench note (model not changed): `SysDatapath` takes merged words into its
receive FIFO while the core is disabled and pops them only once it is
enabled again. In the SysDatapath bench the rest of a line pushed after a
mid-line stop therefore stays there and `quiescent()` stays false; the
mid-line test runs the words in and checks the drained state (Control FSM
IDLE, master PASSTHROUGH and empty) instead of waiting for quiescence.

Reviewer repros (`plan/evidence/direct_d2a_review_20261123/run_repros.py`,
run unchanged against the round-1 build, output in a new directory): all four
benches exit 0 (`observer_stop`: abort seq 1 then boundary, late beats 0;
`pending_bound`: 1024 frames, 1023 boundaries, the latest capture's boundary
out, 1 resident; `controller_stop`: event 19 abort, event 20 boundary;
`traffic_outstanding`: MIPI write / IDMA read counted at issue, outstanding
1), the product checker on that report exits 1.

CI (fresh build, GCC 11.5.0): **PASS**. manifest 556/556; smoke 490/490
(478 + 12), components 6/6, regression 61/61, isp 40/40, npu_off ok. The
camera_direct reports now carry `*_done` and `outstanding=0`.

Mutations (11, round-1 build; source restored and byte-compared): D01-D05,
D07 killed as in round 0 (D02 / D03 anchors moved to `stream_resident`);
D06 still not observable (D2b, D2.3); new:

| Mutation | Killed by |
|---|---|
| D08 boundary ignores a pending abort | sys_obs_stop_between_lines / _midline, observer_stop_line / _stop_line_stall |
| D09 abort keeps the released capture | sys_obs_stop_after_fs |
| D10 boundary in order, blocked at the front | sys_obs_pending_bound |
| D11 traffic counted after return | unit_traffic_counter (the checker tests depending on its reports do not run) |

10/11 killed. Evidence: `plan/evidence/direct_d2a_r1_20261123/` (ci/,
traffic/, reviewer_repros/, mutations/, source_manifest.sha256).

## D2b — concurrent RAW dump, platform faults, resets (2026-11-25, review round 1 answered, awaiting re-review)

Scope (P31 split): D2.3 direct + concurrent CSI RAW DMA (kills D2a mutation
D06), FRAME_ID wrap through the platform, CSI reset / fault / missing FE /
CRC recovery on fx1_soc. Contract §10 (compliant reset in the blanking) and
§11 (outcome table of a direct frame with the dump on); decision P33.

### Decisions taken with the user (2026-11-25)

| Topic | Decision |
|---|---|
| DMA outcome vs direct frame | baseline coupling kept: Refused / Rewound → CSI DROPPED → CSI_DROPPED (never published, although the stream is whole); Released with error (policy 1) → CSI OK → published; `stream_follow_dma_drop` stays C4 |
| DMA faults | no free side and layout error from firmware; AXI write error from a new VP-only option `--csi-dma-fault write-error:N`; both `err_frame_policy` values |
| CRC / missing FE | fault knob in the sensor model (`CcsSensorConfig::faults`, `--camera-fault`); PHY and CSI controller untouched |
| FRAME_ID wrap | `--camera-frame-id-preset N` (the existing test-only `debug_set_frame_id_counter`, applied after the power-on reset) |
| Resets | compliant (ingress + ISP stopped, CSI soft reset, restart), violation (soft reset inside a frame), core disable inside a frame |
| Milestone | one review (no D2b/D2c split) |

### Changes (uncommitted, on top of D2a)

| Area | Files |
|---|---|
| Sensor model fault knob (VP only) | `sensor/include/fx1_csi2_sensor/ccs_sensor.h`, `sensor/src/ccs_sensor.cpp`: `CcsSensorFault {frame, crc / no_fe, line}`, frames counted from 1 since construction; crc: the 2 CRC bytes of one line sent inverted; no_fe: FE not sent; `faults_fired()` |
| CSI RAW DMA fault shim | `platforms/fx1_soc/src/csi_dma_fault.h` (new): N-th MIPI_DMA write answered `TLM_GENERIC_ERROR_RESPONSE` (SLVERR), between `ctl.dma_socket` and the write guard |
| Platform options | `fx1_soc_top.h/.cpp`, `main.cpp`: `--camera-fault`, `--camera-frame-id-preset`, `--csi-dma-fault` (camera only, malformed → exit 4), report line `[fx1_soc] camera fault (injection): ...` |
| Firmware | `fw/fx1_soc/tests/camera_direct/direct_common.{h,c}` (new, shared with `direct.c`, behaviour unchanged), `recover.c` (new: CAM_CASE 10–13, 20–22), `CAM_FRAME_LENGTH` (sensor frame period, default unchanged) |
| Host checker | `check_direct_traffic.py --mipi-lo/--mipi-hi`: with the dump, MIPI_DMA writes > 0, all returned, no error, inside the sides; ISP_IDMA still refused |
| Build / CI | `fw/fx1_soc/CMakeLists.txt` (`fx1_camera_direct_test` SOURCE / ARGS / MIPI window; 7 recover tests; 7 refused-option tests), `platforms/fx1_soc/CMakeLists.txt` (4 checker dump fixtures), `mipi-csi2/CMakeLists.txt` (`fx1_csi2_sensor_faults`), `ci/required_tests.txt` (+18) |

No change to the CSI controller, PHY, CPU, DDR3 or the ISP model in D2b.

### Found while implementing

- **DMA drops never touch the stream** (`dma_drop` is not a Control-FSM
  abort cause): a capture the DMA refuses or rewinds reaches the ISP whole
  and is still CSI_DROPPED (dump_faults: seq 4, 5, 7, 9 with 32 lines). This
  is the case D06 needed.
- **A capture can have a valid dump and no direct frame**: after a drop with
  TREADY low the stream master drains (takes and discards every beat, frame
  gate closed); the DMA dumps the following captures whole, the ingress
  retires them NO_STREAM (dump_stall: FRAME_IDs 6, 7). Software must release
  the sides of captures without a context entry, otherwise both sides stay
  SW-owned and every later capture is Refused (first runs of dump_stall).
- **Firmware slower than the frame rate**: NV12 CRC + RAW compare take
  ~4.4 ms of simulated CPU time; at the 3 ms sensor period the firmware falls
  behind ~1.4 ms per frame until a capture finds no free side (Refused, raw
  IRQ_STATUS FRAME_DROPPED only: no overflow, no line buffer full, no AXI
  error; 3 ms diagnostic run). The recover tests use an 8 ms period
  (`CAM_FRAME_LENGTH=800u`, every translation unit); the golden does not
  depend on it. A first hypothesis (DDR contention → RAW FIFO overflow) was
  wrong and is not in the contract.
- **Compliant CSI reset in the blanking returns ENOCLK** (C20, clock lane in
  LP-11 with P16): the epoch, the datapath reset and `source_reset` are done;
  the next capture has epoch + 1 and is published. Treated as success.
- **IN_FRAME is a level**: a mid-frame action must wait for its rising edge
  (a slow firmware otherwise lands in the blanking after a frame already under
  way; first core_disable / csi_reset runs).
- With the dump on, a capture's FRAME_ID → frame association needs one
  capture per sensor frame; across a wrap the DMA buffer FRAME_ID and the
  context FRAME_ID stay equal (16382, 16383, 1, 2, 3).

### Tests (fx1_soc, sensor 64x32 RAW12 through the detailed D-PHY, 8 ms period)

| Test | Scenario | Observed |
|---|---|---|
| `fx1_smoke_camera_direct_dump_wrap` | dump on, `--camera-frame-id-preset 16381` | FRAME_IDs 16382, 16383, 1, 2, 3; 5 published; 5 dumps exact, DMA FRAME_ID = context FRAME_ID |
| `fx1_regression_camera_direct_dump_faults` | policy 0, `--csi-dma-fault write-error:106` | seq 4 no free side, seq 5 SLVERR at 0x8101_0480 (line 9, AXI_FAULT_TRACE resp 2, 1 SLVERR), seq 7 `dma_abort` inside the frame, seq 9 layout error (bytes_per_line 2W−8): each CSI_DROPPED with 32 lines, no dump, not published; 7 published with exact dumps |
| `fx1_regression_camera_direct_dump_release_error` | policy 1, `--csi-dma-fault write-error:42` | seq 2 AXI error (last valid line 8), seq 4 layout (0xFFFF), seq 6 abort (6): each published (P33), side SW_OWNED_ERROR, valid lines exact |
| `fx1_regression_camera_direct_dump_stall` | ISP paused 5 frame periods inside seq 3; stream path off inside a frame for 2 periods | seq 3 CSI_DROPPED (12 lines); seq 4–8 NO_STREAM, of which 6 and 7 dumped whole (checked, released); seq 9 CSI_DROPPED with 32 lines (its claim came before the firmware saw 6/7); stream off: seq 12 INVALID / ABORT (7 lines), its dump and 2 more taken while off exact; seq 15–16 published |
| `fx1_regression_camera_direct_sensor_faults` | `--camera-fault 3:crc:10 5:crc:31 7:no-fe` | seq 3 CSI_DROPPED (11 lines), seq 5 CSI_DROPPED (32 lines: CRC of the last line), seq 7 CSI_MISSING_FE (32 lines); 6 published, sensor frame = seq; CRC_ERROR_COUNT 2 |
| `fx1_regression_camera_direct_csi_reset` | compliant reset in the blanking, then soft reset inside a frame | compliant: rc ENOCLK, next capture epoch 1 → 2, published; violation: seq 5 RESET (7 lines), next epoch + 1, published; 6 published |
| `fx1_regression_camera_direct_core_disable` | core disabled 50 us inside a frame | seq 3 CSI_DROPPED (7 lines), the next ones published |
| `fx1_csi2_sensor_faults` (CSI suite, PHY) | sensor knob at the CSI alone, DMA policy 1 | frame 2 CRC → dropped (released with error), frame 3 without FE closed by FS 4 and delivered whole, CRC_ERROR_COUNT 1, FS/FE log without FE 3 |
| `fx1_unit_traffic_checker_dump_{ok,outside,idma,none}` | checker dump mode | accepted / refused with the expected message |
| `fx1_smoke_*_refused` (7) | malformed fault options or without `--camera` | exit 4 with the usage |

Every recover test also: context sequence, FS timestamp and epoch
monotonic, every capture tracked (no NO_CONTEXT / untracked), SOURCE back to
memory; host traffic check (ISP_IDMA 0, MIPI_DMA writes only inside the sides
with the dump, none without).

### CI (2026-11-25, fresh build `d2b-ci`, GCC 11.5.0, `run_ci.sh --jobs 8`)

| Stage | Result |
|---|---|
| configure, manifest (574 required tests), build | ok |
| smoke | 502 / 502 (D2a: 490; + dump_wrap, 7 refused options, 4 checker dump fixtures) |
| components | 6 / 6 |
| regression | 67 / 67 (D2a: 61; + 6 recover tests) |
| isp (stand-alone ISP suite) | 40 / 40 |
| npu_off | ok |

CSI suite (`ctest -R '^fx1_csi2_' -E '^fx1_csi2_rxphy_bench_'`, build `d2b`):
158 / 158 (D2a: 157; + `fx1_csi2_sensor_faults`). Host traffic re-check
of the CI reports: every dump run accepted with the side window, every
direct-only run accepted without it; a dump report checked as direct-only is
refused (rc 1, negative control).

### Mutations (13; source restored and byte-compared after each run)

Each mutation runs the tests that can see it (`mut_d2b/mutate.py`).

| Mutation | Killed by |
|---|---|
| D04 MissingFe reported Ok (D2a) | `camera_direct_sensor_faults` |
| D05 no `source_reset` on CSI reset (D2a) | `camera_direct_csi_reset` |
| **D06 adapter maps Dropped to ok (survived D2a)** | `camera_direct_dump_faults` (seq 4, no free side, stream whole: published instead of CSI_DROPPED, code 0x162) |
| B01 sensor CRC fault not applied | `fx1_csi2_sensor_faults`, `camera_direct_sensor_faults` |
| B02 sensor FE still sent | `fx1_csi2_sensor_faults`, `camera_direct_sensor_faults` |
| B03 `--csi-dma-fault` forwards the write | `camera_direct_dump_faults`, `camera_direct_dump_release_error` |
| B04 FRAME_ID preset ignored | `camera_direct_dump_wrap` |
| B05 checker ignores the MIPI window | `fx1_unit_traffic_checker_dump_outside` |
| B06 checker accepts ISP IDMA traffic with the dump | `fx1_unit_traffic_checker_dump_idma` |
| B07 `--camera-fault 0:crc` accepted | `fx1_smoke_camera_fault_frame0_refused` |
| B08 adapter maps MissingFe to ok | `camera_direct_sensor_faults` |
| B09 ErrorReleased counted dropped (CSI ledger) | `camera_direct_dump_release_error` |
| B10 adapter drops `source_reset` | `camera_direct_csi_reset` |

13 / 13 killed; D2a's D06 is closed. B07 was first run with a test regex
that matched nothing (runner error, "No tests were found"); rerun with the
corrected regex, result above.

### Evidence

`plan/evidence/direct_d2b_20261125/` (README there): CI summary and JUnit,
firmware UART logs and traffic reports of the 12 camera_direct tests,
checker re-run, mutation definitions / runner / logs, CSI suite JUnit, the
3 ms diagnostic run, source manifest (69 files, HEAD 36ad26a4 + working
changes).

### Not in D2b (D3 and later)

- Camera manager / CLI direct backend (D3); the manager must implement the
  D2b software rules: release the sides of captures without a context entry,
  keep the per-frame work under one frame period, ENOCLK of a compliant reset
  in the blanking is success.
- `stream_follow_dma_drop` and other workbook v1.4 coupling (C4).
- RAW10 dump on the platform (the D2b dump runs RAW12 / RAW16 sides; RAW10
  direct is covered by `camera_direct_raw10`).

### Review round 1 (plan/DIRECT_D2B_REVIEW_20261125.md) — answered 2026-11-25

User decisions (P34): F1 counter before the shim + declared expected errors
+ cross-check with the shim's report; F2 per-transaction windows at issue.

| Finding | Root cause | Fix |
|---|---|---|
| F1 the fault shim stood before the MIPI_DMA counter: an injected write and its error were missing from the report (platform: shim 241 writes, counter 240 / 0 errors) | the shim was bound between `ctl.dma_socket` and the write guard and the counter after the guard; the shim answers the injected write without forwarding it. The contract described the bypass as intended, against the P32 meaning of "issued" | topology `ctl.dma_socket → TrafficCounter → [CsiDmaFault] → WriteGuard → fabric` (`fx1_soc_top.cpp`, `camera::dma_next()`); checker `--mipi-errors N` (default 0: any MIPI error refused; fault tests declare 1); with a shim report on the console, its writes seen must equal the counter's issued writes, fired exactly when an error is declared, at an address inside a RAW side |
| F2 the checker accepted a write into the gap between the two 4 KiB sides | the report had only global lo / hi, checked against one continuous range; disjoint windows cannot be proven from min / max | `TrafficCounter::add_window` (`--traffic-window MASTER:LO:HI`, repeatable): each transaction is checked at entry, one not wholly inside one window is counted `outside`; the report lists the windows; the checker requires the windows to be exactly the given sides (`--mipi-window`, per side) and `outside` = 0. Layout unchanged (disjoint sides) |

Platform runs with the fix: dump_faults MIPI_DMA 241 writes / 241 returned /
1 error / outside 0 (shim: 241 seen, fired at #106, 0x8101_0480);
dump_release_error 145 / 145 / 1 / 0 (shim 145, #42). All 10 camera_direct
tests that the change touches PASS (7 recover, raw12, raw10, tpg640).

New tests:

| Test | Checks |
|---|---|
| `fx1_unit_csi_dma_traffic` | product TrafficCounter + CsiDmaFault + RAM: platform topology (3 issued, 1 error, shim saw 3, the failed write reached no memory); the refused topology as a control (counter 2 / 0); windows: clean 0, gap 1 (same global lo / hi as clean, the gap RAM written), burst past the end of side 0: 1; writes the reports below |
| `fx1_unit_traffic_checker_dump_{fault,clean}` | accepted (fault with `--mipi-errors 1`) |
| `..._dump_fault_unexpected` | the fault report without `--mipi-errors`: refused (1 error, expected 0) |
| `..._dump_lost` | the refused topology: refused (0 errors vs 1, shim 3 vs counter 2) |
| `..._dump_not_fired`, `..._dump_seen_mismatch` | error declared but no shim report; shim saw 4 vs counter 3: refused |
| `..._dump_gap`, `..._dump_cross` | outside 1: refused |
| `..._dump_wrong_windows`, `..._dump_no_windows` | one envelope over both sides and the gap, or no windows: refused |
| `..._dump_idma`, `..._dump_none` | kept (now with windows) |
| `fx1_smoke_traffic_window_{no_report,reversed,mipi_no_camera}_refused` | malformed `--traffic-window`: exit 4 |

Reviewer repros (copy, new output dir `scratchpad/reviewer_repros_d2b_r1`):
their bench snapshots the old topology and a counter without windows; with
the new checker every one of its reports is refused (fault, gap: the
findings; clean, front: no windows configured, no shim report line). The
positive controls with windows and the shim line are in
`fx1_unit_csi_dma_traffic`.

Mutations (7, `mut_d2b_r1/`, source restored and byte-compared):

| Mutation | Killed by |
|---|---|
| R01 counter back after the shim and the guard | `camera_direct_dump_faults`, `camera_direct_dump_release_error` |
| R02 counter ignores its windows | `fx1_unit_csi_dma_traffic` (fixtures needing its reports then Not Run, not counted) |
| R03 checker ignores `outside` | `checker_dump_gap`, `checker_dump_cross` |
| R04 checker ignores the error count | `checker_dump_fault_unexpected`, `checker_dump_lost` |
| R05 checker ignores the shim's writes seen | `checker_dump_seen_mismatch`, `checker_dump_lost` |
| R06 checker does not compare the windows | `checker_dump_wrong_windows` (first run: survived, no fixture had `outside` without the right windows; fixture added, rerun killed) |
| R07 reversed `--traffic-window` accepted | `fx1_smoke_traffic_window_reversed_refused` |

CI (fresh build `d2b-r1-ci`): PASS; smoke 514 / components 6 / regression 67
/ isp 40 / npu_off ok; manifest 586. Evidence
`plan/evidence/direct_d2b_r1_20261125/` (70-file source manifest).

## D3 — camera manager direct backend, bare-metal and FreeRTOS CLI (2026-11-25, done in an unattended session, awaiting the user's approval of the decisions and review)

Done in the autonomous session the user authorised on 2026-11-25 (continue
the plan after D2b round 1, decisions recorded for the next day). Every
decision below is **proposed, awaiting approval**; D2b round 1 is still
awaiting its re-review (its exact hand-over state:
`plan/evidence/direct_d2b_r1_20261125/d2b_r1_handoff_snapshot.tgz`).

### Decisions (proposed, awaiting approval)

| Topic | Choice | Alternatives not taken |
|---|---|---|
| Manager API | one manager, `fx1_camera_cfg.path` (`FX1_CAMERA_PATH_MEMORY` 0 = default, `FX1_CAMERA_PATH_DIRECT`), chosen at configure in state 0; independent of `tpg` (source) | a second manager / separate API |
| Frame metadata | `fx1_camera_frame` gains `path`, `capture_seq` (64-bit), `epoch`, `ts_ns`, `isp_frame` (0 on the memory path); direct: `capture` = low 32 bits of the capture sequence, `csi_side` = `FX1_CAMERA_NO_SIDE`, `zero_copy` 0, `isp_in` 0 (N/A, no fake side 0) | a new frame struct |
| Publish gate | READY context entry with outcome OK **and** the DONE output whose `BUF_TAG` = the entry's TAG; any other outcome counted in `stats.direct_failed[outcome]`, its output armed again unread | publish on ODMA DONE alone |
| ISP driver | new `fx1_isp_start_direct` / `fx1_isp_direct_arm` / `fx1_isp_direct_find` (ISP driver edits are in scope); the memory-path calls refuse a direct stream; `fx1_isp_dev.direct` added at the end of the struct | register pokes in the manager (as the bare-metal D2 tests do) |
| Stop (direct) | sensor standby (same deadline / ESENSOR / state 3 as memory), ingress `SOURCE` memory retried until the deadline (refused while a frame is inside), ISP stop (EBUSY retried by the next stop), CSI-2 stream path off (format kept), context entries drained into `stop_cancelled` | leave the stream path on |
| CSI-2 stream format | `stream_set()` disables the path with its current format first (reset value on + packed; after a direct session on + RAW16), the driver refuses a format change while enabled | change `fx1_csi2_stream_config` (CSI driver) |
| CLI | `camera path [memory\|direct]` (idle only), `camera mode` shows `path`; direct capture line `capture N FRAME_ID F epoch E isp frame I direct out O WxH raw n/a crc ... = golden frame n`; `camera status` adds the context outcomes and the ingress counters; memory-path output unchanged | a separate `camera direct` command set |
| Frame period of the manager tests | 8 ms (`frame_length` 800 in the test cfg): the NV12 CRC of a 64x32 frame takes ~3.5 ms of simulated CPU | 3 ms (frames dropped when the check is slow, D2b lesson) |
| Package (D5 groundwork) | ISP_INGRESS driver + headers shipped in the SDK (`package_sdk.sh`, `sdk/Makefile`, `check_package.sh`), `run_vp.sh camera-direct-cli` + `cli_camera_direct.txt`, `CAMERA_FW_GUIDE.md` and `ISP_PROGRAMMING_GUIDE.md` §9.4 | — (no export, no package run: D5) |

### Found while implementing

- **Pre-existing memory-path defect, fixed:** when both CSI-2 sides
  completed between two polls, `fx1_camera_poll` numbered the captures in
  side order, not FRAME_ID order (the hand-over already used FRAME_ID
  order): `freertos_cli_camera` printed captures 1, 3, 2, 5 once the D3 code
  shifted the CLI image's timing. Now numbered in FRAME_ID order.
- **Timing-dependent expectation relaxed (to approve):** `freertos_cli_camera`
  required `csi ok 4` in the status after `camera capture 4`; the sensor
  keeps streaming and the count depends on the CLI image's speed (code
  layout; same script: simulated 825.6 ms / csi ok 4 before D3, 844.8 ms /
  csi ok 7 after, instret +1.3 %). Now `csi ok [4-9]`; frames, golden and
  drops 0 unchanged.
- `fx1_csi2_stream_config(0, 0)` is refused (ESTATE) while the path is on in
  RAW16: a second direct configure (or memory after direct) failed until
  `stream_set()`.
- `IDMA_STRIDE` resets to 0x1E00 (not 0): the driver test checks "not
  written" against the reset value.
- The ISP traceability report had to be regenerated (new test naming
  registers: `tools/gen_traceability.py`).

### Tests

| Test | Checks |
|---|---|
| `fx1_smoke_camera_mgr_direct_raw12`, `_raw10` | 5 consecutive frames through the manager (sensor through the PHY), metadata (path, N/A RAW fields, capture sequence, FS timestamp, ISP frame, BUF_TS = FRAME_ID, BUF_TAG = TAG), NV12 = golden, consecutive sensor frames / FRAME_IDs, nothing failed, stop within the deadline, SOURCE memory and stream path off after it, direct-only traffic (MIPI_DMA 0, ISP_IDMA 0) |
| `fx1_smoke_camera_mgr_direct_tpg640` | TPG 640x480 RAW12 direct, golden |
| `fx1_regression_camera_mgr_direct_held_outputs` | 4 outputs held for 3 frame periods: frames dropped NO_CREDIT (sensor not paused), nothing published; stop while held OK in time; held NV12 exact, release after stop OK once; restart consecutive exact |
| `fx1_regression_camera_mgr_direct_odma_err` | `--isp-dma-fault odma-error:106`: ISP_FAILED counted, recovery, outputs re-armed, 5 frames published exact |
| `fx1_regression_camera_mgr_direct_sensor_stop` | I2C host off: ESENSOR in time, state 3, ingress + ISP stopped, configure refused; I2C back: stop OK; restart exact |
| `fx1_regression_camera_mgr_direct_sensor_faults` | `--camera-fault 3:crc:10 5:no-fe`: frames 3 (CSI_DROPPED) and 5 (CSI_MISSING_FE, whole, output completed) not published; published sensor frames 1, 2, 4, 6, 7 |
| `fx1_regression_camera_mgr_path_switch` | memory (3, zero-copy) -> direct (3) -> memory (3), each exact; SOURCE follows the path |
| `fx1_regression_camera_mgr_direct_sensor_lost` | the sensor's stream-off written behind the manager: the frames in flight come out (0 here: the stop fell at a frame end), dequeue ETIMEOUT within its budget (24.0 ms of 24), counted; stop OK; restart exact |
| `fx1_regression_camera_mgr_direct_start_fail` | I2C host off between configure and start: start EDEV, state 1 (still configured direct); I2C back: stop OK, state 0, SOURCE memory, ISP off, stream path off, sensor not streaming; restart exact |
| `fx1_regression_camera_direct_tpg4k` | TPG 3840x2160 RAW12 direct through the manager: host NV12 CRC = golden for both frames, direct-only traffic (ODMA 24.9 MB, MIPI_DMA 0, ISP_IDMA 0) |
| `fx1_regression_freertos_cli_camera_direct` | CLI: `camera path direct`, RAW12 4 frames / RAW10 2 frames against the golden, path refused while running, ISP owned by the camera, status outcomes all 0, back to memory (zero-copy frames) |
| `fx1_regression_freertos_cli_camera_direct_tpg` | CLI: TPG 640 direct, golden, status |
| `fx1_isp.integration.direct_driver_tlm` (ISP suite) | driver API states / refusals / arm / find / stop / memory start after |
| existing camera tests | `camera_isp_*` (memory path through the manager), `freertos_cli_camera*`, `camera_direct_*` (D2): PASS after the change |

### Mutations (9, `scratchpad/mut_d3/`, source restored and byte-compared)

| Mutation | Killed by |
|---|---|
| M01 publish gate ignores the outcome (any entry with a DONE output published) | `camera_mgr_direct_sensor_faults` (the missing-FE frame, whole with its output, published) |
| M02 `fx1_isp_start_direct` enables the IDMA | `camera_mgr_direct_raw12` |
| M03 release does not arm the output again | `camera_mgr_direct_raw12` (frames stop after four) |
| M04 no re-arm after an ODMA recovery | `camera_mgr_direct_odma_err` |
| M05 stop leaves SOURCE direct | `camera_mgr_direct_raw12` (stop check) |
| M06 stop leaves the CSI-2 stream path on | `camera_mgr_direct_raw12` (stop check) |
| M07 RAW10 not native (depth 12) | `camera_mgr_direct_raw10` (NV12 against the golden) |
| M08 CLI `camera path` ignored (cfg.path memory) | `freertos_cli_camera_direct` |
| M09 `fx1_isp_direct_find` ignores the TAG (first DONE output) | `camera_mgr_direct_held_outputs` (BUF_TAG check) — first run BUILD-FAILED (unused parameter under -Werror, mutation definition error), fixed and rerun |

9 / 9 killed.

## D3 review round 1 (2026-11-26): stop cleanup, output ownership — fixed (confirmed by the re-review; R1 below)

Review `plan/DIRECT_D3_REVIEW_20261126.md`: two P2 findings, both reproduced
by the reviewer's assertion firmware. User decisions P35 / P36
([spec_decisions.md](spec_decisions.md)).

**F1 — stop reported idle with a direct context left.** Root cause: an entry
the ISP had retired (output DONE) waits for its CSI terminal / boundary
(§9.1 rule 8, kept); the sensor stopped after a capture without FE, before
the next FS, so it never came; the ISP stop does not end it, `drain_contexts`
pops READY entries only, the SOURCE switch is refused (`idle()`: an ACTIVE
entry), and `stop_direct` set the state from the standby result alone. Fix
(`fx1_camera.c` `cleanup_direct`, `stop_direct`): after the ISP stop the READY
entries are drained; if CTX_COUNT.USED is still non-zero, one CSI-2 soft reset
(ENOCLK = done) ends the capture (RESET, drained, never published); SOURCE
memory written and read back with USED = 0, within `FX1_CAMERA_STOP_SETTLE_US`.
Otherwise EBUSY, state unchanged, `stop_cleanup_pending`: the next stop
resumes the cleanup only. State 0 / 3 only once the pipeline is stopped. A
normal stop issues no CSI reset. New `stats.stop_csi_resets` (CLI status
"stop csi resets N"); CLI EBUSY text tells which step is pending.

**F2 — a released slot armed again while the ODMA writes it.** Root cause:
`give_back` did not clear the slot's `unarmed` bit after a successful arm, and
publishing did not clear it either, so the bit set by an ODMA recovery for a
DONE-not-yet-polled output survived; once the ODMA claimed the slot (FREE 0,
DONE 0, no owner) `rearm_outputs` wrote FREE again. Fix: one `arm_output`
helper for every arm (success clears, failure sets), publish clears the bit.

**Regression** (`camera_direct/manager.c`):

| Test | Covers |
|---|---|
| `camera_mgr_direct_stop_no_fe` (case 39, `--camera-fault 1:no-fe --camera-fault 5:no-fe`) | frame 1: every line, output DONE, no FE, sensor stopped before the next FS: stop(24000) OK in 24 117 µs (deadline + slack), one CSI reset, nothing published, SOURCE memory, table empty, stream path off, never state 0 / 3 before; configure + start: frames 2-4 exact. Frame 5 the same with stop(0): ESENSOR, state 3 with the pipeline stopped, 128 µs (inside the 200 µs slack); configure refused; stop again OK; frames 6-8 exact |
| `camera_mgr_direct_odma_err_held` (case 40, `--isp-dma-fault odma-error:106`) | ODMA error with outputs 0 and 1 DONE, not polled; both published and held; output 0 released, the ODMA claims it (FREE 0xC): the poll leaves FREE 0xC (the reviewer's repro saw 0xD); the frame written there published; the rotation then waits at held slot 1: frames dropped without credit, nothing published, held NV12 unchanged, slot not armed; released: 5 frames exact through every slot, TAGs increasing |
| every dequeued frame of `manager.c` | ownership: a slot the application holds is neither FREE nor marked unarmed |
| `camera_mgr_direct_raw12` / `raw10` | a normal stop issues no CSI reset |

Note (contract §8, unchanged): the ODMA rotation does not skip a slot, so an
output the application holds stops the ring when the rotation reaches it
(frames dropped without credit until it is released). "Hold one output
through a full ring" is therefore not a reachable schedule; case 40 checks
the held output across the stall and a full ring after its release.

The reviewer's repros (`run_repros.py`, unchanged, new output directory) on
the fixed tree: both PASS (stop OK, state 0, SOURCE memory, CTX_COUNT 0,
configure again OK; FREE not changed by the poll).

**Mutations** (`plan/evidence/direct_d3_r1_20261126/mutations`):

| Mutation | Result |
|---|---|
| N01 successful arm keeps the `unarmed` bit | killed (`odma_err_held`) |
| N02 publish keeps the `unarmed` bit | killed (`odma_err_held`, ownership check) |
| N03 F2 code as reviewed (both) | killed (`odma_err_held`) |
| N04 no CSI reset in the stop cleanup | killed (`stop_no_fe`) |
| N05 cleanup result ignored (idle anyway) | **survived**: no VP schedule makes the cleanup fail once the CSI reset is done; the EBUSY-cleanup branch is defensive, not exercised (verification gap) |
| N06 CSI reset at every stop | killed (`raw12`, normal stop check) |
| N07 no USED = 0 check (switch accepted = done) | **survived**: the switch already needs no ACTIVE / CANCELLING entry; only an entry turning READY between the last drain and the switch would be left (drained at the next configure); window not reproduced |

5 / 7 killed, 2 survivors explained above. The D3 mutation M05 anchors the
old stop code and would need a new anchor to be rerun.

**CI** (`run_ci.sh --jobs 8`, fresh build `d3r1-ci`): PASS; smoke 517,
components 6, regression 79, isp 41, npu_off ok; manifest 601. Source
manifest (88 files) taken before the CI, verified unchanged after it.

## D3 re-review R1 (2026-11-26): TPG frame while a stop is pending — fixed, awaiting re-review

Re-review `plan/DIRECT_D3_REREVIEW_20261126.md`: F1 / F2 confirmed fixed; one
new P2, R1. Root cause: P35 keeps state 1 / 2 while a stop is pending, and
`start`, `poll` and the release give-back checked both pending flags, but
`fx1_camera_tpg_frame` checked only state 2 and `cfg.tpg`: after a running
stop returned EBUSY it wrote TPG_CONTROL and a CSI-2 frame started with the
ISP stopped (reviewer's diagnostic, SOURCE=memory refused by an ingress HAL
wrapper: CSI FRAME_COUNT 0 -> 1, drop_no_credit 0 -> 1). CLI `camera
capture` reached it (state check only) and then waited for the capture
timeout. Fix (`fx1_camera.c`): one `stopping()` predicate (either flag) for
start, TPG frame (before any MMIO), poll and the release give-back; the TPG
frame returns ESTATE. CLI `camera capture` refuses early ("stop pending
(camera stop again)"). Header comment of `fx1_camera_tpg_frame` states it.

**Regression** `camera_mgr_direct_stop_pending` (`manager.c` case 41, TPG
640x480 as case 33; a test-only ingress HAL wrapper drops SOURCE=memory
writes, the product driver and model unchanged, reads go to the IP):

| Phase | Checks |
|---|---|
| configured, stop(0) | EBUSY, state 1, cleanup pending, SOURCE still direct, ISP stopped; start / configure ESTATE; fault removed: stop OK, SOURCE memory, table empty |
| running, one output held, stop(1000) | EBUSY in 1 127 µs (deadline + slack), state 2 (never 0 / 3), SOURCE direct, ISP stopped |
| while pending | TPG frame ESTATE; start / configure ESTATE; poll 0; dequeue ETIMEOUT; release OK and arms nothing (FREE unchanged, held slot not FREE, `unarmed` unchanged); over one frame period: CSI FRAME_COUNT, ingress admitted / no-credit / invalid, ODMA FRAME_COUNT, `direct_ok`, ISP driver state unchanged |
| stop again (fault on) | EBUSY, no CSI-2 reset (table empty) |
| fault removed | stop OK, SOURCE memory, table empty, stream path off; configure + start: a frame exact; stop OK; no CSI-2 reset in the whole case |

**Mutations** (`scratchpad/mut_d3r2/`, source restored and byte-compared):

| Mutation | Result |
|---|---|
| P01 TPG frame ignores the pending flags (R1 as reviewed) | killed (0x41) |
| P02 release arms while pending | killed |
| P03 poll ignores the pending flags | **survived**: after the ISP stop the ready list is cleared, `unarmed` is 0 and no output is DONE, so a poll while the cleanup is pending has nothing to move; the guard is defensive |
| P04 start ignores the pending flags | killed |
| P05 cleanup result ignored (round 1 N05) | killed (was a survivor in round 1) |
| P06 no USED = 0 check (round 1 N07) | **survived**, as in round 1 |

4 / 6 killed. Not covered by a test: the CLI early refusal (the CLI
firmware has no fault fixture; the manager refuses anyway).

The reviewer's diagnostics on the fixed tree (unchanged runners, new output
directory, build `d3r2-ci`): `run_pending.py` PASS (TPG frame ESTATE, CSI
FRAME_COUNT 0 -> 0, drop_no_credit 0 -> 0); `run_repros.py` both exit 0.

**CI** (`run_ci.sh --jobs 8`, fresh build `d3r2-ci`): PASS; smoke 517,
components 6, regression 80, isp 41, npu_off ok; manifest 602. Source
manifest (88 files) taken before the CI, verified unchanged after it.
Evidence: `plan/evidence/direct_d3_r2_20261126/`.

Not changed, noted: on the memory path a stop that returns ETIMEOUT (CSI-2
DMA neither stopped nor aborted) keeps state 1 / 2 without a pending flag,
so TPG frame and poll stay allowed there (M6 behaviour, not part of D3).

## D4 — verification audit (2026-11-26, final revision after the D3 closure; awaiting review)

Revision audited and tested: CDC-VP HEAD `36ad26a4` + the D3 r2 hand-over
(88 files, `plan/evidence/direct_d3_r2_20261126/source_manifest_handoff.sha256`;
D3 closed in `plan/DIRECT_D3_REVIEW_CLOSURE_20261126.md`). Every D4 run below
used that tree (re-verified after each campaign); this section is the only
change since. Evidence `plan/evidence/direct_d4_20261126/` (mutations, CSI
suite) and `plan/evidence/direct_d3_r2_20261126/ci/` (final CI). The first
D4 draft (2026-11-25) is replaced.

### D4.1 — regression on the final revision

| Suite | Result |
|---|---|
| `run_ci.sh --jobs 8`, fresh build `d3r2-ci` | **PASS**: smoke 517, components 6, regression 80, isp 41, npu_off ok; manifest 602; JUnit 0 failure / error / skipped; source manifest verified unchanged after the run |
| CSI suite `ctest -R '^fx1_csi2_' -E '^fx1_csi2_rxphy_bench_'` (build `d3r1`, same tree) | **158 / 158 PASS**, 151 s wall (43 `fx1_phy_slow`) |
| ISP stand-alone (CI stage `isp`, same CI) | 41 / 41: direct (`ingress_tlm`, `ingress_lifecycle_tlm`, `direct_e2e_tlm`, `direct_raw10_tlm`, `direct_driver_tlm`) and memory path (`e2e_tlm`, `e2e_4k` x2, `dma_tlm`, `outputs_tlm`, `driver_tlm`, `cont_tlm`, unit stats / profile / pipeline vectors, reference vectors up to date) |

**Gap found:** `run_ci.sh` builds `fx1_all fx1_csi2_unit_tests` only; the
PHY / sensor / AXIS demo executables (`test_fx1_csi2_rx_phy`,
`test_fx1_csi2_sensor`, `fx1_csi2_axis_source_demo`) are not built, so those
40 tests (and the CSI suite as a whole) are outside the CI gate. They were
run separately here (and in D2b). Proposed (not done, needs a decision):
build them in `run_ci.sh` and add a `csi` stage, or keep the separate run as
a documented gate step.

Release / NDEBUG verdicts: every fx1_soc test ends on the firmware's
sim-control verdict (`RESULT PASS` / `FAIL code`) and the host checkers' exit
codes, the ISP tests on their own check counters; none depends on `assert`.

### Coverage by plan §8 group

| Group (plan §8) | Covered by | Gap / deviation |
|---|---|---|
| Handshake: stable beat while stalled, SOF accepted once, finite FIFO, no lost / duplicate pixel, partial last beat | ISP `ingress_tlm` (stall, KEEP / TID / SOF-in-frame), `fx1_csi2_observer_tpg_stall`, `fx1_csi2_axisdemo_*` (BFM RAW12 34x8, TKEEP 0x0f tail) | — |
| Format: RAW10 / RAW12 alignment, byte order, tail, Bayer, independent reference, no double shift | `direct_e2e_tlm`, `direct_raw10_tlm` (Python reference), `camera_direct_raw10/12`, `camera_mgr_direct_raw10/12` (golden of the stand-alone model) | — |
| Backpressure / abort: slow ISP, no output credit, short / long stall, null abort, disable / re-enable, sensor never paused, TPG stall | `camera_direct_isp_pause`, `camera_direct_held_outputs`, `camera_mgr_direct_held_outputs`, `camera_mgr_direct_odma_err_held` (ring stalls at a held slot, NO_CREDIT drops), `camera_direct_dump_stall`, `fx1_csi2_sys_obs_*`, ISP `ingress_tlm` (abort) | — |
| Validation: CRC / FE error after the last pixel or after ODMA, missing FE, reset / stop inside a frame, publish only after terminal success + ODMA drain | `camera_direct_sensor_faults`, `camera_mgr_direct_sensor_faults`, `camera_direct_dump_faults`, `camera_direct_csi_reset`, `camera_direct_core_disable`, `camera_mgr_direct_stop_no_fe` (capture without terminal at stop), ISP `ingress_lifecycle_tlm` | — |
| Context: drops, wrap, epoch reset, outputs held long, stalled SOF, no latest-CSR association, no stale completion | `camera_direct_dump_wrap`, `camera_direct_csi_reset` (epoch), `fx1_csi2_observer_wrap`, ISP `ingress_lifecycle_tlm`, ownership check on every `manager.c` frame | — |
| Traffic: direct-only zero CSI RAW write / IDMA read, dump only CSI write, real ODMA NV12, RAW sentinels, no CPU / SYS_DMA bridge | `check_direct_traffic.py` on every camera_direct / camera_mgr_direct run (issued / returned / outstanding, per-transaction windows), `camera_direct_tpg4k` (24.9 MB ODMA, 0 MIPI, 0 IDMA), checker negative controls (`fx1_unit_traffic_*`); **RAW sentinel over the whole FX1_FW_RAW_BUF region on every direct run, any writer (D4 review F1, P37)** | the counters see three masters; the sentinel is address based and does not name the writer |
| E2E: small sensor via PHY RAW10 / 12, tail / odd beat, TPG 3840x2160 host NV12, concurrent dump | `camera_mgr_direct_raw10/12`, `camera_direct_raw10/12`, `camera_direct_tpg4k`, `camera_direct_dump_*` | **sensor 64x32, not 64x16**: ISP v1.07 admits no frame below 32 active lines (MAS §6.6.3.4) |
| Lifecycle / CLI: memory / direct / source switch, ISP owner, start-fail cleanup, stop budget / ESENSOR, held outputs, missing sensor / PHY clock, restart | `camera_mgr_path_switch`, `freertos_cli_camera_direct(_tpg)`, `camera_mgr_direct_sensor_stop`, `_held_outputs`, `_sensor_lost`, `_start_fail`, `_stop_no_fe`, `_stop_pending` (stop EBUSY: nothing starts, publishes or arms) | PHY clock loss is not modelled separately (the sensor stopping is); the CLI's pending-stop refusal has no CLI fault fixture (the manager's refusal is tested) |
| Regression: CSI / PHY affected, ISP suites, camera FW / CLI, full CI incl. NPU-off | above | PHY 640x480 benchmark not run (outside the gate, as in D2) |

### D4.2 — required mutations, rerun on the final revision

Plan §8 mutations, the anchors of the original campaigns (D1a / D1b, D2a, D3)
unchanged, applied to the final tree one at a time (`mut_d4`, source restored
and byte-compared; ISP ones on a fresh ISP build with the four ingress / direct
TLM tests, a control run first: 4 / 4 PASS; fx1_soc ones on the camera build
with `--output-on-failure` and the UART kept). Verdict = the assertion that
fails, never a timeout or crash.

| Plan §8 mutation | Mutation | Killed by (assertion) |
|---|---|---|
| IDMA still reads | Q01 model: IDMA not gated by SOURCE direct | `ingress_tlm`: IDMA read count != 0 (6 checks) |
| | Q02 same, platform level | **survived**: not reachable from the platform (below) |
| | Q03 driver: direct start sets IDMA_EN | `camera_mgr_direct_raw12` 0x20: the model refuses SOURCE direct with IDMA_EN = 1, configure EDEV |
| ready always 1 / consume a stalled beat | Q04 ready always | `ingress_tlm`: `stalls > 0` (3) |
| | Q05 consume a stalled beat | `ingress_tlm`: `stalls > 0` (3) |
| double shift RAW10 | Q06 ingress shifts RAW10 again | `ingress_tlm` output compare, `direct_e2e_tlm`, `direct_raw10_tlm` (2788 pixel checks) |
| | Q07 manager: RAW10 depth not native | `camera_mgr_direct_raw10` 0x36: NV12 != golden |
| null abort becomes pixels | Q08 abort ignored | `ingress_tlm`: outcome cause ABORT (6) |
| SOF taken twice | Q09 SOF in frame kept | `ingress_tlm`: INVALID / MISSING_LINES outcome, cause counter, FRAMES_INVALID (8) |
| context from the latest CSR | Q10 bind any pending entry | `ingress_tlm`: CTX_SEQ != capture seq (4) |
| | Q11 observer: beat seq not carried | `fx1_csi2_observer_tpg` / `_wrap` ("every data beat carries a capture sequence"), `camera_mgr_direct_raw12` 0x30 |
| | Q12 adapter: FRAME_ID 0 at capture start | `camera_direct_dump_wrap` 0x120 (context FRAME_ID), `camera_mgr_direct_raw12` 0x38 |
| NV12 DONE before terminal success | Q13 READY before the CSI end | `ingress_tlm`, `ingress_lifecycle_tlm`: `ctx_ready() == 0` |
| | Q14 manager publishes whatever the outcome | `camera_mgr_direct_sensor_faults` 0x68: faulted frames published |
| | Q15 adapter: Dropped reported OK | `camera_direct_dump_faults` 0x162: published instead of CSI_DROPPED (not observable without the concurrent dump; the first D4 run listed only the sensor-fault tests: both passed, see `results_main.txt`) |
| slot / owner released before the drain | Q16 cancel READY without the drain | `ingress_lifecycle_tlm`: READY before the drain (1) |
| | Q17 no timed drain wake-up | `ingress_lifecycle_tlm`: outcomes ISP_FAILED / OK, seq, ctx_used after the recovery (13) |
| bridge writes RAW scratch (added by the D4 review, F1) | B1 CPU write, B2 SYS_DMA write, B3b running CSI RAW DMA bridge | RAW sentinel (all three), MIPI_DMA counter (B3b); see "D4 review round 1" below |

**16 / 17 killed, all by data / context / traffic / ownership assertions** (the bridge group, missing from this first list, was added in D4 review round 1: B1 / B2 / B3b killed).
Q02 survives by construction: the driver never sets IDMA_EN with SOURCE
direct, and the model refuses SOURCE direct while IDMA_EN = 1
(`ingress.cpp`), so the engine's own IDMA gate is a third layer that only
the ISP-level test can reach (Q01 kills it there).

Survivors kept from the review rounds (not plan §8 mutations, verification
gaps recorded): D3 r1 N07 / r2 P06 (no USED = 0 check after the SOURCE
switch: window not reproducible), r2 P03 (poll guard while a stop is pending:
nothing to move then).

### D4.3 — artifacts and runtime on the final revision

| Item | Evidence |
|---|---|
| final CI (JUnit, logs, camera UARTs, manifest 602) | `plan/evidence/direct_d3_r2_20261126/ci/` |
| small PHY RAW10 / 12, TPG 4K, concurrent dump, context / traffic | the CI UARTs and `*.traffic.txt` of `camera_direct_*` / `camera_mgr_direct_*`; D3 closure (reviewer's 10 / 10 selected rerun) |
| mutations Q01–Q17 (definitions, runner, results, UART / test logs) | `plan/evidence/direct_d4_20261126/mutations/` |
| CSI suite 158 (log, JUnit) | `plan/evidence/direct_d4_20261126/csi_suite/` |

Runtime of the final CI (host, `--jobs 8`): about 43 min wall; smoke 1208 s
wall (517 tests, 7140 s summed; slowest `freertos_cli_camera` 438 s),
regression 1250 s wall (80 tests, 9323 s summed; slowest
`camera_direct_dump_stall` 717 s, `camera_direct_held_outputs` 567 s,
`camera_mgr_direct_odma_err_held` 563 s), isp 45 s, components < 1 s.
CSI suite 151 s. No test timeout was raised to pass; the one D3 r2 test that
first exceeded its simulated budget (`camera_mgr_direct_stop_pending`) was
shortened (one frame after the restart), its budget kept.

Deviations kept: sensor 64x32 (ISP minimum 32 lines); PHY 640x480 benchmark not run; CSI PHY / sensor
suite outside `run_ci.sh` (above). Not D4: memory-path stop ETIMEOUT keeps
state 1 / 2 without a pending flag (M6 semantics, D3 closure note).

## D4 review round 1 (2026-11-26): RAW written by an unobserved master — fixed, awaiting re-review

Review `plan/DIRECT_D4_REVIEW_20261126.md`, F1 / P2: the traffic counters
(`fx1_soc_top.cpp`) observe MIPI_DMA, ISP_IDMA and ISP_ODMA only, so the
checker could not see a RAW write by the CPU, SYS_DMA or any other master;
plan §8 asks for RAW sentinels and a killed "bridge writes RAW scratch"
mutation. Reviewer's repro: one CPU write 0xDEADBEEF into 0x8108_0000 during
a direct run; 5 frames exact, traffic reports byte-identical, checker PASS.
A verification gap, no nominal bridge.

**Fix (P37, user's choices: host sentinel, the whole RAW region, every
direct-only test).** `camera_direct/raw_sentinel.py` makes a 112 MiB pattern
(SHA-256 counter mode, target `fx1_raw_sentinel`, part of `fx1_all`);
`fx1_camera_direct_test` adds `--load 0x81000000:<pattern>` (after the DDR3
preinit, before the CPUs) and `--dump 0x81000000:0x7000000:<name>.raw_sentinel.bin`,
and `check_direct_traffic.py --raw-base/--raw-size/--raw-pattern/--raw-dump`
requires every byte unchanged outside the MIPI_WINDOWS of a RAW dump (changes
inside them are MIPI_DMA's, checked by its counter). The checker reports the
changed ranges and removes the dump, so a stale one is never read (and 25 x
112 MiB do not stay on disk). `check_tpg4k.py --direct --raw-pattern` does the
same for `camera_direct_tpg4k`. Not applied: `camera_mgr_path_switch`, the
FreeRTOS CLI sessions (memory phases write RAW legitimately). Cost measured
on `hello_2hart`: +0.44 s, +245 MB per run.

**Checker tests** (smoke, manifest): `fx1_unit_raw_sentinel_selftest` (no
repeated block, make = pattern, the dump removed, a second check finds none),
`fx1_unit_raw_sentinel_fixtures`, `fx1_unit_traffic_checker_raw_{clean,
cpu_write, zeros, side_direct, side_dump, side_past_end, short, missing}`.

**Clean control** (all 25 direct tests, `-V`): PASS; 24 / 24 sentinel
verdicts "unchanged outside the windows" (the four RAW-dump tests: 5231 to
8160 bytes changed inside the two sides), then `camera_direct_tpg4k` (first
without the sentinel, found by this run; connected): unchanged.

**Mutations** (`scratchpad/mut_d4r1`, source restored and byte-compared):

| Mutation | Killed by |
|---|---|
| B1 CPU write 0xDEADBEEF into 0x8108_0000 in `poll_direct` (the reviewer's probe) | `camera_mgr_direct_raw12` and `_tpg640`: RAW sentinel, 4 bytes changed at 0x81080000-0x81080003 (firmware and traffic PASS) |
| B2 SYS_DMA copies 1 KiB of the image into 0x8120_0000 at the direct start | `camera_mgr_direct_raw12`: RAW sentinel, 1020 bytes changed in 0x81200000-0x812003fe (traffic PASS: SYS_DMA not counted) |
| B3 CSI RAW DMA programmed into the sides and started in direct | `camera_mgr_direct_raw12`: firmware 0x30 (both sides fill, the stream stops, no frame); not a traffic verdict, hence B3b |
| B3b the same with the sides handed back at every poll (a running RAW bridge) | `camera_mgr_direct_raw12`: firmware PASS (5 frames exact), host check FAIL: MIPI_DMA 160 writes issued and RAW sentinel 8160 bytes changed |
| S1 the checker drops the sentinel verdict | 6 of the 8 `fx1_unit_traffic_checker_raw_*` (the clean and inside-window cases pass by design) |

**CI** (`run_ci.sh --jobs 8`, fresh build `d4r1-ci`): PASS; smoke 527 (517 +
10 sentinel tests), components 6, regression 80, isp 41, npu_off ok;
manifest 612. Every one of the 25 direct test commands carries the sentinel
arguments and no dump was left (each checker ran); source manifest (89
files) verified unchanged after the run. Evidence
`plan/evidence/direct_d4_r1_20261126/`.

Limits: inside a RAW-dump side window a write by another master is not
distinguished from MIPI_DMA's (8 KiB of the 112 MiB, dump tests only);
writes outside the RAW region (e.g. a scratch elsewhere in DDR) are not
covered (the plan's sentinel is the declared RAW region); a write that
restores the pattern is not seen.

## D4 re-review R1 (2026-11-26): sentinel checker tests shared a dump — fixed, awaiting re-review

Re-review `plan/DIRECT_D4_REREVIEW_20261126.md`: F1 confirmed fixed; R1 / P2
in the test gate. `fx1_unit_traffic_checker_raw_side_direct` and `_side_dump`
both read `side_write.bin`; the checker removes the dump it read, so whichever
ran second lost its input (serial: `side_dump` "no dump"; `-j4`: the other
one). The earlier PASS was timing. Fix: `raw_sentinel.py fixtures` writes one
dump per test, named after it (and removes a stale `missing.bin`); the CMake
loop takes the dump name from the test name, so two tests cannot share one.
The pattern (read only) stays shared.

Stability (sentinel group, 10 tests): 10 x `-j1`, 10 x `-j8`, 10 x `-j1
--schedule-random`: all 10 / 10; the reviewer's serial repro 3 / 3. S1
(checker drops the verdict) rerun: 6 / 8 killed, every kill "expected exit
status 1, got 0" (none from a missing input).

**CI** (`run_ci.sh --jobs 8`, fresh build `d4r2-ci`): PASS; smoke 527,
components 6, regression 80, isp 41, npu_off ok; manifest 612; the 25 direct
test commands carry the sentinel arguments, no dump left; source manifest (89
files) verified unchanged after the run. Evidence
`plan/evidence/direct_d4_r2_20261126/`.

## D5 — package (2026-11-25, unattended session: preview only, no export)

- `package_sdk.sh --build-dir final-ci --out <scratchpad>/pkg-preview
  --allow-dev-build` (a preview from a dirty tree, never a release) and
  `check_package.sh` on it: **PASS** (SHA256SUMS 234 files, manifest
  revision 36ad26a4b200 dirty; the known note: D-PHY RX model provenance
  pending, THIRD_PARTY.md). The package ships the ISP_INGRESS driver and
  headers, the camera manager with the direct path, `cli_camera_direct.txt`
  and `run_vp.sh camera-direct-cli`.
- Relocated copy, empty environment (`env -i`): `run_vp.sh camera-direct-cli`
  **PASS**: direct RAW12 4 frames and RAW10 2 frames, then memory 2 frames,
  all = golden, `poweroff: 0 failed`.
- **Not done (needs the user):** a release package needs a committed, clean
  CDC-VP tree (the scripts refuse a dirty one); `export_to_sdk.sh --replace`
  into the SDK repository (replaces VP_FX1_V3.0: hard to reverse); the
  tuning-compatibility gate against the V3.0 package the tuning team uses
  (their RAW / profiles / config are not available here). Note for that
  gate: `fx1_isp_dev` gained a field (`direct`, at the end): applications
  must be rebuilt with the new header.
