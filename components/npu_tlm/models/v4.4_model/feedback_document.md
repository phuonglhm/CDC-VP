# SAURIA NPU v4.4 — Hardware Feedback & Architecture Specification Document

> **Document Name**: `feedback_document.md`  
> **Target System**: SAURIA NPU Core v4.4 SystemC Cycle-Accurate Model  
---

## 1. Executive Summary & Architecture Overview

The **SAURIA NPU v4.4** is a dual-lane, profile-aware neural processing engine optimized for edge-AI vision transformers (ViT), object detection (YOLOv8), and deep convolutional networks. The architecture comprises symmetric execution lanes (**Lane A** and **Lane B**), a **64-bit Rich Instruction Set Architecture (ISA)**, a 4-stage **Output Post-Processing Block (OBP)** featuring 16 KB activation SRAM LUTs, a **Reconfigurable Compute Engine (RCE)** for non-linear operations, and a 4-channel AXI DMA controller.

```
                                  +-------------------------------------------------------+
                                  |                    HOST INTERFACE                     |
                                  |   (AXI-Lite MMIO CSRs: 0x40000400 | INST_LO / HI)    |
                                  +---------------------------+---------------------------+
                                                              |
                                           +------------------v------------------+
                                           |      SAURIA CONTROL & DECODER       |
                                           |  (Dual Instruction Queues A & B)    |
                                           +--------+-------------------+--------+
                                                    |                   |
                                   +----------------v---+       +-------v------------+
                                   |  LANE A SCHEDULER  |       |  LANE B SCHEDULER  |
                                   +--------+-----------+       +-------+------------+
                                            |                           |
                               +------------v------------+ +------------v------------+
                               |     DATA FEEDER A       | |     DATA FEEDER B       |
                               | (Act/Wei SRAM Regions)  | | (Act/Wei SRAM Regions)  |
                               +------------+------------+ +------------+------------+
                                            |                           |
                               +------------v------------+ +------------v------------+
                               |   SYSTOLIC ARRAY A      | |   SYSTOLIC ARRAY B      |
                               |    (64x64 PE Grid)      | |    (64x64 PE Grid)      |
                               +------------+------------+ +------------+------------+
                                            |                           |
                               +------------v------------+ +------------v------------+
                               |        OBP TOP A        | |        OBP TOP B        |
                               | (Epilogue/LUT/Requant)  | | (Epilogue/LUT/Requant)  |
                               +------------+------------+ +------------+------------+
                                            |                           |
                                            +------------+--------------+
                                                         |
                                  +----------------------v--------------------------------+
                                  |            AXI DMA ENGINE (4 Channels)                |
                                  |     (Multi-Gigabyte DRAM / DDR Interfacing)           |
                                  +-------------------------------------------------------+
```

---

## 2. Comprehensive Register Map & Audit (`v4.4_model`)

Below is the detailed specification and hardware audit for the control, queue, interrupt, activation LUT, and DMA performance registers in the current `v4.4_model`:

