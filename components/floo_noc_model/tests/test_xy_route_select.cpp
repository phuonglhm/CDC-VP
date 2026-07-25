// SPDX-License-Identifier: Apache-2.0

#include "floo_noc_model/floo_types.hpp"
#include "floo_noc_model/xy_route_select.hpp"

#include <systemc>

#include <iostream>
#include <string>

namespace {

using flit_t = floo::model::test_flit;

class route_testbench : public sc_core::sc_module {
public:
    sc_core::sc_in<bool> i_clk{"i_clk"};
    sc_core::sc_out<bool> o_rst_n{"o_rst_n"};
    sc_core::sc_out<floo::model::coordinate> o_router_id{"o_router_id"};
    sc_core::sc_out<flit_t> o_flit{"o_flit"};
    sc_core::sc_out<bool> o_valid{"o_valid"};
    sc_core::sc_out<bool> o_ready{"o_ready"};
    sc_core::sc_in<sc_dt::sc_uint<3>> i_route{"i_route"};
    sc_core::sc_in<bool> i_locked{"i_locked"};

    SC_HAS_PROCESS(route_testbench);

    explicit route_testbench(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
    {
        SC_THREAD(run);
    }

private:
    void expect(bool condition, const std::string& message)
    {
        if (!condition) {
            SC_REPORT_ERROR("test_xy_route_select", message.c_str());
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
        o_router_id.write(floo::model::coordinate{1, 1});
        o_flit.write(flit_t{});
        o_valid.write(false);
        o_ready.write(false);

        wait(i_clk.posedge_event());
        wait(i_clk.posedge_event());
        o_rst_n.write(true);
        wait(i_clk.negedge_event());

        flit_t first;
        first.hdr.dst_id = floo::model::coordinate{2, 0};
        first.hdr.last = false;
        o_flit.write(first);
        o_valid.write(true);
        o_ready.write(true);
        settle();

        expect(i_route.read() ==
                   floo::model::to_port(floo::model::direction::east),
               "XY must resolve X before Y");
        expect(!i_locked.read(), "route must not lock before first handshake");

        wait(i_clk.posedge_event());
        settle();
        expect(i_locked.read(), "non-last flit must lock its route");

        flit_t continuation = first;
        continuation.hdr.dst_id = floo::model::coordinate{0, 1};
        continuation.hdr.last = true;
        o_flit.write(continuation);
        settle();
        expect(i_route.read() ==
                   floo::model::to_port(floo::model::direction::east),
               "locked route changed when continuation header differed");

        wait(i_clk.posedge_event());
        settle();
        expect(!i_locked.read(), "last flit must release route lock");
        expect(i_route.read() ==
                   floo::model::to_port(floo::model::direction::west),
               "route must recompute after lock release");

        wait(i_clk.negedge_event());
        continuation.hdr.dst_id = floo::model::coordinate{1, 1};
        o_flit.write(continuation);
        settle();
        expect(i_route.read() ==
                   floo::model::to_port(floo::model::direction::eject),
               "local destination must eject");

        o_valid.write(false);
        o_ready.write(false);
        sc_core::sc_stop();
    }
};

} // namespace

int sc_main(int, char**)
{
    sc_core::sc_clock clk{"clk", sc_core::sc_time(10, sc_core::SC_NS)};
    sc_core::sc_signal<bool> rst_n{"rst_n"};
    sc_core::sc_signal<floo::model::coordinate> router_id{"router_id"};
    sc_core::sc_signal<flit_t> in_flit{"in_flit"};
    sc_core::sc_signal<bool> in_valid{"in_valid"};
    sc_core::sc_signal<bool> in_ready{"in_ready"};
    sc_core::sc_signal<flit_t> out_flit{"out_flit"};
    sc_core::sc_signal<sc_dt::sc_uint<3>> route{"route"};
    sc_core::sc_signal<bool> locked{"locked"};

    floo::model::xy_route_select<flit_t> dut{"dut"};
    dut.i_clk(clk);
    dut.i_rst_n(rst_n);
    dut.i_router_id(router_id);
    dut.i_flit(in_flit);
    dut.i_valid(in_valid);
    dut.i_ready(in_ready);
    dut.o_flit(out_flit);
    dut.o_route(route);
    dut.o_locked(locked);

    route_testbench tb{"tb"};
    tb.i_clk(clk);
    tb.o_rst_n(rst_n);
    tb.o_router_id(router_id);
    tb.o_flit(in_flit);
    tb.o_valid(in_valid);
    tb.o_ready(in_ready);
    tb.i_route(route);
    tb.i_locked(locked);

    sc_core::sc_start();

    const int errors =
        sc_core::sc_report_handler::get_count(sc_core::SC_ERROR);
    if (errors == 0) {
        std::cout << "PASS: XY route select and route lock\n";
    }
    return errors == 0 ? 0 : 1;
}
