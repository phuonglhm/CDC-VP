# ISP RAW-frame demo on the FX1 platform

This demo runs one real RAW frame through the whole FX1 SoC VP, the way
firmware will drive it on the chip:

- **CPU:** hart 0 runs `fw/fx1_soc/demo/isp_raw_frame` with the reference
  C99 driver. It initialises the ISP, sets the geometry, applies the register
  profile, starts the stream, queues the frame, takes the ISP interrupt through
  the PLIC, and reads the statistics.
- **ISP:** its IDMA reads the RAW from DDR and its ODMA writes NV12 back, over
  the FX1 bus.
- **Host:** the script loads the inputs with `fx1_soc --load` and reads the
  result with `--dump`. It then runs the same input and profile through the
  stand-alone ISP model (`fx1_isp_run_raw`) and compares the two NV12 outputs
  byte for byte (SHA-256).

## Run

From the CDC-VP root, after building `fx1_all` (see `../README.md`):

```bash
export CC=/usr/bin/gcc CXX=/usr/bin/g++ PATH=/usr/bin:/bin:$PATH
platforms/fx1_soc/demo/run_isp_demo.py \
    --raw "<dataset>/outdoor_hightlight_raw/khoid_outdoor1_raw_2688x1520_5376.raw" \
    --build-dir build-fx1 --out /tmp/isp_demo
```

Expected output (release build, about 4 s of host time):

```text
output         1920x1080 NV12, stride 2688
ISP counters   IDMA 1, ODMA 1 frames; DMA_ERR 0x2
FRAME_ID       AEC 1, AWB 1, AF 1
simulated time 8.214 ms first queue to last completion
platform NV12  /tmp/isp_demo/platform.nv12  sha256 871f0f88bb69dcd4...
reference NV12 sha256 871f0f88bb69dcd4...
result         MATCH
preview        /tmp/isp_demo/platform.png
```

`DMA_ERR 0x2` is `IDMA_UNDERRUN`: a lossless level that means "no next input
queued", expected after the last frame.

Open `platform.png` with `code`, `eog` or `xdg-open`.

## Run it from the FreeRTOS CLI

Job descriptor version 2 (V107-10): `--mode profile` (default) runs the
ISP in CSR v1.07 profile mode (the profile is applied, then built into
bank 0, sealed and applied; a refused profile ends the run with status 2
and its TUNE_ERROR bits, without a legacy fallback); `--mode legacy` runs
the legacy path for comparison. Either output is compared with the
stand-alone model's legacy run of the same profile. `--outputs` selects
the channels (bit 0 CH0, required; bit 1 CH1 NV12; bit 2 CH2 RGB888); CH1
and CH2 are dumped (`y1.bin`, `uv1.bin`, `rgb2.bin`), packed
(`platform_ch1.nv12`, `platform_ch2.rgb`) and reported with their SHA-256.
A version-1 job (64 bytes) still runs, in legacy mode, CH0 only.

The same job can be processed by `isp run` in the FreeRTOS CLI
(`fw/fx1_soc/freertos/`), typed by hand. One command does it all:

```bash
platforms/fx1_soc/demo/run_isp_demo.py --raw "<dataset>/.../khoid_outdoor1_raw_2688x1520_5376.raw" \
    --build-dir build-fx1 --out /tmp/isp_job --interactive
```

The script:

1. prepares the job;
2. starts `freertos_cli.elf` in the background with the job loaded (simulator
   output in `platform.log`);
3. opens the console in the same terminal. Type `isp run`, `isp stats`, then
   `poweroff`;
4. after `poweroff`, packs, previews and compares, like the one-shot run.

If the console is closed with Ctrl-] before `poweroff`, the platform is
stopped and nothing is packed. If port 5000 is in use, use `--port N`.

### Step by step

Use this when the VP and the console must run separately, for example on
another machine or in another terminal layout. The script then works in two
halves around the session.

1. **Prepare the job.** This converts the RAW, writes `profile.bin` and
   `job.bin`, and prints the `fx1_soc` command line:

   ```bash
   platforms/fx1_soc/demo/run_isp_demo.py --raw "<dataset>/.../khoid_outdoor1_raw_2688x1520_5376.raw" \
       --build-dir build-fx1 --out /tmp/isp_job --prepare-only
   ```

   The printed command starts `freertos_cli.elf` with
   `--uart0-socket 5000 --uart0-wait --timeout-ms 0 --realtime`. It also
   carries three `--load` options (RAW, job, profile) and three `--dump`
   options (result block, Y, UV).
2. **Run the printed command** in terminal 1.
3. **Connect from terminal 2.** Run `platforms/fx1_soc/tools/fx1_console.sh
   5000`, then type:

   ```text
   root@fx1:~# isp run
   isp run: 2688x1520 RAW -> 1920x1080 NV12 at Y 0x88000000 UV 0x88400000, 1 frame(s), profile mode (ID 1), outputs 0x00000001, DMA_ERR 0x00000002
     isp run: PASS
   root@fx1:~# isp stats
   AEC FRAME_ID 1, count R/Gr/Gb/B 1019337/1019337/1019337/1019337
   AWB FRAME_ID 1, context 0x000000a2
   AF  FRAME_ID 1, context 0x000000a3
   root@fx1:~# poweroff
   ```

   The `--dump` files are written when the simulation ends, so `poweroff` is
   required. A Ctrl-C on the simulator writes nothing.
