# M3a status — PPI capture, Lane Management, Packet Analyzer

Date: 2026-10-20. Not committed. Scope agreed 2026-10-20: M3 in two review
rounds; this is M3a. Byte-clock-accurate Lane Management, functional Packet
Analyzer (C17). Decisions added: C16–C19, C13 corrected
([spec_decisions.md](spec_decisions.md)).

## What exists

| Part | Files | Notes |
|---|---|---|
| Receive ECC / CRC | `controller/include/fx1_csi2/csi2_protocol.h`, `controller/src/csi2_protocol.cpp` | SEC-DED check from the parity equations (independent of the BFM's column table), CRC-16 |
| Lane Management | `controller/include/fx1_csi2/lane_management.h`, `controller/src/lane_management.cpp` | MAS Block 2: Write Control, PHY delay FIFOs (8, usable 6), Lane Event Capture (C18), Burst FSM with transition priorities, common read, Lane Merging with drain flush, abort marker, lane-count adoption in ST_IDLE |
| Packet Analyzer | `controller/include/fx1_csi2/packet_analyzer.h`, `controller/src/packet_analyzer.cpp` | MAS Block 3: header trust boundary, ST_DISCARD, DT routing table, cut-through payload with CRC/footer, WC check at end of burst, per-VC Frame Supervisor, generic short packets, unsupported VC (linear profile: VC0 only) |
| Controller glue | `controller/src/csi2_rx_controller.cpp` | PPI sampled on every `rxbyteclkhs` edge (X/Z counted), soft_reset reaches Lane Management on byte-clock edges only; events → IRQ 0–8, 19, counters, LANE_ALIGN_STATUS, PROTOCOL_CONFIG.active_lanes_applied, CORE_STATUS stickies, ERROR_CAPTURE, GSP FIFO; payload and FS/FE to a `PayloadSink` (M3b plugs in here) |
| `fx1_csi2_rx_core` | CMake target | the three units above without SystemC |

## Exit-gate items covered in M3a (plan §6)

| Item | Status | Evidence |
|---|---|---|
| PPI capture on `rxbyteclkhs`, X/Z diagnostics | PASS | every PHY test checks `ppi_x_samples() == 0` |
| Lane alignment and merge 1/2/4 lanes, bounded FIFO | PASS | `fx1_csi2_rx_lm_merge`: both PPI conventions × 1/2/4 lanes × 4–41 bytes (all lengths mod 4) × skew 0–5 cycles × gaps × trail; `fx1_csi2_rxphy_skew`: 4/8/16 UI absorbed through the real PHY, 64 UI → `abort_fifo_full` |
| Burst abort marker in order, no mixing of bursts | PASS | `fx1_csi2_rx_lm_abort` (FIFO full, errsotsynchs over armed, unarmed, late lane, deskew burst and a normal burst after it), `fx1_csi2_rxphy_errors` (d), `fx1_csi2_rxphy_unarmed` |
| Lane count applied only in ST_IDLE, read-back | PASS | `fx1_csi2_rx_lm_lanes`, `active_lanes_applied` checked in every PHY case |
| Parser FS/FE, long/short packets, WC, VC/DT, null/blank/unsupported | PASS | `fx1_csi2_rx_pa_lines`, `pa_routing`, `pa_frames`, `pa_payload_errors` |
| ECC single-bit correction, double-bit detection | PASS | `fx1_csi2_rx_pa_ecc`: all 30 single-bit positions corrected, all 435 double-bit pairs detected and not routed, ECC bits 7:6 ignored; `fx1_csi2_rxphy_errors` (b, c) through the PHY |
| CRC-16, truncated packets without over-read | PASS | `pa_payload_errors` (payload byte, CRC byte split over words, truncated, header-only burst); CRC matches the MIPI example; `fx1_csi2_rxphy_errors` (a) bit inverted on the wire |
| Frame-level / frame-sync errors and recovery | PASS | `pa_frames`; `fx1_csi2_rxphy_errors` (a, d, f): frame-level error at FE, clean frame afterwards |
| End-to-end receive through the detailed PHY | PASS | `fx1_csi2_rxphy_frame_raw10_4lane`, `_raw12_2lane`, `_raw10_1lane`: 32×8 frames byte-exact per line, one SoF, PACKET_COUNT, no error IRQ, generic short packet in the FIFO |
| Tests detect faults | yes | seven mutations (C18 reverted — also fails the real-PHY frame —, FSM priority, parity-bit correction, CRC compare, trail parsing, drain flush) each fail their test; sources restored and verified identical |

Deferred to M3b: RAW10/RAW12 unpack, linear frame context (capture ID,
geometry, timestamp, FRAME_COUNT, frame_received, ERROR_CAPTURE frame id),
line buffer, format bridge, TPG and source mux, stream sink checks.

## Findings

1. **PPI timing of this PHY differs from MAS (C18).** `rxactivehs` and `rxsynchs` rise on the same edge and the first byte comes a cycle later; MAS Lane Management taken literally aborts every burst. The deviation is a single condition in Lane Event Capture.
2. **Abort cause reported as `abort_fifo_full` (C19).** With the PHY's trail bytes the waiting lanes' FIFOs fill before `errsotsynchs` or the end of the burst. `abort_lane_sync_seen` identifies the lane correctly.
3. **One packet per burst (C16).** MAS's Packet FSM assumes it; the PHY's trail bytes are harmless only because of it. A sensor sending several packets in one burst would lose every packet after the first: sensor configuration constraint, to be confirmed for the target sensor.
4. BFM trail raised to the D-PHY minimum (56 UI at 800 Mb/s); M1/M2 tests unchanged in result.

## Test run

`ctest -L fx1_csi2` after the review fixes (2026-10-20): **69/69 passed**,
81 s wall with `-j4` (41 `fx1_csi2_unit`, 28 `fx1_phy_slow`). The eight `fx1_csi2_rx_*` unit
tests run in milliseconds; a PHY case takes 7–8 s wall, dominated by
elaboration (~5 s) and PHY startup (~3 s); a 32×8 frame is 7.5–10.3 µs
simulated. Throughput at larger frames is measured in M3b.

## Review 2026-10-20 (plan/M3A_REVIEW_20261020.md) — resolution

| ID | Finding | Resolution |
|---|---|---|
| F1 (P1) | driver soft reset returned OK while Lane Management never saw the pulse | C20: soft_reset 0→1 latched towards the byte-clock domain; `reset_in_progress` waits for the edge that applies it (when the byte clock runs); driver returns `ENOCLK` with the byte clock stopped. Tests through the real PHY with the C99 driver: reset in Aligned (CSR latency 0 and 20 ns) — LM reset counter advanced before OK, rest of the burst discarded as unarmed, next frame exact; no-clock case → ENOCLK, request applied at the first edge. Unit test: reset from Arming/Aligned/Drain/Abort. Driver soft reset now takes one poll interval (10 µs default). |
| F2 (P2) | zero-padded short header produced a valid FS | C21: headers shorter than 4 bytes are never decoded; truncated-packet report with unknown channel; `pa_short_header` covers lengths 1–3 of zero / FS / FE / GSP / long-header prefixes inside and outside a frame, with recovery. |
| F3 (P2) | payload complete, CRC footer missing: no error | C22: truncated packet (`wc_corruption`, frame error), not a CRC error; `pa_missing_footer` covers 1/2 missing bytes for WC mod 4 = 0..3 and WC = 0. |
| F4 (P2) | CRC error before the EOL beat | C23: beat → integrity result → packet_done; `pa_order` checks the sequence for good/bad/truncated packets over all WC mod 4 with a consumer that flushes on error. |

Also from the review: C17 now states the ideal CDC FIFO explicitly and moves FIFO occupancy/overflow to the M3b gate.

The reviewer's repro script (run on a copy against the fixed build): `reset=0 header=0 footer=0 order=0`. One change in the copied reset repro: its main thread checked `worker_done` right after the burst, assuming the driver call returns in zero time; it now waits for the worker, bounded to 200 µs. The repro's own checks then pass: rc 0, LM Idle at driver return, reset_in_progress 0.

Mutation check of the fixes: removing the C20 latch, or the C20 wait in `reset_in_progress`, fails `soft_reset_aligned` on the real PHY; restoring zero padding fails `pa_short_header`; ignoring a missing footer fails `pa_missing_footer`; raising the CRC error before the beat fails `pa_order`. Sources restored and verified identical.
