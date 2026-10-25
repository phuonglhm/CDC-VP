# Generic Vector Unit (GVU) Top-Level Implementation and Verification Guide

This document describes the cycle-accurate C++ / SystemC model of the Generic Vector Unit top level (`has/rtl_ref_gvu_top.h`, class `rtl_ref_gvu::GvuTop`): its interface against the hardware RTL, its control states, the five operations it runs (Softmax, LayerNorm, GEMM_FUSED requantization with activation table, residual add, max pool), and the test benches and RTL oracles that check it. Stage definitions follow the Micro-Architecture Specification (MAS section 5.2.1 to 5.2.6). How the unit is connected to the SAURIA core is in `GVU_TOP_CORE_INTEGRATION.md`; the comparison of every other vector-unit block with the RTL is in `GVU_RTL_CONFORMANCE.md`.

Labels used below: **read** = from the code or the RTL, with a file reference; **measured** = a number from a run, with the command that reproduces it; **estimate** = not measured; **proposal** = not decided.

Paths ending in `.sv` and paths under `neo_core/` name files of the RTL of the hardware team. They are not in this package; they are quoted so that a reader who has the RTL can check each statement.

> **Status summary**
> - **Softmax (`GVU_OP_SOFTMAX`)**: verified in `tile` (4,096 elements, 432 cycles) and `vec` (64 elements, 223 cycles) modes, 0 mismatches (`tb_gvu_top`, measured).
> - **GEMM_FUSED, tile mode (`GVU_OP_GEMM_FUSED`)**: `tile` (4,096 elements, 144 cycles) and `vec` (64 elements, 77 cycles), 0 mismatches (`tb_gvu_top_gemm_fused`, measured).
> - **GEMM_FUSED, stream mode (`i_gemm_stream`)**: one beat per PSM write to the PSUM SRAM, nine stages, own address, lane mask, scale and shift per beat. Equal to the model's cycle-level epilogue on random data over the whole value ranges (`tb_gvu_top_vs_model`, measured) and on 3,392 beats of YOLOv8m (`tb_gvu_top_replay`, measured). Section 4.5.
> - **Residual add (`GVU_OP_RESIDUAL`)**: `tile` (4,096 elements, 274 cycles) and `vec` (64 elements, 140 cycles), 0 mismatches (`tb_gvu_top_residual_add`, measured); a last-row lane mask for tensors whose length is not a multiple of 32. The RTL of the hardware drop has no residual state; the unit follows the MAS (section 5).
> - **Max pool (`GVU_OP_MAXPOOL`, state `POOLING_MAX`)**: port of the RTL, checked cycle by cycle against Verilator on 10 sets and against the hardware golden outputs; the three SPPF max pools of YOLOv8m run on it with 0 differences (section 6).
> - **Connection to the SAURIA core**: the stream mode, the rounding-mode, table-reload and 16-bit zero-point inputs and the last-row lane mask were added as inputs whose reset values keep the dataset runs above unchanged (the instruction path drives them; in the default configuration it runs `GvuTop` on the core clock). `GVU_TOP_CORE_INTEGRATION.md` describes the two connections (call style, and on the clock of the core).

---

## 1. Architectural Overview

### 1.1 Structural Hierarchy & Dual-Use Design
`rtl_ref_gvu_top.h` connects the GVU memory submodules, the functional-unit lane functions and an internal pipeline controller:

```
                                  +-------------------------------------------------------------+
                                  |                     rtl_ref_gvu::GvuTop                     |
                                  |                                                             |
   +--------------------+         |  +--------------------+             +--------------------+  |
   |   Instruction /    |         |  |   Pipeline FSM     |             |    Vector LSU      |  |
   |   Control Beat     |-------->|  |   (Internal FSM)   |<----------->|    (GvuVlsu)       |  |
   +--------------------+         |  +--------------------+             +--------------------+  |
                                  |            |                                   |            |
   +--------------------+         |            v                                   v            |
   | Systolic Array /   |         |  +--------------------+             +--------------------+  |
   | DMA Scratchpad     |-------->|  |     Stream VRF     |             |  Scratchpad SRAM   |  |
   +--------------------+         |  |    (StreamVrf)     |             |  (GvuScratchpad)   |  |
                                  |  +--------------------+             +--------------------+  |
                                  |            |                                   |            |
                                  |            +-----------------+-----------------+            |
                                  |                              |                              |
                                  |                              v                              |
                                  |  +-------------------------------------------------------+  |
                                  |  |                   Functional Units                    |  |
                                  |  | - Comparator (Vcmp32)         - Adder#1/#2 (adder32)  |  |
                                  |  | - Multiplier Mul#0 (vmul48x40) - Mul#1 (vmul32x32)   |  |
                                  |  | - RoundShift#0 (vroundshift72) - RS#1 (vroundshift64) |  |
                                  |  | - Vector Clamp (clamp_lane)   - LUT Unit (LutUnit)    |  |
                                  |  +-------------------------------------------------------+  |
                                  +-------------------------------------------------------------+
```

1. **Pure C++17 core (`rtl_ref_gvu::GvuTop`)**: `eval(in)` evaluates the combinational outputs of the current cycle without changing state; `tick(in, rstn)` advances one clock edge and returns the outputs of that cycle. Inspection accessors: `state()`, `start_d_cnt()`, `end_d_cnt()`, `scratchpad()`, `stream_vrf()`, `vlut()`, `vlsu()`.
2. **SystemC wrapper (`rtl_ref_gvu::ScGvuTop`)**: compiled when `<systemc.h>` is available. `clock_proc` (on `i_clk.pos()`, async reset on `i_rstn`) advances the core and notifies an internal event; `comb_proc` is the **only** driver of the output ports and is woken by the inputs and by that event. `o_layernorm_lut_init_done` and `o_layernorm_pipe_done` are optional ports (`SC_ZERO_OR_MORE_BOUND`), so a softmax-only bench may leave them unbound.

> [!NOTE]
> The two testbenches are currently built in **standalone C++17 mode**: `<systemc.h>` is not on the include path of the `make` rules, so the SystemC branch is not compiled.

### 1.2 Scratchpad SRAM Organization (MAS Table 5-4, 5-5)
Each bank has two buffers (ping-pong, `i_*_buf_sel`); one serves the accelerator ports. Sizes are per buffer.

| Bank | Word | Depth | Bytes | Accelerator ports |
|---|---|---|---|---|
| `sbank` | 32 bit, dual port | 768 | 3 072 | port 0: int32 word (LayerNorm β); port 1: int8 byte (LayerNorm γ) |
| `vbank#0` | 256 bit, single port | 800 | 25 600 | v32int8 (Softmax input / attention score, LayerNorm input / var / z) |
| `vbank#1` | 1024 bit, single port | 32 | 4 096 | port 0: v32int32 (LUT init source, Softmax σ, LayerNorm S); port 1: v32int16 (LayerNorm w); port 2: v32int8 (Softmax row max, LayerNorm result) |
| `vbank#2` | 1024 bit, single port | 256 | 32 768 | port 0: v32int32 (LayerNorm D_k); port 1: v32int8 (Softmax e_i) |

Read data follows the address by **2 cycles** (SRAM output register + bank pipeline register).

Depths and sizes follow `gvu_pkg.sv` of the hardware RTL (read: `has/rtl_ref_gvu_scratchpad.h:39`): `sbank` 768 words, `vbank#0` 800 rows, `vbank#1` 32 rows of 128 bytes, `vbank#2` 256 rows. The RTL numbers the banks in the DMA address space as `vbank#2` at 0, `vbank#0` at 32,768, `vbank#1` at 58,368 and `sbank` at 62,464 (65,536 bytes per buffer). The DMA port of `GvuTop` itself does nothing in the model: `o_scratch_dma_rdata` is zero, so a caller writes operands straight into the banks through `scratchpad()` and charges the DMA cycles itself.

> [!NOTE]
> The delivered RTL of the first drop wrote mask bit *i* to bits [i, i+8) instead of byte *i*. The RTL of the later drops is fixed (`sram_single_port.sv`, `sram_dual_port.sv`) and the model follows it: `ScratchpadParams::wmask_as_written` is `false` by default (read: `has/rtl_ref_gvu_scratchpad.h:41`). Setting it to `true` reproduces the old behaviour (section 9.2, item 1).

### 1.3 Integration with Submodules (`rtl_ref_*`) vs. Internal Logic

| Submodule / Component | Header File | Member | Role |
|---|---|---|---|
| `GvuScratchpad` | `rtl_ref_gvu_scratchpad.h` | `scratchpad_` | **Instantiated**: SRAM banks. `scratchpad_.out(w.sp_in)` / `scratchpad_.tick(w.sp_in)`. |
| `GvuVlsu` | `rtl_ref_gvu_vlsu.h` | `vlsu_` | **Instantiated**: address counters, enables, write masks, port selects. `vlsu_.eval(w.vlsu_in)` / `vlsu_.tick(w.vlsu_in)`. One start address per bank (`i_vbank1_start_addr`, `i_vbank2_start_addr`), address mode `AGU_INCR`. |
| `StreamVrf` | `rtl_ref_gvu_vrf.h` | `stream_vrf_` | **Instantiated**: stream registers `vint64`, `vint32`, `vint16`, `vint8`. `stream_vrf_.eval(w.vrf_in)` / `stream_vrf_.tick(w.vrf_in, i_rstn)`. |
| `LutUnit` | `rtl_ref_gvu_lut.h` | `vlut_` | **Instantiated**: table memories (`LutMem`) and lookup pipeline. `vlut_.eval(w.lut_in)` / `vlut_.tick(w.lut_in, i_rstn)`. |
| `Vcmp32` | `rtl_ref_gvu_fu.h` | `vcmp_` | **Instantiated**: 32-lane comparator for Softmax Stage 1 (`CMP_ACCUMULATE`, `CMP_MAX`). |
| Arithmetic lane functions | `rtl_ref_gvu_fu.h` | direct calls | See table below. |

Which arithmetic is done by the ported FU lane functions and which by plain C++ in `GvuTop`:

