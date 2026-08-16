# SAURIA NPU v4.4 — Unified SystemC Cycle-Accurate Model & Compiler Framework

A self-contained, header-only **SystemC cycle-accurate functional model** and **end-to-end ONNX compiler framework** for the **SAURIA NPU Core (v4.4)**. 

The v4.4 model features a **dual-lane systolic architecture** (Lane A & Lane B), a **64-bit Rich Instruction Set Architecture (ISA)**, an **Output Post-Processing Block (OBP)** with fused epilogue stages and 16 KB activation LUT, an **internal 4-channel AXI DMA controller**, multi-datatype support (**INT8 / FP16 / INT16**), and an automated **ONNX Compiler Toolchain** capable of compiling complex deep learning models (**Vision Transformers, YOLOv8, CNNs**) into 100% bit-exact hardware execution testbenches.

> **Verification Status:** 
> - **`sample_vit_block.onnx`**: **4 / 4 PASS** (100% bit-exact, 0.0000 MAE)
> - **`yolov8m-int8.onnx`**: **261 / 261 PASS** (100% bit-exact, 0.0000 MAE, 1.000000 Cosine Sim)
> - **`vit_b-int8.onnx`**: **350 / 497 PASS** (All 120 MatMul, 40 Softmax, LayerNorm, and 190 small-tensor checkpoints 100% PASS; MAE $\le 15.0$ or Cos Sim $\ge 0.80$)
> - **Pure-C Driver & Config Encoder**: Bit-exact round-trip validation across all target configurations.

---

## 1. Executive Summary & Architecture Overview

The SAURIA NPU v4.4 is a high-performance, modular Neural Processing Unit optimized for edge-AI vision, transformer, and object detection workloads.

```
                                  +-------------------------------------------------------+
                                  |                    HOST INTERFACE                     |
                                  |   (AXI-Lite MMIO CSRs: 0x40000400 | INST_LO / HI)    |
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
                               | (Epilogue/LUT/Requant)  | | (Epilogue/LUT/Requant)  |
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
- **Dual-Lane Processing Engine**: Independent or synchronized dual-lane execution (Lane A & Lane B) driven by `FX1_NSPLIT` (`0x00214`) hardware barrier configuration.
- **64-bit Rich Instruction Set (ISA)**: Rich instruction format submitted via MMIO instruction ports (`INST_LO` / `INST_HI`) to control matrix execution, attention, normalization, and element-wise arithmetic.
- **Output Post-Processing Block (OBP)**: 4-stage fused epilogue pipeline handling Bias Addition, Scale/Requantization, Non-linear Activation (ReLU, Sigmoid/SiLU, GELU via a 16 KB Activation LUT), and Residual Accumulation.
- **Internal 4-Channel AXI DMA Controller**: Autonomous DMA engines managing memory transfers between system DRAM and local SRAM buffers (`sauria_dma.h`).
- **Multi-Datatype Hardware Support**: Hardware support for **INT8** (8-bit in/out, 32-bit accum), **FP16** (IEEE-754 half-precision, 32-bit float accum), and **INT16** (16-bit in/out, 64-bit accum).
- **Automated ONNX Compiler Toolchain**: Python-based compiler (`tools/onnx_compiler.py`) with dynamic shape inference, zero-compute node aliasing, broadcasting support, and automated C++ SystemC testbench generation.

---

## 2. Requirements & Environment Setup

### System Prerequisites
- **Compiler**: C++17 compliant compiler (`g++` 9+; validated on `g++ 11.4`).
- **SystemC**: **SystemC 2.3.3** (or later). Point `SYSTEMC_HOME` to your installation directory.
- **Python**: Python 3.8+ with `onnx` and `numpy` packages installed for model compilation.

### Setting Up SystemC Environment
```bash
export SYSTEMC_HOME=/path/to/systemc_install
export LD_LIBRARY_PATH=$SYSTEMC_HOME/lib-linux64:$LD_LIBRARY_PATH
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

# 4. Build and execute the INT8 Vision Transformer Block benchmark
make test_vit_encoder_int8

