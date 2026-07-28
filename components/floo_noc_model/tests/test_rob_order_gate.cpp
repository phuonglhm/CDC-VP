// SPDX-License-Identifier: Apache-2.0
//
// Contract test for the `NoRoB` response-ordering rule.
//
// Expectations are read out of the `NoRoB` branch of
// `hw/floo_rob_wrapper.sv`:
//
//   push = ax_valid_i && (!in_flight || ax_dest_i == prev_dest) && !counter_full
//
// This is a contract test against the RTL text. The rule is an admission
// decision, so a cycle cross-check of it needs a harness that observes the
// request-side `ready`, which the current content-only chimney harnesses do
// not.

#include "floo_noc_model/rob_order_gate.hpp"

#include <iostream>
#include <string>

namespace {

using namespace floo::model;

int failures = 0;

void check(bool condition, const std::string& message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

} // namespace

int sc_main(int, char**)
{
    const coordinate east{3, 1};
    const coordinate north{1, 3};

    no_rob_order_gate gate{4};

    // Nothing in flight: any destination is admissible.
    check(gate.may_issue(5, east), "an idle ID must admit any destination");
    gate.issue(5, east);
    check(gate.outstanding(5) == 1, "issuing must count one outstanding");

    // Same ID, same destination: not serialised.
    check(gate.may_issue(5, east),
          "reusing an ID for the same destination must not stall");
    gate.issue(5, east);
    check(gate.outstanding(5) == 2, "a second issue must count");

    // Same ID, different destination: blocked until the ID drains.
    check(!gate.may_issue(5, north),
          "reusing an ID for a different destination must stall");

    // A different ID is unaffected.
    check(gate.may_issue(2, north),
          "an unrelated ID must not be blocked");
    gate.issue(2, north);

    // Draining one response is not enough: one is still in flight.
    gate.complete(5);
    check(gate.outstanding(5) == 1, "completing must decrement");
    check(!gate.may_issue(5, north),
          "the destination stays locked while any transaction is in flight");

    // Draining the last one releases the ID.
    gate.complete(5);
    check(gate.outstanding(5) == 0, "the ID must drain to zero");
    check(gate.may_issue(5, north),
          "a fully drained ID must admit a new destination");

    // Capacity: `MaxRoTxnsPerId` outstanding, then the ID blocks even for the
    // same destination.
    no_rob_order_gate small{2};
    small.issue(1, east);
    small.issue(1, east);
    check(!small.may_issue(1, east),
          "a full per-ID counter must stall even the same destination");
    small.complete(1);
    check(small.may_issue(1, east),
          "freeing a slot must re-admit the same destination");

    // Misuse is reported rather than silently accepted.
    bool threw = false;
    try {
        small.complete(7);
    } catch (const std::exception&) {
        threw = true;
    }
    check(threw, "a response for an idle ID must be rejected");

    if (failures == 0) {
        std::cout << "PASS: NoRoB ordering gate\n";
        return 0;
    }
    std::cerr << "FAIL: " << failures << " ordering checks failed\n";
    return 1;
}
