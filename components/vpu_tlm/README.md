# VPU TLM Video Encoder Model

## Overview

This directory contains the functional SystemC/TLM model for a simplified VPU video-encoder pipeline.

The current codebase includes:

```text
PREI  -> Pre-Intra Estimation
POSI  -> Post-Intra Prediction
IME   -> Integer Motion Estimation
FME   -> Fractional Motion Estimation
REC   -> Reconstruction / TQ / InvTQ path
DB    -> Deblocking / SAO-like post-filter path
CABAC -> Entropy coding path
FETCH -> Frame loading helpers
MEM   -> Shared memory abstractions
```

The model is a **functional TLM model**, not a cycle-accurate RTL model.  
It focuses on block-level behavior, dataflow, packet formats, and unit testing.

Public integration headers are exposed under `include/`.  
Wrappers such as `vpu_rec_top.h` and `vpu_cabac_custom_packet.h` provide stable names for module-level types without leaking relative include paths into downstream code.

The canonical build/test flow is `CMake + CTest` from the repository build directory.  
Per-module `Makefile`s are kept only as local legacy helpers and are not the primary integration path.

## Architecture

### Scope

`vpu_tlm` is a functional SystemC/TLM model for a block-level video encoder pipeline.

It is intended to model:

- block traversal
- intra/inter mode evaluation
- reconstruction handoff
- deblocking/CABAC side effects
- packet formats and backend glue

It is not intended to be:

- cycle-accurate RTL
- bitstream-exact HEVC reference software
- a full encode product with GOP/rate-control/session management

### Top-Level Layers

#### 1. Prediction layer

Implemented by:

- `prei/`
- `posi/`
- `ime/`
- `fme/`
- [`src/video_encoder_tlm.cpp`](./src/video_encoder_tlm.cpp)

Responsibilities:

- run the intra path
- run the inter path when a valid reference exists
- choose the final mode with `mode_decision`
- convert the selected prediction into a REC request packet

Main public entry:

- [`video_encoder_tlm`](./include/video_encoder_tlm.h)

#### 2. REC/backend bridge layer

Implemented by:

- [`src/prediction_to_rec_packet.cpp`](./src/prediction_to_rec_packet.cpp)
- [`src/video_encoder_to_rec.cpp`](./src/video_encoder_to_rec.cpp)
- [`src/rec_backend_bridges.cpp`](./src/rec_backend_bridges.cpp)

Responsibilities:

- translate encoder decisions into REC packets
- populate MV memory for inter prediction
- bridge REC outputs into DB and CABAC packet formats

#### 3. Integrated backend layer

Implemented by:

- `rec/`
- `db/`
- `cabac/`
- [`src/video_encoder_full_tlm.cpp`](./src/video_encoder_full_tlm.cpp)

Responsibilities:

- run the reconstruction path
- feed deblocking metadata
- feed entropy-coding input/output
- expose a region-level or frame-level integrated run

Main public entry:

- [`video_encoder_full_tlm`](./include/video_encoder_full_tlm.h)

### Packet Coordinate Contract

REC/DB/CABAC packets use an extended 4x4-block coordinate encoding:

- `x`: low 8 bits of 4x4 block x
- `y`: low 8 bits of 4x4 block y
- `block_idx`: packed high bits as `[y11:8 | x11:8]`

Shared helpers live in:

- [`include/block_coord_codec.h`](./include/block_coord_codec.h)

Any module that interprets packet position should decode through that helper instead of using `x` or `y` directly as full coordinates.

### Test Layers

There are two kinds of tests:

- block-local tests under `*/test/`
- encoder/integration tests under [`tests/`](./tests)

## Testing

Build all `vpu_tlm` test targets from the repository build directory:

```bash
cd /home/indows/CDC-VP/build
cmake --build . --target \
  test_prei \
  test_posi \
  test_ime \
  test_fme \
  test_mem \
  test_mode_decision \
  test_prediction_to_rec_packet \
  test_video_encoder_tlm \
  test_video_encoder_rec_integration \
  test_video_encoder_full_tlm
```

Run the full `vpu_tlm` test set through `ctest`:

```bash
cd /home/indows/CDC-VP/build
LD_LIBRARY_PATH=/opt/systemc-2.3.4/lib:$LD_LIBRARY_PATH \
ctest --output-on-failure -R '^vpu_tlm\.'
```

Run only the integrated video-encoder tests:

```bash
cd /home/indows/CDC-VP/build
LD_LIBRARY_PATH=/opt/systemc-2.3.4/lib:$LD_LIBRARY_PATH \
./components/vpu_tlm/tests/test_mode_decision
LD_LIBRARY_PATH=/opt/systemc-2.3.4/lib:$LD_LIBRARY_PATH \
./components/vpu_tlm/tests/test_prediction_to_rec_packet
LD_LIBRARY_PATH=/opt/systemc-2.3.4/lib:$LD_LIBRARY_PATH \
./components/vpu_tlm/tests/test_video_encoder_tlm
LD_LIBRARY_PATH=/opt/systemc-2.3.4/lib:$LD_LIBRARY_PATH \
./components/vpu_tlm/tests/test_video_encoder_rec_integration
LD_LIBRARY_PATH=/opt/systemc-2.3.4/lib:$LD_LIBRARY_PATH \
./components/vpu_tlm/tests/test_video_encoder_full_tlm
```

