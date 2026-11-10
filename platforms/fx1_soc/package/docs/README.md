# VP_FX1_V3.0 — FX1 SoC virtual platform with FreeRTOS CLI

This delivery gives the firmware and software teams a virtual FX1 SoC to run
and debug firmware on before silicon. It contains:

- the VP binary `fx1_soc`;
- a FreeRTOS V11.2.0 command-line firmware with its full source and a
  stand-alone Makefile;
- ISP sample frames, which a RAW image goes through on the virtual chip.

Treat the VP as the chip: build against `include/fx1/fx1_memory_map.h` and run
the firmware ELF on `fx1_soc`.

## Read first: V3.0 is not a superset of V2.0

V3.0 is built from a different platform model than `VP_FX1_V2.0`. Firmware for
one does not run on the other.

| | VP_FX1_V2.0 (`vp_fx1_full_soc`) | **VP_FX1_V3.0 (`fx1_soc`)** |
|---|---|---|
| CPU | 1 hart, Bremen riscv-vp, `rv32imac` / `ilp32` | **2 harts, RISC-V VP++, `rv32imafc` / `ilp32f`** (hardware FP) |
| Interconnect | generic bus | FX1 bus model with an exclusive monitor (LR/SC and AMOs are correct against DMA) |
| Peripherals | about 25 IPs (UART×2, I2C, SPI, timers, WDT, PWM, GPIO, …) | **UART, CLINT, PLIC, SYS_DMA, ISP, DDR3 controller, I2C0, Ethernet MAC + PHY** ([eth/README.md](eth/README.md)), and with `--camera` the **CSI-2 RX camera path** ([camera/README.md](camera/README.md)) |
| Accelerators | NPU (functional) | **ISP** (functional, bit-exact to the stand-alone ISP model); no NPU |
| Memory map | V2.0 map | FX1 placeholder map ([MEMORY_MAP.md](MEMORY_MAP.md)), pending the HAS |

What is not modelled in V3.0:

- NPU and H.264/H.265: their addresses and IRQ IDs are reserved only
  (MIPI CSI-2 RX is modelled with `--camera`, see
  [camera/README.md](camera/README.md));
- a host network: the Ethernet MAC is modelled, but its line ends in a
  loopback plug, an echo station or no cable (no TAP, no TCP/IP stack);
- SPI/QSPI, timers, WDT, PWM, GPIO, RTC, ADC, TRNG, OTP (I2C0 is modelled);
- PMU, CMU and clock/reset control;
- caches and an MMU.

## What changed in this V3.0 delivery: DDR3 controller

DDR is now a model of the FTEL DDR3 controller (DDR3-1600, 2 ranks, ECC), no
longer a plain RAM. Every CPU, SYS_DMA and ISP access to the DDR goes through it.

- **Existing firmware runs unchanged.** By default the VP initializes the
  controller, runs its ECC startup scrub and loads the ELF before the harts
  start, so DDR is ready at the first instruction. `--timeout-ms` and the
  console replay delay count from that moment.
- **New in the map** ([MEMORY_MAP.md](MEMORY_MAP.md)): the DDR3 controller CSR
  at `0x1200_0000` (`include/fx1_ddr3/fx1_ddr3_regs.h`), DDR_SYSCTRL at
  `0x1201_0000` (`include/fx1/fx1_ddr_sysctrl.h`, VP only: readiness, power
  requests, QoS, resets), boot SRAM at `0x0100_0000`, and PLIC source 8 for
  DDR3 interrupts (masked at reset).
- **Timing.** The harts fetch every instruction from DDR (the VP models no
  cache), so code runs about 2× slower in simulated time, and slower still
  while the ISP streams. Do not calibrate CPU-versus-DMA timing on the VP.
  Scripted console input should be paced at the real UART rate: `run_vp.sh`
  uses `--uart0-rx-delay-us 60000 --uart0-rx-byte-us 87`.
- **Field encodings** of the DDR3 registers that the specification leaves
  open are provisional (`include/fx1_ddr3/provisional_encodings.h`): use the
  macros, not the numbers.
