# FX1 ISP VP — Model interface and event model

Header: [include/fx1_isp/fx1_isp_tlm.h](../include/fx1_isp/fx1_isp_tlm.h).
Namespace: `cdc::components::fx1_isp`. CMake target:
`cdc::components::fx1_isp_tlm`.

## 1. Ports

| Port | Type | HAS equivalent | Notes |
|---|---|---|---|
| `csr_socket` | `simple_target_socket<fx1_isp_tlm>` (32-bit) | `s_axil` | Offsets are relative to the ISP base; the platform router strips the base |
| `idma_socket` | `simple_initiator_socket<fx1_isp_tlm>` (32-bit) | `m_axi_m0` read master | One `b_transport` read per AXI burst |
| `odma_socket` | `simple_initiator_socket<fx1_isp_tlm>` (32-bit) | `m_axi_m1` write master | One `b_transport` write per AXI burst, luma and chroma planes |
| `rst_n` | `sc_in<bool>` | `i_rst_n` | Active low |
| `irq` | `sc_out<bool>` | `o_irq` | Level; bind to one interrupt-controller input |

Socket `BUSWIDTH` stays at the TLM default (32) so the sockets bind to the
existing CDC-VP routers and memories. The AXI data width (beat size `B`) is a
model parameter; it drives alignment checks and burst formation, not the TLM
socket type.

The base address, interrupt number and CPU are platform configuration (DEC-08)
and are unknown to the model.

## 2. Parameters (`fx1_isp_params`)

| Parameter | Default | Source |
|---|---|---|
| `core_period` | 2 ns | HAS Table 6-8, ASIC nominal 500 MHz |
| `csr_latency` | 10 ns | VP parameter, not a silicon figure |
| `soft_reset_cycles` | 32 | CSR row 451, DEC-14 |
| `axi_data_bytes` | 16 | `PARA_AXI_DATA_WIDTH` = 128 (HAS Table 5-1): beat size `B` for alignment checks and bursts; 8, 16 or 32 |
| `axi_addr_bits` | 40 | `PARA_AXI_ADDR_WIDTH` (HAS Table 5-1): higher address bits are dropped (M2-A3) |
| `pipeline_line_overhead_cycles` | 0 | VP parameter: extra core cycles per line on top of 1 pixel per clock |

## 3. Timing model (DEC-06)

- Loosely timed, event based. No cycle accuracy and no pin-level AXI
  handshake. The core clock is used only to turn times into cycle numbers
  (race rule, soft-reset window) and to pace the pipeline (1 pixel per clock
  plus `pipeline_line_overhead_cycles` per line).
- **Synchronise on access.** Register accesses have side effects (W1C/W1S,
  commands, interrupts), so a CSR access takes effect exactly at
  `sc_time_stamp() + annotated delay`. When the annotated delay is non-zero,
  `b_transport` first `wait()`s it out, then applies the access. The delay it
  returns is `csr_latency` only. A temporally decoupled initiator therefore
  loses its quantum at every CSR access. That is intended: register traffic is
  sparse, and correct ordering against hardware events matters more.
- The caller must be an `SC_THREAD` when the annotated delay is non-zero; from
  a method process the access is refused with `TLM_GENERIC_ERROR_RESPONSE` and
  an `SC_REPORT_ERROR`. With zero delay any process may call.
- `irq` follows the register state one delta cycle after the access takes
  effect.

### 3.1 Frame lifecycle (M2)

Three SystemC threads connected by 2-line FIFOs (HAS Table 6-87,
`PARA_DMA_FIFO_LINES` = 2): IDMA → pipeline → ODMA (luma and chroma FIFOs).
A full FIFO back-pressures its producer, so an ODMA with no free buffer stalls
the pipeline and then the IDMA without losing data (DEC-18).

