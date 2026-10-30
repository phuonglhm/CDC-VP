# FX1 ISP VP — CSR behaviour contract (M1)

This is the register-level behaviour the model implements. The map itself
(offsets, fields, reset values) is `include/fx1_isp/fx1_isp_csr.h`, generated
from the pinned CSR spreadsheet. Decision and issue IDs refer to
[ISP_DECISIONS_AND_DISCREPANCIES.md](ISP_DECISIONS_AND_DISCREPANCIES.md).

## 1. Access types (MAS v1.07 Table 8-178, Table 8-180)

| Type | Software write | Software read | Hardware |
|---|---|---|---|
| RW | Bits in strobed lanes take the written value | Stored value | May update (e.g. LUT address auto-increment) |
| RO | Ignored | Hardware value | Drives the value |
| W1C | 1 clears, 0 no effect | Stored value | Sets the bit |
| W1S | 1 sets, 0 no effect | Current state, not the last write | Clears the bit |
| W1SC | 1 pulses a command (recovery request) | Always 0 | — |
| W1P | 1 issues a one-clock command; repeatable; not stored | Always 0 | — |

Next-state equations per bit (Table 8-180), where `M` is the strobed-lane mask:
- RW: `(Q & ~M) | (D & M)`;
- W1C: `(Q & ~(D & M | H)) | S`, where `H` is a hardware clear and `S` a hardware set;
- W1S: `(Q & ~H) | (D & M)`.

General rules:

- Every access completes with OKAY. Unmapped words read 0 and ignore writes.
  Reserved bits read 0 and ignore writes.
- Byte strobes are honoured per lane. A write with no strobes changes nothing
  and triggers no side effect (no LUT write, no auto-increment, no command).
- Several W1C/W1S/W1P bits in one write act independently. In a word that
  mixes RW and W1P fields, the consumer sees the previous and the new RW
  value together with the pulse of that write.
- Byte strobes qualify both field updates and command pulses. A programming
  data register still strobes its consumer on any write with a nonzero byte
  enable.
- **Software set wins (W1S):** if software writes 1 to a W1S bit on the edge
  where hardware clears it, the bit stays set, whether it was 0 or 1 before
  and in whatever order the model applies the two.
- **Consumers sample the registered value (V107-A6):** a request register
  reaches its consumer as registered on the consumer's edge. A request set on
  edge c is seen on edge c + 1, so no consumer takes it on the edge that sets
  it. A request re-asserted on the edge that consumes it is kept and is a new
  request. This covers `IDMA_BUF_VALID` (IDMA arm), `ODMA_BUF_FREE` (ODMA take,
  which `SOF_TS` reports) and `AEC_CTRL.commit` (SOF transfer).
- **Self-feedback clear:** a command bit that the model clears inside the
  handler of its own write (`hw_ack`, e.g. `LSC_LOAD_CTRL`) clears at once,
  outside the priority rules (MAS §8.1.3).
- **Hardware set wins:** if hardware sets a W1C bit in the same core-clock cycle
  as a software write-1-to-clear or a hardware clear, the bit stays set. The cycle is
  `floor((sc_time_stamp() + annotated delay) / core_period)`.
- **Level-held bits** (`DMA_ERR.IDMA_UNDERRUN`, CSR-06): while the condition
  persists a W1C write does not clear the bit. Once the condition drops the
  bit stays set until software clears it. The derived interrupt bits are set
  on the rising edge of the condition only.
- Reserved bits inside declared fields are tied to zero:
  `DMA_IRQ_EN[0]`, `DMA_IRQ_STAT[0]` (SPEC-05), `DMA_ERR[5]`. Bits 7:4 of the
  buffer-ownership fields are tied to zero above the build count of 4
  (`IDMA_BUF_VALID`, CH0/CH1/CH2 `ODMA_BUF_FREE/DONE`; CSR107-BUFCOUNT).
- The custom-size words `RESIZER_CUSTOM_W/H` (CH0/CH1/CH2) are reserved in
  v1.07: they read 0 and ignore writes.
- **Read side effects** run on bus reads only. A debug read (TLM debug
  transport, `csr_read` without a cycle) never runs them.
