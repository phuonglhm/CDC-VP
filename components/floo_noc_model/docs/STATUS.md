# Implementation status

## FlooNoC model technical sign-off v1.5

**PASS, signed 2026-08-24.** D1 owner-aware local bypass and D26 admission-slot
ownership are approved as baseline v1.5. The mandatory closure gate was
executed on one manifest-bound working-tree snapshot: 42/42 SystemC component
tests, 57/57 mutation controls detected with zero missed, and 12/12 isolated
RTL cross-check runners against clean FlooNoC `9a6972a`. Raw transcripts,
complete hashes, scope qualifications and the 133-file tested-source manifest
are under `docs/signoff/v1.5/`; `SIGNOFF.md` is the audit entry point. This
supersedes v1.4 **for the exact snapshot bound by that manifest**, without
rewriting its historical evidence.

**The working tree is no longer that snapshot.** R-P9-3 subsequently changed
`src/noc_interconnect.cpp`, `include/floo_noc_model/noc_interconnect.h`,
`tests/test_noc_interconnect_local_bypass.cpp` and
`rtl_crosscheck/run_negative_controls.sh`, and later review findings changed
the first three again, so `sha256sum -c docs/signoff/v1.5/tested_source_manifest.sha256`
reports mismatches. Quote v1.5 for the manifest, not for the tree: the current
component snapshot is **unsigned** until a v1.6 baseline or a batch re-sign
covers the delta.

### Erratum: the artifact manifests are scoped wrong, in both baselines

`docs/signoff/v1.4/artifact_manifest.sha256` and
`docs/signoff/v1.5/artifact_manifest.sha256` each cover **living** documents
alongside the frozen sign-off package. A living document changes by design, so
each manifest goes red the moment the component moves on, and stays red.

**The two manifests do not cover the same living documents**, and an earlier
version of this section said they did:

| Manifest | Living documents it hashes | Failing today |
|---|---|---|
| v1.4 | `NOC_MODEL_ARCHITECTURE.vi.md`, `NOC_MODEL_ARCHITECTURE.vi.docx` — **not** `STATUS.md` | the `.md`, changed for v1.5. The `.docx` still verifies |
| v1.5 | `NOC_MODEL_ARCHITECTURE.vi.md`, `STATUS.md` | `STATUS.md`, changed by this section. The architecture `.md` still verifies |

This is a **scope error, not tampering**, and it is not repaired by reverting
the documents: v1.6 would have to change this file again and v1.5 would go red
again, permanently. Neither manifest is edited to fix it — a signature that is
rewritten when it becomes inconvenient signs nothing.

**Verifying a signed baseline despite it.** The sign-off package proper —
`SIGNOFF.md`, the four `script(1)` logs and `tested_source_manifest.sha256` —
is frozen and verifies as listed. For the two living documents, check the
signed bytes at the commit that carries them rather than in the working tree:

```bash
# v1.5's copy of this file
git show 8d94ca3:components/floo_noc_model/docs/STATUS.md | sha256sum
# -> e1d597e8a888b3bc580588b76606b87d0bdba7e7a8f9a725ec34ab654e92dbb0

# v1.4's copy of the architecture document
git show 963466e:components/floo_noc_model/docs/NOC_MODEL_ARCHITECTURE.vi.md \
    | sha256sum
# -> c76901185ff0f669b4802a7e1d7b1e51fc3bf916de65c78a5d9256c4968332e4
```

**The two baselines need different commits, and neither is the one its own
`SIGNOFF.md` names.** v1.5's records `963466e` as the base of the dirty tree
its gates ran on, but the signed bytes of *this* file reached git one commit
later, at `8d94ca3` — while v1.4's architecture document is the copy `963466e`
still carries, because v1.5 is what changed it. Reaching for a single commit
for both is the mistake to avoid.

**Closed 2026-08-24 by commit `8652fb0`:** the four v1.5 logs, and v1.4's
three, are now tracked. Until then they matched the root `*.log` rule and a
fresh checkout could not verify them at all; the `.gitignore` negation that
exposed them is `!components/*/docs/signoff/**/*.log`. Both packages now
verify from a clean checkout apart from the living documents above, which is
the scope error this section exists to record.

**Binding on the next baseline.** A v1.6 artifact manifest covers the sign-off
package only. A living document that the sign-off relies on has its hash quoted
inside `SIGNOFF.md`, which is itself hashed — bound, without making a
permanently red line the normal state of a signed package.

This is block-level RTL sign-off plus model-level verification of the TLM
integration layer. It is not a claim of monolithic manager-to-subordinate RTL
equivalence, silicon timing/PPA, or support for deferred v0 features.

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
- Step A-3 integrated the complete timed `axi_noc` into `noc_interconnect`.
  The wrapper now drives manager AW/W/AR and subordinate B/R cycle by cycle;
  `axi_endpoint.hpp` is no longer in the production datapath.
- D1 added owner-aware local bypass for the `NoLoopback` configuration. Only a
  manager accessing its declared same-node target may bypass the mesh; the
  access creates no flit, network accounting or routed admission-slot use, and
  per-port completion ordering remains enforced against routed traffic.
- D26 holds a routed admission slot until the ordered `b_transport()` call is
  ready to return. `network_idle()` now answers only whether physical
  mesh/adapter work remains, while `wrapper_idle()` also accounts for held
  admission slots and active bypass calls. The directed gate and the two
  registered mutations independently prove both halves of this contract.
- Step 10.3 added bounded deterministic-random TLM-adapter stress with three
  concurrent managers and closed the clock-gating proof: production gating now
  requires both wrapper idle and complete mesh/chimney quiescence.
- Step 10.1 split `noc_soc` into mandatory, mutually exclusive survey and
  firmware modes. Firmware mode emits zero synthetic transactions; survey
  retains RAM/peripheral/DMA coverage; ELF `PT_LOAD` overlap with the reserved
  final 4 KiB RAM page is rejected before internal SoC construction.
