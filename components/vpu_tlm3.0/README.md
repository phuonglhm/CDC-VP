# vpu_tlm3.0 — SystemC/TLM HEVC/H.265 VPU Model

`vpu_tlm3.0` is a self-contained virtual video-processing unit (VPU) model for
the CDC-VP platform. It reads planar raw YUV420p 8-bit video and generates an
HEVC/H.265 Annex-B elementary bitstream (`.h265`).

The encoder generates HEVC syntax and bitstream bytes internally. It does not
call FFmpeg, x265, HM, or another external encoder. FFmpeg, FFprobe, and FFplay
are used only as independent decoder, inspection, and playback tools during
verification.

The project contains three execution paths:

1. A functional command-line encoder.
2. A C++ MMIO/DMA/IRQ model with a cycle-stepped bounded-FIFO pipeline.
3. A native SystemC/TLM-2.0 model built and verified with SystemC 2.3.4.

This is an architectural and codec-development model. It is not a
production-quality replacement for x265, and its cycle counts are not claimed
to match RTL or silicon until they are calibrated against an RTL trace.

## Key Features

- Raw planar YUV420p 8-bit input.
- Native generation of HEVC/H.265 Annex-B bitstreams.
- VPS, SPS, PPS, IDR slices, CABAC coding, RBSP-to-EBSP emulation prevention,
  and conformance-window cropping.
- Lossless HEVC PCM mode and several experimental all-intra compression modes.
- 32x32 coding-tree units (CTUs), with one independent slice per CTU.
- Integer transform, quantization/dequantization, coefficient scanning, and
  CABAC coefficient coding.
- DC, horizontal, and vertical luma intra-prediction candidates.
- 32x32 and split 16x16 transform-unit options.
- 32-bit little-endian MMIO register interface.
- 64-bit source and destination DMA addresses.
- START/BUSY/DONE/ERROR control flow and level-sensitive IRQ output.
- Bounded packet FIFOs and cycle-stepped backpressure counters.
- Native SystemC `sc_module`, `SC_THREAD`, `sc_fifo`, clock, reset, IRQ,
  TLM target socket, and TLM DMA initiator socket.
- Configurable FIFO depth, DMA burst size, clock frequency, SRAM latency, and
  SRAM transfer width.
- Terminal hardware-metric dashboard.
- Bit-exact comparison between functional, MMIO, and SystemC bitstreams.

## Encoder Data Path

```text
Raw YUV420p memory
        |
        | TLM DMA reads
        v
  Input DMA stage
        |
        | sc_fifo<FramePacket>
        v
  Prediction stage
        |
        | sc_fifo<FramePacket>
        v
  Transform / quantization stage
        |
        | sc_fifo<FramePacket>
        v
  CABAC / bitstream packer
        |
        | sc_fifo<BitstreamPacket>
        v
  Output DMA stage
        |
        | TLM DMA writes
        v
HEVC Annex-B bitstream memory
```

The CPU/testbench programs the VPU through the MMIO target socket. The
controller submits a `JobConfig` packet, receives a completion packet, updates
the DONE or ERROR status, and asserts IRQ when enabled.

## Executables

| Executable | Description |
|---|---|
| `build/vpu_tlm3.0` | Functional raw-YUV-to-HEVC encoder |
| `build/vpu_mmio_demo` | C++ MMIO/DMA/IRQ and FIFO timing model |
| `build/vpu_tlm3.0_systemc_native` | Native SystemC/TLM-2.0 VPU model |
| `build/vpu_tlm3.0_systemc` | Legacy SystemC adapter testbench |
| `build/yuvgen` | Deterministic raw YUV test-vector generator |
| `build/test_core` | Core bit-writer, CABAC, YUV, and transform tests |
| `build/test_vpu_mmio` | MMIO, DMA, IRQ, and error-path tests |
| `build/test_fifo_cycle_model` | FIFO timing and backpressure tests |
| `build/test_hardware_metrics` | Hardware-report formula and format tests |

