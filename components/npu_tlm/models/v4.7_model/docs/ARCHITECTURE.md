# Architecture Guide

This document describes the two NPU models in the package, the blocks they are built from, and how a
network is executed. For file formats see `INTERFACE_SPEC.md`. For sizes and build options see
`HARDWARE_CONFIG.md`.

## 1. Overview

```
                 +---------------------------------------------------------------+
  ONNX model --> | Python frontend (tools/fe)                                    |
                 |  graph -> IR -> int8 quantization -> integer golden -> tiles  |
                 +--------------+--------------------------------+---------------+
                                | mmio.txt, dram_init.bin,       | prog.bin, dram_init.bin,
                                | dram_golden.bin                | dram_golden.bin
                                | (fe_emit_insts, fe_vit_full)   | (earlier flow)
                                v                                v
   +------------------------------------------------+  +-------------------------------------------------+
   | Delivery top level HasNpuTop (sections 7, 8)   |  | Network testbench (tb_fe_core_net / tb_has_net) |
   |  v4.5 MMIO protocol + extension registers      |  |  v4.5 SRAM + DMA + OBP (or HAS OBP/ELEM_WISE)   |
   |  data flow controller, HAS DMA, vector unit    |  |  one tile at a time, driven by the testbench    |
   |  RTL-ref core rtl_ref_npu_top.h, 32x32, 1 lane |  |  RTL-ref core rtl_ref_npu_top.h (sections 4, 5) |
   +------------------------------------------------+  +-------------------------------------------------+
     driven by tools/has/tb_has_npu_top, or by a system TLM wrapper through core_rtl/

   +------------------------------------------------+
   | Native model npu_top.h (section 2)             |  legacy functional model: instruction-level, 2 lanes
   +------------------------------------------------+
```

| | Native model (`npu_top.h`) | RTL-ref / HAS path |
|---|---|---|
| Role | Legacy functional model of the previous hand-overs (revision v4.6); regression baseline and target of existing integration wrappers | Validated path and basis of the delivery top level `HasNpuTop` (section 7) |
| Abstraction | Instruction-level emulation; the SystemC module hierarchy is present but not driven by instructions | Clock-accurate core that follows the SAURIA RTL |
| Lanes | 2 (Lane A, Lane B), split of one PE grid | 1 |
| Array | `X_DIM x Y_DIM`, template parameters (32x32 default, 64x64 in some unit tests) | 32x32 |
| Control | Host MMIO, 64-bit rich instructions, dual instruction queues | Configuration registers per tile, then start; in `HasNpuTop` the data flow controller does this for every tile of a one-instruction layer |
| Epilogue | OBP (bias, requant, LUT, residual), RE/RCE (softmax, LayerNorm) | v4.5 OBP, or the HAS and vector-unit blocks (OBP, ELEM_WISE ADD / MAX_POOL / AVG_POOL, softmax, LayerNorm, FUSED_ATTN) |
| Typical use | Regression of the legacy flow; reference for the legacy programming protocol | Correctness and cycle measurement of real networks; delivery model |

## 2. Native model (`npu_top.h`): legacy functional model

This package ships the native model at revision v4.6 (changes in `RELEASE_NOTES.md`, section 2). It serves as the
regression baseline, is the model that existing integration wrappers build against, and documents the
memory-mapped protocol of the previous hand-overs (the v4.5 protocol). It is not the computation path of the
delivered model.

### 2.1 Block diagram

```
HOST (MMIO CSRs, INST_LO/INST_HI, queue push registers)
  -> ConfigRegs + InstructionDecoder (dual instruction queues A/B)
       -> Lane A: Control -> IfmapFeeder/WeightFeeder -> SystolicArray (rows 0..NSPLIT-1) -> Psm -> Obp -> RE/RCE
       -> Lane B: Control -> IfmapFeeder/WeightFeeder -> SystolicArray (rows NSPLIT..)    -> Psm -> Obp -> RE/RCE
  -> SauriaDma (4 read channels, 1 write port) <-> DRAM
  -> Sram (weight, ifmap, psum/scratch banks)
```

