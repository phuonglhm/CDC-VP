# ISP SystemC Architecture Model — Build & Run Guide

This document describes the SystemC ISP pipeline model in CDC-VP: what it is, how it is built, how to run it, and what the output means.

---

## 1. Overview

The model is a **SystemC TLM (Transaction-Level Modeling) simulation** of a complete 18-stage image signal processor (ISP) that converts raw Bayer sensor data into display-ready YUV420. It supports:

- **Untimed mode** (default) — functional verification, cycle-accurate pixel outputs, no timing delays.
- **Timed mode** — clock-synchronized block processing, hardware-level cycle accounting, bottleneck analysis, power estimation.

The model sits in `components/isp_tlm/systemc/` and builds into `build/bremen/components/isp_tlm/systemc/`.

---

## 2. Directory Structure

```
components/isp_tlm/systemc/
├── CMakeLists.txt              # Build configuration; registers all testbenches with CTest
├── build.sh                    # Recommended build wrapper (calls cmake + ninja)
│
├── hw/                         # Hardware architecture infrastructure
│   ├── hw.h                    # Central include: pulls in everything below
│   ├── isp_arch_config.h       # Block/link indices, bus/DMA/block default configs
│   ├── metrics.h               # arch_metrics_collector, bottleneck analysis, power
│   ├── power.h                 # Per-block power models (static/dynamic/memory)
│   ├── sweep.h                 # Parameter sweep execution
│   ├── stream_beat.h           # Frame-aware stream token types
│   ├── timed_stream.h          # Clocked instrumented FIFO with bandwidth throttling
│   ├── timed_block.h           # Hardware shell base class (timed/untimed abstraction)
│   ├── local_memory.h          # Line buffer and SRAM models
│   ├── frame_dma.h             # DMA transaction models (input/output bandwidth)
│   └── trace.h                 # VCD tracing utilities
│
├── tb_utils/                   # Shared testbench infrastructure
│   ├── tb_utils.h              # Generic_Driver<T>, Generic_Monitor<T>
│   ├── hardware_params.h       # hw_params: clock, bus width, FIFO depth, timed_mode
│   ├── sc_block_metrics.h      # Per-block latency collector (Layer 1)
│   ├── sc_metrics_wrapper.h    # Passive inter-block FIFO observer (Layer 2)
│   └── metrics_demo_tb.cpp     # Metrics wrapper demo (single-block, blc)
│
├── blocks/                     # 17 ISP processing blocks + input normalizer
│   ├── input_normalizer/       # (in pipeline/ directory instead)
│   ├── blc/                    # sc_blc.{h,cpp}  — Black Level Correction
│   ├── dpc/                    # sc_dpc.{h,cpp}  — Defect Pixel Correction
│   ├── lsc/                    # sc_lsc.{h,cpp}  — Lens Shading Correction
│   ├── dg/                     # sc_dg.{h,cpp}   — Digital Gain
│   ├── bnr/                    # sc_bnr.{h,cpp}  — Bad Pixel Replacement
│   ├── demosaic/               # sc_demosaic.{h,cpp} — Demosaicing (RGB→Bayer)
│   ├── awb/                    # sc_awb.{h,cpp}  — Auto White Balance (statistics)
│   ├── wb/                     # sc_wb.{h,cpp}   — White Balance
│   ├── ccm/                    # sc_ccm.{h,cpp}  — Color Correction Matrix
│   ├── gc/                     # sc_gc.{h,cpp}   — Gamma Correction
│   ├── aec/                    # sc_aec.{h,cpp}  — Auto Exposure Control (statistics)
│   ├── csc/                    # sc_csc.{h,cpp}  — Color Space Conversion (RGB→YUV444)
│   ├── cse/                    # sc_cse.{h,cpp}  — Color Space Enhancement
│   ├── sharpen/                # sc_sharpen.{h,cpp} — Sharpening
│   ├── 2dnr/                   # sc_2dnr.{h,cpp} — 2D Noise Reduction
│   ├── scale/                  # sc_scale.{h,cpp} — Scaling / downsampling
│   └── yuv420/                 # sc_yuv420.{h,cpp} — YUV444→YUV420 conversion
│
├── pipeline/
│   ├── sc_input_normalizer.{h,cpp}  # Input bit-depth normalization (12/14/16 → 12-bit)
│   ├── sc_isp_pipeline.{h,cpp}      # Top-level pipeline module; instantiates all 18 blocks
│   ├── tb_pipeline.cpp               # Full pipeline functional verification
│   ├── tb_arch_pipeline.cpp         # Timed pipeline + architecture metrics
│   ├── tb_power_metrics.cpp         # Per-block power estimation
│   ├── tb_arch_sweep.cpp            # Architecture parameter sweeps
│   └── tb_dma_integration.cpp       # DMA and memory model integration
│
└── docs/
    └── MODEL_EVALUATION.md           # Model evaluation report
```

