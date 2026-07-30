# Sauria NPU SystemC Model (v4.2) — Environment Setup & Test Execution Guide

This guide provides instructions for setting up the environment, compiling, and running the **Sauria NPU v4.2 SystemC model** verification test suite (including the **ViT Encoder Chained Test** and the **11 Sauria Demo Suite**) across any Linux system or EDA server environment.

---

## 1. Prerequisites & Toolchain Requirements

Before running the model on a new machine or environment, ensure the following tools are installed:

| Requirement | Minimum Version | Description |
| :--- | :---: | :--- |
| **C++ Compiler** | `g++` 7.3+ or `clang++` 6.0+ | Supports C++14 or C++17 standard (`-std=c++14` or `-std=c++17`) |
| **Build System** | GNU `make` 3.82+ | Executing build targets and script hooks |
| **SystemC Library** | Accellera SystemC 2.3.2+ | SystemC kernel and header files (`systemc.h`, `libsystemc.so` or `libsystemc.a`) |
| **Shell & Utilities** | `bash`, `python3` | For environment scripts and demo case configuration parsing |

---

## 2. Environment Configuration

### Step 2.1: Clone/Navigate to the Model Directory
Navigate to the `v4.2_model` directory within the workspace:

```bash
cd /path/to/sauria/RTL/src/v4.2_model
```

### Step 2.2: Export SystemC Path (`SYSTEMC_HOME`)
Set the `SYSTEMC_HOME` environment variable to point to your SystemC installation root directory (containing `include/` and `lib/` or `lib-linux64/`):

```bash
# Example 1: Local or custom installation path
export SYSTEMC_HOME=/data/XPU00000/users/vuong.nguyen/project/sauria/systemc_install

# Example 2: System-wide installation (e.g. /usr/local or /opt/systemc)
export SYSTEMC_HOME=/usr/local/systemc
```

> **Note on Custom Library Directories**: If your SystemC library folder is not named `lib` or `lib64`, you can pass `SC_LIBDIR` explicitly on the command line:
> ```bash
> make test_vit_encoder_int8 SYSTEMC_HOME=/path/to/systemc SC_LIBDIR=/path/to/systemc/dynlib
> ```

---

## 3. Running the ViT Encoder Block Test ($64 \times 64$ INT8 Chained Test)

The Vision Transformer (ViT) Encoder test executes a 9-instruction chained pipeline (LN1 $\rightarrow$ QKV Proj $\rightarrow$ Self-Attn $\rightarrow$ Proj GEMM $\rightarrow$ Skip ADD1 $\rightarrow$ LN2 $\rightarrow$ FFN1 GELU $\rightarrow$ FFN2 Linear $\rightarrow$ Skip ADD2) on a $64 \times 64$ INT8 PE array at $800 \text{ MHz}$.

### Step 3.1: Build and Run
```bash
# Clean previous builds (optional)
make clean

# Compile the ViT Encoder test
make test_vit_encoder_int8 SYSTEMC_HOME=$SYSTEMC_HOME

# Execute the test binary
./test_vit_encoder_int8
```

### Step 3.2: One-Line Convenience Command
```bash
make test_vit_encoder_int8 SYSTEMC_HOME=$SYSTEMC_HOME && ./test_vit_encoder_int8
```

### Step 3.3: Expected Output
A successful run will print the bit-exact parity verification results followed by the active hardware performance counters:

```text
==================================================
               VERIFICATION RESULTS               
==================================================
  Total Mismatches : 0
  Maximum Abs Diff : 0
  [PASS] ViT Encoder Block Chained Execution SUCCEEDED!
==================================================

==== [PERF] ViT Encoder Block (64x64) (array 64x64) ====
  total cycles      : 20007
  exec cycles       : 13580
  SA active cycles  : 13580
  OBP active cycles : 1104
  stall cycles      : 0  (0.0%)
  MAC ops performed : 49152000
  PE utilization    : 88.6%
=========================================
```

---

## 4. Running the 11 Sauria Demo Suite

The 11 captured demo cases validate various tensor compute shapes, data types (`int8`, `int16`, `fp16`), matrix geometries ($32 \times 32$, $64 \times 64$, $16 \times 8$), multi-tile processing, and strided convolutions.

