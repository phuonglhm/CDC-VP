# Sauria NPU v4.2 Verification & Layer Coverage Report (19/08/2026)

## 1. Executive Summary & Benchmark Results

The simulation failures on **YOLOv8m INT8** and **ViT-Base INT8** have been diagnosed and resolved. Both models now achieve **100.0% PASS** on all automated golden accuracy checkpoints across the full end-to-end network architectures.

### Automated Simulation & Verification Summary

```
====================================================================================================
                               SAURIA NPU ONNX BENCHMARK SUMMARY
====================================================================================================
  Model                      Total Checkpoints     Passed Checkpoints     Failed Checkpoints     Pass Rate
----------------------------------------------------------------------------------------------------
  YOLOv8m INT8 (261 Ops)     261                   261                    0                      100.0% [PASS]
  ViT-Base INT8 (497 Ops)    497                   497                    0                      100.0% [PASS]
====================================================================================================
```

| Model Name | Total ONNX Graph Nodes | Lowered HW Instructions | Verified Checkpoints | Passed | Failed | Accuracy Pass Rate | Status |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **YOLOv8m INT8** (`yolov8m-int8.onnx`) | **1,084 nodes** | **261 instructions** | **261 checkpoints** | **261** | **0** | **100.0%** | **PASS** |
| **ViT-Base INT8** (`vit_b-int8.onnx`) | **2,297 nodes** | **497 instructions** | **497 checkpoints** | **497** | **0** | **100.0%** | **PASS** |

---

## 2. Root Cause Analysis

