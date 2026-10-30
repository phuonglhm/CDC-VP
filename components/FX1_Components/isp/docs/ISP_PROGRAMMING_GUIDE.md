# FX1 ISP — Programming guide

For firmware and driver engineers who program the FTEL ISP (HAS v1.0,
CSR v1.0) through its register map. The guide describes:
- the bring-up order;
- the buffer and interrupt protocol;
- configuration timing;
- statistics readout;
- error recovery.

Each step names the function of the reference driver (`driver/`,
`fx1_isp/fx1_isp_drv.h`) that implements it. CTest
`fx1_isp.integration.driver_tlm` runs that driver on the VP over every
committed vector and the error cases of §8.

**Status of the information.** The behaviour described here is that of the
VP model, which follows the HAS/CSR and the project decisions in
[ISP_DECISIONS_AND_DISCREPANCIES.md](ISP_DECISIONS_AND_DISCREPANCIES.md)
(DEC-xx). Where the specification was silent, the model's choice is marked
**[assumption]**. The IP owner has not confirmed these choices (DEC-05).
Open points are listed in `plan/alg/OWNER_QUERIES.md`. A driver should keep
each marked behaviour easy to change.

## 1. Register access

- All offsets are relative to the ISP base, which the platform assigns.
  The header is `include/fx1_isp/fx1_isp_csr.h`, generated from the CSR
  map: `FX1_ISP_<REG>_OFFSET`, plus `_<FIELD>_MASK/_SHIFT/_RESET` and
  `_<NAME>_BIT` for each field.
- Accesses are 32 bits wide; address bits [1:0] are ignored. Byte strobes
  are honoured, and sub-word accesses must stay within one word
  (CSR-23, [ISP_CSR_CONTRACT.md](ISP_CSR_CONTRACT.md)).
- Access types:
  - **RW**: configuration.
  - **RO**: status and results; writes are ignored.
  - **W1C**: write 1 to clear a bit. If hardware sets the bit in the same
    cycle, the hardware set wins.
  - **W1S**: write 1 to request; the bit reads 1 while the request is
    pending.
  - **W1SC**: a pulse; it always reads 0.
- Reserved bits read 0. Write them as 0.
- Writing a whole register writes every RW field. To change one field,
  read-modify-write; the driver does this where it matters, for example
  for `EE_LUT_CTRL` and `GTM_CTRL`.

## 2. Bring-up (HAS §9.1 order)

| Step | What | Driver |
|---|---|---|
| 1 | Release `i_rst_n`. Every register takes its reset value. LUT and mesh contents are undefined (MAS §6.5.2): the VP fills them with a deterministic poison pattern, so load every table before enabling its block (Gamma, EE, GTM manual curve, LSC). | — |
| 2 | Optional soft reset, to start from a known engine state (§7). | `fx1_isp_init` |
| 3 | Input geometry `COMMON_FRAME_WIDTH/HEIGHT` and Bayer order `COMMON_BAYER` (0 RGGB, 1 GRBG, 2 GBRG, 3 BGGR, DEC-11). W and H are the even external dimensions before the phase trim, up to 3842 × 2162 (the DMA checks only this). The active image `((W − SH) & ~1, (H − SV) & ~1)` must be 32 × 32..3840 × 2160; software validates it (MAS v1.07 §6.6.3.4–6.6.3.5). Native input depth `COMMON_INPUT_FORMAT.DEPTH`: 0 RAW12 (reset), 1 RAW10. The containers are right-aligned and zero-extended; the ISP shifts RAW10 left two exactly once. Before queuing, check the unused upper bits of every frame (`fx1_isp_raw_upper_bits`). Change all of these only while the pipeline is idle: with the input DMA stopped (or `ISP_EN` clear) and nothing in flight (MAS v1.07 §9.9.3). Otherwise the hardware ignores the write and sets `TUNE_ERROR.MODE`; the driver then returns `FX1_ISP_EBUSY`. | `fx1_isp_set_geometry`, `fx1_isp_set_input_format`, `fx1_isp_raw_upper_bits` |
| 4 | Profile: block enables and parameters, LUTs (§4), the LSC profile, statistics configuration. | `fx1_isp_apply`, §4 helpers |
| 5 | `COMMON_CTRL.isp_en` = 1, before the DMA (HAS §9.1). Setting `ODMA_BUF_FREE` captures nothing (MAS v1.07 §6.25.3.8): each output channel claims its head buffer at the channel's output SOF, with the frame's geometry and the current bases/strides, so a buffer freed before `isp_en` or before a geometry change is fine. | `fx1_isp_start` |
| 6 | Channel geometry (MAS v1.07 §6.20.1.1, §8.3): with the input stopped, program each enabled channel's crop (`CHc_CROP_*`: even, inside the active image; disabled = full image) and Resizer (`RESIZER_CTRL` / `CH1_` / `CH2_RESIZER_CTRL`: SCALE preset or FullRes; scaler disabled passes the crop). Then poll `CHc_GEOMETRY_STATUS` until BUSY clears (a scaling key derives in 35 clocks; a key changed meanwhile is derived after it) and require VALID; ERROR gives the reason (1 active size/format, 2 crop bounds, 3 odd crop, 4 reserved mode, 5 above the channel limit, 6 above the crop). Read `RESIZER_OUT_W/H` only then. An invalid enabled channel admits no frame and sets the sticky `TUNE_ERROR.INVALID`. Example: BGGR 2688×1520 FullRes gives 2686×1518. | `fx1_isp_set_crop`, `fx1_isp_set_resize`, `fx1_isp_geometry_status`; `fx1_isp_start` (polls CH0, `EINVAL` if not VALID; `dev.out_width/height`) |
| 7 | Strides, the 4 input and 4 output buffer addresses (64-bit register pairs; 40 address bits are used), interrupt enables. | `fx1_isp_start` |
| 8 | Queue the first frame(s), **then** enable the engines with `DMA_CTRL` (`idma_en`, `odma_en`, `max_burst_m1`). Enabling first makes the idle IDMA report an underrun before the stream starts (M4-R5). | `fx1_isp_queue_frame` (enables on the first call) |