### Step 4.1: List All Available Demo Cases
```bash
make list
```
*Outputs:*
* `conv5x5_demo`
* `demo_fp16_gemm_32x32`
* `demo_fp16_gemm_64x64`
* `demo_fp16_mvm_8x16`
* `demo_gemm_32x32`
* `demo_gemm_64x64`
* `demo_int16_gemm_32x32`
* `demo_int16_mvm_8x16`
* `demo_multitile_32x32`
* `demo_mvm_8x16`
* `demo_strided_32x32`

---

### Step 4.2: Run ALL 11 Demos (Smoke-CI Suite)
To run every captured demo case sequentially with an automated PASS/FAIL summary table:

```bash
make check SYSTEMC_HOME=$SYSTEMC_HOME
```

*Sample CI Output:*
```text
  [PASS] conv5x5_demo
  [PASS] demo_fp16_gemm_32x32
  [PASS] demo_fp16_gemm_64x64
  [PASS] demo_fp16_mvm_8x16
  [PASS] demo_gemm_32x32
  [PASS] demo_gemm_64x64
  [PASS] demo_int16_gemm_32x32
  [PASS] demo_int16_mvm_8x16
  [PASS] demo_multitile_32x32
  [PASS] demo_mvm_8x16
  [PASS] demo_strided_32x32
[check] ALL demo cases PASS
```

---

### Step 4.3: Run a Single Specific Demo Case
To execute a specific captured demo case (e.g. `demo_gemm_32x32`):

```bash
make demo CASE=demo_gemm_32x32 SYSTEMC_HOME=$SYSTEMC_HOME
```

---

### Step 4.4: Convenience Targets for Standard Demos
```bash
# Matrix-Vector Multiplication (demo_mvm_8x16)
make mvm SYSTEMC_HOME=$SYSTEMC_HOME

# 32x32 INT8 Matrix Multiplication (demo_gemm_32x32)
make gemm32 SYSTEMC_HOME=$SYSTEMC_HOME

# 64x64 INT8 Matrix Multiplication (demo_gemm_64x64)
make gemm64 SYSTEMC_HOME=$SYSTEMC_HOME
```

---

## 5. Running Standalone Sub-Block Testbenches

The model provides isolated testbenches to verify individual sub-components independently of the top-level core:

```bash
# 1. Output Boundary Pipeline (OBP) standalone testbench
make obptest SYSTEMC_HOME=$SYSTEMC_HOME

# 2. Reduction Engine & Reconfigurable Engine (RE / RCE) standalone testbench
make retest SYSTEMC_HOME=$SYSTEMC_HOME
```

---

## 6. Overriding Build Knobs (C++ Standard & Geometry)

You can customize compilation flags directly on the command line:

### Force C++ Standard Version (C++14 or C++17)
```bash
make test_vit_encoder_int8 STD=-std=c++14 SYSTEMC_HOME=$SYSTEMC_HOME
```

### Enable Debug Trace Prints
```bash
make test_vit_encoder_int8 DEBUG=1 SYSTEMC_HOME=$SYSTEMC_HOME
```

### Override Geometry & Memory Region Capacity
```bash
make eval GEO=64x64 RB=65536 SYSTEMC_HOME=$SYSTEMC_HOME
```

---

## 7. Troubleshooting & FAQ

### Q1: `systemc.h: No such file or directory`
* **Cause**: `SYSTEMC_HOME` is not set or points to an invalid directory.
* **Fix**: Run `export SYSTEMC_HOME=/correct/path/to/systemc` and verify that `$SYSTEMC_HOME/include/systemc.h` exists.

### Q2: `cannot open shared object file: libsystemc.so`
* **Cause**: The dynamic linker cannot find SystemC shared libraries at runtime.
* **Fix**: Ensure `SC_LIBDIR` is set or add the library directory to `LD_LIBRARY_PATH`:
  ```bash
  export LD_LIBRARY_PATH=$SYSTEMC_HOME/lib:$LD_LIBRARY_PATH
  ```

### Q3: How to run self-testing C-generated cases without Python?
```bash
make selftest SYSTEMC_HOME=$SYSTEMC_HOME
```
This generates, builds, and executes synthetic workloads entirely within C++.

---
*Document Version: 4.2.0 | Last Updated: 2026-07-27*