---

## Directory Structure

```text
components/vpu_tlm/
├── README.md
├── CMakeLists.txt
├── common/
│   ├── include/
│   │   ├── block.h
│   │   ├── encoder_defs.h
│   │   ├── frame.h
│   │   └── prediction_result.h
│   └── src/
│       └── frame.cpp
├── prei/
│   ├── include/
│   │   ├── prei.h
│   │   └── prei_result.h
│   ├── src/
│   │   └── prei.cpp
│   └── test/
│       ├── CMakeLists.txt
│       └── test_prei.cpp
├── posi/
│   ├── include/
│   │   └── posi.h
│   ├── src/
│   │   └── posi.cpp
│   └── test/
│       ├── CMakeLists.txt
│       └── test_posi.cpp
├── ime/
│   ├── include/
│   │   ├── ime.h
│   │   └── ime_result.h
│   ├── src/
│   │   └── ime.cpp
│   └── test/
│       ├── CMakeLists.txt
│       └── test_ime.cpp
├── fetch/
├── mem/
├── rec/
├── db/
├── cabac/
├── prei/
├── posi/
├── ime/
├── fme/
├── include/
└── tests/
```

---

## Common Files

The `common/` folder contains data structures and definitions shared by all prediction blocks.

### `block.h`

Defines a generic coding block region.

It can represent:

```text
CTU: Coding Tree Unit
CU : Coding Unit
PU : Prediction Unit
TU : Transform Unit
```

Important fields:

```text
x, y          block position
width, height block size
size          compatibility size field
type          block type
depth         partition depth
```

---

### `frame.h` / `frame.cpp`

Defines the input and reference frame container.

The current frame model supports:

```text
- 8-bit luma plane
- YUV 4:2:0 chroma planes
- get/set functions for luma and chroma pixels
```

The prediction blocks mainly use the luma plane.

---

### `prediction_result.h`

Defines the common result format used by intra and inter prediction blocks.

Important fields:

```text
valid
mode
cost
rate
distortion
qp
partition
intra_mode
mv
skip
merge
i_in_p
predicted_luma
residual_luma
```

Both POSI and FME finally generate a `prediction_result`.

---

# Block Descriptions

## 1. PREI Block

### Meaning

PREI stands for **Pre-Intra Estimation**.

PREI is the first stage of the intra prediction path.  
It analyzes the current block and searches for good intra prediction mode candidates.

### Files

```text
prei/include/prei.h
prei/include/prei_result.h
prei/src/prei.cpp
prei/test/test_prei.cpp
```

### Main Responsibilities

```text
- Receive input frame and CTU/CU block
- Evaluate HEVC intra prediction modes 0..34
- Estimate intra prediction cost
- Generate intra mode candidates
- Produce PREI result for POSI
```

### Main API

```cpp
prei_result run(const frame& input,
                const block& ctu) const;
```

### Simplified Flow

```text
Input frame + CTU
      ↓
Build CU list
      ↓
Evaluate intra modes 0..34
      ↓
Estimate mode cost
      ↓
Select candidate intra modes
      ↓
Output prei_result
```

### Output

```text
prei_result
```

The PREI result contains intra mode information that will be used by POSI to generate actual predicted pixels.

### Unit Test

The PREI test checks:

```text
- PREI can run on a test frame
- PREI output is valid
```

Run:

```bash
cmake --build build --target test_prei
./build/components/vpu_tlm/prei/test/test_prei
```

---

## 2. POSI Block

### Meaning

POSI stands for **Post-Intra Prediction**.

POSI is the second stage of the intra prediction path.  
It receives the mode candidates from PREI and generates the actual intra prediction pixels.

### Files

```text
posi/include/posi.h
posi/src/posi.cpp
posi/test/test_posi.cpp
```

### Main Responsibilities

```text
- Receive input frame
- Receive reconstructed reference frame
- Receive PREI result
- Select intra mode candidate
- Generate planar / DC / angular prediction
- Compute residual = original - predicted
- Estimate rate and cost
- Output intra prediction_result
```

### Main API

```cpp
prediction_result run(const frame& input,
                      const frame& reconstructed,
                      const block& region,
                      const prei_result& prei_info,
                      std::uint32_t qp = INIT_QP) const;
```

### Simplified Flow

```text
Input frame + reconstructed frame + PREI result
      ↓
Select intra mode
      ↓
Generate intra prediction pixels
      ↓
Compute residual
      ↓
Estimate rate
      ↓
Compute cost
      ↓
Output intra prediction_result
```

### Output

```text
prediction_result with mode = intra
```

