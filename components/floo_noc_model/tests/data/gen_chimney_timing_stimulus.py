#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
#
# Generates the shared stimulus for the chimney **request-path timing**
# cross-check.
#
# The two existing chimney cross-checks compare flit content: they issue one
# AXI beat at a time and drain the resulting flit before the next, so the
# emitted order follows the beat order and the chimney's internal arbiter never
# has to arbitrate. This stimulus does the opposite. It keeps AW, W, and AR
# contending for the single `req` link at the same time, and applies
# non-uniform back-pressure on that link, so what is compared is *when* each
# flit appears rather than only what it contains.
#
# Fully open loop: it drives inputs only and predicts nothing. Both sides
# replay the identical vector.
#
# Parameters match `hw/test/floo_test_pkg.sv`:
#   AxiCfg.InIdWidth = 3      -> eight AXI IDs
#   RouteCfg.UseIdTable = 0   -> the destination is decoded out of the address,
#                                x from bit 16 and y from bit 20, two bits each
#   ChimneyCfg.MaxTxnsPerId = 32 -> the reorder-buffer counters admit 31
#
# No responses are driven, so the reorder-buffer counters only ever fill. The
# generator therefore asserts a pessimistic bound: it counts every cycle a
# request is *offered* per ID, which is an upper bound on the accepts, and
# requires that to stay under the capacity. That keeps the run clear of the
# saturation corner, which Step 7 already signed on its own.

import sys

NUM_IDS = 8
CAPACITY = 31

HEADER = ("cycle,rst_n,aw_valid,aw_id,aw_addr,w_valid,w_last,w_data,"
          "ar_valid,ar_id,ar_addr,req_ready")


def addr_of(x, y, offset=0):
    """The address whose XY fields decode to (x, y)."""
    return (y << 20) | (x << 16) | offset


DEST_A = addr_of(1, 1, 0x40)
DEST_B = addr_of(2, 3, 0x80)
DEST_C = addr_of(3, 0, 0xC0)

rows = []
aw_offers = [0] * NUM_IDS
ar_offers = [0] * NUM_IDS


def drive(rst_n=1, aw_valid=0, aw_id=0, aw_addr=0, w_valid=0, w_last=0,
          w_data=0, ar_valid=0, ar_id=0, ar_addr=0, req_ready=1, times=1):
    for _ in range(times):
        if rst_n and aw_valid:
            aw_offers[aw_id] += 1
        if rst_n and ar_valid:
            ar_offers[ar_id] += 1
        rows.append((len(rows), rst_n, aw_valid, aw_id, aw_addr, w_valid,
                     w_last, w_data, ar_valid, ar_id, ar_addr, req_ready))


def idle(times=1):
    drive(times=times)


# ---- Phase 0: reset ------------------------------------------------------
drive(rst_n=0, times=4)
idle(2)

# ---- Phase A: one write, single-beat burst -------------------------------
# Exercises the `aw_w_sel_q` round trip SelAw -> SelW -> SelAw.
drive(aw_valid=1, aw_id=1, aw_addr=DEST_A)
drive(w_valid=1, w_last=1, w_data=0xA1)
idle(2)

# ---- Phase B: a write whose W burst is held, with a read contending ------
# The AW leaves with `hdr.last = 0`, so the wormhole arbiter locks onto the W
# slot and the AR *cannot* overtake it until the burst's last beat, even though
# the AR is continuously valid. This is the packet-coupling property at the
# link level, and it is exactly what a content-only harness cannot see.
drive(aw_valid=1, aw_id=2, aw_addr=DEST_B, ar_valid=1, ar_id=5, ar_addr=DEST_C)
drive(ar_valid=1, ar_id=5, ar_addr=DEST_C, times=2)          # W not yet valid
drive(w_valid=1, w_last=0, w_data=0xB0, ar_valid=1, ar_id=5, ar_addr=DEST_C)
drive(w_valid=1, w_last=0, w_data=0xB1, ar_valid=1, ar_id=5, ar_addr=DEST_C)
drive(w_valid=1, w_last=1, w_data=0xB2, ar_valid=1, ar_id=5, ar_addr=DEST_C)
drive(ar_valid=1, ar_id=5, ar_addr=DEST_C, times=2)          # now the AR wins
idle(2)

