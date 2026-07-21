# ISP SystemC Architecture Model — Build & Run Guide

This guide describes the active SystemC ISP line-granular model, how to build it, how to run the focused smoke, and how to interpret its report.

## 1. Overview

The model is an 18-stage image signal processor that converts RAW Bayer input to YUV420 output.

The architecture model uses one `isp_arch_config` clock for SystemC scheduling.

The internal observation window runs from the first RAW line at the ideal source boundary through the last YUV line at the ideal sink boundary.

The functional pipeline remains the byte-exact untimed oracle.

The focused architecture smoke is `tb_arch_pipeline`.

The architecture metrics are internal to the ISP model and do not represent external interface capacity or implementation-level timing claims.

## 2. Directory Structure

The model is in `components/isp_tlm/systemc/`.

The main directories are:

- `hw/` contains architecture configuration, line channels, stage runtime, and metric support.
- `tb_utils/` contains shared testbench utilities and architecture configuration helpers.
- `blocks/` contains the ISP processing blocks.
- `pipeline/` contains the top-level pipeline and testbenches.
- `docs/` contains this guide and the model evaluation report.

The pipeline contains the input normalizer and 17 processing stages.

The reported architecture has 18 link records consisting of the 17 inter-stage links from `input_norm_to_blc` through `scale_to_yuv420` plus the `yuv420_to_output` ideal sink-boundary link.
The ideal RAW source boundary is captured by frame ingress metrics and is not emitted as a reported line row.
The reported link names are `input_norm_to_blc`, `blc_to_dpc`, `dpc_to_lsc`, `lsc_to_dg`, `dg_to_bnr`, `bnr_to_demosaic`, `demosaic_to_awb`, `awb_to_wb`, `wb_to_ccm`, `ccm_to_gc`, `gc_to_aec`, `aec_to_csc`, `csc_to_cse`, `cse_to_sharpen`, `sharpen_to_2dnr`, `2dnr_to_scale_or_yuv420`, `scale_to_yuv420`, and `yuv420_to_output`.

## 3. Environment Requirements

| Component | Version / Notes |
|-----------|-----------------|
| SystemC | 2.3.4 at `/opt/systemc-2.3.4` |
| CMake | ≥ 3.16 |
| Compiler | `g++` ≥ 7 with C++17 support |
| Ninja | Recommended |

Verify the required installation with:

```bash
ls /opt/systemc-2.3.4/include/systemc.h
ls /opt/systemc-2.3.4/lib/libsystemc.so
g++ --version
cmake --version
```

## 4. Build

From the CDC-VP root, use:

```bash
./components/isp_tlm/systemc/build.sh
```

An incremental build can be performed with:

```bash
ninja -C build/bremen isp_tlm
```

A manual configure and build is:

```bash
cmake -S . -B build/bremen -G Ninja \
  -DCMAKE_C_COMPILER=/usr/bin/gcc \
  -DCMAKE_CXX_COMPILER=/usr/bin/g++ \
  -DCDC_BUILD_TESTS=ON \
  -DSYSTEMC_INCLUDE_DIR=/opt/systemc-2.3.4/include \
  -DSYSTEMC_LIBRARY=/opt/systemc-2.3.4/lib/libsystemc.so
cmake --build build/bremen
```

Build a focused architecture executable with:

```bash
ninja -C build/bremen tb_arch_pipeline
```

Executables are placed in `build/bremen/components/isp_tlm/systemc/`.

## 5. Running the Focused Smoke

Run the functional byte-exact oracle with:

```bash
BUILD=build/bremen/components/isp_tlm/systemc
$BUILD/tb_pipeline
```

Run the architecture smoke and select the report path explicitly with:

```bash
$BUILD/tb_arch_pipeline --metrics /tmp/isp_arch_snapshot.csv
```

The caller supplies the report filename after `--metrics`; `write_metrics` does not create parent directories, so any alternate parent directory must already exist.

The default tests write no source artifacts.

Run an architecture sweep comparison with:

```bash
$BUILD/tb_arch_sweep --compare
```

Sweep results are kept in memory and presented on standard output without CSV export.

The focused smoke demonstrates the architecture snapshot path and must not be read as broad suite coverage.

## 6. Retained Architecture Parameters

`isp_arch_config` owns the clock, per-block pixels-per-cycle (PPC), pixel initiation interval (II), pipeline latency, maximum in-flight lines, optional modeled workload and local-memory profiles, and per-link depth.
Stage enable/bypass controls come from functional `isp_config`, not `isp_arch_config`.

The clock period is derived from the configured clock frequency and is used by SystemC scheduling.

A line is exactly one `line_channel` token representing one logical image row.

RGB and YUV channel multiplicity does not multiply the logical pixel count.

A processing beat is one internal group of up to PPC logical pixels.

