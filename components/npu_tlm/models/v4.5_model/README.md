# SAURIA NPU v4.2 — Unified SystemC Cycle-Accurate Model & Compiler Framework

A self-contained, header-only **SystemC cycle-accurate functional model** and **end-to-end ONNX compiler framework** for the **SAURIA NPU Core (v4.2 / FX1 SoC)**. 

The v4.2 model features a **dual-lane systolic architecture** (Lane A & Lane B with default $64 \times 64$ geometry), a **64-bit Rich Instruction Set Architecture (ISA)**, an **Output Post-Processing Block (OBP)** with fused epilogue stages and 16 KB activation LUT per lane, **dual independent Reconfigurable Engines (RCEA / RCEB)** with 24 KB Scratch SRAM each, an **internal 4-channel AXI DMA controller**, multi-datatype support (**INT8 / FP16 / INT16**), and an automated **ONNX Compiler Toolchain** capable of compiling complex deep learning models (**Vision Transformers, YOLOv8, CNNs**) into 100% bit-exact hardware execution testbenches.

> **Verification Status:** 
> - **`sample_vit_block.onnx`**: **4 / 4 PASS (100.0%)** (100% bit-exact, 0.0000 MAE, 1.000000 Cosine Sim)
> - **`yolov8m-int8.onnx`**: **261 / 261 PASS (100.0%)** (100% bit-exact across all 1,084 ONNX nodes & 23 layers, 0.0000 MAE, 1.000000 Cosine Sim)
> - **`vit_b-int8.onnx`**: **497 / 497 PASS (100.0%)** (100% bit-exact across all 2,297 ONNX nodes & 12 Transformer blocks, 0.0000 MAE, 1.000000 Cosine Sim)
> - **Pure-C Driver & Config Encoder**: Bit-exact round-trip validation across all target configurations.

---

## 1. Executive Summary & Architecture Overview

The SAURIA NPU v4.2 is a high-performance, modular Neural Processing Unit optimized for edge-AI vision, transformer, and object detection workloads.

```
                                  +-------------------------------------------------------+
                                  |                    HOST INTERFACE                     |
                                  |   (AXI-Lite MMIO CSRs: 0x40000400 | INST_LO / HI)     |
                                  +---------------------------+---------------------------+
                                                              |
                                           +------------------v------------------+
                                           |      SAURIA CONTROL & DECODER       |
                                           |  (Dual Instruction Queues A & B)    |
                                           +--------+-------------------+--------+
                                                    |                   |
                                   +----------------v---+       +-------v------------+
                                   |  LANE A SCHEDULER  |       |  LANE B SCHEDULER  |
                                   +--------+-----------+       +-------+------------+
                                            |                           |
                               +------------v------------+ +------------v------------+
                               |     DATA FEEDER A       | |     DATA FEEDER B       |
                               | (Act/Wei SRAM Regions)  | | (Act/Wei SRAM Regions)  |
                               +------------+------------+ +------------+------------+
                                            |                           |
                               +------------v------------+ +------------v------------+
                               |   SYSTOLIC ARRAY A      | |   SYSTOLIC ARRAY B      |
                               |    (64x64 PE Grid)      | |    (64x64 PE Grid)      |
                               +------------+------------+ +------------+------------+
                                            |                           |
                               +------------v------------+ +------------v------------+
                               |        OBP TOP A        | |        OBP TOP B        |
                               | (1 Pipe / 64 Channels)  | | (1 Pipe / 64 Channels)  |
                               +------------+------------+ +------------+------------+
                                            |                           |
                               +------------v------------+ +------------v------------+
                               |       RE / RCE A        | |       RE / RCE B        |
                               |  (24 KB ScratchA / LUT) | |  (24 KB ScratchB / LUT) |
                               +------------+------------+ +------------+------------+
                                            |                           |
                                            +------------+--------------+
                                                         |
                                  +----------------------v--------------------------------+
                                  |            AXI DMA ENGINE (4 Channels)                |
                                  |     (Multi-Gigabyte DRAM / DDR Interfacing)           |
                                  +-------------------------------------------------------+
```

