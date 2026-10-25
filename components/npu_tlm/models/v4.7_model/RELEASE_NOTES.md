# Release Notes

## 1. Release

SAURIA NPU SystemC model, delivery package: one repository tree that holds the sources, the documents and the data.

| Part | Content | Size |
|---|---|---|
| Source tree | Native model, RTL-ref core, delivery top level `HasNpuTop`, frontend and tools, and the documents of `docs/` | about 12 MB |
| `data/` | The data package `sauria_npu_data` as a split archive (parts below 100 MB): programs, golden images, quantization parameters, unit-test vectors and reference run logs, in the layout of `$FE_WORK` | about 415 MB; about 810 MB extracted |

`package_manifest.txt` in the root lists the sha256 of every file of the tree, including the parts of `data/`
(`sha256sum -c package_manifest.txt`). `bash tools/unpack_data.sh` joins the parts, checks the archive against
`data/SHA256SUMS`, extracts `sauria_npu_data/` and checks every extracted file against the manifest of the data
package. `.gitattributes` disables line-ending conversion so that the checksums hold in a git clone; `.gitignore`
lists the extracted data and the build outputs.

## 2. What this release contains

- **New default: the flow closest to the hardware RTL.** With no knob set, `HasNpuTop` now runs the vector-unit top level
  `GvuTop` on the core clock behind the PSM stream (FIFO of 4 packets, the core stops on the stream back-pressure),
  the residual add and the max pool as jobs on the clock of the unit, and the data flow controller configures and starts
  the unit only through the register file of `gvu_core` (`HAS_GVU_TOP=3 HAS_GVU_STALL=1 HAS_GVU_ELEM_CLK=1
  HAS_GVU_REGS=1 HAS_GVU_AGU_ND=1`). Setting a knob to 0 returns to the path it replaces; with all five at 0 the model
  gives the values and cycles of the previous release. The parts the RTL does not have yet (residual-add registers
  Reg 9..15, multi-dimensional address generation Reg 16..24) are on as well, as the reference for the hardware team
  (`docs/GVU_DFC_REFERENCE_FLOW.md`).
- **Register file of the vector unit and the data flow controller's write sequences**: a port of `gvu_config_regs.sv`
  (16 registers as delivered, 32 with the proposals), the AXI-Lite write port of the controller with a write latency
  parameter (`HAS_GVU_REG_LAT`, default 4, an estimate), the sequences of GEMM_FUSED, max pool and residual add, and the
  end of a GEMM_FUSED layer by the end-of-tile flag of the stream (`docs/GVU_TOP_CORE_INTEGRATION.md`).
- **Slice and concat on the vector unit**: AGU_ND (4-D stride address mode, as the MAS describes) on the ports of the
  residual add and the max pool, and the MOVE operation as a proposal; the compiler's address alias stays the way
  outermost-axis slices and concats are done (`docs/SLICE_CONCAT_ON_GVU.md`).
- **Three new documents**: `docs/GVU_TOP_CORE_INTEGRATION.md` (every flow of the vector unit in the core, the model
  against the RTL, the pitfalls met and how they were solved), `docs/GVU_DFC_REFERENCE_FLOW.md` (block diagram, register
  map and write order of the recommended flow for the RTL), `docs/SLICE_CONCAT_ON_GVU.md` (options, comparison,
  proposed RTL change).
- **Delivery top level `HasNpuTop`**: the NPU driven only by the v4.5 memory-mapped protocol, with a compatibility layer,
  the data flow controller, the cycle-accurate RTL-ref core, the DMA with HAS bus parameters and the vector-unit blocks
  (`docs/ARCHITECTURE.md`, section 7; `docs/SW_INTEGRATION_GUIDE.md`).
- **Run profiles**: the default configuration reproduces the reference results; `--profile proposals` adds the two
  proposed hardware options; `--profile legacy` reproduces older results (`docs/BUILD_AND_RUN.md`, section 6).
- **Vector-unit data flow**: epilogue inline on the partial-sum path, banked scratchpad, 3-D DMA descriptors, planner
  options (input-channel split, padded edge tiles, positions per context).
- **Transformer instructions**: FUSED_ATTN and LAYERNORM through the instruction stream, bit-exact, with the attention
  products on the core.
- **RTL-accurate core in a system wrapper**: model root `core_rtl/` with a drop-in top level, a proposed wrapper
  change for the DRAM window (`tools/has/wrapper/npu_tlm_rich_window.patch`) and a reference firmware sequence
  (`docs/SW_INTEGRATION_GUIDE.md`, section 10).
- **Vector-unit additions**: AVG_POOL on the instruction path (ELEM_WISE mode 5), structure models of the pipeline
  registers and of the load/store unit, a Scratchpad sizing preset (`HAS_SP_PRESET=gvu68k`); the vector-unit files are
  named `has/gvu_*.h` and their unit testbenches `tools/has/tb_gvu_*`.
- **Frontend**: ONNX to tiled int8 program and independent golden, register-write stream generator for YOLOv8m and ViT-B/16.
- **Run roll-up**: `tools/metrics/has_rollup.py` turns the log and per-tile counters of a `tb_has_npu_top` run (or a
  reference run of the data package) into per-instruction and per-operation tables and the network totals
  (`docs/BUILD_AND_RUN.md`, section 7). The ViT-B/16 PE utilization is now reported for the GEMM instructions
  (76.05 %) and including the attention products (69.26 %); the earlier single figure mixed the two
  (`docs/PERFORMANCE_REPORT.md`, section 5.6).
