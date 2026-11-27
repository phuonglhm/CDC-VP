# CSI-2 RX → AXI4-Stream source demo (internal verification)

Internal tool: it produces and checks the sample data. The ISP owner receives
the information document `../../docs/AXIS_STREAM_FOR_ISP_OWNER.md` with the data
files, not this harness.

What the FX1 CSI-2 RX controller model sends on its AXI4-Stream master
towards the ISP (MAS v1.4 Block No. 12, Table 6-67), and how a consumer is
connected to it. Part of the direct CSI→ISP plan (D2.1); the integration
contract is `../../docs/DIRECT_CSI_ISP_CONTRACT.md`.

The harness runs the **real controller model** (`fx1::csi2::Csi2RxController`)
programmed through its CSR socket. Its own stream master drives an example
sink. No ISP, NV12, firmware or CSI RAW DMA is involved. The detailed D-PHY RX
model is instantiated because the controller is built on it. The TPG sessions
do not use it, and the sensor sessions send packets through it from a
pad-level transmitter (BFM).

## Data path drawn as it runs

```text
TPG sessions:     Test Pattern Generator ─┐
                                          ├─ TPG Mux → Line Buffer → Format Bridge (RAW16) → AXI4-Stream master → sink
sensor sessions:  BFM → D-PHY RX → Lane Management → Packet Analyzer → Byte-to-Pixel ─┘
```

The TPG enters after the receive parser and unpacker. A TPG session is
**not** a sensor/PHY capture.

## Build and run

Needs SystemC 2.3.4, CMake ≥ 3.16, GCC with C++17, and Python 3 for the
expected vectors. From the CDC-VP root:

```bash
export CC=/usr/bin/gcc CXX=/usr/bin/g++
export PATH=/usr/bin:/bin:$PATH
cmake -S components/FX1_Components/mipi-csi2 -B build-csi2 -DCMAKE_BUILD_TYPE=Release \
      -DFX1_MIPI_CSI2_BUILD_TESTS=ON -DSYSTEMC_HOME=/opt/systemc-2.3.4 -G Ninja
cmake --build build-csi2 -j8 --target fx1_csi2_axis_source_demo
mkdir -p out
build-csi2/fx1_csi2_axis_source_demo tpg_raw12 out    # or any session below
ctest --test-dir build-csi2 -R fx1_csi2_axisdemo      # every session + the vector check
```

The first build compiles the D-PHY netlist model, which takes several
minutes.

| Session | Source | What it shows | Host time |
|---|---|---|---|
| `tpg_raw12` | TPG 32×8 RAW12 | TREADY = 1: 8 full beats per line, SOF on beat 0, TLAST on beat 7 | seconds |
| `tpg_raw10` | TPG 32×8 RAW10 | same, samples right-aligned in [9:0] | seconds |
| `tpg_stall` | TPG 32×8 RAW12 | fixed TREADY pattern: the SOF beat refused 3 cycles, line 2 beat 3 refused 4, the EOL beat of line 4 refused 5, every beat of line 6 refused once; payload unchanged, refused beat held stable | seconds |
| `tpg_abort` | TPG 32×8 RAW12 | `STREAM_CONFIG.axis_enable` 1→0 after line 3 beat 4: the held beats drain, then one null abort beat `KEEP=0x00, LAST=1, TUSER=0b10` | seconds |
| `bfm_tail` | sensor BFM, RAW12 34×8, 2 lanes | 9 beats per line, the last one with 2 samples, `TKEEP=0x0f` | tens of seconds |
| `bfm_crc` | sensor BFM | line 3 CRC corrupted: the capture is dropped, the stream ends the frame with a null abort beat (a CRC error is known only after the line, so line 3 may already have left whole); frame 2 is whole | tens of seconds |
| `bfm_crc_last` | sensor BFM | the last line (7) with a bad CRC: all 72 payload beats leave correct, then the abort beat; frame 2 whole. Payload complete is not a validated frame | tens of seconds |
| `bfm_wc` | sensor BFM | line 3 one byte short (WC 50, not a whole RAW12 pair): `TUSER[1]` on that line's last data beat | tens of seconds |

Triggers and limits:
- Packet errors are injected at the BFM, in the packet bytes on the wire.
- CRC, ECC and lane errors never mark data beats. They end the frame with
  the abort beat. `TUSER[1]` on a data beat comes only from the WC-alignment
  error.
- The TPG cuts the width to a multiple of 4, so no TPG frame has a partial
  tail beat.

## Configuration sequence (CSR offsets in `driver/include/fx1_csi2/fx1_csi2_regs.h`)

