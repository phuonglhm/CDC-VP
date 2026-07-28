# Implementation status

## Completed

- P0 scope frozen against FlooNoC revision `9a6972a`.
- P1 timing-independent address-map and XY-path reference.
- P2 signal-safe coordinate, header, and flit types.
- P3 `stream_fifo_optimal_wrap` mirror (spill register / stream FIFO).
- P3 locked XY route selector.
- P3 fair wormhole arbiter.
- P3 five-port XY router with input FIFOs and output arbitration.
- P4 abstract-endpoint rectangular mesh.
- P7.6 common SystemC/SV trace format, route-selector RTL cross-check, and
  input-FIFO RTL cross-check.

Standalone verification:

| Test | Coverage |
|---|---|
| `test_reference_model` | Address boundaries/overlap and expected XY path |
| `test_stream_fifo` | Depth-2 spill and depth-4 FIFO branches: reset, fill, refused push at full, pointer wrap, drain |
| `test_xy_route_select` | XY ordering, local eject, route lock/release |
| `test_wormhole_arbiter` | Round-robin fairness and packet lock |
| `test_floo_router` | Contention, output back-pressure, no packet interleave |
| `test_floo_mesh` | 2×2 multi-hop delivery, destination check, stable stall |
| `test_route_trace_sc` | Directed CSV trace and fixed expected route/lock result |
| `test_fifo_trace_sc_d2` | 133-cycle FIFO trace against the RTL-captured depth-2 golden |
| `test_fifo_trace_sc_d4` | 133-cycle FIFO trace against the RTL-captured depth-4 golden |

All nine tests pass with GCC 11.5.0 and SystemC 2.3.4.

## Accuracy status

RTL-signed blocks:

| Block | RTL reference | Evidence |
|---|---|---|
| XY route selector | `hw/floo_route_select.sv` | 12 cycles exact (`route_sel_id_o`, lock state) |
| Input FIFO | `common_cells` `stream_fifo_optimal_wrap` | 133 cycles exact at depth 2 and depth 4 (pre-edge and post-edge `ready_o`/`valid_o`/`data_o`) |

Arbiter, router, link, mesh, and end-to-end latency remain model-contract
tested but **not** RTL-equivalent. Their timing numbers must still be labeled
estimates.

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

### Cross-check strength

The FIFO trace records both the pre-edge sample (the handshake view the
environment acts on) and the post-edge sample. Post-edge-only sampling was
proven insufficient: it hides an output that wrongly depends on the current
`ready_i`. Two negative controls were run against the harness:

| Injected defect | Result |
|---|---|
| `ready_o` made dependent on `ready_i` | FAIL at the pre-edge column, first at cycle 9 |
| Superseded accept-at-full push rule | FAIL on `data_o` divergence from cycle 9 |

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

1. Cross-check the wormhole arbiter against `hw/floo_wormhole_arbiter.sv` with
   the locked `rr_arb_tree`. The required `common_cells` sources are already
   materialised; `cf_math_pkg` and `lzc` come from the same checkout.
2. Resolve the remaining Bender dependency tree, then cross-check the router.
3. Add measured link/FIFO/arbitration counters.
4. Add single-AXI channel types and an abstract AXI endpoint transactor.
5. Port the single-AXI chimney incrementally: address-to-destination mapping,
   request packetization, AW/W lock, response metadata, then ordering/RoB.
6. Replace the abstract mesh endpoint with the chimney.
7. Add the CDC-VP M:N TLM fabric adapter only after standalone AXI traffic passes.
