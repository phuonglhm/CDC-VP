# M4b status — DMA drains, AXI errors, epochs, C99 driver API

Date: 2026-10-20. Not committed. Second review round of M4 (M4a closed by the
reviewer, plan/M4A_REVIEW_CLOSURE_20261020.md). Scope: reset / abort /
disable while a write is outstanding, SLVERR / DECERR, `err_frame_policy`
under errors, reset epochs, the C99 driver DMA API, a FW-style sequence
through the PHY. RAW only (D04). Decisions added: M23–M25
([spec_decisions.md](spec_decisions.md) §2.3). Choices made by the user on
2026-10-20: struct-based driver API without ping-pong bookkeeping (M25),
data of a failed write undefined (M24), one or two FW-style end-to-end cases.

## What changed

| Part | Files | Notes |
|---|---|---|
| DMA engine | `controller/src/dma_engine.cpp` | `last_valid_line` also counts the line of a write answered OKAY after an abort started (M24, M08). The drain, epoch and abort paths written in M4a are now exercised with held and failing writes. |
| C99 driver | `driver/include/fx1_csi2/fx1_csi2_drv.h`, `driver/src/fx1_csi2_drv.c` | `fx1_csi2_dma_cfg`, `dma_config`, `dma_start`, `dma_stop`, `dma_abort`, `buffer_release`, `get_buffer`, `get_dma_fault`, `stream_config` (M25) |
| Tests | `tests/test_csi2_dma_ctrl.cpp` (new), `tests/test_csi2_rx_phy.cpp` | controller with TPG source and a memory target whose hook holds or fails the n-th write; two FW-style cases through the PHY |

## Exit-gate items of plan §7 (M4b part)

| Item | Status | Evidence |
|---|---|---|
| Reset during a capture with a write outstanding: no new write, drain, `reset_in_progress` / `dma_busy` until the response, side finalised without event, nothing published from the old epoch (M13) | PASS | `fx1_csi2_dmactl_soft_reset_delayed` (two pulses during the drain, epoch moves, queued writes never issued, ownership 0, IRQ_STATUS 0, recovery), `_full_reset_delayed`, `_resetn_delayed` (busy past the PHY pulse and the reset pin, registers at reset, recovery after reprogramming) |
| Abort / disable with the response delayed, both `err_frame_policy` values, exactly one finalisation, `reset_in_progress` falls with it (M16, M17, M07) | PASS | `_abort_delayed_p0` (rewind, `frame_dropped`), `_abort_delayed_p1` (SW_OWNED_ERROR, last_valid_line 2 including the held line, `buf_done_err`, delivered), both: no `buf_done_ok`, no write after the release, nothing claimed while `dma_abort` = 1 (M10), driver abort and restart; `_disable_delayed` (`dma_enable` 1→0; a capture while disabled does not use the DMA) |
| `core_enable` 1→0 with a write outstanding (M16, M19) | PASS | `_core_disable_open` (capture open: dropped, buffer released with error, no `frame_received`), `_core_disable_after_fe` (capture already ended: delivered with error) |
| SLVERR / DECERR at the start, middle and end of a frame; fault address / ID; counters; IRQ 18; no write after the error (M16, M24) | PASS | `_slverr_first` (last_valid_line 0xFFFF), `_slverr_mid` (2), `_slverr_last` (6), `_decerr_mid_p0` (DECERR count, policy 0 rewind); recovery on the other side in each |
| Completion only after the last response, beyond the Frame End (M12) | PASS | `_late_response` (last write held 3 µs after FE: no ownership change, no event; at the response ownership, FRAME_ID, `buf_done_ok`, `frame_received` and the interrupt line together) |
| Driver API | PASS | `_driver_api` (EINVAL layouts incl. a side ending above 4 GiB, ESTATE while enabled and while busy with `dma_enable` 0, release of a HW-owned side, stream format change while enabled; graceful stop waiting for a held write; stop timeout then abort); `fx1_csi2_rxphy_drv_camera` (probe, PHY start, DMA config RAW16, IRQ, 3 frames verified from the handler, release, graceful stop, nothing written after the stop); `fx1_csi2_rxphy_drv_recover` (SLVERR on frame 2: `buf_done_err` + `axi_write_error`, fault address, last_valid_line, release, frame 3 valid; FRAME_COUNT 3, FRAME_DROP_COUNT 0) |
| QoS / PROT / ID | partly | carried in `Csi2AxiExtension`; not observable on the FX1 bus (M14, M21); ID checked in the write log |
| Metadata DMA | N/A | D04 |

