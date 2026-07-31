// SPDX-License-Identifier: SHL-0.51
//
// SystemC mirror of `hw/floo_axi_chimney.sv` in the frozen v0 configuration, at
// cycle granularity. All four of the chimney's quadrants live here, and all
// four are RTL-signed:
//
//   axi_chimney_request           manager AXI  -> `req` link    signed, 141 cyc
//   axi_chimney_response          `req` link   -> AXI out -> `rsp`
//                                                              signed, 221 cyc
//   axi_chimney_manager_response  `rsp` link   -> manager AXI   signed,  78 cyc
//
// This is the composition step. Every part it wires together is already
// RTL-signed on its own:
//
//   stream_fifo.hpp        `spill_register` (133 cycles)
//   wormhole_arbiter.hpp   `floo_wormhole_arbiter` at 2 routes (152 cycles)
//   rob_order_gate.hpp     the `NoRoB` admission rule (127 cycles)
//   axi_chimney_pack.hpp   flit assembly (16 request flits, 8 response flits)
//
// What was never checked is whether they are wired together correctly, and in
// particular how AW, W, and AR contend for the single `req` link. That is what
// this header exists to make comparable.
//
// ## The frozen configuration has no cuts
//
// `floo_pkg::ChimneyDefaultCfg` sets **`CutAx = 0`, `CutOup = 0`, and
// `CutRsp = 0`**, and `hw/test/floo_test_pkg.sv` takes the defaults unchanged.
// So in v0:
//
//   * `gen_no_ax_cuts` wires AW and AR straight through, no spill register;
//   * `gen_no_rsp_cuts` wires the inbound links straight through;
//   * `i_req_out_cut` is instantiated with `Bypass = !CutOup = 1`, and
//     `spill_register_flushable`'s bypass branch is `valid_o = valid_i`,
//     `ready_o = ready_i`, `data_o = data_i` — fully transparent.
//
// This corrects a claim in the earlier roadmap, which assumed the chimney's
// latency came from cuts that had to be modelled. It does not: in the frozen
// configuration the request path's only state is the `aw_w_sel_q` FSM, the
// wormhole arbiter's registers, and the reorder-buffer counters. The one
// unconditional spill register in the module sits on the *subordinate* side
// (`i_aw_out_queue`) and is outside this header's scope.
//
// ## Scope
//
// `AtopSupport` with no ATOP in the stimulus, `EnMgrPort = 1`,
// `MaxUniqueIds = 1`. The class comments below give each module's own scope.
// All four quadrants are RTL-signed as of Step A-1, the twelfth cross-check.

#pragma once

#include "floo_noc_model/axi_chimney_pack.hpp"
#include "floo_noc_model/rob_order_gate.hpp"
#include "floo_noc_model/stream_fifo.hpp"
#include "floo_noc_model/wormhole_arbiter.hpp"

#include <systemc>

#include <array>
#include <cstdint>

namespace floo::model {

/// Request-path arbiter inputs, in the RTL's index order.
enum class req_arb_input : unsigned { w = 0, ar = 1 };

/// Mirrors the request path of `floo_axi_chimney.sv`.
///
/// `AxiIdBits` and `MaxTxnsPerId` size the reorder-buffer counter banks, which
/// the chimney drives from `ChimneyCfg.MaxTxnsPerId`.
template <unsigned AxiIdBits, unsigned MaxTxnsPerId>
class axi_chimney_request : public sc_core::sc_module {
public:
    sc_core::sc_in<bool> i_clk{"i_clk"};
    sc_core::sc_in<bool> i_rst_n{"i_rst_n"};

    /// This chimney's own coordinate, `id_i` in the RTL.
    sc_core::sc_in<coordinate> i_node_id{"i_node_id"};

    // ---- AXI manager port, request side --------------------------------
    sc_core::sc_in<bool> i_aw_valid{"i_aw_valid"};
    sc_core::sc_out<bool> o_aw_ready{"o_aw_ready"};
    sc_core::sc_in<axi_aw_chan> i_aw{"i_aw"};
    sc_core::sc_in<coordinate> i_aw_dest{"i_aw_dest"};

    sc_core::sc_in<bool> i_w_valid{"i_w_valid"};
    sc_core::sc_out<bool> o_w_ready{"o_w_ready"};
    sc_core::sc_in<axi_w_chan> i_w{"i_w"};

    sc_core::sc_in<bool> i_ar_valid{"i_ar_valid"};
    sc_core::sc_out<bool> o_ar_ready{"o_ar_ready"};
    sc_core::sc_in<axi_ar_chan> i_ar{"i_ar"};
    sc_core::sc_in<coordinate> i_ar_dest{"i_ar_dest"};

    // ---- `req` physical link -------------------------------------------
    sc_core::sc_out<axi_req_flit> o_req_data{"o_req_data"};
    sc_core::sc_out<bool> o_req_valid{"o_req_valid"};
    sc_core::sc_in<bool> i_req_ready{"i_req_ready"};

    // ---- Response drain, so the RoB counters can be released ------------
    //
    // The chimney pops the B counter from its unpacker and the R counter from
    // the meta buffer. Neither is modelled *inside this module*, so the drain is
    // an input: the timing harness drives it in lockstep with the RTL's own
    // response stream, and in the integrated datapath
    // `axi_chimney_manager_response` drives it from the arriving `rsp` flits.
    sc_core::sc_in<bool> i_b_pop{"i_b_pop"};
    sc_core::sc_in<unsigned> i_b_pop_id{"i_b_pop_id"};
    sc_core::sc_in<bool> i_r_pop{"i_r_pop"};
    sc_core::sc_in<unsigned> i_r_pop_id{"i_r_pop_id"};

