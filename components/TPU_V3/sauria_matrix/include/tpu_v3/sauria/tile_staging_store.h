// SPDX-License-Identifier: Apache-2.0
//
// The tile staging store: what replaces Sauria's `Sram` under decision record
// D17.
//
// ## What it is, and why it replaces rather than wraps
//
// The kept Sauria modules do not fetch data; they talk to a memory over
// signal-level buses with a fixed read latency, and they have no way to be told
// to wait. D17 records the consequence: operands are **staged** — prefetched
// from core SRAM into private storage over `neo_local_sram_if`, handed to the
// array at the source's own timing, and written back the same way. Pass-through
// is unsafe because a late answer from an arbitrated bank would be latched as
// data (see the record for the full argument).
//
// So this module presents exactly the ports Sauria's `Sram` presented to the
// feeders and the PSM, and nothing else. It is not a wrapper around `Sram`: the
// prefetch and writeback controllers fill and drain it directly, and `Sram`'s
// host port, power gating, deep sleep and bank-select machinery are gone
// because a TPU_V3 core reaches this storage through the native fabric and
// through nothing else.
//
// ## The timing contract, matched exactly
//
// `Sram::beh_process` is an `SC_METHOD` on `i_clk.pos()` that, when `rden` is
// asserted, writes the bank's contents to an output `sc_signal`. Because that
// is a signal write, the consumer observes it on the **following** clock — a
// one-cycle registered read.
//
// That single cycle is the entire reason this class exists rather than a
// `std::vector`. The feeder recovers data through its own `rden_q1`/`rden_q2`
// shift register, which assumes the memory answers on that exact schedule; a
// store that answered combinationally, or a cycle late, would shift the whole
// pipeline and produce wrong results that look like arithmetic defects.
//
// Address wrapping is also copied deliberately: `Sram` reduces every address
// modulo the bank capacity, so an out-of-range access aliases rather than
// faults. That is not a design this project would choose, but the feeders'
// address generators were validated against it, and a store that faulted where
// the original aliased would diverge from the source it is supposed to match.
// The controllers bounds-check *their own* addresses against the core-SRAM
// window, which is where a real error belongs.

#pragma once

#include <array>
#include <cstdint>
#include <stdexcept>
#include <vector>

#include <systemc>

#include "tpu_v3/sauria/sauria_geometry.h"

#include "sauria_types.h"

namespace cdc::components::tpu_v3::sauria {

/// Private operand and result storage for one matrix engine.
///
/// Template parameters mirror the source's so the vector types on the ports are
/// the ones the feeders and the PSM expect. They are supplied from the extracted
/// profile by the adapter, never defaulted — `sauria_geometry.h` explains why a
/// default here would be a defect rather than a convenience.
template <int X_DIM, int Y_DIM, typename T_ACT, typename T_WEI, typename T_PSUM>
class tile_staging_store : public sc_core::sc_module {
public:
    static_assert(X_DIM > 0 && Y_DIM > 0,
                  "tile staging geometry must be non-zero");
    using act_vector = ::sauria::act_vector_t<Y_DIM, T_ACT>;
    using wei_vector = ::sauria::wei_vector_t<X_DIM, T_WEI>;
    using psum_vector = ::sauria::psum_vector_t<Y_DIM, T_PSUM>;
    using psum_mask = ::sauria::sramc_mask_t<Y_DIM>;

    sc_core::sc_in<bool> i_clk{"i_clk"};
    sc_core::sc_in<bool> i_rstn{"i_rstn"};

    // ── SRAM-A, activations, one port per lane ───────────────────────────────
    sc_core::sc_in<std::uint32_t> i_srama_addr_a{"i_srama_addr_a"};
    sc_core::sc_in<bool> i_srama_rden_a{"i_srama_rden_a"};
    sc_core::sc_out<act_vector> o_srama_data_a{"o_srama_data_a"};

    sc_core::sc_in<std::uint32_t> i_srama_addr_b{"i_srama_addr_b"};
    sc_core::sc_in<bool> i_srama_rden_b{"i_srama_rden_b"};
    sc_core::sc_out<act_vector> o_srama_data_b{"o_srama_data_b"};

