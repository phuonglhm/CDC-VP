# ISP SystemC Architecture Model - Build & Run Guide

This document describes how to build and run all testbenches for the SystemC streaming ISP pipeline architecture model in CDC-VP.

---

## 1. Directory Structure

```
components/isp_tlm/systemc/
├── CMakeLists.txt                # Build configuration for all testbenches
├── build.sh                      # Build script (recommended)
├── verify.sh                     # Verification script
│
├── hw/                          # Hardware architecture infrastructure
│   ├── hw.h                     # Central include header
│   ├── isp_arch_config.h        # Architecture configuration (clock, block params)
│   ├── metrics.h                # Architecture metrics collection
│   ├── power.h                  # Power estimation models
│   ├── sweep.h                  # Architecture sweep runner
│   ├── stream_beat.h            # Frame-aware stream types
│   ├── timed_stream.h           # Clocked instrumented FIFO
│   ├── timed_block.h            # Hardware shell base class
│   ├── local_memory.h            # Memory models (line buffers)
│   ├── frame_dma.h               # DMA models (input/output bandwidth)
│   └── trace.h                  # VCD tracing utilities
│
├── tb_utils/                    # Testbench utilities
│   ├── tb_utils.h               # Generic_Driver<T>, Generic_Monitor<T>
│   ├── hardware_params.h        # Hardware configuration (clk, bus width, FIFO depth)
│   ├── sc_block_metrics.h       # Per-block latency collector
│   └── sc_metrics_wrapper.h      # Per-boundary throughput wrapper
│
├── blocks/                      # 17 ISP processing blocks
│   ├── blc/        (sc_blc.{h,cpp}, tb_blc.cpp)
│   ├── dpc/        (sc_dpc.{h,cpp}, tb_dpc.cpp)
│   ├── dg/         (sc_dg.{h,cpp}, tb_dg.cpp)
│   ├── wb/         (sc_wb.{h,cpp}, tb_wb.cpp)
│   ├── ccm/        (sc_ccm.{h,cpp}, tb_ccm.cpp)
│   ├── gc/         (sc_gc.{h,cpp}, tb_gc.cpp)
│   ├── csc/        (sc_csc.{h,cpp}, tb_csc.cpp)
│   ├── cse/        (sc_cse.{h,cpp}, tb_cse.cpp)
│   ├── lsc/        (sc_lsc.{h,cpp}, tb_lsc.cpp)
│   ├── bnr/        (sc_bnr.{h,cpp}, tb_bnr.cpp)
│   ├── demosaic/   (sc_demosaic.{h,cpp}, tb_demosaic.cpp)
│   ├── sharpen/     (sc_sharpen.{h,cpp}, tb_sharpen.cpp)
│   ├── 2dnr/       (sc_2dnr.{h,cpp}, tb_2dnr.cpp)
│   ├── awb/        (sc_awb.{h,cpp}, tb_awb.cpp)
│   ├── aec/        (sc_aec.{h,cpp}, tb_aec.cpp)
│   ├── scale/      (sc_scale.{h,cpp}, tb_scale.cpp)
│   └── yuv420/     (sc_yuv420.{h,cpp}, tb_yuv420.cpp)
│
└── pipeline/
    ├── sc_input_normalizer.{h,cpp}  # Input bit-depth normalization
    ├── sc_isp_pipeline.{h,cpp}      # Top-level SystemC pipeline module
    ├── tb_pipeline.cpp               # Full pipeline testbench
    ├── tb_arch_pipeline.cpp          # Architecture-aware timed pipeline
    ├── tb_power_metrics.cpp          # Power estimation testbench
    ├── tb_arch_sweep.cpp             # Architecture sweep runner
    └── tb_dma_integration.cpp        # DMA integration testbench
```

---

## 2. Environment Requirements

| Component | Version / Note |
|-----------|----------------|
| SystemC   | 2.3.4 (installed at `/opt/systemc-2.3.4`) |
| CMake     | >= 3.16 |
| Compiler  | `g++` >= 7 (C++17) |
| Ninja     | Recommended for faster builds |

Quick verification:

```bash
ls /opt/systemc-2.3.4/include/systemc.h
ls /opt/systemc-2.3.4/lib/libsystemc.so
g++ --version
cmake --version
```

---

## 3. Build Instructions

### 3.1 Recommended: Build via CDC-VP

From the CDC-VP root directory:

