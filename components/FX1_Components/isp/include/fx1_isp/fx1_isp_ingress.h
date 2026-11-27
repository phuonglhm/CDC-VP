// SPDX-License-Identifier: Apache-2.0
// FX1 ISP -- VP-only stream ingress of the direct CSI-2 -> ISP path.
//
// NOT an interface of the FTEL ISP v1.07 (a memory-to-memory core): the VP
// gives the ISP's internal IDMA -> Input Formatter boundary (ISP MAS v1.07
// Table 6-15) a second pixel source, the CSI-2 RX AXI4-Stream master (CSI-2 RX
// MAS v1.4 Table 6-67, RAW16). The platform binds it (D2); the ISP has no
// dependency on the CSI-2 RX component: an adapter maps the CSI's AxisSink
// and capture observer onto this interface.
//
// Contract: components/FX1_Components/mipi-csi2/docs/DIRECT_CSI_ISP_CONTRACT.md.
// Registers: fx1_isp/fx1_isp_ingress_regs.h (ingress_csr_socket).
#pragma once

#include <cstdint>

namespace cdc::components::fx1_isp {

// One AXI4-Stream transfer as the CSI-2 RX master drives it (RAW16 stream).
struct ingress_beat {
   std::uint64_t tdata = 0;  // 4 samples, sample n in bits [16n+15:16n]
   std::uint8_t tkeep = 0;   // valid bytes, a prefix; 0x00 only on the null abort beat
   bool tlast = false;       // end of line
   std::uint8_t tuser = 0;   // [0] SOF, [1] error / abort
   std::uint8_t tid = 0;     // exposure index
   // VP sideband, not AXI4-Stream signals (contract §9): the CSI capture the
   // beat belongs to (0: none) and its 14-bit CSI FRAME_ID.
   std::uint64_t capture_seq = 0;
   std::uint16_t frame_id = 0;
};

// Terminal outcome of a CSI capture (contract §9, P25).
enum class capture_outcome { ok, dropped, missing_fe };

class stream_ingress {
public:
   virtual ~stream_ingress() = default;
   // One executed cycle of the stream master: what it drives, and the TREADY
   // it gets for this cycle. Never blocks and never waits; a beat answered
   // with TREADY = 0 is not consumed (it is offered again).
   virtual bool axis_cycle(bool tvalid, const ingress_beat &beat) = 0;
   // CSI capture observer (contract §9): start at FS / first TPG beat, always
   // before the capture's SOF beat; end from the CSI capture ledger, before or
   // after the SOF beat is accepted.
   virtual void capture_start(std::uint64_t seq, std::uint16_t frame_id, std::uint64_t timestamp_ns,
                              std::uint32_t epoch) = 0;
   virtual void capture_end(std::uint64_t seq, capture_outcome outcome) = 0;
   // The stream boundary of capture `seq`: its last beat has left the stream
   // master (or was suppressed by its frame gate); no further beat of `seq`
   // follows. Usually after capture_end (not ordered with it). An admitted frame's context becomes
   // READY only after it (or after a beat of a newer capture, a reset or a
   // source switch): data beyond HEIGHT is still checked until then.
   virtual void capture_stream_end(std::uint64_t seq) = 0;
   // CSI reset (resetn / soft / full): TVALID dropped without TLAST (§10).
   virtual void source_reset(std::uint32_t epoch) = 0;
   // A stream master is bound (CAPABILITY.BOUND). Set before the simulation.
   virtual void set_bound(bool bound) = 0;
};

}  // namespace cdc::components::fx1_isp
