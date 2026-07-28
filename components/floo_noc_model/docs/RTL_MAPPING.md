# FlooNoC RTL-to-SystemC mapping

This mapping follows FlooNoC structure rather than the NPU worked example.

| SystemC model block | FlooNoC source | Initial responsibility |
|---|---|---|
| `floo_types.hpp` | `hw/include/floo_noc/typedef.svh`, `hw/floo_pkg.sv` | Coordinates, header fields, flit and channel identity |
| `reference_model.hpp` | FlooGen graph/routing/address models | Timing-independent address decode and expected XY path |
| `stream_fifo.hpp` | `stream_fifo_optimal_wrap`, `spill_register_flushable`, `stream_fifo`, `fifo_v3` from `common_cells` 1.39.0 | Input buffering and back-pressure; mirrors the RTL wrap hierarchy including the depth-2 spill-register branch |
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
frozen FlooNoC configuration is ported; the external libraries are not copied
into the component. `rtl_crosscheck/fetch_rtl_deps.sh` checks them out at the
revisions locked in the frozen `Bender.lock`.

## Cross-check mapping

### `rtl_crosscheck/route_select`

Compiles the frozen tree's unmodified `hw/floo_route_select.sv`. Its local shim
reproduces only the enum values, `floo_iomsb`, and `FF`/`FFL` macro behavior
needed to elaborate that leaf module. No route-selection or locking behavior is
reimplemented in the shim.

The shared directed trace covers reset, North/East/South/West/Eject, lock
acquisition, back-pressure while locked, and lock release on `last`. Comparison
is post-rising-edge and exact for `route_sel_id_o` and the internal lock state.

### `rtl_crosscheck/stream_fifo`

Compiles the unmodified `common_cells` sources at the locked revision:

```text
src/fifo_v3.sv
src/stream_fifo.sv
src/spill_register_flushable.sv
src/stream_fifo_optimal_wrap.sv
```

No shim is involved. The testbench instantiates the wrap with the parameters
`hw/floo_router.sv` uses for its input buffers: `flush_i` tied low,
`testmode_i` low, `usage_o` unconnected. `Depth` is a top-level parameter so
one testbench covers both wrap branches (2 and 4).

Comparison is exact on `ready_o`, `valid_o`, and `data_o`, sampled both
pre-edge and post-edge. `usage_o` is excluded because the frozen router leaves
it unconnected and the depth-2 branch drives it to `'x`.