- **Input (V107-2):** `COMMON_INPUT_FORMAT.DEPTH` 0 RAW12 / 1 RAW10, both
  right-aligned. The value is captured with the ingress frame, and the
  pipeline normalizes it once (RAW10 `<< 2`); 2/3 refuse the input buffer
  (V107-A10). `COMMON_FRAME_WIDTH/HEIGHT` are external, at most 3842 × 2162; a
  refused input buffer has its VALID bit cleared and the rotation kept
  (V107-D6). `COMMON_CTRL.FRAME_START` is a self-clearing no-op (V107-D7).
- **Profile controller (V107-3, MAS v1.07 §9.7–9.9):**
  - `TUNE_MODE.ENABLE` selects profile mode. It changes only while idle with no
    leased result bank; otherwise MODE.
  - Profile mode starts with no active profile and holds ingress until the
    first APPLY activates.
  - Bank-owned words read and write the `TUNE_BANK`-selected bank. TUNE
    extension words do so in legacy mode too, as storage; their datapath value
    stays at the reset value in legacy mode.
  - Writes to a sealed, active, pending or referenced bank set LOCKED and have
    no effect. Writes that change no strobed bit are no-ops.
  - Commands: `TUNE_CMD` takes one command per write, with EXACT only together
    with APPLY. The APPLY error order is MODE, PENDING, NOT_READY, INVALID,
    MISSED.
  - IDLE-class words (input geometry, Bayer, input format, OUTPUT_ENABLE,
    LSC_MESH_NODES, TUNE_MODE) are rejected with MODE unless idle. In profile
    mode, the input-domain words are rejected even when idle.
  - At most four admitted, unretired frames in both modes.
- **Tables (V107-4):**
  - Gamma and EE apply only once confirmed. Legacy confirmation needs the
    block disabled and the pipeline idle. Every payload write clears it.
  - GTM MANUAL applies the frame bank's manual curve, and only when all 65
    points have both byte lanes written.
  - Bank payloads (gamma, NR, BPC, LSC sectors) are confirmed with
    `TUNE_TABLE_CMD` in profile mode.
  - SEAL pins the EE and LSC residents it uses.
  - LSC mesh: 2..32 nodes per axis.
- **Kernel parameters (V107-5):**
  - CSC full range is the reset, in legacy mode too (V107-D22); limited range
    and the look matrix are profile-mode settings.
  - GTM KEY, LWHITE and MANUAL are captured at the accepted SOF (V107-D23).
  - The other `TUNE_*` kernel fields (WB Gb, demosaic margins, CNF and NR
    blends, BPC thresholds and factory list, LSC sectors, NR modes) act only in
    profile mode; legacy mode uses their resets (V107-D24..D29).
- **Discovery:** `TUNE_CAPS` reports only the capabilities the model
  implements (V107-A1), not the CSR reset of a complete build.

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

LUT contents after reset are undefined (MAS §6.5.2, Table 183). The model
fills Gamma, EE and the GTM banks with a fixed, deterministic poison pattern,
`((i * 0x9E37) ^ 0xA5A5) & mask`; the Python reference uses the same pattern.
A block enabled without its table therefore gives a visibly wrong picture.
Software must load a table before enabling its block. LSC profile validity is
cleared. (CSR-16 is superseded.)

### 3.2 Soft reset (MAS §6.5.3, Table 183 p316-317)

`COMMON_CTRL.soft_rst` and `DMA_CTRL.soft_reset` are W1SC: they read 0, and a
write requests a recovery (MAS v1.07 §9.9.8, V107-D62..D67): the AXI
transactions already issued are drained, then a **15-core-clock** pulse runs.
The register effects happen at the first edge of the pulse (at the write when
nothing is outstanding); the engines stay in reset until the final retirement,
which clears every VALID / FREE bit and publishes the recovery report
(`COMMON_RECOVERY_STATUS`, `SEQUENCE`, `COMMON_ABORTi_*`). A request while a
recovery runs is rejected with `TUNE_ERROR.PENDING`. An AXI error response
requests a DMA-scope recovery automatically. The two sources have different
scopes.