- **Documentation revised for the delivery flow**: the quick start and the data layout use the data package; the
  delivery testbench is listed in `docs/CODE_MAP.md`; the AVG_POOL mode number (5), the native revision (v4.6), the
  run-C per-layer figures (`docs/PERFORMANCE_REPORT.md`, section 3.3) and the ViT-B/16 core share are corrected.
- **Report tables for `tb_has_npu_top` runs**: `tools/metrics/post_fullcore.sh` and `tools/metrics/fullcore_rollup.py`
  now accept the runs of the delivery top level (YOLOv8m and ViT-B/16): network roll-up with the basis of every
  number, latency and throughput summary, PE utilization breakdown, per-layer table with the array columns and rows
  used, controller state distribution (`docs/BUILD_AND_RUN.md`, section 7; `docs/PERFORMANCE_REPORT.md`,
  section 5.7). `tools/fe/fe_vit_top5.py` reads the ViT-B/16 top-5 classes from a DRAM snapshot.
- **More input images**: survey of the integer golden against the floating-point model on 112 images (YOLOv8m) and
  5 images (ViT-B/16), and the hardest of them through `HasNpuTop` (`docs/VERIFICATION_REPORT.md`, V23).
- **Tiling rules**: the rules behind the reference tile plan, with the sign to look for in the per-layer table and
  a measured example each (`docs/FRONTEND_GUIDE.md`, section 11.1).
- **Data inside the repository**: the data package is carried in `data/`; two reference runs were added to it
  (the YOLOv8m baseline before the planner options, and ViT-B/16 with the hardware as drawn).
- **Third-party notices**: `THIRD_PARTY_NOTICES.md`.
- **Two new documents**: `docs/USER_GUIDE.md` (running the two networks step by step, what runs at each step, reading
  the results) and `docs/TILING_GUIDE.md` (tile limits with their formulas, cycle laws, a procedure per layer, worked
  examples checked against the measured runs, reference files).
- **Integration aids for the system wrapper**: the wrapper change `tools/has/wrapper/npu_tlm_rich_window.patch`
  regenerated against the wrapper revision that bundles the v4.6 native model (the earlier patch no longer applied
  there), with the commands to apply it; a firmware starting point in C under `tools/has/wrapper/fw/` (register
  definitions, program replay engine with CRC checks, bare-metal example, host test through the wrapper); the
  repository's `.gitignore` keeps the planner cost table when the tree is committed inside another repository
  (`docs/SW_INTEGRATION_GUIDE.md`, sections 9 and 10).
- **English sources**: the remaining comments and console messages written in unaccented Vietnamese (network
  testbenches, detection decoder, RTL-ref headers, unit tests) are translated; behaviour and results are unchanged.
- The native model at revision v4.6: OBP bias, scale and shift registers accept the full 32-bit range (negative
  bias values), OBP scale and shift tables are readable through the host bus, the Reduction Engine output is
  multiplexed onto the SRAM C write-back, and debug printing is off by default. The native `NpuTop` has no `o_irq`
  port. Native regression passes with a fresh build. The RTL-ref core and `HasNpuTop` do not use these files, and the
  reference results below are unchanged.
- **Vector unit after the micro-architecture specification (V1.0) and the RTL of its functional units**
  (`docs/HAS_VALIDATION.md`, section 6): the default configuration of `HasNpuTop` uses the 128 KB double-buffered
  scratchpad of the specification, with softmax and LayerNorm rows per group limited by the bank sizes, LUT tables of
  256 / 512 entries and the LUT edge behaviour of the RTL. Added: rounding modes ceil, toward zero and away from zero;
  the log-scale LUT mode; a scratchpad access map with capacity checks (`has/gvu_sp_map.h`); state traces of the
  pipeline controller (`has/gvu_pipeline_ctrl.h`); a v32int16 port and address generation in the load/store unit
  model; a unit test against an independent reference (`tools/has/tb_gvu_mas`, `tools/has/mas_ref.py`); the same
  items in the frontend golden (`tools/fe/fe_ref_gvu.py --only mas`). `HAS_SPEC=v47ref` selects the earlier
  configuration and reproduces the earlier reference results exactly; `HAS_SPEC=rtl` and `HAS_SPEC=mas10` select two
  further groups (`docs/HARDWARE_CONFIG.md`, section 6.3).
- **Vector-unit blocks ported from the hardware RTL**: `has/rtl_ref_gvu_fu.h`, `rtl_ref_gvu_lut.h`, `rtl_ref_gvu_vrf.h`,
  `rtl_ref_gvu_scratchpad.h`, `rtl_ref_gvu_vlsu.h` and `rtl_ref_gvu_ctrl.h` follow the RTL of the functional units, the
  LUT unit, the stream registers, the Scratchpad, the load / store unit and the table-load sequence line by line. They
  follow the RTL revision in which the softmax data path of the vector-unit top level is connected (zero flag of the LUT
  carried through its pipeline, load / store unit with address modes) and were compared with a simulation of that RTL
  (Verilator 5, every output at every beat, no mismatch). They are not used by
  the instruction path; the default results are unchanged. `docs/GVU_RTL_CONFORMANCE.md` holds the comparison of the
  functional model with the RTL and what the simulation found in the RTL; `docs/GVU_PIPELINE_STEPS.md` lists every
  pipeline step by step as the specification draws it. The tests are `tools/has/tb_rtl_ref_gvu_*.cpp`, the RTL harnesses
  are in `tools/has/gvu_oracle/` (`run_all.sh` runs all of them), and `tools/has/check_rtl_trace.sh` checks without a
  simulator that the ported blocks still give the outputs the RTL gave.
