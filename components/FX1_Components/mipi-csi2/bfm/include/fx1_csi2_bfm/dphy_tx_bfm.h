// Pad-level D-PHY transmitter BFM (sensor side) for the mipirxphy model.
//
// Drives the lane pads (vl::sres) with the line states the model's receiver
// decodes (docs/PHY_ADAPTER_CONTRACT.md §4):
//   LP-1 / LP-0  value at ST_PULL; HS  complementary strong P/N written from
//   one process at the same timestamp (P=1,N=0 is bit 1). A strong drive reads
//   as LP-00 on the LP receiver, so HS-0 also serves as the LP-00 state.
// Bytes are serialised LSB first; the leader is HS-0 for hs_zero_ui followed
// by the sync byte (0xB8). Data bits change on a UI grid aligned to time 0;
// clock-lane edges are offset by clk_phase_ui.
//
// One SC_THREAD owns the clock-lane pads and one owns all data-lane pads, so
// every pad has a single writer process. Control is by blocking calls from a
// test thread.
#pragma once

#include <cstdint>
#include <utility>
#include <vector>

#include <systemc>

#include "vl/vl_rt.h"

namespace fx1::csi2::bfm {

struct DphyTxTiming {
    sc_core::sc_time ui{1250, sc_core::SC_PS};             // 800 Mb/s
    double clk_phase_ui = 0.5;                             // clock edge at data centre
    sc_core::sc_time t_lpx{50, sc_core::SC_NS};            // each LP state (> 13 ns filter)
    sc_core::sc_time clk_prepare_zero{300, sc_core::SC_NS};  // clock lane HS-0 before toggling
    sc_core::sc_time clk_trail{60, sc_core::SC_NS};        // clock lane HS-0 before LP-11
    unsigned hs_zero_ui = 128;                             // data lane HS-0 before sync
    unsigned trail_ui = 56;   // inverted last bit before LP-11; D-PHY THS-TRAIL >= 60 ns + 4 UI = 52 UI at 800 Mb/s
};

struct LaneBurst {
    std::vector<uint8_t> bytes;              // payload after the sync byte
    unsigned skew_ui = 0;                    // delay of this lane's whole sequence
    uint8_t sync = 0xB8;                     // leader byte (corrupt it to test SoT errors)
    std::vector<size_t> flip_bits;           // payload bit indices (byte*8 + bit) to invert on the wire
};

class DphyTxBfm : public sc_core::sc_module {
public:
    SC_HAS_PROCESS(DphyTxBfm);
    DphyTxBfm(sc_core::sc_module_name nm, vl::sres& clkp, vl::sres& clkn, sc_core::sc_vector<vl::sres>& dp,
              sc_core::sc_vector<vl::sres>& dn, DphyTxTiming timing = {});

    // Clock lane LP-11 -> LP-01 -> HS-0 -> continuous DDR clock. Returns at the first clock edge.
    void clock_start();
    // Clock lane HS-0 trail -> LP-11. Returns once LP-11 is driven.
    void clock_stop();
    // One HS burst on data lanes 0..lanes.size()-1 (an empty byte vector keeps
    // that lane in LP-11). Returns when every lane is back in LP-11.
    void send_burst(const std::vector<LaneBurst>& lanes);

    const DphyTxTiming& timing() const { return t_; }
    // Takes effect at the next clock_start().
    void set_clock_phase_ui(double phase) { t_.clk_phase_ui = phase; }
    bool clock_running() const { return clk_running_; }

private:
    enum class Sym : uint8_t { LP11, LP01, HS0, HS1 };
    void clock_thread();
    void data_thread();
    void drive(vl::sres& p, vl::sres& n, Sym s);
    sc_core::sc_time next_grid(double phase_ui) const;

    vl::sres& clkp_;
    vl::sres& clkn_;
    sc_core::sc_vector<vl::sres>& dp_;
    sc_core::sc_vector<vl::sres>& dn_;
    DphyTxTiming t_;

    bool clk_req_ = false, clk_running_ = false;
    sc_core::sc_event clk_req_ev_, clk_ack_ev_;
    std::vector<LaneBurst> burst_;
    bool burst_busy_ = false;
    sc_core::sc_event burst_req_ev_, burst_done_ev_;
};

}  // namespace fx1::csi2::bfm
