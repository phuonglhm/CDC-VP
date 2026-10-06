# FX1 ISP VP — CSR behaviour contract (M1)

This is the register-level behaviour the model implements. The map itself
(offsets, fields, reset values) is `include/fx1_isp/fx1_isp_csr.h`, generated
from the pinned CSR spreadsheet. Decision and issue IDs refer to
[ISP_DECISIONS_AND_DISCREPANCIES.md](ISP_DECISIONS_AND_DISCREPANCIES.md).

## 1. Access types (HAS Table 8-1, Table 7-10)

| Type | Software write | Software read | Hardware |
|---|---|---|---|
| RW | Bits in strobed lanes take the written value | Stored value | May update (e.g. LUT address auto-increment) |
| RO | Ignored | Hardware value | Drives the value |
| W1C | 1 clears, 0 no effect | Stored value | Sets the bit |
| W1S | 1 sets, 0 no effect | Current state, not the last write | Clears the bit |
| W1SC | 1 pulses a command | Always 0 | — |

General rules:

- Every access completes with OKAY. Unmapped words read 0 and ignore writes.
  Reserved bits read 0 and ignore writes.
- Byte strobes are honoured per lane. A write with no strobes changes nothing
  and triggers no side effect (no LUT write, no auto-increment, no command).
- Several W1C/W1S bits in one write act independently.
- **Hardware set wins:** if hardware sets a W1C bit in the same core-clock cycle
  as a software write-1-to-clear, the bit stays set. The cycle is
  `floor((sc_time_stamp() + annotated delay) / core_period)`.
- **Level-held bits** (`DMA_ERR.IDMA_UNDERRUN`, CSR-06): while the condition
  persists a W1C write does not clear the bit. Once the condition drops the
  bit stays set until software clears it. The derived interrupt bits are set
  on the rising edge of the condition only.
- Reserved bits inside declared fields are tied to zero:
  `DMA_IRQ_EN[0]`, `DMA_IRQ_STAT[0]` (SPEC-05), `DMA_ERR[5]`.

## 2. TLM adapter (CSR-23)

The CSR socket models the AXI4-Lite slave behind a CPU-side bridge:

| TLM access | Behaviour |
|---|---|
| 4 bytes, any address | Word `addr & ~3`, all lanes; address bits [1:0] are ignored |
| 1–3 bytes | Byte lanes `addr[1:0] … addr[1:0]+len-1` of word `addr & ~3` |
| 1–3 bytes crossing a word boundary, or length > 4 | `TLM_BURST_ERROR_RESPONSE`, no effect |
| Address ≥ 0x10000 | `TLM_ADDRESS_ERROR_RESPONSE` (routing error, outside the aperture) |
| Null data pointer (read/write) | `TLM_GENERIC_ERROR_RESPONSE`, no effect; debug read returns 0 bytes |
| Non-null byte-enable pointer with zero length | `TLM_BYTE_ENABLE_ERROR_RESPONSE`, no effect |
| Byte-enable array | Honoured, `TLM_BYTE_ENABLED` per data byte, repeating over its length |
| `TLM_IGNORE_COMMAND` | OK, no effect |

An access takes effect at its annotated time: the target waits out a non-zero
incoming delay before applying it (synchronise-on-access, see
[ISP_MODEL_INTERFACE.md](ISP_MODEL_INTERFACE.md) §3), then returns
`csr_latency` as the delay. DMI is refused
(registers have side effects). `transport_dbg` supports reads only; reads have
no side effects, so a debugger sees exactly what software would read.

While `rst_n` is low, CSR writes are ignored and reads return reset values.

## 3. Reset

### 3.1 External reset (`rst_n` low, HAS §6.5)

All registers return to their reset values, all level holds and pending
commands are cleared, and any soft-reset window is cancelled. The `irq` output
is low while in reset.

LUT contents after reset (CSR-16): Gamma = 0, EE = 0x8000 (unity), GTM = 0.
The HAS calls LUT contents undefined after reset; software must load a table
before enabling its block.

### 3.2 Soft reset (DEC-14)

Writing 1 to `COMMON_CTRL.soft_rst` or to `DMA_CTRL.soft_reset` has the same
effect. The register effects happen at the write; the pipeline and DMA engines
then stay in reset for **32 core cycles**.

| Preserved | Cleared (to reset value) |
|---|---|
| All RW configuration, including `DMA_CTRL.idma_en/odma_en` and `COMMON_CTRL.isp_en` | Buffer ownership: `IDMA_BUF_VALID`, `ODMA_BUF_FREE`, `ODMA_BUF_DONE` |
| Gamma, EE, GTM and LSC table contents | `COMMON_IRQ_STATUS`, `DMA_IRQ_STAT`, `DMA_ERR` (including held bits) |
| `LSC_PROFILE_STATUS` (profile validity) | `COMMON_STATUS`, per-block status and error flags |
| `IDMA_FRAME_COUNT`, `ODMA_FRAME_COUNT`, `AEC/AWB/AF_FRAME_ID` | Statistics results and done flags |
| RO constants (`COMMON_VER_*`) | Pending command bits (W1S), e.g. `frame_start`, `AEC_CTRL.commit`, LSC load commands |