- **Writing a DDR3 driver or boot code:** start with [ddr3/README.md](ddr3/README.md).
  It covers the two boot modes (`--ddr-boot preinit`, the default, and
  `--ddr-boot firmware`, where firmware runs from BootROM/SRAM and initializes
  the DDR3 itself), DDR_SYSCTRL, fault injection and the limits; register-level
  details are in [ddr3/programming_guide.md](ddr3/programming_guide.md). Reference bring-up code and BootROM examples are in
  `sw/bootloader/sources/VP_FX1_V3.0/ddr3/`: build them with
  `make -C sw/bootloader/sources/VP_FX1_V3.0 ddr3` and run them with
  `sw/bootloader/test/VP_FX1_V3.0/run_vp.sh ddr3`.

## Ethernet MAC

The FTEL Ethernet MAC (CSR `0x1103_0000`, PLIC source 6) with its descriptor
DMA and an RMII PHY on MDIO is part of the platform; `--eth-peer
loopback|partner|none` chooses what is on the line (default loopback). The
firmware ships the portable driver, the FX1 manager (`drivers/eth/fx1_eth.h`:
lifecycle, interrupt split, DMA error recovery), the CLI's `eth` commands and
a bare-metal example (`make eth`, `run_vp.sh eth`). Start with
[eth/README.md](eth/README.md): API, ownership rules, error handling, options
and limits.

## Quick start

Run everything from the SDK repository root.

```bash
# 1. Build the firmware (xPack riscv-none-elf-gcc 15.2 on PATH, or TOOLDIR=<dir>/bin)
make -C sw/bootloader/sources/VP_FX1_V3.0 -j"$(nproc)"

# 2. Deterministic self-check: scripted CLI session, exit status 0 = PASS
sw/bootloader/test/VP_FX1_V3.0/run_vp.sh smoke

# 3. A RAW camera frame through the ISP, driven by the firmware
sw/bootloader/test/VP_FX1_V3.0/run_vp.sh isp            # D65 ColorChecker sample
sw/bootloader/test/VP_FX1_V3.0/run_vp.sh isp outdoor1   # second sample (internal use only)
sw/bootloader/test/VP_FX1_V3.0/run_vp.sh isp "/path/to/my_frame.raw"   # your own frame (see "Your own RAW frame")
sw/bootloader/test/VP_FX1_V3.0/run_vp.sh isp d65 --dump-demosaic        # also the image after Demosaic (VP debug)

# 4. Ethernet: scripted CLI session, and the bare-metal example on each line peer
sw/bootloader/test/VP_FX1_V3.0/run_vp.sh eth-cli
make -C sw/bootloader/sources/VP_FX1_V3.0 eth && sw/bootloader/test/VP_FX1_V3.0/run_vp.sh eth
```

`run_vp.sh smoke` passes when hart 1's boot IP check reports 0 failures and
every check line says PASS. The checks are the FPU context switch, SYS_DMA,
the ISP fixture and `selftest`; then `poweroff` ends the run with PASS.

`run_vp.sh isp` loads a prepared job (RAW frame, ISP register profile, job
descriptor) into DDR. It types `isp run` into the CLI. When the firmware
powers off, it packs the NV12 output, writes a PNG preview and compares the
SHA-256 with the expected output. The expected value comes from the
stand-alone ISP model run on the same input and profile. Results go to
`sw/bootloader/build/VP_FX1_V3.0/isp/<sample>/` (`platform.png`,
`platform.nv12`, `summary.txt`).

`summary.txt` also gives the AEC under- and over-exposure of the job's last
AEC result:

    AEC UE/OE      UE 49.98 %, OE 1.44 % of the active image (AEC FRAME_ID 2)

These are 100 × UE_COUNT (OE_COUNT) / the number of Bayer samples of the
active image, where UE means below TH_UE = 272 and OE means above
TH_OE = 3744. The firmware computes the values (`fx1_isp_read_aec_percent`)
and returns them in the result block. The sample profiles use a 16 × 16 AWB
grid and a 16 × 16 AEC zone grid.

`platform.png` is decoded with the colorimetry of the output that the
firmware returns in the result block: the CSC standard (BT.601 or BT.709) and
the range (full 0..255, or limited 16..235/16..240). The summary prints it
on a `colorimetry` line. When the result lacks this information (an older
firmware) or holds an invalid value, the range is never guessed:
- `platform.nv12` is still written (and compared with the golden);
- no `platform.png` is written;
- `isp_pack.py` exits 3 and `run_vp.sh isp` reports `ISP FAIL: no preview`.

To get the preview in that case, give `--std` and `--range` to
`isp_pack.py`. NV12 files are never converted. The decoding is exact
(integer arithmetic, an exact .5 rounds up): it gives the same pixels as the
component's `nv12_preview.py`.

