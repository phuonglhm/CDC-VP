# FX1 ISP VP — Decisions and discrepancy log

Status values: **DECIDED** (user decision), **RESOLVED** (follows from DEC-09
or from unambiguous spec text), **ASSUMPTION** (model choice where the spec is
silent; observable, documented, may change), **GAP** (requirement that cannot
be implemented or tested with the current map), **OPEN** (needs a decision
before the named milestone).

CSR row numbers refer to worksheet `Registers` of the pinned XLSX
(see [ISP_SOURCE_MANIFEST.md](ISP_SOURCE_MANIFEST.md)).
Rows cited in §1–§8 are rows of the previous revision (`0980b61d…`); the CSR and
MAS revision of 2026-10-07 renumbers them. Entries marked **→ §9** are
superseded or changed by that revision; §9 gives the new contract and the
milestone of plan `ISP_CSR_MAS_UPDATE_PLAN_2026-10-09` that implements it.

## 1. User decisions

DEC-01..DEC-13 are recorded in the project plan (`plan/ISP_VP_PLAN.md` §5.1);
summary:

| ID | Decision |
|---|---|
| DEC-01 | Image algorithms follow the HAS; bit-exact only where the spec is complete; everything else is a documented assumption. |
| DEC-02 | SW rollout bare-metal/RTOS first, Linux V4L2 later. |
| DEC-03 | Acceptance requires RAW → pipeline → processed NV12 end to end. |
| DEC-04 | Real input: `Image Source`, BGGR, RAW10, 2688×1520. |
| DEC-05 | No owner golden/tuning available; the project builds its own reference. |
| DEC-06 | Event-based SystemC/TLM, configurable latency, not cycle accurate. |
| DEC-07 | New implementation in `components/FX1_Components/isp`; old `components/isp_tlm` is not a baseline. |
| DEC-08 | Standalone IP first; platform integration later. |
| DEC-09 | CSR wins over HAS where they differ. |
| DEC-10 | PNGs are real-camera ISP output; reference only, not golden. |
| DEC-11 | Bayer encoding 0=RGGB, 1=GRBG, 2=GBRG, 3=BGGR. **→ §9** |
| DEC-12 | Memory-to-memory topology only in this phase. |
| DEC-13 | Same-frame pairing / camera ISP identity of the PNGs unknown; not a blocker. |

Decisions taken during M0 (2026-09-30):

| ID | Topic | Decision |
|---|---|---|
| DEC-14 | Soft reset (CSR-01, CSR-02) | `COMMON_CTRL.soft_rst` and `DMA_CTRL.soft_reset` are identical: 32 core-clock periods, pixel pipeline + both DMA engines. **Preserved:** all RW configuration (including `IDMA_EN`/`ODMA_EN`), LUT/mesh contents, LSC profile validity, frame counters (`IDMA_FRAME_COUNT`, `ODMA_FRAME_COUNT`, `*_FRAME_ID`). **Cleared:** buffer ownership bits (`IDMA_BUF_VALID`, `ODMA_BUF_FREE`, `ODMA_BUF_DONE`), frame in flight, statistics results and done flags, `COMMON_IRQ_STATUS`, `DMA_IRQ_STAT`, `DMA_ERR`. **→ §9** |
| DEC-15 | Error aggregation (CSR-03). **Superseded by V107-D84 (MAS v1.07 Table 9-188).** | `COMMON_STATUS.error` = OR of the current error flags `LSC_ERROR[5:0]`, `BPC_STATUS.cand_reject_ovf`, `EE_STATUS.error`, `DMA_ERR[4:0]` **including `IDMA_UNDERRUN`**. `COMMON_IRQ_STATUS.error_irq` is set whenever one of those flags goes from 0 to 1. |
| DEC-16 | `COMMON_CTRL.frame_start` (CSR-04) | Pending flag: SW writes 1, it reads 1 until hardware clears it at the next accepted SOF. It neither creates nor blocks frames; frames start from `IDMA_EN` + valid head buffer. **Superseded by V107-D7 (§10.2): self-clearing no-op.** |
| DEC-17 | `COMMON_STATUS.frame_done` (CSR-05) | Set on the same event as `frame_done_irq` (ODMA has the last write response of both planes); cleared at the next accepted SOF, by `i_rst_n` and by soft reset. |
| DEC-18 | ODMA overflow (CSR-19), decided 2026-09-30 | Lossless stall. When an output frame is waiting and the head output buffer is not free (ODMA enabled), set `DMA_ERR.ODMA_OVERFLOW` and `IRQ_ODMA_OVERFLOW` once per stall episode; the pipeline and IDMA are back-pressured and the frame completes intact once a buffer is freed. **→ §9** |
| DEC-19 | AXI error response (CSR-20). **Superseded by V107-D62..D68 (MAS v1.07 §9.9.8).** | Terminal for the frame, discarded end to end. IDMA error: `DMA_ERR.IDMA_AXI` + `IRQ_AXI_ERROR`, the buffer's `IDMA_BUF_VALID` bit is cleared (released), the rotation position is kept, so software re-arms the same index (underrun is reported meanwhile); no IDMA_DONE, no ODMA_DONE. ODMA error: `DMA_ERR.ODMA_AXI` + `IRQ_AXI_ERROR`, the buffer is not marked DONE, its `ODMA_BUF_FREE` bit is cleared, the rotation position is kept; the IDMA side is unaffected. |
| DEC-20 | ISP_EN ordering trap (HAS §9.1) | While `COMMON_CTRL.isp_en` = 0 the output geometry is 0×0, so arming the ODMA sets `DMA_ERR.ALIGN_OR_GEOMETRY` and the unit does not start. |
| DEC-21 | M2 datapath | Test-only stub with the real Input Formatter crop and output geometry: Y = cropped sample >> 4 (nearest-neighbour scaling), UV = 128. Replaced by the real pipeline in M3. |
| DEC-22 | Algorithm review policy (M3), decided 2026-09-30 | Where a HAS figure or block diagram contradicts the HAS numeric contract or equations, the equations win. Formulas are implemented literally even where the HAS makes a claim they refute (e.g. 2DNR Haar round trip, Resizer axis order). All other ALG gaps follow the review proposals in `plan/alg/ALG_*.md` as ASSUMPTIONs, and open points go to the owner query list. |
| DEC-23 | Resizer decimation stage count (ALG-RSZ-01) | Choose k by the HAS rule; if the decimated size is then smaller than the output on either axis, lower k until both axes are at least the output size (bilinear ratio ≤ 2). Identical to the HAS in every case where the HAS rule is sound. |
| DEC-24 | `CCM_CTRL.updated`, `RESIZER_CTRL.updated` (ALG-CCM-01, ALG-RSZ-05) | Commit gates: the CCM coefficient/offset set and the Resizer scale are copied to the active set only while `updated` = 1; hardware never clears the bit. Enables are sampled every SOF. **→ §9** |
| DEC-25 | EE parameter timing (ALG-EE-01/02) | Live, as the CSR states: EE scalars and LUTs take effect as written. Bit-exact comparison only for frames with no EE write in flight. **→ §9** |
| DEC-26 | Commit time for DEC-24/27 | With `updated` = 1 the commit happens immediately if the pipeline is idle (`COMMON_STATUS.busy` = 0), otherwise at the next accepted SOF. `RESIZER_OUT_W/H` derive from the committed scale. **→ §9** |
| DEC-27 | `CNF_CTRL.updated` | Commit gate as DEC-24 (CSR wins over HAS §6.19 "not implemented"). **→ §9** |
| DEC-28 | BPC dynamic detection (ALG-BPC-01/02) | Not modelled: detection is always static (bit-exact); with `dynamic_det_en` = 1 the model warns once and the candidate/defective counters read 0. GAP for the owner. **→ §9** |
| DEC-29 | Statistics buffering (ALG-STAT-02/03) | As the HAS text: one result memory per block; zone and histogram contents of frame N are readable until the SOF of frame N+1 overwrites them. FRAME_ID is assigned when a frame enters the pipeline and travels with it (ALG-STAT-01 proposal). Refined by DEC-30. |

Decisions from the M3 review (2026-10-01):

| ID | Topic | Decision |
|---|---|---|
| DEC-30 | Statistics memory during a frame (review M3-R5) | The AEC zone + histogram memory and the AWB zone memory are updated in place, row by row, as the frame streams. From the SOF of an enabled frame, a zone already written in that frame reads its partial sums; a zone not yet written reads the empty value (0; min 4095, max 0); the histogram reads the partial counts. Global results (AEC/AWB), AF scores, FRAME_ID and RESULT_CONTEXT_ID keep the previous publication until EOF. This replaces the ALG-STAT-02 (a) proposal, under which the view read empty until EOF and hid the race a driver must see. **→ §9** |
| DEC-31 | LSC mesh ceiling (ALG-LSC-11, owner query B17) | 2..32 nodes per axis, from top-level `PARA_LSC_MESH_MAX` (HAS Table 5-1), until the owner confirms. **→ §9** |
| DEC-32 | Pending `AEC_CTRL.commit` at soft reset (B18) | Dropped, per the DEC-14 W1S rule; the AEC active configuration is kept (was M3-A3). **→ §9** |
| DEC-33 | Zones not written in frame N (ALG-STAT-03, B20) | Read empty, even where a 1-bit RTL tag could match frame N−2. ASSUMPTION pending RTL/owner. This does not change DEC-30: zones written in the current frame read their partial sums. |

M4 scope decisions (2026-10-01), detail in [ISP_M4_QUALIFICATION.md](ISP_M4_QUALIFICATION.md) §1:

| ID | Topic | Decision |
|---|---|---|
| DEC-34 | Real-RAW profiles | Four test presets (`basic`, `full`, `full_vga_709`, `gtm_manual_qhd`) from `tools/make_profile.py`; formulas, not calibrations. `full` enables every pipeline block, DG included. |
| DEC-35 | Real-RAW regression | `tools/run_regression.py` with the plan §10 artefacts outside the tree; optional CTest with `-DFX1_ISP_RAW_INVENTORY`. |
| DEC-36 | G-CONT | (a) The 28 real frames back to back in one VP session, for each preset. (b) 100+ synthetic frames from a fixed seed, compared with the reference, with soft resets, AXI errors, underrun and overflow injected. |
| DEC-37 | 4K | Deterministic pattern in C++ and Python; only the SHA-256 and the statistics readout are committed. |

M5 decisions (2026-10-01):

| ID | Topic | Decision |
|---|---|---|
| DEC-38 | Per-block `STATUS.busy` of Demosaic, CCM, Gamma, GTM, 2DNR, EE, CNF and Resizer (CSR: "the block currently holds pixels", used to tell an idle pipeline from a stalled one) | Frame granularity: set at the accepted SOF; cleared when the pipeline has emitted the frame's last row, when the frame is abandoned, and by either reset. The bits stay 1 during a stall (`isp_en` cleared mid-frame, ODMA overflow). A bypassed block still carries the stream. ASSUMPTION: no per-block row-level timing. |
| DEC-39 | OFMT stride padding (`OFMT_CTRL.stride_en`, owner query B14, ALG-OFMT-01) | Not modelled. With `stride_en` = 1 the output is unchanged (in the memory-to-memory topology the ODMA applies the strides), and the pipeline warns once per frame. |

## 2. Specification discrepancies from the plan (SPEC-xx)

| ID | Summary | Status |
|---|---|---|
| SPEC-01 | Bayer enum listed twice in CSR row 29 | DECIDED (DEC-11); generator override asserts the stale list |
| SPEC-02 | DMA soft reset scope/duration: HAS 15 cycles DMA only, CSR 32 cycles pipeline+DMA | RESOLVED by DEC-09, extended to COMMON soft reset by DEC-14 |
| SPEC-03 | CSC: HAS build-time constants vs `CSC_CTRL.std` | RESOLVED: implement `std`, 0=BT.601 (reset), 1=BT.709, sampled at SOF |
| SPEC-04 | HAS §6.25.10.3 buffer tags/timestamps, `FRAME_COUNTER` have no CSR | **GAP**: not implemented, no offsets invented; internal trace only |
| SPEC-05 | HAS `IRQ_CFG_ERROR` not in CSR | RESOLVED: `DMA_IRQ_*` bit 0 tied to zero; ALIGN reported via DEC-15 **→ §9** |
| SPEC-06 | HAS `DMA_WIDTH/HEIGHT` (3840×2160) vs COMMON geometry (1920×1080) | RESOLVED: COMMON geometry only |
| SPEC-07 | `ODMA_BUF_FREE` cleared at buffer start (HAS) vs when full (CSR) | RESOLVED: SW-visible FREE clears when full; internal in-use state prevents reuse (M2) **→ §9** |
| SPEC-08 | 2–8 buffers (HAS) vs 4 (CSR) | RESOLVED: 4 buffers, bits [7:4] unused |
| SPEC-09 | RAW bit depth | RESOLVED: no depth register; fixture shifts source `>> 4` (plan §4) |
| SPEC-10 | `IDMA_STRIDE` reset 7680 with a "1920 pixels" comment | RESOLVED: numeric 0x1E00 kept (it matches HAS 3840×2 bytes) |

## 3. New findings from M0 (CSR-xx)

| ID | Evidence | Status / resolution | Affects |
|---|---|---|---|
| CSR-01 | Row 10: COMMON soft reset "behaves like DMA_CTRL.soft_reset"; duration only given for DMA (row 451, 32 cycles). HAS Table 9-1 differs per source. | DECIDED (DEC-14) | M1, M2 |
| CSR-02 | HAS Table 9-1 does not list counters or `DMA_ERR` | DECIDED (DEC-14) | M1, M2 |
| CSR-03 | Rows 15/18: error sources are "any block" | DECIDED (DEC-15) | M1 |
| CSR-04 | Row 11: "software-triggered mode" has no mode register; HAS §9.1 IDMA self-starts | DECIDED (DEC-16) | M1, M2 |
| CSR-05 | Row 14: `frame_done` clear rule not given | DECIDED (DEC-17) | M1, M2 |
| CSR-06 | Row 457: `IDMA_UNDERRUN` is "level; set while it persists" in a W1C field | RESOLVED by HAS §6.25.8.3: level-held, a W1C write does not clear it while the condition persists. ASSUMPTION: `DMA_IRQ_STAT.IDMA_UNDERRUN` and `error_irq` are set on the rising edge of the condition only (no re-trigger while it persists). | M1 mechanism, M2 source |
| CSR-07 | Rows 403–404: `AF_CTRL.search_start/abort` are W1S with no hardware | ASSUMPTION: behave as a W1S flop that nothing clears — reads 1 after a write until `i_rst_n`. Soft reset clears it (it is command state, DEC-14). | M1 |
| CSR-08 | Rows 163, 275, 286: `CCM_CTRL.updated`, `CNF_CTRL.updated`, `RESIZER_CTRL.updated` are RW; HAS §6.19 says CNF's is not implemented; who clears CCM/Resizer's is not stated | M1: plain RW storage. Datapath meaning decided at M3 (proposal: no hardware clear; CCM/Resizer take config at SOF as every block does). | M3 |
| CSR-09 | Rows 288–291: `RESIZER_OUT_W/H` derived by hardware; latch point not stated; HAS says output geometry is "only driven while ISP_EN is set" but the reset values (1920×1080) are readable with ISP_EN=0 | ASSUMPTION: register readback is derived live from `COMMON_FRAME_*`, `COMMON_BAYER` and `RESIZER_CTRL.{en,scale}`, not gated by ISP_EN. The ISP_EN gating applies to what the ODMA captures (M2). | M1, M2 |
| CSR-10 | Row 25 says width "must be even after any phase crop"; HAS Table 6-17 rounds down to even | RESOLVED: HAS rule `W_out = floor((W − S_H)/2)·2`, same for height | M1 (OUT_W), M3 |
| CSR-11 | HAS §6.25.8.2 sets `DMA_STAT.IDMA_FRAME_DONE`; CSR `DMA_STAT` has only busy bits | RESOLVED (DEC-09): no such bit | M2 |
| CSR-12 | HAS names `DMA_ERR.ALIGN/AXI_RD/AXI_WR`; CSR names `ALIGN_OR_GEOMETRY/IDMA_AXI/ODMA_AXI` | RESOLVED: CSR names | — |
| CSR-13 | HAS `MAX_BURST_BEATS` 1..256 vs CSR `max_burst_m1` | RESOLVED: beats = `max_burst_m1 + 1` | M2 |
| CSR-14 | HAS §6.20.2 tells SW to program an "Input Formatter crop window"; no such CSR | **GAP**: only the Bayer phase crop exists | M3 |
| CSR-15 | HAS Table 6-4 lists `AF_CTRL.BYPASS` as a control; CSR row 401 says no hardware | RESOLVED (DEC-09): no hardware | M3 |
| CSR-16 | HAS §6.5.2: LUT memories undefined after reset; CSR: Gamma LUT "powers up as zeros" (row 191), EE tables "reset to" 0x8000 (row 267) | ASSUMPTION: on `i_rst_n` the model fills Gamma = 0, EE = 0x8000, GTM = 0 (GTM unstated). Deterministic, matches CSR text; SW must still load tables before enabling (HAS §9.1). Soft reset preserves all tables. | M1 **→ §9** |
| CSR-17 | HAS §6.21.6.2: AEC shadow set includes "image height"; no AEC height register | RESOLVED in M3 (ALG-AEC-05, ALG-STAT-10): the frame's Input Formatter output geometry, taken at every accepted SOF | M3 |
| CSR-18 | Rows 94–98, 208: LSC load sequencer and GTM LUT write port depend on block state | Implemented in M3 (see [ISP_CSR_CONTRACT.md](ISP_CSR_CONTRACT.md) §6) | M3 |
| CSR-19 | HAS §6.25.9.3 ODMA overflow back-pressures losslessly vs Table 6-100 "frame is corrupt"; CSR row 457 "a frame completed with no free output buffer" | DECIDED (DEC-18) | M2 |
| CSR-20 | HAS §7.1.8 "returns to idle, releasing the buffer" vs §6.25.11 "without advancing the rotation … software must re-arm" | DECIDED (DEC-19) | M2 |
| CSR-21 | Rows 193–196, 264–267: LUT address auto-increment; wrap and partial-strobe behaviour not stated | ASSUMPTION: an accepted write to the data port (any strobe set) writes the merged data register value to the entry at the current address, then the address increments modulo its field width (Gamma 256, EE 64; EE contrast tables use `addr & 31`). A write with no strobes changes nothing and does not increment. | M1 |
| CSR-22 | Row 453: `DMA_STAT` 0 on a wedged engine | Informational; model reports busy only while a transfer is outstanding | M2 |
| CSR-23 | HAS Table 7-10 "address bits 1:0 ignored" vs sub-word TLM accesses | ASSUMPTION (adapter contract): a 4-byte TLM access addresses the word `addr & ~3` with all lanes (bits [1:0] ignored); a 1–3 byte access uses byte lanes `addr[1:0]..` of that word and must not cross it (else `TLM_BURST_ERROR_RESPONSE`, since it is not one AXI4-Lite beat). See [ISP_CSR_CONTRACT.md](ISP_CSR_CONTRACT.md). | M1 |