Rule used by the model: every RO/W1C/W1S bit returns to its reset value
except for the preserved registers listed above.

During the 32-cycle window, software writes still update RW configuration,
but their W1S/W1C effects are dropped, because the state they target is held
in reset. For example, arming `IDMA_BUF_VALID` inside the window has no
effect. A new soft-reset write inside the window restarts the 32 cycles.

## 4. Interrupts (HAS §9.2)

```
irq = |(COMMON_IRQ_STATUS & COMMON_IRQ_EN) | |(DMA_IRQ_STAT & DMA_IRQ_EN)
```

Status bits latch whether or not their enable bit is set. The line stays high
while any enabled status bit is set.

| COMMON_IRQ_STATUS bit | Set by |
|---|---|
| `frame_done_irq` | Output frame complete: last write response of both planes (M2) |
| `error_irq` | Rising edge of any bit in `LSC_ERROR[5:0]`, `BPC_STATUS.cand_reject_ovf`, `EE_STATUS.error`, `DMA_ERR[4:0]` (DEC-15, including `IDMA_UNDERRUN`) |
| `stats_ready_irq` | Every statistics publication (AEC, AWB, AF), also when that block's done bit is still set (M3-A4); a backdoor set of a done bit also raises it |

`DMA_IRQ_STAT` bits are set by the DMA engines (M2). Bit 0 is tied to zero,
so the only interrupt path for `DMA_ERR.ALIGN_OR_GEOMETRY` is `error_irq`.

## 5. Register-specific behaviour implemented in M1

| Register | Behaviour |
|---|---|
| `COMMON_STATUS.error` | Live OR of the DEC-15 error flags; clears when those flags are cleared |
| `COMMON_STATUS.frame_done` | Set together with `frame_done_irq`; cleared at the next accepted SOF (DEC-17) |
| `COMMON_CTRL.frame_start` | Pending flag, cleared at the next accepted SOF; does not start or block frames (DEC-16) |
| `GAMMA_LUT_ADDR/DATA/RDATA` | A DATA write stores the merged 12-bit value at ADDR, then ADDR increments modulo 256. DATA reads back the last value written; RDATA reads the table at ADDR (CSR-21) |
| `EE_LUT_CTRL/ADDR/WDATA/RDATA` | `lut_sel` picks the bank; ADDR increments modulo 64. Contrast banks (2, 3) use `addr & 31` |
| `GTM_LUT_RDATA` | Reads the table at `GTM_LUT_ADDR`; index > 64 reads 0 |
| `RESIZER_OUT_W/H` | Live: active size = `floor((W − S_H)/2)·2 × floor((H − S_V)/2)·2` with `S_H = pattern[0]`, `S_V = pattern[1]`. If the resizer is enabled and the mode's target fits inside the active size, the target is used; otherwise the active size (CSR-09) |
| `AF_CTRL.search_start/abort` | No hardware behind them: stay 1 after a write until reset or soft reset (CSR-07) |

## 6. Commands and their milestone

| Register | Behaviour | Status |
|---|---|---|
| `COMMON_CTRL.isp_en` | Gates the pipeline (accepted SOF and every line) and the output geometry the ODMA captures (DEC-20) | M2 |
| `DMA_CTRL.idma_en/odma_en/max_burst_m1` | Engine enables, effective at frame boundaries (M2-A2); burst length captured at arm | M2 |
| `IDMA_BUF_VALID`, `ODMA_BUF_FREE`, `ODMA_BUF_DONE` | Ownership handshake of HAS §6.25.10 with SPEC-07 (FREE cleared when full) | M2 |
| `LSC_LOAD_CTRL`, `LSC_COEF_DATA`, `LSC_PROFILE_SEL` | Load sequencer with a staging buffer. The command bits self-clear, with priority abort > validate > begin. Begin is rejected (`LSC_ERROR`) while a load is busy, for an invalid destination or geometry, or for the active valid profile (ALG-LSC-05). Validate commits only with the exact coefficient count; otherwise `COEF_COUNT` is raised and the old contents are kept. A coefficient above 4.0 sets `COEF_RANGE`. `LSC_MESH_NODES` writes invalidate every profile. `LSC_PROFILE_SEL` takes effect at the next accepted SOF (ALG-LSC-06) | M3 |
| `GTM_LUT_DATA` | Writes go to the read bank only while `GTM_CTRL.en` = 0 or `manual` = 1; ADDR increments modulo 128. In auto mode the curve is built at EOF into the other bank and swapped at the next SOF | M3 |
| `AEC_CTRL.commit` | Pending (reads 1) until the next accepted SOF, which copies the shadow set (`en`, `ZONE_CFG`, `ZONE_SIZE`, `SAMPLE_CLIP`, `THRESH`) to the active set and clears the bit. Soft reset drops a pending commit (M3-A3) | M3 |