| Stage | Ported FU lane functions | Plain C++ arithmetic in `GvuTop` |
|---|---|---|
| Softmax ST1 | `Vcmp32` | — |
| Softmax ST2 | `adder32_lane` (ADD_8 sub, ADD_32 accumulate) | — |
| Softmax ST3 | `vmul48x40_lane` (MB_3X16X8), `vroundshift72_lane`, `vmul32x32_lane` (MA_32X32), `vroundshift64_lane`, `adder32_lane` (ADD_16), `clamp_lane` | — |
| LayerNorm ST1 | — | row sum `ln_s_acc_ += x` |
| LayerNorm ST2 | `adder32_lane` (ADD_32 sub) | `H * x_k`, `D_k^2`, `V` accumulation, var requant (`ln_requant_clamp`) |
| LayerNorm ST3 | `clamp_lane` | offset `u` (`ln_rshift_wide`) |
| LayerNorm ST4 | `clamp_lane` | `r * D_k * M0_div` (i128), shift (`ln_rshift_wide`) |
| LayerNorm ST5 | `clamp_lane` | affine requant (`ln_requant_clamp`) |

#### Components Handled Directly Inside `GvuTop`
1. **Pipeline FSM controller**: `rtl_ref_gvu_ctrl.h` ports only the table-load sequence (`LutInitSequence`), because the delivered RTL controller never leaves `SOFTMAX_ST2`. `GvuTop` therefore implements the full controller (`pipe_ctrl_fsm_`, beat counters `start_d_cnt_` / `end_d_cnt_`, transitions for `SOFTMAX_ST1..ST3` and `LAYERNORM_ST1..ST5`).
2. **LayerNorm scalar registers and helpers**: `ln_s_acc_`, `ln_v_acc_`, `ln_final_s_`, `ln_final_var_`, `ln_final_w_` and the helpers `ln_rshift_wide`, `ln_requant_clamp` (64/128-bit fixed point).

### 1.4 Interface of `GvuTop` against the RTL (`gvu_top.sv` of the hardware drop)

`GvuTop::In` is the set of inputs of one clock edge, `GvuTop::Out` the outputs of that cycle. The right-hand column gives the port of `gvu_top.sv` (RTL of the drop used for the ports: `neo_core/gvu/gvu_top.sv`) or "model only" when the RTL has no such port. The model has more inputs than the RTL because the RTL gets some of them from configuration registers (`gvu_config_regs.sv`), from the sbank, or not at all yet.

**Control and PSUM stream**

| Field | Type, default | Meaning | RTL port |
|---|---|---|---|
| `i_pipeline_en` | bool, 0 | the controller changes state only when it is 1 | `i_pipeline_en` |
| `i_gvu_op` | `GvuOpcode`, `GVU_OP_SOFTMAX` | operation: REQUANT 0, DEQUANT 1, GEMM_FUSED 2, SOFTMAX 3, FUSED_ATTN 4, LAYERNORM 5, RESIDUAL 6, MAXPOOL 7, AVGPOOL 8, ELEMWISE 9 | `i_gvu_op` (same numbering, `gvu_pkg.sv`) |
| `i_psum_valid` | bool, 0 | a PSUM beat is on `i_psum_vec` | `i_psum_valid` |
| `i_psum_vec` | `int32_t[32]`, 0 | the 32 lane int32 PSUM beat | `i_psum_data[NUM_LANE][31:0]` |
| `i_psum_data` | `uint32_t`, 0 | older single-word form of the beat, kept for the three dataset benches (tile mode only; ignored in stream mode) | model only |
| `o_psum_ready` (Out) | bool | constant 1 in the model | `o_psum_ready` (RTL: 1 only in `GEMM_FUSED_OBP`) |

The RTL has two more inputs on this stream, `i_oc_last` and `i_oc_tile_last`, which end the `GEMM_FUSED_OBP` state; the model ends a stream run with `i_gemm_stream_stop` instead (section 4.5). The four table resets of the RTL (`i_lut_act_reset`, `i_lut_exp_reset`, `i_lut_recip_reset`, `i_lut_rsqrt_reset`) are not inputs of the model; a new activation table is requested with `i_gemm_lut_reload`.

**Softmax and attention**

| Field | Type, default | Meaning | RTL port |
|---|---|---|---|
| `i_num_tile` | uint32, 1 | tiles per run | `i_num_tile[8:0]` |
| `i_token_len` | uint32, 0 | beats per stage | `i_token_len[8:0]` |
| `i_attn_z`, `i_attn_scale`, `i_attn_shift` | int8, int32, uint8; 0 | zero point, multiplier and shift of the Softmax requantization | `i_attn_z`, `i_attn_scale`, `i_attn_shift` |

**LayerNorm** (`i_ln_*`, the table in section 3.3): model only. The RTL drop has no LayerNorm configuration ports on `gvu_top.sv`.

**GEMM_FUSED, tile mode**

| Field | Type, default | Meaning | RTL port |
|---|---|---|---|
| `i_gemm_scale` | int32, 1 | multiplier M0 | none (RTL: Out_Scale read from the sbank, word 0 onward) |
| `i_gemm_shift` | int8, 0 | shift TS | none (RTL: Out_Shift read from the sbank, one byte per channel) |
| `i_gemm_zp` | int8, 0 | zero point | `i_gemm_fused_z[7:0]` |
| `i_gemm_per_channel` | bool, 0 | per-channel parameters from the sbank | none |
| `i_gemm_skip_act` | bool, 0 | bypass the activation table | `i_gemm_fused_skip_act` |
| `i_gemm_skip_zp`, `i_gemm_out_int16` | bool, 0 | no zero point / int16 clamp range | model only |
| `i_gemm_round` | `RoundMode`, `RND_NEAR_AWAY` | rounding mode of the round-shift | none (RTL: fixed `RND_NEAR_AWAY`, `NARROW_WRAP`, `NW_16`; `gvu_pipe_controller.sv`) |
| `i_gemm_narrow` | `NarrowMode`, `NARROW_SATURATE` | narrowing to int16 after the round-shift, tile and stream mode: saturate, or wrap as the RTL; the instruction path sets it from the requant knob `HAS_REQ_NARROW` (SAT16 / WRAP16) | none (RTL: fixed `NARROW_WRAP`) |
| `i_gemm_lut_reload` | bool, 0 | load the activation table again from rows 0 and 1 of `vbank#1` buffer 1; hold with `i_pipeline_en` until the state is `LUT_ACT_INIT`, then drop | `i_lut_act_reset` (nearest) |

**GEMM_FUSED, stream mode** (section 4.5)

| Field | Type, default | Meaning | RTL port |
|---|---|---|---|
| `i_gemm_stream` | bool, 0 | stream mode: one beat per cycle on `i_psum_valid`, no token count, no sbank read, no vbank write | none |
| `i_gemm_stream_stop` | bool, 0 | end the run once the pipeline is empty | none (RTL: `i_oc_tile_last & i_oc_last`, 9 beats later) |
| `i_psum_addr` | uint32, 0 | word address of the beat, carried to `o_gemm_fused_addr` | none |
| `i_psum_mask` | uint32, `0xFFFFFFFF` | lane mask of the beat, carried to `o_gemm_fused_mask`; an empty mask passes and leaves empty | none |
| `i_psum_scale`, `i_psum_shift` | int32, uint8; 1, 0 | Out_Scale and Out_Shift of the beat's output channel | none (RTL: sbank) |
| `i_gemm_zp16` | int32, 0 | zero point into the 16-bit adder (wraps), held while beats are in flight; the connection drives it with the 8-bit register value sign-extended | `i_gemm_fused_z` is 8 bits |

**Residual add** (section 5): `i_res_scale_{a,b,out}` (int32, 1), `i_res_shift_{a,b,out}` (uint8, 0), `i_res_zp_{a,b,out}` (int8, 0), `i_res_round` (`RND_NEAR_UP`), `i_res_out_int16` (0), `i_res_{a,b,sum}_row` (0), `i_res_last_lane_mask` (`0xFFFFFFFF`): model only. The RTL drop has no residual state and no residual ports.

**Max pool** (section 6)

| Field | Type, default | Meaning | RTL port |
|---|---|---|---|
| `i_pooling_mode` | int, `POOLING_2X2` | window: `POOLING_2X2` 0, `3X3` 1, `4X4` 2, `5X5` 3 | `i_pooling_mode` |
| `i_pooling_stride` | uint, 1 | stride | `i_pooling_stride[2:0]` |
| `i_pooling_num_w_tile` | uint, 1 | W tiles of 32 columns | `i_pooling_num_w_tile[7:0]` |
| `i_pooling_c`, `i_pooling_oh` | uint, 1 | channels of the run, output rows | `i_pooling_c[7:0]`, `i_pooling_oh[7:0]` |
| `i_pooling_c_move`, `i_pooling_h_move` | uint, 0 | row step between channels (`num_w_tile * H`) and between output rows (`num_w_tile * stride`) | `i_pooling_c_move`, `i_pooling_h_move` |

**Buffers and Scratchpad DMA**

| Field | Type | Meaning | RTL port |
|---|---|---|---|
| `i_sbank_buf_sel`, `i_vbank{0,1,2}_buf_sel` | bool, 0 | ping-pong buffer used by the unit | same names |
| `i_scratch_dma_{en,wen,addr,wdata,wmask}` | | one Scratchpad DMA port; does nothing (section 1.2) | RTL: separate `i_dma_sbank_*` (32 bit) and `i_dma_vbank_*` (`DMA_DW` bit) ports |

**Outputs (`GvuTop::Out`)**

| Field | Meaning | RTL port |
|---|---|---|
| `o_psum_ready` | constant 1 | `o_psum_ready` |
| `o_softmax_lut_init_done`, `o_layernorm_lut_init_done`, `o_gemm_fused_lut_init_done` | the table of the operation is loaded | one `o_lut_init_done` |
| `o_softmax_pipe_done`, `o_layernorm_pipe_done`, `o_gemm_fused_pipe_done`, `o_residual_pipe_done`, `o_maxpool_pipe_done` | the operation is finished | one `o_pipe_done` |
| `o_gvu_pipe_ctrl_state` | controller state (`control_fsm_t` of the model, section 1.5) | model only |
| `o_gemm_fused_data`, `_valid` | int8 result row and its valid strobe (stream mode: `_addr`, `_mask`, `_busy` too) | none: the RTL writes the result to `vbank#2` |
| `o_residual_data`, `_valid` | int8 result row of the residual add | model only |
| `o_scratch_dma_rdata` | zero | `o_dma_sbank_rdata`, `o_dma_vbank_rdata` |
| `d_vbank0_en`, `d_vbank0_addr`, `d_vbank2_wen_1`, `d_vbank2_addr_1`, `d_vbank2_wmask_1`, `d_vbank2_wdata_1` | Scratchpad requests of the cycle, for cycle traces against the RTL (`tools/has/gvu_oracle/tb_oracle_maxpool.sv`) | model only |

### 1.5 Control states against the RTL

