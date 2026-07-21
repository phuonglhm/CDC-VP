# SystemC ISP Model Parameter and Metric Summary

This document summarizes the hardware parameters, metrics output structure, run commands, and legacy components for the SystemC ISP line-granular architectural model.

## 1. Configurable Hardware Parameters

The architectural simulation configures performance assumptions using `isp_arch_config` (declared in `hw/isp_arch_config.h`).

### Global Parameters
* **`clock_freq_mhz`** (float)
  * The clock frequency driving all SystemC scheduling and cycle conversions.
  * Default is 200.0 MHz.

### Stage Scheduling Parameters (per block/stage)
* **`pixels_per_cycle`** (uint32_t)
  * Number of pixels processed per clock cycle (PPC).
  * Default is 1.
* **`pixel_initiation_interval_cycles`** (uint32_t)
  * Cycle spacing between consecutive processing beats.
  * Default is 1.
* **`pipeline_latency_cycles`** (uint32_t)
  * Clock cycle latency from line token issue to completion.
  * Default is 1 for point, spatial, and frame stages, and 2 for rate-change stages `scale` and `yuv420`.
* **`max_in_flight_lines`** (uint32_t)
  * Maximum number of line tokens allowed concurrently inside the stage scheduler.
  * Default is 2 for point, frame, and rate-change stages, and 6 for spatial stages `dpc`, `bnr`, `demosaic`, `sharpen`, and `2dnr`.

### Link Parameters (per link/FIFO)
* **`depth`** (uint32_t)
  * Bounded queue slot capacity of the `line_channel` connecting two stages.
  * Default is 1024.

### Analytical Workload Profiles (optional)
* **`additions_per_pixel`** (uint32_t)
* **`multiplications_per_pixel`** (uint32_t)
* **`comparisons_per_pixel`** (uint32_t)
* **`reads_per_pixel`** (uint32_t)
* **`writes_per_pixel`** (uint32_t)

### Local Memory Profiles (optional)
* **`read_ports`** (uint32_t)
  * Read port count on the local stage SRAM.
* **`write_ports`** (uint32_t)
  * Write port count on the local stage SRAM.
* **`access_cycles`** (uint32_t)
  * Clock cycle latency per port access transaction.

### Sweep Runner Parameter Names

The `tb_arch_sweep` runner accepts these parameter names internally when constructing architecture candidates.
* **`frequency`**: Maps to `clock_freq_mhz`.
* **`ppc`**: Maps to `pixels_per_cycle` for every block.
* **`pixel_ii`**: Maps to `pixel_initiation_interval_cycles` for every block.
* **`pipeline_latency`**: Maps to `pipeline_latency_cycles` for every block.
* **`max_in_flight`**: Maps to `max_in_flight_lines` for every block.
* **`link_depth`**: Maps to `depth` for every link.
* **`downstream_latency`**: Overrides `pipeline_latency_cycles` for the YUV420 stage.
* **`downstream_max_in_flight`**: Overrides `max_in_flight_lines` for the YUV420 stage.
* **`memory_read_ports`**, **`memory_write_ports`**, and **`memory_access_cycles`** enable the modeled workload and local-memory profile for every block.
* **`dpc`**, **`bnr`**, and **`sharpen`** accept `on` or `off` to change functional block enablement.

The public `sc_isp_pipeline` constructor accepts a complete `isp_arch_config` rather than parsing these names directly.

The 18 stage slots are `normalizer`, `blc`, `dpc`, `lsc`, `dg`, `bnr`, `demosaic`, `awb`, `wb`, `ccm`, `gc`, `aec`, `csc`, `cse`, `sharpen`, `2dnr`, `scale`, and `yuv420`.

The 18 link slots are `input_norm_to_blc`, `blc_to_dpc`, `dpc_to_lsc`, `lsc_to_dg`, `dg_to_bnr`, `bnr_to_demosaic`, `demosaic_to_awb`, `awb_to_wb`, `wb_to_ccm`, `ccm_to_gc`, `gc_to_aec`, `aec_to_csc`, `csc_to_cse`, `cse_to_sharpen`, `sharpen_to_2dnr`, `2dnr_to_scale_or_yuv420`, `scale_to_yuv420`, and `yuv420_to_output`.

Workload and local-memory profiles are accepted only with modeled provenance, and invalid zero or overflowing values are rejected.

### Functional Inputs That Are Not Hardware Parameters

The pipeline also accepts an `isp_config`, an LSC lookup table, input bit depth, Bayer pattern, and RAW/YUV FIFO boundaries.

These inputs select functional processing and data layout rather than architectural timing resources.



## 2. Emitted Metrics

The pipeline outputs one unified `pipeline_metrics` struct (defined in `hw/metrics.h`) containing:

### Frame Metrics
* `first_output_latency_cycles`: Clock cycles from the first RAW line input to the first YUV line output.
* `first_output_latency_us`: Latency in microseconds.
* `frame_cycles`: Clock cycles from the first input line to the last output line.
* `achieved_pixels_per_cycle`: Logical pixels processed per frame cycle.
* `input_bandwidth_mbps` / `output_bandwidth_mbps`: Bandwidth computed at ideal-boundary traffic crossings.
* `input_bytes` / `output_bytes`: Byte totals crossing boundaries.

### Stage (Block) Metrics
* Input and output lines.
* Logical pixels and processing beats.
* Active cycles, bypass cycles, input-starved cycles, output-blocked cycles, and completion-wait cycles.
* Utilization and effective initiation interval (II).
* Optional modeled memory wait cycles and operation counts.

### Link (Channel) Metrics
* Published, read, and released line counts.
* High-water mark occupancy.
* Total logical bytes transferred.
* Producer/consumer wait event counts and durations (in seconds and cycles).

### Bottleneck Candidates
* Ranked list of bottleneck candidates categorized by type (`input_starvation`, `downstream_credit`, `memory_port`, `compute_ii`) and severity.

## 3. How to Run

Commands must be run inside the container workspace.

### Build
Build all targets using the build script:
```bash
 ./components/isp_tlm/systemc/build.sh
```

Or compile the individual smoke executables manually:
```bash
 ninja -C build/bremen tb_arch_pipeline tb_arch_sweep
```

### Run tb_arch_pipeline
Runs a single frame simulation and optionally dumps metrics:
```bash
 build/bremen/components/isp_tlm/systemc/tb_arch_pipeline --print-metrics
```

Write the metrics directly to a CSV report:
```bash
 build/bremen/components/isp_tlm/systemc/tb_arch_pipeline --metrics /tmp/metrics.csv
```

### Run tb_arch_sweep
Runs resolution, frequency, block enablement, and sensitivity sweeps:
```bash
 build/bremen/components/isp_tlm/systemc/tb_arch_sweep --all
```

Compare candidate architecture variants:
```bash
 build/bremen/components/isp_tlm/systemc/tb_arch_sweep --compare
```

## 4. Legacy and Unused Parts

* **`hw_params`** (`tb_utils/hardware_params.h`): Legacy clock, bus-width, pixel-width, FIFO-depth, and timed-mode helper.
  The legacy block-shell headers still expose optional `hw_params` pointers, but `sc_isp_pipeline` does not pass them and the active line timing engine does not use them.
* **`power.h` / `power_estimator`**: Legacy uncalibrated power and energy helpers.
  They are not referenced by the active `sc_isp_pipeline` metrics path and are not exported by `write_metrics`.
* **Legacy external timing-source fields**: Older block scheduling concepts are not part of the active contract.
  The active pipeline uses the unified `clock_freq_mhz` value from `isp_arch_config`.
