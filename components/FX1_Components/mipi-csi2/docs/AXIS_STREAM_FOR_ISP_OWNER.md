# FX1 CSI-2 RX → ISP: the AXI4-Stream pixel output

What the FX1 MIPI CSI-2 RX controller sends on its AXI4-Stream master, how to
decode it, and sample data with traces. Read with the files in `data/`; no
simulator, repository or build is needed.

Source of the behaviour: FTEL IP MIPI CSI-2 Controller MAS v1.4, Block 12
(AXI4-Stream master, Table 6-67), and the FX1 virtual-platform model of the
controller. The samples were produced by running that model and were checked
against pixel values computed independently of it.

## 1. Data path

```text
Sensor path:  sensor ─ D-PHY RX ─ Lane Management ─ Packet Analyzer ─ Byte-to-Pixel ─┐
                                                                                     ├─ TPG Mux ─ Line Buffer ─ Format Bridge ─ AXI4-Stream master ─▶ ISP
TPG path:                                             Test Pattern Generator ───────┘                          (RAW16 format)     (TVALID/TREADY)
```

- The Test Pattern Generator (TPG) joins after the packet parser and the
  unpacker. A TPG frame has never been through the PHY or packet checks.
- The Format Bridge is set to **RAW16**: one 16-bit container per sample.
  The packed formats (RAW10/RAW12 bytes as on the wire) are not used towards
  the ISP.
- Back-pressure: TREADY low stops the master; its output and skid registers
  hold, then the Format Bridge FIFO and the Line Buffer fill. The TPG simply
  waits. A sensor cannot wait: when the Line Buffer reaches its watermark the
  controller drops the capture (§5).

## 2. Signals

| Signal | Width | Meaning on this stream |
|---|---|---|
| `axis_aclk` | 1 | the controller's system clock, 250 MHz (4 ns) in the samples |
| `m_axis_tvalid` | 1 | beat valid. Once raised with a beat, the beat stays unchanged until accepted |
| `m_axis_tready` | 1 | from the ISP. A beat moves on a rising edge where `tvalid && tready`. The only back-pressure signal |
| `m_axis_tdata` | 64 | 4 samples, sample *n* in bits [16n+15:16n] |
| `m_axis_tkeep` | 8 | valid bytes, always a prefix: `0xFF`, or on a line's last beat `(1 << 2·samples) − 1`; `0x00` only on the abort beat. No TSTRB |
| `m_axis_tlast` | 1 | end of **line** (never end of frame) |
| `m_axis_tuser[0]` | 1 | start of frame: first beat of a frame, once |
| `m_axis_tuser[1]` | 1 | error: a packet error reported on a data beat, or the abort beat |
| `m_axis_tid` | 2 | exposure index; 0 for the linear (single-exposure) configuration used here |

There is no end-of-frame signal: a frame ends with the TLAST of its last line,
known from the configured height, or with an abort beat. The stream carries
no frame number or timestamp.

## 3. Decoding a beat

- **Samples:** `samples = popcount(tkeep) / 2`; sample *n* = `(tdata >> 16n) & 0xFFFF`.
  RAW12 occupies bits [11:0], RAW10 bits [9:0]; the unused upper bits are 0.
  RAW10 is **not** shifted up to 12 bits.
- **Order:** sample 0 is the leftmost pixel of the beat; beats of a line
  come left to right; a line starts at x = 0 after the previous TLAST.
- **Line length:** `ceil(W / 4)` beats; the last beat has `W mod 4` samples
  when that is not 0 (RAW12, W ≡ 2 mod 4: `tkeep = 0x0F`). RAW10 lines from a
  sensor are whole 4-pixel groups, so their last beat is full.
- **Colour:** the stream carries no Bayer information; the ISP's
  configured CFA phase applies. The TPG samples use RGGB.