LSC uses three resident tables with one 17 × 13 mesh. The profile loads
residents 1 and 2. After the first frame, the firmware copies the mesh into
resident 0. The console prints the resulting valid mask of the LSC residents, which should be 7.

### Interactive console

```bash
# Terminal 1: the VP waits for the console, then boots
sw/bootloader/test/VP_FX1_V3.0/run_vp.sh interactive 5000
# Terminal 2: raw-mode client (socat if installed, else bash's /dev/tcp: nothing to install)
sw/bootloader/test/VP_FX1_V3.0/fx1_console.sh 5000
```

Type `help`, then `poweroff` to end the simulation. The VP runs paced to wall
time (`--realtime`) with no time limit, so an idle console neither races ahead
nor disconnects. To process a sample frame by hand in one terminal:

```bash
sw/bootloader/test/VP_FX1_V3.0/run_vp.sh isp d65 interactive 5000
# then type: isp run, isp stats, poweroff
```

Use `fx1_console.sh` rather than plain `nc` or `telnet`. The firmware echoes
like a real UART console and ends lines with CR LF. A terminal in line mode
echoes locally as well, so every command would appear twice.

## Prerequisites

| Need | For |
|---|---|
| Linux x86-64 (AlmaLinux/RHEL 9 validated) | the VP; `libsystemc` ships beside it (`$ORIGIN`), no SystemC install needed |
| `riscv-none-elf-gcc` 15.2 (xPack), GNU Make | building the firmware |
| `python3` (standard library only) | `run_vp.sh isp` (packing NV12, PNG preview) |
| nothing extra; `socat` optional | the interactive console (`fx1_console.sh` falls back to bash's `/dev/tcp`) |

On Windows or macOS, use the AlmaLinux 9 container described in the SDK
README. The VP needs nothing installed in it: `python3` is already in the
image, and the console needs neither `socat` nor `nc`.

**The console listens on 127.0.0.1 inside the container only.** A published
port (`-p 5000:5000`) does not reach it: the connection from the host is
reset. Run the console client in the same container instead:

```bash
# start the container with a name (from the SDK root, as in the SDK README)
docker run --rm -it --platform linux/amd64 --name fx1-vp -v "$PWD:/workspace" -w /workspace almalinux:9 bash

# inside it: the VP waits for the console
sw/bootloader/test/VP_FX1_V3.0/run_vp.sh interactive 5000

# second terminal on the host: the client, in the same container
docker exec -it fx1-vp /workspace/sw/bootloader/test/VP_FX1_V3.0/fx1_console.sh 5000
```

The ISP flow needs only the container's own terminal:
`run_vp.sh isp d65 interactive 5000` starts the VP and attaches the console
in that one terminal.

## Layout

```text
vp/bin/VP_FX1_V3.0/                 fx1_soc + libsystemc.so.2.3
vp/doc/VP_FX1_V3.0/                 this README, MEMORY_MAP.md, BUILD_MANIFEST.json, SHA256SUMS, licenses/
sw/bootloader/sources/VP_FX1_V3.0/  FreeRTOS CLI firmware: app/, bsp/, include/, drivers/, kernel subset, Makefile
sw/bootloader/build/VP_FX1_V3.0/    make outputs (freertos_cli.elf/.dis/.map); not tracked
sw/bootloader/test/VP_FX1_V3.0/     run_vp.sh, fx1_console.sh, isp_pack.py, csrw_to_bin.sh, cli_*.txt, isp/<sample>/, profiles/<id>/
```

The firmware sources, their structure and how to add a command are described
in `sw/bootloader/sources/VP_FX1_V3.0/README.md`.

## The firmware

| | |
|---|---|
| Kernel | FreeRTOS V11.2.0, GCC RISC-V port, `heap_4` with a 256 KiB heap, 1 kHz tick from CLINT `mtime` (20 MHz) |
| Harts | AMP. FreeRTOS runs on hart 0. Hart 1 runs a bare-metal IP check at boot, then parks in WFI |
| FP | the FX1 port extension saves f0–f31 and `fcsr` in every task context (`ilp32f`) |
| Console | UART, RX by interrupt (PLIC source 1) into a queue, TX by polling, CR LF line endings |

**Boot sequence:**

1. Hart 0 prints the banner and releases hart 1.
2. Hart 1 checks every IP, touching nothing that hart 0 uses:
   - BootROM is read-only;
   - a DDR window passes pattern and byte-merge tests;
   - an unmapped access faults;
   - for CLINT, PLIC, UART, SYS_DMA, the ISP and the Ethernet MAC: ID/reset
     values, and read/write registers that are restored afterwards (for the
     MAC also the PHY ID over MDIO).
3. Any failure halts the boot (`boot halted: hwcheck FAIL`, exit code 1,
   FAIL 0x50).
4. Otherwise hart 0 starts the scheduler and the CLI.

| Command | Does |
|---|---|
| `help`, `version` | command list; firmware identity, ISP `COMMON_VER_ID`, SYS_DMA capability |
| `uptime`, `tasks`, `heap` | tick time and `mtime`; tasks with state, priority and stack headroom; heap use |
| `hwcheck` | reprint hart 1's boot report |
| `md ADDR [WORDS]`, `mw ADDR VALUE` | 32-bit reads/writes in the mapped regions. An address the device rejects prints `bus error` instead of trapping |
| `regdump isp\|dma\|plic\|clint\|uart\|eth` | key registers of one IP |
| `fpu test` | two tasks interleave FP work; results must equal the switch-free value bit for bit |
| `dma test [BYTES]` | SYS_DMA memory-to-memory copy with completion IRQ, data verified |
| `isp selftest` | 8 synthetic 64×48 frames through the ISP driver; every NV12 CRC must match the reference |
| `isp run auto` | one AWB probe, then at least two render frames with FW-selected WB/tone (demo policy); legacy mode |
| `isp run` | process the RAW job loaded into DDR (`run_vp.sh isp`), result block for the host; profile mode by default (job version 2), legacy for a version-1 job |
| `isp run legacy` | the same job in legacy mode, for comparison |
| `isp stats` | AEC/AWB/AF statistics of the last frame |
| `eth probe\|start\|stop\|send\|stats\|status\|recover\|selftest\|…` | the Ethernet MAC through the FX1 manager; full list in [eth/README.md](eth/README.md) |
| `selftest` | hwcheck report + `fpu test` + `dma test` + `isp selftest` |
| `poweroff` | end the simulation: PASS if no check failed so far, otherwise FAIL `0x400 + failures` |

## Running your own image

```bash
FX1_FW=path/to/app.elf sw/bootloader/test/VP_FX1_V3.0/run_vp.sh run --timeout-ms 5000
vp/bin/VP_FX1_V3.0/fx1_soc --fw app.elf --timeout-ms 5000 --uart-log uart.log
```

The image is one ELF linked at `0x8000_0000`; both harts start at its entry.
The firmware ends a run by writing a verdict to the VP-only sim-control
device: `fx1_pass()` or `fx1_fail(code)` in the BSP.

**`fx1_soc` options:**

| Option | Meaning |
|---|---|
| `--fw ELF` | image to load into DDR (required) |
| `--timeout-ms N` | simulated-time budget from CPU start, default 1000; `0` = no limit. The platform first initializes and scrubs the DDR3 (about 0.67 s of simulated time, a few ms of wall time) |
| `--realtime` | never let simulated time run ahead of wall time (interactive use) |
| `--uart-log FILE`, `--quiet-uart` | copy the console to FILE; do not print it |
| `--uart0-socket PORT [--uart0-wait]` | console over TCP on 127.0.0.1:PORT; optionally wait for the client before starting |
| `--uart0-rx-file FILE [--uart0-rx-delay-us N] [--uart0-rx-byte-us N]` | type FILE into the UART N µs after CPU start, one byte per `--uart0-rx-byte-us` (use 60000 and 87 for the CLI, as `test/run_vp.sh` does) |
| `--load ADDR:FILE`, `--dump ADDR:LEN:FILE` | copy host data into DDR before the CPUs start; write DDR to a file after the run (at `poweroff`) |
| `--quantum-ns N` | TLM quantum, a multiple of 10 ns (default 1000) |
| `--eth-peer loopback\|partner\|none` | what is on the Ethernet line: frames come back, an echo station, or link down (default loopback) |
| `--eth-wb-delay-us N` | Ethernet fault injection: DES0 write-backs held N µs after DONE ([eth/README.md](eth/README.md)) |
| `--trace` | bus trace |
| `--version` | build identity: package, CDC-VP revision, build type, VP++ pin |

**Exit status:**

| Status | Meaning |
|---|---|
| 0 | PASS |
| 1 | FAIL (the firmware reported a code) |
| 2 | timeout without a verdict |
| 3 | simulator or integration error |
| 4 | usage error |

## ISP samples

| Sample | Frame | Status |
|---|---|---|
| `d65` (default) | `D65_raw_2688x1520_5376.raw`, a ColorChecker under D65, 2688×1520 BGGR 10-bit | Apache-2.0 project asset, cleared for distribution |
| `outdoor1` | `khoid_outdoor1_raw_2688x1520_5376.raw`, outdoor scene, same format | **Internal use only**: not cleared for public release. Do not redistribute outside the project |

**Common to both samples:**

- **Profile:** both samples select `shared_bggr_2688x1520_full` through
  `PROFILE_ID` in `job.env`. Its CSR list and binary are stored once in
  `sw/bootloader/test/VP_FX1_V3.0/profiles/shared_bggr_2688x1520_full/`.
  The shared v2 tuning uses BLC pedestal `0x100`, fixed
  `WB_GAIN_R=0x1F3`, `WB_GAIN_G=0x100`, `WB_GAIN_B=0x1A6`, a gentler CCM,
  GTM auto (`KEY=0xA0`, `LWHITE=0x240`) and `EE_ALPHA=0x8000`.
  It is not a sensor calibration. Reading AWB statistics does not automatically
  update the WB gains; firmware needs an AWB algorithm for that.
  Changing a profile requires regenerating the golden for each affected sample.
  After editing a `profile.csrw`, rebuild its `profile.bin` with
  `sw/bootloader/test/VP_FX1_V3.0/csrw_to_bin.sh PATH/profile.csrw`. The
  sample's `job.bin` must declare the same number of CSR writes; the script
  warns when the number changes, but does not update `job.bin`.
- **Profile syntax:** each line is one full 32-bit CSR write, in order. A line
  can name the whole register (`GAMMA_CTRL 0x100`), one field
  (`GAMMA_CTRL_en 1`, every other field 0), or several fields of one
  register (`AEC_CTRL en=1 commit=1`, the others 0). Field values are the
  field's code before the shift. Two lines are two writes: `AEC_CTRL_en 1`
  followed by `AEC_CTRL_commit 1` clears `en` again. Errors stop the
  conversion with file:line, and the old `profile.bin` is kept.
  `python3 csrw_codec.py explain profile.csrw` (in the same directory) lists
  every write with its fields. The sample profiles are written this way.
  Every write names its fields, even on a one-field register
  (`CSC_CTRL std=0`, `WB_GAIN_R gain_r=0x1F3`). The exception is the
  table/mesh data, which keeps one `REGISTER VALUE` line per entry
  (`LSC_COEF_DATA`, `GAMMA_LUT_DATA`, `EE_LUT_WDATA`, `GTM_LUT_DATA`).
- **Frames:** each job processes the same RAW twice from reset. The first frame
  warms up GTM auto; the delivered image and golden are from the second frame.
  When preparing a new sample, use `run_isp_demo.py --prepare-only --frames 2`
  with this shared profile, and generate its standalone golden with `--frames 2`.
- **Output:** 1920×1080 NV12.
- **Files:** each sample's `SAMPLE.txt` records the RAW SHA-256, the profile,
  the expected NV12 SHA-256 and the expected post-Demosaic capture SHA-256
  (`expected_demosaic.sha256`; `expected_auto_demosaic.sha256` for `auto`).

**After a run:**

- **`DMA_ERR 0x2`** (`IDMA_UNDERRUN`) is expected: no next frame was queued.
- **The preview** uses the same inverse transform as the ISP team's tool. It
  is a visual check; the pass/fail criterion is the SHA-256.

### The image after Demosaic (VP debug, `--dump-demosaic`)

```bash
sw/bootloader/test/VP_FX1_V3.0/run_vp.sh isp d65 --dump-demosaic
sw/bootloader/test/VP_FX1_V3.0/run_vp.sh isp outdoor1 --dump-demosaic
sw/bootloader/test/VP_FX1_V3.0/run_vp.sh isp d65 auto --dump-demosaic
sw/bootloader/test/VP_FX1_V3.0/run_vp.sh isp "/path/to/my_frame.raw" --dump-demosaic
```

The option may come anywhere after `isp`. Besides the NV12, the run writes
the ISP image at the Demosaic output, **before CCM** (BLC, LSC, BPC, WB, DG and
Demosaic applied; CCM, Gamma, CSC, GTM, 2DNR, EE, CNF not), of the **same
frame** as `platform.png`: the job's final frame (for `auto`, the last render
frame, with the WB the firmware applied, not the probe). Files in
`sw/bootloader/build/VP_FX1_V3.0/isp/<sample>/`:

| File | Content |
|---|---|
| `demosaic.rgb16` | R, G, B interleaved, unsigned 16-bit little-endian per component, **12 significant bits [11:0]** (values 0..4095, not scaled to 16 bits), no header, no row padding: width × height × 6 bytes. For the 2688×1520 BGGR samples the active image is **2686×1518** (Bayer phase trim): 24 464 088 bytes |
| `demosaic.png` | RGB8 preview, `v8 = v12 >> 4`: no WB, gamma, tone mapping or auto-level, so it looks darker than `platform.png` by design. Check values with `demosaic.rgb16`, not the PNG |
| `demosaic.json` | stage, frame key (ordinal, epoch, sequence), ingress frame ID, profile ID, geometry, layout, SHA-256 of `demosaic.rgb16`, the frame's BLC/LSC/BPC/WB/DG settings |
| `demosaic.d/` | every frame's capture (`frames/<key>.*`) and `index.jsonl` |

- **This is a host debug utility of the VP, not an ISP output.** The ISP has
  no such port: no CSR, DMA, interrupt or DDR buffer, nothing the firmware can
  see or use; the option changes neither the NV12 nor the simulated timing.
- **Check:** a sample's capture must equal the stand-alone ISP model's
  capture of the same frame (`DEMOSAIC PASS`); your own RAW is not compared
  (`DEMOSAIC DONE`). A capture is published only for a frame that retired
  with every output DONE; if the job's final frame did not, the run fails
  (an earlier frame is never shown instead). `summary.txt` lists the capture.
  `python3 sw/bootloader/test/VP_FX1_V3.0/demosaic_validate.py <dir>/demosaic`
  re-checks a capture (size, 12-bit range, SHA-256, PNG).
