#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
#
# Generates the shared stimulus for the chimney **manager-side response**
# cross-check — Step A-1, the fourth and last chimney quadrant.
#
# Why this harness has to exist
# -----------------------------
#
# The three signed chimney cross-checks all hold the manager response link
# idle. `tb_floo_axi_chimney_rsp_timing_trace.sv` says so literally:
#
#     floo_rsp_in.valid = 1'b0;
#
# So `floo_rsp_i` has never been driven against the RTL, and everything
# downstream of it — the B/R channel decode, `floo_rsp_o.ready`, and the
# reorder-buffer counter release — is unproven. That is exactly where the
# `rlast-ignored` defect lived: the model popped the R counter on *every* beat
# of a read burst instead of only on `RLAST`, and no existing harness could see
# it, because none of them ever sent an R beat.
#
# What this stimulus drives
# -------------------------
#
#   * a **push phase** that issues AR and AW so the per-id counters are
#     non-zero before any response arrives. Popping an empty counter would
#     underflow `delta_counter` and the comparison would be measuring a harness
#     bug rather than the model;
#   * a **multi-beat R burst**, `rsp_last` low on every beat but the last. This
#     is the case the whole step exists for: the counter must fall exactly once,
#     on the final beat;
#   * **full 64-bit `RDATA`**, including a vector whose upper half is
#     non-trivial. The first version of this file drove and traced only the low
#     32 bits, so corrupting the upper half was invisible and the cross-check
#     PASSed anyway — the payload was never signed, only its bottom half;
#   * **`BUSER` and `RUSER`**, with at least one response carrying `user = 1`.
#     They were absent from the first version entirely. `AxiCfg.UserWidth` is 1
#     in `hw/test/floo_test_pkg.sv`, so one bit is the whole field at these
#     parameters — that is coverage of the field, not of a wide user bus;
#   * `r_ready` and `b_ready` held low across a valid response, so
#     `floo_rsp_o.ready` falls and the flit is held rather than consumed;
#   * B and R responses interleaved on the shared `rsp` link;
#   * two AXI ids, so the per-id counter bank is addressed rather than a single
#     counter standing in for all of them.
#
# The transaction budget
# ----------------------
#
# Every `AxVALID` is a **single-cycle pulse**. An earlier version held it for
# six cycles, which — with `AxREADY` high — is six transactions, not one: id 1
# reached 12 outstanding reads against a comment claiming 2, and the counters
# never returned to zero. The trace still compared exactly, because both sides
# replayed the same vector, but it proved a different scenario than the one
# documented, and it never exercised a counter falling back to empty.
#
#   read  id 1  2 pushes  ->  2 pops   (a 4-beat burst and a 2-beat burst)
#   read  id 2  1 push    ->  1 pop
#   write id 1  1 push    ->  1 pop
#
# The runner asserts both halves directly: the observed **maxima** are exactly
# `(2, 1, 1)` and the **final** values are `(0, 0, 0)`. An earlier version instead
# tested `counter >= MaxTxnsPerId` and called that a wrap detector. It was not:
# `CounterWidth = $clog2(MaxTxnsPerId) = 5`, so `in_flight` is five bits and
# cannot reach 32 at all. The check could never fire, and an underflow from zero
# lands on 31 rather than anything above the limit. Checking the budget itself
# catches an underflow, an extra push and a missed pop alike.
#
# Fully open loop: inputs only, no prediction. Both sides replay this vector.
#
# Parameters match `hw/test/floo_test_pkg.sv`: InIdWidth 3, OutIdWidth 3,
# UserWidth 1, MaxTxns 32, MaxTxnsPerId 32, and the XY address fields at bit
# 16 (x), 20 (y). The chimney under test is at node (2,2); every request is
# addressed to (1,2).

import sys

HEADER = ("cycle,rst_n,aw_valid,aw_id,aw_addr,aw_len,w_valid,w_data,w_last,"
          "ar_valid,ar_id,ar_addr,ar_len,req_ready,"
          "rsp_valid,rsp_ch,rsp_id,rsp_data,rsp_resp,rsp_last,rsp_user,"
          "b_ready,r_ready")

# `floo_pkg::axi_ch_e`, and identically `floo::model::axi_channel`.
CH_B = 3
CH_R = 4

# `axi_pkg` response codes.
RESP_OKAY = 0
RESP_SLVERR = 2

NODE_X, NODE_Y = 2, 2
DEST_X, DEST_Y = 1, 2

rows = []


def drive(rst_n=1, aw_valid=0, aw_id=0, aw_addr=0, aw_len=0,
          w_valid=0, w_data=0, w_last=0,
          ar_valid=0, ar_id=0, ar_addr=0, ar_len=0,
          req_ready=1,
          rsp_valid=0, rsp_ch=CH_R, rsp_id=0, rsp_data=0,
          rsp_resp=RESP_OKAY, rsp_last=1, rsp_user=0,
          b_ready=1, r_ready=1, times=1):
    for _ in range(times):
        rows.append((len(rows), rst_n, aw_valid, aw_id, aw_addr, aw_len,
                     w_valid, w_data, w_last,
                     ar_valid, ar_id, ar_addr, ar_len, req_ready,
                     rsp_valid, rsp_ch, rsp_id, rsp_data, rsp_resp, rsp_last,
                     rsp_user, b_ready, r_ready))


