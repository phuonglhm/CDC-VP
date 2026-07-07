# PREI Block

## Overview

PREI stands for **Pre-Intra Estimation**.

This block is the first stage of the intra prediction path in the VPU TLM model. PREI analyzes the current CTU/CU and evaluates HEVC intra prediction modes to generate good intra mode candidates for POSI.

This is a **functional TLM model**, not a cycle-accurate RTL model.

---

## Directory Structure

```text
prei/
├── include/
│   ├── prei.h
│   └── prei_result.h
├── src/
│   └── prei.cpp
├── test/
│   ├── CMakeLists.txt
│   └── test_prei.cpp
└── README.md
```

---

## Role in VPU Pipeline

PREI belongs to the intra prediction path:

```text
Input Frame
    ↓
PREI
    ↓
POSI
    ↓
Intra prediction_result
```

PREI does not generate the final predicted pixels.  
Its main job is to choose or prepare candidate intra modes for POSI.

---

## Main Responsibilities

```text
- Receive input frame and CTU/CU block
- Build the CU/block candidate list
- Evaluate HEVC intra prediction modes 0..34
- Estimate intra mode cost
- Generate intra mode candidates
- Generate prei_result for POSI
```

---

## Main Input

```text
frame input
block ctu
```

`input` is the current frame.  
`ctu` is the block region being analyzed.

---

## Main Output

```text
prei_result
```

The `prei_result` contains intra mode information used by POSI.

---

## Main API

```cpp
prei_result run(const frame& input,
                const block& ctu) const;

prei_result run(const frame& input,
                const block& ctu,
                const prei_rate_control_config& rc_config) const;
```

---

## Simplified Functional Flow

```text
Input frame + CTU
      ↓
Build CU list
      ↓
Evaluate intra modes 0..34
      ↓
Estimate distortion / rate / cost
      ↓
Select candidate intra modes
      ↓
Generate prei_result
```

---

## Relationship with POSI

PREI output is consumed by POSI:

```text
PREI result
    ↓
POSI reads candidate intra modes
    ↓
POSI generates actual intra prediction pixels
```

PREI is used for mode candidate analysis, while POSI performs actual prediction generation.

---

## Test Description

Test file:

```text
prei/test/test_prei.cpp
```

The PREI unit test is a functional-level test for the PREI block. It verifies that PREI can analyze different CTU/CU input patterns, generate valid intra mode information, and reject invalid input cases.

### Test Cases

#### 1. Gradient CTU Test

This test creates a 64x64 frame with changing luma pixel values.

```text
[TEST] PREI gradient CTU
```

Purpose:

```text
- Check PREI on a normal non-uniform image block
- Verify that PREI can analyze a textured CTU
- Verify that the generated prei_result is valid
```

Main checks:

```text
- result.valid == true
- result.ctu matches the input CTU
- result.qp is inside the valid QP range
- result.mode_entries is not empty
- each mode entry is valid
- each CU has valid intra mode candidates
- best_mode_index is in range 0..34
- best_cost matches the minimum candidate cost
```

#### 2. Flat CTU Test

This test creates a flat frame where all luma pixels have the same value.

```text
[TEST] PREI flat CTU
```

Purpose:

```text
- Check PREI on a simple low-texture block
- Verify that PREI handles uniform image regions correctly
- Verify that the selected mode is DC or Planar for flat regions
```

Main checks:

```text
- result.valid == true
- result.mode_entries is not empty
- each entry selects a valid best mode
- best mode is DC or Planar
```

#### 3. Horizontal Edge CTU Test

This test creates a frame with a horizontal edge pattern.

```text
[TEST] PREI horizontal edge CTU
```

Purpose:

```text
- Check PREI on a directional edge pattern
- Verify that PREI can process non-flat directional content
- Verify that mode candidate generation works for edge-like blocks
```

Main checks:

```text
- result.valid == true
- mode candidates are generated
- candidate mode indices are in range 0..34
- candidate costs are valid
- best mode cost is the minimum candidate cost
```

#### 4. Vertical Edge CTU Test

