// SPDX-License-Identifier: Apache-2.0
//
// Configurations the wrapper must refuse, and refuse *cleanly*.
//
// Both are rejected while the caller is still making ordinary function calls,
// so the exception unwinds normally and SystemC tears down as usual. This file
// deliberately contains no `std::_Exit`: an earlier version needed one, because
// the clock was validated only after the mesh had been built and the self-node
// conflict only during elaboration. A configuration error that can only be
// survived by skipping destructors is not being handled, it is being escaped.
//
// `sc_start` is never called. Nothing here needs simulation — that is the
// point, these are configuration errors and must surface before any of it.

#include "floo_noc_model/noc_interconnect.h"

#include <iostream>
#include <stdexcept>
#include <string>

#include <systemc>

namespace {

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
    // ---- manager-ID capacity ----------------------------------------------
    //
    // The frozen chimney has a 3-bit manager AXI ID. The wrapper assigns one
    // ID per upstream port, so ports 0..7 are representable and port 8 is not.
    // Refuse both an empty manager set and an ID-truncating manager set before
    // any mesh hierarchy is built.
    {
        bool zero_threw = false;
        try {
            cdc::components::noc_interconnect bad{
                "no_managers", 2, 2, 1, 0};
        } catch (const std::invalid_argument&) {
            zero_threw = true;
        }
        check(zero_threw, "at least one upstream port must be required");

        bool too_many_threw = false;
        try {
            cdc::components::noc_interconnect bad{
                "too_many_managers", 2, 2, 1, 9};
        } catch (const std::invalid_argument&) {
            too_many_threw = true;
        }
        check(too_many_threw,
              "a 3-bit manager ID must refuse more than 8 upstream ports");
    }

    // ---- per-port outstanding capacity ------------------------------------
    //
    // The frozen response metadata FIFOs are 32 entries deep. The wrapper may
    // choose a smaller combined read/write admission limit, but zero would
    // deadlock every caller and more than 32 would over-promise the signed
    // configuration's capacity.
    {
        bool zero_threw = false;
        try {
            cdc::components::noc_interconnect bad{
                "zero_slots", 2, 2, 1, 1,
                sc_core::sc_time(1, sc_core::SC_NS), 0};
        } catch (const std::invalid_argument&) {
            zero_threw = true;
        }
        check(zero_threw,
              "max_outstanding_per_port zero must be refused");

        bool too_many_threw = false;
        try {
            cdc::components::noc_interconnect bad{
                "too_many_slots", 2, 2, 1, 1,
                sc_core::sc_time(1, sc_core::SC_NS), 33};
        } catch (const std::invalid_argument&) {
            too_many_threw = true;
        }
        check(too_many_threw,
              "max_outstanding_per_port above 32 must be refused");
    }

    // ---- timing backend ---------------------------------------------------
    {
        bool invalid_threw = false;
        try {
            cdc::components::noc_interconnect bad{
                "bad_timing", 2, 2, 1, 1,
                sc_core::sc_time(1, sc_core::SC_NS),
                cdc::components::noc_interconnect::
                    default_max_outstanding_per_port,
                static_cast<
                    cdc::components::noc_interconnect::timing_mode>(99)};
        } catch (const std::invalid_argument&) {
            invalid_threw = true;
        }
        check(invalid_threw,
              "an unknown timing backend must be refused");
    }

    // ---- an unusable network clock -----------------------------------------
    //
    // Every latency in the model is counted in network cycles, so a zero period
    // makes them all meaningless and the half-period waits inside the clock
    // process would spin without advancing time. Checked before the mesh is
    // built, so no hierarchy exists to leave half-constructed.
    {
        bool threw = false;
        try {
            cdc::components::noc_interconnect bad{
                "bad_clock", 2, 2, 1, 1, sc_core::SC_ZERO_TIME};
        } catch (const std::invalid_argument&) {
            threw = true;
        }
        check(threw, "a zero network clock period must be refused");
    }