- **Fix: negative output zero point of GEMM_FUSED.** A negative `ZP_OUT` was decoded as the most negative integer, so
  every output of the layer was clamped to -128 (`has/gvu_obp.h`, the register value is now read as a 32-bit two's
  complement word). Zero points of 0 or above, the other instructions and the cycle counts are unchanged; the unit test
  of the block now writes a negative zero point (`tools/has/tb_gvu_quant.cpp`).
- **Three knobs of the functional model** (off by default): `HAS_SP_PRESET=rtl` sizes the Scratchpad banks as the RTL does,
  `HAS_RANGE_PROBE=1` prints the value ranges of the vector-unit arithmetic against the widths of the RTL units, and
  `HAS_ARITH_RTL=1` takes requant, dequant, the 32-bit add and the LUT lookups from the ported units and counts where
  they differ from the model arithmetic (`has/gvu_rtl_arith.h`; `docs/GVU_RTL_CONFORMANCE.md`, section 4.10). `tools/has/measure_cfg_instr.sh` measures single instructions under a
  list of configurations.
- **PE utilization through a system wrapper**: for every `HasNpuTop` instruction the model root `core_rtl/` now fills
  `PERF_ACTIVE_PE_CYCLES` (real multiply-accumulates) and `PERF_TOTAL_PE_CYCLES` (core busy cycles x 1,024), together
  with `PERF_SA_CYCLES`, `PERF_MAC_OPS` and the DMA port cycles, so software computes the PE utilization from registers
  the wrapper already has (`docs/SW_INTEGRATION_GUIDE.md`, section 10.3). The core busy count is 2 cycles per core pass
  above the count of the instrumented build. Cycle counts are unchanged.
- **Diagnostics for programs of another compiler**: the model root prints a warning line when an instruction is refused
  or retires with an internal error, and `SAURIA_CORE_RTL_RECORD` records the program that reaches `HasNpuTop` for a
  replay with `tools/has/tb_has_npu_top`. `docs/SW_EVALUATION_GUIDE.md` (new) describes how to run, check and compare a
  network through the wrapper and how to read the performance figures.
- **Cycle-level pipelines of the vector unit**: max pool and residual add (`has/rtl_ref_gvu_elemwise.h`), selectable
  in the instruction path with `HAS_ELEM_RTL=1` (off by default), and the Requant pipeline with the GEMM_FUSED epilogue
  (`has/rtl_ref_gvu_requant.h`), which `HAS_OBP_RTL=1` (off by default) puts behind every epilogue beat of the
  instruction path; `HAS_PSM_PROBE=1` observes how the partial-sum manager uses the PSUM SRAM in a core pass;
  `docs/GVU_RTL_CONFORMANCE.md`, section 4.10.
- **Independent reference for a program**: `tools/has/ref_program.py` computes the output of every GEMM_FUSED and
  ELEM_WISE instruction of a program directory from its registers and image with integer arithmetic in numpy, sharing
  no code with the model, and compares it with an image (a recording, a RAM dump, the reference image);
  `tools/has/make_ref_gate.py` writes a check program for the instruction forms the networks do not use
  (`docs/SW_EVALUATION_GUIDE.md`, section 5.1).
- **Report file of a run through a system wrapper**: `SAURIA_CORE_RTL_REPORT=<file>` makes the model root write one CSV
  line per retired instruction and a summary of the run (`docs/SW_EVALUATION_GUIDE.md`, section 6.1).
- `tools/has/tb_has_npu_top` no longer waits for an instruction that was refused at the push: it reports it and ends
  with FAIL.
- **Vector-unit blocks follow the latest RTL delivery**: per-lane shift amounts of the round-shift units, the 32-bit
  clamp unit, the load step of the table memory, the byte mask and the narrow read ports of the Scratchpad, the address
  reset of the load / store unit. The round-shift of a negative input keeps the arithmetic shift
  (`docs/GVU_RTL_CONFORMANCE.md`, section 3.1). The test vectors of the hardware team run on the ported units
  (`tools/has/tb_rtl_ref_gvu_hwvec.cpp`).
- **Checks at the push for programs of another compiler** (model root `core_rtl/`, warning lines on stderr,
  `docs/SW_EVALUATION_GUIDE.md`, section 4.1): a region of the instruction that ends beyond the DRAM image area, and
  a tile that does not fit one buffer of SRAM-A, SRAM-B or SRAM-C. The instruction runs all the same. The report
  summary counts both.
- **Fix: a tile larger than the staging areas of `HasNpuTop` wrote outside the DRAM vector**, which could end the
  simulation later with a memory error. Such a tile is now cut at the size of the staging area and the instruction
  retires with internal errors. The two staging areas of the partial sums have the size of the modelled SRAM-C.
  Results and cycle counts of a legal tiling do not change.
- **Convolution padding per side and padding value in the instruction**: extension registers `PAD_VALUE`, `PAD_T`,
  `PAD_L`, `PAD_B`, `PAD_R` (`0x400004E8` to `0x400004F8`; through a system wrapper at offsets `0x4E8` to `0x4F8` of the
  extension window). Not written: `PAD` on all four sides and value 0, as before (`docs/SW_INTEGRATION_GUIDE.md`,
  rows 31 to 35). The firmware definitions (`tools/has/wrapper/fw/npu_has_fw.h`) and the wrapper change
  (`tools/has/wrapper/npu_tlm_rich_window.patch`) cover the five registers.
