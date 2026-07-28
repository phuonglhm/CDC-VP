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
  destination decode, metadata retention, the `NoRoB` ordering rule, and
  abstract AXI endpoint transactors.
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
| `test_route_trace_sc` | Directed CSV trace and fixed expected route/lock result |
| `test_fifo_trace_sc_d2` | 133-cycle FIFO trace against the RTL-captured depth-2 golden |
| `test_fifo_trace_sc_d4` | 133-cycle FIFO trace against the RTL-captured depth-4 golden |
| `test_arbiter_trace_sc_n2` | 152-cycle arbiter trace against the RTL-captured 2-route golden |
| `test_arbiter_trace_sc_n4` | 152-cycle arbiter trace against the RTL-captured 4-route golden |
| `test_arbiter_trace_sc_n5` | 152-cycle arbiter trace against the RTL-captured 5-route golden |
| `test_router_trace_sc` | 214-cycle router trace against the RTL-captured golden |

All seventeen tests pass with GCC 11.5.0 and SystemC 2.3.4.

## Accuracy status

RTL-signed blocks:

| Block | RTL reference | Evidence |
|---|---|---|
| XY route selector | `hw/floo_route_select.sv` | 12 cycles exact (`route_sel_id_o`, lock state) |
| Input FIFO | `common_cells` `stream_fifo_optimal_wrap` | 133 cycles exact at depth 2 and depth 4 (pre-edge and post-edge `ready_o`/`valid_o`/`data_o`) |
| Wormhole arbiter | `hw/floo_wormhole_arbiter.sv` over `common_cells` `rr_arb_tree`/`lzc` | 152 cycles exact at 5, 4, and 2 routes (pre-edge and post-edge `ready_o`/`valid_o`/`data_o`/selected index, plus `valid_q`, `last_q`, `rr_q`, `lock_q`, `req_q`) |
| Five-port router | `hw/floo_router.sv` in the frozen v0 parameter set | 214 cycles exact (pre-edge and post-edge per-port `ready_o`/`valid_o` masks and per-output `data_o`, plus the one-hot route mask per input) |
| AXI flit sizing | `floo_pkg` sizing functions over `axi 0.39.9` `axi_pkg` | 8 configurations exact (per-channel width, channel-to-link mapping, physical channel width, reserved bits) |
| Chimney request path | `hw/floo_axi_chimney.sv` in the `floo_test_pkg` parameter set | 16 flits exact (channel, destination, source, `last`, `atop`, `rob_req`, `rob_idx`, payload). Flit content and per-beat ordering only, not chimney timing |
| Chimney response path | same, driven as a subordinate | 8 flits exact, with three transactions outstanding per batch so the metadata FIFOs are genuinely exercised |

Link, mesh, and end-to-end latency remain model-contract tested but **not**
RTL-equivalent. Their timing numbers must still be labeled estimates.

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

### Response ordering and endpoints (2026-07-28)

`include/floo_noc_model/rob_order_gate.hpp` models the `NoRoB` branch of
`hw/floo_rob_wrapper.sv`. `NoRoB` is not "no ordering logic": the RTL comment
and code make it an admission rule that stalls a transaction reusing an AXI ID
for a *different* destination until the previous ones complete, tracked by a
per-ID counter. Both reorder buffers are `NoRoB` in the frozen configuration,
so this is the ordering behaviour that v0 actually has. The reordering RoB
types need a different frozen configuration and are out of scope.

`include/floo_noc_model/axi_endpoint.hpp` adds manager and subordinate
transactors that compose the signed pieces: packing, destination decode,
metadata retention, and the ordering gate.

Both are **contract-tested against the RTL text, not RTL-signed**. The ordering
rule is an admission decision, so cross-checking it needs a harness that
observes the request-side `ready`, which the content-only chimney harnesses do
not. The transactors are a transaction-level abstraction with no RTL
counterpart at all, and no timing claim follows from them.

### Chimney: what remains unverified

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

**Verification status.** This is a contract test against the RTL text, not an
equivalence proof. The packing is inline `always_comb` inside the chimney, so
it cannot be isolated; a real cross-check has to instantiate the whole chimney
together with its meta buffer and reorder buffers. Treat this header as
unverified until that harness exists.

What has been established is that the harness is buildable:
`rtl_crosscheck/axi_chimney/tb_floo_axi_chimney_elab.sv` instantiates the
unmodified chimney with the upstream test parameter set and **lints with zero
errors** against the Bender-generated file list. The remaining work is stimulus
and tracing, not type plumbing.

The upstream `hw/tb/tb_floo_axi_chimney.sv` cannot be reused under Verilator:
it depends on the class-based `axi_test` package. It remains usable under VCS,
which is installed on this host.

### Cross-check strength

The FIFO trace records both the pre-edge sample (the handshake view the
environment acts on) and the post-edge sample. Post-edge-only sampling was
proven insufficient: it hides an output that wrongly depends on the current
`ready_i`. The arbiter trace additionally exports every arbiter register, so a
state divergence fails even when the outputs still agree. Eight negative
controls were run against the harnesses:

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
buffer occupancy, and nothing else. There are no link or end-to-end latency
counters, because the mesh links are still cycle-approximate.

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

1. Cross-check the chimney. Build on
   `rtl_crosscheck/axi_chimney/tb_floo_axi_chimney_elab.sv`, which already
   elaborates cleanly; add a CSV-driven AXI stimulus and trace the `floo_req`
   and `floo_rsp` links plus the AXI out side. This turns the currently
   unverified flit assembly into signed behaviour.
2. Model outstanding-transaction tracking and the reorder buffers, which that
   harness will exercise anyway. Sources of truth: `hw/floo_meta_buffer.sv`,
   `hw/floo_rob*.sv`.
3. Replace the abstract mesh endpoint with the chimney. The mesh itself cannot
   be cross-checked until the generated topology is available, which needs
   FlooGen 0.8.4 installed from the frozen tree.
4. Add the CDC-VP M:N TLM fabric adapter only after standalone AXI traffic
   passes.
