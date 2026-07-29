#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
#
# Generates the shared stimulus for the **inter-node** cross-check: a grid of
# `floo_axi_router` against the model's two `floo_mesh` instances.
#
# One line per node per cycle, so the grid size is a parameter rather than a
# column count. The testbench reads `NumX * NumY` lines per cycle and asserts
# the node order.
#
# Fully open loop: it drives every local endpoint's inject and eject signals
# and predicts nothing.
#
# Phases are directed because the interesting mesh behaviour is not reachable
# by chance:
#
#   * XY routing means a flit turns at most once, so a destination that
#     differs in both axes is the only way to exercise the turn;
#   * a multi-flit write packet (AW then W with `last`) is what holds a route
#     across several routers, and a competing packet aimed at the same output
#     is what proves it holds;
#   * the `req` and `rsp` networks are independent, so traffic on one must not
#     move anything on the other. Driving both at once is the only way to see
#     that.

import sys

NUM_X = 3
NUM_Y = 3
NUM_NODES = NUM_X * NUM_Y

HEADER = ("cycle,node,rst_n,"
          "rq_valid,rq_ch,rq_dst_x,rq_dst_y,rq_last,rq_tag,rq_ej_ready,"
          "rs_valid,rs_dst_x,rs_dst_y,rs_tag,rs_ej_ready")

CH_AW = 0
CH_W = 1
CH_AR = 2

rows = []
cycle_count = 0


def coord(node):
    return node % NUM_X, node // NUM_X


def node_of(x, y):
    return y * NUM_X + x


def emit(rst_n=1, per_node=None):
    """One cycle. `per_node` maps a node index to its driven fields."""
    global cycle_count
    per_node = per_node or {}
    for node in range(NUM_NODES):
        spec = per_node.get(node, {})
        rows.append((
            cycle_count, node, rst_n,
            spec.get("rq_valid", 0), spec.get("rq_ch", CH_AR),
            spec.get("rq_dst_x", 0), spec.get("rq_dst_y", 0),
            spec.get("rq_last", 1), spec.get("rq_tag", 0),
            spec.get("rq_ej_ready", 1),
            spec.get("rs_valid", 0),
            spec.get("rs_dst_x", 0), spec.get("rs_dst_y", 0),
            spec.get("rs_tag", 0), spec.get("rs_ej_ready", 1),
        ))
    cycle_count += 1


def idle(times=1):
    for _ in range(times):
        emit()


def read_to(source, dst_x, dst_y, tag, **extra):
    spec = {"rq_valid": 1, "rq_ch": CH_AR, "rq_dst_x": dst_x,
            "rq_dst_y": dst_y, "rq_last": 1, "rq_tag": tag}
    spec.update(extra)
    return {source: spec}


# ---- Phase 0: reset ------------------------------------------------------
for _ in range(4):
    emit(rst_n=0)
idle(2)

# ---- Phase A: a single hop east ------------------------------------------
emit(per_node=read_to(node_of(0, 0), 1, 0, 0x0A))
idle(6)

# ---- Phase B: a route that turns -----------------------------------------
# (0,0) to (2,2) differs in both axes. XY routing sends it east first, then
# north, so it turns exactly once at (2,0).
emit(per_node=read_to(node_of(0, 0), 2, 2, 0x0B))
idle(8)

# ---- Phase C: a multi-flit write packet holding a route ------------------
# AW with `last = 0` opens the packet, W with `last = 1` closes it. Between
# them the route is locked at every router the packet occupies.
emit(per_node={node_of(0, 1): {"rq_valid": 1, "rq_ch": CH_AW, "rq_dst_x": 2,
                               "rq_dst_y": 1, "rq_last": 0, "rq_tag": 0xC0}})
emit(per_node={node_of(0, 1): {"rq_valid": 1, "rq_ch": CH_W, "rq_dst_x": 2,
                               "rq_dst_y": 1, "rq_last": 0, "rq_tag": 0xC1}})
emit(per_node={node_of(0, 1): {"rq_valid": 1, "rq_ch": CH_W, "rq_dst_x": 2,
                               "rq_dst_y": 1, "rq_last": 1, "rq_tag": 0xC2}})
idle(8)

# ---- Phase D: a competing packet aimed at the same output ---------------
# (0,1) opens a write packet towards (2,1) while (1,0) sends a read that has
# to traverse (1,1)'s east output as well. The second must wait for the first
# packet's `last`.
emit(per_node={
    node_of(0, 1): {"rq_valid": 1, "rq_ch": CH_AW, "rq_dst_x": 2,
                    "rq_dst_y": 1, "rq_last": 0, "rq_tag": 0xD0},
})
for beat in range(3):
    spec = {
        node_of(0, 1): {"rq_valid": 1, "rq_ch": CH_W, "rq_dst_x": 2,
                        "rq_dst_y": 1, "rq_last": 1 if beat == 2 else 0,
                        "rq_tag": 0xD1 + beat},
    }
    spec.update(read_to(node_of(1, 1), 2, 1, 0xD8 + beat))
    emit(per_node=spec)
idle(8)

