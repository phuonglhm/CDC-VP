# SAURIA FX1 (v4.4 SoC / v4.2 NPU Core) — Milestone Report Part 4: Implemented Functional Features & Subsystem Specifications

> **Document Title**: SAURIA FX1 Functional Features, MMIO Map & Rich ISA Specification  
> **Document Identifier**: `SAURIA-DOC-MS4-2026-V4.4-P04`  
> **Document Version**: `4.4.0` (Final Release)  
> **Target Hardware**: SAURIA FX1 NPU Core (Dual-Lane $64 \times 64$ Rev2 Architecture / FX1 SoC Integration)  
> **Implementation**: SystemC 2.3.3 / IEEE 1666-2023 Compliant Cycle-Approximate Virtual Prototype (C++17)  
> **Authoring Body**: VP Team / SAURIA NPU Architecture & Systems Engineering  

---

# 4. Implemented Functional Features & Subsystem Specifications

The SAURIA FX1 hardware release introduces a rich set of architectural features designed to accelerate both vision and transformer workloads. The subsystem implementation encapsulates complete hardware capabilities, extending from host register configuration interfaces down to packed 64-bit instruction execution pipelines.

---

## 4.1 Implemented Functional Features & Hardware Capabilities

The table below summarizes the core functional capabilities implemented in the v4.4 milestone, detailing the underlying hardware mechanisms and architectural rationale for each feature:

| Feature Category | Implementation Status | Technical Capability & Microarchitectural Hardware Details |
| :--- | :---: | :--- |
| **Dual-Lane Processing Core** | **Production Ready** | Dynamic $N_{split}$ boundary partitioning across dual $32 \times 64$ PE grids. Supported by independent queue state machines (`state_a` and `state_b`) to eliminate inter-lane head-of-line blocking. |
| **64-Bit Rich ISA** | **Production Ready** | Five packed 64-bit opcodes (`0x05`, `0x12`, `0x13`, `0x14`, `0x15`). Packs multidimensional matrix sizes, scaling factors, activation types, and memory offsets directly into control words. |
| **4-Stage OBP Epilogue Engine** | **Production Ready** | Dedicated 4-stage SIMD post-processing pipeline per lane. Fuses 32-bit Bias Addition, Requantization Scale/Shift, 16 KB Activation SRAM LUT lookups (ReLU, SiLU, GELU), and Residual Skip Additions directly into the memory drain path. |
| **LayerNorm Acceleration Engine** | **Production Ready** | Dual 2-pass LayerNorm engines (`RCEA`/`REA` and `RCEB`/`REB`). Equipped with dedicated **24 KB Scratch SRAM buffers** (`ScratchA` in Bank 4, `ScratchB` in Bank 5) and RSQRT PWL LUT interpolation. |
| **Softmax Reduction Engine** | **Production Ready** | On-chip 2-pass multi-head attention Softmax reduction. Implements exponential (`LUT_exp`) and reciprocal (`LUT_recip`) PWL lookup tables for zero-CPU attention score normalization. |
| **SPPF Max-Pooling Engine** | **Production Ready** | 5x5 Spatial Pyramid Pooling Fast (SPPF) max-pooling. Reuses the 64-wide SIMD max-reduction comparator trees from the Reduction Engine with zero additional silicon area. |
| **AXI DMA Controller** | **Production Ready** | Autonomous 4-channel 256-bit AXI DMA engine (`CH0`–`CH3`) with Round-Robin burst scheduling, double-buffering ping-pong prefetching, and an independent 256-bit write master. |
| **Physical SRAM Banking** | **Production Ready** | Six physical memory banks (~2.1 MB total) with hardware-enforced transfer capacity clamping to protect local SRAM buffers against out-of-bounds DMA write overruns. |

---

## 4.2 Complete Host MMIO Register Map (`config_map.h`, `config_regs.h`)

Host software and bare-metal RISC-V CPU drivers interface with the SAURIA FX1 core by reading and writing to a 32-bit MMIO register map located in host memory space (`0x40000300` through `0x40000460`):

