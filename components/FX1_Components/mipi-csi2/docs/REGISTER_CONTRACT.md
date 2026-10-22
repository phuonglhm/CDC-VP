# CSI-2 RX VP — register contract (workbook v1.4)

Status: **draft for review, 2026-10-19.** Baseline for M2. The field table in
§4 is generated from `FTEL_MEMORY_MAP_REGISTERS_v1_4.xlsx` by
`tools/csr_workbook.py --markdown`; do not edit it by hand, regenerate it.
Behaviour beyond the workbook text follows [spec_decisions.md](spec_decisions.md)
(IDs D.., C.., M.., P..).

## 1. Global rules

| Rule | Value | Ref |
|---|---|---|
| Aperture | 4 KiB; 0x000–0x0FF decoded; 0x100–0xFFF DECERR | D07 |
| Width / alignment | 32-bit registers; TLM length 1/2/4 inside one word, byte enables honoured; otherwise SLVERR | C04 |
| Reserved bits / offsets in 0x00–0xFF | read 0, writes ignored (0x1C, 0x3C, 0x5C, 0x8C, 0xB8, 0xBC, 0xDC, 0xF8, 0xFC) | D04, D07 |
| RO / WO / RW1C | C01; W1C race: set wins | C01, C02 |
| Debug access | read side-effect free; debug write unsupported | C05 |
| Reset values | as workbook; `max_lanes` reads the build parameter (MAX_LANES=4 → `11`) | §4 |

## 2. Reset scope

| Control | Registers (config) | Status / counters / IRQ_STATUS | Timestamp | GSP FIFO | Ownership | PHY |
|---|---|---|---|---|---|---|
| `resetn` (power-on) | reset values | cleared | cleared | flushed | `00`/`00`; writes in flight drained (`reset_drain_pending`) | reset pulse (P04) |
| `CORE_CONFIG.full_reset` (WO) | reset values | cleared | cleared | flushed | `00`/`00`; writes in flight drained (`reset_drain_pending`) | reset pulse |
| `CORE_CONFIG.soft_reset` (RW level; 0→1 starts the sequence) | preserved | cleared, exact list in C10; events suppressed while 1, stale-epoch events never recorded | preserved | flushed | SW_OWNED sides kept; side in flight drained (`reset_drain_pending`, also after a short pulse) and rewound without event, epoch+1 (M13, M15) | not reset |
| `PHY_CONFIG.dphy_rst_req` (WO) | preserved | preserved | preserved | preserved | preserved | reset pulse |
| `core_enable` 1→0, `dma_enable` 1→0, `dma_abort` 0→1 ("disable") | preserved | events of the abort recorded normally | preserved | preserved | capture in flight aborted: drain, then finalised once per `err_frame_policy` (M16, M07) | not reset |

Ownership and IRQ for each case: transition table in spec_decisions §2.3.

`CORE_STATUS.reset_in_progress` (M17) =
`soft_reset` OR PHY reset pulse active OR `reset_drain_pending` OR
`disable_drain_pending` OR a byte-clock soft reset not yet applied (C20) OR a
`core_enable` 1→0 whose stop has not completed (C37: captures dropped,
Control FSM in ST_IDLE, stream abort accepted by the ISP).

- `reset_drain_pending` (M13) is latched at `soft_reset` 0→1, `full_reset`
  and `resetn`, and clears only when every write issued before the reset has
  its response and the side in flight is finalised without event. It does not
  depend on the level of `soft_reset`: a short `soft_reset` pulse keeps
  `reset_in_progress` = 1 until the old responses are back.
- `disable_drain_pending` runs from a disable cause with a capture in flight
  until its finalisation is published (M16).
- No side is claimed while `reset_in_progress` = 1.

