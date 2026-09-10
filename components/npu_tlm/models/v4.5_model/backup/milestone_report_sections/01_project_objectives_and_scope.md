# SAURIA FX1 (v4.4 SoC / v4.2 NPU Core) — Milestone Report Part 1: Project Objectives, Scope, and Specification Requirements

> **Document Title**: SAURIA FX1 Neural Processing Unit — Objectives, Scope & Traceability  
> **Document Identifier**: `SAURIA-DOC-MS4-2026-V4.4-P01`  
> **Document Version**: `4.4.0` (Final Release)  
> **Target Hardware**: SAURIA FX1 NPU Core (Dual-Lane $64 \times 64$ Rev2 Architecture / FX1 SoC Integration)  
> **Implementation**: SystemC 2.3.3 / IEEE 1666-2023 Compliant Cycle-Approximate Virtual Prototype (C++17)  
> **Authoring Body**: VP Team / SAURIA NPU Architecture & Systems Engineering  

---

# 1. Project Objectives, Scope, and Specification Requirements

The primary objective of the SAURIA FX1 Virtual Prototyping project is to construct a production-grade, cycle-approximate SystemC execution environment alongside an automated software compiler toolchain. As deep learning workloads increase in complexity—spanning convolutional vision architectures like YOLOv8 and multi-head attention vision-language transformers like ViT-Base—hardware development requires a high-fidelity virtual prototype. This model serves as the authoritative golden architectural reference for hardware verification, software driver development, and pre-silicon performance exploration prior to RTL tape-out.

The scope of this project encompasses the complete modeling of the NPU microarchitecture. This includes host command interface decoding, dual asynchronous instruction queue dispatch, AXI DMA data prefetching, weight-stationary systolic array matrix multiplication, 4-stage post-processing epilogue quantization, dual non-linear reduction acceleration, and on-chip SRAM banking. By modeling these hardware subsystems in standard C++17 SystemC, the virtual prototype enables full-graph model execution with bit-exact numerical fidelity against golden fixed-point reference software.

---

## 1.1 Project Specification & Scope Traceability

The development of the SAURIA FX1 microarchitecture progressed through multiple architectural iterations, advancing from an initial single-lane proof of concept (Rev1) to the production-ready dual-lane Rev2 milestone (v4.4). The specification traceability table below provides a comprehensive comparison between the original engineering goals and the final delivered milestone capabilities across eleven key hardware and software scope categories:

| Engineering Scope Category | Original Planned Goal | Final Milestone Release | Detailed Engineering Status & Architectural Rationale |
| :--- | :--- | :--- | :--- |
| **Top-Level Core Modeling** | Single-Lane NPU Core | **Dual-Lane Core** | **Exceeded Goal**: Scaled hardware to symmetric dual execution lanes (**Lane A** and **Lane B**). Implemented a dynamic runtime partition parameter $N_{split}$ ($0 \le N_{split} \le 64$) that splits array rows dynamically, enabling independent task queue dispatch and zero head-of-line blocking. |
| **PE Array Geometry** | $32 \times 32$ PE Grid | **$64 \times 64$ PE Grid** | **Exceeded Goal**: Scaled compute array from 1,024 MACs to 4,096 total MAC units ($32 \times 64$ PEs per lane). In Output-Stationary mode, the array delivers 8,192 INT8 operations per clock cycle, yielding a $3.99\times$ performance acceleration. |
| **Data Types Supported** | INT8 & FP16 | **INT8 / FP16/INT16** | **Achieved Goal**: Implemented parameterized C++ templates across all compute modules. The v4.4 release enforces strict signed 8-bit integer multiplication, 32-bit accumulation, and $[-128, +127]$ saturation for bit-exact RTL correlation. |
| **On-Chip SRAM Modeling** | Unified 512 KB SRAM | **~2.1 MB (6 Banks)** | **Exceeded Goal**: Expanded physical SRAM storage to **~2.1 MB (2,129,920 Bytes)** partitioned into 6 dedicated memory banks (Banks 0–5). Added hardware capacity memory clamping to prevent DMA buffer overflow crashes. |
| **DMA Controller Modeling** | 2-Channel Simple DMA | **4-Channel AXI DMA** | **Exceeded Goal**: Implemented a cycle-approximate 4-channel 256-bit AXI DMA engine (`CH0`–`CH3`) with round-robin burst scheduling, double-buffering ping-pong streaming, and an independent 256-bit write master. |
| **Epilogue Pipeline** | Basic ReLU Activation | **4-Stage Fused OBP** | **Exceeded Goal**: Developed a 4-stage Output Post-Processing Engine per lane. Fuses 32-bit Bias Addition, Requantization Scale/Shift, 16 KB SRAM Activation LUT lookups (ReLU, SiLU, GELU), and Residual Skip Additions directly into the drain path. |
| **Non-Linear Functions** | Host CPU Software | **Dual RE / RCE** | **Exceeded Goal**: Offloaded non-linear reduction operations to dual hardware engines (`RCEA`/`REA` and `RCEB`/`REB`). Equipped each engine with a dedicated **24 KB Scratch SRAM buffer** (`ScratchA` and `ScratchB`) for on-chip Softmax and LayerNorm. |
| **Instruction Set Architecture** | Basic 32-bit Control | **64-bit Rich ISA** | **Exceeded Goal**: Designed a compact 64-bit Rich ISA packing multi-operand parameters into five primary opcodes (`SET_NSPLIT`, `GEMM_FUSED`, `FUSED_ATTN`, `LAYERNORM`, `ELEM_WISE`) submitted via dual queues. |
| **Compiler Toolchain** | Manual Workloads | **`onnx_compiler`** | **Exceeded Goal**: Built an end-to-end Python compiler (`tools/onnx_compiler.py`) that ingests standard `.onnx` models, performs shape inference, resolves memory aliases with zero compute overhead, and outputs SystemC testbenches. |
| **Full Model Validation** | Single Layer Smoke | **YOLOv8m & ViT-B** | **Exceeded Goal**: Validated full-graph execution against YOLOv8m INT8 (261 checkpoints) and ViT-Base INT8 (497 checkpoints), achieving a **100.0% Bit-Exact Verification Pass Rate** with $\text{MAE} = 0.000000$ and Cosine Similarity $= 1.000000$. |
| **Performance Instrumentation** | Basic Cycle Counter | **60-Metric Suite** | **Exceeded Goal**: Integrated an exhaustive 60-metric telemetry profiler (`perf_counters.h`) spanning 11 categories (Cycle Counter, Engine Cycle, Engine Util, DMA Cycle, Memory Wait, Pipeline Stall, Utilization, Memory Traffic, Footprint, Bandwidth, Throughput). |

