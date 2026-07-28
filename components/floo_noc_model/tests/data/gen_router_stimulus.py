#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Regenerate the shared five-port router cross-check stimulus.

Consumed unchanged by the SystemC runner (`tests/router_trace_sc.cpp`) and by
the SystemVerilog testbench
(`rtl_crosscheck/floo_router/tb_floo_router_trace.sv`).

Columns:
    cycle,rst_n,valid_i,last_i,ready_i,dst0,dst1,dst2,dst3,dst4

`valid_i`, `last_i`, and `ready_i` are 5-bit masks in hex, one bit per port in
the order North, East, South, West, Eject. `dstN` is one hex digit encoding the
destination coordinate of the flit presented on input N as `x + 4 * y`, so both
x and y are in 0..3.

The router under test sits at `xy_id = (1, 1)` in a 4x4 grid, so every
direction is reachable from it. Flit payloads are not carried in the CSV: both
sides derive `data_i[n].payload` from `(cycle + 1) * 16 + n`.

`hw/floo_router.sv` asserts

    StableValidIn: valid_i && !ready_o |=> $stable(valid_i)

so this generator never drops a `valid` that has not been accepted. It cannot
know the DUT's `ready_o`, so it takes the conservative route: once an input
asserts valid it holds valid, the destination, and `last` for a fixed burst of
cycles, which is long enough that the flit is always accepted first. Withdrawal
mid-request is therefore never generated, deliberately.

Usage:
    python3 gen_router_stimulus.py > router_stimulus.csv