## Supported Coding Modes

| Mode | Description | Intended use |
|---|---|---|
| `pcm` | HEVC PCM, lossless, approximately raw-sized | Golden bring-up and byte-exact input/decoded comparison |
| `intra-dc` | DC prediction without coded residual | I-slice and CABAC syntax testing |
| `hybrid-dc` | Selects DC or PCM according to input error and QP | Early input-dependent compression model |
| `intra-dc-tq` | DC prediction, integer transform, quantization, and DC coefficient coding | Basic lossy transform path |
| `intra-full-tq` | Full DC/AC transform coefficients and grouped diagonal scan | 32x32 luma and 16x16 chroma transform path |
| `intra-full-tq16` | Fixed split transform tree | Four 16x16 luma TUs and 8x8 chroma TUs |
| `intra-adaptive-tq` | Selects between 32x32 and split 16x16 reconstruction by SSE | Adaptive transform-size baseline |
| `intra-directional-tq` | Tests DC, horizontal, and vertical candidates and selects the lowest-SSE option | Recommended mode for quality and metric evaluation |

The default mode is `pcm` to preserve the original lossless regression path.

## Repository Structure

```text
vpu_tlm3.0/
├── app/                 Functional CLI and MMIO demo applications
├── include/hevc/        HEVC bitstream, CABAC, transform, and YUV interfaces
├── include/model/       Functional/MMIO/FIFO model interfaces
├── include/systemc_vpu/ Native SystemC packets and hardware-report interface
├── src/                 Codec, transform, MMIO, FIFO, and pipeline sources
├── optional_systemc/    SystemC/TLM modules and testbenches
├── tests/               Unit and regression tests
├── tools/               Raw-YUV test-vector generator
├── docs/                Architecture, register map, metrics, and release notes
├── input/               Raw `.yuv` input workspace
├── output/              `.h265`, decoded YUV, and metric-report workspace
├── Makefile
└── README.md
```

Generated files under `build/`, raw `.yuv` files, and `.h265` bitstreams are
ignored by Git.

## Requirements

### Required to build the functional and MMIO models

- GNU Make.
- A C++20 compiler, such as GCC 11 or newer.

### Required to build the CDC-VP SystemC model

- Accellera SystemC 2.3.4.
- CDC-VP installation used by this project:

```text
/opt/systemc-2.3.4/include
/opt/systemc-2.3.4/lib/libsystemc.so.2.3.4
```

### Required for independent verification and playback

- FFmpeg.
- FFprobe.
- FFplay, if interactive playback is required.

These tools are not part of the encoding path.

## Build

Change to the component directory:

```bash
cd ~/CDC-VP/components/vpu_tlm3.0
```

### Build the functional encoder and MMIO model

```bash
make clean
make all
```

### Build the native SystemC/TLM model with SystemC 2.3.4

Always pass both paths on the current CDC-VP installation. The SystemC library
is stored in `lib`, not `lib-linux64`.

```bash
make systemc-native \
  SYSTEMC_HOME=/opt/systemc-2.3.4 \
  SYSTEMC_LIBDIR=/opt/systemc-2.3.4/lib
```

The compile and link command must contain:

```text
-I/opt/systemc-2.3.4/include
-L/opt/systemc-2.3.4/lib
-Wl,-rpath,/opt/systemc-2.3.4/lib
```

Confirm the linked library:

```bash
ldd build/vpu_tlm3.0_systemc_native | grep systemc
```

Expected library:

```text
/opt/systemc-2.3.4/lib/libsystemc.so.2.3.4
```

At startup, the executable must print:

```text
SystemC 2.3.4-Accellera
```

The Ubuntu 22.04 `libsystemc-dev` package provides SystemC 2.3.3. Do not rely
on that system package when exact CDC-VP 2.3.4 synchronization is required.

