# FX1 ISP VP — CSR extension 0x20261109 (consolidated update)

Workspace plan `ISP_CONSOLIDATED_UPDATE_PLAN_2026-11-09`, issues ISP-UPD-001,
-002 and -007. Owner decisions of 2026-11-09, applied **on top of** the
pinned CSR workbook `FTEL_IP_ISP_CSR_v1.07.xlsm` (SHA-256 `444e72f8…b721fa`).
The workbook stays the baseline and is not modified.

**Status: VP extension, not part of the official CSR v1.07.** The owner
decided *what* is needed: the gain range, eight BLC profiles with four
scales, the offset units. The register names, offsets, field widths, resets
and the compatibility policy below are the VP design that implements those
decisions. They are to be reconciled when a CSR revision publishes the
change. `COMMON_VER_ID` keeps the workbook value `0x00010007`. Software
detects the extension with `FX1_ISP_CSR_EXTENSION` (`0x20261109`) in
`fx1_isp_csr.h`.

## 1. Source and generation

- `tools/gen_csr.py` (`EXTENSIONS`, generator 2.1). The flow is:
  1. read the workbook;
  2. apply the existing overrides;
  3. assert the workbook counts (573 registers, 819 fields);
  4. apply the extension;
  5. assert the extended counts (621 registers, 867 fields, 512 RW fields).

  Each extension entry is checked against the workbook item it changes
  (bit range, offset, absence of the new register). A later revision that
  already contains a change therefore fails loudly instead of being
  extended twice.
- The generated header, register table and schema carry the extension:
  - in the header, `(extension ISP-UPD-xxx)` comments;
  - in the schema, the `"extension"` object and the per-register and
    per-field `"extension"` keys.

  New registers have no workbook row (`row` 0).
- `tools/check_csr_header.py` restates the extension independently and
  cross-checks the header against the workbook plus that restatement.

## 2. Changes

| ID | Item | v1.07 | Extension | Reset |
|---|---|---|---|---|
| ISP-UPD-001 | `LSC_COEF_DATA.coef_data` | [20:0], UQ3.18, range error above 4.0 | **[21:0], UQ4.18**, 0 ≤ gain ≤ **8.0 inclusive** (`0x200000`) | 0 (unchanged) |
| ISP-UPD-002 | `BLC_GAIN_SEL.gain_level` | [1:0], G0..G3 | **[2:0], G0..G7** (`range_scale_en` stays bit 8) | 0, word `0x100` (unchanged) |
| ISP-UPD-002 | `BLC_SCALE_G0..G3` (0x0258..0x0264) | one UQ2.16 scale per profile, all four colours | **removed**; the offsets are reserved (read 0, writes ignored) | — |
| ISP-UPD-002 | `BLC_OFS_G4..G7_{DFT,R,GR,GB,B}` | — | **new**, 0x0268 + 0x14·(g−4) + 4·i (i = DFT, R, Gr, Gb, B), unsigned 12 bit [11:0], RW | DFT 200 (`0xC8`), trims 0 |
| ISP-UPD-002 | `BLC_SCALE_G0..G7_{R,GR,GB,B}` | — | **new**, 0x02B8 + 0x10·g + 4·c (c = R, Gr, Gb, B), UQ2.16 18 bit [17:0], RW | `0x10000` (1.0) |
| ISP-UPD-007 | `CCM_OFS_R/G/B.offset_*` | signed 12 bit, Q3.9 at the accumulator (512 = one level) | signed 12 bit, **output pixel levels** (1 = one level), −2048..2047 | 0 (unchanged) |

All new registers lie in reserved space of the BLC block (0x0200..0x03FF),
which ends at 0x0334. G4..G7 cannot be reached by extending the v1.07
stride: 0x0208 + 4·0x14 = 0x0258 would hit the old `BLC_SCALE_G0`.
Profile g is therefore addressed through a table of per-register offsets
(model `blc_map`, driver `blc_map`), not through `g · stride`.

### 2.1 LSC (ISP-UPD-001)

- The stored node gain is UQ4.18 and the quantisation step stays 2^-18, so
  every v1.07 code keeps its meaning. Exact 8.0 is `0x200000`; it no longer
  wraps to 0 through a 21-bit mask.
