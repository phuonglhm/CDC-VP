# M4a status — DMA RAW write path, layout, bounds, ownership

Date: 2026-10-20. Not committed. Scope agreed 2026-10-20: M4 in two review
rounds. **M4a** (this round): DMA engine (claim, layout and stride, 4 KiB
split, bounds, packed and RAW16), ownership and IRQ ordering, FRAME_ID, the
capture ledger, unit memory model and a TLM memory target. **M4b** (next):
abort / reset / disable drains with delayed responses, SLVERR / DECERR,
`err_frame_policy` under AXI errors, epochs, C99 driver API. RAW only (D04):
metadata DMA is N/A. Decisions added: M18–M22, C34 refined
([spec_decisions.md](spec_decisions.md) §2.3). Choices made by the user on
2026-10-20: M4a/M4b split, DMA overflow = fault and abort (M20), one TLM
transaction per burst with an `mm_aclk` parameter (M21), minimal driver API
in M4 (in M4b), Control-FSM drop always counted dropped (M19).

## What exists

| Part | Files | Notes |
|---|---|---|
| DMA engine (sys_clk side) | `controller/include/fx1_csi2/dma_engine.h`, `controller/src/dma_engine.cpp` | claim / refusal (M10, M04, M11, M22), beat intake by capture tag (M18), burst assembly (M06: max_burst_len + 1, 4 KiB, line end), layout (M03), bounds and 32-bit wrap (M05), RAW data and command FIFOs (M20), abort and finalisation per `err_frame_policy` (M07, M16), last_valid_line (M08), reset epoch and drain without event (M13) |
| Capture ledger | `controller/src/sys_datapath.cpp` | one outcome per capture (M19), in Frame Start order |
| Capture tag | `PxBeat::tag`, `OutBeat::tag` | VP sideband: frame id set at the Line Buffer write port, carried by Format Bridge (M18) |
| AXI4 write worker | `controller/src/csi2_rx_controller.cpp` (`dma_thread`, `dma_socket`, `Csi2AxiExtension`) | one `b_transport` per burst, `mm_aclk` 200 MHz default, BRESP mapping, AXI fault trace / counters (M21) |
| Register side | `DmaGlue` in the controller | BUFFER_OWNERSHIP, FRAME_ID at claim, DMA_STATUS busy / active_buffer, BUFFER_ERROR_INFO, IRQ 13/16/17/18; `dma_enable` 1→0, `dma_abort` 0→1 and `core_enable` 1→0 abort the DMA (M16) |
| Test memory target | `tests/tlm_mem.h` | DDR window with sentinel, latency, hook for delay / error (M4b), write log with AXI attributes |

## Exit-gate items of plan §7 covered in M4a

| Item | Status | Evidence |
|---|---|---|
| Write-only TLM initiator, worker `SC_THREAD`, bounded queue, no bus call from the PPI / sys_clk processes | PASS | design (M21); `fx1_csi2_rxphy_dma_sensor` |
| Address generation base / size / stride (M02), 4 KiB and line-end split, max burst (M06), tail byte enables, padding never written | PASS | `fx1_csi2_dma_frame_raw10/_raw12/_raw10_to16/_raw12_to16` (every line exact at base + n × stride, padding and the rest of the side untouched, every burst inside one line and one 4 KiB page); `fx1_csi2_dma_bursts` (max_burst_len 0 / 1 / 255, base 16 bytes below a 4 KiB boundary) |
| Bounds with overflow-safe arithmetic, no write outside the buffer | PASS | `fx1_csi2_dma_bounds` (line ending exactly at DMA_BUFFER_SIZE allowed, next one refused, last_valid_line; size 0; 32-bit wrap) |
| Layout errors | PASS | `fx1_csi2_dma_layout` (line longer than bytes_per_line: nothing of the capture written, policy 0 rewind / policy 1 release with error; shorter lines written, no error); `fx1_csi2_dma_claim_refused` (misaligned base / stride) |
| Configuration latched at the claim | PASS | `fx1_csi2_dma_cfg_latched` |
| Two sides HW_OWNED / SW_OWNED_VALID / SW_OWNED_ERROR; both SW-owned → capture dropped, nothing overwritten; release | PASS | `fx1_csi2_dma_pingpong`, `fx1_csi2_rxphy_dma_sensor` (through the CSR: BUFFER_OWNERSHIP, BUFFER_RELEASE, FRAME_ID halves, DMA_STATUS) |
| Completion only after every response; ownership and FRAME_ID before the event | PASS | `fx1_csi2_dma_frame_*` (event order `buf=SW_OWNED_VALID` → `buf_done_ok` → `delivered`, no write in flight at `buf_done`) |
| `frame_received` not used for `buf_done_ok`; counters invariant | PASS | M19 ledger; `fx1_csi2_dma_ctl_drop` (Control-FSM drop under both policies), `fx1_csi2_rxphy_dma_sensor` (4 captures = 3 delivered + 1 dropped) |
| DMA cannot keep up | PASS | `fx1_csi2_dma_overflow` (sensor: data FIFO full → abort, IRQ 13; single-beat bursts: command FIFO full; TPG: held, frame whole) |
| Back-to-back frames, FS of the next frame before the tail of the previous | PASS | `fx1_csi2_dma_back_to_back` |
| TPG frames to DDR | PASS | `fx1_csi2_rxphy_dma_tpg` (packed RAW12, both sides, CSR only) |
| Abort / reset / disable with delayed responses, SLVERR / DECERR, epoch | M4b | implemented; only the paths without a delayed response are covered here |
| Metadata DMA | N/A | D04 |