```bash
cd /home/hoangquan/workspace/CDC-VP

# Configure CMake
cmake -S . -B build/bremen -G Ninja \
  -DCMAKE_C_COMPILER=/usr/bin/gcc \
  -DCMAKE_CXX_COMPILER=/usr/bin/g++ \
  -DCDC_BUILD_TESTS=ON \
  -DSYSTEMC_INCLUDE_DIR=/opt/systemc-2.3.4/include \
  -DSYSTEMC_LIBRARY=/opt/systemc-2.3.4/lib/libsystemc.so

# Build all targets
./components/isp_tlm/systemc/build.sh

# Or build specific targets
ninja -C build/bremen tb_blc tb_pipeline
```

### 3.2 Using the Build Script

```bash
cd /home/hoangquan/workspace/CDC-VP
./components/isp_tlm/systemc/build.sh              # Build all
./components/isp_tlm/systemc/build.sh configure     # Reconfigure
./components/isp_tlm/systemc/build.sh clean         # Clean build
./components/isp_tlm/systemc/build.sh help           # Show options
```

Build outputs are placed in `build/bremen/components/isp_tlm/systemc/`.

---

## 4. Testbench Overview

### 4.1 Individual Block Testbenches (17 blocks)

Each block has its own self-contained testbench that verifies correctness against a golden reference.

| Binary | Block | Processing Type |
|--------|-------|-----------------|
| `tb_blc` | Black Level Correction | Pixel-wise |
| `tb_dpc` | Defect Pixel Correction | Frame-wise |
| `tb_dg` | Digital Gain | Pixel-wise |
| `tb_wb` | White Balance | Pixel-wise |
| `tb_ccm` | Color Correction Matrix | Pixel-wise |
| `tb_gc` | Gamma Correction | Pixel-wise |
| `tb_csc` | Color Space Conversion (RGB→YUV) | Pixel-wise |
| `tb_cse` | Color Space Enhancement | Pixel-wise |
| `tb_lsc` | Lens Shading Correction | Frame-wise |
| `tb_bnr` | Bad Pixel Replacement | Frame-wise |
| `tb_demosaic` | Demosaicing | Frame-wise |
| `tb_sharpen` | Sharpening | Pixel-wise |
| `tb_2dnr` | 2D Noise Reduction | Frame-wise |
| `tb_awb` | Auto White Balance | Frame-wise |
| `tb_aec` | Auto Exposure Control | Frame-wise |
| `tb_scale` | Scaling | Frame-wise |
| `tb_yuv420` | YUV420 Conversion | Frame-wise |

### 4.2 Pipeline Testbenches

| Binary | Purpose |
|--------|---------|
| `tb_pipeline` | Full ISP pipeline verification |
| `tb_d65_pipeline` | Full pipeline with real D65 image |
| `tb_d65_blc` | BLC with real D65 image |
| `tb_arch_pipeline` | Architecture-aware timed pipeline with metrics |
| `tb_power_metrics` | Per-block power estimation |
| `tb_arch_sweep` | Architecture parameter sweeps |
| `tb_dma_integration` | DMA and memory model integration |
| `tb_metrics_demo` | Metrics wrapper demonstration |

### 4.3 Original TLM Testbench

| Binary | Purpose |
|--------|---------|
| `isp_run` | Original reference C++ TLM pipeline (via `components/isp_tlm/tests/`) |

---

## 5. Running Tests

### 5.1 Individual Block Tests

```bash
BUILD=build/bremen/components/isp_tlm/systemc

# Test all blocks individually
$BUILD/tb_blc
$BUILD/tb_dpc
$BUILD/tb_dg
$BUILD/tb_wb
$BUILD/tb_ccm
$BUILD/tb_gc
$BUILD/tb_csc
$BUILD/tb_cse
$BUILD/tb_lsc
$BUILD/tb_bnr
$BUILD/tb_demosaic
$BUILD/tb_sharpen
$BUILD/tb_2dnr
$BUILD/tb_awb
$BUILD/tb_aec
$BUILD/tb_scale
$BUILD/tb_yuv420
```

Expected output:

```
==================================================
Testbench: Black Level Correction (BLC)
==================================================
[TB] Generated test pattern: 4096 pixels
[TB] Golden reference computed
[TB] Starting simulation...
[TB] Simulation completed
TEST RESULT: PASS
==================================================
```

### 5.2 Full Pipeline Tests

