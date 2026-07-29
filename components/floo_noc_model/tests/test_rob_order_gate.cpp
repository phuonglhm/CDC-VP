// SPDX-License-Identifier: Apache-2.0
//
// Contract test for the `NoRoB` response-ordering rule.
//
// Expectations are read out of the `NoRoB` branch of
// `hw/floo_rob_wrapper.sv` and the `axi_demux_id_counters` it instantiates:
//
//   push = ax_valid_i && (!in_flight || ax_dest_i == prev_dest) && !counter_full
//
// The cycle-exact comparison of the same rule lives in
// `rtl_crosscheck/run_rob_crosscheck.sh`. This test covers the arithmetic that
// the transaction-level wrapper exposes to `axi_endpoint.hpp`.

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

    // `MaxRoTxnsPerId = 4` gives `CounterWidth = $clog2(4) = 2`, so the bank
    // saturates at `2**2 - 1 = 3`, not at 4.
    no_rob_order_gate gate{4, 2};
    check(gate.capacity() == 3,
          "capacity is 2**$clog2(MaxRoTxnsPerId) - 1, not MaxRoTxnsPerId");

    // Nothing in flight: any destination is admissible.
    check(gate.may_issue(0, east), "an idle ID must admit any destination");
    gate.issue(0, east);
    check(gate.outstanding(0) == 1, "issuing must count one outstanding");

    // Same ID, same destination: not serialised.
    check(gate.may_issue(0, east),
          "reusing an ID for the same destination must not stall");
    gate.issue(0, east);
    check(gate.outstanding(0) == 2, "a second issue must count");

    // Same ID, different destination: blocked until the ID drains.
    check(!gate.may_issue(0, north),
          "reusing an ID for a different destination must stall");

    // A different ID is unaffected while no counter is saturated.
    check(gate.may_issue(1, north),
          "an unrelated ID must not be blocked below saturation");

    // Draining one response is not enough: one is still in flight.
    gate.complete(0);
    check(gate.outstanding(0) == 1, "completing must decrement");
    check(!gate.may_issue(0, north),
          "the destination stays locked while any transaction is in flight");

    // Draining the last one releases the ID.
    gate.complete(0);
    check(gate.outstanding(0) == 0, "the ID must drain to zero");
    check(gate.may_issue(0, north),
          "a fully drained ID must admit a new destination");

    // ---- `full_o` is a global OR, not a per-ID signal -------------------
    //
    // `axi_demux_id_counters` exports `full_o = |cnt_full` across every
    // counter in the bank. Saturating one ID therefore stalls all of them.
    // This is the rule a per-ID model gets wrong.

    no_rob_order_gate bank{4, 2};
    for (unsigned issue = 0; issue < bank.capacity(); ++issue) {
        bank.issue(2, east);
    }
    check(bank.outstanding(2) == bank.capacity(), "ID 2 must be saturated");
    check(bank.full(), "a saturated counter must raise the global full");
    check(!bank.may_issue(2, east),
          "a full counter must stall even the same destination");
    check(!bank.may_issue(3, north),
          "a saturated ID must stall an unrelated, completely idle ID");

    bank.complete(2);
    check(!bank.full(), "freeing a slot must drop the global full");
    check(bank.may_issue(3, north),
          "the unrelated ID must be admitted again once the bank is not full");

    // Misuse is reported rather than silently accepted.
    bool threw = false;
    try {
        bank.complete(1);
    } catch (const std::exception&) {
        threw = true;
    }
    check(threw, "a response for an idle ID must be rejected");

    threw = false;
    try {
        bank.may_issue(9, east);  // outside a 2-bit bank
    } catch (const std::exception&) {
        threw = true;
    }
    check(threw, "an AXI ID outside the counter bank must be rejected");

    if (failures == 0) {
        std::cout << "PASS: NoRoB ordering gate\n";
        return 0;
    }
    std::cerr << "FAIL: " << failures << " ordering checks failed\n";
    return 1;
}