| | `COMMON_CTRL.soft_rst` | `DMA_CTRL.soft_reset` |
|---|---|---|
| Scope | Pixel pipeline and both DMA engines | DMA engines and the frame in flight |
| RW configuration (incl. `idma_en/odma_en`, `isp_en`) | Preserved | Preserved |
| Gamma, EE, GTM, LSC contents; LSC validity | Preserved | Preserved |
| `IDMA/ODMA_FRAME_COUNT`, `AEC/AWB/AF_FRAME_ID` | Preserved; the aborted frame's number is skipped | Preserved; skipped |
| Pending `AEC_CTRL.commit` | Preserved, consumed at the next accepted SOF (owner answer Q4) | Preserved |
| Buffer ownership, frame in flight | Cleared / discarded | Cleared / discarded |
| `DMA_IRQ_STAT`, `DMA_ERR` (incl. held bits) | Cleared | Kept (v1.07) |
| `*_ODMA_BUF_DONE` and published output metadata of completed buffers | Cleared | Kept (v1.07) |
| Recovery status, sequence and records | Kept (cleared by `i_rst_n`) | Kept |
| `COMMON_IRQ_STATUS`, `COMMON_STATUS`, statistics results and done flags, other pending commands, LSC load in progress, 2DNR estimate | Cleared | Kept, except `COMMON_STATUS.busy`, which clears because the pipeline is flushed (assumption U1-A1 for `COMMON_IRQ_STATUS`) |

Rule used by the model: every RO/W1C/W1S bit in the scope returns to its reset
value, except for the preserved registers listed above. For the DMA scope, the
registers in scope are those from `DMA_CTRL` (0x3000) up.

During the pulse, software writes still update RW configuration, but their
W1S/W1C effects and W1P commands are dropped, because the state they target is
held in reset (V107-D4).
For example, arming `IDMA_BUF_VALID` inside the pulse has no effect.

## 4. Interrupts (HAS §9.2)

```
irq = |(COMMON_IRQ_STATUS & COMMON_IRQ_EN) | |(DMA_IRQ_STAT & DMA_IRQ_EN)
```

Status bits latch whether or not their enable bit is set. The line stays high
while any enabled status bit is set.

| COMMON_IRQ_STATUS bit | Set by |
|---|---|
| `frame_done_irq` | Output frame complete: last write response of both planes (M2) |
| `error_irq` | Rising edge of `DMA_ERR.ALIGN`, `AXI_RD` or `AXI_WR`, any `LSC_ERROR` bit, `EE_STATUS.error` or any `TUNE_ERROR` bit, and every accepted recovery (MAS v1.07 Table 9-188; DEC-15 superseded). Flow control (`IDMA_UNDERRUN`, `ODMA_OVERFLOW`) and statistics drops (`BPC_STATUS.cand_reject_ovf`) set nothing. A result word read without a lease sets `TUNE_ERROR.NO_LEASE`, hence `error_irq` |
| `stats_ready_irq` | Every statistics publication (AEC, AWB, AF), also when that block's done bit is still set (M3-A4); a backdoor set of a done bit also raises it |

`DMA_IRQ_STAT` bits are set by the DMA engines (M2). Bit 0 is `CFG_ERROR`
(U1, SPEC-05 superseded): a refused IDMA or ODMA start sets it together with
`DMA_ERR.ALIGN`, which also raises `error_irq`. A geometry mismatch found
after the start sets `DMA_ERR.ALIGN` only (U1-A2).

## 5. Register-specific behaviour implemented in M1