| Step | Event | Register effects |
|---|---|---|
| 1 | IDMA arm: `IDMA_EN` and head `IDMA_BUF_VALID` | Working set captured (base, stride, `COMMON_FRAME_*`, burst length); illegal → `ALIGN_OR_GEOMETRY`, no start. `idma_busy`, `IRQ_IDMA_START` |
| 2 | IDMA reads line by line, one TLM read per burst (HAS Table 6-92) | — |
| 3 | Accepted SOF: the pipeline takes line 0 (waits for `ISP_EN`) | Pending commits (DEC-26), GTM bank swap, frame counter / FRAME_ID, AEC commit, AWB/AF sampling, LSC profile selection; then the configuration snapshot (HAS §9.4.3); `frame_start` and `frame_done` cleared; `COMMON_STATUS.busy`, AEC/AF busy |
| 4 | ODMA arm: `ODMA_EN` and head `ODMA_BUF_FREE` (may precede step 1) | Bases, strides, output geometry (`RESIZER_OUT_*`, or 0×0 while `ISP_EN` = 0, DEC-20), burst length captured; illegal → `ALIGN_OR_GEOMETRY` |
| 5 | IDMA retire: the pipeline has accepted the last line | `IDMA_BUF_VALID` bit cleared, rotation +1, `IDMA_FRAME_COUNT`+1, `IRQ_IDMA_DONE` |
| 6 | ODMA complete: the last write response of both planes has returned | `ODMA_BUF_FREE` bit cleared, `ODMA_BUF_DONE` bit set, rotation +1, `ODMA_FRAME_COUNT`+1, `IRQ_ODMA_DONE`, `frame_done_irq`, `COMMON_STATUS.frame_done`; ODMA busy cleared; common busy cleared only when no accepted frame remains |

During the frame, the AEC and AWB stages update the control unit's single
zone/histogram memories row by row, so a CSR read in the middle of a frame
returns partial sums (DEC-30). At the end of a frame, the pipeline thread publishes the block counters (LSC,
BPC), builds the next GTM curve, hands the 2DNR variance over and publishes
the statistics (AEC → AWB → AF, each followed by `stats_ready_irq`). This
happens after the last output line is queued to the ODMA, so it can precede
`frame_done_irq`.

No completion is reported inside the CSR write that starts a frame; a frame
takes at least `H × (W + overhead)` core cycles plus memory latency.

Errors and resets: see DEC-18..20 and M2-A1..A12 in
[ISP_DECISIONS_AND_DISCREPANCIES.md](ISP_DECISIONS_AND_DISCREPANCIES.md).
Each external or soft reset starts a new epoch. Every engine drops its frame
on its next wake-up and issues no further memory access, status update or
interrupt for it. A burst already inside the target when the reset occurs
completes (HAS §7.1.8), and its result is ignored.

### 3.2 Pixel pipeline (M3)

`pipe::isp_pipeline` (src/pipeline, no SystemC) receives the frame line by
line and emits NV12 rows as soon as they are computable. Block order follows
HAS Table 6-2:

```text
IFMT → BLC → LSC → BPC ─┬─(AEC tap)→ WB → DG → Demosaic ─┬─(AWB tap)→ CCM → Gamma → CSC ─(AF, inline)→ GTM ─┬→ 2DNR → EE ─┐
                                                                                                          └→ CNF ───────┴→ join (Y from EE, UV from CNF) → Resizer → OFMT
```

The parameter `test_datapath_stub` = true selects the M2 stub instead (DEC-21:
Y = sample >> 4, UV = 128, real geometry). Only the DMA tests use it, to keep
their expected memory images simple. It produces no statistics.

The pipeline reports with `SC_REPORT_WARNING` (message type
`fx1_isp/pipeline`) only for a configuration it does not model, once per
frame:
- BPC dynamic detection (DEC-28);
- OFMT stride padding (DEC-39);
- D_WDR and TNR_3D enables (M5-A1). It also warns when it drops a frame it
cannot process (for example a geometry rejected by a block). The ODMA receives an abort
marker, and no statistics are published.

Host tools:
- `fx1_isp_run_raw` (tools/run_raw.cpp) drives the complete IP like a driver
  (M4-A2):
  - input: one RAW container file (`--input`), an input list
    (`--input-list`), or the 4K pattern (`--synthetic`);
  - one CSR profile, applied once;
  - frames: up to 4 in flight (`--queue`) through the rotation;
  - output: the NV12 of the last frame, or of every frame with
    `--per-frame`, plus `events.log` and status;
  - checks: `--stats-script` runs a statistics readout script, and
    `--check-stats` and `--expect-sha256` turn the run into a
    self-checking test.
