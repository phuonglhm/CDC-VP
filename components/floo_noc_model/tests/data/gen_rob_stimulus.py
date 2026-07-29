#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
#
# Generates the shared stimulus for the `NoRoB` ordering cross-check.
#
# The stimulus drives the module's inputs only. Both the SystemC model and the
# unmodified `hw/floo_rob_wrapper.sv` replay the identical vector, so any
# divergence between their traces is the model's.
#
# This generator does carry a shadow of the push/pop rule, but only to keep the
# response stream *legal*: `axi_demux_id_counters` carries an underflow
# assertion, and answering a transaction that was never admitted would corrupt
# the counter bank on both sides rather than compare it. The shadow decides
# when a response may be offered; it never decides what the trace should say.
#
# Phases are hand-built rather than random, because the interesting states of
# this module are unreachable by chance:
#
#   * a same-ID/different-destination stall needs the ID to be outstanding at
#     the moment the second destination is offered;
#   * `counter_full` needs an ID driven to `2**$clog2(MaxRoTxnsPerId) - 1`
#     outstanding with no intervening response;
#   * the *global* nature of `full_o` only shows if a second, completely idle
#     ID is offered while the first is saturated.
#
# Parameters are chosen small so those states are a few cycles apart:
# AxiIdBits = 2 (four counters) and MaxRoTxnsPerId = 4, which gives
# CounterWidth = 2 and a real capacity of 3.

import sys

AX_ID_BITS = 2
NUM_IDS = 1 << AX_ID_BITS
MAX_RO_TXNS_PER_ID = 4

HEADER = "cycle,rst_n,ax_valid,ax_id,ax_dest,ax_ready,rsp_valid,rsp_id,rsp_last,rsp_ready"

# Destinations are a 4-bit code; the model maps it onto a coordinate
# bijectively, so only equality matters.
DEST_A = 0x1
DEST_B = 0x6
DEST_C = 0x9


def clog2(value):
    return (value - 1).bit_length()


class Shadow:
    """Mirror of the `NoRoB` admission rule and its counter bank.

    Faithful to `axi_demux_id_counters`: the counter is `CounterWidth + 1` bits
    with `in_flight` the low `CounterWidth`, `cnt_full` is per counter, and
    `full` is the global OR across the bank.
    """

    def __init__(self):
        self.counter_width = clog2(MAX_RO_TXNS_PER_ID)
        self.q_mask = (1 << self.counter_width) - 1
        self.counter_mask = (1 << (self.counter_width + 1)) - 1
        self.cnt = [0] * NUM_IDS
        self.sel = [0] * NUM_IDS

    @property
    def capacity(self):
        return self.q_mask

    def reset(self):
        self.cnt = [0] * NUM_IDS
        self.sel = [0] * NUM_IDS

    def in_flight(self, axi_id):
        return self.cnt[axi_id] & self.q_mask

    def cnt_full(self, axi_id):
        overflow = (self.cnt[axi_id] >> self.counter_width) & 1
        return bool(overflow) or self.in_flight(axi_id) == self.q_mask

    def full(self):
        return any(self.cnt_full(i) for i in range(NUM_IDS))

    def push(self, ax_valid, ax_id, ax_dest):
        occupied = self.in_flight(ax_id) != 0
        return bool(ax_valid) and (not occupied or ax_dest == self.sel[ax_id]) \
            and not self.full()

    def step(self, rst_n, ax_valid, ax_id, ax_dest, ax_ready,
             rsp_valid, rsp_id, rsp_last, rsp_ready):
        if not rst_n:
            self.reset()
            return
        push_en = self.push(ax_valid, ax_id, ax_dest) and bool(ax_ready)
        pop_en = bool(rsp_valid) and bool(rsp_last) and bool(rsp_ready)
        for index in range(NUM_IDS):
            p = push_en and ax_id == index
            o = pop_en and rsp_id == index
            if p and not o:
                self.cnt[index] = (self.cnt[index] + 1) & self.counter_mask
            elif o and not p:
                self.cnt[index] = (self.cnt[index] - 1) & self.counter_mask
            if p:
                self.sel[index] = ax_dest


rows = []
shadow = Shadow()
CAPACITY = shadow.capacity


