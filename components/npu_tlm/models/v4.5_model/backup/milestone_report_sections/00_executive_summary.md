# SAURIA FX1 (v4.4 SoC / v4.2 NPU Core) — Milestone Report Part 0: Executive Summary & Overview

> **Document Title**: SAURIA FX1 Neural Processing Unit — SystemC Virtual Prototype & Compiler Milestone Report  
> **Document Identifier**: `SAURIA-DOC-MS4-2026-V4.4-P00`  
> **Document Version**: `4.4.0` (Final Release)  
> **Target Hardware**: SAURIA FX1 NPU Core (Dual-Lane $64 \times 64$ Rev2 Architecture / FX1 SoC Integration)  
> **Implementation**: SystemC 2.3.3 / IEEE 1666-2023 Compliant Cycle-Approximate Virtual Prototype (C++17)  
> **Toolchain Platform**: Automated ONNX Compiler Toolchain & Testbench Generator (`tools/onnx_compiler.py`)  
> **Verification Status**: **100.0% Bit-Exact PASS** across YOLOv8m INT8 (261 Checkpoints) & ViT-Base INT8 (497 Checkpoints)  
> **Release Date**: August 25, 2026  
> **Authoring Body**: VP Team / SAURIA NPU Architecture & Systems Engineering  
> **Classification**: Proprietary / Hardware Engineering Milestone Report  

---

## Document Revision History

The architectural evolution of the SAURIA FX1 Neural Processing Unit virtual prototype spans five major release milestones. The table below summarizes the core engineering revisions, authoring bodies, and major feature updates delivered throughout the development cycle:

| Version | Date | Primary Author(s) | Summary of Major Architectural Changes & Revisions |
| :--- | :--- | :--- | :--- |
| **v1.0.0** | May 15, 2026 | Architecture Team | Initial baseline single-lane $32 \times 32$ PE model specification and functional proof of concept. Established baseline SystemC structures and basic matrix multiplication emulation. |
| **v2.0.0** | June 20, 2026 | Virtual Prototype Team | Introduction of dual-lane processing, basic 4-channel AXI DMA controller modeling, initial LayerNorm vector acceleration, and double-buffered SRAM prefetching mechanisms. |
| **v3.0.0** | Compiler & Systems Team | Integration of the automated ONNX compiler framework (`onnx_compiler.py`), 4-stage Output Post-Processing (OBP) epilogue pipeline, and 16 KB Activation SRAM lookup tables. |
| **v4.2.0** | August 16, 2026 | Verification Team | Hardware expansion to a $64 \times 64$ PE grid geometry, dual independent RE/RCE engines equipped with 24 KB Scratch SRAM buffers per lane, and resolution of LayerNorm Lane B scratchpad collision bugs. |
| **v4.4.0** | August 25, 2026 | Core Engineering Team | **Final Milestone Release**: Achieved 100.0% bit-exact verification pass rates across YOLOv8m INT8 (261 checkpoints) and ViT-Base INT8 (497 checkpoints), enforced pure INT8 integer MAC accumulation, integrated a 60-metric performance profiler, and aligned with VCS gate-level RTL hand-off requirements. |

---

## Executive Summary & Detailed Architectural Overview

This technical document serves as Part 0 of the formal engineering milestone completion report for the **SAURIA FX1 (v4.4 SoC / v4.2 NPU Core)** cycle-approximate SystemC virtual prototype and automated AI compiler infrastructure. The primary objective of this engineering program is to deliver a golden, software-executable architectural reference model capable of driving full-graph deep learning workloads while providing precise cycle-level telemetry for hardware verification and SoC driver integration.

The virtual prototype models the complete microarchitectural pipeline of the NPU, including host instruction decoding, asynchronous queue scheduling, AXI DMA data transfers, weight-stationary systolic matrix multiplication, post-processing quantization, and non-linear reduction operations. The report explicitly addresses the three fundamental engineering questions governing the sign-off of this milestone:

### 1. What has been developed?

