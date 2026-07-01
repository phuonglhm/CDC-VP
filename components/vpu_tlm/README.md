# VPU TLM Model

## Overview

This directory contains a functional SystemC/TLM model of a simplified H.265/HEVC Video Processing Unit (VPU).

The component directory was renamed from:

```text
components/video_encoder_tlm
```

to:

```text
components/vpu_tlm
```

The model focuses on the prediction path of a video encoder:

```text
TOP
 ├── PREI
 ├── POSI
 ├── IME
 └── FME
```

This is a **functional TLM model**, not a cycle-accurate RTL model.

---

## Directory Structure

```text
components/vpu_tlm/
├── CMakeLists.txt
├── Makefile
├── README.md
├── include/
│   ├── block.h
│   ├── encoder_defs.h
│   ├── frame.h
│   ├── prediction_result.h
│   ├── prei.h
│   ├── prei_result.h
│   ├── posi.h
│   ├── ime.h
│   ├── ime_result.h
│   ├── fme.h
│   ├── fme_result.h
│   └── video_encoder_tlm.h
├── src/
│   ├── frame.cpp
│   ├── prei.cpp
│   ├── posi.cpp
│   ├── ime.cpp
│   ├── fme.cpp
│   └── video_encoder_tlm.cpp
└── tests/
    ├── CMakeLists.txt
    └── test_video_encoder_tlm.cpp
```

Note: The directory is now named `vpu_tlm`, but some class names, headers, and test names may still keep the old compatibility name `video_encoder_tlm`.

---

# Block Description

## 1. TOP Block

Files:

```text
include/video_encoder_tlm.h
src/video_encoder_tlm.cpp
```

The TOP block is the high-level SystemC/TLM wrapper of the VPU model.

Main responsibilities:

```text
- Represent the VPU as a SystemC module
- Provide the TLM socket interface
- Act as the top-level container of the encoder model
- Allow the VPU component to be instantiated and connected in a virtual platform
```

In the current model, TOP is mainly used to verify that:

```text
- The VPU TLM module can be created
- The TLM socket can be bound
- The SystemC simulation can start successfully
```

The TOP block does not model exact RTL timing. It is used as the functional wrapper for the VPU TLM component.

---

## 2. PREI Block

Files:

```text
include/prei.h
include/prei_result.h
src/prei.cpp
```

PREI means **Pre-Intra Estimation**.

PREI models the first stage of the intra prediction path. Its job is to analyze the current block and find good intra prediction mode candidates.

Main responsibilities:

```text
- Evaluate HEVC intra prediction modes 0..34
- Analyze the current CTU/CU
- Estimate the cost of different intra modes
- Generate candidate mode information
- Output PREI result for POSI
```

Main API:

```cpp
prei_result run(const frame& input,
                const block& ctu) const;
```

Simplified flow:

```text
Input frame + CTU
      ↓
Build CU list
      ↓
Evaluate intra modes
      ↓
Estimate cost
      ↓
Select best intra candidates
      ↓
Generate prei_result
```

Output:

```text
prei_result
```

This result is used by POSI to generate the actual intra prediction pixels.

---

## 3. POSI Block

Files:

```text
include/posi.h
src/posi.cpp
```

POSI means **Post-Intra Prediction**.

POSI receives the mode candidates from PREI and generates the actual intra prediction result.

Main responsibilities:

```text
- Read PREI mode candidates
- Fetch top and left reference pixels
- Generate intra prediction pixels
- Compute residual = original - predicted
- Estimate rate
- Compute cost
- Output final intra prediction_result
```

Main API:

```cpp
prediction_result run(const frame& input,
                      const frame& reconstructed,
                      const block& region,
                      const prei_result& prei_info,
                      std::uint32_t qp = INIT_QP) const;
```

Simplified flow:

```text
Input frame
Reconstructed reference frame
PREI result
      ↓
Select intra mode candidate
      ↓
Generate planar / DC / angular prediction
      ↓
Compute residual
      ↓
Estimate rate and cost
      ↓
Generate intra prediction_result
```

Output:

```text
prediction_result with mode = intra
```

POSI provides the intra candidate for the later mode decision stage.

---

## 4. IME Block

Files:

```text
include/ime.h
include/ime_result.h
src/ime.cpp
```

IME means **Integer Motion Estimation**.

IME is the first stage of the inter prediction path. It searches for the best integer-pixel motion vector between the current frame and the reference frame.

Main responsibilities:

```text
- Generate integer-pixel search points
- Read current block pixels
- Read reference block pixels
- Compute SAD for each candidate motion vector
- Estimate motion vector rate
- Compute motion estimation cost
- Select best integer motion vector
- Output IME result for FME
```

Main API:

```cpp
ime_result run(const frame& input,
               const frame& reference,
               const block& ctu,
               std::uint32_t qp = INIT_QP) const;
```

Simplified flow:

```text
Current frame + reference frame + CTU
      ↓
Build search window
      ↓
Generate integer MV candidates
      ↓
For each MV:
    - fetch reference block
    - compute SAD
    - estimate rate
    - compute cost
      ↓
Select best integer MV
      ↓
Generate ime_result
```

Output:

```text
ime_result
```

Important output fields:

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

Motion vectors are stored in quarter-pel units.

Example:

```text
mv.x = 4   means +1 integer pixel
mv.x = -4  means -1 integer pixel
mv.x = 1   means +1/4 pixel
mv.x = 2   means +1/2 pixel
```

For IME, the generated motion vectors are integer-pixel vectors, so they are multiples of 4.

---

## 5. FME Block

Files:

```text
include/fme.h
include/fme_result.h
src/fme.cpp
```

FME means **Fractional Motion Estimation**.

FME refines the integer motion vector generated by IME. It searches around the IME motion vector using half-pixel and quarter-pixel positions.

Main responsibilities:

```text
- Receive integer MV from IME
- Generate half-pel candidate motion vectors
- Generate quarter-pel candidate motion vectors
- Interpolate reference pixels
- Generate fractional-pixel prediction
- Compute residual
- Compute SATD
- Estimate motion vector rate
- Compute RD-like cost
- Select best refined fractional MV
- Optionally detect skip candidate
- Output final inter prediction_result
```

Main API:

```cpp
fme_result run(const frame& input,
               const frame& reference,
               const ime_result& ime_info,
               std::uint32_t qp = INIT_QP) const;
```

Simplified flow:

```text
IME integer MV
      ↓
Half-pel refinement
      ↓
Quarter-pel refinement
      ↓
Generate fractional prediction
      ↓
Compute SATD
      ↓
Compute cost = SATD + lambda * MV bit cost
      ↓
Select best fractional MV
      ↓
Generate fme_result
```

Output:

```text
fme_result
```

Important output fields:

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

This result is later compared against the intra prediction result.

---

## Prediction Flow

### Intra Path

```text
Input frame
   ↓
PREI
   ↓
POSI
   ↓
Intra prediction_result
```

### Inter Path

```text
Current frame + reference frame
   ↓
IME
   ↓
FME
   ↓
Inter prediction_result
```

### Overall Prediction Flow

```text
                 ┌──────── PREI ──────── POSI ──────── Intra result
Input frame ─────┤
                 └──────── IME ───────── FME ───────── Inter result
```

---

## Unit Tests

The test file is:

```text
components/vpu_tlm/tests/test_video_encoder_tlm.cpp
```

Current tests cover:

```text
- TOP module instantiation
- TLM socket binding
- PREI basic intra analysis
- POSI intra prediction from PREI result
- IME integer motion estimation
- FME fractional refinement from IME result
```

The tests check functional behavior, not bit-exact RTL equivalence.

---

## How to Build and Run Tests

From the repository root:

```bash
cd ~/CDC-VP
```

Build the test target:

```bash
cmake --build build --target test_video_encoder_tlm
```

Run the test executable:

```bash
./build/components/vpu_tlm/tests/test_video_encoder_tlm
```

Expected result:

```text
Video Encoder TLM Unit Tests PASSED
```

---

## Rename Note

The old directory name was:

```text
components/video_encoder_tlm
```

The new directory name is:

```text
components/vpu_tlm
```

Therefore, the top-level `components/CMakeLists.txt` should use:

```cmake
add_subdirectory(vpu_tlm)
```

instead of:

```cmake
add_subdirectory(video_encoder_tlm)
```

---

## Summary

This VPU TLM model currently focuses on the prediction pipeline.

Implemented main blocks:

```text
TOP  : SystemC/TLM wrapper
PREI : Pre-intra mode estimation
POSI : Intra prediction generation
IME  : Integer motion estimation
FME  : Fractional motion estimation
```

The model is suitable for:

```text
- Understanding VPU prediction dataflow
- Testing block-level encoder behavior
- Building a virtual platform level VPU model
- Connecting video encoder behavior with other SystemC/TLM components
```