## 4. M2 model assumptions (DMA and frame lifecycle)

| ID | Assumption | Basis |
|---|---|---|
| M2-A1 | The IDMA and ODMA rotation positions return to buffer 0 on `i_rst_n` and on soft reset (sequencing state). | HAS Table 6-12 resets every state machine and counter; the HAS is silent on soft reset |
| M2-A2 | Clearing `IDMA_EN` / `ODMA_EN` takes effect at a frame boundary: the frame in progress completes, no new frame starts. The IDMA checks its enable at every arm; the ODMA checks its enable at arm **and again before accepting a frame's SOF**, so an ODMA that is armed and then disabled keeps its captured buffer and waits (review M2-R1). | HAS §9.4.2 "stop the input DMA at a frame boundary" |
| M2-A3 | Address bits above `PARA_AXI_ADDR_WIDTH` (40) are dropped; alignment is checked on the 40-bit address. | The ports have 40 address wires (HAS Table 5-1) |
| M2-A4 | If the frame reaching the ODMA does not have the geometry captured at arm, `ALIGN_OR_GEOMETRY` is set, the frame is discarded and the ODMA re-arms. | HAS §9.4.2: geometry changes only while idle; behaviour otherwise unspecified |
| M2-A5 | Clearing `ISP_EN` mid-frame stalls the pipeline at the next line; setting it again resumes. Soft reset is the recovery path. | CSR row 9 "stops all processing" |
| M2-A6 | With `ODMA_EN` = 0 an output frame waits without raising `ODMA_OVERFLOW`. | Overflow is about buffer availability, not a disabled engine |
| M2-A7 | `DMA_STAT.idma_busy` is 1 from arm to retire; `odma_busy` from the first output line to completion or abort. | CSR row 454–455 "transfer outstanding" |
| M2-A8 | After an `ALIGN_OR_GEOMETRY` refusal the unit re-checks on every register write; the sticky bit is set again only after software cleared it. | HAS Table 6-100 "the unit does not start" |
| M2-A9 | (Superseded by V107-D64.) On an IDMA error after line 0, lines already forwarded may have been written into the armed output buffer; that buffer is not DONE and stays FREE, and the next frame overwrites it. | DEC-19; no partial-frame recovery (HAS §7.1.8) |
| M2-A10 | Transactions are issued one burst at a time (no outstanding-transaction overlap); the two ODMA channels are serialised per line (Y line, then UV line) instead of round-robin per burst. Memory latency is the target's annotated delay. | DEC-06 (LT model); observable ordering within a line is not specified |
| M2-A11 | Pipeline pacing: one input line takes `(W + pipeline_line_overhead_cycles)` core cycles. The IDMA retires a frame when the pipeline has accepted its last line. | HAS §6.4 (1 ppc), §6.25.8.2 step 5 |
| M2-A12 | HAS Table 6-100 "the frame is corrupt" for output overflow is superseded by DEC-18. | DEC-18 |

Review findings fixed in M2:

| ID | Finding | Fix | Regression |
|---|---|---|---|
| M2-R1 | An ODMA armed and then disabled still wrote the next frame | `ODMA_EN` re-checked before accepting SOF | `test_odma_disabled_after_arm` |
| M2-R2 | A reset did not wake an engine sleeping on a timed wait, so a re-armed frame waited out the old line latency | Every engine wait is also woken by the reset event, and the epoch is checked before and after it | `test_reset_wakes_line_wait` (second DUT, 2 ms per line) |

## 5. M3 algorithm review (ALG-xx)

The per-block arithmetic review (P02) was done before each M3 batch. The
review sheets are kept with the plan, outside this repository, because they
quote HAS equations: `plan/alg/ALG_A_ifmt_blc_lsc_bpc.md`,
`ALG_B_wb_dg_demosaic_ccm.md`, `ALG_C_gamma_csc_gtm.md`, `ALG_D_2dnr_ee.md`,
`ALG_E_cnf_resizer_ofmt.md` and `ALG_F_aec_awb_af.md`. Each sheet lists the
integer algorithm, the CSR fields used and when they are sampled, the border
policy, and the gaps as `ALG-<block>-nn`. Under DEC-22 every PROPOSAL in those
sheets is implemented as an ASSUMPTION unless a DEC entry above overrides it.
The code cites the ALG identifier at each such choice. Open points for the IP
owner are in `plan/alg/OWNER_QUERIES.md`.

Model-level choices made in M3 that are not on the review sheets:

| ID | Assumption | Basis |
|---|---|---|
| M3-A1 | The model is row streaming: each window block emits row y once row y+r has arrived, as a line buffer would, and the Python reference is frame based. The two are written independently, and agreement is checked bit for bit. | DEC-05, DEC-06 |
| M3-A2 | `EE_STATUS.error` (radial gains out of range) is a level condition. It is re-evaluated on every EE write and right after a soft reset, so it reasserts at once if the condition still holds. | ALG-EE, DEC-14 |
| M3-A3 | Superseded by DEC-32 (same behaviour, now decided). | DEC-32 |
| M3-A4 | `stats_ready_irq` is set on every publication, even when the block's done bit is still set from an unread earlier frame. It is an event, not the rising edge of the done bit. | HAS Table 9-2, §6.21.6.5, ALG-STAT-11 |
| M3-A5 | The 2DNR noise-variance shadow is a measured value, cleared by soft reset like the other results. | ALG-2DNR-09, DEC-14 |
| M3-A6 | Superseded by DEC-31 (2..32 nodes per axis; the LSC block parameters give 128). | DEC-31 |
| M3-A7 | An aborted frame (IDMA AXI error, an SOF in the middle of a frame, unsupported geometry) publishes no statistics and clears the AEC/AF busy bits. It still consumed a FRAME_ID. Its rows remain in the zone/histogram memory (DEC-30): software sees partial data with the previous FRAME_ID and must rely on the done bit. | ALG-STAT-09 |
| M3-A8 | Before the first enabled SOF after `i_rst_n` or a soft reset, every zone and histogram field reads the CSR reset value 0, including `green_min`. From that SOF on, DEC-30 applies. The post-reset tag sweep takes no time (ALG-STAT-08). This refines ALG-AEC-04 (a), which said "until the first publication". | ALG-AEC-04, ALG-STAT-08 |
| M3-A9 | The demo tool `fx1_isp_run_raw` queues one buffer and leaves `IDMA_EN` set, so `DMA_ERR.IDMA_UNDERRUN` (0x2) is expected at the end of the run. | HAS §6.25.8.3, CSR-06 |

Review findings fixed in M3:

| ID | Finding | Fix | Regression |
|---|---|---|---|
| M3-R1 | `stats_ready_irq` was derived from the 0→1 edge of each done bit, so a new publication raised no interrupt while software had not cleared the previous done bit | Explicit set per publication (M3-A4) | `test_stats` `test_frame_id_abort_irq` |
| M3-R2 | Mutation testing: the pipeline vectors did not catch the EE gain order, CNF inclusive thresholds or the 2DNR lower median | Block-level vectors (`tests/data/blocks`) | `fx1_isp.unit.block_vectors` |
| M3-R3 | Mutation testing: no statistics vector had WB or GTM enabled or a grid smaller than the frame, so the tap points and the last-zone clamp were untested | Vectors strengthened | `stats_scene_bggr_2frames_134x98`, `stats_flat_channels_64x48` |
| M3-R4 | Review: `snapshot_config()` still listed AEC/AWB/AF as "not implemented", so every enabled frame logged a false "passed through" warning | Mechanism removed | Pipeline vectors (warn sink) and E2E (`SC_REPORT_WARNING` count): only the DEC-28 warning, once per frame of `bpc_dynamic_requested_64x32` |
| M3-R5 | Review: zones and histogram read empty from SOF to EOF, so a read 5 lines into a frame got 0 instead of the partial sums | DEC-30: the pipeline writes the control unit's single memory row by row | `test_stats` `test_single_memory_partial_reads` (every row of a frame), E2E `probe_partial_stats` (over the CSR socket, mid-frame) |
| M3-R6 | Mutation harness: restoring a file with `rsync -a` kept its old mtime, so ninja did not rebuild it and one mutant could leak into the next run. A test value also hid a missing UE mask | Harness copies by checksum with a fresh mtime; batch re-run in full; test value changed | Mutation log 2026-10-01: 38 of 41 killed, 3 equivalent |

## 6. M4 model and test-bench choices

| ID | Choice | Basis |
|---|---|---|
| M4-A1 | `fx1_isp_tlm::debug_set_frame_counter()` (testbench backdoor, like the other `debug_*` methods) sets the statistics frame counter, so the FRAME_ID wrap at 2^32 can be tested without 2^32 frames. | Plan G-CONT "counter wrap checked with a suitable fixture" |
| M4-A2 | `fx1_isp_run_raw` is a driver-like harness. One profile is applied once. Frames come from one input, an input list or the synthetic pattern, with up to 4 in flight through the 4-buffer rotation. The first buffers are queued before the DMA is enabled. Each output is read out after its DONE and recycled, and the interrupt status is read and acknowledged per frame. `--per-frame` writes every frame's NV12; `--check-stats` and `--expect-sha256` make it a self-checking test. | DEC-35, DEC-36, DEC-37 |
| M4-A3 | In G-CONT, configuration changes only between bursts, with the pipeline idle (HAS §9.4.4). Inside a burst, frames queue back to back. Events are injected in the first frame of a burst, so the frames they affect are deterministic. | DEC-36 |

Review findings fixed in M4 (detail in [ISP_M4_QUALIFICATION.md](ISP_M4_QUALIFICATION.md) §8):
- M4-R1: no real-frame sequence in one VP session.
- M4-R2: only the last frame of two-frame runs was compared.
- M4-R3: `full` did not enable DG.
- M4-R4: the synthetic sequence lost the BGGR geometry when regenerated.
- M4-R5: start-up underrun in the driver harness.

## 7. M5 findings and choices

| ID | Item | Resolution |
|---|---|---|
| M5-A1 | `D_WDR_*` and `TNR_3D_*` registers: HAS §3.1 lists WDR and 3DNR as unsupported | Storage only; enabling either warns once per frame (like DEC-28 and DEC-39) |
| M5-R1 | The per-register traceability found that the 8 block `busy` bits were never set (CSR-defined status) | DEC-38; tests in E2E (mid-frame), G-CONT (set during an overflow stall, clear when idle) and `test_stats` |
| M5-R2 | The same review found that OFMT `stride_en` was ignored and nowhere documented | DEC-39 |
| M5-R3 | BLC profiles 2 and 3 and the profile-0 per-channel trims had never been written by a test; the model selects profiles by offset arithmetic | 3 vectors with decoy values in the other profiles (`blc_profile*`), bit-exact |
| M5-R4 | Mutation testing of the reference driver: first version of its test missed wrong-buffer recovery, a missing DONE acknowledge, the queue limit and the start-up order | Distinct inputs per frame, a late interrupt handler, 4 frames in flight, explicit checks; 10 of 15 mutants killed, 5 equivalent (programming guide §9) |
| M5-R5 | Review: after an ODMA AXI error, the driver freed `out_done`, the next buffer to collect. With a late interrupt handler that buffer is a completed frame not yet collected, so the failed buffer was never freed (stream stuck) and the completed one could be overwritten | The failed buffer is the first in-flight one, in rotation order, whose DONE bit is clear. Test: ODMA error with the handler run only after the previous frame is DONE, then 4 frames in flight; checks the order, `in_flight` and the rotation. The old code deadlocks this test |
| M5-R6 | Found by the new install test: the installed `fx1_isp_run_raw` had no RPATH to `libsystemc` (only the build tree had one) | `INSTALL_RPATH` from `SystemC::systemc`; the package test runs the installed tool |
| M5-R7 | Review: `run_regression.py --only <frame> --baseline` failed on the subset's own "sequence of N" runs, which the 28-frame baseline cannot contain | A subset compares the runs the baseline has and reports the others as not compared; a full run still requires every run to match |
| M5-R8 | Review: the install prefix lacked the traceability report, the evidence, the scripts and sample configurations listed for the handover (plan P14) | Installed under `share/doc/fx1_isp` and `share/fx1_isp` (tools, reference, schema, samples, 4K check). README "Handover package" lists what needs the source checkout |

## 8. Follow-up of the logic audit of 2026-10-02

The audit (report `VP_BK/ISP/report/ISP_VP_LOGIC_AUDIT_2026-10-02.md`) fixed A1–A4 and listed five further
improvements. Items 1–3 are done below. Item 4 (CPU integration ABI) waits for the CPU choice (DEC-08).
Item 5 (image quality) waits for owner tuning and paired data (DEC-05).

| ID | Item | Resolution |
|---|---|---|
| M5-R9 | Coordinated fault campaign (item 1) found a driver defect. When the ODMA failed on a frame while the IDMA was still reading its input, the driver declared the frame lost at once. If the IDMA then also failed on that frame, the hardware waited for a re-arm of that input. The driver re-armed another input (IDMA stuck) or the frame completed after the caller resubmitted it (duplicate). | A failed-output frame stays in flight, marked, until the IDMA has retired its input without an error (then lost, reported by `fx1_isp_next_lost`) or the IDMA error names it (then retried). IDMA_DONE is enabled to trigger the decision (programming guide §8.1). |
| M5-A2 | `fx1_isp_recover` acts only on error bits still set in `DMA_ERR`. A repeated call with the same snapshot is a no-op. | Item 3; campaign checks it after every recovery. |
| M5-A3 | Driver state machine UNINIT/CONFIG/STREAMING with `FX1_ISP_ESTATE`; `fx1_isp_stop` cancels a stream; `fx1_isp_get_progress` supports a caller watchdog. The timeout budget stays the caller's choice (no silicon figure). | Item 3; guide §8.2. |
| M5-A4 | Zone/histogram data after a software-visible abort (IDMA error), a soft reset or `stop` is refused (`FX1_ISP_ENODATA`) until `AEC_FRAME_ID` moves past the FRAME_ID recorded at the latest such event. It is not refused with FRAME_ID 0. Internal aborts of the model (SOF mid-frame, block geometry rejection) are not visible to software and not covered. No CSR bit was added. | Item 2; guide §5. |
| M5-A5 | Reset while the engine thread is suspended inside the target's `b_transport`: the transfer returns into an old epoch and is dropped; no access starts after the reset, no completion, retry correct. | Item 1; `test_reset_inside_blocking_target` (IDMA/ODMA × soft/external), test memory `blocking` mode. Mutation: dropping the epoch check after `b_transport` fails 10 checks. |

