# FlooNoC RTL-to-SystemC mapping

This mapping follows FlooNoC structure rather than the NPU worked example.

| SystemC model block | FlooNoC source | Initial responsibility |
|---|---|---|
| `floo_types.hpp` | `hw/include/floo_noc/typedef.svh`, `hw/floo_pkg.sv` | Coordinates, header fields, flit and channel identity |
| `reference_model.hpp` | FlooGen graph/routing/address models | Timing-independent address decode and expected XY path |
| `ready_valid_fifo.hpp` | `stream_fifo_optimal_wrap` used by `hw/floo_router.sv` | Input buffering and back-pressure |
| `xy_route_select.hpp` | `hw/floo_route_select.sv` | XY next-hop selection and burst route lock |
| `wormhole_arbiter.hpp` | `hw/floo_wormhole_arbiter.sv` | Fair output selection and packet lock through `last` |
| `floo_router.hpp` | `hw/floo_router.sv`, `hw/floo_output_arbiter.sv` | Input FIFOs, routing, crossbar, per-output arbitration |
| future `axi_chimney.hpp` | `hw/floo_axi_chimney.sv` | AXI/flit mapping, AW/W coupling, response ordering |
| future `meta_buffer.hpp` | `hw/floo_meta_buffer.sv` | Source metadata and downstream AXI ID management |
| future `reorder_buffer.hpp` | `hw/floo_rob*.sv` | Same-ID AXI response ordering |
| `floo_mesh.hpp` | FlooGen generated `floo_*_noc.sv` | Rectangular router/link topology with abstract local endpoints |
| future TLM transactors | CDC-VP wrapper plus AXI semantics | TLM generic payload to/from signal-level AXI |

External RTL dependencies such as `common_cells` and `axi` are behavioral
dependencies of the named FlooNoC blocks. Only the behavior exercised by the
frozen FlooNoC configuration is ported; the external libraries are not copied.

## Cross-check mapping

`rtl_crosscheck/route_select` compiles the frozen tree's unmodified
`hw/floo_route_select.sv`. Its local shim reproduces only the enum values,
`floo_iomsb`, and `FF`/`FFL` macro behavior needed to elaborate that leaf
module. No route-selection or locking behavior is reimplemented in the shim.

The shared directed trace currently covers reset, North/East/South/West/Eject,
lock acquisition, back-pressure while locked, and lock release on `last`.
Comparison is post-rising-edge and exact for `route_sel_id_o` and the internal
lock state.