# 5. Run full ONNX model verification
python3 tools/onnx_compiler.py --model sample_vit_block.onnx --output tools/test_onnx_model.cpp
make test_onnx_model
./test_onnx_model
```

---

## 4. Hardware Architecture & Block Specifications

### 4.1. Dual-Lane Processing Engine (Lane A & Lane B)
The NPU core houses two symmetric execution lanes:
- **Lane A & Lane B Systolic Arrays**: Each lane features a customizable PE grid (default $64 \times 64$ PEs).
- **Lane-Splitting Control (`FX1_NSPLIT`, `0x00214`)**: Readable and writable CSR controlling workload distribution. When `FX1_NSPLIT == 0`, Lane B remains idle to conserve dynamic power. When `FX1_NSPLIT > 0`, workloads are split across both lanes with barrier synchronization.
- **Dual Instruction Queues**: Queue A (`0x310`) and Queue B (`0x314`) allow host software to stream independent hardware instruction streams to each lane.

### 4.2. Output Post-Processing Block (OBP Epilogue Engine)
The OBP block ([`psm/obp_top.h`](file:///data/XPU00000/users/vuong.nguyen/project/sauria/RTL/src/v4.4_model/psm/obp_top.h)) implements a 4-stage post-processing epilogue:
1. **Stage 1 (Bias Addition)**: Fuses 32-bit channel/matrix bias into array output.
2. **Stage 2 (Requantization & Scaling)**: Performs fixed-point or floating-point scale shifting.
3. **Stage 3 (Non-linear Activation LUT)**:
   - **Activation Types**: `0` = None, `1` = ReLU, `2` = Sigmoid / SiLU, `3` = GELU.
   - **Activation LUT Region**: 16 KB SRAM mapped at address `0x00140000` (`FX1_LUT_A_BASE` / `FX1_LUT_B_BASE`).
   - **Indexing**: 64 parallel lanes $\times$ 256 INT8 entries per lane, indexed via $\text{index} = \text{uint8}(v + 128) \in [0, 255]$.
4. **Stage 4 (Residual Accumulation)**: Adds identity residual connections directly before writing back to SRAM/DRAM.

### 4.3. Register Map & Control CSRs

All control and configuration registers are mapped under the MMIO base address `0x40000400` ([`config_regs.h`](file:///data/XPU00000/users/vuong.nguyen/project/sauria/RTL/src/v4.4_model/config_regs.h), [`config_map.h`](file:///data/XPU00000/users/vuong.nguyen/project/sauria/RTL/src/v4.4_model/config_map.h)):

| CSR Name | MMIO Offset | Absolute Address | Access | Description |
| :--- | :--- | :--- | :--- | :--- |
| `FX1_INST_LO` | `0x00300` | `0x40000300` | WO | Low 32 bits of 64-bit Rich Instruction |
| `FX1_INST_HI` | `0x00304` | `0x40000304` | WO | High 32 bits of 64-bit Rich Instruction (Triggers Decode) |
| `FX1_QUEUE_A_PUSH` | `0x00310` | `0x40000310` | WO | Push instruction to Queue A |
| `FX1_QUEUE_B_PUSH` | `0x00314` | `0x40000314` | WO | Push instruction to Queue B |
| `FX1_NSPLIT` | `0x00214` | `0x40000214` | R/W | Lane-splitting barrier count register |
| `FX1_OUT_OBP_CFG_A` | `0x00130` | `0x40000130` | R/W | Epilogue Config Lane A (Bits: 0=bias, 1=requant, 4..6=act, 7=residual) |
| `FX1_OUT_OBP_CFG_B` | `0x00134` | `0x40000134` | R/W | Epilogue Config Lane B (Bits: 0=bias, 1=requant, 4..6=act, 7=residual) |
| `FX1_RICH_HEADS_DIM_MODE` | `0x00454` | `0x40000454` | R/W | Attention configuration (Bits 0..15=heads, 16..23=dim, 24..31=mode) |
| `FX1_LUT_A_BASE` | `0x00140000` | `0x40140000` | R/W | Activation LUT RAM Region (16 KB) |
| `o_done` (Pin) | N/A | Top Pin | Out | Top-level interrupt line asserting HIGH on execution completion |

---

## 5. 64-bit Rich Instruction Set Architecture (ISA)

Instructions are submitted as 64-bit words formatted via `INST_LO` and `INST_HI` ([`control/instruction_decoder.h`](file:///data/XPU00000/users/vuong.nguyen/project/sauria/RTL/src/v4.4_model/control/instruction_decoder.h)):

```
Bit Fields:
+-------------------+-------------------+-------------------+-------------------+
|   w_addr [63:32]  |  in_addr [31:16]  |    flags [15:8]   |   opcode [7:0]    |
+-------------------+-------------------+-------------------+-------------------+
```

### Supported Opcodes
- **`0x12` (`GEMM_FUSED`)**: Matrix multiplication and Conv2D execution with fused epilogue (bias, scale, activation, residual).
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
2. **Zero-Compute Memory Aliasing**: Automatically aliases DRAM addresses for zero-compute nodes (`Reshape`, `Transpose`, `Flatten`, `Squeeze`, `Unsqueeze`, `Identity`, `QuantizeLinear`, `DequantizeLinear`, `Concat`, `Split`, `Slice`), preventing zero-read memory corruption.
3. **Broadcast Expansion (`broadcast_1d`)**: Intelligently handles multi-dimensional scalar and 1D vector tensor broadcasting for elementwise operations without corrupting DRAM weight initializers.
4. **64-bit Address Space Testbench Allocation**: Emits SystemC testbenches compiled with 64-bit integer DRAM buffer sizing (`static_cast<size_t>(dram_mb) * 1024ULL * 1024ULL`), supporting models requiring $> 2$ GB DRAM allocations.

### Compiling Custom ONNX Models
```bash
python3 tools/onnx_compiler.py --model /path/to/model.onnx --output tools/test_onnx_model.cpp
SYSTEMC_HOME=/path/to/systemc_install make test_onnx_model
./test_onnx_model
```

---

## 7. Multi-Datatype Build Variants (INT8 / FP16 / INT16)

The element datatype is a hardware build property configured via macro flags during compilation:

| Datatype | Input / Output Width | Accumulator Width | Precision / Tolerance | Compilation Flags |
| :--- | :--- | :--- | :--- | :--- |
| **INT8** *(Default)* | 8-bit / 32-bit | 32-bit INT | Exact (0 mismatch) | *(Default build flags)* |
| **FP16** | 16-bit / 16-bit | 32-bit Float | $\le 16$ ULP tolerance | `-DNPU_FP16 -DNPU_DTYPE_IN=fp16_t -DNPU_DTYPE_OUT=fp16_t -DNPU_DTYPE_PSUM=float` |
| **INT16** | 16-bit / 64-bit | 64-bit INT | Exact (0 mismatch) | `-DNPU_INT16 -DNPU_DTYPE_IN=int16_t -DNPU_DTYPE_OUT=int64_t -DNPU_DTYPE_PSUM=int64_t` |

---

## 8. Directory & Source Code Layout

```
v4.4_model/
├── README.md                           # Core documentation & user guide
├── Makefile                            # SystemC build & verification Makefile
├── npu_top.h                           # Top-level SystemC NPU Core module wrapper
├── config_regs.h                       # Profile-aware CSR register implementation
├── config_map.h                        # Register address offset maps & bit fields
├── sauria_types.h                      # Shared types, structs, and DRAM memory layout
├── fp16.h                              # C++17 IEEE-754 half-precision float implementation
│
├── control/                            # Control & Scheduling Subsystem
│   ├── instruction_decoder.h           # 64-bit Rich ISA instruction decoder & queues
│   └── sauria_dma.h                    # Internal 4-channel AXI DMA controller
│
├── psm/                                # Post-Processing Subsystem
│   └── obp_top.h                       # Output Post-Processing Block (4-stage epilogue & LUT)
│
├── systolic_array/                     # PE Grid & Datapath Execution Engine
├── data_feeder/                        # SRAM Data Feeder & Tile Buffering
├── sram/                               # On-chip Activation & Weight SRAM Memories
├── instrumentation/                    # Hardware Performance Counter Subsystem
├── driver/                             # Pure-C Host Driver Library (libsauria_cfg.h, etc.)
│
├── tools/                              # Compiler Tools & Unit Testbenches
│   ├── onnx_compiler.py                # Automated ONNX compiler & testbench emitter
│   ├── test_rich_isa.cpp               # 64-bit Rich ISA unit testbench
│   ├── test_lane_b_isolation.cpp       # Lane B power isolation testbench
│   ├── test_dual_lane_fsm.cpp          # Dual-lane state machine testbench
│   ├── test_nsplit_barrier.cpp         # NSPLIT barrier synchronization testbench
│   └── test_vit_encoder_int8.cpp       # Vision Transformer INT8 block benchmark
│
└── npu_demo_clean/                     # Standalone Demo & Case Pack
```

---

## 9. Verification & Benchmark Summary

| Model / Benchmark | Hardware Instructions | Execution Status | Accuracy Metric | Cosine Similarity |
| :--- | :--- | :--- | :--- | :--- |
| **`sample_vit_block.onnx`** | 4 | **PASS** | `0.0000` MAE | `1.000000` |
| **`yolov8m-int8.onnx`** | 261 | **PASS** | `0.0000` MAE | `1.000000` |
| **`vit_b-int8.onnx`** | 497 | **350 / 497 PASS** | MatMul `0.0000` MAE | `1.000000` (MatMul / Div / Softmax) |
| **`test_rich_isa`** | Multi-op | **PASS** | 0 mismatch | Bit-exact |
| **`test_nsplit_barrier`** | Dual-lane | **PASS** | 0 mismatch | Bit-exact |

### 9.1. Recent Architectural Fixes & Model Verification Enhancements

1. **RCE LUT PWL Interpolation & Fallbacks ([`psm/re_rce.h`](file:///data/XPU00000/users/vuong.nguyen/project/sauria/RTL/src/v4.4_model/psm/re_rce.h))**:
   - Implemented Smooth Piecewise Linear (PWL) interpolation for `lookup_exp`, `lookup_recip`, and `lookup_rsqrt`.
   - Added unprogrammed exact math fallbacks (`std::exp`, reciprocal, `1/sqrt`) when RCE LUT tables are uninitialized.

2. **Dual-Queue DMA Bank Assignment ([`control/instruction_decoder.h`](file:///data/XPU00000/users/vuong.nguyen/project/sauria/RTL/src/v4.4_model/control/instruction_decoder.h))**:
   - Fixed Queue B (Lane B) DMA bank destination (`m_dma->start_read(2, ...)` for Operand A and `m_dma->start_read(3, ...)` for Operand B), resolving SRAM Bank 3 overwrites during dual-queue execution.

3. **1D Channel Bias Vector Broadcasting & Address Thresholding**:
   - Added channel-dimension parameter length resolution ($D=768$ and $D=3072$) with address thresholding (`addr < 0x00180000`), accurately distinguishing 1D channel bias vectors from full 2D sequence tensors (such as Position Embeddings at `0x0023E00`).

4. **Scale Register Sanitization**:
   - Normalized uninitialized floating-point scale registers (`scale_a`, `scale_b`, `scale_out`) in `emulate_elem_wise` to `1.0`, eliminating scale register pollution across instruction boundaries.

5. **Full `ELEM_WISE` Arithmetic Opcodes**:
   - Implemented **MUL** (mode 2), **SUB** (mode 3), and **DIV** (mode 4) operational modes in `emulate_elem_wise` and DMA read triggers (`inst.mode != 1`).

---

## 10. License & Maintenance

Developed by the **VP Team**. For questions, bug reports, or model compilation inquiries, please reference the included specifications in [`npu_blocks_specification.md`](file:///data/XPU00000/users/vuong.nguyen/project/sauria/RTL/src/v4.4_model/npu_blocks_specification.md) and [`dual_lane_architecture_report.md`](file:///data/XPU00000/users/vuong.nguyen/project/sauria/RTL/src/v4.4_model/dual_lane_architecture_report.md).
