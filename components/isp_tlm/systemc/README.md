# ISP SystemC architecture-exploration model

## Status and scope

This directory contains the register-driven SystemC model of the ISP only. It
does not modify or implement the VPU, NPU, or their firmware.

The control and memory architecture is now:

```text
                                      +-----------------------+
AXI4-Lite pins ---------------------->|                       |
                                      | shared ISP register   |-- private,
fast control TLM target ------------->| bank                  |   frame-latched
                                      |                       |   configuration
                                      +-----------+-----------+
                                                  |
RAW pins / direct-RGB pins / RAW memory ----------+--> ISP pipeline
                                                        |
                                                        +--> one VIP
                                                             |
                                                             +--> sparse I420 pins
                                                             +--> planar I420 memory
```

There is exactly one output `VIP`. The old first-VIP register offsets are
preserved under unnumbered `VIP_*` names. The former second-VIP scalar and OSD
regions are unmapped and must not be used.

The model is intended for architecture exploration and virtual-platform
integration. It is not claimed to be a bit-accurate or cycle-accurate
replacement for the RTL. Production firmware is outside this directory's
scope; the map below is the handoff reference for the future SDK team.

## Canonical sources

The software-visible constants are separated from the datapath:

- `src/registers/isp_register_map.h` is the canonical C++ offset/field header.
- `src/registers/isp_register_bank.h/.cpp` is the canonical access, reset,
  shadowing, and side-effect implementation.
- `src/tlm/isp_control_target.h` exposes the fast control path.
- `src/axi/isp_axi_lite_adapter.h` exposes the cycle-level AXI4-Lite path.
- `src/pipeline/isp_top.h` is the public ISP integration boundary.

The map starts from the existing Infinite-ISP RTL AXI offsets. Valid offsets
are preserved; WSTRB, channel selection, status semantics, and the fixed
64 KiB decoding defects are corrected in the model.

## Public `isp_top` interface

All tuning parameters are private connections driven by the active register
bank. They are no longer public `isp_top` ports.

| Group | Public interface |
| --- | --- |
| Pixel clock/reset | `pclk`, active-low `rst_n` |
| RAW stream | `in_href`, `in_vsync`, `in_raw[BITS-1:0]` |
| Direct RGB stream | `in_href_rgb`, `in_vsync_rgb`, `in_r/g/b[BITS-1:0]` |
| Gamma observation | `out_gamma_href`, `out_gamma_vsync`, `out_gamma_r/g/b[BITS-1:0]` |
| Sole VIP output | `out_href`, `out_vsync`, `out_y/u/v[7:0]` |
| Interrupt | `irq` |
| AXI control | 32-bit AXI4-Lite AW/W/B/AR/R channels, `aclk`, active-low `aresetn` |
| Fast control | `control_socket`, a blocking TLM target socket |
| Frame memory | `memory_socket`, a blocking TLM initiator socket |

`VSYNC` is high between frames. Its falling edge starts a frame and its rising
edge ends a frame. `HREF` is high for active pixels.

The RAW pipeline order is:

```text
Crop -> DPC -> BLC/linearization -> OECF -> DGain -> LSC -> BNR -> WB
     -> Demosaic -> CCM -> Gamma -> CSC -> LDCI -> Sharpen -> 2DNR -> VIP
```

Direct RGB enters immediately before CCM. AE and AWB observe their pipeline
taps and update hardware-owned status registers.

## Shared control behavior

AXI4-Lite and control TLM access the same `IspRegisterBank`; there is no
separate or duplicated map.

- Register words and memory-window entries are 32-bit little-endian values.
- The canonical address space is exactly `0x0000` through `0xFFFF`.
- AXI supports independent AW and W arrival, held B/R responses, backpressure,
  and all four `WSTRB` byte lanes.
- AXI returns `OKAY` for valid accesses, `DECERR` for misaligned,
  out-of-range, or unmapped addresses, and `SLVERR` for read-only or invalid
  values.
- Control TLM supports aligned 1-, 2-, and 4-byte transfers that do not cross a
  register word. It supports TLM byte enables and optional annotated latency.
- Rejected control-TLM transactions increment `ISP_TLM_ERROR_COUNT`. AXI
  errors do not increment that TLM-specific counter.
- `aresetn` is sampled synchronously on `aclk` and resets both the AXI channel
  state and the shared bank. `rst_n` resets the pixel-side state and shared
  bank. `ISP_RESET` provides a software reset.

### Access notation

| Mark | Meaning |
| --- | --- |
| RO | Software read-only; hardware/build configuration owns the value |
| RW-F | Software read/write pending value; becomes active atomically at a frame commit |
| RW-I | Software read/write and effective immediately |
| W1C | Writing one clears the selected status bit |
| W1S | Writing one performs the action; the bit does not remain set |
| Mixed | Per-field behavior is described in the field table |

Software reads return the pending copy. The datapath reads the active copy.
All `RW-F` scalars and LUT/RAM contents commit together:

- immediately when a valid `START` request is consumed; or
- on the next input-frame start for normal, unarmed streaming.

Job descriptors are snapshotted at `START`. A write made after `START` affects
a later frame/job, not the job already running.

Interrupt masks are `RW-I`. A mask bit of one disables that interrupt source.
Status is retained until cleared with W1C.

## Register map

All offsets below are relative to the ISP device base. Unlisted words are
unmapped and return a bus error.

### ISP identification, control, job, and descriptors

| Offset | Name | Access | Reset | Description |
| ---: | --- | --- | ---: | --- |
| `0x0000` | `ISP_RESET` | W1S | `0` | Bit 0 resets scalar state |
| `0x0004` | `ISP_SENSOR_WIDTH` | RO | `WIDTH` | Elaborated sensor width |
| `0x0008` | `ISP_SENSOR_HEIGHT` | RO | `HEIGHT` | Elaborated sensor height |
| `0x000C` | `ISP_CROP_WIDTH` | RO | `WIDTH` | Current fixed crop width |
| `0x0010` | `ISP_CROP_HEIGHT` | RO | `HEIGHT` | Current fixed crop height |
| `0x0014` | `ISP_BITS` | RO | `BITS` | RAW/RGB sample width, 8 through 12 |
| `0x0018` | `ISP_BAYER` | RO | template | 0 RGGB, 1 GRBG, 2 GBRG, 3 BGGR |
| `0x0040` | `ISP_TOP_ENABLE` | RW-F | `0x0003EFDF` | Bits 17:0 enable ISP blocks |
| `0x0044` | `ISP_INTERRUPT_STATUS` | W1C | `0` | Bits 5:0; see below |
| `0x0048` | `ISP_INTERRUPT_MASK` | RW-I | `0x3F` | Bits 5:0; one means masked |
| `0x0080` | `ISP_JOB_CONTROL` | Mixed | `0` | Source, START, and direct-RGB fields |
| `0x0084` | `ISP_JOB_STATUS` | Mixed | `0` | BUSY plus W1C DONE/ERROR |
| `0x0088` | `ISP_JOB_ERROR_CODE` | RO | `0` | Last job error code |
| `0x008C` | `ISP_FRAME_COUNT` | RO | `0` | Terminal job attempts plus completed unarmed stream frames |
| `0x0100` | `ISP_SOURCE_ADDRESS` | RW-F | `0` | 32-bit physical RAW base address |
| `0x0104` | `ISP_SOURCE_STRIDE_BYTES` | RW-F | `0` | RAW row stride in bytes |
| `0x0108` | `ISP_SOURCE_SIZE_BYTES` | RW-F | `0` | Declared RAW buffer capacity |
| `0x010C` | `ISP_DESTINATION_Y_ADDRESS` | RW-F | `0` | Y-plane physical base |
| `0x0110` | `ISP_DESTINATION_U_ADDRESS` | RW-F | `0` | Cb/U-plane physical base |
| `0x0114` | `ISP_DESTINATION_V_ADDRESS` | RW-F | `0` | Cr/V-plane physical base |
| `0x0118` | `ISP_DESTINATION_Y_STRIDE_BYTES` | RW-F | `0` | Y-plane row stride |
| `0x011C` | `ISP_DESTINATION_UV_STRIDE_BYTES` | RW-F | `0` | Shared U and V row stride |
| `0x0120` | `ISP_DESTINATION_Y_SIZE_BYTES` | RW-F | `0` | Declared Y-plane capacity |
| `0x0124` | `ISP_DESTINATION_U_SIZE_BYTES` | RW-F | `0` | Declared U-plane capacity |
| `0x0128` | `ISP_DESTINATION_V_SIZE_BYTES` | RW-F | `0` | Declared V-plane capacity |
| `0x012C` | `ISP_OUTPUT_WIDTH` | RO | `WIDTH` | Fixed I420 width |
| `0x0130` | `ISP_OUTPUT_HEIGHT` | RO | `HEIGHT` | Fixed I420 height |
| `0x0134` | `ISP_OUTPUT_FORMAT` | RO | `0` | Zero means planar I420 |
| `0x0140` | `ISP_LAST_JOB_READ_BYTES` | RO | `0` | Successful source payload bytes |
| `0x0144` | `ISP_LAST_JOB_WRITE_BYTES` | RO | `0` | Successful destination payload bytes |
| `0x0148` | `ISP_LAST_JOB_READ_TRANSACTIONS` | RO | `0` | Issued source row transactions |
| `0x014C` | `ISP_LAST_JOB_WRITE_TRANSACTIONS` | RO | `0` | Issued destination row transactions |
| `0x0150` | `ISP_TLM_ERROR_COUNT` | RO | `0` | Rejected fast-control transactions |

