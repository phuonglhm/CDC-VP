#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
#
# Generates the shared stimulus for the chimney **response-path and
# subordinate-side timing** cross-check.
#
# `run_chimney_rsp_crosscheck.sh` already compares response flit *content*: it
# drives requests in, answers them, and checks the emitted B/R flits. What it
# never exercises is contention and back-pressure. This stimulus does:
#
#   * B and R answers offered in the same cycle, so the response wormhole
#     arbiter actually arbitrates;
#   * back-pressure on the outgoing `rsp` link;
#   * back-pressure on `axi_out`'s AW, which is the one place in the frozen
#     configuration where a spill register is *not* bypassed
#     (`i_aw_out_queue` is unconditional);
#   * requests offered while the metadata FIFOs still hold entries, so their
#     `full` back-pressure onto the inbound `req` link is reachable.
#
# Fully open loop: inputs only, no prediction. Both sides replay the identical
# vector.
#
# Parameters match `hw/test/floo_test_pkg.sv`: InIdWidth 3, OutIdWidth 3,
# MaxTxns 32, and the XY address fields at bit 16 (x) and 20 (y).

import sys

HEADER = ("cycle,rst_n,req_valid,req_ch,req_id,req_addr,req_last,req_data,"
          "req_src_x,req_src_y,aw_ready,w_ready,ar_ready,"
          "b_valid,b_id,b_resp,r_valid,r_id,r_data,r_resp,r_last,rsp_ready")

# Channel codes, `floo_pkg::axi_ch_e`.
CH_AW = 0
CH_W = 1
CH_AR = 2

rows = []


def drive(rst_n=1, req_valid=0, req_ch=CH_AW, req_id=0, req_addr=0,
          req_last=0, req_data=0, req_src_x=1, req_src_y=2,
          aw_ready=1, w_ready=1, ar_ready=1,
          b_valid=0, b_id=0, b_resp=0,
          r_valid=0, r_id=0, r_data=0, r_resp=0, r_last=1,
          rsp_ready=1, times=1):
    for _ in range(times):
        rows.append((len(rows), rst_n, req_valid, req_ch, req_id, req_addr,
                     req_last, req_data, req_src_x, req_src_y,
                     aw_ready, w_ready, ar_ready,
                     b_valid, b_id, b_resp,
                     r_valid, r_id, r_data, r_resp, r_last, rsp_ready))


def idle(times=1):
    drive(times=times)


def addr_of(x, y, offset=0):
    return (y << 20) | (x << 16) | offset


# ---- Phase 0: reset ------------------------------------------------------
drive(rst_n=0, times=4)
idle(2)

# ---- Phase A: one write request, then its B answer -----------------------
drive(req_valid=1, req_ch=CH_AW, req_id=3, req_addr=addr_of(1, 1, 0x40))
drive(req_valid=1, req_ch=CH_W, req_last=1, req_data=0xA5)
idle(2)
drive(b_valid=1, b_id=7, b_resp=0)
idle(2)

# ---- Phase B: one read request, then its R answer ------------------------
drive(req_valid=1, req_ch=CH_AR, req_id=5, req_addr=addr_of(2, 0, 0x80),
      req_src_x=3, req_src_y=1)
idle(2)
drive(r_valid=1, r_id=7, r_data=0x1234, r_last=1)
idle(2)

# ---- Phase C: B and R answers contending for the `rsp` link --------------
# Two requests in first so both metadata FIFOs have an entry, then answer both
# in the same cycle. The response arbiter has to choose, and its round-robin
# priority is what decides. A content-only harness never reaches this.
drive(req_valid=1, req_ch=CH_AW, req_id=1, req_addr=addr_of(0, 3))
drive(req_valid=1, req_ch=CH_W, req_last=1, req_data=0xB1)
drive(req_valid=1, req_ch=CH_AR, req_id=2, req_addr=addr_of(3, 3),
      req_src_x=0, req_src_y=0)
idle(1)
drive(b_valid=1, b_id=7, r_valid=1, r_id=7, r_data=0xB2, r_last=1, times=4)
idle(2)

# ---- Phase D: back-pressure on the outgoing `rsp` link -------------------
drive(req_valid=1, req_ch=CH_AR, req_id=4, req_addr=addr_of(1, 0))
idle(1)
drive(r_valid=1, r_id=7, r_data=0xD0, r_last=1, rsp_ready=0, times=3)
drive(r_valid=1, r_id=7, r_data=0xD0, r_last=1, rsp_ready=1)
idle(2)

# ---- Phase E: back-pressure on `axi_out`'s AW ----------------------------
# `i_aw_out_queue` is the one spill register the frozen configuration does not
# bypass. Holding `aw_ready` low fills it, and the inbound link must stall
# once it cannot accept another AW.
drive(req_valid=1, req_ch=CH_AW, req_id=6, req_addr=addr_of(2, 2),
      aw_ready=0, times=4)