- `tools/nv12_preview.py` turns the NV12 into a PNG.
- `tools/run_regression.py` runs the real-RAW regression. The tool queues one buffer
and leaves `IDMA_EN` set, so `DMA_ERR.IDMA_UNDERRUN` (0x2) is expected at the
end. Its exit status is non-zero only if the frame did not complete or
another `DMA_ERR` bit is set (M3-A9).

## 4. Debug and testbench backdoor

The `debug_*` methods let a testbench drive the hardware side of registers
before the engines exist. They are **not** part of the FW-visible interface,
and a platform must not depend on them.

| Method | Purpose |
|---|---|
| `debug_hw_set(offset, mask)` | Hardware sets bits (W1C status, W1S clear-by-hw, RO) |
| `debug_hw_hold(offset, mask, level)` | Level-held status bits |
| `debug_hw_write(offset, mask, value)` | Hardware writes RO values (counters, results) |
| `debug_accepted_sof()` | Signals an accepted SOF (DEC-16/17 effects) |
| `debug_peek(offset)` | Stored value without read hooks |
| `debug_soft_reset_active()` | Whether the soft-reset window is open |
| `debug_pending_commands()` | Commands not consumed by a block model (0 in the integrated IP since M3) |
| `debug_idma_head()`, `debug_odma_head()` | Current rotation positions (not software visible) |
| `debug_set_frame_counter(v)` | Sets the statistics frame counter (FRAME_ID source) for wrap tests (M4-A1) |

## 5. Internal structure

```text
fx1_isp_tlm (SystemC)          TLM decode, reset/irq processes, timing
 ├─ idma_engine / pipeline_engine / odma_engine   SC_THREADs, line FIFOs (src/engine)
 │   ├─ dma rules (C++)        burst formation, start checks (src/dma)
 │   └─ isp_pipeline (C++)     row-streaming blocks + statistics taps (src/pipeline)
 └─ control_unit (C++)         soft reset, IRQ/error aggregation, commit gates,
     │                         LUT ports, LSC loader, GTM banks, statistics
     │                         publication and readout (control_unit_stats.cpp)
     └─ register_file (C++)    access semantics over the generated table
         └─ csr::registers[]   generated from the CSR spreadsheet
```

`fx1_isp_core` (register file, control, DMA rules, pipeline) has no SystemC
dependency and is unit-tested on its own. `fx1_isp_drv`, the C99 reference
driver in `driver/`, depends only on the register header; see
[ISP_PROGRAMMING_GUIDE.md](ISP_PROGRAMMING_GUIDE.md).

## 5.1 Using the model in a platform

| Way | How |
|---|---|
| Subproject | `add_subdirectory(components/FX1_Components/isp)`; link `cdc::components::fx1_isp_tlm` (and `cdc::components::fx1_isp_drv` for firmware-side code). The parent provides `SystemC::systemc`, and the IP's tests are not built. |
| Installed package | Build the IP standalone, then `cmake --install build --prefix <p>`. In the platform, use `find_package(fx1_isp 0.1)` with `CMAKE_PREFIX_PATH=<p>` and link `fx1_isp::fx1_isp_tlm` and `fx1_isp::fx1_isp_drv`. SystemC is taken from the consumer's `SystemC::systemc`, else from `SystemCLanguageConfig.cmake`, else from `SYSTEMC_HOME`. |

Both ways are tested: CTest `fx1_isp.package.consumer` installs the package,
then builds and runs `tests/package/consumer` against it. The subproject way
was built with a scratch parent project during M5. The base address, IRQ
line and memory map are platform choices (DEC-08).

## 6. Reference model and vectors (MR)

`reference/fx1_isp_ref` is an independent, frame-based numpy implementation
(`blocks.py` pipeline, `stats.py` statistics and their register view,
`regs.py` profile replay). `tools/gen_vectors.py` and
`tools/gen_block_vectors.py` produce the committed vectors from it. With
`--check`, they verify that the committed files are up to date. Building and
testing need neither Python nor numpy.

Vector directory format: `input.bin`, `profile.csrw`, optional `frames.csrw`
(`<frame> <REG> <value>` writes before that frame's SOF), `expected_y.bin`,
`expected_uv.bin`, `meta.txt`, and `expected_stats.txt`. The last one holds
`<REG> <value>` expected reads in order, with `SET <REG> <value>` lines for
readout selectors.