## 9. CSR/MAS revision of 2026-10-07 (U0, plan ISP_CSR_MAS_UPDATE_PLAN_2026-10-09)

The CSR revision `bda077b6…` (254 registers, 336 fields) and the MAS of the
same date replace the pinned map `0980b61d…`. Under the user decision of
2026-10-09, an entry is superseded where the new CSR or MAS is explicit; where
it is silent or contradictory, the entry stays and the point is listed in
§9.3 for the owner. MAS pages are PDF pages of the 336-page MAS. The ABI
delta and the generator changes are in [ISP_CSR_REVISION_0710.md](ISP_CSR_REVISION_0710.md).

### 9.1 Superseded or changed entries

| Entry | Previous VP contract | 07/10 contract (source) | Status | Milestone |
|---|---|---|---|---|
| DEC-11 | Bayer encoding adopted over the CSR enum list | The CSR enum list now has the same encoding | RESOLVED in the CSR; the generator asserts it | U0 (done) |
| SPEC-05 | `DMA_IRQ_*` bit 0 tied to zero | Bit 0 is `CFG_ERROR`: any illegal start configuration sets it with `DMA_ERR.ALIGN`, and the unit does not start (MAS Table 161–162, p294–295) | SUPERSEDED | U0 (ABI), U1 (behaviour, done) |
| DEC-14 | Both soft resets identical, 32 core clocks | 15-core-clock W1SC pulse, restart on rewrite. `COMMON_CTRL.soft_rst`: pipeline + both DMA engines, clears result ownership/status. `DMA_CTRL.soft_reset`: DMA and the affected frame only, keeps CSR configuration, statistics and completed/leased AWB results (MAS p33, Table 183 p316–317, p248) | SUPERSEDED | U1 (done) |
| DEC-32 | Pending `AEC_CTRL.commit` dropped by soft reset | Kept across soft reset, consumed at the next accepted ingress SOF; hard reset clears it (owner answer Q4 2026-10-09; MAS §6.21.4.2) | SUPERSEDED | U1 (done) |
| DEC-24, DEC-26, DEC-27 | `CCM/CNF/RESIZER_CTRL.updated` commit gates, immediate commit when idle | `updated` is a compatibility field with no effect. CCM and CNF are captured at the accepted SOF; the Resizer publishes a derived snapshot (E0–E35) (owner Q1; MAS §6.20.4) | SUPERSEDED | U1 (CCM, CNF: done), U4 (Resizer: done) |
| CSR-16 | Gamma = 0, EE = 0x8000, GTM = 0 at `i_rst_n` | RAM contents are undefined after reset (MAS §6.5.2 p33, p317, p334). The VP fills a fixed deterministic poison pattern (user decision 2026-10-09) | SUPERSEDED | U1 (done) |
| DEC-31 | LSC mesh 2..32 nodes per axis | 2..128 nodes per axis, not above the active width/height. A failed validate or an abort leaves the destination invalid (owner Q5; MAS §6.8.3–6.8.4 p55–61) | SUPERSEDED | U1 (done) |
| DEC-28 | BPC dynamic detection: warning, counters read 0 | Spatial BPC only; `dynamic_det_en`, `pixel_age` and `temporal_var` have no effect. The counters clear at frame start, count during the frame and saturate at 16383 (MAS §6.9 p67–78) | SUPERSEDED | U2 (done) |
| DEC-30 (AWB part) | AWB zone memory updated in place | Two result banks with acquire/release; reads without a lease return 0; a frame on an illegal grid is not published (MAS §6.22.3.7, §6.22.4 p246–248). The AEC part of DEC-30 stays | SUPERSEDED for AWB | U2 (done) |
| M2-A4 | A frame whose geometry differs from the one captured at ODMA arm is discarded with `ALIGN` | The ODMA takes each frame's line length and count from the frame, without re-arm; strides are checked at arm (MAS §9.2) | SUPERSEDED | U4 (done) |
| AEC untouched zones after reset | `GREEN_MIN_MAX` read the CSR reset value 0 until the first enabled SOF | Reset clears the written flags, so an untouched zone reads min 4095, max 0 (MAS §6.21.4.3, p237) | SUPERSEDED | U2 (done) |
| ALG-AF-01/02 | AF `EN` read live per row; partial frame publishes with `SCORE_VALID` = 0 | `EN` is sampled once per frame as it enters the pipeline (MAS p260, p264) | SUPERSEDED | U2 (done) |
| DEC-25 | EE scalars and LUTs live | Three resident profiles. `PROFILE_SEL` and the scalars are captured at the accepted window-centre SOF; `PROFILE_WR_SEL` selects the programmed profile; LUTs stay live, so writes to an in-use profile can tear (MAS §6.18.4.1 p188–189, §9.2 p318). User decision: model tearing per row | SUPERSEDED | U3 (done) |
| GTM LUT write gate (CU `!EN \|\| MANUAL`) | Writes accepted while disabled or MANUAL | Writes update the table only while `EN` = 0; `MANUAL` only holds automatic construction (MAS p146) | SUPERSEDED | U3 (done) |
| SPEC-07 | SW-visible `ODMA_BUF_FREE` clears when the buffer is full | Cleared when the output frame starts (MAS p290, Table 159 p292) | SUPERSEDED | U5 (done) |
| DEC-18 (status part) | `ODMA_OVERFLOW` set once per stall episode | Held as a level while the stall lasts; the lossless stall itself is unchanged (MAS §6.25.3.9 p291) | SUPERSEDED (status) | U5 (done) |
| SPEC-04 | Buffer tags, timestamps and `FRAME_COUNTER` have no CSR: not implemented | `IDMA_BUF_TAG/TS0..3` (RW), `ODMA_BUF_TAG/TS/SOF_TS/EOF_TS0..3` (RO), `FRAME_COUNTER` (RO) at `0x3100`–`0x3160`; captured at frame start, published at completion (MAS §6.25.4.1.0.3, Table 160 p293–294) | RESOLVED | U5 (done) |

### 9.2 Kept, because the 07/10 contract agrees

CCM arithmetic, Gamma, WB, CNF code 511, 2DNR variance, LSC strength and spacing,
Input Formatter dimensions, DMA burst splitting and alignment, AXI error handling,
AEC context/FRAME_ID capture and commit transfer: checked against the MAS in U0,
no change.

### 9.3 Open points for the owner (batched for U6)

| ID | Point |
|---|---|
| U0-Q1 | `COMMON_CTRL.frame_start`: the CSR says it is needed only to release the first frame, while MAS §6.25.3.6 starts the IDMA on `IDMA_EN` and a valid head buffer. Does the first frame wait for it, and does it read 0 or 1 until SOF? DEC-16 stays until answered. |
| U0-Q2 | Crop Y must be even (CSR only): a software constraint, or a hardware error? |
| U0-Q3 | AF `SCORE_VALID`: meaning beyond "identifies published scores" (the MAS body does not define it). |
| U0-Q4 | BPC counters while `BPC_EN` = 0: counted or not? |
| U0-Q5 | Effect of a soft reset on the AEC zone "untouched" state (min = 4095). |
| U0-Q6 | LSC: does `COEF_DATA` while idle, or a destination ≥ 3, set `LOAD_PROTOCOL`? (the model sets it) |
| (open) | Three-output architecture; LSC `LOAD_ADDR` after coefficient 65536 (follow-up of 2026-10-09). |

### 9.4 U1 implementation notes (2026-10-09)

Assumptions taken where the MAS is silent. Each one is checked by a test and
goes to the owner with §9.3.

| ID | Assumption | Test |
|---|---|---|
| U1-A1 | `DMA_CTRL.soft_reset` keeps `COMMON_IRQ_STATUS`. Table 183 limits its scope to the DMA block (offsets from `0x3000`) and the busy flag. | U `test_soft_reset` (DMA scope) |
| U1-A2 | A geometry mismatch found when a frame arrives (the unit has already started) sets `DMA_ERR.ALIGN` only. `IRQ_CFG_ERROR` is reserved for a refused start (MAS Table 161: illegal start configuration). | D `test_alignment_rejection`, `test_isp_en_trap` |
| U1-A3 | Poison pattern `((i·0x9E37) ⊕ 0xA5A5) & mask`, with index `64·t + i` for EE and `65·b + i` for the GTM banks. Model and reference are identical. Software must load every LUT it enables. | U `test_external_reset`, EE port test |

Test data that changed with U1. Each change was attributed to one rule by
undoing the rules one at a time (workspace `output/u1_attribution/`):

- `gbrg_ccm_gate` vector: the CCM gate is gone, so the matrix applies at SOF.
- `edge_*` vectors: the generator now clamps the mesh to the image (DEC-31),
  so LSC applies on these small images.
- G-CONT `cont_100`: one frame differs, because of the DMA-only reset scope.
- The generators load unity EE tables before enabling EE; otherwise poison
  RAM would reach the output. With this, `full_chain_*` and the 4K expected
  SHA-256 are unchanged.
- Full-preset profiles load LSC into profile 1 and select it, since a begin
  into the active profile is rejected.

Corrections after the U1 review of 2026-10-09 (`report/ISP_CSR_MAS_U0U1_REVIEW_2026-10-09.md`):

| ID | Defect | Fix | Test |
|---|---|---|---|
| U1-R1 | A DMA-only soft reset abandoned the frame in flight but left the busy flags of the 8 pixel blocks and of AEC/AF set, so firmware waiting for idle could time out | The DMA scope also ends the in-flight frame state, like any other abort: block busy and AEC/AF busy drop. Published results, done bits, errors and configuration outside the scope are kept. The old epoch is aborted, so it cannot clear the busy flags of a later frame (MAS §6.5.3, §9.1.1) | U `test_soft_reset` (both scopes), C G-CONT reset bursts: idle after the pulse and one frame time later, FRAME_ID kept, no late DONE |
| U1-R2 | The accepted SOF adopted `LSC_PROFILE_SEL` without re-checking validity. A begin or an abort after the selector was written made the model switch to an invalid profile and bypass LSC | The selection is re-checked at the SOF: an invalid profile keeps the previous one active and sets `PROFILE_SEL_REJECT` at every SOF while the selector differs; a later validate lets it take effect (MAS Table 31, Figure 29 p65). The reference adopts the selector at the SOF as well | U `test_lsc_selection_at_sof` |

### 9.5 U2 implementation notes (2026-10-09)

| ID | Assumption | Test |
|---|---|---|
| U2-A1 | A COMMON soft reset returns every AEC zone to untouched (min 4095), as the hard reset does. MAS p237 says it "clears published values"; whether the written flags clear too is owner query U0-Q5. | S `test_resets` |
| U2-A2 | `AF_STATUS.score_valid` is set with every publication. With EN sampled at SOF no partial frame exists; the CSR only says it "identifies published scores" (owner query U0-Q3). | S `test_af_enable` |
| U2-A3 | BPC counters: the VP clears them at the accepted SOF and writes the frame's counts at its end; a read in the middle of a frame returns 0, not the running count. A frame with `BPC_EN` = 0 is not counted (owner query U0-Q4). | S `test_bpc_counters` |
| U2-A4 | A bank freed by an abort is eligible at the next SOF even on the same core clock. In silicon the next frame's SOF reaches AWB some clocks after the abort; this loosely-timed model can place both on one edge. The same-edge rule of MAS §6.22.4.3 is applied to RELEASE. | C G-CONT `idma_axi_error` bursts |
| U2-A5 | Not modelled: the four-entry ingress context FIFO (ingress and local SOF are the same event in the VP, so context overflow cannot occur), the E2 publication delay (the bank becomes READY at the frame-end event), and the one-clock read latency of the AEC zone/histogram memories. The AWB zone read latency is modelled (review U2-R2): a read on the edge of the `AWB_ZONE_ADDR` write returns the previous address's word. `csr_latency` does not bound the core clock, so software must wait the clocks itself. | S `test_awb_commands` |
| U2-A7 | Statistics are published when the pipeline thread ends the frame. Under output backpressure this can follow the ODMA completion of that frame, whereas in silicon the AWB/AEC taps publish before the last output pixel is written. Software must wait for the publication (`stats_ready_irq`, `READY_COUNT`), as MAS §6.22.4.1 prescribes, not infer it from `ODMA_DONE`. | D `test_awb_driver` (fast CSR) |
| U2-A6 | Driver compatibility reader `fx1_isp_read_awb_global` drains every READY result and returns the newest; with none ready it returns the result it read last, so code written for the previous map keeps its semantics. `FX1_ISP_ENODATA` before any result and after a soft reset; `FX1_ISP_EBUSY` while the caller holds a lease. | D `test_awb_driver` |

Test data that changed with U2 (statistics only; no NV12 file changed, the
4K expected SHA-256 and the platform fixture CRCs are unchanged):

- Every statistics script now reads AWB through ownership: `AWB_RESULT_STATUS`
  and `AWB_DROP_COUNT`, then each READY result acquired, read and released in
  completion order. Multi-frame vectors and G-CONT bursts of three frames
  therefore show the two oldest results and count the later frames as drops.
- Every vector reads `BPC_NUM_*` = 0 when BPC is off, and
  `bpc_dynamic_requested_64x32` now reports its real counts.
- Platform fixture: the statistics profile uses a 1×1 AWB grid instead of the
  0×0 "global only" grid, which is illegal since CSR/MAS 07/10. FRAME_IDs,
  context IDs and CRCs are unchanged.
- The `gtm_manual_qhd` profile of `make_profile.py` keeps its 0×0 AWB grid on
  purpose: it now exercises the illegal-grid path (no AWB publication).

Corrections after the U2 review of 2026-10-12 (`report/ISP_CSR_MAS_U2_REVIEW_2026-10-12.md`):

| ID | Defect | Fix | Test |
|---|---|---|---|
| U2-R1 | When a lease was found lost (a COMMON soft reset the driver did not issue), the compatibility reader still returned its cached pre-reset result | The driver drops the cache together with the lease. A reset issued behind the driver while it holds no lease stays invisible to it; whoever resets the ISP must call `fx1_isp_soft_reset` or `fx1_isp_init` (header) | D `test_awb_driver`: compatibility reader `ENODATA` right after the lost lease |
| U2-R2 | The driver assumed a CSR transaction lasts a core clock. With CSR accesses shorter than the clock, it read `READ_VALID` on the ACQUIRE edge, returned `ENODATA` and lost the lease it held | The driver waits one core clock (`delay_cycles`) after ACQUIRE, after RELEASE (so the next SOF can use the bank) and after every result-memory address write (AWB and AEC zones, AEC histogram). The model now has the AWB zone read latency | `driver_tlm_fast_csr` (core 10 ns, CSR 1 ns: AWB driver test and the statistics vectors), S `test_awb_commands` |

### 9.6 U3 implementation notes (2026-10-12)

| ID | Assumption | Test |
|---|---|---|
| U3-A1 | The EE profile, enable and scalars are sampled at the accepted SOF. The MAS samples them at the EE window-centre SOF, which is later than ingress; the VP has one accepted SOF per frame. | U `test_ee_profiles`, P `test_frame_coherence` |
| U3-A2 | `PROFILE_WR_SEL` code 3: a table-port write is ignored but `EE_LUT_ADDR` still advances; `EE_LUT_RDATA` reads 0; a scalar write is ignored and the register keeps its readback. The MAS says only "its writes are ignored". | U `test_ee_profiles` |
| U3-A3 | The radial error event is raised at the end of a processed frame. An aborted frame raises nothing. | U `test_ee_radial_error` |
| U3-A4 | GTM: a table write while `EN` = 1 is discarded and `GTM_LUT_ADDR` still advances. | U `test_gtm_lut_gate` |
| U3-A5 | Driver: `fx1_isp_ee_load_profile` refuses (`FX1_ISP_EBUSY`) while `EE_STATUS.busy` = 1 every profile selected since EE was last seen idle (a set, review U3-R1); reading busy = 0 resets the set to the current selection. GTM software curve (MAS §9.2): `fx1_isp_gtm_manual_begin` (step 1) records `IDMA_FRAME_COUNT` and the frames in flight; `fx1_isp_load_gtm_lut` returns `FX1_ISP_EAGAIN` until the pipeline has taken `in_flight + 1` more frames, so a frame queued after step 1 has had its SOF and consumed any pending automatic curve (step 2), then loads, verifies (`FX1_ISP_EIO`) and restores `EN` (review U3-R2). `IDMA_FRAME_COUNT` counts every frame whose end can have built a curve, also one whose output failed, and only `i_rst_n` clears it: the load proceeds at once only when it is 0 and nothing is queued (review U3-R3), which also covers a re-initialisation without `i_rst_n`. "Nothing queued" matters on its own: the first frame after `i_rst_n` may already run on a manual curve, reading the read bank live, while `IDMA_FRAME_COUNT` is still 0 (review C1). | D `test_ee_driver`, `test_gtm_driver` |