## Quick Verification

Run the complete native SystemC regression:

```bash
cd ~/CDC-VP/components/vpu_tlm3.0

make clean

make verify-systemc-native \
  SYSTEMC_HOME=/opt/systemc-2.3.4 \
  SYSTEMC_LIBDIR=/opt/systemc-2.3.4/lib
```

This target performs the following checks:

1. Builds the functional, MMIO, test-vector, and SystemC executables.
2. Generates an eight-frame 96x66 YUV420p test sequence.
3. Creates the functional reference bitstream and reconstruction.
4. Runs SystemC with FIFO depths 1 and 4.
5. Requires FIFO depth 1 to produce observable backpressure.
6. Compares both SystemC bitstreams with the functional reference bitstream.
7. Uses FFprobe to confirm HEVC, 96x66, and YUV420p.
8. Decodes the bitstream and compares it byte-for-byte with reconstruction.
9. Checks the output-overflow error path.
10. Checks that the hardware dashboard contains the required metrics.

## Raw Input Format

Input files must be headerless planar YUV420p 8-bit data in this order:

```text
Y plane, followed by Cb plane, followed by Cr plane, repeated for every frame
```

The number of bytes per frame is:

```text
frame_bytes = width * height * 3 / 2
```

Width and height must be even. A raw file contains no width, height, frame
rate, or frame-count metadata, so those values must be supplied separately.

The project test sequence uses:

```text
input/bus_cif.yuv
352x288, YUV420p 8-bit, 150 frames
Expected size: 22,809,600 bytes
```

Check the input size:

```bash
stat -c "%n: %s bytes" input/bus_cif.yuv
```

## Run the Functional Encoder

The functional path is useful as the golden codec reference and can also write
the internal reconstruction for lossy-mode verification.

```bash
mkdir -p output

./build/vpu_tlm3.0 \
  -i input/bus_cif.yuv \
  -w 352 -h 288 -n 150 \
  --mode intra-directional-tq \
  --qp 26 \
  --recon output/bus_cif_directional_recon.yuv \
  -o output/bus_cif_directional_cli.h265
```

If `--frames` is zero or omitted, the encoder processes every complete frame
available in the raw file.

## Run the C++ MMIO/DMA/FIFO Model

The MMIO demo stages raw YUV in simulated memory, programs source/destination
addresses and control registers, starts the VPU, waits for IRQ, and retrieves
exactly `BITSTREAM_BYTES` bytes from the destination DMA buffer.

```bash
./build/vpu_mmio_demo \
  -i input/bus_cif.yuv \
  -w 352 -h 288 -n 150 \
  --mode intra-directional-tq \
  --qp 26 \
  --fifo-depth 4 \
  -o output/bus_cif_directional_mmio.h265
```

Compare the functional and MMIO bitstreams:

```bash
cmp output/bus_cif_directional_cli.h265 \
    output/bus_cif_directional_mmio.h265 \
  && echo "CLI/MMIO BITSTREAM EXACT"
```

## Run the Native SystemC/TLM Model

Build it first with the exact SystemC 2.3.4 paths shown above, then run:

```bash
mkdir -p output
set -o pipefail

./build/vpu_tlm3.0_systemc_native \
  -i input/bus_cif.yuv \
  -w 352 -h 288 -n 150 \
  --mode intra-directional-tq \
  --qp 26 \
  --fifo-depth 4 \
  --dma-burst 64 \
  --clock-mhz 800 \
  --sram-read-latency 1 \
  --sram-write-latency 1 \
  --sram-bytes-per-cycle 16 \
  -o output/bus_cif_systemc.h265 \
  2>&1 | tee output/bus_cif_hardware_metrics.txt
```

Main outputs:

```text
output/bus_cif_systemc.h265
output/bus_cif_hardware_metrics.txt
```

The hardware dashboard is enabled by default. Use `--no-hw-report` to suppress
the detailed dashboard, or `--hw-report` to enable it explicitly.

