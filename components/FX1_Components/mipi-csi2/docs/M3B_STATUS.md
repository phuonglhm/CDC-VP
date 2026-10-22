# M3b status — sys_clk datapath: unpack, line buffer, format bridge, frame context, Control FSM, TPG, stream output

Date: 2026-10-20. Not committed. Scope agreed 2026-10-20: one review round for
Blocks 4–10 and 12 in the linear profile. Embedded data: accounting and field
extraction, no Record Builder / metadata stream (D04). Benchmark: one 640×480
RAW10 frame on 4 lanes. Decisions added: C24–C38 (C37, C38 from the review), C17 updated, D05
refined by C33 ([spec_decisions.md](spec_decisions.md) §2.5).

## What exists

| Part | Files | Notes |
|---|---|---|
| Byte-to-Pixel Unpacking, Test Pattern Generator, TPG Mux, Line Buffer (Blocks 4–7) | `controller/include/fx1_csi2/pixel_path.h`, `controller/src/pixel_path.cpp` | MAS formulas: carry/pad/sync-pending logic, colour bars + blanking + 4-state FSM, Switch Control FSM with armed mask, SRAM FIFO with FWFT output/skid stage and end-of-line slot |
| Format Bridge, AXI4-Stream Master (Blocks 8, 12) | `controller/include/fx1_csi2/stream_out.h`, `controller/src/stream_out.cpp` | bypass re-packer and RAW16 formatter with shared line holds, two output FIFOs with one-entry almost-full margin; skid buffer, Finalize Sequencer (abort beats), frame gate |
| Frame Context, Control FSM (Blocks 9, 10) | `controller/include/fx1_csi2/frame_control.h`, `controller/src/frame_control.cpp` | Capture Tracker (2 slots), Identity Generator, Geometry Accumulator, Resolution Checker, Embedded Data Path with extractor, Context Latch; Cause Collector, Capture Watch, Drop FSM, Drop Report |
| sys_clk datapath | `controller/include/fx1_csi2/sys_datapath.h`, `controller/src/sys_datapath.cpp` | receive FIFO after Lane Management (C25), Packet Analyzer and Blocks 4–12 wired per cycle (C24); `AxisSink` / `DmaBeatSink` consumer interfaces |
| Stream checking sink | `controller/include/fx1_csi2/axis_monitor.h`, `controller/src/axis_monitor.cpp` | TREADY pattern, held-valid / TKEEP / TID / SOF rules, line reassembly per TID; reusable as the platform sink monitor (M5) |
| Controller integration | `controller/src/csi2_rx_controller.cpp` | `SC_THREAD` at `sys_clk` (250 MHz default) with idle skipping (C26); CSR: CORE_STATUS live levels, BUFFER_STATUS, FRAME_COUNT, FRAME_DROP_COUNT, VCn_IMAGE_INFO*, EMBEDDED_DATA_INFO_*, EMB_EXTRACT_*, TIMESTAMP at capture open, ERROR_CAPTURE frame id / line; IRQ 12–15, 20–23; `set_stream_sink`, `set_dma_sink` |

All new units are in `fx1_csi2_rx_core` (no SystemC). No PHY model source, BFM,
driver, platform or FW file changed in M3b.

## Exit-gate items covered (plan §6)

