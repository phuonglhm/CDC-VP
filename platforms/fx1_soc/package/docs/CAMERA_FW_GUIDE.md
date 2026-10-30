# Camera on VP_FX1_V3.0: CSI-2 RX, D-PHY and the VP camera sensor

`fx1_soc --camera` adds a camera path to the FX1 SoC VP:

```
firmware ──I2C0──► camera sensor model ──D-PHY TX──► D-PHY RX (detailed model)
                                                        │
                       DDR ◄── MIPI_DMA ◄── CSI-2 RX controller ──► PLIC source 7
```

Without `--camera` the platform is unchanged except for I2C0, which is always
present (no device answers on it). `fx1_soc --version` reports whether the
binary contains the camera.

## Map

| Block | Base | IRQ | Header |
|---|---|---|---|
| CSI-2 RX controller (CSR) | `FX1_MIPI_CSR_BASE` 0x1104_0000 | `FX1_IRQ_MIPI` 7 | `include/fx1_csi2/fx1_csi2_regs.h` |
| I2C0 host | `FX1_I2C0_BASE` 0x1003_0000 | `FX1_IRQ_I2C0` 9 | `drivers/camera/fx1_i2c.h` |
| Camera sensor | I2C0, 7-bit address 0x10 | — | `drivers/camera/fx1_ccs_sensor.h` |

All registers take 32-bit accesses. The CSI-2 RX DMA writes DDR as bus master
`MIPI_DMA`; firmware hands it two buffer sides (ping-pong) and gets each one
back with an interrupt.

## Software

| File | What it is |
|---|---|
| `drivers/csi2/fx1_csi2_drv.c`, `include/fx1_csi2/fx1_csi2_drv.h` | reference CSI-2 RX driver (C99, HAL based): probe, D-PHY start, DMA, buffers, interrupts, faults |
| `drivers/camera/fx1_csi2_hal_mmio.[ch]` | its MMIO HAL for this SoC |
| `drivers/camera/fx1_i2c.[ch]` | I2C0 host driver: one transaction per call, waited for by polling within its budget (completion from the controller, or from PLIC source 9 in interrupt mode); at the budget the transaction is aborted and the bus left idle (`FX1_I2C_ETIMEOUT`) |
| `drivers/camera/fx1_ccs_sensor.[ch]` | sensor driver: probe, mode, stream on/off, software reset, frame count, and `fx1_ccs_pattern()`, the pixel formula of the built-in image |
| `camera/` | the camera examples (below) and their shared code `camera.[ch]` |
| `drivers/camera/fx1_camera.[ch]` | camera manager: sensor → CSI-2 RX DMA → ISP → NV12 on top of the drivers above and the ISP driver (below) |
| `drivers/camera/fx1_isp_hal_mmio.[ch]` | MMIO HAL of the ISP driver for this SoC |
| `camera_isp/` | camera → ISP examples (below), `cam_isp.[ch]`, and `camera_isp_golden.h`: the ISP profiles (basic) and the expected NV12 CRCs from the stand-alone ISP model |
| `app/cmd_camera.c` | the FreeRTOS CLI's `camera` commands (below) |

Bring-up order (as `camera/camera.c` does it):

1. I2C0 open; sensor probe (`model_id` 0xF1C5); sensor mode (standby).
2. CSI-2 RX probe; D-PHY start with the sensor's lane count; AXIS path off;
   `EXPECTED_RESOLUTION` = height << 16 | width; DMA configured and started;
   interrupts enabled at the controller and at the PLIC (source 7).
3. Sensor stream on. Each frame ends with `BUF_DONE_OK` (or `BUF_DONE_ERR`)
   on a side; read it, then release the side.
4. Stream off: the frame in progress still completes; then stop the DMA.

## Camera manager (`drivers/camera/fx1_camera.h`)

`probe` → `configure` (sensor mode, D-PHY, CSI-2 DMA in RAW16, ISP geometry,
profile and buffers) → `start` → `poll` / `dequeue` / `release` → `stop`.
The application's trap handler calls `fx1_camera_csi_isr()` for source 7 and
`fx1_camera_isp_isr()` for source 3 (ISP).

- Only CSI-2 buffers that completed without error go to the ISP; an error
  buffer is released at once and counted.
- RAW12 is zero-copy: the manager writes the CSI-2 side's address into
  `IDMA_BUF_ADDR` of the ISP input slot about to be armed, and gives the side
  back when that slot's `IDMA_BUF_VALID` is clear and no IDMA read error is
  pending (a read error also clears VALID, and recovery reads the same input
  again). **Open item:** this register write is outside the ISP driver; it is
  to be confirmed by the ISP owner (or replaced by an ISP driver API).
- RAW10 is staged: each 16-bit sample << 2 (ISP container bits [11:2]) into a
  staging buffer; the CSI-2 side is released after the copy.
- Outputs come back in order with their capture number and CSI-2 FRAME_ID
  (`fx1_camera_frame`). While the application holds an output, the next
  frame does not go to the ISP; with both CSI-2 sides held the receiver drops
  frames (counted).
- ISP DMA errors: an optional `isp_error_hook` lets the application repair the
  cause (`fx1_camera_set_output`), then the manager recovers; lost frames are
  counted.
