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
#   * `r_ready` and `b_ready` held low across a valid response, so
#     `floo_rsp_o.ready` falls and the flit is held rather than consumed;
#   * B and R responses interleaved on the shared `rsp` link;
#   * two AXI ids, so the per-id counter bank is addressed rather than a single
#     counter standing in for all of them.
#
# Pops are budgeted against pushes: id 1 is pushed twice on the read side and
# popped twice, id 2 once and once, and the write side once each way. The trace
# carries the counters, so an underflow would show as a wrapped value rather
# than pass silently.
#
# Fully open loop: inputs only, no prediction. Both sides replay this vector.
#
# Parameters match `hw/test/floo_test_pkg.sv`: InIdWidth 3, OutIdWidth 3,
# MaxTxns 32, MaxTxnsPerId 32, and the XY address fields at bit 16 (x),
# 20 (y). The chimney under test is at node (2,2); every request is addressed
# to (1,2).

import sys

HEADER = ("cycle,rst_n,aw_valid,aw_id,aw_addr,w_valid,w_data,w_last,"
          "ar_valid,ar_id,ar_addr,ar_len,req_ready,"
          "rsp_valid,rsp_ch,rsp_id,rsp_data,rsp_resp,rsp_last,"
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


def drive(rst_n=1, aw_valid=0, aw_id=0, aw_addr=0,
          w_valid=0, w_data=0, w_last=0,
          ar_valid=0, ar_id=0, ar_addr=0, ar_len=0,
          req_ready=1,
          rsp_valid=0, rsp_ch=CH_R, rsp_id=0, rsp_data=0,
          rsp_resp=RESP_OKAY, rsp_last=1,
          b_ready=1, r_ready=1, times=1):
    for _ in range(times):
        rows.append((len(rows), rst_n, aw_valid, aw_id, aw_addr,
                     w_valid, w_data, w_last,
                     ar_valid, ar_id, ar_addr, ar_len, req_ready,
                     rsp_valid, rsp_ch, rsp_id, rsp_data, rsp_resp, rsp_last,
                     b_ready, r_ready))


def idle(times=1):
    drive(times=times)


def addr_of(x, y, offset=0):
    return (y << 20) | (x << 16) | offset


DEST = addr_of(DEST_X, DEST_Y)


def push_read(axi_id, length, offset, hold=6):
    """Offer an AR long enough for the arbiter to grant it."""
    drive(ar_valid=1, ar_id=axi_id, ar_addr=DEST | offset, ar_len=length,
          times=hold)
    idle(2)


def push_write(axi_id, offset, beats=1, hold=8):
    """Offer an AW, then its W burst. One packet, `w_last` on the final beat."""
    drive(aw_valid=1, aw_id=axi_id, aw_addr=DEST | offset, times=hold)
    for beat in range(beats):
        drive(w_valid=1, w_data=0x1000 + beat,
              w_last=1 if beat == beats - 1 else 0, times=3)
    idle(2)


def r_beat(axi_id, data, last, resp=RESP_OKAY, r_ready=1, times=1):
    drive(rsp_valid=1, rsp_ch=CH_R, rsp_id=axi_id, rsp_data=data,
          rsp_resp=resp, rsp_last=1 if last else 0,
          r_ready=r_ready, times=times)


def b_beat(axi_id, resp=RESP_OKAY, b_ready=1, times=1):
    drive(rsp_valid=1, rsp_ch=CH_B, rsp_id=axi_id, rsp_resp=resp,
          rsp_last=1, b_ready=b_ready, times=times)


# ---- reset -----------------------------------------------------------------
drive(rst_n=0, times=4)
idle(2)

# ---- push phase: make the counters non-zero before anything is popped ------
#
# Read side: id 1 twice (a 4-beat burst and a 2-beat burst), id 2 once.
# Write side: id 1 once.
push_read(axi_id=1, length=3, offset=0x00)
push_read(axi_id=2, length=0, offset=0x40)
push_read(axi_id=1, length=1, offset=0x80)
push_write(axi_id=1, offset=0xC0, beats=2)
idle(4)

# ---- the case this step exists for: a multi-beat R burst -------------------
#
# Four beats, `rsp_last` only on the fourth. The R counter for id 1 must fall
# exactly once, on that beat — not four times.
r_beat(1, 0x2000, last=False)
r_beat(1, 0x2001, last=False)
r_beat(1, 0x2002, last=False)
r_beat(1, 0x2003, last=True)
idle(3)

# ---- back-pressure across a valid response --------------------------------
#
# `r_ready` low while a beat is valid: `floo_rsp_o.ready` must fall and the
# beat must not be consumed. Then released, and the burst completes.
r_beat(1, 0x3000, last=False, r_ready=0, times=3)
r_beat(1, 0x3000, last=False, r_ready=1)
idle(2)
r_beat(1, 0x3001, last=True, r_ready=0, times=2)
r_beat(1, 0x3001, last=True, r_ready=1)
idle(3)

# ---- B response, with back-pressure ----------------------------------------
b_beat(1, resp=RESP_OKAY, b_ready=0, times=2)
b_beat(1, resp=RESP_OKAY, b_ready=1)
idle(3)

# ---- the second id, and a non-OKAY response code ---------------------------
r_beat(2, 0x4000, last=True, resp=RESP_SLVERR)
idle(3)

# ---- the channel select, pinned in both directions -------------------------
#
# `floo_rsp_o.ready = axi_ready_out[hdr.axi_ch]`: the ready a flit sees is
# chosen by its own channel. Each case below holds the *other* channel's ready
# high while its own is low, so a B/R mix-up in the select shows up as a ready
# that should have fallen and did not.
#
# Neither response is consumed, by design — `r_ready`/`b_ready` stay low — so
# neither pops a counter. That matters: the B counter is pushed once by the
# write above and popped once already, and a second pop here would underflow
# `delta_counter` and make the trace a record of a harness bug.
r_beat(1, 0x5000, last=True, r_ready=0, times=3)
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