    /// `RLAST` of the arriving R beat.
    ///
    /// The reorder buffer releases a counter on `rsp_valid_i && rsp_last_i`, so
    /// a read burst must release **once**, on its final beat. The RTL wires
    /// this per direction and the two directions differ:
    ///
    /// ```systemverilog
    /// i_b_rob: .rsp_last_i ( 1'b1 )                            // B is single-beat
    /// i_r_rob: .rsp_last_i ( floo_rsp_in.axi_r.payload.last )  // R is not
    /// ```
    ///
    /// An earlier revision tied both high, so every beat of a burst released a
    /// counter and a multi-beat read under-counted its outstanding reads. The
    /// request-timing cross-check cannot catch that: it holds the response link
    /// idle, so the counters only ever fill. B keeps the constant, matching the
    /// RTL.
    sc_core::sc_in<bool> i_r_pop_last{"i_r_pop_last"};

    // ---- RoB response-side handshake, exposed for the unpacker -----------
    //
    // `axi_ready_out[AxiB] = b_rob_ready_out` in the RTL, and
    // `floo_rsp_out_ready = axi_ready_out[hdr.axi_ch]`. So the inbound `rsp`
    // link's ready comes from whichever of these two the arriving flit selects.
    // The timing harness binds `i_*_rsp_ready` high, which is what it always
    // was internally; exposing it changes no behaviour and the 141-cycle
    // cross-check is re-run to prove that.
    sc_core::sc_out<bool> o_b_rsp_ready{"o_b_rsp_ready"};
    sc_core::sc_out<bool> o_r_rsp_ready{"o_r_rsp_ready"};
    sc_core::sc_in<bool> i_b_rsp_ready{"i_b_rsp_ready"};
    sc_core::sc_in<bool> i_r_rsp_ready{"i_r_rsp_ready"};

    SC_HAS_PROCESS(axi_chimney_request);

