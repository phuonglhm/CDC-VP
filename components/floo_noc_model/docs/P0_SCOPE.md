# P0 — FlooNoC Direction-2 scope and locked decisions

## Frozen source

- FlooNoC upstream: `https://github.com/pulp-platform/FlooNoC.git`
- Local working copy:
  `/home/duyptt_HW/Documents/work/Study_FlooNoC/FlooNoC`
- FlooNoC revision: `9a6972a`
- FlooGen package version: `0.8.4`
- Integration target: CDC-VP, SystemC 2.3.4, C++17.

The model must not silently track another FlooNoC revision. Changing the frozen
revision requires an RTL cross-check rerun and an update to this document.

### Provenance verification (2026-07-28)

The local working copy was verified to be the upstream repository, not a
re-import or a fork:

```bash
git -C <FlooNoC> remote -v          # origin = https://github.com/pulp-platform/FlooNoC.git
git -C <FlooNoC> describe --tags 9a6972a5f9b8117506d1df8a6505ce1da2bc9084
                                    # v0.8.4-10-g9a6972a
git -C <FlooNoC> branch -r --contains 9a6972a5f9b8117506d1df8a6505ce1da2bc9084
                                    # origin/main
```

The frozen revision is 10 commits after tag `v0.8.4`, consistent with the
recorded FlooGen package version. Upstream `main` has since advanced to
`0a08447`; the working tree is intentionally held behind it. Do not fast-forward
the working tree to follow upstream.

External RTL dependencies are pinned to the revisions resolved in the frozen
`Bender.lock` and are materialised by `rtl_crosscheck/fetch_rtl_deps.sh`, which
guards both the lock entry and the SHA-256 of every compiled file. Currently
resolved: `common_cells` `1.39.0` @ `9ca8a76`. A dependency-revision change is
a scope change and follows the same rules as a FlooNoC revision change.

## Vertical slice v0

The first end-to-end slice is deliberately constrained to:

| Parameter | Locked value |
|---|---|
| Network type | single AXI |
| Routing | deterministic XY |
| Traffic | unicast |
| Physical channels | `req`, `rsp` |
| Link flow control | ready/valid |
| Virtual channels | disabled |
| Router ports | North, East, South, West, Eject |
| Router input FIFO | enabled; depth fixed per instantiated test (depth 2 selects the RTL spill-register branch) |
| Router output FIFO | **`OutFifoDepth = 2`**, which every FlooGen router template hardcodes. An earlier revision of this table said "initially disabled"; that came from a Step-4 testbench choice, not from the RTL, and the model was built without an output FIFO for five steps because of it |
| Topology | rectangular 2-D mesh |
| Link latency | one configured cycle per registered boundary |

Deferred features:

- narrow-wide AXI and the `wide` channel;
- YX, source, and table-based routing;
- virtual channels and credit flow control;
- multicast, synchronization, and reduction;
- AXI ATOPs;
- configurable RoB modes and multiple unique downstream IDs;
- CDC links and asynchronous clock domains.

Deferred does not mean unsupported permanently. Each feature is added only
after the lower-level RTL equivalence tests remain green.

## Direction-2 decisions

1. **Authoritative output:** the SystemC datapath/network state is authoritative.
   A golden/reference model is comparison-only and never overwrites output.
2. **Timing target:** develop cycle-approximate first; promote a block to
   RTL-signed cycle accuracy only after per-cycle trace comparison.
3. **Runtime style:** traffic-driven. There is no synthetic accelerator
   `START/DONE` contract.
4. **Clock control:** the future CDC-VP wrapper may gate the clock only when all
   ingress queues, router FIFOs, locks, egress queues, and outstanding responses
   are empty.
5. **Integration position:** FlooNoC replaces or hierarchically composes the
   CDC-VP fabric; it is not attached as one MMIO peripheral.

## Frozen router parameter set (v0)

Derived from `hw/floo_router.sv` at the frozen revision on 2026-07-28, not
assumed. This is the configuration the router cross-check compares against.

