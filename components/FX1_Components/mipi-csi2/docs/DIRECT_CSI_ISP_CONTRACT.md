# Direct CSI-2 → ISP integration contract (D0)

Status: D0 of the direct CSI→ISP plan, contract frozen for review, **no model
or firmware code changed yet**. Plan:
`plan/DIRECT_CSI_ISP_IMPLEMENTATION_PLAN_20261118.md` (D0–D5) and its
checklist `DIRECT_CSI_ISP_CHECKLIST_20261118.csv`. Decisions of this document
are recorded as P19–P36 in [spec_decisions.md](spec_decisions.md); the
baseline, test inventory and review status are in
[DIRECT_STATUS.md](DIRECT_STATUS.md).

## 1. Scope

```text
Memory path (default, kept):
  sensor → detailed D-PHY → CSI-2 RX → CSI DMA → DDR RAW → ISP IDMA → ISP → ODMA → DDR NV12
Direct path (added):
  sensor/TPG → detailed D-PHY → CSI-2 RX → AXI4-Stream → ISP ingress → ISP → ODMA → DDR NV12
  (CSI RAW DMA off, ISP IDMA idle)
Direct + RAW dump (verification fixture): the direct path, with the CSI DMA also writing RAW to DDR.
```

Linear VC0, RAW10 and RAW12, one ISP pipeline/profile/ODMA for both sources.
Out of scope: HDR / dual-VC processing, metadata DMA and the new metadata
layout, CSR workbook v1.4 (IP 1.2.0) encodings, CPU or SYS_DMA RAW bridges.