Memory rules. The ISP refuses to start an engine that breaks these rules,
and reports `DMA_ERR.ALIGN` and `DMA_IRQ_STAT.CFG_ERROR` (MAS Table 161-162) without any memory access:
- buffer bases and strides are multiples of the AXI beat (16 bytes at the
  128-bit build, `PARA_AXI_DATA_WIDTH`);
- the input stride is at least 2 × W;
- the Y and UV strides are at least the output width.

Input format (HAS Table 6-88): one little-endian 16-bit container per
sample. RAW10 goes in bits [11:2] and RAW12 in bits [11:0]; bits [15:12]
are 0. The ISP does not shift the data. `tools/raw_fixture.py` converts a
`[15:6]`-aligned sensor dump (`>> 4`).

Output: NV12, with the Y plane then interleaved Cb/Cr at half height. Only
the active width of each line is written; the stride padding is left as it
was.

## 3. Buffers, frames and interrupts

The four input buffers and four output buffers rotate independently, in
index order 0, 1, 2, 3, 0, …, starting from 0 after any reset (M2-A1):
- **Input:** software sets `IDMA_BUF_VALID[k]` when buffer k is filled.
  The IDMA reads buffers in order, clears the bit when it has finished
  reading, and raises `IRQ_IDMA_DONE`.
- **Output:** software sets `ODMA_BUF_FREE[k]`. The ODMA writes into the
  next buffer in order and clears FREE when it starts the frame there (MAS
  Table 159). When the last write of both planes has completed, it sets
  `ODMA_BUF_DONE[k]`, and raises `IRQ_ODMA_DONE` and
  `COMMON_IRQ_STATUS.frame_done_irq`. A frame abandoned upstream gives its
  buffer back (FREE set again, U5-A3).
- **Recycle:** software consumes buffer k, writes 1 to `ODMA_BUF_DONE[k]`
  (the bit is not cleared by hardware), and may set FREE again.

Check `fx1_isp_input_ready()` **before writing input memory**, including after
an ODMA error: a freed output slot does not imply IDMA has released the next
input buffer. Serialise checking, filling and queuing against other driver calls.
If it returns 0, retry from the software worker after IDMA makes progress.
`fx1_isp_queue_frame` also checks ownership and returns `FX1_ISP_EBUSY` without
changing the ring when IDMA still owns that input.

The driver keeps the software view of both rotations.
`fx1_isp_queue_frame` hands over one input and one output buffer, and
`fx1_isp_next_done` pops completed outputs in order. Up to 4 frames can be
in flight, and frames queued back to back follow each other with no gap.

Interrupts. `o_irq` is a level: the OR of `COMMON_IRQ_STATUS & COMMON_IRQ_EN`
and `DMA_IRQ_STAT & DMA_IRQ_EN`. Status bits latch even while masked. In the
handler, read both status registers, write back the value read (W1C) and
act on the bits; `fx1_isp_irq` does this. A source that fires again during
the handler sets its bit again, so the line stays asserted.

| Source | Meaning |
|---|---|
| `frame_done_irq` | A frame is in memory (both planes written). |
| `stats_ready_irq` | AEC, AWB or AF published results. It is raised for each publication, even if the block's done bit was not cleared (M3-A4). |
| `error_irq` | Rising edge of `DMA_ERR.ALIGN/AXI_RD/AXI_WR`, any `LSC_ERROR` bit, `EE_STATUS.error`, any `TUNE_ERROR` bit, and every accepted recovery (MAS v1.07 Table 9-188). Not underrun, overflow or statistics drops. Debug code must not read result words without a lease: that sets `TUNE_ERROR.NO_LEASE` and so `error_irq`. |
| `DMA_IRQ_STAT` | `IDMA_START/DONE/UNDERRUN`, `ODMA_DONE/OVERFLOW`, `AXI_ERROR`. |

**Frame metadata (MAS Table 160).** Before setting `IDMA_BUF_VALID[k]`,
software may write `IDMA_BUF_TAG[k]` (any 32-bit identity) and
`IDMA_BUF_TS[k]` (capture time in the `FRAME_COUNTER` time base). The IDMA
takes both when it starts the frame; writes after that reach the next use
of the buffer. When the frame completes in output buffer m, the ODMA
publishes `ODMA_BUF_TAG[m]`, `ODMA_BUF_TS[m]`, `ODMA_BUF_SOF_TS[m]` (output
start of frame) and `ODMA_BUF_EOF_TS[m]` (last line accepted), so input and
output are matched by tag even when k ≠ m. Read them before freeing m
again. Nothing is published for an aborted or failed frame. `FRAME_COUNTER`
counts core clocks since `i_rst_n` release and wraps modulo 2^32; compute
spans as unsigned 32-bit differences (`EOF_TS − SOF_TS`: output span,
`EOF_TS − TS`: end-to-end latency). VP timestamps follow the model's clock
and memory parameters; they are not silicon performance. The driver calls
are `fx1_isp_queue_frame_meta`, `fx1_isp_next_done_meta` and
`fx1_isp_frame_counter`.

`DMA_ERR.IDMA_UNDERRUN` is a **level**: it stays 1 while `IDMA_EN` = 1 and
the next input buffer is not valid, and W1C cannot clear it while the
condition holds (CSR-06). A stream that runs out of input therefore ends
with `DMA_ERR` = 0x2. That is expected, not an error. The reference driver
does not enable the underrun interrupt. `DMA_ERR.ODMA_OVERFLOW` is a level
in the same way while the ODMA waits for a free buffer (MAS Table 161);
`IRQ_ODMA_OVERFLOW` is raised once per stall.

