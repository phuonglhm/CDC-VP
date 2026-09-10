# SAURIA FX1 (v4.4 SoC / v4.2 NPU Core) — Milestone Report Part 5: Key Architectural & Microarchitectural Design Decisions

> **Document Title**: SAURIA FX1 Design Decisions, Dataflow & SRAM Banking Matrix  
> **Document Identifier**: `SAURIA-DOC-MS4-2026-V4.4-P05`  
> **Document Version**: `4.4.0` (Final Release)  
> **Target Hardware**: SAURIA FX1 NPU Core (Dual-Lane $64 \times 64$ Rev2 Architecture / FX1 SoC Integration)  
> **Implementation**: SystemC 2.3.3 / IEEE 1666-2023 Compliant Cycle-Approximate Virtual Prototype (C++17)  
> **Authoring Body**: VP Team / SAURIA NPU Architecture & Systems Engineering  

---

# 5. Key Architectural & Microarchitectural Design Decisions

The microarchitectural design of the SAURIA FX1 NPU involved strategic evaluations between spatial array dataflow topologies, physical memory layout, double-buffering ping-pong streaming mechanisms, and memory boundary protection. This section details the key architectural and design decisions that govern compute execution, physical SRAM banking, and hardware data protection.

---

## 5.1 Array Dataflow Selection: Output-Stationary (OS) Microarchitecture

During the architectural definition phase, the engineering team evaluated three primary spatial array dataflow paradigms: **Output-Stationary (OS)**, **Weight-Stationary (WS)**, and **Input-Stationary (IS)**.

The SAURIA FX1 microarchitecture explicitly selected an **Output-Stationary (OS)** spatial array dataflow:
* **Stationary Accumulators**: Each Processing Element (PE) at coordinate $(i, j)$ in the $64 \times 64$ grid contains a dedicated 32-bit partial sum accumulator register $P_{i,j}$. During the $K$ reduction iterations of a matrix tile, accumulated partial sums remain stationary inside PE accumulators without intermediate inter-PE wire shifting.
* **Horizontal Activation Streaming**: Input activation vectors stream horizontally across PE rows $i$ from left to right, fed continuously by Data Feeder A (SRAM Bank 2) and Data Feeder B (SRAM Bank 3).
* **Vertical Weight Streaming**: Weight matrix parameters stream vertically down PE columns $j$ from top to bottom, fed by Data Feeder A (SRAM Bank 0) and Data Feeder B (SRAM Bank 1).
* **Clock-by-Clock Computation**: On every clock cycle $k \in [0, K-1]$, PE $(i, j)$ computes the 32-bit multiply-accumulate operation:
  $$P_{i,j} \leftarrow P_{i,j} + A_{i,k} \cdot B_{k,j}$$
* **Sequential Epilogue Drain Phase**: After completing all $K$ contraction cycles, partial sums $P_{i,j}$ are drained sequentially out of the PE array columns into the 4-stage Output Post-Processing Block (OBP Epilogue Engine) for channel bias addition, fixed-point scale requantization, 16 KB Activation LUT indexing, and residual skip addition before writing to SRAM Banks 4 and 5.

### Architectural Rationale & Energy Advantages of Output-Stationary Dataflow

The choice of an Output-Stationary dataflow delivers several critical microarchitectural benefits for the SAURIA FX1 core:

1. **Elimination of Inter-PE Accumulator Shift Energy**: In Weight-Stationary arrays, partial sums must be continuously shifted between adjacent PEs on every clock cycle. In contrast, the Output-Stationary dataflow holds accumulating values inside local 32-bit registers, completely eliminating high-energy inter-PE wire switching during $K$ reduction iterations.
2. **High Precision Accumulation**: Maintaining 32-bit accumulation inside stationary PE registers prevents intermediate truncation or precision loss. Full 32-bit precision is preserved across thousands of MAC steps before single-pass requantization down to 8-bit signed integers inside the OBP epilogue pipeline.
3. **Optimized Tile Throughput for Large Contractions**: For large GEMM tiles ($K \ge 64$), streaming activations and weights while keeping output accumulators stationary maximizes systolic array compute utilization, achieving up to 73.65% aggregate engine utilization during full-model ViT-Base execution.

---

## 5.2 On-Chip Physical SRAM Memory Banking (~2.1 MB Total)

