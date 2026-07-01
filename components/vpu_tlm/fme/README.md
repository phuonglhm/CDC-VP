# FME Block

## Overview

FME stands for **Fractional Motion Estimation**.

This block is the second stage of the inter prediction path in the VPU TLM model. FME refines the integer motion vector from IME using half-pixel and quarter-pixel candidates.

This is a **functional TLM model**, not a cycle-accurate RTL model.

---

## Directory Structure

```text
fme/
├── include/
│   ├── fme.h
│   └── fme_result.h
├── src/
│   └── fme.cpp
├── test/
│   ├── CMakeLists.txt
│   └── test_fme.cpp
└── README.md
```

---

## Role in VPU Pipeline

FME belongs to the inter prediction path:

```text
Current Frame + Reference Frame
        ↓
       IME
        ↓
       FME
        ↓
Inter prediction_result
```

FME receives the integer motion vector from IME and refines it to fractional-pixel precision.

---

## Main Responsibilities

```text
- Receive IME result
- Use IME integer MV as the search center
- Generate half-pel motion vector candidates
- Generate quarter-pel motion vector candidates
- Interpolate reference pixels
- Generate fractional-pixel prediction
- Compute residual = original - predicted
- Compute SATD
- Estimate motion vector coding rate
- Compute RD-like cost
- Select best refined fractional MV
- Optionally detect skip candidate
- Output fme_result
```

---

## Main Input

```text
frame input
frame reference
ime_result ime_info
qp
```

Meaning:

```text
input      current frame
reference  reference frame
ime_info   integer motion estimation result
qp         quantization parameter
```

---

## Main Output

```text
fme_result
```

Important fields:

```text
valid
ctu
qp
best_partition
best_mv
best_satd
best_rate
best_cost
skip
candidates
best_inter_result
```

The final inter prediction candidate is:

```cpp
fme_result.best_inter_result
```

---

## Main API

```cpp
fme_result run(const frame& input,
               const frame& reference,
               const ime_result& ime_info,
               std::uint32_t qp = INIT_QP) const;

fme_result run(const frame& input,
               const frame& reference,
               const ime_result& ime_info,
               const fme_refine_config& config,
               std::uint32_t qp = INIT_QP) const;
```

---

## Simplified Functional Flow

```text
IME integer MV
      ↓
Half-pel search around IME MV
      ↓
Quarter-pel search around best half-pel MV
      ↓
Generate fractional prediction
      ↓
Compute residual
      ↓
Compute SATD
      ↓
Compute cost = SATD + lambda * MV bit cost
      ↓
Select best fractional MV
      ↓
Optional skip decision
      ↓
Generate fme_result
```

---

## Motion Vector Unit

Motion vectors are stored in quarter-pel units.

Examples:

```text
mv.x = 4   means +1 integer pixel
mv.x = 2   means +1/2 pixel
mv.x = 1   means +1/4 pixel
mv.x = -1  means -1/4 pixel
```

FME may generate motion vectors that are not multiples of 4 because it searches fractional-pixel positions.

---

## Relationship with IME

```text
IME result
    ↓
Integer MV
    ↓
FME refinement
    ↓
Fractional MV
```

IME provides the initial integer-pixel motion vector.  
FME refines it using half-pixel and quarter-pixel interpolation.

---

## Test Description

Test file:

```text
fme/test/test_fme.cpp
```

The FME unit test first runs IME, then passes the IME result into FME.

The test checks:

```text
- IME output is valid
- FME output is valid
- FME best_inter_result is valid
- Output mode is inter
- predicted_luma size matches block area
- Refined MV is near zero for identical frames
```

---

## Build and Run Test

From repository root:

```bash
cd ~/CDC-VP
cmake --build build --target test_fme
```

Run:

```bash
./build/components/vpu_tlm/fme/test/test_fme
```

Expected result:

```text
FME test PASSED
```

---

## Notes

FME is currently modeled at functional level. It preserves the main RTL-level algorithmic behavior such as half-pel search, quarter-pel search, interpolation, SATD cost, MV rate cost, and best candidate selection. It does not model exact RTL pipeline timing or SRAM access cycles.
