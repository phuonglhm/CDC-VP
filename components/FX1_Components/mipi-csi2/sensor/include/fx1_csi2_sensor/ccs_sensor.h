// Camera sensor model for the VP (M5a): an I2C target with a subset of the
// MIPI Camera Command Set (CCS) register map, streaming CSI-2 frames through
// the pad-level D-PHY transmit BFM. It models no commercial sensor; model_id
// is a VP value. Sensor control over I2C replaces the host-side fixture of
// P06 (spec_decisions P12).
//
// Registers (16-bit index, 8-bit data, big-endian multi-byte values, index
// auto-increment on both reads and writes, CCS addresses):
//   0x0000-0x0001  model_id             RO  0xF1C5
//   0x0002         revision_number      RO  0x01
//   0x0005         frame_count          RO  frames sent since the last reset (8 bits, wraps)
//   0x0100         mode_select          RW  bit 0: 1 streaming, 0 standby (stops at the next frame end)
//   0x0103         software_reset       WO  bit 0: every register to its reset value, standby
//                                           (frame_count 0, frame numbers restart at 1)
//   0x0112-0x0113  csi_data_format      RW  0x0A0A RAW10, 0x0C0C RAW12
//   0x0114         csi_lane_mode        RW  lanes - 1: 0, 1 or 3
//   0x0340-0x0341  frame_length_lines   RW  frame period in line periods (>= y_output_size)
//   0x0342-0x0343  line_length_pck      RW  line period in pixel clocks (>= x_output_size)
//   0x034C-0x034D  x_output_size        RW  pixels per line
//   0x034E-0x034F  y_output_size        RW  lines per frame
// Writes to read-only or unknown indexes are acknowledged and ignored;
// reads of unknown indexes return 0.
//
// The mode is latched at each Frame Start. A mode that cannot be sent (lanes
// not built, unknown format, width not a whole number of packing units, zero
// size, a RAW file that is not a whole number of frames of that mode) leaves
// the sensor in standby with mode_select still 1; error_count() counts such
// attempts and each one is reported as a warning.
//
// A frame once started is sent whole and unchanged (M5a review F1): its mode,
// CSI-2 frame number (the same in FS and FE) and pixel source are latched at
// FS. Standby and software_reset take effect at its end; registers read their
// reset values at once after software_reset, and a frame in progress at the
// reset is not counted in frame_count.
//
// Stop request (M5b): writing mode_select 0 or software_reset while streaming
// latches a stop. At the end of the frame in progress the sensor stops (clock
// lane to LP-11 for at least standby_min()) even if mode_select has been set to
// 1 again meanwhile; it then restarts with the mode latched at that moment
// (frame number 1 after a software_reset).
//
// RAW file (M5a review F3): 16-bit little-endian samples. An empty file or an
// odd number of bytes is refused when the sensor is built (SC_REPORT_ERROR).
// With a file configured, frames come only from it, never from the pattern.
#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include <systemc>

#include "fx1_csi2_bfm/dphy_tx_bfm.h"
#include "i2c_device.h"

namespace fx1::csi2 {

struct CcsSensorConfig {
    uint8_t i2c_addr = 0x10;       // 7-bit
    unsigned max_lanes = 4;        // lanes wired to the receiver
    double pixel_rate_hz = 200e6;  // pixel clock of line_length_pck
    std::string raw_file;          // optional: frames of 16-bit little-endian samples, W x H each (see above)
    // Clock lane: true keeps the HS clock running for the whole stream; false
    // (non-continuous clock between frames, P16) puts it in LP-11 after each
    // FE and starts it again before the next FS. The detailed PHY costs ~50 s
    // of host time per simulated ms while the clock runs, so long vertical
    // blanking is cheap only with false.
    bool continuous_clock = true;
};

class CcsSensor : public sc_core::sc_module, public i2c_device {
public:
    static constexpr uint16_t kModelId = 0xF1C5;
    // Not a static sc_time: no sc_time may exist before sc_main sets 1 fs (P02).
    static sc_core::sc_time standby_min() { return sc_core::sc_time(1, sc_core::SC_US); }
    SC_HAS_PROCESS(CcsSensor);
    CcsSensor(sc_core::sc_module_name name, bfm::DphyTxBfm& tx, CcsSensorConfig cfg = {});

    // i2c_device
    bool i2c_start(bool read) override;
    bool i2c_write(uint8_t byte) override;
    uint8_t i2c_read(bool ack) override;
    void i2c_stop() override;

    // Pixel value of the built-in image (no raw_file): frame number f (1, 2,
    // ...), column x, line y, bit depth bits. Reproduced in firmware oracles.
    static uint16_t pattern(unsigned f, unsigned x, unsigned y, unsigned bits) {
        return static_cast<uint16_t>((x * 37u + y * 101u + f * 53u + 5u) & ((1u << bits) - 1u));
    }
    // Frames sent completely since the last software_reset (= CCS frame_count).
    uint32_t frames_sent() const { return frames_; }
    bool streaming() const { return streaming_; }
    // Between the Frame Start and the Frame End packets of a frame.
    bool in_frame() const { return in_frame_; }
    unsigned error_count() const { return errors_; }
    // Debug/test tap: called for every packet sent, with the data type and
    // 16-bit field decoded from the header bytes put on the link (frame number
    // of FS/FE, word count of a long packet).
    std::function<void(uint8_t dt, uint16_t field)> packet_observer;

private:
    struct Mode {
        unsigned width = 0, height = 0, lanes = 0, bits = 0;
        unsigned line_length = 0, frame_length = 0;
    };
    void reset_registers();
    uint8_t reg_read(uint16_t index) const;
    void reg_write(uint16_t index, uint8_t v);
    uint16_t reg16(uint16_t index) const { return uint16_t(reg_read(index) << 8 | reg_read(index + 1)); }
    bool latch_mode(Mode& m);
    std::vector<uint16_t> line_pixels(const Mode& m, unsigned frame, unsigned y) const;
    void send_packet(const std::vector<uint8_t>& pkt, unsigned lanes);
    void stream_thread();

    bfm::DphyTxBfm& tx_;
    CcsSensorConfig cfg_;
    // RW register storage
    uint8_t mode_select_ = 0, lane_mode_ = 0;
    uint16_t data_format_ = 0, frame_length_ = 0, line_length_ = 0, x_size_ = 0, y_size_ = 0;
    // I2C transfer state
    uint16_t index_ = 0;
    unsigned wbytes_ = 0;
    // streaming
    uint32_t frames_ = 0;
    uint16_t fnum_ = 0;            // last CSI-2 frame number sent (0: none since reset)
    uint64_t reset_epoch_ = 0;     // software_reset count: a frame started before one is not counted
    bool streaming_ = false, in_frame_ = false, stop_req_ = false;
    unsigned errors_ = 0;
    sc_core::sc_event reg_ev_;
    std::vector<uint16_t> raw_;
};

}  // namespace fx1::csi2