This test creates a frame with a vertical edge pattern.

```text
[TEST] PREI vertical edge CTU
```

Purpose:

```text
- Check PREI on another directional edge pattern
- Verify that PREI works with different image structures
- Verify that the mode decision result remains valid
```

Main checks:

```text
- result.valid == true
- mode entries are valid
- mode candidates are valid
- best mode index is valid
- best cost is consistent with the candidate list
```

#### 5. Multiple Block Positions Test

This test runs PREI on several CTU positions inside the same frame.

```text
[TEST] PREI multiple block positions
```

Purpose:

```text
- Check that PREI does not only work at one fixed block position
- Verify PREI behavior at different CTU coordinates
- Include a near-boundary valid block position
```

Tested positions:

```text
(0, 0)
(16, 16)
(32, 32)
(48, 48)
```

For a 64x64 frame and 16x16 block, `(48, 48)` is still valid because:

```text
48 + 16 = 64
```

#### 6. Different CTU/CU Sizes Test

This test runs PREI with different block sizes.

```text
[TEST] PREI different CTU/CU sizes
```

Purpose:

```text
- Check PREI with multiple block sizes
- Verify that CU list generation works for different block dimensions
- Verify that mode entries and candidates are still valid
```

Tested sizes:

```text
8x8
16x16
32x32
```

#### 7. Deterministic Output Test

This test runs PREI twice using the same input frame and CTU.

```text
[TEST] PREI deterministic output
```

Purpose:

```text
- Verify that PREI produces stable output for the same input
- Detect unintended randomness or uninitialized state
```

Main checks:

```text
- both results are valid
- QP values match
- modebest64_sum values match
- number of mode entries match
- best mode index and best cost match for each entry
```

#### 8. Rate-Control Configuration Test

This test calls the PREI API with a custom `prei_rate_control_config`.

```text
[TEST] PREI rate-control config clamp
```

Purpose:

```text
- Verify the PREI overload that accepts rate-control configuration
- Check that output QP is clamped to the configured QP range
```

Example configuration:

```cpp
config.initial_qp = 40;
config.min_qp = 30;
config.max_qp = 35;
```

Expected behavior:

```text
result.qp >= config.min_qp
result.qp <= config.max_qp
```

#### 9. Empty Frame Invalid Input Test

This test passes an empty frame to PREI.

```text
[TEST] PREI empty frame invalid input
```

Purpose:

```text
- Verify input validation
- PREI should reject an empty frame
```

Expected behavior:

```text
result.valid == false
```

#### 10. Zero-Size Block Invalid Input Test

This test passes a block with size 0.

```text
[TEST] PREI zero-size block invalid input
```

Purpose:

```text
- Verify that PREI rejects invalid block dimensions
```

Expected behavior:

```text
result.valid == false
```

#### 11. Out-of-Bound Block Invalid Input Test

This test passes a CTU that exceeds the frame boundary.

```text
[TEST] PREI out-of-bound block invalid input
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

Expected behavior:

```text
result.valid == false
```

This test is useful because it verifies that PREI does not silently accept blocks outside the input frame.

### Summary

The PREI test verifies:

```text
- Valid input frame handling
- Gradient, flat, horizontal edge and vertical edge patterns
- Multiple CTU positions
- Multiple block sizes
- Intra mode candidate generation
- Best mode cost consistency
- QP range and rate-control configuration
- Deterministic behavior
- Invalid input handling
```

This is still a functional-level unit test. It does not prove bit-exact equivalence with RTL, but it provides stronger functional validation than a simple smoke test.

---

## Build and Run Test

From repository root:

```bash
cd ~/CDC-VP
cmake --build build --target test_prei
```

Run:

```bash
./build/components/vpu_tlm/prei/test/test_prei
```

Expected result:

```text
PREI full functional test PASSED
```

---

## Notes

PREI is currently modeled at functional level. It does not model exact RTL pipeline stages, SRAM timing, or cycle-by-cycle control behavior.

The current test is designed for functional validation. Full RTL-equivalent verification would require comparison against RTL simulation results or a bit-exact golden reference model.
