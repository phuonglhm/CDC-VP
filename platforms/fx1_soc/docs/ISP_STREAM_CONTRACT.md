# FX1 VP — ISP RAW-sequence stream contract (`isp-stream`)

Status: **S1 contract** (2026-11-12; review S1 fixes and S2 decisions 2026-11-13). This page fixes the session contract that
S2 (firmware and host bridge) and S3 (launcher and video) implement. It is a
**VP-only** mechanism: nothing here is an ISP CSR, an ISP interrupt or a SoC
interface. The machine-readable parts are:

| File | Role |
|---|---|
| `components/FX1_Components/fx1_map/include/fx1/fx1_isp_stream.h` | C ABI (the firmware builds C11; the platform C++): DDR layout, blocks, codes |
| `platforms/fx1_soc/demo/isp_stream_prep.py` | session preparation (stdlib only), Python mirror of the ABI, stand-alone reference |
| `platforms/fx1_soc/demo/stream_fixture.py` | synthetic RAW sequences and their profiles (stdlib only) |
| `platforms/fx1_soc/demo/tests/test_stream_prep.py` | CTest `fx1_regression_isp_stream_prep`: ABI = mirror, layout, conversion, refusals, reference |

## 1. Session flow

```text
host   isp_stream_prep.py prepare   manifest / --raw + profile.csrw  ->  SESSION/ (pool, desc.bin, profile.bin, loads.txt)
host   fx1_soc --fw fx1_freertos.elf --load <loads.txt> --isp-stream-spool SPOOL --uart0-rx-file <isp stream start / wait / poweroff>
FW     isp stream start: claim the ISP and SYS_DMA channel 1, configure once, apply the profile once, start
FW     per frame: SYS_DMA pool -> input slot, queue, completion -> publish record -> doorbell
bridge on the doorbell: drain records by seq, append NV12 + metadata to SPOOL, ACK
FW     isp stream wait: until STOPPED or ERROR; status block final -> doorbell
host   encode SPOOL -> output.mp4, verify, summary            (S3)
```

One boot and one ISP session for the whole sequence. The ISP is configured
and started once; the profile is applied once (profile mode: built into bank
0, sealed, applied, as `isp run`); temporal state (GTM curve, 2DNR,
statistics IDs) carries across every frame, every repeat and the list wrap.

## 2. Input

### 2.1 Manifest (version 1)

```json
{
  "version": 1,
  "width": 2688, "height": 1520, "bayer": "BGGR",
  "source_format": "le16_raw10_msb", "source_stride": 5376,
  "input_depth": "raw12",
  "frames": ["raw/frame_000.raw", "raw/frame_001.raw"]
}
```

- **Keys:** every key except `source_stride` (default `2 × width`) is required. Unknown keys are refused.
- **Paths:** relative paths resolve from the manifest's directory. Spaces are allowed. The order is the list order and is never sorted.
- **Geometry:** the driver's contract (`fx1_isp_set_geometry`, MAS v1.07 §6.6.3.4/5), checked before anything is read or written:
  - **Container:** `width`/`height` are even, at most 3842 × 2162 (the phase overscan included).
  - **Active image** after the Bayer phase trim, `(width − (bayer & 1)) & ~1` × `(height − ((bayer >> 1) & 1)) & ~1`: 32..3840 × 32..2160.
  - The smallest container is therefore 32×32 (RGGB), 34×32 (GRBG), 32×34 (GBRG) or 34×34 (BGGR). The test runs each of these on the model.
- **`source_format`:** one of the `tools/raw_fixture.py` formats.

  | Format | Layout |
  |---|---|
  | `le16_raw10_msb` | RAW10 in bits [15:6]: the "Image Source" set and the samples |
  | `le16_raw10_has` | RAW10 in bits [11:2] |
  | `le16_raw12_has` | RAW12 in bits [11:0] |

  Every sample's declared-zero bits are checked. A violation is refused with the exact count and the first sample's position; the check is linear in the frame size.

  This check is **not packing detection**. A right-aligned RAW12 file whose low six bits happen to be zero passes it as `le16_raw10_msb` and comes out 16× darker. `prepare` therefore warns when every sample of an `le16_raw10_msb` source is below `0x1000` (under 64/1023), and prints the format it used.