The model numbers its states in `control_fsm_t` (`has/rtl_ref_gvu_top.h`); the RTL numbers them in `gvu_pkg.sv` and has two more names. The names agree except where the table says so.

| State | Model code | RTL code (6 bit) | Note |
|---|---|---|---|
| `IDLE` | 0 | 0 | |
| `LUT_ACT_INIT` | 1 | 1 | 2 writes of 128 bytes from `vbank#1` |
| `LUT_EXP_INIT` | 2 | 2 | |
| `LUT_RECIP_INIT` | 3 | 3 | |
| `LUT_RSQRT_INIT` | 4 | 4 | |
| `GEMM_FUSED_ST1` | 13 | `GEMM_FUSED_OBP` = 5 | same role. The RTL leaves it on `gemm_fused_last_write`; the model leaves it after `token_len` beats (tile mode) or on `i_gemm_stream_stop` (stream mode) |
| `SOFTMAX_ST1` to `ST3` | 5 to 7 | 6 to 8 | |
| `LAYERNORM_ST1` to `ST5` | 8 to 12 | 9 to 13 | |
| `RESIDUAL_ST1`, `RESIDUAL_ST2` | 14, 15 | none | residual add is not in the RTL drop |
| `POOLING_MAX` | 16 | 14 | |
| `POOLING_AVG` | none | 15 | RTL has the state and the adder / multiplier configuration, but the data path to `vbank#2` is not connected (finding B6, section 9.3) |

The numbers in the model are not those of the RTL, so a trace from the RTL is compared by name, not by code.

## 2. Softmax Pipeline Implementation

### 2.1 Mathematical Formulation & HW Principle (MAS §5.2.3)
$$\text{Softmax}(x_i) = \frac{e^{x_i - \max(x)}}{\sum_{j=1}^H e^{x_j - \max(x)}}$$

Hardware computes the non-negative index $d_i = \text{row\_max} - x_i \ge 0$ and looks it up in an $e^{-x}$ table (`LUT_exp`), so no signed index is needed.

### 2.2 FSM States & Execution Stages

```
   +--------+      +----------------+      +--------------+
   |  IDLE  |----->| LUT_RECIP_INIT |----->| LUT_EXP_INIT |
   +--------+      +----------------+      +--------------+
                                                  |
                                                  v
   +--------+      +----------------+      +--------------+
   |  IDLE  |<-----|  SOFTMAX_ST3   |<-----| SOFTMAX_ST2  |<----- SOFTMAX_ST1
   +--------+      +----------------+      +--------------+
```

Each beat reads one 32-lane row; `i_token_len` (256 in the tests) is the number of beats per stage. Stage boundaries use `softmax_token_last = (end_d_cnt_ == token_len - 1)`.

#### Step 0: LUT Initialization (`LUT_RECIP_INIT`, `LUT_EXP_INIT`)
* Source: `vbank#1` port 0, rows of 1024 bits; a table write follows its read by 2 cycles.
* `LUT_RECIP_INIT` (6 cycles): 4 writes of 64 × 16-bit words → all 256 entries of `LUT_recip`.
* `LUT_EXP_INIT` (2 cycles): 2 writes of 128 bytes. The 7-bit address increment of the table memory is 128 & 0x7F = 0, so **both writes land on entries 0..127** and entries 128..255 are never written (RTL behaviour, see `rtl_ref_gvu_ctrl.h`).
* `o_softmax_lut_init_done` = both tables loaded. The flags persist, so later softmax runs skip the init states.

#### Stage 1: Row Maximum Search (`SOFTMAX_ST1`)
* **Input**: x_i (32 × int8) from `vbank#0`.
* **FU**: `vcmp_` in `CMP_ACCUMULATE` / `CMP_MAX`, signed 8-bit.
* **Writeback**: when the comparator output is valid, `row_max` (32 × int8) is written to `vbank#1` port 2 and the FSM moves to `SOFTMAX_ST2`.

#### Stage 2: Exponential Lookup & Summation (`SOFTMAX_ST2`)
* **Inputs**: x_i from `vbank#0`, `row_max` from `vbank#1` port 2.
* **Adder#2** (`adder32_lane`, `ADD_8`, sub mode): $d_i = \text{row\_max} - x_i$, passed through `vint8[0]` to the LUT.
* **LUT unit**: $e_i = \text{LUT\_exp}[d_i]$.
* **Adder#1** (`adder32_lane`, `ADD_32`): running sum $\sigma = \sum e_j$ held in Stream VRF `vint32[2]`.
* **Writeback**: e_i (32 × int8) to `vbank#2` port 1 on every valid lookup.
* **σ writeback (intended)**: at `softmax_token_last` the code drives σ on `vbank#1` port 0 write data. **This write never reaches the SRAM**: the `vbank#1` port select stays at 2 in `SOFTMAX_ST2`, so the VLSU enables port 2, not port 0 (Section 9.2, item 2).

#### Stage 3: Attention Score Normalization (`SOFTMAX_ST3`)
* **Inputs**: σ from `vbank#1` port 0 (the LUT_recip index), e_i from `vbank#2` port 1.
* **LUT unit**: $(r, E_{\text{shift}}) = \text{LUT\_recip}(\sigma)$; E_shift is delayed 3 cycles to line up with the product.
* **Mul#0**: `vmul48x40_lane(MB_3X16X8)`: $e_i \times r$.
* **RoundShift#0**: `vroundshift72_lane` by E_shift (round-nearest-away, 32-bit wrap).
* **Mul#1**: `vmul32x32_lane(MA_32X32)` by `i_attn_scale`.
* **RoundShift#1**: `vroundshift64_lane` by `i_attn_shift` (narrow to 16 bits).
* **Adder#2**: `adder32_lane(ADD_16)` adds `i_attn_z`.
* **Clamp**: `clamp_lane` to signed int8 [−128, 127].
* **Writeback**: attention score (32 × int8) to `vbank#0` when `vint16[5]` is valid.
* **Completion**: `o_softmax_pipe_done` = (`SOFTMAX_ST3` and `softmax_token_last`); the FSM returns to `IDLE`.

Stream VRF registers used: `vint8[0]`, `vint32[2]` (ST2); `vint16[2]`, `vint32[1]`, `vint32[3]`, `vint64[1]`, `vint16[4]`, `vint16[5]` (ST3).

---

## 3. LayerNorm Pipeline Implementation

### 3.1 Mathematical Formulation & HW Optimization (MAS §5.2.5)
$$\mu = \frac{1}{H} \sum_{k=1}^H x_k, \quad \sigma^2 = \frac{1}{H} \sum_{k=1}^H (x_k - \mu)^2, \quad z_k = \frac{x_k - \mu}{\sqrt{\sigma^2 + \epsilon}}, \quad y_k = \gamma_k z_k + \beta_k$$

Hardware avoids dividing by H with the row sum $S = \sum x_k$ and $D_k = H \cdot x_k - S$; the factor 1/H is folded into the requantization multipliers ($M_0$) and shifts ($TS$).

### 3.2 FSM States & Execution Stages

```
   +--------+      +----------------+      +---------------+
   |  IDLE  |----->| LUT_RSQRT_INIT |----->| LAYERNORM_ST1 |
   +--------+      +----------------+      +---------------+
                                                   |
                                                   v
   +--------+      +----------------+      +---------------+
   |  IDLE  |<-----|  LAYERNORM_ST5 |<-----| LAYERNORM_ST4 |<----- LAYERNORM_ST3 <----- LAYERNORM_ST2
   +--------+      +----------------+      +---------------+
```

Stage boundaries use `layernorm_token_last = (end_d_cnt_ >= token_len - 1)`. H = `i_ln_h` if > 0, else `i_token_len`, else 32.

> [!IMPORTANT]
> In the current model **S, var and w are single scalars** shared by all 32 lanes, and **γ and β are one value per beat** shared by all lanes. See Section 9.2, items 3 and 4.

#### Step 0: LUT Initialization (`LUT_RSQRT_INIT`)
* 10 cycles: 8 writes of 64 × 16-bit words from `vbank#1` port 0 → all 512 entries of `LUT_rsqrt`.
* `o_layernorm_lut_init_done` stays set; later LayerNorm runs go straight to `LAYERNORM_ST1`.

#### Stage 1: Row Sum (`LAYERNORM_ST1`)
* **Input**: 32 × int8 from `vbank#0` per beat.
* **Operation** (plain C++): `ln_s_acc_` adds **all 32 lanes** of every beat into one scalar.
* **Stream VRF**: the scalar is streamed into `vint32[0]` (same value in every lane).
* **Writeback**: at `layernorm_token_last` the scalar is written to all 32 lanes of `vbank#1` port 0. The value written is the register **before** the last beats are added; in the trace it covers 254 of 256 beats (Section 9.2, item 3).

#### Stage 2: Deviation & Variance (`LAYERNORM_ST2`)
* **Inputs**: x_k from `vbank#0`, S from `vbank#1` port 0 (if that read is zero, the internal `ln_final_s_` is used instead).
* **Operation**: `H * x_k` (plain C++), then **Adder#2** `adder32_lane(ADD_32, sub)`: $D_k = H \cdot x_k - S$.
* **Variance** (plain C++): `ln_v_acc_ += D_k^2` over all lanes; at row end $V_{\text{pre}} = V \gg \text{Pre\_Shift}$ and
  $$var = \text{clamp8}(\text{RoundShift}(V_{\text{pre}} \cdot M_{0\_\text{var}}, TS_{\text{var}}) + Z_{\text{var}})$$
* **Writeback**: D_k (32 × int32) to `vbank#2` port 0 every beat (also `vint32[2]`, and D_k² to `vint64[0]`); at row end the scalar var is written to all lanes of `vbank#0`.

#### Stage 3: Offset & Floor (`LAYERNORM_ST3`, 3 cycles)
* **Input**: the scalar var (`ln_final_var_`).
* **Operation**:
  $$u = \text{RoundShift}((var - Z_{\text{var}}) \cdot M_{0\_7} + E_{\text{bias}}, TS_7), \quad f = \text{clamp8}(u + Z_7)$$
  $$w = (f > Z_7) \;?\; (f - Z_7) \;:\; (\texttt{i\_ln\_r8\_floor\_z\_out} \;?\; \texttt{i\_ln\_r8\_z\_out} : 1)$$
* **Writeback**: scalar w (16 bit) to all lanes of `vbank#1` port 1; also `vint8[0]` (var) and `vint16[0]` (w).