## 4. Configuration timing

The pipeline samples its configuration at each accepted start of frame
(SOF), with the exceptions below (HAS §9.4.3, DEC-24..27). To take effect
on a given frame, a change must be written before that frame's SOF. Only
geometry changes require the pipeline to be idle.

| Item | Rule | Driver |
|---|---|---|
| Scalar parameters (enables, gains, thresholds) | Sampled at SOF. | `fx1_isp_apply` |
| CCM set, CNF thresholds | Sampled at the accepted SOF and held for the frame. `updated` is a compatibility field with no effect (owner answer Q1, CSR/MAS 07/10). Write the whole set between frames; a write during a frame applies to the next one. | `fx1_isp_ccm_set` |
| Output channels | OUTPUT_ENABLE (IDLE class) selects CH0/CH1 (NV12) and CH2 (RGB888, one plane, stride ≥ 3W); each channel has four buffers (`CHc_ODMA_*`), its own rotation, DONE, FRAME_COUNT and metadata (`CHc_ODMA_BUF_TAG/TS/SOF_TS/EOF_TS`, `TUNE_CHc_BUFn_FRAME/PROFILE`), and `OUTPUT_IRQ_STATUS.DONE[c]`. An enabled channel without a free buffer stalls every channel (no frame lost); FRAME_DONE rises when a frame has retired at every enabled output. The driver arms CH0; its CH1/CH2 buffer API is V107-10. | — |
| Channel geometry | IDLE class (v1.07): `RESIZER_CTRL` (three channels) and the crop words are written only with the input stopped and every frame retired, else the write is rejected with `TUNE_ERROR.MODE` and nothing changes. `updated` has no effect. Presets (Table 6-122): 1..14, 16..21 (AI 640×640 .. 320×320); 15 and 22..31 reserved; CH2 accepts 0, 8, 14, 16..21. No FullRes fallback: a target larger than the crop or the channel limit (CH0/CH1 3840×2160, CH2 640×640) is invalid. Each stage decimates vertically, then horizontally. A frame uses the sets published before its SOF. Frames below 32×32 active are never admitted (ERROR 1). After a COMMON soft reset the readback is zero until the live key publishes, from the end of the 15-clock pulse. | `fx1_isp_set_crop`, `fx1_isp_set_resize`, `fx1_isp_start` |
| Gamma table | **Live**: takes effect as written (DEC-25). Change it between frames, or accept a frame that mixes old and new values. | `fx1_isp_load_gamma` |
| EE profiles | **Three resident profiles** (MAS §6.18.4.1). Each owns `EE_ALPHA` … `EE_RADIAL_GAIN_23` and the four tables. `EE_CTRL.profile_wr_sel` names the profile that register and table-port writes go to (code 3: ignored); `EE_LUT_RDATA` reads that profile; a scalar register reads back the last value written through it. `EE_CTRL.en` and `profile_sel` (code 3 = profile 0) and the selected profile's scalars are sampled at the SOF, so a frame never mixes profiles. Its tables stay **live**: a write to the in-use profile's table reaches the rows processed after it. Load a profile no frame in flight uses, with full 32-bit writes (a partial write merges the bytes last written, possibly another profile's), then select it. | `fx1_isp_ee_load_profile` (refuses, while EE is busy, every profile selected since EE was last seen idle), `fx1_isp_ee_select`; `fx1_isp_load_ee_table` (profile `profile_wr_sel`) |
| Gamma / EE / GTM table ports | Write the address register, then the data port. The address increments after each data write (Gamma modulo 256, EE 64, GTM 128). | as above |
| GTM | Auto mode: hardware builds the next curve at the end of each frame and swaps banks at the next SOF; the first frame after enable passes through. Manual mode (MAS §9.2): clear `EN` and set `MANUAL`; wait for one frame to complete, because a curve built at the end of an earlier frame still waits for its SOF; then write the 65 entries, verify them and set `EN`. A table write while `EN` = 1 is discarded, whatever `MANUAL` says (MAS p146). | `fx1_isp_gtm_manual_begin`, `fx1_isp_load_gtm_lut` (`FX1_ISP_EAGAIN` until the pipeline has taken a frame queued after the begin; its output may fail) |
| LSC profiles | Three resident profiles. The load protocol:<br>1. `LSC_LOAD_CTRL.begin(profile)`;<br>2. 4·nx·ny writes to `LSC_COEF_DATA` (R, Gr, Gb, B per node, x then y);<br>3. `validate`.<br>A load into the **active** profile is refused, valid or not (MAS Table 31, owner Q5). Profile 0 is active after reset, so boot by loading profile 1 (or 2), validating it and selecting it. `begin` invalidates its destination; it stays invalid after `abort` or a wrong count (`coef_count`). Changing `LSC_MESH_NODES` invalidates every profile; mesh sizes are 2..128 nodes per axis and not above the active width/height (MAS §6.8.4). `LSC_PROFILE_SEL` takes effect at the next SOF; selecting an invalid profile is refused, when written and again at that SOF. Do not begin a load into a selected profile before its SOF: the previous profile then stays active and `PROFILE_SEL_REJECT` is set at every SOF until the profile is validated again. | `fx1_isp_lsc_load`, `fx1_isp_lsc_select` |
| AEC | Shadow set (`en`, zone grid, clip, thresholds), transferred at the next SOF after `AEC_CTRL.commit` (W1S, reads 1 while pending). Writing `en` alone has no effect. A pending commit survives a soft reset and is consumed at the next accepted SOF; a hard reset clears it (owner answer Q4). | `fx1_isp_aec_configure` |
| AWB | Limits, grid and enable sampled at the accepted SOF. An enabled frame needs a legal grid (1 ≤ nx ≤ min(64, W), 1 ≤ ny ≤ min(32, H); a 0×0 "global only" grid is illegal) and a free result bank, otherwise it is skipped and counted (§5). | `fx1_isp_apply` |
| AF | EN is sampled once per frame at the SOF (MAS §6.23.2): a frame that started enabled is measured and published even if EN is cleared during it; a frame that started disabled is ignored. `score_valid` is set with every publication. | `fx1_isp_apply` |
| BPC counters | `NUM_CANDIDATES`/`NUM_DEFECTIVE` clear at every SOF and count an enabled frame (14-bit, saturating); they read 0 after a disabled frame. `dynamic_det_en` has no effect (MAS §6.9). The VP writes the counts at the end of the frame. | — |
| `*_CONTEXT_ID` | Latched at SOF and returned as `*_RESULT_CONTEXT_ID` with that frame's results. Use it to tag a frame with the settings it was captured under. | — |

The `D_WDR_*` and `TNR_3D_*` registers are storage only, because HAS §3.1
lists WDR and TNR as unsupported (M5-A1).

## 5. Statistics readout

Since V107-6 every engine (AEC, AWB, AF) has **two result banks owned by
software** (MAS v1.07 §9.8, §6.22.4). An enabled frame with a legal set
reserves a FREE bank at its SOF; the bank becomes READY after the frame's last
sample, sets the done bit (`AEC_STATUS.stat_done`, `AWB_STATUS.stat_done`,
`AF_STATUS.frame_done`; W1C) and raises `stats_ready_irq`. Software leases the
oldest READY bank, reads it and releases it. A leased bank is never written,
so no FRAME_ID retry is needed, and no partial result of a frame in flight is
ever visible. AE/AF results need a lease in legacy mode too. `*_FRAME_ID` is
the measured frame's ID: one counter, +1 per accepted SOF since reset.

| Data | How to read it | Driver |
|---|---|---|
| AEC (aggregate Bayer sum/count, OE/UE, min/max, 64-bin histogram, zones, weighted histogram) | `TUNE_AEC_CMD.ACQUIRE`, wait one core clock, confirm `TUNE_AEC_STATUS.LEASE_VALID`, read the identity words and the 64-bit results through `TUNE_AEC_INDEX` / `DATA_LO` (snapshots the high word) / `DATA_HI`, then `RELEASE`. The 07/10 narrow windows (`AEC_GLOBAL_*`, `AEC_ZONE_*`, `AEC_HIST_DATA`) are modulo projections of the lease; `AEC_CHANNEL_SEL` 1..3 read 0. | `fx1_isp_aec_acquire`, `fx1_isp_aec_read_global`, `fx1_isp_aec_read_zones`, `fx1_isp_aec_read_hist`, `fx1_isp_aec_result`, `fx1_isp_aec_release`, `fx1_isp_aec_drops`; `fx1_isp_read_aec_global` (compatibility: drains the READY banks, returns the newest) |
| AF (16 windows: focus, luma, count; effective rectangles) | As AEC with `TUNE_AF_*`. `AF_STATUS.score_valid` is 1 while a readable lease is held. | `fx1_isp_af_acquire`, `fx1_isp_af_read`, `fx1_isp_af_release`, `fx1_isp_af_drops`; `fx1_isp_read_af` (compatibility: newest) |
| AWB globals, zones, FRAME_ID, CONTEXT_ID | `AWB_RESULT_CTRL.acquire`, wait one core clock, confirm `AWB_RESULT_STATUS.read_valid`, read FRAME_ID, CONTEXT_ID, globals and zones of the captured grid (`read_num_zone_x/y`; one core clock between an `AWB_ZONE_ADDR` write and the data read), then release. Without a lease every AWB word reads 0, without error. | `fx1_isp_awb_acquire`, `fx1_isp_awb_read_global`, `fx1_isp_awb_read_zones`, `fx1_isp_awb_release`, `fx1_isp_awb_report`; `fx1_isp_read_awb_global` (compatibility: newest) |

Rules common to the three engines:
- A CSR access may be shorter than a core clock: wait the clock after
  ACQUIRE explicitly before checking the lease (the driver does).
- Release promptly. While both banks are READY or LEASED, enabled frames are
  skipped and counted (`TUNE_AEC_DROPS`, `TUNE_AF_DROPS`, `AWB_DROP_COUNT` /
  `overrun`). A frame starting on the RELEASE clock cannot use the bank yet.
- AE/AF: a second ACQUIRE, a RELEASE without a lease and a result read
  without a lease set `TUNE_ERROR.NO_LEASE`; a reserved index sets INVALID.
- An illegal AEC set (MAS §6.21.4: (NX−1)·ZW < W ≤ NX·ZW and the same for H,
  MIN_CLIP ≤ MAX_CLIP, TH_UE < TH_OE) skips the measurement and counts a drop.
  The 07/10 "last zone absorbs the remainder" rule is gone. An illegal AWB
  grid (frames below 32×32, NX > min(64, W/2), NY > min(32, H/2)) is skipped
  likewise.
- Legacy AEC: the zone grid, clip, thresholds and `TUNE_AEC_SOURCE` (0
  post-BPC, 1 post-DG) transfer at the SOF after `AEC_CTRL.commit`. Profile
  mode uses the active bank.
- Resets: a COMMON soft reset clears banks, leases and drops (the driver drops
  its lease state); the DMA reset keeps READY and LEASED banks and aborts only
  the measurement in flight, which publishes nothing.
- An aborted frame publishes nothing, so the 07/10 "discard zone data after
  an abort" rule and its driver tracking are no longer needed.

## 6. Interrupt-driven streaming (example)

```c
fx1_isp_dev dev;
fx1_isp_init(&dev, &hal);                         /* soft reset, clean IRQ state */
fx1_isp_set_geometry(&dev, 2688, 1520, 3);        /* BGGR */
fx1_isp_apply(&dev, profile, n_profile);          /* enables, gains, ... */
fx1_isp_load_gamma(&dev, gamma_lut);
fx1_isp_start(&dev, &buffers);                    /* ISP_EN, geometry, buffers, IRQ enables */
for (k = 0; k < 4; ++k) { fill_input(fx1_isp_next_input(&dev)); fx1_isp_queue_frame(&dev); }

void isp_irq_handler(void) {                      /* o_irq */
    fx1_isp_events ev; unsigned idx;
    uint32_t seq; unsigned input;
    fx1_isp_irq(&dev, &ev);
    if (ev.dma_err & (FX1_ISP_DMA_ERR_ERR_AXI_RD_BIT | FX1_ISP_DMA_ERR_ERR_AXI_WR_BIT))
        fx1_isp_recover(&dev, ev.dma_err);        /* §8 */
    while (fx1_isp_next_lost(&dev, &seq, &input) == FX1_ISP_OK)
        resubmit_later(seq);                      /* §8: a frame whose output failed */
    while (fx1_isp_next_done(&dev, &idx) == FX1_ISP_OK) {
        consume_output(idx);                      /* buffers.y[idx], buffers.uv[idx] */
        if (fx1_isp_input_ready(&dev)) {
            fill_input(fx1_isp_next_input(&dev));
            fx1_isp_queue_frame(&dev);
        } else {
            schedule_refill_worker(); /* retry when IDMA releases the input */
        }
    }
    if (ev.common & FX1_ISP_COMMON_IRQ_STATUS_STATS_READY_IRQ_MASK)
        read_statistics();                        /* §5 */
}
```

The HAL `delay_cycles` callback is required by `fx1_isp_init`; without it,
initialisation returns `FX1_ISP_EINVAL`. It must wait at least the requested
number of ISP core cycles so buffer commands are not lost inside the reset window.

## 7. Resets

| Reset | Effect |
|---|---|
| `i_rst_n` | Everything returns to its reset value: registers, LUTs, LSC profiles, rotation, counters, frame metadata. `FRAME_COUNTER` reads 0 while asserted and counts from the release. |
| Soft reset, `COMMON_CTRL.soft_rst` (pipeline + both DMA engines) | A recovery (§8.1): drain, then a 15-core-clock W1SC pulse. A rewrite while it runs is rejected with `TUNE_ERROR.PENDING` (v1.07; it no longer restarts the pulse); W1S/W1C writes are ignored until the final retirement. **Kept:** RW configuration (including `isp_en`, `idma_en`/`odma_en`), LUTs, LSC profiles and their validity, GTM banks, frame counters, a pending AEC commit, `IDMA_BUF_TAG/TS`, `FRAME_COUNTER` (free-running, U5-A1). **Cleared:** buffer ownership (`VALID/FREE/DONE`), the published `ODMA_BUF_*` metadata, frames in flight, statistics result banks, leases, drops and done bits, `COMMON_IRQ_STATUS`, `DMA_IRQ_STAT`, `DMA_ERR`, an LSC load in progress, the 2DNR variance. The rotation returns to buffer 0. No memory access or completion is reported for the aborted frames (MAS Table 183). |
| Soft reset, `DMA_CTRL.soft_reset` (DMA engines only) | Same pulse. Clears `VALID` / `FREE` and frames in flight; keeps the DONE bits and published metadata of completed buffers, `DMA_IRQ_STAT` and `DMA_ERR` (MAS v1.07 §9.9.8; `IDMA_BUF_TAG/TS` and `FRAME_COUNTER` kept too); the pipeline is flushed to a frame boundary, and the busy flags of the abandoned frame (pixel blocks, AEC, AF) drop. Published statistics, `COMMON_IRQ_STATUS`, an LSC load and the 2DNR estimate are kept; the aborted frame publishes nothing and its FRAME_ID is skipped (MAS p317). |

`fx1_isp_soft_reset` issues the COMMON soft reset. `fx1_isp_dma_reset` (while
streaming) issues the DMA soft reset. It returns the number of frames it
discarded, keeps the stream enabled and any AWB, AEC or AF lease held (the
DMA reset keeps READY and LEASED banks; the aborted frame publishes nothing,
MAS §9.8). A GTM hand-over in progress then waits for one frame taken after
the reset.

After a soft reset, software re-queues its frames from buffer 0. Write
`soft_rst` together with the current `isp_en`, because `isp_en` is RW in
the same register. The driver does both (`fx1_isp_soft_reset`).

## 8. Errors and recovery

| `DMA_ERR` bit | Cause | Effect | Recovery (driver) |
|---|---|---|---|
| `ALIGN` (was `ALIGN_OR_GEOMETRY`) | Bad base, stride or geometry when an engine captures its working set: at IDMA frame start, or at an output channel's claim (CH2: stride ≥ 3W). Also raises `IRQ_CFG_ERROR`. The rejected buffer's VALID/FREE bit is cleared, the rotation does not advance and nothing is read or written; an output frame waits (`ODMA_OVERFLOW`) until a buffer is freed again. | Correct the configuration, clear `ALIGN`, set the bit again. |
| `IDMA_UNDERRUN` | `IDMA_EN` = 1 and the next input is not valid. | Level; no data loss. | None (expected at the end of a stream). |
| `ODMA_OVERFLOW` | A frame is ready and the next output buffer is not free. | Lossless stall of the pipeline and the IDMA (DEC-18). The bit is held while the stall lasts; the interrupt is raised once per stall (U5-A5). | Free the buffer, then clear the bit. |
| `AXI_RD` (was `IDMA_AXI`) | Read error response. | Automatic DMA-scope recovery (§8.1): every admitted, unretired frame is aborted and recorded (cause 1). | `fx1_isp_recover`, then resubmit what `fx1_isp_next_lost` reports. |
| `AXI_WR` (was `ODMA_AXI`) | Write error response. | As `AXI_RD`, cause 2. The aborted frames publish no statistics. | As `AXI_RD`. |

### 8.1 Recovery and lost frames (MAS v1.07 §9.9.8)

Every soft reset and every AXI error response runs one recovery sequence;
DEC-19 (re-arm the same index) no longer applies.

1. **Request.** `COMMON_CTRL.soft_rst` (common scope, cause 4),
   `DMA_CTRL.soft_reset` (DMA scope, cause 3) or an AXI error (automatic
   DMA scope, cause 1 read, 2 write). `COMMON_RECOVERY_SEQUENCE` counts up,
   `COMMON_RECOVERY_STATUS.BUSY` = 1, `COMMON_IRQ_STATUS.ERROR_IRQ` is set,
   and the abort set is captured: up to four admitted, unretired frames, each
   with its ingress frame ID, the enabled outputs that had no DONE yet, and
   the cause. No frame is admitted until the end.
2. **Drain.** No new address is issued for the aborted frames; the
   responses already due are awaited.
3. **Reset pulse.** 15 core clocks; the scope's effects (§7).
4. **Final retirement.** Every `IDMA_BUF_VALID` and `CHc_ODMA_BUF_FREE` bit
   is cleared and every rotation restarts at buffer 0. The records, `COUNT`
   and `DONE` appear together and `BUSY` clears. `OUTPUT_IRQ_STATUS.ERROR[c]`
   is set for each channel a recorded frame failed, and `FRAME_DONE` when a
   frame was recorded. The DMA scope keeps `DMA_ERR`, `DMA_IRQ_STAT`, the DONE
   bits and metadata of completed buffers; the common scope clears them.

A software request while `BUSY` is rejected with `TUNE_ERROR.PENDING`; an
AXI error during a recovery joins it. Read the records with `BUSY` = 0 and
the same `SEQUENCE` before and after them: `fx1_isp_recovery_report`.

A frame the IDMA was reading ahead but whose SOF had not been accepted is
not in the abort set (V107-A35): it is lost without a record.

The reference driver settles its queue against `SEQUENCE` in
`fx1_isp_irq`, `fx1_isp_next_done`, `fx1_isp_input_ready`,
`fx1_isp_queue_frame`, `fx1_isp_recover` and `fx1_isp_next_lost`. Frames whose
CH0 output is DONE stay collectable; every other frame in flight is lost
and reported by `fx1_isp_next_lost`, oldest first, for the caller to
resubmit. Queueing waits while a recovery runs and while the restarted
rotation reaches an output that still holds an uncollected frame. If a
recovery finishes between `fx1_isp_next_input()` and `fx1_isp_queue_frame()`,
the latter returns `FX1_ISP_EAGAIN`: fill the new `fx1_isp_next_input()` and
call it again.

Nothing is collected while a recovery runs: `fx1_isp_next_done` returns
`FX1_ISP_ENODATA` until it is settled, and `FX1_ISP_EAGAIN` (frame kept) if a
recovery started during the collection and dropped the DONE acknowledgement.

The reset calls (`fx1_isp_soft_reset_ex`, `fx1_isp_dma_reset`, `fx1_isp_stop`,
`fx1_isp_init`) wait, bounded, for their recovery. A drain lasts as long as the
transactions already accepted, which MAS does not bound, so they return
`FX1_ISP_EBUSY` when it outlasts the wait: nothing in the driver changed, the
reset stays pending, and calling again completes it without a second request.
Buffers are not reusable before it returns `FX1_ISP_OK`.

`fx1_isp_recover` waits (bounded) for the recovery to finish, settles the
queue and clears the error bits it handled. It acts only on error bits that
are still set in `DMA_ERR`: a second call with the same interrupt snapshot
returns `FX1_ISP_ENODATA` and changes nothing. It returns `FX1_ISP_EBUSY`,
keeping the bits, if the recovery is still running.

### 8.2 Driver states, cancel and watchdog

States: `UNINIT` → `fx1_isp_init` → `CONFIG` → `fx1_isp_start` →
`STREAMING` → `fx1_isp_stop` → `CONFIG`. State-dependent APIs that return
an error code reject calls outside their allowed state; void helpers cannot
return `FX1_ISP_ESTATE` and ignore an uninitialised driver. Zero-initialise
`fx1_isp_dev` before `fx1_isp_init`. `fx1_isp_start` with frames in flight returns
`FX1_ISP_EBUSY`; after the queue has drained it returns `FX1_ISP_ESTATE`
until `fx1_isp_stop` resets the hardware buffer rotation. If `fx1_isp_start`
rejects an output stride, it restores the prior `ISP_EN` state.
`fx1_isp_soft_reset` keeps the state and drops the frames.

`fx1_isp_stop` cancels a stream:
- it clears `IDMA_EN`/`ODMA_EN` before soft reset, preventing an underrun
  from reappearing during the reset window;
- soft reset discards the frames in flight (DEC-14), returns every buffer
  to software, then `ISP_EN` is cleared for the CONFIG state;
- its return value is the number of frames discarded.

Timeout policy for the caller (the driver has no timer):
- **Budget.** Pick a per-frame budget from the frame time
  (`H × (W + line overhead)` core cycles on the VP; the silicon figure is
  not specified) plus memory latency and a margin.
- **Detect.** Sample `fx1_isp_get_progress` (IDMA/ODMA frame counts,
  frames in flight, `DMA_ERR`, `COMMON_STATUS.busy`, `isp_en`). If
  `odma_frames` has not moved within the budget while `in_flight` > 0, the
  stream is stuck.
- **Explain.** Read the reason from the snapshot: `isp_en` = 0, an
  `ODMA_OVERFLOW` (no free output), an uncleared AXI error, or an underrun
  (no valid input).
- **Act.** Fix the cause if it is software's, otherwise call
  `fx1_isp_stop`, then `fx1_isp_start` and re-queue.

When nothing else helps, recover with a soft reset (§7) and re-queue. Also
watch `LSC_ERROR` (load protocol), `EE_STATUS.error` (a radial gain of the
frame's profile above 1.5, raised again for every frame EE processes while it
stays programmed) and `TUNE_ERROR`; they feed `error_irq`.
`BPC_STATUS.cand_reject_ovf` is a statistics-drop indication and does not
(MAS v1.07 Table 9-188).

## 9. Reference driver

| Function | Section |
|---|---|
| `fx1_isp_init`, `fx1_isp_soft_reset(_ex)`, `fx1_isp_dma_reset` | 2, 7, 8.1 |
| `fx1_isp_set_geometry`, `fx1_isp_set_input_format`, `fx1_isp_apply`, `fx1_isp_start` | 2 |
| `fx1_isp_stop`, `fx1_isp_get_progress` | 8.2 |
| `fx1_isp_load_gamma`, `fx1_isp_load_ee_table`, `fx1_isp_load_gtm_lut`, `fx1_isp_lsc_load`, `fx1_isp_lsc_select`, `fx1_isp_ccm_set`, `fx1_isp_aec_configure` | 4 |
| `fx1_isp_input_ready`, `fx1_isp_queue_frame(_ex, _meta)`, `fx1_isp_next_input`, `fx1_isp_irq`, `fx1_isp_next_done(_ex, _meta)`, `fx1_isp_frame_counter` | 3, 6 |
| `fx1_isp_awb_acquire`, `fx1_isp_awb_read_global`, `fx1_isp_awb_read_zones`, `fx1_isp_awb_release`, `fx1_isp_awb_report` | 5 |
| `fx1_isp_ee_load_profile`, `fx1_isp_ee_select`, `fx1_isp_gtm_manual_begin` | 4 |
| `fx1_isp_recover(_ex)`, `fx1_isp_next_lost`, `fx1_isp_recovery_report` | 8, 8.1 |
| `fx1_isp_discover`, `fx1_isp_require_caps` | 9.1 |
| `fx1_isp_set_channel_buffers`, `fx1_isp_output_meta`, `fx1_isp_set_crop`, `fx1_isp_set_resize`, `fx1_isp_geometry_status` | 9.2 |
| `fx1_isp_profile_mode`, `fx1_isp_profile_begin`, `fx1_isp_profile_confirm_tables`, `fx1_isp_profile_seal`, `fx1_isp_profile_apply`, `fx1_isp_profile_cancel`, `fx1_isp_profile_unseal`, `fx1_isp_profile_state` | 9.3 |
| `fx1_isp_aec_acquire`, `fx1_isp_aec_result`, `fx1_isp_aec_read_global/zones/hist`, `fx1_isp_aec_release`, `fx1_isp_aec_drops`, `fx1_isp_af_acquire`, `fx1_isp_af_read`, `fx1_isp_af_release`, `fx1_isp_af_drops` | 5 |
| `fx1_isp_read_aec_global`, `fx1_isp_read_awb_global`, `fx1_isp_read_af` (compatibility readers) | 5 |

### 9.1 Discovery (V107-10)

`fx1_isp_discover` returns `TUNE_ID`, `TUNE_REV` (0x00010007 for CSR v1.07)
and `TUNE_CAPS`. Firmware calls `fx1_isp_require_caps` with the
`FX1_ISP_TUNE_CAPS_*_MASK` bits it relies on (for example `PROFILE`,
`RECOVERY_REPORT`, `CHANNEL_CROP`) before using them; a missing bit returns
`FX1_ISP_ENOTSUP` instead of silently running a different behaviour.

### 9.2 Output channels (V107-10)

CH1 (NV12) and CH2 (packed RGB888, `y_stride` ≥ 3 × width, no UV plane) get
their buffers with `fx1_isp_set_channel_buffers` in CONFIG state. Set
`OUTPUT_ENABLE` (IDLE class) before `fx1_isp_start`. Start latches the mask.
It refuses a stream (`FX1_ISP_EINVAL`) when an enabled channel has no
buffers, an invalid geometry, or a stride shorter than a line. Every queued
frame arms the same buffer index on every enabled channel. `next_done`
returns that index once every enabled channel published DONE, and returns
all of them to software. `fx1_isp_output_meta(ch, index)` reads a channel's
metadata, ingress frame ID and profile ID. The ID is the same on every
channel for one frame. After a recovery, a channel that had already
published DONE for an aborted frame has that DONE cleared by the driver.

### 9.3 Profile transaction (V107-10)

1. Set the input geometry and format while still in legacy mode.
2. `fx1_isp_profile_mode(1)` (idle only, else `EBUSY`).
3. `fx1_isp_profile_begin(bank)`: INIT copies the current configuration
   into the bank.
4. Write the bank with `fx1_isp_apply`. INIT clears the bank's payload
   validity: load a Gamma curve with `fx1_isp_profile_load_gamma` (banked
   port, verified, confirmed); other bank payloads are confirmed with
   `fx1_isp_profile_confirm_tables`.
5. `fx1_isp_profile_seal(bank, id)`.
6. `fx1_isp_profile_apply(bank, exact, frame)`: from the next admitted
   frame, or EXACT at ingress frame `frame`.

`fx1_isp_profile_from_legacy(bank, writes, n, id)` is the firmware's
default bring-up of a legacy profile (V107-10): after `fx1_isp_apply` of the
writes in legacy mode it enters profile mode, INITs the bank from that
configuration, reloads what INIT cleared (the confirmed legacy Gamma curve,
read back through `GAMMA_LUT_RDATA`, through the banked port; the GTM manual
curve and TUNE payload writes replayed in order through their own ports),
seals with `id` and applies. An unconfirmed legacy curve with Gamma enabled
makes SEAL fail (`NOT_READY`): legacy mode would pass the pixels through,
which a profile cannot express. On the VP this reproduces
the legacy output and statistics of every test vector with a single profile
and of every RAW regression run; a configuration that legacy mode measures
with drops (an illegal AEC/AWB grid) is refused at SEAL (`INVALID`).

`fx1_isp_profile_state` reports the active ID, the first frame and
`NEXT_FRAME`. Every command returns `FX1_ISP_EIO` with the `TUNE_ERROR` bits
it raised (LOCKED, NOT_READY, MODE, ...) and acknowledges them. Outputs
carry the profile ID in `TUNE_CHc_BUFn_PROFILE`.

The driver is C99 with no allocation; all access goes through
`fx1_isp_hal`. It is not thread-safe. Build: CMake target
`cdc::components::fx1_isp_drv` (installed as `fx1_isp::fx1_isp_drv`, see
README).

How it was verified (CTest `fx1_isp.integration.driver_tlm`):
- The driver ran all 33 vectors. Profiles went through its helpers, frames
  were queued back to back, completions came from the interrupt handler,
  and statistics were read with its readers. The NV12 output and the
  statistics equal the Python reference.
- Error cases, each with distinct inputs per frame:
  - an IDMA error handled late;
  - an ODMA error, handled at once and handled late (the previous frame
    already DONE and not collected), each followed by a stream with 4
    frames in flight;
  - a soft reset in the middle of a stream;
  - a zone read during a frame, which must return `EAGAIN`;
  - LSC and argument refusals.
- API states:
  - every call on a never-initialised device;
  - streaming calls before `start`;
  - restart and geometry change with frames in flight;
  - `stop` with frames in flight;
  - watchdog detection of a stall, then `stop` and a restart.
- Statistics after an abort and after a soft reset, including an abort that
  follows an unread publication (§5).
- CSR/MAS 07/10 (U2–U6):
  - AWB ownership, with normal and with sub-clock CSR timing (`driver_tlm_fast_csr`);
  - EE profile loading and selection;
  - the GTM manual hand-over;
  - the Resizer start waits (v1.07: GEOMETRY_STATUS polling, refused starts);
  - frame metadata through the rotation and when queued to a waiting IDMA;
  - `fx1_isp_dma_reset` in the middle of a frame.
- On the FX1 platform (CI): `fx1_smoke_isp_mas0710`, `fx1_smoke_isp_recovery`.
  - AWB lease, drops and bank reuse against the reference.
  - An AWB lease kept across `fx1_isp_dma_reset`.
  - Resizer from the first frame and a mode switch while streaming (07/10 contract; under v1.07 the switch needs stop/drain, ported in V107-10).
  - Tags matched while input and output indices differ.
  - DMA reset mid-frame, then the stream continues.
- A frame whose output fails while its input is still being read, in both
  outcomes (retried, and lost), acting only on interrupt edges (§8.1).
- Coordinated fault campaign: fixed seed, 48 runs, 4–8 frames each.
  - Faults: an IDMA read and/or an ODMA write fault at a random buffer and
    line, either of which may hit again after its re-arm.
  - Handler: run 0 µs to 2 ms late.
  - Caller: resubmits every reported loss.
  - Checks: every frame completes exactly once with the right content,
    completions come in queue order, a second `recover` with the same
    snapshot is a no-op, no deadlock.
  - Coverage the test requires: runs with both errors at once (4 here),
    repeats after re-arm (36), handler calls with ≥ 2 completions (70).
  - Of 56 output failures, 54 frames were lost and 2 were retried by the
    IDMA.
- Mutation check of the follow-up rules (2026-10-02): 13 mutants, 12 killed.
  The rules covered: deferred loss, retry clearing the mark, stale recover,
  statistics marks, settlement in `input_ready`, the state checks, `stop`,
  and the read order of VALID and `DMA_ERR`.
  The survivor removes the IDMA_DONE interrupt. On this model it is
  equivalent: when the lost frame is the last one, the end of input raises
  `IDMA_UNDERRUN`, whose rising edge set `error_irq` under DEC-15 (no longer
  since v1.07, Table 9-188). The driver
  keeps IDMA_DONE so it does not depend on an error source to end a normal
  stream.
- Driver mutation check (M5): 15 mutants, 10 killed. The 5 survivors are
  equivalent on this model:
  - closing the CCM gate before the writes, which matters only if a SOF
    falls inside the update;
  - the early busy check of the zone reader, which its final check
    repeats;
  - VALID before FREE;
  - the retry of the global readers, which needs a publication racing the
    read;
  - writing an unchanged LSC mesh size.

## 10. Limits of this guide

- No owner golden or RTL (DEC-05). Every [assumption] above can change
  when the owner answers the queries.
- The VP is loosely timed. Latencies are parameters, not silicon numbers,
  and AXI is not modelled at handshake level (DEC-06, M2-A10).
- Not available in the CSR map: buffer tags and timestamps (SPEC-04), and
  an Input Formatter crop window (CSR-14).