To support concurrent dual-lane execution without memory port collisions or read-write arbitration stalls, the physical SRAM array (`sram/sram_top.h`) is partitioned into six dedicated memory banks. Total physical storage equals **~2.1 MB (2,129,920 Bytes)**, allocated across specific functional regions:

### Physical SRAM Bank Allocation & Mapping Matrix (~2.1 MB Total)

| Bank ID | Assigned Target Consumers & Modules | Dedicated SRAM Storage Region | Physical Capacity (Bytes / KB) | Bus Width & Memory Access Characteristics |
| :---: | :--- | :--- | :---: | :--- |
| **Bank 0** | Lane A Weights (`wei_feeder_a`), Attention Q, LayerNorm $\gamma$ | Weight Region A | **320 KB** (`0x0005_0000`) | Dedicated 256-bit Read Port connected to DMA Read `CH0` and Feeder A. |
| **Bank 1** | Lane B Weights (`wei_feeder_b`), Attention K, LayerNorm $\beta$ | Weight Region B | **320 KB** (`0x0005_0000`) | Dedicated 256-bit Read Port connected to DMA Read `CH1` and Feeder B. |
| **Bank 2** | Lane A IFMap (`act_feeder_a`), Attention V, Elementwise Vector A | Activation Region A | **416 KB** (`0x0006_8000`) | Dedicated 256-bit Read Port connected to DMA Read `CH2` and Feeder A. |
| **Bank 3** | Lane B IFMap (`act_feeder_b`), Residual Skip Matrix, Vector B | Activation Region B | **408 KB** (`0x0006_6000`) | Dedicated 256-bit Read Port connected to DMA Read `CH3` and Feeder B. |
| **Bank 4** | Lane A Output Drain (`obp_inst_a`), Lane A ScratchA (`re_inst_a`) | Output / Scratch Region A | **50 KB** ($26\text{K} + 24\text{K}$) | Dedicated 256-bit Read/Write Master Port for Lane A writeback & reduction. |
| **Bank 5** | Lane B Output Drain (`obp_inst_b`), Lane B ScratchB (`re_inst_b`) | Output / Scratch Region B | **50 KB** ($26\text{K} + 24\text{K}$) | Dedicated 256-bit Read/Write Master Port for Lane B writeback & reduction. |

### Memory Bank Isolation Benefits
By assigning dedicated, physically separate memory banks to Lane A weights (Bank 0), Lane B weights (Bank 1), Lane A activations (Bank 2), Lane B activations (Bank 3), Lane A outputs (Bank 4), and Lane B outputs (Bank 5), the hardware completely eliminates read-write port conflicts. Lane A and Lane B stream weights and activations simultaneously at full bus width (256 bits per cycle per bank) without memory arbitration delays.

---

## 5.3 Double Buffering & Hardware Memory Clamping

### Double-Buffered Ping-Pong Streaming
To prevent compute units from idling while waiting for DRAM transfers, each SRAM bank is logically divided into dual ping-pong buffers (`Buffer 0` and `Buffer 1`):
* While the Systolic Array computes matrix multiplication for tile $T_i$ using operands stored in `Buffer 0`, the AXI DMA engine concurrently prefetches weight and activation payloads for tile $T_{i+1}$ from system DRAM into `Buffer 1`.
* Upon completion of tile $T_i$, the main controller swaps buffer pointers in a single clock cycle ($T_{\text{cswitch}} = X_{\text{DIM}} + Y_{\text{DIM}} + 16\text{ cycles}$).
* This double-buffering mechanism effectively hides external DRAM memory access latency behind active matrix computation cycles, reducing DMA memory stall cycles (`dma_stall_cycles`) to zero across compute-bound workloads.

### Hardware Memory Capacity Clamping
To protect local scratchpad memories against software compilation errors or invalid DRAM transfer descriptors, the AXI DMA engine (`sauria_dma.h`) implements **hardware capacity memory clamping** across all six physical banks.

During transfer descriptor initialization, the DMA controller checks the requested transfer size against the physical boundary of the target bank. If a compiler descriptor attempts to write beyond the bank's allocated limit (e.g. attempting a 64 KB transfer into 50 KB Bank 4), the DMA hardware automatically clamps the byte transfer count to the exact bank boundary and asserts an MMIO error status flag. This hardware clamping mechanism prevents local SRAM corruption and traps invalid software pointer offsets prior to execution.
