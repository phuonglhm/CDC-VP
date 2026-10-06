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
| 1 | Release `i_rst_n`. Every register takes its reset value; the LUTs are 0, except the EE tables, which are unity (CSR-16). | — |
| 2 | Optional soft reset, to start from a known engine state (§7). | `fx1_isp_init` |
| 3 | Input geometry `COMMON_FRAME_WIDTH/HEIGHT` and Bayer order `COMMON_BAYER` (0 RGGB, 1 GRBG, 2 GBRG, 3 BGGR, DEC-11). W and H must be even, 2..3840 × 2..2160. Change them only while the pipeline is idle (HAS §9.4.4). | `fx1_isp_set_geometry` |
| 4 | Profile: block enables and parameters, LUTs (§4), the LSC profile, statistics configuration. | `fx1_isp_apply`, §4 helpers |
| 5 | `COMMON_CTRL.isp_en` = 1. **Before the DMA**: while `isp_en` = 0 the output geometry is 0×0, so an ODMA armed then fails with `ALIGN_OR_GEOMETRY` (DEC-20). | `fx1_isp_start` |
| 6 | Read the output geometry `RESIZER_OUT_W/H`: the Input Formatter crop, then the Resizer mode if it fits. Example: BGGR 2688×1520 gives 2686×1518. | `fx1_isp_start` (`dev.out_width/height`) |
| 7 | Strides, the 4 input and 4 output buffer addresses (64-bit register pairs; 40 address bits are used), interrupt enables. | `fx1_isp_start` |
| 8 | Queue the first frame(s), **then** enable the engines with `DMA_CTRL` (`idma_en`, `odma_en`, `max_burst_m1`). Enabling first makes the idle IDMA report an underrun before the stream starts (M4-R5). | `fx1_isp_queue_frame` (enables on the first call) |

Memory rules. The ISP refuses to start an engine that breaks these rules,
and reports `DMA_ERR.ALIGN_OR_GEOMETRY` without any memory access:
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
  next buffer in order. When the last write of both planes has completed,
  it clears FREE, sets `ODMA_BUF_DONE[k]`, and raises `IRQ_ODMA_DONE` and
  `COMMON_IRQ_STATUS.frame_done_irq`.
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
| `error_irq` | Rising edge of any error flag: `LSC_ERROR`, `BPC_STATUS.cand_reject_ovf`, `EE_STATUS.error`, `DMA_ERR[4:0]` (DEC-15). |
| `DMA_IRQ_STAT` | `IDMA_START/DONE/UNDERRUN`, `ODMA_DONE/OVERFLOW`, `AXI_ERROR`. |

`DMA_ERR.IDMA_UNDERRUN` is a **level**: it stays 1 while `IDMA_EN` = 1 and
the next input buffer is not valid, and W1C cannot clear it while the
condition holds (CSR-06). A stream that runs out of input therefore ends
with `DMA_ERR` = 0x2. That is expected, not an error. The reference driver
does not enable the underrun interrupt.

## 4. Configuration timing

The pipeline samples its configuration at each accepted start of frame
(SOF), with the exceptions below (HAS §9.4.3, DEC-24..27). To take effect
on a given frame, a change must be written before that frame's SOF. Only
geometry changes require the pipeline to be idle.