#### Stage 4: Normalization (`LAYERNORM_ST4`)
* **Inputs**: w from `vbank#1` port 1 (if zero, `ln_final_w_` is used), D_k from `vbank#2` port 0.
* **LUT unit**: $(r, E) = \text{LUT\_rsqrt}(w)$; r = 1 if the table returns 0.
* **Operation** (plain C++, i128): $P = r \cdot D_k \cdot M_{0\_\text{div}}$ (with `i_ln_r9_exact = false`, the low `i_ln_r9_lsb_bits` are dropped before multiplying by M0_div), then
  $$z_k = \text{clamp8}(\text{RoundShift}(P, TS_{\text{div}} + E) + Z_{\text{div}})$$
  via `clamp_lane`.
* **Writeback**: z_k (32 × int8) to `vbank#0` and `vint8[0]`.

#### Stage 5: Affine Scale and Bias (`LAYERNORM_ST5`)
* **Inputs**: z_k from `vbank#0`; γ = `sbank` port 1 byte (γ = 1 if the byte is 0); β = `sbank` port 0 word. One γ and one β per beat, shared by all lanes.
* **Operation**:
  $$t_k = \text{clamp}(\text{RoundShift}((z_k - Z_{\text{div}}) \cdot \gamma \cdot M_{0\_\text{mul}}, TS_{\text{mul}}) + Z_{\text{mul}}, lo, hi)$$
  $$y_k = \text{clamp}(t_k - Z_{\text{mul}} + \beta + Z_{\text{out}}, lo, hi)$$
  with [lo, hi] = [−128, 127], or [−32768, 32767] when `i_ln_out_int16 = 1`.
* **Writeback**: the low byte of y_k (32 × int8) to `vbank#1` port 2. With `i_ln_out_int16 = 1` the upper byte is dropped.
* **Completion**: `o_layernorm_pipe_done` = (`LAYERNORM_ST5` and `layernorm_token_last`); the FSM returns to `IDLE`. `o_softmax_pipe_done` is asserted in the same cycle as well.

### 3.3 LayerNorm Configuration Parameters (`GvuTop::In`)

| Parameter | C++ type | Default | Role |
|---|---|---|---|
| `i_ln_h` | `int32_t` | 0 | H; 0 → use `i_token_len` (then 32) |
| `i_ln_pre_shift` | `int32_t` | 0 | Pre_Shift applied to V |
| `i_ln_m0_var` | `int64_t` | 1 | M0_var (variance requant) |
| `i_ln_ts_var` | `int32_t` | 0 | TS_var |
| `i_ln_z_var` | `int32_t` | −128 | Z_var |
| `i_ln_m0_7` | `int64_t` | 1 | M0_7 (offset u) |
| `i_ln_ts_7` | `int32_t` | 0 | TS_7 |
| `i_ln_e_bias` | `int64_t` | 0 | E_bias (ε term) |
| `i_ln_z_7` | `int32_t` | −128 | Z_7 |
| `i_ln_m0_div` | `int64_t` | 1 | M0_div (normalization) |
| `i_ln_ts_div` | `int32_t` | 0 | TS_div |
| `i_ln_z_div` | `int32_t` | 0 | Z_div |
| `i_ln_m0_mul` | `int64_t` | 1 | M0_mul (affine) |
| `i_ln_ts_mul` | `int32_t` | 0 | TS_mul |
| `i_ln_z_mul` | `int32_t` | 0 | Z_mul |
| `i_ln_z_out` | `int32_t` | 0 | Z_out |
| `i_ln_r8_floor_z_out` | `bool` | true | Stage 3 floor: use `i_ln_r8_z_out` (else 1) |
| `i_ln_r8_z_out` | `int32_t` | 1 | Stage 3 floor value |
| `i_ln_r9_exact` | `bool` | true | Stage 4: exact product (else drop low bits) |
| `i_ln_r9_lsb_bits` | `int32_t` | 32 | Stage 4: bits dropped when not exact |
| `i_ln_out_int16` | `bool` | false | int16 clamp range instead of int8 |

M0 and TS values are per-tensor scalars. The MAS reference (`has::LnParams`) has per-channel `gamma`, `beta`, `M0_mul` and `TS_mul`; the model has one `M0_mul` / `TS_mul` and reads γ / β from `sbank`. `tb_gvu_top_layernorm.cpp` sets none of these parameters, so the defaults above are used.

## 4. GEMM_FUSED Requantization and Activation Pipeline

### 4.1 Mathematical formulation and hardware mapping (MAS 5.2.1, 5.2.2, Table 5-2)
The fused GEMM pipeline post-processes the partial sums of the matrix-multiplication engine: requantization followed by a direct table activation.

1. **Requantization**
   $$P_l = x_{\text{psum}, l} \times M_0, \quad Q_l = \text{RoundShift}(P_l, TS), \quad A_l = Q_l + ZP, \quad y_{\text{clamp}, l} = \text{clamp}(A_l, lo, hi)$$
   with [lo, hi] = [-128, 127] (or [-32768, 32767] when `i_gemm_out_int16` is set). `ZP` is left out when `i_gemm_skip_zp` is set.
2. **Activation**
   $$y_{\text{act}, l} = \text{LUT\_act}[y_{\text{clamp}, l} + 128] \;(\text{if not skip\_act}), \quad y_{\text{clamp}, l}\;(\text{else})$$
   The +128 turns the signed byte into the index 0 to 255 of the 256-byte table.

### 4.2 Functional units

| Operation | Unit | Configuration | Role |
|---|---|---|---|
| Scaling | Mul#1 (`vmul32x32_lane`) | `MA_32X32` | 32-bit signed multiply psum x M0 |
| Narrowing | RoundShift#1 (`vroundshift64_lane`) | `NW_16` | round by TS (mode `i_gemm_round`), narrowed to 16 bits as `i_gemm_narrow` says (default: the model saturates; `NARROW_WRAP` as the RTL wraps: `NARROW_WRAP`, section 9.3) |
| Zero point | Adder#2 (`adder32_lane`) | `ADD_16` | 16-bit add of ZP (wraps) |
| Clamp | `clamp_lane` | 16-bit signed input | to [-128, 127] |
| Activation | LUT unit (`vlut_.mem.rdata8`) | `LUT_ACT` direct lookup | 256-byte table indexed by y + 128 |
| Result | tile mode: `vbank#0` and the output ports; stream mode: the output ports only | | 32 x int8 |

### 4.3 Tile mode (default): states and flow

```
   IDLE ---> LUT_ACT_INIT ---> GEMM_FUSED_ST1 ---> IDLE
```

1. **`LUT_ACT_INIT`**: skipped if `o_gemm_fused_lut_init_done` is already high and `i_gemm_lut_reload` is 0. Source: `vbank#1` port 0, rows 0 and 1 of the buffer chosen by `i_vbank1_buf_sel`, 1,024 bits each; 2 writes of 128 bytes load the whole 256-byte table (`INIT_DW` 1,024 bits). Then `GEMM_FUSED_ST1`.
2. **`GEMM_FUSED_ST1`**: on each cycle with `i_psum_valid`: 32 lanes of `i_psum_vec` through Mul#1, RoundShift#1, Adder#2, Clamp and the table; the packed 32 x int8 row goes to `vbank#0` at address `end_d_cnt_` and to `o_gemm_fused_data` / `o_gemm_fused_valid`. When `vbank2` holds the PSUM tile instead (cycles without `i_psum_valid`), the unit reads it and runs the same path (the form the dataset benches use). Completion at `end_d_cnt_ == token_len - 1`: `o_gemm_fused_pipe_done`, back to `IDLE`.
3. **Parameters per channel** are read from the sbank by cycle count since the state was entered, with both sbank ports starting at address 0: scale (word) and shift (byte) overlap for more than one channel, so `i_gemm_per_channel` is not usable with any sbank layout today (read: `has/rtl_ref_gvu_top.h`; see section 9.3, M3). The stream mode below takes the parameters with the beat and avoids this.

### 4.4 Tile mode parameters (`GvuTop::In`)

| Parameter | Type | Default | Description |
|---|---|---|---|
| `i_gemm_scale` | int32 | 1 | multiplier M0 |
| `i_gemm_shift` | int8 | 0 | shift TS |
| `i_gemm_zp` | int8 | 0 | zero point |
| `i_gemm_per_channel` | bool | false | per-channel selection (see 4.3) |
| `i_gemm_skip_act` | bool | false | bypass the table |
| `i_gemm_skip_zp` | bool | false | bypass the zero point |
| `i_gemm_out_int16` | bool | false | int16 range instead of int8 |
| `i_gemm_round` | `RoundMode` | `RND_NEAR_AWAY` | `RND_FLOOR`, `RND_NEAR_UP`, `RND_NEAR_AWAY`, `RND_NEAR_EVEN`, `RND_CEIL`, `RND_TRUNC`, `RND_AWAY` |
| `i_psum_vec`, `i_psum_valid` | `int32_t[32]`, bool | 0, false | the PSUM beat |

### 4.5 Stream mode (`i_gemm_stream`)

Purpose: take the PSUM of the SAURIA core one write at a time, as the PSM-to-GVU stream of the RTL does, and return the int8 result of each beat. In `GEMM_FUSED_ST1` with `i_gemm_stream = 1` the unit counts no tokens, does not read `vbank#2` or the sbank and does not write `vbank#0`.

Per beat: `i_psum_valid` with `i_psum_vec`, `i_psum_addr`, `i_psum_mask`, `i_psum_scale` and `i_psum_shift` (one output channel per beat). The beat leaves 9 cycles after it entered with `o_gemm_fused_valid`, `_data`, `_addr`, `_mask`. A beat with an empty mask goes through and leaves with an empty mask. At most one beat per cycle, gaps allowed. The rounding mode, `i_gemm_zp16` (16 bits, wraps), `i_gemm_skip_act` are held while beats are in flight.

| Stage | Cycles | Register cut (MAS drawings of Requant and GEMM_FUSED) |
|---|---|---|
| multiplier 32 x 32 | 4 | |
| v32int64 register | 1 | |
| round-shift, saturate to int16 | 1 | |
| v32int16 register | 1 | |
| add zero point (16 bits) | 0 | |
| v32int16 register | 1 | |
| clamp to int8 | 0 | |
| v32int8 register | 1 | |
| LUT_act | 0 | |
| **depth** | **9** | first result at the 10th tick counted from the one that carries the beat |