    // ── SRAM-B, weights ──────────────────────────────────────────────────────
    sc_core::sc_in<std::uint32_t> i_sramb_addr_a{"i_sramb_addr_a"};
    sc_core::sc_in<bool> i_sramb_rden_a{"i_sramb_rden_a"};
    sc_core::sc_out<wei_vector> o_sramb_data_a{"o_sramb_data_a"};

    sc_core::sc_in<std::uint32_t> i_sramb_addr_b{"i_sramb_addr_b"};
    sc_core::sc_in<bool> i_sramb_rden_b{"i_sramb_rden_b"};
    sc_core::sc_out<wei_vector> o_sramb_data_b{"o_sramb_data_b"};

    // ── SRAM-C, partial sums and results. Read *and* written by the PSM ──────
    sc_core::sc_in<std::uint32_t> i_sramc_addr_a{"i_sramc_addr_a"};
    sc_core::sc_in<bool> i_sramc_rden_a{"i_sramc_rden_a"};
    sc_core::sc_in<bool> i_sramc_wren_a{"i_sramc_wren_a"};
    sc_core::sc_in<psum_vector> i_sramc_wdata_a{"i_sramc_wdata_a"};
    sc_core::sc_in<psum_mask> i_sramc_wmask_a{"i_sramc_wmask_a"};
    sc_core::sc_out<psum_vector> o_sramc_rdata_a{"o_sramc_rdata_a"};

    sc_core::sc_in<std::uint32_t> i_sramc_addr_b{"i_sramc_addr_b"};
    sc_core::sc_in<bool> i_sramc_rden_b{"i_sramc_rden_b"};
    sc_core::sc_in<bool> i_sramc_wren_b{"i_sramc_wren_b"};
    sc_core::sc_in<psum_vector> i_sramc_wdata_b{"i_sramc_wdata_b"};
    sc_core::sc_in<psum_mask> i_sramc_wmask_b{"i_sramc_wmask_b"};
    sc_core::sc_out<psum_vector> o_sramc_rdata_b{"o_sramc_rdata_b"};

    SC_HAS_PROCESS(tile_staging_store);

    /// Capacities are in *vectors*, not bytes: one entry is one array row's
    /// worth of operands, which is how the feeders address it.
    tile_staging_store(sc_core::sc_module_name name, std::size_t a_vectors,
                       std::size_t b_vectors, std::size_t c_vectors)
        : sc_core::sc_module(name)
        , activations_{std::vector<act_vector>(a_vectors),
                       std::vector<act_vector>(a_vectors)}
        , weights_{std::vector<wei_vector>(b_vectors),
                   std::vector<wei_vector>(b_vectors)}
        , results_{std::vector<psum_vector>(c_vectors),
                   std::vector<psum_vector>(c_vectors)}
    {
        if (a_vectors == 0 || b_vectors == 0 || c_vectors == 0) {
            throw std::invalid_argument(
                "tile_staging_store capacities must all be non-zero");
        }
        SC_METHOD(clocked);
        sensitive << i_clk.pos();
    }

    // ── the controller side ──────────────────────────────────────────────────
    //
    // Plain calls rather than a second set of ports. The prefetch and writeback
    // controllers are `SC_THREAD`s in the same adapter that owns this store, and
    // giving them a signal-level port would mean modelling a second memory
    // interface that no hardware has: in the real design these buffers are
    // filled by the same block that owns them.
    //
    // Bounds are checked here and reported, unlike the array-facing ports which
    // wrap. A controller reading past the end of a staged tile is a defect in
    // this repository's code; a feeder doing so is behaviour the source was
    // validated with.

    void store_activation(std::size_t lane, std::size_t index,
                          const act_vector& value)
    {
        activations_.at(lane).at(index) = value;
    }

    void store_weight(std::size_t lane, std::size_t index,
                      const wei_vector& value)
    {
        weights_.at(lane).at(index) = value;
    }

    psum_vector load_result(std::size_t lane, std::size_t index) const
    {
        return results_.at(lane).at(index);
    }

