// SPDX-License-Identifier: Apache-2.0
//
// Exclusive monitor: LR/SC reservations and AMO exclusion shared by every bus
// master of one address space, CPU and non-CPU alike.
//
// A CPU backend that supports it calls these hooks from its LR, SC, AMO and
// store paths; a non-CPU master (a DMA engine, an image DMA) calls the write
// hooks from a bus-side guard. The monitor itself is a platform object: the
// reservation granule and the set of masters belong to the SoC, not to a core.
//
// Contract, per hook. Every call is made from an SC_THREAD; the ones that may
// block say so.
//
//  * `load_reserved` -- LR. Opens (or replaces) the hart's reservation over the
//    granule containing [addr, addr + size). Called **before** the load is
//    issued, so a write that completes after the load observed memory is seen.
//  * `drop_reservation` -- the LR's load faulted: the reservation it opened is
//    void (an LR that did not complete reserves nothing), so a later SC fails.
//  * `atomic_begin` / `atomic_end` -- bracket an SC's check-and-store or an
//    AMO's load-modify-store. `atomic_begin` **blocks** until no write by
//    another master that overlaps the granule is in flight, and holds off new
//    ones until `atomic_end`. The CPU backend also excludes its sibling harts
//    for the same span (a VP++ hart holds its bus lock).
//  * `take_reservation` -- SC, inside the bracket: true when the hart holds a
//    reservation that is still valid and covers [addr, addr + size). The
//    reservation is consumed either way.
//  * `try_begin_write` / `end_write` -- bracket every write: plain CPU stores,
//    the store of a successful SC or AMO, and every non-CPU write. A write that
//    overlaps another master's atomic bracket is refused (`false`); the caller
//    waits on `changed()` and retries. `end_write` invalidates the reservations
//    of every *other* master whose granule the write overlapped.
//  * `reset_hart` -- the hart's reset: drops its reservation and any bracket.
//
// Master identifiers: CPU harts use their hart ID; non-CPU masters use IDs a
// platform assigns outside the hart range.

#pragma once

#include <cstdint>

#include <systemc>

namespace cdc::cpu {

class exclusive_monitor_if {
public:
    virtual ~exclusive_monitor_if() = default;

    virtual void load_reserved(unsigned hart, std::uint64_t addr, unsigned size) = 0;
    virtual void drop_reservation(unsigned hart) = 0;
    virtual void atomic_begin(unsigned hart, std::uint64_t addr, unsigned size) = 0;
    virtual bool take_reservation(unsigned hart, std::uint64_t addr, unsigned size) = 0;
    virtual void atomic_end(unsigned hart) = 0;

    virtual bool try_begin_write(unsigned master, std::uint64_t addr, unsigned size,
                                 std::uint64_t& ticket) = 0;
    virtual void end_write(std::uint64_t ticket) = 0;

    virtual void reset_hart(unsigned hart) = 0;

    /// Notified whenever a bracket or an in-flight write ends.
    virtual const sc_core::sc_event& changed() const = 0;
};

}  // namespace cdc::cpu