4. **Pack and compare.** This produces `platform.nv12` and `platform.png`,
   and compares them with the stand-alone model, like the one-shot run:

   ```bash
   platforms/fx1_soc/demo/run_isp_demo.py --build-dir build-fx1 --out /tmp/isp_job --pack-only
   ```

   Geometry and frame count come from the `job.bin` of step 1. `--profile`
   must be the same file; a different one is rejected. The prepare step
   prints the exact `--pack-only` line.

   The pack step checks the inputs before it writes anything:

   - the result block's size, magic and status, and that every frame
     completed;
   - an even output size within the input, and the job's stride;
   - the exact dump lengths;
   - the preview's exit status.

   On any failure it exits 2 and leaves no `platform.nv12`, `platform.png`
   or `summary.txt`, also with `--no-reference`. `demo/tests/test_pack.py`
   (`fx1_regression_isp_demo_pack`) covers these cases.

The FreeRTOS run gives the same NV12 SHA-256 as the bare-metal demo.

## Options

| Option | Default | Meaning |
|---|---|---|
| `--raw FILE` | required (except `--pack-only`) | RAW frame |
| `--format` | `le16_raw10_msb` | Source format (`isp/tools/raw_fixture.py`) |
| `--width/--height/--bayer` | 2688 / 1520 / BGGR | Frame geometry and CFA order |
| `--src-stride` | 2 × width | Stride of the RAW file |
| `--profile FILE.csrw` | `isp/samples/profiles/outdoor1_bggr_2688x1520_full.csrw` | ISP register profile, written in order |
| `--frames N` | 1 | Process the frame N times (temporal blocks such as GTM, 2DNR) |
| `--build-dir` | `build-fx1` | Where `fx1_all` was built |
| `--out DIR` | required | Output directory |
| `--no-reference` | off | Skip the stand-alone comparison |
| `--interactive` | off | Prepare, run the FreeRTOS CLI with the console in this terminal, pack after `poweroff` |
| `--port N` | 5000 | Console TCP port (`--interactive`, `--prepare-only`) |
| `--prepare-only` | off | Steps 1–2 only; print the FreeRTOS CLI command line (see above) |
| `--pack-only` | off | Steps 4–5 only, from the dumps in `--out`; `--raw` is not needed |

Other test profiles of the same frame are in `isp/samples/profiles/`: `basic`,
`full_vga_709` (640×360 output, BT.709) and `gtm_manual_qhd`. For a different
frame, generate a test profile with `isp/tools/make_profile.py`. These are
test profiles, not sensor calibrations.

Exit status: 0 the run passed and matched (or `--no-reference`), 1 NV12
mismatch, 2 a step failed, 3 usage error.

## What lands in `--out`

| File | Content |
|---|---|
| `input.isp16` | RAW in the ISP 16-bit container (stride rounded up to 16 bytes) |
| `profile.bin`, `job.bin` | What the host loaded into DDR (format: `fw/fx1_soc/demo/isp_raw_frame/isp_demo_job.h`) |
| `uart.log`, `platform.log` | Firmware console (one-shot run) and simulator output |
| `result.bin` | Result block written by the firmware |
| `y.bin`, `uv.bin` | Raw dumps of the output buffers (stride included) |
| `platform.nv12`, `platform.png` | Active NV12 bytes (Y then UV) and a preview |
| `reference/frame.nv12`, `reference/frame.txt` | The stand-alone model's output |
| `summary.txt` | The lines printed at the end |

## DDR layout (VP placeholders, `fx1_memory_map.h`)

| Address | Content |
|---|---|
| `FX1_FW_SHARED_BASE` + 0x0000 | Job descriptor (host → firmware) |
| `FX1_FW_SHARED_BASE` + 0x0100 | Result block (firmware → host) |
| `FX1_FW_SHARED_BASE` + 0x1000 | Profile: `{u32 offset, u32 value}` pairs |
| `FX1_FW_RAW_BUF_BASE` | RAW input |
| `FX1_FW_YUV_BUF_BASE` | Y; UV at the next 1 MiB boundary after Y |

## As a test

The demo is not part of the CI gate, because it needs a RAW frame that is not
in the repository. To run it as an optional test (label `fx1_demo`), configure
with `-DFX1_DEMO_RAW=<path to the outdoor1 RAW>`, then:

```bash
ctest --test-dir build-fx1 -L fx1_demo
```

## Limits

- The output is checked against the project's ISP model, not against silicon.
- The profile is a test profile computed from this frame.
- Simulated time is functional, not silicon timing.