def drive(rst_n=1, ax_valid=0, ax_id=0, ax_dest=0, ax_ready=1,
          rsp_valid=0, rsp_id=0, rsp_last=1, rsp_ready=1, times=1):
    for _ in range(times):
        if rsp_valid and rsp_last and rsp_ready and shadow.in_flight(rsp_id) == 0:
            raise AssertionError(
                f"cycle {len(rows)}: popping ID {rsp_id} with nothing in flight")
        rows.append((len(rows), rst_n, ax_valid, ax_id, ax_dest, ax_ready,
                     rsp_valid, rsp_id, rsp_last, rsp_ready))
        shadow.step(rst_n, ax_valid, ax_id, ax_dest, ax_ready,
                    rsp_valid, rsp_id, rsp_last, rsp_ready)


def idle(times=1):
    drive(times=times)


def drain(axi_id):
    """Answer every outstanding transaction of one ID."""
    while shadow.in_flight(axi_id) > 0:
        drive(rsp_valid=1, rsp_id=axi_id, rsp_last=1)


def drain_all():
    for axi_id in range(NUM_IDS):
        drain(axi_id)


# ---- Phase 0: reset ------------------------------------------------------
drive(rst_n=0, times=4)
idle(2)

# ---- Phase A: one transaction, then its response -------------------------
drive(ax_valid=1, ax_id=0, ax_dest=DEST_A)
assert shadow.in_flight(0) == 1
idle(2)
drive(rsp_valid=1, rsp_id=0, rsp_last=1)
assert shadow.in_flight(0) == 0
idle(2)

# ---- Phase B: same ID, same destination, back to back --------------------
# Must not stall: `ax_dest_i == prev_dest` is the escape in the push rule.
drive(ax_valid=1, ax_id=1, ax_dest=DEST_B, times=2)
assert shadow.in_flight(1) == 2, "same destination must not have stalled"
idle(2)

# ---- Phase C: same ID, different destination, while outstanding ----------
drive(ax_valid=1, ax_id=1, ax_dest=DEST_C, times=3)
assert shadow.in_flight(1) == 2, "a different destination must have stalled"
idle(1)

# ---- Phase D: drain ID 1, then the new destination is admitted -----------
drive(rsp_valid=1, rsp_id=1, rsp_last=1)
# One still outstanding: still stalled.
drive(ax_valid=1, ax_id=1, ax_dest=DEST_C)
assert shadow.in_flight(1) == 1, "one outstanding still locks the destination"
drive(rsp_valid=1, rsp_id=1, rsp_last=1)
# Fully drained: admitted.
drive(ax_valid=1, ax_id=1, ax_dest=DEST_C)
assert shadow.in_flight(1) == 1, "a drained ID must admit a new destination"
idle(1)
drain(1)
idle(2)

# ---- Phase E: response handshake qualifiers ------------------------------
# `pop = rsp_valid_i && rsp_last_i`, applied to the counter only when
# `rsp_ready_i` is also set. Neither of the next two may pop.
drive(ax_valid=1, ax_id=2, ax_dest=DEST_A)
drive(rsp_valid=1, rsp_id=2, rsp_last=0, rsp_ready=1)   # not last
drive(rsp_valid=1, rsp_id=2, rsp_last=1, rsp_ready=0)   # no handshake
assert shadow.in_flight(2) == 1, "neither qualifier may pop the counter"
# ID 2 is still outstanding, so a new destination stalls.
drive(ax_valid=1, ax_id=2, ax_dest=DEST_B)
assert shadow.in_flight(2) == 1
drive(rsp_valid=1, rsp_id=2, rsp_last=1, rsp_ready=1)   # this one pops
assert shadow.in_flight(2) == 0
drive(ax_valid=1, ax_id=2, ax_dest=DEST_B)
idle(1)
drain(2)
idle(2)

# ---- Phase F: request back-pressure --------------------------------------
# `ax_ready_o = push && ax_ready_i`, and the counter pushes only on the
# handshake. With `ax_ready_i` low the request must neither complete nor count.
drive(ax_valid=1, ax_id=3, ax_dest=DEST_A, ax_ready=0, times=3)
assert shadow.in_flight(3) == 0, "a stalled request must not count"
drive(ax_valid=1, ax_id=3, ax_dest=DEST_A, ax_ready=1)
assert shadow.in_flight(3) == 1
idle(1)
drain(3)
idle(2)