It falls in the same timestamp as the last pending finalisation, so software
that reads 0 may reconfigure and reuse buffers: no old write can still land. Event recording is suppressed only for the reset causes, not
during a disable drain (C08). PHY calibration after the reset pulse (~340 µs,
PHY_ADAPTER_CONTRACT §3.2) is not covered: wait for stop-state.
`full_reset` scope over the register file follows the workbook text ("same
path as resetn"); open item in spec_decisions §4.

## 3. Dynamic fields and event sources

Status must come from the real source; no hard-coded "ready" values.

| Register.field | Source | Notes |
|---|---|---|
| PROTOCOL_CONFIG.active_lanes_applied | Lane Management, latched in ST_IDLE | 0 until first adoption |
| PHY_STATUS.cl_stopstate | PHY `stopstateclk` via adapter | PHY_ADAPTER_CONTRACT |
| PHY_STATUS.cl_ulps | `!cl_ulpsactivenot` | unavailable at model top: always 0 (P11) |
| PHY_STATUS.cl_rxclkactivehs | PHY `rxclkactivehs` | |
| PHY_STATUS.cl_ulpsclknot | PHY `rxulpsclknot` | not the same as cl_ulps; reads 1 after reset with this PHY (P11) |
| PHY_STATUS.init_done | adapter: FSM state 7 and lanes enabled (P05, P11) | never proof of calibration |
| LANE_STATUS.laneN_stopstate | PHY `stopstatedata_N` | |
| LANE_STATUS.laneN_sot_error / sot_sync_error | sticky from PHY `errsoths_N` / `errsotsynchs_N` of an enabled lane | cleared by soft_reset (C11) |
| LANE_STATUS.laneN_skewcalhs | PHY `rxskewcalhs` (single, common) | adapter contract: not per lane |
| CORE_STATUS.line_buffer_full, BUFFER_STATUS.linebuf_fill | Line Buffer, live level (C27) | |
| CORE_STATUS.active_src / src_switch_pending | TPG Mux, live (Switch Control FSM) | |
| BUFFER_STATUS.linebuf_overflow | sticky from a refused Line Buffer write | cleared by soft_reset |
| CORE_STATUS.* (other), LANE_ALIGN_STATUS.* | Packet Analyzer, Lane Management | |
| VCn_IMAGE_INFO1/2, EMBEDDED_DATA_INFO_VCn, EMB_EXTRACT_VCn_VALm | Frame Context, published at FE of a non-orphan, non-dropped sensor frame | C28, C32 |
| FRAME_COUNT | Frame Context `capture_done` (delivered capture) | C34, D06 |
| TIMESTAMP_LO/HI | Frame Context, loaded at capture open | C30 |
| ERROR_CAPTURE | Packet Analyzer triggers (D05), frame id / line from Frame Context | C33 |
| DMA_STATUS.active_buffer, FRAME_ID | DMA, written at the claim (M18, M22) | |
| DMA_STATUS.dma_busy | DMA: claim to finalisation, and old writes draining (M17) | |
| BUFFER_OWNERSHIP | DMA finalisation (M07, M12); BUFFER_RELEASE | |
| BUFFER_ERROR_INFO | DMA, release with error (M08) | |
| AXI_FAULT_TRACE / AXI_FAULT_ADDR / AXI_ERROR_COUNT | DMA write response SLVERR / DECERR (M21) | |
| ECC/CRC/FRAME_DROP/LANE_ABORT/AXI_ERROR counters | blocks 2, 3, 10, 13 | saturating (C09) |
| IRQ_STATUS bits 0–8 | Packet Analyzer | |
| IRQ_STATUS bits 9, 10, 11 | PHY errsoths / errsotsynchs / stop-state entry of an enabled data lane | C14 |
| IRQ_STATUS bit 12 | rise of `linebuffer_full` while the sensor path is selected | |
| IRQ_STATUS bit 13 | rise of a Line Buffer overflow, receive FIFO overflow (C25), GSP FIFO overflow; DMA from M4 | |
| IRQ_STATUS bits 14, 15 | Control FSM `frame_drop_pulse`, Frame Context `capture_done` | D06, C34 |
| IRQ_STATUS bits 16–18 | DMA (M4) | |
| IRQ_STATUS bit 19 | Lane Management abort | |
| IRQ_STATUS bits 20, 21, 22, 23 | Frame Context resolution check, Byte-to-Pixel `err_wc_align`, Format Bridge overflow, Frame Context frame-number check | C36 |

### Registers with read or write side effects

| Register | Access | Side effect |
|---|---|---|
| CORE_CONFIG.full_reset | WO | PHY + datapath + registers reset; reads 0 |
| CORE_CONFIG.soft_reset | RW level | datapath reset while 1 |
| PHY_CONFIG.dphy_rst_req | WO | PHY reset pulse (P04); reads 0 |
| IRQ_STATUS | RW1C | clear; set-wins race (C02) |
| GENERIC_SHORT_PACKET | RO | bus read pops head; debug read peeks (D12) |
| TIMESTAMP_LO | RO | bus read snapshots HI (C06) |
| METADATA_RECORD_ID | RW | sampled at capture open |
| EXPECTED_RESOLUTION | RW | sampled at each Frame Start of the channel; 0 reports every frame (C36) |
| STREAM_CONFIG.stream_format | RW | 0 = RAW16, 1 = packed (workbook, C27); changed only while the path is disabled |
| STREAM_CONFIG.axis_enable | RW | 1→0: the line in flight at the ISP is closed with an abort beat |
| TPG_CONTROL.tpg_enable | RW | also selects the source: the switch takes effect when neither source has a frame in flight (CORE_STATUS.src_switch_pending) |
| BUFFER_RELEASE | WO | SW_OWNED_* → HW_OWNED; no effect on HW_OWNED (M11) |
| DMA_CONTROL.dma_abort | RW level | 0→1: abort in-flight capture, drain, finalise per policy (M16); while 1: no claim, starting captures dropped (M10) |
| DMA_CONTROL.dma_enable, CORE_CONFIG.core_enable | RW | 1→0 with a capture in flight: same abort as `dma_abort` (M16); `reset_in_progress` covers the drain (M17) |
| CORE_CONFIG.core_enable | RW | 1→0 latched (C37): open captures end as dropped, the drop sequence and the stream abort run even if the bit is set again at once; receive FIFO held flushed and `reset_in_progress` = 1 until the abort beat has been accepted |
| TPG_CONTROL.* (except tpg_enable) | RW | effective at next TPG frame |
| PROTOCOL_CONFIG.active_lanes | RW | effective in Lane Management ST_IDLE |

## 4. Field inventory (generated)

`Row` is the Excel row of sheet "Register Definition". `param` = value of a
build parameter.

| Offset | Register | Reset | Field | Bits | Access | Field reset | Row |
|---|---|---|---|---|---|---|---|
| 0x00 | IP_VERSION | 0x00010100 | ip_version | [31:0] | RO | 0x10100 | 5 |
| 0x04 | CORE_CONFIG | 0x00000001 | core_enable | [0] | RW | 0x1 | 7 |
|  |  |  | soft_reset | [1] | RW | 0x0 | 8 |
|  |  |  | full_reset | [2] | WO | 0x0 | 9 |
|  |  |  | Reserved | [31:3] | RO | 0x0 | 10 |
| 0x08 | PROTOCOL_CONFIG | 0x00000003 | active_lanes | [1:0] | RW | 0x3 | 12 |
|  |  |  | Reserved | [7:2] | RO | 0x0 | 13 |
|  |  |  | max_lanes | [9:8] | RO | param | 14 |
|  |  |  | active_lanes_applied | [11:10] | RO | 0x0 | 15 |
|  |  |  | vc_exposure_map | [12] | RW | 0x0 | 16 |
|  |  |  | Reserved | [31:13] | RO | 0x0 | 17 |
| 0x0C | PHY_CONFIG | 0x00000078 | phy_enable | [0] | RW | 0x0 | 19 |
|  |  |  | dphy_rst_req | [1] | WO | 0x0 | 20 |
|  |  |  | cl_enable | [2] | RW | 0x0 | 21 |
|  |  |  | dl_enable | [6:3] | RW | 0xF | 22 |
|  |  |  | forcerxmode | [10:7] | RW | 0x0 | 23 |
|  |  |  | Reserved | [31:11] | RO | 0x0 | 24 |
| 0x10 | METADATA_DT_CONFIG | 0x00000012 | metadata_dt | [5:0] | RW | 0x12 | 26 |
|  |  |  | Reserved | [7:6] | RO | 0x0 | 27 |
|  |  |  | metadata_capture_en | [8] | RW | 0x0 | 28 |
|  |  |  | fnum_check_en | [9] | RW | 0x0 | 29 |
|  |  |  | emb_extract_en | [10] | RW | 0x0 | 30 |
|  |  |  | Reserved | [31:11] | RO | 0x0 | 31 |
| 0x14 | STREAM_CONFIG | 0x00000003 | axis_enable | [0] | RW | 0x1 | 33 |
|  |  |  | stream_format | [1] | RW | 0x1 | 34 |
|  |  |  | Reserved | [31:2] | RO | 0x0 | 35 |
| 0x18 | EXPECTED_RESOLUTION | 0x00000000 | expected_width | [15:0] | RW | 0x0 | 37 |
|  |  |  | expected_height | [31:16] | RW | 0x0 | 38 |
| 0x20 | CORE_STATUS | 0x00000000 | reset_in_progress | [0] | RO | 0x0 | 40 |
|  |  |  | line_buffer_full | [1] | RO | 0x0 | 41 |
|  |  |  | sp_fifo_not_empty | [2] | RO | 0x0 | 42 |
|  |  |  | sp_fifo_full | [3] | RO | 0x0 | 43 |
|  |  |  | null_blank_seen | [4] | RO | 0x0 | 44 |
|  |  |  | active_src | [5] | RO | 0x0 | 45 |
|  |  |  | src_switch_pending | [6] | RO | 0x0 | 46 |
|  |  |  | dt_conflict | [7] | RO | 0x0 | 47 |
|  |  |  | Reserved | [11:8] | RO | 0x0 | 48 |
|  |  |  | vc_unsupported_seen | [12] | RO | 0x0 | 49 |
|  |  |  | Reserved | [31:13] | RO | 0x0 | 50 |
| 0x24 | PHY_STATUS | 0x00000000 | cl_stopstate | [0] | RO | 0x0 | 52 |
|  |  |  | cl_ulps | [1] | RO | 0x0 | 53 |
|  |  |  | cl_rxclkactivehs | [2] | RO | 0x0 | 54 |
|  |  |  | cl_ulpsclknot | [3] | RO | 0x0 | 55 |
|  |  |  | init_done | [4] | RO | 0x0 | 56 |
|  |  |  | Reserved | [31:5] | RO | 0x0 | 57 |
| 0x28 | LANE_STATUS | 0x00000000 | lane0_stopstate | [0] | RO | 0x0 | 59 |
|  |  |  | lane0_sot_error | [1] | RO | 0x0 | 60 |
|  |  |  | lane0_sot_sync_error | [2] | RO | 0x0 | 61 |
|  |  |  | lane0_skewcalhs | [3] | RO | 0x0 | 62 |
|  |  |  | Reserved | [4] | RO | 0x0 | 63 |
|  |  |  | Reserved | [7:5] | RO | 0x0 | 64 |
|  |  |  | lane1_stopstate | [8] | RO | 0x0 | 65 |
|  |  |  | lane1_sot_error | [9] | RO | 0x0 | 66 |
|  |  |  | lane1_sot_sync_error | [10] | RO | 0x0 | 67 |
|  |  |  | lane1_skewcalhs | [11] | RO | 0x0 | 68 |
|  |  |  | Reserved | [12] | RO | 0x0 | 69 |
|  |  |  | Reserved | [15:13] | RO | 0x0 | 70 |
|  |  |  | lane2_stopstate | [16] | RO | 0x0 | 71 |
|  |  |  | lane2_sot_error | [17] | RO | 0x0 | 72 |
|  |  |  | lane2_sot_sync_error | [18] | RO | 0x0 | 73 |
|  |  |  | lane2_skewcalhs | [19] | RO | 0x0 | 74 |
|  |  |  | Reserved | [20] | RO | 0x0 | 75 |
|  |  |  | Reserved | [23:21] | RO | 0x0 | 76 |
|  |  |  | lane3_stopstate | [24] | RO | 0x0 | 77 |
|  |  |  | lane3_sot_error | [25] | RO | 0x0 | 78 |
|  |  |  | lane3_sot_sync_error | [26] | RO | 0x0 | 79 |
|  |  |  | lane3_skewcalhs | [27] | RO | 0x0 | 80 |
|  |  |  | Reserved | [28] | RO | 0x0 | 81 |
|  |  |  | Reserved | [31:29] | RO | 0x0 | 82 |
| 0x2C | PACKET_COUNT | 0x00000000 | packet_count | [31:0] | RO | 0x0 | 84 |
| 0x30 | FRAME_COUNT | 0x00000000 | frame_count | [31:0] | RO | 0x0 | 86 |
| 0x34 | BUFFER_STATUS | 0x00000000 | linebuf_fill | [15:0] | RO | 0x0 | 88 |
|  |  |  | linebuf_overflow | [16] | RO | 0x0 | 89 |
|  |  |  | sp_fifo_overflow | [17] | RO | 0x0 | 90 |
|  |  |  | meta_overflow | [18] | RO | 0x0 | 91 |
|  |  |  | Reserved | [31:19] | RO | 0x0 | 92 |
| 0x38 | LANE_ALIGN_STATUS | 0x00000000 | lm_fsm_state | [2:0] | RO | 0x0 | 94 |
|  |  |  | abort_sync_err | [3] | RO | 0x0 | 95 |
|  |  |  | abort_fifo_full | [4] | RO | 0x0 | 96 |
|  |  |  | abort_unarmed | [5] | RO | 0x0 | 97 |
|  |  |  | deskew_burst_seen | [6] | RO | 0x0 | 98 |
|  |  |  | Reserved | [7] | RO | 0x0 | 99 |
|  |  |  | abort_lane_sync_seen | [11:8] | RO | 0x0 | 100 |
|  |  |  | Reserved | [31:12] | RO | 0x0 | 101 |
| 0x40 | GLOBAL_IRQ_ENABLE | 0x00000000 | global_enable | [0] | RW | 0x0 | 103 |
|  |  |  | Reserved | [31:1] | RO | 0x0 | 104 |
| 0x44 | IRQ_STATUS | 0x00000000 | frame_level_err_vc0 | [0] | RW1C | 0x0 | 106 |
|  |  |  | frame_sync_err_vc0 | [1] | RW1C | 0x0 | 107 |
|  |  |  | frame_level_err_vc1 | [2] | RW1C | 0x0 | 108 |
|  |  |  | frame_sync_err_vc1 | [3] | RW1C | 0x0 | 109 |
|  |  |  | crc_error | [4] | RW1C | 0x0 | 110 |
|  |  |  | ecc_1bit_error | [5] | RW1C | 0x0 | 111 |
|  |  |  | ecc_2bit_error | [6] | RW1C | 0x0 | 112 |
|  |  |  | unsupported_dt | [7] | RW1C | 0x0 | 113 |
|  |  |  | wc_corruption | [8] | RW1C | 0x0 | 114 |
|  |  |  | sot_error | [9] | RW1C | 0x0 | 115 |
|  |  |  | sot_sync_error | [10] | RW1C | 0x0 | 116 |
|  |  |  | stop_state | [11] | RW1C | 0x0 | 117 |
|  |  |  | line_buffer_full | [12] | RW1C | 0x0 | 118 |
|  |  |  | buffer_overflow | [13] | RW1C | 0x0 | 119 |
|  |  |  | frame_dropped | [14] | RW1C | 0x0 | 120 |
|  |  |  | frame_received | [15] | RW1C | 0x0 | 121 |
|  |  |  | buf_done_ok | [16] | RW1C | 0x0 | 122 |
|  |  |  | buf_done_err | [17] | RW1C | 0x0 | 123 |
|  |  |  | axi_write_error | [18] | RW1C | 0x0 | 124 |
|  |  |  | lane_align_abort | [19] | RW1C | 0x0 | 125 |
|  |  |  | resolution_mismatch | [20] | RW1C | 0x0 | 126 |
|  |  |  | err_wc_align | [21] | RW1C | 0x0 | 127 |
|  |  |  | fmtbridge_overflow | [22] | RW1C | 0x0 | 128 |
|  |  |  | frame_num_seq_err | [23] | RW1C | 0x0 | 129 |
|  |  |  | Reserved | [31:24] | RO | 0x0 | 130 |
| 0x48 | IRQ_ENABLE | 0x00000000 | irq_enable_mask | [23:0] | RW | 0x0 | 132 |
|  |  |  | Reserved | [31:24] | RO | 0x0 | 133 |
| 0x4C | ECC_ERROR_COUNT | 0x00000000 | ecc_1bit_count | [15:0] | RO | 0x0 | 135 |
|  |  |  | ecc_2bit_count | [31:16] | RO | 0x0 | 136 |
| 0x50 | CRC_ERROR_COUNT | 0x00000000 | crc_error_count | [31:0] | RO | 0x0 | 138 |
| 0x54 | FRAME_DROP_COUNT | 0x00000000 | frame_drop_count | [31:0] | RO | 0x0 | 140 |
| 0x58 | LANE_ABORT_COUNT | 0x00000000 | lane_abort_count | [15:0] | RO | 0x0 | 142 |
|  |  |  | Reserved | [31:16] | RO | 0x0 | 143 |
| 0x60 | VC0_IMAGE_INFO1 | 0x00000000 | byte_count | [15:0] | RO | 0x0 | 145 |
|  |  |  | line_count | [31:16] | RO | 0x0 | 146 |
| 0x64 | VC0_IMAGE_INFO2 | 0x00000000 | data_type | [5:0] | RO | 0x0 | 148 |
|  |  |  | width_mismatch | [6] | RO | 0x0 | 149 |
|  |  |  | height_mismatch | [7] | RO | 0x0 | 150 |
|  |  |  | Reserved | [31:8] | RO | 0x0 | 151 |
| 0x68 | VC1_IMAGE_INFO1 | 0x00000000 | byte_count | [15:0] | RO | 0x0 | 153 |
|  |  |  | line_count | [31:16] | RO | 0x0 | 154 |
| 0x6C | VC1_IMAGE_INFO2 | 0x00000000 | data_type | [5:0] | RO | 0x0 | 156 |
|  |  |  | width_mismatch | [6] | RO | 0x0 | 157 |
|  |  |  | height_mismatch | [7] | RO | 0x0 | 158 |
|  |  |  | Reserved | [31:8] | RO | 0x0 | 159 |
| 0x70 | GENERIC_SHORT_PACKET | 0x00000000 | data_type | [5:0] | RO | 0x0 | 161 |
|  |  |  | virtual_channel | [7:6] | RO | 0x0 | 162 |
|  |  |  | data | [23:8] | RO | 0x0 | 163 |
|  |  |  | Reserved | [31:24] | RO | 0x0 | 164 |
| 0x74 | EMBEDDED_DATA_INFO_VC0 | 0x00000000 | embedded_word_count_vc0 | [15:0] | RO | 0x0 | 166 |
|  |  |  | embedded_line_count_vc0 | [23:16] | RO | 0x0 | 167 |
|  |  |  | Reserved | [31:24] | RO | 0x0 | 168 |
| 0x78 | EMBEDDED_DATA_INFO_VC1 | 0x00000000 | embedded_word_count_vc1 | [15:0] | RO | 0x0 | 170 |
|  |  |  | embedded_line_count_vc1 | [23:16] | RO | 0x0 | 171 |
|  |  |  | Reserved | [31:24] | RO | 0x0 | 172 |
| 0x7C | METADATA_RECORD_ID | 0x00000000 | metadata_record_id | [31:0] | RW | 0x0 | 174 |
| 0x80 | FRAME_ID | 0x00000000 | frame_id_buf0 | [13:0] | RO | 0x0 | 176 |
|  |  |  | Reserved | [15:14] | RO | 0x0 | 177 |
|  |  |  | frame_id_buf1 | [29:16] | RO | 0x0 | 178 |
|  |  |  | Reserved | [31:30] | RO | 0x0 | 179 |
| 0x84 | TIMESTAMP_LO | 0x00000000 | timestamp_lo | [31:0] | RO | 0x0 | 181 |
| 0x88 | TIMESTAMP_HI | 0x00000000 | timestamp_hi | [31:0] | RO | 0x0 | 183 |
| 0x90 | DMA_CONTROL | 0x00000010 | dma_enable | [0] | RW | 0x0 | 185 |
|  |  |  | dma_start | [1] | RW | 0x0 | 186 |
|  |  |  | dma_abort | [2] | RW | 0x0 | 187 |
|  |  |  | dma_format | [3] | RW | 0x0 | 188 |
|  |  |  | err_frame_policy | [4] | RW | 0x1 | 189 |
|  |  |  | Reserved | [31:5] | RO | 0x0 | 190 |
| 0x94 | DMA_BUFFER0_ADDR | 0x00000000 | dma_buffer0_addr | [31:0] | RW | 0x0 | 192 |
| 0x98 | DMA_BUFFER1_ADDR | 0x00000000 | dma_buffer1_addr | [31:0] | RW | 0x0 | 194 |
| 0x9C | DMA_BUFFER_SIZE | 0x00000000 | dma_buffer_size | [31:0] | RW | 0x0 | 196 |
| 0xA0 | DMA_ATTR | 0x00000000 | awqos | [3:0] | RW | 0x0 | 198 |
|  |  |  | max_burst_len | [11:4] | RW | 0x0 | 199 |
|  |  |  | awprot | [14:12] | RW | 0x0 | 200 |
|  |  |  | Reserved | [15] | RO | 0x0 | 201 |
|  |  |  | max_outstanding | [22:16] | RW | 0x0 | 202 |
|  |  |  | Reserved | [31:23] | RO | 0x0 | 203 |
| 0xA4 | DMA_STATUS | 0x00000000 | dma_busy | [0] | RO | 0x0 | 205 |
|  |  |  | active_buffer | [1] | RO | 0x0 | 206 |
|  |  |  | Reserved | [31:2] | RO | 0x0 | 207 |
| 0xA8 | LINE_PITCH | 0x00000000 | bytes_per_line | [15:0] | RW | 0x0 | 209 |
|  |  |  | line_padding | [23:16] | RW | 0x0 | 210 |
|  |  |  | Reserved | [31:24] | RO | 0x0 | 211 |
| 0xAC | BUFFER_OWNERSHIP | 0x00000000 | buf0_state | [1:0] | RO | 0x0 | 213 |
|  |  |  | buf1_state | [3:2] | RO | 0x0 | 214 |
|  |  |  | Reserved | [31:4] | RO | 0x0 | 215 |
| 0xB0 | BUFFER_RELEASE | 0x00000000 | release_buf0 | [0] | WO | 0x0 | 217 |
|  |  |  | release_buf1 | [1] | WO | 0x0 | 218 |
|  |  |  | Reserved | [31:2] | RO | 0x0 | 219 |
| 0xB4 | BUFFER_ERROR_INFO | 0x00000000 | buf0_last_valid_line | [15:0] | RO | 0x0 | 221 |
|  |  |  | buf1_last_valid_line | [31:16] | RO | 0x0 | 222 |
| 0xC0 | TPG_CONTROL | 0x00000000 | tpg_enable | [0] | RW | 0x0 | 224 |
|  |  |  | pattern | [1] | RW | 0x0 | 225 |
|  |  |  | tpg_dual_vc_en | [2] | RW | 0x0 | 226 |
|  |  |  | tpg_pixel_format | [3] | RW | 0x0 | 227 |
|  |  |  | Reserved | [31:4] | RO | 0x0 | 228 |
| 0xC4 | ERROR_CAPTURE | 0x00000000 | fault_line_number | [15:0] | RO | 0x0 | 230 |
|  |  |  | fault_frame_id | [29:16] | RO | 0x0 | 231 |
|  |  |  | fault_vc | [31:30] | RO | 0x0 | 232 |
| 0xC8 | AXI_FAULT_TRACE | 0x00000000 | awid | [7:0] | RO | 0x0 | 234 |
|  |  |  | resp | [9:8] | RO | 0x0 | 235 |
|  |  |  | Reserved | [31:10] | RO | 0x0 | 236 |
| 0xCC | AXI_FAULT_ADDR | 0x00000000 | fault_awaddr | [31:0] | RO | 0x0 | 238 |
| 0xD0 | AXI_ERROR_COUNT | 0x00000000 | bresp_slverr_count | [15:0] | RO | 0x0 | 240 |
|  |  |  | bresp_decerr_count | [31:16] | RO | 0x0 | 241 |
| 0xD4 | TPG_RESOLUTION | 0x00000000 | tpg_width | [15:0] | RW | 0x0 | 243 |
|  |  |  | tpg_height | [31:16] | RW | 0x0 | 244 |
| 0xD8 | TPG_BLANKING | 0x00000000 | tpg_hblank | [15:0] | RW | 0x0 | 246 |
|  |  |  | tpg_vblank | [31:16] | RW | 0x0 | 247 |
| 0xE0 | EMB_EXTRACT_CFG0 | 0x00000000 | len | [2:0] | RW | 0x0 | 249 |
|  |  |  | Reserved | [7:3] | RO | 0x0 | 250 |
|  |  |  | byte_offset | [23:8] | RW | 0x0 | 251 |
|  |  |  | line_idx | [31:24] | RW | 0x0 | 252 |
| 0xE4 | EMB_EXTRACT_CFG1 | 0x00000000 | len | [2:0] | RW | 0x0 | 254 |
|  |  |  | Reserved | [7:3] | RO | 0x0 | 255 |
|  |  |  | byte_offset | [23:8] | RW | 0x0 | 256 |
|  |  |  | line_idx | [31:24] | RW | 0x0 | 257 |
| 0xE8 | EMB_EXTRACT_VC0_VAL0 | 0x00000000 | value | [31:0] | RO | 0x0 | 259 |
| 0xEC | EMB_EXTRACT_VC0_VAL1 | 0x00000000 | value | [31:0] | RO | 0x0 | 261 |
| 0xF0 | EMB_EXTRACT_VC1_VAL0 | 0x00000000 | value | [31:0] | RO | 0x0 | 263 |
| 0xF4 | EMB_EXTRACT_VC1_VAL1 | 0x00000000 | value | [31:0] | RO | 0x0 | 265 |