The PE grid is shared by the two lanes. The register `NSPLIT` (default `Y_DIM/2`, also set by the
`SET_NSPLIT` instruction) decides how many rows belong to Lane A. `NSPLIT=0` gives the whole array to Lane A.

### 2.2 Instruction set

Instructions are 64-bit words written through `INST_LO` / `INST_HI` (writing `INST_HI` triggers decode) or
pushed to a lane queue.

```
| w_addr [63:32] | in_addr [31:16] | flags [15:8] | opcode [7:0] |
```

| Opcode | Mnemonic | Function |
|---|---|---|
| `0x05` | `SET_NSPLIT` | Reconfigure the lane split, synchronizing both lanes |
| `0x12` | `GEMM_FUSED` | `Y = Act(Requant(X*W + Bias)) + Residual`, int8 in, int32 accumulate |
| `0x13` | `FUSED_ATTN` | `Y = Softmax(Q*K^T / sqrt(d_k)) * V` |
| `0x14` | `LAYERNORM` | `Y = gamma * (X - mean) / sqrt(var + eps) + beta` |
| `0x15` | `ELEM_WISE` | Add, MaxPool, Mul, Sub, Div with 1-D broadcast |

The instruction set is frozen. The exact register offsets are defined in `config_map.h` and implemented in
`config_regs.h`. These two files are the single source of truth.

### 2.3 Blocks

| Block | File | Summary |
|---|---|---|
| Configuration registers | `config_regs.h` | MMIO register file, profile selection (`npu_profile.h`) |
| Instruction decoder | `control/instruction_decoder.h` | Queues, per-lane state machines, DMA orchestration |
| Controller | `control/main_controller.h` | Context sequencing for one lane |
| Feeders | `data_feeder/ifmap_feeder.h`, `wei_feeder.h` | Address generation (im2col, dilation, tiling), FIFOs |
| Systolic array | `systolic_array/sa_array.h`, `sa_processing_element.h` | Output-stationary PE grid |
| Partial-sum manager | `psm/psm_top.h` | Scan-chain drain of the array, SRAM-C accumulation |
| OBP | `psm/obp_top.h` | 4-stage epilogue: bias, requant (`(x*scale) >> shift`), 256-entry LUT, residual add |
| RE / RCE | `psm/re_rce.h` | Reduction trees and exp / recip / rsqrt LUTs for softmax and LayerNorm |
| DMA | `control/sauria_dma.h` | 4 read channels sharing one port round-robin, 1 write port, 256-byte bursts of 8 cycles |
| SRAM | `sram/sram_top.h` | Weight, ifmap and psum banks with scratch space |

Data types are selected at build time: int8 (default), fp16 or int16 (see `HARDWARE_CONFIG.md`).

### 2.4 Execution semantics

The instruction decoder evaluates each instruction in double-precision C++ (`emulate_gemm_fused`,
`emulate_fused_attn`, `emulate_layernorm`, `emulate_elem_wise`). It re-implements bias, requantization,
activation, softmax and normalization itself, and moves memory through the DMA and SRAM models. The array,
feeders, PSM, OBP and RE/RCE modules are instantiated and wired but are not driven by the instruction stream.
Each of them is verified only by its own unit testbench (`tb_obp`, `tb_re`, the `tools/test_lane_a_*` tests).
The Reduction Engine output is multiplexed onto the SRAM C write-back path, selected while it drives a valid vector.

The full-network tests of this model (`test_yolo`, `test_onnx_model`, `test_vit`) use programs and goldens
generated by `tools/onnx_compiler.py`. That compiler lowers convolutions with simplified arithmetic (see
`KNOWN_LIMITATIONS.md`, N2), and the golden uses the same arithmetic. A pass therefore shows that the compiler
and the emulator agree with each other. It does not validate real network semantics or clock-level behaviour.

## 3. RTL-ref core (`rtl_ref_npu_top.h`)

### 3.1 Block diagram

