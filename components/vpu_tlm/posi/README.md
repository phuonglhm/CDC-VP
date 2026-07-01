# POSI Block

## Overview

POSI stands for **Post-Intra Prediction**.

This block is the second stage of the intra prediction path in the VPU TLM model. POSI receives intra mode candidates from PREI and generates the actual intra prediction pixels.

This is a **functional TLM model**, not a cycle-accurate RTL model.

---

## Directory Structure

```text
posi/
├── include/
│   └── posi.h
├── src/
│   └── posi.cpp
├── test/
│   ├── CMakeLists.txt
│   └── test_posi.cpp
└── README.md
```

---

## Role in VPU Pipeline

POSI belongs to the intra prediction path:

```text
Input Frame
    ↓
PREI
    ↓
POSI
    ↓
Intra prediction_result
```

POSI generates the final intra prediction candidate.

---

## Main Responsibilities

```text
- Receive input frame
- Receive reconstructed reference frame
- Receive PREI result
- Select intra mode candidate
- Generate planar / DC / angular intra prediction
- Compute predicted luma pixels
- Compute residual = original - predicted
- Estimate rate
- Compute cost
- Output intra prediction_result
```

---

## Main Input

```text
frame input
frame reconstructed
block region
prei_result prei_info
qp
```

Meaning:

```text
input          current frame
reconstructed  reconstructed reference frame
region         current block region
prei_info      intra mode candidates from PREI
qp             quantization parameter
```

---

## Main Output

```text
prediction_result
```

The output has:

```text
mode = prediction_mode::intra
```

---

## Main API

```cpp
prediction_result run(const frame& input,
                      const block& region) const;

prediction_result run(const frame& input,
                      const frame& reconstructed,
                      const block& region,
                      const prei_result& prei_info,
                      std::uint32_t qp = INIT_QP) const;
```

---

## Simplified Functional Flow

```text
Input frame + reconstructed frame + PREI result
      ↓
Select intra mode candidate
      ↓
Generate intra prediction pixels
      ↓
Compute residual
      ↓
Estimate rate
      ↓
Compute cost
      ↓
Generate intra prediction_result
```

---

## Intra Prediction Modes

POSI can model simplified versions of HEVC intra prediction modes:

```text
- Planar mode
- DC mode
- Angular modes
```

The purpose is to generate a functional intra prediction result for mode decision or later pipeline stages.

---

## Relationship with PREI

```text
PREI
  ↓
prei_result
  ↓
POSI
  ↓
prediction_result with mode = intra
```

PREI searches or ranks intra modes.  
POSI uses those modes to generate actual predicted pixels and residuals.

---

## Test Description

Test file:

```text
posi/test/test_posi.cpp
```

The POSI unit test checks:

```text
- PREI result is valid
- POSI output is valid
- Output mode is intra
- predicted_luma size matches block area
- residual_luma size matches block area
```

---

## Build and Run Test

From repository root:

```bash
cd ~/CDC-VP
cmake --build build --target test_posi
```

Run:

```bash
./build/components/vpu_tlm/posi/test/test_posi
```

Expected result:

```text
POSI test PASSED
```

---

## Notes

POSI is currently modeled at functional level. It focuses on intra prediction behavior and does not model exact RTL timing or internal memory access cycles.