## Findings

1. **Beat-to-capture binding** is not specified by MAS for the case where the next Frame Start is decoded while the previous frame's tail is still in Line Buffer (MAS relies on `ctx_slot` at `ev_fs`). The VP carries the capture's frame id with every entry (M18). The RTL needs an equivalent; to be confirmed with IP.
2. **Switch TPG → RX** needs only an empty Line Buffer (MAS Block 6), so the last beats of a TPG frame can reach the DMA after `active_sel` is back to 0. A DMA that takes the back-pressure permission from `active_sel` loses them (found by `fx1_csi2_dma_overflow`); the source is latched per capture (M20).
3. **`max_burst_len` reset value 0** gives single-beat bursts; against a target slower than the beat rate the 16-entry command FIFO fills and the capture is aborted. FW must program `max_burst_len` (M20, D03).
4. **Counting**: D06 and MAS Block 11 disagreed for a Control-FSM drop whose buffer is released with error; resolved by the user (M19).

## Mutation check

Each mutation applied alone, the DMA unit binary rebuilt, the named case run,
the source restored (MD5 checked): no 4 KiB split → `bursts`; burst not closed
at line end → `frame_raw10`; max_burst_len ignored → `bursts`; bounds off by
one → `bounds`; claim ignores ownership → `pingpong`; last completed side not
tracked → `pingpong`; finalise before the responses → `frame_raw10`; no
capture tag → `frame_raw10`; TPG permission from `active_sel` → `overflow`;
alignment not checked → `claim_refused`; Control-FSM drop not counted dropped
→ `ctl_drop`; unissued data kept on abort → `layout`; line too long not
checked → `layout`; refused capture counted delivered → `claim_refused`,
`pingpong`; data FIFO limit ignored → `overflow`. **15/15 detected.**

## Test run

After the review fixes: `ctest -L fx1_csi2 -j4` (2026-10-20): **117/117
passed**, 102 s wall (80 `fx1_csi2_unit`, 37 `fx1_phy_slow`). Before: 115/115. New: 13 `fx1_csi2_dma_*` unit cases,
`fx1_csi2_rxphy_dma_sensor`, `fx1_csi2_rxphy_dma_tpg`. The M2, M3a and M3b
reviewer repro copies still pass.

## Review 2026-10-20 (plan/M4A_REVIEW_20261020.md) — resolution

| ID | Finding | Resolution |
|---|---|---|
| F1 (P1) | a beat of capture N closed the intake of **every** other ended capture, including a later one whose Frame End was decoded while its data was still behind N's tail (ISP stalled, stream + DMA on): the later side was released valid with nothing written, FRAME_COUNT 2 | `DmaEngine::beat()` closes only the captures opened **before** the beat's capture, in Frame Start order (no frame-id comparison, ids wrap); a beat of an unknown capture (orphan, already forgotten) closes nothing; the pipeline-empty rule is unchanged (M18 text holds). Tests: `fx1_csi2_dma_stream_backlog` (ISP stalled while 2 frames end: no completion, nothing released, Line Buffer holding data; after release both sides exact, 2 × `buf_done_ok`; variant with the second and third captures refused) and `fx1_csi2_rxphy_dma_stream_backlog` (through the PHY, CSR: FRAME_COUNT 0 and ownership 0 while stalled, then 0x5 / 2 / both sides exact). Reverting the rule fails both. |
| F2 (P2) | M09 promised to latch the format at the claim; Format Bridge applies `dma_format` live | Contract aligned with MAS Block 8 (user decision): **`dma_format` is not latched**; changed only with `dma_enable` = 0 and `dma_busy` = 0, otherwise unsupported with a VP warning; same for `stream_format` with `axis_enable`. `DmaConfig::raw16` removed (it controlled nothing). M09 rewritten; the M4b driver will enforce it. `fx1_csi2_rxphy_dma_sensor` checks the warning (enabled) and its absence (disabled and idle). |

Reviewer repros run from a copy (two lines adapted: `DmaConfig::raw16` no
longer exists): unit `stalled` 0, PHY `stalled` 0 (were 1), `ready` 0, `tiny`
0. `format` still exits 1 by design: it changes `dma_format` while the DMA is
enabled, which the contract no longer supports. The M2, M3a and M3b repro
copies still pass.