### Data flow through the pipeline

```
RAW Input (12/14/16-bit Bayer)
  │
  ▼
input_normalizer (bit-depth normalization)
  │
  ▼
blc  →  dpc  →  lsc  →  dg  →  bnr  →  demosaic
                                              │
RAW domain                               ▼
                                   awb (statistics)
                                   wb   →  ccm  →  gc  →  aec (statistics)
                                   │
                                   ▼
RGB domain                         csc  →  cse  →  sharpen  →  2dnr
                                                    │
YUV444 domain                                          ▼
                                                 scale (optional)
                                                      │
                                                      ▼
                                                 yuv420
                                                      │
YUV420 Output                                            ▼
```

---

## 3. Environment Requirements

| Component | Version / Notes |
|-----------|---------------|
| SystemC | 2.3.4 at `/opt/systemc-2.3.4` |
| CMake | ≥ 3.16 |
| Compiler | `g++` ≥ 7 (C++17) |
| Ninja | Recommended (faster than make) |

Verify your setup:

```bash
ls /opt/systemc-2.3.4/include/systemc.h
ls /opt/systemc-2.3.4/lib/libsystemc.so
g++ --version
cmake --version
```

---

## 4. Build

### 4.1 Recommended: via the wrapper script

From the CDC-VP root:

```bash
cd /home/hoangquan/workspace/CDC-VP

# Build everything (cmake configure + ninja)
./components/isp_tlm/systemc/build.sh

# Rebuild after editing source
ninja -C build/bremen isp_tlm

# Clean and rebuild from scratch
./components/isp_tlm/systemc/build.sh clean
./components/isp_tlm/systemc/build.sh
```

### 4.2 Manual cmake

```bash
cmake -S . -B build/bremen -G Ninja \
  -DCMAKE_C_COMPILER=/usr/bin/gcc \
  -DCMAKE_CXX_COMPILER=/usr/bin/g++ \
  -DCDC_BUILD_TESTS=ON \
  -DSYSTEMC_INCLUDE_DIR=/opt/systemc-2.3.4/include \
  -DSYSTEMC_LIBRARY=/opt/systemc-2.3.4/lib/libsystemc.so

cmake --build build/bremen
```

### 4.3 Build a specific testbench

```bash
ninja -C build/bremen tb_pipeline
ninja -C build/bremen tb_arch_pipeline
ninja -C build/bremen tb_d65_pipeline
```

All built executables land in `build/bremen/components/isp_tlm/systemc/`.

---

## 5. Running Tests

### 5.1 Pipeline testbenches (recommended)

```bash
BUILD=build/bremen/components/isp_tlm/systemc

# Functional verification with synthetic pattern
$BUILD/tb_pipeline

# Full pipeline with real D65 image (2688×1520, 16-bit RGGB)
$BUILD/tb_d65_pipeline

# BLC only with real D65 image
$BUILD/tb_d65_blc

# Timed mode — clock-synchronized, cycle accounting, bottleneck analysis
$BUILD/tb_arch_pipeline

# Per-block power breakdown
$BUILD/tb_power_metrics

# Architecture parameter sweeps (resolution, frequency, block enables)
$BUILD/tb_arch_sweep

# DMA and memory model integration
$BUILD/tb_dma_integration

# Single-block metrics demo (blc + wrapper)
$BUILD/tb_metrics_demo
```

### 5.2 Individual block testbenches (17 blocks)

```bash
BUILD=build/bremen/components/isp_tlm/systemc
for tb in $BUILD/tb_{blc,dpc,dg,wb,ccm,gc,csc,cse,lsc,bnr,demosaic,sharpen,2dnr,awb,aec,scale,yuv420}; do
  $tb && echo "PASS: $tb" || echo "FAIL: $tb"
done
```

