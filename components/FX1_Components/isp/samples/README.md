# FX1 ISP samples

Installed under `<prefix>/share/fx1_isp/samples`.

| File | Use |
|---|---|
| `profiles/outdoor1_bggr_2688x1520_<preset>.csrw` | The four test profiles of DEC-34 (`tools/make_profile.py --preset`), computed from one RAW frame of the project dataset: BGGR 2688×1520, grey-world white balance. They are test profiles, not calibrations. Each file is a list of `<REGISTER> <value>` writes, applied in order before `ISP_EN` (programming guide §2). Regenerated for CSR/MAS v1.07 (V107-D102): the Gamma and EE legacy confirmations are part of the list. The frame is loaded as `run_regression.py` loads it (`raw_fixture.load_source`, format `le16_raw10_msb`), then `make_profile.profile_writes`: the `make_profile.py` command line reads a right-aligned container, which the dataset file is not. |
| `profiles/shared_bggr_2688x1520_full.csrw` | The SDK sample profile (`platforms/fx1_soc/package`): the `full` preset with fixed tuned BLC, WB, CCM, EE_ALPHA and GTM values, the same for every input. |

Consolidated update (2026-11-09, ISP-UPD-001/003/004/007), applied to the
`full` and `full_vga_709` outdoor1 profiles (regenerated with
`make_profile.py`) and to the shared profile (a transform of the file):
- LSC mesh 17×13 × 4 channels (884 coefficients) of the same radial formula.
  It was 17×9 before.
- The mesh is loaded into residents 1 and 2, and 1 is selected. The firmware
  fills resident 0 after the first SOF.
- CCM offsets 0 are written explicitly.

For the `full` preset and the shared profile only:
- AWB and AEC grids 16×16; on 2686×1518 the AEC zones are 168×95.
- AEC TH_UE 272, TH_OE 3744.

The `basic` and `gtm_manual_qhd` profiles regenerate bit-identically.

The sample profiles use the field syntax of ISP-UPD-008 (programming guide
§1.1): `BLC_GAIN_SEL gain_level=0 range_scale_en=1`, `GAMMA_CTRL en=0
lut_load_confirm=1`, and so on. Every write names its fields, also on a
one-field register (`CSC_CTRL std=0`, `WB_GAIN_R gain_r=0x1F3`). Only the
table/mesh data ports (`LSC_COEF_DATA`, `GAMMA_LUT_DATA`, `EE_LUT_WDATA`,
`GTM_LUT_DATA`) keep `REGISTER VALUE`, one line per entry. The text was converted with `csrw_codec.py
emit`, and every file compiles to the same write stream, write for write, as
its v1.07-syntax predecessor. `tools/csrw_codec.py explain FILE` decodes each
write.

TUNE block (ISP-UPD-009, 2026-11-10, tuning team request): every sample
profile ends with the TUNE extension controls the tuning team adjusts:
- `TUNE_DEMOSAIC_MARGIN` and `TUNE_CNF_BLEND`;
- the CNF controls, where the preset has none of its own (`basic`,
  `gtm_manual_qhd`);
- `TUNE_CSC_*`, `TUNE_NR_*` and `TUNE_BPC_*`;
- the empty factory bad-pixel list (`TUNE_BPC_COUNT VALUE=0`, then
  `BPC_CONFIRM`).

The values are the CSR reset values, which are the Tuning v1.07 startup
values, so the outputs equal those of the profiles without the block. The
NR variance curve (MODE 2 only) is a commented template of 17 explicit-index
writes and `NR_CONFIRM`.

The firmware's profile-mode bring-up and `fx1_isp_run_raw --profile-mode`
write these values into the bank after INIT. In legacy mode they stay bank
storage and the datapath keeps their reset. Use `make_profile.py
--no-tune` to leave the block out.
| `4k/<case>/` | The two 4K acceptance cases (DEC-37): profile, frame count, expected NV12 SHA-256 and statistics readout script. |
| `run_4k_check.sh` | Runs both 4K cases through the installed `fx1_isp_run_raw`, compares each against the reference values, and prints PASS or FAIL. It needs neither Python nor the dataset. |

To run a sample profile on a RAW frame:

```sh
<prefix>/share/fx1_isp/tools/raw_fixture.py convert --in frame.raw --format le16_raw10_msb \
    --width 2688 --height 1520 --bayer BGGR --out frame.isp16
<prefix>/bin/fx1_isp_run_raw --input frame.isp16 --width 2688 --height 1520 \
    --profile <prefix>/share/fx1_isp/samples/profiles/outdoor1_bggr_2688x1520_full.csrw --frames 2 --out out/frame
```
