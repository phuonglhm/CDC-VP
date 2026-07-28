#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Regenerate the shared stream-FIFO cross-check stimulus.

The stimulus is consumed unchanged by the SystemC runner
(`tests/fifo_trace_sc.cpp`) and by the SystemVerilog testbench
(`rtl_crosscheck/stream_fifo/tb_stream_fifo_trace.sv`). It has a directed
prefix that walks the reset/fill/full/drain corners of both wrap branches
(depth 2 spill register, depth 4 stream FIFO), followed by a deterministic
pseudo-random tail that stresses pointer wrap and back-to-back handshakes.

Usage:
    python3 gen_stream_fifo_stimulus.py > stream_fifo_stimulus.csv
"""

import sys

MASK64 = (1 << 64) - 1
DATA_STEP = 0x0123456789ABCDEF


def directed():
    """(rst_n, valid_i, ready_i) triples covering the handshake corners."""
    rows = []

    # Reset, then an idle cycle.
    rows += [(0, 0, 0)] * 2
    rows += [(1, 0, 0)]

    # Fill past full with the output stalled. Depth 2 saturates at row 2,
    # depth 4 at row 4; the trailing rows must be refused by both.
    rows += [(1, 1, 0)] * 6

    # Push while full and popping in the same cycle. Both RTL branches must
    # refuse the push, because ready_o is a function of registers only.
    rows += [(1, 1, 1)] * 3

    # Drain to empty, then idle.
    rows += [(1, 0, 1)] * 4
    rows += [(1, 0, 0)] * 2

    # Full-rate streaming.
    rows += [(1, 1, 1)] * 6

    # Fill, hold, drain.
    rows += [(1, 1, 0)] * 3
    rows += [(1, 0, 0)] * 2
    rows += [(1, 0, 1)] * 4

    # Reset in mid-stream, then confirm the buffer restarts empty.
    rows += [(1, 1, 0)] * 2
    rows += [(0, 0, 0)] * 2
    rows += [(1, 0, 0)]
    rows += [(1, 1, 0)] * 2
    rows += [(1, 0, 1)] * 3

    # Single-cycle valid and ready pulses.
    rows += [(1, 1, 0), (1, 0, 0), (1, 0, 1), (1, 0, 0)]
    rows += [(1, 1, 1), (1, 0, 0)]

    return rows


def pseudo_random(count, seed=0x1234_5678):
    """Deterministic LCG toggling of valid_i/ready_i."""
    rows = []
    state = seed
    for _ in range(count):
        state = (state * 1103515245 + 12345) & 0x7FFF_FFFF
        valid = (state >> 16) & 1
        ready = (state >> 20) & 1
        rows.append((1, valid, ready))
    return rows


def main() -> int:
    rows = directed() + pseudo_random(80)
    # A final reset proves the pseudo-random tail leaves no sticky state.
    rows += [(0, 1, 1), (1, 0, 0), (1, 1, 0), (1, 0, 1)]

    out = sys.stdout
    out.write("cycle,rst_n,valid_i,ready_i,data_i\n")
    for cycle, (rst_n, valid, ready) in enumerate(rows):
        data = ((cycle + 1) * DATA_STEP) & MASK64
        out.write(f"{cycle},{rst_n},{valid},{ready},{data:016x}\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