Example, first beat of the RAW12 TPG frame, `tdata = 0x0fff0fff0fff0fff`,
`tkeep = 0xff`, `tuser = 1`: samples x = 0..3 of line 0 are
0xFFF, 0xFFF, 0xFFF, 0xFFF (R G R G sites of the white bar). The same beat in
RAW10 is `0x03ff03ff03ff03ff`.

Example, last beat of a line of the 34-pixel RAW12 sensor frame,
`tdata = 0x0000000004ff04da`, `tkeep = 0x0f`, `tlast = 1`: 2 samples,
x = 32 → 0x4DA, x = 33 → 0x4FF; bytes 4..7 are not part of the line.

Pseudocode for a receiver:

```text
on every clock edge where tvalid && tready:
    if tkeep == 0x00 and tlast and tuser[1]:      # abort beat (§5)
        mark the current frame bad -- also when its H lines are already complete
        wait for the next SOF
    else:
        if tuser[0]:
            the previous frame gets no more beats (in-band status final, §5)
            start a new frame (y = 0, x = 0); a frame with fewer than H lines is incomplete
        n = popcount(tkeep) / 2
        for i in 0 .. n-1: pixel[y][x + i] = (tdata >> 16*i) & 0xFFFF
        x += n
        if tuser[1]: mark the frame as bad
        if tlast:   check x == W; y += 1; x = 0
                    if y == H: payload complete -- NOT yet validated (§5)
```

"Payload complete" means all pixels have arrived. It does not mean the frame
is good: an abort beat for the same frame can still follow (CRC error on the
last line, §5), and some capture errors never appear on the stream at all.

## 4. Handshake rules

- TVALID does not wait for TREADY. A beat offered with TREADY = 0 is offered
  again, unchanged, on the next cycles until it is accepted.
- TREADY may change on any cycle, including on the SOF beat and on a TLAST
  beat. The stream has no minimum or maximum stall.
- Between lines and frames TVALID is low for the blanking time of the source.
- TREADY must eventually be raised to accept the abort beat, even when the
  ISP has no room for pixels; until then the controller cannot finish the
  abort.

## 5. Errors and abort

| Case | What the stream shows | Note |
|---|---|---|
| Word count not a whole pixel group (line too short by a byte) | `tuser[1] = 1` on the **last beat of that line**, with TLAST; the frame continues | the samples of that beat are not reliable; the frame is bad |
| CRC error, ECC double error, truncated packet (burst ends before its word count), lane abort, Line Buffer overflow, Format Bridge overflow, controller disabled | the controller drops the capture: the beats it already holds leave, then **one abort beat** `tkeep 0x00, tlast 1, tuser 0b10`, then nothing of that frame | a CRC error is known only after its line: that line may already have left complete with its TLAST before the abort beat |
| CRC error on the **last** line | all H lines leave complete and correct, **then** the abort beat | the abort beat comes after a payload-complete frame and before the next SOF; it still invalidates that frame (`bfm_crc_last`) |
| Stream disabled during a frame (`STREAM_CONFIG.axis_enable` 1 → 0) | as above: held beats, then the abort beat | |
| Controller reset | TVALID drops; no TLAST, no abort beat | the receiver must be reset with it |

- The abort beat can close an open line (TLAST without the line's last
  pixels) or stand alone after a complete line.
- When the controller drops a capture it also discards every beat still
  waiting in its Line Buffer, including, under back-pressure, the end of an
  *earlier* frame: that earlier frame then stops early and is closed by the
  abort beat too, although its Frame End had been received.
- TUSER[1] on a data beat is the only in-band error flag; other packet errors
  appear only as the abort beat.
- The frame after a dropped or bad frame starts normally with its SOF.
- **When the in-band status of a frame is final:** only once the next SOF has
  been accepted with no abort beat in between, or the controller has been
  stopped and its stream has gone idle. Between the last TLAST of a frame and the next SOF, an abort beat
  for that frame may still arrive.