# ---- Phase C: AW and AR offered together from idle -----------------------
# Both request in the same cycle with the arbiter unlocked, so the round-robin
# priority decides. Repeated so the priority actually rotates.
for repeat in range(3):
    drive(aw_valid=1, aw_id=3, aw_addr=DEST_A,
          ar_valid=1, ar_id=6, ar_addr=DEST_B)
    drive(w_valid=1, w_last=1, w_data=0xC0 + repeat,
          ar_valid=1, ar_id=6, ar_addr=DEST_B)
    idle(1)
idle(2)

# ---- Phase D: link back-pressure mid-burst -------------------------------
# `req_ready` drops in the middle of a wormhole packet. The arbiter must hold
# its selection and the AXI-side `w_ready` must fall with it.
drive(aw_valid=1, aw_id=4, aw_addr=DEST_C)
drive(w_valid=1, w_last=0, w_data=0xD0)
drive(w_valid=1, w_last=0, w_data=0xD1, req_ready=0, times=3)
drive(w_valid=1, w_last=0, w_data=0xD1)
drive(w_valid=1, w_last=1, w_data=0xD2, req_ready=0, times=2)
drive(w_valid=1, w_last=1, w_data=0xD2)
idle(2)

# ---- Phase E: back-to-back reads with intermittent back-pressure ---------
for beat in range(6):
    drive(ar_valid=1, ar_id=beat % 4, ar_addr=DEST_A,
          req_ready=0 if beat % 3 == 2 else 1)
idle(2)

# ---- Phase F: AW accepted, then the W stream stalls ----------------------
# While `aw_w_sel_q == SelW` and `w_valid` is low the W slot does not request,
# but the arbiter is still locked on it, so a valid AR must wait.
drive(aw_valid=1, aw_id=7, aw_addr=DEST_B)
drive(ar_valid=1, ar_id=7, ar_addr=DEST_B, times=4)          # W silent
drive(w_valid=1, w_last=1, w_data=0xF0, ar_valid=1, ar_id=7, ar_addr=DEST_B)
drive(ar_valid=1, ar_id=7, ar_addr=DEST_B, times=2)
idle(2)

# ---- Phase G: reorder-buffer stall inside the arbitration ----------------
# ID 0 is sent to one destination and then, while outstanding, to another. The
# `NoRoB` gate refuses the second, so the AW slot stops requesting and the AR
# takes the link. This is the ordering rule and the arbiter interacting.
drive(aw_valid=1, aw_id=0, aw_addr=DEST_A)
drive(w_valid=1, w_last=1, w_data=0x01)
drive(aw_valid=1, aw_id=0, aw_addr=DEST_C,
      ar_valid=1, ar_id=1, ar_addr=DEST_A, times=4)
idle(2)

# ---- Phase H: pseudo-random mix -----------------------------------------
# Deterministic LCG so the vector is reproducible without numpy.
state = 0x0BADC0DE


def next_random(modulus):
    global state
    state = (1103515245 * state + 12345) & 0x7FFF_FFFF
    return (state >> 16) % modulus


in_burst = False
for _ in range(70):
    aw_valid = (not in_burst) and next_random(3) != 0
    w_valid = next_random(3) != 0
    w_last = next_random(3) == 0
    ar_valid = next_random(2) != 0
    req_ready = next_random(4) != 0

    if aw_valid:
        in_burst = True
    if w_valid and w_last and req_ready:
        in_burst = False

    drive(aw_valid=int(aw_valid), aw_id=next_random(NUM_IDS),
          aw_addr=[DEST_A, DEST_B, DEST_C][next_random(3)],
          w_valid=int(w_valid), w_last=int(w_last), w_data=next_random(256),
          ar_valid=int(ar_valid), ar_id=next_random(NUM_IDS),
          ar_addr=[DEST_A, DEST_B, DEST_C][next_random(3)],
          req_ready=int(req_ready))

idle(3)

for axi_id in range(NUM_IDS):
    assert aw_offers[axi_id] < CAPACITY, (
        f"AW ID {axi_id} offered {aw_offers[axi_id]} times, which could "
        f"saturate the reorder-buffer counter")
    assert ar_offers[axi_id] < CAPACITY, (
        f"AR ID {axi_id} offered {ar_offers[axi_id]} times, which could "
        f"saturate the reorder-buffer counter")


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