- **Interactive** (`... interactive [port] --dump-demosaic`): every frame of
  the session is kept in `demosaic.d/frames/` with its key; no frame is
  selected (the firmware gives no job ID, so ordinal/epoch/sequence identify
  the frames).
- **Cost:** about 36 MB per frame on disk (RGB16 + uncompressed PNG) and some
  host time; a host write error makes the run fail (`ISP FAIL ... Demosaic
  capture failed`). `fx1_soc --dump-isp-demosaic DIR` is the underlying VP
  option (DIR must be new or empty).

### Your own RAW frame

```bash
sw/bootloader/test/VP_FX1_V3.0/run_vp.sh isp "/path/to/my_frame.raw"
sw/bootloader/test/VP_FX1_V3.0/run_vp.sh isp "/path/to/my_frame.raw" auto
sw/bootloader/test/VP_FX1_V3.0/run_vp.sh isp "/path/to/my_frame.raw" interactive 5000
```

- **Format:** the samples' format only: 2688×1520 BGGR, RAW10 in bits
  [15:6] of little-endian 16-bit words, no padding (8 171 520 bytes). The
  frame is converted the way the samples are (`>> 4` to the ISP's 12-bit
  scale; the 6 low bits must be zero) and runs with `d65`'s job and the
  shared profile. Another size, Bayer order or bit layout is refused: it needs
  its own job and profile.
