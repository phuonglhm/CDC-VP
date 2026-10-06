# FX1 ISP samples

Installed under `<prefix>/share/fx1_isp/samples`.

| File | Use |
|---|---|
| `profiles/outdoor1_bggr_2688x1520_<preset>.csrw` | The four test profiles of DEC-34 (`tools/make_profile.py --preset`), computed from one RAW frame of the project dataset: BGGR 2688×1520, grey-world white balance. They are test profiles, not calibrations. Each file is a list of `<REGISTER> <value>` writes, applied in order before `ISP_EN` (programming guide §2). |
| `4k/<case>/` | The two 4K acceptance cases (DEC-37): profile, frame count, expected NV12 SHA-256 and statistics readout script. |
| `run_4k_check.sh` | Runs both 4K cases through the installed `fx1_isp_run_raw`, compares each against the reference values, and prints PASS or FAIL. It needs neither Python nor the dataset. |

To run a sample profile on a RAW frame:

```sh
<prefix>/share/fx1_isp/tools/raw_fixture.py convert --in frame.raw --format le16_raw10_msb \
    --width 2688 --height 1520 --bayer BGGR --out frame.isp16
<prefix>/bin/fx1_isp_run_raw --input frame.isp16 --width 2688 --height 1520 \
    --profile <prefix>/share/fx1_isp/samples/profiles/outdoor1_bggr_2688x1520_full.csrw --frames 2 --out out/frame
```