    explicit axi_chimney_request(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
    {
        bind_b_rob();
        bind_r_rob();
        bind_arbiter();

        SC_METHOD(comb);
        sensitive << i_node_id << i_aw_valid << i_aw << i_aw_dest << i_w_valid
                  << i_w << i_ar_valid << i_ar << i_ar_dest << i_req_ready
                  << aw_w_sel_q_ << aw_rob_valid_out_ << ar_rob_valid_out_
                  << aw_rob_ready_out_ << ar_rob_ready_out_ << aw_rob_req_out_
                  << ar_rob_req_out_ << arb_gnt_w_ << arb_gnt_ar_
                  << arb_valid_ << arb_data_;

        SC_METHOD(seq);
        sensitive << i_clk.pos() << i_rst_n.neg();
        dont_initialize();
    }

    /// `aw_w_sel_q`, exported for the RTL trace comparison.
    bool selects_aw() const { return !aw_w_sel_q_.read(); }
    /// Reorder-buffer occupancy per direction.
    unsigned b_outstanding(unsigned axi_id) const
    {
        return b_rob_.outstanding(axi_id);
    }
    unsigned r_outstanding(unsigned axi_id) const
    {
        return r_rob_.outstanding(axi_id);
    }
    /// Wormhole arbiter state.
    unsigned arb_valid_q() const { return arbiter_.valid_q(); }
    bool arb_last_q() const { return arbiter_.last_q(); }
    unsigned arb_rr_q() const { return arbiter_.rr_q(); }
    bool arb_lock_q() const { return arbiter_.lock_q(); }
    unsigned arb_req_q() const { return arbiter_.req_q(); }

private:
    void bind_b_rob()
    {
        b_rob_.i_clk(i_clk);
        b_rob_.i_rst_n(i_rst_n);
        b_rob_.i_ax_valid(aw_rob_valid_in_);
        b_rob_.o_ax_ready(aw_rob_ready_out_);
        b_rob_.i_ax_id(aw_rob_id_);
        b_rob_.i_ax_dest(i_aw_dest);
        b_rob_.o_ax_valid(aw_rob_valid_out_);
        b_rob_.i_ax_ready(aw_rob_ready_in_);
        b_rob_.o_ax_rob_req(aw_rob_req_out_);
        b_rob_.o_ax_rob_idx(aw_rob_idx_out_);
        b_rob_.i_rsp_valid(i_b_pop);
        b_rob_.o_rsp_ready(o_b_rsp_ready);
        b_rob_.i_rsp_id(i_b_pop_id);
        b_rob_.i_rsp_last(const_true_);
        b_rob_.o_rsp_valid(b_rsp_valid_out_);
        b_rob_.i_rsp_ready(i_b_rsp_ready);
    }

    void bind_r_rob()
    {
        r_rob_.i_clk(i_clk);
        r_rob_.i_rst_n(i_rst_n);
        r_rob_.i_ax_valid(i_ar_valid);
        r_rob_.o_ax_ready(ar_rob_ready_out_);
        r_rob_.i_ax_id(ar_rob_id_);
        r_rob_.i_ax_dest(i_ar_dest);
        r_rob_.o_ax_valid(ar_rob_valid_out_);
        r_rob_.i_ax_ready(ar_rob_ready_in_);
        r_rob_.o_ax_rob_req(ar_rob_req_out_);
        r_rob_.o_ax_rob_idx(ar_rob_idx_out_);
        r_rob_.i_rsp_valid(i_r_pop);
        r_rob_.o_rsp_ready(o_r_rsp_ready);
        r_rob_.i_rsp_id(i_r_pop_id);
        r_rob_.i_rsp_last(i_r_pop_last);
        r_rob_.o_rsp_valid(r_rsp_valid_out_);
        r_rob_.i_rsp_ready(i_r_rsp_ready);
    }

    void bind_arbiter()
    {
        arbiter_.i_clk(i_clk);
        arbiter_.i_rst_n(i_rst_n);
        // **AR is arbiter index 0, W is index 1.** The chimney declares
        //
        //   floo_req_chan_t [AxiW:AxiAr] floo_req_arb_in;
        //
        // and `AxiW = 1`, `AxiAr = 2`, so that packed range is **ascending**,
        // `[1:2]`. In an ascending packed range the first index is the most
        // significant element, so connecting it to `data_i[NumRoutes-1:0]`
        // puts the `AxiW` slot on bit 1 and the `AxiAr` slot on bit 0 — the
        // reverse of the declaration's reading order.
        //
        // This decides round-robin priority whenever AW and AR contend, so
        // getting it backwards is observable. It was found by this
        // cross-check, not by reading the declaration.
        arbiter_.i_data[0](arb_data_ar_);
        arbiter_.i_valid[0](arb_req_ar_);
        arbiter_.o_ready[0](arb_gnt_ar_);
        arbiter_.i_data[1](arb_data_w_);
        arbiter_.i_valid[1](arb_req_w_);
        arbiter_.o_ready[1](arb_gnt_w_);
        arbiter_.o_data(arb_data_);
        arbiter_.o_valid(arb_valid_);
        // `i_req_out_cut` has `Bypass = !CutOup = 1`, so the arbiter's
        // `ready_i` is the link's `ready` with no register in between.
        arbiter_.i_ready(i_req_ready);
        arbiter_.o_selected(arb_selected_);
        arbiter_.o_locked(arb_locked_);
    }

    void comb()
    {
        const bool sel_aw = !aw_w_sel_q_.read();
        const auto aw = i_aw.read();

        // `gen_atop_support`: the AW/B reorder buffer is bypassed for ATOPs.
        // The stimulus carries none, so this reduces to a pass-through, but the
        // qualification is kept because it is what the RTL computes.
        const bool is_atop = aw.atop != 0;
        aw_rob_valid_in_.write(i_aw_valid.read() && !is_atop);
        aw_rob_id_.write(static_cast<unsigned>(aw.id));
        ar_rob_id_.write(static_cast<unsigned>(i_ar.read().id));

        // `aw_rob_ready_in = floo_req_arb_gnt_out[AxiW] && (aw_w_sel_q == SelAw)`
        const bool aw_rob_ready_in = arb_gnt_w_.read() && sel_aw;
        aw_rob_ready_in_.write(aw_rob_ready_in);
        // `ar_rob_ready_in = floo_req_arb_gnt_out[AxiAr]`
        ar_rob_ready_in_.write(arb_gnt_ar_.read());

        // `axi_aw_queue_ready_in`, and with `CutAx = 0` this is also the AXI
        // port's `aw_ready`.
        const bool aw_queue_ready =
            is_atop ? aw_rob_ready_in : aw_rob_ready_out_.read();
        o_aw_ready.write(aw_queue_ready);
        // With `CutAx = 0` the AR port's ready is the R RoB's `ax_ready_o`.
        o_ar_ready.write(ar_rob_ready_out_.read());
        // `axi_rsp_out.w_ready = floo_req_arb_gnt_out[AxiW] && (sel == SelW)`
        o_w_ready.write(arb_gnt_w_.read() && !sel_aw);

        // `floo_req_arb_req_in[AxiW]`
        const bool w_slot_req =
            (sel_aw && (aw_rob_valid_out_.read() || (is_atop && i_aw_valid.read())))
            || (!sel_aw && i_w_valid.read());
        arb_req_w_.write(w_slot_req);
        arb_req_ar_.write(ar_rob_valid_out_.read());

        // `floo_req_arb_in[AxiW] = (sel == SelAw) ? floo_axi_aw : floo_axi_w`
        const rob_tag aw_tag{aw_rob_req_out_.read(), aw_rob_idx_out_.read()};
        const rob_tag ar_tag{ar_rob_req_out_.read(), ar_rob_idx_out_.read()};
        arb_data_w_.write(
            sel_aw ? pack_aw(aw, i_node_id.read(), i_aw_dest.read(), aw_tag)
                   : pack_w(i_w.read(), i_node_id.read(), w_dest_q_.read(),
                            aw_tag));
        arb_data_ar_.write(
            pack_ar(i_ar.read(), i_node_id.read(), i_ar_dest.read(), ar_tag));

        // `i_req_out_cut` is transparent at `CutOup = 0`.
        o_req_valid.write(arb_valid_.read());
        o_req_data.write(arb_data_.read());
    }

    void seq()
    {
        if (!i_rst_n.read()) {
            aw_w_sel_q_.write(false);  // `FF(aw_w_sel_q, ..., SelAw)`
            w_dest_q_.write(coordinate{});
            return;
        }

        // `aw_w_sel_d` in the RTL's order: an accepted `w.last` wins over an
        // accepted AW in the same cycle.
        bool next = aw_w_sel_q_.read();
        const bool aw_accepted = i_aw_valid.read() && o_aw_ready.read();
        if (aw_accepted) {
            next = true;  // SelW
        }
        if (i_w_valid.read() && o_w_ready.read() && i_w.read().last) {
            next = false;  // SelAw
        }
        aw_w_sel_q_.write(next);

        // The W flits of a burst inherit the destination latched when the AW
        // was accepted; W never decodes an address of its own.
        if (aw_accepted) {
            w_dest_q_.write(i_aw_dest.read());
        }
    }

    no_rob_gate<AxiIdBits, MaxTxnsPerId> b_rob_{"b_rob"};
    no_rob_gate<AxiIdBits, MaxTxnsPerId> r_rob_{"r_rob"};
    wormhole_arbiter<axi_req_flit, 2> arbiter_{"req_arbiter"};

    sc_core::sc_signal<bool> const_true_{"const_true", true};

    sc_core::sc_signal<bool> aw_rob_valid_in_{"aw_rob_valid_in"};
    sc_core::sc_signal<bool> aw_rob_ready_out_{"aw_rob_ready_out"};
    sc_core::sc_signal<bool> aw_rob_valid_out_{"aw_rob_valid_out"};
    sc_core::sc_signal<bool> aw_rob_ready_in_{"aw_rob_ready_in"};
    sc_core::sc_signal<bool> aw_rob_req_out_{"aw_rob_req_out"};
    sc_core::sc_signal<unsigned> aw_rob_idx_out_{"aw_rob_idx_out"};
    sc_core::sc_signal<unsigned> aw_rob_id_{"aw_rob_id"};
    sc_core::sc_signal<bool> b_rsp_valid_out_{"b_rsp_valid_out"};

    sc_core::sc_signal<bool> ar_rob_ready_out_{"ar_rob_ready_out"};
    sc_core::sc_signal<bool> ar_rob_valid_out_{"ar_rob_valid_out"};
    sc_core::sc_signal<bool> ar_rob_ready_in_{"ar_rob_ready_in"};
    sc_core::sc_signal<bool> ar_rob_req_out_{"ar_rob_req_out"};
    sc_core::sc_signal<unsigned> ar_rob_idx_out_{"ar_rob_idx_out"};
    sc_core::sc_signal<unsigned> ar_rob_id_{"ar_rob_id"};
    sc_core::sc_signal<bool> r_rsp_valid_out_{"r_rsp_valid_out"};

    sc_core::sc_signal<axi_req_flit> arb_data_w_{"arb_data_w"};
    sc_core::sc_signal<axi_req_flit> arb_data_ar_{"arb_data_ar"};
    sc_core::sc_signal<bool> arb_req_w_{"arb_req_w"};
    sc_core::sc_signal<bool> arb_req_ar_{"arb_req_ar"};
    sc_core::sc_signal<bool> arb_gnt_w_{"arb_gnt_w"};
    sc_core::sc_signal<bool> arb_gnt_ar_{"arb_gnt_ar"};
    sc_core::sc_signal<axi_req_flit> arb_data_{"arb_data"};
    sc_core::sc_signal<bool> arb_valid_{"arb_valid"};
    sc_core::sc_signal<unsigned> arb_selected_{"arb_selected"};
    sc_core::sc_signal<bool> arb_locked_{"arb_locked"};

    /// `false` is `SelAw`, matching the RTL enum's reset value.
    sc_core::sc_signal<bool> aw_w_sel_q_{"aw_w_sel_q"};
    sc_core::sc_signal<coordinate> w_dest_q_{"w_dest_q"};
};

/// Mirrors `fifo_v3` with `FALL_THROUGH = 1'b0`, which is what the
/// `MaxUniqueIds == 1` branch of `hw/floo_meta_buffer.sv` instantiates for its
/// request metadata. Plain state rather than a module: the meta buffer needs
/// raw push/pop/full/`data_o`, not a valid-ready wrapper.
template <typename T, unsigned Depth>
class meta_fifo {
public:
    void reset()
    {
        count_ = 0;
        read_ = 0;
        write_ = 0;
    }