Each block testbench verifies correctness against a golden reference. Expected output:

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

### 5.3 CTest (all registered tests)

```bash
cd build/bremen

# List all registered tests
ctest -N

# Run all tests, show output on failure
ctest --output-on-failure

# Run only SystemC tests
ctest -R '^tb_' --output-on-failure

# Run only pipeline tests
ctest -R 'pipeline' --output-on-failure

# Run in parallel
ctest -j$(nproc) --output-on-failure
```

---

## 6. Timed vs Untimed Mode

Every ISP block supports two simulation modes controlled by `hw_params.timed_mode`:

| | Untimed (`timed_mode = false`) | Timed (`timed_mode = true`) |
|---|---|---|
| **Clock** | No clock port bound | `sc_in<bool>* clk` bound via `sc_isp_pipeline::bind_clock()` |
| **FIFO waits** | Blocking `read()`/`write()` only | `wait(clk->posedge_event())` before each read |
| **Processing** | No `wait()` in processing loops | `wait(clk->posedge_event())` in timed processing/output loops |
| **Timing** | `sc_time_stamp()` stays at 0 | Real cycle-accurate delays |
| **Use case** | Functional verification, MSE vs golden reference | Hardware cycle accounting, bottleneck analysis, power |

To enable timed mode in a testbench:

```cpp
hw_params hw;
hw.timed_mode = true;
hw.clk_mhz = 200.0f;
hw.bus_width_bits = 64;
hw.pixel_bits = 16;

sc_isp_pipeline pipeline("isp", cfg, lsc_lut, raw_in_fifo, yuv_out_fifo,
                          12, cfa_types::RGGB, &hw);

// Create and bind a clock (200 MHz, 50% duty cycle)
sc_clock* clk = new sc_clock("clk", sc_time(5.0, SC_NS), 0.5);
pipeline.bind_clock(clk);

sc_start();
```

---

## 7. Hardware Parameters

`hw_params` (defined in `tb_utils/hardware_params.h`) drives all timing and throughput calculations:

```cpp
hw_params hw;
hw.clk_mhz          = 200.0f;   // Clock frequency (MHz)
hw.bus_width_bits   = 64;        // Data bus width (bits/cycle)
hw.pixel_bits       = 16;        // Token size (uint16_t=16, uint8_t=8)
hw.fifo_depth       = 1024;      // Inter-block FIFO depth
hw.timed_mode       = true;      // Enable clock-synchronized waits
hw.default_cycles_per_pixel = 1; // Processing cycles per pixel

// Derived:
hw.cycle_ns()            // 5.0 ns @ 200 MHz
hw.tokens_per_cycle()     // 4.0 @ 64-bit / 16-bit
hw.throughput_mpixels_s() // 50.0 Mpixels/s @ 200 MHz, 4 tokens/cycle
```

---

## 8. Metrics Collection

The model uses a **three-layer metrics architecture**:

```
RAW Input
  │
  ▼  Layer 1: sc_block_metrics<T>  (per-block processing latency)
  ┌─────────────────────────────────────────────────────────────┐
  │  normalizer → blc → dpc → lsc → dg → bnr → demosaic  ... │
  │  → block_<name>.csv  +  block_<name>_summary.txt           │
  └─────────────────────────────────────────────────────────────┘
  │
  ▼  Layer 2: sc_metrics_wrapper<T>  (inter-block boundary throughput)
  ┌─────────────────────────────────────────────────────────────┐
  │  17 wrapper instances between every adjacent block pair       │
  │  → <upstream>_to_<downstream>.csv  +  <upstream>_to_<downstream>_summary.txt │
  └─────────────────────────────────────────────────────────────┘
  │
  ▼  Layer 3: arch_metrics_collector  (frame timing, bottleneck, power)
  ┌─────────────────────────────────────────────────────────────┐
  │  → frame_metrics.csv, block_metrics.csv, link_metrics.csv    │
  │  → bottleneck_report.txt, power_metrics.csv                  │
  └─────────────────────────────────────────────────────────────┘
  │
YUV420 Output
```

### Enabling metrics