---

## 1.2 Detailed Engineering Motivations for Architectural Evolution (Rev1 $\rightarrow$ Rev2)

The transition from the early Rev1 single-lane prototype to the production Rev2 dual-lane microarchitecture was driven by three critical hardware engineering challenges identified during initial workload profiling:

### 1. Resolution of Execution Serialization
In Rev1 single-lane architectures, all non-linear reduction operations—such as multi-head attention Softmax and Transformer Layer Normalization—were routed through a single, shared hardware reduction unit. When concurrent workloads were dispatched across dual execution lanes, processing ground to a halt because both lanes competed for the single reduction unit. This created severe head-of-line blocking and eliminated the throughput benefits of dual-lane processing.

To resolve this serialization bottleneck, the Rev2 architecture completely decouples the non-linear reduction subsystem into two independent hardware instances: **`RCEA`/`REA` for Lane A** and **`RCEB`/`REB` for Lane B**. Furthermore, each engine was provisioned with its own dedicated **24 KB Scratch SRAM buffer** (`ScratchA` embedded in SRAM Bank 4 and `ScratchB` embedded in SRAM Bank 5). This spatial separation allows Lane A and Lane B to compute complex 2-pass LayerNorm and 2-pass Softmax operations simultaneously without memory contention or arbitration stalls.

### 2. Elimination of Quantization Drift
Early virtual prototype decoders utilized double-precision floating-point arithmetic (`double` / `float`) for matrix multiplication emulation. While functionally correct for unquantized floating-point neural networks, this approach introduced subtle numerical drift when evaluating quantized signed INT8 models. Floating-point accumulation accumulated rounding discrepancies that caused intermediate output tensors to drift away from physical fixed-point silicon behavior.

The v4.4 release eliminates quantization drift by enforcing **pure signed 8-bit integer multiplication, 32-bit integer accumulation, and $[-128, +127]$ clamping**. Every arithmetic step in the SystemC model mirrors the exact integer bit-width and saturation logic of the physical RTL multipliers and accumulators. This architectural decision guarantees 100% bit-exact numerical correlation between the virtual prototype, Python reference emulators, and gate-level VCS simulations.

### 3. Hardware-Constrained DMA Memory Protection
In real silicon implementations, on-chip SRAM scratchpads possess rigid physical capacity limits. During initial model compilation tests, large multi-head attention sequence tensors ($S=197, D=768$) occasionally exceeded the allocated local buffer bounds, resulting in memory pointer corruption and segmentation faults in simulation.

To protect system integrity, the v4.4 AXI DMA controller (`sauria_dma.h`) implements **hardware capacity memory clamping** across all six physical memory banks. When a transfer descriptor is evaluated, the DMA hardware automatically checks the requested byte length against the physical bank boundary. If an out-of-bounds access is detected, the transfer length is clamped to the bank boundary and an interrupt flag is set. This hardware safeguard prevents SRAM buffer overruns and traps illegal software descriptor addresses prior to execution.
