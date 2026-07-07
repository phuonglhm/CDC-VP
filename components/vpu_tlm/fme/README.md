# FME Block

## Overview

FME stands for **Fractional Motion Estimation**.

This block is the second stage of the inter prediction path in the VPU TLM model. FME receives the integer motion vector from IME and refines it using half-pixel and quarter-pixel candidates.

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

IME performs coarse integer-pixel motion estimation.

FME refines the IME result to fractional-pixel precision.

---

## Main Responsibilities

```text
- Receive IME result
- Use IME integer MV as the search center
- Generate half-pel motion vector candidates
- Generate quarter-pel motion vector candidates
- Interpolate reference pixels
- Generate fractional-pixel prediction
- Compute SATD for candidate prediction blocks
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
fme_refine_config config
qp
```

Meaning:

```text
input      current frame
reference  reference frame
ime_info   integer motion estimation result from IME
config     FME refinement configuration
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

The `best_inter_result` field is a `prediction_result` with:

```text
mode = prediction_mode::inter
```

It contains the selected inter prediction result after FME refinement.

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

The first API uses the default FME refinement configuration.

The second API allows explicit control of half-pel search, quarter-pel search, skip decision, search radius, and interpolation mode.

---

## Refinement Configuration

```cpp
struct fme_refine_config {
    bool enable_half_pel;
    bool enable_quarter_pel;
    bool enable_skip_decision;

    std::int32_t half_pel_radius_qpel;
    std::int32_t quarter_pel_radius_qpel;

    bool use_hevc_luma_filter;
};
```

Meaning:

```text
enable_half_pel          enable half-pixel refinement
enable_quarter_pel       enable quarter-pixel refinement
enable_skip_decision     enable skip candidate decision
half_pel_radius_qpel     half-pel search radius in quarter-pel units
quarter_pel_radius_qpel  quarter-pel search radius in quarter-pel units
use_hevc_luma_filter     use HEVC-like luma interpolation filter
```

---

## Simplified Functional Flow

```text
Current frame + reference frame + IME result
      ↓
Validate input
      ↓
Read IME integer MV
      ↓
Generate half-pel candidates
      ↓
Generate quarter-pel candidates
      ↓
Interpolate reference pixels
      ↓
Generate prediction block
      ↓
Compute SATD
      ↓
Estimate MV rate
      ↓
Compute cost = SATD + lambda * rate
      ↓
Select best refined MV
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

IME usually outputs integer-pixel motion vectors, so IME MVs are normally multiples of 4.

FME may output fractional-pixel motion vectors, so FME MVs may not be multiples of 4.

---

## Cost Calculation

FME uses a simplified RD-like cost:

```text
cost = SATD + lambda * rate
```

Where:

```text
SATD    Sum of Absolute Transformed Differences
lambda  QP-dependent weighting factor
rate    estimated motion vector coding rate
```

The candidate with the lowest cost is selected as the best refined motion vector.

---

## Relationship with IME

```text
IME
  ↓
integer MV
  ↓
FME
  ↓
refined fractional MV
```

IME provides the initial integer-pixel motion vector.

FME searches around that MV to improve prediction accuracy.

Example:

```text
IME output MV: +8.00 pixels
FME refined MV: +8.25 pixels
```

The refined MV can reduce prediction error and improve compression efficiency.

---

## Relationship with Later Blocks

```text
FME
 ↓
REC / Mode Decision / CABAC
```

FME output can be used by later encoder stages:

```text
best_inter_result  used by mode decision
predicted_luma     used by reconstruction / residual generation
best_mv            used for motion information coding
best_cost          used for inter/intra decision
skip               used for skip-mode decision
```

Depending on implementation stage, `residual_luma` may be generated inside FME or generated later by downstream blocks such as REC.

---

## Test Description

Test file:

```text
fme/test/test_fme.cpp
```

The FME unit test is a full functional-level test for the FME block. It verifies FME refinement behavior, IME-to-FME dataflow, configurable refinement modes, motion-vector refinement, SATD/cost behavior, deterministic output, and invalid input handling.

---

## Test Cases

### 1. Default Refinement

Test name:

```text
[TEST] FME default refinement
```

Purpose:

```text
- Run FME with default refinement configuration
- Verify FME can receive a valid IME result
- Verify FME generates a valid inter prediction result
```

Main checks:

```text
- ime_info.valid == true
- result.valid == true
- best_inter_result.valid == true
- best_inter_result.mode == prediction_mode::inter
- predicted_luma size matches CTU area
- cost fields are valid
- MV is near zero for identical current/reference frames
```

---

### 2. Explicit Refine Config

Test name:

```text
[TEST] FME explicit refine config
```

Purpose:

```text
- Verify FME works with an explicit fme_refine_config
- Check configurable refinement path
```

Main checks:

```text
- result.valid == true
- best_inter_result.valid == true
- predicted_luma size matches CTU area
- cost fields are valid
```

---

### 3. Integer-Only Refinement Config

Test name:

```text
[TEST] FME integer-only refinement config
```

Purpose:

```text
- Disable half-pel and quarter-pel refinement
- Verify FME can operate using only the IME integer MV
```

Main checks:

```text
- result.valid == true
- best_mv matches the input IME MV
```

---

### 4. Half-Pel Only Refinement Config

Test name:

```text
[TEST] FME half-pel only refinement config
```

Purpose:

```text
- Enable half-pel refinement
- Disable quarter-pel refinement
- Verify quarter-pel candidates are not required
```

Main checks:

```text
- result.valid == true
- best_inter_result.valid == true
- no quarter-pel candidate is selected/exported when quarter-pel is disabled
```

---

### 5. Quarter-Pel Enabled Config

Test name:

```text
[TEST] FME quarter-pel enabled config
```

Purpose:

```text
- Enable both half-pel and quarter-pel refinement
- Verify FME can run with fractional refinement enabled
```

Main checks:

```text
- result.valid == true
- best_inter_result.valid == true
- refined MV and prediction result are valid
```

---

### 6. Known Integer Shift from Manual IME Result

Test name:

```text
[TEST] FME known integer shift from manual IME result
```

Purpose:

```text
- Provide a manual IME result with a known motion vector
- Verify FME refines around the expected integer MV
```

Main checks:

```text
- result.valid == true
- best_inter_result.valid == true
- refined MV is close to expected horizontal MV
```

---

### 7. Known Vertical Shift

Test name:

```text
[TEST] FME known vertical shift from manual IME result
```

Purpose:

```text
- Verify FME behavior for vertical motion
```

Main checks:

```text
- result.valid == true
- refined MV is close to expected vertical MV
```

---

### 8. Known Diagonal Shift

Test name:

```text
[TEST] FME known diagonal shift from manual IME result
```

Purpose:

```text
- Verify FME behavior for diagonal motion
```

Main checks:

```text
- result.valid == true
- refined MV is close to expected horizontal and vertical MV
```

---

### 9. Skip Decision on Flat Frame

Test name:

```text
[TEST] FME skip decision on flat frame
```

Purpose:

```text
- Use a flat current/reference frame
- Verify zero-difference prediction behavior
- Check skip-related behavior on easy-to-predict blocks
```

Main checks:

```text
- result.valid == true
- best_satd == 0
```

---

### 10. Different CTU Sizes

Test name:

```text
[TEST] FME different CTU sizes
```

Purpose:

```text
- Verify FME works with different CTU/block sizes
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
- best_satd == 0 for identical current/reference frames
```

---

### 11. QP Clamp

Test name:

```text
[TEST] FME QP clamp
```

Purpose:

```text
- Verify FME clamps QP into valid range
- Prevent invalid QP from propagating into cost calculation
```

Main checks:

```text
- result.valid == true
- result.qp == clamped_qp
- best_inter_result.qp == clamped_qp
```

---

### 12. Deterministic Output

Test name:

```text
[TEST] FME deterministic output
```

Purpose:

```text
- Run FME twice with the same input
- Verify output is stable and repeatable
```

Main checks:

```text
- both results are valid
- best_partition matches
- best_mv matches
- best_satd matches
- best_rate matches
- best_cost matches
- skip flag matches
- best_inter_result fields match
- predicted_luma values match
```

---

### 13. Invalid IME Input

Test name:

```text
[TEST] FME invalid IME input
```

Purpose:

```text
- Verify FME rejects invalid IME result
```

Expected behavior:

```text
result.valid == false
```

---

### 14. Empty Current Frame Invalid Input

Test name:

```text
[TEST] FME empty current frame invalid input
```

Purpose:

```text
- Verify FME rejects an empty current frame
```

Expected behavior:

```text
result.valid == false
```

---

### 15. Empty Reference Frame Invalid Input

Test name:

```text
[TEST] FME empty reference frame invalid input
```

Purpose:

```text
- Verify FME rejects an empty reference frame
```

Expected behavior:

```text
result.valid == false
```

---

### 16. Zero-Size Block Invalid Input

Test name:

```text
[TEST] FME zero-size block invalid input
```

Purpose:

```text
- Verify FME rejects a CTU/block with zero area
```

Expected behavior:

```text
result.valid == false
```

---

### 17. Out-of-Bound Block Invalid Input

Test name:

```text
[TEST] FME out-of-bound block invalid input
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
- Verify FME rejects a CTU outside the frame boundary
- Prevent silent clamped processing of invalid input regions
```

Expected behavior:

```text
result.valid == false
```

---

## Test Summary

The full functional FME test verifies:

```text
- FME result validity
- IME-to-FME dataflow
- Inter prediction_result generation
- Default refinement configuration
- Explicit refinement configuration
- Integer-only refinement mode
- Half-pel-only refinement mode
- Quarter-pel-enabled refinement mode
- Horizontal motion refinement
- Vertical motion refinement
- Diagonal motion refinement
- Skip-related behavior on flat frames
- Different CTU sizes
- QP clamping
- Deterministic output
- Invalid IME input handling
- Empty current frame handling
- Empty reference frame handling
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
cmake --build build --target test_fme
```

Run:

```bash
./build/components/vpu_tlm/fme/test/test_fme
```

Expected result:

```text
FME full functional test PASSED
```

---

## Notes

FME is currently modeled at functional level. It preserves the main algorithmic behavior of fractional motion estimation, including motion-vector refinement, interpolation-based prediction, SATD/cost calculation, QP-dependent cost behavior, and best candidate selection.

The model does not implement exact RTL pipeline timing, SRAM access cycles, valid/ready handshakes, or cycle-level SATD/interpolation hardware behavior.