- **`input_depth`:** `raw12` writes the container normalized to 12 bits, with `COMMON_INPUT_FORMAT.DEPTH = 0`. `raw10` writes it native in bits [9:0] with `DEPTH = 1`, needs a RAW10 source, and the ISP shifts it exactly once. The same scene gives identical NV12 either way: this is checked, frame by frame, on the fixture.
- **Rows:** rows longer than the image (`source_stride`) are cut. Every file must be exactly `source_stride × height` bytes.

### 2.2 Quick form

`--raw A.raw B.raw ...` takes its settings from three places:

- **Profile:** the Bayer phase (`COMMON_BAYER`, required) and the depth (`COMMON_INPUT_FORMAT`; default RAW12).
- **Options, with the samples' layout as default:** the geometry (`--width/--height`, default 2688×1520), `--source-format` (default `le16_raw10_msb`) and `--source-stride` (default `2 × width`).

  The profile does not hold the input geometry: the firmware programs it from the descriptor, as `isp run` does from the job.

  The `le16_raw10_msb` default is the samples' **convention**, not a detection: any other file needs `--source-format`. A defaulted format is printed as such and recorded in `session.json` (`input.source_format_defaulted`).
- **Order:** the order typed.

The resolved manifest is written to the session (`manifest.resolved.json`).

### 2.3 Profile cross-checks

The profile is compiled with the shared codec (`csrw_codec.py`: register,
alias and field syntax, names, hex or decimal). Its writes must agree with the
sequence, or the session is refused naming `file:line`:

- **Bayer:** `COMMON_BAYER` must be written, and must equal the manifest's `bayer`.
- **Depth:** `COMMON_INPUT_FORMAT.DEPTH` must equal `input_depth`. When it is not written, it is 0, i.e. RAW12.
- **Geometry:** `COMMON_FRAME_WIDTH/HEIGHT`, when written, must equal the geometry.
- **Size:** at most 32768 writes, the profile area.

**One read, one version.** `prepare` reads every input file exactly once:

- **Profile:** the bytes read are the snapshot (`profile.csrw`), the compiled `profile.bin`, the cross-checks and the recorded SHA-256. A tuner saving the file during `prepare` or during a run cannot mix two versions, and only the snapshot is used afterwards, by the platform and by the reference.
- **Sources:** each RAW is read once, checked, converted and hashed from the same bytes. The containers wait in memory until written; the pool budget bounds that memory at 254 MiB.

The test rewrites the profile and a RAW right after `prepare` has read them, and checks that the session holds the version read.

### 2.4 Schedule and identity

- **Inputs:** `S` sources and the repeat count `K >= 1` (`--repeat`, default 1).
- **Physical frames:** `N = S × K` by default. `--frames N` overrides it and may end inside a repeat group. `--loop` runs until a stop request and is exclusive with `--frames`.
- **Identity:** frame `n` (0-based ordinal) uses source `(n // K) mod S`, with repeat index `n mod K`.
- **Real frames only:** every frame is a real ISP frame and is captured; nothing is dropped as a warm-up.
- **Per-capture identity:** session ID (64-bit, random per run), ordinal (64-bit), source index, repeat index, driver seq and tag, ingress frame ID, profile ID and timestamps.
- **Queue:** 1, 2 or 4 frames in flight (`--queue`, default 1). The output is independent of the queue depth: the reference equals frame for frame at queue 1, 2 and 4 on the fixture and on D65→A→D65 × 2.

**GTM lag (why `--repeat`):** GTM auto builds a frame's curve from the previous
frame. In a sequence of different scenes, the first frame of a scene is tone
mapped with the previous scene's statistics. S1 measured this on an internal
sequence with the shared profile, as the Y mean of repeat 0 → repeat 1:

| Scene | Y mean, repeat 0 → repeat 1 |
|---|---|
| night | 97.0 → 108.7 |
| basement | 128.1 → 106.8 |

From the second frame of a scene on, the output equals the steady state; for example D65's second frame equals the shipped d65 golden `8da0af67`.