The engineering team has designed and delivered a modular, header-only C++17 SystemC cycle-approximate virtual prototype modeling the dual-lane **SAURIA FX1 NPU Core**, alongside a fully automated Python software compiler toolchain. 

At the core of the compute architecture is a **Symmetric Dual-Lane Processing Array**, which houses two independent compute lanes designated as **Lane A** and **Lane B**. Each lane contains a $32 \times 64$ Processing Element (PE) grid, establishing a combined chip-level compute array of $64 \text{ rows} \times 64 \text{ columns}$ ($4,096$ total MAC units). Operating in Output-Stationary (OS) dataflow mode, the array yields a peak computational throughput of 8,192 INT8 operations per clock cycle. The row boundary between the two lanes is dynamically managed by a runtime partition parameter $N_{split}$, enabling flexible allocation of array resources between single-lane isolated execution and dual-lane concurrent processing.

To drive this execution hardware efficiently without control bottlenecks, the core implements a **64-bit Rich Instruction Set Architecture (ISA)**. The ISA packs multi-operand control descriptors into specialized opcodes (`SET_NSPLIT`, `GEMM_FUSED`, `FUSED_ATTN`, `LAYERNORM`, and `ELEM_WISE`). Instructions are submitted via two asynchronous, non-blocking hardware queues (`Queue A` and `Queue B`) mapped directly into host MMIO register space (`0x40000310` and `0x40000314`). This dual-queue architecture allows host software to stream independent task graphs to Lane A and Lane B concurrently, eliminating head-of-line blocking and enabling true multi-tenant hardware execution.

For post-processing and activation scaling, each execution lane incorporates a dedicated **Output Post-Processing Block (OBP Epilogue Engine)**. Mapped directly into the partial sum drain path, the OBP engine processes 64 parallel channels in SIMD fashion through a 4-stage hardware pipeline. Stage 1 adds 32-bit channel bias vectors; Stage 2 applies fixed-point scale multiplication and right-shift requantization; Stage 3 indexes into a **16 KB SRAM Activation Lookup Table (LUT)** supporting non-linear functions such as ReLU, SiLU ($x \cdot \sigma(x)$), and GELU; and Stage 4 performs residual skip additions before clamping final output elements to signed 8-bit integer bounds $[-128, +127]$.

Non-linear reduction operations—such as multi-head attention Softmax and Transformer Layer Normalization—are offloaded to a **Dual Reconfigurable & Reduction Subsystem (RCE / RE)**. Duplicated across both lanes (`RCEA`/`REA` on Lane A and `RCEB`/`REB` on Lane B), each engine is equipped with a dedicated **24 KB Scratch SRAM buffer** (`ScratchA` in Bank 4 and `ScratchB` in Bank 5). These engines implement Piecewise Linear (PWL) mathematical lookup tables (`LUT_exp`, `LUT_recip`, `LUT_rsqrt`) and shared 64-wide SIMD max-reduction comparator trees, allowing two-pass Softmax, two-pass Layer Normalization, and 5x5 Spatial Pyramid Pooling Fast (SPPF) max-pooling to run on-chip without CPU intervention or memory bank collisions.

Data transfer between external system DRAM and local hardware storage is coordinated by an **Autonomous 4-Channel AXI DMA Controller**. Operating over a 256-bit wide AXI bus (32 bytes per cycle) with 8-beat burst transfers, the DMA engine streams data into **~2.1 MB of on-chip physical SRAM** partitioned into 6 dedicated memory banks (320 KB, 320 KB, 416 KB, 408 KB, 50 KB, 50 KB). The DMA hardware implements double-buffering (ping-pong streaming) to overlap memory transfer latency with active matrix computation, while enforcing strict physical memory capacity clamping to protect local SRAM buffers against out-of-bounds burst transfers.

Finally, the hardware release is complemented by the **End-to-End ONNX Compiler Toolchain (`tools/onnx_compiler.py`)**. This automated tool ingests standard ONNX model files, performs dynamic shape inference, resolves tensor transformations (`Reshape`, `Transpose`, `Concat`, `Split`) through zero-compute memory address pointer remapping, allocates 256-byte aligned DRAM payloads, runs NumPy-based bit-exact golden emulation, and outputs C++ SystemC verification testbenches (`test_onnx_model.cpp`).