The POSI output is the final intra prediction candidate.

### Unit Test

The POSI test checks:

```text
- PREI result is valid
- POSI output is valid
- Output mode is intra
- predicted_luma size matches block area
- residual_luma size matches block area
```

Run:

```bash
cmake --build build --target test_posi
./build/components/vpu_tlm/posi/test/test_posi
```

---

## 3. IME Block

### Meaning

IME stands for **Integer Motion Estimation**.

IME is the first stage of the inter prediction path.  
It searches for the best integer-pixel motion vector between the current frame and the reference frame.

### Files

```text
ime/include/ime.h
ime/include/ime_result.h
ime/src/ime.cpp
ime/test/test_ime.cpp
```

### Main Responsibilities

```text
- Receive current frame and reference frame
- Generate integer-pixel search points
- Fetch current block and reference block pixels
- Compute SAD for each motion vector candidate
- Estimate motion vector coding rate
- Compute cost
- Select best integer motion vector
- Generate IME result for FME
```

### Main API

```cpp
ime_result run(const frame& input,
               const frame& reference,
               const block& ctu,
               std::uint32_t qp = INIT_QP) const;
```

### Simplified Flow

```text
Current frame + reference frame + CTU
      ↓
Build search window
      ↓
Generate integer MV candidates
      ↓
For each candidate MV:
    - read reference block
    - compute SAD
    - estimate MV rate
    - compute cost
      ↓
Select best integer MV
      ↓
Output ime_result
```

### Output

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

### Motion Vector Unit

Motion vectors are stored in quarter-pel units.

Example:

```text
mv.x = 4   means +1 integer pixel
mv.x = -4  means -1 integer pixel
mv.x = 1   means +1/4 pixel
mv.x = 2   means +1/2 pixel
```

Since IME only searches integer-pixel positions, IME motion vectors are usually multiples of 4.

### Unit Test

The IME test uses identical current and reference frames.

The test checks:

```text
- IME output is valid
- best_inter_result is valid
- Output mode is inter
- predicted_luma size matches block area
- residual_luma size matches block area
- Best MV is near zero for identical frames
```

Run:

```bash
cmake --build build --target test_ime
./build/components/vpu_tlm/ime/test/test_ime
```

---

## 4. FME Block

### Meaning

FME stands for **Fractional Motion Estimation**.

FME is the second stage of the inter prediction path.  
It refines the integer motion vector generated by IME using half-pixel and quarter-pixel search.

### Files

```text
fme/include/fme.h
fme/include/fme_result.h
fme/src/fme.cpp
fme/test/test_fme.cpp
```

### Main Responsibilities

```text
- Receive IME result
- Use IME integer MV as the search center
- Generate half-pel candidates
- Generate quarter-pel candidates
- Interpolate reference pixels
- Generate fractional-pixel prediction
- Compute residual
- Compute SATD
- Estimate MV rate
- Compute RD-like cost
- Select best refined fractional MV
- Optionally detect skip
- Output FME result
```

### Main API

```cpp
fme_result run(const frame& input,
               const frame& reference,
               const ime_result& ime_info,
               std::uint32_t qp = INIT_QP) const;
```

### Simplified Flow

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
Cost = SATD + lambda * MV bit cost
      ↓
Select best fractional MV
      ↓
Output fme_result
```

### Output

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

### Unit Test

The FME test first runs IME, then passes the IME result into FME.

The test checks:

```text
- IME output is valid
- FME output is valid
- FME best_inter_result is valid
- Output mode is inter
- predicted_luma size matches block area
- Refined MV is near zero for identical frames
```

Run:

```bash
cmake --build build --target test_fme
./build/components/vpu_tlm/fme/test/test_fme
```

---

# Prediction Paths

## Intra Path

```text
Input frame
   ↓
PREI
   ↓
POSI
   ↓
Intra prediction_result
```

## Inter Path

```text
Current frame + reference frame
   ↓
IME
   ↓
FME
   ↓
Inter prediction_result
```

---

# Build and Run All Tests

From repository root:

```bash
cd ~/CDC-VP
```

Configure:

```bash
cmake -S . -B build
```

Build all prediction block tests:

```bash
cmake --build build --target test_prei
cmake --build build --target test_posi
cmake --build build --target test_ime
cmake --build build --target test_fme
```

Run all tests:

```bash
./build/components/vpu_tlm/prei/test/test_prei
./build/components/vpu_tlm/posi/test/test_posi
./build/components/vpu_tlm/ime/test/test_ime
./build/components/vpu_tlm/fme/test/test_fme
```

Expected result:

```text
PREI test PASSED
POSI test PASSED
IME test PASSED
FME test PASSED
```

---

# Summary

The current VPU TLM prediction model is organized by block:

```text
PREI : Pre-intra mode estimation
POSI : Intra prediction generation
IME  : Integer motion estimation
FME  : Fractional motion estimation
```

This structure makes each block easier to understand, test, and maintain independently.
