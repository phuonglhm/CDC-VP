// SPDX-License-Identifier: SHL-0.51
//
// Response ordering for the frozen v0 configuration.
//
// Source of truth: the `NoRoB` branch of `hw/floo_rob_wrapper.sv`, over
// `axi_demux_id_counters` in the locked axi `src/axi_demux_simple.sv` and
// `delta_counter` in common_cells.
//
// `floo_pkg::ChimneyDefaultCfg` sets `BRoBType` and `RRoBType` to `NoRoB`, and
// `floogen/examples/axi_mesh_xy.yml` does not override them. `NoRoB` does not
// mean "no ordering logic". The RTL's own comment says it
//
//   "stalls transactions of the same ID going to different destinations until
//    the previous transaction is completed"
//
// and implements exactly that:
//
//   push        = ax_valid_i && (!in_flight || ax_dest_i == prev_dest)
//                 && !counter_full
//   pop         = rsp_valid_i && rsp_last_i
//   ax_valid_o  = push
//   ax_ready_o  = push && ax_ready_i
//
// where `in_flight`, `prev_dest`, and `counter_full` come from an
// `axi_demux_id_counters` looked up by the request's AXI ID.
//
// Three details of that counter bank are easy to get wrong, and the earlier
// transaction-level model got all three wrong. All are RTL-signed by
// `rtl_crosscheck/run_rob_crosscheck.sh`:
//
//   * `full_o = |cnt_full` is a **global** OR across all `2**AxiIdBits`
//     counters. One saturated ID therefore stalls *every* ID, not just its
//     own. There is no per-ID full signal on this module's boundary.
//   * `cnt_full[i] = overflow | (&in_flight)` saturates when the counter's
//     `$clog2(MaxRoTxnsPerId)` bits are all ones, so the real capacity is
//     `2**$clog2(MaxRoTxnsPerId) - 1`, not `MaxRoTxnsPerId`. At the default
//     `MaxRoTxnsPerId = 32` that is 31 outstanding, not 32.
//   * the counter is popped by `rsp_i.id`, the ID carried by the *response*,
//     and only on a response handshake with `rsp_last_i` set.
//
// The reordering RoB types are a different branch of the same wrapper and are
// out of v0 scope; modelling them requires freezing a configuration that
// enables one.

#pragma once

#include "floo_noc_model/floo_types.hpp"
#include "floo_noc_model/rr_arb_tree.hpp"

#include <systemc>

#include <array>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace floo::model {

/// Mirrors `axi_demux_id_counters` (axi `src/axi_demux_simple.sv`) with
/// `inject_i` tied low, which is how `floo_rob_wrapper.sv` instantiates it.
///
/// `delta_counter` holds `WIDTH + 1` bits and exports `q_o` as the low `WIDTH`
/// and `overflow_o` as the top bit, so that shape is reproduced here rather
/// than collapsed into a saturating counter.
template <unsigned AxiIdBits, unsigned CounterWidth>
class id_counter_bank {
public:
    static constexpr unsigned num_counters = 1u << AxiIdBits;
    /// `q_o` mask: `counter_q[WIDTH-1:0]`.
    static constexpr std::uint32_t q_mask =
        CounterWidth == 0 ? 0u : (1u << CounterWidth) - 1u;
    /// `overflow_o`: `counter_q[WIDTH]`.
    static constexpr std::uint32_t overflow_bit = 1u << CounterWidth;
    static constexpr std::uint32_t counter_mask = (1u << (CounterWidth + 1)) - 1u;

    void reset()
    {
        counter_.fill(0);
        select_.fill(coordinate{});
    }

    /// `in_flight` inside the generate loop, i.e. `delta_counter`'s `q_o`.
    std::uint32_t in_flight(unsigned axi_id) const
    {
        return counter_[axi_id] & q_mask;
    }

    /// `overflow_o`, the counter bit above `q_o`.
    bool overflow(unsigned axi_id) const
    {
        return (counter_[axi_id] & overflow_bit) != 0u;
    }

    /// `occupied[i] = |in_flight`.
    bool occupied(unsigned axi_id) const { return in_flight(axi_id) != 0u; }

    /// `cnt_full[i] = overflow | (&in_flight)`.
    bool counter_full(unsigned axi_id) const
    {
        return overflow(axi_id) || in_flight(axi_id) == q_mask;
    }

    /// `full_o = |cnt_full`. Global across the whole bank: this is the signal
    /// the `NoRoB` branch consumes, and it is not qualified by the looked-up
    /// ID.
    bool full() const
    {
        for (unsigned index = 0; index < num_counters; ++index) {
            if (counter_full(index)) {
                return true;
            }
        }
        return false;
    }

    /// `lookup_mst_select_o = mst_select_q[lookup_axi_id_i]`.
    coordinate select(unsigned axi_id) const { return select_[axi_id]; }