```systemverilog
floo_router #(
  .NumRoutes       ( 5                           ),
  .NumInput        ( 5                           ),  // defaults to NumRoutes
  .NumOutput       ( 5                           ),  // defaults to NumRoutes
  .NumVirtChannels ( 1                           ),
  .NumPhysChannels ( 1                           ),
  .InFifoDepth     ( 2                           ),
  .OutFifoDepth    ( 2                           ),   // see the note below
  .RouteAlgo       ( floo_pkg::XYRouting         ),
  .IdWidth         ( $bits(id_t)                 ),
  .id_t            ( id_t                        ),
  .NumAddrRules    ( 1                           ),
  .XYRouteOpt      ( 1'b1                        ),
  .NoLoopback      ( 1'b1                        ),
  .VcImpl          ( floo_pkg::VcNaive           ),
  .CollectiveCfg   ( CollectiveSupportDefaultCfg ),  // all-zero: collectives off
  .RedCfg          ( '0                          ),
  .AxiCfgOffload   ( '0                          ),
  .AxiCfgParallel  ( '0                          ),
  .addr_rule_t     ( logic                       ),
  .flit_t          ( flit_t                      ),
  .hdr_t           ( hdr_t                       ),
  .red_req_t       ( logic                       ),
  .red_rsp_t       ( logic                       )
)
```

What that configuration reduces the RTL to, verified by reading the generate
blocks rather than by assumption:

| RTL element | Behaviour at these parameters |
|---|---|
| `CollectiveSupportDefaultCfg` | `'{default: '0}`, so `EnMultiCast`, `EnSequentialReduction`, and `EnParallelReduction` are all 0 |
| Reduction demux / logic | `gen_no_red_offload`: `cross_valid = in_valid`, `in_ready = cross_ready` |
| `floo_output_arbiter` | `NumParallelRedRoutes = 0`, so it degenerates to a single `floo_wormhole_arbiter` |
| `OutFifoDepth = 2` | `gen_out_fifo`: a `stream_fifo_optimal_wrap` per output. **Corrected 2026-07-29.** This file previously recorded `0`, the `gen_no_out_fifo` bypass. That was a choice made in the router testbench, not a property of the IP: every FlooGen router template hardcodes `.OutFifoDepth (2)` and `hw/test/floo_test_pkg.sv` defines no router FIFO depths at all, so no generated NoC uses `0`. The model was one buffer short on every router output — one cycle per hop. Both branches are now signed |
| `floo_vc_arbiter` | `NumVirtChannels == NumPhysChannels`, so `gen_virt_eq_phys` is a pure pass-through |
| `VcImpl = VcNaive` | `gen_no_credit`: `credit_o` tied high; credit path unused |
| `NumPhysChannels = 1` | `gen_single_phys`: `in_p = '0` |

The resulting datapath is exactly the model's structure:

```text
stream_fifo_optimal_wrap(Depth=2 -> spill register)
  -> floo_route_select(XYRouting, EnMultiCast=0)
  -> crossbar mask with NoLoopback and XYRouteOpt tie-offs
  -> floo_wormhole_arbiter per output
  -> output
```

Two properties established by reading the RTL, both previously unverified
assumptions in the model:

- `route_sel_o == 1 << route_sel_id_o` for unicast, in both the locked and
  unlocked case, because `route_sel_unicast[route_sel_id] = 1'b1` and the lock
  latches mask and index together with the same enable. The model's use of the
  encoded index is therefore equivalent to the RTL's one-hot mask here.
- `hdr.collective_op` is only read when `EnMultiCast = 1`, so the model's
  missing `collective_op`/`collective_mask` fields are inert in v0.

## Metrics

Directly measured:

- transaction and flit latency;
- flits and payload bytes per cycle, per link/channel;
- injection/ejection stall cycles;
- output arbitration wait cycles;
- FIFO occupancy and high-water mark;
- link utilization;
- hop count;
- outstanding response and RoB occupancy when that phase is implemented.

Analytic metrics, if added, must be reported separately from measured counters.

## Completion criteria for the vertical slice

- Every leaf block has a standalone SystemC test.
- A single router passes directed routing, contention, back-pressure, and
  wormhole-lock tests.
- A small mesh delivers every flit exactly once to the expected endpoint.
- The same directed stimuli are run against FlooNoC RTL.
- Function and handshake cycle traces match within the declared tolerance.
