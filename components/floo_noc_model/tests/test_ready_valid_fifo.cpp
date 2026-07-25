// SPDX-License-Identifier: Apache-2.0

#include "floo_noc_model/floo_types.hpp"
#include "floo_noc_model/ready_valid_fifo.hpp"

#include <systemc>

#include <cstdint>
#include <iostream>
#include <string>

namespace {

using flit_t = floo::model::test_flit;

flit_t make_flit(std::uint64_t payload)
{
    flit_t flit;
    flit.payload = payload;
    return flit;
}

std::uint64_t payload_of(const flit_t& flit)
{
    return flit.payload.to_uint64();
}

class fifo_testbench : public sc_core::sc_module {
public:
    sc_core::sc_in<bool> i_clk{"i_clk"};
    sc_core::sc_out<bool> o_rst_n{"o_rst_n"};
    sc_core::sc_out<flit_t> o_data{"o_data"};
    sc_core::sc_out<bool> o_valid{"o_valid"};
    sc_core::sc_in<bool> i_ready{"i_ready"};
    sc_core::sc_in<flit_t> i_data{"i_data"};
    sc_core::sc_in<bool> i_valid{"i_valid"};
    sc_core::sc_out<bool> o_ready{"o_ready"};
    sc_core::sc_in<unsigned> i_occupancy{"i_occupancy"};

    SC_HAS_PROCESS(fifo_testbench);

    explicit fifo_testbench(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
    {
        SC_THREAD(run);
    }

private:
    void expect(bool condition, const std::string& message)
    {
        if (!condition) {
            SC_REPORT_ERROR("test_ready_valid_fifo", message.c_str());
        }
    }

    void settle()
    {
        wait(sc_core::SC_ZERO_TIME);
        wait(sc_core::SC_ZERO_TIME);
    }

    void push(std::uint64_t payload)
    {
        wait(i_clk.negedge_event());
        o_data.write(make_flit(payload));
        o_valid.write(true);
        settle();
        expect(i_ready.read(), "FIFO unexpectedly back-pressured a push");
        wait(i_clk.posedge_event());
        settle();
        o_valid.write(false);
    }

    void run()
    {
        o_rst_n.write(false);
        o_valid.write(false);
        o_ready.write(false);
        o_data.write(flit_t{});

        wait(i_clk.posedge_event());
        wait(i_clk.posedge_event());
        o_rst_n.write(true);
        wait(i_clk.posedge_event());
        settle();

        expect(i_ready.read(), "empty FIFO must accept input");
        expect(!i_valid.read(), "empty FIFO must not assert output valid");

        push(0x11);
        expect(i_occupancy.read() == 1, "first push must increment occupancy");
        expect(i_valid.read(), "FIFO must expose its first element");
        expect(payload_of(i_data.read()) == 0x11, "first FIFO payload mismatch");

        push(0x22);
        expect(i_occupancy.read() == 2, "second push must fill the FIFO");
        settle();
        expect(!i_ready.read(), "full FIFO must back-pressure without a pop");

        // Full FIFO: pop the head and push a replacement on the same edge.
        wait(i_clk.negedge_event());
        o_ready.write(true);
        o_data.write(make_flit(0x33));
        o_valid.write(true);
        settle();
        expect(i_ready.read(),
               "full FIFO must accept input when output pops in the same cycle");
        wait(i_clk.posedge_event());
        settle();
        o_valid.write(false);

        expect(i_occupancy.read() == 2,
               "simultaneous pop/push must preserve occupancy");
        expect(payload_of(i_data.read()) == 0x22,
               "FIFO ordering failed after simultaneous pop/push");

        wait(i_clk.posedge_event());
        settle();
        expect(i_occupancy.read() == 1, "first drain must decrement occupancy");
        expect(payload_of(i_data.read()) == 0x33,
               "replacement payload must be last");

        wait(i_clk.posedge_event());
        settle();
        expect(i_occupancy.read() == 0, "second drain must empty FIFO");
        expect(!i_valid.read(), "empty FIFO asserted output valid");

        o_ready.write(false);
        sc_core::sc_stop();
    }
};

} // namespace

int sc_main(int, char**)
{
    sc_core::sc_clock clk{"clk", sc_core::sc_time(10, sc_core::SC_NS)};
    sc_core::sc_signal<bool> rst_n{"rst_n"};
    sc_core::sc_signal<flit_t> in_data{"in_data"};
    sc_core::sc_signal<bool> in_valid{"in_valid"};
    sc_core::sc_signal<bool> in_ready{"in_ready"};
    sc_core::sc_signal<flit_t> out_data{"out_data"};
    sc_core::sc_signal<bool> out_valid{"out_valid"};
    sc_core::sc_signal<bool> out_ready{"out_ready"};
    sc_core::sc_signal<unsigned> occupancy{"occupancy"};

    floo::model::ready_valid_fifo<flit_t, 2> dut{"dut"};
    dut.i_clk(clk);
    dut.i_rst_n(rst_n);
    dut.i_data(in_data);
    dut.i_valid(in_valid);
    dut.o_ready(in_ready);
    dut.o_data(out_data);
    dut.o_valid(out_valid);
    dut.i_ready(out_ready);
    dut.o_occupancy(occupancy);

    fifo_testbench tb{"tb"};
    tb.i_clk(clk);
    tb.o_rst_n(rst_n);
    tb.o_data(in_data);
    tb.o_valid(in_valid);
    tb.i_ready(in_ready);
    tb.i_data(out_data);
    tb.i_valid(out_valid);
    tb.o_ready(out_ready);
    tb.i_occupancy(occupancy);

    sc_core::sc_start();

    const int errors =
        sc_core::sc_report_handler::get_count(sc_core::SC_ERROR);
    if (errors == 0) {
        std::cout << "PASS: ready/valid FIFO\n";
    }
    return errors == 0 ? 0 : 1;
}