```cpp
// Method A: set static request before pipeline construction
sc_isp_pipeline::set_metrics_request(true, "output/metrics");

// Method B: set public fields after construction (but before sc_start)
sc_isp_pipeline pipeline(...);
pipeline.enable_metrics = true;
pipeline.metrics_output_dir = "output/metrics";

// After sc_start()
pipeline.dump_pipeline_metrics();    // Layer 2 CSVs
pipeline.dump_all_block_metrics();  // Layer 1 CSVs
pipeline.print_metrics_summary();    // Console table
```

### Output structure

```
output/metrics/
├── block_normalizer.csv              # Layer 1: per-block latency
├── block_normalizer_summary.txt
├── block_blc.csv
├── block_blc_summary.txt
├── ...
├── block_yuv420.csv
├── block_yuv420_summary.txt
│
├── input_norm_to_blc.csv             # Layer 2: boundary throughput
├── input_norm_to_blc_summary.txt
├── blc_to_dpc.csv
├── blc_to_dpc_summary.txt
├── ...
├── scale_to_yuv420.csv
└── scale_to_yuv420_summary.txt

output/arch_metrics/                  # Layer 3 (timed mode only)
├── frame_metrics.csv
├── block_metrics.csv
├── link_metrics.csv
├── bottleneck_report.txt
├── power_metrics.csv
└── power_summary.txt
```

---

## 9. Test Images

| File | Resolution | Bit Depth | Pattern |
|------|-----------|-----------|---------|
| `D65_raw_2688x1520_5376.raw` | 2688×1520 | 16-bit RGGB | D65 illuminant |
| `ColorChecker_2592x1536_12bits_RGGB.raw` | 2592×1536 | 12-bit RGGB | Color checker |
| `A_raw_2688x1520_5376.raw` | 2688×1520 | 16-bit RGGB | A illuminant |

Located in `components/isp_tlm/input/`.

---

## 10. Quick Reference

### Build all
```bash
./components/isp_tlm/systemc/build.sh
```

### Run pipeline tests
```bash
./build/bremen/components/isp_tlm/systemc/tb_pipeline
./build/bremen/components/isp_tlm/systemc/tb_d65_pipeline
./build/bremen/components/isp_tlm/systemc/tb_arch_pipeline
```

### Run architecture tests
```bash
./build/bremen/components/isp_tlm/systemc/tb_power_metrics
./build/bremen/components/isp_tlm/systemc/tb_arch_sweep
./build/bremen/components/isp_tlm/systemc/tb_dma_integration
```

### Run all via CTest
```bash
cd build/bremen && ctest -R '^tb_' --output-on-failure
```

### Post-process metrics
```bash
python3 components/isp_tlm/systemc/tb_utils/metrics_summary.py
```

---

## 11. Troubleshooting

| Symptom | Cause | Solution |
|---------|-------|---------|
| `fatal error: systemc.h: No such file` | Missing SystemC include path | Check `SYSTEMC_INCLUDE` in CMake |
| `undefined reference to sc_core::sc_module_name` | Missing `-lsystemc` or rpath | Add `-Wl,-rpath,/opt/systemc-2.3.4/lib` |
| Simulator hangs (deadlock) | FIFO size mismatch with W×H | Verify token count matches frame pixels |
| Test reports `FAIL` | Float precision or config mismatch | Check "Differences / max difference" output |
| Latency values all 0 | Running in untimed mode | Set `hw_params.timed_mode = true` and bind a clock |
| Build fails | Stale object files | `rm -rf build/bremen && ./build.sh` |

---

## 12. Architecture Model Files

### `hw/` — Hardware Infrastructure

| File | Purpose |
|------|---------|
| `isp_arch_config.h` | Block/link indices and per-block default configurations |
| `metrics.h` | `arch_metrics_collector`, bottleneck classification, power estimation |
| `power.h` | Per-block power models; static/dynamic/memory breakdown |
| `sweep.h` | Parameter sweep execution across resolution, frequency, enables |
| `timed_stream.h` | Clocked instrumented FIFO with bandwidth throttling |
| `timed_block.h` | Hardware shell base class (timed/untimed abstraction) |
| `local_memory.h` | Line buffer and SRAM models |
| `frame_dma.h` | DMA transaction models (input/output bandwidth) |
| `stream_beat.h` | Frame-aware stream token types |
| `trace.h` | VCD tracing utilities |

### Key configuration structs

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