`ISP_TOP_ENABLE` fields retain the RTL bit assignments:

| Bit | Block | Bit | Block |
| ---: | --- | ---: | --- |
| 0 | DPC | 9 | CCM |
| 1 | BLC | 10 | Gamma |
| 2 | Linearization | 11 | CSC |
| 3 | OECF | 12 | LDCI |
| 4 | DGain | 13 | 2DNR |
| 5 | LSC | 14 | Sharpen |
| 6 | BNR | 15 | AE |
| 7 | WB | 16 | AWB |
| 8 | Demosaic | 17 | Crop |

The reset mask enables every listed block except LSC and LDCI. For a
deterministic architecture baseline, tests program only Demosaic and CSC:
`ISP_TOP_ENABLE = (1 << 8) | (1 << 11)`.

`ISP_JOB_CONTROL` fields:

| Bit | Name | Access | Meaning |
| ---: | --- | --- | --- |
| 0 | `SOURCE_MODE` | RW-F | 0 external stream, 1 memory RAW job |
| 1 | `START` | W1S/self-clear | Snapshot/commit configuration and launch or arm one job |
| 2 | `DIRECT_RGB_INPUT` | RW-F | In stream mode, 0 RAW pins and 1 direct-RGB pins |

`START` sets BUSY immediately. A second START while BUSY is rejected, records
error code 1, and raises the job-error interrupt. A stream START arms the next
selected frame. A memory START reads the source descriptor, drives that RAW
frame through the same pipeline, captures one I420 output frame, and writes
the three destination planes. The sole VIP and core CSC must both be enabled,
and CSC standard 1 or 2 must be selected; an unusable I420 pipeline is rejected
at job start rather than silently producing black output.

`ISP_JOB_STATUS` fields:

| Bit | Name | Access | Meaning |
| ---: | --- | --- | --- |
| 0 | `BUSY` | RO | A job is armed or executing |
| 1 | `DONE` | W1C | The job reached a terminal state |
| 2 | `ERROR` | W1C | The terminal state contains an error |

`ISP_JOB_ERROR_CODE` values:

| Value | Meaning |
| ---: | --- |
| 0 | No error |
| 1 | START issued while BUSY |
| 2 | Unsupported source/configuration combination |
| 3 | Invalid RAW source descriptor |
| 4 | Source-memory transport error |
| 5 | Output frame had the wrong Y/U/V sample count |
| 6 | Invalid or overlapping destination descriptors |
| 7 | Destination-memory transport error |

ISP interrupt bits are common to `ISP_INTERRUPT_STATUS` and
`ISP_INTERRUPT_MASK`:

| Bit | Source |
| ---: | --- |
| 0 | Input frame start |
| 1 | Output frame done |
| 2 | AE done |
| 3 | AWB done |
| 4 | Job done, including an error terminal state |
| 5 | Job error |

### ISP algorithm controls

Unless marked RO, these registers are `RW-F`. `pixel_mask` means the low
`BITS` bits are implemented.

| Offset/range | Name | Reset/format |
| ---: | --- | --- |
| `0x0200` | `DPC_THRESHOLD` | `2`, `pixel_mask` |
| `0x0400` | `BLC_R` | `16 << (BITS-8)`, `pixel_mask` |
| `0x0404` | `BLC_GR` | Same as BLC_R |
| `0x0408` | `BLC_GB` | Same as BLC_R |
| `0x040C` | `BLC_B` | Same as BLC_R |
| `0x0410` | `LINEAR_R` | `0x4445`, low 16 bits |
| `0x0414` | `LINEAR_GR` | `0x4445`, low 16 bits |
| `0x0418` | `LINEAR_GB` | `0x4445`, low 16 bits |
| `0x041C` | `LINEAR_B` | `0x4445`, low 16 bits |
| `0x0600` | `AE_CENTER_ILLUMINANCE` | `110`, low 8 bits |
| `0x0604` | `AE_SKEWNESS` | `275`, low 16 bits |
| `0x0608` | `AE_CROP_LEFT` | `12`, low 12 bits |
| `0x060C` | `AE_CROP_RIGHT` | `12`, low 12 bits |
| `0x0610` | `AE_CROP_TOP` | `22`, low 12 bits |
| `0x0614` | `AE_CROP_BOTTOM` | `2`, low 12 bits |
| `0x0618` | `AE_RESPONSE` | RO, reset 0, low 2 bits |
| `0x061C` | `AE_RESULT_SKEWNESS` | RO, reset 0, low 16 bits |
| `0x0620` | `AE_RESPONSE_DEBUG` | RO, reset 0, low 2 bits |
| `0x0624` | `AE_DONE` | RO, reset 0, bit 0 |
| `0x0800` | `DGAIN_IS_MANUAL` | `0`, bit 0 |
| `0x0804` | `DGAIN_MANUAL_INDEX` | `0`, low 7 bits |
| `0x0808` | `DGAIN_INDEX_OUT` | RO, reset 0, low 7 bits |
| `0x0840-0x09CC` | `DGAIN_ARRAY[0..99]` | Low 8 bits; reset values 1 through 100 |
| `0x0C00` | `AWB_UNDEREXPOSED_LIMIT` | `51`, `pixel_mask` |
| `0x0C04` | `AWB_OVEREXPOSED_LIMIT` | `972`, `pixel_mask` |
| `0x0C08` | `AWB_FRAMES` | `1`, `pixel_mask` |
| `0x0C0C` | `AWB_FINAL_R_GAIN` | RO, reset 0, low 12 bits |
| `0x0C10` | `AWB_FINAL_B_GAIN` | RO, reset 0, low 12 bits |
| `0x0E00` | `WB_R_GAIN` | `0x13F`, low 12 bits |
| `0x0E04` | `WB_B_GAIN` | `0x2CF`, low 12 bits |
| `0x1200-0x1220` | `CCM_{RR,RG,RB,GR,GG,GB,BR,BG,BB}` | Signed low 16-bit coefficients, consumed with a 10-bit post-shift |
| `0x1400` | `CSC_CONVERSION_STANDARD` | `2`; 1 BT.709 full range, 2 BT.601 full range, 0/3 reserved |
| `0x1C00` | `SHARPEN_STRENGTH` | `0x399`, low 12 bits |
| `0x1C40-0x1D80` | `SHARPEN_KERNEL[0..80]` | 81 row-major 20-bit entries |
| `0x2000-0x2024` | `BNR_SPATIAL_R[0..9]` | Packed 5x5 R-channel kernel |
| `0x2040-0x2064` | `BNR_SPATIAL_G[0..9]` | Packed 5x5 G-channel kernel |
| `0x2080-0x20A4` | `BNR_SPATIAL_B[0..9]` | Packed 5x5 B-channel kernel |
| `0x2100-0x2120` | `BNR_COLOR_R[0..8]` | Difference in bits `BITS-1:0`, weight in bits 23:16 |
| `0x2140-0x2160` | `BNR_COLOR_G[0..8]` | Same packed format |
| `0x2180-0x21A0` | `BNR_COLOR_B[0..8]` | Same packed format |
| `0x2A00-0x2A1C` | `NR2D_DIFFERENCE[0..31]` | 32 byte entries packed four per word |
| `0x2A40-0x2A5C` | `NR2D_WEIGHT[0..31]` | 32 five-bit entries packed four per word |