- **Padding generated by the IFMAP feeder** (`HAS_FEEDER_PAD=1`, off by default): SRAM-A receives only the part of a
  tile's input window inside the image, and the feeder of the RTL-ref core replaces every other element by the padding
  value while it gathers. Four configuration registers of the core (`ACT_PAD_CTRL`, `ACT_PAD_COL`, `ACT_PAD_ROW_LO`,
  `ACT_PAD_ROW_HI`) written by the data flow controller, a padding generator per tile (`has/has_pad_gen.h`), 8 cycles
  per padded core pass; 1x1 stride-1 layers keep the staged padding (`docs/HARDWARE_CONFIG.md`, section 6.5). Results
  are bit-exact; whole YOLOv8m: 62,698,248 cycles (+0.05 %), core busy unchanged (`docs/VERIFICATION_REPORT.md`, V31).
  This logic has no hardware description yet; it is a proposal to the hardware team.
- **Fix: `--profile legacy` with `PAD_TAIL`** gave wrong results: the sequential schedule lacked the `PAD_TAIL` guards
  on bias, scale, shift and write-back that the overlapped schedule has. They are now applied there too; the default
  profile is unchanged.
- **Check program for padding**: `tools/has/make_pad_gate.py` (fixed and random layers, golden from explicit loops),
  unit test `tools/has/tb_has_pad_gen.cpp`; both in `tools/has/regress_gvu.sh`.
- **Dilation**: extension register `DILATION` (`0x400004FC`, index 36; through a system wrapper at window offset
  `0x4FC`); kernel taps `DILATION` positions apart, not written or 0 = 1, `(KW - 1) x DILATION` at most 63
  (`docs/SW_INTEGRATION_GUIDE.md`, row 36). The extension range now ends at `0x40000500`: `core_rtl/npu_top.h`, the
  firmware definitions and the wrapper change follow. The RTL-ref core already had dilation (the 64-position
  dilation pattern of the IFMAP feeder); the instruction decoder, the tile walk and the padding generator now use the
  effective kernel `(K - 1) x DILATION + 1`. Check programs `make_pad_gate.py --set dilation` (13 layers) and
  `--set dilrandom` (random layers), and a dilation block in `tb_has_pad_gen.cpp` (`docs/VERIFICATION_REPORT.md`,
  V33). Programs that do not write the register run exactly as before.
- **Error `SWEEP` (11) at the push**: a GEMM_FUSED instruction whose tiles would need a row sweep of the IFMAP feeder
  longer than 256 positions (large stride or dilation with many positions per context) is refused. Such instructions
  gave wrong results without an error, or hung, in the core and in the SAURIA RTL alike (`docs/KNOWN_LIMITATIONS.md`,
  O24; `docs/VERIFICATION_REPORT.md`, V34). `tools/has/ref_program.py` refuses them as well.
- **Vector-unit top level `GvuTop`** (`has/rtl_ref_gvu_top.h`, contributed by the vector-unit owner): controller with
  Softmax, LayerNorm, GEMM_FUSED and residual-add states, test benches `tb_gvu_top`, `tb_gvu_top_gemm_fused`,
  `tb_gvu_top_residual_add`, `tools/trace_gvu_top.cpp` and their datasets under `target/test/`. The GEMM_FUSED path
  takes the rounding mode as an input, saturates to int16 after the shift and can load a new activation table;
  `tools/has/tb_gvu_top_vs_model.cpp` compares it with the model's cycle-level units on random data (in
  `regress_gvu.sh`). A stream mode of GEMM_FUSED (one beat per cycle, nine stages after the register cuts of the
  specification) and the socket `has/gvu_top_bridge.h` connect it to the instruction path behind the option
  `HAS_GVU_TOP=1|2` (call style): the GEMM epilogue, the residual add and the max pool then run on `GvuTop`
  (`docs/GVU_TOP_CORE_INTEGRATION.md`, section 1.1).
  `docs/GVU_TOP_VERIFICATION_REPORT.md` describes the runs of its own test benches.
- **`GvuTop` connected to the core on the core clock** (`HAS_GVU_TOP=3`, the default; `has/gvu_top_stream.h`): a
  stream tap of the partial-sum manager hands every GEMM_FUSED epilogue beat to `GvuTop` with the signals of the PSM-to-GVU
  stream of the hardware RTL (FIFO of 4 packets, `valid` / `ready`, `oc_last` / `oc_tile_last`); the int8 result is written
  back into SRAM-C. Whole YOLOv8m (98 / 98) and whole ViT-B/16 (244 / 244) pass with 0 lanes different from the
  functional model, in 62,420,669 and 41,581,353 cycles (level B without the register file); the core busy cycles equal those of the
  runs with the knobs at 0. With
  `HAS_GVU_STALL=1` the core stops on the back-pressure of the stream (`o_stream_stall`) as `psm_top.sv` does
  (`docs/GVU_TOP_CORE_INTEGRATION.md`, sections 2.1, 2.5 and 5.4; `docs/VERIFICATION_REPORT.md`, V36).
- **Max pool on `GvuTop`** (`POOLING_MAX`): the pooling feeder (`has/rtl_ref_gvu_pool_feeder.h`), the per-lane mode of the
  comparator and the pooling address mode of the load/store unit, ported from the hardware RTL and checked against it
  cycle by cycle with Verilator (`tools/has/gvu_oracle/run_verilator_maxpool.sh`); the unit is reset before each run
  because the RTL keeps a counter between runs (`docs/GVU_TOP_IMPLEMENTATION_AND_TEST.md`, section 6). A whole layer
  checked against the RTL: the three SPPF max pools of YOLOv8m as vector sets in the format of the hardware vectors, run
  back to back without a reset on the RTL with the fix of that counter, equal to the model trace line by line in ModelSim
  and Verilator (`docs/VERIFICATION_REPORT.md`, V39).