| Item | Rule | Driver |
|---|---|---|
| Scalar parameters (enables, gains, thresholds) | Sampled at SOF. | `fx1_isp_apply` |
| CCM set, CNF thresholds, Resizer scale | **Commit gates**: the active set is copied from the registers only while `<BLK>_CTRL.updated` = 1. The copy happens at once if the pipeline is idle, otherwise at the next SOF. Hardware never clears `updated`. To change a set atomically: write `updated` = 0, write the set, then write `updated` = 1 (DEC-24/26/27) [assumption]. `RESIZER_OUT_W/H` follow the committed scale. | `fx1_isp_ccm_set` |
| Gamma and EE tables, EE scalars | **Live**: they take effect as written (DEC-25). Change them between frames, or accept a frame that mixes old and new values. | `fx1_isp_load_gamma`, `fx1_isp_load_ee_table` |
| Gamma / EE / GTM table ports | Write the address register, then the data port. The address increments after each data write (Gamma modulo 256, EE 64, GTM 128). | as above |
| GTM | Auto mode: hardware builds the next curve at the end of each frame and swaps banks at the next SOF; the first frame after enable passes through. Manual mode: software loads 65 entries, and table writes are accepted only while GTM is disabled or manual. | `fx1_isp_load_gtm_lut` |
| LSC profiles | Three resident profiles. The load protocol:<br>1. `LSC_LOAD_CTRL.begin(profile)`;<br>2. 4·nx·ny writes to `LSC_COEF_DATA` (R, Gr, Gb, B per node, x then y);<br>3. `validate`.<br>A load into the active, valid profile is refused (ALG-LSC-05). A wrong count keeps the old contents (`coef_count`). Changing `LSC_MESH_NODES` invalidates every profile; mesh sizes are 2..32 nodes per axis (DEC-31) [assumption]. `LSC_PROFILE_SEL` takes effect at the next SOF; selecting an invalid profile is refused. | `fx1_isp_lsc_load`, `fx1_isp_lsc_select` |
| AEC | Shadow set (`en`, zone grid, clip, thresholds), transferred at the next SOF after `AEC_CTRL.commit` (W1S, reads 1 while pending). Writing `en` alone has no effect. A soft reset drops a pending commit (DEC-32). | `fx1_isp_aec_configure` |
| AWB, AF | Sampled at SOF. AF EN is also applied row by row: results are published only if EN = 1 at the end of the frame, and `score_valid` = 1 only if EN was 1 from SOF to EOF (ALG-AF-01/02) [assumption]. | `fx1_isp_apply` |
| `*_CONTEXT_ID` | Latched at SOF and returned as `*_RESULT_CONTEXT_ID` with that frame's results. Use it to tag a frame with the settings it was captured under. | — |

Not modelled: BPC dynamic detection. With `dynamic_det_en` = 1 the VP uses
static detection, warns, and reads its counters as 0 (DEC-28). The
`D_WDR_*` and `TNR_3D_*` registers are storage only, because HAS §3.1 lists
WDR and TNR as unsupported (M5-A1).

## 5. Statistics readout

Each block publishes at the end of a frame it measured. It sets its done bit
(`AEC_STATUS.stat_done`, `AWB_STATUS.stat_done`, `AF_STATUS.frame_done`;
W1C) and raises `stats_ready_irq`. `*_FRAME_ID` holds the ID of the
measured frame: one counter, incremented at each accepted SOF since reset.
A frame aborted after its SOF still consumes an ID (ALG-STAT-01/09).

| Data | Buffering | How to read it coherently | Driver |
|---|---|---|---|
| AEC globals, AWB globals | Double-buffered: stable until the next publication. | Read FRAME_ID, then the values, then FRAME_ID again; retry if it changed (HAS §6.21.9). | `fx1_isp_read_aec_global`, `fx1_isp_read_awb_global` |
| AF scores (4×4) | Latched registers. | As above. | `fx1_isp_read_af` |
| AEC zones and histogram, AWB zones | **One in-place memory** (DEC-30). From the next enabled SOF it shows that frame's partial sums. A zone not yet written reads empty: 0, min 4095, max 0. | FRAME_ID alone cannot detect the overlap. Read while `AEC_STATUS.busy` = 0 (AWB: `COMMON_STATUS.busy` = 0) before and after, with FRAME_ID unchanged; or hold the next input buffer until the readout is done. | `fx1_isp_read_aec_zones` (returns `FX1_ISP_EAGAIN` otherwise) |

After an aborted frame, discard its zone/histogram data even if busy is 0:
the in-place memory may contain partial sums while FRAME_ID still names the
last completed frame. Busy plus read-tag-read alone cannot certify that data
after an abort. Resume reading after a known successful statistics publication.
The same holds after a soft reset: the memory reads 0 under the kept FRAME_ID.