    bool full() const { return count_ == Depth; }
    bool empty() const { return count_ == 0; }
    /// `data_o`, the head. Meaningful only while not empty.
    const T& head() const { return storage_[read_]; }
    unsigned occupancy() const { return count_; }

    void step(bool push, const T& data, bool pop)
    {
        if (pop && !empty()) {
            read_ = (read_ + 1) % Depth;
            --count_;
        }
        if (push && count_ < Depth) {
            storage_[write_] = data;
            write_ = (write_ + 1) % Depth;
            ++count_;
        }
    }

private:
    std::array<T, Depth> storage_{};
    unsigned count_{0};
    unsigned read_{0};
    unsigned write_{0};
};

/// SystemC mirror of the **subordinate side and response path** of
/// `hw/floo_axi_chimney.sv` in the frozen v0 configuration.
///
/// This is the other half of `axi_chimney_request`. An inbound request flit is
/// unpacked, its metadata retained, and the AXI transaction reissued on
/// `axi_out`; the subordinate's answer is matched back to that metadata, packed
/// into a B or R flit, and arbitrated onto the `rsp` link.
///
/// Two things here are *not* bypassed in the frozen configuration, unlike the
/// request path:
///
///   * `i_aw_out_queue`, a `spill_register` between the meta buffer and
///     `axi_out_req_o.aw`. It is unconditional — no `Cut*` parameter gates it.
///     The RTL comment gives the reason: AW and W share one link, so a
///     downstream module may refuse the AW until its W is valid.
///   * the metadata FIFOs, whose `full` back-pressures the inbound link.
///
/// **Response-arbiter index order.** `floo_rsp_arb_in` is declared
/// `[AxiB:AxiR]`, and `AxiB = 3`, `AxiR = 4`, so that packed range is
/// **ascending** exactly like the request side's `[AxiW:AxiAr]`. Index 0 is
/// therefore the **R** slot and index 1 the **B** slot. Getting this backwards
/// on the request side was a real defect found by the request-path
/// cross-check.
///
/// Scope: no ATOPs, `MaxUniqueIds = 1`, `EnSbrPort = 1`.
template <unsigned OutIdWidth, unsigned MaxTxns>
class axi_chimney_response : public sc_core::sc_module {
public:
    /// `no_atop_aw_req_id = '1`: every non-atomic downstream transaction is
    /// reissued with the same constant ID.
    static constexpr std::uint64_t downstream_id = (1ull << OutIdWidth) - 1ull;

