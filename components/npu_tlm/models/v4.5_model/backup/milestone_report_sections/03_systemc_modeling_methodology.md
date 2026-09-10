# SAURIA FX1 (v4.4 SoC / v4.2 NPU Core) — Milestone Report Part 3: SystemC Model Architecture & Modeling Methodology

> **Document Title**: SAURIA FX1 SystemC Modeling Architecture & Methodology  
> **Document Identifier**: `SAURIA-DOC-MS4-2026-V4.4-P03`  
> **Document Version**: `4.4.0` (Final Release)  
> **Target Hardware**: SAURIA FX1 NPU Core (Dual-Lane $64 \times 64$ Rev2 Architecture / FX1 SoC Integration)  
> **Implementation**: SystemC 2.3.3 / IEEE 1666-2023 Compliant Cycle-Approximate Virtual Prototype (C++17)  
> **Authoring Body**: VP Team / SAURIA NPU Architecture & Systems Engineering  

---

# 3. SystemC Model Architecture & Modeling Methodology

The SAURIA FX1 virtual prototype is constructed using **SystemC 2.3.3** in compliance with the **C++17 IEEE 1666-2023 standard**. The model functions as a cycle-approximate, transaction-level reference environment designed to capture hardware execution latencies, memory bus contention, and control state transitions with high fidelity prior to RTL synthesis.

By abstracting low-level wire signals into structured C++ data transactions while preserving clock-cycle precision, the SystemC model provides a fast, software-executable platform. It allows software engineers to develop bare-metal drivers and evaluate compiled deep learning graphs while giving hardware verification engineers a golden reference model for gate-level VCS RTL co-simulation.

---

## 3.1 Modeling Mechanics & SystemC Construct Implementation

The top-level `NpuTop` module and its underlying hardware sub-blocks leverage standard SystemC language constructs to model concurrent hardware execution, event synchronization, and signal clocking:

### 1. Structural Module Encapsulation (`SC_MODULE`)
Every hardware block within the NPU pipeline—including `InstructionDecoder`, `MainController`, `SystolicArray`, `DataFeeder`, `ObpTop`, `ReRce`, `SauriaDma`, and `SramTop`—is defined as an `SC_MODULE` class instance. Sub-modules expose explicit SystemC input and output ports (`sc_in`, `sc_out`, `sc_vector`), enforcing strict structural hierarchy and modular boundaries that correspond directly to physical RTL module partitions.

### 2. Concurrent Processing Threads (`SC_THREAD`)
Hardware finite state machines and streaming pipelines are implemented as persistent `SC_THREAD` process loops. For example, state machine transitions within `ctrl_inst_a` and `ctrl_inst_b` run inside infinite C++ loops (`while(true)`) sensitive to the positive edge of the global clock signal (`sensitive << i_clk.pos()`). This ensures that state updates, counter decrements, and control signal assertions execute synchronously on clock boundaries.

### 3. Non-Blocking Event Synchronization (`sc_event`)
Inter-module signaling and pipeline handshakes—such as DMA transfer completion notifications, OBP drain assertions, and barrier wait state synchronization—use SystemC `sc_event` primitives. Threads block execution by calling `wait(event)`, resuming immediately when the driving module triggers `event.notify()`. This event-driven mechanism eliminates polling overhead in simulation, allowing millions of clock cycles to simulate in seconds.

### 4. Precision Cycle Clocking (`sc_clock`)
The virtual prototype is driven by a global SystemC clock object (`sc_clock i_clk`). For standard baseline benchmarks, the clock period is set to $1.0\text{ ns}$, corresponding to an operating frequency $f_{\text{clk}} = 1.0\text{ GHz}$. For SOC integration profiling, the clock period is adjusted to $1.25\text{ ns}$ ($800\text{ MHz}$). All hardware wait delays (`wait(N)`) advance simulation time by exact integer multiples of the clock period.

---

## 3.2 Cycle-Approximate Timing & Latency Formulations

The virtual prototype accounts for hardware structural latencies across data feeding, matrix computation, accumulator scan-out, and memory writeback.

### Matrix Tile Execution Latency Formulation
For any matrix multiplication tile instruction processed by the systolic array, the overall tile execution duration $T_{\text{tile}}$ (measured in clock cycles) is formulated as:

$$T_{\text{tile}} = T_{\text{fill}} + T_{\text{comp}} + T_{\text{drain}} + T_{\text{cswitch}}$$

where each component models a specific hardware pipeline phase:
* **$T_{\text{fill}} = 3\text{ cycles}$**: Data Feeder pipeline fill latency required to align activation and weight vectors before entering the PE array.
* **$T_{\text{comp}} = K\text{ cycles}$**: Active systolic contraction cycles required to process $K$ reduction channel elements across the matrix tile.
* **$T_{\text{drain}} = X_{\text{DIM}} + Y_{\text{DIM}} + \text{PE\_LAT} + 8\text{ cycles}$**: Accumulator scan-out latency required to drain 32-bit partial sums from PE accumulators through the OBP epilogue pipeline.
* **$T_{\text{cswitch}} = X_{\text{DIM}} + Y_{\text{DIM}} + 16\text{ cycles}$**: Double-buffering context switch latency required to swap ping-pong SRAM banks and complete AXI DMA writeback alignment.

### Closed-Form Instruction Latency Calculations
Inside the instruction decoder (`InstructionDecoder`), state machine delays during the `COMPUTE_WAIT` state are calculated using closed-form analytical equations derived from physical hardware timing:

* **Opcode `0x12` (`GEMM_FUSED`)**:
  $$\text{Compute Cycles} = \frac{M \times K \times N}{32 \times 32} + 50$$
* **Opcode `0x13` (`FUSED_ATTN`)**:
  $$\text{Compute Cycles} = \frac{\text{SeqLen} \times \text{HeadDim} \times \text{SeqLen}}{32 \times 32} + \frac{\text{SeqLen} \times \text{SeqLen} \times \text{HeadDim}}{32 \times 32} + 100$$
* **Opcode `0x14` (`LAYERNORM`)**:
  $$\text{Compute Cycles} = \frac{\text{SeqLen} \times \text{Dim}}{32} + 30$$
* **Opcode `0x15` (`ELEM_WISE`)**:
  $$\text{Compute Cycles} = \frac{\text{Len}}{32} + 20$$

---

## 3.3 Memory Interfacing & Telemetry Instrumentation

### Zero-Latency Local Memory Pointer Interfacing
To achieve high simulation throughput during full-graph model execution, local SRAM banks utilize C++ pointer passing (`uint8_t*`) for data movement. Rather than simulating individual wire toggles for every byte transferred within local SRAM, data arrays are accessed directly via host pointers while charging the exact cycle latencies to SystemC wait timers. This zero-latency memory pointer abstraction maintains 100% timing accuracy while enabling full ViT-Base model verification (497 checkpoints) to run in less than two minutes.

### Non-Intrusive Telemetry Hooks (`perf_counters.h`)
The SystemC top-level module embeds non-intrusive performance hooks (`dut->attach_perf(&perf)`). These hooks monitor active PE cycles, MAC operations, AXI DMA bytes transferred, and engine active cycles in real time. Because profiler hook evaluation occurs in zero simulation time on clock boundaries, telemetry collection does not alter hardware execution timing or affect numerical outputs.