```bash
# Basic pipeline verification
./build/bremen/components/isp_tlm/systemc/tb_pipeline

# Full pipeline with D65 real image (2688x1520)
./build/bremen/components/isp_tlm/systemc/tb_d65_pipeline

# BLC with D65 real image
./build/bremen/components/isp_tlm/systemc/tb_d65_blc
```

### 5.3 Architecture Testbenches

```bash
# Architecture-aware timed pipeline
./build/bremen/components/isp_tlm/systemc/tb_arch_pipeline

# Power estimation
./build/bremen/components/isp_tlm/systemc/tb_power_metrics

# Architecture sweeps
./build/bremen/components/isp_tlm/systemc/tb_arch_sweep

# DMA integration
./build/bremen/components/isp_tlm/systemc/tb_dma_integration
```

### 5.4 Original TLM Reference

```bash
./build/bremen/components/isp_tlm/tests/isp_run \
  -i components/isp_tlm/input/D65_raw_2688x1520_5376.raw \
  -o /tmp/output.yuv \
  -w 2688 --height 1520
```

### 5.5 Running All Tests via CTest

**Important:** Run from the build directory.

```bash
cd build/bremen

# Run all tests
ctest --output-on-failure

# Run only SystemC tests
ctest -R '^tb_' --output-on-failure

# Run specific pipeline tests
ctest -R 'pipeline' --output-on-failure
```

---

## 6. Metrics Collection

### 6.1 Three-Layer Architecture

```
RAW Input (sensor)
  │
  ▼
┌──────────────────────────────────────────────────────────────────┐
│ LAYER 1: Block Processing Latency                                │
│ Each block: sc_block_metrics<T>                                  │
│   - Point ops: normalizer, BLC, DG, WB, CCM, GC, CSC, CSE      │
│   - Frame ops: DPC, BNR, demosaic, AWB, AEC, sharpen,          │
│                2DNR, scale, yuv420                              │
│   → block_<name>.csv + block_<name>_summary.txt                 │
└──────────────────────────────────────────────────────────────────┘
  │
  ▼
┌──────────────────────────────────────────────────────────────────┐
│ LAYER 2: Boundary Throughput                                     │
│ sc_metrics_wrapper<T> between every block pair (17 wrappers)      │
│   → <upstream>_to_<downstream>.csv + summary.txt                │
└──────────────────────────────────────────────────────────────────┘
  │
  ▼
YUV420 Output (display)
```

### 6.2 Hardware Parameters

All timing calculations use `hw_params`:

| Parameter | Description | Default |
|-----------|-------------|---------|
| `clk_mhz` | Clock frequency (MHz) | 200 |
| `bus_width_bits` | Bus width (bits/cycle) | 64 |
| `pixel_bits` | Pixel bits (token size) | 16 |
| `fifo_depth` | FIFO depth | 1024 |
| `timed_mode` | Enable timing delays | false |

### 6.3 Key Calculations

| Method | Formula | Example @ 200MHz |
|--------|---------|------------------|
| `cycle_ns()` | `1000/clk_mhz` | 5.0 ns |
| `tokens_per_cycle()` | `bus_width_bits/pixel_bits` | 4.0 |
| `throughput_Mpix_s()` | `clk_mhz*1e6/tokens_per_cycle` | 50.0 Mpix/s |

### 6.4 Metrics Output Structure

```
output/metrics/
├── block_normalizer.csv          # Layer 1: block latency
├── block_normalizer_summary.txt
├── block_blc.csv
├── block_blc_summary.txt
├── ...
├── block_yuv420.csv
├── block_yuv420_summary.txt
│
├── input_norm_to_blc.csv        # Layer 2: boundary throughput
├── input_norm_to_blc_summary.txt
├── blc_to_dpc.csv
├── blc_to_dpc_summary.txt
├── ...
├── scale_to_yuv420.csv
├── scale_to_yuv420_summary.txt
│
└── summary.csv                  # Aggregated summary
```

### 6.5 Enabling Metrics in Testbench

```cpp
// Enable metrics collection
dut.enable_metrics = true;
dut.metrics_output_dir = "output/metrics";

// After simulation, dump metrics
dut.dump_all_block_metrics();    // → block_*.csv + block_*_summary.txt
dut.dump_pipeline_metrics();     // → *_to_*.csv + *_to_*_summary.txt
dut.print_metrics_summary();     // → console table
```

### 6.6 Architecture Metrics

