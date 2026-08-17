// SPDX-License-Identifier: Apache-2.0
//
// The tile staging store's timing contract.
//
// This component exists to replace Sauria's `Sram` (decision record D17), and
// the only thing that makes a replacement safe is that it answers on the same
// schedule. The feeders recover data through their own `rden_q1`/`rden_q2`
// shift register with no way to be told to wait, so a store that answered
// combinationally, or a cycle late, would shift the whole pipeline and produce
// results that look like arithmetic defects rather than timing ones.
//
// So the checks here are about *when*, not only *what*.

#define SC_INCLUDE_DYNAMIC_PROCESSES

#include <iostream>
#include <string>

#include <systemc>

#include "tpu_v3/sauria/tile_staging_store.h"

namespace sauria_tpu = cdc::components::tpu_v3::sauria;

using store_t =
    sauria_tpu::tile_staging_store<sauria_tpu::columns, sauria_tpu::rows,
                                   sauria_tpu::activation_t,
                                   sauria_tpu::weight_t,
                                   sauria_tpu::accumulator_t>;

namespace {

int failures = 0;

void check(bool condition, const std::string& what)
{
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << what << '\n';
    }
}

} // namespace

int sc_main(int, char*[])
{
    constexpr std::size_t kCapacity = 16;
    store_t store("store", kCapacity, kCapacity, kCapacity);

    sc_core::sc_clock clock("clock", 10, sc_core::SC_NS);
    sc_core::sc_signal<bool> rstn{"rstn"};
    sc_core::sc_signal<std::uint32_t> a_addr{"a_addr"}, b_addr{"b_addr"};
    sc_core::sc_signal<std::uint32_t> c_addr{"c_addr"};
    sc_core::sc_signal<std::uint32_t> a_addr_b{"a_addr_b"}, b_addr_b{"b_addr_b"};
    sc_core::sc_signal<std::uint32_t> c_addr_b{"c_addr_b"};
    sc_core::sc_signal<bool> a_rden{"a_rden"}, b_rden{"b_rden"};
    sc_core::sc_signal<bool> a_rden_b{"a_rden_b"}, b_rden_b{"b_rden_b"};
    sc_core::sc_signal<bool> c_rden{"c_rden"}, c_wren{"c_wren"};
    sc_core::sc_signal<bool> c_rden_b{"c_rden_b"}, c_wren_b{"c_wren_b"};
    sc_core::sc_signal<store_t::act_vector> a_data{"a_data"}, a_data_b{"a_data_b"};
    sc_core::sc_signal<store_t::wei_vector> b_data{"b_data"}, b_data_b{"b_data_b"};
    sc_core::sc_signal<store_t::psum_vector> c_wdata{"c_wdata"}, c_wdata_b{"c_wdata_b"};
    sc_core::sc_signal<store_t::psum_vector> c_rdata{"c_rdata"}, c_rdata_b{"c_rdata_b"};
    sc_core::sc_signal<store_t::psum_mask> c_wmask{"c_wmask"}, c_wmask_b{"c_wmask_b"};

    store.i_clk(clock);
    store.i_rstn(rstn);
    store.i_srama_addr_a(a_addr);   store.i_srama_rden_a(a_rden);   store.o_srama_data_a(a_data);
    store.i_srama_addr_b(a_addr_b); store.i_srama_rden_b(a_rden_b); store.o_srama_data_b(a_data_b);
    store.i_sramb_addr_a(b_addr);   store.i_sramb_rden_a(b_rden);   store.o_sramb_data_a(b_data);
    store.i_sramb_addr_b(b_addr_b); store.i_sramb_rden_b(b_rden_b); store.o_sramb_data_b(b_data_b);
    store.i_sramc_addr_a(c_addr);   store.i_sramc_rden_a(c_rden);   store.i_sramc_wren_a(c_wren);
    store.i_sramc_wdata_a(c_wdata); store.i_sramc_wmask_a(c_wmask); store.o_sramc_rdata_a(c_rdata);
    store.i_sramc_addr_b(c_addr_b); store.i_sramc_rden_b(c_rden_b); store.i_sramc_wren_b(c_wren_b);
    store.i_sramc_wdata_b(c_wdata_b); store.i_sramc_wmask_b(c_wmask_b); store.o_sramc_rdata_b(c_rdata_b);

    // Stage a recognisable pattern on both lanes before anything runs.
    for (std::size_t index = 0; index < kCapacity; ++index) {
        store_t::act_vector activations;
        store_t::wei_vector weights;
        for (int y = 0; y < sauria_tpu::rows; ++y) {
            activations[y] = static_cast<sauria_tpu::activation_t>(index * 2 + y);
        }
        for (int x = 0; x < sauria_tpu::columns; ++x) {
            weights[x] = static_cast<sauria_tpu::weight_t>(index * 3 + x);
        }
        store.store_activation(0, index, activations);
        store.store_activation(1, index, activations);
        store.store_weight(0, index, weights);
        store.store_weight(1, index, weights);
    }

    rstn.write(false);
    sc_core::sc_start(20, sc_core::SC_NS);
    check(a_data.read()[0] == 0, "reset must drive the read ports to zero");
    rstn.write(true);
    sc_core::sc_start(10, sc_core::SC_NS);

    // ── the one-cycle registered read ────────────────────────────────────────
    //
    // Assert `rden` with an address, then sample after exactly one clock. The
    // source writes its output signal on the posedge, so the consumer observes
    // it on the next one — no sooner, and no later.
    a_addr.write(5);
    a_rden.write(true);
    b_addr.write(7);
    b_rden.write(true);

    sc_core::sc_start(20, sc_core::SC_NS);
    check(a_data.read()[0] == static_cast<sauria_tpu::activation_t>(5 * 2 + 0),
          "SRAM-A did not answer after rden");
    check(a_data.read()[3] == static_cast<sauria_tpu::activation_t>(5 * 2 + 3),
          "SRAM-A returned the wrong lane element");
    check(b_data.read()[0] == static_cast<sauria_tpu::weight_t>(7 * 3 + 0),
          "SRAM-B did not answer one cycle after rden");

    // ── the read port is enable-gated and holds ──────────────────────────────
    //
    // Move the address while `rden` is low: the port must keep presenting what
    // it last latched, not follow the address. The feeders drive an address
    // continuously and raise `rden` only when they want that location, so a
    // port that tracked the address would hand them data for cycles they never
    // requested.
    //
    // This checks enable gating and hold — *not* the read's cycle latency.
    // Latency is not observable from `sc_main`: after `sc_start` crosses a
    // posedge the output signal has already settled, and stopping exactly on an
    // edge leaves it ambiguous whether that edge has been processed, so a check
    // written either side of one passes or fails on where the test happened to
    // leave simulated time rather than on the store's behaviour. The one-cycle
    // latency is established by construction instead — `clocked()` is an
    // `SC_METHOD` on `i_clk.pos()` writing an output `sc_signal`, exactly as
    // `Sram::beh_process` does — and it becomes observable for real once the
    // feeders are wired in and their `rden_q1`/`rden_q2` capture either lines
    // up or does not.
    a_rden.write(false);
    b_rden.write(false);
    const auto held = a_data.read()[0];
    a_addr.write(9);
    sc_core::sc_start(30, sc_core::SC_NS);
    check(a_data.read()[0] == held,
          "the read port followed the address while rden was low; it is not "
          "enable-gated, and the feeders would receive data for cycles they "
          "never requested");
    check(a_data.read()[0] != static_cast<sauria_tpu::activation_t>(9 * 2 + 0),
          "the read port presented the new address's contents although rden "
          "was low");

    // ── the masked SRAM-C write ──────────────────────────────────────────────
    //
    // The PSM writes results under a per-lane mask. A store that ignored the
    // mask would overwrite lanes the array had not produced yet.
    {
        store_t::psum_vector value;
        store_t::psum_mask mask;
        for (int y = 0; y < sauria_tpu::rows; ++y) {
            value[y] = static_cast<sauria_tpu::accumulator_t>(0x1000 + y);
            mask[y] = (y % 2 == 0);
        }
        c_addr.write(3);
        c_wdata.write(value);
        c_wmask.write(mask);
        c_wren.write(true);
        sc_core::sc_start(10, sc_core::SC_NS);
        c_wren.write(false);
        sc_core::sc_start(10, sc_core::SC_NS);

        const auto stored = store.load_result(0, 3);
        check(stored[0] == 0x1000, "a masked-in lane was not written");
        check(stored[2] == 0x1002, "a masked-in lane was not written");
        check(stored[1] == 0, "a masked-out lane was written anyway");
        check(stored[3] == 0, "a masked-out lane was written anyway");
    }

    // ── address wrapping, copied from the source deliberately ────────────────
    //
    // `Sram` reduces accelerator-side addresses modulo the bank capacity, so an
    // out-of-range read aliases rather than faults. The feeders' address
    // generators were validated against that, so the store must do the same;
    // the controllers bounds-check their own addresses instead, which is where
    // a real error belongs.
    a_addr.write(static_cast<std::uint32_t>(kCapacity + 5));
    a_rden.write(true);
    sc_core::sc_start(20, sc_core::SC_NS);
    check(a_data.read()[0] == static_cast<sauria_tpu::activation_t>(5 * 2 + 0),
          "an out-of-range address did not alias the way the source's SRAM does");

    // ── the two lanes are independent storage ────────────────────────────────
    //
    // They are separate banks in the source. A store that shared one buffer
    // would let lane B's prefetch corrupt lane A's operands, and the split-lane
    // execution the array supports would silently compute on the wrong tile.
    {
        store_t::act_vector marker;
        for (int y = 0; y < sauria_tpu::rows; ++y) {
            marker[y] = static_cast<sauria_tpu::activation_t>(0x40 + y);
        }
        store.store_activation(1, 5, marker);

        a_addr.write(5);
        a_rden.write(true);
        a_addr_b.write(5);
        a_rden_b.write(true);
        sc_core::sc_start(20, sc_core::SC_NS);

        check(a_data_b.read()[0] == static_cast<sauria_tpu::activation_t>(0x40),
              "lane B did not return its own contents");
        check(a_data.read()[0] == static_cast<sauria_tpu::activation_t>(5 * 2),
              "writing lane B changed lane A; the lanes share storage");
    }

    // ── results survive reset ────────────────────────────────────────────────
    //
    // D17: a job interrupted part way leaves C partially written, and the
    // committed byte count is the only account of how far it got. A store that
    // cleared itself on reset would destroy that evidence.
    rstn.write(false);
    sc_core::sc_start(20, sc_core::SC_NS);
    rstn.write(true);
    sc_core::sc_start(10, sc_core::SC_NS);
    check(store.load_result(0, 3)[0] == 0x1000,
          "reset cleared the result store; a partially written C region must "
          "survive so the committed count means something (D17)");

    store.clear_results();
    check(store.load_result(0, 3)[0] == 0,
          "clear_results() did not clear the result store");

    if (failures != 0) {
        std::cerr << failures << " checks failed\n";
        return 1;
    }
    std::cout << "tile staging store: timing and masking PASS\n";
    return 0;
}