- **What the stream cannot tell.** Even then the frame is only free of
  in-band errors, not validated. A missing Frame End packet produces nothing
  on the stream: the controller closes that capture when the next Frame Start
  arrives and counts it as delivered. The AXI4-Stream signals alone do not
  prove that a capture ended correctly; in the FX1 integration a separate,
  per-capture outcome (outside this interface) decides whether a frame is
  used.

## 6. Configuration used for the samples

Controller registers of the FX1 CSI-2 RX (IP 1.1.0 register map):

1. Release reset. `CORE_CONFIG` resets to `core_enable = 1`.
2. RAW16 stream. `STREAM_CONFIG` resets to `0x3` (enabled, **packed**); the
   format may change only while the stream is disabled. Write `0x2`
   (disabled, format unchanged), then `0x0` (RAW16, disabled), then `0x1`
   (RAW16, enabled).
3. TPG sessions: `TPG_RESOLUTION = height << 16 | width`, `TPG_BLANKING`
   (vertical blanking in the upper half), `TPG_CONTROL.tpg_enable` (bit 0),
   with `tpg_pixel_format` (bit 3) for RAW10.
4. Sensor sessions: lanes configured and enabled, then the sensor sends
   FS / lines / FE (2 data lanes, RAW12 34×8).

The CSI DMA to memory is off in all sessions.

## 7. Sample sessions and files

