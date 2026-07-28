// SPDX-License-Identifier: Apache-2.0
//
// Contract test for the SystemC mirror of `stream_fifo_optimal_wrap`.
// Both RTL branches are exercised: depth 2 (spill register) and depth 4
// (`stream_fifo` over `fifo_v3`).
//
// The decisive property in both branches is that `ready_o` is a function of
// registers only: a full buffer refuses a push even when its head is popped in
// the same cycle. The RTL trace comparison lives in
// `rtl_crosscheck/run_stream_fifo_crosscheck.sh`.

#include "floo_noc_model/floo_types.hpp"
#include "floo_noc_model/stream_fifo.hpp"

#include <systemc>

#include <cstdint>
#include <iostream>
#include <string>

namespace {

using flit_t = floo::model::test_flit;

int failures = 0;

void check(bool condition, const std::string& message)
{
    if (!condition) {
        std::cerr << "FAIL @" << sc_core::sc_time_stamp() << ": " << message
                  << '\n';
        ++failures;
    }
}

flit_t make_flit(std::uint64_t payload)
{
    flit_t flit;
    flit.payload = payload;
    return flit;
}

template <unsigned Depth>
class harness {
public:
    harness(const std::string& name, sc_core::sc_signal<bool>& clk)
        : rst_n_((name + "_rst_n").c_str())
        , in_data_((name + "_in_data").c_str())
        , in_valid_((name + "_in_valid").c_str())
        , in_ready_((name + "_in_ready").c_str())
        , out_data_((name + "_out_data").c_str())
        , out_valid_((name + "_out_valid").c_str())
        , out_ready_((name + "_out_ready").c_str())
        , occupancy_((name + "_occupancy").c_str())
        , dut_(name.c_str())
    {
        dut_.i_clk(clk);
        dut_.i_rst_n(rst_n_);
        dut_.i_data(in_data_);
        dut_.i_valid(in_valid_);
        dut_.o_ready(in_ready_);
        dut_.o_data(out_data_);
        dut_.o_valid(out_valid_);
        dut_.i_ready(out_ready_);
        dut_.o_occupancy(occupancy_);
    }

    void drive(bool rst_n, bool valid, std::uint64_t payload, bool ready)
    {
        rst_n_.write(rst_n);
        in_valid_.write(valid);
        in_data_.write(make_flit(payload));
        out_ready_.write(ready);
    }

    void idle() { drive(true, false, 0, false); }

