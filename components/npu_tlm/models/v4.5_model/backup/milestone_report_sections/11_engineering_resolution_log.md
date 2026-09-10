# SAURIA FX1 (v4.4 SoC / v4.2 NPU Core) — Milestone Report Part 11: Engineering Resolution Log & Root-Cause Defect Analyses

> **Document Title**: SAURIA FX1 Critical Defect Resolution Log & Root-Cause Analyses  
> **Document Identifier**: `SAURIA-DOC-MS4-2026-V4.4-P11`  
> **Document Version**: `4.4.0` (Final Release)  
> **Target Hardware**: SAURIA FX1 NPU Core (Dual-Lane $64 \times 64$ Rev2 Architecture / FX1 SoC Integration)  
> **Implementation**: SystemC 2.3.3 / IEEE 1666-2023 Compliant Cycle-Approximate Virtual Prototype (C++17)  
> **Authoring Body**: VP Team / SAURIA NPU Architecture & Systems Engineering  

---

# 11. Engineering Resolution Log & Root-Cause Defect Analyses

During the v4.4 milestone development phase, comprehensive verification uncovered four major hardware and software defects. Each defect was isolated, diagnosed to its root cause, and resolved with architectural fixes.

---

## 11.1 Critical Defect Resolution Log

The table below summarizes the four major engineering defects resolved during the milestone cycle:

| Bug ID | Subsystem / Location | Symptoms & Failure Mode | Root Cause Analysis | Engineering Fix & Resolution | Verification Status |
| :---: | :--- | :--- | :--- | :--- | :---: |
| **BUG-01** | `ReRce` Subsystem (Lane B) | LayerNorm output tensors on Lane B corrupted with garbage values during dual-lane execution. | Lane B non-linear engine (`RCEB`/`REB`) was erroneously reading reduction statistics from Lane A's ScratchA buffer (Bank 4) instead of ScratchB (Bank 5). | Created dedicated 24 KB ScratchB buffer in SRAM Bank 5 and decoupled `RCEB`/`REB` memory pointer routing. | **RESOLVED** (0/256 Mismatches in `ST06`) |
| **BUG-02** | `InstructionDecoder` | Queue A decoder stalled indefinitely when issuing back-to-back `SET_NSPLIT` opcodes (`0x05`). | Head-of-line blocking: Decoder checked `sa_busy` without verifying if sibling Queue B had transitioned to `IDLE` state. | Added explicit `WAIT_BARRIER` state to FSM, stalling decoder until both lanes report inactive. | **RESOLVED** (Verified in `TC08` & `ST04`) |
| **BUG-03** | `ObpTop` Epilogue Engine | Cumulative quantization scale drift on signed INT8 GEMM outputs. | Double-precision floating-point accumulation allowed out-of-range intermediate products without saturation. | Enforced pure signed 32-bit integer accumulation with 4-stage SIMD fixed-point scaling and $[-128, +127]$ clamping. | **RESOLVED** (100% Bit-Exact in YOLO & ViT) |
| **BUG-04** | `SauriaDma` Controller | Memory access violation simulation crashes during large ViT sequence transfers. | DMA controller lacked physical transfer size boundary checks, overflowing local 50 KB Bank 4/5 buffers into DRAM. | Implemented hardware capacity memory clamping in `sauria_dma.h` across all 6 physical SRAM banks. | **RESOLVED** (Clean 497/497 ViT Pass) |

---

## 11.2 Detailed Technical Root-Cause Case Studies

### Case Study 1: LayerNorm Lane B Scratch Buffer Collision (BUG-01)
During initial dual-lane execution tests, executing `LAYERNORM` instructions (`0x14`) on Lane B resulted in severe numerical corruption, whereas Lane A executed with zero errors.

A deep investigation into `psm/re_rce.h` revealed that the reduction engine `re_inst_b` was hardcoded to read mean ($\mu$) and variance ($\sigma^2$) reduction statistics from `ScratchA` located in SRAM Bank 4. When Lane A and Lane B executed LayerNorm instructions concurrently, `re_inst_b` overwritten Lane A's scratch statistics or read stale data from Lane A's previous tile.

To fix this defect, the engineering team provisioned a separate **24 KB ScratchB memory buffer** in SRAM Bank 5 (`sram_top.h`). Pointer routing inside `re_rce.h` was updated so that `re_inst_a` accesses Bank 4 (`ScratchA`) and `re_inst_b` accesses Bank 5 (`ScratchB`) exclusively. Re-running the standalone microbenchmark (`ST06`) confirmed 100% bit-exact symmetry with 0 mismatches across 256 random test vectors.

### Case Study 2: Head-of-Line Blocking on Spatial Barrier Synchronization (BUG-02)
Submitting a `SET_NSPLIT` opcode (`0x05`) to Queue A while Queue B was actively processing an extended matrix multiplication instruction caused the instruction decoder to freeze permanently.

Root cause analysis of `control/instruction_decoder.h` showed that the decoder attempted to update configuration register `r_nsplit` immediately without verifying whether Systolic Array Lane B was still processing tiles. This created an invalid hardware state where PE array row steering multiplexers changed mid-execution, causing the FSM in `ctrl_inst_b` to hang while waiting for partial sum drains.

The defect was resolved by introducing an explicit `WAIT_BARRIER` state in `InstructionDecoder.h`. When `SET_NSPLIT` is dequeued, the decoder evaluates $\text{sa\_busy} = \text{i\_ctrl\_active\_a} \lor \text{i\_ctrl\_active\_b} \lor (\text{sibling\_state} \neq \text{IDLE})$. If $\text{sa\_busy}$ is true, the decoder stalls in `WAIT_BARRIER` until both controllers report idle. Once both lanes transition to `IDLE`, $N_{split}$ is updated safely without interrupting active compute operations.

### Case Study 3: Quantization Scale Precision Drift (BUG-03)
During initial full-graph ViT-Base evaluation, output tensors exhibited minor numerical discrepancies compared to golden fixed-point hardware expectations.

The root cause was traced to `ObpTop` epilogue emulation, which utilized double-precision floating-point arithmetic (`double`) for intermediate scaling. Floating-point accumulation permitted out-of-range intermediate values without intermediate rounding or integer saturation.

The epilogue pipeline was refactored to enforce **pure signed 32-bit integer accumulation, 32-bit fixed-point scale multiplication, right-shift arithmetic scaling, and $[-128, +127]$ saturation clamping**. This architectural change eliminated quantization drift, delivering 100% bit-exact agreement ($\text{MAE} = 0.000000$) across all 758 model checkpoints.