- Step 10.4 proved the distribution boundary: clean-prefix installation and an
  independent consumer of `cdc::components::noc_interconnect` pass; the
  packaged `noc_soc` runs with exact `$ORIGIN` RPATH; Apache-2.0, SHL-0.51,
  CPU MIT, NOTICE, third-party inventory and pinned FlooNoC provenance ship at
  the applicable package level. `noc_soc_packaging_regression` now reproduces
  every one of those checks from a fresh private build/package root, including
  the packaged firmware regression.
- Step 10.5 removed the wrapper-only one-in-flight limit without changing the
  frozen RTL configuration: one upstream port now has a bounded FIFO of
  per-call waiters, independent B/R completion queues and exact per-transaction
  target-delay ownership. The default/hard bound is 32; `MaxUniqueIds = 1` and
  the constant downstream ID remain selected.
- Step 11 added a construction-time fast backend beside the unchanged detailed
  reference. It reuses payload validation, AXI lane shaping, target replay and
  response mapping, but annotates a calibrated no-contention estimate instead
  of ticking the mesh. The estimate is checked from one to six hops over narrow,
  full-width and burst traffic with a hard one-cycle tolerance.
- Gate V0-CLOSE added direct standalone tests for the last two uncovered leaf
  interfaces, `rr_arb_tree.hpp` and `meta_buffer.hpp`, plus executable
  mutations. The original close was 40/40 component tests and 42/42 controls;
  metrics later raised it to 41/41 and 51/51, and signed v1.5 now carries
  42/42 and 57/57 after D1/D26.
- Step 12.0 froze the FreeRTOS/`noc_soc` contract and added
  `fw/freertos_noc_soc`: a mandatory `NPU=0` profile whose linker ends at
  `0x80fff000` and whose checker validates the entry, stack and every ELF
  `PT_LOAD`. Its 700 ms fast-mode compatibility probe boots the reused
  scheduler, CLINT tick and TIMER0/PLIC path with zero synthetic firmware
  traffic.
- Step 12.1 replaced that compatibility application with a dedicated minimum
  `FreeRTOS NoC` image. Priority-1 and priority-2 tasks complete eight strict
  notification ping-pong rounds (16 ordered handoffs), then print
  `FreeRTOS NoC PASS`. Its bounded self-checking fast runner and a separate
  6 ms detailed smoke both pass with zero synthetic traffic.
- Step 12.2 added independent CLINT and TIMER0/PLIC proofs while keeping the
  level-121 scheduler image reproducible. Three `vTaskDelay(1 ms)` wakes prove
  the 1 kHz tick; exactly three level-sensitive TIMER0 interrupts are cleared
  device-first and completed through PLIC source 4. The self-checking 50 ms
  fast run and a 9 ms detailed smoke pass with zero synthetic traffic.
- Step 12.3 added a sequenced FreeRTOS DMA task at build level 123. Its channel
  program and buffers are linker-bounded firmware objects; completion is one
  direct ISR notification through PLIC source 7, with event W1C before claim
  completion and no polling acceptance. The task verifies channel state,
  interrupt cleanup, final SAR/DAR and all 32 bytes. The self-checking 100 ms
  fast run and 12 ms detailed smoke pass with no abort and zero synthetic
  traffic.
- Step 12.4 made build level 124 the default and overlapped every earlier
  proof: two priority-1 CPU tasks driving their own RAM buffers and one checked
  NoC-crossing MMIO register each, a free-running TIMER0 through PLIC source 4,
  and repeated 4 KiB DMA transfers completing through source 7, with a
  supervisor task as the only printer. Forward progress is required at every
  checkpoint, and concurrency is proved by counting worker words strictly
  between a DMA launch and its completion interrupt. Two earlier formulations
  were rejected by their own checks: a 32-byte transfer is shorter than one CPU
  iteration and showed zero overlap, and TIMER0 at the Step 12.2 rate saturated
  the phase with trap handling. The 500 ms fast run passes with zero synthetic
  traffic and is byte-identical across repeats, and a 45 ms detailed run
  reaches the same acceptance at about 45x the host cost.
- Step 12.5 registered the FreeRTOS acceptance as
  `noc_soc_freertos_regression` (labels `firmware;freertos`, skip 77, timeout
  900). Its runner is a thin platform-level wrapper over the existing staged
  contract rather than a second oracle. It now drives levels 121, 122, 123,
  124 and 128, so the independent reproducibility of the earlier stages is
  checked, not asserted. The packaging gate runs the strongest level 128
  image against the packaged executable with `LD_LIBRARY_PATH` unset.
- Step 12.6 added the platform/firmware control registry, since extended for the
  host-assisted dashboard:
  `noc_soc_freertos_negative_controls`: **11 detected, 0 missed**. Each
  control records two expectations — what the runner must name and what the
  platform log must contain — so a mutation that merely broke the build cannot
  score as a detection, and absence-based evidence is written `!text` with the
  clean run required to have produced it. Writing them produced two findings:
  a wrong DMA destination must land where nothing else is verified, and the
  CLINT control cannot be written on the firmware side at all because the
  existing compile-time address assertions already reject that whole
  misconfiguration class. Two additional controls pin the Step 12.4 proof
  itself: final-generation RAM corruption and loss of in-flight worker
  progress. The eighth keeps the CLI task present but removes UART RX/PLIC
  arming, proving host commands are not accepted through a fake firmware path.
  The ninth removes the dashboard's private UART request while leaving its
  visible CLI acknowledgement intact. Controls ten and eleven corrupt a
  hardware-scan identity and bypass the register test's RW pattern write.
