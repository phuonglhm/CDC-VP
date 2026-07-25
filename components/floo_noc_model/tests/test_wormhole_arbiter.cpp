// SPDX-License-Identifier: Apache-2.0

#include "floo_noc_model/floo_types.hpp"
#include "floo_noc_model/wormhole_arbiter.hpp"

#include <systemc>

#include <cstdint>
#include <iostream>
#include <string>

namespace {

using flit_t = floo::model::test_flit;

flit_t make_flit(std::uint64_t payload, bool last)
{
    flit_t flit;
    flit.payload = payload;
    flit.hdr.last = last;
    return flit;
}

class arbiter_testbench : public sc_core::sc_module {
public:
    sc_core::sc_in<bool> i_clk{"i_clk"};
    sc_core::sc_out<bool> o_rst_n{"o_rst_n"};
    sc_core::sc_vector<sc_core::sc_out<flit_t>> o_data{"o_data", 2};
    sc_core::sc_vector<sc_core::sc_out<bool>> o_valid{"o_valid", 2};
    sc_core::sc_vector<sc_core::sc_in<bool>> i_ready{"i_ready", 2};
    sc_core::sc_in<flit_t> i_data{"i_data"};
    sc_core::sc_in<bool> i_valid{"i_valid"};
    sc_core::sc_out<bool> o_ready{"o_ready"};
    sc_core::sc_in<unsigned> i_selected{"i_selected"};
    sc_core::sc_in<bool> i_locked{"i_locked"};

    SC_HAS_PROCESS(arbiter_testbench);

    explicit arbiter_testbench(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
    {
        SC_THREAD(run);
    }

private:
    void expect(bool condition, const std::string& message)
    {
        if (!condition) {
            SC_REPORT_ERROR("test_wormhole_arbiter", message.c_str());
        }
    }

    void settle()
    {
        wait(sc_core::SC_ZERO_TIME);
        wait(sc_core::SC_ZERO_TIME);
    }

    void run()
    {
        o_rst_n.write(false);
        o_ready.write(false);
        for (unsigned i = 0; i < 2; ++i) {
            o_data[i].write(flit_t{});
            o_valid[i].write(false);
        }

        wait(i_clk.posedge_event());
        wait(i_clk.posedge_event());
        o_rst_n.write(true);
        wait(i_clk.negedge_event());

        o_data[0].write(make_flit(0x10, false));
        o_valid[0].write(true);
        o_data[1].write(make_flit(0x20, true));
        o_valid[1].write(true);
        o_ready.write(true);
        settle();

        expect(i_valid.read(), "arbiter did not expose a valid request");
        expect(i_selected.read() == 0, "round-robin reset priority must be input 0");
        expect(i_ready[0].read() && !i_ready[1].read(),
               "ready must be asserted only for the selected input");

        wait(i_clk.posedge_event());
        settle();
        expect(i_locked.read(), "non-last flit must lock the arbiter");

        o_data[0].write(make_flit(0x11, false));
        settle();
        expect(i_selected.read() == 0,
               "locked arbiter switched to another valid input");

        wait(i_clk.posedge_event());
        settle();
        o_data[0].write(make_flit(0x12, true));
        settle();
        expect(i_selected.read() == 0,
               "final packet flit must use the locked input");

        wait(i_clk.posedge_event());
        settle();
        expect(!i_locked.read(), "last flit must release the arbiter");
        expect(i_selected.read() == 1,
               "round-robin must advance after a complete packet");
        expect(i_data.read().payload.to_uint64() == 0x20,
               "arbiter selected wrong input after lock release");

        wait(i_clk.posedge_event());
        settle();
        o_valid[1].write(false);
        settle();
        expect(i_selected.read() == 0,
               "round-robin must return to the remaining requester");

        o_valid[0].write(false);
        o_ready.write(false);
        sc_core::sc_stop();
    }
};

} // namespace

int sc_main(int, char**)
{
    constexpr unsigned num_routes = 2;

    sc_core::sc_clock clk{"clk", sc_core::sc_time(10, sc_core::SC_NS)};
    sc_core::sc_signal<bool> rst_n{"rst_n"};
    sc_core::sc_vector<sc_core::sc_signal<flit_t>> in_data{"in_data", num_routes};
    sc_core::sc_vector<sc_core::sc_signal<bool>> in_valid{"in_valid", num_routes};
    sc_core::sc_vector<sc_core::sc_signal<bool>> in_ready{"in_ready", num_routes};
    sc_core::sc_signal<flit_t> out_data{"out_data"};
    sc_core::sc_signal<bool> out_valid{"out_valid"};
    sc_core::sc_signal<bool> out_ready{"out_ready"};
    sc_core::sc_signal<unsigned> selected{"selected"};
    sc_core::sc_signal<bool> locked{"locked"};

    floo::model::wormhole_arbiter<flit_t, num_routes> dut{"dut"};
    dut.i_clk(clk);
    dut.i_rst_n(rst_n);
    dut.o_data(out_data);
    dut.o_valid(out_valid);
    dut.i_ready(out_ready);
    dut.o_selected(selected);
    dut.o_locked(locked);
    for (unsigned i = 0; i < num_routes; ++i) {
        dut.i_data[i](in_data[i]);
        dut.i_valid[i](in_valid[i]);
        dut.o_ready[i](in_ready[i]);
    }

    arbiter_testbench tb{"tb"};
    tb.i_clk(clk);
    tb.o_rst_n(rst_n);
    tb.i_data(out_data);
    tb.i_valid(out_valid);
    tb.o_ready(out_ready);
    tb.i_selected(selected);
    tb.i_locked(locked);
    for (unsigned i = 0; i < num_routes; ++i) {
        tb.o_data[i](in_data[i]);
        tb.o_valid[i](in_valid[i]);
        tb.i_ready[i](in_ready[i]);
    }

    sc_core::sc_start();

    const int errors =
        sc_core::sc_report_handler::get_count(sc_core::SC_ERROR);
    if (errors == 0) {
        std::cout << "PASS: wormhole arbiter lock and fairness\n";
    }
    return errors == 0 ? 0 : 1;
}