- **Parameters for open hardware choices in `GvuTop`**: the depth of the GEMM_FUSED stream pipeline is one build
  parameter (`GVU_GEMM_STREAM_DEPTH`, 9 as the specification, 11 as the hardware RTL measures) and the narrowing after the
  round-shift follows `HAS_REQ_NARROW` (saturate or wrap to 16 bits). Defaults leave every result and cycle unchanged.
- **GEMM_FUSED output zero point as in the hardware register**: `GvuTop` takes the 8-bit zero point of its GEMM_FUSED
  register, sign-extended into the 16-bit adder, as the latest hardware RTL does; a layer whose zero point is outside
  int8 keeps the result of the functional model and is counted as "not expressible". Both reference networks use zero
  point 0, so their results and cycles do not change.
- **Residual add and max pool on the core clock** (`HAS_GVU_ELEM_CLK=1`, with `HAS_GVU_TOP=3`, the default): the two
  ELEM_WISE operations run as jobs that tick `GvuTop` once per clock edge instead of the call style; values and cycles
  equal the call style (check programs and YOLOv8m windows of the three SPPF max pools and two residual adds, difference
  0 cycles) (`docs/GVU_TOP_CORE_INTEGRATION.md`, sections 2.6 and 2.7).
- **Checks against the latest hardware RTL**: every ported vector-unit block was compared again with the latest RTL drop
  (scratchpad, load/store unit, LUT load, max pool and the other oracle lines: 0 mismatches) and the oracle harness was
  adapted to it; two test references of the load/store unit that still described an older drop were brought to it
  (`tools/has/tb_rtl_ref_gvu_g2.cpp`, `tools/has/gvu_oracle/rtl_trace_digest.txt`) and both checks are now part of
  `tools/has/regress_gvu.sh`.
- **Padding documents**: `docs/IFMAP_FEEDER_PADDING.md` describes the padding by the IFMAP feeder for the hardware team
  (SRAM-A layout, registers, feeder datapath, per-tile generator, cost, how to check an RTL implementation);
  `docs/SW_INTEGRATION_GUIDE.md`, section 4.7, lists what a compiler writes for each padding case of a framework and
  what is not supported.
- **Report file**: the last six columns give the padding per side, the padding value and the dilation each
  instruction uses (`docs/SW_EVALUATION_GUIDE.md`, section 6.1); the earlier columns keep their order.
- **Fix: short contexts in the RTL-ref core** (`docs/KNOWN_LIMITATIONS.md`, O23): a GEMM_FUSED tile whose K = kernel
  positions x input channels of the core pass is very small could leave the last context of the tile without weights
  (bias-only outputs), and K = 1 ran two MACs per context. The "done" path of the weight counter now reaches the
  feeder controller in the cycle the RTL gives, and K = 1 keeps one MAC per context (`FX1_A3_WEI_DONE_Q_GATE`,
  `FX1_A3_INCNTLIM_ZERO_RTL`, on by default in `control/rtl_ref_defaults.h`; `docs/HARDWARE_CONFIG.md`, section 4).
  Cycle counts of the reference programs are unchanged (`docs/VERIFICATION_REPORT.md`, V32).
- **Fix in `tools/has/ref_program.py`**: the note about the output of one tile used 49,152 elements as the limit; the
  limit of one SRAM-C buffer is 24,576.
- **New reference results**: both networks rerun with the default configuration (section 3;
  `docs/PERFORMANCE_REPORT.md`, section 5.8). YOLOv8m: 62,666,123 cycles (before: 62,681,721). ViT-B/16: 41,680,289
  cycles (before: 41,065,258; LayerNorm over 768 channels now runs 14 rows per group). Core figures are unchanged.
- **Native model updated to the current v4.6 revision**: OBP requantization rounds half away from zero and adds an
  output zero point (per-channel RAM at `0x001C0000` / `0x001D0000`, or a scalar port), guards shifts of 64 or more,
  and takes the channel index from a PSM side band (`Psm::o_channel_idx`, `Obp::i_channel_idx`); the Reduction Engine
  has per-vector softmax maxima, single-step rounding and double-precision LayerNorm accumulators; register fields
  `F_RE_MODE_A/B`; testbenches `tb_obp` (17 checks), `tb_re` (32), `tb_pipeline_re` (12), `tb_unified_smoke` (32).
  Testbenches of this tree that drive the native `Obp` attach `tools/obp_port_compat.h`. `HasNpuTop` does not include
  any of these files; through the model root `core_rtl/` the native part of a system wrapper follows the update.

## 3. Reference results

All at 0.8 GHz with the HAS AXI-128 DMA, bit-exact against the integer golden. Details and the timing fidelity of each
block: `docs/PERFORMANCE_REPORT.md`, sections 5.4 to 5.6, and `docs/VERIFICATION_REPORT.md`, section 1.1.

Since this release the default runs the vector-unit flow of section 2 (first two rows). The other rows were measured with
the functional vector unit: reproduce them with the five knobs at 0, written `GVU0` below for
`HAS_GVU_TOP=0 HAS_GVU_STALL=0 HAS_GVU_ELEM_CLK=0 HAS_GVU_REGS=0 HAS_GVU_AGU_ND=0`.

