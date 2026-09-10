# SAURIA FX1 (v4.4 SoC / v4.2 NPU Core) — Milestone Report Part 12: Current Model Limitations, Assumptions & Known Issues

> **Document Title**: SAURIA FX1 Virtual Prototype Scope, Limitations & System Assumptions  
> **Document Identifier**: `SAURIA-DOC-MS4-2026-V4.4-P12`  
> **Document Version**: `4.4.0` (Final Release)  
> **Target Hardware**: SAURIA FX1 NPU Core (Dual-Lane $64 \times 64$ Rev2 Architecture / FX1 SoC Integration)  
> **Implementation**: SystemC 2.3.3 / IEEE 1666-2023 Compliant Cycle-Approximate Virtual Prototype (C++17)  
> **Authoring Body**: VP Team / SAURIA NPU Architecture & Systems Engineering  

---

# 12. Current Model Limitations, Assumptions & Known Issues

While the SAURIA FX1 v4.4 SystemC virtual prototype accurately captures cycle-approximate hardware behavior, software engineers and verification teams must evaluate simulation results within the context of specific model limitations and operating assumptions.

---

## 12.1 Virtual Prototype Model Scope & Limitations

### 1. Ideal AXI DRAM Memory Channel Modeling
The AXI DMA controller (`sauria_dma.h`) models 256-bit wide bus transfers using fixed 8-beat burst latencies (256 bytes per transaction). The virtual prototype assumes an ideal external DDR DRAM memory subsystem. It does not currently model physical DRAM refresh cycles, bank-conflict page-miss latencies, or multi-master bus arbitration contention from external SoC peripherals (such as host CPUs or display controllers).

### 2. Physical 16 KB Activation SRAM LUT Capacity
Non-linear activation functions (SiLU and GELU) are evaluated via a **16 KB physical SRAM Activation Lookup Table** embedded inside the OBP epilogue engine. The LUT is pre-populated for activation domains spanning $[-8.0, +8.0]$. Evaluating custom activation functions outside this input range requires pre-scaling adjustments in `tools/onnx_compiler.py` to prevent table lookup truncation.

### 3. Static Compiler Matrix Tiling
Matrix tile decomposition is performed statically at compile time by `tools/onnx_compiler.py`. The virtual prototype executes pre-tiled instruction sequences as dispatched by host software; it does not implement dynamic, out-of-order hardware tile re-scheduling at runtime.

### 4. Software-Aliased 2D Image Resizing Scope
In the current release, 2D spatial feature resizing—such as the 2x nearest-neighbor upsampling used in YOLOv8 FPN necks—is handled via zero-compute compiler address aliasing or software pre-processing. Integration of a dedicated hardware 2D bilinear/nearest-neighbor `Resize` engine into the physical RTL datapath is scheduled for Phase 2 development.

---

## 12.2 System-Level Operating Assumptions

* **Operating Frequency**: All baseline performance metrics are modeled assuming a target clock frequency $f_{\text{clk}} = 1.0\text{ GHz}$ ($T_{\text{clk}} = 1.0\text{ ns}$) or $800\text{ MHz}$ ($T_{\text{clk}} = 1.25\text{ ns}$).
* **Host Driver MMIO Polling Overhead**: Host driver interaction via MMIO register writes (`0x40000310`/`0x40000314`) is assumed to require $\le 5$ clock cycles per instruction submission.
* **Single-Cycle Local SRAM Read Access**: Memory feeder modules (`act_feeder` and `wei_feeder`) access SRAM Banks 0–3 with single-cycle read latency over parallel 256-bit buses.