```cpp
// Enable architecture metrics collection
dut.enable_arch_metrics("output/arch_metrics");

// After simulation
dut.collect_block_metrics();
dut.update_arch_frame_timing(0);
dut.dump_arch_metrics();         // Print bottleneck analysis
dut.dump_arch_summary();         // Dump CSV files
```

---

## 7. Test with Real Images

### 7.1 Available Test Images

| File | Resolution | Bit Depth | Pattern |
|------|------------|-----------|---------|
| `D65_raw_2688x1520_5376.raw` | 2688×1520 | 16-bit RGGB | D65 illuminant |
| `ColorChecker_2592x1536_12bits_RGGB.raw` | 2592×1536 | 12-bit RGGB | Color checker |
| `A_raw_2688x1520_5376.raw` | 2688×1520 | 16-bit RGGB | A illuminant |

### 7.2 Running Real Image Tests

```bash
# Build D65 testbenches
cmake --build build/bremen --target tb_d65_blc tb_d65_pipeline

# Run BLC with D65 image
./build/bremen/components/isp_tlm/systemc/tb_d65_blc

# Run full pipeline with D65 image
./build/bremen/components/isp_tlm/systemc/tb_d65_pipeline
```

### 7.3 Original TLM Runner with Real Image

```bash
./build/bremen/components/isp_tlm/tests/isp_run \
  -i components/isp_tlm/input/D65_raw_2688x1520_5376.raw \
  -o /tmp/output.yuv \
  -w 2688 --height 1520
```

---

## 8. Testbench Descriptions

### 8.1 tb_pipeline - Full Pipeline Verification

Verifies the complete ISP pipeline produces correct output by comparing against a golden reference computed from the C++ reference implementation.

```bash
./build/bremen/components/isp_tlm/systemc/tb_pipeline
```

**What it tests:**
- End-to-end correctness (bit-exact MSE = 0)
- Frame processing time measurement
- AWB/AEC feedback values

### 8.2 tb_arch_pipeline - Architecture-Aware Timed Pipeline

Demonstrates the full architecture model with clock and reset signals, timed block processing, architecture metrics collection, and bottleneck analysis.

```bash
./build/bremen/components/isp_tlm/systemc/tb_arch_pipeline
```

**What it measures:**
- Per-block utilization (active/starved/blocked cycles)
- Frame timing and FPS estimation
- Bottleneck identification
- Architecture metrics CSV output

**Output structure:**
```
output/arch_metrics/
├── frame_metrics.csv
├── block_metrics.csv
├── link_metrics.csv
└── bottleneck_report.txt
```

### 8.3 tb_power_metrics - Power Estimation

Estimates power consumption for each ISP block using activity-based models.

```bash
./build/bremen/components/isp_tlm/systemc/tb_power_metrics
```

**Power breakdown by component:**
- Static power (leakage)
- Dynamic power (switching)
- Memory power (SRAM/line buffers)

**Example output:**
```
--- Per-Block Power Estimation ---
         Block      Util %   Static mW  Dynamic mW   Memory mW    Total mW
--------------------------------------------------------------------------
         blc       92.0%       0.300     190.771       0.000     191.071
      demosaic       70.0%       2.000   28449.793    2394.112   30845.905
           ...
         TOTAL                 13.100   70340.662   16905.434   87259.196

--- Configuration Comparison ---
              Config            FPS   Frame Energy      Avg Power
-----------------------------------------------------------------
        1080p@200MHz           96.5      904703.3 nJ       87259.2 mW
           4K@200MHz           24.1     3618813.4 nJ       87259.2 mW
        1080p@400MHz          192.9      452351.7 nJ       87259.2 mW
```

### 8.4 tb_arch_sweep - Architecture Parameter Sweeps

Runs architecture exploration sweeps across resolution, clock frequency, and block enables to identify optimal configurations.

```bash
./build/bremen/components/isp_tlm/systemc/tb_arch_sweep
```

**Sweep types:**
1. **Resolution sweep** - Test different image sizes
2. **Frequency sweep** - Test different clock frequencies
3. **Block enable sweep** - Test critical path analysis

**Example output:**
```
=== Starting Sweep: resolution_sweep ===
Total points: 5

[1/5] resolution=640x480 ... OK (FPS=651.0, Power=7.4mW)
[2/5] resolution=1280x720 ... OK (FPS=217.0, Power=22.2mW)
[3/5] resolution=1920x1080 ... OK (FPS=96.5, Power=50.0mW)
[4/5] resolution=2560x1440 ... OK (FPS=54.3, Power=88.9mW)
[5/5] resolution=3840x2160 ... OK (FPS=24.1, Power=200.0mW)

--- Energy Efficiency ---
Best FPS/mW: 87.879

--- Bottleneck Distribution ---
bandwidth: 2 configs (40.0%)
compute: 3 configs (60.0%)

Exported to: output/sweeps/resolution/resolution_sweep.csv
```