    sc_core::sc_in<bool> i_clk{"i_clk"};
    sc_core::sc_in<bool> i_rst_n{"i_rst_n"};
    sc_core::sc_in<coordinate> i_node_id{"i_node_id"};

    // ---- inbound `req` link ---------------------------------------------
    sc_core::sc_in<axi_req_flit> i_req_data{"i_req_data"};
    sc_core::sc_in<bool> i_req_valid{"i_req_valid"};
    sc_core::sc_out<bool> o_req_ready{"o_req_ready"};

    // ---- reissued AXI towards the subordinate ---------------------------
    sc_core::sc_out<axi_aw_chan> o_axi_aw{"o_axi_aw"};
    sc_core::sc_out<bool> o_axi_aw_valid{"o_axi_aw_valid"};
    sc_core::sc_in<bool> i_axi_aw_ready{"i_axi_aw_ready"};
    sc_core::sc_out<axi_w_chan> o_axi_w{"o_axi_w"};
    sc_core::sc_out<bool> o_axi_w_valid{"o_axi_w_valid"};
    sc_core::sc_in<bool> i_axi_w_ready{"i_axi_w_ready"};
    sc_core::sc_out<axi_ar_chan> o_axi_ar{"o_axi_ar"};
    sc_core::sc_out<bool> o_axi_ar_valid{"o_axi_ar_valid"};
    sc_core::sc_in<bool> i_axi_ar_ready{"i_axi_ar_ready"};

    // ---- the subordinate's answers --------------------------------------
    sc_core::sc_in<axi_b_chan> i_axi_b{"i_axi_b"};
    sc_core::sc_in<bool> i_axi_b_valid{"i_axi_b_valid"};
    sc_core::sc_out<bool> o_axi_b_ready{"o_axi_b_ready"};
    sc_core::sc_in<axi_r_chan> i_axi_r{"i_axi_r"};
    sc_core::sc_in<bool> i_axi_r_valid{"i_axi_r_valid"};
    sc_core::sc_out<bool> o_axi_r_ready{"o_axi_r_ready"};

    // ---- outbound `rsp` link --------------------------------------------
    sc_core::sc_out<axi_rsp_flit> o_rsp_data{"o_rsp_data"};
    sc_core::sc_out<bool> o_rsp_valid{"o_rsp_valid"};
    sc_core::sc_in<bool> i_rsp_ready{"i_rsp_ready"};

    SC_HAS_PROCESS(axi_chimney_response);