| Register Offset | Register Name | Bit Width / Type | Description & Functional Purpose |
| :--- | :--- | :---: | :--- |
| `0x40000014` | `F_NSPLIT` | 32-bit R/W | Sets the dynamic PE row split boundary $N_{split}$ ($0 \le N_{split} \le 64$). Determines array partition between Lane A (rows $0..N_{split}-1$) and Lane B (rows $N_{split}..63$). |
| `0x40000300` | `temp_word_a` | 32-bit WO | Temporary low 32-bit word buffer for legacy 64-bit Queue A instructions. |
| `0x40000304` | `trigger_legacy_a` | 32-bit WO | High 32-bit word write; triggers legacy Queue A instruction compilation and queue submission. |
| `0x40000308` | `temp_word_b` | 32-bit WO | Temporary low 32-bit word buffer for legacy 64-bit Queue B instructions. |
| `0x4000030C` | `trigger_legacy_b` | 32-bit WO | High 32-bit word write; triggers legacy Queue B instruction compilation and queue submission. |
| `0x40000310` | `trigger_rich_a` | 32-bit WO | Writing opcode value assembles Rich Instruction from current MMIO parameter registers and pushes descriptor to **Queue A**. |
| `0x40000314` | `trigger_rich_b` | 32-bit WO | Writing opcode value assembles Rich Instruction from current MMIO parameter registers and pushes descriptor to **Queue B**. |
| `0x40000400` | `r_in_addr` | 32-bit R/W | Activation input tensor DRAM base address offset. |
| `0x40000404` | `r_w_addr` | 32-bit R/W | Weight matrix DRAM base address offset. |
| `0x40000408` | `r_out_addr` | 32-bit R/W | Output destination DRAM base address offset. |
| `0x4000040C` | `r_bias_addr` | 32-bit R/W | Channel bias vector DRAM base address offset. |
| `0x40000410` | `r_m` | 32-bit R/W | GEMM dimension M (output feature rows / sequence tokens). |
| `0x40000414` | `r_k` | 32-bit R/W | GEMM dimension K (reduction channels / input feature size). |
| `0x40000418` | `r_n` | 32-bit R/W | GEMM dimension N (output feature channels). |
| `0x4000041C` | `r_kh` | 32-bit R/W | Convolution kernel height ($K_H$). |
| `0x40000420` | `r_kw` | 32-bit R/W | Convolution kernel width ($K_W$). |
| `0x40000424` | `r_stride` | 32-bit R/W | Downsampling stride (Convolution / MaxPool). |
| `0x40000428` | `r_pad` | 32-bit R/W | Zero-padding width. |
| `0x4000042C` | `r_act_type` | 32-bit R/W | Non-linear activation selection: `0`=None, `1`=ReLU, `2`=SiLU, `3`=GELU. |
| `0x40000430` | `r_has_skip` | 32-bit R/W | Residual skip connection enable: `0`=Disable, `1`=Enable. |
| `0x40000434` | `r_skip_addr` | 32-bit R/W | Residual skip matrix DRAM base address offset. |
| `0x40000438` | `r_in_scale` | Float32 R/W | Input activation quantization scale factor ($\text{Scale}_{\text{in}}$). |
| `0x4000043C` | `r_w_scale` | Float32 R/W | Weight matrix quantization scale factor ($\text{Scale}_{\text{w}}$). |
| `0x40000440` | `r_out_scale` | Float32 R/W | Output quantization scale factor ($\text{Scale}_{\text{out}}$). |
| `0x40000444` | `r_q_addr / r_gamma_addr / r_a_addr` | 32-bit R/W | Multi-purpose operand A (Attention Query Q / LayerNorm Gamma $\gamma$ / Elementwise Vector A DRAM base address). |
| `0x40000448` | `r_k_addr / r_b_addr` | 32-bit R/W | Multi-purpose operand B (Attention Key K / Elementwise Vector B DRAM base address). |
| `0x4000044C` | `r_v_addr / r_beta_addr` | 32-bit R/W | Multi-purpose operand C (Attention Value V / LayerNorm Beta $\beta$ DRAM base address). |
| `0x40000450` | `r_seq_len / r_len` | 32-bit R/W | Attention sequence length ($S$) / Flat elementwise vector length. |
| `0x40000454` | `r_num_heads / r_dim / r_mode` | 32-bit R/W | Attention head count ($h$) / LayerNorm features dimension ($D$) / Elementwise mode (`0`=ADD, `1`=MAX_POOL). |
| `0x40000458` | `r_head_dim / r_eps_shift / r_scale_a` | 32-bit/F R/W| Key-Query dimension per head ($d$) / LayerNorm $\epsilon$ shift / Elementwise scale A. |
| `0x4000045C` | `r_attn_scale / r_scale_b` | Float32 R/W | Attention scaling factor ($1/\sqrt{d}$) / Elementwise scale B. |
| `0x40000460` | `r_scale_out` | Float32 R/W | Elementwise output scaling factor. |

---

## 4.3 64-bit Rich ISA Opcode Specifications & Mathematical Definitions

The 64-bit Rich ISA compresses complex multi-tensor operations into five primary opcodes.