## Findings

1. **`last_valid_line` after an abort** (fixed in this round): M4a ignored the OKAY answer of the write in flight when the abort started, so a line fully written was not reported. M08 counts it; M24 states it.
2. A capture that has already reached its Frame End when `core_enable` is cleared is not dropped by Control FSM; the DMA abort releases it with error and it counts as delivered (M19). FW sees `buf_done_err` with `frame_received` in that case, `frame_dropped` only when the capture was still open.
3. With CSR latency 0 a write issued by the worker (taken from the FIFO) completes even when a reset follows in the same timestamp (M23). The VP cannot recall it, as AXI cannot.

## Mutation check

Each mutation applied alone, the binary rebuilt, the named cases run, the
source restored (MD5 checked):

| Mutation | Detected by |
|---|---|
| drain ignores the write in flight | `soft_reset_delayed`, `full_reset_delayed`, `resetn_delayed` |
| abort finalised before the response | `abort_delayed_p0`, `abort_delayed_p1`, `disable_delayed` |
| OKAY after abort not counted | `abort_delayed_p1` |
| AXI error does not abort | `slverr_mid` |
| DECERR reported as SLVERR | `decerr_mid_p0` |
| reset keeps queued bursts | `soft_reset_delayed` |
| completion before the last response | `late_response` |
| `dma_abort` level ignored at the claim | `abort_delayed_p0` |
| driver config ignores busy | `driver_api` (after adding the busy-without-enable check) |
| driver stop does not wait | `driver_api` |
| driver abort does not wait the drain (both waits removed) | `driver_api` |

## Test run

After the review fixes: `ctest -L fx1_csi2 -j4` (2026-10-20): **140/140
passed**, 127 s wall (101 `fx1_csi2_unit`, 39 `fx1_phy_slow`). Before: 133/133. New: 14 `fx1_csi2_dmactl_*` (about 5 s
each, D-PHY elaboration), `fx1_csi2_rxphy_drv_camera`,
`fx1_csi2_rxphy_drv_recover`. The M2, M3a, M3b and M4a reviewer repro copies
keep their results (`format` of M4a still exits 1 by design: it changes the
format while the DMA is enabled, M09).

## Review 2026-10-20 (plan/M4B_REVIEW_20261020.md) — resolution

| ID | Finding | Resolution |
|---|---|---|
| F1 (P2) | `fx1_csi2_dma_abort()` gave its second wait (`reset_in_progress`) a fresh `timeout_us`: up to twice the caller's timeout (real controller: timeout 10 µs, OK after 15 µs) | One budget per public call: `wait_for()` now spends a budget passed by pointer. The same defect existed since M2 in `fx1_csi2_phy_start()` (PHY reset, clock-lane stop and lane stop each with a fresh timeout, up to three times): fixed the same way. On timeout `dma_abort()` returns ETIMEDOUT with `dma_abort` still set. Tests on a timeline fake HAL (time = sum of the delays): `fx1_csi2_drv_timeout_abort_in_budget` (OK after 8 µs of 10), `_abort_phase2_late` (busy clears at 8, reset at 14: ETIMEDOUT after exactly 10, abort left pending), `_abort_zero`, `_abort_below_poll`, `_phy_start_late`, `_phy_start_in_budget`. |
| F2 (P2) | `fx1_csi2_stream_config(dev, 1, 1)` from disabled + packed wrote format and enable in one write, which the controller reports as unsupported | From disabled, a new format is written first with the path disabled, then the enable; an enabled path still refuses a format change with ESTATE and writes nothing. `fx1_csi2_dmactl_driver_api`: no warning, CSR = enabled + RAW16, refusal leaves the CSR unchanged. Whether the stream pipeline has drained before a restart stays in the caller's flow. |
| suggestion | a failed burst that wrote part of its data | `fx1_csi2_dmactl_slverr_partial` (20 of 40 bytes land, then SLVERR); every `slverr_*` case now also checks that the failing line's padding and every later line are untouched. |

Mutation check: a fresh budget for the second wait of `dma_abort` fails
`drv_timeout_abort_phase2_late`; a fresh budget for the lane wait of
`phy_start` fails `drv_timeout_phy_start_late`; format and enable in one write
fails `dmactl_driver_api`. Sources restored and verified identical.

Reviewer repros run from a copy: fake HAL `late_success` 0, `timeout` 0,
controller `review_stream_enable` 0, `review_abort_budget` 0 (were 1), and
`review_partial_error` 0. The M2, M3a and M3b repro copies still pass.