---

### 2. How has the model been verified and validated?

The validation strategy for the SAURIA FX1 virtual prototype followed a multi-tiered engineering methodology combining unit-level microbenchmarks, standalone hardware stress tests, and automated full-graph production model execution.

For unit verification, a suite of 10 microbenchmarks (`TC01`–`TC10`) and 7 standalone hardware testbenches (`ST01`–`ST07`) was constructed. These tests systematically validated individual microarchitectural features, including single-cycle INT8 MAC accumulation, full $64 \times 64$ tile matrix multiplication, $3 \times 3$ sliding-window convolution, extreme numerical boundary saturation, dual-queue asynchronous dispatch, LayerNorm bank isolation (achieving 0 mismatches across 256 random test vectors), and SPPF max-pooling comparator reuse.

For full-graph production validation, the virtual prototype was evaluated against two benchmark deep neural networks representing computer vision and vision-language transformer workloads:

* **YOLOv8m INT8 (`yolov8m-int8.onnx`)**: A $26\text{ MB}$ ONNX object detection network comprising 1,084 ONNX graph nodes compiled into 261 hardware instruction checkpoints. The virtual prototype executed all 23 architectural layers (`model.0` through `model.22`) with an overall pass rate of **261 / 261 Checkpoints Passed (100.0% Pass Rate)**, achieving a Maximum Absolute Error ($\text{MAE}$) of `0.000000` and a Cosine Similarity of `1.000000`.
* **ViT-Base INT8 (`vit_b-int8.onnx`)**: A $62\text{ MB}$ Vision Transformer model comprising 2,297 ONNX graph nodes compiled into 497 hardware instruction checkpoints. The virtual prototype executed all 15 major submodules—including Token Embeddings, Positional Additions, 12 Multi-Head Self-Attention Encoder Blocks, Final Layer Normalization, and Output Linear Classification Projection—achieving **497 / 497 Checkpoints Passed (100.0% Pass Rate)** with an $\text{MAE}$ of `0.000000` and a Cosine Similarity of `1.000000`.

---

### 3. What are the key results and next steps?

Quantitative profiling of the virtual prototype operating at a target clock frequency of $1.0\text{ GHz}$ demonstrated high performance and energy efficiency. On the ViT-Base INT8 benchmark, the core achieved an end-to-end execution latency of **405,700 clock cycles** ($0.41\text{ ms}$), delivering an inference throughput of **1,971.9 inferences per second**. On the YOLOv8m INT8 benchmark, the core completed full inference in **2,059,200 clock cycles** ($2.06\text{ ms}$), achieving a throughput of **388.5 inferences per second**.

Architectural design space exploration confirmed the effectiveness of the Rev2 microarchitecture. Upgrading the hardware array geometry from the initial $32 \times 32$ single-lane baseline to the Rev2 dual-lane $64 \times 64$ configuration yielded a **$3.99\times$ performance acceleration**, enabling real-time frame rates ($> 30\text{ FPS}$) for high-resolution vision workloads.

With the virtual prototype milestone fully signed off, immediate engineering efforts are focused on the following four roadmap objectives:
1. **RTL Gate-Level Correlation**: Correlating SystemC golden trace dumps (`golden_ref.bin`) against Synopsys VCS gate-level RTL simulations using the DPI-C scoreboard framework (`dpi_predictor.cpp`).
2. **Bare-Metal SoC Driver Finalization**: Completing the bare-metal Ibex RISC-V C driver firmware (`firmware.c` / `firmware.h`) and MMIO register polling routines on the virtual SoC platform.
3. **Hardware 2D Image Resizer Integration**: Designing and synthesizing a hardware 2D bilinear/nearest-neighbor `Resize` engine into the RTL datapath to offload pre-processing upsample and downsample operations.
4. **Multi-Context Workload Batching**: Extending instruction queue arbitration to support multi-context hardware batching for simultaneous multi-tenant model inference.
