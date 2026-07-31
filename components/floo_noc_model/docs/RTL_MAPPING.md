# FlooNoC RTL-to-SystemC mapping

This mapping follows FlooNoC structure rather than the NPU worked example.

| SystemC model block | FlooNoC source | Initial responsibility |
|---|---|---|
| `floo_types.hpp` | `hw/include/floo_noc/typedef.svh`, `hw/floo_pkg.sv` | Coordinates, header fields, flit and channel identity. Direction and AXI-channel encodings verified exact; the header carries the full `FLOO_TYPEDEF_HDR_T` field set in declaration order |
| `reference_model.hpp` | FlooGen graph/routing/address models | Timing-independent address decode and expected XY path |
| `stream_fifo.hpp` | `stream_fifo_optimal_wrap`, `spill_register_flushable`, `stream_fifo`, `fifo_v3` from `common_cells` 1.39.0 | Input buffering and back-pressure; mirrors the RTL wrap hierarchy including the depth-2 spill-register branch |
| `xy_route_select.hpp` | `hw/floo_route_select.sv` | XY next-hop selection and burst route lock |
| `rr_arb_tree.hpp` | `rr_arb_tree`, `lzc`, `cf_math_pkg::idx_width` from `common_cells` 1.39.0 | Round-robin tree, trailing-zero counters, and fair next-index state |
| `wormhole_arbiter.hpp` | `hw/floo_wormhole_arbiter.sv` | Request snapshot, output selection, and packet lock through `last` |
| `floo_router.hpp` | `hw/floo_router.sv`, `hw/floo_output_arbiter.sv` | Input FIFOs, routing, crossbar with the `NoLoopback`/`XYRouteOpt` tie-offs, per-output arbitration, and the `OutFifoDepth` output buffer |
| `axi_chimney_pack.hpp` | `hw/floo_axi_chimney.sv`, `hw/floo_id_translation.sv` | Flit assembly per AXI channel, both destination-decode modes, AW/W select FSM |
| `meta_buffer.hpp` | `hw/floo_meta_buffer.sv` (`MaxUniqueIds == 1` branch) | Request metadata as a plain in-order `fifo_v3` with no ID matching, and the constant downstream reissue ID. The `id_queue` branch is deliberately not modelled |
| `rob_order_gate.hpp` | `hw/floo_rob_wrapper.sv` (`NoRoB` branch) over axi `axi_demux_id_counters` | The same-ID/different-destination admission stall, the counter bank, and its **global** `full_o`. Signed, 127 cycles |
| `axi_chimney.hpp` | `hw/floo_axi_chimney.sv` | Timed chimney, three of four quadrants signed. **Manager request path** — reorder-buffer gates, `aw_w_sel_q` FSM, request arbiter, bypassed output cut: signed, 141 cycles. **Subordinate side** — unpacker, metadata FIFOs, the unconditional `i_aw_out_queue` spill register, response arbiter: signed, 221 cycles. **Manager-side response unpacker** (`axi_chimney_manager_response`) — implemented and unit-tested, **not signed**; Step A-1 |
| `axi_lanes.hpp` | AXI byte-lane rules | `AxSIZE`/`AxLEN` selection and per-beat `WSTRB`. No RTL counterpart: it is the TLM-to-AXI mapping, not a hardware block |
| `floo_mesh.hpp` | FlooGen generated `floo_*_noc.sv` | Rectangular router/link topology; port index order confirmed against the generated netlist |
| `axi_noc.hpp` | `hw/floo_axi_router.sv` (two `floo_router` instances) | Separate `req` and `rsp` meshes over the same coordinates |
| `noc_counters.hpp` | none: passive observation of `hw/floo_router.sv` boundary signals | Measured accept/stall/occupancy counters; drives nothing |
| `axi_types.hpp` | `hw/floo_pkg.sv` sizing functions, `hw/include/floo_noc/typedef.svh`, `axi 0.39.9` `src/axi_pkg.sv` | AXI config, five channel payloads, channel-to-link mapping, flit width and reserved-bit arithmetic |
| `axi_endpoint.hpp` | **no RTL counterpart**: composes the blocks above | Transaction-level AXI manager and subordinate — a driver and collector built on signed rules, not a hardware block. **Model-side abstraction, not RTL-signable.** Step A-3 replaces it in the datapath with the timed chimney |
| `noc_interconnect.h` / `src/noc_interconnect.cpp` | **no RTL counterpart**: a CDC-VP integration layer | TLM-2.0 wrapper with `bus_router`'s interface plus mesh placement, and the TLM-payload-to-AXI mapping. **Not RTL-derived** |

External RTL dependencies such as `common_cells` and `axi` are behavioral
dependencies of the named FlooNoC blocks. Only the behavior exercised by the
frozen FlooNoC configuration is ported; the external libraries are not copied
into the component.

`rtl_crosscheck/install_floogen.sh` installs FlooGen 0.8.4 from the frozen
tree (never PyPI), under `python3.11` because FlooGen needs >= 3.10 while this
host's default `python3` is 3.9, and generates `floo_axi_mesh_noc.sv` into a
build directory. It fails if the frozen checkout ends up modified.

Two resolution paths exist. `rtl_crosscheck/fetch_rtl_deps.sh` checks out
single repositories at the revisions locked in the frozen `Bender.lock` and
guards them by per-file SHA-256; the leaf cross-checks use it.
`rtl_crosscheck/gen_rtl_filelist.sh` runs Bender against the same lock to
resolve the full transitive tree and emit ordered tool file lists, and asserts
that its `common_cells` result agrees with the leaf path.

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

