// SPDX-License-Identifier: SHL-0.51

#pragma once

#include <systemc>

namespace floo::model {

template <typename FlitT, unsigned NumRoutes>
class wormhole_arbiter : public sc_core::sc_module {
public:
    static_assert(NumRoutes > 0, "wormhole arbiter needs at least one route");

    sc_core::sc_in<bool> i_clk{"i_clk"};
    sc_core::sc_in<bool> i_rst_n{"i_rst_n"};

    sc_core::sc_vector<sc_core::sc_in<FlitT>> i_data{"i_data", NumRoutes};
    sc_core::sc_vector<sc_core::sc_in<bool>> i_valid{"i_valid", NumRoutes};
    sc_core::sc_vector<sc_core::sc_out<bool>> o_ready{"o_ready", NumRoutes};

    sc_core::sc_out<FlitT> o_data{"o_data"};
    sc_core::sc_out<bool> o_valid{"o_valid"};
    sc_core::sc_in<bool> i_ready{"i_ready"};

    sc_core::sc_out<unsigned> o_selected{"o_selected"};
    sc_core::sc_out<bool> o_locked{"o_locked"};

    SC_HAS_PROCESS(wormhole_arbiter);

    explicit wormhole_arbiter(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
    {
        SC_METHOD(comb);
        sensitive << i_ready << locked_q_ << selected_q_ << rr_next_q_;
        for (unsigned i = 0; i < NumRoutes; ++i) {
            sensitive << i_valid[i] << i_data[i];
        }

        SC_METHOD(seq);
        sensitive << i_clk.pos() << i_rst_n.neg();
        dont_initialize();
    }

private:
    sc_core::sc_signal<bool> locked_q_{"locked_q"};
    sc_core::sc_signal<unsigned> selected_q_{"selected_q"};
    sc_core::sc_signal<unsigned> rr_next_q_{"rr_next_q"};
    sc_core::sc_signal<unsigned> selected_comb_{"selected_comb"};

    unsigned choose_route() const
    {
        if (locked_q_.read()) {
            return selected_q_.read();
        }

        const unsigned start = rr_next_q_.read();
        for (unsigned offset = 0; offset < NumRoutes; ++offset) {
            const unsigned candidate = (start + offset) % NumRoutes;
            if (i_valid[candidate].read()) {
                return candidate;
            }
        }
        return start;
    }

    void comb()
    {
        const unsigned selected = choose_route();
        const bool any_valid = i_valid[selected].read();

        selected_comb_.write(selected);
        o_selected.write(selected);
        o_locked.write(locked_q_.read());
        o_valid.write(any_valid);
        o_data.write(any_valid ? i_data[selected].read() : FlitT{});

        for (unsigned i = 0; i < NumRoutes; ++i) {
            o_ready[i].write(any_valid && i == selected && i_ready.read());
        }
    }

    void seq()
    {
        if (!i_rst_n.read()) {
            locked_q_.write(false);
            selected_q_.write(0);
            rr_next_q_.write(0);
            return;
        }

        if (!(o_valid.read() && i_ready.read())) {
            return;
        }

        const unsigned selected = selected_comb_.read();
        if (o_data.read().hdr.last) {
            locked_q_.write(false);
            rr_next_q_.write((selected + 1) % NumRoutes);
        } else {
            locked_q_.write(true);
            selected_q_.write(selected);
        }
    }
};

} // namespace floo::model