**Neither IP specifies this connection.** The CSI-2 RX MAS v1.4 defines the
AXI4-Stream master towards the ISP (Block 12, Table 6-67). The ISP MAS v1.07
defines a memory-to-memory core with no sensor-facing port ("A frame reaches
it ... by the memory-to-memory topology", IP boundary, Table 4-1); its pixel
AXI4-Stream interfaces are internal to the wrapper. The direct path is a
**VP integration extension**: the ISP's internal stream boundary between the
IDMA and the Input Formatter (ISP MAS v1.07 Table 6-15: `i_s_tvalid`,
`o_s_tready`, `i_s_tdata` 12-bit lanes, `i_s_tuser` = SOF, `i_s_tlast` = EOL)
gets a second source. Source selection and per-frame context are VP-only
controls (§6, §9); nothing here claims conformance of either IP beyond its
own specification.

## 2. Sources

| Source | Revision / SHA-256 (first 16 hex) | Used for |
|---|---|---|
| `FTEL_IP_MIPI_CSI-2_CONTROLLER_MAS_v1.4.docx` | V1.4 (10/23/2026), `e2f708ca001d6e57` | Block 12 AXI4-Stream master, Table 6-67 signal list, Finalize Sequencer, integration constraints. Block 12 text is identical to v1.3 apart from table numbering (checked) |
| `FTEL_IP_MIPI_CSI2_RX_Controller_HAS_V1.2.docx` | `7ee871a163a4bf6f` | interfaces |
| `FTEL_HCMUT_CSI2_Requirement_V4_0.md` | `7d99b239b4cdbdc6` | pixel unpacking, §4.4 |
| `FTEL_MEMORY_MAP_REGISTERS_v1_4.xlsx` (IP 1.1.0, 55 registers) | `d89a8fe6e1703b60` | **CSR baseline kept** (P19) |
| `FTEL_IP_MIPI_CSI2_RX_CONTROLLER_CSR_V1_4.xlsx` (IP 1.2.0) | `3b219facf866e028` | not applied (later CSR migration C0–C7) |
| `FTEL_IP_ISP_MAS_v1.07.docx` | `3fa4097d8f000b4a` | ISP input boundary, input depth, admission, ODMA back-pressure |
| `FTEL_IP_ISP_CSR_v1.07.xlsm` | `444e72f8d7fd3f35` | no input-source CSR exists (checked: no field selects the pixel source) |
| CDC-VP | `36ad26a4b200232ff49570c2b31f1120ccec5bd3`, clean | code cross-check below |

## 3. AXI4-Stream signals: specification vs model

Table 6-67 of the MAS v1.4, checked against `controller/src/stream_out.cpp`,
`sys_datapath.cpp`, `frame_control.cpp` and `pixel_path.cpp` at the baseline.

| Signal | MAS v1.4 | Model at baseline | Contract for the sink |
|---|---|---|---|
| `axis_aclk`, reset | sys_clk; `resetn` async, `soft_reset` sync | `SC_THREAD sys_thread`, 4 ns grid (250 MHz, C24); idle cycles skipped while quiescent (C26), never while TVALID = 1 | the sink is called once per executed cycle and must use `sc_time_stamp()`, not count calls (§4) |
| `m_axis_tdata[63:0]` | pixel payload | RAW16: 4 pixels per beat, pixel *n* in bits [16n+15:16n], little endian, upper nibble 0 (`stream_out.cpp:107-111`); RAW10 right-aligned [9:0], RAW12 [11:0] | §5 |
| `m_axis_tvalid` / `m_axis_tready` | transfer when both are 1; TREADY is the only back-pressure point | `AxisSink::axis_cycle(tvalid, beat)` returns TREADY for that cycle (`sys_datapath.cpp:176, 225`); held beat stable while TREADY = 0 (skid buffer) | the sink must not consume a beat it answers TREADY = 0 to, and must not `wait()` |
| `m_axis_tkeep[7:0]` | `fbs_byte_en`; TSTRB not implemented | full beats 0xFF; line tail `(1<<bytes)-1`: RAW12 width ≡ 2 mod 4 → 0x0F; RAW10 always 0xFF (4-pixel groups); abort beat 0x00 | samples = popcount(tkeep)/2; an odd byte count or a non-prefix mask is a format error |
| `m_axis_tlast` | end of line; forced on abort beats | end of line only (`stream_out.cpp:176`) | EOL, never end of frame |
| `m_axis_tuser[0]` | SOF on the first beat | first beat of a frame only | the frame starts on the **accepted** SOF beat (`tvalid && tready`), once |
| `m_axis_tuser[1]` | packet error or forced abort | set on data beats only by `err_wc_align` (last beat of the line, `stream_out.cpp:64, 87`); CRC/ECC/lane-abort errors are reported by the abort beat (§3.1) | any TUSER[1] makes the frame invalid |
| `m_axis_tid[1:0]` | exposure index after `vc_exposure_map` | `fbs.vc & 3`; `vc_exposure_map` stored only (D09, C28); `supported_vc_mask` = 0x1 → always 0 | linear expects TID 0; another TID is discarded and counted (P24) |
| `fbs_ready` | from skid state, held 1 in DRAIN/INJECT | as specified; shared by stream and DMA sides (`stream_out.cpp:19-23`) | the stream stalls the DMA path and vice versa (TPG) |
| `asm_abort_req/mask/done` | pulse, mask per exposure, done = observation | mask fixed 0x1 (`frame_control.cpp:237`): exactly one abort beat per abort; `abort_done` observation only | §3.1 |
| `asm_skip_req`, `stream_follow_dma_drop` | v1.4 Block 12 | **not implemented** at the baseline (C29, D02; `CtlIn.dma_drop` never driven) | no skip; DMA drops do not reach the stream (§11) |
| `axis_enable` | STREAM_CONFIG bit 0 | as specified; 1→0 requests an abort | |

STREAM_CONFIG at the baseline (IP 1.1.0): bit 0 `axis_enable`, bit 1
`stream_format` **0 = RAW16, 1 = packed**, reset **0x3** (enabled, packed).
The direct path requires RAW16. The format may change only while the stream
is disabled, and a single write that disables the stream and changes the
format counts as a change while enabled (model warning). From reset the
camera manager calls `fx1_csi2_stream_config(dev, 0, 0)` (disable, format
unchanged: 0x2) and then `fx1_csi2_stream_config(dev, 1, 1)` (writes 0x0, then
0x1). The second call alone, from reset, returns `FX1_CSI2_ESTATE` and
writes nothing. Never a literal (P19).

### 3.1 Abort and packet errors on the stream

- Drop causes (Control FSM, `frame_control.cpp:259-264`): ECC2, CRC, WC error,
  lane abort, Line Buffer full/overflow (sensor path), Format Bridge overflow,
  core disable; and `axis_enable` 1→0.
- The Finalize Sequencer drains the beats already held (legitimate pixels of
  the line), then injects one **null beat** `{tkeep 0, tlast 1, tuser 0b10, tid}`
  that closes the open line or stands alone. The frame gate suppresses the rest
  of that frame until the next SOF.
- **The drop flushes the Line Buffer and the Format Bridge** (Control FSM
  state FLUSH: `lb_flush`, `fb_flush`), whatever capture their beats belong
  to. Under back-pressure they can still hold the tail of an *earlier*
  capture whose `capture_end` was already reported OK (its FE had been
  processed): that frame then ends early, closed by the abort beat
  (measured in D2a, `camera_direct_isp_pause`: OK at the CSI, 6 of 32 lines
  on the stream, INVALID / ABORT at the ingress, not published). A CSI OK is
  therefore not proof that the stream delivered the frame; the ingress's
  stream outcome is (§9.2).
- **A CRC error is known after its line's payload**: that line may already have
  left complete with TLAST, followed by a separate null abort beat. The sink
  treats any abort beat as "this frame is invalid", whatever came before.
- Long TREADY = 0: sensor path — stream FIFO, then Line Buffer reaches its
  watermark, the capture is dropped and flushed; the abort beat comes only when
  TREADY rises again. TPG path — back-pressure only (the generator stalls).
- The null abort beat must be accepted even when the ISP has no output credit
  (it never needs one, §8), otherwise the CSI stays in DRAIN/INJECT.

## 4. Clocks, sampling and buffering

| Item | Value |
|---|---|
| CSI side | `axis_cycle()` called from the CSI `sys_thread` (250 MHz grid), registered outputs, TREADY used the same cycle |
| ISP side | core 500 MHz (`fx1_isp_tlm.h` `core_period` 2 ns); pipeline line time `(width + overhead) × 2 ns` |
| Ingress beat FIFO | **16 beats** (64 samples), filled in the CSI domain; `TREADY = FIFO not full`, computed without waiting; a beat is pushed only when `tvalid && TREADY` (P23) |
| Line assembly | one line, up to 3842 samples (ISP external width limit), in the ISP domain; a completed line is pushed into the existing `in_fifo` (2 lines) |
| Capacity bound | at most 16 beats (64 samples) in the ingress FIFO + 1 line in assembly + 2 lines in `in_fifo` between the CSI stream master and the pipeline; no DDR RAW staging. This bounds **storage**, not time |
| Back-pressure | the ISP pipeline / ODMA stall at line boundaries (ISP MAS v1.07 "Output DMA Overflow"); the ingress FIFO then fills, TREADY falls, the CSI Line Buffer absorbs up to its watermark and then drops the capture (sensor); the TPG only stalls |

The two domains are modelled as a FIFO between two SystemC processes; no
equal-clock assumption is made. Reset of either side clears the ingress (§10).

**Rates and latency.** The CSI master can deliver one beat (4 samples) per
4 ns cycle, 1 sample/ns peak; the pipeline consumes one sample per 2 ns core
cycle, a line taking `(W + pipeline_line_overhead_cycles) × 2 ns` (overhead 0
at the baseline). The FIFO absorbs the burst inside a line; the **sustained**
condition is sensor line time ≥ pipeline line time, otherwise back-pressure
accumulates until the CSI drops the capture.

Without back-pressure (pipeline and ODMA never stalled, condition above met)
a line is store-and-forward through the assembly: it enters `in_fifo` when its
EOL beat is accepted, and the pipeline starts it after finishing the lines
ahead of it, at most 2 × `(W + overhead) × 2 ns`. Added latency per line is
therefore its own transfer time plus at most two pipeline line times
(3842 samples: ≤ 15.4 µs).

**With back-pressure there is no time bound.** A stalled ODMA (no free buffer,
DDR back-pressure) or a stalled pipeline (auto-GTM replay) holds TREADY low for
as long as it lasts; finite buffering only means nothing is lost silently.
The stall ends in one of these ways, none of which the ingress times itself:
the ISP resumes; the CSI Line Buffer reaches its watermark and the CSI drops
the capture (`capture_end(DROPPED)` and an abort beat when TREADY rises,
frame invalid, §7); the TPG holds its frame indefinitely (no drop); the camera
manager's frame deadline (D3) stops the stream and runs the recovery of §10.
The ingress counts stall cycles (§6) so that tests can bound them.

ISP-internal buffering is outside this bound and depends on the profile: auto
GTM (`gtm_en && !gtm_manual`) keeps the whole pre-GTM YUV888 frame (3·W·H
bytes, ~24.9 MB at 3840×2160) and replays it at frame end; the other stages
use row windows of radius 1–2. Profile "basic" (all tests) has no frame store.
The direct path inherits whatever the profile does; it does not promise
one-line latency.

## 5. Pixels, depth and geometry

- CSI STREAM_CONFIG = RAW16 (0x1): one 16-bit right-aligned container per
  sample, the same representation as the ISP's DDR input (ISP MAS v1.07 Table
  6-150: "Unpack to bits 9:0 of a halfword ... no pre-shift" for RAW10).