Test data that changed with U3:

- New vector `ee_profiles_3frames_64x48`: three profiles with distinct content, selected 0, code 3, then 2.
- G-CONT `cont_100`: the EE delta now programs a random profile and selects a random code, drawn from a separate random stream; base configuration loads unity tables into all three profiles. 42 expected frames changed. With every profile choice forced to 0 (same draws), all 155 expected files equal the post-U2 data, so every change comes from the profile choice (`output/u3_attribution/attribute_sequence_u3.py`).
- No other vector, block vector, 4K vector or fixture data changed: existing profiles program profile 0 between frames, which behaves as before.

Corrections after the U3 review of 2026-10-12 (`report/ISP_CSR_MAS_U3_REVIEW_2026-10-12.md`):

| ID | Defect | Fix | Test |
|---|---|---|---|
| U3-R1 | The driver kept only the last two selections: switching 2 → 0 → 1 while a frame used profile 2 let a load into profile 2 through and changed that frame | The driver keeps the set of profiles selected since EE was last seen idle and refuses any of them while EE is busy | D `test_ee_driver` (two switches in one busy period: profile 2 and the intermediate profile refused, the frame equals the golden) |
| U3-R2 | `fx1_isp_load_gtm_lut` skipped MAS §9.2 step 2: an automatic curve built at the end of the previous frame was still pending and replaced the software curve at the next SOF | Step 1 is `fx1_isp_gtm_manual_begin`; the load returns `FX1_ISP_EAGAIN` until a frame started after step 1 has completed, then loads, verifies and restores EN. The API now returns `int` (was `void`) | D `test_gtm_driver` (auto frame, refused, one frame, load, the curve survives two more SOFs; discarded writes detected) |
| U3-R3 | The cold-state test used `ODMA_FRAME_COUNT` and a snapshot taken at `fx1_isp_init`: a first automatic frame whose output failed (lost) left a curve pending with no successful output, and a re-initialisation without `i_rst_n` reset the snapshot; both loaded at once and lost the curve at the next SOF | Cold state and the hand-over wait use `IDMA_FRAME_COUNT` (frames the pipeline took, kept by soft reset): cold only when it is 0 and nothing is queued | D `test_gtm_driver` (first frame lost, warm init; normal and fast CSR) |

### 9.7 U4 implementation notes (2026-10-12)

| ID | Assumption | Test |
|---|---|---|
| U4-A1 | A frame whose active raster does not match the published set (started before the derivation of its key finished, or after a reset) is not scaled. The MAS states this for a frame started after an abort ("is not scaled") and asks software to complete the configuration before starting a frame otherwise. | U `test_resizer_publication` (reset set), pipeline `resizer_applies` |
| U4-A2 | A frame wider than the ODMA Y/UV stride captured at arm is refused with `DMA_ERR.ALIGN` (no `IRQ_CFG_ERROR`, U1-A2) and discarded, nothing written. The MAS only asks software to size buffers for the largest mode. | D `test_resizer_switch` |
| U4-A3 | The derivation runs whatever `COMMON_CTRL.isp_en`; DEC-20 is kept: an ODMA armed while `isp_en` = 0 sees 0 × 0. | D `test_isp_en_trap` |
| U4-A4 | Edges: a key written on edge w is stable from w + 1; several writes before one edge give one key, saved at its E0 (review U4-R3). A scaling set is published at E35 = E0 + 35 and seen from the next edge; E0 = max(key stable, dividers free). A pass-through set is published on the edge after its key is stable, whether or not a derivation runs, and a derivation that completes later does not override it (review U4-R2). A derivation whose key changed ends at its E35 unpublished, and the next one starts there, so publication can take two derivations after the last change. Reset: the reset set reads 1920 × 1080 (the CSR reset values, unscaled, no key); nothing starts before the end of the soft-reset pulse, which then uses the live key, also one written during the pulse; after `i_rst_n` from edge 0. | U `test_resizer_publication` |
| U4-A5 | Frame retention and the drain rule hold by construction: the pipeline thread finishes a frame before it accepts the next SOF, so frames never overlap in the Resizer. The overlap of unchanged configurations in the decimators is not modelled (timing only). | D `test_resizer_switch` |

Driver: `fx1_isp_start` waits 80 core clocks (two derivations plus margin,
review U4-R1) after `ISP_EN` before it reads `RESIZER_OUT_W/H` and arms;
`fx1_isp_soft_reset` waits 80 more after the pulse. The tests and `tools/run_raw.cpp` that drive registers directly wait
the same before the first frame after a configuration change. No vector,
sequence, 4K or fixture data changed: every profile writes `RESIZER_CTRL`
with `updated` set, and configuration precedes the frames.

Corrections after the U4 review of 2026-10-12 (`report/ISP_CSR_MAS_U4_REVIEW_2026-10-12.md`):

| ID | Defect | Fix | Test |
|---|---|---|---|
| U4-R1 | The driver waited 40 clocks, one derivation; a key changed while the dividers worked on a previous key needs two, so `fx1_isp_start` read the previous set (wrong `EINVAL`, or a stale cached geometry) | The driver, `run_raw` and the tests wait 80 clocks (2 × 36 plus margin) | D `test_resizer_waits` (key changed during a derivation, stride sized for the last mode and for the largest; normal and fast CSR) |
| U4-R2 | Pass-through waited for a running derivation to end | Event-ordered controller: the pass-through set is due on the edge after its key is stable, independent of the dividers; the derivation's completion is dropped | U `test_resizer_publication` (disable during a derivation, SOF before and after) |
| U4-R3 | The controller saved a key before its E0: two writes before one edge, or a key written during the reset pulse, derived an obsolete key first | The key is saved only when its E0 comes; a key written during the pulse is stable from the end of the pulse | U `test_resizer_publication` (two writes before one edge; scaling and pass-through keys written in the pulse) |

### 9.8 U5 implementation notes (2026-10-12)