| Register Symbol | Address / Offset | Implementation Status | Description & Hardware Mapping |
| :--- | :--- | :---: | :--- |
| **`FX1_QUEUE_A_PUSH`** | `0x40000310` | **EXISTS** | Host MMIO WO register to assemble & push a 64-bit rich instruction to **Queue A** (Lane A). |
| **`FX1_QUEUE_B_PUSH`** | `0x40000314` | **EXISTS** | Host MMIO WO register to assemble & push a 64-bit rich instruction to **Queue B** (Lane B). |
| **`FX1_QUEUE_A_STATUS`** | N/A | *Internal C++ Queue* | Queue depth/fullness is managed in `control/instruction_decoder.h` (`std::queue<SauriaRichInstruction> queue_a`). Not exposed as a host MMIO readback CSR. |
| **`FX1_QUEUE_B_STATUS`** | N/A | *Internal C++ Queue* | Queue depth/fullness is managed in `control/instruction_decoder.h` (`std::queue<SauriaRichInstruction> queue_b`). Not exposed as a host MMIO readback CSR. |
| **`FX1_NSPLIT`** | `0x40000214` | **EXISTS (Default: 32)** | Host R/W MMIO register (`CFG_CON_OFFSET + 0x14`). **Default value is `32`** (`Y_DIM / 2` for default $64 \times 64$ PE geometry), assigning **32 rows to Lane A** ($0 \dots 31$) and **32 rows to Lane B** ($32 \dots 63$). Can also be modified dynamically via ISA Opcode `0x05` (`SET_NSPLIT`). |
| **`FX1_LUT_A_BASE`** | `0x40140000` | **EXISTS** | Host R/W MMIO base address for **Lane A OBP Activation LUT** (16 KB SRAM, $64 \times 256$ entries). |
| **`FX1_LUT_B_BASE`** | `0x40180000` | **EXISTS** | Host R/W MMIO base address for **Lane B OBP Activation LUT** (`LUT_OFFSET + 0x00040000`, 16 KB SRAM, $64 \times 256$ entries). |
| **`FX1_DMA_BYTES_READ`**| N/A | *Perf Counter Field* | Measured dynamically by the 60-metric instrumentation engine (`fx1::PerfCounters::ddr_read_bytes` in `instrumentation/perf_counters.h`). Reported in simulation performance logs. |
| **`FX1_DMA_BYTES_WRITTEN`**| N/A | *Perf Counter Field* | Measured dynamically by the 60-metric instrumentation engine (`fx1::PerfCounters::ddr_write_bytes` in `instrumentation/perf_counters.h`). Reported in simulation performance logs. |

---

## 3. Dual-Lane Partitioning & `FX1_NSPLIT` Register Details

The **`FX1_NSPLIT`** register (MMIO address `0x40000214`) controls how the $Y\_DIM \times X\_DIM$ systolic PE array grid is partitioned across Lane A and Lane B.

### 3.1. Default Value & Partition Mapping ($64 \times 64$ Geometry)
* **Default Value**: **`32`** ($\text{NSPLIT} = \text{Y\_DIM} / 2 = 64 / 2 = 32$).
* **Lane Row Partitioning**:
  * **Lane A Execution Scope**: Rows $0 \dots (\text{NSPLIT} - 1)$ $\rightarrow$ **Rows 0 to 31** (**32 rows total**).
  * **Lane B Execution Scope**: Rows $\text{NSPLIT} \dots (\text{Y\_DIM} - 1)$ $\rightarrow$ **Rows 32 to 63** (**32 rows total**).

### 3.2. C++ Source Initialization & Decoding
In `config_regs.h:L154`:
```cpp
uint32_t r_nsplit{Y_DIM / 2}; // Evaluates to 32 for Y_DIM = 64
```

In `control/instruction_decoder.h:L182,L361`:
```cpp
uint32_t r_nsplit{32}; // Default split (32 rows Lane A, 32 rows Lane B)
```

In `npu_top.h:L1389-L1395`:
```cpp
uint32_t nsplit = s_nsplit.read();
if (nsplit == 0) {
    // Lane B idle / power-gated (100% workload processed by Lane A)
} else if (nsplit < Y_DIM) {
    // Dual-lane active: Lane A processes rows [0, nsplit-1], Lane B processes rows [nsplit, Y_DIM-1]
}
```

---

## 4. AXI DMA Controller Specification (Software User Guide)

The **SAURIA AXI DMA Engine** (`control/sauria_dma.h`) provides high-speed, 4-channel burst data transfers between system DRAM and local SRAM scratchpad banks.

```
                                SYSTEM DRAM
                                     |
                +--------------------+--------------------+
                |                                         |
     AXI Shared Read Port                      AXI Dedicated Write Port
    (256-bit, Round-Robin)                      (256-bit, Independent)
                |                                         |
   +----+----+----+----+                        +---------+---------+
   | CH0| CH1| CH2| CH3|                        |   Write Controller|
   +--+-+--+-+--+-+--+-+                        +----+--------------+
      |    |    |    |                               |
      v    v    v    v                               v
    Bank0 Bank1 Bank2 Bank3                      Bank4 / Bank5
    (WeiA)(WeiB)(IFmA)(IFmB)                     (Output Memory)
```

