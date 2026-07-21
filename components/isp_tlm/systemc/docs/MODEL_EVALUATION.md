# SystemC ISP Pipeline — Model Evaluation Report

**Date:** 2026-07-21

**Status:** The active line-granular architecture model and focused smoke are documented here.

## 1. Executive Summary

The SystemC ISP model contains an 18-stage image signal processor from RAW Bayer input to YUV420 output.

The functional pipeline is the byte-exact untimed oracle.

The architecture model schedules line events from one `isp_arch_config` clock.

Its observation window spans the first RAW line entering the ideal source boundary through the last YUV line leaving the ideal sink boundary.

The architecture report is one unified immutable snapshot of frame, stage, and link metrics.

The focused architecture smoke is `tb_arch_pipeline`.

This report makes no claim of broad suite coverage.

## 2. Functional Evaluation Material Retained

The existing block-level and pipeline-level functional evaluation remains relevant to output correctness.

The block tests compare SystemC output with reference output using deterministic inputs.

The functional pipeline test is run with:

```bash
build/bremen/components/isp_tlm/systemc/tb_pipeline
```

The byte-exact untimed oracle remains the reference for output comparison.

The existing real-image evaluation material under `components/isp_tlm/input/` is retained for functional investigations.

AWB and AEC are statistical stages whose behavior depends on frame input and configuration.

The focused architecture smoke is not a replacement for those functional evaluations.

## 3. Build and Focused Smoke

Build the SystemC component with:

```bash
./components/isp_tlm/systemc/build.sh
```

Build the focused architecture executable with:

```bash
ninja -C build/bremen tb_arch_pipeline
```

Run the focused smoke with an explicit caller-owned report path:

```bash
BUILD=build/bremen/components/isp_tlm/systemc
$BUILD/tb_arch_pipeline --metrics /tmp/isp_arch_snapshot.csv
```

The report path is selected by the caller after `--metrics`; `write_metrics` does not create parent directories, so any alternate parent directory must already exist.

The default tests write no source artifacts.

Compare architecture sweep points with:

```bash
$BUILD/tb_arch_sweep --compare
```

These commands establish the documented smoke path only.

They do not establish broad suite coverage.

## 4. Active Architecture Model

### 4.1 Clock and scheduling

One clock in `isp_arch_config` drives `sc_time` scheduling for the architecture model.

The model does not expose a separate timed-versus-untimed clock binding contract.

Functional output remains checked against the byte-exact untimed oracle.

### 4.2 Retained parameters

`isp_arch_config` owns the clock, per-block pixels-per-cycle (PPC), pixel initiation interval (II), pipeline latency, maximum in-flight lines, optional modeled workload and local-memory profiles, and per-link depth.
Stage enable/bypass controls come from functional `isp_config`, not `isp_arch_config`.

A line is one `line_channel` token representing one logical image row.

RGB and YUV channel multiplicity does not multiply logical pixels.

For logical pixel count $P$ and PPC $Q$, a processing beat count is:

```text
processing_beats = ceil(P / Q)
```

For pixel initiation interval $I$, compute demand is:

```text
compute_cycles = processing_beats * I
```

Processing beats are internal groups of logical pixels and are not bus units.

The architecture reports 18 stages and 18 links.

The 18 reported link records consist of the 17 inter-stage links from `input_norm_to_blc` through `scale_to_yuv420` plus the `yuv420_to_output` ideal sink-boundary link.
The ideal source boundary is not emitted as a reported line row; it is instead captured by frame ingress metrics.
The exact reported link names are `input_norm_to_blc`, `blc_to_dpc`, `dpc_to_lsc`, `lsc_to_dg`, `dg_to_bnr`, `bnr_to_demosaic`, `demosaic_to_awb`, `awb_to_wb`, `wb_to_ccm`, `ccm_to_gc`, `gc_to_aec`, `aec_to_csc`, `csc_to_cse`, `cse_to_sharpen`, `sharpen_to_2dnr`, `2dnr_to_scale_or_yuv420`, `scale_to_yuv420`, and `yuv420_to_output`.

### 4.3 Unified snapshot and API

The pipeline API is `metrics()`.

The optional `write_metrics(file)` API writes one unified snapshot to the caller-selected file.

Raw event ownership remains in the stage runtime, line stage, and line channel components.

Derivations consume one immutable raw snapshot.

The only metric report filesystem side effect is the explicit `pipeline.write_metrics(file_path)` operation.
Sweep comparisons consume in-memory snapshots and present results on standard output without CSV export.

## 5. Metric Definitions