| Workload | Command (data package as `$FE_WORK`) | Total cycles | Frame rate |
|---|---|---:|---:|
| YOLOv8m, default (vector unit as in the RTL, through its register file) | `tools/has/tb_has_npu_top $FE_WORK/has/insts_pe3` | 62,422,935 | 12.82 FPS |
| ViT-B/16, default (vector unit as in the RTL, through its register file) | `tools/has/tb_has_npu_top $FE_WORK/has/vit_full` | 41,582,468 | 19.24 FPS |
| YOLOv8m, vector unit on the core clock, configured by assignment (no register file) | `HAS_GVU_REGS=0 HAS_GVU_AGU_ND=0 tools/has/tb_has_npu_top $FE_WORK/has/insts_pe3` | 62,420,669 | 12.82 FPS |
| YOLOv8m, hardware as drawn, functional vector unit | `GVU0 tools/has/tb_has_npu_top $FE_WORK/has/insts_pe3` | 62,666,123 | 12.77 FPS |
| ViT-B/16, hardware as drawn, functional vector unit | `GVU0 tools/has/tb_has_npu_top $FE_WORK/has/vit_full` | 41,680,289 | 19.19 FPS |
| YOLOv8m, earlier vector-unit configuration (run C) | `GVU0 HAS_SPEC=v47ref tools/has/tb_has_npu_top $FE_WORK/has/insts_pe3` | 62,681,721 | 12.76 FPS |
| YOLOv8m, two proposed hardware options (run D), earlier configuration | `GVU0 HAS_SPEC=v47ref tools/has/tb_has_npu_top $FE_WORK/has/insts_pe4 --profile proposals` | 60,386,685 | 13.25 FPS |
| ViT-B/16, earlier configuration | `GVU0 HAS_SPEC=v47ref tools/has/tb_has_npu_top $FE_WORK/has/vit_full` | 41,065,258 (1) | 19.48 FPS |
| ViT-B/16, bias-broadcast proposal, earlier configuration | `GVU0 HAS_SPEC=v47ref tools/has/tb_has_npu_top $FE_WORK/has/vit_full --c-bcast` | 39,922,498 | 20.04 FPS |
| Quick check, two YOLOv8m instructions, functional vector unit | `GVU0 tools/has/tb_has_npu_top $FE_WORK/has/insts_pe3 --first 40 --count 2` | 1,658,986 | |

Program files (data package):

| File | sha256 |
|---|---|
| `has/insts_pe3/mmio.txt` | `2297f5d12405ded5a7e454eba3c3a0029dd519187116dfc93278ea75a7ff0704` |
| `has/insts_pe3/dram_init.bin` | `12114574e544d725c57faeeffe6798a8e99d2ef5ae3f82ebd08ffd1700a48ae1` |
| `has/insts_pe3/dram_golden.bin` | `854e37553f0a05a902a675eaf0a81fad355977c8b8539a5cd4869c1f08554657` |
| `has/insts_pe4/mmio.txt` | `97aeaabaeeebc62bb74ae0a6ab85afc20dafcec6b91aa2f867338985bfef9c58` |
| `has/insts_pe4/dram_init.bin` | `cd958e854fa98853a52e3a92ea5072af7012d9ed8646522f35e9dc9da7be6be8` |
| `has/insts_pe4/dram_golden.bin` | `e17d1551ba40beab20cb8dd3680e490b7b343031e7f8adc68f4935aeb8df092e` |
| `has/vit_full/mmio.txt` | `48ebc2b2686c3d297807a688bef9d519f0f0b625c69fa85704e34a2b4c56017e` |
| `has/vit_full/dram_init.bin` | `22dee0aa5c118a163267cbebf08175069bc6fda2cdd9ee9e5f409bce5b166e66` |
| `has/vit_full/dram_golden.bin` | `a60236f1375d5852ed54a4d564a44a603fe01e6135dac9d0cd208ab19885b8e6` |
| `step3/pc_p9999_has/params.npz` (quantization parameters) | `aab3a947bec00446a192b481ae7cea0658b907a041ecd58d9ce33b88cb518cb7` |

The measured logs of these six whole-network runs, and of the YOLOv8m baseline before the planner and data-flow
options (112,912,950 cycles, 7.09 FPS; `docs/PERFORMANCE_REPORT.md`, section 5.3), are in `reference_runs/` of the
data package.

(1) Measured on coco128 image 000000000502 with an instruction stream identical to `vit_full` (timing does not depend
on data values; the first 23 instructions run on the shipped program give identical cycles, and a third image gives
the same total). The log is `reference_runs/vit_b16_as_drawn/` (`docs/VERIFICATION_REPORT.md`, V22 and V23).

## 4. How this release was checked

On Ubuntu 22.04 with g++ 11 and SystemC 2.3.3, from a fresh copy of the repository tree only (data extracted with
`tools/unpack_data.sh`):