```
Testbench host bus --> ConfigRegs --> Control (context FSM, feeders FSM, context-switch controller)
                                          | start, valid, count enables, pipeline enable
Backdoor / host bus --> Sram (A, B, C x 2 buffers, i_select ping-pong)
                          A --> IfmapFeeder (FIFO 5 positions) --+
                          B --> WeightFeeder (FIFO 4 positions) -+--> SystolicArray 32x32 --> Psm --> Sram C
                                                                                        (reads preload, writes psum)
```

The top level, the configuration registers and every sub-block are ported one-to-one from the reference
SystemC model of the SAURIA RTL. The equivalence is checked cycle by cycle on sampled tiles (see
`VERIFICATION_REPORT.md`).

| Block | File |
|---|---|
| Top level | `rtl_ref_npu_top.h` |
| Configuration registers | `rtl_ref_config_regs.h`, `rtl_ref_config_map.h` |
| Controller | `control/rtl_ref_main_controller.h`, `rtl_ref_context_fsm.h`, `rtl_ref_feeders_fsm.h`, `rtl_ref_context_switch_controller.h` |
| Feeders | `data_feeder/rtl_ref_ifmap_feeder*.h`, `rtl_ref_wei_feeder*.h`, `*_idxcnt.h`, `rtl_ref_feed_*.h`, `rtl_ref_fifo_memory_ff.h` |
| Array | `systolic_array/rtl_ref_sa_array.h`, `rtl_ref_sa_processing_element.h` |
| Partial-sum manager | `psm/rtl_ref_psm_*.h` |
| SRAM | `sram/rtl_ref_sram_top.h` |
| Build-time options | `control/rtl_ref_defaults.h` (`FX1_A3_*` macros) |
| Performance counters | `instrumentation/rtl_ref_*.h` |

### 3.2 Mapping of a convolution tile

- Output channels map to array columns (`X_used = Cout_t`).
- Output positions of one row map to array rows (`Y_used` <= 32, a divisor of the tile width: `gcd(W_t, 32)` by
  default; `HasNpuTop` also accepts an explicit `Y_USED`, for example 31, section 8.2).
- One context = `Y_used` output positions x `X_used` channels. A tile of `H_t x W_t` outputs has `H_t * W_t / Y_used` contexts.
- Each context accumulates `K = Cin * kh * kw` products per PE. The bias is preloaded into SRAM-C as the initial partial sum.
- In the network testbench (section 4) input channels are never split across tiles. `HasNpuTop` can split them
  into several core passes that accumulate in SRAM-C (`TILE_CIN`, section 8.1).

### 3.3 Execution of one tile

1. Reset the core; select the host-side buffers (`i_select = 000`).
2. Load SRAM A (input window with padding, `[Cin, A_H, A_W]`), SRAM B (weights in SAURIA order) and SRAM C (bias preload).
3. Swap the buffers to the accelerator side (`i_select = 111`).
4. Write about 45 configuration registers (loop limits and steps, dilation pattern, active rows and columns, context count).
5. Pulse start. The controller walks through its states: preparation, array busy, context switch, partial-sum drain, finish.
6. Wait for `o_done` and read SRAM C.

The controller state histogram of each tile is available in the metrics build. Over YOLOv8m, 88 % of busy
cycles are array-busy states, 7.5 % context switches and 2.4 % partial-sum drain waits (see
`PERFORMANCE_REPORT.md`).

## 4. Network testbench

`tools/fe/sysc/tb_fe_core_net.cpp` runs a complete network program. It instantiates:

| Instance | Model | Role |
|---|---|---|
| `Sram` | v4.5 `sram/sram_top.h` | Staging SRAM written by the DMA |
| `SauriaDma` | v4.5 `control/sauria_dma.h` | Moves inputs, weights, bias, skip and outputs between DRAM and SRAM |
| `Obp` | v4.5 `psm/obp_top.h` | Requant, SiLU LUT, residual |
| `NpuTop` | `rtl_ref_npu_top.h` | Convolution on the cycle-accurate core |

### 4.1 Per-step flow

