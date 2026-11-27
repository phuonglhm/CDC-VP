# fx1_soc bandwidth report (`--bw-report`, schema `fx1-bw/1`)

VP-only instrumentation for the CSI → ISP bandwidth measurement (plan
`CSI_ISP_BANDWIDTH_MEASUREMENT_PROPOSAL_20261126.md`, milestone BW1). It is
**not** a model of the SoC and changes nothing that it observes:
- no payload, TREADY, callback or timing changes;
- no bus transaction (the NV12 dump reads DDR by backdoor).

Off by default. The binary without these options behaves as before.

| Option | Effect |
|---|---|
| `--bw-report FILE` | JSONL event log in FILE, one JSON object per line. Also enables the window marker. |
| `--bw-marker` | The window marker register only, without any recorder. This is the control run: the same benchmark firmware with no observers. Cannot be combined with `--bw-report`. |
| `--bw-dump-nv12 DIR` | With `--bw-report` only. Each DONE NV12 output is written to DIR, which must be new or empty. The file name appears in the `output` event. |

## Window marker: SIM_CTRL `0xC` (`FX1_SIM_CTRL_BW_MARK_OFF`)

The register exists only with `--bw-report` or `--bw-marker`. Without them, an access answers SLVERR, as before.

**Write** (32 bits):

| Bits | Field | Values |
|---|---|---|
| `[3:0]` | op | 1 = START, 2 = STOP |
| `[7:4]` | mode | START only: 0 = batch, 1 = steady |
| `[31:8]` | count | batch START: the number of captures in the set, ≥ 1; otherwise 0 |

The device rejects a write with SLVERR, records an `error` mark and changes no state when:
- START comes while a window is open;
- STOP comes while no window is open;
- op or mode is not one of the values above;
- count is wrong for the op and mode.

The device validates writes the same way with and without the recorder, so the firmware sees the same responses in both runs.

**Read**: 1 while a window is open, 0 otherwise.

**Timing**: a mark takes effect at `sc_time_stamp()` + the incoming local delay, that is, when the write reaches the device. The device's 10 ns access latency is then added to the annotated delay.

## Time

Every `t` and `t_*` field is an **integer tick** of the SystemC time resolution. The resolution is given in the header as `time_resolution_fs`. fx1_soc runs at 1 fs. Convert to seconds as `t × time_resolution_fs × 1e-15`; do not assume picoseconds.

## Events

Every line carries an `ev` field. The table describes each event.