The observation window is one frame from the first RAW line at the ideal source boundary through the last YUV line at the ideal sink boundary.

All cycle values are integer clock cycles.

Time values are microseconds unless otherwise stated.

Measured values are accumulated at event boundaries in the timed SystemC model.

Modeled values are architecture assumptions and retain their profile provenance.

Unavailable values indicate that the required profile or event was not supplied.

An unavailable value is emitted with an explicit `unavailable` marker and is never replaced by an invented zero.

### 5.1 Frame metrics

| Metric | Equation | Unit | Provenance |
| `first_output_latency_cycles` | `ceil((first_output_time - first_input_time) / cycle_period)` | cycles | derived from measured timestamps |
| `first_output_latency_us` | `(first_output_time - first_input_time) / 1us` | microseconds | derived from measured timestamps |
| `frame_cycles` | `ceil((last_output_time - first_input_time) / cycle_period)` | cycles | derived from measured timestamps |
| `achieved_pixels_per_cycle` | `input_raw_logical_pixels / frame_cycles` | pixels/cycle | derived from measured events |
| `input_bandwidth_mbps` | `input_bytes * 8 / frame_time_us` | Mbps | derived from measured logical bytes |
| `output_bandwidth_mbps` | `output_bytes * 8 / frame_time_us` | Mbps | derived from measured logical bytes |
| `input_bytes`, `output_bytes` | logical samples crossing ideal ISP boundaries | bytes | measured |

The bandwidth fields describe internal ideal-boundary traffic.

They are not measurements of external interfaces or memory systems.

### 5.2 Stage metrics

Each stage reports input lines, output lines, logical pixels, processing beats, active cycles, bypass cycles, input-starved cycles, output-blocked cycles, completion-wait cycles, utilization, and effective II when the required events exist.

`processing_beats` is the sum of `ceil(line_pixels / PPC)` over issued output lines.

`active_cycles` is the modeled compute demand for explicitly enabled issues accumulated at measured issue events.

`bypass_cycles` records service demand for explicitly disabled issues.

`input_starved_cycles`, `output_blocked_cycles`, and `completion_wait_cycles` are measured at their corresponding events.

Stage durations can overlap for in-flight lines and are not a cycle partition.

`utilization` is `min(1, active_cycles / stage_observation_cycles)` for enabled stages and zero for bypass.

`effective_ii` is `issue_window_cycles / processing_beats`.

Workload operation counts remain unavailable without an explicit modeled workload profile.

### 5.3 Optional local-memory metrics

If valid local-memory profiles are supplied for logical pixel count $P$, the model derives:

```text
read_cycles = ceil(reads_per_pixel * P / read_ports) * access_cycles
write_cycles = ceil(writes_per_pixel * P / write_ports) * access_cycles
memory_service_cycles = max(read_cycles, write_cycles)
line_issue_cycles = max(compute_cycles, memory_service_cycles)
memory_wait_cycles = max(0, memory_service_cycles - compute_cycles)
```

Zero demand has zero cycles and does not require a port.

Memory service and wait metrics remain unavailable when either required profile is absent.

### 5.4 Link metrics

A link is a bounded preallocated `line_channel`.

Its depth is the only link architecture parameter.

`published_lines`, `read_lines`, and `released_lines` are measured token events.

`logical_bytes` is the byte total represented by valid samples in published or read tokens.

`occupancy_high_water` is measured in slots.

Producer and consumer wait event counts and completed wait durations are measured at their corresponding credit or data events.

## 6. Report Contract

The report contains frame metrics, 18 stage records, 18 link records, availability markers, units, and provenance.
The report is written only when the caller supplies an explicit path through `--metrics <caller-file>` or the equivalent `write_metrics(file)` API.
Sweep commands present results on standard output and do not write metric or sweep CSV artifacts.
Default test execution writes no source artifacts and no report.

The model does not report external interface capacity, arbitration, or operation counts without their explicit profiles.

## 7. Known Characteristics

Synthetic patterns remain useful for deterministic functional comparisons.

Statistical stages such as AWB and AEC depend on frame input and configuration.

FIFO sizing remains an architecture configuration concern for line-channel execution.

Integer metric arithmetic is overflow checked.

Invalid zero divisors and overflowing values are errors rather than wrapped results.

## 8. Conclusion

The active model provides a line-granular architecture snapshot with explicit equations, units, availability, and provenance.

The byte-exact untimed oracle provides the functional output reference.

The focused `tb_arch_pipeline` smoke exercises the documented explicit-report path.

The documented evidence is intentionally bounded to that focused smoke and the retained functional evaluation material.