### A. Logic Address Bounding & Address Threshold Collision
- **Defect**: In [`control/instruction_decoder.h`](file:///data/XPU00000/users/vuong.nguyen/project/sauria/RTL/src/v4.2_model/control/instruction_decoder.h), the `get_p_len` lambda heuristically returned `768` whenever operand address was below `0x00180000` (`if (addr >= 0x00180000) return tlen; return 768;`).
- **Consequences**:
  - **ViT-Base (147 Fails)**: Any full sequence tensor (`length = 409,600` bytes) residing in lower DRAM ($< 1.5\text{ MB}$) had its DMA transfer clamped to 768 bytes, and compute only operated on `i % 768`, resulting in 147 failing checkpoints.
  - **YOLOv8m (`Div_1` checkpoint)**: The scalar divisor $2.0$ at `0x0014F00` ($< 0x00180000$) was read as 768 elements instead of 1 element.

### B. Multi-tiling & Operand Length / Broadcasting Misalignment
- **Defect 1 (Hardware Execution)**: `InstructionDecoder` did not accept explicit `a_len` and `b_len` configuration registers, leaving 1D broadcast strides (e.g. YOLOv8 `Mul_2` with $L_A=33,600$ and $L_B=8,400$) unable to perform 4x broadcast wrapping.
- **Defect 2 (Golden Reference Slicing)**: In [`tools/onnx_compiler.py`](file:///data/XPU00000/users/vuong.nguyen/project/sauria/RTL/src/v4.2_model/tools/onnx_compiler.py), `np.frombuffer(dram_bytes[b_addr:b_addr+length])` sliced the full output `length` rather than the operand's true `b_len`. For scalar constants ($L_B=1$) or stride tensors ($L_B=8,400$), this read adjacent uninitialized DRAM memory rather than properly broadcasting the true tensor.
- **Defect 3 (DMA SRAM Bank Capacity Bounding)**: Unclamped GEMM IFMAP/weight transfer sizes could exceed hardware bank capacities (e.g., Bank 2 IFMap capacity is 416 KB), which led to multi-megabyte burst read loops in simulation.

---

## 3. Implementation Changes

### 3.1 C++ Hardware Model (`control/instruction_decoder.h`)
- Added `a_len` and `b_len` fields to `SauriaRichInstruction`.
- Added host MMIO configuration registers `0x40000464` (`r_a_len`) and `0x40000468` (`r_b_len`).
- Removed the flawed `get_p_len` address heuristic in DMA read setup for Queue A & B.
- Updated `emulate_elem_wise` to use explicit `a_len` and `b_len` modulo indexing for exact broadcasting of scalars ($L=1$), strides ($L=8400$), 1D biases ($L=768$), and full tensors ($L=409600$).
- Clamped DMA read/write transfers to physical SRAM bank capacities (Bank 0/1: 320 KB, Bank 2: 416 KB, Bank 3: 408 KB, Bank 4/5: 512 KB).

### 3.2 ONNX Compiler & Generator (`tools/onnx_compiler.py`)
- Updated `lower_elemwise` to track `a_len` and `b_len` for each element-wise operation.
- Updated C++ testbench generator for Opcode `0x15` to emit `wr(0x40000464, a_len)` and `wr(0x40000468, b_len)`.
- Fixed the sequential golden reference pass for `ELEM_WISE` to slice `dram_bytes` using `a_len` and `b_len` before calling `broadcast_1d`.
- Adjusted `wait_cycles` in the generated testbench to ensure all queued instructions complete execution before verification.

---

## 4. Full Architectural Layer Coverage Proof

A comprehensive analysis of both ONNX computational graphs was performed to verify that tests execute all network layers end-to-end across the full model pipelines.

### 4.1 YOLOv8m Layer-by-Layer Coverage (`model.0` to `model.22`)

The test executes the entire YOLOv8m object detection architecture from input image (`images`) to final bounding box and class predictions (`predictions`):

| Network Submodule | Layer Range | Internal Operators Executed | HW Instructions Lowered | Checkpoint Status |
| :--- | :--- | :--- | :--- | :--- |
| **Input Stem** | `model.0` | Conv2d (3→48, k=3, s=2), Sigmoid, Mul (SiLU activation) | 3 (Inst 1–3) | **100% PASS** |
| **Backbone Stage 1** | `model.1`, `model.2` | Conv2d (s=2), C2f (Bottlenecks, Residual Adds, Concat) | 22 (Inst 4–25) | **100% PASS** |
| **Backbone Stage 2** | `model.3`, `model.4` | Conv2d (s=2), C2f (Bottlenecks, Residual Adds, Concat) | 38 (Inst 26–63) | **100% PASS** |
| **Backbone Stage 3** | `model.5`, `model.6` | Conv2d (s=2), C2f (Bottlenecks, Residual Adds, Concat) | 36 (Inst 64–99) | **100% PASS** |
| **Backbone Stage 4** | `model.7`, `model.8` | Conv2d (s=2), C2f (Bottlenecks, Residual Adds, Concat) | 20 (Inst 100–119) | **100% PASS** |
| **Backbone SPPF** | `model.9` | Conv2d, 3x MaxPool2d (k=5, s=1), Concat, Conv2d | 8 (Inst 120–127) | **100% PASS** |
| **Neck FPN Top-Down** | `model.10`–`model.15` | Resize (2x), Concat, C2f, Conv2d, C2f | 44 (Inst 128–171) | **100% PASS** |
| **Neck PAN Bottom-Up**| `model.16`–`model.21` | Conv2d (s=2), Concat, C2f, Conv2d, Concat, C2f | 44 (Inst 172–215) | **100% PASS** |
| **Detect Head** | `model.22` | BBox Conv (`cv2`), Cls Conv (`cv3`), DFL (`dfl/conv`), Softmax, Sub, Add, Div, Mul | 46 (Inst 216–261) | **100% PASS** |
| **Total YOLOv8m** | **`model.0`–`model.22`** | **1,084 ONNX Graph Nodes** | **261 Instructions** | **261 / 261 PASS** |

### 4.2 ViT-Base Layer-by-Layer Coverage (Embeddings, 12 Transformer Blocks, Head)

The test executes the complete 12-layer Vision/Text Transformer pipeline end-to-end:

| Transformer Submodule | Layer Range | Internal Operators Executed | HW Instructions Lowered | Checkpoint Status |
| :--- | :--- | :--- | :--- | :--- |
| **Embeddings** | `embeddings` | Token Embeddings, Position Embeddings, Embedding Vector Addition | 1 (Inst 1) | **100% PASS** |
| **Encoder Block 0** | `layers.0` | LayerNorm1, Q/K/V Projections, Attention Softmax, Out Proj, Residual Add, LayerNorm2, MLP (fc1, GeLU, fc2), Residual Add | 41 (Inst 2–42) | **100% PASS** |
| **Encoder Block 1** | `layers.1` | Self-Attention (Q, K, V, Softmax, Proj), LayerNorm 1 & 2, MLP (fc1, fc2), Residual Adds | 41 (Inst 43–83) | **100% PASS** |
| **Encoder Block 2** | `layers.2` | Self-Attention (Q, K, V, Softmax, Proj), LayerNorm 1 & 2, MLP (fc1, fc2), Residual Adds | 41 (Inst 84–124) | **100% PASS** |
| **Encoder Block 3** | `layers.3` | Self-Attention (Q, K, V, Softmax, Proj), LayerNorm 1 & 2, MLP (fc1, fc2), Residual Adds | 41 (Inst 125–165) | **100% PASS** |
| **Encoder Block 4** | `layers.4` | Self-Attention (Q, K, V, Softmax, Proj), LayerNorm 1 & 2, MLP (fc1, fc2), Residual Adds | 41 (Inst 166–206) | **100% PASS** |
| **Encoder Block 5** | `layers.5` | Self-Attention (Q, K, V, Softmax, Proj), LayerNorm 1 & 2, MLP (fc1, fc2), Residual Adds | 41 (Inst 207–247) | **100% PASS** |
| **Encoder Block 6** | `layers.6` | Self-Attention (Q, K, V, Softmax, Proj), LayerNorm 1 & 2, MLP (fc1, fc2), Residual Adds | 41 (Inst 248–288) | **100% PASS** |
| **Encoder Block 7** | `layers.7` | Self-Attention (Q, K, V, Softmax, Proj), LayerNorm 1 & 2, MLP (fc1, fc2), Residual Adds | 41 (Inst 289–329) | **100% PASS** |
| **Encoder Block 8** | `layers.8` | Self-Attention (Q, K, V, Softmax, Proj), LayerNorm 1 & 2, MLP (fc1, fc2), Residual Adds | 41 (Inst 330–370) | **100% PASS** |
| **Encoder Block 9** | `layers.9` | Self-Attention (Q, K, V, Softmax, Proj), LayerNorm 1 & 2, MLP (fc1, fc2), Residual Adds | 41 (Inst 371–411) | **100% PASS** |
| **Encoder Block 10** | `layers.10` | Self-Attention (Q, K, V, Softmax, Proj), LayerNorm 1 & 2, MLP (fc1, fc2), Residual Adds | 41 (Inst 412–452) | **100% PASS** |
| **Encoder Block 11** | `layers.11` | Self-Attention (Q, K, V, Softmax, Proj), LayerNorm 1 & 2, MLP (fc1, fc2), Residual Adds | 41 (Inst 453–493) | **100% PASS** |
| **Final LayerNorm** | `final_layer_norm` | Layer Normalization (Mean, Variance, Scale, Bias) | 3 (Inst 494–496) | **100% PASS** |
| **Projection Head** | `text_projection` | Linear Projection MatMul to Classification Embeddings | 1 (Inst 497) | **100% PASS** |
| **Total ViT-Base** | **12 Encoder Blocks** | **2,297 ONNX Graph Nodes** | **497 Instructions** | **497 / 497 PASS** |

---

## 5. Operator Execution Mapping Classification

```
+-----------------------------------------------------------------------------------------------+
|                                      ONNX GRAPH OPERATORS                                     |
+-----------------------------------------------+-----------------------------------------------+
|       DIRECT HARDWARE COMPUTE (NPU)           |         COMPILER MEMORY GRAPH (ZERO-COPY)     |
+-----------------------------------------------+-----------------------------------------------+
|  - Conv, Gemm, MatMul, MatMulInteger (0x12)   |  - Reshape, Transpose, Flatten, Squeeze       |
|  - Softmax, Fused Multi-Head Attention (0x13) |  - QuantizeLinear, DequantizeLinear (Folded)  |
|  - LayerNorm (0x14)                           |  - Concat (Contiguous DRAM Packing)           |
|  - Add, Mul, Sub, Div, Sigmoid, MaxPool (0x15)|  - Split, Slice (DMA Address Offset Indexing) |
|                                               |  - Resize (Nearest-Neighbor Aliasing)         |
+-----------------------------------------------+-----------------------------------------------+
```

### 5.1 Direct Hardware Acceleration
1. **Opcode `0x12` (`GEMM_FUSED`)**:
   - Executes 2D Convolutions, Linear Layers, and MatMul projections on the 64×64 PE Systolic Array.
   - Fuses post-activations (`ReLU`, `Sigmoid`, `GeLU`) directly in the Output Bias Unit (Obp).
2. **Opcode `0x13` (`FUSED_ATTN`)**:
   - Executes multi-head self-attention score computations and scaled dot-product Softmax.
3. **Opcode `0x14` (`LAYERNORM`)**:
   - Computes channel mean, variance, normalization, scaling ($\gamma$), and offset ($\beta$).
4. **Opcode `0x15` (`ELEM_WISE`)**:
   - Executes vector arithmetic (Modes 0: ADD, 1: MAX_POOL, 2: MUL, 3: SUB, 4: DIV) with dynamic channel and scalar broadcasting.

### 5.2 Compiler Memory Graph Transformations (Zero-Copy)
- **Shape & Index Transformations**: `Reshape`, `Transpose`, `Flatten`, `Squeeze`, `Unsqueeze`, and `Identity` perform zero-copy memory stride aliasing.
- **Quantization Folding**: `QuantizeLinear` and `DequantizeLinear` nodes fold scale factors directly into hardware MMIO registers (`0x40000438`–`0x40000440`).
- **Buffer Management**: `Concat` groups outputs into contiguous DRAM regions; `Split` and `Slice` adjust DMA source addresses without intermediate copies.
- **`Resize` Nodes (2 nodes in YOLOv8m)**: The nearest-neighbor $2\times$ upsampling nodes (`/model.10/Resize` and `/model.13/Resize`) are handled via zero-copy memory aliasing.

---

## 6. Updated Host MMIO CSR Register Map (`0x40000400`–`0x40000468`)

| Register Symbol | MMIO Address | Type | Function |
| :--- | :--- | :--- | :--- |
| `FX1_CFG_IN_ADDR` | `0x40000400` | WO | Input Feature Map DRAM Address |
| `FX1_CFG_W_ADDR` | `0x40000404` | WO | Weight Tensor DRAM Address |
| `FX1_CFG_OUT_ADDR` | `0x40000408` | WO | Output Result DRAM Address |
| `FX1_CFG_BIAS_ADDR`| `0x4000040C` | WO | Bias Tensor DRAM Address |
| `FX1_CFG_M` | `0x40000410` | WO | Matrix M Dimension |
| `FX1_CFG_K` | `0x40000414` | WO | Matrix K Dimension |
| `FX1_CFG_N` | `0x40000418` | WO | Matrix N Dimension |
| `FX1_CFG_A_ADDR` | `0x40000444` | WO | Operand A DRAM Address (ELEM_WISE / Attention Q) |
| `FX1_CFG_B_ADDR` | `0x40000448` | WO | Operand B DRAM Address (ELEM_WISE / Attention K) |
| `FX1_CFG_LEN` | `0x40000450` | WO | Output Element Count $L_{out}$ |
| `FX1_CFG_MODE` | `0x40000454` | WO | Operation Mode (0: ADD, 1: MAX_POOL, 2: MUL, 3: SUB, 4: DIV) |
| **`FX1_CFG_A_LEN`** | **`0x40000464`** | **WO** | **Explicit Operand A Element Count $L_A$ (NEW)** |
| **`FX1_CFG_B_LEN`** | **`0x40000468`** | **WO** | **Explicit Operand B Element Count $L_B$ (NEW)** |
| `FX1_QUEUE_A_PUSH` | `0x40000310` | WO | Trigger Execution on Lane A |
| `FX1_QUEUE_B_PUSH` | `0x40000314` | WO | Trigger Execution on Lane B |

---

## 7. How to Reproduce & Verify

To compile and verify both models from scratch:

```bash
cd /data/XPU00000/users/vuong.nguyen/project/sauria/RTL/src/v4.2_model

# 1. Compile & Run YOLOv8m INT8 (261 Checkpoints)
python3 tools/onnx_compiler.py --model ../yolov8m-int8.onnx --output tools/test_onnx_model.cpp
make test_onnx_model SYSTEMC_HOME=/data/XPU00000/users/vuong.nguyen/project/sauria/systemc_install
./test_onnx_model

# 2. Compile & Run ViT-Base INT8 (497 Checkpoints)
python3 tools/onnx_compiler.py --model ../vit_b-int8.onnx --output tools/test_onnx_model.cpp
make test_onnx_model SYSTEMC_HOME=/data/XPU00000/users/vuong.nguyen/project/sauria/systemc_install
./test_onnx_model
```