- **No golden.** Nothing in the SDK computes the stand-alone ISP model's
  output for a new frame, so the run ends with `ISP DONE` (not compared), not
  `ISP PASS`. The result is in `sw/bootloader/build/VP_FX1_V3.0/isp/raw_<name>/`
  (`platform.nv12`, `platform.png`), `<name>` being the file name without its
  extension, in lower case, other characters than `a-z0-9_` replaced by `_`.
- **Paths with spaces:** quote them. An unquoted one is accepted as long as
  its words, joined with single spaces, name the file.

## Integrity and provenance

```bash
sha256sum -c vp/doc/VP_FX1_V3.0/SHA256SUMS
vp/bin/VP_FX1_V3.0/fx1_soc --version
```

- **`SHA256SUMS`** covers every delivered file that the SDK repository tracks.
  The firmware outputs under `build/` are excluded, because every user rebuilds
  them.
- **`BUILD_MANIFEST.json`** records:
  - the CDC-VP revision, and whether the tree was clean;
  - the compilers;
  - the VP++ pin and patch series, and the FreeRTOS pin;
  - the memory and IRQ map (generated from `fx1_memory_map.h`; `check_package.sh`
    checks it against the shipped header), the modelled and absent blocks, the
    samples and their expected hashes;
  - the SHA-256 of every binary, including the packaged firmware ELF.

