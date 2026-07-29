// SPDX-License-Identifier: Apache-2.0

#include "floo_noc_model/floo_router.hpp"
#include "floo_noc_model/floo_types.hpp"

#include <systemc>

#include <cstdint>
#include <iostream>
#include <string>

namespace {

using flit_t = floo::model::test_flit;
constexpr unsigned num_ports = 5;

flit_t make_flit(std::uint64_t payload, bool last)
{
    flit_t flit;
    flit.payload = payload;
    flit.hdr.dst_id = floo::model::coordinate{2, 1};
    flit.hdr.src_id = floo::model::coordinate{0, 1};
    flit.hdr.last = last;
    return flit;
}

class router_testbench : public sc_core::sc_module {
public:
    sc_core::sc_in<bool> i_clk{"i_clk"};
    sc_core::sc_out<bool> o_rst_n{"o_rst_n"};
    sc_core::sc_out<floo::model::coordinate> o_router_id{"o_router_id"};

    sc_core::sc_vector<sc_core::sc_out<flit_t>> o_in_data{
        "o_in_data", num_ports};
    sc_core::sc_vector<sc_core::sc_out<bool>> o_in_valid{
        "o_in_valid", num_ports};
    sc_core::sc_vector<sc_core::sc_in<bool>> i_in_ready{
        "i_in_ready", num_ports};

    sc_core::sc_vector<sc_core::sc_in<flit_t>> i_out_data{
        "i_out_data", num_ports};
    sc_core::sc_vector<sc_core::sc_in<bool>> i_out_valid{
        "i_out_valid", num_ports};
    sc_core::sc_vector<sc_core::sc_out<bool>> o_out_ready{
        "o_out_ready", num_ports};

    sc_core::sc_vector<sc_core::sc_in<unsigned>> i_occupancy{
        "i_occupancy", num_ports};
    sc_core::sc_vector<sc_core::sc_in<unsigned>> i_selected{
        "i_selected", num_ports};
    sc_core::sc_vector<sc_core::sc_in<bool>> i_locked{
        "i_locked", num_ports};

    SC_HAS_PROCESS(router_testbench);