### Opcode `0x05`: `SET_NSPLIT` (Spatial Barrier Synchronization)
* **Instruction Layout**: Bits `[63:56]` encode opcode `0x05`; bits `[55:7]` are reserved; bits `[6:0]` specify the new `nsplit` row boundary ($0 \le nsplit \le 64$).
* **Hardware Execution Flow**: Upon dequeuing `0x05`, the decoder checks the barrier status flag:
  $$\text{sa\_busy} = \text{i\_ctrl\_active\_a} \lor \text{i\_ctrl\_active\_b} \lor (\text{sibling\_state} \neq \text{IDLE})$$
  If $\text{sa\_busy}$ is true, the decoder enters `WAIT_BARRIER` and stalls pipeline decoding. Once both controllers report idle, $N_{split}$ is updated safely without interrupting active matrix computations.

### Opcode `0x12`: `GEMM_FUSED` (Fused Matrix Multiplication & Convolution)
* **Instruction Layout**: Bits `[63:56]` encode opcode `0x12`; bits `[55:32]` encode weight base pointer (`w_addr >> 8`); bits `[31:16]` encode activation base pointer (`in_addr >> 8`); bits `[15:0]` encode output base pointer (`out_addr >> 8`).
* **Mathematical Definition**:
  $$\text{Out}_{r,c} = \text{Scale}_{\text{out}} \times \left( \text{Activation}\left( \text{Scale}_{\text{in}} \times \text{Scale}_{\text{w}} \sum_{i=0}^{K-1} (A_{r,i} \times B_{i,c}) + \text{Bias}_c \right) + \text{Skip}_{r,c} \right)$$
  Where the activation function is selected by MMIO register `r_act_type`:
  * **SiLU Activation (`r_act_type = 2`)**: $\text{SiLU}(x) = x \cdot \sigma(x) = \frac{x}{1 + e^{-x}}$
  * **GELU Activation (`r_act_type = 3`)**: $\text{GELU}(x) = x \cdot \Phi(x) \approx 0.5x \times \left( 1.0 + \tanh\left( \sqrt{\frac{2}{\pi}} \left( x + 0.044715 x^3 \right) \right) \right)$

### Opcode `0x13`: `FUSED_ATTN` (Multi-Head Self-Attention)
* **Parameters**: Base pointers `q_addr`, `k_addr`, `v_addr`; head count $h$; sequence length $S$; head dimension $d$; attention scale $\frac{1}{\sqrt{d}}$.
* **Mathematical Definition (per head)**:
  1. **QK Matrix Multiply & Scaling**:
     $$\text{QK}_{i,j} = \text{Scale}_{\text{attn}} \times \sum_{k=0}^{d-1} \left( Q_{i, h, k} \times K_{j, h, k} \right)$$
  2. **Row-Wise 2-Pass Softmax Normalization**:
     $$\text{SoftmaxRow}_{i,j} = \frac{e^{\text{QK}_{i,j} - \max_m(\text{QK}_{i,m})}}{\sum_{m} e^{\text{QK}_{i,m} - \max_p(\text{QK}_{i,p})}}$$
  3. **Context Matrix Multiply against Value V**:
     $$\text{Out}_{i, h, k} = \sum_{j=0}^{S-1} \left( \text{SoftmaxRow}_{i,j} \times V_{j, h, k} \right)$$

### Opcode `0x14`: `LAYERNORM` (Channel-Wise Layer Normalization)
* **Parameters**: Input pointer `in_addr`; scale pointer `gamma_addr`; shift pointer `beta_addr`; feature dimension $D$; sequence length $S$.
* **Mathematical Definition**:
  1. **Pass 1 (Mean & Variance Calculation in 24 KB Scratch)**:
     $$\mu_i = \frac{1}{D} \sum_{d=0}^{D-1} x_{i,d}, \quad \sigma^2_i = \frac{1}{D} \sum_{d=0}^{D-1} (x_{i,d} - \mu_i)^2$$
  2. **Pass 2 (RSQRT PWL Interpolation & Gamma/Beta Scaling)**:
     $$\text{Out}_{i,d} = \frac{x_{i,d} - \mu_i}{\sqrt{\sigma^2_i + \epsilon}} \times \gamma_d + \beta_d \quad (\text{where } \epsilon = 10^{-5})$$

### Opcode `0x15`: `ELEM_WISE` (Vector Addition & SPPF Max-Pooling)
* **Parameters**: Base pointers `a_addr`, `b_addr`; vector length `len`; operational mode `mode` (`0`=ADD, `1`=MAX_POOL); sliding stride `stride`.
* **ADD Mode (`mode = 0`)**:
  $$\text{Out}_i = \text{Scale}_{\text{out}} \times (\text{Scale}_A \cdot A_i + \text{Scale}_B \cdot B_i)$$
* **MAX_POOL Mode (`mode = 1`)**:
  $$\text{Out}_i = \max_{k \in \text{window}} \left( A_{i \cdot \text{stride} + k} \right)$$
  Calculated by reusing the shared 64-wide SIMD max-reduction comparator trees within the RCE/RE engine.