    // ---- a target sharing a node with a manager ----------------------------
    //
    // `floo_router` defaults to `NoLoopback = 1`: the Eject-input to
    // Eject-output crossbar leg is tied to zero, so a flit addressed to the
    // node that injected it can never be delivered. The platform hit this once
    // and simply hung. Rejected at `add_target`, which is where the
    // configuration becomes complete enough to be wrong.
    {
        cdc::components::noc_interconnect noc{"self_node", 2, 2, 2, 1};
        noc.place_initiator(0, {0, 0});

        bool threw = false;
        try {
            noc.add_target(0x8000'0000, 0x1000, {0, 0});
        } catch (const std::runtime_error&) {
            threw = true;
        }
        check(threw, "a target on a manager's own node must be refused");

        // The opposite order must be caught too: a target placed first, then a
        // manager moved on top of it.
        bool moved_threw = false;
        try {
            cdc::components::noc_interconnect other{"moved", 2, 2, 2, 1};
            other.add_target(0x8000'0000, 0x1000, {1, 1});
            other.place_initiator(0, {1, 1});
        } catch (const std::runtime_error&) {
            moved_threw = true;
        }
        check(moved_threw,
              "moving a manager onto an existing target must be refused");

        // And a legal layout must still be accepted, or the check above proves
        // nothing except that the guard fires on everything.
        bool legal_threw = false;
        try {
            cdc::components::noc_interconnect fine{"fine", 2, 2, 2, 1};
            fine.place_initiator(0, {0, 0});
            fine.add_target(0x8000'0000, 0x1000, {1, 0});
        } catch (const std::exception&) {
            legal_threw = true;
        }
        check(!legal_threw, "a target on a free node must be accepted");
    }

    // ---- R3-F3: the documented default (0,0) is a real placement -----------
    //
    // The public contract says an unplaced port sits at (0,0). A target added
    // there must therefore be refused by `add_target()` itself — not accepted
    // during configuration and rejected later from `end_of_elaboration()`,
    // which is the late failure the whole check was moved forward to avoid.
    {
        cdc::components::noc_interconnect noc{"default_node", 2, 2, 3, 1};
        // Deliberately no `place_initiator` call: the port is at its default.

        bool threw = false;
        try {
            noc.add_target(0x8000'0000, 0x1000, {0, 0});
        } catch (const std::runtime_error&) {
            threw = true;
        }
        check(threw,
              "a target on an unplaced initiator's default (0,0) must be "
              "refused by add_target itself");

        // The rejection must not have consumed a slot: three legal targets must
        // still fit.
        bool slots_intact = true;
        try {
            noc.add_target(0x8000'0000, 0x1000, {1, 0});
            noc.add_target(0x9000'0000, 0x1000, {1, 1});
            noc.add_target(0xA000'0000, 0x1000, {0, 1});
        } catch (const std::exception&) {
            slots_intact = false;
        }
        check(slots_intact,
              "a refused self-node add_target must not consume a slot");
    }

    // A rejected `place_initiator` must leave the port where it was.
    //
    // The observable consequence, not the call's own return: `add_target` on
    // the port's node is refused, so where the port *is* can be read back
    // through the target table. An earlier version of this test only re-placed
    // the port at its original node and checked that the call succeeded — which
    // an implementation that commits the position before throwing also passes,
    // because moving a port to a node with nothing on it is legal wherever the
    // port started. The mutation was injected and this test stayed green.
    {
        cdc::components::noc_interconnect noc{"keep_position", 2, 2, 4, 1};
        noc.place_initiator(0, {1, 1});
        noc.add_target(0x8000'0000, 0x1000, {0, 1});

        bool threw = false;
        try {
            noc.place_initiator(0, {0, 1});
        } catch (const std::runtime_error&) {
            threw = true;
        }
        check(threw, "moving a port onto a target must be refused");

        // The port must still be at (1,1): a target there must be refused.
        // If the rejected move had been committed, (1,1) would now be free and
        // this would be accepted.
        bool still_at_old_node = false;
        try {
            noc.add_target(0x9000'0000, 0x1000, {1, 1});
        } catch (const std::runtime_error&) {
            still_at_old_node = true;
        }
        check(still_at_old_node,
              "after a refused placement the port must still occupy its old "
              "node, so a target there is still refused");

        // And it must *not* be at (0,1), the node it was refused from. That
        // node already hosts a target, so the port being there would be the
        // very conflict the rejection prevented. Placing a second port is not
        // possible here, so this is read the other way round: moving the port
        // to a third node must succeed, and only then does (1,1) open up.
        // A distinct address, so this check cannot fail as a side effect of the
        // one above having wrongly consumed 0x9000'0000. Each check has to fail
        // only for its own reason, or the control's evidence says nothing about
        // which property broke.
        noc.place_initiator(0, {1, 0});
        bool old_node_now_free = true;
        try {
            noc.add_target(0xA000'0000, 0x1000, {1, 1});
        } catch (const std::exception&) {
            old_node_now_free = false;
        }
        check(old_node_now_free,
              "once the port really moves, its previous node accepts a target");
    }

    // ---- R3-F2: target mappings validated before a slot is consumed --------
    {
        cdc::components::noc_interconnect noc{"regions", 2, 2, 4, 1};
        noc.place_initiator(0, {0, 0});

        const auto rejected = [&](const char* what, std::uint64_t base,
                                  std::uint64_t size) {
            bool threw = false;
            try {
                noc.add_target(base, size, {1, 0});
            } catch (const std::invalid_argument&) {
                threw = true;
            }
            check(threw, what);
        };

        rejected("a zero-sized region must be refused", 0x8000'0000, 0);
        rejected("a region whose last byte wraps must be refused",
                 0xFFFF'FFFF'FFFF'FF00ull, 0x200);

        // A valid region ending exactly at UINT64_MAX. `addr < base + size`
        // wraps to zero here and makes it unreachable, which is why the decode
        // uses subtraction.
        bool top_ok = true;
        try {
            noc.add_target(0xFFFF'FFFF'FFFF'FF00ull, 0x100, {1, 0});
        } catch (const std::exception&) {
            top_ok = false;
        }
        check(top_ok, "a region ending exactly at UINT64_MAX must be accepted");

        // A one-byte region at the very top.
        bool one_byte_ok = true;
        try {
            cdc::components::noc_interconnect edge{"top_byte", 2, 2, 1, 1};
            edge.place_initiator(0, {0, 0});
            edge.add_target(0xFFFF'FFFF'FFFF'FFFFull, 1, {1, 0});
        } catch (const std::exception&) {
            one_byte_ok = false;
        }
        check(one_byte_ok,
              "a one-byte region at UINT64_MAX must be accepted");

        rejected("an overlapping region must be refused",
                 0xFFFF'FFFF'FFFF'FF80ull, 0x80);

        // Every rejection above must have left the slot count alone: three
        // slots remain, so three more legal additions must succeed.
        bool slots_intact = true;
        try {
            noc.add_target(0x1000'0000, 0x1000, {1, 0});
            noc.add_target(0x2000'0000, 0x1000, {1, 0});
            noc.add_target(0x3000'0000, 0x1000, {1, 0});
        } catch (const std::exception&) {
            slots_intact = false;
        }
        check(slots_intact,
              "a refused add_target must not consume a target slot");
    }

    if (failures != 0) {
        std::cerr << failures << " configuration checks failed\n";
        return 1;
    }
    std::cout << "PASS: invalid configurations are refused cleanly\n";
    return 0;
}
