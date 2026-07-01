# IME Block

## Overview

IME stands for **Integer Motion Estimation**.

This block is the first stage of the inter prediction path in the VPU TLM model. IME searches for the best integer-pixel motion vector between the current frame and the reference frame.

This is a **functional TLM model**, not a cycle-accurate RTL model.

---

## Directory Structure

```text
ime/
├── include/
│   ├── ime.h
│   └── ime_result.h
├── src/
│   └── ime.cpp
├── test/
│   ├── CMakeLists.txt
│   └── test_ime.cpp
└── README.md
```

---

## Role in VPU Pipeline

IME belongs to the inter prediction path:

```text
Current Frame + Reference Frame
        ↓
       IME
        ↓
       FME
        ↓
Inter prediction_result
```

IME generates the best integer motion vector.  
FME later refines this result to half-pixel and quarter-pixel accuracy.

---

## Main Responsibilities

```text
- Receive current frame and reference frame
- Receive CTU/block region
- Generate integer-pixel search points
- Fetch current block pixels
- Fetch reference block pixels
- Compute SAD for each motion vector candidate
- Estimate motion vector coding rate
- Compute RD-like cost
- Select best integer motion vector
- Select best inter partition
- Output ime_result for FME
```

---

## Main Input

```text
frame input
frame reference
block ctu
qp
```

Meaning:

```text
input      current frame
reference  reference frame
ctu        current block region
qp         quantization parameter
```

---

## Main Output

```text
ime_result
```

Important fields:

```text
valid
ctu
qp
best_partition
best_mv
best_sad
best_rate
best_cost
candidates
best_inter_result
```

---

## Main API

```cpp
ime_result run(const frame& input,
               const frame& reference,
               const block& ctu,
               std::uint32_t qp = INIT_QP) const;

ime_result run(const frame& input,
               const frame& reference,
               const block& ctu,
               const ime_search_config& config,
               std::uint32_t qp = INIT_QP) const;
```

---

## Simplified Functional Flow

```text
Current frame + reference frame + CTU
      ↓
Build partition list
      ↓
Build search window
      ↓
Generate integer MV candidates
      ↓
For each candidate MV:
    - Read current block
    - Read reference block
    - Compute SAD
    - Estimate MV rate
    - Compute cost
      ↓
Select best integer MV
      ↓
Generate ime_result
```

---

## Motion Vector Unit

Motion vectors are stored in quarter-pel units.

Examples:

```text
mv.x = 4   means +1 integer pixel
mv.x = -4  means -1 integer pixel
mv.x = 1   means +1/4 pixel
mv.x = 2   means +1/2 pixel
```

Since IME only searches integer-pixel positions, IME motion vectors are usually multiples of 4.

---

## Relationship with FME

```text
IME
  ↓
ime_result.best_mv
  ↓
FME
  ↓
refined fractional MV
```

IME provides the integer motion vector center for FME refinement.

---

## Test Description

Test file:

```text
ime/test/test_ime.cpp
```

The IME unit test uses identical current and reference frames.

The test checks:

```text
- IME output is valid
- best_inter_result is valid
- Output mode is inter
- predicted_luma size matches block area
- residual_luma size matches block area
- Best MV is near zero for identical frames
```

---

## Build and Run Test

From repository root:

```bash
cd ~/CDC-VP
cmake --build build --target test_ime
```

Run:

```bash
./build/components/vpu_tlm/ime/test/test_ime
```

Expected result:

```text
IME test PASSED
```

---

## Notes

IME is currently modeled at functional level. It focuses on integer motion search behavior and does not model exact RTL control FSM, SRAM banking, or cycle-level SAD array behavior.
