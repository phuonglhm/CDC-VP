# Implementation status

## Completed

- P0 scope frozen against FlooNoC revision `9a6972a`.
- P1 timing-independent address-map and XY-path reference.
- P2 signal-safe coordinate, header, and flit types.
- P3 `stream_fifo_optimal_wrap` mirror (spill register / stream FIFO).
- P3 locked XY route selector.
- P3 wormhole arbiter over an `rr_arb_tree` mirror.
- P3 five-port XY router with input FIFOs and output arbitration.
- P4 abstract-endpoint rectangular mesh.
- P5 measured router counters over RTL-signed signals only.
- P6 single-AXI channel types and sizing, chimney flit assembly and
  destination decode, metadata retention, and abstract AXI endpoint
  transactors.
- P7 the `NoRoB` ordering rule, cycle cross-checked against the unmodified
  reorder-buffer wrapper.
- P8 the chimney request path composed at cycle granularity and cross-checked
  under contention and back-pressure.
- P9 separate `req` and `rsp` meshes with AXI transactors attached, running AXI
  end to end.
- P9.1 the chimney response path and subordinate side composed at cycle
  granularity and cross-checked under contention and back-pressure.
- P9.2 inter-node timing cross-checked against a grid of the real router, which
  found the model missing the output FIFO every generated router has.
- P7.6 common SystemC/SV trace format plus route-selector, input-FIFO,
  wormhole-arbiter, and five-port router RTL cross-checks.

Standalone verification:

| Test | Coverage |
|---|---|
| `test_reference_model` | Address boundaries/overlap and expected XY path |
| `test_stream_fifo` | Depth-2 spill and depth-4 FIFO branches: reset, fill, refused push at full, pointer wrap, drain |
| `test_xy_route_select` | XY ordering, local eject, route lock/release |
| `test_wormhole_arbiter` | Round-robin selection and packet lock at two routes |
| `test_floo_router` | Contention, output back-pressure, no packet interleave |
| `test_floo_mesh` | 2×2 multi-hop delivery, destination check, stable stall |
| `test_noc_counters` | Hand-derived accept/stall/high-water counts, per-port identities, conservation across a drained router |
| `test_axi_types` | Hand-computed AXI channel widths, channel-to-link mapping, reserved-bit padding, `OutIdWidth` independence |
| `test_axi_sizing_trace` | 8-configuration sizing table against the RTL-captured golden |
| `test_axi_chimney_pack` | Flit assembly per channel, both destination-decode modes, AW/W select FSM |
| `test_rob_order_gate` | The `NoRoB` admission arithmetic exposed to the transactors: destination stall, the `2**$clog2(MaxRoTxnsPerId) - 1` capacity, and the global `full_o` stalling an unrelated idle ID |
| `test_axi_endpoint` | Manager/subordinate composition: AW/W coupling, response routing to the requester, AXI ID restoration, ordering-gate release |
| `test_route_trace_sc` | Directed CSV trace and fixed expected route/lock result |
| `test_fifo_trace_sc_d2` | 133-cycle FIFO trace against the RTL-captured depth-2 golden |
| `test_fifo_trace_sc_d4` | 133-cycle FIFO trace against the RTL-captured depth-4 golden |
| `test_arbiter_trace_sc_n2` | 152-cycle arbiter trace against the RTL-captured 2-route golden |
| `test_arbiter_trace_sc_n4` | 152-cycle arbiter trace against the RTL-captured 4-route golden |
| `test_arbiter_trace_sc_n5` | 152-cycle arbiter trace against the RTL-captured 5-route golden |
| `test_chimney_req_trace_sc` | 16-flit request trace against the RTL-captured golden |
| `test_chimney_rsp_trace_sc` | 8-flit response trace against the RTL-captured golden |
| `test_rob_trace_sc` | 127-cycle ordering trace against the RTL-captured golden |
| `test_axi_noc` | AXI end to end over the two-network mesh: multi-hop delivery, AXI id restored across the NoC, structural latency bounds, and the ordering rule |
| `test_chimney_timing_trace_sc` | 141-cycle chimney request-path timing trace against the RTL-captured golden |
| `test_chimney_rsp_timing_trace_sc` | 221-cycle chimney response-path and subordinate-side timing trace against the RTL-captured golden |
| `test_mesh_trace_sc` | 1872 node-cycles of a 3x3 two-network mesh against the RTL-captured golden |
| `test_router_trace_sc_d2` / `_d0` | 214-cycle router trace against the RTL-captured golden, at output-FIFO depth 2 and 0 |
| `test_noc_interconnect` | The TLM wrapper contract: address decode to node, multi-initiator ownership, `AxSIZE` preservation, per-node hold-off, and the self-node placement guard |

All twenty-eight tests pass with GCC 11.5.0 and SystemC 2.3.4.

The wrapper covered by the last row has **no RTL counterpart**, so unlike every
other block on this page it is not signed and cannot be. It is the highest-risk
correctness layer in the component; see `AI_HANDOFF_CONTEXT.md` Step 10.3.

## Accuracy status

RTL-signed blocks:

