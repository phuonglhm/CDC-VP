// SPDX-License-Identifier: Apache-2.0
//
// Direct contract test for `rr_arb_tree.hpp`, the round-robin arbitration tree
// the wormhole arbiter is built on.
//
// Why this exists separately from the cross-check. The 152-cycle
// `run_wormhole_arbiter_crosscheck.sh` already compares this leaf against the
// frozen `rr_arb_tree` through `floo_wormhole_arbiter`, at 5, 4 and 2 routes.
// That is the hardware evidence and this test does not replace it. What it adds
// is isolation: when the cross-check fails, a direct leaf test says whether the
// arbitration arithmetic moved or whether the fault is in the wormhole lock,
// the FIFO or the composition around it. Rule 4 in `AI_HANDOFF_CONTEXT.md`
// section 16 asks for exactly that ordering, and this closes the last v0
// definition-of-done criterion.
//
// Expectations are read off the frozen RTL, not off this model:
//
//   assign req_d = (lock_q) ? req_q : req_i;
//   sel  = ~req_d[l*2] | req_d[l*2+1] & rr_q[NumLevels-1-level];
//   idx  = {sel, index_nodes[child]};
//
// and the `FairArb` next state, which walks to the next requester above `rr_q`
// and wraps through the lower mask when the upper one is empty.

#include "floo_noc_model/rr_arb_tree.hpp"

#include <iostream>
#include <string>

// `rr_arb_tree.hpp` is pure C++ and pulls in no SystemC. The entry point below
// is still `sc_main`, so the declaration has to come from somewhere or the
// definition will not match the one `libsystemc` calls.
#include <systemc>