    /// One clock edge. `push`/`pop` are the already-qualified handshake
    /// enables the RTL feeds in, not the raw valid signals.
    void step(
        bool push,
        unsigned push_id,
        const coordinate& push_select,
        bool pop,
        unsigned pop_id)
    {
        for (unsigned index = 0; index < num_counters; ++index) {
            const bool push_en = push && push_id == index;
            const bool pop_en = pop && pop_id == index;

            // `unique case ({push_en, inject_en, pop_en})` with `inject_en`
            // tied low. 3'b101 (push and pop together) falls to the default
            // arm, so the counter holds.
            if (push_en && !pop_en) {
                counter_[index] = (counter_[index] + 1u) & counter_mask;
            } else if (pop_en && !push_en) {
                counter_[index] = (counter_[index] - 1u) & counter_mask;
            }

            // `FFLARN(mst_select_q[i], push_mst_select_i, push_en[i], ...)`:
            // the load enable is the push alone, and the register is never
            // cleared when the counter drains.
            if (push_en) {
                select_[index] = push_select;
            }
        }
    }

private:
    std::array<std::uint32_t, num_counters> counter_{};
    std::array<coordinate, num_counters> select_{};
};

/// SystemC mirror of the `NoRoB` branch of `hw/floo_rob_wrapper.sv`.
///
/// The response path is a pass-through in this branch, so only the request
/// admission logic and the counter bank carry behaviour.
template <unsigned AxiIdBits, unsigned MaxRoTxnsPerId>
class no_rob_gate : public sc_core::sc_module {
public:
    static_assert(AxiIdBits >= 1, "the counter bank needs at least one ID bit");
    static_assert(MaxRoTxnsPerId >= 2, "$clog2 degenerates below two");

    /// `localparam int unsigned CounterWidth = $clog2(MaxRoTxnsPerId);`
    static constexpr unsigned counter_width =
        arb_detail::clog2(MaxRoTxnsPerId);
    /// What the bank actually admits, which is not `MaxRoTxnsPerId`.
    static constexpr unsigned capacity = (1u << counter_width) - 1u;

    sc_core::sc_in<bool> i_clk{"i_clk"};
    sc_core::sc_in<bool> i_rst_n{"i_rst_n"};

    sc_core::sc_in<bool> i_ax_valid{"i_ax_valid"};
    sc_core::sc_out<bool> o_ax_ready{"o_ax_ready"};
    sc_core::sc_in<unsigned> i_ax_id{"i_ax_id"};
    sc_core::sc_in<coordinate> i_ax_dest{"i_ax_dest"};

    sc_core::sc_out<bool> o_ax_valid{"o_ax_valid"};
    sc_core::sc_in<bool> i_ax_ready{"i_ax_ready"};
    sc_core::sc_out<bool> o_ax_rob_req{"o_ax_rob_req"};
    sc_core::sc_out<unsigned> o_ax_rob_idx{"o_ax_rob_idx"};

    sc_core::sc_in<bool> i_rsp_valid{"i_rsp_valid"};
    sc_core::sc_out<bool> o_rsp_ready{"o_rsp_ready"};
    sc_core::sc_in<unsigned> i_rsp_id{"i_rsp_id"};
    sc_core::sc_in<bool> i_rsp_last{"i_rsp_last"};
    sc_core::sc_out<bool> o_rsp_valid{"o_rsp_valid"};
    sc_core::sc_in<bool> i_rsp_ready{"i_rsp_ready"};

    SC_HAS_PROCESS(no_rob_gate);

