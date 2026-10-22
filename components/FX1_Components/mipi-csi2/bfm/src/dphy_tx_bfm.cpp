#include "fx1_csi2_bfm/dphy_tx_bfm.h"

#include <algorithm>
#include <cmath>

namespace fx1::csi2::bfm {

DphyTxBfm::DphyTxBfm(sc_core::sc_module_name nm, vl::sres& clkp, vl::sres& clkn, sc_core::sc_vector<vl::sres>& dp,
                     sc_core::sc_vector<vl::sres>& dn, DphyTxTiming timing)
    : sc_core::sc_module(nm), clkp_(clkp), clkn_(clkn), dp_(dp), dn_(dn), t_(timing) {
    SC_THREAD(clock_thread);
    SC_THREAD(data_thread);
}

void DphyTxBfm::drive(vl::sres& p, vl::sres& n, Sym s) {
    switch (s) {
    case Sym::LP11: p.write_s(sc_dt::SC_LOGIC_1, vl::ST_PULL); n.write_s(sc_dt::SC_LOGIC_1, vl::ST_PULL); break;
    case Sym::LP01: p.write_s(sc_dt::SC_LOGIC_0, vl::ST_PULL); n.write_s(sc_dt::SC_LOGIC_1, vl::ST_PULL); break;
    case Sym::HS0: p.write_s(sc_dt::SC_LOGIC_0, vl::ST_STRONG); n.write_s(sc_dt::SC_LOGIC_1, vl::ST_STRONG); break;
    case Sym::HS1: p.write_s(sc_dt::SC_LOGIC_1, vl::ST_STRONG); n.write_s(sc_dt::SC_LOGIC_0, vl::ST_STRONG); break;
    }
}

// First time >= now on the grid {(k + phase_ui) * ui}.
sc_core::sc_time DphyTxBfm::next_grid(double phase_ui) const {
    const double ui = t_.ui.to_seconds();
    const double now = sc_core::sc_time_stamp().to_seconds();
    const double k = std::ceil(now / ui - phase_ui - 1e-9);
    const sc_core::sc_time t = t_.ui * (k + phase_ui);
    return t < sc_core::sc_time_stamp() ? sc_core::sc_time_stamp() : t;
}

void DphyTxBfm::clock_start() {
    clk_req_ = true;
    clk_req_ev_.notify(sc_core::SC_ZERO_TIME);
    sc_core::wait(clk_ack_ev_);
}

void DphyTxBfm::clock_stop() {
    clk_req_ = false;
    clk_req_ev_.notify(sc_core::SC_ZERO_TIME);
    sc_core::wait(clk_ack_ev_);
}

void DphyTxBfm::clock_thread() {
    drive(clkp_, clkn_, Sym::LP11);
    for (;;) {
        while (!clk_req_) wait(clk_req_ev_);
        drive(clkp_, clkn_, Sym::LP01);
        wait(t_.t_lpx);
        drive(clkp_, clkn_, Sym::HS0);
        wait(t_.clk_prepare_zero);
        wait(next_grid(t_.clk_phase_ui) - sc_core::sc_time_stamp());
        clk_running_ = true;
        clk_ack_ev_.notify(sc_core::SC_ZERO_TIME);
        bool level = true;
        while (clk_req_) {
            drive(clkp_, clkn_, level ? Sym::HS1 : Sym::HS0);
            level = !level;
            wait(t_.ui);
        }
        drive(clkp_, clkn_, Sym::HS0);
        clk_running_ = false;
        wait(t_.clk_trail);
        drive(clkp_, clkn_, Sym::LP11);
        clk_ack_ev_.notify(sc_core::SC_ZERO_TIME);
    }
}

void DphyTxBfm::send_burst(const std::vector<LaneBurst>& lanes) {
    burst_ = lanes;
    burst_busy_ = true;
    burst_req_ev_.notify(sc_core::SC_ZERO_TIME);
    sc_core::wait(burst_done_ev_);
}

void DphyTxBfm::data_thread() {
    const size_t nl = dp_.size();
    for (size_t i = 0; i < nl; ++i) drive(dp_[i], dn_[i], Sym::LP11);
    const unsigned lpx_ui = static_cast<unsigned>(std::ceil(t_.t_lpx / t_.ui));
    for (;;) {
        while (!burst_busy_) wait(burst_req_ev_);
        // Per-lane symbol sequence, one symbol per UI tick.
        std::vector<std::vector<Sym>> seq(nl);
        size_t len = 0;
        for (size_t i = 0; i < std::min(nl, burst_.size()); ++i) {
            const LaneBurst& b = burst_[i];
            if (b.bytes.empty()) continue;
            auto& s = seq[i];
            s.insert(s.end(), b.skew_ui, Sym::LP11);
            s.insert(s.end(), lpx_ui, Sym::LP01);
            s.insert(s.end(), t_.hs_zero_ui, Sym::HS0);
            for (int k = 0; k < 8; ++k) s.push_back((b.sync >> k & 1) ? Sym::HS1 : Sym::HS0);
            std::vector<uint8_t> flip(b.bytes.size() * 8, 0);
            for (size_t f : b.flip_bits)
                if (f < flip.size()) flip[f] = 1;
            for (size_t byte = 0; byte < b.bytes.size(); ++byte)
                for (int k = 0; k < 8; ++k) {
                    const bool bit = ((b.bytes[byte] >> k) & 1) ^ flip[byte * 8 + k];
                    s.push_back(bit ? Sym::HS1 : Sym::HS0);
                }
            const Sym last = s.back();
            s.insert(s.end(), t_.trail_ui, last == Sym::HS1 ? Sym::HS0 : Sym::HS1);
            s.push_back(Sym::LP11);
            len = std::max(len, s.size());
        }
        wait(next_grid(0.0) - sc_core::sc_time_stamp());
        std::vector<Sym> cur(nl, Sym::LP11);
        for (size_t tick = 0; tick < len; ++tick) {
            for (size_t i = 0; i < nl; ++i) {
                if (seq[i].empty()) continue;
                const Sym s = tick < seq[i].size() ? seq[i][tick] : Sym::LP11;
                if (s != cur[i]) {
                    drive(dp_[i], dn_[i], s);
                    cur[i] = s;
                }
            }
            wait(t_.ui);
        }
        burst_busy_ = false;
        burst_done_ev_.notify(sc_core::SC_ZERO_TIME);
    }
}

}  // namespace fx1::csi2::bfm