The depth is one build parameter, `GVU_GEMM_STREAM_DEPTH` at the top of `has/rtl_ref_gvu_top.h` (default 9, range 9 to 16), shared by `GvuTop`, the clocked connection (`GvuTopStream::kDepth`) and the call-style connection (`GvuTopBridge::kDepth`). A depth above 9 adds pure delay stages behind the multiplier: values do not change. `-DGVU_GEMM_STREAM_DEPTH=11` gives the 11 cycles the hardware RTL measures (two of them to read Out_Scale / Out_Shift from the sbank). Checked by `tools/has/tb_gvu_top_params.cpp` (depth 11 equal to depth 9 lane by lane, latency 11) and by the stream test built with 11.

Entering and leaving a run:
1. write the 256-byte table into rows 0 and 1 of `vbank#1` buffer 1 and set `i_vbank1_buf_sel`; for a second layer also set `i_gemm_lut_reload` and hold it together with `i_pipeline_en` until the state is `LUT_ACT_INIT`, then drop it (otherwise the controller, which passes through `IDLE`, loads the table again);
2. set `i_gvu_op = GVU_OP_GEMM_FUSED`, `i_gemm_stream = 1`, `i_pipeline_en = 1`; the controller reaches `GEMM_FUSED_ST1` after `LUT_ACT_INIT` (5 cycles from `IDLE` measured on a spare unit, `GvuTopBridge::init_cycles_`);
3. send beats;
4. set `i_gemm_stream_stop = 1`; the controller returns to `IDLE` once the pipeline is empty; drop `i_pipeline_en`.

Against the RTL of the hardware drop (read and measured, section 9.3 and `GVU_TOP_CORE_INTEGRATION.md`, sections 3 and 7):

| Item | Model, stream mode | RTL `GEMM_FUSED_OBP` |
|---|---|---|
| Out_Scale / Out_Shift | with the beat | read from the sbank, scale at the first beat, shift when the scale is almost done; index runs on 1 per beat and resets on a state change |
| Result | on the output ports | written to `vbank#2` port 1 (the controller raises `o_vbank2_rd` instead of `o_vbank2_wr`: finding B1) |
| Latency psum to result | 9 cycles after the beat is latched | 11 beats from psum to the `vbank#2` write, 2 of them the sbank read (measured on the RTL by the hardware-review runs) |
| Narrowing after round-shift | saturate to int16 | wrap to 16 bits (`NARROW_WRAP`, `NW_16`) |
| End of run | `i_gemm_stream_stop` | `i_oc_tile_last & i_oc_last`, 9 beats later (finding B9) |
| Zero point | 16 bits | `i_gemm_fused_z`, 8 bits |

The model's value path is equal to `has::requant` followed by the table (`has/gvu_quant.h`), which the hardware-review run of the GEMM vector shows equal to the RTL with the one-line change of B1 (0 of 20 lanes different, `GVU_TOP_CORE_INTEGRATION.md`, section 4.4).

---

## 5. Element-Wise Residual Add Pipeline Implementation

### 5.1 Mathematical Formulation & HW Mapping (MAS §5.2.6, Fig 5-7, Fig 5-18, Table 5-2, Table 5-4, Table 5-5)
The Element-Wise Residual Add pipeline (`GVU_OP_RESIDUAL` / `GVU_OP_ELEMWISE`) implements 2-stage residual summation across two quantized tensors: Tensor A (preloaded in `vbank#0`) and Tensor B (preloaded in `vbank#1`).