# ---- Phase E: eject back-pressure ---------------------------------------
# The destination refuses to accept, so the flit has to sit in the network.
emit(per_node=read_to(node_of(0, 0), 2, 0, 0xE0))
for _ in range(4):
    emit(per_node={node_of(2, 0): {"rq_ej_ready": 0}})
idle(6)

# ---- Phase F: both networks busy at once --------------------------------
# The `req` and `rsp` meshes are separate router instances, so traffic on one
# must move nothing on the other.
for step in range(4):
    spec = {}
    spec.update(read_to(node_of(0, 0), 2, 2, 0xF0 + step))
    spec[node_of(2, 2)] = {"rs_valid": 1, "rs_dst_x": 0, "rs_dst_y": 0,
                           "rs_tag": step % 8}
    emit(per_node=spec)
idle(10)

# ---- Phase G: every node injects at once --------------------------------
# Maximum contention: all nine endpoints inject towards the same corner.
for step in range(4):
    spec = {}
    for node in range(NUM_NODES):
        x, y = coord(node)
        if (x, y) == (2, 2):
            continue
        spec[node] = {"rq_valid": 1, "rq_ch": CH_AR, "rq_dst_x": 2,
                      "rq_dst_y": 2, "rq_last": 1,
                      "rq_tag": 0x100 + node * 8 + step}
    emit(per_node=spec)
idle(16)

# ---- Phase H: pseudo-random mix -----------------------------------------
# **Single-flit packets only.** Injection here is open loop: the generator
# writes `valid` without knowing whether the port was ready that cycle. A
# multi-flit packet whose closing `last` flit lands on a refused cycle is
# truncated, and the half-open packet holds its route in every router it
# occupies — permanently. An earlier version of this phase did exactly that and
# deadlocked all nine nodes by cycle 139 of 208, so the whole tail of the run
# was comparing a dead network. Multi-flit packets stay in the directed phases
# above, where the network is lightly loaded and every injection is accepted.
state = 0x0C0F_FEE0


def next_random(modulus):
    global state
    state = (1103515245 * state + 12345) & 0x7FFF_FFFF
    return (state >> 16) % modulus


for step in range(60):
    spec = {}
    for node in range(NUM_NODES):
        entry = {}
        own_x, own_y = coord(node)
        if next_random(3) != 0:
            # Never address the injecting node itself. `NoLoopback` defaults to
            # 1, so `floo_router` ties the Eject-input to Eject-output crossbar
            # leg to zero: a self-addressed flit is undeliverable and wedges
            # that node's input FIFO permanently. An earlier version of this
            # phase did that and every node was stuck by cycle 125 of 207.
            dst_x, dst_y = own_x, own_y
            while (dst_x, dst_y) == (own_x, own_y):
                dst_x = next_random(NUM_X)
                dst_y = next_random(NUM_Y)
            entry = {"rq_valid": 1, "rq_ch": CH_AR,
                     "rq_dst_x": dst_x, "rq_dst_y": dst_y, "rq_last": 1,
                     "rq_tag": 0x400 + node * 16 + step % 16}
        # Eject back-pressure is kept short so the network always drains.
        entry["rq_ej_ready"] = 1 if next_random(6) != 0 else 0
        entry["rs_ej_ready"] = 1 if next_random(6) != 0 else 0
        if next_random(2) == 0:
            rs_x, rs_y = own_x, own_y
            while (rs_x, rs_y) == (own_x, own_y):
                rs_x = next_random(NUM_X)
                rs_y = next_random(NUM_Y)
            entry.update({"rs_valid": 1, "rs_dst_x": rs_x, "rs_dst_y": rs_y,
                          "rs_tag": next_random(8)})
        spec[node] = entry
    emit(per_node=spec)
idle(24)

# ---- Phase I: a destination outside the mesh ----------------------------
# Every edge output is tied off, so its `ready` is unobservable as long as all
# traffic is addressed inside the grid — a negative control proved that by
# passing when the edge `ready` was inverted. XY routing sends a flit addressed
# beyond the east edge out of the mesh, where it wedges against the tie-off.
# Both sides must wedge identically. Kept last, because the stuck flit blocks
# that row for the rest of the run.
# Enough flits to fill the path and push back-pressure all the way to the
# injecting endpoint: the input spill register holds two, the output one holds
# two more, so a single flit would wedge invisibly.
for beat in range(8):
    emit(per_node=read_to(node_of(2, 1), NUM_X, 1, 0x7A + beat))
idle(6)
for beat in range(8):
    emit(per_node=read_to(node_of(1, 1), NUM_X, 1, 0x8A + beat))
idle(12)


def main():
    if len(sys.argv) != 2:
        print(f"usage: {sys.argv[0]} <output.csv>", file=sys.stderr)
        return 2
    with open(sys.argv[1], "w", encoding="utf-8") as output:
        output.write(HEADER + "\n")
        for row in rows:
            output.write(",".join(str(field) for field in row) + "\n")
    print(f"wrote {cycle_count} cycles x {NUM_NODES} nodes "
          f"= {len(rows)} stimulus lines to {sys.argv[1]}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