```
for each step in prog.bin:
    conv       -> for each tile: DMA read -> SRAM -> core -> OBP -> DMA write -> output tensor
    slice_ch, concat, maxpool, upsample -> executed by the host on the DRAM image
    compare the step output with the golden, print [STEP] PASS/FAIL, optionally snapshot DRAM
after the last step: compare every written tensor, print RESULT
```

### 4.2 Per-tile flow

| Phase | Block | Details |
|---|---|---|
| Stage | Host | Build the padded input window, bias preload and skip values in DRAM staging areas |
| DMA read | `SauriaDma` | ch0 weights to bank 0, ch3 preload to bank 4, ch2 skip to bank 5 (scratch), ch1 input to bank 2 or 3. ch1 of the next tile is prefetched while the current tile computes |
| OBP setup | Host bus | Per-channel scale and shift; LUT once per layer |
| Compute | `NpuTop` | Section 3.3 |
| Epilogue | `Obp` | One vector per cycle per context. Channel index from an internal counter, so 6 idle cycles separate contexts |
| DMA write | `SauriaDma` | Bank 4 to the DRAM output staging area, then the host copies into the output tensor |

## 5. HAS epilogue and element-wise blocks (`has/`)

The `has/` blocks implement the OBP and ELEM_WISE pipelines as drawn in the hardware architecture
specification. `tools/fe/sysc/tb_has_net.cpp` is the network testbench that uses them.

| Block | File | Function |
|---|---|---|
| Quantization primitives | `has/gvu_quant.h` | Requant `sat8(narrow(rshift(x * S, s)) + zp)` and dequant, with selectable rounding and narrowing |
| `HasObp` | `has/gvu_obp.h` | GEMM_FUSED epilogue: requant then LUT. Channel index from the vector address and a host-written `NCH` register, fixed latency |
| `HasElemwise` | `has/gvu_elemwise.h` | ELEM_WISE ADD (dequant both inputs, add, requant, through the scratchpad), MAX (5x5, direct or separable), AVG (int16 window sum, Avg_Scale, Avg_Shift; mode 5) |
| `HasScratchpad` | `has/gvu_scratchpad.h` | Scratchpad with capacity check and traffic counters |
| `HasLut` | `has/gvu_lut.h` | Indirect LUT for reciprocal and reciprocal square root (mantissa index with linear interpolation) |
| Pipeline registers | `has/gvu_vrf.h` | Structure model of the vector-unit pipeline registers (3 x v32int64 + 4 x v32int32 with reconfiguration): checks that the registers of every pipeline stage fit (`tools/has/tb_gvu_vrf`) |
| `GvuLsu` | `has/gvu_lsu.h` | Load/store unit accounting: Scratchpad transfers per bank (`sbank`, `vbank#0..2`) and address mode (v32int32, v32int8, int32, int8), reported as `[GVU]` lines by `tools/has/tb_has_npu_top` |

Model options, identical in Python and C++:

| Option | Values | HAS default |
|---|---|---|
| `ROUND_MODE` | FLOOR, HALF_UP, HALF_AWAY, HALF_EVEN | HALF_UP |
| `REQ_NARROW` | SAT16, WRAP16, NONE | SAT16 |
| `DEQ_ZP_ORDER` | SUB_BEFORE, ADD_AFTER | SUB_BEFORE |
| `SCALE_FMT` | I32, U32 | I32 |
| `MP_MODE` | DIRECT, SEPARABLE | DIRECT |
| `SCRATCH_BYTES` | integer | 24,576 |

Differences of `tb_has_net` from `tb_fe_core_net`:
- The residual add becomes an ELEM_WISE ADD step.
- The SPPF max-pools become ELEM_WISE MAX steps.
- The program has 129 steps instead of 117.
- The option values are read from the program header.

## 6. Modelling fidelity

This section describes the network testbench of section 4 (and the `legacy` profile of `HasNpuTop`). The
default profile of `HasNpuTop` overlaps DMA, core and epilogue and uses the HAS AXI-128 DMA timing (sections 7
and 8, `HARDWARE_CONFIG.md`, section 6.4).

