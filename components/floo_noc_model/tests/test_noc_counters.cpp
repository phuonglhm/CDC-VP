// SPDX-License-Identifier: Apache-2.0
//
// Contract test for the measured router counters.
//
// The absolute expectations below are hand-derived from the RTL-signed input
// buffer, not copied from a run. At depth 2 the buffer is a spill register:
// `ready_o = !a_full_q || !b_full_q`, so with the output continuously drained
// it accepts one flit per cycle, and with the output stalled it accepts
// exactly two before refusing.

#include "floo_noc_model/floo_router.hpp"
#include "floo_noc_model/floo_types.hpp"
#include "floo_noc_model/noc_counters.hpp"

#include <cstdint>
#include <iostream>
#include <string>
#include <systemc>

namespace {

using flit_t = floo::model::test_flit;

constexpr unsigned num_ports = 5;
constexpr unsigned router_x = 1;
constexpr unsigned router_y = 1;

// Port indices follow floo_pkg::route_direction_e.
constexpr unsigned port_east = 1;
constexpr unsigned port_west = 3;

int failures = 0;

void check(bool condition, const std::string& message)
{
    if (!condition) {
        std::cerr << "FAIL @" << sc_core::sc_time_stamp() << ": " << message
                  << '\n';
        ++failures;
    }
}

} // namespace

