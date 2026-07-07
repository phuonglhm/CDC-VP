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

IME generates the best integer-pixel motion vector.

FME later refines the IME result to half-pixel and quarter-pixel accuracy.

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
ime_search_config config
qp
```

Meaning:

```text
input      current frame
reference  reference frame
ctu        current block region
config     IME search configuration
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

The `best_inter_result` field is a `prediction_result` with:

```text
mode = prediction_mode::inter
```

It contains the selected inter prediction result generated from the best integer motion vector.

---

## Main API

```cpp
prediction_result run(const frame& input,
                      const block& region) const;

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

The first API is a compatibility API. It uses the same frame as both current frame and reference frame.

The second API is the normal IME API.

The third API is the configurable IME API.

---

## Simplified Functional Flow

```text
Current frame + reference frame + CTU
      ↓
Validate input
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
Evaluate inter partitions
      ↓
Select best partition and best integer MV
      ↓
Generate predicted_luma
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

Since IME only searches integer-pixel positions, IME motion vectors are expected to represent integer-pixel motion.

---

## Search Window

The IME model uses an integer-pixel search range similar to the RTL search-window behavior.

The current implementation uses a fixed RTL-like search window internally.

```text
Horizontal range: -64 to +63 integer pixels
Vertical range:   -32 to +31 integer pixels
```

The model evaluates candidate integer motion vectors, computes SAD and MV cost, then selects the best candidate with the lowest cost.

---

## Cost Calculation

IME uses a simplified RD-like cost:

```text
cost = SAD + MVD_cost
```

Where:

```text
SAD       Sum of Absolute Differences between current block and reference block
MVD_cost  Estimated motion vector coding cost
```

This is not a bit-exact CABAC cost model, but it keeps the functional role of the IME cost engine.

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

FME then searches around this integer MV to refine the result to half-pel or quarter-pel accuracy.

---

## Test Description

Test file:

```text
ime/test/test_ime.cpp
```

The IME unit test is a functional-level test for the IME block. It verifies integer motion search behavior, inter prediction result generation, SAD-based matching, QP clamping, deterministic output, compatibility API behavior, and invalid input handling.

---

## Test Cases

### 1. Identical Current/Reference Frame

Test name:

```text
[TEST] IME identical current/reference frame
```

Purpose:

```text
- Verify IME works when current frame and reference frame are identical
- Best SAD should be zero
- Best MV should be near zero
```

Main checks:

```text
- result.valid == true
- best_inter_result.valid == true
- best_inter_result.mode == prediction_mode::inter
- predicted_luma size matches CTU area
- best_sad == 0
- best_cost is valid
- best MV is near zero
```

---

### 2. Zero-Search Config

Test name:

```text
[TEST] IME zero-search config
```

Purpose:

```text
- Verify IME can run with a zero-search configuration
- Verify zero-motion behavior on identical frames
```

Main checks:

```text
- result.valid == true
- best_mv == (0, 0)
- best_sad == 0
```

---

### 3. Known Horizontal Shift

Test name:

```text
[TEST] IME known horizontal shift
```

Purpose:

```text
- Create a reference frame shifted horizontally from the current frame
- Verify IME can find a motion vector close to the known horizontal shift
```

Main checks:

```text
- result.valid == true
- best_inter_result.valid == true
- best_mv.x is close to the expected horizontal shift
- best_mv.y is close to zero
```

---

### 4. Known Vertical Shift

Test name:

```text
[TEST] IME known vertical shift
```

Purpose:

```text
- Create a reference frame shifted vertically from the current frame
- Verify IME can find a motion vector close to the known vertical shift
```

Main checks:

```text
- result.valid == true
- best_mv.x is close to zero
- best_mv.y is close to the expected vertical shift
```

---

### 5. Known Diagonal Shift

Test name:

```text
[TEST] IME known diagonal shift
```

Purpose:

```text
- Create a reference frame shifted in both x and y directions
- Verify IME can detect diagonal motion
```

Main checks:

```text
- result.valid == true
- best_mv.x is close to the expected horizontal shift
- best_mv.y is close to the expected vertical shift
```

---

### 6. Negative Horizontal Shift

Test name:

```text
[TEST] IME negative horizontal shift
```

Purpose:

```text
- Verify IME can detect negative motion direction
- Test motion search when the matching block is shifted in the opposite direction
```

Main checks:

```text
- result.valid == true
- best_mv.x is close to the expected negative shift
- best_mv.y is close to zero
```

---

### 7. Noisy Reference Frame

Test name:

```text
[TEST] IME noisy reference frame still produces valid result
```

Purpose:

```text
- Verify IME still produces a valid result when the reference frame contains noise
- Check robustness of the search and cost calculation
```

Main checks:

```text
- result.valid == true
- best_inter_result.valid == true
- best_cost is valid
- predicted_luma size matches CTU area
```

---

### 8. Golden SAD Comparison

Test name:

```text
[TEST] IME golden SAD comparison with small search
```

Purpose:

```text
- Compute a simple golden SAD in the testbench
- Compare IME best_sad against the golden result
```

Main checks:

```text
- result.valid == true
- result.best_sad == golden_sad
```

This test is important because SAD is the core metric of integer motion estimation.

---

### 9. Different CTU Sizes

Test name:

```text
[TEST] IME different CTU sizes
```

Purpose:

```text
- Verify IME works with different CTU/block sizes
- Check predicted_luma size for each block size
```

Tested sizes:

```text
8x8
16x16
32x32
```

Main checks:

```text
- result.valid == true
- predicted_luma size == ctu.area()
- best_sad == 0 for identical current/reference frames
```

---

### 10. QP Clamp

Test name:

```text
[TEST] IME QP clamp
```

Purpose:

```text
- Verify IME clamps QP into the valid range
- Prevent invalid QP from propagating into cost calculation
```

Main checks:

```text
- result.valid == true
- result.qp == clamped_qp
- best_inter_result.qp == clamped_qp
```

---

### 11. Deterministic Output

Test name:

```text
[TEST] IME deterministic output
```

Purpose:

```text
- Run IME twice with the same input
- Verify output is stable and repeatable
```

Main checks:

```text
- both results are valid
- best_partition matches
- best_mv matches
- best_sad matches
- best_rate matches
- best_cost matches
- best_inter_result fields match
- predicted_luma values match
```

---

### 12. Compatibility API

Test name:

```text
[TEST] IME compatibility API
```

Purpose:

```text
- Verify the simplified IME API still works
- Verify IME can return a prediction_result directly
```

Main checks:

```text
- result.valid == true
- result.mode == prediction_mode::inter
- result.qp == INIT_QP
- predicted_luma size matches CTU area
- cost is valid
```

---

### 13. Empty Current Frame Invalid Input

Test name:

```text
[TEST] IME empty current frame invalid input
```

Purpose:

```text
- Verify IME rejects an empty current frame
```

Expected behavior:

```text
result.valid == false
```

---

### 14. Empty Reference Frame Invalid Input

Test name:

```text
[TEST] IME empty reference frame invalid input
```

Purpose:

```text
- Verify IME rejects an empty reference frame
```

Expected behavior:

```text
result.valid == false
```

---

### 15. Zero-Size Block Invalid Input

Test name:

```text
[TEST] IME zero-size block invalid input
```

Purpose:

```text
- Verify IME rejects an invalid CTU/block with zero area
```

Expected behavior:

```text
result.valid == false
```

---

### 16. Out-of-Bound Block Invalid Input

Test name:

```text
[TEST] IME out-of-bound block invalid input
```

Example:

```text
Frame size: 64x64
CTU: x = 60, y = 60, size = 16
```

This is invalid because:

```text
60 + 16 > 64
```

Purpose:

```text
- Verify IME rejects a CTU outside the frame boundary
- Prevent silent clamped processing of invalid input regions
```

Expected behavior:

```text
result.valid == false
```

---

## Test Summary

The full functional IME test verifies:

```text
- IME result validity
- Inter prediction_result generation
- Integer motion vector search
- Zero-motion behavior
- Horizontal motion
- Vertical motion
- Diagonal motion
- Negative motion
- Noisy reference handling
- SAD correctness using a golden helper
- Multiple CTU sizes
- QP clamping
- Deterministic output
- Compatibility API behavior
- Invalid current frame handling
- Invalid reference frame handling
- Zero-size block handling
- Out-of-bound CTU handling
```

This is stronger than a basic smoke test, but it is still a functional-level unit test.

It does not prove bit-exact equivalence with RTL. Full verification would require comparison against RTL simulation output or a bit-exact golden model.

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
IME full functional test PASSED
```

---

## Notes

IME is currently modeled at functional level. It focuses on integer motion search behavior and does not model exact RTL control FSM, SRAM banking, valid/ready handshakes, or cycle-level SAD array behavior.

The current IME model generates `predicted_luma` for the selected inter prediction result. Depending on the implementation stage, `residual_luma` may be generated later by FME or another downstream block.