    explicit router_testbench(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
    {
        SC_THREAD(run);
    }

private:
    void expect(bool condition, const std::string& message)
    {
        if (!condition) {
            SC_REPORT_ERROR("test_floo_router", message.c_str());
        }
    }

    void settle()
    {
        // FIFO -> route select -> crossbar -> arbiter spans several
        // combinational SystemC processes. Allow all signal-update deltas to
        // settle before sampling a router-level observation.
        for (unsigned delta = 0; delta < 8; ++delta) {
            wait(sc_core::SC_ZERO_TIME);
        }
    }

    void drive_input(unsigned port, const flit_t& flit)
    {
        o_in_data[port].write(flit);
        o_in_valid[port].write(true);
    }

    void clear_inputs()
    {
        for (unsigned port = 0; port < num_ports; ++port) {
            o_in_valid[port].write(false);
        }
    }

    void run()
    {
        const unsigned west = floo::model::to_port(floo::model::direction::west);
        const unsigned east = floo::model::to_port(floo::model::direction::east);
        const unsigned eject = floo::model::to_port(floo::model::direction::eject);

        o_rst_n.write(false);
        o_router_id.write(floo::model::coordinate{1, 1});
        clear_inputs();
        for (unsigned port = 0; port < num_ports; ++port) {
            o_in_data[port].write(flit_t{});
            o_out_ready[port].write(port != east);
        }

        wait(i_clk.posedge_event());
        wait(i_clk.posedge_event());
        o_rst_n.write(true);
        wait(i_clk.negedge_event());

        // Two contenders for East. West injects a two-flit packet; Eject
        // injects a one-flit packet. East is stalled while queues fill.
        drive_input(west, make_flit(0xa0, false));
        drive_input(eject, make_flit(0xb0, true));
        settle();
        expect(i_in_ready[west].read() && i_in_ready[eject].read(),
               "empty router input FIFOs did not accept traffic");
        wait(i_clk.posedge_event());
        settle();
        clear_inputs();

        wait(i_clk.negedge_event());
        drive_input(west, make_flit(0xa1, true));
        settle();
        expect(i_in_ready[west].read(),
               "West input FIFO did not accept packet continuation");
        wait(i_clk.posedge_event());
        settle();
        clear_inputs();

        expect(i_occupancy[west].read() == 2,
               "West FIFO must contain both packet flits while stalled");
        expect(i_occupancy[eject].read() == 1,
               "Eject FIFO must contain its competing packet");
        expect(i_out_valid[east].read(),
               "East output must advertise queued traffic under back-pressure");
        expect(i_out_data[east].read().payload.to_uint64() == 0xa0,
               "round-robin did not select the lower West input first");

        // Release East. The two West flits must remain contiguous even though
        // Eject has been requesting the same output throughout.
        wait(i_clk.negedge_event());
        o_out_ready[east].write(true);
        settle();
        expect(i_out_data[east].read().payload.to_uint64() == 0xa0,
               "first East transfer payload mismatch");

        wait(i_clk.posedge_event());
        settle();
        expect(i_locked[east].read(),
               "East output arbiter did not lock after non-last flit");
        expect(i_selected[east].read() == west,
               "East output locked the wrong input");
        expect(i_out_data[east].read().payload.to_uint64() == 0xa1,
               "packet continuation was interleaved");

        wait(i_clk.posedge_event());
        settle();
        expect(!i_locked[east].read(),
               "East output lock was not released by last flit");
        expect(i_selected[east].read() == eject,
               "round-robin did not select waiting Eject input");
        expect(i_out_data[east].read().payload.to_uint64() == 0xb0,
               "waiting Eject packet payload mismatch");

        wait(i_clk.posedge_event());
        settle();
        expect(!i_out_valid[east].read(),
               "East output remained valid after all flits drained");
        expect(i_occupancy[west].read() == 0
                   && i_occupancy[eject].read() == 0,
               "input FIFOs did not drain");

        o_out_ready[east].write(false);
        sc_core::sc_stop();
    }
};

} // namespace

int sc_main(int, char**)
{
    sc_core::sc_clock clk{"clk", sc_core::sc_time(10, sc_core::SC_NS)};
    sc_core::sc_signal<bool> rst_n{"rst_n"};
    sc_core::sc_signal<floo::model::coordinate> router_id{"router_id"};

    sc_core::sc_vector<sc_core::sc_signal<flit_t>> in_data{
        "in_data", num_ports};
    sc_core::sc_vector<sc_core::sc_signal<bool>> in_valid{
        "in_valid", num_ports};
    sc_core::sc_vector<sc_core::sc_signal<bool>> in_ready{
        "in_ready", num_ports};
    sc_core::sc_vector<sc_core::sc_signal<flit_t>> out_data{
        "out_data", num_ports};
    sc_core::sc_vector<sc_core::sc_signal<bool>> out_valid{
        "out_valid", num_ports};
    sc_core::sc_vector<sc_core::sc_signal<bool>> out_ready{
        "out_ready", num_ports};
    sc_core::sc_vector<sc_core::sc_signal<unsigned>> occupancy{
        "occupancy", num_ports};
    sc_core::sc_vector<sc_core::sc_signal<unsigned>> selected{
        "selected", num_ports};
    sc_core::sc_vector<sc_core::sc_signal<bool>> locked{
        "locked", num_ports};

    // Pinned to the `gen_no_out_fifo` bypass. This test walks a precise
    // cycle-by-cycle sequence at the arbiter boundary, mixing arbiter state
    // (`i_locked`, `i_selected`) with the data the port presents. With
    // `OutFifoDepth = 2` those two are a cycle apart and the sequence no longer
    // lines up, so the hand-derived expectations would have to be re-derived
    // against the model rather than against the RTL — which is exactly what
    // `test_router_trace_sc_d2` already does, cycle-exactly, at the depth every
    // generated FlooNoC router uses.
    floo::model::floo_router<flit_t, 2, 0> dut{"dut"};
    dut.i_clk(clk);
    dut.i_rst_n(rst_n);
    dut.i_router_id(router_id);

    router_testbench tb{"tb"};
    tb.i_clk(clk);
    tb.o_rst_n(rst_n);
    tb.o_router_id(router_id);

    for (unsigned port = 0; port < num_ports; ++port) {
        dut.i_data[port](in_data[port]);
        dut.i_valid[port](in_valid[port]);
        dut.o_ready[port](in_ready[port]);
        dut.o_data[port](out_data[port]);
        dut.o_valid[port](out_valid[port]);
        dut.i_ready[port](out_ready[port]);
        dut.o_input_occupancy[port](occupancy[port]);
        dut.o_output_selected[port](selected[port]);
        dut.o_output_locked[port](locked[port]);

        tb.o_in_data[port](in_data[port]);
        tb.o_in_valid[port](in_valid[port]);
        tb.i_in_ready[port](in_ready[port]);
        tb.i_out_data[port](out_data[port]);
        tb.i_out_valid[port](out_valid[port]);
        tb.o_out_ready[port](out_ready[port]);
        tb.i_occupancy[port](occupancy[port]);
        tb.i_selected[port](selected[port]);
        tb.i_locked[port](locked[port]);
    }

    sc_core::sc_start();

    const int errors =
        sc_core::sc_report_handler::get_count(sc_core::SC_ERROR);
    if (errors == 0) {
        std::cout << "PASS: FlooNoC router contention and wormhole flow\n";
    }
    return errors == 0 ? 0 : 1;
}
