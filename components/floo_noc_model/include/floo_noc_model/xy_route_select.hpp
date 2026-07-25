// SPDX-License-Identifier: SHL-0.51

#pragma once

#include "floo_noc_model/floo_types.hpp"
#include "floo_noc_model/reference_model.hpp"

#include <systemc>

namespace floo::model {

template <typename FlitT>
class xy_route_select : public sc_core::sc_module {
public:
    sc_core::sc_in<bool> i_clk{"i_clk"};
    sc_core::sc_in<bool> i_rst_n{"i_rst_n"};

    sc_core::sc_in<coordinate> i_router_id{"i_router_id"};
    sc_core::sc_in<FlitT> i_flit{"i_flit"};
    sc_core::sc_in<bool> i_valid{"i_valid"};
    sc_core::sc_in<bool> i_ready{"i_ready"};

    sc_core::sc_out<FlitT> o_flit{"o_flit"};
    sc_core::sc_out<sc_dt::sc_uint<3>> o_route{"o_route"};
    sc_core::sc_out<bool> o_locked{"o_locked"};

    SC_HAS_PROCESS(xy_route_select);

    explicit xy_route_select(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
    {
        SC_METHOD(comb);
        sensitive << i_router_id << i_flit << locked_q_ << route_q_;

        SC_METHOD(seq);
        sensitive << i_clk.pos() << i_rst_n.neg();
        dont_initialize();
    }

private:
    sc_core::sc_signal<bool> locked_q_{"locked_q"};
    sc_core::sc_signal<sc_dt::sc_uint<3>> route_q_{"route_q"};

    unsigned computed_route() const
    {
        return to_port(xy_next_hop(i_router_id.read(), i_flit.read().hdr.dst_id));
    }

    void comb()
    {
        o_flit.write(i_flit.read());
        o_locked.write(locked_q_.read());
        const sc_dt::sc_uint<3> selected_route =
            locked_q_.read()
                ? route_q_.read()
                : sc_dt::sc_uint<3>(computed_route());
        o_route.write(selected_route);
    }

    void seq()
    {
        if (!i_rst_n.read()) {
            locked_q_.write(false);
            route_q_.write(0);
            return;
        }

        if (i_valid.read() && i_ready.read()) {
            if (!locked_q_.read()) {
                route_q_.write(computed_route());
            }
            locked_q_.write(!i_flit.read().hdr.last);
        }
    }
};

} // namespace floo::model