    explicit axi_chimney_response(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
    {
        aw_meta_.reset();
        ar_meta_.reset();
        bind_aw_queue();
        bind_arbiter();

        SC_METHOD(comb);
        sensitive << i_node_id << i_req_data << i_req_valid << i_axi_aw_ready
                  << i_axi_w_ready << i_axi_ar_ready << i_axi_b << i_axi_b_valid
                  << i_axi_r << i_axi_r_valid << i_rsp_ready << aw_queue_ready_
                  << aw_queue_valid_ << aw_queue_data_ << arb_gnt_b_
                  << arb_gnt_r_ << arb_valid_ << arb_data_ << state_epoch_;

        SC_METHOD(seq);
        sensitive << i_clk.pos() << i_rst_n.neg();
        dont_initialize();
    }

    unsigned aw_meta_occupancy() const { return aw_meta_.occupancy(); }
    unsigned ar_meta_occupancy() const { return ar_meta_.occupancy(); }
    unsigned arb_valid_q() const { return arbiter_.valid_q(); }
    bool arb_last_q() const { return arbiter_.last_q(); }
    unsigned arb_rr_q() const { return arbiter_.rr_q(); }
    bool arb_lock_q() const { return arbiter_.lock_q(); }
    unsigned arb_req_q() const { return arbiter_.req_q(); }

private:
    /// `meta_buf_t` in the RTL: the original AXI id plus the request header.
    struct meta_entry {
        std::uint64_t id{};
        flit_header hdr{};
    };

    void bind_aw_queue()
    {
        aw_queue_.i_clk(i_clk);
        aw_queue_.i_rst_n(i_rst_n);
        aw_queue_.i_data(aw_queue_in_);
        aw_queue_.i_valid(aw_queue_valid_in_);
        aw_queue_.o_ready(aw_queue_ready_);
        aw_queue_.o_data(aw_queue_data_);
        aw_queue_.o_valid(aw_queue_valid_);
        aw_queue_.i_ready(i_axi_aw_ready);
        aw_queue_.o_occupancy(aw_queue_occupancy_);
    }

    void bind_arbiter()
    {
        arbiter_.i_clk(i_clk);
        arbiter_.i_rst_n(i_rst_n);
        // `[AxiB:AxiR]` is ascending, so index 0 is R and index 1 is B.
        arbiter_.i_data[0](arb_data_r_);
        arbiter_.i_valid[0](arb_req_r_);
        arbiter_.o_ready[0](arb_gnt_r_);
        arbiter_.i_data[1](arb_data_b_);
        arbiter_.i_valid[1](arb_req_b_);
        arbiter_.o_ready[1](arb_gnt_b_);
        arbiter_.o_data(arb_data_);
        arbiter_.o_valid(arb_valid_);
        // `i_rsp_out_cut` has `Bypass = !CutOup = 1`.
        arbiter_.i_ready(i_rsp_ready);
        arbiter_.o_selected(arb_selected_);
        arbiter_.o_locked(arb_locked_);
    }

    void comb()
    {
        const auto flit = i_req_data.read();
        const auto channel =
            static_cast<axi_channel>(flit.hdr.axi_ch.to_uint());
        const bool valid = i_req_valid.read();

        // Flit unpacker.
        const bool aw_in = valid && channel == axi_channel::aw;
        const bool w_in = valid && channel == axi_channel::w;
        const bool ar_in = valid && channel == axi_channel::ar;

        // Meta buffer. `meta_buf_rsp_in = axi_out_rsp_i` except that its
        // `aw_ready` is the spill register's, not the subordinate's.
        const bool aw_full = aw_meta_.full();
        const bool ar_full = ar_meta_.full();
        const bool meta_aw_ready = aw_queue_ready_.read();

        const bool aw_out_valid = aw_in && !aw_full;
        const bool aw_ready_out = meta_aw_ready && !aw_full;
        const bool ar_out_valid = ar_in && !ar_full;
        const bool ar_ready_out = i_axi_ar_ready.read() && !ar_full;

        // `floo_req_out_ready = axi_ready_out[hdr.axi_ch]`: the inbound link's
        // ready is selected by the channel the arriving flit names.
        bool req_ready = false;
        switch (channel) {
        case axi_channel::aw: req_ready = aw_ready_out; break;
        case axi_channel::w:  req_ready = i_axi_w_ready.read(); break;
        case axi_channel::ar: req_ready = ar_ready_out; break;
        default: req_ready = false; break;
        }
        o_req_ready.write(req_ready);

        // The AW spill register sits between the meta buffer and `axi_out`.
        axi_aw_chan aw_out = flit.aw;
        aw_out.id = downstream_id;
        aw_queue_in_.write(aw_out);
        aw_queue_valid_in_.write(aw_out_valid);

        o_axi_aw.write(aw_queue_data_.read());
        o_axi_aw_valid.write(aw_queue_valid_.read());
        o_axi_w.write(flit.w);
        o_axi_w_valid.write(w_in);
        axi_ar_chan ar_out = flit.ar;
        ar_out.id = downstream_id;
        o_axi_ar.write(ar_out);
        o_axi_ar_valid.write(ar_out_valid);

        // Responses: the retained metadata restores the manager's AXI id and
        // says where to route the flit back to.
        const auto& b_meta = aw_meta_.head();
        const auto& r_meta = ar_meta_.head();

        axi_b_chan b_payload = i_axi_b.read();
        b_payload.id = b_meta.id;
        axi_rsp_flit b_flit{};
        b_flit.hdr.rob_req = b_meta.hdr.rob_req;
        b_flit.hdr.rob_idx = b_meta.hdr.rob_idx;
        b_flit.hdr.dst_id = b_meta.hdr.src_id;
        b_flit.hdr.src_id = i_node_id.read();
        b_flit.hdr.last = true;
        b_flit.hdr.atop = b_meta.hdr.atop;
        b_flit.hdr.axi_ch = static_cast<unsigned>(axi_channel::b);
        b_flit.b = b_payload;

        axi_r_chan r_payload = i_axi_r.read();
        r_payload.id = r_meta.id;
        axi_rsp_flit r_flit{};
        r_flit.hdr.rob_req = r_meta.hdr.rob_req;
        r_flit.hdr.rob_idx = r_meta.hdr.rob_idx;
        r_flit.hdr.dst_id = r_meta.hdr.src_id;
        r_flit.hdr.src_id = i_node_id.read();
        r_flit.hdr.last = true;
        r_flit.hdr.atop = r_meta.hdr.atop;
        r_flit.hdr.axi_ch = static_cast<unsigned>(axi_channel::r);
        r_flit.r = r_payload;

        arb_data_b_.write(b_flit);
        arb_data_r_.write(r_flit);
        arb_req_b_.write(i_axi_b_valid.read());
        arb_req_r_.write(i_axi_r_valid.read());

        // `meta_buf_req_in.b_ready = floo_rsp_arb_gnt_out[AxiB]`
        o_axi_b_ready.write(arb_gnt_b_.read());
        o_axi_r_ready.write(arb_gnt_r_.read());

        o_rsp_valid.write(arb_valid_.read());
        o_rsp_data.write(arb_data_.read());
    }

    void seq()
    {
        if (!i_rst_n.read()) {
            aw_meta_.reset();
            ar_meta_.reset();
        } else {
            const auto flit = i_req_data.read();
            const auto channel =
                static_cast<axi_channel>(flit.hdr.axi_ch.to_uint());
            const bool valid = i_req_valid.read();

            meta_entry entry{};
            entry.hdr = flit.hdr;

            // `aw_no_atop_push = axi_req_o.aw_valid && axi_rsp_i.aw_ready`,
            // where `axi_rsp_i.aw_ready` at the meta buffer is the spill
            // register's ready, not the subordinate's.
            const bool aw_in = valid && channel == axi_channel::aw;
            const bool ar_in = valid && channel == axi_channel::ar;
            const bool aw_push =
                aw_in && !aw_meta_.full() && aw_queue_ready_.read();
            const bool ar_push =
                ar_in && !ar_meta_.full() && i_axi_ar_ready.read();

            // `aw_no_atop_pop = axi_rsp_o.b_valid && axi_req_i.b_ready`
            const bool aw_pop = i_axi_b_valid.read() && arb_gnt_b_.read();
            // `ar_no_atop_pop` additionally requires the response's `last`.
            const bool ar_pop = i_axi_r_valid.read() && arb_gnt_r_.read()
                && i_axi_r.read().last;

            meta_entry aw_entry = entry;
            aw_entry.id = flit.aw.id;
            aw_meta_.step(aw_push, aw_entry, aw_pop);

            meta_entry ar_entry = entry;
            ar_entry.id = flit.ar.id;
            ar_meta_.step(ar_push, ar_entry, ar_pop);
        }
        state_epoch_.write(state_epoch_.read() + 1);
    }

    meta_fifo<meta_entry, MaxTxns> aw_meta_{};
    meta_fifo<meta_entry, MaxTxns> ar_meta_{};

    spill_register<axi_aw_chan> aw_queue_{"aw_out_queue"};
    wormhole_arbiter<axi_rsp_flit, 2> arbiter_{"rsp_arbiter"};

    sc_core::sc_signal<axi_aw_chan> aw_queue_in_{"aw_queue_in"};
    sc_core::sc_signal<bool> aw_queue_valid_in_{"aw_queue_valid_in"};
    sc_core::sc_signal<bool> aw_queue_ready_{"aw_queue_ready"};
    sc_core::sc_signal<axi_aw_chan> aw_queue_data_{"aw_queue_data"};
    sc_core::sc_signal<bool> aw_queue_valid_{"aw_queue_valid"};
    sc_core::sc_signal<unsigned> aw_queue_occupancy_{"aw_queue_occupancy"};

    sc_core::sc_signal<axi_rsp_flit> arb_data_b_{"arb_data_b"};
    sc_core::sc_signal<axi_rsp_flit> arb_data_r_{"arb_data_r"};
    sc_core::sc_signal<bool> arb_req_b_{"arb_req_b"};
    sc_core::sc_signal<bool> arb_req_r_{"arb_req_r"};
    sc_core::sc_signal<bool> arb_gnt_b_{"arb_gnt_b"};
    sc_core::sc_signal<bool> arb_gnt_r_{"arb_gnt_r"};
    sc_core::sc_signal<axi_rsp_flit> arb_data_{"arb_data"};
    sc_core::sc_signal<bool> arb_valid_{"arb_valid"};
    sc_core::sc_signal<unsigned> arb_selected_{"arb_selected"};
    sc_core::sc_signal<bool> arb_locked_{"arb_locked"};

    sc_core::sc_signal<unsigned> state_epoch_{"state_epoch", 0};
};

/// Manager-side response unpacker: the fourth quadrant of the chimney.
///
/// `axi_chimney_request` covers manager AXI to the `req` link, and
/// `axi_chimney_response` covers the whole subordinate side. This closes the
/// remaining path: a B or R flit arriving on the `rsp` link, back out to the
/// AXI manager, releasing the reorder-buffer counter on the way.
///
/// It is purely combinational, and that is not a simplification. In the frozen
/// `NoRoB` branch of `hw/floo_rob_wrapper.sv` the response side is a literal
/// pass-through:
///
/// ```systemverilog
/// assign rsp_ready_o = rsp_ready_i;
/// assign rsp_valid_o = rsp_valid_i;
/// assign rsp_o       = rsp_i;
/// assign pop         = rsp_valid_i && rsp_last_i;
/// // pop_axi_id_i = rsp_i.id, pop_i = pop && rsp_ready_i
/// ```
///
/// so every register on this path lives in the counter bank, which belongs to
/// `axi_chimney_request`. This module only decodes the channel and routes the
/// handshake, mirroring `floo_axi_chimney.sv`:
///
/// ```systemverilog
/// assign axi_valid_in[AxiB]  = EnMgrPort && floo_rsp_in_valid &&
///                              (unpack_rsp_generic.hdr.axi_ch == AxiB);
/// assign axi_ready_out[AxiB] = b_rob_ready_out;          // no ATOP in v0
/// assign floo_rsp_out_ready  = axi_ready_out[unpack_rsp_generic.hdr.axi_ch];
/// assign b_rob_valid_in      = axi_valid_in[AxiB];       // !is_atop_b_rsp
/// ```
///
/// The manager's own `b_ready`/`r_ready` do **not** pass through here. They go
/// straight to `axi_chimney_request`'s `i_b_rsp_ready`/`i_r_rsp_ready`, because
/// the RTL wires `b_rob_ready_in = axi_req_in.b_ready` directly to the reorder
/// buffer.
///
/// **The response ID is not restored here.** By the time a flit reaches the
/// manager it already carries the original AXI ID: the *subordinate's* chimney
/// restored it from its retained metadata when it packed the B or R. See
/// `pack_b`/`pack_r`.
///
/// Scope: `EnMgrPort = 1`, no ATOPs, `MaxUniqueIds = 1`. With ATOPs the RTL
/// adds the `b_sel_atop`/`r_sel_atop` bypass around the reorder buffer, which
/// is deliberately absent here.
///
/// **Verification status: RTL cross-checked — Step A-1, 78 cycles exact.**
/// `rtl_crosscheck/run_chimney_mgr_rsp_crosscheck.sh` drives `floo_rsp_i` on
/// the unmodified frozen `floo_axi_chimney.sv` and compares every cycle of
/// `axi_in_rsp_o`, `floo_rsp_o.ready` and both per-id reorder-buffer counters
/// against this module composed with `axi_chimney_request`.
///
/// It is the twelfth cross-check and closes the chimney: all four quadrants are
/// now signed. It exists because the other three hold this link idle —
/// `tb_floo_axi_chimney_rsp_timing_trace.sv` pins `floo_rsp_in.valid = 1'b0` —
/// which is how the `RLAST` defect below survived three of them.
class axi_chimney_manager_response : public sc_core::sc_module {
public:
    // ---- inbound `rsp` link ---------------------------------------------
    sc_core::sc_in<axi_rsp_flit> i_rsp_data{"i_rsp_data"};
    sc_core::sc_in<bool> i_rsp_valid{"i_rsp_valid"};
    sc_core::sc_out<bool> o_rsp_ready{"o_rsp_ready"};