## 3. DDR layout

Every address is a macro of `fx1_isp_stream.h` derived from `fx1_memory_map.h`;
the test checks that every region lies inside its firmware region and that no
two overlap.

| Region | Address | Size | Use |
|---|---|---|---|
| demo job / result / profile | `SHARED + 0x000000` | 1 MiB | `isp run` (unchanged) |
| desc | `SHARED + 0x100000` | 128 B | host → FW, loaded before boot |
| status | `SHARED + 0x100100` | 128 B | FW → host |
| control | `SHARED + 0x100200` | 64 B | host → FW (stop request) |
| records 0..3 | `SHARED + 0x100400` | 4 × 128 B | FW ↔ host, one per output slot |
| profile | `SHARED + 0x110000` | 256 KiB | {offset, value} pairs, ≤ 32768 |
| SYS_DMA chains 0..3 | `SHARED + 0x200000` | 4 × 320 KiB | 16-byte descriptors, one chain per input slot |
| input slots 0..3 | `RAW_BUF + i × 16 MiB` | 16 MiB each | ISP IDMA input (up to 3840×2160 RAW16) |
| output slots 0..3 | `YUV_BUF + i × 16 MiB` | 16 MiB each | CH0 NV12: Y, UV at the next 1 MiB boundary |
| source pool | `0x9020_0000 .. 0xA000_0000` | 254 MiB | immutable preloaded containers, 4 KiB aligned |

- **Pool start:** the pool starts 2 MiB into the test area, after the buffers of `dma test` (TEST_AREA + 0x100000), so that command stays usable.
- **Capacity:** at 2688×1520, the pool holds **32 sources** (8,171,520 B each). Duplicated entries each take a pool slot; use `--repeat` instead.
- **Over budget:** an over-budget session is refused before any RAW is read. The message says how many such frames fit.
- **Budget by bytes:** the pool limit is checked in bytes (4 KiB pitch), never as a fixed number of frames.
- **Shared slots:** the input and output slots overlap the camera's buffers. The ISP owner (`isp_claim`) makes the stream, `camera` and `isp run` mutually exclusive. S2 keeps that claim and reserves SYS_DMA channel 1 for the session.
- **Input slot ownership:** a copy into input slot `i` starts only after the driver has released it (`fx1_isp_input_ready` / `fx1_isp_next_input`, i.e. IDMA has consumed it). The output ACK rule (§5.2) does not replace this condition.
- **Strides:** these are the strides `fx1_isp_run_raw` programs.
  - `in_stride = align16(2 × width)`.
  - `out_stride = align16(active width)`. The resizer only reduces, so the output fits.
- **Pool containers are DDR images.** Rows sit at `in_stride`, so the firmware copies a frame with one linear chain. `fx1_isp_run_raw` reads packed `2 × width` rows and lays them out at the same stride itself. When `2 × width` is not a multiple of 16 (for example the 3842 overscan or a 34-wide container), the reference cuts the padding into temporary packed copies, which it deletes after the run.
- **Limits:** a 3842×2162 container (16,638,752 B, 16,507 commands) fits one slot and one chain.

## 4. Blocks

The field lists are in `fx1_isp_stream.h`. All words are LE `uint32`; 64-bit counts are {lo, hi}.

- **desc** (host, before boot): the geometry, Bayer and depth; the pool layout (base, pitch, count); `repeat`, `frames`, `FLAG_LOOP`, `queue` and `mode` (profile or legacy); the profile count and ID; `out_stride`; and `frame_budget_us` (simulated time per frame, default 2 s). The firmware refuses any field out of range with `ERROR / ERR_DESC` and `detail` = the field index.
- **status** (FW): `state` `IDLE → PREPARED → RUNNING → DRAINING → STOPPED`, or `ERROR`.
  - `stop_reason` is `DONE`, `USER`, `LIMIT` or `ERROR`.
  - `error` and `detail` say what failed.
  - `submitted`, `completed`, `cancelled` and `failed` count the frames.
  - The block also gives the output geometry, the colorimetry (committed before start, fixed for the session), `dma_err`, the ISP counter deltas, `publish_seq` and the elapsed mtime ticks.