### 4.1. Hardware Bus & Burst Performance Parameters
* **Data Bus Width**: **256 bits** ($32 \text{ bytes per clock cycle}$).
* **AXI Burst Length**: **8 beats/burst** ($8 \times 32\text{B} = 256 \text{ bytes per transfer burst}$).
* **Burst Timing Latency**: Exactly **8 clock cycles per burst**.
* **Dual Master Port Architecture**:
  * **AXI Read Master Port**: Shared by channels **CH0–CH3** via Round-Robin arbitration.
  * **AXI Write Master Port**: Independent dedicated write channel operating concurrently with zero read/write bus contention.

### 4.2. DMA Channel Destination Mapping
| Channel | Direction | Priority | Target SRAM Bank | Data Type / Operational Purpose |
| :---: | :---: | :---: | :---: | :--- |
| **`CH0`** | Read (DRAM $\rightarrow$ SRAM) | High | **Bank 0** | Weight Operand A Prefetch (Lane A Systolic Weights) |
| **`CH1`** | Read (DRAM $\rightarrow$ SRAM) | High | **Bank 1** | Weight Operand B Prefetch (Lane B Systolic Weights) |
| **`CH2`** | Read (DRAM $\rightarrow$ SRAM) | High | **Bank 2** | Activation / IFMap Operand A Prefetch (Lane A Input Features) |
| **`CH3`** | Read (DRAM $\rightarrow$ SRAM) | Medium | **Bank 3** | Activation / IFMap Operand B Prefetch (Lane B Input Features) |
| **`Write`** | Write (SRAM $\rightarrow$ DRAM) | Medium | **Bank 4 / 5** | Output Memory Flush (Post-Processed Activations to DRAM) |

### 4.3. Software Control API (`sauria_dma.h`)
Software triggers DMA operations asynchronously through C++ member functions:
```cpp
// 1. Initiate DRAM -> SRAM Read Transfer on channel 0-3
void start_read(int ch_id, uint32_t dram_addr, int bank_id, uint32_t bank_offset, uint32_t size_bytes);

// 2. Initiate SRAM -> DRAM Writeback Transfer
void start_write(uint32_t dram_addr, int bank_id, uint32_t bank_offset, uint32_t size_bytes);

// 3. Status Query Functions
bool is_read_active(int ch_id) const;   // Check if channel ch_id is busy
bool is_write_active() const;          // Check if write engine is busy
bool is_any_read_active() const;       // Check if any read channel is busy
```

---

## 5. OBP Activation LUT Specification (Lane A & Lane B)

The **Output Post-Processing Block (OBP)** in `psm/obp_top.h` implements a 4-stage post-processing epilogue pipeline. **Stage 3** houses a 64-lane parallel SRAM Activation LUT.

```
                              STAGE 3: ACTIVATION LUT (OBP)
+-----------------------------------------------------------------------------------------+
|                                                                                         |
|   Requantized INT8 Input (v) ---> Clamp [-128, 127] ---> Index = uint8(v + 128) in [0,255]|
|                                                                 |                       |
|                                                       +---------v--------+              |
|                                                       |  16 KB SRAM LUT  |              |
|                                                       | (64x256 Entries) |              |
|                                                       +---------+--------+              |
|                                                                 |                       |
|   Activated Output <--- int8_t(lut_val) <-----------------------+                       |
+-----------------------------------------------------------------------------------------+
```

### 5.1. Memory Geometry & Base Addresses
* **Lane A LUT Base Address (`FX1_LUT_A_BASE`)**: `0x40140000` (Local MMIO Offset: `0x00140000`).
* **Lane B LUT Base Address (`FX1_LUT_B_BASE`)**: `0x40180000` (Local MMIO Offset: `0x00180000` = `LUT_OFFSET + 0x00040000`).
* **Capacity & Structure**: **16 KB (16,384 bytes)** per lane.
  * $64 \text{ parallel PE lanes} \times 256 \text{ entries/lane} \times 1 \text{ byte/entry}$.
  * Declared in `psm/obp_top.h:L89` as `uint8_t lut_ram[Y_DIM][256];` (where `Y_DIM = 64`).

### 5.2. Index Clamping & Lookup Formula
Signed 8-bit quantization values $v \in [-128, 127]$ are clamped and mapped to an unsigned 8-bit SRAM index $\in [0, 255]$:
$$\text{index} = \text{uint8}\big(\text{clamp}_{\text{int8}}(v) + 128\big)$$