    bool ready() const { return in_ready_.read(); }
    bool valid() const { return out_valid_.read(); }
    unsigned occupancy() const { return occupancy_.read(); }
    std::uint64_t data() const { return out_data_.read().payload.to_uint64(); }

private:
    sc_core::sc_signal<bool> rst_n_;
    sc_core::sc_signal<flit_t> in_data_;
    sc_core::sc_signal<bool> in_valid_;
    sc_core::sc_signal<bool> in_ready_;
    sc_core::sc_signal<flit_t> out_data_;
    sc_core::sc_signal<bool> out_valid_;
    sc_core::sc_signal<bool> out_ready_;
    sc_core::sc_signal<unsigned> occupancy_;
    floo::model::stream_fifo_optimal_wrap<flit_t, Depth> dut_;
};

sc_core::sc_signal<bool>* g_clk = nullptr;

/// One clock cycle. Inputs must already be driven; outputs are observed after
/// the rising edge, which is the sampling point the RTL cross-check also uses.
void tick()
{
    g_clk->write(false);
    sc_core::sc_start(sc_core::sc_time(1, sc_core::SC_NS));
    g_clk->write(true);
    sc_core::sc_start(sc_core::sc_time(1, sc_core::SC_NS));
}

void check_spill_register(harness<2>& fifo)
{
    // Reset.
    fifo.drive(false, false, 0, false);
    tick();
    tick();
    check(fifo.ready(), "spill: reset must leave the input ready");
    check(!fifo.valid(), "spill: reset must clear output valid");
    check(fifo.occupancy() == 0, "spill: reset must clear occupancy");

    // First accepted flit lands in the A register.
    fifo.drive(true, true, 0x11, false);
    tick();
    check(fifo.occupancy() == 1, "spill: first push must fill one slot");
    check(fifo.valid(), "spill: first push must expose the head");
    check(fifo.data() == 0x11, "spill: first head payload mismatch");
    check(fifo.ready(), "spill: one occupied slot must still accept");

    // Second accepted flit spills the first into the B register.
    fifo.drive(true, true, 0x22, false);
    tick();
    check(fifo.occupancy() == 2, "spill: second push must fill both slots");
    check(fifo.data() == 0x11, "spill: spilling must preserve FIFO order");
    check(!fifo.ready(), "spill: both slots occupied must back-pressure");

    // A full spill register refuses a push while the output is stalled.
    fifo.drive(true, true, 0x33, false);
    tick();
    check(fifo.occupancy() == 2, "spill: full buffer must refuse a push");
    check(fifo.data() == 0x11, "spill: refused push must not disturb the head");

    // RTL-critical case: at full, `ready_o` stays low even though the head is
    // popped in the same cycle. The superseded model accepted 0x33 here.
    fifo.drive(true, true, 0x33, true);
    tick();
    check(fifo.valid(), "spill: pop at full must leave the buffer valid");
    check(fifo.occupancy() == 1,
          "spill: pop at full must not be paired with a push");
    check(fifo.data() == 0x22, "spill: pop must advance to the second flit");
    check(fifo.ready(), "spill: one free slot must reassert ready");

    // With one slot free, a push and a pop are accepted in the same cycle.
    fifo.drive(true, true, 0x33, true);
    tick();
    check(fifo.occupancy() == 1,
          "spill: simultaneous push/pop below full must hold occupancy");
    check(fifo.data() == 0x33,
          "spill: replacement payload must become the head");

    // Drain.
    fifo.drive(true, false, 0, true);
    tick();
    check(fifo.occupancy() == 0, "spill: final pop must empty the buffer");
    check(!fifo.valid(), "spill: empty buffer must clear valid");
    check(fifo.ready(), "spill: empty buffer must accept");

    fifo.idle();
    tick();
}

void check_stream_fifo(harness<4>& fifo)
{
    fifo.drive(false, false, 0, false);
    tick();
    tick();
    check(fifo.ready(), "fifo: reset must leave the input ready");
    check(!fifo.valid(), "fifo: reset must clear output valid");
    check(fifo.occupancy() == 0, "fifo: reset must clear occupancy");

    // Fill to full while the output is stalled.
    for (unsigned index = 0; index < 4; ++index) {
        fifo.drive(true, true, 0xA0 + index, false);
        tick();
        check(fifo.occupancy() == index + 1, "fifo: push must fill one slot");
        check(fifo.data() == 0xA0, "fifo: head must stay at the first flit");
        check(fifo.ready() == (index + 1 < 4),
              "fifo: ready must deassert exactly at full");
    }

    // A full FIFO refuses a push even while its head is popped.
    fifo.drive(true, true, 0xBB, true);
    tick();
    check(fifo.occupancy() == 3, "fifo: full buffer must refuse a push");
    check(fifo.data() == 0xA1, "fifo: pop must advance the read pointer");
    check(fifo.ready(), "fifo: a freed slot must reassert ready");

    // Below full, push and pop are accepted together. Four such cycles wrap
    // both pointers once and drain the remaining 0xA* flits.
    for (unsigned index = 0; index < 4; ++index) {
        fifo.drive(true, true, 0xC0 + index, true);
        tick();
        check(fifo.occupancy() == 3,
              "fifo: simultaneous push/pop must hold occupancy");
    }
    check(fifo.data() == 0xC1, "fifo: pointer wrap must preserve FIFO order");

    // Drain and confirm ordering across the wrap.
    for (unsigned index = 0; index < 3; ++index) {
        check(fifo.data() == 0xC1 + index, "fifo: drain order mismatch");
        fifo.drive(true, false, 0, true);
        tick();
    }
    check(fifo.occupancy() == 0, "fifo: drain must empty the buffer");
    check(!fifo.valid(), "fifo: empty buffer must clear valid");

    // A mid-stream reset clears the pointers, the counter, and the memory.
    fifo.drive(true, true, 0xD1, false);
    tick();
    check(fifo.occupancy() == 1, "fifo: post-drain push must be accepted");
    fifo.drive(false, false, 0, false);
    tick();
    check(fifo.occupancy() == 0, "fifo: reset must clear occupancy");
    check(!fifo.valid(), "fifo: reset must clear valid");
    check(fifo.data() == 0, "fifo: reset must clear the memory");

    fifo.drive(true, false, 0, false);
    tick();
}

} // namespace

int sc_main(int, char**)
{
    sc_core::sc_signal<bool> clk{"clk"};
    g_clk = &clk;

    harness<2> spill{"fifo_depth2", clk};
    harness<4> deep{"fifo_depth4", clk};

    clk.write(false);
    spill.drive(false, false, 0, false);
    deep.drive(false, false, 0, false);
    sc_core::sc_start(sc_core::SC_ZERO_TIME);

    check_spill_register(spill);
    check_stream_fifo(deep);

    if (failures == 0) {
        std::cout << "PASS: stream_fifo_optimal_wrap depth 2 and depth 4\n";
        return 0;
    }
    std::cerr << "FAIL: " << failures << " stream FIFO checks failed\n";
    return 1;
}