- **control** (bridge): `request = STOP` with `reason = USER` (Ctrl+C) or `LIMIT` (spool budget). The session ID must match.
- **record i** (FW publishes, bridge reads): the full identity, the slot's Y/UV addresses, geometry, stride, colorimetry and `status = 0`. The bridge writes `ack_seq` only.

## 5. Protocol

### 5.1 Doorbell

sim-control (`0x1002_0000`, VP-only) gains `FX1_SIM_CTRL_DOORBELL_OFF = 0x8`
(S2: `fx1_memory_map.h`, `sim_control`):

- **write:** any 32-bit value rings the doorbell. The value is only logged.
- **read:** bit 0 is 1 when a host bridge is attached (`--isp-stream-spool`). Bits 31..1 read 0.
- **other offsets:** offsets 0x0 (finisher) and 0x4 (test IRQ lines) are unchanged. Every other offset is still a slave error.

This is a VP-only register, not an ISP CSR. `isp stream start` refuses with
`ERR_HOST` when no bridge is attached: frames nobody captures would be lost
silently.

The records and their `seq` are the source of truth. A doorbell is only a wake-up, and does not correspond to exactly one frame. The S2 tests cover two cases: several notifies coalesced into one wake, and a notify that arrives before the bridge waits.

### 5.2 Publish and ACK

**Session start (firmware):** before the first submit, every record is cleared and initialized: `state = FREE`, `seq = ack_seq = 0`, magic and this session's ID. Publishing starts at `seq = 1`, so the `seq = ack_seq = 0` of a FREE record never stands for a frame the host received.

**Firmware, per completed frame**, collected with `fx1_isp_next_done_meta` and `fx1_isp_output_meta` after the driver's completion. A frame is never published on IDMA_DONE, SOF or a bare IRQ.

1. Write every field of record `slot`, with `state` still FREE.
2. Fence, then write `seq = previous + 1`.
3. Fence, then write `state = PUBLISHED` and update `status.publish_seq`.
4. Ring the doorbell.

**Bridge**, an `SC_THREAD` woken by the doorbell event, with a periodic 1 ms simulated poll as a fallback:

1. Loop: find the record whose `state == PUBLISHED` and `seq == next`.
2. Check its magic, session, `ordinal == next - 1`, slot, address range and geometry.
3. Copy the active Y and UV rows with the DDR debug read (stored bytes, after ECC; not a bus transaction).
4. Append them to the spool and write one `frames.jsonl` line, both written and fsync'd.
5. Only then write `ack_seq = seq` and set `next += 1`. If the spool or metadata write fails, the bridge never ACKs. It requests `STOP` and the run fails as a host error, so a lost frame is never ACKed as captured.

Doorbells may coalesce or arrive before the bridge waits. The bridge never assumes one frame per event: it drains by `seq` until no record matches. Any of the following is a host protocol error, and the run fails: a `seq` that skips, a record that repeats, an identity mismatch, a wrong session ID, or a published record outside its slot.

**Slot reuse (firmware):** output slot `i` is queued again only when both conditions hold:

- the ISP has completed the frame;
- the record still carries this session's ID and `record[i].ack_seq == record[i].seq` for the `seq` it published.

The firmware then sets the record FREE first. "The driver popped DONE" never implies "the host has read it".

**Ordering:** a fence separates the record fields from `seq` and from `state` on publish. On the host side, the bridge reads `state`/`seq` first and the fields after (acquire). On ACK, it finishes the spool write before writing `ack_seq` (release). The firmware reads `ack_seq` before reusing the slot.

**No wrap:** `seq` is 32-bit, while `ordinal` is 64-bit; `ordinal == seq − 1` holds only while `seq` does not wrap. A session therefore never wraps `seq`. Before `seq` would pass `UINT32_MAX` (about 4.29 × 10⁹ frames), the firmware stops with the normal drain and `stop_reason = LIMIT`, and `detail` names the sequence limit.

### 5.3 Copy (SYS_DMA)