- Out of range: a written value above `0x200000` sets
  `LSC_ERROR.COEF_RANGE`, and the value masked to [21:0] is still stored
  and counted (flag-and-store, as for 4.0 in v1.07, ALG-LSC-03). This
  covers a field code above 8.0 and any bit above the field. The profile
  validates when the count is right.
- The driver `fx1_isp_lsc_load` refuses a coefficient above 8.0 with
  `FX1_ISP_EINVAL` before writing any register.
- The datapath formula, the strength (UQ1.16), the rounding points and the
  final Bayer12 saturation are unchanged. The intermediate products are
  64-bit, and the effective gain after the strength is not clamped.
- Three residents are unchanged. The data decision "duplicate the current
  mesh into profiles 0/1/2" and its boot sequence are documented in the
  programming guide and in decisions §13. The active-profile protection
  is not relaxed.

### 2.2 BLC (ISP-UPD-002)

```text
g = BLC_GAIN_SEL.gain_level (0..7), c = CFA channel after the Input Formatter (R, Gr, Gb, B)
offset[c] = min(DFT[g] + trim[g][c], 4095)
d = max(input - offset[c], 0)
out = range_scale_en ? sat12((d * scale[g][c] + 2^15) >> 16) : d
```

- The only datapath change is `scale` → `scale[g][c]`. The rounding, the
  saturations and the bypass with `BLC_CTRL.en = 0` are unchanged.
- `range_scale_en = 0` ignores all four scales; the offsets still apply.
- All 72 values and the selector (mask `0x107`) are bank-owned algorithm
  scalars in profile mode (`profile_fields.cpp`). They are latched with
  the bank at the activating SOF.
- The eight BLC gain profiles are independent of the two TUNE algorithm
  banks.

**Compatibility of the removed `BLC_SCALE_Gg`:** the registers are not
kept, not even as an alias.
- A v1.07 profile is migrated by writing the old scale to the four
  per-channel scales. Equal scales give bit-identical output; this was
  verified on every vector, sample and firmware fixture of the tree.
- `tools/migrate_profile_upd.py` performs this rewrite on `.csrw` files.
- The SDK converter rejects the old name as an unknown CSR. A profile that
  was not migrated therefore fails loudly instead of silently running
  with scale 1.0.
- A binary profile (`profile.bin`) written against v1.07 would still
  write offset 0x0258..0x0264, which is now unmapped, so the scale stays
  at its reset value. Regenerate such binaries from migrated `.csrw`.

### 2.3 CCM (ISP-UPD-007)

```text
dot[c] = sum_k m[c][k] * RGB_in[k]           (m: signed Q3.9)
acc[c] = dot[c] + offset[c] * 512             (offset: signed 12-bit pixel levels)
out[c] = sat12(acc[c] >> 9)                   (floor, single saturation)
```

- The C++ model multiplies the offset by 512 in 64-bit arithmetic, so a
  negative value is never left-shifted.
- The matrix format, the floor and the saturation are unchanged. The
  offset is not scaled by the matrix.
- `FX1_ISP_CCM_OFFSET_PIXEL_UNITS` (1) marks the new units in the header.
  `fx1_isp_ccm_set` keeps its signature; its `ofs[3]` are now pixel levels.
- Default `(0, 0, 0)` per owner: zero offsets give the same output as
  before.
- A nonzero v1.07 code changes meaning: `0x200` used to add one level and
  now adds 512. `migrate_profile_upd.py` reports every nonzero offset and
  keeps the code; `--strict` makes it an error.
  - It decodes the CCM lines with `tools/csrw_codec.py`, in every profile
    syntax: `CCM_OFS_R 0x200`, `CCM_OFS_R_offset_r 0x200` and
    `CCM_OFS_R offset_r=0x200`.
  - A line it cannot read stops it with `file:line`, exit 2, and leaves the
    file unchanged.
  - A UTF-8 byte order mark and CRLF line ends are kept.
- In this tree, three test vectors use `(5, −3, 0)`: `full_bggr_66x34`,
  `input_raw10_full_bggr_66x34` and `input_raw12_full_bggr_66x34`. They
  keep the codes with the new meaning, and their expected outputs were
  regenerated (decisions §13).

## 3. Not changed

- `COMMON_VER_ID`, every other register, the AWB/AEC reset values and maps
  (ISP-UPD-003/004 change firmware/profile defaults only).
- The LSC mesh capacity, the LSC load protocol and the number of LSC
  residents.
- The TUNE algorithm banks (two).
