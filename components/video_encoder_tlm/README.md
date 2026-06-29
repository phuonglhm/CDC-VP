# H.265 / HEVC Video Encoder TLM Model

## Overview

This component provides an initial SystemC/TLM skeleton for an H.265/HEVC video encoder model.

The structure is based on the main RTL block organization of the open-source `xk265` encoder. The current implementation focuses on defining the high-level encoder architecture, common data structures, and block-level interfaces before implementing the full encoding algorithms.

This is not yet a bitstream-compliant H.265 encoder. It is an early TLM model intended for architectural modeling, integration, and step-by-step functional development.

## Reference RTL Structure

The TLM skeleton maps to the main blocks in the original xk265 RTL structure:

| xk265 RTL Block     | TLM File                   |
| ------------------- | -------------------------- |
| `rtl/enc_defines.v` | `encoder_defs.h`           |
| `rtl/top`           | `video_encoder_tlm.h/.cpp` |
| `rtl/prei`          | `prei.h/.cpp`              |
| `rtl/posi`          | `posi.h/.cpp`              |
| `rtl/ime`           | `ime.h/.cpp`               |
| `rtl/fme`           | `fme.h/.cpp`               |
| `rtl/rec`           | `rec.h/.cpp`               |
| `rtl/db`            | `db.h/.cpp`                |
| `rtl/cabac`         | `cabac.h/.cpp`             |
| `rtl/fetch`         | `fetch.h/.cpp`             |
| `rtl/mem`           | `mem.h/.cpp`               |

## Directory Structure

```text
components/video_encoder_tlm/
├── include/
│   ├── encoder_defs.h
│   ├── video_encoder_tlm.h
│   ├── frame.h
│   ├── block.h
│   ├── prediction_result.h
│   ├── fetch.h
│   ├── mem.h
│   ├── prei.h
│   ├── posi.h
│   ├── ime.h
│   ├── fme.h
│   ├── mode_decision.h
│   ├── rec.h
│   ├── db.h
│   └── cabac.h
│
├── src/
│   ├── video_encoder_tlm.cpp
│   ├── frame.cpp
│   ├── fetch.cpp
│   ├── mem.cpp
│   ├── prei.cpp
│   ├── posi.cpp
│   ├── ime.cpp
│   ├── fme.cpp
│   ├── mode_decision.cpp
│   ├── rec.cpp
│   ├── db.cpp
│   └── cabac.cpp
│
└── tests/
    ├── CMakeLists.txt
    └── test_video_encoder_tlm.cpp
```

## Current Status

The current version provides:

* Initial H.265/HEVC encoder block structure
* Common encoder definitions in `encoder_defs.h`
* Frame data structure with YUV 4:2:0 support
* Block description for CTU/CU/PU/TU-level modeling
* Prediction result structure for intra/inter prediction, motion vector, cost, QP, residual, skip, and merge information
* Initial TOP block header for `video_encoder_tlm`
* Basic SystemC/TLM integration structure

The following parts are still under development:

* Full TOP pipeline implementation
* PREI algorithm
* POSI algorithm
* IME algorithm
* FME algorithm
* Mode decision algorithm
* Reconstruction path
* Deblocking and SAO
* CABAC functional modeling
* Fetch and memory model integration
* Full encoder-level testbench

## Intended Encoder Flow

The final TLM flow is expected to follow this sequence:

```text
Input Frame
   ↓
Fetch Current Block
   ↓
PREI: Pre-intra mode estimation and QP preparation
   ↓
POSI: Intra prediction and intra cost calculation
   ↓
IME: Integer motion estimation
   ↓
FME: Fractional motion estimation and inter prediction
   ↓
Mode Decision: Select best intra/inter result
   ↓
REC: Reconstruct selected block
   ↓
DB/SAO: In-loop filtering
   ↓
CABAC: Entropy coding / bitstream generation
   ↓
Update Reference Frame
```

## Build

From the repository root:

```bash
cd ~/CDC-VP

cmake -S . -B build -G Ninja \
  -DCMAKE_C_COMPILER=/usr/bin/gcc \
  -DCMAKE_CXX_COMPILER=/usr/bin/g++ \
  -DCDC_CPU_BACKEND=riscv_vp \
  -DCDC_BUILD_MINI_TLM=OFF \
  -DCDC_BUILD_CPU_EVAL=OFF \
  -DCDC_BUILD_CUSTOM_SOC=ON \
  -DSYSTEMC_INCLUDE_DIR=/opt/systemc-2.3.4/include \
  -DSYSTEMC_LIBRARY=/opt/systemc-2.3.4/lib/libsystemc.so

cmake --build build --target test_video_encoder_tlm
```

## Run Test

```bash
./build/components/video_encoder_tlm/tests/test_video_encoder_tlm
```

Or from the component directory:

```bash
cd ~/CDC-VP/components/video_encoder_tlm
make run
```

## Notes

This component is currently an architectural TLM skeleton. The goal is to first stabilize the block structure and interfaces, then implement each encoder block step by step.

Recommended development order:

```text
1. encoder_defs
2. frame / block / prediction_result
3. block interfaces
4. TOP pipeline
5. PREI / POSI
6. IME / FME
7. Mode Decision
8. REC
9. DB/SAO
10. CABAC
11. Full encoder test
```