- **Channel:** SYS_DMA channel 1 is the stream's; `dma test` keeps channel 0. The single PLIC source 2 is dispatched per channel through `CORE_STATUS`.
- **Commands:** one command moves at most 1023 bytes (`FX1_DMA_MAX_TRANSFER`). A frame is therefore a linked chain of 1008-byte commands, one chain per input slot. The destinations are fixed; the sources are rewritten per frame, which is 8107 commands at 2688×1520.
- **Queueing:** the frame is queued only after `COMMAND_COMPLETE` of the last command. An error or a timeout gives `ERR_COPY`, and a partial input is never queued.
- **Driver order:** `fx1_isp_input_ready()` → `fx1_isp_next_input()` → copy → `fx1_isp_queue_frame_meta()`.
  - `EBUSY`: wait.
  - `EAGAIN` (a recovery restarted the rotation): copy again into the new `next_input`; never queue the old index.

**Measured in S1** (workspace probe, one 2688×1520 RAW16 frame, TEST_AREA → RAW_BUF, both copies byte-identical to the source):

| Copy | Simulated | Wall |
|---|---|---|
| SYS_DMA chain | 78 ms | about 1 s |
| CPU word copy | 960 ms | about 15 s |

### 5.4 Stop, end, errors

**Ending the session.** Every end follows the same drain: no new submit, then wait for the queued frames, then capture and ACK them, then `fx1_isp_stop`, then `status` final (`STOPPED`), then the doorbell. The end can come from:

- **End of schedule:** `stop_reason = DONE`.
- **Stop request** in `control`, polled once per scheduling round: the same drain. `cancelled` is the scheduled frames never submitted. `stop_reason` is `USER` or `LIMIT`.

**Fatal errors.** A copy error, a DMA_ERR other than the end-of-input `IDMA_UNDERRUN`, a missing completion within `frame_budget_us` (simulated time, mtime), or a profile/geometry/start refusal ends the session:

- `state = ERROR`, with `error`/`detail`.
- The frames already published stay valid; `failed` counts the lost ones.
- There is no automatic reset or replay.

**Underrun.** `IDMA_UNDERRUN` at the end of the input (and while the queue runs dry) is reported in `dma_err` but is not an error, as in `fx1_isp_run_raw`.

**Host-side waits.** The bridge's file I/O runs in the SystemC thread. A slow disk costs wall time and never simulated time, so it cannot trigger a firmware timeout. The launcher's watchdog is wall-clock and is reported separately.

**Ctrl+C:**

- **First press:** a graceful stop.
  1. The launcher forwards one SIGINT to `fx1_soc`.
  2. The platform's handler only sets a flag. The bridge, in its SystemC thread, turns the flag into `control.request = STOP, USER`.
  3. The firmware stops submitting, then drains: the frames already submitted are completed, captured and ACKed.
  4. S3 encodes the complete prefix and reports `USER_STOPPED` with the real frame count, never as a full schedule.
  5. If no frame was captured, no MP4 is created (never an empty one), and this is reported.
- **Drain timeout:** the drain is bounded. If the firmware or the bridge makes no progress within the launcher's wall-clock drain timeout, the run fails with a clear message naming the last `seq` and state.
- **Second press:** a hard stop. The spool and the evidence are kept, but no playable video is promised.
- **One request per press:** the launcher runs `fx1_soc` in its own process group and delivers SIGINT itself. One Ctrl+C at the terminal therefore reaches `fx1_soc` once, not once through the launcher and again through the shared process group.

**Spool budget (loop):** the bridge reserves room for the frames in flight. It requests `STOP, LIMIT` before a capture would exceed `--max-spool-mib`.

## 6. Spool and video

**Spool.** `frames.nv12` holds the active bytes of every frame in completion order, Y rows then UV rows, with no row padding. `frames.jsonl` has one line per frame: the identity, byte offset, geometry, colorimetry and SHA-256. Records are appended and fsync'd before the ACK. A torn tail is truncated to the last whole frame.

**Encoding.** The S1 FFmpeg probe (system FFmpeg 7.1.5, libx264) settled the mapping. The input range and matrix **must be declared on the rawvideo input**:

```bash
ffmpeg -f rawvideo -pix_fmt nv12 -color_range pc -colorspace smpte170m -s 1920x1080 -framerate 30 -i frames.nv12 \
       -i labels.srt -map 0:v -map 1:s -c:v libx264 -preset medium -crf 10 -profile:v high \
       -pix_fmt nv12 -color_range pc -colorspace smpte170m -c:s mov_text -movflags +faststart -f mp4 output.tmp.mp4
```

- **Range declaration:** without the input `-color_range`, FFmpeg treats the NV12 as limited range and stretches it to full: (Y − 16) × 255/219. The probe measured Y PSNR of only 28–32 dB and a Y mean shifted by up to 5 levels. With the input declared, CRF 10 gives 43–50 dB and equal means.
- **Flags:** `limited` maps to `tv`, and BT.709 maps to `bt709`. Primaries and transfer stay unspecified rather than being guessed.
- **Labels:** one `mov_text` cue per frame, `#ordinal name r=k/K`.
- **Verification:** `ffprobe` must report the frame count, geometry, fps/duration, `color_range` and `color_space`, and the number of subtitle cues. Only then is `output.tmp.mp4` renamed to `output.mp4`.
- **What is bit-exact:** the bit-exact oracle is the NV12 spool, never the decoded H.264.

FFmpeg is **not shipped** (RPM Fusion build, GPLv3 with x264). The launcher
checks for `ffmpeg`/`ffprobe` with `libx264` before the boot and prints the
AlmaLinux 9 install steps when they are missing.

## 7. Oracle

`isp_stream_prep.py reference --session S --run-raw fx1_isp_run_raw` runs the
stand-alone model on one session: the same containers, the profile snapshot,
the mode (`--profile-mode` by default), the queue depth and the physical frame
count. It writes `reference.json`, the SHA-256 of every frame with its
identity. S2 compares every captured frame's SHA-256 to it, never only the
last frame. When the containers carry row padding (§3), the model gets them
with the padding cut; the image bytes are the same.

The S1 baselines are in the workspace evidence and are not committed. CI recomputes the reference from the model each run instead of storing goldens.

## 8. To be added (S2, S3)

| Stage | Change |
|---|---|
| S2 | `fx1_memory_map.h`: `FX1_SIM_CTRL_DOORBELL_OFF`; `sim_control`: optional doorbell (constructor flag, event and presence bit), unit test |
| S2 | `platforms/fx1_soc/src`: `--isp-stream-spool DIR` (bridge `SC_THREAD`, DDR debug read/write helpers on `Ddr3Subsystem`, SIGINT → control), off by default: existing runs unchanged |
| S2 | `fw/fx1_soc/freertos/cmd_isp_stream.c`: `isp stream start / status / stop / wait`, the stream task, `ISP_OWNER_STREAM`, SYS_DMA channel 1 chains; `cmd_dma.c` ISR dispatch per channel |
| S2 | CTest: fixture sessions at queue 1/2/4 (≥ 12 frames), D65→A→D65, every frame equal to the reference; the smallest geometry of each Bayer phase; stop at each state; coalesced doorbells and a doorbell before the bridge waits; records initialized per session (a stale record of the previous session is never taken); a failed spool write is never ACKed; negative probes from the plan |
| S3 | `isp_stream.py` launcher + `run_vp.sh isp-stream`, encode/verify, `--keep-nv12`, re-encode helper, Ctrl+C, exit codes |

## 9. Limits

- **VP-only:** the doorbell, the bridge and the debug-read capture do not exist on silicon. The latency and backpressure are not ASIC timing.
- **Fixed profile:** WB, CCM and GTM key are fixed by the profile for the whole session; there is no auto-WB, so scenes far from the profile's illuminant show a colour cast. There is no geometry or profile change while running.
- **Output geometry:** it comes from the ISP, never from the input. Without the resizer, the Demosaic trim depends on the Bayer phase: the 256×144 fixture gives 256×144, 254×144, 256×142 and 254×142 for RGGB, GRBG, GBRG and BGGR.
- **Finite pool:** 254 MiB. A host refill for unbounded sequences is a later extension.