| Check | Result |
|---|---|
| Repository tree: every file against `package_manifest.txt`; data package joined, checked and extracted by `tools/unpack_data.sh`; a git clone made with line-ending conversion requested still matches the manifest, and git tracks every shipped file | PASS |
| Build of the delivery testbench and 12 unit testbenches | PASS |
| Data package self-sufficiency: vector-unit tables exported from the shipped vectors, transformer check regenerated byte-identical | PASS |
| Unit tests: run profiles, register decoder, quantization, element-wise, DMA, attention, LayerNorm, softmax tables | PASS |
| Transformer instruction path (14 instructions) | PASS, 285,205 vector-unit cycles; 250,885 with `HAS_SPEC=v47ref` |
| Vector-unit items of the micro-architecture specification: `tb_gvu_mas` and the frontend golden against the independent reference `mas_ref.py` | PASS |
| Two-instruction YOLOv8m window, default profile, functional vector unit (`GVU0`) | PASS, 1,658,986 cycles (equal to the reference run) |
| Native model regression (fresh build, v4.6 revision with the OBP and Reduction Engine update) | PASS |
| Python byte-compilation of `tools/` | PASS |
| YOLOv8m program regenerated by the packaged frontend from the data package | byte-identical to the shipped program |
| YOLOv8m baseline program regenerated (`fe_emit_insts.py --plan has`) | byte-identical to the program of the baseline reference run |
| Report tables regenerated by `tools/metrics/post_fullcore.sh` from the reference runs (YOLOv8m and ViT-B/16 in the default configuration, YOLOv8m run C and baseline, ViT-B/16 with `HAS_SPEC=v47ref`), `has_rollup.py`, `fe_vit_top5.py` on the shipped ViT-B/16 program | PASS: figures equal to `docs/PERFORMANCE_REPORT.md`, sections 5.7 and 5.8 |
| System TLM wrapper with the model root `core_rtl/` (wrapper built unchanged; with the DRAM-window change: whole YOLOv8m and ViT-B/16 programs) | PASS: 0 mismatches, per-instruction cycles equal to the reference runs |
| Current wrapper revision (the DRAM-window registers are part of it, no patch needed), built unchanged against the model root `core_rtl/`: its own unit test | PASS |
| Firmware engine `tools/has/wrapper/fw/` (`docs/VERIFICATION_REPORT.md`, V24 and V29): transformer check through the wrapper on the host, default configuration and `HAS_SPEC=v47ref`; host steps and output CRCs of whole YOLOv8m and ViT-B/16 on the golden images; negative control; example compiled for RV32 | PASS: 285,205 and 250,885 cycles. Not run on a CPU model inside a full platform |