- Step 12.7 recorded the measurement baseline. `noc_interconnect` gained a
  passive completion observer, because PLIC claim and PLIC complete are the
  same target at the same address separated only by direction, so no
  per-target counter can split them and polling a global afterwards attributes
  one to the other. The archived artifact at
  `platforms/noc_soc/evidence/noc_baseline.txt` carries its own provenance and
  no thresholds. DMA start-to-interrupt timing begins at the DMA channel's
  architectural `STOPPED -> EXECUTING` transition; using completion of the
  DBGCMD NoC transaction was rejected because that response arrives after the
  channel has already started. The same workload in fast mode leaves
  the DMA-active RAM bucket empty, which demonstrates rather than asserts that
  fast mode cannot answer a contention question. Production request/response
  router counters are now attached after D1 of the metric-dashboard roadmap;
  the archival runner requires their flit/stall/FIFO records and stores the
  versioned JSON plus rendered dashboard beside the text baseline. A final
  continuation review rebuilt from fresh `/tmp` trees, passed the 40/40 suite,
  four-level FreeRTOS regression and 42/42 component controls, and reproduced
  the detailed artifact. It also made the completeness runner validate anchored
  numeric provenance/manager/mesh/tick records, preserve dirty source as a
  checksummed patch alongside revision and firmware/platform hashes, and write
  CTest evidence into the binary tree rather than the source tree.
- Step 12.8 added the FreeRTOS UART console without changing levels 121 to 125.
  The `noc_soc` platform now accepts deterministic UART0 file replay and a
  bidirectional loopback TCP socket in firmware mode. Both enter the pin-side
  UART RX FIFO; PLIC source 1 and the existing FreeRTOS ISR queue deliver bytes
  to the new level-128 CLI. Its parser is released only after the complete
  level-124 workload reports PASS. The final CLI has `help`, `soc`,
  `noc_dashboard`, `hw_scan` and `reg_test`. `noc_dashboard` sends a private
  UART request to `noc_soc`; in detailed mode with `--noc-metrics`, the
  platform atomically publishes live JSON and `tools/noc_cli.py` renders it
  with the same full `noc_dashboard.py` used offline. The scan verifies 22
  mapped blocks while safely reporting three reserved and two absent windows;
  the restoring register matrix passes 37/37. The five-level regression,
  packaged level-128 run, 11/11 mutation registry, UART bridge unit test and
  host-client TCP protocol test pass. A TCP session must wait for
  `FreeRTOS NoC CLI ready` before sending a burst because `--uart0-wait`
  synchronises client connection, not firmware RX arming.