1. Release `resetn`. `CORE_CONFIG` resets to `core_enable = 1`.
2. Stream RAW16. `STREAM_CONFIG` resets to 0x3 (enabled, **packed**), and
   the format may change only while the stream is disabled. Write 0x2 first
   (disabled, format unchanged), then 0x0 (RAW16), then 0x1 (enabled). A
   single write from 0x3 to 0x0 or 0x1 is a format change while enabled: the
   model warns. With the C99 driver this is `fx1_csi2_stream_config(dev, 0, 0)`
   then `fx1_csi2_stream_config(dev, 1, 1)`. The second call alone, from
   reset, returns `FX1_CSI2_ESTATE` and writes nothing.
3. TPG: write `TPG_RESOLUTION = height << 16 | width`, `TPG_BLANKING`
   (vertical blanking in the upper half), then `TPG_CONTROL` with
   `tpg_enable` (bit 0), plus `tpg_pixel_format` (bit 3) for RAW10. Writing
   `TPG_CONTROL = 0` stops at the end of the frame in progress.
4. Sensor: `PROTOCOL_CONFIG` active lanes, `PHY_CONFIG` (phy, clock and data
   lane enables), wait for the data lanes' stop state in `LANE_STATUS`, then
   `CORE_CONFIG = core_enable`. The sensor starts its HS clock and sends
   FS / lines / FE.

The CSI RAW DMA stays disabled (`DMA_CONTROL` reset 0x10).

## Connecting a sink

```cpp
#include "fx1_csi2/csi2_rx_controller.h"   // AxisSink, AxisBeat

struct IspIngress : fx1::csi2::AxisSink {
    // Called once per executed sys_clk cycle (250 MHz grid) with what the master
    // drives. Return TREADY for this cycle. A beat moves when tvalid && TREADY.
    // A beat answered with TREADY = 0 comes back unchanged on the next call.
    // Do not wait() here: this runs inside the controller's process.
    bool axis_cycle(bool tvalid, const fx1::csi2::AxisBeat& b) override {
        const bool ready = fifo.size() < 16;              // your finite buffer
        if (tvalid && ready) fifo.push_back(b);           // tdata, tkeep, tlast, tuser, tid
        return ready;
    }
    std::deque<fx1::csi2::AxisBeat> fifo;
};

IspIngress sink;
controller.set_stream_sink(&sink);                         // before the stream starts
```

- The calls follow the executed cycles. The controller skips idle cycles
  while its datapath is quiescent, and never while TVALID = 1. Use
  `sc_time_stamp()` for time; do not count calls.
- Without a sink the stream is always ready and its beats are dropped.

## Beat format (RAW16 stream)

| Field | Meaning |
|---|---|
| `tdata[63:0]` | 4 samples, sample *n* in bits [16n+15:16n], little endian. RAW12 in [11:0], RAW10 in [9:0], unused upper bits 0 |
| `tkeep[7:0]` | valid bytes: 0xFF, or `(1 << 2·samples) − 1` on a line's last beat; 0x00 on an abort beat. No TSTRB |
| `tlast` | end of line, never end of frame |
| `tuser[0]` | start of frame, first beat of the frame only |
| `tuser[1]` | packet error (WC alignment, last beat of the line) or forced abort |
| `tid[1:0]` | exposure index (0 = Long). Always 0 in this linear build |

Decoding the first beat of `tpg_raw12` (`tdata = 0x0fff0fff0fff0fff`): samples
x = 0..3 of line 0 are 0xFFF, 0xFFF, 0xFFF, 0xFFF (R, G, R, G sites, white
bar). The last beat of a `bfm_tail` line, `tdata = 0x0000000004ff04da`,
`tkeep = 0x0f`, holds the samples x = 32 and x = 33 of that line.

## Files

| File | Content |
|---|---|
| `axis_source_demo.cpp` | harness, example sink, sessions, checks |
| `gen_expected.py` | expected payload beats, computed independently of the model (TPG formula C35 / D11, the BFM's sensor pattern); `--check DIR` compares |
| `expected/*.csv` | `line,beat,tdata,tkeep,tlast,tuser,tid`: valid-frame payload only |
| `<out>/<session>.trace.csv` | one row per sink call: `cycle,time_ps,tvalid,tready,tdata,tkeep,tlast,tuser,tid,accepted`. `cycle` = time / 4 ns, `time_ps` in ps, `tdata` 16 hex digits with sample 0 in the lowest 16 bits |
| `<out>/<session>.accepted.csv` | the accepted beats: `index,cycle,tdata,tkeep,tlast,tuser,tid` |

The model revision used to produce the published traces is recorded in the
delivery status. Capture start/end sideband events (frame ID, timestamp,
outcome) come with the controller's observer hook in a later step and are not
part of the AXI4-Stream signals.