| Aspect | Status |
|---|---|
| Core datapath and control (controller, feeders, array, PSM, core SRAM) | Clock-accurate, equivalent to the reference core |
| OBP and HAS blocks | Bit-exact arithmetic; fixed pipeline latency |
| DMA | Burst scheduling modelled (32 bytes per cycle, round-robin); no DRAM latency; data copied at burst end. The HAS specifies a 128-bit AXI bus (16 bytes per beat), so DMA time is optimistic by about 2x |
| Staging SRAM to core SRAM | Copied by the testbench without cycles (two separate SRAM instances) |
| Core to OBP | The testbench forwards the tile result after the core finishes; OBP does not overlap the core |
| Host operations (slice, concat, maxpool, upsample) | Executed by the host, zero simulated cycles |
| Overlap between tiles | Only the input prefetch (ch1) overlaps computation |

The total simulated cycle count of a network run is therefore a testbench time, not an NPU latency. Core
cycles are measured per tile and reported separately (`PERFORMANCE_REPORT.md`).

## 7. Delivery top level: `HasNpuTop`

`HasNpuTop` (`has/has_npu_top.h`) replaces the testbench orchestration of section 4 with hardware-style
control. Existing software keeps programming it with the v4.5 memory-mapped protocol; the testbench acts only
as the CPU.

```
Host MMIO (v4.5 protocol + extension + status registers)
  -> MmioCompat (has/has_mmio_compat.h): gather registers, one layer instruction per push, instruction queue
  -> Data flow controller (SC_THREAD inside has/has_npu_top.h):
       tile iterator (has/has_tile_iter.h), DMA scheduling (has/has_dma.h), buffer swaps
       -> RTL-ref core (controller, feeders, 32x32 array, PSM, SRAM)
       -> HasObp (requant, LUT)          -> HasElemwise (ADD, MAX_POOL, AVG_POOL)
       -> FUSED_ATTN (Q.K^T and A.V on the core, softmax) and LAYERNORM in the vector-unit blocks
  -> STATUS / RETIRED registers, o_irq
```

| Element | Status |
|---|---|
| Ports | `i_clk`, `i_rstn`, the host bus of `NpuTop` (`i_host_addr`, `i_host_wren`, `i_host_rden`, `i_host_wdata`, `i_host_wmask`, `o_host_rdata`), `o_irq`, `o_done`, `o_deadlock`. The full port list of the native `NpuTop` (with `i_start`, `i_mvm_k`, `i_select` and the other legacy inputs) is provided by the drop-in top level `core_rtl/npu_top.h` (`SW_INTEGRATION_GUIDE.md`, section 10) |
| Compatibility layer (`has/has_mmio_compat.h`) | Implemented, unit-tested, connected |
| Tile iterator (`has/has_tile_iter.h`) | Implemented; reproduces all 9,811 YOLOv8m tiles of the offline plan |
| Reference compiler backend (`tools/fe/fe_emit_insts.py`) | Whole YOLOv8m as 98 memory-mapped instructions, verified by replay |
| Data flow controller, sequential schedule | Implemented and verified (`VERIFICATION_REPORT.md`, V8) |
| Data flow controller, overlapped schedule (true ping-pong in the core SRAM) | Implemented and verified (V9) |
| Parameterized DMA (`has/has_dma.h`: bus width, burst, DRAM latency, CH3 priority) | Implemented; v4.5 settings match the v4.5 DMA cycle for cycle |
| FUSED_ATTN and LAYERNORM | Implemented: one attention head or a block of LayerNorm rows per instruction, parameters from a DRAM block, attention products on the core; whole ViT-B/16 verified (`VERIFICATION_REPORT.md`, V17, V18). Softmax, LayerNorm and operand DMA timing estimated |

