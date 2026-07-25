# Implementation status

## Completed

- P0 scope frozen against FlooNoC revision `9a6972a`.
- P1 timing-independent address-map and XY-path reference.
- P2 signal-safe coordinate, header, and flit types.
- P3 ready/valid FIFO.
- P3 locked XY route selector.
- P3 fair wormhole arbiter.
- P3 five-port XY router with input FIFOs and output arbitration.
- P4 abstract-endpoint rectangular mesh.
- P7.6 common SystemC/SV trace format and route-selector RTL cross-check.

Standalone verification:

| Test | Coverage |
|---|---|
| `test_reference_model` | Address boundaries/overlap and expected XY path |
| `test_ready_valid_fifo` | Full/empty, ordering, simultaneous pop/push |
| `test_xy_route_select` | XY ordering, local eject, route lock/release |
| `test_wormhole_arbiter` | Round-robin fairness and packet lock |
| `test_floo_router` | Contention, output back-pressure, no packet interleave |
| `test_floo_mesh` | 2×2 multi-hop delivery, destination check, stable stall |
| `test_route_trace_sc` | Directed CSV trace and fixed expected route/lock result |

All seven tests pass with GCC 11.5.0 and SystemC 2.3.4.

## Accuracy status

The XY route-selector slice passed an exact 12-cycle comparison against the
unmodified frozen FlooNoC `hw/floo_route_select.sv` using Verilator 5.022.
Coverage includes reset, every XY/eject result, lock acquisition, locked
back-pressure, and release on `last`.

This signs off the route-selector behavior only. FIFO, arbiter, router, link,
and end-to-end latency remain model-contract tested but **not** RTL-equivalent.
Their timing numbers must still be labeled estimates.

Verilator reports `WIDTHEXPAND` at the RTL expression
`Eject + channel_i.hdr.dst_id.port_id`. It does not affect the tested single
Eject port (`port_id == 0`), but multi-local-port routing is not signed off by
this cross-check and remains outside the frozen v0 scope.

Available on the development host:

- Verilator: `/usr/bin/verilator`
- VCS: `/opt/synopsys/vcs/X-2025.06/bin/vcs`
- FlooNoC `Bender.lock` is present.

Missing from `PATH`:

- `bender`

The FlooNoC external RTL dependencies have not been checked out in this working
tree. The leaf route-selector harness therefore uses a minimal compile-only
`floo_pkg`/`common_cells` shim. Full router compilation remains blocked on the
Bender dependency tree.

## Next implementation order

1. Cross-check FIFO and wormhole arbiter with shared cycle traces.
2. Resolve the Bender dependency tree, then cross-check the router.
3. Add measured link/FIFO/arbitration counters.
4. Add single-AXI channel types and an abstract AXI endpoint transactor.
5. Port the single-AXI chimney incrementally: address-to-destination mapping,
   request packetization, AW/W lock, response metadata, then ordering/RoB.
6. Replace the abstract mesh endpoint with the chimney.
7. Add the CDC-VP M:N TLM fabric adapter only after standalone AXI traffic passes.