| Register | Behaviour |
|---|---|
| `COMMON_STATUS.error` | Live OR of those sticky error bits (Table 9-188); clears when they are cleared |
| `COMMON_STATUS.frame_done` | Set together with `frame_done_irq`; cleared at the next accepted SOF (DEC-17) |
| `COMMON_CTRL.frame_start` | Pending flag, cleared at the next accepted SOF; does not start or block frames (DEC-16) |
| `GAMMA_LUT_ADDR/DATA/RDATA` | A DATA write stores the merged 12-bit value at ADDR, then ADDR increments modulo 256. DATA reads back the last value written; RDATA reads the table at ADDR (CSR-21) |
| `EE_LUT_CTRL/ADDR/WDATA/RDATA` | `lut_sel` picks the bank of the profile `EE_CTRL.profile_wr_sel` names; ADDR increments modulo 64, also for a write ignored under code 3; RDATA reads 0 under code 3 (U3-A2). Contrast banks (2, 3) use `addr & 31` |
| `EE_ALPHA` … `EE_RADIAL_GAIN_23` | Written into the profile `profile_wr_sel` names; code 3 ignores the write and keeps the readback. Readback = last value written through the register. Each profile resets to the CSR values (MAS p189) |
| `EE_CTRL.en`, `.profile_sel` | Sampled at the accepted SOF with the selected profile's scalars (code 3 selects profile 0); the profile's tables are read live row by row (MAS §6.18.4.1) |
| `EE_STATUS.error` | W1C; set at the end of every frame EE processes (EE on, at least 5 × 5) with radial gain on and a radial gain of the frame's profile above 1.5 (MAS §9.2); not a level of the live registers |
| `GTM_LUT_RDATA` | Reads the table at `GTM_LUT_ADDR`; index > 64 reads 0 |
| `RESIZER_OUT_W/H` (and `CH1_`/`CH2_`), `CHc_ACTIVE_CROP_ORIGIN/SIZE`, `CHc_GEOMETRY_STATUS` | The published set of the channel (MAS v1.07 §6.20.4.3, §8.3), for the key {active size = `floor((W − S_H)/2)·2 × floor((H − S_V)/2)·2` and input format, crop enable/origin/size, scaler enable and SCALE}: the preset (or the crop for FullRes / a disabled scaler) when the setup is legal. GEOMETRY_STATUS: BUSY while the live key is not yet published (VALID = ERROR = 0); else VALID or the ERROR priority code (1 active size/format, 2 crop bounds, 3 odd crop, 4 reserved mode for the channel, 5 above CHc_OUTPUT_LIMIT, 6 above the crop; lowest wins). An invalid set keeps the last valid readback; an invalid enabled channel sets the sticky TUNE_ERROR.INVALID. A key written on edge w is stable from w + 1; writes before one edge give one key; a scaling set is published at E35 (visible from w + 37 when the dividers are free), a pass-through or invalid set on the edge after the key is stable; a key changed during a derivation is derived when the dividers are free, the stale result never published. Reset (i_rst_n, COMMON soft reset): readback zero; the live key is derived from the end of the pulse |
| `RESIZER_CTRL.updated` | No effect (MAS §9.2; DEC-24/26 superseded) |
| `RESIZER_CTRL` (three channels), `CHc_CROP_CTRL/ORIGIN/SIZE` | IDLE class (V107-7): written only with the input stopped and no frame in flight; otherwise rejected with TUNE_ERROR.MODE |
| `CHc_OUTPUT_LIMIT` | Fixed RO: 3840 × 2160 (CH0, CH1), 640 × 640 (CH2) |
| `AF_CTRL.search_start/abort` | No hardware behind them: stay 1 after a write until reset or soft reset (CSR-07) |

## 6. Commands and their milestone

| Register | Behaviour | Status |
|---|---|---|
| `COMMON_CTRL.isp_en` | Gates the pipeline (accepted SOF and every line). The ODMA takes a frame's geometry from the frame at its claim (MAS v1.07 §6.25.3.8; DEC-20 superseded in V107-8) | M2, V107-8 |
| `DMA_CTRL.idma_en/odma_en/max_burst_m1` | Engine enables, effective at frame boundaries (M2-A2); `odma_en` enables the three output engines; burst length captured at each claim | M2, V107-8 |
| `IDMA_BUF_VALID`, `ODMA_BUF_FREE`, `ODMA_BUF_DONE` | Ownership handshake of HAS §6.25.10; `FREE` cleared when the ODMA starts the frame (MAS Table 159; SPEC-07 superseded) | M2, U5 |
| `IDMA_BUF_TAG/TS0..3`, `ODMA_BUF_TAG/TS/SOF_TS/EOF_TS0..3`, `FRAME_COUNTER` | Frame metadata (MAS Table 160): input tag/timestamp captured at frame start, published with the output buffer at completion; `FRAME_COUNTER` core clocks since `i_rst_n` release, modulo 2^32 (U5-A1..A5) | U5 |
| `LSC_LOAD_CTRL`, `LSC_COEF_DATA`, `LSC_PROFILE_SEL` | Load sequencer with a staging buffer. The command bits self-clear, with priority abort > validate > begin. Begin is rejected (`LSC_ERROR`) while a load is busy, for an invalid destination or geometry (2..128 nodes per axis, not above the active size), or for the active profile, valid or not (MAS Table 31, owner Q5). Begin invalidates its destination at once. Validate commits only with the exact coefficient count; otherwise `COEF_COUNT` is raised and the destination stays invalid, as after an abort. A coefficient above 4.0 sets `COEF_RANGE`. `LSC_MESH_NODES` writes invalidate every profile. `LSC_PROFILE_SEL` is checked when written and again at the next accepted SOF: it becomes active only if its profile is still valid then; otherwise the previous profile stays active and `PROFILE_SEL_REJECT` is set at every SOF while the selector differs (MAS Figure 29, review R2) | M3, U1 |
| `GTM_LUT_DATA` | Writes go to the read bank only while `GTM_CTRL.en` = 0, whatever `manual` says (MAS p146, U3); ADDR increments modulo 128 on every write. In auto mode the curve is built at EOF into the other bank and swapped at the next SOF | M3 |
| `AEC_CTRL.commit` | Pending (reads 1) until the next accepted SOF, which copies the shadow set (`en`, `ZONE_CFG`, `ZONE_SIZE`, `SAMPLE_CLIP`, `THRESH`) to the active set and clears the bit. A pending commit survives both soft resets and is cleared by `i_rst_n` (owner Q4, DEC-32 superseded) | M3, U1 |