The packed BNR spatial layout uses two words per 5-element row: columns 0
through 3 occupy the four bytes of the first word and column 4 occupies the
low byte of the second word.

The CCM reset coefficients, in matrix order, are:

```text
 2966  -1687   -255
 -663   2312   -625
 -104  -1049   2177
```

### OECF indexed LUT

The previous mode-2/mode-3 address spaces do not fit the corrected 64 KiB
map. OECF therefore uses one indexed interface while preserving all mode-0
scalar offsets:

| Offset | Name | Access | Fields |
| ---: | --- | --- | --- |
| `0x2C00` | `OECF_LUT_CONTROL` | RW-I | bits 1:0 channel; bit 8 auto-increment |
| `0x2C04` | `OECF_LUT_INDEX` | RW-I | Entry `0 .. (1<<BITS)-1` |
| `0x2C08` | `OECF_LUT_DATA` | RW-F | Low `BITS` bits of selected entry |

Channel values are 0 R, 1 Gr, 2 Gb, and 3 B. With auto-increment enabled, a
successful data read or write advances the index modulo the LUT size.

### Sole VIP scalar registers

These retain the former first-VIP offsets, but the public ABI names the block
`VIP`, never `VIP1`.

| Offset | Name | Access | Reset |
| ---: | --- | --- | ---: |
| `0x4000` | `VIP_RESET` | W1S | `0` |
| `0x4004` | `VIP_WIDTH` | RO | `WIDTH` |
| `0x4008` | `VIP_HEIGHT` | RO | `HEIGHT` |
| `0x400C` | `VIP_BITS` | RO | `8` |
| `0x4040` | `VIP_TOP_ENABLE` | RW-F | `0x1F` |
| `0x4044` | `VIP_INTERRUPT_STATUS` | W1C | `0` |
| `0x4048` | `VIP_INTERRUPT_MASK` | RW-I | `0x3` |
| `0x4200` | `VIP_RGB_CONVERSION_STANDARD` | RW-F | `2` |
| `0x4400` | `VIP_IRC_X` | RW-F | `16` |
| `0x4404` | `VIP_IRC_Y` | RW-F | `30` |
| `0x4408` | `VIP_IRC_OUTPUT` | RW-F | `1` |
| `0x4600` | `VIP_SCALE_INPUT_CROP_WIDTH` | RW-F | `WIDTH` |
| `0x4604` | `VIP_SCALE_INPUT_CROP_HEIGHT` | RW-F | `HEIGHT` |
| `0x4608` | `VIP_SCALE_OUTPUT_CROP_WIDTH` | RW-F | `WIDTH` |
| `0x460C` | `VIP_SCALE_OUTPUT_CROP_HEIGHT` | RW-F | `HEIGHT` |
| `0x4610` | `VIP_SCALE_DOWNSCALE_WIDTH` | RW-F | `1` |
| `0x4614` | `VIP_SCALE_DOWNSCALE_HEIGHT` | RW-F | `1` |
| `0x4800` | `VIP_OSD_X` | RW-F | `16` |
| `0x4804` | `VIP_OSD_Y` | RW-F | `16` |
| `0x4808` | `VIP_OSD_WIDTH` | RW-F | `128` |
| `0x480C` | `VIP_OSD_HEIGHT` | RW-F | `64` |
| `0x4810` | `VIP_OSD_FOREGROUND_COLOR` | RW-F | `0x0000FF` |
| `0x4814` | `VIP_OSD_BACKGROUND_COLOR` | RW-F | `0xFFFFFF` |
| `0x4818` | `VIP_OSD_ALPHA` | RW-F | `50` |
| `0x4A00` | `VIP_OUTPUT_FORMAT` | RO | `0` = I420 |

The preserved `VIP_TOP_ENABLE` bit meanings are:

| Bit | Preserved RTL name | Current SystemC behavior |
| ---: | --- | --- |
| 0 | YUV-to-RGB | Stored for ABI compatibility |
| 1 | IRC/crop | Stored for ABI compatibility |
| 2 | Downscale | Stored for ABI compatibility |
| 3 | OSD | Stored for ABI compatibility |
| 4 | YUV output stage | Enables the sole I420 VIP; must be one for a job |

VIP interrupt bit 0 is frame start and bit 1 is frame done. One in the mask
register disables the corresponding source.

### LUT/RAM windows and reserved regions

| Range | Access | Meaning |
| ---: | --- | --- |
| `0x8000-0xBFFF` | RW-F | Gamma LUT window |
| `0xC000-0xDFFF` | RW-F | Sole-VIP OSD RAM window |
| `0x6000...` former VIP2 scalars | Unmapped | Deliberately removed |
| `0xE000-0xFFFF` former VIP2 OSD | Unmapped | Deliberately removed |

Gamma index is `((address - 0x8000) / 4) % (1<<BITS)`. The fixed window is
therefore exact for 12-bit builds and mirrored for smaller LUT depths.

The VIP OSD RAM contains 512 32-bit words. Its canonical 2 KiB contents mirror
throughout `0xC000-0xDFFF`.

OECF and Gamma LUTs are initialized as identity tables when the bank is
constructed. OSD RAM is initialized to zero. A later software/reset-pin reset
resets scalar state but intentionally does not clear these potentially large
memories.

`ISP_RESET` and `VIP_RESET` are register-bank resets: they do not abort a
running job, reset block pipelines, clear performance counters, or reset VIP
pixel coordinates. The external pixel reset is required for that state.
Similarly, AXI `aresetn` resets the AXI adapter and shared bank, not the
pixel-side pipeline.

## Source and destination memory contract

The ISP owns a generic blocking TLM initiator. It issues the programmed
physical addresses unchanged; translation and routing belong to the virtual
platform. Bind `memory_socket` through the platform bus/interconnect to
`memory_tlm`.

The ISP does not embed `dma_tlm`. A separate DMA is unnecessary for the
approved descriptor-driven model because the ISP is already the active memory
master. A platform DMA can still coexist for other devices without changing
this ISP interface.

### Source modes

| `SOURCE_MODE` | `DIRECT_RGB_INPUT` | Source |
| ---: | ---: | --- |
| 0 | 0 | External RAW pins |
| 0 | 1 | External direct-RGB pins |
| 1 | 0 | Descriptor-driven RAW memory |
| 1 | 1 | Unsupported; job terminates with error 2 |

Memory input is currently validated for a 12-bit RGGB fixture:

- one little-endian 16-bit word per pixel;
- sample in bits 11:0;
- bits 15:12 ignored;
- source address and stride are 2-byte aligned;
- stride is at least `width * 2`;
- required span is `(height - 1) * stride + width * 2`;
- one blocking read transaction is issued per active source row.

Other compile-time Bayer orders remain available to the external RAW pin
stream, but memory-source jobs reject them until their memory-format contract
is explicitly defined.