1. **Stage 1: Dual Dequantization and 32-bit Addition:**
   $$D_{A,l} = \text{RoundShift}\Big(\big(A_l - ZP_A\big) \times M_{0,A}, \; TS_A\Big)$$
   $$D_{B,l} = \text{RoundShift}\Big(\big(B_l - ZP_B\big) \times M_{0,B}, \; TS_B\Big)$$
   $$S_l = D_{A,l} + D_{B,l} \quad (\text{stored as 32-bit int32 in } \texttt{vbank\#2} \text{ port 0})$$
   Both dequantized operands $D_{A,l}$ and $D_{B,l}$ are 32-bit signed integers, and their sum $S_l$ is stored as a 1024-bit vector (32 × int32) into `vbank#2`.

2. **Stage 2: Requantization and Output Clamping:**
   $$P_l = S_l \times M_{0,\text{out}}$$
   $$Q_l = \text{RoundShift}\big(P_l, \; TS_{\text{out}}\big)$$
   $$A_l = Q_l + ZP_{\text{out}}$$
   $$Y_l = \text{clamp}(A_l, lo, hi) \quad (\text{stored in } \texttt{vbank\#0} \text{ and driven to } \texttt{o\_residual\_data})$$
   where $[lo, hi] = [-128, 127]$ (or $[-32768, 32767]$ when $\texttt{i\_res\_out\_int16}$ is set).

### 5.2 Hardware Functional Units & Datapath

| Stage | Operation | Functional Unit | Configuration | Role |
|---|---|---|---|---|
| **Stage 1** | Subtract $ZP_A$ | **Adder#1** (`adder32_lane`) | `ADD_16`, sub mode | 16-bit subtraction $(A_l - ZP_A)$ |
| **Stage 1** | Scale $A$ | **Mul#0** (`vmul48x40_lane`) | `MB_40X32` | 40-bit signed multiply $(A_l - ZP_A) \times M_{0,A}$ |
| **Stage 1** | Round-shift $A$ | **RoundShift#0** (`vroundshift72_lane`) | `NW_32` | Round-nearest-up by $TS_A$, narrowed to 32 bits |
| **Stage 1** | Subtract $ZP_B$ | **Adder#2** (`adder32_lane`) | `ADD_16`, sub mode | 16-bit subtraction $(B_l - ZP_B)$ |
| **Stage 1** | Scale $B$ | **Mul#1** (`vmul32x32_lane`) | `MA_32X16` | 32-bit multiply $(B_l - ZP_B) \times M_{0,B}$ |
| **Stage 1** | Round-shift $B$ | **RoundShift#1** (`vroundshift64_lane`) | `NW_32` | Round-nearest-up by $TS_B$, narrowed to 32 bits |
| **Stage 1** | 32-bit Add | **Adder#1** (`adder32_lane`) | `ADD_32` | Dual 32-bit sum $S_l = D_{A,l} + D_{B,l}$ |
| **Stage 1** | Stage 1 Writeback | **Scratchpad** (`vbank#2`) | Port 0, single-port | 1024-bit word (32 × int32) written to `vbank#2` at row `r` |
| **Stage 2** | Scale Sum | **Mul#1** (`vmul32x32_lane`) | `MA_32X32` | 32-bit multiply $S_l \times M_{0,\text{out}}$ |
| **Stage 2** | Narrowing | **RoundShift#1** (`vroundshift64_lane`) | `NW_16` | Round-nearest-up by $TS_{\text{out}}$, narrowed to 16 bits |
| **Stage 2** | Zero-Point Add | **Adder#2** (`adder32_lane`) | `ADD_16` | 16-bit add of $ZP_{\text{out}}$ |
| **Stage 2** | Clamping | **Clamp Unit** (`clamp_lane`) | 16-bit signed | Saturated clamp to $[-128, 127]$ (or $[-32768, 32767]$) |
| **Stage 2** | Output Writeback | **Scratchpad** (`vbank#0`) & Output Ports | Port 0, single-port | Packed 32 × int8 written to `vbank#0`; driven to `o_residual_data` with `o_residual_valid = 1` |

### 5.3 FSM States & Execution Flow

```
   +--------+      +----------------+      +----------------+      +--------+
   |  IDLE  |----->|  RESIDUAL_ST1  |----->|  RESIDUAL_ST2  |----->|  IDLE  |
   +--------+      +----------------+      +----------------+      +--------+
```

1. **Stage 1: Dequantize & Add (`RESIDUAL_ST1`)**:
   - `vbank#0` provides Tensor A row $r$ (v32int8).
   - `vbank#1` port 2 provides Tensor B row $r$ (v32int8 slice from 128-byte SRAM lines holding 4 vectors each). The quadrant selector `b_part_delay_` tracks quadrant alignment across the 2-cycle read delay.
   - Dequantization arithmetic executes in parallel on Mul#0 / RS#0 and Mul#1 / RS#1.
   - 32-bit adder computes sum $S_l$ and writes to `vbank#2` port 0.
   - Counter `start_d_cnt_` tracks issued reads ($0 \dots \text{token\_len} - 1$); unshifted delayed beat counter `end_d_cnt_` tracks write completion.
   - When all beats are committed, asserts `w.residual_st1_done` and transitions to `RESIDUAL_ST2`.
2. **Stage 2: Requantize & Writeback (`RESIDUAL_ST2`)**:
   - Reads intermediate 32-bit sums from `vbank#2` port 0.
   - Ingests sums through Mul#1 -> RS#1 -> Adder#2 -> Clamp Unit.
   - Writes final quantized elements (32 × int8) to `vbank#0` at row $r$.
   - Drives streaming output ports `o_residual_data` and asserts `o_residual_valid = 1`.
   - On final beat completion, asserts `o_residual_pipe_done = 1` and returns to `IDLE`.

### 5.4 Residual Configuration Parameters (`GvuTop::In`)

| Parameter | C++ Type | Default | Description |
|---|---|---|---|
| `i_res_scale_a` | `int32_t` | 1 | Multiplier $M_{0,A}$ for Tensor A |
| `i_res_shift_a` | `uint8_t` | 0 | Shift amount $TS_A$ for Tensor A |
| `i_res_zp_a` | `int8_t` | 0 | Zero-point offset $ZP_A$ for Tensor A |
| `i_res_scale_b` | `int32_t` | 1 | Multiplier $M_{0,B}$ for Tensor B |
| `i_res_shift_b` | `uint8_t` | 0 | Shift amount $TS_B$ for Tensor B |
| `i_res_zp_b` | `int8_t` | 0 | Zero-point offset $ZP_B$ for Tensor B |
| `i_res_scale_out` | `int32_t` | 1 | Multiplier $M_{0,\text{out}}$ for Output |
| `i_res_shift_out` | `uint8_t` | 0 | Shift amount $TS_{\text{out}}$ for Output |
| `i_res_zp_out` | `int8_t` | 0 | Zero-point offset $ZP_{\text{out}}$ for Output |
| `i_res_round` | `RoundMode` | `RND_NEAR_UP` | Rounding mode (nearest-even, nearest-up, etc.) |
| `i_res_out_int16` | `bool` | false | Clamp to int16 $[-32768, 32767]$ instead of int8 |
| `i_res_a_row` | `unsigned` | 0 | Starting row index in `vbank#0` for Tensor A |
| `i_res_b_row` | `unsigned` | 0 | Starting row index in `vbank#1` for Tensor B |
| `i_res_sum_row` | `unsigned` | 0 | Starting row index in `vbank#2` for intermediate sums |
| `i_res_last_lane_mask` | `uint32_t` | `0xFFFFFFFF` | lanes written in the last row of a run (last tile, last token) by both stages; a tensor whose length is not a multiple of 32 leaves the other lanes of that row untouched |

> [!NOTE]
> The RTL of the hardware drop has no `RESIDUAL_ST1` / `RESIDUAL_ST2` states and no residual ports on `gvu_top.sv`. This operation follows the MAS (section 5.2.6) and its Python reference; there is no RTL oracle for it (section 10).

### 5.5 SRAM Capacity & Token Length Considerations
`vbank#1` has `VBANK1_DEPTH = 32` lines of 128 bytes each, holding a maximum of $32 \times 4 = 128$ vectors of 32 bytes (4,096 elements). While `vbank#0` holds 800 rows and `vbank#2` holds 256 rows, `vbank#1` sets the upper bound for single-buffer residual operands to **128 rows**. The regression testbench `tb_gvu_top_residual_add.cpp` runs 2 tiles of 64 tokens (128 rows of 32 lanes, 4,096 elements), fully utilizing the available capacity of `vbank#1`.

One run of the unit handles at most 128 rows of 32 elements (4,096 elements, the rows of `vbank#1`); a longer tensor is cut into runs of at most 128 rows, each starting from `IDLE`. The socket of the instruction path does this (`GvuTopBridge::add`); the cycles of one run are the cycles from the start to `o_residual_pipe_done` (for example 194, 362 and 270 cycles for tensors of 3,000, 5,600 and 4,097 elements, measured with `tools/has/tb_gvu_top_stream.cpp`).

---

## 6. Max Pool (`GVU_OP_MAXPOOL`, state `POOLING_MAX`)

Port of the RTL of the hardware drop: `gvu_top.sv` (pooling data path to `vbank#2`), `vlsu/gvu_vlsu.sv` (address mode `AGU_POOLING`), `vlsu/gvu_pooling_feeder.sv` (window feeder) and `FUs/gvu_vcmp32.sv` (comparator in per-lane mode). The two library cells the feeder uses, `fifo_v3` (fall-through) and `shift_reg` (depth 2), were not delivered; their behaviour in the model is written from the `common_cells` library and is an assumption until the files arrive (finding B8).

### 6.1 What it computes and the layout
For each channel, the maximum over every k x k window at stride s, with no padding inside the unit (a caller that needs padding writes the pad value, -128 for int8, into the tensor before the run).

| Item | Layout |
|---|---|
| Input | `vbank#0` buffer `i_vbank0_buf_sel`, rows of 32 bytes (32 columns of one W tile): row `c * H * num_w_tile + h * num_w_tile + w_tile`; bytes past W are 0 |
| Output | `vbank#2` port 1 (v32int8) from row 0: row `c * OH * num_w_tile + oh * num_w_tile + w_tile`, lane `ow mod OL`, with OL = 32 for stride 1, `(32 - k) / s + 1` when k = s, else `(32 - k) / s + 2` |
| Registers | `i_pooling_mode`, `_stride`, `_num_w_tile`, `_c`, `_oh`, `_c_move` = `num_w_tile * H`, `_h_move` = `num_w_tile * stride` (section 1.4) |
| Supported (k, s) | 2x2 s1 and s2, 3x3 s1, 4x4 s1, s2 and s4, 5x5 s1, as in the RTL; the other combinations are TODO in the RTL and give no output in the model |
| Run size | channels per run: at most the number that fit `vbank#0` (800 rows) and `vbank#2` port 1 (1,024 rows), and 255 |

### 6.2 Blocks
1. **Pooling feeder** (`has/rtl_ref_gvu_pool_feeder.h`): every `vbank#0` read (one row) is pushed, 2 cycles later, into 32 lane FIFOs of depth 5, byte l into FIFO l. Lane l takes the byte of FIFO `l + x_cnt[l]`, so each output lane sees the k columns of its window one per cycle, row after row, with first / last flags for the accumulate mode of the comparator. Lanes start one cycle after each other, or every k-th lane starts on its own when the windows do not overlap. Indices are 5 bits wide as in the RTL.
2. **Comparator** (`Vcmp32` in `has/rtl_ref_gvu_fu.h`): per-lane mode (`in_lane_mode`), running maximum per lane (`CMP_ACCUMULATE`) loaded on `in_first`, emitted on `in_last`.
3. **Load / store unit** (`has/rtl_ref_gvu_vlsu.h`): `vbank#0` and `vbank#2` take the pooling address when their mode is `AGU_POOLING` (code 2; it was `AGU_WRAPPED` before the RTL of the hardware drop).
4. **Controller**: `IDLE` to `POOLING_MAX` on `i_pipeline_en` with `i_gvu_op = GVU_OP_MAXPOOL`; `o_maxpool_pipe_done` at the last write; back to `IDLE`.

### 6.3 Timing
About 32 cycles per output row of one W tile, almost independent of k: the lanes start one cycle apart and the next read is issued only when lane 30 has started. Measured with `tb_gvu_top_maxpool`: the SPPF max pool of YOLOv8m (288 channels of 20 x 20 padded to 24 x 24, 5 x 5, stride 1, one W tile) takes 184,536 cycles in 9 runs of up to 33 channels (the bench fills `vbank#0`); on the single-channel sets of the hardware vectors, 24,325 cycles for 760 output rows (k2) and 23,049 for 720 rows (k3).

### 6.4 One run needs a reset (finding B12)
The pooling feeder keeps `first_load_cnt` between runs; only the reset clears it. A run started right after another without a reset reads extra rows and gives wrong values: on the RTL, a second `single/stride_1/k3` run of 40 channels differs from the golden in 4,221 of 12,960 values and finishes in 2,181 cycles instead of 23,049 (measured with Verilator; the port gives the same wrong result). The model therefore resets the unit before every run (`GvuTopBridge::maxpool`, `GvuTopStream::maxpool`). A reset also clears the flags "activation table loaded", so a GEMM_FUSED layer after a max pool loads its table again (the sockets handle this).

### 6.5 Checks (measured)

| Check | Result |
|---|---|
| `tools/has/tb_gvu_top_maxpool.cpp <vector dir> 1` | 17 / 17 sets equal to the plain max pool, among them the nine golden sets of the hardware vectors (`single` 288 channels of 20 x 20, `multi` 40 channels of 80 x 80) and the SPPF case; the socket cases (8 / 8: several runs, several W tiles, an add in between, two shapes that must be refused: 3x3 s2 and 5x5 s2) |
| Cycle-by-cycle against the RTL: `tools/has/gvu_oracle/run_verilator_maxpool.sh <rtl gvu dir> <work dir> <vector dir> <tb_gvu_top_maxpool binary>` | for each of the nine sets and a two-run case, one run of the model and of the RTL write the same `vbank#2` rows in the same cycles: 0 lines differ on 10 of 10. The RTL run uses stand-in files for the three undelivered cells (`tools/has/gvu_oracle/*_standin.sv`) |
| A whole layer, runs back to back: `tools/has/gvu_oracle/tb_oracle_maxpool_runs.sv` against `tools/has/tb_gvu_top_maxpool_runs.cpp` | the three SPPF max pools of YOLOv8m and a random set in the format of the hardware vectors (input padded with -128), 21 runs of 14 channels without a reset on the RTL with patch B12: trace equal to the model's run with a reset before each run, 0 wrong; 9 runs of 33 channels: 184,536 unit cycles, as the bench of the row below; ModelSim and Verilator (`VERIFICATION_REPORT.md`, V39) |
| 3 SPPF max pools of YOLOv8m through the socket (`tools/has/tb_rtl_ref_gvu_replay.cpp <program dir> 0 --gvu-top`) | 345,600 elements, 0 differ, 63 groups, 554,472 `GvuTop` cycles (184,824 per instruction); in the instruction path the window of the three instructions takes 558,532 cycles. The 288 cycles between 184,536 (bench, 9 runs) and 184,824 (instruction path, 21 groups of 14 channels, the group size of the default Scratchpad) are the start-up and drain of the 12 extra runs, 24 cycles each (derived) |
| Comparator vectors | the vectors written by `Vcmp32` for the lane mode are identical to the ones replayed on the RTL (0 differences) |

---

## 7. Test Benches and RTL Oracles

### 7.1 Dataset benches (Softmax, GEMM_FUSED tile, residual add)

All three benches (`tb_gvu_top.cpp`, `tb_gvu_top_residual_add.cpp`, `tb_gvu_top_gemm_fused.cpp`) follow one method (`[tile|vec]`):
- **`tile` (default)**: `num_token = 64, token_len = 64, num_tile = 2, tile_lane = 32` (4,096 elements)
- **`vec`**: `num_token = 1, token_len = 64, num_tile = 1, tile_lane = 1` (64 elements)

| Bench | Op | Dataset files (`target/test/<op>/{tile,vec}/`) | Check | Result (measured) |
|---|---|---|---|---|
| `tb_gvu_top.cpp` | Softmax | `lut_recip.txt`, `lut_exp.txt`, `input.txt`, `golden_output.txt` | `vbank0` against `golden_output.txt`, bit exact | `tile` 432 cycles, `vec` 223 cycles, 0 mismatches |
| `tb_gvu_top_residual_add.cpp` | residual add | `input_a.txt`, `input_b.txt`, `golden_sum.txt`, `golden_output.txt` | stage 1 `vbank2` and stage 2 `vbank0` | `tile` 274 cycles, `vec` 140 cycles, 0 mismatches |
| `tb_gvu_top_gemm_fused.cpp` | GEMM_FUSED tile | `lut_act.txt`, `input.txt`, `golden_output.txt` | `vbank0` against `golden_output.txt` | `tile` 144 cycles, `vec` 77 cycles, 0 mismatches |
| `tb_gvu_top_layernorm.cpp` | LayerNorm | see the header of the bench | functional run | 10,480 ns, 0 mismatches |

```bash
make tb_gvu_top tb_gvu_top_residual_add tb_gvu_top_gemm_fused tb_gvu_top_layernorm
./tb_gvu_top tile && ./tb_gvu_top vec
./tb_gvu_top_residual_add tile && ./tb_gvu_top_residual_add vec
./tb_gvu_top_gemm_fused tile && ./tb_gvu_top_gemm_fused vec
```

Sequence of one run: load the dataset into the Scratchpad banks; reset (5 cycles of `tick(in, false)`); one idle tick; set `i_num_tile`, `i_token_len`, the quantization parameters, the buffer selects (`i_vbank1_buf_sel = true` for the table load, switched to false when `*_lut_init_done` rises) and `i_pipeline_en`; one `tick` per cycle with the state, tile counter and beat counters logged; on `pipe_done` plus 15 cycles compare the Scratchpad with the golden files, print the first 8 positions for lanes 0 to 3, exit code 0 on 0 mismatches. The benches build in standalone C++17 mode; `ScGvuTop` (SystemC wrapper) is used when `<systemc.h>` is on the include path.

### 7.2 Benches added for the connection to the core

| Bench | What it checks | Result (measured) |
|---|---|---|
| `tools/has/tb_gvu_top_vs_model.cpp [runs] [seed]` | GEMM_FUSED tile mode, stream mode and residual add of `GvuTop` against the model's own cycle-level units (`has::RtlObp::beat`, `has::RtlElem::add`) on random data over the full value ranges: scales, zero points, the seven rounding modes, psums of mixed size, 12 stream runs of 400 beats in bursts of 1 to 64 with gaps up to 100 cycles, residual adds with a partial last row | 51 / 51 runs, 0 differences (`regress_gvu.sh`: "GvuTop vs model: 51 runs") |
| `tools/has/tb_gvu_top_replay.cpp <dump file>` | replays the epilogue beats of a run of the instruction path (`HAS_OBP_RTL=1 HAS_OBP_DUMP=<file>`) on `GvuTop` outside the core, back to back and one at a time | instructions 86-88 of YOLOv8m: 3,392 beats; window D (`--first 40 --count 2`): 29,808 beats; 0 lanes differ, latency 9 on every beat, address and mask carried |
| `tools/has/tb_rtl_ref_gvu_replay.cpp <program dir> 0 --gvu-top` | every ELEM_WISE instruction of YOLOv8m on `GvuTop` (residual adds and max pools), values and cycles | 12 adds (6,374,400 elements), 3 max pools (345,600 elements), 0 differ |
| `tools/has/tb_gvu_top_maxpool.cpp` | section 6.5 | 17 / 17, socket 8 / 8 |
| `tools/has/tb_gvu_top_stream.cpp` | the clocked stream unit (`GVU_TOP_CORE_INTEGRATION.md`, sections 2.1 and 5.1) | PASS in both orders of the processes |

### 7.3 Oracles against the RTL (Verilator 5)

Run from the repository root on the Linux machine; `<rtl gvu dir>` is a read-only copy of `neo_core/gvu` of the hardware drop.

| Script | Block | Result (measured) |
|---|---|---|
| `bash tools/has/gvu_oracle/run_all.sh <rtl gvu dir> <work dir>` | every block below in one go (about 500 MB of scratch) | one summary line per block, `<work dir>/summary.txt` |
| `run_verilator.sh <rtl gvu dir> <vector dir> <work dir>` | functional units (adders, multipliers, round-shifts, clamp) | `ORACLE ... mismatches <m>` per unit |
| `run_verilator_b.sh <rtl gvu dir> <vector dir> <work dir>` | LUT unit, comparator, 48x40 multiplier, stream registers, Scratchpad, load / store unit | `ORACLE ... mismatches <m>` per block; the load / store unit of the later drop: 0 of 40,000 requests differ |
| `run_verilator_g2.sh <rtl gvu dir> <vector dir> <work dir>` | register file | `ORACLE ... mismatches <m>` |
| `run_verilator_directed.sh <rtl gvu dir> <work dir>` | directed checks of the stream switch and the comparator | prints one line per directed case |
| `run_verilator_maxpool.sh <rtl gvu dir> <work dir> <vector dir> <tb_gvu_top_maxpool binary>` | whole `POOLING_MAX` path, cycle by cycle | section 6.5 |
| `tb_oracle_maxpool_runs.sv` (plusargs `+in +out +h +w +k +s +c +group`, defines `GVU_R3`, `GVU_AGU_ND`) with `chk_maxpool_runs.py` | a whole layer in runs back to back, each run against the golden | `VERIFICATION_REPORT.md`, V39 |
| `run_verilator_top.sh <rtl gvu dir> <work dir>` | elaborates and runs `gvu_top.sv` (Softmax), a report | not a check |

Measured against the latest RTL drop with Verilator 5.052 (`bash tools/has/gvu_oracle/run_all.sh`): with the earlier headers, 18 `ORACLE` lines with 0 mismatches, `stream_vrf` 6,000 / 0 and `scratchpad` 40,000 / 0 differ; the load / store unit gave 11,482 of 40,000 different, all at address code 2 (`AGU_POOLING`), which is why `rtl_ref_gvu_vlsu.h` and `rtl_ref_gvu_fu.h` were re-frozen against this drop. After that: load / store unit 40,000 / 0, comparator 400,000 / 0 (`cmp_out_pipe_0` and `_1` 200,000 / 0), max pool 10 of 10 sets with 0 trace lines different. The `lut_load` line differs in one value only, the state code: the RTL inserts `GEMM_FUSED_OBP` at 5, so `SOFTMAX_ST1` is 6 against 5 in the model; compared by name the table is identical and the cycles to Softmax are the same 9.

Frozen headers: `tools/has/gvu_oracle/frozen_headers.sha256` lists the hashes of the headers checked against the RTL (`rtl_ref_gvu_fu.h`, `_lut.h`, `_vrf.h`, `_vlsu.h`, `_scratchpad.h`, `_ctrl.h`, `_pool_feeder.h`); `regress_gvu.sh` checks them. Changing one of them means running its oracle again and updating the list.

### 7.4 Regression

```bash
bash tools/has/regress_gvu.sh [out dir] [--unit-only]
```
Builds and runs the unit benches, then the instruction path; the output lists every check as PASS or FAIL and ends with `RESULT: <n> passed, <m> failed`. The regression has 131 items (the 26 added cover the register file, the write sequences, the address generator for slice and concat and the check programs through the register file); it sets the five vector-unit knobs to 0 for its reference values. The checks of this unit are the 51 runs of `tb_gvu_top_vs_model`, the stream depth and narrowing runs of `tb_gvu_top_params` (depth 9 and 11), instructions 86-88 with `HAS_GVU_TOP` 1, 2 and 3, window D with 3, the 12 residual adds and the 3 max pools of YOLOv8m on `GvuTop`, and `tb_gvu_top_maxpool`.

---

## 8. Detailed Trace Tool

`trace_gvu_top.cpp` (in `tools/`, built via `make trace_gvu_top`) applies the same reset and stimulus as the testbenches and logs, **every cycle**, everything `GvuTop` exposes publicly:
* FSM state, `start_d_cnt`, `end_d_cnt`, all status outputs (`o_psum_ready`, `*_lut_init_done`, `*_pipe_done`);
* LUT memory write pointer and counter (`lut_waddr`, `wr_cnt`);
* Stream VRF registers `vint64[1]`, `vint32[1..3]` (valid bit and lane data);
* every Scratchpad row (buffer 0 of all four banks) whose contents change, with the new contents.

```bash
make trace_gvu_top
./trace_gvu_top softmax|gemm_fused|residual [--preload] [--byte-mask]
```

---

## 9. Known Issues, Resolved Model Bugs and Hardware Findings

### 9.1 Bugs Resolved During GEMM Fused & Residual Add Implementation

1. **Direct Mode LUT Addressing (`rtl_ref_gvu_lut.h`)**:
   - *Issue*: In direct mode (`LUT_ACT`), table entries are 8-bit bytes written across 1024-bit lines (`INIT_DW / DW = 1024 / 8 = 128` bytes per write). The original code used `(w / DW) & 0x7Fu`, which masked 128 with `0x7F`, evaluating to `0`. Consequently, `lut_waddr` never incremented after the first write, leaving entries 128..255 uninitialized.
   - *Fix*: Changed mask to `& 0x1FFu` so 128 is retained and the second write correctly targets entries 128..255.
2. **16-bit Clamp Sign Extension (`rtl_ref_gvu_top.h`)**:
   - *Issue*: In `GEMM_FUSED_ST1`, `Adder#2` operates in `ADD_16` mode and yields a 16-bit word (`w.adder2_r[l] & 0xFFFFu`). The clamp unit was called with `width_bits = 32`. A negative 16-bit intermediate (e.g., `-152` = `0xFF68`) was interpreted as unsigned `+65384` and erroneously clamped to `+127`.
   - *Fix*: Updated `clamp_lane` invocation to specify `width_bits = 16`, ensuring bit 15 is properly sign-extended before clamping to $[-128, 127]$.
3. **Completion Counter Timing Skew on Multi-Stage Pipelines (`rtl_ref_gvu_top.h`)**:
   - *Issue*: In `tick()`, shifting the delay registers (`vbank0_rd_delay_[1] = vbank0_rd_delay_[0]`) before checking `vbank0_rd_delay_[1]` to increment `end_d_cnt_` caused `end_d_cnt_` to advance 1 cycle too early. As a result, `w.residual_st1_done` and `w.residual_st2_done` were asserted when beat 126 was processed, transitioning to the next state and truncating beat 127.
   - *Fix*: Captured `vbank0_valid_beat = vbank0_rd_delay_[1]` and `vbank2_valid_beat = vbank2_rd_delay_[1]` before register shifting, ensuring exact alignment with data writeback and 100% completion of all beats.

### 9.1.1 RTL updates in the second drop and the model

The updated RTL (`neo_new`) resolved several hardware bugs and refined the Softmax datapath, which have now been incorporated into the cycle-accurate reference model (`rtl_ref_gvu_*.h`):

1. **SRAM Byte-Masking Bug Fix (`sram_single_port.sv`, `sram_dual_port.sv`)**:
   - *RTL Change*: Fixed bit vs byte indexing in write mask (`mem[addr][i*8 +: 8] <= wdata[i*8 +: 8]`).
   - *Model Alignment*: `ScratchpadParams::wmask_as_written = false` is now the natural default, ensuring correct byte-level masking across all scratchpad banks.
2. **LUT Memory Write Width (`gvu_lut_mem.sv`)**:
   - *RTL Change*: Widened `word_per_wr` from `logic [6:0]` to `logic [7:0]`, eliminating overflow during 128-word writes.
   - *Model Alignment*: `rtl_ref_gvu_lut.h` `word_per_wr` and `lut_waddr` calculation verified to handle up to 256/512 entries without wrap-around.
3. **Dedicated 32-Lane Clamp Unit (`gvu_vector_clamp32.sv`)**:
   - *RTL Change*: Refactored generic clamp into `gvu_vector_clamp32` with scalar 32-bit bounds (`i_clamp_hi`, `i_clamp_lo`) and per-lane clamping.
   - *Model Alignment*: Added `clamp32_lane` and `vector_clamp32` in `rtl_ref_gvu_fu.h`.
4. **Per-Lane Shift Widths (`gvu_vroundshift*.sv`)**:
   - *RTL Change*: Parameterized `in_shift` as `[LANES-1:0][$clog2(IN_W)-1:0]` across 64-bit and 72-bit round-shift units.
   - *Model Alignment*: `vroundshift_lane` invoked per lane with individual shift amounts.
5. **Scratchpad Subword Address Delay Pipelines (`gvu_scratchpad.sv`)**:
   - *RTL Change*: Added shift registers matching memory read latency (`SBANK_LATENCY`, `VBANK1_LATENCY`, `VBANK2_LATENCY`) for subword address selection (`sbank_word_addr_1_delay`, `vbank1_word_addr_*_delay`, `vbank2_word_addr_1_delay`).
   - *Model Alignment*: Implemented delay queues `sbank_word_addr_1_delay_`, `vbank1_word_addr_*_delay_`, and `vbank2_word_addr_1_delay_` in `rtl_ref_gvu_scratchpad.h`.
6. **VLSU AGU Reset Mechanism (`gvu_vlsu.sv`)**:
   - *RTL Change*: Added `i_agu_addr_reset` input to force immediate reload of AGU internal address registers when entering new pipeline phases.
   - *Model Alignment*: Ported `i_agu_addr_reset` to `GvuVlsu::In` and wired to phase transition detector `(w.fsm_nxt != pipe_ctrl_fsm_)`.
7. **Softmax Multi-Tile Processing & ST3 Datapath Swap (`gvu_pipe_controller.sv`, `gvu_top.sv`)**:
   - *RTL Change*: Added `i_num_tile` input and `tile_cnt` counter; calculated `vbank0_start_addr = tile_cnt * i_token_len`; swapped ST3 multiplier roles (`Mul#1 = MA_16X16` for recip * exp, `Mul#0 = MB_32X32_16X8` for requantization); increased `exp_e_shift_buf` depth to 4; connected `pipe_ctrl_softmax_st3_requant_valid = clamp_valid_out` to trigger `vbank0_wr`.
   - *Model Alignment*: Fully ported multi-tile logic, ST3 datapath swap, 4-stage shift buffer, and writeback trigger into `rtl_ref_gvu_top.h`.

---

### 9.2 Open items of the Softmax and LayerNorm model

| Item | Description |
|---|---|
| 1 | Write mask: the first RTL drop wrote mask bit i to bits [i, i+8). `ScratchpadParams::wmask_as_written = true` reproduces it; the default `false` follows the fixed RTL (section 1.2) |
| 2 | Softmax stage 2 drives sigma on the `vbank#1` port 0 write data at `softmax_token_last`, but the port select stays at 2 in `SOFTMAX_ST2`, so the VLSU enables port 2 and the sigma write does not reach the SRAM (read: `has/rtl_ref_gvu_top.h`, matches the RTL controller) |
| 3 | LayerNorm S, var and w are single scalars shared by the 32 lanes; the row sum written at `layernorm_token_last` is the register before the last beats are added (254 of 256 beats in the trace) |
| 4 | LayerNorm gamma and beta are one value per beat shared by all lanes (read from the sbank); the MAS reference has per-channel gamma, beta, M0_mul and TS_mul |

### 9.3 Findings in the RTL of the hardware drop that concern this unit

Each finding names its evidence. B1, B9 and B12 are errors in the RTL, B6 and B8 are parts that are missing, M1 and M3 are open questions; the proposal column is for the hardware team.

| Id | Finding | Evidence | Proposal |
|---|---|---|---|
| B1 | in `GEMM_FUSED_OBP` the controller raises `o_vbank2_rd` instead of `o_vbank2_wr` (`gvu_pipe_controller.sv:496`, `vlsu/gvu_vlsu.sv:376-377`): the GEMM_FUSED result is not written to `vbank#2` | measured: RTL as delivered, 20 of 20 lanes different from the golden of the hardware GEMM vector, `vbank#2` written 0 times; with the one-line change, 0 of 20 | change to `o_vbank2_wr`; add a golden compare to the GEMM case of `tb_gvu_top.sv` (B7) |
| B9 | `gemm_fused_last_read = i_oc_tile_last & i_oc_last` is not qualified with `i_psum_valid`; the end flag is that flag delayed by a fixed 9 beats | read (`gvu_pipe_controller.sv:250-266`) | qualify with `i_psum_valid` or count beats |
| B12 | the pooling loader keeps `first_load_cnt` between runs; a second max pool without a reset reads extra rows | measured: 4,221 of 12,960 values differ on the RTL, finishes in 2,181 cycles instead of 23,049 | clear the counter on `o_pooling_last_wr` or define a reset between runs |
| B6 | `POOLING_AVG`: state and adder / multiplier configuration exist, the data path to `vbank#2` takes the comparator output for both max and average pooling | read (`gvu_top.sv:1413`) | connect the average path |
| B8 | `shift_reg`, `fifo_v3`, `spill_register_flushable` not delivered; the model and the oracle use stand-ins written from the behaviour of `common_cells` | read | deliver the files |
| M1 | after the round-shift the RTL narrows by wrapping to 16 bits (`NARROW_WRAP`, `NW_16`) and adds the 16-bit zero point with wrap; the model saturates | read; YOLOv8m has 48,326,400 requantizations with no value outside 16 bits (counted) | confirm wrap as intended |
| M3 | sbank layout for GEMM_FUSED: scale (int32) in words 0 to 613, shift (int8) four per word from byte 2,456; 614 channels per run | read | confirm the layout |

The remaining findings (B2, F1, F2, F8) concern the PSM-to-GVU stream and are in `GVU_TOP_CORE_INTEGRATION.md`, sections 3, 4 and 7.


---

## 10. Specification and RTL Conformance Matrix

| Feature | MAS Requirement | `rtl_ref_gvu_top.h` Implementation | Status |
|---|---|---|---|
| **Residual Add FSM** | 2 stages without LUT init (§5.2.6, Fig. 5-7, 5-18) | `IDLE` → `RESIDUAL_ST1` → `RESIDUAL_ST2` → `IDLE` | **Conformant & Verified** |
| **Residual Stage 1 (Dequant A+B)** | Dual dequant + 32-bit add into `vbank#2` | Mul#0, Mul#1, RS#0, RS#1, Adder#1, Adder#2 | **Conformant & Verified** |
| **Residual Stage 2 (Requant Output)** | Requant sum into `vbank#0` & stream out | Mul#1 (`MA_32X32`), RS#1 (`NW_16`), Adder#2, Clamp | **Conformant & Verified** |
| **Residual Golden Match** | 100% bit-accurate functional verification | 2 tiles of 64 tokens / 4,096 elements verified against golden reference | **PASS (0 mismatches)** |
| **GEMM Fused FSM** | Table init then streaming ST1 (§5.2.1) | `LUT_ACT_INIT` → `GEMM_FUSED_ST1` → `IDLE` | **Conformant** |
| **GEMM Fused Requant** | Mul#1 (`MA_32X32`), RS#1 (`NW_16`), Adder#2 (`ADD_16`), Clamp | Bit-accurate lane pipeline matching MAS §5.2.1 | **Conformant & Verified** |
| **GEMM Fused Activation** | Direct `LUT_act` lookup (256 bytes) | Direct byte lookup `rdata8(LUT_ACT, y_clamp + 128)` | **Conformant & Verified** |
| **GEMM Fused Golden Match** | Functional accuracy | 2 tiles of 64 tokens / 4,096 elements verified against golden reference | **PASS (0 mismatches)** |
| **Softmax FSM** | Multi-tile support with 3 stages after LUT init (§5.2.3, Fig. 5-16) | `LUT_RECIP_INIT` → `LUT_EXP_INIT` → `SOFTMAX_ST1..ST3` (iterating over `i_num_tile`) → `IDLE` | **Conformant & Verified** |
| **Softmax ST3 Datapath** | recip × exp, RS#0, scale, RS#1, + zp, clamp | Mul#1 (`MA_16X16`) → RS#0 (72b) → Mul#0 (`MB_32X32_16X8`) → RS#1 (64b) → Adder#2 (`ADD_16`) → Clamp32 → `vbank0` | **Conformant (neo_new)** |
| **Softmax Multi-Tile** | Process `i_num_tile` consecutive tiles | `tile_cnt_` tracking, AGU start addr `tile_cnt_ * token_len` | **Conformant (neo_new)** |
| LayerNorm FSM | 5 stages after table init (§5.2.5, Fig. 5-17) | `LUT_RSQRT_INIT` → `LAYERNORM_ST1..ST5` → `IDLE` | **Conformant** (control flow) |
| LayerNorm rsqrt | $(r, E) = \text{LUT\_rsqrt}(var + \epsilon)$, 512 × 2 B | `LUT_RSQRT` in `vlut_`, all 512 entries loaded | **Conformant** |
| Scratchpad writes | Byte-masked writes | Byte-masked writes (`mem[addr][i*8 +: 8]`) | **Conformant (neo_new)** |
| **GEMM_FUSED stream mode** | per-beat requant and table, nine register cuts (MAS Requant and GEMM_FUSED drawings) | `i_gemm_stream`: depth 9, own address / mask / scale / shift per beat | **Verified against the model's epilogue** (51 / 51 runs, 3,392 + 29,808 beats of YOLOv8m); RTL: 11 beats, scale from the sbank, wrap: section 4.5 |
| **Residual last-row mask** | tensor length not a multiple of 32 | `i_res_last_lane_mask` | **Verified** (`tb_gvu_top_vs_model`) |
| **Residual add vs RTL** | not in the RTL drop | follows the MAS | **No RTL oracle** |
| **Max pool** | `POOLING_MAX`: 2x2 s1 / s2, 3x3 s1, 4x4 s1 / s2 / s4, 5x5 s1 | port of `gvu_pooling_feeder.sv`, `gvu_vcmp32.sv` per lane, `AGU_POOLING` | **Equal to the RTL cycle by cycle** on 10 sets (Verilator), golden of the hardware vectors on 9 sets; one run needs a reset (B12) |
| Average pool | `POOLING_AVG` | not in the unit | **Not implemented** (RTL data path not connected, B6) |
| GEMM_FUSED value path | RTL with the B1 change | equal to `has::requant` + table | **Equal on the hardware GEMM vector** (0 of 20 lanes different); wrap vs saturate (M1) not exercised |