- `fx1_camera_stop(cam, timeout_us)` returns within `timeout_us +
  FX1_CAMERA_STOP_SLACK_US` (200 µs). It stops the sensor (written and read
  back), the CSI-2 DMA (aborted if the capture in flight does not end) and the
  ISP, and cancels what is left in the pipeline (`stats.stop_cancelled`);
  outputs the application dequeued stay readable until the next configure.
  `FX1_CAMERA_ESENSOR`: the sensor did not confirm standby; the CSI-2 RX and
  the ISP are stopped, the manager is in state 3, call stop again once I2C
  answers (configure is refused until then). `FX1_CAMERA_ETIMEOUT`: the
  CSI-2 DMA did not stop, state unchanged. Stop also cleans up state 1: a
  `fx1_camera_start` that failed (for example the sensor's stream-on write)
  leaves the manager configured, possibly with the CSI-2 DMA started.

## The sensor model

A VP model, not a commercial sensor: a subset of the MIPI CCS registers
(model_id, frame_count, mode_select, software_reset, csi_data_format RAW10 /
RAW12, csi_lane_mode, frame/line length, x/y output size), 16-bit index,
big-endian values, auto-increment. A frame once started is sent whole; a stop
(stream off or software reset) takes effect at its end and the sensor then
stops even if streaming is requested again meanwhile. Frame numbers restart at
1 after a software reset. The clock lane goes to LP-11 between frames.

Pixels come from a built-in image, `(x*37 + y*101 + f*53 + 5) mod 2^bits`
(f = frame number), or from `--camera-raw FILE`: 16-bit little-endian samples,
W x H per frame at the mode firmware sets, repeated. An empty or odd-length
file is refused at start-up; a file that does not hold a whole number of frames
of the mode gives no frame (the sensor stays in standby).

## Examples (`make camera`, `test/VP_FX1_V3.0/run_vp.sh camera`)

All 64x16, frames checked pixel by pixel against `fx1_ccs_pattern()`.

| Example | Shows |
|---|---|
| `camera_raw10` | RAW10, 4 lanes, packed RAW10 in DDR, 3 frames |
| `camera_raw12` | RAW12, 2 lanes, RAW16 right-aligned in DDR, 3 frames |
| `camera_recover` | no free buffer (a frame dropped, capture resumes), sensor software reset mid-stream, DMA stop and restart while streaming |
| `camera_decerr` | a buffer side at an unmapped address: DECERR contained to its frame, recovery by reprogramming |
| `camera_slverr` | a DDR write error (run with `--ddr3-ecc off --ddr3-fault 6200:write-result:1`): SLVERR on frame 2, recovery |
| `camera_i2c_irq` | the I2C driver in interrupt mode |
| `camera_i2c_timeout` | I2C budgets: write and read aborted at their budget, bus recovered at once, both modes |

Camera → ISP (`run_vp.sh camera-isp`): `camera_isp_raw12` (4 lanes,
zero-copy) and `camera_isp_raw10` (2 lanes, staged), eight frames each through
the camera manager; every NV12 must equal the stand-alone ISP model's for its
sensor frame number (`camera_isp_golden.h`).

The FreeRTOS CLI (`run_vp.sh camera-cli`, or interactively with `--camera`;
`help` prints the usage) has:

| Command | Does |
|---|---|
| `camera probe` | CSI-2 RX identity, sensor `model_id`; `camera: absent` without `--camera` |
| `camera mode [W H BITS LANES]` | set or show the mode (default 64 32 12 4, up to 3840x2160) |
| `camera source [sensor\|tpg]` | sensor through the D-PHY, or the CSI-2 RX test pattern generator (fast at any size) |
| `camera start` / `camera stop [US]` | start (takes the ISP: `isp selftest`/`isp run` are refused meanwhile); stop within US (default 24000) + 200 µs, `OK` / `ESENSOR` / `ETIMEOUT` |
| `camera capture N [crc]` | N frames: metadata, RAW spot check, NV12 CRC against the golden (NV12 up to 1 MiB, or with `crc`: a 4K CRC takes about ten minutes of host time); without the CRC, TPG 4K is spot-checked on 32 Y and 32 UV bytes of the golden |
| `camera status`, `camera recover` | counters and state; stop + sensor software reset + configure + start |

Golden: sensor 64x32 RAW12 / RAW10 frames 1..32 since the last sensor reset,
TPG 640x480 and 3840x2160 RAW12. The script `test/VP_FX1_V3.0/cli_camera.txt`
probes, captures 64x32 RAW12 and RAW10 frames against the golden and stops.

`camera_slverr` relies on its timing: the fault time is chosen inside frame
2 for this image (see the comment at the top of `camera/slverr.c`); a changed
image needs a new time.

## Cost

The D-PHY RX is a detailed model (netlist level). A 64x32 frame costs several
seconds of host time; each example takes one to three minutes. Keep camera
images small; a 640x480 frame through this PHY takes minutes.

## Provenance

The D-PHY RX model (`mipirxphy`) was converted to SystemC from RTL, gate-level
netlists and analog behavioural models. Its supplier, owner and original
licence are not confirmed; see `licenses/THIRD_PARTY.md` (provenance pending).