No input filename is embedded in the model. The test or virtual platform
loads bytes into memory and programs the descriptor.

### I420 output

Output is always planar 8-bit I420:

```text
Y size = width * height
U size = (width / 2) * (height / 2)
V size = (width / 2) * (height / 2)
```

The elaborated top requires even, nonzero width and height. Each destination
base is 4-byte aligned. Each stride must be at least its active row width, each
declared size must cover the used span, and the used Y/U/V ranges must not
overlap. Padding bytes are preserved. The memory master issues one write per
plane row. Writes are not transactional: if a later row or plane receives a
transport error, earlier successful writes remain in memory.

On pins, `out_y` is meaningful for every active `out_href` pixel. `out_u` and
`out_v` are meaningful only at even X and even Y, the upper-left pixel of each
2x2 block; they are zero on all other active pixels. Chroma uses upper-left
decimation rather than 2x2 averaging.

The memory path captures those sparse samples and writes dense planar Y, U,
and V buffers. These shared-memory planes are the intended handoff point to a
later VPU/NPU virtual-platform flow; no VPU/NPU model is changed here.

## Recommended job sequence

1. Keep both interrupt masks at all ones while polling, or W1C stale status
   and unmask only required sources.
2. Program `ISP_TOP_ENABLE`, CSC standard, all source/destination descriptors,
   and any tuning controls.
3. Ensure `VIP_TOP_ENABLE[4]` is one.
4. W1C `JOB_STATUS.DONE|ERROR`.
5. Write the persistent source fields and `START=1` in `ISP_JOB_CONTROL`.
6. Poll BUSY/DONE or wait for `irq`.
7. If ERROR is set, read `ISP_JOB_ERROR_CODE`.
8. Read the functional byte/transaction counters if useful.
9. W1C status and interrupt bits before the next job.

There is no model-internal watchdog. A stream job remains BUSY if the platform
never supplies a correctly framed next input frame; timeout policy belongs to
the virtual platform or software.

For the current deterministic RAW baseline, use:

```text
ISP_TOP_ENABLE = 0x00000900  # Demosaic + CSC
CSC_CONVERSION_STANDARD = 2 # BT.601 full range
VIP_TOP_ENABLE[4] = 1
ISP_OUTPUT_FORMAT = 0       # fixed I420
```

## Build, run, and validation guide

This section describes the current CMake targets under `systemc/tests/`. It is
also the reference for what each test measures and, equally importantly, what
it does not measure.

The supported build system is CMake; there is no supported Makefile in this
directory. SystemC 2.3.4, CMake 3.21 or newer, and a C++17 compiler are
required. Run all commands below from the CDC-VP repository root.

### CMake builds; CTest runs

CMake and CTest have separate jobs:

- `cmake -S ... -B ...` configures a build directory and registers tests.
- `cmake --build ...` compiles libraries and test executables.
- `ctest --test-dir ...` runs tests already registered in that build.
- CTest does not compile a missing or stale executable before running it.
- `--output-on-failure` prints a test's stdout/stderr only when it fails.
- `--verbose` (or `-V`) also prints the command and output for a passing test.
- The elapsed seconds in the normal CTest summary are host wall-clock runtime,
  not simulated hardware time.

For each of the seven short tests, the CMake build-target name, executable
name, and CTest name are identical. `isp_top_job_test` is built from
`tests/isp_top_memory_job_test.cpp`; the shorter target name is intentional.

### Test and metric vocabulary

The suite uses several kinds of observations. They must not be mixed together
when interpreting architecture results.

| Observation | Meaning | Hardware metric? |
| --- | --- | --- |
| Expected register value, response code, or pixel | Functional correctness assertion | No |
| `sc_time` or pclk-cycle delta between modeled events | Simulated timing for the stated boundary | Model-derived only; not measured silicon |
| Read/write bytes and transaction counts | Functional TLM traffic accounting | Workload metric, not bandwidth |
| CTest `0.00 sec` or `34.8 sec` | Host time needed to execute the simulator | No |
| FNV output hash | Reproducible output fingerprint | No image-quality score |
| LUT/FF/DSP/BRAM constants in block collectors | Hand-written estimator hints | No; not synthesis results |
| Generic block power formula | Uncalibrated estimate using fixed technology constants | No; not sign-off power |

The current canonical tests do not report synthesized area, achievable Fmax,
DDR bandwidth, bus utilization, calibrated power, PSNR, SSIM, or Delta-E.

### Standalone configuration profiles

Set a build-directory variable once if desired:

```sh
ISP_SYSTEMC_BUILD=/tmp/isp-systemc-build
```

#### Seven-test short suite

This profile explicitly leaves the long ColorChecker test unregistered:

```sh
cmake -S components/isp_tlm/systemc \
      -B "$ISP_SYSTEMC_BUILD" \
      -DSYSTEMC_HOME=/opt/systemc-2.3.4 \
      -DISP_SYSTEMC_BUILD_TESTS=ON \
      -DISP_SYSTEMC_RUN_COLORCHECKER_TEST=OFF
```

Build and run the registered suite:

```sh
cmake --build "$ISP_SYSTEMC_BUILD" --parallel
ctest --test-dir "$ISP_SYSTEMC_BUILD" --output-on-failure
```

The convenience target performs both steps for the registered tests:

```sh
cmake --build "$ISP_SYSTEMC_BUILD" \
      --target isp_systemc_check \
      --parallel
```

With this configuration, `isp_systemc_check` contains seven tests.

#### Eight-test suite including ColorChecker

The repository contains the expected fixture. Register it explicitly because
the test is long and requires a file selected at configure time:

```sh
cmake -S components/isp_tlm/systemc \
      -B "$ISP_SYSTEMC_BUILD" \
      -DSYSTEMC_HOME=/opt/systemc-2.3.4 \
      -DISP_SYSTEMC_BUILD_TESTS=ON \
      -DISP_SYSTEMC_RUN_COLORCHECKER_TEST=ON \
      -DISP_COLORCHECKER_RAW="$PWD/components/isp_tlm/input/ColorChecker_2592x1536_12bits_RGGB.raw"
```

The RAW path must exist when CMake configures the build. After this command,
`isp_systemc_check` builds and runs eight tests, including ColorChecker:

```sh
cmake --build "$ISP_SYSTEMC_BUILD" \
      --target isp_systemc_check \
      --parallel
```

#### Library-only build

Use this when consuming the model without any test executable:

```sh
cmake -S components/isp_tlm/systemc \
      -B "$ISP_SYSTEMC_BUILD" \
      -DSYSTEMC_HOME=/opt/systemc-2.3.4 \
      -DISP_SYSTEMC_BUILD_TESTS=OFF
cmake --build "$ISP_SYSTEMC_BUILD" --parallel
```

There are no per-test CMake enable switches for the seven short tests.
`ISP_SYSTEMC_BUILD_TESTS=ON` creates all of them; select an individual test by
building its target and using an exact CTest regular expression.

### CMake options and targets

| Name | Default | Purpose |
| --- | --- | --- |
| `SYSTEMC_HOME` | `/opt/systemc-2.3.4` | SystemC installation prefix |
| `ISP_SYSTEMC_BUILD_TESTS` | Parent `CDC_BUILD_TESTS`, otherwise ON | Create all short tests and the ColorChecker executable |
| `ISP_SYSTEMC_RUN_COLORCHECKER_TEST` | OFF | Register ColorChecker with CTest and add it to `isp_systemc_check` |
| `ISP_COLORCHECKER_RAW` | Empty | RAW fixture passed to ColorChecker by CTest |
| `isp_systemc_model` | — | Reusable static model library |
| `cdc::components::isp_systemc_model` | — | Alias for the reusable model target |
| `isp_systemc_check` | — | Build and run every currently registered ISP test |

`ISP_SYSTEMC_RUN_COLORCHECKER_TEST` controls registration, not creation of the
executable. When tests are enabled but ColorChecker registration is off, this
still works:

```sh
cmake --build "$ISP_SYSTEMC_BUILD" \
      --target isp_colorchecker_model_test \
      --parallel
"$ISP_SYSTEMC_BUILD/isp_colorchecker_model_test" \
  "$PWD/components/isp_tlm/input/ColorChecker_2592x1536_12bits_RGGB.raw"
```

However, `ctest -R '^isp_colorchecker_model_test$'` finds nothing until the
registration option is on and CMake has been rerun.

### Inspecting cached configuration and registered tests

CMake options are cached. Reusing a build directory does not automatically
change an earlier `OFF` value. Inspect the cache and CTest registry with:

```sh
cmake -LAH -N "$ISP_SYSTEMC_BUILD" | \
  rg '^(ISP_SYSTEMC_|ISP_COLORCHECKER_RAW|SYSTEMC_HOME)'
ctest --test-dir "$ISP_SYSTEMC_BUILD" -N
```

The short profile lists seven tests. The full profile also lists:

```text
isp_colorchecker_model_test
```

ColorChecker has the `long` and `colorchecker` labels:

```sh
ctest --test-dir "$ISP_SYSTEMC_BUILD" -L colorchecker --verbose
ctest --test-dir "$ISP_SYSTEMC_BUILD" -L long --verbose
```

### Running every test individually

The following commands build each exact executable and run only its matching
CTest entry. Use `--output-on-failure` instead of `--verbose` for compact CI
output.

```sh
cmake --build "$ISP_SYSTEMC_BUILD" --target isp_register_bank_test --parallel
ctest --test-dir "$ISP_SYSTEMC_BUILD" -R '^isp_register_bank_test$' --verbose

cmake --build "$ISP_SYSTEMC_BUILD" --target isp_vip_i420_test --parallel
ctest --test-dir "$ISP_SYSTEMC_BUILD" -R '^isp_vip_i420_test$' --verbose

cmake --build "$ISP_SYSTEMC_BUILD" --target isp_frame_memory_test --parallel
ctest --test-dir "$ISP_SYSTEMC_BUILD" -R '^isp_frame_memory_test$' --verbose

cmake --build "$ISP_SYSTEMC_BUILD" --target isp_control_target_test --parallel
ctest --test-dir "$ISP_SYSTEMC_BUILD" -R '^isp_control_target_test$' --verbose

cmake --build "$ISP_SYSTEMC_BUILD" --target isp_axi_lite_adapter_test --parallel
ctest --test-dir "$ISP_SYSTEMC_BUILD" -R '^isp_axi_lite_adapter_test$' --verbose

cmake --build "$ISP_SYSTEMC_BUILD" --target isp_csc_test --parallel
ctest --test-dir "$ISP_SYSTEMC_BUILD" -R '^isp_csc_test$' --verbose

cmake --build "$ISP_SYSTEMC_BUILD" --target isp_top_job_test --parallel
ctest --test-dir "$ISP_SYSTEMC_BUILD" -R '^isp_top_job_test$' --verbose

cmake --build "$ISP_SYSTEMC_BUILD" --target isp_colorchecker_model_test --parallel
ctest --test-dir "$ISP_SYSTEMC_BUILD" \
      -R '^isp_colorchecker_model_test$' \
      --verbose
```

All short executables take no arguments and can also be invoked directly, for
example:

```sh
"$ISP_SYSTEMC_BUILD/isp_top_job_test"
```

ColorChecker requires exactly one RAW path argument.

### Test-suite summary

| Test | Level and system under test | Observations reported or asserted |
| --- | --- | --- |
| `isp_register_bank_test` | Pure C++ unit test of `IspRegisterBank` | Register values, access results, status/counter semantics; no timing metrics |
| `isp_vip_i420_test` | VIP buffer helpers and clocked VIP pins | Exact Y/U/V values, packing, padding, validation; no metrics collector |
| `isp_frame_memory_test` | `IspFrameMemory` TLM initiator | Functional bytes, issued transactions, errors, and stub-annotated delay |
| `isp_control_target_test` | Fast control-TLM target | TLM response mapping, annotated delay, TLM error count |
| `isp_axi_lite_adapter_test` | Pin-level AXI4-Lite adapter | Handshake/response behavior and shared-bank visibility; no summary metric |
| `isp_csc_test` | Clocked 12-bit RGB-to-YUV CSC block | Exact samples and pipeline drain; internal collector exists but is not read by the test |
| `isp_top_job_test` | Small integrated `isp_top<12,RGGB,8,4>` | Job traffic, exact/direct output, frame count, nonzero frame timing |
| `isp_colorchecker_model_test` | Full integrated `isp_top<12,RGGB,2592,1536>` | Full-frame traffic, nontrivial planes, hashes, cycles, simulated time, FPS |

## Detailed test reference

### `isp_register_bank_test`

Source: `tests/isp_register_bank_test.cpp`

Purpose: verify the canonical register storage without a SystemC clock or a
bus frontend. The test directly calls:

```text
IspRegisterBank::write()       -> update state according to policy/strobes
IspRegisterBank::read()        -> read software-pending state
IspRegisterBank::read_active() -> read pipeline-visible state
IspRegisterBank::commit_frame()-> atomically publish shadowed state
```

Covered scenarios:

1. Identification and decode:
   - default 2592x1536, 12-bit RGGB identification;
   - fixed ISP/VIP I420 format;
   - read-only identification writes;
   - misaligned, invalid-strobe, out-of-range, reserved, and removed-VIP2
     address handling;
   - discoverable RW, W1C, mixed, and unmapped policies.
2. Shadowing and byte strobes:
   - pending DPC configuration differs from the active reset value until
     `commit_frame()`;
   - a lane-one strobe changes `0x11223344` to `0x1122AA44`;
   - a zero strobe is a valid no-op;
   - scalar and descriptor state commit atomically.
3. Jobs, interrupts, and hardware-owned values:
   - START self-clears, sets BUSY, and creates one consumable request;
   - a second START while BUSY latches ERROR and `COMMAND_WHILE_BUSY`;
   - a synthetic `JobStatistics` object injects all four traffic counters; the
     test explicitly checks the published read-byte field and completion state;
   - DONE/ERROR and interrupt status implement W1C;
   - mask/unmask behavior drives core and sole-VIP IRQ levels;
   - hardware can update AE status but cannot change structural IDs;
   - the dedicated TLM error counter increments.
4. OECF indexed LUT:
   - identity reset values, channel selection, auto-increment, bit masking,
     invalid index rejection, and frame-shadowed activation;
   - software reset preserves the OECF SRAM contents.
5. Aliased windows:
   - a 10-bit Gamma LUT mirrors after its canonical `2^10` entries;
   - sole-VIP OSD RAM mirrors every `0x800` bytes;
   - both memories remain frame-shadowed;
   - former VIP2 OSD space remains unmapped.
6. Compile-time ABI guards preserve important register offsets.

Metrics: none. The injected `4096/6144` bytes and `16/24` transactions are
test constants used to validate register publication; they are not traffic
measured from a running pipeline. The failure count and CTest runtime are also
not hardware metrics.

### `isp_vip_i420_test`

Source: `tests/vip_i420_test.cpp`

Purpose: verify pure buffer-conversion helpers and the clocked sole-VIP pin
interface. Chroma is upper-left decimation: U/V are sampled at even X and even
Y, never averaged over a 2x2 block.

Covered scenarios:

1. A padded 4x2 `VIP<10>` RGB buffer contains red, green, blue, white, yellow,
   cyan, magenta, and black. `convert_i420_contiguous()` must produce exact
   BT.601 planes:

   ```text
   Y = 77,149,29,255,226,178,106,0
   U = 85,255
   V = 255,107
   ```

2. Strided `convert_i420()` preserves Y/U/V row-padding sentinels and uses the
   same upper-left U/V samples.