The reference driver enforces this. `fx1_isp_irq` (on `IDMA_AXI`),
`fx1_isp_recover` and `fx1_isp_soft_reset` record the current `AEC_FRAME_ID`.
`fx1_isp_read_aec_zones` then returns `FX1_ISP_ENODATA` until `AEC_FRAME_ID`
moves past that mark, that is, until a new frame has been published. Each
event refreshes the mark, so a publication nobody read cannot vouch for a
later abort. `AEC_FRAME_ID` = 0 is not sufficient to trust the memory:
the first accepted frame may have written partial zones before abort. The
driver keeps data untrusted until a successful AEC publication. A pending
`DMA_ERR.IDMA_AXI` also makes reads return `FX1_ISP_ENODATA` before the
interrupt handler runs. A reset before any AEC publication still exposes
empty defaults under FRAME_ID 0.

Limits: only the aborts software can see are covered (IDMA read errors, soft reset, `fx1_isp_stop`).
An abort the model makes internally, such as an SOF in the middle of a frame
or a geometry rejected by a block (ALG-STAT-09), raises no software-visible
event. An IDMA error before the frame's SOF also marks the data, which is
conservative.

Readout uses selector registers: `AEC_CHANNEL_SEL` (0 R, 1 Gr, 2 Gb, 3 B),
`AEC_ZONE_ADDR`, `AEC_HIST_ADDR`, `AWB_ZONE_ADDR` and `AF_STAT_ADDR`. Zone
addresses run in raster order. Addresses beyond the grid read empty. Until
the first enabled SOF after a reset, every zone field reads 0
(M3-A8) [assumption]. Field widths: AEC sums 33 bits (`HI` holds bit 32),
AWB sums 35 bits (`_H` holds bits 34:32). Accumulators wrap.

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
    if (ev.dma_err & (FX1_ISP_DMA_ERR_ERR_IDMA_AXI_BIT | FX1_ISP_DMA_ERR_ERR_ODMA_AXI_BIT))
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
| `i_rst_n` | Everything returns to its reset value: registers, LUTs, LSC profiles, rotation, counters. |
| Soft reset, `COMMON_CTRL.soft_rst` or `DMA_CTRL.soft_reset` (identical, DEC-14) | 32 core cycles; W1S/W1C writes are ignored during the window. **Kept:** RW configuration (including `isp_en`, `idma_en`/`odma_en`), LUTs, LSC profiles and their validity, GTM banks, frame counters and FRAME_IDs. **Cleared:** buffer ownership (`VALID/FREE/DONE`), frames in flight, statistics results and done bits, the zone memories, `COMMON_IRQ_STATUS`, `DMA_IRQ_STAT`, `DMA_ERR`, a pending AEC commit, the 2DNR variance. The rotation returns to buffer 0. No memory access or completion is reported for the aborted frames. |

After a soft reset, software re-queues its frames from buffer 0. Write
`soft_rst` together with the current `isp_en`, because `isp_en` is RW in
the same register. The driver does both (`fx1_isp_soft_reset`).

## 8. Errors and recovery

| `DMA_ERR` bit | Cause | Effect | Recovery (driver) |
|---|---|---|---|
| `ALIGN_OR_GEOMETRY` | Bad base, stride or geometry when an engine arms; output geometry 0×0 because `isp_en` = 0 (DEC-20); a frame whose geometry differs from the one captured at arm (M2-A4). | The engine does not start, or the frame is discarded. | Fix the setting, clear the bit. |
| `IDMA_UNDERRUN` | `IDMA_EN` = 1 and the next input is not valid. | Level; no data loss. | None (expected at the end of a stream). |
| `ODMA_OVERFLOW` | A frame is ready and the next output buffer is not free. | Lossless stall of the pipeline and the IDMA; flagged once per stall (DEC-18). | Free the buffer. |
| `IDMA_AXI` | Read error response. | The frame is discarded end to end. The input buffer's VALID bit is cleared and the rotation is kept (DEC-19). | Re-arm the same input index: `fx1_isp_recover`. |
| `ODMA_AXI` | Write error response. | The frame is not DONE, its FREE bit is cleared, and the rotation is kept. The next frames stall until that buffer is free again (DEC-19). The pipeline did complete the frame, so its statistics were published. | Free the failed output index again (`fx1_isp_recover`). The failed index is the first in-flight buffer, in rotation order, whose DONE bit is clear. It is not necessarily the next buffer to collect: when the handler runs late, earlier frames may be DONE and not yet collected (M5-R5). Resubmit the frame only once `fx1_isp_next_lost` reports it (see below). |

