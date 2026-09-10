# SAURIA FX1 (v4.4 SoC / v4.2 NPU Core) — Milestone Report Part 13: Comparison Against Baseline & Scope Traceability

> **Document Title**: SAURIA FX1 Comparison Against Original Engineering Baseline & Milestone Traceability  
> **Document Identifier**: `SAURIA-DOC-MS4-2026-V4.4-P13`  
> **Document Version**: `4.4.0` (Final Release)  
> **Target Hardware**: SAURIA FX1 NPU Core (Dual-Lane $64 \times 64$ Rev2 Architecture / FX1 SoC Integration)  
> **Implementation**: SystemC 2.3.3 / IEEE 1666-2023 Compliant Cycle-Approximate Virtual Prototype (C++17)  
> **Authoring Body**: VP Team / SAURIA NPU Architecture & Systems Engineering  

---

# 13. Comparison Against Baseline & Scope Traceability

The SAURIA FX1 virtual prototype project successfully advanced through three major architectural milestones: the original Rev1 single-lane baseline, the intermediate Rev1.5 dual-lane prototype, and the final Rev2 v4.4 milestone release. This section provides a detailed comparative analysis demonstrating how the final release met or exceeded every original architectural objective.

---

## 13.1 Quantitative Evolution Matrix (Rev1 Baseline vs Rev2 Final)

The evolution matrix table below contrasts the three major design releases across ten core architectural dimensions:

| Architectural Dimension | Original Baseline (Rev1) | Intermediate Design (Rev1.5) | Final Milestone Release (Rev2 / v4.4) | Detailed Architectural Delta & Engineering Impact |
| :--- | :---: | :---: | :---: | :--- |
| **Execution Core Topology** | Single-Lane Core | Dual-Lane Core | **Dual-Lane Core (Lane A & B)** | **Expanded Core**: Dual symmetric execution lanes with dynamic $N_{split}$ spatial boundary partitioning. Eliminates head-of-line blocking. |
| **PE Array Geometry** | $32 \times 32$ Grid (1,024 PEs) | $32 \times 32$ Dual (2,048 PEs) | **$64 \times 64$ Grid (4,096 PEs)** | **$4\times$ Parallel Compute**: Scaled grid to 4,096 total MACs ($32 \times 64$ per lane), delivering 8,192 INT8 Ops/cycle peak throughput. |
| **On-Chip SRAM Storage** | 512 KB Unified SRAM | 1.2 MB Partitioned | **~2.1 MB (6 Physical Banks)** | **$4.1\times$ Storage Capacity**: Expanded physical SRAM to ~2.1 MB across Banks 0–5 with hardware capacity memory clamping. |
| **AXI DMA Channels** | 2-Channel Simple DMA | 4-Channel AXI DMA | **4-Channel AXI DMA + Write Master** | **Double-Buffered DMA**: 256-bit wide bus data paths with Round-Robin burst scheduling and dedicated Write Master. |
| **Epilogue Pipeline** | Basic ReLU Unit | 2-Stage Epilogue | **4-Stage Fused OBP Engine** | **Fused Epilogue**: SIMD 4-stage engine fusing Bias Add, Requantization Scale/Shift, 16 KB Activation LUT, and Skip Add. |
| **Non-Linear Acceleration** | Host CPU Software | Shared Single Engine | **Dual RE / RCE (24 KB ScratchA/B)** | **Decoupled Reduction**: Dual non-linear engines with 24 KB ScratchA/B for on-chip 2-pass Softmax and LayerNorm. |
| **Instruction Control** | Basic 32-bit Control | Legacy 64-bit Word | **64-bit Fused Rich ISA** | **Compact Control**: 5 packed Rich ISA opcodes (`0x05`, `0x12`, `0x13`, `0x14`, `0x15`) submitted via dual queues. |
| **Compiler Infrastructure** | Manual Workloads | Basic Layer Script | **Automated `onnx_compiler.py`** | **End-to-End ONNX Toolchain**: Automated shape inference, zero-compute memory alias resolution, and SystemC testbench generation. |
| **Model Verification Scope** | Single Layer Smoke | Single Block ViT | **YOLOv8m & ViT-Base Full Graphs** | **100.0% Pass Rate**: Validated full-graph execution across 261 YOLO checkpoints and 497 ViT checkpoints with $\text{MAE} = 0.000000$. |
| **Performance Telemetry** | Basic Cycle Counter | 12 Performance Metrics | **Exhaustive 60-Metric Profiler** | **Comprehensive Telemetry**: Integrated profiler (`perf_counters.h`) reporting across 11 telemetry categories. |

---

## 13.2 Scope Expansion Highlights & Architectural Milestones

### 1. Compute Scaling ($4\times$ MAC Expansion)
Expanding PE array geometry from $32 \times 32$ (1,024 MACs) to $64 \times 64$ (4,096 MACs) yielded a **$3.99\times$ latency reduction** across full-graph neural network models, enabling real-time frame rates ($> 30\text{ FPS}$) for high-resolution vision models.

### 2. Decoupled Non-Linear Reduction Subsystem
Replacing host CPU software emulation for Softmax and LayerNorm with dual hardware engines (`RCEA`/`REA` and `RCEB`/`REB`) equipped with 24 KB Scratch buffers eliminated non-linear execution bottlenecks, allowing attention operations to execute on-chip without CPU stalls.

### 3. Automated Software Compiler Integration
Developing `tools/onnx_compiler.py` established a seamless software-hardware co-design bridge, automating graph ingestion, dynamic shape inference, zero-compute memory alias resolution, and C++ SystemC testbench generation directly from standard ONNX models.
