// SPDX-License-Identifier: SHL-0.51
//
// SystemC mirror of FlooNoC `hw/floo_wormhole_arbiter.sv` at the frozen
// revision, over the locked `rr_arb_tree` (see `rr_arb_tree.hpp`).
//
// The RTL keeps no explicit "selected requester" register. Packet continuity
// comes from two mechanisms:
//
//   * `valid_q` is a snapshot of `valid_i`, refreshed only when the previous
//     snapshot is empty or the previous cycle accepted a `last` flit. The
//     arbiter tree arbitrates over that snapshot, not over the live requests.
//   * the tree's own `LockIn` holds `idx_o` while `gnt_i` is low, and `gnt_i`
//     is `ready_i & last_out`, so the round-robin priority advances only when a
//     packet's `last` flit is accepted.
//
// Consequences that a hand-written arbiter model easily gets wrong, and that
// the earlier model did get wrong:
//
//   * selection is driven by the snapshot, so `valid_o` is the *live* valid of
//     the selected index and is low if that input dropped its request, even
//     when other inputs are asserting;
//   * `ready_o` is asserted on the selected index whenever any input is valid,
//     regardless of that index's own valid;
//   * `data_o` is always driven from the selected index, including when
//     `valid_o` is low;
//   * the next priority is the next *requesting* index above `rr_q`, not
//     `selected + 1`.

#pragma once

#include "floo_noc_model/rr_arb_tree.hpp"

#include <systemc>

namespace floo::model {

template <typename FlitT, unsigned NumRoutes>
class wormhole_arbiter : public sc_core::sc_module {
public:
    static_assert(NumRoutes > 0, "wormhole arbiter needs at least one route");
    static_assert(NumRoutes <= 32, "request masks are carried in an unsigned");

    sc_core::sc_in<bool> i_clk{"i_clk"};
    sc_core::sc_in<bool> i_rst_n{"i_rst_n"};

    sc_core::sc_vector<sc_core::sc_in<FlitT>> i_data{"i_data", NumRoutes};
    sc_core::sc_vector<sc_core::sc_in<bool>> i_valid{"i_valid", NumRoutes};
    sc_core::sc_vector<sc_core::sc_out<bool>> o_ready{"o_ready", NumRoutes};

    sc_core::sc_out<FlitT> o_data{"o_data"};
    sc_core::sc_out<bool> o_valid{"o_valid"};
    sc_core::sc_in<bool> i_ready{"i_ready"};

    /// Debug outputs. `o_selected` is the RTL's `valid_selected_idx`.
    /// `o_locked` is model-defined: it reports that `valid_q` is being held
    /// rather than refreshed, which is the RTL's packet-continuity state.
    sc_core::sc_out<unsigned> o_selected{"o_selected"};
    sc_core::sc_out<bool> o_locked{"o_locked"};

    SC_HAS_PROCESS(wormhole_arbiter);

    explicit wormhole_arbiter(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
    {
        SC_METHOD(comb);
        sensitive << i_ready << valid_q_ << last_q_ << rr_q_ << lock_q_
                  << req_q_;
        for (unsigned i = 0; i < NumRoutes; ++i) {
            sensitive << i_valid[i] << i_data[i];
        }

        SC_METHOD(seq);
        sensitive << i_clk.pos() << i_rst_n.neg();
        dont_initialize();
    }

    /// Registered arbiter state, exposed for the RTL trace comparison.
    unsigned valid_q() const { return valid_q_.read(); }
    bool last_q() const { return last_q_.read(); }
    unsigned rr_q() const { return rr_q_.read(); }
    bool lock_q() const { return lock_q_.read(); }
    unsigned req_q() const { return req_q_.read(); }

private:
    using tree = rr_arb_tree<NumRoutes>;

    sc_core::sc_signal<unsigned> valid_q_{"valid_q"};
    sc_core::sc_signal<bool> last_q_{"last_q"};
    sc_core::sc_signal<unsigned> rr_q_{"rr_q"};
    sc_core::sc_signal<bool> lock_q_{"lock_q"};
    sc_core::sc_signal<unsigned> req_q_{"req_q"};

    struct evaluation {
        unsigned valid_i{};
        unsigned valid_d{};
        typename tree::decision arb{};
        unsigned valid_selected_idx{};
        bool valid_o{};
        FlitT data_o{};
        unsigned ready_o{};
        bool last_out{};
    };

    evaluation evaluate() const
    {
        evaluation out;

        for (unsigned i = 0; i < NumRoutes; ++i) {
            if (i_valid[i].read()) {
                out.valid_i |= 1u << i;
            }
        }

        // always_comb proc_valid:
        //   valid_d = valid_q;
        //   if (valid_q == '0 || last_q) valid_d = valid_i;
        out.valid_d = (valid_q_.read() == 0 || last_q_.read())
            ? out.valid_i
            : valid_q_.read();

        out.arb = tree::evaluate(
            out.valid_d, lock_q_.read(), req_q_.read(), rr_q_.read());

        // assign valid_selected_idx = (|valid_i) ? selected_idx : '0;
        out.valid_selected_idx = out.valid_i != 0 ? out.arb.idx_o : 0u;

        // The index is bounded by NumRoutes whenever any input is valid,
        // because req_d is then non-zero. The guard only protects the model
        // from an out-of-range read in the degenerate all-idle case.
        if (out.valid_selected_idx < NumRoutes) {
            out.valid_o = arb_detail::bit_of(
                out.valid_i, out.valid_selected_idx);
            out.data_o = i_data[out.valid_selected_idx].read();
        }

        // always_comb proc_ready_o:
        //   ready_o = '0;
        //   ready_o[valid_selected_idx] = (|valid_i) ? ready_i : '0;
        if (out.valid_selected_idx < NumRoutes && out.valid_i != 0
            && i_ready.read()) {
            out.ready_o = 1u << out.valid_selected_idx;
        }

        // assign last_out = data_o.hdr.last & valid_o;
        out.last_out = out.data_o.hdr.last && out.valid_o;

        return out;
    }

    void comb()
    {
        const auto state = evaluate();

        o_valid.write(state.valid_o);
        o_data.write(state.data_o);
        for (unsigned i = 0; i < NumRoutes; ++i) {
            o_ready[i].write(arb_detail::bit_of(state.ready_o, i));
        }

        o_selected.write(state.valid_selected_idx);
        o_locked.write(valid_q_.read() != 0 && !last_q_.read());
    }

    void seq()
    {
        if (!i_rst_n.read()) {
            valid_q_.write(0);
            last_q_.write(false);
            rr_q_.write(0);
            lock_q_.write(false);
            req_q_.write(0);
            return;
        }

        const auto state = evaluate();
        const bool gnt_i = i_ready.read() && state.last_out;

        // `FF(valid_q, valid_d, '0)` and `FF(last_q, last_out & ready_i, '0)`
        valid_q_.write(state.valid_d);
        last_q_.write(state.last_out && i_ready.read());

        // rr_arb_tree registers.
        if (gnt_i && state.arb.req_o) {
            rr_q_.write(tree::next_rr(state.arb.req_d, rr_q_.read()));
        }
        lock_q_.write(state.arb.req_o && !gnt_i);
        req_q_.write(state.arb.req_d);
    }
};

} // namespace floo::model