int sc_main(int, char**)
{
    sc_core::sc_signal<bool> clk{"clk"};
    sc_core::sc_signal<bool> rst_n{"rst_n"};
    sc_core::sc_signal<floo::model::coordinate> router_id{"router_id"};
    sc_core::sc_vector<sc_core::sc_signal<flit_t>> in_data{"in_data", num_ports};
    sc_core::sc_vector<sc_core::sc_signal<bool>> in_valid{"in_valid", num_ports};
    sc_core::sc_vector<sc_core::sc_signal<bool>> in_ready{"in_ready", num_ports};
    sc_core::sc_vector<sc_core::sc_signal<flit_t>> out_data{
        "out_data", num_ports};
    sc_core::sc_vector<sc_core::sc_signal<bool>> out_valid{
        "out_valid", num_ports};
    sc_core::sc_vector<sc_core::sc_signal<bool>> out_ready{
        "out_ready", num_ports};
    sc_core::sc_vector<sc_core::sc_signal<unsigned>> occupancy{
        "occupancy", num_ports};
    sc_core::sc_vector<sc_core::sc_signal<unsigned>> output_occupancy{
        "output_occupancy", num_ports};
    sc_core::sc_vector<sc_core::sc_signal<unsigned>> selected{
        "selected", num_ports};
    sc_core::sc_vector<sc_core::sc_signal<bool>> locked{"locked", num_ports};

    floo::model::floo_router<flit_t, 2> dut{"dut"};
    floo::model::router_counters<flit_t, num_ports> counters{"counters"};

    dut.i_clk(clk);
    dut.i_rst_n(rst_n);
    dut.i_router_id(router_id);
    counters.i_clk(clk);
    counters.i_rst_n(rst_n);
    for (unsigned port = 0; port < num_ports; ++port) {
        dut.i_data[port](in_data[port]);
        dut.i_valid[port](in_valid[port]);
        dut.o_ready[port](in_ready[port]);
        dut.o_data[port](out_data[port]);
        dut.o_valid[port](out_valid[port]);
        dut.i_ready[port](out_ready[port]);
        dut.o_input_occupancy[port](occupancy[port]);
        dut.o_output_occupancy[port](output_occupancy[port]);
        dut.o_output_selected[port](selected[port]);
        dut.o_output_locked[port](locked[port]);

        counters.i_in_data[port](in_data[port]);
        counters.i_in_valid[port](in_valid[port]);
        counters.i_in_ready[port](in_ready[port]);
        counters.i_out_data[port](out_data[port]);
        counters.i_out_valid[port](out_valid[port]);
        counters.i_out_ready[port](out_ready[port]);
        counters.i_input_occupancy[port](occupancy[port]);
        counters.i_output_occupancy[port](output_occupancy[port]);
    }

    const auto idle_flit = [&]() {
        flit_t flit{};
        flit.hdr.dst_id = floo::model::coordinate(router_x, router_y);
        flit.hdr.last = true;
        return flit;
    };

    const auto drive = [&](bool reset_n, unsigned valid_mask,
                           unsigned ready_mask, const flit_t& west_flit) {
        rst_n.write(reset_n);
        for (unsigned port = 0; port < num_ports; ++port) {
            in_data[port].write(port == port_west ? west_flit : idle_flit());
            in_valid[port].write(((valid_mask >> port) & 1u) != 0);
            out_ready[port].write(((ready_mask >> port) & 1u) != 0);
        }
    };

    const auto tick = [&]() {
        clk.write(false);
        sc_core::sc_start(sc_core::sc_time(1, sc_core::SC_NS));
        clk.write(true);
        sc_core::sc_start(sc_core::sc_time(1, sc_core::SC_NS));
    };

    // A single-flit packet from the West input to the East output: the
    // destination is east of the router, so XY routing leaves along X.
    flit_t east_bound{};
    east_bound.hdr.dst_id = floo::model::coordinate(3, router_y);
    east_bound.hdr.last = true;
    east_bound.payload = 0xA5;

    clk.write(false);
    router_id.write(floo::model::coordinate(router_x, router_y));
    drive(false, 0x00, 0x00, idle_flit());
    sc_core::sc_start(sc_core::SC_ZERO_TIME);

    // Reset is observed but never counted.
    tick();
    tick();
    check(counters.counted_cycles() == 0,
          "cycles in reset must not be counted");

    // Phase A: six cycles of West traffic with every output ready.
    // The spill register is drained every cycle, so it accepts one flit per
    // cycle and never back-pressures.
    for (int cycle = 0; cycle < 6; ++cycle) {
        drive(true, 1u << port_west, 0x1F, east_bound);
        tick();
    }
    for (int cycle = 0; cycle < 4; ++cycle) {
        drive(true, 0x00, 0x1F, idle_flit());
        tick();
    }

    check(counters.input(port_west).accepted_flits == 6,
          "West input must accept one flit per offered cycle when drained");
    check(counters.input(port_west).stall_cycles == 0,
          "a continuously drained spill register must not back-pressure");
    check(counters.input(port_west).accepted_packets == 6,
          "every accepted flit carried last, so each is a packet");
    check(counters.buffer(port_west).high_water == 1,
          "a drained spill register never holds more than one flit");
    check(counters.output(port_east).accepted_flits == 6,
          "every accepted flit must leave through the East output");
    check(counters.output(port_east).accepted_packets == 6,
          "East output packet count must match its flit count here");

    for (unsigned port = 0; port < num_ports; ++port) {
        if (port != port_west) {
            check(counters.input(port).accepted_flits == 0,
                  "idle inputs must not accept flits");
        }
        if (port != port_east) {
            check(counters.output(port).accepted_flits == 0,
                  "no other output may carry this traffic");
        }
    }

    counters.reset_counts();

    // Phase B: same traffic with the East output stalled. The spill register
    // takes exactly two flits, then refuses.
    for (int cycle = 0; cycle < 5; ++cycle) {
        drive(true, 1u << port_west, 0x1F & ~(1u << port_east), east_bound);
        tick();
    }

    // Five cycles are offered. The path from the West input to the stalled
    // East output holds two flits in the input spill register plus two in the
    // output one, because the default router carries `OutFifoDepth = 2` — the
    // value every generated FlooNoC router has. So four are accepted and the
    // fifth stalls.
    check(counters.input(port_west).accepted_flits == 4,
          "the input and output spill registers together hold four flits");
    check(counters.input(port_west).stall_cycles == 1,
          "the fifth offered cycle must be counted as a stall");
    check(counters.buffer(port_west).high_water == 2,
          "the spill register must reach full occupancy");
    check(counters.output(port_east).accepted_flits == 0,
          "a stalled output must not accept a transfer");
    check(counters.output(port_east).stall_cycles > 0,
          "a stalled output asserting valid must record stall cycles");
    check(counters.output_buffer(port_east).high_water == 2,
          "the stalled output FIFO must reach its depth-two high-water mark");

    // Release the stall and drain, then check conservation across the router.
    for (int cycle = 0; cycle < 8; ++cycle) {
        drive(true, 0x00, 0x1F, idle_flit());
        tick();
    }

    std::uint64_t total_in = 0;
    std::uint64_t total_out = 0;
    for (unsigned port = 0; port < num_ports; ++port) {
        total_in += counters.input(port).accepted_flits;
        total_out += counters.output(port).accepted_flits;

        // Identity that must hold for any port by construction.
        const auto& in_counts = counters.input(port);
        check(in_counts.busy_cycles
                  == in_counts.accepted_flits + in_counts.stall_cycles,
              "input busy cycles must split into accepted and stalled");
        const auto& out_counts = counters.output(port);
        check(out_counts.busy_cycles
                  == out_counts.accepted_flits + out_counts.stall_cycles,
              "output busy cycles must split into accepted and stalled");
        check(counters.buffer(port).high_water <= 2,
              "occupancy cannot exceed the configured depth");
        check(counters.output_buffer(port).high_water <= 2,
              "output occupancy cannot exceed the configured depth");
    }
    check(total_in == total_out,
          "a drained router must emit exactly what it accepted");
    check(total_in > 0, "the conservation check must not pass vacuously");

    // Phase C: packet count is not another name for flit count. A two-flit
    // packet has one non-last transfer and exactly one last transfer.
    counters.reset_counts();
    auto packet_flit = east_bound;
    packet_flit.hdr.last = false;
    drive(true, 1u << port_west, 0x1F, packet_flit);
    tick();
    packet_flit.hdr.last = true;
    drive(true, 1u << port_west, 0x1F, packet_flit);
    tick();
    for (int cycle = 0; cycle < 4; ++cycle) {
        drive(true, 0x00, 0x1F, idle_flit());
        tick();
    }
    check(counters.input(port_west).accepted_flits == 2
              && counters.input(port_west).accepted_packets == 1,
          "a two-flit input packet must increment packet count only on last");
    check(counters.output(port_east).accepted_flits == 2
              && counters.output(port_east).accepted_packets == 1,
          "a two-flit output packet must increment packet count only on last");

    if (failures == 0) {
        std::cout << "PASS: router measured counters\n";
        return 0;
    }
    std::cerr << "FAIL: " << failures << " counter checks failed\n";
    return 1;
}