### `rtl_crosscheck/wormhole_arbiter`

Compiles the unmodified frozen `hw/floo_wormhole_arbiter.sv` over the
unmodified locked dependencies:

```text
common_cells/src/cf_math_pkg.sv
common_cells/src/lzc.sv
common_cells/src/rr_arb_tree.sv
```

The only local SystemVerilog file in the compile is `floo_pkg_empty.sv`, an
intentionally empty package. `floo_wormhole_arbiter.sv` imports `floo_pkg` but
references no symbol from it, and compiling against an empty package proves
that rather than asserting it.

`NumRoutes` is a top-level parameter, covering 5 (the router configuration,
non power-of-two tree), 4 (full binary tree), and 2 (single-level tree, the
configuration the standalone unit test uses).

Comparison is exact on `ready_o`, `valid_o`, `data_o`, and the selected index,
sampled pre-edge and post-edge, plus the registered state `valid_q`, `last_q`,
`rr_q`, `lock_q`, and `req_q` read through hierarchical references. The
`rr_arb_tree` data and grant outputs are excluded because the frozen
instantiation leaves them unconnected.

### `rtl_crosscheck/floo_router`

Compiles the unmodified frozen `hw/floo_router.sv` in the v0 parameter set
recorded in `docs/P0_SCOPE.md`. This harness uses **no shim of any kind**: the
compile comes from the Bender-generated file list, so `floo_pkg`,
`floo_route_select`, `floo_wormhole_arbiter`, `floo_vc_arbiter`, and every
`common_cells` dependency are the real frozen sources. Flit and header types
are built from the frozen `floo_noc/typedef.svh` macros.

At these parameters the RTL reduces to the model's structure: the reduction
demux, the parallel-reduction path in `floo_output_arbiter`, the output FIFO,
and `floo_vc_arbiter` all degenerate away, leaving input FIFO, route select,
masked crossbar, and one wormhole arbiter per output.

Comparison is exact on the per-port `ready_o`/`valid_o` masks and per-output
`data_o`, sampled pre-edge and post-edge, plus the one-hot `route_mask` per
input. The router's `StableValidIn`/`StableValidOut` assertions stay enabled
and the runner fails the run if either fires.

### `rtl_crosscheck/axi_sizing`

Compares the model's flit sizing arithmetic against the real `floo_pkg`
functions over a list of AXI configurations. Not a cycle comparison: the
functions are pure. The compile comes from the Bender-generated file list, so
`floo_pkg` and `axi_pkg` are the frozen sources.

Compared per configuration: per-channel payload width, channel-to-link
mapping, both physical channel widths, and per-channel reserved bits.

### `rtl_crosscheck/axi_chimney`

Two harnesses over the unmodified frozen `hw/floo_axi_chimney.sv`, both built
on the Bender-generated file list with no shim.

`run_chimney_req_crosscheck.sh` drives AXI beats into the manager port and
compares every emitted request flit. `run_chimney_rsp_crosscheck.sh` drives
request flits into `floo_req_i`, answers on `axi_out_rsp_i`, and compares every
emitted response flit; it issues three transactions per batch so the metadata
FIFOs hold more than one entry.

Both compare flit content and ordering, not chimney timing or arbitration.

`run_chimney_timing_crosscheck.sh` covers what they cannot. It drives AW, W,
and AR concurrently with non-uniform back-pressure on the `req` link and
compares a cycle trace: the three AXI `ready` signals, the link's `valid` and
flit, and the composed state (`aw_w_sel_q` plus every request-arbiter
register). It also drops the `ApplTime`/`TestTime` phase arithmetic the content
harnesses use, in favour of the cycle-driven idiom the other cross-checks use.

`run_chimney_rsp_timing_crosscheck.sh` does the same for the other direction:
request flits in, answers on `axi_out`, back-pressure on the outgoing `rsp`
link and on `axi_out`'s AW, compared per cycle. It also exports both metadata
FIFO occupancies.

Note **both** arbiter input arrays are declared with **ascending** packed
ranges — `floo_req_arb_in [AxiW:AxiAr]` and `floo_rsp_arb_in [AxiB:AxiR]` — so
index 0 is the AR slot and the R slot respectively, the reverse of reading the
declaration left to right. On the request side that mistake was a real defect
found by cross-check; it is observable in round-robin priority.

### `rtl_crosscheck/mesh`

`run_mesh_crosscheck.sh` builds a 3x3 grid of the unmodified
`hw/floo_axi_router.sv` with the wiring rule read out of the FlooGen netlist,
and compares the model's two `floo_mesh` instances per node and per cycle.

Not through the FlooGen top: that module exposes only AXI ports per endpoint,
so it would drag a full chimney into a mesh measurement. `floo_axi_router` has
flit-level ports and is the isolatable unit.

### `rtl_crosscheck/rob`

`run_rob_crosscheck.sh` instantiates the unmodified `hw/floo_rob_wrapper.sv`
with `RoBType = NoRoB` as a leaf, on the Bender-generated file list with no
shim, and compares a handshake per cycle rather than flit content: `NoRoB` is
an admission rule, so `ax_ready_o` is its only boundary observable.

The trace also exports `in_flight`, `prev_dest`, `counter_full`, and every
counter and destination register of `axi_demux_id_counters` through
hierarchical references. That matters because `full_o` is a global OR across
the bank: a model with correct per-ID counters but a per-ID full would agree on
the boundary until a counter saturates.

Both `hw/floo_rob_wrapper.sv` and the axi `src/axi_demux_simple.sv` that
defines the counters are SHA-256 pinned by the runner.