### Native SystemC options

| Option | Meaning | Default/range |
|---|---|---|
| `-i`, `--input` | Raw YUV input path | Required |
| `-o`, `--output` | HEVC Annex-B output path | `output.h265` |
| `-w`, `--width` | Luma width | Required, positive and even |
| `-h`, `--height` | Luma height | Required, positive and even |
| `-n`, `--frames` | Number of frames | All complete frames when zero/omitted |
| `--mode` | Coding mode from the supported-mode table | `pcm` |
| `--qp` | Quantization parameter | 0 to 51, default 26 |
| `--fifo-depth` | Depth of all four native `sc_fifo` channels | 1 to 255, default 4 |
| `--dma-burst` | Maximum TLM DMA burst payload | 1 to 4096 bytes, default 64 |
| `--clock-mhz` | Modeled VPU clock | 1 to 10000 MHz, default 1000 |
| `--sram-read-latency` | Fixed target read latency | 0 to 1000 cycles, default 1 |
| `--sram-write-latency` | Fixed target write latency | 0 to 1000 cycles, default 1 |
| `--sram-bytes-per-cycle` | SRAM target transfer width | 1 to 4096, default 16 |
| `--hw-report` | Enable the detailed dashboard | Enabled by default |
| `--no-hw-report` | Disable the detailed dashboard | Optional |
| `--dst-capacity` | Override the simulated output-buffer capacity | Verification/debug option |
| `--expect-error` | Require a specific VPU error code | Verification/debug option |

## Hardware Metric Dashboard

The native executable prints the dashboard after a successful encode. It uses
source tags so measured values are not confused with assumptions:

| Tag | Meaning |
|---|---|
| `[M]` | Measured by counters or timestamps during SystemC execution |
| `[D]` | Derived from measured counters and the configured clock |
| `[S]` | User-selected model or testbench setting |
| `[A]` | Requires an external analytic/synthesis/calibration model |

Reported metrics include:

- Total cycles and derived latency.
- Frames per second, megapixels per second, and cycles per frame.
- Per-stage active cycles, completed frames, cycles per frame, first-done
  latency, and utilization.
- Pipeline fill and drain cycles.
- FIFO peak occupancy and producer-stall cycles.
- DMA read/write payload bytes and TLM burst counts.
- SRAM read/write response delay and effective DMA bandwidth.
- Raw input size, bitstream size, compression ratio, and bits per pixel.
- DONE/IRQ completion state.

The model intentionally reports area, power, energy, PE-array utilization, and
RTL correlation as `N/A` when no synthesis result or calibrated external model
is available. It does not invent physical-design metrics from simulation
cycles.

Multiple stages can stall in the same clock cycle. Therefore, the sum of all
stall counters may be greater than total job latency.

The simulated SRAM target applies this timing rule to each TLM burst:

```text
access_cycles = configured_read_or_write_latency
                + ceil(payload_bytes / sram_bytes_per_cycle)
```

Changing clock frequency changes simulated time and derived throughput but
does not change the architectural cycle count. Increasing SRAM latency can
increase cycle count without changing bitstream contents.

See [docs/hardware_metrics.md](docs/hardware_metrics.md) for additional metric
definitions.

## Inspect and Decode the Output

Inspect codec metadata:

```bash
ffprobe -v error \
  -select_streams v:0 \
  -show_entries stream=codec_name,width,height,pix_fmt \
  -of default=nw=1 \
  output/bus_cif_systemc.h265
```

Expected fields:

```text
codec_name=hevc
width=352
height=288
pix_fmt=yuv420p
```

Decode to raw YUV:

```bash
ffmpeg -v error \
  -i output/bus_cif_systemc.h265 \
  -f rawvideo -pix_fmt yuv420p \
  -y output/bus_cif_systemc_decoded.yuv
```

For lossless `pcm`, compare decoded output directly with the raw input:

```bash
cmp input/bus_cif.yuv output/bus_cif_systemc_decoded.yuv \
  && echo "BYTE-EXACT LOSSLESS PASS"
```

For a lossy mode, compare the decoded output with reconstruction generated by
the functional model, not with the original input:

```bash
cmp output/bus_cif_directional_recon.yuv \
    output/bus_cif_systemc_decoded.yuv \
  && echo "RECONSTRUCTION EXACT"
```

## Play the Video

Play the HEVC elementary stream:

```bash
ffplay -framerate 30 output/bus_cif_systemc.h265
```

Play decoded raw YUV:

```bash
ffplay \
  -f rawvideo \
  -pixel_format yuv420p \
  -video_size 352x288 \
  -framerate 30 \
  output/bus_cif_systemc_decoded.yuv
```

Raw YUV files do not contain an index, so seeking in FFplay may be limited.

## Check Output File Size

Human-readable size:

```bash
ls -lh output/bus_cif_systemc.h265
```

Exact byte count:

```bash
stat -c "%n: %s bytes" output/bus_cif_systemc.h265
```

List all output bitstreams from smallest to largest:

```bash
du -h output/*.h265 | sort -h
```

Compare raw-input and bitstream sizes:

```bash
stat -c "%n: %s bytes" \
  input/bus_cif.yuv \
  output/bus_cif_systemc.h265
```

## MMIO Interface

The complete register map is documented in
[docs/register_map.md](docs/register_map.md). Important registers include:

| Register | Function |
|---|---|
| `CONTROL` | START, SOFT_RESET, IRQ_ENABLE |
| `STATUS` | BUSY, DONE, ERROR, IRQ |
| `SRC_ADDR` | 64-bit raw-YUV DMA source address |
| `DST_ADDR` | 64-bit bitstream DMA destination address |
| `DST_CAPACITY` | Destination-buffer capacity in bytes |
| `WIDTH`, `HEIGHT`, `STRIDE_Y` | Raw-frame geometry |
| `FRAME_COUNT`, `QP`, `ENCODER_MODE` | Encode configuration |
| `BITSTREAM_BYTES`, `FRAMES_DONE` | Completion results |
| `ERROR_CODE`, `IRQ_STATUS` | Error and interrupt handling |
| `CYCLES` | 64-bit modeled job-cycle counter |
| `FIFO_CONFIG` | Four packed FIFO depths |
| `STALL_*` | Backpressure cycle counters |
| `*_ACTIVE` | Stage activity counters |

The native SystemC implementation reports VPU register version
`0x0003000A`.

## Additional Test Targets

```bash
make test                 # Unit tests
make verify               # PCM functional verification
make verify-mmio          # MMIO/DMA/IRQ verification
make verify-m2            # DC transform/quantization milestone
make verify-m21           # Full coefficient milestone
make verify-m22           # Adaptive transform milestone
make verify-m3            # Directional prediction milestone
make verify-m7a           # Complete C++ FIFO cycle-model regression
make verify-systemc-native \
  SYSTEMC_HOME=/opt/systemc-2.3.4 \
  SYSTEMC_LIBDIR=/opt/systemc-2.3.4/lib
```

The verification targets use FFmpeg only on the decoder side. They compare
decoded raw YUV with either the original lossless input or the encoder's
internal reconstruction.

## Timing Accuracy

The C++ FIFO and native SystemC paths are deterministic architectural timing
models for the throughput, FIFO, SRAM, and stage rules implemented in this
source tree. They model:

- Clocked stage execution.
- Bounded FIFO occupancy and backpressure.
- Stage active and stall cycles.
- TLM burst count and annotated memory delay.
- Pipeline fill and drain timing.
- MMIO completion and IRQ behavior.

They do not currently model:

- RTL implementation-specific state timing.
- Bus arbitration or contention from other SoC initiators.
- Multiple outstanding DMA transactions.
- SRAM banking conflicts or cache behavior.
- Synthesis area, physical frequency closure, power, or energy.