### 8.1 When is a frame lost?

A frame whose output failed is lost **only if the IDMA finishes reading its
input without an error**. The IDMA may still be reading that input when the
ODMA fails. If the IDMA then also fails on the same frame, the hardware
keeps the IDMA on that input and requires it to be re-armed. The frame is
then read again, lands in the same output buffer and completes. A driver
that declares the frame lost at the ODMA error therefore re-arms the wrong
input or none: the IDMA waits forever, or the frame completes after the
caller has resubmitted it and appears twice. The coordinated fault campaign
found exactly that (M5-R9).

The reference driver keeps such a frame in flight, marked "output failed",
and decides its fate later:
- **Lost.** When its input's VALID bit is clear and no IDMA error names it,
  the driver removes the frame, moves the later frames' outputs up one
  buffer, and queues it for `fx1_isp_next_lost`.
- **Retried.** When the IDMA error names it, recovery re-arms its input
  and the mark is dropped.

The check runs in `fx1_isp_irq`, `fx1_isp_next_done`, `fx1_isp_input_ready`,
`fx1_isp_recover` and `fx1_isp_next_lost`. The IDMA_DONE interrupt is
enabled so the decision is not left waiting when the lost frame is the
last one queued.

`fx1_isp_recover` acts only on error bits that are still set in `DMA_ERR`.
Calling it twice with the same interrupt snapshot is safe: the second call
returns `FX1_ISP_ENODATA` and changes nothing. If it does handle something,
that is a new error, for example a fault that hit again right after the
re-arm.

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
watch `LSC_ERROR` (load protocol), `BPC_STATUS.cand_reject_ovf` and
`EE_STATUS.error` (radial gains out of range). All three feed `error_irq`.

## 9. Reference driver

| Function | Section |
|---|---|
| `fx1_isp_init`, `fx1_isp_soft_reset` | 2, 7 |
| `fx1_isp_set_geometry`, `fx1_isp_apply`, `fx1_isp_start` | 2 |
| `fx1_isp_stop`, `fx1_isp_get_progress` | 8.2 |
| `fx1_isp_load_gamma`, `fx1_isp_load_ee_table`, `fx1_isp_load_gtm_lut`, `fx1_isp_lsc_load`, `fx1_isp_lsc_select`, `fx1_isp_ccm_set`, `fx1_isp_aec_configure` | 4 |
| `fx1_isp_input_ready`, `fx1_isp_queue_frame(_ex)`, `fx1_isp_next_input`, `fx1_isp_irq`, `fx1_isp_next_done(_ex)` | 3, 6 |
| `fx1_isp_recover(_ex)`, `fx1_isp_next_lost` | 8, 8.1 |
| `fx1_isp_read_aec_global`, `fx1_isp_read_aec_zones`, `fx1_isp_read_awb_global`, `fx1_isp_read_af` | 5 |

The driver is C99 with no allocation; all access goes through
`fx1_isp_hal`. It is not thread-safe. Build: CMake target
`cdc::components::fx1_isp_drv` (installed as `fx1_isp::fx1_isp_drv`, see
README).

How it was verified (CTest `fx1_isp.integration.driver_tlm`):
- The driver ran all 32 vectors. Profiles went through its helpers, frames
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
  `IDMA_UNDERRUN`, whose rising edge sets `error_irq` (DEC-15). The driver
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