Checked for this release in addition (`docs/VERIFICATION_REPORT.md`, V30): both networks rerun through `HasNpuTop` in
the default configuration with the binary built from this tree -- YOLOv8m PASS, 62,666,123 cycles, PE utilization
69.26 %; ViT-B/16 PASS, 41,680,289 cycles, 69.61 %, equal to the reference runs cycle for cycle; the wrapper test built
on the current wrapper sources (14 of 14, 285,205 cycles, PE utilization 23.18 % read from the wrapper's registers); the
recorder and the warning lines through the wrapper; the cycle-level pipelines (`tb_rtl_ref_gvu_elemwise`,
`tb_rtl_ref_gvu_requant`, `tb_rtl_ref_gvu_replay`); the whole YOLOv8m with `HAS_ELEM_RTL=1` (PASS, 62,666,994
cycles) and with `HAS_ELEM_RTL=1 HAS_OBP_RTL=1` (PASS, 62,661,785 cycles), no value different from the functional
model; `tools/has/ref_program.py` on both networks, on the check program of `make_ref_gate.py` against the model, and
on planted faults; the report file through the wrapper on the transformer check. Padding (`docs/VERIFICATION_REPORT.md`,
V31): the check program of `make_pad_gate.py` with both ways of padding in four run profiles and through the system
wrapper, random layers, the unit test of the padding generator, one-line faults, sanitizer builds, and the whole YOLOv8m
with both ways of padding (PASS, 62,666,123 and 62,698,248 cycles). Dilation (V33): the RTL-ref core against the
SAURIA RTL on 10 shapes, the check program and random layers with both ways of padding. Short contexts (V32): the
SAURIA RTL on 12 shapes, and both networks rerun with the correction (cycles unchanged). Row sweep (V34): the limit
measured on the core and on the SAURIA RTL, the instructions past it refused at the push, none of the reference
programs refused. Vector-unit top level: its test benches, `tools/has/tb_gvu_top_vs_model.cpp` against the model's
cycle-level units (51 runs, stream mode included), and through the instruction path with `HAS_GVU_TOP`: YOLOv8m
instructions 86 to 88 and a two-instruction window bit-exact with the cycles of `HAS_OBP_RTL=1`, the recorded epilogue
beats replayed on `GvuTop` outside the core, the residual adds of both networks replayed on `GvuTop` (V35). Clocked
connection and max pool on `GvuTop` (V36): whole YOLOv8m and whole ViT-B/16 with `HAS_GVU_TOP=3`, each in one continuous run
(PASS, 62,420,669 and 41,581,353 cycles, 0 lanes different, core busy cycles equal to the default runs); YOLOv8m also in
28 windows of instructions; the core stop on the stream back-pressure with the ready of the unit held low; max pool against
the hardware RTL with Verilator. Zero point of the hardware register: unit test of the 8-bit zero point (inside and
outside int8) and the check program with an output zero point of 200 (results equal, the layer counted as not
expressible). Residual add and max pool on the clock: unit test (12 cases equal to the call style) and five YOLOv8m windows
(difference 0 cycles). Whole YOLOv8m with `HAS_GVU_TOP=3 HAS_GVU_STALL=1 HAS_GVU_ELEM_CLK=1` in one run: PASS, 62,420,669 cycles,
equal to the earlier clocked run cycle for cycle, 0 lanes different, core busy cycles equal to the default run. Register file of
the vector unit (V37): its port against the RTL (200,000 cycles, 0 mismatches), the write sequences at three write latencies,
the check programs of 6 and 9 instructions, and YOLOv8m through the register file in seven windows of instructions (98 of 98
PASS, 0 elements different, 83 of 83 layers ended by the stream flag, 1,819 cycles more than the run without it) plus the 12
residual adds one at a time (values and cycles equal with and without the register file). Max pool of a whole layer against
the hardware RTL (V39): the SPPF layers in 21 runs back to back on the patched RTL, traces equal to the model (32 ModelSim
cases, 26 Verilator traces). Default flow in one continuous run
(V38): YOLOv8m PASS, 62,422,935 cycles; ViT-B/16 PASS, 41,582,468 cycles; 0 elements different. The package check of this tree passed 81 of 81 items, and
`tools/has/regress_gvu.sh` 131 of 131 with the new defaults.

The row "System TLM wrapper ... whole YOLOv8m and ViT-B/16 programs" was measured on the previous release (earlier
vector-unit configuration); it was not repeated. Network testbenches that use the native OBP (`tb_fe_network`,
`tb_fe_core_net`) need a golden with the rounding of the updated native OBP (`docs/VERIFICATION_REPORT.md`, section 4).

The whole-network results of section 3 come from the measured runs whose logs are shipped; a full rerun takes about
10 hours (YOLOv8m) or 5 hours (ViT-B/16) on one core.

## 5. Not included

- Floating-point trained weights, ONNX exports and image datasets (third-party material): needed only to rerun the
  frontend from the start or to regenerate the ViT-B/16 program. See `THIRD_PARTY_NOTICES.md` and
  `docs/FRONTEND_GUIDE.md`. The data package does contain weights derived from these models: the quantized int8
  weights and biases of YOLOv8m and ViT-B/16 inside the program DRAM images (`dram_init.bin`) and the quantization
  parameters (`params.npz`).
- The case-driven demo pack of earlier native revisions (`npu_demo_clean`, `make demo`, `make check`): its testbench
  flow does not run on the current native model. The native regression (`tools/has/regress_native.sh`) and the unit
  testbenches of section 4 cover the native model instead.
- A C driver library for `HasNpuTop`: developed by the integrating team; the testbench
  `tools/has/tb_has_npu_top.cpp` shows the register sequence
  (`docs/SW_INTEGRATION_GUIDE.md`, section 4).

## 6. Known limitations and open points

- Timing of the DMA, the epilogue, the element-wise blocks and the vector-unit stages of the transformer instructions is
  modelled with parameters that stand in for open hardware questions (`docs/KNOWN_LIMITATIONS.md`, section 7;
  `docs/HAS_VALIDATION.md`). Only the core is cycle-accurate (`docs/VERIFICATION_REPORT.md`, section 1.1).
- The accuracy of the integer ViT-B/16 against the floating-point model is limited by the per-tensor LayerNorm
  quantization (`docs/KNOWN_LIMITATIONS.md`, Q4).
- Debug-only accessors and diagnostic macro blocks (all off by default) remain in the RTL-ref sources.
- Softmax, LayerNorm and FUSED_ATTN still run on the functional vector-unit models on the instruction path; `GvuTop` has
  their pipelines and test benches, but `HasNpuTop` does not call them yet.
- The latency of one register write (`HAS_GVU_REG_LAT`, 4 cycles) is an estimate; the residual-add registers (Reg 9..15)
  and AGU_ND (Reg 16..24) are proposals that the hardware RTL does not have yet (`docs/GVU_DFC_REFERENCE_FLOW.md`).

## 7. Integration notes

- The default changed (section 2): a system wrapper or script that relied on the cycle counts of the previous release must
  set the five vector-unit knobs to 0 (`GVU0`, section 3); `tools/has/regress_gvu.sh` and `tools/pkg/check_package_vm.sh`
  do so for their reference values.
- The native top level (`npu_top.h`, class `NpuTop`) keeps the port list and register map used by existing
  integration wrappers: a wrapper built for the previous native model builds and runs unchanged against this tree.
  Point the wrapper's model root (for example a `SAURIA_NPU_ROOT` build variable) at the extracted source tree.
- The reference results of section 3 come from the delivery top level `HasNpuTop` (`tools/has/tb_has_npu_top`).
  Through a wrapper with the model root at the repository root, cycle counts and performance counters come from the
  native core, whose timing is not RTL-accurate, and are not comparable with section 3. With the model root
  `core_rtl/`, extended instructions run on `HasNpuTop` and their counters equal `tb_has_npu_top`
  (`docs/SW_INTEGRATION_GUIDE.md`, section 10).
- For a platform that already integrates the previous model through `core_rtl/`: replace the model tree and rebuild;
  the platform's firmware register header and the wrapper's document need the six new extension registers and error
  code 11, and one range constant of the wrapper is best extended (`docs/INTEGRATOR_QUICK_START.md`, section 2.1).
- In the native top, instructions pushed through the instruction registers are executed by the functional decoder
  (`emulate_*()`): a whole network run through the native path gives the functional result, not RTL-accurate timing
  (`docs/KNOWN_LIMITATIONS.md`, N1).
- The Reduction Engine output multiplexer on the SRAM C write-back of the native top is covered by the build and the
  native regression; no test drives data through the Reduction Engine yet.
- Distribution: this package is provided for internal integration and evaluation. Do not redistribute it, or binaries
  built from it, without the approval of the rights owner of the SystemC implementation. The third-party terms that
apply to the ported SAURIA design and to the network weights inside the data package are in `THIRD_PARTY_NOTICES.md`,
section 4.
