// Line side of the board's Ethernet PHY (fx1_soc --eth-peer).
//
//   loopback  a loopback plug: every frame the PHY sends starts back on its
//             receive side at the same time (the MAC's own unicast address or
//             broadcast passes its filter)
//   partner   a station that answers each frame after it ends plus the
//             inter-packet gap: DA = the frame's SA, SA = kPartnerMac, the
//             rest unchanged, a new FCS; broadcast and multicast frames from
//             the VP are echoed the same way
//   none      cable unplugged: the PHY reports link down and drops its frames
//
// Frame transport as in icsl_eth/eth_frame.h: one b_transport per frame at its
// start, DA..FCS, the sender waits out the wire time.
#pragma once

#include <cstdint>
#include <deque>
#include <string>
#include <vector>

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

#include <icsl_eth/eth_core.h>
#include <icsl_eth/eth_frame.h>

namespace cdc::platforms::fx1_soc {

class EthLinePeer : public sc_core::sc_module {
public:
    enum class Mode { loopback, partner, none };
    static constexpr std::uint8_t kPartnerMac[6] = {0x02, 0x00, 0x00, 0x00, 0x00, 0xFE};

    tlm_utils::simple_target_socket<EthLinePeer> line_rx{"line_rx"};      // from the PHY's line_tx
    tlm_utils::simple_initiator_socket<EthLinePeer> line_tx{"line_tx"};   // to the PHY's line_rx

    SC_HAS_PROCESS(EthLinePeer);
    EthLinePeer(sc_core::sc_module_name name, Mode mode) : sc_module(name), mode_(mode)
    {
        line_rx.register_b_transport(this, &EthLinePeer::rx);
        SC_THREAD(tx_thread);
    }

    Mode mode() const { return mode_; }
    std::uint64_t frames_in() const { return in_; }
    std::uint64_t frames_out() const { return out_; }

    static bool parse(const std::string& text, Mode& mode)
    {
        if (text == "loopback") mode = Mode::loopback;
        else if (text == "partner") mode = Mode::partner;
        else if (text == "none") mode = Mode::none;
        else return false;
        return true;
    }
    static const char* name_of(Mode mode)
    {
        return mode == Mode::loopback ? "loopback" : mode == Mode::partner ? "partner" : "none";
    }

private:
    struct Pending {
        std::vector<std::uint8_t> bytes;
        icsl_eth::EthFrameExt ext;
        sc_core::sc_time at;
    };

    void rx(tlm::tlm_generic_payload& t, sc_core::sc_time& delay)
    {
        Pending p;
        t.set_response_status(tlm::TLM_OK_RESPONSE);
        if (!icsl_eth::recv_frame(t, p.bytes, p.ext) || p.bytes.size() < 18) return;
        ++in_;
        if (mode_ == Mode::none) return;
        p.at = sc_core::sc_time_stamp() + delay;
        if (mode_ == Mode::partner) {
            // answer after the frame and a 12-byte inter-packet gap
            p.at += icsl_eth::frame_time(p.bytes.size(), p.ext.speed100, p.ext.preamble) +
                    icsl_eth::byte_time(p.ext.speed100) * 12.0;
            for (int i = 0; i < 6; ++i) {
                p.bytes[i] = p.bytes[6 + i];
                p.bytes[6 + i] = kPartnerMac[i];
            }
            const auto fcs = icsl_eth::core::fcs(p.bytes.data(), p.bytes.size() - 4);
            for (int i = 0; i < 4; ++i) p.bytes[p.bytes.size() - 4 + i] = fcs[i];
        }
        p.ext.rx_er = false;
        queue_.push_back(std::move(p));
        ev_.notify(sc_core::SC_ZERO_TIME);
    }

    void tx_thread()
    {
        for (;;) {
            while (queue_.empty()) wait(ev_);
            Pending p = std::move(queue_.front());
            queue_.pop_front();
            if (p.at > sc_core::sc_time_stamp()) wait(p.at - sc_core::sc_time_stamp());
            icsl_eth::send_frame(line_tx, p.bytes, p.ext);
            ++out_;
            wait(icsl_eth::frame_time(p.bytes.size(), p.ext.speed100, p.ext.preamble));
        }
    }

    Mode mode_;
    std::deque<Pending> queue_;
    sc_core::sc_event ev_;
    std::uint64_t in_ = 0, out_ = 0;
};

} // namespace cdc::platforms::fx1_soc