For logical pixel count $P$ and PPC $Q$, the exact equation is:

```text
processing_beats = ceil(P / Q)
```

For pixel initiation interval $I$, the exact compute equation is:

```text
compute_cycles = processing_beats * I
```

Processing beats are internal model units and are not bus beats.

## 7. Metrics and Provenance

The pipeline API is `metrics()` with optional `write_metrics(file)`.

`metrics()` returns one immutable unified snapshot of frame, stage, and link metrics.

`write_metrics(file)` is the only metric report filesystem side effect.

Sweep commands consume in-memory snapshots and present results on standard output without CSV export.

The observation window is one frame from the first RAW line entering the ideal source boundary through the last YUV line leaving the ideal sink boundary.

All cycle values are integer clock cycles.

Time values are microseconds unless a report field states otherwise.

A measured value is accumulated at an event boundary in the timed SystemC model.

A modeled value is an architecture assumption derived from configured demand and retains its profile provenance.

An unavailable value means the required profile or event was not supplied.

Unavailable values are reported with an explicit `unavailable` availability marker and are never replaced by invented zeroes.

### Frame equations

| Metric | Equation | Unit | Provenance |
| `first_output_latency_cycles` | `ceil((first_output_time - first_input_time) / cycle_period)` | cycles | derived from measured timestamps |
| `first_output_latency_us` | `(first_output_time - first_input_time) / 1us` | microseconds | derived from measured timestamps |
| `frame_cycles` | `ceil((last_output_time - first_input_time) / cycle_period)` | cycles | derived from measured timestamps |
| `achieved_pixels_per_cycle` | `input_raw_logical_pixels / frame_cycles` | pixels/cycle | derived from measured events |
| `input_bandwidth_mbps` | `input_bytes * 8 / frame_time_us` | Mbps | derived from measured logical bytes |
| `output_bandwidth_mbps` | `output_bytes * 8 / frame_time_us` | Mbps | derived from measured logical bytes |
| `input_bytes`, `output_bytes` | logical samples crossing ideal ISP boundaries | bytes | measured |

These bandwidth values describe internal ideal-boundary traffic only.

They are not measurements of a memory system or external interface.

### Stage and link metrics

Each stage reports input and output lines, logical pixels, processing beats, active cycles, bypass cycles, input-starved cycles, output-blocked cycles, completion-wait cycles, utilization, and effective II when their events are available.

Stage durations can overlap for in-flight lines and are not a cycle partition.

`utilization` is `min(1, active_cycles / stage_observation_cycles)` for enabled stages and zero for bypass.

`effective_ii` is `issue_window_cycles / processing_beats` and is measured-derived.

A link is a bounded preallocated `line_channel` whose depth is its only link architecture parameter.

Published, read, and released lines, logical bytes, occupancy high water, wait event counts, and completed wait durations are measured at their corresponding line-channel events.

Workload operation counts and local-memory service or wait metrics remain unavailable without explicit modeled workload and local-memory profiles.

If profiles are supplied, the model uses:

```text
read_cycles = ceil(reads_per_pixel * P / read_ports) * access_cycles
write_cycles = ceil(writes_per_pixel * P / write_ports) * access_cycles
memory_service_cycles = max(read_cycles, write_cycles)
line_issue_cycles = max(compute_cycles, memory_service_cycles)
memory_wait_cycles = max(0, memory_service_cycles - compute_cycles)
```

Zero demand has zero cycles and does not require a port.

## 8. Test Images and Functional Material

The existing functional test images and block-level verification material remain available under `components/isp_tlm/input/` and the pipeline testbenches.

The byte-exact untimed oracle is the reference for functional output comparison.

This guide does not claim that the focused architecture smoke validates the entire test suite.

## 9. Troubleshooting

| Symptom | Cause | Solution |
|---------|-------|---------|
| `fatal error: systemc.h: No such file` | Missing SystemC include path | Check the SystemC CMake paths. |
| `undefined reference to sc_core::sc_module_name` | Missing SystemC library or runtime path | Check the SystemC link and runtime paths. |
| Test reports `FAIL` | Functional output or configuration mismatch | Inspect the reported differences and configuration. |
| Metrics field is `unavailable` | Required profile or event was not supplied | Provide the explicit modeled profile when that metric is required. |
| Report is missing | No explicit report path was supplied | Pass `--metrics <caller-file>` to `tb_arch_pipeline`. |

## 10. Quick Reference

```bash
./components/isp_tlm/systemc/build.sh
BUILD=build/bremen/components/isp_tlm/systemc
$BUILD/tb_pipeline
$BUILD/tb_arch_pipeline --metrics /tmp/isp_arch_snapshot.csv
$BUILD/tb_arch_sweep --compare
```

The commands above cover the functional oracle, the focused architecture smoke, and sweep comparison.

They do not imply broad suite coverage.