| `ev` | When | Content |
|---|---|---|
| `header` | first line | `schema`, `time_resolution_fs`, `package`, `revision`, `revision_state`, `build_type`, `systemc`, `timeout_ms`, `command` (argv), `isp_core_period_ticks`, `isp_axi_data_bytes`, `csi_sys_clk_period_ticks` and `axis_data_bytes` (with `--camera`), `quantum_ticks`, `dma_masters`, `not_observed`, `nv12_dump` |
| `mark` | each BW_MARK write | `kind` (start / stop / error), `value`, `window` (1, 2, …; 0 for error), `t`. A start also carries `mode` and `count`. |
| `rx_frame` | CSI FE; also at the next FS without an FE, or at the end of the run (`fe` = 0) | One frame as the CSI Packet Analyzer parsed it. Payload bytes are counted **as received, before every forward/discard gate**: `image_bytes`, `embedded_bytes`, `null_bytes`, `unsupported_dt_bytes`, `unsupported_vc_bytes`. Also `image_packets`, `not_forwarded_bytes`, `crc_bad`, `truncated`, `unclassified` (headers whose VC, DT and WC are unknown), `dt`, `seq` (the capture started in this frame, 0 if none), `t_fs`, `t_fe`, `t_first_byte`, `t_last_byte`. Header and CRC footer bytes are not counted. |
| `capture_start` / `capture_end` / `capture_stream_end` / `source_reset` | CSI capture observer | `seq` (64-bit capture sequence), `frame_id`, `epoch`, `fs_ts_ns`, `outcome` (ok / dropped / missing_fe), `t` |
| `axis_capture` | at the capture's stream boundary | AXI4-Stream totals of that capture (see below) |
| `axis_capture_open` | end of run | The same totals, for a capture whose boundary never came. `seq` = 0 collects beats outside any capture (for example the null abort beat of a stream disable). |
| `axis_window` / `rx_window` | once simulation time has passed the effective STOP, or at the end of the run | The same totals, counted only over handshakes and payload bytes **whose own time lies in `[effective START, effective STOP)`**. A mark can arrive before its effective time (an initiator's annotated delay), so window membership comes from timestamps, not from the callback order. Data at the exact time of START is in, data at the exact time of STOP is out, whichever callback runs first within that simulation time: the recorder keeps the current tick's AXIS and payload data apart and assigns it once time has moved past it, when every mark that takes effect at that time is known. DMA events are filtered by the same rule (`start <= t < stop`) in the report. `rx_window` gives `image_bytes`, `other_bytes`, `packets`, `t_start`, `t_stop`, and `drained`: 1 when the STOP time was passed before the window was written, 0 when the run ended first. Several windows are allowed. |
| `dma` | each MIPI_DMA / ISP_IDMA / ISP_ODMA transaction | `ph` = `issue`: `m`, `id`, `w`, `a`, `n` (bytes), `be`, `t` = issue time. `ph` = `resp`: `m`, `id`, `ok`, `t` = response time. See "DMA times" below. |
| `isp_admit` / `isp_drop` | ISP stream ingress | admit: `seq`, `epoch`, `frame_id`, `tag`. drop: `seq`, `reason` (`FX1_ISP_INGRESS_DROP_REASON_*`). |
| `output` | ODMA end of one frame | `how` (done / failed / aborted), `n`, `seq` (0 for a memory-path frame), `direct`, `tag`, `ch`, `slot`, `frame_id`, `w`, `h`, `y_base`, `uv_base`, `y_stride`, `uv_stride`, `writes`, `write_bytes`, `t_first_write_issue`, `t_last_write_response`, `t_end` (DONE, or the end time for failed / aborted), `dump` (file name) or `dump_error` |
| `retire` | ISP frame retired | `direct`, `tag`, `seq`, `ok` (every enabled output DONE). A frame abandoned by a reset or a DMA recovery does not retire. |
| `footer` | last line, at the end of the run | `complete` = 1, `reason` (pass / fail / timeout), `t`, `window_open`, `dma_issued`, `dma_responded`, `outputs`, `dump_errors`, `rx_outside_bytes`, `rx_outside_unclassified` |

A file without the footer is **incomplete**, for example after a crash or a killed run, and consumers must refuse it.

### AXI4-Stream totals

`valid_cycles` counts the cycles with TVALID = 1 that the CSI master executed. The CSI skips idle cycles, so `valid_cycles` is not an elapsed-cycle count: compute occupancy from elapsed time ÷ `csi_sys_clk_period_ticks`.

The other fields:
- `accepted_beats`: TVALID && TREADY.
- `useful_bytes`: `popcount(TKEEP)` at the handshake. A tail beat carries fewer bytes; the null abort beat carries 0.
- `abort_beats`: TKEEP = 0, TLAST and TUSER[1].
- `error_beats`: TUSER[1] on a data beat.
- `stall_cycles`: TVALID && !TREADY.
- `max_stall_run`: the longest run of consecutive stall cycles.
- `tlast_beats`, `sof_beats`.
- `t_first`, `t_last`: the first valid cycle and the last accepted beat.

### DMA times

The counter is the **first hop** of each master, and forwards every transaction unchanged:

| Master | Path |
|---|---|
| MIPI_DMA | CSI DMA → counter → [CSI fault shim] → write guard → fabric |
| ISP_IDMA | ISP → counter → [ISP fault shim] → fabric |
| ISP_ODMA | ISP → counter → [ISP fault shim] → write guard → fabric |

The events are therefore the **master's demand**: every transaction it issued, at its issue time. This includes one that a VP fault shim answers with an error without forwarding it, and one that a shim or a write guard holds. They are not the DDR-serviced traffic: a request answered with an error by a shim never reached DDR.

Command, address and length are snapshot before forwarding.

| Time | Definition |
|---|---|
| issue | `sc_time_stamp()` + the incoming local delay |
| response | `sc_time_stamp()` + the delay left when `b_transport` returns |

The response time is therefore what the target annotated and did not wait out; a target that blocks has already advanced `sc_time_stamp()`.

A response can carry a time later than events written after it. **Order events by `t`, not by line.** Outstanding at time T = issues with `t ≤ T` minus responses with `t ≤ T`.

### Output times and good output

- The ODMA waits out each write's response delay, so `t_last_write_response` is when the last write of the frame completed.
- `t_end` for `done` is when DONE was published: after that response, at the same time or later.
- A **good output** is: `output.how` = done + `retire.ok` = 1 + the capture's `capture_end.outcome` = ok + its `capture_stream_end`. A late CSI error after DONE therefore turns the capture bad even though its pixels were written.

## Scope

The report covers three DMA masters: MIPI_DMA, ISP_IDMA and ISP_ODMA. CPU (including DMI), SYS_DMA, ETH_DMA and NPU_DMA are not observed. The three counters are "Camera DMA", not the total DDR traffic of the platform.

The CSI payload is the received image payload. Packet and PHY overhead are not measured.

Speeds and latencies are results of the VP timing model; they are not silicon figures.

## Checker

`platforms/fx1_soc/tools/check_bw_report.py check REPORT [--dump-dir DIR] [--uart-log LOG] [--direct-only] [--min-good N]`

The checker verifies a report:
- **Completeness and DMA pairing.**
- **Direct path:** with `--direct-only`, no RAW in DDR.
- **Volumes per good capture:** received RAW = W·H·bits/8, AXIS = 2·W·H, NV12 = 1.5·W·H.
- **Output times:** first issue < last response ≤ DONE, and the last response equal to the latest ODMA response in the DMA log.
- **DMA bytes and status of a good output:** its ODMA writes in the DMA log are those issued between its first write and its end, each wholly inside one of its planes. Their bytes must equal the output record and 1.5·W·H, their count must be the record's, and every response must be OK.
- **DMA errors:** no error response at all, unless `--expect-dma-errors MASTER:N` gives the exact count.
- **ISP fault shim:** with the console log, the shim's reported transaction count must equal the report's. The fired transaction must be in the report at the shim's address and issue time, with an error response for an error fault. `--expect-stall MASTER:N:US` additionally requires the N-th transaction's response at least US after its issue.
- **NV12 dumps:** equal to the firmware's CRC lines.

`selftest REPORT [check options]` checks the checker itself. The clean report must PASS, and each of these modified copies must FAIL:
- footer removed;
- one DMA response removed;
- every ODMA issue's bytes set to 1;
- every ODMA response set to an error;
- one ODMA write running past its plane;
- one capture's AXIS bytes reduced by 8;
- one NV12 byte flipped.

`same-run UART_A UART_B CONSOLE_A CONSOLE_B` compares two runs of the same stimulus, observers on and off: identical UART logs and verdict lines.

The regression tests `fx1_regression_freertos_cli_camera_direct_bw`, `fx1_regression_freertos_cli_camera_direct_bw_off` and `fx1_regression_freertos_cli_camera_direct_bw_same_run` run this on the FreeRTOS CLI.