    explicit no_rob_gate(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
    {
        counters_.reset();

        SC_METHOD(comb);
        sensitive << i_ax_valid << i_ax_id << i_ax_dest << i_ax_ready
                  << i_rsp_valid << i_rsp_id << i_rsp_last << i_rsp_ready
                  << state_epoch_;

        SC_METHOD(seq);
        sensitive << i_clk.pos() << i_rst_n.neg();
        dont_initialize();
    }

    /// Internal signals exported for the RTL trace comparison. These are the
    /// three that decide admission.
    bool in_flight() const { return counters_.occupied(i_ax_id.read()); }
    coordinate prev_dest() const { return counters_.select(i_ax_id.read()); }
    bool counter_full() const { return counters_.full(); }
    std::uint32_t outstanding(unsigned axi_id) const
    {
        return counters_.in_flight(axi_id);
    }
    /// `mst_select_q[axi_id]`, per counter rather than through the lookup port.
    coordinate select_of(unsigned axi_id) const
    {
        return counters_.select(axi_id);
    }

private:
    bool push() const
    {
        return i_ax_valid.read() && (!in_flight() || i_ax_dest.read() == prev_dest())
            && !counter_full();
    }

    void comb()
    {
        const bool push_now = push();

        o_ax_valid.write(push_now);
        o_ax_ready.write(push_now && i_ax_ready.read());

        // `assign ax_rob_req_o = 1'b1;` even here, which is why every request
        // flit leaves the chimney with `rob_req` set.
        o_ax_rob_req.write(true);
        o_ax_rob_idx.write(0);

        o_rsp_ready.write(i_rsp_ready.read());
        o_rsp_valid.write(i_rsp_valid.read());
    }

    void seq()
    {
        if (!i_rst_n.read()) {
            counters_.reset();
        } else {
            const bool pop = i_rsp_valid.read() && i_rsp_last.read();
            counters_.step(
                push() && i_ax_ready.read(),
                i_ax_id.read(),
                i_ax_dest.read(),
                pop && i_rsp_ready.read(),
                i_rsp_id.read());
        }
        // The counter bank is plain state, so the combinational process has
        // nothing to be sensitive to. This signal republishes "the bank
        // changed" into the SystemC event graph.
        state_epoch_.write(state_epoch_.read() + 1);
    }

    id_counter_bank<AxiIdBits, counter_width> counters_{};
    sc_core::sc_signal<unsigned> state_epoch_{"state_epoch", 0};
};

/// Transaction-level view of the same rule, for callers that drive the network
/// through `axi_endpoint.hpp` rather than a signal-level port list.
///
/// This is the *same* arithmetic as `no_rob_gate`, only without the clock: the
/// global `full_o` and the `2**$clog2(MaxRoTxnsPerId) - 1` capacity are both
/// reproduced. It is a convenience wrapper, not a second model.
class no_rob_order_gate {
public:
    /// `MaxRoTxnsPerId` in the RTL, which the chimney drives from
    /// `ChimneyCfg.MaxTxnsPerId`, and the AXI ID width feeding the bank.
    explicit no_rob_order_gate(unsigned max_txns_per_id, unsigned axi_id_bits = 4)
        : counter_width_(arb_detail::clog2(max_txns_per_id))
        , counters_(std::size_t{1} << axi_id_bits, 0u)
        , selects_(std::size_t{1} << axi_id_bits)
    {
        if (max_txns_per_id < 2) {
            throw std::invalid_argument(
                "no_rob_order_gate: MaxRoTxnsPerId below two degenerates");
        }
        if (axi_id_bits == 0 || axi_id_bits > 16) {
            throw std::invalid_argument("no_rob_order_gate: bad AXI ID width");
        }
    }

    /// What the counter bank admits per ID, which is one less than a reader of
    /// `MaxRoTxnsPerId` would expect.
    unsigned capacity() const { return (1u << counter_width_) - 1u; }

    /// `push = ax_valid_i && (!in_flight || ax_dest_i == prev_dest)
    ///         && !counter_full`
    bool may_issue(std::uint64_t axi_id, const coordinate& destination) const
    {
        const auto index = checked_index(axi_id);
        if (full()) {
            return false;
        }
        if (counters_[index] == 0u) {
            return true;
        }
        return selects_[index] == destination;
    }

    void issue(std::uint64_t axi_id, const coordinate& destination)
    {
        if (!may_issue(axi_id, destination)) {
            throw std::logic_error(
                "no_rob_order_gate: issued a request the gate would block");
        }
        const auto index = checked_index(axi_id);
        selects_[index] = destination;
        ++counters_[index];
    }

    void complete(std::uint64_t axi_id)
    {
        const auto index = checked_index(axi_id);
        if (counters_[index] == 0u) {
            throw std::runtime_error(
                "no_rob_order_gate: response for an ID with nothing in flight");
        }
        --counters_[index];
    }

    unsigned outstanding(std::uint64_t axi_id) const
    {
        return counters_[checked_index(axi_id)];
    }

    /// `full_o`, the global OR. Exposed because a caller that sees `may_issue`
    /// refuse a fresh ID needs to be able to tell this apart from a
    /// destination conflict.
    bool full() const
    {
        for (const auto count : counters_) {
            if (count >= capacity()) {
                return true;
            }
        }
        return false;
    }

    /// The destination the in-flight transactions of this ID went to. Only
    /// meaningful while `outstanding(axi_id)` is nonzero.
    coordinate destination_of(std::uint64_t axi_id) const
    {
        return selects_[checked_index(axi_id)];
    }

private:
    std::size_t checked_index(std::uint64_t axi_id) const
    {
        if (axi_id >= counters_.size()) {
            throw std::out_of_range(
                "no_rob_order_gate: AXI ID outside the counter bank");
        }
        return static_cast<std::size_t>(axi_id);
    }

    unsigned counter_width_;
    std::vector<unsigned> counters_;
    std::vector<coordinate> selects_;
};

} // namespace floo::model
