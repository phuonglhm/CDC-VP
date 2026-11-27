// Direct CSI-2 RX -> ISP binding (VP-only; plan DIRECT_CSI_ISP D2, contract
// components/FX1_Components/mipi-csi2/docs/DIRECT_CSI_ISP_CONTRACT.md).
//
// The CSI-2 RX controller's AXI4-Stream master (its AxisSink) and its
// read-only capture observer are adapted to the ISP's stream ingress
// (fx1_isp_ingress.h). Neither component depends on the other; only the
// platform knows both. Called from the CSI's sys_clk process: nothing here
// waits. With ISP_INGRESS.CTRL.SOURCE = memory (reset) the ingress answers
// TREADY = 1 and discards every beat, as an unbound sink would (P21).
#pragma once

#ifdef FX1_SOC_HAS_CAMERA

#include <fx1_csi2/packet_analyzer.h>
#include <fx1_csi2/sys_datapath.h>
#include <fx1_isp/fx1_isp_ingress.h>

#include "bw_recorder.h"

namespace cdc::platforms::fx1_soc {

class CsiIspDirect final : public fx1::csi2::AxisSink, public fx1::csi2::CaptureObserver {
public:
    explicit CsiIspDirect(cdc::components::fx1_isp::stream_ingress& ingress) : in_(ingress) {}

    bool axis_cycle(bool tvalid, const fx1::csi2::AxisBeat& b) override
    {
        cdc::components::fx1_isp::ingress_beat ib;
        ib.tdata = b.tdata;
        ib.tkeep = b.tkeep;
        ib.tlast = b.tlast;
        ib.tuser = b.tuser;
        ib.tid = b.tid;
        ib.capture_seq = b.vp_capture_seq;
        ib.frame_id = b.vp_frame_id;
        const bool ready = in_.axis_cycle(tvalid, ib);  // called once, its TREADY returned unchanged
        if (bw_) bw_->axis(tvalid, b.tkeep, b.tlast, b.tuser, b.vp_capture_seq, ready);
        return ready;
    }
    void capture_start(std::uint64_t seq, std::uint16_t frame_id, std::uint64_t ts_ns, std::uint32_t epoch) override
    {
        if (bw_) bw_->capture_start(seq, frame_id, ts_ns, epoch);
        in_.capture_start(seq, frame_id, ts_ns, epoch);
    }
    void capture_end(std::uint64_t seq, fx1::csi2::CaptureOutcome o) override
    {
        using cdc::components::fx1_isp::capture_outcome;
        if (bw_) {
            bw_->capture_end(seq, o == fx1::csi2::CaptureOutcome::Ok        ? "ok"
                                  : o == fx1::csi2::CaptureOutcome::Dropped ? "dropped"
                                                                            : "missing_fe");
        }
        in_.capture_end(seq, o == fx1::csi2::CaptureOutcome::Ok        ? capture_outcome::ok
                             : o == fx1::csi2::CaptureOutcome::Dropped ? capture_outcome::dropped
                                                                       : capture_outcome::missing_fe);
    }
    void capture_stream_end(std::uint64_t seq) override
    {
        in_.capture_stream_end(seq);
        if (bw_) bw_->capture_stream_end(seq);
    }
    void source_reset(std::uint32_t epoch) override
    {
        if (bw_) bw_->source_reset(epoch);
        in_.source_reset(epoch);
    }
    // VP-only bandwidth recorder (--bw-report); null: off. Read-only.
    void set_bw(BwRecorder* bw) { bw_ = bw; }

private:
    cdc::components::fx1_isp::stream_ingress& in_;
    BwRecorder* bw_ = nullptr;
};

// The CSI Packet Analyzer's received-payload view, to the bandwidth recorder.
class CsiRxToBw final : public fx1::csi2::RxPayloadObserver {
public:
    explicit CsiRxToBw(BwRecorder& bw) : bw_(bw) {}
    void rx_long_header(fx1::csi2::RxPacketClass c, unsigned vc, uint8_t dt, uint16_t wc, bool fwd) override
    {
        using C = fx1::csi2::RxPacketClass;
        bw_.rx_long_header(c == C::Image ? 0 : c == C::Embedded ? 1 : c == C::NullBlank ? 2 : c == C::UnsupportedDt ? 3 : 4,
                           vc, dt, wc, fwd);
    }
    void rx_payload_bytes(unsigned n) override { bw_.rx_payload_bytes(n); }
    void rx_packet_end(bool truncated, bool crc_ok) override { bw_.rx_packet_end(truncated, crc_ok); }
    void rx_unclassified() override { bw_.rx_unclassified(); }
    void rx_frame_start(unsigned vc, uint16_t n) override { bw_.rx_frame_start(vc, n); }
    void rx_frame_end(unsigned vc, uint16_t n) override { bw_.rx_frame_end(vc, n); }

private:
    BwRecorder& bw_;
};

}  // namespace cdc::platforms::fx1_soc

#endif