- The ISP input depth `COMMON_INPUT_FORMAT.DEPTH` (0 = RAW12, 1 = RAW10) is set
  to the sensor depth; the pipeline normalizes **once** (RAW10 `<< 2`,
  `isp_pipeline.h:137-141`). **The direct path never shifts in the adapter**
  (the memory path's firmware staging `<< 2` with DEPTH = RAW12 gives the same
  pipeline values; both stay valid, P22).
- A sample with bits above the configured depth (RAW10 > 0x3FF, RAW12 > 0xFFF)
  marks the frame invalid (counted).
- Geometry comes from the ISP CSRs latched at the accepted SOF:
  `COMMON_FRAME_WIDTH/HEIGHT` (external size, 32..3842 × 32..2162, even),
  Bayer phase and crop/resize as for the memory path. The CSI carries no Bayer
  information. Every line must hold exactly WIDTH samples and the frame exactly
  HEIGHT lines (§7).

## 6. Source selection: VP block `ISP_INGRESS`

There is no ISP or CSI CSR for the pixel source (§1). The VP adds a register
block at **`0x1105_0000`** (PERIBUS_1, 64 KiB APB slot, `FX1_ISP_INGRESS_BASE`
in `fx1_memory_map.h`), VP-only, documented as such. No PLIC source: the
manager polls it on ISP (source 3) and CSI (source 7) interrupts and in its
poll loop (P20). Map (frozen in D1a; exact bits in
`components/FX1_Components/isp/include/fx1_isp/fx1_isp_ingress_regs.h`):