With M3 every command register is consumed by a block model, so
`debug_pending_commands()` stays 0 in the integrated IP. Commands without an
installed hook (unit-level use of the control unit) are still recorded. They
are discarded by external reset and by the COMMON soft reset, together with
the state they target; the DMA-only soft reset keeps them (MAS Table 183). The soft-reset write itself is consumed by the
control unit and is never recorded. The engines re-evaluate their start
conditions after every CSR write.

## 7. Block status and registers without hardware (M5)

| Register | Behaviour |
|---|---|
| `DEMOSAIC_STATUS`, `CCM_STATUS`, `GAMMA_STATUS`, `GTM_STATUS`, `NR_2D_STATUS`, `EE_STATUS`, `CNF_STATUS`, `RESIZER_STATUS` `.busy` | 1 from the accepted SOF until the pipeline has emitted the frame's last row; it stays 1 during a stall. Cleared by abort and reset (DEC-38) |
| `OFMT_CTRL`, `OFMT_Y_STRIDE`, `OFMT_UV_STRIDE` | Storage only. `stride_en` = 1 warns and does not change the output (DEC-39) |
| `D_WDR_*`, `TNR_3D_*` | Storage only. The feature is unsupported (HAS §3.1), and enabling it warns (M5-A1) |
| `BPC_MODE`, `BPC_TEMPORAL_VAR`, `BPC_PIXEL_AGE` | Storage only: spatial BPC only, `dynamic_det_en` has no effect (MAS §6.9; DEC-28 superseded) |
| `CNF_STRENGTH`, `AWB_X/Y_BOUND_*`, `AF_ZONE_EN`, `AF_METRIC_CFG`, `AF_LUT_*`, `AF_VCM_*`, `AF_PEAK_SCORE`, `AF_FOCUS_RANGE` | Storage only: the CSR states there is no hardware behind them |

The complete per-register classification is
[ISP_CSR_TRACEABILITY.md](ISP_CSR_TRACEABILITY.md).

## 8. Statistics readout (M3, v1.07 result banks since V107-6)

Detail and rationale: MAS v1.07 §6.21–6.23, §9.8 and decisions §10.6
(V107-D31..D40, V107-A22..A25). DEC-29/30/33 and M3-A7/A8 (one in-place
memory with partial sums) are superseded.

Every engine has two result banks: FREE → FILLING (reserved at an enabled SOF
with a legal set) → READY (after the frame's last sample) → LEASED (acquired
by software) → FREE. READY and LEASED banks are never written, so a leased
result is coherent by construction. AE/AF results need a lease in legacy mode
too.