### 8.5 tb_dma_integration - DMA and Memory Model Integration

Demonstrates DMA and memory model integration with bandwidth throttling.

```bash
./build/bremen/components/isp_tlm/systemc/tb_dma_integration
```

**Features:**
- DMA burst simulation
- Bandwidth limiting on input/output
- Frame DMA models
- Local memory (line buffer) models

**Key configurations:**
- Input DMA: 64-bit bus, 16-beat bursts, 800 Mbps limit
- Output DMA: 64-bit bus, 16-beat bursts, 400 Mbps limit

### 8.6 tb_metrics_demo - Metrics Wrapper Demonstration

Single-block demonstration of the metrics wrapper between Generic_Driver and sc_blc.

```bash
./build/bremen/components/isp_tlm/systemc/tb_metrics_demo
```

**Demonstrates:**
- Per-token latency CSV logging
- Summary statistics
- Block-level metrics collection

### 8.7 tb_d65_pipeline / tb_d65_blc - Real Image Tests

Tests with real D65 illuminant RAW images for validation.

```bash
./build/bremen/components/isp_tlm/systemc/tb_d65_pipeline
./build/bremen/components/isp_tlm/systemc/tb_d65_blc
```

**Results:**
- `tb_d65_blc`: **PASS** (bit-exact MSE = 0)
- `tb_d65_pipeline`: Full 17-block streaming verified

---

## 9. Quick Reference

### Build All
```bash
cd /home/hoangquan/workspace/CDC-VP
./components/isp_tlm/systemc/build.sh
```

### Run All Block Tests
```bash
cd build/bremen/components/isp_tlm/systemc
for tb in tb_blc tb_dpc tb_dg tb_wb tb_ccm tb_gc tb_csc tb_cse tb_lsc tb_bnr tb_demosaic tb_sharpen tb_2dnr tb_awb tb_aec tb_scale tb_yuv420; do
  ./$tb && echo "PASS: $tb" || echo "FAIL: $tb"
done
```

### Run Architecture Tests
```bash
./tb_power_metrics
./tb_arch_sweep
./tb_arch_pipeline
./tb_dma_integration
```

### Run via CTest
```bash
cd build/bremen
ctest --output-on-failure
```

---

## 10. Troubleshooting

| Symptom | Cause | Solution |
|---------|-------|----------|
| `undefined reference to sc_core::sc_module_name` | Missing `-lsystemc` or rpath | Add `-Wl,-rpath,/opt/systemc-2.3.4/lib64` |
| `fatal error: systemc.h: No such file` | Missing include path | Check `SYSTEMC_HOME` in CMake |
| Simulator hangs (deadlock) | FIFO mismatch | Verify input/output token counts match W×H |
| Test reports `FAIL` | Float precision or config | Check "Differences / max difference" output |
| Build fails | Missing dependencies | Run `setup_third_party.sh` |

---

## 11. Architecture Model Files

### Hardware Infrastructure (hw/)

| File | Purpose |
|------|---------|
| `isp_arch_config.h` | Architecture configuration, block parameters |
| `metrics.h` | Architecture metrics collection, bottleneck analysis |
| `power.h` | Power estimation models, block power defaults |
| `sweep.h` | Parameter sweep execution and results |
| `timed_stream.h` | Clocked instrumented FIFO with bandwidth throttling |
| `timed_block.h` | Hardware shell base class |
| `local_memory.h` | Line buffer and memory models |
| `frame_dma.h` | DMA transaction models |
| `stream_beat.h` | Frame-aware stream types |
| `trace.h` | VCD tracing utilities |

### Key Configuration Structures

```cpp
// Architecture configuration
isp_arch_config arch_cfg;
arch_cfg.clock_freq_mhz = 200.0f;
arch_cfg.enable_metrics = true;
arch_cfg.init_defaults();

// Hardware parameters
hw_params hw;
hw.clk_mhz = 200.0f;
hw.bus_width_bits = 64;
hw.pixel_bits = 16;
hw.fifo_depth = 1024;
hw.timed_mode = true;
```