| Offset | Name | Access | Content |
|---|---|---|---|
| 0x000 | ID | RO | `'ISPI'` = 0x49535049 |
| 0x004 | CAPABILITY | RO | [0] BOUND (a CSI stream is bound), [7:4] VERSION 1, [15:8] FIFO beats 16, [19:16] context depth 8, [31:20] max line samples 3842 |
| 0x008 | CTRL | RW | [0] SOURCE (0 = memory, reset; 1 = direct), [5:4] EXPECTED_TID (reset 0) |
| 0x00C | STATUS | RO / W1C | RO [0] IDLE, [1] IN_FRAME, [2] DIRECT; W1C sticky [8] SWITCH_REFUSED, [9] CTX_FULL (a capture found no free context entry), [10] NOT_BOUND |
| 0x010–0x034 | counters | RO | BEATS_ACCEPTED, FRAMES_ADMITTED, DROP_NO_CREDIT, DROP_NO_CONTEXT, DROP_NO_STREAM, DROP_CONFIG (input geometry or depth not legal at the SOF), FRAMES_INVALID, CAPTURES_UNTRACKED, BEATS_DISCARDED (memory mode, TID, dropped / invalid frames), STALL_CYCLES (TVALID answered TREADY 0) |
| 0x038, 0x0A0–0x0A4 | LAST_DROP | RO | reason [3:0] (1 NO_CREDIT, 2 NO_CONTEXT, 3 NO_STREAM, 4 CONFIG), FRAME_ID [29:16]; capture sequence (64-bit) |
| 0x040–0x068 | CONTEXT | RO + POP | head READY entry (§9): TAG, CAPTURE_SEQ (64-bit), FRAME (FRAME_ID [13:0], EPOCH [31:16]), TIMESTAMP (64-bit), OUTCOME (code [3:0]: 1 OK, 2 RESET, 3 CSI_DROPPED, 4 CSI_MISSING_FE, 5 INVALID, 6 ISP_FAILED, in this precedence; cause [15:8]), LINES, SAMPLES; COUNT (READY [3:0], USED [11:8]); POP (W1 bit 0) |
| 0x06C | CTX_ISP_FRAME | RO | ISP frame ID of the head entry (assigned at the pipeline's accepted SOF; the ID its statistics `AEC/AWB/AF_FRAME_ID`, `TUNE_*_FRAME` and `TUNE_CHx_BUFn_FRAME` carry); 0 if the frame never reached the ISP. Firmware discards statistics of a non-OK capture with it (P30) |
| 0x080–0x09C | cause counters | RO | one per invalid cause: 1 LINE_LENGTH, 2 EXTRA_LINES, 3 MISSING_LINES, 4 KEEP, 5 DEPTH, 6 TUSER_ERR, 7 ABORT, 8 TAG_MISMATCH |

Rules:
- SOURCE changes only while idle and drained (no frame in the ingress, no
  frame in flight in the ISP); otherwise the write is ignored and
  SWITCH_REFUSED set. Reset value memory: existing firmware is unaffected.
- **Memory mode:** the ingress answers TREADY = 1 and discards every beat
  (counted), exactly like an unbound sink, so the memory path, the shared
  `fb_ready` coupling and all existing tests behave as before (P21).
- **Direct mode:** the IDMA issues no read, prefetch or completion; no
  `IDMA_BUF_VALID`, `IDMA_DONE`, `IDMA_FRAME_COUNT` or UNDERRUN is generated or
  faked. A direct mode selected without a bound stream sets NOT_BOUND and
  admits nothing.

## 7. Frame acceptance and validation (direct mode)

| Event at the ingress | Action |
|---|---|
| accepted SOF beat whose `vp_capture_seq` has a PENDING context entry (§9), ISP can admit (§8) | open a frame bound to **that** entry: ACTIVE, next TAG, latch geometry/depth/profile at the ISP's accepted SOF as for IDMA input |
| accepted SOF beat with a PENDING entry, ISP cannot admit | **drop the whole frame**: TREADY stays 1, its beats are discarded, counter NO_CREDIT; the entry is released, no readable record (P23, §9) |
| accepted SOF beat whose `vp_capture_seq` has no entry (untracked capture, seq 0, entry already bound) | drop the whole frame as above, counter NO_CONTEXT |
| beat with TID ≠ EXPECTED_TID | discarded, counted, TREADY 1 (P24) |
| data beat whose `vp_capture_seq` ≠ the open frame's | frame invalid, cause TAG_MISMATCH (cannot happen with an in-order CSI; a check, not a recovery path) |
| line shorter/longer than WIDTH, more than HEIGHT lines, odd KEEP bytes, sample above depth, TUSER[1] on a data beat | frame invalid: an abort marker goes down the pipeline (the existing `line.abort` path), rest of the frame discarded, context outcome by cause (P24) |
| null abort beat | invalidates the open frame, whatever its `vp_capture_seq`; always accepted, also without output credit; with no frame open it is counted and ignored |
| SOF while a frame is open | the open frame is aborted (invalid, MISSING_LINES), the new SOF handled as above |
| CSI reset (hook, §10) | open frame aborted (abort marker down the pipeline), FIFO cleared, outcome RESET, entry CANCELLING until the ISP has retired the frame (§9.1 rule 8) |

After its last line and until its stream boundary (§9.1) a frame is still
open: a line beyond HEIGHT of the same capture always invalidates it
(EXTRA_LINES; a CSI OK does not prove the stream matched HEIGHT, D1a review
F2), and a late null abort beat (CRC error of the last line) invalidates it
(ABORT) unless its capture has already ended OK (the abort then belongs to a
later capture or to a stream disable). The entry is not READY before the
boundary, so nothing is published before these checks are over.

The stream boundary also closes a frame that is still open at it (D1a
re-review R1): a partial line makes it invalid with LINE_LENGTH, fewer than
HEIGHT complete lines with MISSING_LINES. The frame is abandoned at once (the
abort marker down the pipeline, or, if no line reached the ISP, no ISP frame
and its credit returned); it does not wait for a next SOF.

A beat whose TID is not EXPECTED_TID, a null abort beat included, is
discarded and counted before it can touch any frame (D1a review F3).

From its accepted SOF beat an admitted frame is an admitted context of the
ISP (IDLE condition, ISP MAS v1.07 §9.9.3): `COMMON_STATUS.BUSY` is set and
IDLE-class writes (geometry, depth, Bayer, output enable, crop, resizer) are
refused with `TUNE_ERROR.MODE`, before its first line reaches the pipeline as
after (D1a review F1). Its configuration therefore stays the one latched at
the SOF; profile and tuning commits follow the pipeline's accepted SOF, as
on the memory path.

Pixels go down the pipeline before the frame is validated (cut-through). A
frame is **published** only when its terminal outcome is success (§9). The
next frame after any invalid frame must be processed normally.

## 8. Admission and output credits

At the accepted SOF the ingress admits the frame only if its capture holds a
context entry (reserved at `capture_start`, §9) **and** the ISP would accept
it now: `control_unit::admission_state(...) == go` (context credits < 4, no
recovery, valid geometry on every enabled channel, profile mode rules) **and**
the ODMA buffer each enabled channel will take is free (`ODMA_BUF_FREE`
bit of rotation head + the admitted frames whose ODMA of that channel has not
finished yet: the rotation advances only at DONE), with ISP_EN and ODMA_EN
set.
Otherwise the frame is dropped at the SOF (§7). This uses the common admission
and output-credit rules of the ISP for both sources; IDMA bookkeeping stays
IDMA-only. Inside an admitted frame back-pressure is real (TREADY low) and may
make the CSI drop the capture; the ingress then retires it as invalid. Abort
beats and the discard of a dropped frame never need a credit; no credit is
created by the ingress.

## 9. Frame context and lifecycle

The AXI4-Stream signals of Table 6-67 carry no capture ID or timestamp, and
the CSI's own events carry no ID for the end of a capture. A capture's
`capture_end` can come **before its first beat is accepted** (TREADY low with
the frame held in the CSI Line Buffer / FIFO: reproduced in review, capture
delivered with 0 beats accepted, then all 72 beats valid once TREADY rose), so
no association can be inferred from the order of hook events and accepted
SOFs. The VP adds two observer-only items to the CSI controller (P25; no CSR,
IRQ, counter, timing or behaviour change):

**Per-beat capture sequence.** The CSI FRAME_ID cannot be the key: it is
14-bit (1..16383, 0 never assigned), it advances for **every** capture,
tracked by the ingress or not, and a full reset clears it, so an old entry
and a new capture can carry the same FRAME_ID (D0 re-review R1). The key is a
**64-bit capture sequence** owned by the VP observer:

- `capture_seq` is allocated in the CSI Frame Context together with the
  FRAME_ID, at every capture start (sensor FS or TPG frame), from 1 and +1 per
  capture; 0 means "no capture". It is **never reset** by `resetn`,
  `soft_reset` or a full reset of the CSI (observer state, cleared only when
  the model is constructed), so it never repeats within a simulation (2^64
  captures).
- Each pixel beat carries it from the Line Buffer write onwards, next to the
  existing FRAME_ID sideband (`tag`, M18, used by the CSI DMA), through the
  Format Bridge and the stream master's output and skid registers. The sink
  sees it as `AxisBeat::vp_capture_seq`, with `AxisBeat::vp_frame_id` (the
  FRAME_ID, for RAW dump association and diagnostics). Both are VP sideband
  fields, **not Table 6-67 signals**, excluded from `AxisBeat::operator==`,
  never shown in the AXI4-Stream tables or traces delivered to the ISP owner.
  The null abort beat carries the values of the last beat the master accepted,
  until that capture's `capture_stream_end` has been reported; after it, an
  abort beat carries 0 ("no capture"): an abort of a later capture whose
  beats never reached the master (dropped between its FS and its first line)
  is not attributed to a capture already closed (D2a review F1, P32). The
  ingress ignores 0 for boundaries.
- Order is plain integer order of `capture_seq`; there is no ring arithmetic.

**Capture events** (implemented in D2a: `fx1::csi2::CaptureObserver`,
`Csi2RxController::set_capture_observer()`, observer-only; the 64-bit
sequence is allocated in the Frame Context with the FRAME_ID and carried by
`PxBeat` / `OutBeat` / the master registers to `AxisBeat::vp_capture_seq`;
the boundary is reported when no beat of the capture is left in the Line
Buffer, the Format Bridge stream side or the master registers **and** no
abort beat can still carry it: while a drop / stop sequence is on its way to
an abort (Control FSM HOLD / FLUSH or a latched stream stop, or the master in
DRAIN / INJECT) the master's last accepted capture stays open until the abort
beat has been handshaked (D2a review F1). The boundary is per capture: it can
come before that of an older capture whose beats are still held. The pending
set holds only the ended captures still resident in the stream path (each
keeps a beat of the Line Buffer / Format Bridge / master, one more for the
abort), so it is bounded by line_buffer_depth + stream_fifo_depth + 4
entries whatever the number of frames sent during a stall (D2a review F2,
P32)):

| Hook event | When (baseline code) | Payload |
|---|---|---|
| `capture_start(seq, id, ts)` | cycle of the FS packet (sensor) or first accepted TPG beat; always before the SOF beat leaves the stream (`sys_datapath.cpp:275-277`) | capture sequence, CSI 14-bit FRAME_ID, FS timestamp (ns), epoch |
| `capture_end(seq, outcome)` | from the capture ledger (`sys_datapath.cpp:36-54`): FE, next FS or core disable — **before the last stream beats leave** | OK, DROPPED (Control FSM drop: CRC, ECC, WC, lane abort, line buffer, format bridge, core disable), MISSING_FE (closed by the next FS; the CSI counts it delivered) |
| `capture_stream_end(seq)` | after `capture_end(seq)`: the last beat of the capture (its abort beat included) has left the stream master, or was suppressed by its frame gate or flushed (D1a review F2, D2a review F1); per capture, not ordered with the boundaries of other captures (D2a review F2) | capture sequence |
| `reset(epoch)` | `resetn`, `soft_reset`, full reset: open captures end with no ledger event | outcome RESET for every open capture |

### 9.1 Context entries and binding

One table of **8 context entries** (CAPABILITY.CTX_DEPTH) holds every
capture the ingress tracks, from its `capture_start` until it is either
released internally or popped by firmware. There is no separate pending-start
queue and no terminal FIFO: the entry reserved at the start is the place
where that capture's terminal, stream and ISP outcomes are kept, so a
terminal can never be lost for lack of space.

| State | Entered on | Holds | Leaves on |
|---|---|---|---|
| free | construction, release, POP | — | `capture_start` |
| PENDING | `capture_start(seq, id, ts)` with a free entry | CAPTURE_SEQ (key), FRAME_ID, TS, EPOCH; CSI outcome once `capture_end(seq)` arrives (before or after the SOF) | accepted SOF with `vp_capture_seq` = seq (→ ACTIVE or released), retirement (rule 5), reset (released) |
| ACTIVE | SOF admitted (§8) | + TAG, stream outcome when the frame ends at the ingress, ISP retirement | all three outcomes known (→ READY); a reset (→ CANCELLING) |
| CANCELLING | a CSI or ISP reset while ACTIVE | outcome RESET latched; the reservation, TAG and output slot stay held | the ISP has retired or abandoned the frame safely (rule 8) (→ READY) |
| READY | final outcome known and nothing of the frame outstanding | final OUTCOME, LINES, SAMPLES | firmware POP of the head |

Rules:
1. **Reservation at `capture_start`.** A start that finds a free entry takes
   it. A start that finds none is **untracked**: no entry is created,
   evicted or overwritten; counter "captures untracked", sticky CTX_FULL.
   Its frame is later dropped at its SOF with NO_CONTEXT (§7). The capture
   stays untracked even if an entry becomes free before its SOF.
2. **Terminal before or after the SOF.** `capture_end(seq, outcome)` is
   stored in the entry with that CAPTURE_SEQ, whatever its state. An end with
   no entry (untracked capture) is counted and ignored. FRAME_ID is never used
   to look an entry up.
3. **Binding by capture sequence.** The accepted SOF beat binds to the entry
   whose CAPTURE_SEQ equals its `vp_capture_seq` (§7), never to "the oldest"
   entry, never by FRAME_ID and never to a value read from a "latest" CSR.
4. **Not admitted at the SOF** (NO_CREDIT): the entry's stream side is closed
   at once; it is released as soon as its CSI outcome is also known. Only
   counters and LAST_DROP record it; **no readable record** is queued, so
   any number of dropped frames needs no storage.
5. **Captures that never put an SOF on the stream** (dropped before the first
   beat left the CSI, or a frame with no lines): a PENDING entry is retired
   with NO_STREAM when an SOF of a **newer** capture is accepted, tracked or
   not (`vp_capture_seq` > the entry's CAPTURE_SEQ, plain integer compare; the
   CSI emits captures in order on one linear stream), at once on its
   `capture_stream_end(seq)`, or on reset, a SOURCE switch or the manager's
   stop. Any number of untracked captures in between
   changes nothing. It is
   released once its CSI outcome is known; counter NO_STREAM. Retirement
   never touches entries newer than the SOF (they may still be in the CSI's
   buffers).
6. **ACTIVE → READY** when the CSI outcome, the ingress stream outcome, the
   ISP retirement on every enabled output **and the stream boundary** are all
   known. The boundary is `capture_stream_end(seq)`, or a beat of a newer
   capture (in-order stream), or a reset or source switch. ISP retirement means
   the frame has left the pipeline and every enabled ODMA channel has
   finished it, published (DONE) or abandoned (buffer handed back free), with
   every write response received. Admitted frames
   become READY in admission order (the CSI, the ingress and the ISP are all
   in order), so the readable entries form a FIFO; the head is shown at
   0x040.
7. **POP** frees the head entry. Firmware that never pops leaves at most 8
   READY entries: new captures become untracked and their frames are
   dropped (NO_CONTEXT) with TREADY 1; nothing deadlocks, admitted frames
   in flight still complete because their entries were reserved, and the
   ISP's credits and ODMA buffers are not consumed by dropped frames. After
   a POP, the first capture that **starts** afterwards is tracked again.
8. **Reset** (§10). PENDING entries are released at once: they have no ISP
   work, and the reset has cleared the ingress FIFO and line assembly, so no
   beat of theirs can arrive. An ACTIVE entry is **not** released and not
   made readable: it latches outcome RESET and becomes CANCELLING, keeping its
   reservation, TAG and output slot. It becomes READY (visible, POP-able, its
   slot reusable by the manager) only when nothing of that frame is
   outstanding:
   - **CSI reset** (the ISP keeps running): the ingress sends the abort marker
     down the pipeline (the existing `line.abort` path, the same as for an
     invalid frame); the entry waits for the ISP retirement of rule 6 (the
     ODMA abandons the frame, hands its buffer back free, all write
     responses received). The CSI reset does not reset the ISP.
   - **ISP soft / common reset, or an error recovery**: the entry waits until
     the reset has been released and the ISP is drained: no admitted frame
     unretired and no AXI transaction outstanding (the IDLE condition of ISP
     MAS v1.07 §9.9.3; model `frames_in_flight == 0 && axi_outstanding == 0`).
     The ingress uses that condition, not `DMA_STAT.ODMA_BUSY`. (Correction,
     D1b measurement: the model resets the engines' internal busy count at
     once, but the DMA_STAT register keeps ODMA_BUSY = 1 until the reset
     pulse, which follows the drain; ODMA_BUSY therefore does not drop early
     in the model. The earlier statement that "the reset clears it while a
     write is outstanding" described the internal count, not the register.)
     A recovery for an error (AXI read / write response, malformed frame)
     gives the aborted frames outcome **ISP_FAILED**; a software soft /
     common reset gives **RESET**. Frames the ISP had already retired (all
     outputs DONE) are not affected by an ISP reset: they keep waiting for
     their CSI terminal / stream boundary, with the stream checks of §7
     (extra data, late abort) still applied until that boundary, and
     complete normally (D1b review F1). PENDING entries are released by an
     ISP reset or recovery as by a CSI reset (D1b review F2): a later SOF of
     such a capture finds no entry (dropped NO_CONTEXT), its terminal is
     ignored; it is then neither tracked nor counted untracked.
   - If the ISP is itself stalled (ODMA without a free buffer, DDR stall), the
     entry stays CANCELLING until the ISP resumes or the manager's recovery
     resets the ISP (D3); both end in one of the two cases above. Its
     reservation keeps counting against the 8 entries meanwhile.

Accounting at any time: `free + PENDING + ACTIVE + CANCELLING + READY = 8`;
`ACTIVE + CANCELLING ≤ 4` (ISP context credits, released only by the ISP's
retirement or its drained reset); a capture is either tracked in exactly one entry or
counted untracked; every not-admitted frame is counted once by reason
(NO_CREDIT, NO_CONTEXT, NO_STREAM).

TAG = sequence of **admitted** frames (32-bit, from 1, never 0), assigned at
admission, travels in the frame context through the pipeline to the ODMA's
`BUF_TAG`; the CSI FRAME_ID of the bound entry goes to `BUF_TS`, as the
memory path's camera firmware writes it (P29, D1 decision); the full FS
timestamp stays in the context entry.

### 9.2 Outcome and publish

**Terminal success** of a direct frame = `capture_end(OK)` for its capture
**and** the ingress received exactly HEIGHT complete lines with no abort beat
or TUSER[1] **and** the ISP retired it on every enabled output. The entry gets
its final OUTCOME when the last of the three is known, in any order
(`capture_end` often comes first, possibly before the SOF, rule 2).

**Publish rule (manager, P26):** a direct NV12 output is handed to the
application only when its ODMA completion (tag T) and the context entry with
TAG T and OUTCOME OK are both present. An entry with any other outcome makes
the manager release the output slot without publishing (counted). A
completion whose context does not arrive within a bounded time is released
as invalid. ISP `BUF_DONE`/`FRAME_DONE` semantics are unchanged.

### 9.3 Regression required (D1 synthetic beats, D2 with the CSI)

- Terminal OK before the accepted SOF (TREADY held low, the review repro):
  the frame binds to its own entry, all beats valid, published with its own
  TAG/TS.
- Two captures close together with different IDs/timestamps, the second
  started while the first is still in the CSI buffers: each frame gets its
  own ID/TS; retirement does not touch the newer one.
- A capture dropped before its SOF between two good ones: retired NO_STREAM
  only when the next SOF arrives; the next frame keeps its own context.
- TREADY reopened after a long stall: tag/context of the held frame
  unchanged, nothing published for the wrong frame.
- Firmware never POPs: admitted frames in flight plus several NO_CREDIT /
  NO_CONTEXT frames; the reserved entries keep their terminals, no new frame
  is admitted, no deadlock; after POP the next capture that starts is tracked
  and published correctly.
- Untracked capture whose entry would free before its SOF: still dropped.
- Capture sequence across a FRAME_ID wrap (16383 → 1) and half range
  (8192 captures), with untracked captures and firmware not popping: old
  PENDING / READY entries never match a new capture of the same FRAME_ID;
  NO_STREAM retirement still happens; terminals of old READY entries are not
  overwritten. D1 drives the hook with synthetic sequences; D2 runs a real
  FRAME_ID wrap from a test-only preset of the CSI counter: D2a at the CSI
  (`fx1_csi2_observer_wrap`), D2b through the platform with the RAW dump
  (`--camera-frame-id-preset 16381`, `camera_direct_dump_wrap`; P33).
- CSI soft and full reset while ODMA is writing a frame, with slow write
  responses and firmware polling/POPping immediately: the entry stays
  CANCELLING (not readable, slot not reused) until the ISP retirement; no
  stale write or event of the old frame after the next frame starts.
- ISP soft reset with outstanding writes: READY only after the reset release
  and `axi_outstanding == 0`, not when ODMA_BUSY drops.

## 10. Reset

- Platform power-on reset is shared by the CSI and the ISP (one reset domain,
  as the MAS v1.4 integration constraint requires).
- CSI `resetn`/`soft_reset`/full reset drop TVALID without TLAST: the reset
  hook makes the ingress abort the open frame (abort marker down the
  pipeline), clear its FIFO and line assembly, release PENDING entries and
  move ACTIVE ones to CANCELLING; they become READY only after the ISP has
  retired the frame (§9.1 rule 8, P27). The capture sequence is not reset.
- ISP soft/common reset and error recovery clear the ingress the same way
  (its FIFOs are registered in the ISP reset, `ctx.fifos`), with the ISP's
  epoch; ACTIVE entries the ISP has not retired become READY only after the
  reset release with no AXI transaction outstanding (§9.1 rule 8), with
  RESET (software reset) or ISP_FAILED (error recovery).
- `i_rst_n` (platform reset) returns ISP_INGRESS to its reset values: SOURCE
  memory, empty table, counters 0; the TAG counter restarts.
- Software rule (manager): stop the ISP and the ingress before resetting the
  CSI; the hooks make a violation safe, not supported. The exception for the
  manager stop is described below (P35).
- Compliant reset in the vertical blanking with the non-continuous clock
  (P16): the clock lane is in LP-11, so `fx1_csi2_soft_reset` returns
  `FX1_CSI2_ENOCLK` (C20): the datapath reset, the epoch increment and the
  observer's `source_reset` are done; Lane Management applies its part at the
  next byte clock, before the next FS. The manager treats ENOCLK there as
  success (D2b `camera_direct_csi_reset`: the next capture has epoch + 1 and
  is published).
- **Manager stop, capture left without terminal (D3 review F1, P35).** A
  frame the ISP had retired (all outputs DONE) keeps its entry until its CSI
  terminal / stream boundary (§9.1 rule 8). If the sensor stopped before
  sending it (a capture without FE, then no FS), nothing ever ends it, the
  reservation refuses the SOURCE switch, and the ISP stop does not help.
  After its ISP stop the manager drains the READY entries; if CTX_COUNT.USED
  is still non-zero it issues one CSI soft reset (ENOCLK = done). This is the
  one case where the CSI is reset with SOURCE still direct: the ISP is
  stopped, its reset has cleared the ingress FIFOs and line assembly, and
  only retired entries remain, so the hook only supplies their boundary. The
  entry becomes READY with RESET, is drained (never published), then SOURCE
  memory is written and read back. Until that holds, stop returns EBUSY with
  the state unchanged; the manager is never idle with SOURCE direct or a
  context entry left. A normal stop issues no CSI reset (D3
  `camera_mgr_direct_stop_no_fe`). While the stop is pending, nothing
  starts a capture (TPG frame, start), publishes or arms an output; only
  stop proceeds (re-review D3 R1, `camera_mgr_direct_stop_pending`).

## 11. Direct + CSI RAW DMA (dump)

Baseline coupling only (no `stream_follow_dma_drop`): a DMA refusal or
overflow does not affect the stream (M20); the shared `fb_ready` couples
stalls; with `dma_enable` = 1 `capture_end` is reported at the DMA
finalisation. RAW dumps are associated with direct frames by CSI FRAME_ID
(DMA buffer FRAME_ID ↔ context FRAME_ID). The ISP IDMA still reads nothing.

**Outcome of a direct frame with the dump on (P33, D2b).** The CSI capture
ledger (`sys_datapath.cpp` `resolve_ledger`) gives one outcome per capture,
and the direct frame takes it:

| DMA outcome of the capture | CSI `capture_end` | Stream | Direct frame |
|---|---|---|---|
| Valid (every write OKAY) | OK | whole frame | published |
| Refused at the claim (no HW_OWNED side, misaligned base/stride, M04/M10/M11) | DROPPED | **whole frame** | CSI_DROPPED, never published (its ODMA buffer, if completed, released unread) |
| Rewound, `err_frame_policy` 0 (AXI write error, layout error M03, RAW FIFO overflow M20, `dma_abort` M16) | DROPPED | **whole frame** | CSI_DROPPED, never published |
| Released with error, `err_frame_policy` 1 (same causes) | **OK** | whole frame | **published** (the stream was not affected; only the dump is incomplete: SW_OWNED_ERROR + BUFFER_ERROR_INFO) |
| Control-FSM drop (CRC, ECC, WC, lane abort, line buffer, format bridge, core disable) | DROPPED | cut, then abort | CSI_DROPPED, never published (the DMA side is aborted per its policy, counted dropped either way) |
| Valid, while the stream master is draining / injecting an earlier abort (TREADY held low after a drop: sink mode, every beat taken and discarded, the frame gate blocks the SOF, `stream_out.h` `fbs_ready`) | OK | none | no context entry: retired NO_STREAM at the ingress; **the dump is whole and valid** (D2b `camera_direct_dump_stall`: FRAME_IDs 6 and 7 dumped during a 5-frame ISP pause) |

So, at the baseline, the direct path follows the CSI's view of the capture:
a dump failure under policy 0 costs the direct frame although its pixels
reached the ISP; under policy 1 it does not. A policy that decouples them
(`stream_follow_dma_drop`) is migration C4, not claimed here.

A capture can therefore have a valid dump and no direct frame; software that
associates dumps by FRAME_ID must also release the sides of captures that
never got a context entry (FRAME_ID older than the next entry), or both sides
stay SW-owned and every later capture is Refused (first D2b runs).

**Firmware slower than the frame rate.** With two sides, the side of
capture N must be given back before the claim of capture N + 2. Firmware
that spends longer than one frame period per frame (first D2b run: NV12 CRC
plus RAW compare, ~4.4 ms of simulated CPU time at a 3 ms period) falls
further behind each frame until a capture finds no HW_OWNED side: Refused,
so the direct frame is CSI_DROPPED with all its lines (raw IRQ_STATUS:
FRAME_DROPPED only, no BUFFER_OVERFLOW / LINE_BUFFER_FULL / AXI error;
3 ms diagnostic run of `camera_direct_dump_wrap`). The D2b tests use an 8 ms
frame period so that the checks of a frame end in its blanking.

**Fault injection on fx1_soc (VP only, not a SoC model).** `--csi-dma-fault
write-error:N`: the N-th CSI RAW DMA write gets SLVERR (`csi_dma_fault.h`).
MIPI_DMA path: CSI DMA socket → traffic counter (with `--traffic-report`) →
fault shim → write guard → fabric (D2b review F1, P34): the counter counts
the injected write as issued and returned with an error; the write reaches
neither the guard nor DDR; the shim's report (writes seen) equals the
counter's issued writes. `--camera-fault FRAME:crc[:LINE]` /
`FRAME:no-fe`: the sensor sends an inverted payload CRC on one line / no FE
for one frame (`CcsSensorConfig::faults`, frames counted from 1 since
construction). `--camera-frame-id-preset N`: the CSI FRAME_ID counter is set
to N after the power-on reset (`debug_set_frame_id_counter`, test hook).

Test layout (`fw/fx1_soc/tests/camera_direct/recover.c`): two sides of
4 KiB at 0x8100_0000 / 0x8101_0000 (disjoint, the gap between them is not a
side), RAW16, stride 2W. The MIPI_DMA counter is given the two sides as
windows (`--traffic-window MIPI_DMA:LO:HI`) and checks every transaction at
issue: one not wholly inside one side (a write into the gap, a burst past
the end of a side) is counted `outside` (D2b review F2, P34; global lo / hi
cannot tell those apart). The host checker requires the counter's windows to
be the two sides, `outside` = 0, write errors exactly the injected ones
(`--mipi-errors`, 0 by default) and, with the shim, its writes seen = the
counter's issued writes; no ISP IDMA traffic.

## 12. DDR use

| Range | Owner | Direct path |
|---|---|---|
| 0x8000_0000–0x8040_0000 | firmware image | — |
| 0x8050_0000–0x8100_0000 FW_SHARED | ISP stream desc/status/records +0x100000.., profile +0x110000, SYS_DMA chain +0x200000; NPU job/result/lock 0x8065_0000–0x8066_2000; NPU small arena 0x8090_0000 (4 MiB) | unused |
| 0x8100_0000–0x8500_0000 | ISP host-stream input slots (4 × 16 MiB) | — |
| 0x8200_0000–0x8400_0000 | camera CSI sides (2 × 16 MiB) | **optional RAW dump only** |
| 0x8400_0000–0x8800_0000 | camera RAW10 staging (4 × 16 MiB) | not used |
| 0x8800_0000–0x8C00_0000 | ISP host-stream output slots | — |
| 0x8900_0000–0x8E00_0000 | camera NV12 slots 4 × 0x140_0000 (Y, UV at +0xC0_0000) | **NV12 outputs** |
| 0x8E00_0000–0x9000_0000 | NPU large arena (32 MiB) | — |
| 0x9020_0000–0xA000_0000 | ISP host-stream source pool | — |

The host-stream slots overlap the camera regions; they are never used at the
same time because the ISP has one owner (`isp_claim`). The direct path needs
no new region and allocates no RAW buffer; RAW layout of the memory path is
unchanged.

## 13. Information for the ISP owner (D2.1, delivered right after D0)

**Delivered: information and data only** (scope clarified 2026-11-20): no
repository, source, patch or build. The ISP owner receives one self-contained
document, [AXIS_STREAM_FOR_ISP_OWNER.md](AXIS_STREAM_FOR_ISP_OWNER.md), with:
the CSI → AXI4-Stream data path, the signal mapping (Table 6-67 vs. what the
model drives), how to decode TDATA / TKEEP / SOF / EOL / TID for RAW10 and
RAW12, the handshake rules, and worked examples taken from the traces; plus
the sample data files it describes (expected beats, per-cycle traces,
accepted beats) for nominal RAW10/RAW12, stall, partial tail, packet error,
WC error and forced abort.

**Internal verification only:** the harness that produces and checks those
files, `components/FX1_Components/mipi-csi2/examples/axis_source_demo`
(target `fx1_csi2_axis_source_demo`, tests `fx1_csi2_axisdemo_*`, its README
for build/run): the real `Csi2RxController` programmed through its CSR
socket, with the required D-PHY object and the CSI's own `AxisMaster`, and an
example `AxisSink` bound with `set_stream_sink()`; no ISP, NV12, FreeRTOS or
CSI DMA.

| Session | Source | Shows |
|---|---|---|
| nominal RAW10, RAW12 | TPG 32×8 | READY = 1, full beats, SOF/EOL, pixel order |
| stall | TPG | deterministic READY pattern: stalls on the SOF beat, mid-line, on the EOL beat; held beat stable |
| forced abort | TPG | `axis_enable` 1→0 mid-frame → null abort beat `KEEP 0, LAST 1, USER 0b10` |
| tail | sensor BFM through the D-PHY, RAW12 34×8 | partial last beat TKEEP = 0x0F |
| packet error | sensor BFM, a line with a bad CRC | capture dropped, null abort beat after the line |
| late packet error | sensor BFM, bad CRC on the last line | all H lines leave complete, then the null abort beat: payload complete ≠ validated (§9.2) |
| WC error | sensor BFM, a long packet whose WC is not a whole pixel group | TUSER[1] on the line's last beat |

Expected pixels and RAW16 bytes come from an independent generator
(TPG formula C35 / D11; the harness's own BFM pattern), never from a DUT
trace. Trace format (CSV, one row per executed cycle):
`cycle,time_ps,tvalid,tready,tdata,tkeep,tlast,tuser,tid,accepted` (`tdata`
hex, pixel 0 in the low 16 bits) and an accepted-beats list. Only Table 6-67
signals appear; the VP sideband fields `vp_capture_seq` / `vp_frame_id` and the capture events (§9) are
not AXI4-Stream signals and are not part of the delivery.