# ---- Phase G: saturate one counter, then probe an idle ID ----------------
# This is the phase that separates a per-ID full from the RTL's global
# `full_o = |cnt_full`.
drive(ax_valid=1, ax_id=0, ax_dest=DEST_A, times=CAPACITY)
assert shadow.in_flight(0) == CAPACITY
assert shadow.full(), "the bank must report full once one counter saturates"
# Offering the saturated ID again must stall.
drive(ax_valid=1, ax_id=0, ax_dest=DEST_A, times=2)
assert shadow.in_flight(0) == CAPACITY
# ID 3 is completely idle and its own counter is empty. Under a per-ID full it
# would be admitted; under the RTL's global full it must stall.
drive(ax_valid=1, ax_id=3, ax_dest=DEST_C, times=3)
assert shadow.in_flight(3) == 0, "the global full must stall an idle ID"
# Free one slot on ID 0 and the idle ID becomes admissible again.
drive(rsp_valid=1, rsp_id=0, rsp_last=1)
assert not shadow.full()
drive(ax_valid=1, ax_id=3, ax_dest=DEST_C, times=2)
assert shadow.in_flight(3) > 0, "freeing a slot must re-admit the idle ID"
idle(2)
drain_all()
idle(2)

# ---- Phase H: simultaneous push and pop ----------------------------------
# On the *same* counter, `{push_en, inject_en, pop_en} == 3'b101` falls through
# to the default arm, so the counter must hold rather than move.
drive(ax_valid=1, ax_id=0, ax_dest=DEST_A)
before = shadow.in_flight(0)
drive(ax_valid=1, ax_id=0, ax_dest=DEST_A, rsp_valid=1, rsp_id=0, rsp_last=1,
      times=3)
assert shadow.in_flight(0) == before, "push and pop together must hold"
idle(2)
# On *different* counters in the same cycle, both must move.
drive(ax_valid=1, ax_id=1, ax_dest=DEST_A, rsp_valid=1, rsp_id=0, rsp_last=1)
assert shadow.in_flight(1) == 1 and shadow.in_flight(0) == before - 1
idle(2)
drain_all()
idle(2)

# ---- Phase I: pseudo-random mix ------------------------------------------
# A deterministic LCG, so the vector is reproducible without numpy. This is
# coverage padding on top of the directed phases, not a substitute for them.
state = 0x1234_5678


def next_random(modulus):
    global state
    state = (1103515245 * state + 12345) & 0x7FFF_FFFF
    return (state >> 16) % modulus


for _ in range(48):
    ax_valid = next_random(4) != 0
    ax_id = next_random(NUM_IDS)
    ax_dest = [DEST_A, DEST_B, DEST_C][next_random(3)]
    ax_ready = next_random(5) != 0
    rsp_last = next_random(4) != 0
    rsp_ready = next_random(5) != 0

    # Only answer an ID that really has something outstanding. When `rsp_last`
    # or `rsp_ready` is low the response cannot pop, so any ID is legal then.
    candidates = [i for i in range(NUM_IDS) if shadow.in_flight(i) > 0]
    if rsp_last and rsp_ready:
        rsp_valid = bool(candidates) and next_random(3) != 0
        rsp_id = candidates[next_random(len(candidates))] if candidates else 0
    else:
        rsp_valid = next_random(3) != 0
        rsp_id = next_random(NUM_IDS)

    drive(ax_valid=int(ax_valid), ax_id=ax_id, ax_dest=ax_dest,
          ax_ready=int(ax_ready), rsp_valid=int(rsp_valid), rsp_id=rsp_id,
          rsp_last=int(rsp_last), rsp_ready=int(rsp_ready))

drain_all()
idle(2)

assert all(shadow.in_flight(i) == 0 for i in range(NUM_IDS)), \
    "the run must end with every counter drained"


def main():
    if len(sys.argv) != 2:
        print(f"usage: {sys.argv[0]} <output.csv>", file=sys.stderr)
        return 2
    with open(sys.argv[1], "w", encoding="utf-8") as output:
        output.write(HEADER + "\n")
        for row in rows:
            output.write(",".join(str(field) for field in row) + "\n")
    print(f"wrote {len(rows)} stimulus cycles to {sys.argv[1]}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
