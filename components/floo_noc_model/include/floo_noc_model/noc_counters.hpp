// SPDX-License-Identifier: SHL-0.51
//
// Measured performance counters for the five-port router.
//
// Scope rule: a counter may only observe a signal whose timing has passed an
// RTL cycle cross-check. At the time of writing that is the router boundary
// handshake (`hw/floo_router.sv`, 214 cycles exact), the input buffer
// (`stream_fifo_optimal_wrap`, 133 cycles exact), and the output arbiter
// (`hw/floo_wormhole_arbiter.sv`, 152 cycles exact). Nothing here observes the
// mesh links or end-to-end latency, because those are still cycle-approximate.
//
// Design rule: this module is a passive observer. It declares `sc_in` ports
// only, never drives a datapath signal, and never gates a process the datapath
// depends on. Attaching it cannot change what the model does. That claim is
// checked, not asserted: `tests/router_trace_sc.cpp` instantiates these
// counters alongside the router under test, so the 214-cycle router
// cross-check runs with them attached.
//
// Reporting rule, from `docs/P0_SCOPE.md`: measured counters and analytic
// estimates must stay separate. Three tiers are distinguished here:
//
//   measured  integers incremented only on an accepted transfer
//             (`valid && ready`) or on a directly sampled state
//   derived   plain arithmetic over measured integers, such as a utilisation
//             ratio; carries no model of its own
//   analytic  a value produced by a formula rather than by observation.
//             This header deliberately provides none.

#pragma once

#include "floo_noc_model/floo_types.hpp"

#include <array>
#include <cstdint>
#include <ostream>
#include <systemc>
#include <vector>

namespace floo::model {

/// Measured counts for one router port direction.
struct port_counters {
    /// Transfers accepted, counted at `valid && ready`.
    std::uint64_t accepted_flits{};
    /// Accepted transfers whose header carried `last`, i.e. completed packets.
    std::uint64_t accepted_packets{};
    /// Cycles where the sender asserted `valid` and was refused.
    std::uint64_t stall_cycles{};
    /// Cycles where the sender asserted `valid`, accepted or not.
    std::uint64_t busy_cycles{};
};

/// Measured counts for one input buffer.
struct buffer_counters {
    /// Highest occupancy observed.
    unsigned high_water{};
    /// Sum of occupancy over all counted cycles; divide by `counted_cycles`
    /// for a mean.
    std::uint64_t occupancy_sum{};
};

/// One immutable router snapshot. The port order is the RTL enum order:
/// North, East, South, West, Eject.
struct router_counter_snapshot {
    static constexpr unsigned num_ports = 5;

    std::uint64_t counted_cycles{};
    std::array<port_counters, num_ports> inputs{};
    std::array<port_counters, num_ports> outputs{};
    std::array<buffer_counters, num_ports> input_buffers{};
    std::array<buffer_counters, num_ports> output_buffers{};
};

/// Runtime-sized snapshot of one physical request or response mesh.
struct mesh_counter_snapshot {
    unsigned width{};
    unsigned height{};
    std::vector<router_counter_snapshot> routers;
};

/// Passive counter block for `floo_router`. Bind its inputs to the same
/// signals the router is already bound to; it drives nothing.
template <typename FlitT, unsigned NumPorts = 5>
class router_counters : public sc_core::sc_module {
public:
    static_assert(
        NumPorts == router_counter_snapshot::num_ports,
        "FlooNoC production counters require the five RTL router ports");

    sc_core::sc_in<bool> i_clk{"i_clk"};
    sc_core::sc_in<bool> i_rst_n{"i_rst_n"};

    sc_core::sc_vector<sc_core::sc_in<FlitT>> i_in_data{"i_in_data", NumPorts};
    sc_core::sc_vector<sc_core::sc_in<bool>> i_in_valid{"i_in_valid", NumPorts};
    sc_core::sc_vector<sc_core::sc_in<bool>> i_in_ready{"i_in_ready", NumPorts};

    sc_core::sc_vector<sc_core::sc_in<FlitT>> i_out_data{"i_out_data", NumPorts};
    sc_core::sc_vector<sc_core::sc_in<bool>> i_out_valid{
        "i_out_valid", NumPorts};
    sc_core::sc_vector<sc_core::sc_in<bool>> i_out_ready{
        "i_out_ready", NumPorts};

    sc_core::sc_vector<sc_core::sc_in<unsigned>> i_input_occupancy{
        "i_input_occupancy", NumPorts};
    sc_core::sc_vector<sc_core::sc_in<unsigned>> i_output_occupancy{
        "i_output_occupancy", NumPorts};

    SC_HAS_PROCESS(router_counters);

