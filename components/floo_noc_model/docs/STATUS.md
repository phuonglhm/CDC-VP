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
- P7.6 common SystemC/SV trace format plus route-selector, input-FIFO, and
  wormhole-arbiter RTL cross-checks.

Standalone verification:

| Test | Coverage |
|---|---|
| `test_reference_model` | Address boundaries/overlap and expected XY path |
| `test_stream_fifo` | Depth-2 spill and depth-4 FIFO branches: reset, fill, refused push at full, pointer wrap, drain |
| `test_xy_route_select` | XY ordering, local eject, route lock/release |
| `test_wormhole_arbiter` | Round-robin selection and packet lock at two routes |
| `test_floo_router` | Contention, output back-pressure, no packet interleave |
| `test_floo_mesh` | 2×2 multi-hop delivery, destination check, stable stall |
| `test_route_trace_sc` | Directed CSV trace and fixed expected route/lock result |
| `test_fifo_trace_sc_d2` | 133-cycle FIFO trace against the RTL-captured depth-2 golden |
| `test_fifo_trace_sc_d4` | 133-cycle FIFO trace against the RTL-captured depth-4 golden |
| `test_arbiter_trace_sc_n2` | 152-cycle arbiter trace against the RTL-captured 2-route golden |
| `test_arbiter_trace_sc_n4` | 152-cycle arbiter trace against the RTL-captured 4-route golden |
| `test_arbiter_trace_sc_n5` | 152-cycle arbiter trace against the RTL-captured 5-route golden |

All twelve tests pass with GCC 11.5.0 and SystemC 2.3.4.

## Accuracy status

RTL-signed blocks:

| Block | RTL reference | Evidence |
|---|---|---|
| XY route selector | `hw/floo_route_select.sv` | 12 cycles exact (`route_sel_id_o`, lock state) |
| Input FIFO | `common_cells` `stream_fifo_optimal_wrap` | 133 cycles exact at depth 2 and depth 4 (pre-edge and post-edge `ready_o`/`valid_o`/`data_o`) |
| Wormhole arbiter | `hw/floo_wormhole_arbiter.sv` over `common_cells` `rr_arb_tree`/`lzc` | 152 cycles exact at 5, 4, and 2 routes (pre-edge and post-edge `ready_o`/`valid_o`/`data_o`/selected index, plus `valid_q`, `last_q`, `rr_q`, `lock_q`, `req_q`) |

Router, link, mesh, and end-to-end latency remain model-contract tested but
**not** RTL-equivalent. Their timing numbers must still be labeled estimates.

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

`usage_o` is not compared. `hw/floo_router.sv` leaves it unconnected, and the
depth-2 wrap branch drives it to `'x`. The model's `o_occupancy` is therefore
debug-only and outside the signed contract.

Verilator reports `WIDTHEXPAND` at the route-selector RTL expression
`Eject + channel_i.hdr.dst_id.port_id`. It does not affect the tested single
Eject port (`port_id == 0`), but multi-local-port routing is not signed off by
this cross-check and remains outside the frozen v0 scope.

## Dependency and tool state

Available on the development host:

- Verilator: `/usr/bin/verilator` 5.022
- VCS: `/opt/synopsys/vcs/X-2025.06/bin/vcs`
- FlooNoC `Bender.lock` is present.

Still missing from `PATH`:

- `bender`

`rtl_crosscheck/fetch_rtl_deps.sh` now materialises the locked dependency
revisions without Bender. It reads the revision from the frozen `Bender.lock`,
refuses to continue if the lock no longer matches the recorded frozen value,
checks out `common_cells` 1.39.0 at `9ca8a76`, and verifies the SHA-256 of
every dependency file a cross-check compiles. Default checkout location:

```text
/home/duyptt_HW/Documents/work/Study_FlooNoC/floo_rtl_deps
```

Override with `FLOO_RTL_DEPS_ROOT`. The remaining FlooNoC dependencies (`axi`,
`idma`, `axi_riscv_atomics`, FPnew) are not needed yet and are not fetched.
Full router compilation still needs the complete dependency tree.

## Next implementation order

1. Resolve the remaining Bender dependency tree, then cross-check the router.
   This is the first step that needs more than `common_cells`: `floo_router.sv`
   pulls in `floo_vc_arbiter`, `floo_route_select`, the generated `floo_pkg`,
   and through it `axi_pkg`.
2. Add measured link/FIFO/arbitration counters.
4. Add single-AXI channel types and an abstract AXI endpoint transactor.
5. Port the single-AXI chimney incrementally: address-to-destination mapping,
   request packetization, AW/W lock, response metadata, then ordering/RoB.
6. Replace the abstract mesh endpoint with the chimney.
7. Add the CDC-VP M:N TLM fabric adapter only after standalone AXI traffic passes.