namespace {

using namespace floo::model;

int failures = 0;

void check(bool condition, const std::string& what)
{
    if (!condition) {
        std::cerr << "FAIL: " << what << '\n';
        ++failures;
    }
}

/// A degenerate tree still has to answer. `NumIn == 1` takes its own branch in
/// the header, so it is the one shape whose result comes from no tree at all.
void test_single_input()
{
    using arb = rr_arb_tree<1>;

    const auto idle = arb::evaluate(/*req_i=*/0, false, 0, 0);
    check(!idle.req_o, "NumIn=1: no request must not grant");
    check(idle.idx_o == 0, "NumIn=1: idle index must be 0");

    const auto busy = arb::evaluate(/*req_i=*/1, false, 0, 0);
    check(busy.req_o, "NumIn=1: a request must grant");
    check(busy.idx_o == 0, "NumIn=1: the only input is index 0");

    // `rr_q` cannot change the answer when there is nothing to rotate between.
    for (unsigned rr = 0; rr < 4; ++rr) {
        const auto out = arb::evaluate(1, false, 0, rr);
        check(out.req_o && out.idx_o == 0,
              "NumIn=1: rr_q must not affect the only input");
    }
}

/// Power-of-two width: every level of the tree is full.
void test_power_of_two_priority()
{
    using arb = rr_arb_tree<4>;

    check(!arb::evaluate(0b0000, false, 0, 0).req_o,
          "no request must not grant");

    // A lone requester wins regardless of where `rr_q` points.
    for (unsigned bit = 0; bit < 4; ++bit) {
        for (unsigned rr = 0; rr < 4; ++rr) {
            const auto out = arb::evaluate(1u << bit, false, 0, rr);
            check(out.req_o, "a lone requester must be granted");
            check(out.idx_o == bit,
                  "a lone requester must be granted its own index");
        }
    }

    // Competing requests: `rr_q` selects which side of each node wins. With
    // every input asking, the granted index is `rr_q` itself — the tree walks
    // the pointer bit by bit from the root down.
    for (unsigned rr = 0; rr < 4; ++rr) {
        const auto out = arb::evaluate(0b1111, false, 0, rr);
        check(out.req_o, "a full request mask must grant");
        check(out.idx_o == rr,
              "with every input asking, rr_q selects the winner");
    }

    // Two requesters on opposite halves: the top `rr_q` bit decides.
    check(arb::evaluate(0b0101, false, 0, 0b00).idx_o == 0,
          "rr_q pointing low must pick the low half");
    check(arb::evaluate(0b0101, false, 0, 0b10).idx_o == 2,
          "rr_q pointing high must pick the high half");
}

/// Non-power-of-two width is the shape the router actually uses: five ports,
/// so the last leaf node is a single input and the remaining nodes are unused.
void test_non_power_of_two()
{
    using arb = rr_arb_tree<5>;

    check(!arb::evaluate(0, false, 0, 0).req_o,
          "5 routes: no request must not grant");

    for (unsigned bit = 0; bit < 5; ++bit) {
        const auto out = arb::evaluate(1u << bit, false, 0, 0);
        check(out.req_o && out.idx_o == bit,
              "5 routes: a lone requester must win its own index");
    }

    // Input 4 is the odd one out: it sits alone under the last leaf node, so
    // reaching it exercises the `l * 2 == NumIn - 1` branch rather than the
    // paired one.
    const auto lone_top = arb::evaluate(1u << 4, false, 0, 0);
    check(lone_top.req_o && lone_top.idx_o == 4,
          "5 routes: the unpaired top input must still be reachable");

    // Every input asking: the winner must be a real input, and must be one
    // that actually asked. This is the invariant a mis-sized tree breaks.
    for (unsigned rr = 0; rr < 8; ++rr) {
        const auto out = arb::evaluate(0b11111, false, 0, rr);
        check(out.req_o, "5 routes: a full mask must grant");
        check(out.idx_o < 5, "5 routes: the index must be in range");
    }
}

/// `req_d = lock_q ? req_q : req_i`. While locked, the live request lines are
/// ignored entirely — this is what holds a wormhole packet on one output.
void test_lock_selects_the_registered_request()
{
    using arb = rr_arb_tree<4>;

    // Unlocked: the live mask decides.
    const auto live = arb::evaluate(/*req_i=*/0b0010, false, /*req_q=*/0b1000, 0);
    check(live.req_d == 0b0010, "unlocked must take req_i");
    check(live.idx_o == 1, "unlocked must grant from req_i");

    // Locked: the registered mask decides and `req_i` is not consulted.
    const auto locked = arb::evaluate(/*req_i=*/0b0010, true, /*req_q=*/0b1000, 0);
    check(locked.req_d == 0b1000, "locked must take req_q");
    check(locked.idx_o == 3, "locked must grant from req_q");

    // Locked onto nothing grants nothing, even with a live request pending.
    const auto locked_idle = arb::evaluate(0b1111, true, 0, 0);
    check(!locked_idle.req_o,
          "locked onto an empty mask must not grant a live request");
}

/// Bits above `NumIn` are not inputs. A caller passing a wider mask must not
/// change a decision the real inputs already determine.
void test_bits_above_num_in_are_ignored()
{
    using arb = rr_arb_tree<4>;

    for (unsigned rr = 0; rr < 4; ++rr) {
        const auto clean = arb::evaluate(0b0100, false, 0, rr);
        const auto dirty = arb::evaluate(0b1111'0100, false, 0, rr);
        check(clean.req_o == dirty.req_o && clean.idx_o == dirty.idx_o,
              "request bits above NumIn must not change the decision");
    }

    // The same for the next-state pointer.
    check(arb::next_rr(0b0100, 0) == arb::next_rr(0b1111'0100, 0),
          "request bits above NumIn must not change next_rr");
}

/// `next_rr` walks to the next requester strictly above `rr_q`, and wraps
/// through the lower mask when nothing above is asking. The wrap is the part a
/// naive "search upward" implementation gets wrong.
void test_next_rr_walk_and_wrap()
{
    using arb = rr_arb_tree<4>;

    // Strictly above: from 0, the next requester above is 2.
    check(arb::next_rr(0b0100, 0) == 2, "next_rr must move above rr_q");

    // `rr_q` itself is *not* above `rr_q`, so a requester sitting exactly on
    // the pointer belongs to the lower mask and is only chosen by wrapping.
    check(arb::next_rr(0b0100, 2) == 2,
          "a requester on the pointer is reached by wrapping, not by moving up");

    // Wrap: pointer at the top, the only requester is at the bottom.
    check(arb::next_rr(0b0001, 3) == 0,
          "next_rr must wrap to the lower mask when nothing is above");

    // Upper wins over lower when both are populated.
    check(arb::next_rr(0b1001, 1) == 3,
          "next_rr must prefer the upper mask over the lower one");

    // And the lowest of the upper mask, not the highest.
    check(arb::next_rr(0b1100, 0) == 2,
          "next_rr must take the lowest requester above rr_q");

    // Wrap picks the lowest of the lower mask, not the closest below.
    check(arb::next_rr(0b0011, 3) == 0,
          "wrapping must take the lowest requester, not the nearest below");
}

/// Five routes is the router's real width, and the wrap has to respect it: a
/// pointer at input 4 must come back to 0, not to some index the tree cannot
/// produce.
void test_next_rr_at_router_width()
{
    using arb = rr_arb_tree<5>;

    check(arb::next_rr(0b00001, 4) == 0,
          "5 routes: wrapping from the top input must reach input 0");
    check(arb::next_rr(0b10000, 0) == 4,
          "5 routes: the top input must be reachable by moving up");
    check(arb::next_rr(0b11111, 4) == 0,
          "5 routes: a full mask at the top pointer must wrap to 0");

    for (unsigned rr = 0; rr < 5; ++rr) {
        check(arb::next_rr(0b11111, rr) < 5,
              "5 routes: next_rr must stay inside the input range");
    }
}

} // namespace

// No kernel runs here: the arbitration tree is stateless and pure. `sc_main`
// rather than `main` because SystemC owns the real entry point, matching every
// other non-kernel test in this directory.
int sc_main(int, char**)
{
    test_single_input();
    test_power_of_two_priority();
    test_non_power_of_two();
    test_lock_selects_the_registered_request();
    test_bits_above_num_in_are_ignored();
    test_next_rr_walk_and_wrap();
    test_next_rr_at_router_width();

    if (failures != 0) {
        std::cerr << failures << " rr_arb_tree checks failed\n";
        return 1;
    }
    std::cout << "test_rr_arb_tree: all checks passed\n";
    return 0;
}