def idle(times=1):
    drive(times=times)


def addr_of(x, y, offset=0):
    return (y << 20) | (x << 16) | offset


DEST = addr_of(DEST_X, DEST_Y)


def push_read(axi_id, length, offset):
    """One AR handshake. A single-cycle pulse is one transaction, not several."""
    drive(ar_valid=1, ar_id=axi_id, ar_addr=DEST | offset, ar_len=length)
    idle(3)


def push_write(axi_id, offset, beats=1):
    """One AW handshake, then its W burst.

    `AWLEN` encodes `beats - 1` and must agree with where `WLAST` falls. An
    earlier version left `AWLEN` at 0 while emitting two W beats: both sides
    handled it identically so the cross-check PASSed, but the stimulus was
    outside the AXI contract and would have misled anyone reusing it.
    """
    drive(aw_valid=1, aw_id=axi_id, aw_addr=DEST | offset, aw_len=beats - 1)
    idle(1)
    for beat in range(beats):
        drive(w_valid=1, w_data=0x1000 + beat,
              w_last=1 if beat == beats - 1 else 0)
        idle(1)
    idle(3)


def r_beat(axi_id, data, last, resp=RESP_OKAY, user=0, r_ready=1, times=1):
    drive(rsp_valid=1, rsp_ch=CH_R, rsp_id=axi_id, rsp_data=data,
          rsp_resp=resp, rsp_last=1 if last else 0, rsp_user=user,
          r_ready=r_ready, times=times)


def b_beat(axi_id, resp=RESP_OKAY, user=0, b_ready=1, times=1):
    drive(rsp_valid=1, rsp_ch=CH_B, rsp_id=axi_id, rsp_resp=resp,
          rsp_last=1, rsp_user=user, b_ready=b_ready, times=times)


# ---- reset -----------------------------------------------------------------
drive(rst_n=0, times=4)
idle(2)

# ---- push phase: exactly four transactions ---------------------------------
push_read(axi_id=1, length=3, offset=0x00)
push_read(axi_id=2, length=0, offset=0x40)
push_read(axi_id=1, length=1, offset=0x80)
push_write(axi_id=1, offset=0xC0, beats=2)
idle(4)

# ---- the case this step exists for: a multi-beat R burst -------------------
#
# Four beats, `rsp_last` only on the fourth. The R counter for id 1 must fall
# exactly once, on that beat — not four times.
#
# The data vectors exercise the full 64-bit bus: an upper half that is not a
# sign-extension of the lower, a value with only high bits set, and the
# all-ones pattern. Truncating `RDATA` to 32 bits shows up on the first of them.
r_beat(1, 0xDEADBEEF12345678, last=False)
r_beat(1, 0xFFFFFFFF00000000, last=False)
r_beat(1, 0x00000000FFFFFFFF, last=False, user=1)
r_beat(1, 0xFFFFFFFFFFFFFFFF, last=True)
idle(3)

# ---- back-pressure across a valid response --------------------------------
#
# `r_ready` low while a beat is valid: `floo_rsp_o.ready` must fall and the
# beat must not be consumed. Then released, and the burst completes.
r_beat(1, 0x0123456789ABCDEF, last=False, r_ready=0, times=3)
r_beat(1, 0x0123456789ABCDEF, last=False, r_ready=1)
idle(2)
r_beat(1, 0xFEDCBA9876543210, last=True, user=1, r_ready=0, times=2)
r_beat(1, 0xFEDCBA9876543210, last=True, user=1, r_ready=1)
idle(3)

# ---- B response, with back-pressure and a user bit ------------------------
b_beat(1, resp=RESP_OKAY, user=1, b_ready=0, times=2)
b_beat(1, resp=RESP_OKAY, user=1, b_ready=1)
idle(3)

# ---- the second id, and a non-OKAY response code ---------------------------
r_beat(2, 0x8000000000000001, last=True, resp=RESP_SLVERR)
idle(3)

# ---- the channel select, pinned in both directions -------------------------
#
# `floo_rsp_o.ready = axi_ready_out[hdr.axi_ch]`: the ready a flit sees is
# chosen by its own channel. Each case below holds the *other* channel's ready
# high while its own is low, so a B/R mix-up in the select shows up as a ready
# that should have fallen and did not.
#
# Neither response is consumed, by design — `r_ready`/`b_ready` stay low — so
# neither pops a counter. Every counter is already back at zero here, and a pop
# would underflow `delta_counter` and make the trace a record of a harness bug.
r_beat(1, 0xA5A5A5A5A5A5A5A5, last=True, r_ready=0, times=3)
idle(2)
b_beat(1, b_ready=0, times=3)
idle(4)

# ---- idle tail -------------------------------------------------------------
idle(6)


def main():
    path = sys.argv[1] if len(sys.argv) > 1 else "chimney_mgr_rsp_stimulus.csv"
    with open(path, "w") as handle:
        handle.write(HEADER + "\n")
        for row in rows:
            handle.write(",".join(str(field) for field in row) + "\n")
    print(f"{path}: {len(rows)} cycles, {len(HEADER.split(','))} fields")


if __name__ == "__main__":
    main()