### Key Hardware Highlights
- **Dual-Lane 64×64 Processing Engine**: Symmetric dual-lane execution (Lane A & Lane B with default $64 \times 64$ geometry) driven by `FX1_NSPLIT` (`0x00214`, default $32 = Y\_DIM / 2$) hardware barrier configuration.
- **Pure INT8 Integer Hardware Execution**: Pure signed integer multiplication, 32-bit partial sum accumulation, and $[-128, 127]$ saturation for `GEMM_FUSED` / `GEMM` operations in [`control/instruction_decoder.h`](file:///data/XPU00000/users/vuong.nguyen/project/sauria/RTL/src/v4.2_model/control/instruction_decoder.h).
- **64-bit Rich Instruction Set (ISA)**: Rich instruction format submitted via MMIO instruction ports (`FX1_QUEUE_A_PUSH` / `FX1_QUEUE_B_PUSH`) to control matrix execution, attention, normalization, and element-wise arithmetic.
- **Output Post-Processing Block (OBP)**: Exactly **1 OBP pipeline per lane**, processing **64 parallel channels (`Y_DIM = 64`)** through a 4-stage fused epilogue pipeline (Bias Addition, Requantization, 16 KB Non-linear Activation LUT, and Residual Skip Accumulation).
- **Dual Reconfigurable & Reduction Engines (RCEA / RCEB & REA / REB)**: Two independent instances with dedicated **24 KB Scratch SRAM** each (`ScratchA` and `ScratchB`). Computes non-linear functions (`LUT_exp`, `LUT_recip`, `LUT_rsqrt`) for two-pass Softmax (`FUSED_ATTN`) and LayerNorm (`LAYERNORM`), and shares the max-comparator tree for SPPF MaxPool (`ELEM_WISE MODE=MAX_POOL`) with 0 area overhead.
- **Internal 4-Channel AXI DMA Controller**: Autonomous DMA engine managing memory transfers between system DRAM and local SRAM buffers (`sauria_dma.h`), clamped to physical SRAM bank capacities.
- **Physical SRAM Banking (~2.1 MB On-Chip)**:
  - Bank 0 (Weight A): **320 KB**
  - Bank 1 (Weight B): **320 KB**
  - Bank 2 (IFMap A): **416 KB**
  - Bank 3 (IFMap B / Skip): **408 KB**
  - Bank 4 (PSums A & Scratch A): **512 KB**
  - Bank 5 (PSums B & Scratch B): **512 KB**
- **Automated ONNX Compiler Toolchain**: Python-based compiler (`tools/onnx_compiler.py`) with dynamic shape inference, zero-compute node aliasing, broadcasting support, and automated C++ SystemC testbench generation.

---

## 2. Requirements & Environment Setup

### System Prerequisites
- **Compiler**: C++17 compliant compiler (`g++` 9+; validated on `g++ 11.4`).
- **SystemC**: **SystemC 2.3.3** (or later). Point `SYSTEMC_HOME` to your installation directory.
- **Python**: Python 3.8+ with `onnx` and `numpy` packages installed for model compilation.

### Setting Up SystemC Environment
```bash
export SYSTEMC_HOME=/data/XPU00000/users/vuong.nguyen/project/sauria/systemc_install
export LD_LIBRARY_PATH=$SYSTEMC_HOME/lib:$LD_LIBRARY_PATH
```

---

## 3. Quick Start & Build System

The build system is managed via `Makefile`. Run `make help` to inspect all supported build targets and parameters.

```bash
# 1. Inspect Makefile targets and configuration knobs
make help

# 2. Build and run basic smoke test
make smoke

# 3. Build and execute standard hardware unit testbenches
make test_rich_isa
make test_lane_b_isolation
make test_dual_lane_fsm
make test_nsplit_barrier
make test_dual_instruction_queues

# 4. Build and execute the INT8 Vision Transformer Block benchmark
make test_vit_encoder_int8

# 5. Compile and run full ONNX models (100% Pass)
# YOLOv8m INT8 (261 Checkpoints PASS)
python3 tools/onnx_compiler.py --model ../yolov8m-int8.onnx --output tools/test_onnx_model.cpp
make test_onnx_model SYSTEMC_HOME=$SYSTEMC_HOME
./test_onnx_model

# ViT-Base INT8 (497 Checkpoints PASS)
python3 tools/onnx_compiler.py --model ../vit_b-int8.onnx --output tools/test_onnx_model.cpp
make test_onnx_model SYSTEMC_HOME=$SYSTEMC_HOME
./test_onnx_model
```

---

## 4. Hardware Architecture & Block Specifications

### 4.1. Dual-Lane Processing Engine (Lane A & Lane B)
The NPU core houses two symmetric execution lanes:
- **Lane A & Lane B Systolic Arrays**: Each lane features a $64 \times 64$ PE grid (`X_DIM = 64`, `Y_DIM = 64`).
- **Lane-Splitting Control (`FX1_NSPLIT`, `0x00214`)**: Readable and writable CSR controlling workload distribution. Default value is **32** ($Y\_DIM / 2$), assigning 32 rows to Lane A ($0 \dots 31$) and 32 rows to Lane B ($32 \dots 63$).
- **Dual Instruction Queues**: Queue A (`0x310`) and Queue B (`0x314`) allow host software to stream independent hardware instruction streams to each lane concurrently.

### 4.2. Output Post-Processing Block (OBP Epilogue Engine)
The OBP block ([`psm/obp_top.h`](file:///data/XPU00000/users/vuong.nguyen/project/sauria/RTL/src/v4.2_model/psm/obp_top.h)) implements 1 pipeline per lane with 64 parallel channels:
1. **Stage 1 (Bias Addition)**: Fuses 32-bit channel/matrix bias into array output using `bias_ram[64]`.
2. **Stage 2 (Requantization & Scaling)**: Performs fixed-point multiplier scaling and bit-shifting using `scale_ram[64]` and `shift_ram[64]`.
3. **Stage 3 (Non-linear Activation LUT)**:
   - **Activation Types**: `0` = None, `1` = ReLU, `2` = Sigmoid / SiLU, `3` = GELU.
   - **Activation LUT Regions**: 16 KB SRAM mapped at address `0x00140000` (`FX1_LUT_A_BASE`) and `0x00160000` (`FX1_LUT_B_BASE`).
   - **Indexing**: 64 parallel channels $\times$ 256 INT8 entries per lane, indexed via $\text{index} = \text{uint8}(v + 128) \in [0, 255]$.
4. **Stage 4 (Residual Accumulation)**: Adds identity residual skip connections directly before writing back to SRAM.

### 4.3. Reconfigurable & Reduction Engines (RCE / RE)
The RE/RCE subsystem ([`psm/re_rce.h`](file:///data/XPU00000/users/vuong.nguyen/project/sauria/RTL/src/v4.2_model/psm/re_rce.h)) provides non-linear vector acceleration:
- **`ReconfigurableEngine` (RCE)**: Houses non-linear lookup tables (`LUT_exp` 256 B, `LUT_recip` 512 B, `LUT_rsqrt` 2048 B).
- **`ReductionEngine` (RE)**: Houses 24 KB intermediate Scratch SRAM, 64-wide combinational adder and max-comparator trees.
- **Two-Pass Softmax**: Pass 1 computes row max ($\max(x)$) to prevent overflow; Pass 2 computes $\exp(x - \max)$, accumulates sum, and multiplies by reciprocal.
- **Two-Pass LayerNorm**: Pass 1 computes mean $\mu = \frac{\sum x}{N}$; Pass 2 computes variance $\sigma^2 = \frac{\sum (x-\mu)^2}{N}$, evaluates $\text{rsqrt}(\sigma^2+\epsilon)$, and applies $\gamma / \beta$ scaling.
- **SPPF MaxPool**: Reuses the Softmax Pass 1 max-comparator tree for 0 area overhead.

### 4.4. Register Map & Control CSRs

All control and configuration registers are mapped under MMIO ([`config_regs.h`](file:///data/XPU00000/users/vuong.nguyen/project/sauria/RTL/src/v4.2_model/config_regs.h), [`config_map.h`](file:///data/XPU00000/users/vuong.nguyen/project/sauria/RTL/src/v4.2_model/config_map.h)):

| CSR Name | MMIO Offset | Absolute Address | Access | Description |
| :--- | :--- | :--- | :--- | :--- |
| `FX1_INST_LO` | `0x00300` | `0x40000300` | WO | Low 32 bits of 64-bit Rich Instruction |
| `FX1_INST_HI` | `0x00304` | `0x40000304` | WO | High 32 bits of 64-bit Rich Instruction (Triggers Decode) |
| `FX1_QUEUE_A_PUSH` | `0x00310` | `0x40000310` | WO | Push instruction to Queue A (Lane A) |
| `FX1_QUEUE_B_PUSH` | `0x00314` | `0x40000314` | WO | Push instruction to Queue B (Lane B) |
| `FX1_NSPLIT` | `0x00214` | `0x40000214` | R/W | Lane-splitting barrier count register (Default: 32) |
| `FX1_OUT_OBP_CFG_A` | `0x00130` | `0x40000130` | R/W | Epilogue Config Lane A (Bits: 0=bias, 1=requant, 4..6=act, 7=residual) |
| `FX1_OUT_OBP_CFG_B` | `0x00134` | `0x40000134` | R/W | Epilogue Config Lane B (Bits: 0=bias, 1=requant, 4..6=act, 7=residual) |
| `FX1_RICH_HEADS_DIM_MODE` | `0x00454` | `0x40000454` | R/W | Attention configuration (Bits 0..15=heads, 16..23=dim, 24..31=mode) |
| `FX1_LUT_A_BASE` | `0x00140000` | `0x40140000` | R/W | Lane A OBP Activation LUT RAM Region (16 KB) |
| `FX1_LUT_B_BASE` | `0x00160000` | `0x40160000` | R/W | Lane B OBP Activation LUT RAM Region (16 KB) |
| `FX1_RCE_A_EXP_BASE` | `0x00200000` | `0x40200000` | R/W | Lane A RCE EXP LUT (256 B) |
| `FX1_RCE_A_RECIP_BASE` | `0x00210000` | `0x40210000` | R/W | Lane A RCE Reciprocal LUT (512 B) |
| `FX1_RCE_A_RSQRT_BASE` | `0x00220000` | `0x40220000` | R/W | Lane A RCE RSQRT LUT (2048 B) |
| `FX1_RCE_B_EXP_BASE` | `0x00230000` | `0x40230000` | R/W | Lane B RCE EXP LUT (256 B) |
| `FX1_RCE_B_RECIP_BASE` | `0x00240000` | `0x40240000` | R/W | Lane B RCE Reciprocal LUT (512 B) |
| `FX1_RCE_B_RSQRT_BASE` | `0x00250000` | `0x40250000` | R/W | Lane B RCE RSQRT LUT (2048 B) |
| `o_done` (Pin) | N/A | Top Pin | Out | Top-level interrupt line asserting HIGH on execution completion |

---

## 5. 64-bit Rich Instruction Set Architecture (ISA)

Instructions are submitted as 64-bit words formatted via `INST_LO` and `INST_HI` ([`control/instruction_decoder.h`](file:///data/XPU00000/users/vuong.nguyen/project/sauria/RTL/src/v4.2_model/control/instruction_decoder.h)):

```
Bit Fields:
+-------------------+-------------------+-------------------+-------------------+
|   w_addr [63:32]  |  in_addr [31:16]  |    flags [15:8]   |   opcode [7:0]    |
+-------------------+-------------------+-------------------+-------------------+
```

### Supported Opcodes
- **`0x05` (`SET_NSPLIT`)**: Reconfigures the active spatial lane boundary and synchronizes lanes.
- **`0x12` (`GEMM_FUSED`)**: Pure signed INT8 Matrix multiplication and Conv2D execution with fused epilogue (bias, scale, activation, residual).
- **`0x13` (`FUSED_ATTN`)**: Fused Multi-Head Self-Attention / Softmax block execution for Transformer models.
- **`0x14` (`LAYERNORM`)**: Fused Layer Normalization computing mean, variance, scale, and bias.
- **`0x15` (`ELEM_WISE`)**: Vectorized arithmetic supporting 5 operational modes:
  - Mode `0`: `Add`
  - Mode `1`: `MaxPool`
  - Mode `2`: `Mul`
  - Mode `3`: `Sub`
  - Mode `4`: `Div`
  - *Broadcasting*: Supports automatic 1D scalar/vector broadcast-tiling (`broadcast_1d`).

---

## 6. ONNX Compiler Toolchain (`tools/onnx_compiler.py`)

The automated compiler lowers standard ONNX models directly into executable C++ SystemC testbenches:

```
                                +---------------------------+
                                |      INT8 ONNX MODEL      |
                                | (YOLOv8 / ViT / CNN Graph)|
                                +-------------+-------------+
                                              |
                                              v
                                +---------------------------+
                                |    tools/onnx_compiler.py |
                                +-------------+-------------+
                                              |
                     +------------------------+------------------------+
                     |                        |                        |
                     v                        v                        v
        +-------------------------+ +-------------------+ +-------------------------+
        |   tools/dram_init.bin   | | tools/golden_ref.bin| |  tools/test_onnx_model.cpp|
        |  (Packed DRAM Payload)  | |(Golden Checkpoints)| |  (SystemC Testbench)   |
        +-------------------------+ +-------------------+ +-------------------------+
```

### Key Compiler Features
1. **Dynamic Shape Inference**: Derives exact dynamic matrix shapes from ONNX metadata without relying on hardcoded tensor dimensions.
2. **Zero-Compute Memory Aliasing**: Automatically aliases DRAM addresses for zero-compute nodes (`Reshape`, `Transpose`, `Flatten`, `Squeeze`, `Unsqueeze`, `Identity`, `QuantizeLinear`, `DequantizeLinear`, `Concat`, `Split`, `Slice`), preventing redundant memory operations.
3. **Broadcast Expansion (`broadcast_1d`)**: Intelligently handles multi-dimensional scalar and 1D vector tensor broadcasting for elementwise operations without corrupting DRAM weight initializers.
4. **64-bit Address Space Testbench Allocation**: Emits SystemC testbenches compiled with 64-bit integer DRAM buffer sizing (`static_cast<size_t>(dram_mb) * 1024ULL * 1024ULL`), supporting models requiring $> 2$ GB DRAM allocations.

---

## 7. Multi-Datatype Build Variants (INT8 / FP16 / INT16)

The element datatype is a hardware build property configured via macro flags during compilation:

| Datatype | Input / Output Width | Accumulator Width | Precision / Tolerance | Compilation Flags |
| :--- | :--- | :--- | :--- | :--- |
| **INT8** *(Default)* | 8-bit / 8-bit | 32-bit INT | Exact (0 mismatch) | *(Default build flags)* |
| **FP16** | 16-bit / 16-bit | 32-bit Float | $\le 16$ ULP tolerance | `-DNPU_FP16 -DNPU_DTYPE_IN=fp16_t -DNPU_DTYPE_OUT=fp16_t -DNPU_DTYPE_PSUM=float` |
| **INT16** | 16-bit / 16-bit | 64-bit INT | Exact (0 mismatch) | `-DNPU_INT16 -DNPU_DTYPE_IN=int16_t -DNPU_DTYPE_OUT=int64_t -DNPU_DTYPE_PSUM=int64_t` |

---

## 8. Directory & Source Code Layout

```
v4.2_model/
├── README.md                           # Core documentation & user guide
├── Makefile                            # SystemC build & verification Makefile
├── npu_top.h                           # Top-level SystemC NPU Core module wrapper (64x64)
├── config_regs.h                       # Profile-aware CSR register implementation
├── config_map.h                        # Register address offset maps & bit fields
├── sauria_types.h                      # Shared types, structs, and DRAM memory layout
├── fp16.h                              # C++17 IEEE-754 half-precision float implementation
│
├── control/                            # Control & Scheduling Subsystem
│   ├── instruction_decoder.h           # 64-bit Rich ISA instruction decoder & queues (INT8)
│   ├── sauria_dma.h                    # Internal 4-channel AXI DMA controller
│   └── main_controller.h              # State machine & loop scheduling controller
│
├── psm/                                # Post-Processing & Reduction Subsystem
│   ├── obp_top.h                       # Output Post-Processing Block (4-stage epilogue & 16 KB LUT)
│   ├── re_rce.h                        # Dual Reduction Engine & Reconfigurable Compute Engine (24 KB)
│   └── psm_top.h                       # Partial Sum Scanner & Drain Unit
│
├── systolic_array/                     # PE Grid & Datapath Execution Engine (64x64)
├── data_feeder/                        # SRAM Data Feeder & Tile Buffering (IFMap & Weight)
├── sram/                               # On-chip SRAM Banks (Banks 0–5: ~2.1 MB)
├── instrumentation/                    # Hardware Performance Counter Subsystem (60 metrics)
│
├── tools/                              # Compiler Tools & Unit Testbenches
│   ├── onnx_compiler.py                # Automated ONNX compiler & testbench emitter
│   ├── test_rich_isa.cpp               # 64-bit Rich ISA unit testbench
│   ├── test_lane_b_isolation.cpp       # Lane B power isolation testbench
│   ├── test_dual_lane_fsm.cpp          # Dual-lane state machine testbench
│   ├── test_nsplit_barrier.cpp         # NSPLIT barrier synchronization testbench
│   ├── test_dual_instruction_queues.cpp# Dual instruction queue testbench
│   ├── test_layernorm_lane_ab.cpp      # Dual-lane LayerNorm testbench
│   └── test_vit_encoder_int8.cpp       # Vision Transformer INT8 block benchmark
```

---

## 9. Verification & Benchmark Summary

| Model / Benchmark | Total Nodes / Layers | Hardware Instructions | Execution Status | Accuracy Metric | Checkpoints PASS |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **`yolov8m-int8.onnx`** | **1,084 nodes / 23 layers** | 261 | **PASS** | `0.0000` MAE | **261 / 261 (100.0%)** |
| **`vit_b-int8.onnx`** | **2,297 nodes / 15 modules**| 497 | **PASS** | `0.0000` MAE | **497 / 497 (100.0%)** |
| **`sample_vit_block.onnx`** | 4 nodes | 4 | **PASS** | `0.0000` MAE | **4 / 4 (100.0%)** |
| **`test_rich_isa`** | Multi-op | Multi-op | **PASS** | 0 mismatch | **Bit-exact** |
| **`test_dual_instruction_queues`** | Multi-queue | 5 | **PASS** | 0 mismatch | **Bit-exact** |
| **`test_layernorm_lane_ab`** | Dual-lane LN | 2 | **PASS** | 0 mismatch | **Bit-exact** |
| **`test_nsplit_barrier`** | Dual-lane | 2 | **PASS** | 0 mismatch | **Bit-exact** |

---

## 10. License & Maintenance

Developed by the **SAURIA NPU Architecture Team**. For technical reports and architecture specifications, please reference [`report_1908.md`](file:///data/XPU00000/users/vuong.nguyen/project/sauria/RTL/src/v4.2_model/report_1908.md), [`1908.md`](file:///data/XPU00000/users/vuong.nguyen/project/sauria/RTL/src/v4.2_model/1908.md), and [`dual_lane_architecture_report.md`](file:///data/XPU00000/users/vuong.nguyen/project/sauria/RTL/src/v4.2_model/dual_lane_architecture_report.md).
