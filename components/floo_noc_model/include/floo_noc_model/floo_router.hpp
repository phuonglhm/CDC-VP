// SPDX-License-Identifier: SHL-0.51

#pragma once

#include "floo_noc_model/floo_types.hpp"
#include "floo_noc_model/ready_valid_fifo.hpp"
#include "floo_noc_model/wormhole_arbiter.hpp"
#include "floo_noc_model/xy_route_select.hpp"

#include <systemc>

namespace floo::model {

template <typename FlitT, unsigned InFifoDepth = 2>
class floo_router : public sc_core::sc_module {
public:
    static constexpr unsigned num_ports = 5;

    sc_core::sc_in<bool> i_clk{"i_clk"};
    sc_core::sc_in<bool> i_rst_n{"i_rst_n"};
    sc_core::sc_in<coordinate> i_router_id{"i_router_id"};

    sc_core::sc_vector<sc_core::sc_in<FlitT>> i_data{
        "i_data", num_ports};
    sc_core::sc_vector<sc_core::sc_in<bool>> i_valid{
        "i_valid", num_ports};
    sc_core::sc_vector<sc_core::sc_out<bool>> o_ready{
        "o_ready", num_ports};

    sc_core::sc_vector<sc_core::sc_out<FlitT>> o_data{
        "o_data", num_ports};
    sc_core::sc_vector<sc_core::sc_out<bool>> o_valid{
        "o_valid", num_ports};
    sc_core::sc_vector<sc_core::sc_in<bool>> i_ready{
        "i_ready", num_ports};

    sc_core::sc_vector<sc_core::sc_out<unsigned>> o_input_occupancy{
        "o_input_occupancy", num_ports};
    sc_core::sc_vector<sc_core::sc_out<unsigned>> o_output_selected{
        "o_output_selected", num_ports};
    sc_core::sc_vector<sc_core::sc_out<bool>> o_output_locked{
        "o_output_locked", num_ports};

    SC_HAS_PROCESS(floo_router);

    explicit floo_router(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
        , input_fifos_("input_fifos", num_ports)
        , route_selectors_("route_selectors", num_ports)
        , output_arbiters_("output_arbiters", num_ports)
        , fifo_data_("fifo_data", num_ports)
        , fifo_valid_("fifo_valid", num_ports)
        , fifo_ready_("fifo_ready", num_ports)
        , routed_data_("routed_data", num_ports)
        , route_index_("route_index", num_ports)
        , route_locked_("route_locked", num_ports)
        , cross_data_("cross_data", num_ports * num_ports)
        , cross_valid_("cross_valid", num_ports * num_ports)
        , cross_ready_("cross_ready", num_ports * num_ports)
    {
        for (unsigned input = 0; input < num_ports; ++input) {
            auto& fifo = input_fifos_[input];
            fifo.i_clk(i_clk);
            fifo.i_rst_n(i_rst_n);
            fifo.i_data(i_data[input]);
            fifo.i_valid(i_valid[input]);
            fifo.o_ready(o_ready[input]);
            fifo.o_data(fifo_data_[input]);
            fifo.o_valid(fifo_valid_[input]);
            fifo.i_ready(fifo_ready_[input]);
            fifo.o_occupancy(o_input_occupancy[input]);

            auto& route = route_selectors_[input];
            route.i_clk(i_clk);
            route.i_rst_n(i_rst_n);
            route.i_router_id(i_router_id);
            route.i_flit(fifo_data_[input]);
            route.i_valid(fifo_valid_[input]);
            route.i_ready(fifo_ready_[input]);
            route.o_flit(routed_data_[input]);
            route.o_route(route_index_[input]);
            route.o_locked(route_locked_[input]);
        }

        for (unsigned output = 0; output < num_ports; ++output) {
            auto& arbiter = output_arbiters_[output];
            arbiter.i_clk(i_clk);
            arbiter.i_rst_n(i_rst_n);
            arbiter.o_data(o_data[output]);
            arbiter.o_valid(o_valid[output]);
            arbiter.i_ready(i_ready[output]);
            arbiter.o_selected(o_output_selected[output]);
            arbiter.o_locked(o_output_locked[output]);

            for (unsigned input = 0; input < num_ports; ++input) {
                const unsigned index = cross_index(output, input);
                arbiter.i_data[input](cross_data_[index]);
                arbiter.i_valid[input](cross_valid_[index]);
                arbiter.o_ready[input](cross_ready_[index]);
            }
        }

        SC_METHOD(connect_crossbar);
        for (unsigned input = 0; input < num_ports; ++input) {
            sensitive << fifo_valid_[input]
                      << routed_data_[input]
                      << route_index_[input];
        }
        for (unsigned index = 0; index < num_ports * num_ports; ++index) {
            sensitive << cross_ready_[index];
        }
    }

private:
    sc_core::sc_vector<ready_valid_fifo<FlitT, InFifoDepth>> input_fifos_;
    sc_core::sc_vector<xy_route_select<FlitT>> route_selectors_;
    sc_core::sc_vector<wormhole_arbiter<FlitT, num_ports>> output_arbiters_;

    sc_core::sc_vector<sc_core::sc_signal<FlitT>> fifo_data_;
    sc_core::sc_vector<sc_core::sc_signal<bool>> fifo_valid_;
    sc_core::sc_vector<sc_core::sc_signal<bool>> fifo_ready_;

    sc_core::sc_vector<sc_core::sc_signal<FlitT>> routed_data_;
    sc_core::sc_vector<sc_core::sc_signal<sc_dt::sc_uint<3>>> route_index_;
    sc_core::sc_vector<sc_core::sc_signal<bool>> route_locked_;

    sc_core::sc_vector<sc_core::sc_signal<FlitT>> cross_data_;
    sc_core::sc_vector<sc_core::sc_signal<bool>> cross_valid_;
    sc_core::sc_vector<sc_core::sc_signal<bool>> cross_ready_;

    static constexpr unsigned cross_index(unsigned output, unsigned input)
    {
        return output * num_ports + input;
    }

    static bool optimized_connection_is_legal(unsigned input, unsigned output)
    {
        if (input == output) {
            return false;
        }

        const bool entered_from_y =
            input == to_port(direction::north)
            || input == to_port(direction::south);
        const bool exits_along_x =
            output == to_port(direction::east)
            || output == to_port(direction::west);

        // Matches XYRouteOpt in floo_router.sv: once traffic entered from a
        // Y-direction it cannot legally return to the X dimension.
        return !(entered_from_y && exits_along_x);
    }

    void connect_crossbar()
    {
        for (unsigned output = 0; output < num_ports; ++output) {
            for (unsigned input = 0; input < num_ports; ++input) {
                const unsigned index = cross_index(output, input);
                const bool selected =
                    route_index_[input].read().to_uint() == output;
                const bool connected =
                    optimized_connection_is_legal(input, output);
                cross_data_[index].write(routed_data_[input].read());
                cross_valid_[index].write(
                    connected && selected && fifo_valid_[input].read());
            }
        }

        for (unsigned input = 0; input < num_ports; ++input) {
            const unsigned output = route_index_[input].read().to_uint();
            const bool legal =
                output < num_ports
                && optimized_connection_is_legal(input, output);
            fifo_ready_[input].write(
                legal ? cross_ready_[cross_index(output, input)].read() : false);
        }
    }
};

} // namespace floo::model