```cpp
// Implementation from psm/obp_top.h:L346-L357
for (int l = 0; l < Y_DIM; l++)
{
    T_PSUM val = stage2_reg.requant_data[l];
    if (lut_en)
    {
        int32_t val_int = static_cast<int32_t>(clamp_val<T_ACT>(val));
        uint8_t index = static_cast<uint8_t>(val_int + 128);
        uint8_t lut_val = lut_ram[l][index];
        next_stage3.activated_data[l] = static_cast<T_PSUM>(static_cast<int8_t>(lut_val));
    }
    else
    {
        next_stage3.activated_data[l] = val;
    }
}
```

### 5.3. Supported Activation Functions
| `act_type` | Mode | Mathematical Function | Description |
| :---: | :--- | :--- | :--- |
| `0` | **Bypass / None** | $f(x) = x$ | Linear pass-through |
| `1` | **ReLU** | $f(x) = \max(0, x)$ | Standard Rectified Linear Unit |
| `2` | **Sigmoid / SiLU** | $f(x) = x \cdot \sigma(x) = \frac{x}{1 + e^{-x}}$ | Sigmoid-Weighted Linear Unit |
| `3` | **GELU** | $f(x) = 0.5x \cdot \left(1 + \text{tanh}\left(\sqrt{\frac{2}{\pi}}(x + 0.044715x^3)\right)\right)$ | Gaussian Error Linear Unit |

---

## 6. 64-bit Rich Instruction Format (`INST_LO` / `INST_HI`)

Rich instructions are formatted as 64-bit words submitted via MMIO configuration registers into `control/instruction_decoder.h`.

### 6.1. Bitfield Layout
```
+-------------------+-------------------+-------------------+-------------------+
|   w_addr [63:32]  |  in_addr [31:16]  |    flags [15:8]   |   opcode [7:0]    |
+-------------------+-------------------+-------------------+-------------------+
```

### 6.2. MMIO Interface Registers
| CSR Name | Absolute Address | Local Offset | Access | Functional Description |
| :--- | :--- | :--- | :---: | :--- |
| `FX1_INST_LO` | `0x40000300` | `0x00300` | WO | Low 32 bits (`[31:0]`: `opcode`, `flags`, `in_addr`) |
| `FX1_INST_HI` | `0x40000304` | `0x00304` | WO | High 32 bits (`[63:32]`: `w_addr`). Writing triggers decode. |
| `FX1_QUEUE_A_PUSH` | `0x40000310` | `0x00310` | WO | Assembles instruction context & pushes to **Queue A** (Lane A) |
| `FX1_QUEUE_B_PUSH` | `0x40000314` | `0x00314` | WO | Assembles instruction context & pushes to **Queue B** (Lane B) |

### 6.3. Rich ISA Opcodes
```
+--------+------------------+-------------------------------------------------------------------+
| Opcode | Mnemonic         | Execution & Epilogue Pipeline                             |
+--------+------------------+-------------------------------------------------------------------+
| 0x05   | SET_NSPLIT       | Sets dual-lane partition barrier count (N_split)                  |
| 0x12   | GEMM_FUSED       | Y = Act(Requant(X * W + Bias)) + Residual                         |
| 0x13   | FUSED_ATTN       | Y = Softmax((Q * K^T) / sqrt(d_k)) * V                            |
| 0x14   | LAYERNORM        | Y = gamma * ((X - mean) / sqrt(var + eps)) + beta                 |
| 0x15   | ELEM_WISE        | Vector Arithmetic (Mode 0:ADD, 1:MAXPOOL, 2:MUL, 3:SUB, 4:DIV)    |
+--------+------------------+-------------------------------------------------------------------+
```

---

## 7. `FX1_RICH_HEADS_DIM_MODE` Register Specification

Mapped at MMIO Address **`0x40000454`** (Offset `0x00454`) in `control/instruction_decoder.h:L235-L242`.

### 7.1. Packed Bitfield Structure
```
+-------------------+-------------------+---------------------------------------+
|   mode [31:24]    |    dim [23:16]    |           num_heads [15:0]            |
+-------------------+-------------------+---------------------------------------+
| 8-bit Op Mode     | 8-bit Channel Dim | 16-bit Attention Head Count           |
+-------------------+-------------------+---------------------------------------+
```