## Limits

- **Placeholder map.** The memory map, IRQ IDs and `mtime` rate are VP
  placeholders, pending the HAS. Use the header, never literals, so a map
  update is a rebuild.
- **Functional, not timing-accurate.** Use the VP to validate register
  semantics, interrupt ordering and data movement, not cycle counts or WCET.
- **Single-core FreeRTOS.** Hart 1 is not scheduled; use it bare-metal if
  needed. There is no tickless idle; the idle task executes `wfi`.
- **No caches.** `fx1_dma_prepare()` / `fx1_dma_complete()` are no-ops in the
  VP. Call them anyway: silicon will need them.

## Licences

`licenses/` holds every licence the binaries and sources require:

- the CDC-VP model code (Apache-2.0, with `CDC-VP-NOTICE.txt`);
- RISC-V VP++ (MIT) and Berkeley SoftFloat 3d (BSD-3-Clause), both statically
  linked into `fx1_soc`;
- SystemC (Apache-2.0), shipped as `libsystemc`;
- FreeRTOS-Kernel (MIT), in the firmware sources.

`fx1_soc` also links the MIPI D-PHY RX model (`mipirxphy`) of the camera path.
It was converted to SystemC from RTL, gate-level netlists and analog
behavioural models; its supplier, owner and original licence are not
confirmed, and no licence is asserted for it (provenance pending,
`licenses/THIRD_PARTY.md`).

Keep `licenses/` with the tree whenever it is passed on.

## Adaptive WB/tone demo

```bash
sw/bootloader/test/VP_FX1_V3.0/run_vp.sh isp d65 auto
sw/bootloader/test/VP_FX1_V3.0/run_vp.sh isp indoor0 auto
make -j1 -C sw/bootloader/sources/VP_FX1_V3.0 check-isp-auto
```

Use a sample returned by `run_vp.sh isp list`. The shared base stays in
`profiles/shared_bggr_2688x1520_full/`; the host never computes WB gains.
Firmware acquires/releases AWB zones from one completed probe, selects bright
neutral candidates and applies a bounded WB/tone/EE interpolation. At the
base/daylight end it keeps the accepted v2 controls; toward the LED end it
adopts the neutral-zone estimate and the accepted LED tone. No RAW filename or
indoor/outdoor tag is used. With weak evidence it retains the base controls.
This is a demo heuristic, not calibrated or universal AWB, illuminant detection,
AE sensor control, or a continuous video 3A loop. It makes one decision per job.

`auto` creates a transient job in the output directory: one probe plus at least
2 render frames (a usual 2-frame sample therefore runs 3 physical frames).
After the probe it restarts the driver, disables GTM for the render warm-up,
then enables GTM auto. This isolates GTM/NR history for reference comparison.
The result counters include all physical frames of this job; FRAME_ID also
includes earlier jobs in an interactive boot. Input sample/profile files stay
fixed. Auto currently requires WB enabled, GTM auto and EE profile 0.

The probe frame is measured on a 32 × 16 AWB grid, the grid for which the
demo policy (`neutral-v1`, at least 8 neutral zones) was designed. The
render frames use the profile's 16 × 16 grid again. `auto.json` records both
grids (`probe_awb_grid`, `render_awb_grid`).

Outputs include `auto.json`, the exact applied CSR values in
`resolved_profile.csrw`, and the usual `platform.png`/NV12/log. A delivered
`expected_auto_nv12.sha256` is checked separately from the fixed-mode golden.
Without an auto golden the launcher prints **ISP AUTO COMPLETE** (valid output,
no reference comparison), rather than claiming a golden match. In CDC-VP,
use `fx1_isp_run_raw` with the same `input.isp16`, the resolved profile and
`--frames 2` (or the auto.json render count) to independently verify a new sample.
The regular `run_vp.sh isp SAMPLE` fixed mode and its golden remain supported.