    explicit router_counters(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
    {
        SC_METHOD(sample);
        sensitive << i_clk.pos();
        dont_initialize();
    }

    /// Cycles counted, i.e. rising edges observed out of reset.
    std::uint64_t counted_cycles() const { return counted_cycles_; }

    const port_counters& input(unsigned port) const { return inputs_[port]; }
    const port_counters& output(unsigned port) const { return outputs_[port]; }
    const buffer_counters& buffer(unsigned port) const
    {
        return input_buffers_[port];
    }
    const buffer_counters& input_buffer(unsigned port) const
    {
        return input_buffers_[port];
    }
    const buffer_counters& output_buffer(unsigned port) const
    {
        return output_buffers_[port];
    }

    /// Derived: fraction of counted cycles in which this port accepted a
    /// transfer. Plain arithmetic over measured counts, no model.
    double output_utilisation(unsigned port) const
    {
        return counted_cycles_ == 0
            ? 0.0
            : static_cast<double>(outputs_[port].accepted_flits)
                / static_cast<double>(counted_cycles_);
    }

    /// Derived: mean input buffer occupancy.
    double mean_occupancy(unsigned port) const
    {
        return counted_cycles_ == 0
            ? 0.0
            : static_cast<double>(input_buffers_[port].occupancy_sum)
                / static_cast<double>(counted_cycles_);
    }

    /// Derived: mean output-buffer occupancy.
    double mean_output_occupancy(unsigned port) const
    {
        return counted_cycles_ == 0
            ? 0.0
            : static_cast<double>(output_buffers_[port].occupancy_sum)
                / static_cast<double>(counted_cycles_);
    }

    void reset_counts()
    {
        counted_cycles_ = 0;
        for (unsigned port = 0; port < NumPorts; ++port) {
            inputs_[port] = port_counters{};
            outputs_[port] = port_counters{};
            input_buffers_[port] = buffer_counters{};
            output_buffers_[port] = buffer_counters{};
        }
    }

    router_counter_snapshot snapshot() const
    {
        router_counter_snapshot result{};
        result.counted_cycles = counted_cycles_;
        for (unsigned port = 0; port < NumPorts; ++port) {
            result.inputs[port] = inputs_[port];
            result.outputs[port] = outputs_[port];
            result.input_buffers[port] = input_buffers_[port];
            result.output_buffers[port] = output_buffers_[port];
        }
        return result;
    }

    /// Measured counters first, derived ratios clearly separated. No analytic
    /// estimate is emitted.
    void report(std::ostream& out) const
    {
        out << "counted_cycles " << counted_cycles_ << '\n';
        out << "# measured\n";
        for (unsigned port = 0; port < NumPorts; ++port) {
            out << "in" << port
                << " accepted_flits " << inputs_[port].accepted_flits
                << " accepted_packets " << inputs_[port].accepted_packets
                << " stall_cycles " << inputs_[port].stall_cycles
                << " busy_cycles " << inputs_[port].busy_cycles
                << " occupancy_high_water "
                << input_buffers_[port].high_water
                << '\n';
        }
        for (unsigned port = 0; port < NumPorts; ++port) {
            out << "out" << port
                << " accepted_flits " << outputs_[port].accepted_flits
                << " accepted_packets " << outputs_[port].accepted_packets
                << " stall_cycles " << outputs_[port].stall_cycles
                << " busy_cycles " << outputs_[port].busy_cycles
                << " occupancy_high_water "
                << output_buffers_[port].high_water
                << '\n';
        }
        out << "# derived from the measured counters above\n";
        for (unsigned port = 0; port < NumPorts; ++port) {
            out << "out" << port << " utilisation "
                << output_utilisation(port) << '\n';
        }
    }

private:
    std::uint64_t counted_cycles_{};
    port_counters inputs_[NumPorts]{};
    port_counters outputs_[NumPorts]{};
    buffer_counters input_buffers_[NumPorts]{};
    buffer_counters output_buffers_[NumPorts]{};

    void sample()
    {
        // Reset is observed, not counted: the router is not transferring.
        if (!i_rst_n.read()) {
            return;
        }
        ++counted_cycles_;

        for (unsigned port = 0; port < NumPorts; ++port) {
            observe(inputs_[port], i_in_valid[port].read(),
                    i_in_ready[port].read(), i_in_data[port].read());
            observe(outputs_[port], i_out_valid[port].read(),
                    i_out_ready[port].read(), i_out_data[port].read());

            sample_buffer(
                input_buffers_[port], i_input_occupancy[port].read());
            sample_buffer(
                output_buffers_[port], i_output_occupancy[port].read());
        }
    }

    static void sample_buffer(buffer_counters& counters, unsigned occupancy)
    {
        counters.occupancy_sum += occupancy;
        if (occupancy > counters.high_water) {
            counters.high_water = occupancy;
        }
    }

    static void observe(
        port_counters& counters, bool valid, bool ready, const FlitT& flit)
    {
        if (!valid) {
            return;
        }
        ++counters.busy_cycles;
        if (!ready) {
            ++counters.stall_cycles;
            return;
        }
        // Accepted transfer: this is the only place a flit is counted.
        ++counters.accepted_flits;
        if (flit.hdr.last) {
            ++counters.accepted_packets;
        }
    }
};

} // namespace floo::model