| Block | RTL reference | Evidence |
|---|---|---|
| XY route selector | `hw/floo_route_select.sv` | 12 cycles exact (`route_sel_id_o`, lock state) |
| Input FIFO | `common_cells` `stream_fifo_optimal_wrap` | 133 cycles exact at depth 2 and depth 4 (pre-edge and post-edge `ready_o`/`valid_o`/`data_o`) |
| Wormhole arbiter | `hw/floo_wormhole_arbiter.sv` over `common_cells` `rr_arb_tree`/`lzc` | 152 cycles exact at 5, 4, and 2 routes (pre-edge and post-edge `ready_o`/`valid_o`/`data_o`/selected index, plus `valid_q`, `last_q`, `rr_q`, `lock_q`, `req_q`) |
| Five-port router | `hw/floo_router.sv` | 214 cycles exact at **both** `OutFifoDepth = 2` (what every generated router has) and `0` (the `gen_no_out_fifo` bypass): pre-edge and post-edge per-port `ready_o`/`valid_o` masks and per-output `data_o`, plus the one-hot route mask per input |
| AXI flit sizing | `floo_pkg` sizing functions over `axi 0.39.9` `axi_pkg` | 8 configurations exact (per-channel width, channel-to-link mapping, physical channel width, reserved bits) |
| Chimney request path | `hw/floo_axi_chimney.sv` in the `floo_test_pkg` parameter set | 16 flits exact (channel, destination, source, `last`, `atop`, `rob_req`, `rob_idx`, payload). Flit content and per-beat ordering only, not chimney timing |
| Chimney response path | same, driven as a subordinate | 8 flits exact, with three transactions outstanding per batch so the metadata FIFOs are genuinely exercised |
| `NoRoB` ordering rule | `hw/floo_rob_wrapper.sv` over the locked axi `axi_demux_id_counters` | 127 cycles exact (`ax_ready_o`/`ax_valid_o`/`rsp_*` per cycle, plus `in_flight`, `prev_dest`, `counter_full`, and every counter and destination register in the bank) |
| Chimney request-path **timing** | `hw/floo_axi_chimney.sv` in the `floo_test_pkg` parameter set | 141 cycles exact (`aw_ready`/`w_ready`/`ar_ready` and the `req` link's `valid`, channel, destination, `last` and payload per cycle, plus `aw_w_sel_q` and every request-arbiter register) |
| Inter-node mesh | a grid of `hw/floo_axi_router.sv`, wired as the FlooGen netlist wires it | 1872 node-cycles exact (per node and per network: local inject `ready`, eject `valid`, and the ejected flit's channel, destination, `last` and payload tag, pre-edge and post-edge) |
| Chimney response path and subordinate side, **timing** | same, driven from the `req` link | 221 cycles exact (inbound `req` `ready`, the reissued `axi_out` request boundary, the `axi_out` response `ready` signals, the `rsp` link's `valid`/channel/destination/id, plus both metadata FIFO occupancies and every response-arbiter register) |

Every timing path from an AXI manager port to an AXI subordinate port is now
RTL-signed: both chimney directions and the mesh between them. What remains
unsigned is the endpoint transactors, which have no RTL counterpart and cannot
be signed; they are a driver and collector built on signed rules, not part of
the modelled datapath.

### Corrected router crossbar (2026-07-28)

The router cross-check found one real model defect. `hw/floo_router.sv` ties
**both** the handshake and the data of an illegal input/output pair to zero:

```systemverilog
if ((NoLoopback && (in == out)) || (XYRouting && XYRouteOpt && ...)) begin
  assign masked_ready_transposed[in][v][out] = '0;
  assign masked_valid[out][v][in]            = '0;
  assign masked_data[out][v][in]             = '0;   // <- the model missed this
end
```

The model tied off only the handshake and still presented the routed flit on
every crossbar leg. That is observable, because `floo_wormhole_arbiter` drives
`data_o` from the selected index even when that index is not valid: an output
whose arbiter happens to select an illegal leg shows `'0` in the RTL and stale
flit data in the model. The fix mirrors the RTL tie-off in
`floo_router.hpp::connect_crossbar`.

Two model assumptions were confirmed against the RTL rather than assumed, and
both map onto real parameters: the loopback block is `NoLoopback` (default
`1'b1`), and the Y-to-X restriction is `XYRouteOpt` (default `1'b1`).

### Corrected FIFO semantics (2026-07-28)

The superseded `ready_valid_fifo` modeled an "optimal" FIFO that accepted a
push while full whenever its head was popped in the same cycle. The frozen RTL
does not behave that way in either branch of `stream_fifo_optimal_wrap`:

- depth 2 instantiates `spill_register_flushable`, whose
  `ready_o = !a_full_q || !b_full_q`;
- depth > 2 instantiates `stream_fifo`/`fifo_v3`, whose `ready_o = ~full`.

In both cases `ready_o` is a function of registers only, so a full buffer
refuses a push even while it pops. The old model therefore overstated input
buffer acceptance by one flit per stalled-then-released cycle and introduced a
combinational `ready_i -> ready_o` path that the RTL does not have. The model
now mirrors the RTL hierarchy in `include/floo_noc_model/stream_fifo.hpp`, and
the router instantiates `stream_fifo_optimal_wrap<FlitT, InFifoDepth>`.

Note that the frozen router uses `InFifoDepth = 2`, so the modeled input buffer
is a spill register, not a circular FIFO.

### Corrected arbiter semantics (2026-07-28)

The superseded wormhole arbiter searched the **live** `valid_i` inputs starting
from an explicit `rr_next_q` register and advanced that register to
`selected + 1` after each accepted `last` flit. The frozen RTL does none of
that. It arbitrates through `rr_arb_tree` with `LockIn = 1`, `FairArb = 1`,
`AxiVldRdy = 1`, granted only by `ready_i & last_out`. Four concrete
divergences were found and fixed:

| Aspect | Superseded model | Frozen RTL |
|---|---|---|
| Round-robin advance | `selected + 1` | `FairArb`: next *requesting* index above `rr_q`, via two `lzc` over masked requests |
| Arbitrated request set | live `valid_i` | `valid_q` snapshot, held by the tree's `LockIn` |
| `ready_o` | asserted only if the selected input is itself valid | asserted on the selected index whenever any input is valid |
| `data_o` when invalid | zeroed | always driven from `data_i[valid_selected_idx]` |

The model now mirrors the RTL hierarchy: `include/floo_noc_model/rr_arb_tree.hpp`
reproduces `rr_arb_tree`, `lzc` (`MODE = 0`), and `cf_math_pkg::idx_width`
structurally, and `wormhole_arbiter.hpp` reproduces the FlooNoC wrapper over it.
The data multiplexer and per-input grant decode of `rr_arb_tree` are not
modeled because the frozen instantiation leaves `data_o` and `gnt_o`
unconnected and drives `data_i` with `'0`.

`floo_wormhole_arbiter.sv` opens with `import floo_pkg::*;` but references no
symbol from it. The cross-check proves this by compiling it against an
intentionally empty `floo_pkg`, so no shim supplies behavior.

### Redundant hold mechanisms in the RTL

The wrapper's `valid_q` snapshot and the tree's `LockIn` implement the same
packet hold. Whenever `lock_q` is low, the reachable state guarantees
`valid_d == valid_i`, and whenever `lock_q` is high the tree ignores its
request input. Either mechanism alone reproduces the frozen behavior; removing
both does not. This was confirmed by negative control, not only by argument,
and it is why two of the six injected defects below are equivalent rewrites
rather than harness gaps.

### Flit header field set corrected (2026-07-28)

The audit had found the model's `flit_header` missing two fields that the
frozen `FLOO_TYPEDEF_HDR_T` defines. Both are now present, and the declaration
order follows the macro:

```text
rob_req, rob_idx, dst_id, collective_mask, src_id, last, atop, axi_ch,
collective_op
```

They stay inert in v0, which is unicast with `EnMultiCast = 0`, because
`floo_route_select.sv` only reads `collective_op` when `EnMultiCast` is set.
Adding them changed no cross-check result, which was verified by re-running all
of them.

### AXI sizing arithmetic (2026-07-28)

`include/floo_noc_model/axi_types.hpp` mirrors `floo_pkg::axi_cfg_t`, the
channel-to-link mapping, `get_axi_chan_width`, `get_max_axi_payload_bits`, and
`get_axi_rsvd_bits`, over the `axi_pkg` field width constants at the locked
revision.

This is arithmetic, not timing, so the cross-check evaluates both sides over a
configuration list rather than over cycles. It exists because two details are
easy to transcribe wrongly and would then be silently wrong everywhere
downstream:

- `get_max_axi_payload_bits` adds one spare bit, so a physical channel is
  always at least one bit wider than its widest payload;
- the channel widths use `cfg.InIdWidth`, never `OutIdWidth`.

Both were confirmed by negative control.

### Chimney request path (2026-07-28) — RTL cross-checked

`rtl_crosscheck/run_chimney_req_crosscheck.sh` drives a 16-beat AXI list into
the unmodified chimney and compares every emitted request flit:
**16 flits match exactly.**

Scope, stated precisely: this compares **flit content and per-beat ordering**.
The stimulus issues one beat at a time and the testbench drains each flit
before the next beat, so the order does not depend on the chimney's internal
request arbiter. Chimney timing, arbitration, back-pressure behaviour, and the
whole response path remain uncompared.

**The defect it found.** `hw/floo_rob_wrapper.sv` drives `ax_rob_req_o = 1'b1`
even in its `NoRoB` branch. "Reorder buffer disabled" therefore does **not**
mean `rob_req = 0` on the wire: every request flit carries `rob_req = 1` with
index 0. The model had assumed the intuitive reading and was wrong.

Three negative controls confirm the harness detects real divergence:

| Injected defect | Result |
|---|---|
| `rob_req` default back to false | FAIL from the first flit |
| AW terminates its own packet | FAIL on the `last` column |
| W decodes its own address instead of the latched AW destination | FAIL on the destination columns |

### Chimney response path (2026-07-28) — RTL cross-checked

`rtl_crosscheck/run_chimney_rsp_crosscheck.sh` drives request flits into
`floo_req_i`, lets the chimney reissue them on `axi_out`, answers there, and
compares every response flit on `floo_rsp_o`: **8 flits match exactly.**

This is what exercises `hw/floo_meta_buffer.sv`. Confirmed against the RTL:

- a response routes back to the requester's `src_id`;
- the manager's original AXI ID is restored into the B/R payload, replacing
  the chimney's downstream reissue ID;
- `hdr.last = 1` and `rob_req = 1` on every response flit;
- metadata is retained in order, separately per direction.

`include/floo_noc_model/meta_buffer.hpp` models the `MaxUniqueIds == 1` branch,
where the downstream reissue ID is the constant `'1` (7 at `OutIdWidth = 3`,
confirmed by probing `axi_out_req.ar.id`) and metadata sits in a plain in-order
FIFO. The `MaxUniqueIds > 1` branch keys an `id_queue` by the original AXI ID
and is deliberately not modeled.

**Coverage note, found by negative control.** With one transaction in flight
the metadata FIFOs never exceed one entry, and a control that swapped the read
and write buffers still passed. The stimulus therefore issues transactions in
batches of three before answering any of them. With that, all four controls
detect:

| Injected defect | Result |
|---|---|
| Original AXI ID not restored | FAIL on the response ID |
| Response routed to the chimney's own ID | FAIL on the destination |
| Read and write metadata buffers swapped | detected: the model underflows |
| Metadata popped LIFO instead of FIFO | FAIL from the fourth flit |

Not covered: `downstream_id()` is overwritten by the ID restoration before it
reaches the trace, so the comparison does not test it; it was confirmed
separately by probe. Multi-beat R bursts, ATOPs, and back-pressure on the
response link are untested.

### Harness lesson

Four separate defects in this harness came from mixing explicit `#delay` phase
arithmetic with sequential driving. The fix that worked was abandoning
`ApplTime`/`TestTime` arithmetic entirely for a synchronous idiom: assert with
a non-blocking assignment, sample the handshake at the clock edge. Two earlier
diagnoses based on phase reasoning were wrong and led to fixes in the wrong
place.

### Response ordering (2026-07-29) — RTL cross-checked

`include/floo_noc_model/rob_order_gate.hpp` models the `NoRoB` branch of
`hw/floo_rob_wrapper.sv`. `NoRoB` is not "no ordering logic": the RTL comment
and code make it an admission rule that stalls a transaction reusing an AXI ID
for a *different* destination until the previous ones complete. Both reorder
buffers are `NoRoB` in the frozen configuration, so this is the ordering
behaviour that v0 actually has. The reordering RoB types need a different
frozen configuration and are out of scope.

`rtl_crosscheck/run_rob_crosscheck.sh` instantiates the unmodified wrapper as a
leaf, drives it from the shared stimulus, and compares every cycle:
**127 cycles match.** Unlike the two chimney cross-checks, which compare flit
*content*, this one compares a handshake per cycle — `NoRoB` is an admission
decision, so `ax_ready_o` is its only boundary observable. The trace also
exports `in_flight`, `prev_dest`, `counter_full`, and the whole counter bank
through hierarchical references.

Instantiating the wrapper directly, rather than reaching it through the
chimney, keeps the comparison free of the chimney's arbitration and cuts, which
remain unsigned.

**Three counter-bank rules that a per-ID reading gets wrong.** All come from
`axi_demux_id_counters` in the locked axi `src/axi_demux_simple.sv`, and all
are now modelled and signed:

| Rule | The intuitive but wrong reading |
|---|---|
| `full_o = \|cnt_full` is a **global** OR across all `2**AxiIdBits` counters, so one saturated ID stalls *every* ID | a per-ID full signal |
| `cnt_full[i] = overflow \| (&in_flight)` saturates at `2**$clog2(MaxRoTxnsPerId) - 1`, so the default `MaxRoTxnsPerId = 32` admits **31**, not 32 | capacity equals `MaxRoTxnsPerId` |
| the counter pops by `rsp_i.id`, the ID carried by the *response* | pops by the request's ID |

Twelve negative controls were run against the harness and **all twelve are
detected**:

| Injected defect | First divergence |
|---|---|
| `full_o` made per-ID instead of the global OR | line 53 |
| capacity read as `MaxRoTxnsPerId` | line 50 |
| `prev_dest` comparison dropped | line 18 |
| `ax_ready_o` not qualified by `ax_ready_i` | line 40 |
| counter pushed without the request handshake | line 40 |
| pop ignores `rsp_last` | line 31 |
| pop ignores `rsp_ready` | line 32 |
| simultaneous push/pop counted as a push | line 68 |
| `prev_dest` cleared when the counter drains | line 11 |
| `ax_valid_o` driven from `ax_valid_i` instead of `push` | line 18 |
| pop keyed by `ax_id` instead of `rsp_id` | line 22 |
| `rob_req` driven low under `NoRoB` | line 2 |

A thirteenth control — rewriting the simultaneous push/pop arm as an
unconditional `+1` followed by an unconditional `-1` — passed, and was
**discarded rather than recorded as a gap**: on a wrapping counter those two
operations cancel exactly, so it injects no defect at all. It was replaced by
the "counted as a push" control above, which does.

The stimulus is directed, not random, because the interesting states are
unreachable by chance: a same-ID/different-destination stall needs the ID
outstanding at the moment the second destination is offered, and the global
nature of `full_o` only shows when a second, *completely idle* ID is offered
while the first is saturated. `tests/data/gen_rob_stimulus.py` carries a shadow
of the push/pop rule, used **only** to keep the response stream legal — the
counter bank carries an underflow assertion, and answering a transaction that
was never admitted would corrupt both sides rather than compare them.

### Chimney request-path timing (2026-07-29) — RTL cross-checked

`rtl_crosscheck/run_chimney_timing_crosscheck.sh` keeps AW, W, and AR
contending for the single `req` link and applies non-uniform back-pressure on
it, then compares every cycle: **141 cycles match.**

This is the composition step. Every part it wires together was already signed
on its own — the spill register (133 cycles), the wormhole arbiter at 2 routes
(152 cycles), the `NoRoB` gate (127 cycles), and flit assembly (16 + 8 flits).
What had never been checked is whether they are wired together correctly.

**The frozen configuration has no cuts.** `floo_pkg::ChimneyDefaultCfg` sets
`CutAx = 0`, `CutOup = 0`, and `CutRsp = 0`, and `hw/test/floo_test_pkg.sv`
takes the defaults unchanged. So `gen_no_ax_cuts` and `gen_no_rsp_cuts` wire
straight through, and `i_req_out_cut` is instantiated with
`Bypass = !CutOup = 1`, whose branch is `valid_o = valid_i`,
`ready_o = ready_i`, `data_o = data_i`. This **corrects the earlier roadmap
entry**, which assumed the chimney's latency came from cuts that had to be
modelled. It does not: in v0 the request path's only state is the `aw_w_sel_q`
FSM, the arbiter's registers, and the reorder-buffer counters.

**The defect it found: the request arbiter's index order is reversed.** The
chimney declares

```systemverilog
floo_req_chan_t [AxiW:AxiAr] floo_req_arb_in;
```

and `AxiW = 1`, `AxiAr = 2`, so that packed range is **ascending**, `[1:2]`. In
an ascending packed range the first index is the most significant element, so
connecting it to the arbiter's `data_i[NumRoutes-1:0]` puts the `AxiW` slot on
bit 1 and the `AxiAr` slot on bit **0** — the reverse of the declaration's
reading order. The model had W at index 0. Since the index decides round-robin
priority, this is observable whenever AW and AR contend, and it showed up in
the very first traced cycle. Verilator's `ASCRANGE` warning on that line is the
only hint the RTL gives.

Eleven negative controls were run and **all eleven are detected**:

| Injected defect | First divergence |
|---|---|
| Arbiter index order reverted (W at 0, AR at 1) | line 2 |
| `w_ready` not gated by `SelW` | line 8 |
| `aw_rob_ready_in` not gated by `SelAw` | line 12 |
| W slot requests on the RoB valid instead of `w_valid` | line 8 |
| W decodes its own destination instead of the latched AW one | line 9 |
| FSM enters `SelW` on `aw_valid` without the handshake | line 22 |
| FSM returns to `SelAw` on any accepted W, ignoring `last` | line 15 |
| `aw_ready` bypasses the reorder-buffer gate | line 76 |
| FSM reset to `SelW` | line 2 |
| W destination latched every cycle, not on AW acceptance | line 15 |
| AR `ready` taken from the arbiter, skipping the R reorder buffer | line 52 |

Two further controls passed and were **discarded as equivalent rewrites rather
than recorded as gaps**, each with the argument checked against the RTL:

- *swapping the order of the two `aw_w_sel_d` assignments.* They can never both
  fire: an AW acceptance needs `sel == SelAw` (through
  `aw_rob_ready_out = push && gnt[AxiW] && sel_aw`) and a W acceptance needs
  `sel == SelW` (`w_ready = gnt[AxiW] && !sel_aw`). Mutually exclusive, so the
  order is unobservable.
- *building the AW flit with the AR reorder tag.* Under `NoRoB` both reorder
  buffers drive `rob_req = 1'b1` and `rob_idx = '0` unconditionally, so the two
  tags are the same constant. Those fields are signed by the content
  cross-check, which does compare them.

**What this does not cover.** The response path's timing, the subordinate side
including `i_aw_out_queue` and the meta buffer, multi-beat R bursts, ATOPs, and
the saturation corner of the reorder-buffer counters. The response link is held
idle for this run, so the counters only fill; the stimulus generator bounds the
offers per ID below capacity, and the saturation behaviour is signed separately
by the ordering cross-check.

### Chimney response path and subordinate side, timing (2026-07-29) — RTL cross-checked

`rtl_crosscheck/run_chimney_rsp_timing_crosscheck.sh` drives request flits into
the `req` link, answers on `axi_out`, applies back-pressure on the outgoing
`rsp` link and on `axi_out`'s AW, and compares every cycle: **221 cycles
match.**

This closes the other half of the chimney. `include/floo_noc_model/axi_chimney.hpp`
now carries `axi_chimney_response` alongside `axi_chimney_request`.

**Two things here are not bypassed**, unlike the request path where
`CutAx = CutOup = CutRsp = 0` removed every cut:

- `i_aw_out_queue`, a `spill_register` between the meta buffer and
  `axi_out_req_o.aw`. It is **unconditional** — no `Cut*` parameter gates it.
  The RTL comment gives the reason: AW and W share one link, so a downstream
  module may refuse the AW until its W is valid.
- the metadata FIFOs, whose `full` back-pressures the inbound `req` link.

**The ascending-range trap again.** `floo_rsp_arb_in` is declared
`[AxiB:AxiR]`, and `AxiB = 3`, `AxiR = 4`, so index 0 is the **R** slot and
index 1 the **B** slot — the same reversal that was a real defect on the
request side. Modelled correctly this time because the request-path
cross-check had already exposed the pattern.

Also modelled: `floo_req_out_ready = axi_ready_out[hdr.axi_ch]`, so the inbound
link's `ready` is *selected by the channel the arriving flit names*, not a
single combined signal.

Eleven negative controls, **all eleven detected**:

| Injected defect | First divergence |
|---|---|
| Response-arbiter index order reverted (B at 0, R at 1) | line 2 |
| Inbound `ready` not selected by the flit channel | line 63 |
| Metadata `full` does not back-pressure the inbound link | line 115 |
| AW spill register bypassed | line 39 |
| Metadata pushed on valid alone, not on the handshake | line 174 |
| AR metadata pop ignores the response `last` | line 77 |
| B response id not restored to the manager's original | line 12 |
| Response routed to this node instead of the requester | line 12 |
| `axi_out` `b_ready` from the link instead of the arbiter grant | line 2 |
| Downstream reissue id 0 rather than all ones | line 2 |
| AW and AR metadata FIFOs swapped | line 8 |

**Two stimulus gaps found by those controls and closed.** The first run had
only nine of eleven detected, and neither miss was an equivalent rewrite:

- the metadata `full` control passed because `MaxTxns = 32` and the stimulus
  never accumulated 32 outstanding writes. Fixed by a phase that drives 34
  back-to-back AW flits with no B answers, holds a request against the
  resulting back-pressure, then drains.
- the `r.last` control passed because every R beat in the stimulus carried
  `last = 1`. Fixed by a multi-beat R burst, with link back-pressure inside
  it.

Both are worth recording as a pattern: a control that passes is either an
equivalent rewrite or an unreachable state, and the two need different fixes.
Three earlier passes were equivalences; these two were coverage.

### Two-network NoC and end-to-end AXI (2026-07-29)

`include/floo_noc_model/axi_noc.hpp` instantiates the `req` and `rsp` physical
channels as two separate meshes. That shape comes from the IP, not from a
modelling preference: `hw/floo_axi_router.sv` is literally two `floo_router`
instances with identical parameters, one per flit type, carrying two
independent `[NumRoutes-1:0]` port arrays with no shared arbitration, no shared
buffering, and no ordering between them.

**FlooGen 0.8.4 is now installed and the reference topology generated**, by
`rtl_crosscheck/install_floogen.sh`. It installs from the frozen tree rather
than PyPI, uses `python3.11` because FlooGen needs >= 3.10 and this host's
default `python3` is 3.9, writes only into a build directory, and fails if the
frozen checkout ends up modified.

The generated `floo_axi_mesh_noc.sv` confirms the model's port index order. For
`router_0_1` at (1,1):

```text
req_in[0] <- router_0_2   (y+1, North)
req_in[1] <- router_1_1   (x+1, East)
req_in[2] <- router_0_0   (y-1, South)
req_in[3] <- hbm_ni_1     (x-1 edge, the West slot)
req_in[4] <- cluster_ni   (Eject)
```

which matches `floo_pkg::route_direction_e` and `direction` in
`floo_types.hpp`. This is evidence by *reading* the generated netlist, not a
cycle comparison: a mesh-level cross-check needs the whole generated top
elaborated against the model, and that harness does not exist yet. Inter-node
timing therefore remains an estimate.

`test_axi_noc` runs AXI end to end on a 4x4 mesh and checks structural latency
bounds rather than exact numbers. Every router registers both its input and its
output (`InFifoDepth = 2`, `OutFifoDepth = 2`, both spill registers), so a
round trip cannot beat one cycle per hop each way. Measured: **1 hop 11 cycles,
6 hops 30 cycles**. Both controls run against it are detected — swapping the
address regions, and ejecting from the wrong network.

> These numbers replace the 7 and 16 recorded before the inter-node
> cross-check. They were measured against a router with no output FIFO, which
> is not a configuration FlooGen generates; see the section below.

### Inter-node timing (2026-07-29) — RTL cross-checked

`rtl_crosscheck/run_mesh_crosscheck.sh` builds a 3x3 grid of the unmodified
`hw/floo_axi_router.sv` and compares the model's two `floo_mesh` instances
against it per node and per cycle: **1872 node-cycles match.**

It deliberately does **not** go through the FlooGen-generated top. That module
exposes only AXI ports per endpoint, so comparing against it would need a full
chimney at every node and would fold chimney behaviour into a mesh measurement.
`floo_axi_router` has clean flit-level ports, so the grid is the isolatable
unit — the same reasoning that put the ordering cross-check on
`floo_rob_wrapper` rather than through the chimney. The generated netlist
remains the authority for the wiring rule, which the testbench reproduces.

**The defect it found: the model had no output FIFO at all.** Every FlooGen
router template hardcodes `.OutFifoDepth (2)`, and `hw/test/floo_test_pkg.sv`
does not define router FIFO depths at all — so the `OutFifoDepth = 0` recorded
in `docs/P0_SCOPE.md` as "the frozen v0 parameter set" was a choice made in the
router testbench, not a property of the IP. **No generated FlooNoC NoC uses
it.** The model was therefore one `stream_fifo_optimal_wrap` short on every
router output, which is one cycle per hop.

The consequences were not small. End-to-end latency in `test_axi_noc` went from
7 to 11 cycles at one hop, and from 16 to 30 at six. Every latency figure
recorded before this section understated the design.

`floo_router` now takes `OutFifoDepth` as a template parameter, defaulting to
2, and `run_router_crosscheck.sh` signs **both** branches: 214 cycles at depth
2 and 214 at depth 0.

**A second, smaller defect: the model's idle flit did not match the RTL's.**
`flit_header::last` defaulted to `true`, but the RTL ties unconnected inputs to
`'0`. The mesh writes `FlitT{}` into edge inputs, mirroring FlooGen's
`assign ..._req_in[p] = '0`, so every idle cycle diverged on `last`. Earlier
cross-checks never saw it because they compared payloads, not the header's
`last`, while idle.

Seven negative controls were run; **six are detected**:

| Injected defect | First divergence |
|---|---|
| Mesh built with the output FIFO bypassed | line 66 |
| North and South neighbour links swapped | line 208 |
| East link reads the neighbour's East output instead of its West | line 541 |
| Input FIFO depth raised to 4 | line 832 |
| Edge outputs held ready instead of not-ready | line 1600 |
| Local endpoint attached to the North port instead of Eject | line 84 |

One passed and was discarded as an equivalence: leaving the edge input's *data*
stale instead of writing `FlitT{}`. An edge input's `valid` is permanently
false, so the input spill register never latches it; and the signal is never
written anywhere else, so it keeps its default value regardless. The model
trace is byte-identical with and without that write.

**Two stimulus defects found while closing controls, both of which had silently
killed the run.** Neither was visible from the comparison — both sides agreed,
on a dead network:

- *Truncated wormhole packets.* The random phase injected multi-flit AW/W
  packets open loop. When a closing `last` flit landed on a cycle the port
  refused, the packet stayed open and held its route in every router it
  occupied, permanently. Multi-flit packets now stay in the directed phases,
  where the network is lightly loaded and every injection is accepted.
- *Self-addressed flits.* `NoLoopback` defaults to 1, so `floo_router` ties the
  Eject-input to Eject-output crossbar leg to zero: a flit addressed to its own
  node is undeliverable and wedges that node's input FIFO forever. The random
  phase was generating them.

Together these had every one of the nine nodes deadlocked by cycle 139 of 208,
so the whole tail of the run was comparing a stopped network and agreeing. The
generator now excludes both, and the run stays live to the last cycle apart
from the two nodes the off-mesh phase wedges on purpose. **A passing
cross-check says nothing if the stimulus stopped moving; check that it is still
live.**

### A constraint of the frozen configuration: `MaxUniqueIds = 1`

`ChimneyDefaultCfg` sets `MaxUniqueIds: 1`, and the FlooGen-generated 4x4 mesh
takes it unchanged (`set_ports(ChimneyDefaultCfg, 1'b1, 1'b1)`). In that branch
`hw/floo_meta_buffer.sv` stores request metadata in a plain `fifo_v3`
(`gen_no_atop_fifos`), popped in order with **no ID matching at all**; the
`id_queue` keyed by AXI ID is the `MaxUniqueIds > 1` branch.

So the chimney assumes responses return **in request order per direction**. A
single destination gives that, because all downstream transactions are reissued
with the constant ID `'1` and AXI ordering applies. Two different destinations
do not: `NoRoB` only serialises *the same* AXI ID to a different destination,
so different IDs to different endpoints may be outstanding together and can
return out of order, which the in-order FIFO would mis-attribute.

This is a usage constraint on the frozen configuration, stated here because it
directly shapes CDC-VP integration: either each manager uses a single AXI ID,
or `MaxUniqueIds` must be raised. It has not been demonstrated against RTL
simulation of a full system, only derived from the RTL text; `test_axi_noc`
respects it by draining between destinations rather than by relying on it.

### CDC-VP integration: the TLM wrapper (2026-07-29)

`include/floo_noc_model/noc_interconnect.h` puts the network behind the same
interface `cdc::components::bus_router` presents — `target_socket`,
`cpu_port(i)`, `add_target(base, size)` — so a platform can swap one for the
other. `platforms/noc_soc` is that platform.

The one thing `bus_router` has no equivalent for is **placement**: a flat bus
has no geometry, and here every initiator and target sits on a mesh node.
`add_target` takes a node and `place_initiator` sets the upstream ports.

Measured on the 2x2 `noc_soc` layout at a 1 ns network clock: 1 hop 21 ns,
2 hops 24-25 ns, and a four-beat burst at 2 hops 27-28 ns. One extra hop costs
about a cycle, and so does each extra beat, which is the wormhole packet
pipelining rather than four separate transactions.

Three behaviours a platform has to plan for, all of them the frozen
configuration's actual behaviour:

- **One AXI ID per upstream port.** `MaxUniqueIds = 1` makes the response
  metadata a plain in-order FIFO, so a port's transactions are serialised.
- **`b_transport` spends simulated time** rather than annotating `delay`. A
  caller using temporal decoupling will find its quantum consumed.
- **A burst is one packet.** `axi_transaction` now carries a beat vector and
  the manager endpoint emits AW plus N W flits with `last` on the final one.
  Splitting a burst into N transactions would lose the route holding that
  distinguishes a NoC from a bus.

Two implementation notes worth keeping:

- **Idle cycles are skipped, and that is exact.** With no `valid` asserted
  anywhere every register in the mesh holds, so a skipped cycle changes
  nothing. It keeps a mostly-idle platform from paying for the interconnect.
- **A target's own latency becomes a per-node hold-off in cycles**, not a
  `wait` inside the network thread. Waiting there would freeze every other
  node's traffic for the duration, which a real subordinate does not do.

**The bug that cost the most to find:** a `b_transport` issued at time zero
handed flits to a mesh still in reset, which swallowed them and hung the
simulation. The wrapper now blocks until reset has elapsed, and `step_once`
refuses injections while `rst_n` is low.

**Verification status.** The wrapper itself is contract-tested
(`test_noc_interconnect`): round-trip data, bursts, distance costing cycles,
debug access bypassing the network, and unmapped addresses reported rather than
routed. The network underneath is RTL-signed; the endpoint transactors between
them are not, and cannot be.

### Endpoints — still not RTL-signable

`include/floo_noc_model/axi_endpoint.hpp` adds manager and subordinate
transactors that compose the signed pieces: packing, destination decode,
metadata retention, and the ordering gate.

These remain a transaction-level abstraction with **no RTL counterpart at
all**, so no timing claim follows from them. `no_rob_order_gate`, the
transaction-level face of the ordering rule that these transactors call, is the
same arithmetic as the signed `no_rob_gate` without the clock; it is a
convenience wrapper, not a second model.

### Chimney: the rules, and what they cost to sign

`include/floo_noc_model/axi_chimney_pack.hpp` mirrors the `always_comb` blocks
of `hw/floo_axi_chimney.sv` that assemble flits, its `gen_route` destination
rules over `hw/floo_id_translation.sv`, and its `aw_w_sel_q` state.

Rules that a hand-written model gets wrong easily, all taken from the RTL text:

| Rule | Why it matters |
|---|---|
| AW carries `hdr.last = 0`, W carries `hdr.last = w.last` | AW and its W burst form one wormhole packet, so they hold one route |
| W carries the **AW's** reorder tag, not its own | the W flits belong to the AW's transaction |
| AR, B, and R carry `hdr.last = 1` | each is a single-flit packet; the RTL notes R bursts are deliberately not wormholed |
| `hdr.atop` is `aw.atop != ATOP_NONE` | it is a flag, not the ATOP code |
| B and R restore the manager's original AXI id from retained metadata | the downstream id is a chimney-local reissue |
| W's destination is the id latched at AW acceptance | W never decodes an address of its own |

Destination decode has two modes under XY routing and the frozen tree uses
both, so both are modeled: `UseIdTable = 1` is a system-address-map lookup, as
`floogen/examples/axi_mesh_xy.yml` selects; `UseIdTable = 0` extracts the
coordinate from address bit fields, as `hw/test/floo_test_pkg.sv` selects.

**Verification status: signed, by four separate cross-checks.**
`tests/test_axi_chimney_pack.cpp` on its own is only a contract test against the
RTL text, so read it as such. The equivalence proof is elsewhere on this page:

| Cross-check | Section above | Result |
|---|---|---|
| request content | "Chimney request path" | 16 flits |
| response content | "Chimney response path" | 8 flits |
| request timing | "Chimney request-path timing" | 141 cycles |
| response and subordinate side | "Chimney response path and subordinate side, timing" | 221 cycles |

The packing is inline `always_comb` and could not be isolated, so all four
instantiate the whole chimney, which pulls in the meta buffer and the `NoRoB`
gate as well. That turned out to be the right thing to do: the request-timing
harness is what found the reversed arbiter index order, and no isolated packing
harness would have.

The four testbenches live in `rtl_crosscheck/axi_chimney/`. The oldest of them
began as an elaboration-only file, `tb_floo_axi_chimney_elab.sv`; it became
`tb_floo_axi_chimney_req_trace.sv` once it carried stimulus, so that name no
longer exists.

The upstream `hw/tb/tb_floo_axi_chimney.sv` cannot be reused under Verilator:
it depends on the class-based `axi_test` package. It remains usable under VCS,
which is installed on this host. The four harnesses here are hand-written BFMs
instead; the "Harness lesson" section above is the price that was paid for
that.

### Cross-check strength

The FIFO trace records both the pre-edge sample (the handshake view the
environment acts on) and the post-edge sample. Post-edge-only sampling was
proven insufficient: it hides an output that wrongly depends on the current
`ready_i`. The arbiter trace additionally exports every arbiter register, so a
state divergence fails even when the outputs still agree. Fourteen negative
controls were run against the cycle harnesses below; the chimney request and
response harnesses add three and four more of their own, for twenty-one in
total:

| Injected defect | Harness | Result |
|---|---|---|
| FIFO `ready_o` made dependent on `ready_i` | stream-fifo | FAIL at the pre-edge column, first at cycle 9 |
| Superseded accept-at-full push rule | stream-fifo | FAIL on `data_o` divergence from cycle 9 |
| Round-robin advance replaced by `selected + 1` | arbiter | FAIL on `rr_q` from cycle 4 |
| `ready_o` gated by the selected input's own valid | arbiter | FAIL on `ready_o` from cycle 40 |
| Snapshot never refreshed on `last_q` | arbiter | FAIL from cycle 5 |
| Both packet holds removed at once | arbiter | FAIL on selection from cycle 13 |
| Tree fed live `valid_i` instead of the snapshot | arbiter | PASS — equivalent rewrite, see above |
| Tree `LockIn` disabled | arbiter | PASS — equivalent rewrite, see above |
| Crossbar data tie-off reverted | router | FAIL from cycle 15 |
| `XYRouteOpt` Y-to-X restriction removed | router | FAIL from cycle 15 |
| `NoLoopback` restriction removed | router | FAIL from cycle 15 |
| Spare bit dropped from the physical channel width | axi-sizing | FAIL on every configuration |
| `OutIdWidth` used for the B channel width | axi-sizing | FAIL on every configuration |
| Wrong W-channel strobe divisor | axi-sizing | FAIL on every configuration |

`usage_o` is not compared. `hw/floo_router.sv` leaves it unconnected, and the
depth-2 wrap branch drives it to `'x`. The model's `o_occupancy` is therefore
debug-only and outside the signed contract.

### Reference-anchoring audit (2026-07-28)

Every claim of the three signed cross-checks was re-derived from the frozen
FlooNoC tree rather than from this document. Results:

| Checked | Result |
|---|---|
| Local tree is the upstream repo at the frozen revision | exact (`origin` = upstream URL, `describe` = `v0.8.4-10-g9a6972a`) |
| Route-selector harness `floo_pkg` shim vs `hw/floo_pkg.sv` | `route_algo_e`, `route_direction_e`, `collect_op_e`, `floo_iomsb` all identical |
| Harness `LockRouting`/`RouteSelWidth` vs what `floo_router.sv` relies on | identical; the router leaves both at their defaults, `1'b1` and `$clog2(NumRoutes)` = 3 |
| Model `direction` vs `floo_pkg::route_direction_e` | exact |
| Model `axi_channel` vs `floo_pkg::axi_ch_e` | exact, including the 3-bit width |
| Bender-resolved `common_cells` vs the leaf path's independent pin | identical revision and file hashes |
| Model `flit_header` vs `FLOO_TYPEDEF_HDR_T` | **mismatch**: `collective_mask` and `collective_op` are absent |

No functional defect was found in the signed cross-checks. One documentation
inaccuracy was: the type-completeness note described the header gap as unproven
*widths*, when in fact two named fields are missing. The gap is behaviourally
inert in frozen v0 (Unicast, `EnMultiCast = 0`), which is why nothing failed,
but it must be closed before multicast, collectives, or reduction, and before
any packed-representation claim.

A coverage limit that was previously unstated: the route-selector cross-check
used a 2-bit `x`/`y` id type, so only coordinates 0..3 are signed off.

Verilator reports `WIDTHEXPAND` at the route-selector RTL expression
`Eject + channel_i.hdr.dst_id.port_id`. It does not affect the tested single
Eject port (`port_id == 0`), but multi-local-port routing is not signed off by
this cross-check and remains outside the frozen v0 scope.

## Measured counters (2026-07-28)

`include/floo_noc_model/noc_counters.hpp` adds `router_counters<FlitT,
NumPorts>`. Two rules govern it.

**Scope.** A counter may only observe a signal whose timing has passed an RTL
cross-check. It therefore observes the router boundary handshake, the input
buffer occupancy, and nothing else. There are still no link or end-to-end
latency counters, but the reason has changed: when this was written the mesh was
unsigned, and since the inter-node cross-check it is signed. What blocks them now
is only that per-flit tagging has not been written. The platform measures
end-to-end latency at the TLM boundary instead
(`noc_interconnect::last_latency_cycles()`), which is coarser — it sees a
transaction, not a flit.

**Passivity.** The block declares `sc_in` ports only and drives nothing. That
claim is checked rather than asserted: `tests/router_trace_sc.cpp` instantiates
the counters alongside the router, so the 214-cycle router cross-check runs
with them attached and still matches the RTL exactly.

Reporting keeps three tiers separate, as `docs/P0_SCOPE.md` requires:

| Tier | Meaning | Provided |
|---|---|---|
| measured | incremented only on an accepted transfer (`valid && ready`) or a directly sampled state | `accepted_flits`, `accepted_packets`, `stall_cycles`, `busy_cycles`, occupancy high-water and sum |
| derived | plain arithmetic over measured counts | `output_utilisation`, `mean_occupancy` |
| analytic | produced by a formula rather than observation | none |

Invariants that hold by construction and are asserted in the unit test:

- `busy_cycles == accepted_flits + stall_cycles` per port;
- occupancy high-water never exceeds the configured depth.

One caveat found while validating against the router stimulus: **flit
conservation does not hold across a reset.** A reset discards whatever the
input buffers were holding, so accepted-in exceeds accepted-out by the number
of flits in flight at that moment. Any conservation check must be scoped to a
reset-free, fully drained window. The unit test does exactly that; the
214-cycle router stimulus contains two reset events and legitimately shows 179
accepted in against 165 out.

## Dependency and tool state

Available on the development host:

- Verilator: `/usr/bin/verilator` 5.022
- VCS: `/opt/synopsys/vcs/X-2025.06/bin/vcs`
- Bender: `/home/duyptt_HW/.local/bin/bender` 0.32.1
- FlooNoC `Bender.lock` is present and is tracked by git in the FlooNoC
  repository. Its `.gitignore` entry for `Bender.lock` is inert because the
  file was committed upstream.

Two dependency paths now exist, and they cross-validate each other.

### Leaf path — `rtl_crosscheck/fetch_rtl_deps.sh`

Materialises single pinned repositories without Bender. It reads the revision
from the frozen `Bender.lock`, refuses to continue if the lock no longer
matches the recorded frozen value, checks out `common_cells` 1.39.0 at
`9ca8a76`, and verifies the SHA-256 of every dependency file a cross-check
compiles. Default checkout location:

```text
/home/duyptt_HW/Documents/work/Study_FlooNoC/floo_rtl_deps
```

Override with `FLOO_RTL_DEPS_ROOT`. This path stays in use for the FIFO and
arbiter cross-checks: it is fast, needs no Bender, and pins by file hash.

### Full path — `rtl_crosscheck/gen_rtl_filelist.sh`

Resolves the complete transitive tree with Bender and emits ordered tool file
lists, which the leaf path cannot do. Guarantees enforced:

- Bender is exactly 0.32.1;
- the FlooNoC tree is at the frozen revision and is clean;
- `Bender.lock` hashes identical before and after the run;
- only `bender checkout` runs, never `bender update`;
- the Bender-resolved `common_cells` revision equals the independent pin in
  `fetch_rtl_deps.sh`.

Outputs under `/tmp/floo_noc_rtl_filelist` by default: `floo_verilator.f` (370
lines), `floo_vcs.sh` (465), `floo_flist_plus.f` (307), and
`resolved_deps.txt` (13 packages).

Verified 2026-07-28: 13 dependencies checked out, `Bender.lock` unchanged, and
`floo_router.sv` elaborates from the generated list with **0 errors** under
Verilator 5.022. Guard behaviour was tested: a dirty FlooNoC tree is rejected,
a modified `Bender.lock` is rejected, and the lock-hash backstop fires when
exercised in isolation.

Note that `bender script verilator` emits `+define+TARGET_SYNTHESIS` and
`+define+TARGET_VERILATOR` by default. The route-selector harness passes
`TARGET_SYNTHESIS` explicitly for the same reason; a router harness built on
the generated list inherits it.

Resolved dependency set (from `Bender.lock`):

| Package | Version | Revision |
|---|---|---|
| apb | 0.2.4 | `77ddf07` |
| axi | 0.39.9 | `a256a3b` |
| axi_riscv_atomics | 0.8.3 | `97a1dd2` |
| axi_stream | 0.1.1 | `54891ff` |
| common_cells | 1.39.0 | `9ca8a76` |
| common_verification | 0.2.5 | `fb1885f` |
| fpnew | — | `e5aa6a0` |
| fpu_div_sqrt_mvp | 1.0.4 | `86e1f55` |
| idma | 0.6.5 | `28a36e5` |
| obi | 0.1.7 | `0155fc3` |
| register_interface | 0.4.7 | `d6e1d4c` |
| tech_cells_generic | 0.2.13 | `7968dd6` |
| floo_noc_pd | path `./pd` | — |

## Next implementation order

All four items of the previous list are done: the chimney is cross-checked in
both directions, the meta buffer and the `NoRoB` gate are modelled and signed,
the abstract mesh endpoints are replaced by chimney-backed AXI endpoints on two
physical networks, and `noc_interconnect` is the CDC-VP M:N fabric adapter.
Note item 3's premise was wrong: the mesh was signed against a hand-built grid
of the frozen `floo_axi_router`, so FlooGen was never needed.

What is left is no longer datapath modelling. The authoritative, ordered list is
`AI_HANDOFF_CONTEXT.md` section 14, and its sub-steps deliberately do **not**
run in numeric order:

1. **Step 10.2** — an automated real-firmware regression. Nothing at platform
   level is tested today, which is how the `kSurveyScratch` corruption survived
   28 passing component tests.
2. **Step 10.3** — scoreboard-driven stress on `axi_endpoint.hpp` and
   `noc_interconnect`. These have no RTL counterpart and every integration
   defect so far has been in them. Also closes the unproven clock-gating
   condition.
3. **Step 10.1** — separate survey and firmware ownership in `noc_soc`. Needs
   item 1 to be verifiable.
4. **Step 10.4** — clean-prefix install, packaging, licence and provenance
   audit.
5. **Step 10.5** — decide whether `MaxUniqueIds > 1` is required. Only worth it
   for congestion work; see "A constraint of the frozen configuration" above.
6. **Step 11** — the fast approximately-timed mode, calibrated against this
   model. Unstarted, and the largest remaining item: without it the NoC cannot
   be the interconnect of a VP that boots software at speed.