| Session | Source | Shows | Files |
|---|---|---|---|
| `tpg_raw12` | TPG RAW12 32×8 | TREADY = 1: 8 full beats per line, SOF on the first beat, TLAST on beat 7 | `traces/tpg_raw12.*`, `expected/tpg_raw12_32x8.csv` |
| `tpg_raw10` | TPG RAW10 32×8 | same, samples in [9:0] | `traces/tpg_raw10.*`, `expected/tpg_raw10_32x8.csv` |
| `tpg_stall` | TPG RAW12 32×8 | fixed TREADY pattern: the SOF beat refused 3 cycles, line 2 beat 3 refused 4, line 4's TLAST beat refused 5, every beat of line 6 refused once; the accepted payload equals `tpg_raw12` | `traces/tpg_stall.*`, `expected/tpg_raw12_32x8.csv` |
| `tpg_abort` | TPG RAW12 32×8 | stream disabled after line 3 beat 4: held beats leave, then the abort beat | `traces/tpg_abort.*` |
| `bfm_tail` | sensor RAW12 34×8 | 9 beats per line, the last with 2 samples, `tkeep 0x0F` | `traces/bfm_tail.*`, `expected/bfm_raw12_34x8.csv` |
| `bfm_crc` | sensor RAW12 34×8, two frames | frame 1 line 3 with a bad CRC: lines 0–3 leave (36 beats), then the abort beat; frame 2 complete | `traces/bfm_crc.*`, `expected/bfm_raw12_34x8.csv` (frame 2) |
| `bfm_crc_last` | sensor RAW12 34×8, two frames | frame 1 line 7 (the last) with a bad CRC: all 72 payload beats leave correct, then the abort beat; frame 2 complete | `traces/bfm_crc_last.*`, `expected/bfm_raw12_34x8.csv` (both frames' payload) |
| `bfm_wc` | sensor RAW12 34×8 | line 3 one byte short: `tuser[1]` with `tkeep 0x0F`, TLAST on line 3's last beat; the frame continues | `traces/bfm_wc.*` |

Paths are relative to `data/`. File formats (CSV, header line first):

- `expected/<frame>.csv`: `line,beat,tdata,tkeep,tlast,tuser,tid`, the beats of
  a good frame, computed from the pixel formulas below, not from a simulation.
- `traces/<session>.trace.csv`: `cycle,time_ps,tvalid,tready,tdata,tkeep,tlast,tuser,tid,accepted`,
  one row per clock cycle the sink observed. `cycle` = time / 4 ns,
  `time_ps` in picoseconds, `accepted = tvalid && tready`. Idle stretches
  with TVALID = 0 are not all listed; use `time_ps`, not row counts.
- `traces/<session>.accepted.csv`: `index,cycle,tdata,tkeep,tlast,tuser,tid`,
  the accepted beats only, in order.
- `tdata`: 16 hex digits, most significant first, so sample 0 is the
  **rightmost** 4 digits. `tkeep` hex, `tuser` decimal (2 = bit 1).

Pixel formulas of the expected data:

- TPG colour bars (vertical): `base = (W & ~3) / 8`, `band` = number of
  k in 0..6 with x ≥ (k+1)·base; R on for bands 0, 1, 4, 5; G on for bands
  0..3; B on for even bands; RGGB (even row/even column R, odd/odd B, else G);
  on = 0xFFF (RAW12) or 0x3FF (RAW10), off = 0. The TPG cuts the width to a
  multiple of 4, so TPG lines never have a partial beat.
- Sensor pattern: `pixel(x, y) = (37·x + 101·y + 58) & 0xFFF`, the same for
  every frame.

## 8. Trace excerpts

`tpg_stall`, start of frame (TREADY low 3 cycles on the SOF beat, then the
line streams):

```text
cycle time_ps  tvalid tready tdata            tkeep tlast tuser accepted
257   1028000  1      0      0fff0fff0fff0fff 0xff  0     1     0
258   1032000  1      0      0fff0fff0fff0fff 0xff  0     1     0
259   1036000  1      0      0fff0fff0fff0fff 0xff  0     1     0
260   1040000  1      1      0fff0fff0fff0fff 0xff  0     1     1     <- SOF accepted
261   1044000  1      1      0fff0fff0fff0fff 0xff  0     0     1
...
267   1068000  1      1      0000000000000000 0xff  1     0     1     <- end of line 0
```

`tpg_stall`, line 4's TLAST beat refused 5 cycles and held:

```text
303   1212000  1      0      0000000000000000 0xff  1     0     0
...   (304..307 identical, tready 0)
```

`bfm_tail`, line 0 (sensor RAW12 34 wide; gaps in `cycle` are TVALID = 0
while bytes arrive from the 2 lanes):

```text
88165 ... 1 1 00a90084005f003a 0xff 0 1 1   <- SOF, x = 0..3: 0x03A 0x05F 0x084 0x0A9
88170 ... 1 1 013d011800f300ce 0xff 0 0 1
...
88221 ... 1 1 0000000004ff04da 0x0f 1 0 1   <- x = 32, 33; tail beat
```

`bfm_crc`, accepted beats around the drop (frame 1 line 3 bad CRC):

```text
index cycle tdata            tkeep tlast tuser
35    88881 00000000062e0609 0x0f  1     0     <- line 3 left complete
36    88884 0000000000000000 0x00  1     2     <- abort beat: frame 1 is invalid
37    90733 00a90084005f003a 0xff  0     1     <- frame 2 SOF
```

`bfm_crc_last`, accepted beats at the end of frame 1 (bad CRC on the last
line):

```text
index cycle tdata            tkeep tlast tuser
71    89764 0000000007c2079d 0x0f  1     0     <- line 7 complete: payload complete, 72 beats
72    89767 0000000000000000 0x00  1     2     <- abort beat: frame 1 is invalid after all
73    90733 00a90084005f003a 0xff  0     1     <- frame 2 SOF
```

`bfm_wc`, line 3 one byte short:

```text
index cycle tdata            tkeep tlast tuser
35    88881 0000000006200600 0x0f  1     2     <- TUSER[1] on the line's last beat
36    89045 023d021801f301ce 0xff  0     0     <- line 4 follows, same frame
```

`tpg_abort`, end:

```text
cycle time_ps  tvalid tready tdata            tkeep tlast tuser accepted
285   1140000  1      1      0fff00000fff0000 0xff  0     0     1     <- last held pixel beat
288   1152000  1      1      0000000000000000 0x00  1     2     1     <- abort beat closes line 3
```