| ID | Assumption | Test |
|---|---|---|
| U5-A1 | `FRAME_COUNTER` counts core clocks from the release of `i_rst_n` (0 on the release edge), modulo 2^32, and reads 0 while `i_rst_n` is asserted. Neither soft reset touches it: it is the time base of the timestamps software writes, and the MAS calls it free-running. It is computed at the read edge (no clock event). The C++ `csr_read` without a cycle returns the count at the last edge the control unit processed; the TLM debug transport reads it at the current time. | U `test_time_base`; D `test_frame_counter`, `test_frame_counter_wrap`, `test_metadata_resets` |
| U5-A2 | (Superseded by V107-D65: the DMA scope keeps published metadata.) MAS Table 183 has no metadata row. `IDMA_BUF_TAG/TS` are configuration: kept by both soft resets. The published `ODMA_BUF_*` metadata is DMA state: cleared by both soft resets with the buffer ownership bits (no buffer stays `DONE` to claim it). `i_rst_n` clears all of them. | D `test_metadata_resets` |
| U5-A3 | A frame abandoned upstream (IDMA AXI error, frame dropped by the pipeline) after the ODMA took its buffer: the ODMA sets the buffer's `FREE` bit back, keeps the rotation, and publishes no `DONE` and no metadata, so the next frame uses the same buffer (MAS §9.1.1: "processed as if it had never existed"). MAS Table 156 names only software as the setter of `FREE`; the alternative (the buffer stays taken and software frees it, as after an ODMA AXI error) is an owner question. Software sees the same `FREE` state as before U5, so the driver is unchanged. | D `test_axi_read_error` (32 rows) |
| U5-A4 | Time points. Capture: the IDMA takes the buffer's tag and timestamp when it arms it (with the geometry), before its first read; later writes reach the next use of the buffer. `SOF_TS`: the edge on which the ODMA accepts the first luma line of the frame, before its first write (also the edge on which `FREE` is cleared). `EOF_TS`: the edge on which the ODMA accepts the last line of the frame (both planes), before that line's writes. Publication (metadata, then `DONE`, same edge) follows when the last write response returns. | D `test_metadata_snapshot_and_points` (exact edges from the bus log) |
| U5-A5 | Overflow condition: ODMA enabled, the first line of a frame waiting, and the head buffer (rotation order) not free; MAS "no output buffer is free" is read as the buffer the unit waits for. The level is released when the condition ends (buffer freed, ODMA disabled, reset) and the bit then stays set until W1C. `IRQ_ODMA_OVERFLOW` is raised on each rising edge of the condition. | D `test_overflow_lossless`, C `test_cont_tlm` (overflow) |
| U5-A6 | Driver: `fx1_isp_queue_frame_meta` writes the tag and timestamp before `IDMA_BUF_VALID`; `fx1_isp_next_done_meta` reads the metadata before it returns the buffer (`DONE` W1C). The older queue calls do not write the tag registers, so their frames carry whatever the registers hold (0 after reset, or the buffer's previous tag). | D `test_metadata_driver` |

Timestamps are VP core-clock edges with the parameters of the run
(`core_period`, memory latency): they show order and latency in the model,
not silicon performance.

### 9.9 U6 notes (2026-10-12)

| ID | Assumption | Test |
|---|---|---|
| U6-A1 | Driver `fx1_isp_dma_reset` (DMA soft reset while streaming): what it keeps and changes is listed below the table. | D `test_dma_reset_driver`, `test_dma_reset_stats`, `test_dma_reset_gtm` (normal and fast CSR); platform `fx1_smoke_isp_recovery` E, `fx1_smoke_isp_mas0710` B |

U6-A1 in detail:
- **Keeps:** the enables, the AWB lease and the cached AWB result, the EE
  driver state and `COMMON_IRQ_STATUS` (U1-A1).
- **Clears:** `DMA_IRQ_STAT` and `DMA_ERR`, and restarts both rotations at 0.
- **AEC zones:** marked untrusted only if a frame in flight had not
  completed, because only such a frame can have overwritten them (MAS
  §9.1.1). "Completed" means the `ODMA_BUF_DONE` bit of that frame's own
  output (queue position k owns output `out_done` + k). A DONE bit of any
  other buffer is not counted (review U6-R3). An idle reset, or one that
  drops only DONE frames, keeps the trust state (review U6-R1).
- **DONE acknowledgement (all callers of `fx1_isp_next_done*`):** a W1C on
  the core clock on which the hardware sets DONE loses (hardware set wins).
  This is possible when a CSR access is shorter than a core clock. The
  driver reads the bit back and clears it again one core clock later, so a
  collected buffer never keeps a stale DONE that would make it look complete
  when queued again (review U6-R3).
- **GTM hand-over:** a pending one stops counting the dropped frames. If its
  fence is not reached, it waits for one frame taken after the reset
  (`IDMA_FRAME_COUNT` + 1). A dropped frame builds no curve, so that frame's
  SOF consumes any pending curve. With no frame taken since `i_rst_n` the
  load is cold (review U6-R2).
- **FRAME_ID:** an aborted frame keeps the number it was given (MAS §9.1.1,
  "FRAME_ID skips one"). A first platform run that aborted the only frame in
  flight observed the skip (AWB FRAME_ID 9, then 11). The final test aborts
  the second of two queued frames, and checks only that results after the
  reset are newer than the leased one.

Acceptance baseline (`docs/evidence/raw_regression_baseline.json`) regenerated in U6.
- **Frames:** the NV12 checksums of all 308 frames are unchanged.
- **Statistics:** the readout checksums of all 116 runs changed with the U2
  rules (AWB ownership and drops, BPC counters, AEC untouched zones).
- **Attribution:** compared read by read with the previous run, every
  difference has one of those causes and none is unexplained.
- The previous baseline was from before the revision. With it,
  `fx1_isp.regression.raw` (registered only with
  `-DFX1_ISP_RAW_INVENTORY`) would have failed since U2.

Platform coverage of the revision (plan U6 item 3):
- `fx1_smoke_isp_mas0710`: AWB ownership with drops and reuse, frame
  metadata, an AWB lease across a DMA reset, Resizer from the first frame and
  a mode switch. Its expected values come from the reference through
  `gen_fixture.py`.
- `fx1_smoke_isp_recovery` phase E and tag checks in every phase.

The questions for the owner (§9.3–§9.8) are batched in the workspace
document `plan/alg/OWNER_QUERIES_CSR_MAS_0710_2026-10-12.md` (23 questions).
Until they are answered, the acceptance covers the VP contract described
here, not silicon behaviour. In particular the LSC `LOAD_ADDR` terminal value
is not asserted at the maximum mesh.


## 10. CSR/MAS/Tuning revision v1.07 (plan ISP_V107_UPDATE_PLAN_2026-10-28)

The v1.07 CSR `444e72f8…` (573 registers, 819 fields), MAS `3fa4097d…`
(MAS pages are pages of its 421-page PDF conversion) and Tuning
`9119eb12…` replace the 07/10 sources. The user decisions of 2026-10-28:
- follow the spec where it is explicit;
- where it is not, record a V107-Ax assumption with a test, and batch the
  question for the owner (`plan/alg/OWNER_QUERIES_V107_2026-10-28.md`);
- keep the old driver entry points whose semantics v1.07 keeps, and remove
  the others and port their callers.

The 23 owner questions of 07/10 are closed by v1.07. The ABI delta is in
[ISP_CSR_REVISION_V107.md](ISP_CSR_REVISION_V107.md).

### 10.1 V107-1 notes (2026-10-28)

| ID | Decision or assumption | Test |
|---|---|---|
| V107-A1 | `TUNE_CAPS` reports only the capabilities the model implements. The CSR reset `0x003FFFFF` describes a complete build. Each milestone sets its bits when its capability is complete (`control_unit::implemented_caps`, 0 in V107-1). Software that trusts the CSR reset would use features the VP lacks. | U `test_register_engine` `test_discovery` |
| V107-A6 | W1S follows MAS Table 8-180 exactly in the register engine: a software set wins over a hardware clear on the same edge, whether Q was 0 or 1 and in either order of the calls (review V107-1 R1 replaced the first V107-1 rule, which protected only bits already 1). The zero-latency engines are fixed at the consumer instead: a consumer samples a request register as registered on its edge (`register_file::registered`, `engine_context::request`), so a request software set on edge c reaches it on edge c + 1. The IDMA (`IDMA_BUF_VALID`), the ODMA (`ODMA_BUF_FREE`) and the AEC SOF transfer (`AEC_CTRL.commit`) use it. The IDMA and ODMA wait one core clock when their head request was set on the current edge. A request re-asserted on the edge that consumes it stays set and is a new request. With the engine taking requests on the edge that set them, the literal rule kept 29 taken buffers set in the driver fault campaign and 8 of 48 campaigns failed: 1 deadlock and 7 with wrong frame content (V107-1 report §3.1). | U `test_register_engine` (`test_w1s_software_set_wins`: Q × order; `test_registered_view`; `test_aec_commit_on_sof_edge`); D `test_dma_tlm` `test_odma_free_next_edge_and_reassert`, `test_idma_valid_next_edge`; `driver_tlm` campaign (normal and fast CSR) |
| V107-A7 | `RESIZER_CTRL.scale` is 5 bits in v1.07. Until V107-7 implements the presets 16..21, codes 15..31 are stored and select FullRes (pass-through) in the control unit, the C++ pipeline plan and the Python reference. Before, the C++ plan and the reference aliased a code onto its low four bits (21 → 1920×1080), and the control unit read past its 16-entry table. | U `test_resizer_geometry` (codes 16..31); `pipeline_vectors` `resizer_plans.txt` (32 codes × 10 sizes, reference vs C++) |
| V107-D1 | W1P is a register access type (`w1p_mask`). The write handler receives `write_effect::w1p_pulse`. The bits are never stored, and they read 0. `AWB_RESULT_CTRL` uses the pulse in place of the U2 store-then-clear. | U `test_register_engine` (all 19 W1P fields), `test_stats` |
| V107-D2 | `RESIZER_OUT_W/H` reset to 0 in v1.07. The VP keeps publishing the 07/10 reset set (1920×1080 derived from the reset input) until V107-7 replaces the Resizer contract (U4-A4 is superseded there). | U `test_literal_resets` |
| V107-D3 | `COMMON_VER_ID` reports `0x00010007` from V107-1, while the v1.07 behaviour lands milestone by milestone. Software must use `TUNE_CAPS` (V107-A1) to discover features, not the version. | U `test_discovery`, I `csr_tlm` |
| V107-D4 | During a soft-reset window, W1P commands are dropped like W1S/W1C effects; RW fields still update. | U `test_register_engine` `test_w1p_command_block` |
| V107-D5 | The register file gains a pre-write filter (`on_pre_write`: reject the write or narrow it to some lanes, with no storage change and no hook), bus-read actions (`on_bus_read`, run by `bus_read` only, never by debug reads) and `hw_ack` (self-feedback clear inside a write handler, outside the priority rules; used for `LSC_LOAD_CTRL`). V107-3/4/6 use them for SEAL locks, table ports and lease reads. | U `test_register_engine` |

### 10.2 V107-2 notes (2026-10-28)

| ID | Decision or assumption | Test |
|---|---|---|
| V107-A8 | Container bits above the native depth (bits [15:12] for RAW12, [15:10] for RAW10) are not part of the sample: the input-boundary adapter keeps `raw[11:0]` or `raw[9:0] << 2`. MAS calls them "shall be zero" and has no detector; firmware validates them before submission (`fx1_isp_raw_upper_bits`). | D `test_dma_tlm` `test_input_depth` (double shift visible); driver `run_error_cases` |
| V107-A9 | The DMA start check is MAS Table 6-153 only: even external W/H, 2 ≤ W ≤ 3842, 2 ≤ H ≤ 2162. The derived active limits 32×32..3840×2160 are a software precondition ("the DMA's even-size check alone does not enforce this condition", §6.6.3.5): the reference driver refuses such a geometry (`FX1_ISP_EINVAL`), and the VP processes one programmed directly. Owner question: does the Input Formatter reject such a tuple in hardware? | U `test_dma_rules` `test_active_extent` (Table 6-20), `test_setup_checks`; D alignment cases; driver `run_error_cases`, vector replay (5 kernel-edge vectors below the minimum refused by the driver and replayed as raw writes) |
| V107-A10 | `COMMON_INPUT_FORMAT.DEPTH` 2/3 ("invalid") is handled like a Table 6-153 violation at IDMA frame start: `DMA_ERR.ALIGN`, `IRQ_CFG_ERROR`, the buffer is not read. The driver refuses the value. Owner question: is the write itself rejected instead? | D alignment cases "depth 2/3"; U `test_setup_checks`; driver `run_error_cases` |
| V107-D6 | A buffer refused at IDMA frame start is returned: its `IDMA_BUF_VALID` bit is cleared and the rotation does not advance, so a later valid buffer is not taken ahead of it (MAS v1.07 §6.25.3.4). Before, the VP kept the bit set and retried on every register write. The ODMA claim rule of the same section lands with the ODMA rework in V107-8. | D `test_idma_reject_keeps_rotation`, alignment cases |
| V107-D7 | `COMMON_CTRL.FRAME_START` is a compatibility no-op (CSR v1.07): accepted, cleared on the write (self-feedback), no effect on admission. DEC-16 and U0-Q1 are superseded. | U `test_frame_start_and_done`, `test_soft_reset` |
| V107-D8 | The native depth is captured with the ingress frame (accepted SOF snapshot) and normalized at the input boundary of the pipeline (and of the M2 stub datapath); the IDMA transports the container unchanged (§6.25). The IDMA start check reads the same register. The IDLE-update rule, under which a write outside IDLE is rejected, lands with the IDLE class in V107-3; until then software must change DEPTH and geometry only while idle (the driver enforces this). | U pipeline vectors `input_raw10/raw12_full_bggr_66x34` (equal expected, asserted by `gen_vectors.py`); T `fx1_isp.tools.input_depth` |
| V107-D9 | The ingress sequence is the existing frame counter: it starts at 0, the first accepted SOF is 1, it wraps modulo 2^32, and soft resets and mode changes keep it; only `i_rst_n` restarts it. It is published as `*_FRAME_ID` today and as `TUNE_CHc_BUFn_FRAME` from V107-8. | U `test_ingress_sequence` |

### 10.3 V107-3 notes (2026-10-28)

| ID | Decision or assumption | Test |
|---|---|---|
| V107-D10 | Bank-owned fields: 210 words (73 legacy algorithm words, 137 TUNE extension words), from the CSR v1.07 "Update: PROFILE" tags and the Tuning v1.07 "profile" application rules, minus the loader ports of Table 9-194 (`GAMMA_LUT_*`, `GTM_LUT_*`), resident EE scalars, `EE_CTRL.PROFILE_WR_SEL` and the LSC load ports. LSC EN/STRENGTH/PROFILE_SEL and the `*_CONTEXT_ID` words are bank-owned as the sources classify them. Masks are per field: words that mix bank-owned fields with commands, no-ops or live selectors (`AEC_CTRL`, `AF_CTRL`, `EE_CTRL`, `GAMMA_CTRL`, `CCM_CTRL`, `CNF_CTRL`) keep their other bits live. | U `test_profile` |
| V107-D11 | Implementation: the register file keeps the datapath value of every bank-owned field. A CSR write in profile mode (or to a TUNE extension field in either mode) goes to the TUNE_BANK-selected bank, and the register file sees the same write with those bits unchanged. Reads merge the selected bank. Activation copies the bank into the register file on the admitted SOF. This is exact for the VP because a frame's configuration is captured once, at the accepted SOF, and the drain barrier keeps two epochs from overlapping. The frame token is the bank in effect for every in-flight frame. | U `test_profile`; D `test_profile_admission` |
| V107-D12 | The IDLE condition (§9.9.3): IDMA_EN = 0 or ISP_EN = 0; no admitted, unretired frame; no AXI transaction outstanding (engine probe); no soft-reset window. The VP has no long-running automatic builder: GTM curve, NR estimate and LSC preparation complete on their edge. IDLE class in V107-3: input geometry, Bayer, input format, OUTPUT_ENABLE, LSC_MESH_NODES, TUNE_MODE. `RESIZER_CTRL` and the channel crop words join with the v1.07 Resizer contract in V107-7, because the U4 mode-switch-while-streaming behaviour is replaced there. | U `test_profile` `test_idle_class`; D `test_profile_admission`; C (G-CONT harness stops and drains ingress before geometry and mesh writes) |
| V107-D13 | APPLY eligibility: accepted on edge c, first eligible on c + 1. With an active profile, an admission on edge c still uses the old profile. Without one, admission waits for the edge. A NEXT request holds admission from eligibility until the old epoch has drained. An EXACT future target admits older sequences with the old profile until NEXT_FRAME equals the target, then drains. A CANCEL on the admission edge loses (the request activates). | U `test_profile` `test_init_edit_seal_apply`, `test_command_rules`, `test_cancel_and_wrap` |
| V107-A11 | UNSEAL of an active, referenced or pending bank, INIT of a protected bank, and SEAL of an active/pending/referenced (hence sealed) bank: rejected with LOCKED. MAS names the conditions but not the error bit. SEAL of an already sealed, unprotected bank is a no-op without error. | U `test_profile` |
| V107-A12 | Profile mode before V107-4: TUNE_TABLE_CMD confirms nothing and sets NOT_READY, so a bank whose enabled blocks need a bank payload cannot be sealed yet. SEAL checks in V107-3: NR mode/blend/limits, CNF blend, EE/LSC selector code 3, AWB ellipses/ROI/MIN_G/grid, AF rectangles, weighted-histogram ROI/steps, and LSC (EN = 1) resident validity. The AEC relations join with the v1.07 AE engine in V107-6; payload readiness of Gamma/GTM/NR/BPC/LSC sectors and EE residents in V107-4. | U `test_profile` `test_seal_validation` |
| V107-A13 | Epoch isolation: activation discards the automatic GTM curve (when the activated GTM is automatic) and the NR estimate, so the first frame of an epoch uses no history from the previous one (as after a reset). A manual GTM curve is software payload and is kept; it becomes bank-owned in V107-4. | U `test_profile` (activation path); review |
| V107-A14 | Four context credits apply in both modes: a fifth admission waits for a retirement. | U `test_admission_holds` |
| V107-D14 | DMA soft reset scope: the DMA blocks 0x3000..0x7FFF only. Before, it also covered the TUNE and extension pages (0x8000 up), so it would have cleared TUNE_ERROR, which MAS clears only by common reset. The 07/10 map had no register above 0x3200, so no earlier behaviour changes. | U `test_profile` `test_resets` |
| V107-D15 | Legacy commands in profile mode: AEC COMMIT is rejected with MODE. Legacy gamma confirmation joins with the gamma banks in V107-4. LSC_PROFILE_SEL in profile mode is a bank scalar: the legacy write-time select check (and its LSC_ERROR) does not run. | U `test_legacy_commands_in_profile_mode` |
| V107-D16 | `debug_accepted_sof()` (a test-bench SOF without a frame) retires its ingress context at once. | D/I suites |

### 10.4 V107-4 notes (2026-10-28)

| ID | Decision or assumption | Test |
|---|---|---|
| V107-D17 | Gamma (§6.14.4): two 256-entry shared-RGB curve banks with per-entry write flags; unwritten entries read zero. The legacy port and the datapath use bank 0, `TUNE_GAMMA_PORT/DATA` the TUNE_BANK bank in both modes. A curve applies only once confirmed: legacy `LUT_LOAD_CONFIRM` (EN = 0, pipeline idle, all 256 entries, no write on the same edge), profile `GAMMA_CONFIRM`. Every write clears the confirmation. `GAMMA_STATUS.LUT_LOADED` is bank 0's validity. A legacy common reset keeps it; INIT of bank 0, leaving profile mode, a profile-mode common reset and a hard reset clear it. | U `test_table_confirmation`, `test_profile_tables`; vectors (profiles confirm) |
| V107-D18 | GTM manual curves (§6.16.4.3): two 65-point banks, separate from the automatic ping-pong pair, with both byte lanes tracked per point. The legacy port writes manual bank 0 in legacy mode and the TUNE_BANK bank in profile mode, only while GTM is disabled in that profile. MANUAL = 1 applies the frame bank's curve only when complete; readback returns the manual curve, or the automatic one in legacy mode with MANUAL = 0. Before, a manual load went into the automatic read bank, where a pending automatic curve replaced it at the next SOF. This was visible in G-CONT: 12 frames changed, all attributed to this by reverse patching. | U `test_table_confirmation`, `test_gtm_lut_gate`, `test_profile_tables`; C G-CONT (12 frames) |
| V107-D19 | EE residents (§9.9.1): 192 LUT write flags per resident. A resident is used only once confirmed (`PROFILE_LOAD_CONFIRM`): legacy with EE disabled and the pipeline idle; profile mode when the resident is not owned by a protected bank. Confirmation fails (NOT_READY) without all flags or with a write on the same edge. Scalar and LUT writes clear the resident's validity, and an input-domain change also clears its flags. Masked scalar writes merge with the selected resident's value, and scalar/LUT reads return the selected resident. Code 3 discards writes and reads resident 0 (audit R2 fixed). `EE_STATUS.lut_valid/active_profile/active_valid` are reported; an unconfirmed selected resident passes the frame through. | U `test_table_confirmation`, `test_ee_profiles`, `test_ee_radial_error`, `test_ee_profiles` (rig), `test_resident_pins` |
| V107-D20 | Bank payloads (§9.9.5, Table 9-197): NR curve (17 knots), BPC factory list (≤ 1024 points, `TUNE_BPC_COUNT` bank-owned), LSC sector intervals (≤ 31 per axis), each with write flags. `TUNE_TABLE_CMD` bits combine. Each check sets its `TUNE_TABLE_STATUS` bit or NOT_READY; a protected bank gives LOCKED. Payload writes (including COUNT) clear the confirmation. The checks are instantaneous in the VP, so the "write during a running check" case cannot occur. The datapath use of the NR curve, BPC list and LSC sectors lands in V107-5. | U `test_profile_tables` |
| V107-D21 | SEAL payload readiness (Table 9-196): Gamma bank curve, GTM manual completeness, NR mode 2 curve, BPC factory list, LSC sectors, EE selected resident. SEAL pins the selected EE and LSC residents until UNSEAL or exit. Writes, confirmations and LSC loads aimed at a pinned resident are LOCKED. | U `test_profile_tables`, `test_resident_pins` |
| V107-A15 | Conditions of a confirmation are read before the edge (registered Q): a write that clears `GAMMA_CTRL.EN` and pulses `LUT_LOAD_CONFIRM` is refused (EN was 1), and one that sets EN on the confirming edge is accepted (EN was 0). | U `test_table_confirmation` |
| V107-A16 | Legacy confirmations that cannot be accepted (Gamma: EN = 1, not idle, incomplete; EE: enabled, not idle) leave the table invalid without an error bit, except an EE confirmation with missing flags, which sets NOT_READY as §9.9.1 states. Driver: `fx1_isp_load_gamma` returns EBUSY / EAGAIN / EIO; `fx1_isp_ee_load_profile` returns EAGAIN when it cannot confirm. | D `run_error_cases`, EE driver test |
| V107-A17 | LSC mesh 2..32 nodes per axis (MAS v1.07, owner question A13 pending; the 07/10 limit was 128). | U `test_lsc_*`; driver |

### 10.5 V107-5 notes (2026-10-28)

| ID | Decision or assumption | Test |
|---|---|---|
| V107-D22 | CSC (§6.15.3.3, Table 6-92): RSY, RSCB, RSCR and OY are selected by range and standard. `TUNE_CSC_CTRL.FULL_RANGE` resets to 1 and the legacy datapath uses the reset (Table 9-195), so **every legacy NV12 output is now full range** (Y offset 0); before it was limited range. The look (profile mode) applies contrast and brightness around the pedestal (0 full, 16 limited) and a 2×2 matrix to the original U0/V0. Each stage rounds once with R(n, d) (ties toward +∞) and clamps to 0..255 only. Attribution: with the legacy range reverted to limited in a scratch copy of the reference, every regenerated golden (vectors, 4K, G-CONT, blocks) equals the earlier data except changes already attributed in V107-1/V107-4; a C++ probe with limited legacy range reproduces the RAW baseline (report V107-5). | U `kernels` `test_csc_range`, `test_csc_look` (MAS-derived literals); `pipeline_vectors` (`tune_wb_dms_csc_full_look_cnf`, `tune_csc_limited_look_bggr`, all legacy vectors); RAW |
| V107-D23 | GTM (§6.16.4.1, §6.16.4.3): KEY, LWHITE and MANUAL are captured at the accepted SOF of the measured frame; the EOF builder uses the captured values (ALG-GTM-09 superseded). The reference replays configuration between frames, so no golden changed. | U `control` `test_gtm_capture_at_sof`, `profile` `test_history_per_epoch` |
| V107-D24 | WB (§6.10.3.6): the Gb sites use `TUNE_WB_GB` in profile mode and `WB_GAIN_G` in legacy mode. | U `kernels` `test_wb_gb`; `pipeline_vectors` (`tune_wb_dms_csc_full_look_cnf`, `tune_ignored_in_legacy`) |
| V107-D25 | Demosaic margins (§6.12.3.7): the equal branch is taken when \|dh − dv\| ≤ GREEN (and the R/B equivalent ≤ RB). Reset 0 is the earlier behaviour. | U `pipeline_vectors` |
| V107-D26 | Output blends: CNF with the aligned original (§6.19.3.6), 2DNR with the pre-median input of the same row (§6.17.3.10). 256 (reset) is no blend. | U `kernels` `test_cnf_blend`, `test_nr_models` (0, 128/64, 256); `pipeline_vectors` (`tune_wb_dms_csc_full_look_cnf`, `tune_nr_curve_bands_2frames`) |
| V107-D27 | BPC (§6.9.3.6): COLOR_THRESH_EN selects separate K and floor for G and R/B sites. FACTORY_EN forces the listed sites (bank payload, confirmed, raster order) to the same average correction, and they are counted in `BPC_NUM_DEFECTIVE`. | U `pipeline_vectors` (`tune_bpc_lsc_sectors`) |
| V107-D28 | 2DNR (§6.17.3.10): MODE 0 uses the automatic estimate of the current epoch, or SEED before the first estimate (the estimate is cleared at profile activation and by a common soft reset). MODE 1 uses VARIANCE; MODE 2 uses the 17-knot curve. Then SCALE (UQ8.8), MIN/MAX and the four band scales apply. MEDIAN_EN = 0 feeds the aligned original to the Haar stage. Legacy mode is MODE 0 with SEED 0, unity scales and no blend, which is the earlier behaviour. | U `kernels` `test_nr_models` (model equivalences), `profile` `test_history_per_epoch`; `pipeline_vectors` (`tune_nr_*`, four vectors), `block_vectors` |
| V107-D29 | Legacy mode: extension writes are stored in bank 0 and read back, and the datapath uses the field resets (Table 9-195). | U `pipeline_vectors` (`tune_ignored_in_legacy`), `unit.profile` |
| V107-D30 | `TUNE_CAPS` (V107-A1) reports `0x00205DE2` after V107-5: RAW_INPUT, GAMMA_BANK, BPC_CAL, LSC_SECTORS, RESIDENT_OWNERSHIP, NR, WB_GB, DEMOSAIC, CNF_BLEND, CSC_LOOK. V107-2 and V107-4 had left their bits clear (found in V107-5). PROFILE waits for the statistics SEAL rules (V107-6), CSC_WIDE_RANGE for the CH2 inverse (V107-7). | U `test_register_engine` `test_discovery` |
| V107-A18 | LSC sectors (§6.8.3.9): node k is the sum of the first k programmed intervals; the cell selection and the UQ0.16 weight rule of the uniform mesh apply to each interval's span. | U `pipeline_vectors` (`tune_bpc_lsc_sectors`; `tune_lsc_sectors_as_uniform` equals `tune_lsc_uniform`, asserted by `gen_vectors.py`) |
| V107-A19 | The NR curve is evaluated per Haar group at L = (A + B + C + D + 2) >> 2 of the group's inputs (after the median stage). | U `pipeline_vectors` (`tune_nr_curve_bands_2frames`) |
| V107-A20 | Curve interpolation: k = min(L >> 4, 15); segment length 16, or 15 for the last one (knot 16 at L = 255); base = knot[k] + ⌊(f·d + len/2) / len⌋ with a floor division for negative slopes. | U `pipeline_vectors` (`tune_nr_curve_last_segment`: a bright scene with groups at L = 240..254) |
| V107-A21 | The Wiener and BayesShrink stages of a group use the variances of the window's centre group, also in the curve mode where neighbours have different L. | as V107-A19 |


### 10.6 V107-6 notes (2026-10-29)

Statistics move to the v1.07 engines and their result banks. DEC-29, DEC-30,
DEC-33, M3-A7 and M3-A8 (one in-place result memory, partial sums readable
during the frame) and the 07/10 per-CFA AEC results are superseded: a result
is visible only after its bank is READY and leased.

| ID | Decision or assumption | Test |
|---|---|---|
| V107-D31 | `TUNE_AEC_SOURCE` is live in legacy mode and joins the AEC COMMIT shadow set (Table 9-195), like the legacy AEC controls; in profile mode it is a bank field. V107-3 had classed it as legacy storage only. | U `test_stats` `test_aec_commit`, `test_aec_numeric` (both taps) |
| V107-D32 | AEC (§6.21): normalized RAW12 Bayer at the post-BPC (SOURCE 0) or post-DG (SOURCE 1) tap. Sum/count over samples inside [MIN_CLIP, MAX_CLIP]; OE (> TH_OE) and UE (< TH_UE) strict over every sample; min/max and the 64-bin histogram over every sample. Zone `zx = min(x / ZW, NX − 1)`. A set is legal when (NX−1)·ZW < W ≤ NX·ZW, (NY−1)·ZH < H ≤ NY·ZH, MIN ≤ MAX, UE < OE and, with the weighted histogram enabled, steps ≥ 1 and a ROI inside the image. An illegal set at the SOF skips the measurement and counts it in `TUNE_AEC_DROPS`. The 07/10 rule "the last zone absorbs the remainder" (CSR #302) is gone. The narrow 07/10 windows are modulo projections of the 64-bit lease results; `AEC_CHANNEL_SEL` 1..3 read 0. | U `test_aec_numeric`, `test_aec_commit`; `pipeline_vectors` (`stats_*`); I `e2e_tlm` |
| V107-D33 | Weighted histogram (§6.21.3.4, profile mode): the ROI lattice x = X0 + k·STEP_X (and y), the 5×5 weights with cell `cx = min(⌊5(x − X0)/(X1 − X0)⌋, 4)`; clip does not qualify it. | U `test_aec_weighted` |
| V107-D34 | AWB (§6.22): 2×2 post-BPC cells; a cell is rejected when any of its four samples is ≤ UNDER or ≥ SAT; G = (Gr + Gb + 1) >> 1. Zones over the cell raster `min(((i+1)·NX − 1) / Wc, NX − 1)`. Legal for 32..3840 × 32..2160 with NX ≤ min(64, W/2), NY ≤ min(32, H/2). Profile mode: cell ROI (half-open), inclusive R/G/B limits, then with COLOR_EN MIN_G and the ellipse union with rg = R(256R, G), bg = R(256B, G) (rejected above 65535) and A·dr² + 2B·dr·db + C·db² ≤ RADIUS2 · 2^14. | U `test_awb_cells`, `test_awb_*` (limits, quadratic form, indexed ellipse E5) |
| V107-D35 | AF (§6.23): Sobel magnitude minus NOISE, floored at 0. Mode 0: a uniform 4×4 grid with cut(d, k) = ⌊(k·d + 3)/4⌋. Mode 1: 16 half-open rectangles with WINDOW_MASK. Per window: focus, luma and count, 64-bit saturating; saturation sets TUNE_ERROR.OVERFLOW. | U `test_af` (W15 addressed by index) |
| V107-D36 | AE/AF result banks (§9.8, Table 9-198): two per engine, FREE → FILLING → READY → LEASED. `ACQUIRE`/`RELEASE` are W1P; a combined write releases first, then acquires. A second ACQUIRE or a RELEASE without a lease sets NO_LEASE; ACQUIRE with nothing READY is no lease and no error. A bank published on edge c is acquirable from c + 1; a bank released on edge c is admissible from c + 1, so an SOF on the release edge drops. Readout: INDEX, DATA_LO (snapshots the high word), DATA_HI; an INDEX write or a RELEASE invalidates the snapshot; a reserved index reads 0 and sets INVALID. Indices per Table 9-193. A bus read without a lease reads 0 and sets NO_LEASE. `TUNE_*_DROPS` is W1C (a nonzero write clears the word; a drop on the same edge wins). A TUNE_MODE change with any lease held (AWB, AE, AF) is rejected with MODE. Common and hard reset clear banks, leases and drops; the DMA reset keeps READY/LEASED and aborts only the measurement in flight. Legacy mode uses the same banks: AE/AF results need a lease in both modes. | U `test_aec_lease`, `test_readout_widths`, `test_resets`, `test_frame_id_abort_irq`; D `driver_tlm` (normal and fast CSR) |
| V107-D37 | `LEASE_VALID` rises with readability, one edge after ACQUIRE, as AWB's READ_VALID (U2, V107-A6). The first V107-6 build raised it on the ACQUIRE edge while the data words were readable only from the next edge: with CSR accesses shorter than a core clock, the driver confirmed the lease and read zero metadata. The driver now waits one clock after ACQUIRE (`stat_acquire`). | U `test_aec_lease` (LEASE_VALID 0 on the ACQUIRE edge, 1 on the next); D `driver_tlm_fast_csr` (metadata of every lease) |
| V107-D38 | SEAL (Table 9-196): the AEC relations of V107-D32 are checked (INVALID). The ellipse coefficients A and C are **unsigned** Q2.14 (CSR v1.07 rows 934–938), B signed Q1.14: V107-3 read A and C as signed, so A or C ≥ 2.0 was rejected as INVALID. | U `test_profile` `test_seal_validation` (A = 2.0, C = 3.99 seal; C = 0 and A·C = B² rejected), `test_stats` quadratic form |
| V107-D39 | Profile mode takes the AEC set from the active bank at every SOF. V107-3 left AEC inactive in profile mode, because COMMIT is rejected there (V107-D15) and nothing else transferred the set. | U `test_aec_weighted`, `test_aec_lease` (profile cases) |
| V107-D40 | `TUNE_CAPS` reports `0x0030FFFF`: V107-D30 plus PROFILE (the statistics SEAL rules complete the profile controller), AWB_COLOR, AF_WINDOWS, HIST_WEIGHT, AEC_LEASE, AF_LEASE, BAYER_AE, LEASE_GEOMETRY. | U `test_register_engine` `test_discovery` |
| V107-A22 | `TUNE_AWB_PROFILE` and `TUNE_AWB_FRAME_SIZE` follow AWB's readout rule (0 without a lease, no error), not the AE/AF NO_LEASE rule. | U `test_awb_cells` |
| V107-A23 | An AEC zone index beyond the captured grid (and the padding words 6, 7 of a zone) reads 0 without INVALID: Table 9-193 reserves 0x200 + 8z for every z < 768. | U `test_aec_lease` |
| V107-A24 | `AF_STATUS.SCORE_VALID` is 1 while a readable AF lease is held. | U `test_af` |
| V107-A25 | A DMA reset aborts the measurement in flight and frees its bank on the same edge (U2-A4 carried over to AE/AF). | U `test_resets` (abort, READY and drops kept); the same-edge reuse has no direct test |

Vector changes (`tools/gen_vectors.py`): `stats_flat_channels_64x48` uses a
legal 13×10 grid of 5×5 zones (short last column and row), and
`stats_zone_wrap_512x512` one 512×512 zone (the 16-bit narrow count wraps).
With the 07/10 grids both were illegal under v1.07 and only counted a drop.
`stats_reset_grids_af_edges_130x90` keeps the CSR reset AEC grid, which is
now illegal on that frame: it checks the drop. In
`stats_commit_sequence_4frames_64x48` the readout runs after the fourth frame,
so frame 3 (the commit of a new grid) finds both banks READY and drops; the
commit rule itself is checked by `test_aec_commit`.

#### Review V107-2..6 R1 (2026-10-29)

| ID | Decision or assumption | Test |
|---|---|---|
| V107-D41 | SEAL checks the EE radial row of Table 9-196 in the resident the bank's `EE_CTRL.PROFILE_SEL` selects (not the live PROFILE_WR_SEL): with RADIAL_EN = 1, CENTER_X2 ≤ 2(W−1), CENTER_Y2 ≤ 2(H−1), R2_TH0 ≤ R2_TH1 ≤ R2_TH2, else INVALID. CONFIRM checks no scalar range (§9.9.1). Missing since V107-3. | U `test_profile` `test_seal_ee_radial` |
| V107-D42 | The AWB ROI is checked against the complete-cell raster W/2 × H/2 (§6.22.3.9); AF windows and the weighted-histogram ROI against W × H pixels (§6.21.3.4). V107-3 used pixels for all three. | U `test_seal_rect_domains` |
| V107-D43 | `TUNE_BPC_COUNT` is a payload write (§9.9.5): every write with a byte lane, also of the stored value, clears BPC_CONFIRM; on a protected bank it is LOCKED and changes nothing. A write without lanes has no effect. Other bank fields keep the rule that an unchanged write is a no-op. | U `test_profile_tables`, `test_bpc_count_locked` |
| V107-D44 | `EE_STATUS.active_valid` clears when the active resident loses its confirmation (scalar or LUT write, failed confirm, input-domain change) and on common and hard reset (§9.9.1); it is set again only at the next local SOF that captures a confirmed resident. The frame in flight keeps its captured set. | U `test_ee_active_valid` |
| V107-D45 | A CSR bus read advances the control unit to the read's edge before its side effects run, so NO_LEASE / INVALID raised by the read carry that edge and win over a W1C on it (Table 8-180). Before, they carried the edge of the last write or hardware event. | U `test_stats` `test_read_error_edges` |
| V107-A26 | The EE radial SEAL rule applies whenever RADIAL_EN = 1 in the selected resident, also with EE_CTRL.EN = 0, like the other conditioned rows (AWB ROI, histogram), which do not depend on the block enable. | U `test_seal_ee_radial` ("EE disabled: still checked") |

### 10.7 V107-7 notes (2026-10-29)

Output geometry moves to the v1.07 per-channel contract (MAS §6.20, §6.24.7,
§8.3). V107-D2 (RESIZER_OUT_W/H reset 1920 × 1080), V107-A7 (codes 15..31
select FullRes) and the 07/10 FullRes fallback for an oversized or reserved
mode are superseded.

| ID | Decision or assumption | Test |
|---|---|---|
| V107-D46 | One geometry controller per channel (`control/channel_geometry.{h,cpp}`). Key: active size and input format, crop enable/origin/size, scaler enable and SCALE; the limit is the channel's (CHc_OUTPUT_LIMIT). Validation returns the CSR priority code, lowest wins: 1 input format / active size (outside 32 × 32 .. 3840 × 2160, or DEPTH 2/3), 2 crop empty or out of bounds, 3 odd crop, 4 reserved mode for the channel, 5 output above the channel limit, 6 output above the crop. GEOMETRY_STATUS: BUSY while the live key is not yet published (VALID and ERROR read 0), else VALID / ERROR. Readback (OUT_W/H, ACTIVE_CROP_*) is zero after reset. | U `test_control_unit` `test_channel_geometry`, `test_resizer_geometry`, `test_resizer_publication`; `pipeline_vectors` (`resizer_plans.txt`: 9504 sets, every code) |
| V107-D47 | RESIZER_CTRL.SCALE: 21 presets (Table 6-122), 15 and 22..31 reserved (ERROR 4); CH2 accepts 0, 8, 14 and 16..21 only. No silent FullRes fallback: a target larger than the crop is ERROR 6. | as V107-D46 |
| V107-D48 | Each decimation stage filters vertically, then horizontally, each pass rounding on its own (MAS §6.20.3.6: the order is normative). Audit 16/10 R1 fixed in C++ and Python. Stage count, bounded fallback and no-upscale reduction are unchanged. | `pipeline_vectors` (`resize_360p_1280x720` k = 1, `channels_ai_704x672` CH2 k = 1, 4K FHD k = 1); mutant M1; RAW (`full_vga_709` k = 2, `gtm_manual_qhd` k = 1) |
| V107-D49 | Each channel crops the post-IQ YUV444 active image before its Resizer; rows and columns outside the crop are consumed without output. LSC, EE radial and statistics coordinates stay in the full active frame. | `pipeline_vectors` (`crop_ch0_*`, `channels_ai_704x672`, `ch2_rgb_full709_crop_96x64`); e2e |
| V107-D50 | CH2 RGB888 (§6.15.3, Table 6-89, §6.24.7): the frame's `CSC_CTRL.STD` and `TUNE_CSC_CTRL.FULL_RANGE` select the Q16 set; one rounding R(n, 65536) per output, output clamp only; bytes R, G, B. | U `kernels` `test_rgb888` (literals); `pipeline_vectors` (`ch2_rgb_*`: 601/709 × full/limited) |
| V107-D51 | IDLE class adds RESIZER_CTRL of the three channels and every channel's CROP_CTRL/ORIGIN/SIZE (V107-D12): outside IDLE (input running, a frame in flight, a soft-reset window) the write is rejected with MODE. The 07/10 "mode written during a frame applies from the next one" behaviour (U4) is superseded. | U `test_channel_geometry`, `test_resizer_publication`; D `test_resizer_switch`; C (G-CONT stops and drains before them) |
| V107-D52 | Admission needs a VALID geometry for every channel enabled in OUTPUT_ENABLE: BUSY waits edge by edge, invalid waits for software. A frame below 32 × 32 active is therefore never admitted; V107-A9 (the VP processes such a geometry programmed directly) is superseded. The four edge vectors and `blc_profile2_bggr_64x32` (active 62 × 30) stay kernel-level vectors and are checked as refused over TLM and by the driver. | U `pipeline_vectors` (5 below the minimum, ERROR 1); I `e2e_tlm` (refused, no output, sticky INVALID); D `driver_tlm` (start refused) |
| V107-D53 | A frame retires when the pipeline and the ODMA are both done with it: with a crop the ODMA can complete before the shared IQ has consumed the rows below the crop (blocks still busy, statistics not yet published). Before V107-7 the output always ended last. | I `e2e_tlm` (crop vectors: statistics and idle blocks after retirement); mutant M4 |
| V107-D54 | Driver: `fx1_isp_start` polls CH0 GEOMETRY_STATUS until BUSY clears and returns FX1_ISP_EINVAL when it is not VALID (was a fixed 80-clock wait); new `fx1_isp_set_crop`, `fx1_isp_set_resize` (IDLE class, EBUSY while streaming; the crop is disabled while its words change, so no intermediate setup raises INVALID) and `fx1_isp_geometry_status`. | D `driver_tlm` (`test_resizer_waits`, refused starts) |
| V107-A27 | A pass-through or invalid key publishes on the next edge (no division); only a scaling key takes E0..E35. | U `test_channel_geometry`, `test_resizer_publication` |
| V107-A28 | An invalid set keeps the last valid readback (OUT_W/H, ACTIVE_CROP_*): MAS §8.3 "old derived readback may remain". | U `test_channel_geometry` |
| V107-A29 | SCALE is checked only with the scaler enabled; a disabled scaler passes the crop and ignores the code. | U `test_channel_geometry` |
| V107-A30 | The sticky TUNE_ERROR.INVALID is raised for channels enabled in OUTPUT_ENABLE (at the publication of an invalid set, and when OUTPUT_ENABLE enables a channel whose settled set is invalid). After reset CH2's reset set (1920 × 1080 FullRes) exceeds 640 × 640; raising INVALID for a disabled channel would flag every reset. ERROR is reported for every channel. | U `test_channel_geometry` |
| V107-A31 | Until V107-8 the integrated ODMA writes CH0 only; CH1/CH2 geometry is validated and gates admission, and their pixels are compared at kernel level (`run_channel`, `pipeline_vectors`). TUNE_CAPS CHANNEL_CROP and CSC_WIDE_RANGE are set with the CH1/CH2 outputs in V107-8. | `pipeline_vectors` |

Generator changes: `gen_vectors.py` (7 channel vectors, the geometry table);
`gen_sequence.py` writes only legal Resizer setups (the small frames fit no
preset: scaler disabled, or FullRes) and asserts a valid CH0 set before every
burst.

### 10.8 V107-8 notes (2026-10-29)

Three output channels to memory (MAS v1.07 §6.25.3.8, §9.3, §9.5.1). V107-A31
(CH0-only ODMA) is closed. U4-A2 (a frame wider than the stride checked at
arm is dropped) and DEC-20 (arming before ISP_EN is a configuration error)
are superseded by the claim rule.

| ID | Decision or assumption | Test |
|---|---|---|
| V107-D55 | Three ODMA engines: CH0/CH1 NV12 (two planes), CH2 one packed RGB888 plane (line 3W, bytes R, G, B). They share the ODMA AXI master; in this loosely-timed model each burst is one transaction, so channels interleave at burst boundaries. | I `outputs_tlm` (every mask), `e2e_tlm` (`channels_ai_704x672`, `ch2_rgb_*` in memory) |
| V107-D56 | Claim at the channel's output SOF: the head buffer if FREE; bases and strides from the registers, the geometry from the frame; Table 6-153 checked there (CH2: stride ≥ 3W, no chroma plane). A rejected claim sets ALIGN and CFG_ERROR, clears that FREE bit, does not advance the rotation and writes nothing; the frame waits (ODMA_OVERFLOW) until software frees a buffer again. | I `outputs_tlm` `test_ch2_stride`; D `test_resizer_switch` (narrow stride), `test_isp_en_trap` |
| V107-D57 | Distributor: the OUTPUT_ENABLE mask of the accepted SOF selects the channels; each has its own FIFOs and a stalled channel back-pressures the shared pipeline (lossless). A frame retires when the pipeline and every enabled output are done with it; COMMON_STATUS.FRAME_DONE and FRAME_DONE_IRQ rise at the retirement of a frame whose enabled outputs all published DONE (was CH0's DONE). | I `outputs_tlm` `test_masks`, `test_backpressure`; mutant N1 |
| V107-D58 | Each channel publishes with DONE: TAG, capture TS, SOF/EOF times (CH1 0x3300.., CH2 0x3500..), TUNE_CHc_BUFn_FRAME (ingress frame ID) and TUNE_CHc_BUFn_PROFILE (profile ID of the frame; 0 in legacy mode). Rotations are per channel, so one frame can land in different buffer indices with one ingress frame ID. | I `outputs_tlm` `test_rotations`, `test_metadata`; mutant N4 |
| V107-D59 | Output interrupt group: OUTPUT_IRQ_STATUS.DONE[c] on channel c's DONE, W1C per bit; o_irq is the OR of the pipeline, DMA and output groups after masking; a masked event still latches. ERROR[c] (recovery) lands in V107-9. DMA_IRQ_STAT.ODMA_DONE is raised for every channel's DONE. | I `outputs_tlm` `test_output_irq` |
| V107-D60 | DMA_ERR.ODMA_OVERFLOW is held while any channel is stalled for a buffer (sticky W1C, clearable once the stall is over); DMA_STAT.ODMA_BUSY while any channel writes; CH1/CH2_RESIZER_STATUS.busy with the frame's mask, like the pixel blocks (DEC-38). | I `outputs_tlm` `test_backpressure`, `test_metadata` |
| V107-D61 | `TUNE_CAPS` = `0x003BFFFF`: V107-D40 plus CHANNEL_CROP, CSC_WIDE_RANGE and FRAME_ID. RECOVERY_REPORT (V107-9) is the only bit of the full contract still clear. | U `test_register_engine` `test_discovery` |
| V107-A32 | The 16-entry frame-metadata queue of each output is not a modelled limit: at most four contexts are in flight (context credits), so it never fills. | review |
| V107-A33 | An enabled output that never received a line of a frame abandoned upstream counts as done for that frame's retirement (no DONE, no metadata). | I `e2e_tlm`, `driver_tlm` abort cases |
| V107-A34 | The driver's channel-buffer API is V107-10; until then the driver test bench arms the CH1/CH2 buffers with register writes. | D `driver_tlm` (`bench_channel_buffers`) |

### 10.9 V107-9 notes (2026-10-29)

Observable recovery and frame retirement (MAS v1.07 §9.9.8, Table 9-188,
CSR 0x8C00..0x8C2C). DEC-19 (AXI error: rotation kept, re-arm the same
index; ODMA error leaves the IDMA running), M2-A9 and U5-A2 (both soft
resets clear the published metadata) are superseded.

| ID | Decision or assumption | Test |
|---|---|---|
| V107-D62 | One recovery FSM for every request: COMMON_CTRL.soft_rst (common scope, cause 4), DMA_CTRL.soft_reset (DMA scope, cause 3), an AXI read or write error response (automatic DMA scope, cause 1 or 2). Accepted at the request edge: SEQUENCE + 1, BUSY = 1, SCOPE, DONE cleared, COMMON_IRQ_STATUS.ERROR_IRQ set; the abort set (≤ 4 admitted, unretired frames, admission order: ingress frame ID, enabled outputs without DONE, cause) is captured; admission is blocked until the final retirement. | U `test_control_unit` `test_recovery`; D `test_axi_read_error`, `test_axi_write_error` |
| V107-D63 | Drain: the engines issue no further address for aborted work; the drain ends when every AXI transaction already issued has its response (tracked per engine). The 15-clock pulse then applies the scope's register effects at its first edge (PHASE 2); final retirement is at pulse end. With nothing outstanding the drain is empty, so the pre-v1.07 timing of a soft reset is unchanged. | U `test_recovery`; I `driver_tlm` (slow memory drain) |
| V107-D64 | Final retirement (one edge): every IDMA_BUF_VALID and CHc_ODMA_BUF_FREE bit clear (already from the pulse, V107-A40), every rotation back to 0, records and COUNT published with DONE = 1 and BUSY = 0; OUTPUT_IRQ_STATUS.ERROR[c] for each channel some recorded frame failed; COMMON_STATUS.FRAME_DONE and FRAME_DONE_IRQ when COUNT > 0 ("retired at every output, including by recovery"). | U `test_recovery`; D `dma_tlm` error tests |
| V107-D65 | Scope effects: the DMA scope (and every automatic recovery) keeps DMA_ERR, DMA_IRQ_STAT, the *_ODMA_BUF_DONE bits and the published ODMA metadata (TAG, TS, SOF/EOF, TUNE_CHc_BUFn_FRAME/PROFILE) of completed buffers; the common scope clears them. Supersedes U5-A2 and the DMA-reset clearing of DMA_IRQ_STAT / DMA_ERR. | D `test_metadata_resets`, `test_soft_reset_mid_frame` |
| V107-D66 | While BUSY, a software request is rejected with TUNE_ERROR.PENDING (no new SEQUENCE, the running pulse is not restarted; also a common request during a DMA recovery). An AXI error during a recovery sets its sticky DMA_ERR bit and joins it (no new record). Only i_rst_n escalates. | U `test_recovery` |
| V107-D67 | COMMON_RECOVERY_STATUS, SEQUENCE and the four ABORTi records survive both soft resets; i_rst_n clears them. They are evaluated at the edge of the read: the records of a recovery become visible with its DONE, and BUSY / PHASE / SCOPE describe the running one. | U `test_recovery` |
| V107-D68 | Reference driver: the queue is settled against COMMON_RECOVERY_SEQUENCE (read before BUSY) in every call that looks at it. After a recovery the DONE prefix of the queue stays collectable and every other queued frame is lost (`fx1_isp_next_lost`); both rotations restart at 0 and an output still held by an uncollected frame blocks queueing until it is collected. `fx1_isp_queue_frame` returns FX1_ISP_EAGAIN when a recovery restarted the rotation after the caller filled `fx1_isp_next_input()` (race found by the fault campaign). `fx1_isp_soft_reset` / `fx1_isp_dma_reset` wait (bounded, 4096 × 16 clocks) for a running recovery and for their own, and repeat a request rejected with PENDING once; `fx1_isp_dma_reset` clears the kept DONE bits of the frames it discards. New `fx1_isp_recovery_report` (stable SEQUENCE around the records). | I `driver_tlm` (error cases, `test_immediate_loss`, fault campaign) |
| V107-D69 | `TUNE_CAPS` = `0x003FFFFF`: RECOVERY_REPORT set; every bit of the v1.07 contract is now implemented. | U `test_register_engine` `test_discovery` |
| V107-D70 | G-CONT: an AXI-error burst behaves like soft_reset_dma: the first frame has its SOF only and is the abort set, the later frames are not admitted, software resubmits the whole burst from slot 0. The sequence is regenerated. | I `cont_tlm` |
| V107-A35 | Admission is the accepted SOF at the pipeline (§9.7). A frame the IDMA is reading ahead (2-line DMA FIFO) whose SOF has not been accepted is not in the abort set: it is lost without a record and consumes no ingress frame ID. | I `driver_tlm` "driver idma" |
| V107-A36 | Cause 5 (malformed framing at the Input Formatter) is not reachable in the memory-to-memory build: the IDMA always produces well-formed framing. The encoding is implemented; no debug hook injects it. | review |
| V107-A37 | PHASE 3 (final retirement) lasts one edge: a read sees PHASE 2 up to the final edge and DONE from it. | U `test_recovery` |
| V107-A38 | `fx1_isp_recovery.idma_rearmed` / `idma_input` / `idma_seq` are kept for source compatibility and always 0; `aborted` and `sequence` are new. Firmware callers (`fx1_camera.c`, `tests/recovery/isp_recovery.c`) are adapted in V107-10. | I `driver_tlm` |
| V107-A40 | (Superseded by V107-D80: cleared at the final retirement, as MAS §9.9.8.) The VALID / FREE bits are cleared at the first edge of the reset pulse, with the scope's other effects, not at the final retirement 15 clocks later; the command block drops W1S writes from the request to the final edge, so they stay clear and a read during the pulse returns 0. The explicit clear at final retirement was redundant and is removed (mutant R1 survived; a cycle-by-cycle probe of both scopes with drains of 0/5/10 clocks and re-arms during drain and pulse gave identical traces). | U `test_recovery`; probe `output/v107_9/r1_probe` |
| V107-A39 | The CH0 driver keeps a frame whose CH0 output is DONE even if a CH1/CH2 output of it failed; the recovery report records that failure. Per-channel handling comes with the channel-buffer API (V107-10, V107-A34). | review |

### 10.10 V107-10 notes (2026-10-29)

Driver APIs, tools and firmware migration. V107-A34 (no channel-buffer API)
is closed; V107-A39 is superseded by V107-D72 (a frame is kept only when
every enabled channel is DONE).

| ID | Decision or assumption | Test |
|---|---|---|
| V107-D71 | Discovery: `fx1_isp_discover` (TUNE_ID, TUNE_REV, TUNE_CAPS) and `fx1_isp_require_caps`, which returns the new code `FX1_ISP_ENOTSUP` when a required capability bit is clear. | I `driver_tlm` `test_v107_api` |
| V107-D72 | Channel buffers: `fx1_isp_set_channel_buffers` (CH1 NV12, CH2 RGB888) in CONFIG state; `fx1_isp_start` latches OUTPUT_ENABLE and refuses an enabled channel without buffers, with an invalid geometry or a stride shorter than a line (CH2 3W). One buffer index per frame on every enabled channel; `next_done` when all enabled channels are DONE, acknowledging all; after a recovery the driver clears the DONE a channel kept for an aborted frame. `fx1_isp_output_meta` reads a channel's metadata, ingress frame ID and profile ID. | I `driver_tlm` `test_v107_api` (3 channels, CH2 write error), every CH1/CH2 vector |
| V107-D73 | Profile transaction: `fx1_isp_profile_mode/begin/confirm_tables/seal/apply/cancel/unseal/state`. A command's error is the set of TUNE_ERROR bits that rose across it (bits already set are not blamed on it); they are acknowledged and returned with FX1_ISP_EIO. | I `driver_tlm` `test_v107_api` (MODE, NOT_READY, LOCKED, activation, profile ID in the output) |
| V107-D74 | `fx1_isp_events.output`: `fx1_isp_irq` also reads and acknowledges OUTPUT_IRQ_STATUS. | I `driver_tlm` (state machine) |
| V107-D75 | Per-frame statistics: `fx1_isp_run_raw --stats-per-frame` runs one readout section per frame ("FRAME k") once the frame is done and the pipeline, AEC and AF are idle; it needs queue 1. `run_regression.py` builds the reference readout after every frame and runs sequences at queue 1; `--stats-at-end` keeps the pre-V107-10 behaviour (one readout after the last frame, sequences 4 deep). The statistics SHA of every RAW run changes; NV12 does not. | RAW regression (attribution V107-10) |
| V107-D76 | `make_profile` preset `gtm_manual_qhd`: legal grids (AEC 8×6 with zones covering the active image, AWB 16×8) instead of the illegal ones; illegal and reset grids stay covered by `stats_illegal_grids_64x32` and `stats_reset_grids_af_edges_130x90`. | RAW regression (no drop) |
| V107-D77 | Firmware port (owners' code): `tests/isp_stream/frame_id.c`, `gen_fixture.py` and the regenerated `isp_fixture.h` (AEC aggregate under leases; the AWB ownership rows change with the 2×2 AWB cells of V107-D34, pixel CRCs unchanged), `freertos/cmd_isp.c` (AEC printer), `tests/recovery/isp_recovery.c` and `tests/camera_isp/faults.c` case 6 (recovery semantics), `drivers/fx1_camera.c/.h` (comments, EAGAIN counted as a loss), `tests/isp_stream/isp_mas0710.c` phase C (stop/start between Resizer modes). Cross-compiled with `riscv-none-elf-gcc` and the firmware flags (28 units, `output/v107_10/fw_compile.sh`); not run on the platform until V107-11. | compile check |
| V107-A41 | A command's TUNE_ERROR attribution (V107-D73) also catches an asynchronous error (MISSED at an SOF) that rises during the command; the driver reports it with that command. | review |
| V107-A42 | The per-frame readout waits for COMMON_STATUS.BUSY, AEC and AF BUSY to clear and 1 µs more; the model publishes at the frame end (U2-A7), so the result is there by then. | RAW regression |
| V107-A43 | (Closed by V107-D86: camera tests at 64×32 RGGB.) **Open.** The camera tests (`tests/camera_isp`, FreeRTOS `camera` CLI) use a 64×16 sensor frame, below the 32×32 active minimum of v1.07 (V107-7): the ISP never admits it, and their golden (`camera_isp_golden.h`) also predates the full-range CSC of V107-5. They need a 64×32 (or larger) sensor mode across FW, CMake regexes, CLI scripts and docs, and a regenerated golden. Not changed in V107-10. | — |

### 10.11 Review V107-7..10 R1 (2026-10-29)

Review `report/ISP_V107_7_10_REVIEW_2026-10-29.md` (R1..R8). V107-A40 and
DEC-15 are superseded; V107-A43 is closed.

| ID | Decision or assumption | Test |
|---|---|---|
| V107-D78 | R1: an accepted AXI transaction is outstanding until its response, whether the target blocks inside `b_transport` or annotates the delay. The drain probe answers "unknown" while any is outstanding; the engine that receives the last response reports its edge (`recovery_drained`) and the pulse starts there. No drain end is inferred from a timestamp. | D `test_drain_blocking_target` (IDMA / ODMA × DMA / common scope: PHASE drain while the target holds a transfer, every access ended before BUSY = 0, no memory write after DONE) |
| V107-D79 | R3: every accepted transaction's response is received, also for aborted work; an error response sets DMA_ERR.AXI_RD/WR and the interrupt and joins the running recovery (no new SEQUENCE, the captured cause kept); only then is the aborted work abandoned. | D `test_error_response_in_drain` (read, write) |
| V107-D80 | R8: VALID / FREE are kept through drain and pulse and cleared at the final retirement, together with the records, DONE and BUSY = 0 (MAS §9.9.8). The command block drops W1S / W1C writes from the request to the final edge. | U `test_soft_reset` (VALID kept to the last pulse edge, cleared at the final edge, W1C held off), I `test_soft_reset_window` |
| V107-D81 | R2: the driver collects nothing while a recovery runs or is unsettled (ENODATA). If a recovery starts between `next_done`'s checks and its DONE acknowledgement, the dropped W1C is detected, the frame stays queued and FX1_ISP_EAGAIN is returned; it is collected once settled. A lost frame whose DONE cannot be acknowledged marks its buffer (`pending_ack`) and the buffer is not armed again until acknowledged. | I `driver_tlm` `test_recovery_bookkeeping` (a), (b) |
| V107-D82 | R6: `fx1_isp_soft_reset_ex` (new), `fx1_isp_dma_reset`, `fx1_isp_stop` and `fx1_isp_init` return FX1_ISP_EBUSY when the reset outlasts the bounded wait; the queue, leases and state are unchanged and the accepted request stays pending (`reset_pending`): the next call for it, or any settling call, completes it without a second request. | I `driver_tlm` `test_recovery_bookkeeping` (c) (1 ms blocking target, `dma_reset` and `stop`) |
| V107-D83 | R4: each profile command runs in a clean TUNE_ERROR window: bits left by earlier accesses are acknowledged first (kept in `tune_error_prior`); FX1_ISP_EBUSY if they do not clear. A sticky MODE no longer hides the same rejection. | I `driver_tlm` `test_v107_api` (sticky MODE, repeated APPLY) |
| V107-D84 | R7: COMMON_STATUS.ERROR / ERROR_IRQ per MAS v1.07 Table 9-188: DMA_ERR.ALIGN/AXI_RD/AXI_WR, any LSC_ERROR bit, EE_STATUS.ERROR, any TUNE_ERROR bit, and every accepted recovery; not IDMA_UNDERRUN, ODMA_OVERFLOW or BPC_STATUS.cand_reject_ovf (now classed storage: no source in the VP). `run_raw` logs status words instead of result words read without a lease. | U `test_error_aggregation` (each source, masking, pin, W1C, flow control, command MODE, NO_LEASE read), I `test_irq_pin`, D `test_underrun` |
| V107-D85 | R5: `fx1_isp_profile_load_gamma` loads a curve into a bank through the banked port after INIT (which clears payload validity), verifies it and confirms it. `isp selftest profile` loads the basic fixture's curve after INIT; replayed on the model with the firmware fixture (8 CRCs and profile ID 1). | I `driver_tlm` `test_v107_api` (Gamma vector: NOT_READY, confirm alone fails, load, expected output), `test_fw_profile_selftest_replay` |
| V107-D86 | A43: the camera tests and the FreeRTOS `camera` default mode use 64×32 RGGB (the v1.07 minimum active height); `camera_isp_golden.h` regenerated (also picks up the v1.07 basic profile), `cli_camera_mismatch.raw` resized to one 64×32 frame, CMake regexes, CLI scripts and docs updated. The CSI-2-only smokes (`tests/camera/raw.c`) keep 64×16. | `gen_golden.py --check`; platform CI (V107-11) |
| V107-A44 | The FW/SDK default profile mode (`isp run` in profile mode with a legacy comparison path and a job output selection) is not done in this round; it needs a new demo job version and the per-bank payload handling of every job profile. Required before qualification. | — |

### 10.12 Re-review V107-7..10 R2 (2026-10-29)

Re-review `report/ISP_V107_7_10_REREVIEW_2026-10-29.md` (R9, R10).

| ID | Decision or assumption | Test |
|---|---|---|
| V107-D87 | R9: a hard reset wins over every transaction accepted before it. Each `i_rst_n` assertion starts a new generation; a response of an earlier generation still ends its transaction's lifetime (outstanding count) but sets no DMA_ERR, no interrupt and starts no recovery. The error-in-drain rule of a software recovery (V107-D79) is unchanged. | D `test_error_response_after_hard_reset` (read / write × annotated / blocking × response after the release / while the reset is held: DMA_ERR AXI = 0, SEQUENCE = 0); `test_error_response_in_drain` kept |
| V107-D88 | R10 (firmware camera manager, owners' code): `fx1_camera_stop` keeps the CSI-2 sides, the ready queue and the state when `fx1_isp_stop` returns FX1_ISP_EBUSY and returns the new FX1_CAMERA_EBUSY; the next stop retries only the ISP stop (sensor standby and CSI-2 stop are not repeated, `stop_cancelled` counted once). `fx1_camera_poll` does nothing and `fx1_camera_start` is refused while that stop is pending. | probe `output/v107_7_10_r2/build_camera_stop_probe.py` (verbatim function, mocked dependencies; the pre-fix body fails it) |
| V107-D89 | A pending driver reset completed by a settling call (V107-D82) makes the retried reset call return FX1_ISP_OK without a second request (`reset_completed`, cleared by the next queued frame). Frames already dropped by that completion are not counted again by the retry. | I `driver_tlm` `test_recovery_bookkeeping` (c, settled in between: SEQUENCE + 1 only) |

### 10.13 V107-10 completion: firmware default profile mode (2026-10-29)

V107-A44 is closed. Review follow-up of V107-7..10: test checks of
FRAME_ID words read without a lease replaced.

| ID | Decision or assumption | Test |
|---|---|---|
| V107-D90 | `fx1_isp_profile_from_legacy`: the default profile-mode bring-up of a legacy profile (INIT of the bank from the applied configuration; the Gamma curve reloaded through the banked port; GTM manual curve and TUNE payload writes replayed in order; SEAL with the ID; APPLY). Any failure returns its TUNE_ERROR bits; no fallback. | I `driver_tlm`: every vector with one profile and no TUNE writes, in profile mode (31): every frame and statistic equal to legacy, profile ID on the outputs and leases; 2 illegal-grid vectors refused at SEAL (INVALID) |
| V107-D91 | Demo job version 2 (`fw/fx1_soc/demo/isp_raw_frame/isp_demo_job.h`, 96 bytes): mode (0 profile, default; 1 legacy), output mask (CH0 required), profile ID, CH1/CH2 buffers; result version 2 (mode, outputs, active profile ID, TUNE_ERROR, CH1/CH2 geometry; status 2 = profile refused). A version-1 job runs in legacy mode, CH0 only. Shared bring-up `isp_demo_setup.h` for the bare-metal demo and `isp run`. | host `platforms/fx1_soc/demo/tests/test_pack.py` (23 cases); FW compile |
| V107-D92 | FreeRTOS CLI: `isp run` in the job's mode (profile by default); `isp run legacy` for comparison; `isp run auto` stays in legacy mode (its policy rewrites WB/GTM between frames) and says so. A profile-mode run fails unless the outputs carry the job's profile ID. | FW compile; platform CI (V107-11) |
| V107-D93 | Host `run_isp_demo.py`: `--mode profile|legacy` (default profile), `--outputs MASK`; CH1/CH2 planes dumped, packed and reported; CH0 compared with the stand-alone model's legacy run in both modes (the legacy comparison path); the result's mode, outputs and profile ID checked. | `test_pack.py` |
| V107-D94 | `fx1_isp_run_raw --profile-mode` (the same bring-up over CSR writes) and `run_regression.py --profile-mode`: the RAW inventory in profile mode against the legacy reference. | RAW regression in profile mode (V107-10 completion report) |
| V107-D95 | Tests no longer read result words without a lease as evidence: the ingress counter (`TUNE_NEXT_FRAME`) shows the frame identity kept across soft resets (`cont_tlm`) and a DMA reset (`driver_tlm`); `e2e_tlm` keeps one deliberate leaseless read, named as such. | I `cont_tlm`, `driver_tlm`, `e2e_tlm` |
| V107-A45 | A legacy AEC COMMIT issued before entering profile mode stays pending (AEC_CTRL.COMMIT reads 1): profile mode takes the AEC set from the bank at every SOF and never consumes it (MAS v1.07 §9.7 rejects COMMIT in profile mode but says nothing about one already pending). Harmless to the frames; the profile-mode RAW regression expects the bit. Owner question: should entering profile mode cancel it? | RAW regression in profile mode |

### 10.14 Review of the V107-10 completion, R1 (2026-10-29)

Review `report/ISP_V107_10_COMPLETION_REVIEW_2026-10-29.md` (R1..R3, P2).

| ID | Decision or assumption | Test |
|---|---|---|
| V107-D96 | R1 (refines V107-D90): the legacy Gamma curve is carried into the profile bank as the hardware holds it: when GAMMA_STATUS.LUT_LOADED is set it is read back through GAMMA_LUT_ADDR / GAMMA_LUT_RDATA before INIT (any segment order, rewrites, or a load by `fx1_isp_load_gamma`), written through the banked port and confirmed. An unconfirmed legacy curve is not carried (legacy mode does not apply it either); with Gamma enabled SEAL then reports NOT_READY, so the conversion fails explicitly instead of producing a different picture. GTM manual and TUNE payloads keep being replayed through their own ports, whose addressing reproduces the legacy sequence. `fx1_isp_run_raw --profile-mode` does the same. | I `driver_tlm` `test_profile_gamma_order` (128..255 then 0..127; and with an entry rewritten after the curve was complete: profile output = legacy output ≠ the vector's); reviewer's probe: confirmed variant legacy = profile, unconfirmed variant refused (NOT_READY) |
| V107-D97 | R2 / R3 (refines V107-D93): `isp_pack.check_identity` (mode — the job's, or `--expect-mode` when the console forced one —, output mask, profile ID with 0 read as 1) and `isp_pack.pack_channels` (CH1 / CH2 geometry, strides and dump lengths before anything is written; `platform_ch*` removed on failure) are shared by `isp_pack.py DIR` (the SDK entrypoint, `run_vp.sh`) and `run_isp_demo.py`. `isp_auto.py prepare` accepts job versions 1 and 2 and marks a version-2 auto job legacy (`isp run auto` runs legacy), so the packer expects that mode. | host `test_pack.py` (42 cases: CH1 / CH2 negatives, the SDK entrypoint's identity cases, `--expect-mode`, `isp_auto` v1 / v2); reviewer's host probe |

### 10.15 V107-11 qualification fixes (2026-10-30)

Found by the first full platform CI on CSR v1.07 and by the full mutation
campaign; evidence in `output/v107_11_night` and `output/v107_11_fix` (work
area). Platform CI after the fixes: 341 / 341.

| ID | Decision or assumption | Test |
|---|---|---|
| V107-D98 | `fx1_isp_apply` returns `int`. In legacy mode a write list that loads the Gamma curve (LUT data or LUT_LOAD_CONFIRM), the GTM manual curve or an EE resident (LUT data or PROFILE_LOAD_CONFIRM) is replayed with that block's EN cleared first: the legacy confirmation needs EN = 0 sampled before the write (V107-A15) and GTM manual writes are ignored while EN = 1, so a profile applied over an earlier configuration (camera reconfigure, second run) lost Gamma / EE or kept the old GTM curve. The list's own CTRL writes enable the block again. After the replay, `FX1_ISP_EIO` when a Gamma confirmation left GAMMA_STATUS.LUT_LOADED clear or an EE resident it confirmed is not LUT_VALID (a refused confirmation raises no error). Profile mode: replayed as is. `(NULL, 0)` is OK, `(NULL, n)` EINVAL. | `driver_tlm` `test_profile_reapply` (Gamma, EE, GTM vectors: legacy twice and profile mode on one device, each equal to the expected output; 255 Gamma entries + confirm and an EE confirm without a load give EIO); mutants W1–W4 |
| V107-D99 | Rotation generation: `fx1_isp_dev.rotation_gen` advances whenever the rotation restarts at 0 (`fx1_isp_start`, and the settle of an automatic recovery, which any driver call may run). `fx1_isp_next_input` records it (no longer `const`); `fx1_isp_queue_frame*` returns `FX1_ISP_EAGAIN` when the slot read before has become stale, so the caller refills the input `fx1_isp_next_input` now names and queues again. Before, a recovery settled by another call between the two (e.g. `fx1_isp_input_ready` in a pair loop) made the frame use the wrong buffer. | `driver_tlm` `test_queue_after_foreign_settle` (slot read through `fx1_isp_next_input` and settled by another call; slot tracked by the caller and settled inside the queue, which only the slot check catches); platform `isp_recovery` A / B; mutants W5, W6, R4 (re-anchored) |
| V107-D100 | The abort-in-flight phases of the platform tests (`isp_recovery` C/D/E, `isp_mas0710` A/B) use 1-beat ISP bursts (`ISP_ABORT_MAX_BURST_BEATS`, was 2). The v1.07 driver reads RECOVERY_STATUS, RECOVERY_SEQUENCE and TUNE_ERROR before each reset request (rejected-request detection), about 20 us on the cacheless VP hart, and the 64x48 input was fully read before the reset landed (bus trace). With 1 beat the soft reset lands after 3952 of 6144 input bytes. Not a functional change: the burst length changes no data and no completion order. | platform `fx1_smoke_isp_recovery*`, `fx1_smoke_isp_mas0710*` |
| V107-D101 | Camera artifact test (`fw/fx1_soc/tests/camera_isp/artifacts.c`): the dump layout is derived from the frame size (it overlapped at 64x32, V107-A43), dump 0x20000 bytes (`check_artifacts.py` follows), and the frame period doubled for that test: the CPU copies of the 64x32 planes outlast one 3 ms frame and dropped a frame in RAW10. The 4K TPG golden of the `mixed_crc` CLI regex is the post-V107-5 CRC `0x1420e85c`. | platform `fx1_regression_camera_isp_artifacts_raw10/raw12`, `fx1_regression_freertos_cli_camera_mixed_crc` |
| V107-A46 | (Closed by V107-D102.) The sample profiles `samples/profiles/*.csrw` (and the SDK shared profile built from them) predate v1.07: they enable Gamma / EE without the v1.07 legacy confirmation, so under v1.07 those blocks run bypassed. | — |
| V107-D102 | A46 (user, 2026-10-30): the sample profiles are regenerated. The four `outdoor1` presets are `make_profile.profile_writes` of the dataset frame loaded as `run_regression.py` loads it (format `le16_raw10_msb`); they equal the RAW regression's v1.07 profiles of that frame write for write. Against the old files: the Gamma and EE confirmations, and for `gtm_manual_qhd` the legal AEC grid (V107-D76) and its AWB_CTRL. `shared_bggr_2688x1520_full` is the new `full` with the 16 tuned values of the old shared profile kept (BLC, WB, CCM, EE_ALPHA, GTM key / white), i.e. only the two confirmations added. Probe on the SDK D65 input (`fx1_isp_run_raw`, 2 frames): the old shared profile gives the same NV12 as the new one with Gamma and EE disabled, the new one differs. SDK goldens are computed at packaging time (`package_sdk.sh`), none is checked in. | write-for-write equality with the RAW regression profiles; probe (`output/v107_11_profiles`); host `test_pack.py` |
