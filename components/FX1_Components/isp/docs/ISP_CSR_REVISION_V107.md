# FX1 ISP VP — CSR/MAS/Tuning revision v1.07 (V107-1)

Milestone V107-1 of plan `ISP_V107_UPDATE_PLAN_2026-10-28` covers the ABI and
the register engine:
- pin CSR v1.07 and regenerate the ABI artefacts;
- add the W1P access type and the v1.07 priority rules (MAS Table 8-178, Table 8-180);
- report discovery (`COMMON_VER_ID`, `TUNE_ID`, `TUNE_REV`, `TUNE_CAPS`);
- list what the model does not implement yet, with its milestone.

Behaviour lands in later milestones (V107-2..9). Each one moves its registers
from `deferred` to `model` in [ISP_CSR_TRACEABILITY.md](ISP_CSR_TRACEABILITY.md).
Decisions and assumptions are in §10 of
[ISP_DECISIONS_AND_DISCREPANCIES.md](ISP_DECISIONS_AND_DISCREPANCIES.md).
The sources are confidential and are pinned in
[ISP_SOURCE_MANIFEST.md](ISP_SOURCE_MANIFEST.md); this page carries no
spreadsheet text.

## ABI delta

| | 07/10 (`bda077b6…`) | v1.07 (`444e72f8…`) |
|---|---|---|
| Registers | 254 | 573 |
| Fields | 336 | 819 |
| RW / RO / W1C / W1S / W1SC / W1P fields | 201 / 101 / 20 / 12 / 2 / — | 464 / 288 / 35 / 11 / 2 / 19 |

No register was removed or moved. 319 registers are new and hold 477 fields.
Of the 324 retained fields:
- 12 changed bits, access or reset;
- 6 fields are new in retained registers.

The `Registers` and `Flat Registers` sheets agree.

**Retained registers whose ABI changed (13):**

| Register | Change | Model in V107-1 |
|---|---|---|
| `COMMON_VER_ID` | reset `0x00010002` → `0x00010007` | reports v1.07 (§10 V107-D3) |
| `COMMON_VER_DATE` | reset `0x18072026` → `0x18102026` | reports the map date |
| `AEC_THRESH` | reset UE `0x100` → `0x110`, OE `0xF00` → `0xEA0` | new reset used, see "Regenerated data" |
| `AEC_GLOBAL_COUNT` | count 21 → 23 bits | saturates at the new width |
| `AEC_HIST_DATA` | count 22 → 23 bits | saturates at the new width |
| `AWB_RESULT_CTRL` | `acquire` / `release` / `clear_report` W1S → W1P | one-clock commands (the 07/10 model already cleared them) |
| `GAMMA_CTRL` | new `lut_load_confirm` (W1P) | stored nowhere; gate in V107-4 |
| `GAMMA_STATUS` | new `lut_loaded` (RO) | reads its reset until V107-4 |
| `EE_LUT_CTRL` | new `profile_load_confirm` (W1P) | stored nowhere; gate in V107-4 |
| `EE_STATUS` | new `lut_valid`, `active_profile`, `active_valid` (RO) | read their reset until V107-4 |
| `RESIZER_CTRL` | `scale` 4 → 5 bits [7:3] | codes 16..31 stored, select FullRes (V107-A7) |
| `RESIZER_OUT_W/H` | reset 1920×1080 → 0 | VP keeps the published 07/10 set until V107-7 (V107-D2) |

**Header macros:**
- Removed: `FX1_ISP_AEC_CHANNEL_SEL_CHANNEL_SEL_{R,GR,GB,B}`. The v1.07 field
  has a different meaning, so there is no alias; no code used them.
- Values changed: 21, listed in the V107-1 report. Users outside the generated
  files:
  - `src/control/control_unit.cpp` and `control_unit_stats.cpp`, which use the
    widened masks;
  - `fw/fx1_soc/freertos/hwcheck.c`, which compares `COMMON_VER_ID` against its
    `_RESET` macro, so it follows the header;
  - `tools/gen_csr.py` (`CSR_NUM_REGISTERS`).
- `RESIZER_CTRL_SCALE_*` names: kept for codes 0..14 and added for 16..21
  (MAS Table 6-122), through override `VP-NAMES-RESIZER`.

## Generator changes (`tools/gen_csr.py` v2.0)

- **Input:** reads the `.xlsm` map; pinned SHA `444e72f8…`; expected counts
  573 / 819 and per-access counts as above.
- **Access type `W1P`:** `masks["W1P"]` in the schema, `w1p_mask` in
  `csr::reg_desc`.
- **Overrides:** each one is asserted against the spreadsheet text, as before.
  - `VP-NAMES-CSC`: text updated.
  - `VP-NAMES-ODMA-BUF`: extended to CH1/CH2.
  - `VP-NAMES-RESIZER`: CH0/CH1/CH2 preset names; CH2 has only its legal codes.
  - `CSR107-BUFCOUNT`: bits 7:4 of the buffer-ownership fields are tied to zero
    (build buffer count 4). This covers `IDMA_BUF_VALID`, `ODMA_BUF_FREE/DONE`
    and CH1/CH2 `FREE/DONE`.
- **Cross-check:** `tools/check_csr_header.py` accepts `.xlsm` and also
  checks each field's access type and msb against the header comments.

## Register engine (V107-1)

The rules are in [ISP_CSR_CONTRACT.md](ISP_CSR_CONTRACT.md) §1:
- W1P one-clock pulses that are never stored;
- software set wins on W1S, as MAS Table 8-180;
- consumers (IDMA, ODMA, AEC commit) sample requests as registered on their
  edge (V107-A6);
- hardware set wins on W1C, also over a hardware clear;
- a pre-write filter that can reject a write or narrow it to some lanes;
- bus-read actions that run on bus reads, not on debug reads;
- `hw_ack`, the immediate self-feedback clear.

New unit test `fx1_isp.unit.register_engine`. The access sweep of
`test_control_unit` covers every register of the v1.07 map.

## Discovery

| Register | Value | Note |
|---|---|---|
| `COMMON_VER_ID` | `0x00010007` | v1.07 map |
| `COMMON_VER_DATE` | `0x18102026` | |
| `TUNE_ID` | `0x54554E45` ("TUNE") | constant |
| `TUNE_REV` | `0x00010007` | constant |
| `TUNE_CAPS` | `0x00000000` | implemented capabilities only (V107-A1). The CSR reset `0x003FFFFF` describes a complete build. Each milestone sets its bits as it lands. |

## Regenerated data (attributed)

- `tests/data/vectors/stats_reset_grids_af_edges_130x90/expected_stats.txt`
  and `tests/data/sequences/cont_100/stats_00{0,1}.txt`: 26 values of
  `AEC_ZONE_GREEN_OE_UE` changed.
  - These cases do not program `AEC_THRESH`, so they count against the new
    reset thresholds.
  - No other readout and no pixel changed.
- `tests/data/vectors/resizer_plans.txt`: extended from 16 to 32 scale codes,
  160 → 320 rows. The new rows are pass-through (V107-A7).
- `fw/fx1_soc/tests/isp_stream/isp_fixture.h`: only the source hash changed,
  because the schema changed. The profiles and CRCs are identical.
- `docs/ISP_CSR_TRACEABILITY.md`: 573 registers.
  - model 153, model (indexed) 75, constant 4, storage 25, deferred 316.
  - Every deferred entry names the milestone from the V107-0 field matrix.
    The generator checks that a deferred register gains no model reference.