Acceptance milestones:
1. A window of layers through the memory-mapped path, bit-exact: passed.
2. The same windows with DMA/compute overlap and the HAS DMA, bit-exact, with cycle comparison: passed.
3. The whole YOLOv8m (98 instructions), bit-exact, with detection boxes identical to the golden. This is the
   delivery milestone; its result is reported in `VERIFICATION_REPORT.md`, V12.

The register map and the software changes are in `SW_INTEGRATION_GUIDE.md`.

## 8. Data flow following the GVU specification

The hardware team's Generic Vector Unit (GVU) specification describes the epilogue and vector units in more
detail than the HAS drawings. `HasNpuTop` implements the following options to follow it. The `recommended`
run profile, the default, turns on all of them except the two hardware proposals; each option is also a
switch of the testbench `tools/has/tb_has_npu_top` (profiles: `HARDWARE_CONFIG.md`, section 6.4).

| Option | Switch | What changes |
|---|---|---|
| Epilogue on the partial-sum path | `--obp-inline` | The OBP is applied at every real write of the partial-sum manager into SRAM-C, as in the GVU pipeline (array, PSM, requant, activation LUT, PSUM SRAM). The separate epilogue pass after the core is removed; only the OBP pipeline latency remains after the last write |
| Banked scratchpad | `--sp-banked` | The scratchpad is split into `sbank`, `vbank#0`, `vbank#1/#2` (4, 4 and 16 KB per half, twice for ping-pong: 48 KB). ELEM_WISE operands are placed per bank as in the GVU specification and each chunk is scheduled in ping-pong with the DMA |
| GVU pipeline latencies | `--gvu-lat` | OBP 10, ELEM_WISE ADD stage 1 8, stage 2 10, MAX 3 cycles, derived from the per-unit pipeline depths of the GVU specification (previous estimates 6/6/5/3) |
| 3-D DMA descriptors | `--desc3d` | CH1 reads only the part of the input window inside the image (padding is zero-filled, not transferred); CH2 writes int8 results packed. Bytes on the bus are counted separately from data bytes |
| Layer tile order | `--s4` | For each layer the controller chooses between output-channel-outer and spatial-outer tile order by comparing the DMA bytes of the two orders; the input window is kept (not reloaded, no buffer swap) while the spatial region does not change |

### 8.1 Input-channel split (TILE_CIN)

When a layer has `TILE_CIN` smaller than its input channels, the controller runs each tile in several core
passes, one per input-channel part:

```
for each tile (output-channel band, spatial region):
    for part j = 0 .. IN_C / TILE_CIN - 1:
        DMA: input window channels [j*TILE_CIN, (j+1)*TILE_CIN) and weight part j into the host half
        swap SRAM-A and SRAM-B only; SRAM-C stays on the NPU side
        core pass: the partial sums of part j-1 already in SRAM-C are the preload of part j
        (part 0 preloads the bias)
    the epilogue (on the partial-sum path) is active only during the last part
    DMA: write the int8 results
```

No partial sums travel through DRAM between parts. The input-channel split requires the epilogue on the
partial-sum path (`--obp-inline`). With `TILE_CIN` not written, a tile runs in a single pass as in section 7.

### 8.2 Tile-planner options used with this data flow

| Option | Effect | Applied to |
|---|---|---|
| Input-channel split | Uses tiles of 32 output channels where the SRAM limits previously forced 16 | Layers limited by the weight buffer |
| 1x1 convolutions as a single row | A 1x1 convolution is mapped with the output height and width flattened into one row, up to 768 positions | 1x1 convolution layers |
| Stem with 32 output channels | The first layer uses tiles of 32 output channels | First layer |
| Padded edge tiles (`FLAGS.PAD_TAIL`) | Edge tiles are computed at the full tile size (whole array), only real results are written back | Layers whose sizes are not multiples of the tile |
| Larger tiles | Tiles use more of the SRAM capacity | Layers with room in SRAM-A/B/C |
| 31 positions per context (`Y_USED`) | Avoids the slower array mode of 3x3 stride-2 convolutions at 32 positions | 3x3 stride-2 layers |

Measured effect on the whole network: `PERFORMANCE_REPORT.md`, section 5.4.