    // ---- reorder-buffer readiness, from `axi_chimney_request` ------------
    sc_core::sc_in<bool> i_b_rob_ready{"i_b_rob_ready"};
    sc_core::sc_in<bool> i_r_rob_ready{"i_r_rob_ready"};

    // ---- counter release, back into `axi_chimney_request` ----------------
    sc_core::sc_out<bool> o_b_pop{"o_b_pop"};
    sc_core::sc_out<unsigned> o_b_pop_id{"o_b_pop_id"};
    sc_core::sc_out<bool> o_r_pop{"o_r_pop"};
    sc_core::sc_out<unsigned> o_r_pop_id{"o_r_pop_id"};
    /// `RLAST`, which decides whether this beat releases the counter. Kept
    /// separate from `o_r_pop` rather than folded into it, because the RTL
    /// keeps `rsp_valid_i` raw and qualifies the pop inside the reorder buffer
    /// with `rsp_last_i`. Gating the valid instead would also suppress the
    /// buffer's `rsp_valid_o` pass-through on non-final beats.
    sc_core::sc_out<bool> o_r_pop_last{"o_r_pop_last"};

    // ---- AXI manager port, response side ---------------------------------
    sc_core::sc_out<axi_b_chan> o_axi_b{"o_axi_b"};
    sc_core::sc_out<bool> o_axi_b_valid{"o_axi_b_valid"};
    sc_core::sc_out<axi_r_chan> o_axi_r{"o_axi_r"};
    sc_core::sc_out<bool> o_axi_r_valid{"o_axi_r_valid"};