3. BT.709 red converts to Y/U/V `54/99/255`; reserved standards 0 and 3 are
   rejected in RGB mode.
4. Odd I420 dimensions, undersized output, and input samples wider than BITS
   are rejected before any partial output write.
5. `pack_yuv444_i420()` copies active Y, sparsely samples U/V, preserves all
   padding, and rejects an undersized input without modifying output planes.
6. The pin test checks reset, one-cycle VSYNC sampling, BT.601 and BT.709 RGB,
   odd-X/odd-Y zero chroma, reserved-standard error behavior, low-eight-bit YUV
   bypass, and disabled-VIP suppression.

Metrics: none. Exact pixels and padding are functional golden observations.
The 10 ns pclk schedules pin behavior, but the test does not calculate cycles,
latency, FPS, traffic, area, power, or image-quality scores.

### `isp_frame_memory_test`

Source: `tests/test_isp_frame_memory.cpp`

Purpose: verify `IspFrameMemory` as a blocking TLM initiator. The in-test
`PhysicalMemoryStub` records command/address/length, copies data, can inject a
failure at one address, and adds 2 ns to every transaction's annotated delay.
`IspFrameMemory` waits that delay from its SystemC process.

Covered RAW scenarios:

1. A 3x2 RAW12 descriptor has an 8-byte stride and 6 active bytes per row.
   Two reads decode LE16 words while ignoring bits 15:12. The result is:

   ```text
   0x123,0xabc,0xfff,0x000,0x001,0x456
   ```

2. The successful read checks exactly two row transactions, 12 successful
   bytes, exact physical addresses, and 4 ns of consumed stub delay.
3. Misaligned address, odd stride, too-small stride, too-small declared size,
   and address overflow are rejected before any TLM access. Caller output is
   preserved.
4. A row-one target failure reports plane, row, address, and TLM response. Two
   requests were issued but only six bytes succeeded; partial decoded output
   is not published.

Covered I420 scenarios:

1. Odd 3x3 is supported by this helper, with 3x3 Y and ceil-divided 2x2 Cb/Cr.
   It issues 3+2+2 row writes, reports 17 successful bytes and 14 ns of stub
   delay, uses exact plane strides, and preserves destination padding.
2. Misaligned plane address, too-small stride/size, overlapping used ranges,
   address overflow, and tightly-packed source-size mismatch are rejected
   before any write.
3. A failure on the first Cb row occurs after all three Y rows. Four writes
   were issued, nine bytes succeeded, later Cb/Cr writes stop, and successful
   earlier Y writes remain. Output writes are deliberately not transactional.

Metrics and accounting API:

```cpp
const MemoryIoCounters& counters = frame_memory.counters();
```

`read_transactions` and `write_transactions` increment before dispatch, so a
failed issued request is counted. Byte counters increment only after
`TLM_OK_RESPONSE`. `errors` counts validation and transport errors. These are
functional payload/request counters, not DDR bursts, bandwidth, utilization,
or contention. The 2 ns target delay is a test constant, not calibrated memory
latency.

### `isp_control_target_test`

Source: `tests/test_isp_control_target.cpp`

Purpose: verify the fast, loosely timed TLM-2.0 control frontend backed by the
same register bank as AXI4-Lite.

API path:

```text
test transport()
 -> initiator_socket->b_transport(payload, delay)
 -> IspControlTarget::b_transport()
    -> validate command/data/length/streaming/byte-enables/address/alignment
    -> IspRegisterBank::read() or write()
    -> map AccessResult to tlm_response_status
```

Covered scenarios:

1. A full little-endian write with 1 ns initial annotation and 5 ns target
   latency returns 6 ns. TLM sees pending state while active state remains
   unchanged before commit.
2. Single-byte and halfword addressing, repeating byte-enable patterns,
   selective reads, and untouched disabled bytes are verified exactly.
3. `commit_frame()` publishes pending state, and a direct bank write is visible
   through TLM, proving there is no private frontend cache.
4. Rejected RO, reserved, misaligned, cross-word, out-of-range, three-byte,
   short-streaming-width, invalid-byte-enable, unsupported-command, null-data,
   and invalid-value accesses map to the required TLM response. Each adds the
   configured 7 ns annotation and increments `ISP_TLM_ERROR_COUNT` once.
5. A one-byte job-control write verifies START's one-shot request,
   self-clearing behavior, and frame commit of persistent job fields.
6. DMI is never advertised.

Metrics: the test checks annotated delay and the functional TLM error count.
`b_transport()` adds to the delay argument; it does not wait that delay. No
successful-transfer count, cycle latency, bandwidth, FPS, area, or power is
reported.

### `isp_axi_lite_adapter_test`

Source: `tests/test_isp_axi_lite_adapter.cpp`

Purpose: verify the pin-level AXI4-Lite adapter and its coherence with a fast
TLM frontend sharing one `IspRegisterBank`. The test advances a 10 ns clock one
edge at a time.

Signal/API path:

```text
AW/W/AR pin handshake
 -> IspAxiLiteAdapter clocked state
 -> IspRegisterBank::write()/read()
 -> held B/R response

auxiliary control TLM socket
 -> IspControlTarget
 -> the same IspRegisterBank
```

Covered scenarios:

1. AW-first/W-later and W-first/AW-later writes both complete. The independent
   channel slot remains ready until used.
2. One outstanding B response blocks both new write channels and remains
   stable under backpressure.
3. `WSTRB=0x5` changes only byte lanes 0 and 2; AXI writes remain pending until
   the shared frame commit.
4. AXI reads TLM-written pending values. A held R response remains stable even
   when TLM changes the underlying register; a later read sees the new value.
5. RO and mapped-invalid writes return `SLVERR`; reserved, misaligned, and
   out-of-range accesses return `DECERR`.
6. Active-low reset is synchronous: nothing changes between edges, the next
   rising edge clears outstanding responses and resets the shared bank, and a
   release edge restores readiness.
7. One read and one write response can coexist. The adapter does not model
   multiple outstanding reads/writes, AXI IDs, or bursts.

Metrics: none are aggregated. The 10 ns clock is protocol stimulus; the test
does not publish AXI latency, handshake count, throughput, bandwidth, area, or
power. AXI errors do not increment the TLM-specific error counter.

### `isp_csc_test`

Source: `tests/csc_test.cpp`

Purpose: verify the clocked `isp_csc<12>` RGB-to-YUV datapath, 12-to-8-bit
normalization, conversion standards, rounding/clamping, and pipeline drain.

Covered scenarios:

1. BT.601 full-range:

   ```text
   black = 0,128,128       white = 255,128,128
   red   = 77,85,255       green = 149,43,21
   blue  = 29,255,107
   ```

2. BT.709 full-range:

   ```text
   red   = 54,99,255
   green = 182,29,12
   blue  = 18,255,116
   ```

3. Reserved standards 0 and 3 produce delayed active zero samples.
4. After input VSYNC ends a frame, the test waits `DLY_CLK + 2` cycles and
   verifies that every expected sample drains in order. It does not assert the
   exact cycle index of every output.

The CSC block has an internal collector API:

```cpp
auto& collector = dut.get_metrics();
const BlockMetrics& common = collector.get_metrics();
const CscMetrics& detail = collector.get_csc_metrics();
```

The current test never reads, asserts, prints, or exports that collector. The
block internally records recognized conversions, pixels, cycles, standard
counts, and three multiplies per converted pixel. Its LUT/FF/DSP/BRAM and
power fields are hard-coded estimator values, not synthesis/P&R data; the
estimator also assumes 200 MHz while this test clock is 100 MHz.

### `isp_top_job_test`

Source: `tests/isp_top_memory_job_test.cpp`

Purpose: exercise the current public `isp_top` boundary with real register
programming, blocking memory TLM, a small 8x4 frame, and both supported source
paths. The test instantiates `isp_top<12, RGGB, 8, 4>`, a 10 ns pclk, a 14 ns
AXI clock, and a 64 KiB TLM memory. AXI pins are bound but programming uses the
fast `control_socket`.