"""

import sys

NORTH, EAST, SOUTH, WEST, EJECT = range(5)
ROUTER_X, ROUTER_Y = 1, 1
ALL = 0x1F


def dst(x, y):
    return x + 4 * y


def row(rst_n, valid, last, ready, dsts):
    return (rst_n, valid, last, ready, list(dsts))


def hold(rows, cycles, rst_n, valid, last, ready, dsts):
    """Hold one stimulus vector for several cycles, honouring StableValidIn."""
    for _ in range(cycles):
        rows.append(row(rst_n, valid, last, ready, dsts))


def directed():
    rows = []
    idle = [dst(ROUTER_X, ROUTER_Y)] * 5

    # Reset, then idle.
    hold(rows, 2, 0, 0x00, 0x00, 0x00, idle)
    hold(rows, 1, 1, 0x00, 0x00, 0x00, idle)

    # One single-flit packet per input, each to a distinct legal destination.
    # West input -> East output (x increasing), single flit, ready high.
    d = list(idle)
    d[WEST] = dst(3, ROUTER_Y)
    hold(rows, 3, 1, 1 << WEST, 1 << WEST, ALL, d)
    hold(rows, 1, 1, 0x00, 0x00, ALL, idle)

    # East input -> West output.
    d = list(idle)
    d[EAST] = dst(0, ROUTER_Y)
    hold(rows, 3, 1, 1 << EAST, 1 << EAST, ALL, d)
    hold(rows, 1, 1, 0x00, 0x00, ALL, idle)

    # Eject input -> North output (y increasing, x already matches).
    d = list(idle)
    d[EJECT] = dst(ROUTER_X, 3)
    hold(rows, 3, 1, 1 << EJECT, 1 << EJECT, ALL, d)
    hold(rows, 1, 1, 0x00, 0x00, ALL, idle)

    # North input -> South output.
    d = list(idle)
    d[NORTH] = dst(ROUTER_X, 0)
    hold(rows, 3, 1, 1 << NORTH, 1 << NORTH, ALL, d)
    hold(rows, 1, 1, 0x00, 0x00, ALL, idle)

    # Any input whose destination equals the router -> Eject output.
    d = list(idle)
    d[SOUTH] = dst(ROUTER_X, ROUTER_Y)
    hold(rows, 3, 1, 1 << SOUTH, 1 << SOUTH, ALL, d)
    hold(rows, 1, 1, 0x00, 0x00, ALL, idle)

    # Contention: West and South both target the East output. Only West is a
    # legal East requester; South->East is tied off by XYRouteOpt, so South
    # must never win East and must instead stall.
    d = list(idle)
    d[WEST] = dst(3, ROUTER_Y)
    d[SOUTH] = dst(3, ROUTER_Y)
    hold(rows, 6, 1, (1 << WEST) | (1 << SOUTH), (1 << WEST) | (1 << SOUTH), ALL, d)
    hold(rows, 2, 1, 0x00, 0x00, ALL, idle)

    # Contention on the Eject output from two legal inputs.
    d = list(idle)
    d[WEST] = dst(ROUTER_X, ROUTER_Y)
    d[EAST] = dst(ROUTER_X, ROUTER_Y)
    hold(rows, 8, 1, (1 << WEST) | (1 << EAST), (1 << WEST) | (1 << EAST), ALL, d)
    hold(rows, 2, 1, 0x00, 0x00, ALL, idle)

    # Output back-pressure: requesters present, destination not ready.
    d = list(idle)
    d[WEST] = dst(ROUTER_X, ROUTER_Y)
    d[NORTH] = dst(ROUTER_X, ROUTER_Y)
    hold(rows, 4, 1, (1 << WEST) | (1 << NORTH), (1 << WEST) | (1 << NORTH), 0x00, d)
    hold(rows, 6, 1, (1 << WEST) | (1 << NORTH), (1 << WEST) | (1 << NORTH), ALL, d)
    hold(rows, 2, 1, 0x00, 0x00, ALL, idle)

    # Multi-flit packet: two head flits then a last flit, from West to Eject,
    # while East also requests Eject. The wormhole lock must hold the output.
    d = list(idle)
    d[WEST] = dst(ROUTER_X, ROUTER_Y)
    d[EAST] = dst(ROUTER_X, ROUTER_Y)
    both = (1 << WEST) | (1 << EAST)
    hold(rows, 3, 1, both, 0x00, ALL, d)          # head flits, no last
    hold(rows, 3, 1, both, 1 << WEST, ALL, d)     # West finishes its packet
    hold(rows, 3, 1, both, 1 << EAST, ALL, d)     # East finishes its packet
    hold(rows, 2, 1, 0x00, 0x00, ALL, idle)

    # Every input requesting at once, all to Eject, ready high.
    d = [dst(ROUTER_X, ROUTER_Y)] * 5
    hold(rows, 10, 1, ALL, ALL, ALL, d)
    hold(rows, 2, 1, 0x00, 0x00, ALL, idle)

    # X-before-Y ordering: destination differs in both axes, so the flit must
    # leave along X first even though Y also mismatches.
    d = list(idle)
    d[EJECT] = dst(3, 3)
    hold(rows, 3, 1, 1 << EJECT, 1 << EJECT, ALL, d)
    # Idle between the two destinations: the flit data must not change while a
    # request is still pending.
    hold(rows, 2, 1, 0x00, 0x00, ALL, idle)
    d = list(idle)
    d[EJECT] = dst(0, 0)
    hold(rows, 3, 1, 1 << EJECT, 1 << EJECT, ALL, d)
    hold(rows, 2, 1, 0x00, 0x00, ALL, idle)

    # Mid-packet reset.
    d = list(idle)
    d[WEST] = dst(3, ROUTER_Y)
    hold(rows, 2, 1, 1 << WEST, 0x00, 0x00, d)
    hold(rows, 2, 0, 0x00, 0x00, 0x00, idle)
    hold(rows, 1, 1, 0x00, 0x00, 0x00, idle)

    return rows


def pseudo_random(count, seed=0x5EED_1234):
    """Deterministic traffic that still respects StableValidIn.

    A new vector is only presented after the previous one has been held long
    enough to be accepted, so `valid` never falls before a grant.
    """
    rows = []
    state = seed
    idle = [dst(ROUTER_X, ROUTER_Y)] * 5
    while len(rows) < count:
        state = (state * 1103515245 + 12345) & 0x7FFF_FFFF
        valid = (state >> 7) & ALL
        state = (state * 1103515245 + 12345) & 0x7FFF_FFFF
        ready = (state >> 11) & ALL
        dsts = []
        for port in range(5):
            state = (state * 1103515245 + 12345) & 0x7FFF_FFFF
            dsts.append(dst((state >> 5) & 0x3, (state >> 9) & 0x3))
        # Each burst is single-flit traffic held under the randomised ready
        # mask, then drained with every output ready, then idled. Draining
        # before the mask changes keeps `valid` from falling on a request that
        # was never granted.
        hold(rows, 4, 1, valid, valid, ready, dsts)
        hold(rows, 6, 1, valid, valid, ALL, dsts)
        hold(rows, 2, 1, 0x00, 0x00, ALL, idle)
    return rows[:count]


def main() -> int:
    rows = directed() + pseudo_random(120)
    rows += [row(0, 0x00, 0x00, 0x00, [dst(ROUTER_X, ROUTER_Y)] * 5)] * 2
    rows += [row(1, 0x00, 0x00, 0x1F, [dst(ROUTER_X, ROUTER_Y)] * 5)]

    out = sys.stdout
    out.write("cycle,rst_n,valid_i,last_i,ready_i,dst0,dst1,dst2,dst3,dst4\n")
    for cycle, (rst_n, valid, last, ready, dsts) in enumerate(rows):
        fields = ",".join(f"{value:x}" for value in dsts)
        out.write(f"{cycle},{rst_n},{valid:02x},{last:02x},{ready:02x},{fields}\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