The correct description is **cycle-stepped architectural model**, not
RTL-correlated cycle-accurate model. RTL cycle accuracy requires an RTL trace
oracle and stage-by-stage calibration.

## Current Codec Limitations

- Input format is limited to planar YUV420p 8-bit.
- Width and height must be even.
- All pictures are IDR/all-intra.
- CTU size is fixed at 32x32.
- Every CTU is encoded as an independent slice.
- The model does not implement inter prediction, motion estimation, reference
  picture management, GOP structures, or B/P frames.
- The directional mode currently supports only DC, horizontal, and vertical
  luma candidates; it does not implement the complete HEVC planar and angular
  prediction set.
- Chroma prediction is DC-based.
- There is no rate control, VBV/HRD model, adaptive GOP, or production preset
  system.
- VUI timing is not currently signaled in the elementary stream.
- Main profile is signaled, and Level 6.2 is used during this model-development
  stage to avoid early conformance restrictions.
- Compression efficiency and visual quality are below production encoders such
  as x265 and HM.
- Frame-level pipeline tokens are used in the native SystemC model; further CTU
  and transform-block partitioning is required for an RTL-oriented microarchitecture.

## CDC-VP Integration

The component directory name must remain `vpu_tlm3.0`:

```bash
mkdir -p ~/CDC-VP/components/vpu_tlm3.0
cp -a /path/to/vpu_tlm3.0/. ~/CDC-VP/components/vpu_tlm3.0/

cd ~/CDC-VP/components/vpu_tlm3.0

make verify-systemc-native \
  SYSTEMC_HOME=/opt/systemc-2.3.4 \
  SYSTEMC_LIBDIR=/opt/systemc-2.3.4/lib
```

If the parent CDC-VP build uses a component manifest or top-level CMake list,
add `components/vpu_tlm3.0` using the same mechanism as the existing
components.

## Troubleshooting

### The runtime banner shows SystemC 2.3.3

The executable was linked against Ubuntu's system library. Clean and rebuild
with the CDC-VP 2.3.4 paths:

```bash
make clean
make verify-systemc-native \
  SYSTEMC_HOME=/opt/systemc-2.3.4 \
  SYSTEMC_LIBDIR=/opt/systemc-2.3.4/lib
```

### `cannot find -lsystemc`

Check that the library exists and pass the correct directory:

```bash
ls -l /opt/systemc-2.3.4/lib/libsystemc.so*
```

### FFprobe reports no stream or the decoder reports missing slices

Delete the partial output and run the encoder again. Also verify that the raw
input size, width, height, and frame count agree.

### Raw playback has incorrect colors or geometry

Use exactly `yuv420p` and the original width and height when invoking FFplay.

### The decoded lossy output does not match the original input

This is expected. A lossy stream must be compared with the functional model's
`--recon` output. Only `pcm` is expected to reproduce the original input
byte-for-byte.

## Related Documentation

- [Architecture and roadmap](docs/architecture.md)
- [MMIO register map](docs/register_map.md)
- [Hardware metric dashboard](docs/hardware_metrics.md)
- [Native SystemC release](docs/release_systemc_native.md)
- [CDC-VP integration](docs/cdc_vp_integration.md)
- [All-intra development plan](docs/all_intra_plan.md)

## Standards and References

- [ITU-T H.265](https://www.itu.int/rec/T-REC-H.265)
- [Accellera SystemC](https://www.accellera.org/downloads/standards/systemc)
- [Accellera TLM-2.0 Language Reference Manual](https://www.accellera.org/images/downloads/standards/systemc/TLM_2_0_LRM.pdf)
- [HEVC Test Model reference software](https://vcgit.hhi.fraunhofer.de/jvet/HM)

## License

This project is licensed under the MIT License. See [LICENSE](LICENSE).
