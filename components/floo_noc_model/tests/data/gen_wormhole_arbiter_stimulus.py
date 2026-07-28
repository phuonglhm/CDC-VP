#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Regenerate the shared wormhole-arbiter cross-check stimulus.

Consumed unchanged by the SystemC runner (`tests/arbiter_trace_sc.cpp`) and by
the SystemVerilog testbench
(`rtl_crosscheck/wormhole_arbiter/tb_wormhole_arbiter_trace.sv`).

Columns:
    cycle,rst_n,ready_i,valid_i,last_i

`valid_i` and `last_i` are 5-bit masks in hex, one bit per input route. A
testbench parameterised with fewer routes uses the low bits. Flit payloads are
not carried in the CSV: both sides derive `data_i[j].payload` from
`(cycle + 1) * 16 + j`, which keeps the packet payloads distinguishable per
input and per cycle without widening the file.

The directed prefix walks the arbitration corners; the pseudo-random tail is a
deterministic LCG so both simulators see identical traffic.

Usage:
    python3 gen_wormhole_arbiter_stimulus.py > wormhole_arbiter_stimulus.csv
"""

import sys

NUM_ROUTES = 5
ALL = (1 << NUM_ROUTES) - 1


def directed():
    """(rst_n, ready_i, valid_i, last_i) rows."""
    rows = []

    # Reset and idle.
    rows += [(0, 0, 0x00, 0x00)] * 2
    rows += [(1, 0, 0x00, 0x00)]

    # Single requester, single-flit packet: request without ready, then accept.
    rows += [(1, 0, 0x01, 0x01)]
    rows += [(1, 1, 0x01, 0x01)]
    rows += [(1, 0, 0x00, 0x00)]

    # Each input alone, one accepted last flit each. This exposes the fair
    # round-robin advance, which is not "selected + 1".
    for route in range(NUM_ROUTES):
        rows += [(1, 1, 1 << route, 1 << route)]
    rows += [(1, 0, 0x00, 0x00)]

    # Two-flit packet from route 1 while routes 0 and 2 keep requesting:
    # the arbiter must not switch mid-packet.
    rows += [(1, 0, 0x02, 0x00)]          # route 1 head flit, stalled
    rows += [(1, 1, 0x07, 0x00)]          # accept head, others also asking
    rows += [(1, 1, 0x07, 0x02)]          # accept route 1 last flit
    rows += [(1, 1, 0x05, 0x05)]          # remaining requesters
    rows += [(1, 1, 0x04, 0x04)]
    rows += [(1, 0, 0x00, 0x00)]

    # All routes request simultaneously, all single-flit, ready held high.
    rows += [(1, 1, ALL, ALL)] * 6
    rows += [(1, 0, 0x00, 0x00)]

    # Contention with back-pressure: requests present, ready low for a while.
    rows += [(1, 0, 0x1B, 0x1B)] * 3
    rows += [(1, 1, 0x1B, 0x1B)] * 3
    rows += [(1, 0, 0x00, 0x00)]

    # Multi-flit packets from two routes, interleaved requests, so the snapshot
    # and the tree lock both matter.
    rows += [(1, 1, 0x09, 0x00)]          # routes 0 and 3, head flits
    rows += [(1, 1, 0x09, 0x00)]
    rows += [(1, 1, 0x09, 0x01)]          # route 0 finishes
    rows += [(1, 1, 0x08, 0x00)]
    rows += [(1, 1, 0x08, 0x08)]          # route 3 finishes
    rows += [(1, 0, 0x00, 0x00)]

    # A requester that withdraws while selected. The RTL protects this with an
    # assumption, but the model must still not diverge on it.
    rows += [(1, 0, 0x10, 0x10)]
    rows += [(1, 0, 0x00, 0x00)]
    rows += [(1, 1, 0x02, 0x02)]
    rows += [(1, 0, 0x00, 0x00)]

    # ready_i asserted with no requester at all.
    rows += [(1, 1, 0x00, 0x00)] * 2

    # Mid-packet reset.
    rows += [(1, 1, 0x06, 0x00)]
    rows += [(0, 0, 0x00, 0x00)] * 2
    rows += [(1, 1, 0x06, 0x06)]
    rows += [(1, 0, 0x00, 0x00)]

    return rows


def pseudo_random(count, seed=0x0BADC0DE):
    rows = []
    state = seed
    for _ in range(count):
        state = (state * 1103515245 + 12345) & 0x7FFF_FFFF
        valid = (state >> 8) & ALL
        ready = (state >> 19) & 1
        # `last` only ever asserts on a requesting input.
        last = valid & ((state >> 3) & ALL)
        rows.append((1, ready, valid, last))
    return rows


def main() -> int:
    rows = directed() + pseudo_random(100)
    rows += [(0, 0, 0x00, 0x00), (1, 1, 0x11, 0x11), (1, 0, 0x00, 0x00)]

    out = sys.stdout
    out.write("cycle,rst_n,ready_i,valid_i,last_i\n")
    for cycle, (rst_n, ready, valid, last) in enumerate(rows):
        out.write(f"{cycle},{rst_n},{ready},{valid:02x},{last:02x}\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