| Item | Status | Evidence |
|---|---|---|
| RAW10 5 B → 4 px, RAW12 3 B → 2 px; carry reset at line start/end; WC alignment errors, tail bytes | PASS | `fx1_csi2_sys_b2p_raw10/_raw12` (20 widths, every WC mod 4, idle gaps), `b2p_wc_align` (MAS Table 622 cases, line without its end-of-line beat, CRC-only end beat), `pipe_wc_align` (TUSER[1] on that line's TLAST beat only). Oracle: source pixels packed by the BFM packer, not by the DUT |
| Linear context: frame ID, line count, geometry, timestamp; completion not deferred to the next FS | PASS | `pipe_frame_*`, `fc_checks` (ids 1..n, resolution width/height, RAW10/RAW12 references, frame-number check, lost FE → sync close), `fx1_csi2_rxphy_frame_*` (FRAME_COUNT, IRQ 15 and every line at the ISP right after FE with the sensor stopped; VC0_IMAGE_INFO1/2; TIMESTAMP) |
| Record handle | N/A | METADATA_RECORD_ID is sampled at capture open (Identity Generator) but only the metadata record carries it: deferred with metadata DMA (D04) |
| Bounded Line Buffer / Format Bridge; D-PHY never back-pressured; FIFO full → event, drop, recovery | PASS | `pipe_lb_full` and `fx1_csi2_rxphy_stall_drop` (ISP stalled: max fill 1740 = FULL_WATERMARK, IRQ 12, frame dropped, Line Buffer and Format Bridge emptied by the flush, next frame whole, no overflow, `dropped_bytes` 0 at Lane Management); `pipe_lb_overflow` (reaction budget too small: refused write, sticky + IRQ 13, drop, recovery); `fb_formats` (consumer stalled at every residual alignment: no beat lost) |
| Receive FIFO occupancy, service rate, overflow (C17 gate) | PASS | `fx1_csi2_rxphy_frame_*`: max fill ≤ 3 at 250 MHz against 100 Mword/s; `pipe_cdc`: `sys_clk` at 50 MHz loses words, abort marker in place, frame dropped, short lines afterwards delivered |
| TPG pattern / format / geometry (D11, C35), safe switch, minimum / invalid dimensions; no packet/CRC counters | PASS | `tpg_pattern` (RAW10/12 × vertical/horizontal × widths 8–66 incl. truncation × heights 8–16, independent oracle), `tpg_timing` (frame period formula with hblank/vblank, guard, stop at end of frame, quick re-enable, stalls), `mux_unit`, `pipe_tpg` (switch with no sensor, back to RX, switch requested during an open sensor frame waits for its FE), `fx1_csi2_rxphy_tpg` (CSR only, PACKET_COUNT 0, no geometry published, src_switch_pending until the sensor's first SOF) |
| Unit stream sink: TDATA 64, TKEEP, SOF/TUSER[0], TLAST/EOL, TUSER[1], TID, held-valid under stall | PASS | `AxisMonitor` in every pipeline and PHY test (0 violations); `axis_skid` (random TREADY), `axis_abort` (held beats delivered before the abort beat, frame gate) |
| Frame in error dropped, next valid frame recovers | PASS | `pipe_crc_drop`, `fx1_csi2_rxphy_errors` (CRC, ECC 2-bit, lane abort: FRAME_DROP_COUNT 2, FRAME_COUNT then 1 with the recovered frame whole), `ctl_core_disable`, `ctl_unit` (command sequencing of MAS Tables 654/655) |
| Exact samples and frame boundaries for both depths and 1/2/4 lanes through the PHY | PASS | `fx1_csi2_rxphy_frame_raw10_4lane`, `_raw12_2lane`, `_raw10_1lane`: stream lines byte-exact, SOF only on the first beat, TID 0 |
| DTs other than RAW10/RAW12 not taken as image | PASS | unchanged from M3a (`pa_routing`); Byte-to-Pixel receives image beats only, embedded beats go to Frame Context only (`fc_checks`) |
| Benchmark of a medium frame through the PHY | done | below |

## Benchmark (fx1_csi2_rxphy_bench_640x480, label `fx1_bench`)

One 640×480 RAW10 frame (384 000 payload bytes) on 4 lanes at 800 Mb/s,
through the detailed PHY: **1.255 ms simulated in 150 s wall** (8.4 µs simulated
per wall second, 2.6 KB/s of payload), peak RSS 428 MB; the reviewer measured
154.7 s and 417 MiB on the same machine. The sys_clk datapath alone
(`fx1_csi2_sys_pipe_bench`, same frame, no SystemC): 252 024 cycles in
0.025 s. The run through the PHY is therefore dominated by the part outside the
datapath model (PHY netlist, BFM, wrapper, SystemC kernel and scheduling); this
split has not been profiled further.

Consequence for M5/M6 (to be decided then): by linear extrapolation in payload
bytes, not measured, one 1920×1080 RAW10 frame (2 592 000 bytes, 6.75 × the
benchmark) would take about 17 min wall through the PHY. Smoke tests through the PHY should use small
frames; large-frame and frame-rate tests need a PPI-level source that bypasses
the PHY model, reported as such. The benchmark was not repeated after the
review fixes (C37, C38): they change no per-byte path.

## Findings

1. **STREAM_CONFIG.stream_format encoding differs between workbook and MAS** (workbook: 0 = RAW16; MAS Block 8: 1 = RAW16). Workbook followed (C27).
2. **TIMESTAMP update point differs** (workbook: at capture open; MAS: at the end of a delivered capture). Workbook followed (C30).
3. **FRAME_COUNT** advances at every capture end in MAS Block 9, dropped or not; D06 (delivered only) kept (C34).
4. **Abort mask on the sensor path**: MAS sends an abort beat on both TIDs; in the linear profile only TID 0 is used (C29).
5. **EXPECTED_RESOLUTION resets to 0** and has no enable: every frame raises `resolution_mismatch` until FW programs it (C36). The frame-number check after reset flags a first frame numbered above 1 (C36).
6. On the stream path the frame gate of the stream master already hides the beats a missing flush would leave behind; the flush is observable through the Line Buffer / Format Bridge levels (and, from M4, the DMA path). Tests check those levels directly (mutation evidence below).
7. Several Block 6/10 properties (output mask, discard set in the decision cycle, HOLD length) are not observable at the outputs of a single-VC pipeline; they are covered by unit tests of the block (`mux_unit`, `ctl_unit`).

## Deviations and limits (all in spec_decisions)

- C24: Packet Analyzer input register and ECC latency not modelled; RTL latencies may differ by a few cycles.
- C25: receive FIFO depth 16 is a VP value; its overflow has no BUFFER_STATUS bit in v1.4.
- C28/C29: linear profile only; `tpg_dual_vc_en`, `vc_exposure_map` have no effect; no stream skip on DMA drop.
- Not built: Record Builder, metadata stream and `meta_overflow` (D04); DMA claim, FRAME_ID, `dma_drop` / `dma_abort_req` consumers (M4).
- The `PayloadSink` tap of M3a now observes the Packet Analyzer outputs after the discard gate.

## Mutation check

Each mutation applied alone to a copy of the source, the unit-test binary
rebuilt, the named case run, the source restored (MD5 checked).

| Mutation | Detected by |
|---|---|
| carry not cleared at line start | `b2p_wc_align` |
| sync flag on a zero-pixel cycle | `b2p_raw10` |
| no end-of-line reservation in Line Buffer | `lb_fwft` |
| read issue ignores the read in flight | `lb_fwft` |
| fb_ready high during the flush cycle | `fb_formats` |
| almost-full margin 0 | `fb_formats` |
| raw12 re-pack nibble order | `fb_formats` |
| stream master ready ignores the skid register | `axis_skid` |
| frame gate kept open on abort | `axis_abort` |
| FLUSH does not flush | `pipe_lb_full` |
| discard not set in the decision cycle | `ctl_unit` |
| HOLD one cycle | `ctl_unit` |
| capture opened during the drop forgotten (lead_seen) | `ctl_unit` |
| truncated header not a drop cause (C31) | `ctl_unit` |
| core_enable 1→0 not latched (C37) | `ctl_core_pulse`, `rxphy_core_pulse_lat0`, `_lat1` |
| latched stop released before Control FSM reaches ST_IDLE (C37) | `ctl_core_pulse` |
| stop completed without waiting for the stream abort (C37, R1) | `ctl_core_pulse`, `rxphy_core_pulse_lat0`, `_lat1`, `_lat20` |
| receive FIFO not held flushed during the stop (C37, R1) | `ctl_core_pulse` (stop between bursts) |
| WC = 0 image packet consumes the SOF (C38) | `pipe_zero_wc`, `rxphy_zero_wc` |
| WC = 0 image packet counted as a line by the Frame Supervisor (C38, R2) | `pipe_zero_wc`, `rxphy_zero_wc` |
| core_enable low not a drop cause | `ctl_core_disable` |
| capture ends only at the next FS | `pipe_frame_raw10` |
| no output mask in TPG Mux | `mux_unit` |
| switch away from TPG with Line Buffer not empty | `mux_unit` |
| switch while the outgoing source presents a beat | `mux_unit` |
| TPG blanking reload one cycle late | `tpg_timing` |
| no abort marker after a receive FIFO loss | `pipe_cdc` |
| RAW12 width reference wrong | `fc_checks` |
| extraction byte window off by one | `fc_checks` |

Before the last additions six of these were missed (carry, margin, flush,
discard timing, output mask, marker); the tests listed were added for them.

## Test run

After the re-review fixes: `ctest -L fx1_csi2 -j4` (2026-10-20): **100/100
passed**, 96 s wall (66 `fx1_csi2_unit`, 34 `fx1_phy_slow`). Before the first
review: 94/94, 87 s. New: 23 `fx1_csi2_sys_*` unit cases
(milliseconds each, `pipe_bench` 0.03 s), `fx1_csi2_rxphy_stall_drop` (14 s),
`fx1_csi2_rxphy_tpg` (8 s). The benchmark has its own label `fx1_bench` and is
not in the default set (`ctest -L` takes a regex, so a label containing
`fx1_csi2` would be selected). M2 and M3a reviewer repros, run from copies
against this build: `resetn=0 size3=0 timeout=0` and
`reset=0 header=0 footer=0 order=0` (the M2 copy needs `libfx1_csi2_rx_core.a`
on its link line, which it predates). The CDC-VP tree configures with
`FX1_BUILD_MIPI_CSI2=ON`.

Also in this round (M3a review, non-blocking note): the three CSR test HALs
wait for the delay returned by `b_transport`; `soft_reset_latency` checks that
the driver's soft reset takes at least the 20 ns of the two accesses.

## Review 2026-10-20 (plan/M3B_REVIEW_20261020.md) — resolution

| ID | Finding | Resolution |
|---|---|---|
| F1 (P1) | core_enable 0 then 1 between two sys_clk edges: Packet Analyzer and the receive FIFO were reset, but Frame Context and Control FSM never saw the low level; the interrupted capture was counted delivered and its prefix headed the next frame's line (25 bytes instead of 10) | C37: the 1→0 write latches a stop; the sys_clk domain sees core_enable = 0 until the open captures have ended as dropped and Control FSM is in ST_IDLE (HOLD/FLUSH/abort beat when needed); `reset_in_progress` covers it. Tests: `fx1_csi2_sys_ctl_core_pulse` (zero-time and one-cycle low × ISP stalled or not, capture open with a line half received, then an idle pulse; dropped once, abort beat, next frame exact with one SOF, stop pending through FLUSH until ST_IDLE); `fx1_csi2_rxphy_core_pulse_lat0/_lat1/_lat20` with the C99 driver (CSR latency 0, 1 ns = low shorter than sys_clk, 20 ns = multi-cycle low). |
| F2 (P2) | RAW packet with WC = 0 consumed the first-image mark: the next frame had pixels but no SOF and still counted as received | C38 (policy chosen by the user: accept, no error, do not consume SOF): the first-image mark is set by the first delivered payload beat. Tests: `fx1_csi2_sys_pipe_zero_wc` (RAW10 and RAW12; empty packet at frame start, after an embedded line, between lines, a frame of empty packets only, then a clean frame; scoreboard of one SOF per frame on its first line), `fx1_csi2_rxphy_zero_wc`. |
| non-blocking | no warning when FW sets `tpg_dual_vc_en` | VP warning on 0→1 of the bit; `fx1_csi2_rxphy_tpg` sets it, checks one warning, read-back 1 and TID 0 on every line. |
| non-blocking | benchmark attribution and 1080p estimate worded too strongly | Benchmark section reworded: not profiled, extrapolation marked as such. |

The reviewer's repro script, run from a copy against the fixed build: unit and
PHY `review_core_pulse` and `review_zero_wc` all exit 0 (were 1). PHY log:
`FRAME_COUNT=1 FRAME_DROP_COUNT=1 lines=2 last_line_bytes=10 aborts=1
violations=0`; `FRAME_COUNT=1 SOF=1`. The M2 and M3a repro copies still pass.

Mutation check of the fixes: without the latch, `ctl_core_pulse`,
`rxphy_core_pulse_lat0` and `_lat1` fail (`_lat20` passes, as expected: its low
level lasts several sys_clk cycles); releasing the latch after one cycle fails
`ctl_core_pulse`; consuming the SOF at the header again fails `pipe_zero_wc` and
`rxphy_zero_wc`. Sources restored and verified identical.

## Re-review 2026-10-20 (plan/M3B_REREVIEW_20261020.md) — resolution

| ID | Finding | Resolution |
|---|---|---|
| R1 (P1) | the latched stop completed (busy = 0) when Control FSM reached ST_IDLE, while the stream master was still in DRAIN with TVALID = 1 and no abort_done; a frame offered then was reported delivered but gated away | C37 completion now also requires the Finalize Sequencer back in PASSTHROUGH (abort beat accepted); while the stop is pending the receive FIFO is held flushed, so no capture opens. Tests no longer accept early completion: `ctl_core_pulse` (8 variants: zero-time / one-cycle low, ISP stalled or not, no sink, stop between bursts) checks that with TREADY low the stop stays pending, the held beat is stable, a frame offered meanwhile is never captured, and completion comes only after the abort beat; `rxphy_core_pulse_lat0/_lat1/_lat20` read `reset_in_progress` = 1 through CSR while stalled, 0 only after the abort beat. |
| R2 (P2) | WC = 0 still advanced the Frame Supervisor line count (PA 2, Geometry 1 for empty + one line) | C38: the Frame Supervisor line count advances with the first delivered payload beat of an image packet. `pipe_zero_wc` asserts the context after empty packets at frame start, end and between lines, and the line number reported with a CRC error after two empty packets (2); `rxphy_zero_wc` asserts the context. |

Reviewer follow-ups run from a copy against this build: `stop_unit=0
line_count=0 stop_PHY=0` (were 1/1/1). PHY log: `reset_in_progress=1
stop_pending=1 AXI_state=1 TVALID=1` while stalled, then PASS. The four
earlier M3b repros, the M3a and the M2 repro copies still pass.

Mutation check: dropping the stream-abort condition fails `ctl_core_pulse` and
all three `rxphy_core_pulse_*`; not holding the receive FIFO (both guards)
fails `ctl_core_pulse`; counting the line at the header again fails
`pipe_zero_wc` and `rxphy_zero_wc`. Sources restored and verified identical.
The PHY benchmark was not repeated (no per-byte path changed).