    SC_HAS_PROCESS(axi_chimney_manager_response);

    explicit axi_chimney_manager_response(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
    {
        SC_METHOD(comb);
        sensitive << i_rsp_data << i_rsp_valid << i_b_rob_ready
                  << i_r_rob_ready;
    }

private:
    void comb()
    {
        const axi_rsp_flit flit = i_rsp_data.read();
        const bool valid = i_rsp_valid.read();
        const auto channel =
            static_cast<axi_channel>(flit.hdr.axi_ch.to_uint());

        const bool is_b = valid && channel == axi_channel::b;
        const bool is_r = valid && channel == axi_channel::r;

        o_axi_b.write(flit.b);
        o_axi_b_valid.write(is_b);
        o_axi_r.write(flit.r);
        o_axi_r_valid.write(is_r);

        // `pop_axi_id_i = rsp_i.id`: the counter is released by the id the
        // response carries, which is the manager's own id. The gate applies the
        // `&& rsp_ready_i` handshake itself, so this is the raw valid.
        o_b_pop.write(is_b);
        o_b_pop_id.write(static_cast<unsigned>(flit.b.id));
        o_r_pop.write(is_r);
        o_r_pop_id.write(static_cast<unsigned>(flit.r.id));
        // A burst releases its counter once, on `RLAST`.
        o_r_pop_last.write(flit.r.last);

        // `floo_rsp_out_ready = axi_ready_out[hdr.axi_ch]`. A flit naming any
        // other channel is not something the `rsp` link carries, so it is
        // refused rather than silently accepted.
        bool ready = false;
        if (channel == axi_channel::b) {
            ready = i_b_rob_ready.read();
        } else if (channel == axi_channel::r) {
            ready = i_r_rob_ready.read();
        }
        o_rsp_ready.write(ready);
    }
};

} // namespace floo::model
