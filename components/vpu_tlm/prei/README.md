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

The PREI unit test checks:

```text
- A test frame can be created
- PREI can run on a CTU/block
- PREI output is valid
```

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
PREI test PASSED
```

---

## Notes

PREI is currently modeled at functional level. It does not model exact RTL pipeline stages, SRAM timing, or cycle-by-cycle control behavior.