drive(req_valid=1, req_ch=CH_AW, req_id=6, req_addr=addr_of(2, 2), aw_ready=1)
drive(req_valid=1, req_ch=CH_W, req_last=1, req_data=0xE0)
idle(1)
drive(b_valid=1, b_id=7)
idle(2)

# ---- Phase F: several requests outstanding, answered later ---------------
# The metadata FIFOs hold more than one entry, so a response is matched against
# a head that is not the most recent push.
for index in range(3):
    drive(req_valid=1, req_ch=CH_AR, req_id=index, req_addr=addr_of(index, 1),
          req_src_x=index, req_src_y=3)
idle(2)
for index in range(3):
    drive(r_valid=1, r_id=7, r_data=0xF0 + index, r_last=1)
    idle(1)
idle(2)

# ---- Phase G: W back-pressure and an interleaved read --------------------
drive(req_valid=1, req_ch=CH_AW, req_id=2, req_addr=addr_of(3, 2))
drive(req_valid=1, req_ch=CH_W, req_last=0, req_data=0x10, w_ready=0, times=3)
drive(req_valid=1, req_ch=CH_W, req_last=0, req_data=0x10)
drive(req_valid=1, req_ch=CH_W, req_last=1, req_data=0x11)
drive(req_valid=1, req_ch=CH_AR, req_id=1, req_addr=addr_of(0, 1))
idle(1)
drive(b_valid=1, b_id=7, r_valid=1, r_id=7, r_data=0x12, r_last=1, times=3)
idle(2)

# ---- Phase I: multi-beat R burst ----------------------------------------
# `ar_no_atop_pop` requires `axi_rsp_o.r.last`, so intermediate beats of a
# burst must NOT release the metadata entry. Without this phase a model that
# pops on any R beat passes, which a negative control proved.
drive(req_valid=1, req_ch=CH_AR, req_id=1, req_addr=addr_of(1, 2),
      req_src_x=2, req_src_y=1)
idle(1)
drive(r_valid=1, r_id=7, r_data=0x90, r_last=0)
drive(r_valid=1, r_id=7, r_data=0x91, r_last=0)
drive(r_valid=1, r_id=7, r_data=0x92, r_last=0, rsp_ready=0, times=2)
drive(r_valid=1, r_id=7, r_data=0x92, r_last=0)
drive(r_valid=1, r_id=7, r_data=0x93, r_last=1)
idle(2)

# ---- Phase J: fill the write metadata FIFO ------------------------------
# `MaxTxns = 32`, and the FIFO's `full` is what back-pressures the inbound
# `req` link. Reaching it needs 32 AW flits admitted with no B answers. Without
# this phase a model that ignores `full` passes, which a negative control also
# proved.
drive(req_valid=1, req_ch=CH_AW, req_id=2, req_addr=addr_of(3, 1),
      req_src_x=1, req_src_y=1, times=34)
# The link must now be refusing; hold the request up against it.
drive(req_valid=1, req_ch=CH_AW, req_id=2, req_addr=addr_of(3, 1), times=3)
# Drain, which releases the back-pressure one entry at a time.
drive(b_valid=1, b_id=7, times=34)
idle(3)

# ---- Phase K: pseudo-random mix -----------------------------------------
state = 0x5EED_1234


def next_random(modulus):
    global state
    state = (1103515245 * state + 12345) & 0x7FFF_FFFF
    return (state >> 16) % modulus


# Track a conservative shadow only to keep responses plausible: never answer
# more than could have been admitted. It bounds the stimulus, it does not
# predict the trace.
admitted_writes = 0
admitted_reads = 0
in_burst = False
for _ in range(60):
    if in_burst:
        channel = CH_W
    else:
        channel = [CH_AW, CH_AR][next_random(2)]
    req_valid = next_random(4) != 0
    req_last = next_random(3) != 0

    if req_valid and channel == CH_AW:
        in_burst = True
        admitted_writes += 1
    if req_valid and channel == CH_W and req_last:
        in_burst = False
    if req_valid and channel == CH_AR:
        admitted_reads += 1

    b_valid = admitted_writes > 0 and next_random(3) == 0
    r_valid = admitted_reads > 0 and next_random(3) == 0
    if b_valid:
        admitted_writes -= 1
    if r_valid:
        admitted_reads -= 1

    drive(req_valid=int(req_valid), req_ch=channel,
          req_id=next_random(8), req_addr=addr_of(next_random(4), next_random(4)),
          req_last=int(req_last), req_data=next_random(256),
          req_src_x=next_random(4), req_src_y=next_random(4),
          aw_ready=int(next_random(4) != 0), w_ready=int(next_random(4) != 0),
          ar_ready=int(next_random(4) != 0),
          b_valid=int(b_valid), b_id=7,
          r_valid=int(r_valid), r_id=7, r_data=next_random(256),
          r_last=int(next_random(3) != 0),
          rsp_ready=int(next_random(4) != 0))

idle(4)


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