Scenario 1, descriptor-driven RAW memory:

- initialize a deterministic 8x4 RAW12 LE16 gradient;
- enable only Demosaic and CSC, select BT.601, and program source/Y/U/V
  descriptors;
- start `SOURCE_MODE | START` and require DONE within 2,000 pclk cycles;
- require no ERROR, 64 RAW bytes, 48 I420 bytes, four RAW row reads, and eight
  I420 row writes (`4 Y + 2 U + 2 V`);
- require nonzero Y output;
- call `dut.performance()` and require one completed output frame plus nonzero
  cycles, simulated time, and FPS.

Scenario 2, direct-RGB stream:

- clear old status, enable CSC, arm `DIRECT_RGB_INPUT | START`, and verify BUSY
  while the job waits for a frame;
- drive one 8x4 frame with `R=4095, G=0, B=0`;
- require no source-memory read, 48 output bytes, and exact BT.601 planes:
  every Y=77, U=85, and V=255;
- require two completed performance frames and `ISP_FRAME_COUNT=2`.

Scenario 3, invalid stream configuration:

- disable CSC and start another stream job;
- require prompt DONE+ERROR with `UNSUPPORTED_CONFIGURATION` rather than a
  hang;
- require `performance().frames_completed` to remain two because no output
  frame existed;
- require `ISP_FRAME_COUNT=3` because the software-visible count includes
  terminal failed job attempts.

Metrics: this test reads the four functional last-job traffic registers and
the `IspPerformanceSnapshot`. It checks performance values only for nonzero
validity; it does not print or require a fixed cycle/FPS baseline. Its memory
stub has no annotated latency, so no bandwidth/contention is modeled.

### `isp_colorchecker_model_test`

Source: `tests/colorchecker_model_test.cpp`

Purpose: run the current full-resolution architecture model through the same
register and physical-memory interfaces that a virtual platform uses. It is a
long integration test, not merely a file-to-file image utility.

#### Configure and run

Register it with CTest:

```sh
cmake -S components/isp_tlm/systemc \
      -B "$ISP_SYSTEMC_BUILD" \
      -DSYSTEMC_HOME=/opt/systemc-2.3.4 \
      -DISP_SYSTEMC_BUILD_TESTS=ON \
      -DISP_SYSTEMC_RUN_COLORCHECKER_TEST=ON \
      -DISP_COLORCHECKER_RAW="$PWD/components/isp_tlm/input/ColorChecker_2592x1536_12bits_RGGB.raw"
cmake --build "$ISP_SYSTEMC_BUILD" \
      --target isp_colorchecker_model_test \
      --parallel
ctest --test-dir "$ISP_SYSTEMC_BUILD" \
      -R '^isp_colorchecker_model_test$' \
      --verbose
```

Or build and run it directly without CTest registration:

```sh
cmake --build "$ISP_SYSTEMC_BUILD" \
      --target isp_colorchecker_model_test \
      --parallel
"$ISP_SYSTEMC_BUILD/isp_colorchecker_model_test" \
  "$PWD/components/isp_tlm/input/ColorChecker_2592x1536_12bits_RGGB.raw"
```

#### Fixed fixture and expected accounting

| Item | Value |
| --- | ---: |
| Dimensions | 2592 x 1536 |
| Pixels | 3,981,312 |
| Storage | RAW12 RGGB, one little-endian 16-bit word per pixel |
| RAW active row | 5,184 bytes |
| RAW file/read bytes | 7,962,624 |
| Y bytes | 3,981,312 |
| U bytes | 995,328 |
| V bytes | 995,328 |
| Total I420 write bytes | 5,971,968 |
| RAW read transactions | 1,536 |
| I420 write transactions | 1,536 Y + 768 U + 768 V = 3,072 |
| Bundled fixture SHA-256 | `c30268dbaee34fc2f1a7ecfceef8fe84bef11b91270642cfa9b11cd8a5f0842c` |

The loader verifies file open/read success and the exact 7,962,624-byte size.
It does not calculate or assert the SHA-256; the table records the known
bundled fixture identity.

`FixtureMemory` exposes four non-overlapping TLM regions:

```text
RAW  0x10000000
Y    0x20000000
U    0x21000000
V    0x22000000
```

The test uses the deterministic minimal profile: Demosaic and CSC enabled,
BT.601 full-range selected, descriptor-driven RAW source, and the sole VIP's
reset-default I420 enable.

#### Exact API and execution flow

```text
CTest
 -> isp_colorchecker_model_test <raw-path>
    -> sc_main(argc, argv)
       -> construct ColorCheckerTester
          -> FixtureMemory loads RAW and allocates Y/U/V
          -> control.bind(dut.control_socket)
          -> dut.memory_socket.bind(memory.socket)
          -> register SC_THREAD(run)
       -> sc_start()
```

`ColorCheckerTester::run()` initializes unused pins, holds pixel and AXI reset
low for two pclk cycles, releases reset, and calls `program_job()`.

Every register write follows this API chain:

```text
ColorCheckerTester::write_register(address, value)
 -> construct a four-byte little-endian tlm_generic_payload
 -> control->b_transport(transaction, delay)
 -> IspControlTarget::b_transport()
 -> IspRegisterBank::write()
```

Configuration writes change the pending frame-shadowed bank. The final write
of `SOURCE_MODE | START` also sets BUSY, creates a one-shot start request, and
self-clears START while preserving SOURCE_MODE.

On the next pclk, the top consumes that request:

```text
isp_top::control_thread()
 -> IspRegisterBank::consume_start_request()
 -> isp_top::start_requested_job()
    -> register_bank_.commit_frame()
    -> load_active_configuration()
    -> snapshot_descriptors() using active register values
    -> frame_memory_.reset_counters()
    -> validate memory mode, VIP, CSC, format, BITS, and Bayer
    -> notify job_start_event_
```

The worker first fetches the complete RAW frame:

```text
isp_top::job_worker()
 -> IspFrameMemory::read_raw12_frame(source_descriptor, raw_pixels)
    -> validate alignment, stride, size, overflow, and dimensions
    -> for each of 1,536 rows:
       -> IspFrameMemory::transport(TLM_READ, row address, 5,184 bytes)
       -> dut.memory_socket->b_transport()
       -> FixtureMemory::b_transport()
       -> decode LE16 and mask each word with 0x0fff
```

After every RAW row has been read, `drive_memory_frame()` turns the decoded
vector into a pin-like pclk stream:

```text
register_memory_frame_start()
 -> store timing start and input sequence
 -> VSYNC low
 -> for each row:
    -> HREF high for 2,592 pixels
    -> HREF low for one pclk
 -> VSYNC high
```

The active datapath is:

```text
memory source mux -> Demosaic -> BT.601 CSC -> VIP -> sparse I420 pins
```

`output_monitor()` samples the internal VIP output every pclk:

- output VSYNC falling starts the matching capture;
- active HREF stores every Y sample;
- U and V are stored only at even X and even Y;
- output VSYNC rising records frame timing and completes capture;
- capture completion requires exactly `width*height` Y and one quarter as many
  U and V samples.

The worker wakes after capture and writes dense planar I420:

```text
write_captured_frame()
 -> IspFrameMemory::write_i420_frame(Y, U, V)
    -> validate vector sizes and plane descriptors
    -> reject overlapping used ranges
    -> one TLM_WRITE per active plane row
 -> finish_job()
    -> copy MemoryIoCounters into JobStatistics
    -> IspRegisterBank::complete_job()
       -> clear BUSY, set DONE, publish LAST_JOB counters
       -> increment frame count and raise job-done status
```

While this runs, the test polls `ISP_JOB_STATUS` every 4,096 pclk iterations.
The timeout is `width*height + height + 100000 = 4,082,848` pclk cycles. Every
poll uses the reverse control path:

```text
read_register(address)
 -> control->b_transport()
 -> IspControlTarget::b_transport()
 -> IspRegisterBank::read()
```

#### Metric mechanism 1: functional memory accounting

The test reads these hardware-visible registers:

```cpp
read_register(reg::kIspLastJobReadBytes);
read_register(reg::kIspLastJobWriteBytes);
read_register(reg::kIspLastJobReadTransactions);
read_register(reg::kIspLastJobWriteTransactions);
```

They originate in `IspFrameMemory::MemoryIoCounters`. Transaction counts
increment when a row request is issued. Byte counts increment only after a
successful TLM response. They count active payload, not stride padding.

The `FixtureMemory` target adds no delay and models no arbitration, bursts, or
contention. These values therefore describe workload and functional traffic;
they are not DDR bandwidth, bus utilization, or memory latency.

#### Metric mechanism 2: output-frame timing API

The test reads timing with a direct C++ API, not a register or TLM request:

```cpp
const IspPerformanceSnapshot metrics = dut.performance();
```

The returned fields are:

| Field | Definition |
| --- | --- |
| `pclk_cycles` | pclk edges counted by `output_monitor()` since pixel reset release |
| `frames_completed` | input sequences matched to completed VIP output frames |
| `last_frame_cycles` | current pclk count minus the count saved at input frame start |
| `last_frame_time` | current `sc_time_stamp()` minus input-frame-start time |
| `last_frame_fps` | `1.0 / last_frame_time.to_seconds()` |

For a memory job, `register_memory_frame_start()` records the start only after
the entire RAW frame has already been fetched. `record_completed_frame()`
records the end when VIP output VSYNC rises. Destination I420 writes begin
after that event.

Therefore the timing includes pixel streaming, one input blank pclk per row,
pipeline latency, and output framing. It excludes:

- source RAW memory reads;
- destination I420 memory writes;
- register-programming and polling time;
- AXI-control latency;
- host simulator runtime;
- memory contention or bandwidth.

It is pipeline/frame latency, not total job latency.

A validation run with the bundled fixture produced:

```text
3,982,988 pclk cycles
39,829,880 ns at a 10 ns pclk
25.1068 FPS = 1 / 0.039829880 seconds
```

That validation run took about 35 seconds of host time. Host runtime varies by
machine and is unrelated to the 39.82988 ms of simulated frame time.

#### Metric mechanism 3: output fingerprints and content checks

The test calls `fnv1a64()` directly on the dense destination vectors:

```cpp
fnv1a64(memory.y);
fnv1a64(memory.u);
fnv1a64(memory.v);
```

The same validation run printed these fingerprints:

```text
Y = d17fb01c38aeaa52
U = 38a520dea0d87546
V = dcb828c9b66e5e74
```

They are printed but not compared against constants. The actual assertions
only require each plane to be nontrivial (`min != max`), plus correct transfer
counts and successful completion. The test does not calculate PSNR, SSIM,
Delta-E, ColorChecker patch error, or RTL bit accuracy.

#### Per-block collectors: available but not reported here

`isp_top` exposes collectors for 14 blocks. For example:

```cpp
auto& csc_collector = dut.get_csc_metrics();
const BlockMetrics& csc_common = csc_collector.get_metrics();
const CscMetrics& csc_detail = csc_collector.get_csc_metrics();

csc_collector.print_summary();
std::string csc_json = csc_collector.to_json();
```

Similar getters exist for Crop, DPC, BLC, OECF, BNR, WB, Demosaic, CCM,
Gamma, Sharpen, 2DNR, AWB, and AE. ColorChecker currently calls none of these,
does not aggregate them, and does not export a metrics JSON/CSV file.

The enabled Demosaic and CSC blocks still update some internal counters, such
as total/active cycles, pixels, frames, standard/conversion counts, and
operation counts. Not every `BlockMetrics` field is populated by every block.
DGain, LSC, LDCI, VIP, frame memory, control, and AXI integration are absent
from the 14 resource collectors.

Collector resource numbers are fixed hints, and their power formula uses
generic constants including 200 MHz while ColorChecker pclk is 100 MHz.
Runtime enable bits bypass/gate algorithms; they do not physically remove the
instantiated blocks. These collector values must not be presented as measured
area, Fmax, or power without RTL synthesis and calibration.

#### ColorChecker coverage boundary

Covered:

- valid full-resolution RAW12 RGGB loading and exact-size contract;
- register-driven memory job startup;
- LE16 row reads and RAW12 masking;
- the minimal Demosaic + BT.601 CSC datapath;
- VIP capture and dense planar I420 writes;
- job completion/error status and exact functional traffic accounting;
- nonuniform output planes, reproducible fingerprints, and frame timing.

Not covered:

- AXI4-Lite handshakes or interrupt servicing;
- invalid descriptors and injected memory transport failures;
- modeled memory delay, arbitration, contention, or bandwidth;
- other Bayer orders, RAW packings, pipeline profiles, or output formats;
- a qualified golden image or image-quality score;
- total job latency including memory I/O;
- synthesized area, Fmax, or calibrated power.

### Parent CDC-VP build

When the repository is configured with `CDC_BUILD_TESTS=ON`, the parent
`components/isp_tlm/CMakeLists.txt` adds this directory. An ISP-focused parent
build can disable unrelated platform executables:

```sh
cmake -S . \
      -B /tmp/cdc-vp-build \
      -DSYSTEMC_HOME=/opt/systemc-2.3.4 \
      -DCDC_BUILD_TESTS=ON \
      -DCDC_BUILD_MINI_TLM=OFF \
      -DCDC_BUILD_CPU_EVAL=OFF \
      -DCDC_BUILD_CUSTOM_SOC=OFF
cmake --build /tmp/cdc-vp-build \
      --target isp_systemc_check \
      --parallel
```

Omit the three `OFF` options when configuring the complete virtual platform.
The ColorChecker registration options may also be passed to the parent CMake
configure command.

### Older files under `systemc/tb/`

The canonical CMake file builds only the sources under `systemc/tests/` listed
above. The files under `systemc/tb/` use the pre-refactor top interface and are
not current CMake targets. They are not required for any canonical test.

Do not rely on a stale executable under `systemc/build/` for architecture
results. Port a legacy bench to the current register-driven `isp_top` and add
an explicit CMake target before treating it as supported. A future dedicated
architecture-report executable should aggregate current collectors, define
its measurement boundaries, and export machine-readable JSON/CSV separately
from the functional CTest pass/fail policy.

## Current limitations

- The model is for architecture exploration; several algorithms are compact
  functional approximations rather than bit-accurate RTL translations.
- LSC and LDCI are explicit passthrough placeholders.
- Crop is currently full-frame with X/Y zero; its width and height registers
  are build-identification values.
- Gamma uses one frame-shadowed LUT broadcast to R, G, and B.
- VIP IRC/crop, scale, OSD scalars, OSD RAM, and RGB-conversion controls are
  retained in the ABI but are not applied by the current sole-I420 VIP.
- The memory-source job is RAW12 in little-endian 16-bit storage. Other RAW
  packing, valid-bit, LSB-shift, and dynamic-format descriptors are deferred.
- Dimensions and Bayer order are compile-time identification values, not
  runtime format changes.
- The software-visible physical addresses are currently 32-bit.
- Register-to-pixel clock crossing is modeled functionally with frame
  shadowing; it is not a gate-level CDC implementation.
- Frame capture validates aggregate Y/U/V sample counts, not each line's
  geometry independently.
- Some 12-bit-oriented reset constants are masked, not numerically rescaled,
  in 8- through 11-bit template builds.
- Output is only I420. NV12, packed YUV, RGB memory output, and a second VIP
  are intentionally absent.
- The canonical CMake tests do not build the older files under `tb/`; those
  legacy benches use the pre-refactor top interface.
- No production SDK driver or generated C register header is delivered yet;
  firmware should derive its definitions from the canonical map header during
  the later SDK handoff.
