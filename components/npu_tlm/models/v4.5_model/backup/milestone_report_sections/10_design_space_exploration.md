# SAURIA FX1 (v4.4 SoC / v4.2 NPU Core) — Milestone Report Part 10: Architectural Design Space Exploration & Sensitivity Analysis

> **Document Title**: SAURIA FX1 Architectural Design Space Exploration & Sensitivity Matrix Report  
> **Document Identifier**: `SAURIA-DOC-MS4-2026-V4.4-P10`  
> **Document Version**: `4.4.0` (Final Release)  
> **Target Hardware**: SAURIA FX1 NPU Core (Dual-Lane $64 \times 64$ Rev2 Architecture / FX1 SoC Integration)  
> **Implementation**: SystemC 2.3.3 / IEEE 1666-2023 Compliant Cycle-Approximate Virtual Prototype (C++17)  
> **Authoring Body**: VP Team / SAURIA NPU Architecture & Systems Engineering  

---

# 10. Architectural Design Space Exploration & Sensitivity Analysis

A key deliverable of the SystemC modeling phase was conducting quantitative architectural exploration to guide hardware resource allocation prior to RTL freeze. By parameterized array dimensions, memory bank sizes, queue structures, and non-linear reduction hardware in C++, the engineering team evaluated three major hardware microarchitectures under realistic neural network workloads.

---

## 10.1 Architectural Sensitivity Matrix & Speedup Analysis

The sensitivity matrix table below contrasts three hardware configurations evaluated during the design space exploration phase: the initial Rev1 single-lane baseline, the intermediate Rev1.5 dual-lane prototype, and the final Rev2 dual-lane release:

| Hardware Configuration | Array Geometry | Total Compute Capacity | ViT-Base INT8 Latency | YOLOv8m INT8 Latency | Relative Speedup vs Base | Selection Verdict & Microarchitectural Rationale |
| :--- | :---: | :---: | :---: | :---: | :---: | :--- |
| **Rev1 Baseline** | $32 \times 32$ Single-Lane | 1,024 MACs/cycle | 1,620,000 cycles | 7,850,000 cycles | **1.00× (Baseline)** | **Rejected**: Severe performance bottlenecks caused by MAC compute limits and shared single reduction unit serialization. |
| **Rev1.5 Intermediate** | $32 \times 32$ Dual-Lane | 2,048 MACs/cycle | 840,000 cycles | 4,100,000 cycles | **1.93×** | **Passed Feasibility**: Decoupled queues and dual lanes eliminated head-of-line blocking, but execution remained MAC-bound on large GEMMs. |
| **Rev2 Final (Selected)** | **$64 \times 64$ Dual-Lane** | **4,096 MACs/cycle** | **405,700 cycles** | **2,059,200 cycles** | **3.99× (Selected Target)** | **Selected Production Baseline**: Symmetric dual $32 \times 64$ PE grids per lane. Achieves real-time throughput ($> 30\text{ FPS}$) for high-resolution models. |

---

## 10.2 Detailed Microarchitectural Exploration Findings

### 1. Near-Linear Compute Scalability ($3.99\times$ Latency Reduction)
Scaling the PE array geometry from $32 \times 32$ (1,024 MACs) to $64 \times 64$ (4,096 MACs) delivered a **$3.99\times$ latency speedup** across full-graph neural network models. For compute-bound layers—such as the $768 \times 3072$ MLP projections in Vision Transformers and the $3 \times 3$ stride-2 convolutions in YOLOv8m—the 4,096 MAC array processed matrix tiles with near-linear parallel efficiency.

### 2. Dual-Lane Task Decoupling & Queue Independence
Dividing the $64 \times 64$ PE grid into two symmetric $32 \times 64$ PE arrays (Lane A and Lane B) driven by dual asynchronous queues (`Queue A` and `Queue B`) eliminated task serialization. In single-lane prototypes, light vector operations (e.g. LayerNorm or bias addition) were forced to wait behind long matrix multiplication tiles. In the Rev2 dual-lane architecture, vector operations execute concurrently on Lane B while Lane A processes matrix tiles, maximizing aggregate functional unit utilization (`engine_utilization = 73.65%`).

### 3. Memory Capacity & Bandwidth Balancing (~2.1 MB SRAM)
Design space exploration revealed that increasing compute capacity without proportional SRAM expansion leads to severe AXI memory bus stalls. Expanding local physical SRAM storage to **~2.1 MB across 6 banks** ensured that sequence feature matrices ($S=197, D=768$) remain resident on-chip during multi-head attention processing. Double-buffered ping-pong streaming effectively reduced external memory transfer overhead to zero (`transfer_overhead_cycles = 0`).