With M3 every command register is consumed by a block model, so
`debug_pending_commands()` stays 0 in the integrated IP. Commands without an
installed hook (unit-level use of the control unit) are still recorded. They
are discarded by external reset and by soft reset, together with the state
they target. The soft-reset write itself is consumed by the
control unit and is never recorded. The engines re-evaluate their start
conditions after every CSR write.

## 7. Block status and registers without hardware (M5)

| Register | Behaviour |
|---|---|
| `DEMOSAIC_STATUS`, `CCM_STATUS`, `GAMMA_STATUS`, `GTM_STATUS`, `NR_2D_STATUS`, `EE_STATUS`, `CNF_STATUS`, `RESIZER_STATUS` `.busy` | 1 from the accepted SOF until the pipeline has emitted the frame's last row; it stays 1 during a stall. Cleared by abort and reset (DEC-38) |
| `OFMT_CTRL`, `OFMT_Y_STRIDE`, `OFMT_UV_STRIDE` | Storage only. `stride_en` = 1 warns and does not change the output (DEC-39) |
| `D_WDR_*`, `TNR_3D_*` | Storage only. The feature is unsupported (HAS §3.1), and enabling it warns (M5-A1) |
| `BPC_TEMPORAL_VAR`, `BPC_PIXEL_AGE` | Storage only: dynamic detection is not modelled (DEC-28) |
| `CNF_STRENGTH`, `AWB_X/Y_BOUND_*`, `AF_ZONE_EN`, `AF_METRIC_CFG`, `AF_LUT_*`, `AF_VCM_*`, `AF_PEAK_SCORE`, `AF_FOCUS_RANGE` | Storage only: the CSR states there is no hardware behind them |

The complete per-register classification is
[ISP_CSR_TRACEABILITY.md](ISP_CSR_TRACEABILITY.md).

## 8. Statistics readout (M3)

Detail and rationale: `plan/alg/ALG_F_aec_awb_af.md`, DEC-29/30/33 and M3-A8.

| Register | Behaviour |
|---|---|
| `AEC_GLOBAL_SUM_LO/HI`, `AEC_GLOBAL_COUNT` | Channel `AEC_CHANNEL_SEL` (0 R, 1 Gr, 2 Gb, 3 B) of the last publication; stable until the next one |
| `AEC_ZONE_SUM/COUNT` (by channel), `AEC_ZONE_GREEN_OE_UE`, `AEC_ZONE_GREEN_MIN_MAX` | Zone `AEC_ZONE_ADDR` (raster order) of the single in-place memory (DEC-30), read as it is now. Before the first enabled SOF after a reset: the CSR reset value 0. From the SOF of an enabled frame: a zone already written in that frame reads its partial sums; a zone not yet written, an address beyond the grid, or any zone of an illegal grid reads the empty value (0; min 4095, max 0). After EOF the memory holds the complete frame until the next enabled SOF |
| `AEC_HIST_DATA` | Bin `AEC_HIST_ADDR` of the green histogram, in the same memory: 0 at the enabled SOF, then partial counts |
| `AWB_GLOBAL_*` | Published registers (35-bit sums as `_H[2:0]` + `_L`, 23-bit count) |
| `AWB_ZONE_*` | Zone `AWB_ZONE_ADDR` of the single AWB memory, same rules as AEC (partial sums of written zones; 0 for unwritten zones, beyond the grid and in global-only mode) |
| `AF_STAT_DATA` | Score of zone `AF_STAT_ADDR` (4×4, raster order); latched, coherent until the next publication |
| `*_FRAME_ID` | ID of the frame that was measured: one counter, +1 per accepted SOF since `i_rst_n`, kept by soft reset |
| `*_RESULT_CONTEXT_ID` | `*_CONTEXT_ID` as latched at that frame's SOF; cleared by soft reset |
| `AEC_STATUS` | `busy` from SOF to publication of an enabled frame; `stat_done` W1C, set per publication; `error` reads 0 |
| `AWB_STATUS.stat_done` | Set per publication |
| `AF_STATUS` | `busy` while a measured frame is in flight; `frame_done` W1C, set per publication (EN = 1 at EOF); `score_valid` = EN was 1 from SOF to EOF |

Software protocol (HAS §6.21.9): read FRAME_ID, then the results, then
FRAME_ID again, and retry if it changed. That protects the double-buffered
globals. For zone and histogram data, FRAME_ID changes only at EOF, so it
cannot detect a read that overlaps the next frame. That read returns partial
sums of the new frame mixed with the finished one. Software must also see
`AEC_STATUS.busy` = 0 (AEC) or `COMMON_STATUS.busy` = 0 (AWB) before and
after the read, or hold the next input buffer until the readout is done
(ALG-STAT-02).