- FlooNoC metrics D0-D6 v1 is complete. Both production physical meshes expose
  passive router counters; transaction completion has exact histograms and
  manager/target/flow attribution; `noc_soc --noc-metrics` writes diagnostic
  firmware JSON; and the separate production-wrapper `noc_benchmark` owns the
  drainable warm-up/measure/stop/drain path required for conservation and DSE.
  `noc_dashboard.py` renders JSON without third-party dependencies, while
  `noc_sweep.py` writes per-run evidence, aggregate JSON/CSV and refuses failed,
  undrained or constraint-rejected winners. The 15 required metrics mutations
  are all covered: the full component registry passes 57/57 and the separate
  D3-D6 platform registry passes 10/10, including one behavioural mutation per
  synthetic workload. D7 area/power remains explicitly unavailable pending
  calibrated RTL evidence. See
  `docs/NOC_METRICS_DASHBOARD_IMPLEMENTATION.md`.
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
| `test_rr_arb_tree` | The round-robin tree directly: `NumIn` 1, 4 and 5 (the router's own non-power-of-two width); no/one/competing requests; `rr_q`-selected priority; the `lock_q ? req_q : req_i` path; request bits above `NumIn` ignored; and `next_rr()` walking above the pointer and wrapping through the lower mask |
| `test_meta_buffer` | The `MaxUniqueIds == 1` metadata FIFO directly: the all-ones downstream ID at five `OutIdWidth` values including 63; independent read/write state; three-entry FIFO order with the payload checked whole; exact full/outstanding transitions; overflow and underflow both refused; invalid construction rejected on both sides of the boundary |
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
| `test_axi_noc_chimney` | A-2 signal-driven composition: one complete chimney per node over a 2x2 raw mesh, with two-beat write, three-beat read, B/R back-pressure, downstream-ID rewrite/restoration and RoB/metadata release |
| `test_chimney_timing_trace_sc` | 141-cycle chimney request-path timing trace against the RTL-captured golden |
| `test_chimney_rsp_timing_trace_sc` | 221-cycle chimney response-path and subordinate-side timing trace against the RTL-captured golden |
| `test_mesh_trace_sc` | 1872 node-cycles of a 3x3 two-network mesh against the RTL-captured golden |
| `test_router_trace_sc_d2` / `_d0` | 214-cycle router trace against the RTL-captured golden, at output-FIFO depth 2 and 0 |
| `test_noc_interconnect` | The TLM wrapper contract through A-3's timed chimney path, each item a concrete assertion: payload validation (command, zero length, null pointer, wrapped streaming width, zero-length byte enables); byte-enable to `WSTRB` translation with a disabled byte proven unchanged in target memory; lane placement at `+4` and `+6` and across a beat boundary, checked by direct memory inspection; `DECERR` for an unmapped address distinguished from `SLVERR` for a target that refuses; region-crossing refused; delay contract (incoming delay spent, sub-cycle target latency rounded up, 1.5 and 2.0 cycles both costing 2, measured on four targets sharing one node); reset-time submission and idle-to-active wake-up; a scoreboard over a second initiator's writes; `last_latency_cycles()` excluding the target hold-off; bounded waits and a global watchdog. Round 3 added the 256/257-beat `AxLEN` boundary, sparse multi-beat writes, widened-read policy, top-of-address-space accesses and all four AXI response mappings. A-3 moved it to a 4x4 topology and pins the complete signal-driven no-contention baseline at **10 cycles for one hop and 30 for six** |
| `test_noc_interconnect_stress` | Step 10.3 bounded deterministic-random stress: three staggered managers contend for one delayed RAM using 1/2/4/6/8/13/24-byte transfers plus sparse multi-beat writes. Per-requester byte and completion scoreboards verify address, data, response, target order, requester attribution and target-delay accounting. It proves the half-cycle injection window is exercised, checks `SLVERR`/unmapped `DECERR`, observes mesh-idle/wrapper-busy as a legal state, requires whole-network quiescence before clock gating, verifies idle wake-up, and bounds every transaction plus the full run with watchdogs |
| `test_noc_interconnect_concurrency` | Step 10.5 same-port concurrency: seven `b_transport` calls from independent SystemC threads share one upstream socket; a configured capacity of two is saturated; read/write FIFO completion order, simultaneous B/R ownership, data/error ownership, slot release, per-transaction target-delay exclusion and final quiescence are checked with bounded watchdogs |
| `test_noc_interconnect_observer` | Step 12.7 passive completion observer: one record per completion in both timing modes, correct requester/address/length/direction, per-record latencies summing to the interconnect's own total, concurrent managers keeping their own attribution, and identical traffic with and without an observer producing the same transaction count and measured latency |
| `test_noc_interconnect_fast` | Step 11 model-to-model calibration, not RTL equivalence: detailed and fast instances receive the same reads/writes over one through six Manhattan hops, 1/2/4/8-byte widths and 32-byte bursts. A hard one-cycle tolerance pins network timing; exact checks cover incoming-delay preservation, target-delay ceiling, zero internal time advance, sparse unaligned writes, data/error equivalence, target effects, exception-time slot cleanup and mesh bypass |
| `test_axi_lanes` | `AxSIZE`, `AxLEN`, lane offset and per-beat `WSTRB` for 13 address/length shapes, plus byte-enable holes and short repeating enable arrays. Checked on the fields themselves, not through a target, because a packing error and a matching unpacking error cancel |
| `test_noc_interconnect_bad_config` | Configurations refused before any traffic, each rejected while still an ordinary function call so teardown is normal: zero or more than eight upstream ports for the frozen 3-bit AXI ID; a per-port outstanding bound outside 1..32; an unknown timing backend; a non-positive clock period; a target on an initiator's node, including the documented default `(0,0)` of a port never placed; a manager moved onto an existing target; zero-sized, address-space-wrapping and overlapping regions; and proof that a refused call consumes no target slot and leaves a port's position intact. A legal layout is still accepted |
| `test_axi_lanes_odr` | Two translation units including `axi_lanes.hpp` in one link. The only test that can catch a missing `inline` |
| `test_axi_chimney_manager_response` | The manager-side response unpacker: channel decode, per-channel back-pressure, a request channel refused on the `rsp` link, and the AW→B / AR→multi-beat-R counter release loop |
| `test_chimney_mgr_rsp_trace_sc` | 78-cycle manager-side response trace against the RTL-captured golden: the AXI manager's B and R channels in full — id, resp, the whole 64-bit `RDATA`, `RLAST`, `BUSER`/`RUSER` — plus `floo_rsp_o.ready` and both per-id reorder-buffer counters, which must drain to zero |

All forty tests pass with GCC 11.5.0 and SystemC 2.3.4.

Two rows carry a caveat, and they are not the same caveat:

- `test_noc_interconnect` covers the TLM wrapper, which has **no RTL
  counterpart** and so cannot ever be signed. It is the highest-risk correctness
  layer in the component; Step 10.3 now gives it dedicated multi-initiator
  stress and seven automated negative controls.
- `test_axi_chimney_manager_response` covers a module that **does** have an RTL
  counterpart — the manager-side response path of `hw/floo_axi_chimney.sv` — and
  that counterpart is now signed: Step A-1, `run_chimney_mgr_rsp_crosscheck.sh`,
  78 cycles exact. The unit test is kept alongside the cross-check rather than
  replaced by it: the unit test says the module's own contract broke, the
  cross-check says the RTL disagrees, and the `rlast-ignored` /
  `rlast-ignored-vs-rtl` control pair keeps both statements honest.

## Accuracy status

RTL-signed blocks:

| Block | RTL reference | Evidence |
|---|---|---|
| XY route selector | `hw/floo_route_select.sv` | 12 cycles exact, pre-edge and post-edge (`route_sel_id_o` and `locked_route_q`), which separates the edge that takes the route lock from the edges that merely hold it |
| Input FIFO | `common_cells` `stream_fifo_optimal_wrap` | 133 cycles exact at depth 2 and depth 4 (pre-edge and post-edge `ready_o`/`valid_o`/`data_o`) |
| Wormhole arbiter | `hw/floo_wormhole_arbiter.sv` over `common_cells` `rr_arb_tree`/`lzc` | 152 cycles exact at 5, 4, and 2 routes (pre-edge and post-edge `ready_o`/`valid_o`/`data_o`/selected index, plus `valid_q`, `last_q`, `rr_q`, `lock_q`, `req_q`) |
| Five-port router | `hw/floo_router.sv` | 214 cycles exact at **both** `OutFifoDepth = 2` (what every generated router has) and `0` (the `gen_no_out_fifo` bypass): pre-edge and post-edge per-port `ready_o`/`valid_o` masks and per-output `data_o`, plus the one-hot route mask per input |
| AXI flit sizing | `floo_pkg` sizing functions over `axi 0.39.9` `axi_pkg` | 8 configurations exact (per-channel width, channel-to-link mapping, physical channel width, reserved bits) |
| Chimney request path | `hw/floo_axi_chimney.sv` in the `floo_test_pkg` parameter set | 16 flits exact (channel, destination, source, `last`, `atop`, `rob_req`, `rob_idx`, payload). Flit content and per-beat ordering only, not chimney timing |
| Chimney response path | same, driven as a subordinate | 8 flits exact, with three transactions outstanding per batch so the metadata FIFOs are genuinely exercised |
| `NoRoB` ordering rule | `hw/floo_rob_wrapper.sv` over the locked axi `axi_demux_id_counters` | 127 cycles exact (`ax_ready_o`/`ax_valid_o`/`rsp_*` per cycle, plus `in_flight`, `prev_dest`, `counter_full`, and every counter and destination register in the bank) |
| Chimney request-path **timing** | `hw/floo_axi_chimney.sv` in the `floo_test_pkg` parameter set | 141 cycles exact (`aw_ready`/`w_ready`/`ar_ready` and the `req` link's `valid`, channel, destination, `last` and payload per cycle, plus `aw_w_sel_q` and every request-arbiter register) |
| Inter-node mesh | a grid of `hw/floo_axi_router.sv`, wired as the FlooGen netlist wires it | 1872 node-cycles exact (per node and per network: local inject `ready`, eject `valid`, and the ejected flit's channel, destination, `last` and payload tag, pre-edge and post-edge) |
| Chimney **manager-side response** | same, driven on `floo_rsp_i` | 78 cycles exact (`axi_in_rsp_o`'s B and R channels qualified by their valid — id, resp, the full 64-bit `RDATA`, `RLAST` and `BUSER`/`RUSER` — plus `floo_rsp_o.ready`, manager-side `aw_ready`/`ar_ready`, and the `NoRoB` counter `in_flight` for two read ids and one write id, asserted to drain to zero). The other three chimney runners pin `floo_rsp_in.valid = 1'b0`, so this is the only one that drives the link at all |
| Chimney response path and subordinate side, **timing** | same, driven from the `req` link | 221 cycles exact (inbound `req` `ready`, the reissued `axi_out` request boundary, the `axi_out` response `ready` signals, the `rsp` link's `valid`/channel/destination/id, plus both metadata FIFO occupancies and every response-arbiter register) |

Each of those blocks is signed **in isolation**: the chimney's **request path**
(141 cycles), its **subordinate side** (221 cycles), and its **manager-side
response unpacker** (78 cycles, Step A-1), plus the mesh between them. All four
chimney quadrants are signed.

That is still not the same as one composed RTL cross-check. A-2 now instantiates
the timed chimney at every node in the signal-driven `axi_noc`, and
`test_axi_noc_chimney` verifies the SystemC wiring in 56 cycles. A-3 now puts
that `axi_noc` in the integrated TLM path and drives both AXI boundaries cycle
by cycle.

So the composed TLM-boundary latency traverses the individually signed timed
blocks but is not itself a one-piece RTL equivalence path. The TLM-to-AXI
manager/subordinate adapters and the wrapper have no RTL counterpart and cannot
be signed against one; they are covered by model-level integration tests.

Step A changed what the datapath is made of; Step 10.3 stresses the remaining
unsigned TLM adaptation layer above it.

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
channels as two separate meshes. The raw pair was named `axi_noc` when this
step landed and is named `axi_mesh_noc` after A-2; the new `axi_noc` adds
chimneys around it. The two-network shape comes from the IP, not from a
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

The legacy `test_axi_noc` runs abstract endpoints over `axi_mesh_noc` on a 4x4
mesh. It measured **1 hop 11 cycles, 6 hops 30 cycles** after the output-FIFO
correction. A-3's production `noc_interconnect` no longer uses that path:
`test_noc_interconnect` now measures the complete signal-driven `axi_noc` at
**1 hop 10 cycles, 6 hops 30 cycles**, excluding target delay through
`last_latency_cycles()`.

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

The old 2x2 endpoint-transactor measurements are historical and do not describe
the production path. A-3's 4x4 `noc_soc` path drives the complete timed
`axi_noc` at a 1 ns network clock. Its directed no-contention baseline is
10 cycles for one hop and 30 cycles for six hops; the platform report provides
the workload-level figures.

Three behaviours a platform has to plan for:

- **Bounded concurrent blocking calls per upstream port.** Step 10.5 replaces
  the wrapper's single waiter with per-transaction request slots and independent
  FIFO completion queues for B and R. The constructor bound defaults to 32 and
  may be reduced, but not raised beyond the frozen `MaxTxns = 32`. This does not
  change `MaxUniqueIds = 1`: downstream traffic still uses one reissue ID and
  responses must preserve FIFO order within each direction.
- **The two timing modes have deliberately different `b_transport` contracts.**
  Detailed mode spends incoming plus cycle-stepped time and returns zero delay.
  Fast mode never waits inside the interconnect: it preserves the caller's
  annotation and adds request, rounded target and response estimates. Its
  downstream targets must follow the same LT rule and annotate rather than
  call `wait()` themselves. Bremen's assert-enabled quantum keeper passes fast
  firmware; detailed mode remains unsuitable for a caller that requires
  nondecreasing local-time annotation.
- **A burst is one packet.** `axi_transaction` now carries a beat vector and
  the manager endpoint emits AW plus N W flits with `last` on the final one.
  Splitting a burst into N transactions would lose the route holding that
  distinguishes a NoC from a bus.

Two implementation notes worth keeping:

- **Idle cycles are skipped, and that is exact.** With no `valid` asserted
  anywhere every register in the mesh holds, so a skipped cycle changes
  nothing. It keeps a mostly-idle platform from paying for the interconnect.
- **A target's own latency becomes a per-transaction, per-direction hold-off in
  cycles**, not a `wait` inside the network thread. Waiting there would freeze
  every other node's traffic for the duration, which a real subordinate does
  not do. A requester-wide accumulator is insufficient under same-port
  concurrency because one completion could consume another call's delay.

**The bug that cost the most to find:** a `b_transport` issued at time zero
handed flits to a mesh still in reset, which swallowed them and hung the
simulation. The wrapper now blocks until reset has elapsed, and `step_once`
refuses injections while `rst_n` is low.

**Verification status.** `test_noc_interconnect` contract-tests the complete
A-3 TLM-to-signal-driven path; `test_noc_interconnect_stress` adds bounded
three-manager scoreboarding plus whole-network quiescence; and
`test_noc_interconnect_concurrency` covers multiple callers on one port. The
hardware blocks underneath are RTL-signed individually. The TLM adapters are
not, and cannot be, because they have no RTL counterpart.

### Legacy abstract endpoints — reference/test code only

`include/floo_noc_model/axi_endpoint.hpp` adds manager and subordinate
transactors that compose the signed pieces: packing, destination decode,
metadata retention, and the ordering gate.

Since A-3 these transactors are no longer in the production datapath. They
remain a transaction-level reference abstraction with **no RTL counterpart at
all**, so no timing claim follows from them. `no_rob_order_gate`, the
transaction-level face of the ordering rule that they call, is the same
arithmetic as the signed `no_rob_gate` without the clock; it is a convenience
wrapper, not a second model.

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

**Verification status: signed, by five separate cross-checks.**
`tests/test_axi_chimney_pack.cpp` on its own is only a contract test against the
RTL text, so read it as such. The equivalence proof is elsewhere on this page:

| Cross-check | Section above | Result |
|---|---|---|
| request content | "Chimney request path" | 16 flits |
| response content | "Chimney response path" | 8 flits |
| request timing | "Chimney request-path timing" | 141 cycles |
| response and subordinate side | "Chimney response path and subordinate side, timing" | 221 cycles |
| manager-side response unpacker | "Chimney manager-side response" | 78 cycles |

The packing is inline `always_comb` and could not be isolated, so the harnesses
instantiate the whole chimney, which pulls in the meta buffer and the `NoRoB`
gate as well. That turned out to be the right thing to do: the request-timing
harness is what found the reversed arbiter index order, and no isolated packing
harness would have.

The five testbenches live in `rtl_crosscheck/axi_chimney/`. The oldest of them
began as an elaboration-only file, `tb_floo_axi_chimney_elab.sv`; it became
`tb_floo_axi_chimney_req_trace.sv` once it carried stimulus, so that name no
longer exists.

The upstream `hw/tb/tb_floo_axi_chimney.sv` cannot be reused under Verilator:
it depends on the class-based `axi_test` package. It remains usable under VCS,
which is installed on this host. The five harnesses here are hand-written BFMs
instead; the "Harness lesson" section above is the price that was paid for
that.

### Negative controls: what is automated, what is not

A test suite that has never been seen to fail is not evidence. Every fix in this
component was accompanied by an injection that must be detected — but until
2026-07-30 those injections lived only in prose here, so reproducing one meant
editing by hand and nothing checked that they still bite.

**Automated.** `rtl_crosscheck/run_negative_controls.sh` holds the model-level
controls as executable records: file, exact literal, replacement, the test that
must then fail, **the message that failure must contain**, and what the defect
was. It applies each one, rebuilds, and requires both a non-zero exit and that
message. An injection that stops failing is reported as `MISSED` and the script
exits 1 — either the defect became unreachable or the test stopped covering it,
and both need a human decision rather than a silent pass.

Requiring the message is not decoration. Exit status alone counted a mutation
that merely broke the syntax as equal to one that broke behaviour, and a test
failing for an unrelated reason as evidence that it covers this defect.

**Where it runs:** in a private copy of the component under `/tmp`, one per
control. The working tree is never touched, so an interrupt or a lost machine
cannot leave a mutated source behind, and two runs cannot collide. It is not
registered in CTest only because it rebuilds the component forty-two times
and belongs in a slower loop than the unit tests.

Forty-two controls, all detected:

| Control | Test that must fail | Defect it restores |
|---|---|---|
| `lane-placement` | `test_axi_lanes` | the first payload byte always went to lane 0, so a narrow access at a non-zero offset wrote the wrong half of the bus |
| `byte-enables-ignored` | `test_axi_lanes` | byte enables were never read, so a partial write became a full one |
| `target-delay-truncated` | `test_noc_interconnect` | a target latency shorter than one network cycle was rounded down to free |
| `incoming-delay-dropped` | `test_noc_interconnect` | the caller's annotated time was discarded instead of spent |
| `offer-not-atomic` | `test_axi_endpoint` | capacity was checked after the request flits had been queued, so a full metadata buffer threw with a burst already in flight |
| `rlast-ignored` | `test_axi_chimney_manager_response` | every beat of a read burst released a reorder-buffer counter, not just the last |
| `region-decode-addition` | `test_noc_interconnect` | the mapped-region decode used an addition that wraps, making a region whose last byte is `UINT64_MAX` unreachable |
| `axlen-truncated` | `test_axi_endpoint` | a burst longer than `AxLEN` can encode was narrowed instead of rejected, so 257 beats became `ARLEN = 0` and one beat was returned |
| `tlm-mapping-slverr-decerr-swapped` | `test_noc_interconnect` | the two AXI error codes were exchanged at the TLM boundary, so a decode failure looked like a target refusal and the reverse |
| `tlm-mapping-exokay-as-error` | `test_noc_interconnect` | `EXOKAY`, the success code for an exclusive access, was reported to the caller as a failure |
| `b-response-discarded` | `test_axi_endpoint` | the B response code was thrown away, so a failed write completed as OK |
| `sparse-beat-renumbering` | `test_noc_interconnect` | a write whose leading beat was fully disabled had its data placed one beat too early, and still reported success |
| `widened-read-policy-removed` | `test_noc_interconnect` | a read wider than the request reached an MMIO target, where a neighbouring register may clear on read |
| `r-burst-error-lost` | `test_axi_endpoint` | an error on an intermediate R beat was erased by a later OKAY |
| `self-node-check-skips-default` | `test_noc_interconnect_bad_config` | a target on an unplaced port's documented default `(0,0)` was accepted during configuration and rejected only at `end_of_elaboration()` |
| `place-initiator-not-atomic` | `test_noc_interconnect_bad_config` | a rejected placement moved the port anyway, so the throw reported a failure that had already been committed |
| `beat-frame-guard-removed` | `test_noc_interconnect` | a transfer whose beat frame leaves its target's region was accepted; the read then fetched bytes from outside the mapping |
| `rlast-ignored-vs-rtl` | `chimney_mgr_rsp_trace_sc` | every beat of a read burst released a reorder-buffer counter instead of only `RLAST`, and the RTL counter disagrees from the first beat of the burst |
| `rsp-ready-channel-swapped` | `chimney_mgr_rsp_trace_sc` | `floo_rsp_o.ready` was selected by the wrong channel, so a B flit followed the R manager's ready and the reverse |
| `rdata-upper-word-truncated` | `chimney_mgr_rsp_trace_sc` | the upper 32 bits of `RDATA` were dropped, which the first version of the A-1 cross-check could not see because it traced only the low word |
| `ruser-dropped` | `chimney_mgr_rsp_trace_sc` | `RUSER` was not forwarded to the manager, which no trace covered until the payload was traced in full |
| `buser-dropped` | `chimney_mgr_rsp_trace_sc` | `BUSER` was not forwarded to the manager |
| `r-pop-id-from-b-payload` | `chimney_mgr_rsp_trace_sc` | the R counter was released by the id in the B payload rather than the R payload, so a burst decremented the wrong per-id counter |
| `chimney-node-rob-ready-swapped` | `test_axi_noc_chimney` | the composed A-2 node crossed its private B/R reorder-buffer ready links, so an R response followed B's ordering state and corrupted the stalled R payload |
| `a3-manager-aw-address-shifted` | `test_noc_interconnect` | the A-3 manager adapter drove an AW address one bus beat away from the original TLM request, so writes completed at the wrong target bytes |
| `half-cycle-request-reread` | `test_noc_interconnect_stress` | the sample phase consumed a request that arrived after drive, although no AW was presented on the manager signals |
| `write-requester-metadata-collapsed` | `test_noc_interconnect_stress` | all write source metadata became node zero, so target hold-off was charged to the wrong requester |
| `same-port-response-owner-collapsed` | `test_noc_interconnect_concurrency` | an R response status was delivered to the newest same-port waiter instead of its FIFO owner |
| `same-port-capacity-off-by-one` | `test_noc_interconnect_concurrency` | a wrapper configured for two admitted a third concurrent call |
| `same-port-request-order-reversed` | `test_noc_interconnect_concurrency` | the per-port request queue injected newest-first under a one-ID FIFO-order contract |
| `same-port-target-delay-dropped` | `test_noc_interconnect_concurrency` | a completion charged peripheral delay as network latency because its per-transaction hold-off was discarded |
| `odd-burst-tail-dropped` | `test_noc_interconnect_stress` | the final byte was omitted from WDATA/WSTRB, corrupting narrow, odd and multi-beat transfers |
| `clock-gate-does-not-require-both-predicates` | `test_noc_interconnect_stress` | the clock stopped when either wrapper or mesh appeared idle instead of requiring both |
| `mesh-output-fifo-occupancy-ignored` | `test_floo_mesh` | a stalled output-FIFO flit was omitted from the mesh activity snapshot |
| `router-packet-locks-ignored` | `test_floo_router` | an open route/arbiter packet lock with empty FIFOs was misclassified as quiescent |
| `completion-observer-attribution-lost` | `test_noc_interconnect_observer` | every completion was attributed to port 0, which is the attribution loss the observer exists to prevent |
| `fast-hop-cost-removed` | `test_noc_interconnect_fast` | the approximately-timed request path ignored Manhattan distance |
| `fast-incoming-delay-dropped` | `test_noc_interconnect_fast` | the fast backend replaced the caller's local time instead of preserving it |
| `fast-target-delay-truncated` | `test_noc_interconnect_fast` | the fast backend truncated a fractional target cycle instead of rounding up |
| `fast-functional-replay-bypassed` | `test_noc_interconnect_fast` | the fast timing path bypassed the mapped target instead of abstracting only time |
| `rr-arb-tree-wrap-ignored` | `test_rr_arb_tree` | the `FairArb` pointer stopped wrapping through the lower request mask, so once no requester sat above `rr_q` the arbiter walked off its inputs instead of returning to the lowest one |
| `meta-buffer-overflow-accepted` | `test_meta_buffer` | the metadata FIFO accepted a push beyond `MaxTxns`, modelling a deeper buffer than the frozen `fifo_v3` has and losing the back-pressure the chimney relies on |

The runner has its own failure mode worth recording, because it produced a false
pass on its first run. Records were packed into `|`-delimited strings and read
with `read`, which stops at the first newline — so every control whose literal
spanned several lines lost its remaining fields, invoked `cmake --build` with an
empty target, and counted CMake's usage output as "the defect was detected".
Four of six controls had never executed. Records are parallel arrays now, since
the literals routinely span several lines, and an empty required field is a hard
error before anything is built. **A control mechanism needs its own control:**
injecting a comment-only change must be reported `MISSED`, and it is.

**Manual.** The RTL cross-check controls stay manual and are documented in the
sections above, one per cross-check. Each needs Verilator and Bender and takes
minutes rather than seconds, and — more importantly — several of them
legitimately *pass*, because the frozen RTL is redundant at that point. The
wormhole arbiter's snapshot and the tree's `LockIn` implement the same packet
hold, so removing either alone changes nothing. Deciding whether a passing
control means "equivalent rewrite" or "coverage gap" is a judgement, and rule 9e
in `AI_HANDOFF_CONTEXT.md` exists because that judgement was needed twice.

### Cross-check strength

The FIFO trace records both the pre-edge sample (the handshake view the
environment acts on) and the post-edge sample. Post-edge-only sampling was
proven insufficient: it hides an output that wrongly depends on the current
`ready_i`. The arbiter trace additionally exports every arbiter register, so a
state divergence fails even when the outputs still agree. Fourteen negative
controls were run against the cycle harnesses below; the chimney request and
response harnesses add three and four more of their own, for twenty-one in
total.

That twenty-one counts **manual** harness controls only — the ones tabulated
below, each run by hand against its own cross-check. The manager-side response
harness added by Step A-1 is validated differently: its six controls are
**automated**, registered in `run_negative_controls.sh`, and are counted in the
thirty-two above. They are deliberately not added here, because the two totals
are different sets and summing them would count the same six twice.

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

Three of the four items of the previous list are done: the meta buffer and the
`NoRoB` gate are modelled and signed, the mesh endpoints are chimney-backed AXI
endpoints on two physical networks, and `noc_interconnect` is the CDC-VP M:N
fabric adapter. Note item 3's premise was wrong: the mesh was signed against a
hand-built grid of the frozen `floo_axi_router`, so FlooGen was never needed.

The chimney item **is** done as of Step A-1, but "cross-checked in both
directions" was never the right way to say it — the chimney has four quadrants,
not two. All four are now signed: manager request timing (141 cyc), subordinate
request reception and response generation (221 cyc), and the manager-side
response unpacker `axi_chimney_manager_response` (78 cyc,
`run_chimney_mgr_rsp_crosscheck.sh`).

**Step A is done through A-3.** `axi_noc` has one `axi_chimney_node` per
coordinate over the raw `axi_mesh_noc`; `noc_interconnect` now drives its
manager AW/W/AR and subordinate B/R signals cycle by cycle. The legacy
`axi_endpoint.hpp` transactors are no longer in the production datapath.
`test_axi_noc_chimney` remains the 56-cycle composition test, while
`test_noc_interconnect` covers the complete TLM path, concurrent managers and
the new 10-cycle/30-cycle one-hop/six-hop baseline. This is still model-level
integration over individually signed blocks, not a composed RTL harness.
A-3 reruns remain exact: request content 16 flits, request timing 141 cycles,
response content 8 flits, subordinate/response timing 221 cycles, manager
response 78 cycles, and mesh timing 1872 node-cycles. The platform firmware
regression, run after rebuilding the parent `noc_soc` target against A-3,
passes both `DMA PASS` and the synthetic survey; the post-A-3 firmware run
retired 10,726 instructions in 570,683 ns of modeled time (~53.2
ns/instruction, not a host-performance metric).

The authoritative, ordered list is `AI_HANDOFF_CONTEXT.md` section 14, and its
sub-steps deliberately do **not** run in numeric order:

1. ~~**Step 10.2** — an automated real-firmware regression.~~ **Done**
   (2026-07-30): `platforms/noc_soc/tests/run_firmware_regression.sh`, registered
   as `noc_soc_firmware_regression`. Step 10.1 extended it with explicit
   mode-selection failures, zero-synthetic-ownership assertions and a relocated
   ELF that must be rejected at the reserved scratch page.
1b. ~~**Step A** — compose the RTL-signed chimney into the datapath, replacing the
   abstract endpoint transactors.~~ **Done through A-3**. ~~A-1, cross-check the manager-side
   unpacker.~~ **Done** (2026-07-31): 78 cycles exact, six negative controls
   detected. ~~A-2, assemble one chimney per node.~~ **Done** (2026-07-31):
   `test_axi_noc_chimney` passes in 56 cycles; 34/34 standalone tests pass.
   ~~A-3, switch `noc_interconnect` to signal-driven AW/W/AR/B/R.~~ **Done**
   (2026-07-31): 34/34 tests, relevant RTL cross-checks and firmware regression
   pass; integrated baseline is 10 cycles at one hop and 30 at six.
2. ~~**Step 10.3** — scoreboard-driven stress on the unsigned integration layer
   that remains **above** the chimney: `noc_interconnect` and its
   TLM-to-AXI mapping. It has no RTL counterpart and every integration defect so
   far has been in it. Also closes the unproven clock-gating condition.
   `axi_endpoint.hpp` remains only as legacy test/reference code and is outside
   this stress target.~~ **Done** (2026-08-01): 35/35 standalone tests,
   32/32 automated negative controls, all twelve RTL cross-checks, and the
   rebuilt `noc_soc` firmware/survey regression pass.
3. ~~**Step 10.1** — separate survey and firmware ownership in `noc_soc`.~~
   **Done** (2026-08-01): explicit mode selection, zero synthetic firmware
   traffic, and enforced reserved-page ELF validation.
4. ~~**Step 10.4** — clean-prefix install, packaging, licence and provenance
   audit.~~ **Done** (2026-08-01): installed compiled-target consumer passes,
   packaged `noc_soc` runs with exact `$ORIGIN` RPATH, and development/binary
   packages contain the required licences and pinned provenance. The registered
   `noc_soc_packaging_regression` is the single-command reproduction gate.
5. ~~**Step 10.5** — decide wrapper concurrency and, separately, whether
   `MaxUniqueIds > 1` is required.~~ **Done** (2026-08-01): bounded concurrent
   calls on one upstream port are implemented and tested; the frozen one-ID
   FIFO-order policy is retained because no current SoC requirement needs
   out-of-order downstream matching.
6. ~~**Step 11** — the fast approximately-timed mode, calibrated against this
   model.~~ **Done** (2026-08-01): one class now selects detailed or fast timing;
   37/37 component tests and 39/39 mutation controls pass; firmware and survey
   pass in both modes. On the recorded AlmaLinux host, the same Release
   firmware/ELF/2 ms window took 8.12 s detailed and 0.08 s fast (~101x in this
   single sample), while both retired 10,726 instructions and reported
   53.1419 ns/instruction. Step 12 is now explicitly authorised and specified
   by `docs/NOC_SOC_FREERTOS_ROADMAP.md`; this sentence records the Step 11
   snapshot rather than the current roadmap boundary.