| Register | Behaviour |
|---|---|
| `TUNE_AEC_CMD`, `TUNE_AF_CMD` | W1P, read 0: `ACQUIRE` takes the oldest bank READY before this edge; `RELEASE` frees the lease; both in one write release first. A second ACQUIRE or a RELEASE without a lease sets `TUNE_ERROR.NO_LEASE`; ACQUIRE with nothing READY takes nothing, without error |
| `TUNE_AEC_STATUS`, `TUNE_AF_STATUS` | `LEASE_VALID` (from the edge after ACQUIRE, V107-D37), `READY_COUNT`, `BUSY` (a bank FILLING) |
| `TUNE_AEC_INDEX`, `TUNE_AEC_DATA_LO/HI` (and AF) | 64-bit result at the logical index of Table 9-193 (AEC: 0..8 globals, 0x10–0x12 ROI/steps, 0x100–0x17F histograms, 0x200 + 8z zones; AF: 0 status, 0x10–0x13 geometry/mask/mode, 0x100 + 4w window, 0x200 + 2w rectangle). DATA_LO snapshots the high word for DATA_HI; an INDEX write or RELEASE invalidates the snapshot. A reserved index reads 0 and sets INVALID; a zone beyond the captured grid reads 0 without error (V107-A23) |
| `TUNE_AEC_FRAME`, `TUNE_AEC_PROFILE`, `TUNE_AEC_RESULT_META`, `TUNE_AE_FRAME_SIZE/GRID/ZONE_SIZE` (and the AF words) | Identity and captured geometry of the leased bank |
| `TUNE_AEC_DROPS`, `TUNE_AF_DROPS` | Enabled measurements skipped (no FREE bank, illegal AEC set), saturating; W1C: a nonzero write clears the word, a drop on the same edge is kept |
| `AEC_GLOBAL_*`, `AEC_ZONE_*`, `AEC_HIST_DATA`, `AEC_FRAME_ID`, `AEC_RESULT_CONTEXT_ID`, `AF_STAT_DATA`, `AF_FRAME_ID`, `AF_RESULT_CONTEXT_ID` | Narrow 07/10 windows: modulo projections of the leased bank's 64-bit results (Bayer aggregate; `AEC_CHANNEL_SEL` 1..3 read 0). 0 without a lease, and the bus read sets NO_LEASE |
| `AWB_GLOBAL_*`, `AWB_FRAME_ID`, `AWB_RESULT_CONTEXT_ID` | Words of the leased bank (35-bit sums as `_H[2:0]` + `_L`, 23-bit count), from the edge after ACQUIRE; 0 without a lease, without error (MAS §6.22.4; `TUNE_AWB_PROFILE/FRAME_SIZE` likewise, V107-A22) |
| `AWB_ZONE_*` | Zone `AWB_ZONE_ADDR` of the leased bank, for the grid captured with it; 0 for untouched zones, addresses beyond that grid and without a lease |
| `AWB_RESULT_CTRL` | One-clock commands, read 0: `acquire` (oldest READY bank; no-op with a lease or nothing READY, or for a bank published on the same edge), `release` (wins over acquire; no-op without a lease), `clear_report` (clears `overrun` and `AWB_DROP_COUNT` only; a loss on the same edge is kept) |
| `AWB_RESULT_STATUS` | `ready_count`, `leased` = `read_valid`, `read_bank`, `write_busy`, `overrun`, `read_num_zone_x/y` of the lease (0 without one) |
| `AWB_DROP_COUNT` | Enabled measurements skipped (no FREE bank at SOF, illegal grid), saturating at 2^32 − 1; disabled frames do not count |
| `*_FRAME_ID` | ID of the frame that was measured: one counter, +1 per accepted SOF since `i_rst_n`, kept by soft reset |
| `*_RESULT_CONTEXT_ID` | `*_CONTEXT_ID` as latched at that frame's SOF |
| `AEC_STATUS` | `busy` while a bank is FILLING; `stat_done` W1C, set per publication; `error` reads 0 |
| `AWB_STATUS.stat_done` | Set per publication (a bank becoming READY); clearing it releases no bank |
| `AF_STATUS` | `busy` while a measured frame is in flight; `frame_done` W1C, set per publication; `score_valid` while a readable AF lease is held (V107-A24) |

Reset scopes: common soft reset and `i_rst_n` clear banks, leases and drops;
the DMA reset keeps READY and LEASED banks and aborts only the measurement in
flight. A `TUNE_MODE` change with any lease held is rejected with MODE.

Software protocol: ACQUIRE, confirm `LEASE_VALID` one clock later, read the
identity and the results, RELEASE. The FRAME_ID retry of HAS §6.21.9 is not
needed.