### 7.2. Bitfield Description
1. **Bits `[15:0]` (`num_heads`, 16 bits)**:
   * Number of self-attention heads (e.g., `12` for ViT-Base).
2. **Bits `[23:16]` (`dim`, 8 bits)**:
   * Head dimension or channel dimension (e.g., `64` for $768 / 12$, or $768$ / $3072$ for 1D channel bias wrapping).
3. **Bits `[31:24]` (`mode`, 8 bits)**:
   * Operational mode for `ELEM_WISE` (`opcode == 0x15`):
     * `0`: **ADD** ($A + B$)
     * `1`: **MAX_POOL**
     * `2`: **MUL** ($A \times B$)
     * `3`: **SUB** ($A - B$)
     * `4`: **DIV** ($A / B$)

### 7.3. C++ Hardware Decoding Implementation
```cpp
// Decoding logic from control/instruction_decoder.h:L235-L242
else if (addr == 0x40000454) {
    if ((data & 0xFFFF0000) != 0) {
        r_num_heads = data & 0xFFFF;       // Bits [15:0]
        r_dim       = (data >> 16) & 0xFF; // Bits [23:16]
        r_mode      = (data >> 24) & 0xFF; // Bits [31:24]
    } else {
        r_num_heads = data; r_dim = data; r_mode = data; // Backward compatibility fallback
    }
}
```

---

## 8. Reconfigurable Compute Engine (RCE) & Reduction Engine (RE)

Defined in `psm/re_rce.h`. The RCE manages non-linear lookups (`exp`, `recip`, `rsqrt`) while the RE coordinates reduction operations.

### 8.1. RCE Non-linear Lookup Opcodes (`rce_lut_op_t`)
* `LUT_OP_EXP = 0` ($e^x$ lookup for Softmax Pass 2).
* `LUT_OP_RECIP = 1` ($\frac{1}{x}$ reciprocal lookup for Softmax division).
* `LUT_OP_RSQRT = 2` ($\frac{1}{\sqrt{x}}$ inverse square root lookup for LayerNorm variance scaling).

### 8.2. PWL Interpolation & Mathematical Fallback
```cpp
// Implementation from psm/re_rce.h:L82-L135
float lookup_exp(float x) {
    if (!lut_programmed) return std::exp(x); // Unprogrammed LUT fallback
    // Piecewise Linear (PWL) Interpolation:
    int idx = std::clamp(static_cast<int>((x - min_val) / step), 0, LUT_SIZE - 2);
    float frac = (x - (min_val + idx * step)) / step;
    return lut_table[idx] + frac * (lut_table[idx + 1] - lut_table[idx]);
}
```

---

## 9. ViT Model Verification Summary (`vit_b-int8.onnx`)

### 9.1. Pass Rate Breakdown (350 / 497 Checkpoints PASS)
* **MatMul / Linear Layers**: **120 / 120 (100% PASS)** — $\text{MAE} = 0.0000$, $\text{Cos Sim} = 1.000000$.
* **Softmax & Vector Div Layers**: **40 / 40 (100% PASS)** — $\text{Cos Sim} \ge 0.9758$.
* **Small Tensor Layers ($\le 16$ KB)**: **190 / 190 (100% PASS)** — $\text{MAE} = 0.0000$.
* **Full Sequence Tensor Additions**: $147$ checkpoints require multi-tile buffer streaming past local SRAM 280 KB scratch limits.

### 9.2. Recent Hardware & Model Fixes Applied
1. **Dual-Queue DMA Bank Assignment**: Corrected Queue B DMA read destination (`m_dma->start_read(2, ...)` for Operand A and `m_dma->start_read(3, ...)` for Operand B) in `control/instruction_decoder.h:L719-L727`.
2. **1D Channel Bias Vector Broadcasting**: Added channel dimension wrapping ($D=768 / D=3072$) with address thresholding (`addr < 0x00180000`) to separate 1D bias vectors from 2D sequence tensors.
3. **Scale Parameter Sanitization**: Normalized uninitialized floating-point scales (`scale_a`, `scale_b`, `scale_out`) in `emulate_elem_wise` to `1.0`.
4. **PWL Interpolation**: Added PWL linear interpolation and mathematical fallbacks in `psm/re_rce.h`.

---

> **Author**: VP Team  