    /// Clear the result store between jobs.
    ///
    /// Explicit rather than automatic on reset: D17 says a job interrupted part
    /// way leaves C partially written and that the committed byte count is the
    /// only account of how far it got. A store that wiped itself on reset would
    /// destroy exactly that evidence.
    void clear_results()
    {
        for (auto& lane : results_) {
            for (auto& entry : lane) {
                entry = psum_vector();
            }
        }
    }

    std::size_t activation_capacity() const { return activations_[0].size(); }
    std::size_t weight_capacity() const { return weights_[0].size(); }
    std::size_t result_capacity() const { return results_[0].size(); }

private:
    void clocked()
    {
        if (!i_rstn.read()) {
            // Same reset behaviour as the source: the *outputs* return to zero,
            // the *contents* do not. A staged tile surviving reset is what lets
            // the adapter report how much of C was committed before it.
            o_srama_data_a.write(act_vector());
            o_srama_data_b.write(act_vector());
            o_sramb_data_a.write(wei_vector());
            o_sramb_data_b.write(wei_vector());
            o_sramc_rdata_a.write(psum_vector());
            o_sramc_rdata_b.write(psum_vector());
            return;
        }

        // One registered read per port, exactly as `Sram::beh_process` does it:
        // write the signal on this edge, the consumer sees it on the next.
        if (i_srama_rden_a.read()) {
            o_srama_data_a.write(activations_[0][wrap(i_srama_addr_a.read(),
                                                      activations_[0].size())]);
        }
        if (i_srama_rden_b.read()) {
            o_srama_data_b.write(activations_[1][wrap(i_srama_addr_b.read(),
                                                      activations_[1].size())]);
        }
        if (i_sramb_rden_a.read()) {
            o_sramb_data_a.write(
                weights_[0][wrap(i_sramb_addr_a.read(), weights_[0].size())]);
        }
        if (i_sramb_rden_b.read()) {
            o_sramb_data_b.write(
                weights_[1][wrap(i_sramb_addr_b.read(), weights_[1].size())]);
        }

        // SRAM-C is written by the PSM under a per-lane mask, and read back by
        // it for accumulation across contexts. Write before read on the same
        // edge would forward this cycle's data a cycle early; the source writes
        // the bank and reads the *old* contents into the output signal, so the
        // read is ordered first here.
        read_results(i_sramc_rden_a.read(), i_sramc_addr_a.read(), 0,
                     o_sramc_rdata_a);
        read_results(i_sramc_rden_b.read(), i_sramc_addr_b.read(), 1,
                     o_sramc_rdata_b);

        write_results(i_sramc_wren_a.read(), i_sramc_addr_a.read(), 0,
                      i_sramc_wdata_a.read(), i_sramc_wmask_a.read());
        write_results(i_sramc_wren_b.read(), i_sramc_addr_b.read(), 1,
                      i_sramc_wdata_b.read(), i_sramc_wmask_b.read());
    }

    void read_results(bool enable, std::uint32_t address, std::size_t lane,
                      sc_core::sc_out<psum_vector>& out)
    {
        if (enable) {
            out.write(results_[lane][wrap(address, results_[lane].size())]);
        }
    }

    void write_results(bool enable, std::uint32_t address, std::size_t lane,
                       const psum_vector& data, const psum_mask& mask)
    {
        if (!enable) {
            return;
        }
        auto& entry = results_[lane][wrap(address, results_[lane].size())];
        for (int i = 0; i < Y_DIM; ++i) {
            if (mask[i]) {
                entry[i] = data[i];
            }
        }
    }

    /// `Sram` reduces every accelerator-side address modulo the bank capacity.
    /// Reproduced rather than corrected: see the header comment.
    static std::size_t wrap(std::uint32_t address, std::size_t capacity)
    {
        return capacity == 0 ? 0 : static_cast<std::size_t>(address) % capacity;
    }

    std::array<std::vector<act_vector>, 2> activations_;
    std::array<std::vector<wei_vector>, 2> weights_;
    std::array<std::vector<psum_vector>, 2> results_;
};

} // namespace cdc::components::tpu_v3::sauria
