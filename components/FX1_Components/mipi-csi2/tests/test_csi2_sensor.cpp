// M5a: the CCS sensor model controlled over I2C (functional i2c host), its
// frames through the detailed D-PHY into the controller and DDR (CSI-2 side
// programmed with the C99 driver), as firmware will run it on fx1_soc.
//
//   test_fx1_csi2_sensor <case>
//     stream      probe over I2C, mode RAW10 64x4 on 4 lanes, stream on, three
//                 frames to DDR (verify, release), stream off inside the fourth
//                 frame: that frame completes, none follows
//     raw12_2lane same in RAW12 on 2 lanes
//     stream_noncontinuous  as stream, clock lane in LP-11 between frames (P16)
//     controls    NACK of an absent address, read-only and unknown registers,
//                 software_reset, a mode the sensor cannot send
//     raw_file    frames taken from a RAW file instead of the built-in pattern
//     reset_mid_frame  software_reset inside frame 2: frame 2 still whole and
//                 exact (latched number and pixels), not counted, standby;
//                 the next stream restarts at frame number 1 (review F1)
//     restart_before_fe  stop (mode_select 0) and software_reset written with
//                 mode_select 1 again before FE: the frame completes, the
//                 sensor stops (clock lane LP-11), then restarts (M5b)
//     raw_short, raw_partial  a RAW file of less than one frame / 1.5 frames
//                 of the mode: no frame, error count, no pattern (review F3)
//     raw_mode_change  a mode change while streaming that the file does not
//                 fit stops at the frame end; the fitting mode resumes
//     raw_load_errors  empty and odd-length RAW files refused at construction
//
// Oracle: CcsSensor::pattern() (the formula firmware reproduces) or the file,
// the layout of spec_decisions M02 for the DDR image.
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <fstream>
#include <functional>
#include <map>
#include <string>
#include <vector>

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>

#include "fx1_csi2/csi2_rx_controller.h"
#include "fx1_csi2/fx1_csi2_drv.h"
#include "fx1_csi2/fx1_csi2_regs.h"
#include "fx1_csi2_bfm/csi2_packet.h"
#include "fx1_csi2_bfm/dphy_tx_bfm.h"
#include "fx1_csi2_phy/dphy_rx_wrapper.h"
#include "fx1_csi2_sensor/ccs_sensor.h"
#include "i2c.h"
#include "tlm_mem.h"

using namespace fx1::csi2;
using sc_core::sc_time;
using sc_core::SC_NS;
using sc_core::SC_US;

namespace {
int g_fail = 0;
void failed() {
    if (++g_fail >= 50) {
        std::fprintf(stderr, "too many failures, abort\n");
        std::exit(2);
    }
}
#define CHECK(cond)                                                                         \
    do {                                                                                    \
        if (!(cond)) {                                                                      \
            std::fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond);    \
            failed();                                                                       \
        }                                                                                   \
    } while (0)
#define CHECK_EQ(a, b)                                                                                     \
    do {                                                                                                   \
        const auto va_ = (a);                                                                              \
        const auto vb_ = (b);                                                                              \
        if (va_ != vb_) {                                                                                  \
            std::fprintf(stderr, "%s:%d: CHECK_EQ failed: %s = 0x%llX, expected 0x%llX\n", __FILE__, __LINE__, \
                         #a, (unsigned long long)va_, (unsigned long long)vb_);                            \
            failed();                                                                                      \
        }                                                                                                  \
    } while (0)

constexpr uint32_t kCsi = 0x00000, kI2c = 0x10000;  // two windows of the bench's address space
constexpr uint8_t kSensor = 0x10;
std::string g_raw_path;

i2c_config i2c_cfg() {
    i2c_config c;
    c.functional_host = true;
    c.scl_hz = 1e6;  // fast-mode plus keeps the bench short
    return c;
}
CcsSensorConfig sensor_cfg(const std::string& which) {
    CcsSensorConfig c;
    c.pixel_rate_hz = 200e6;
    c.raw_file = g_raw_path;  // empty: built-in pattern
    c.continuous_clock = which != "stream_noncontinuous";
    return c;
}

struct Tb : sc_core::sc_module {
    tlm_utils::simple_initiator_socket<Tb> socket{"socket"};
    tlm_utils::simple_initiator_socket<Tb> i2c_socket{"i2c_socket"};
    sc_core::sc_signal<bool> resetn{"resetn", false};
    sc_core::sc_signal<bool> irq{"irq"}, i2c_irq{"i2c_irq"};
    DphyRxWrapper phy{"phy_model"};
    Csi2RxController ctl;
    bfm::DphyTxBfm tx{"tx", phy.clkp, phy.clkn, phy.dp, phy.dn};
    i2c i2c0;
    CcsSensor sensor;
    TlmMem ddr{"ddr", 0x80000000ull, 4u << 20};
    std::string which;
    fx1_csi2_hal hal{};
    fx1_csi2_dev dev{};

    SC_HAS_PROCESS(Tb);
    Tb(sc_core::sc_module_name nm, std::string w)
        : sc_core::sc_module(nm), ctl("csi2", phy), i2c0("i2c0", i2c_cfg()), sensor("sensor", tx, sensor_cfg(w)),
          which(std::move(w)) {
        socket.bind(ctl.csr_socket);
        i2c_socket.bind(i2c0.socket);
        ctl.resetn(resetn);
        ctl.irq(irq);
        ctl.dma_socket.bind(ddr.socket);
        i2c0.irq(i2c_irq);
        i2c0.attach(kSensor, &sensor);
        SC_THREAD(run);
    }

    uint32_t xfer(tlm::tlm_command cmd, uint32_t addr, uint32_t v) {
        unsigned char b[4] = {uint8_t(v), uint8_t(v >> 8), uint8_t(v >> 16), uint8_t(v >> 24)};
        tlm::tlm_generic_payload p;
        sc_time d = sc_core::SC_ZERO_TIME;
        p.set_command(cmd);
        p.set_address(addr & 0xFFFF);
        p.set_data_ptr(b);
        p.set_data_length(4);
        p.set_streaming_width(4);
        (addr >= kI2c ? i2c_socket : socket)->b_transport(p, d);
        if (d != sc_core::SC_ZERO_TIME) wait(d);
        return b[0] | b[1] << 8 | b[2] << 16 | uint32_t(b[3]) << 24;
    }
    uint32_t rd(uint32_t a) { return xfer(tlm::TLM_READ_COMMAND, a, 0); }
    void wr(uint32_t a, uint32_t v) { xfer(tlm::TLM_WRITE_COMMAND, a, v); }
    static uint32_t hal_read32(void* c, uint32_t off) { return static_cast<Tb*>(c)->rd(kCsi + off); }
    static void hal_write32(void* c, uint32_t off, uint32_t v) { static_cast<Tb*>(c)->wr(kCsi + off, v); }
    static void hal_delay_us(void*, uint32_t us) { sc_core::wait(sc_time(us, SC_US)); }

    // ---- I2C, as firmware drives it (FDATA commands, wait for idle)
    bool i2c_idle(unsigned max_us = 5000) {
        for (unsigned t = 0; t < max_us && !(rd(kI2c + I2C_STATUS) & STATUS_HOSTIDLE); ++t) wait(1, SC_US);
        return rd(kI2c + I2C_STATUS) & STATUS_HOSTIDLE;
    }
    bool nak() { return rd(kI2c + I2C_INTR_STATE) & INTR_NAK; }
    bool reg_write(uint16_t reg, std::vector<uint8_t> data, uint8_t addr = kSensor) {
        wr(kI2c + I2C_INTR_STATE, 0xFFFFFFFF);
        wr(kI2c + I2C_FDATA, FDATA_START | (addr << 1));
        wr(kI2c + I2C_FDATA, reg >> 8);
        wr(kI2c + I2C_FDATA, (reg & 0xFF) | (data.empty() ? FDATA_STOP : 0));
        for (size_t i = 0; i < data.size(); ++i) wr(kI2c + I2C_FDATA, data[i] | (i + 1 == data.size() ? FDATA_STOP : 0));
        CHECK(i2c_idle());
        return !nak();
    }
    bool reg16(uint16_t reg, uint16_t v) { return reg_write(reg, {uint8_t(v >> 8), uint8_t(v)}); }
    std::vector<uint8_t> reg_read(uint16_t reg, unsigned n, uint8_t addr = kSensor) {
        wr(kI2c + I2C_INTR_STATE, 0xFFFFFFFF);
        wr(kI2c + I2C_FDATA, FDATA_START | (addr << 1));
        wr(kI2c + I2C_FDATA, reg >> 8);
        wr(kI2c + I2C_FDATA, reg & 0xFF);
        wr(kI2c + I2C_FDATA, FDATA_START | (addr << 1) | 1);
        wr(kI2c + I2C_FDATA, FDATA_READB | FDATA_STOP | n);
        CHECK(i2c_idle());
        std::vector<uint8_t> v;
        for (unsigned i = 0; i < n && !(rd(kI2c + I2C_STATUS) & STATUS_RXEMPTY); ++i)
            v.push_back(static_cast<uint8_t>(rd(kI2c + I2C_RDATA)));
        CHECK(rd(kI2c + I2C_STATUS) & STATUS_RXEMPTY);
        return v;
    }
    uint32_t read8(uint16_t reg) {
        const auto v = reg_read(reg, 1);
        return v.size() == 1 ? v[0] : 0xFFFFu;
    }
    uint16_t read16(uint16_t reg) {
        const auto v = reg_read(reg, 2);
        return v.size() == 2 ? uint16_t(v[0] << 8 | v[1]) : 0xFFFF;
    }

    // CSI-2 side: PHY, DMA RAW16 at stride 2W, interrupts.
    void csi_up(unsigned lanes, unsigned w) {
        resetn.write(true);
        wait(1, SC_US);
        wr(kI2c + I2C_CTRL, CTRL_ENABLEHOST);
        hal = {&Tb::hal_read32, &Tb::hal_write32, &Tb::hal_delay_us, this};
        dev = fx1_csi2_dev{};
        CHECK(fx1_csi2_probe(&dev, &hal) == FX1_CSI2_OK);
        CHECK(fx1_csi2_phy_start(&dev, lanes, 1000) == FX1_CSI2_OK);
        CHECK(fx1_csi2_stream_config(&dev, 0, 0) == FX1_CSI2_OK);
        fx1_csi2_dma_cfg c{};
        c.base[0] = 0x80000000u;
        c.base[1] = 0x80100000u;
        c.size = 0x10000;
        c.bytes_per_line = static_cast<uint16_t>(2 * w);
        c.max_burst_len = 255;
        c.raw16 = 1;
        c.release_on_error = 1;
        CHECK(fx1_csi2_dma_config(&dev, &c) == FX1_CSI2_OK);
        CHECK(fx1_csi2_irq_enable(&dev, (1u << IRQ_BUF_DONE_OK) | (1u << IRQ_BUF_DONE_ERR) | (1u << IRQ_FRAME_DROPPED)) ==
              FX1_CSI2_OK);
        CHECK(fx1_csi2_dma_start(&dev) == FX1_CSI2_OK);
    }
    // Wait for the interrupt, check the completed side against the oracle, release it.
    bool take_frame(unsigned w, unsigned h, unsigned bits, std::function<uint16_t(unsigned, unsigned)> px, unsigned max_us = 200) {
        // The irq output follows IRQ_STATUS a few sys_clk later: right after a
        // clear it can still read 1 with nothing pending, as firmware also sees.
        uint32_t pending = 0;
        for (unsigned t = 0; t < max_us * 10 && !pending; ++t) {
            if (irq.read()) pending = fx1_csi2_irq_handle(&dev);
            if (!pending) wait(100, SC_NS);
        }
        if (!pending) return false;
        CHECK(pending & (1u << IRQ_BUF_DONE_OK));
        CHECK_EQ(pending & ((1u << IRQ_BUF_DONE_ERR) | (1u << IRQ_FRAME_DROPPED)), 0u);
        bool found = false;
        for (unsigned side = 0; side < 2; ++side) {
            fx1_csi2_buf_info bi{};
            fx1_csi2_get_buffer(&dev, side, &bi);
            if (bi.state != FX1_CSI2_BUF_SW_OWNED_VALID) continue;
            const uint32_t base = side ? 0x80100000u : 0x80000000u;
            bool exact = true;
            for (unsigned y = 0; y < h; ++y) {
                const auto line = ddr.read(base + y * 2 * w, 2 * w);
                for (unsigned x = 0; x < w; ++x) exact &= uint16_t(line[2 * x] | line[2 * x + 1] << 8) == px(x, y);
            }
            CHECK(exact);
            CHECK(fx1_csi2_buffer_release(&dev, side) == FX1_CSI2_OK);
            found = true;
            (void)bits;
        }
        return found;
    }

    // ------------------------------------------------------------------ cases
    void stream(unsigned lanes, unsigned bits) {
        const unsigned w = 64, h = 4;
        csi_up(lanes, w);
        // Probe and mode over I2C (CCS registers).
        CHECK_EQ(read16(0x0000), CcsSensor::kModelId);
        CHECK(reg16(0x0112, bits == 10 ? 0x0A0A : 0x0C0C));
        CHECK(reg_write(0x0114, {uint8_t(lanes - 1)}));
        CHECK(reg16(0x034C, w));
        CHECK(reg16(0x034E, h));
        CHECK(reg16(0x0342, 4000));    // line period 20 us at 200 MHz: frame data 80 us, period 160 us
        CHECK(reg16(0x0340, h + 4));   // vertical blanking 4 lines
        CHECK_EQ(read16(0x034C), w);
        // Stream on: three frames, each verified and released from the interrupt.
        const sc_time t0 = sc_core::sc_time_stamp();
        CHECK(reg_write(0x0100, {1}));
        for (unsigned f = 1; f <= 3; ++f)
            CHECK(take_frame(w, h, bits, [&](unsigned x, unsigned y) { return CcsSensor::pattern(f, x, y, bits); }));
        if (which != "stream_noncontinuous") {
            CHECK(rd(kCsi + FX1_CSI2_PHY_STATUS) & FX1_CSI2_PHY_STATUS_CL_RXCLKACTIVEHS_MASK);  // HS clock while streaming
        } else {
            // Non-continuous clock: in the blanking after frame 3 the clock lane is in LP-11.
            bool lp11 = false;
            for (unsigned t = 0; t < 400 && !lp11; ++t) {
                lp11 = (rd(kCsi + FX1_CSI2_PHY_STATUS) & (FX1_CSI2_PHY_STATUS_CL_RXCLKACTIVEHS_MASK |
                                                         FX1_CSI2_PHY_STATUS_CL_STOPSTATE_MASK)) ==
                       FX1_CSI2_PHY_STATUS_CL_STOPSTATE_MASK;
                if (!lp11) wait(100, SC_NS);
            }
            CHECK(lp11 && sensor.streaming());
        }
        std::printf("  3 frames %ux%u RAW%u on %u lane(s) in %.1f us simulated\n", w, h, bits, lanes,
                    (sc_core::sc_time_stamp() - t0).to_seconds() * 1e6);
        // Stream off inside frame 4 (the I2C write takes about 38 us of its
        // 80 us): the sensor finishes that frame and stops at its end.
        // Frame 3 has just ended (BUF_DONE can come before the sensor leaves
        // its last packet): wait for the start of frame 4.
        for (unsigned t = 0; t < 2000 && sensor.in_frame(); ++t) wait(100, SC_NS);
        for (unsigned t = 0; t < 2000 && !sensor.in_frame(); ++t) wait(100, SC_NS);
        CHECK(sensor.in_frame());
        CHECK(reg_write(0x0100, {0}));
        CHECK(sensor.in_frame());  // the write landed before the frame end
        CHECK(take_frame(w, h, bits, [&](unsigned x, unsigned y) { return CcsSensor::pattern(4, x, y, bits); }));
        for (unsigned t = 0; t < 2000 && sensor.streaming(); ++t) wait(100, SC_NS);
        CHECK(!sensor.streaming());
        CHECK_EQ(sensor.frames_sent(), 4u);
        wait(200, SC_US);
        CHECK_EQ(sensor.frames_sent(), 4u);  // no frame after standby
        // Standby: the clock lane is back in LP-11.
        CHECK_EQ(rd(kCsi + FX1_CSI2_PHY_STATUS) &
                     (FX1_CSI2_PHY_STATUS_CL_RXCLKACTIVEHS_MASK | FX1_CSI2_PHY_STATUS_CL_STOPSTATE_MASK),
                 FX1_CSI2_PHY_STATUS_CL_STOPSTATE_MASK);
        CHECK(!irq.read());
        fx1_csi2_status st{};
        CHECK(fx1_csi2_get_status(&dev, &st) == FX1_CSI2_OK);
        CHECK_EQ(st.frame_count, sensor.frames_sent());  // every frame sent arrived whole
        CHECK_EQ(st.frame_drop_count, 0u);
        CHECK_EQ(read8(0x0005), sensor.frames_sent() & 0xFFu);
        CHECK(fx1_csi2_dma_stop(&dev, 100) == FX1_CSI2_OK);
        CHECK_EQ(ctl.ppi_x_samples(), 0u);
    }

    void controls() {
        csi_up(4, 64);
        // No device at 0x22: NACK; the next transaction works.
        CHECK(!reg_write(0x0100, {1}, 0x22));
        CHECK_EQ(read16(0x0000), CcsSensor::kModelId);
        // Read-only and unknown registers.
        CHECK(reg16(0x0000, 0x1234));
        CHECK_EQ(read16(0x0000), CcsSensor::kModelId);
        CHECK(reg_write(0x7000, {0x55}));
        CHECK_EQ(read8(0x7000), 0u);
        // Auto-increment across registers: x_output_size and y_output_size in one write.
        CHECK(reg_write(0x034C, {0x01, 0x00, 0x00, 0x08}));
        CHECK_EQ(read16(0x034C), 0x100u);
        CHECK_EQ(read16(0x034E), 8u);
        // A mode that cannot be sent: unknown data format; stream on gives no frame.
        CHECK(reg16(0x0112, 0x0808));
        CHECK(reg_write(0x0100, {1}));
        wait(20, SC_US);
        CHECK(!sensor.streaming());
        CHECK(sensor.error_count() >= 1);
        CHECK_EQ(sensor.frames_sent(), 0u);
        // software_reset: defaults back, standby.
        CHECK(reg_write(0x0103, {1}));
        CHECK_EQ(read16(0x034C), 640u);
        CHECK_EQ(read16(0x0112), 0x0A0Au);
        CHECK_EQ(read8(0x0100), 0u);
        CHECK_EQ(rd(kCsi + FX1_CSI2_FRAME_COUNT), 0u);
    }

    void raw_file() {
        const unsigned w = 32, h = 2;
        csi_up(4, w);
        CHECK(reg16(0x034C, w));
        CHECK(reg16(0x034E, h));
        CHECK(reg16(0x0342, 2000));
        CHECK(reg16(0x0340, h + 6));
        CHECK(reg_write(0x0100, {1}));
        // The file holds two frames: the third frame is the first one again.
        for (unsigned f = 1; f <= 3; ++f)
            CHECK(take_frame(w, h, 10, [&](unsigned x, unsigned y) { return file_px((f - 1) % 2, x, y); }));
        CHECK(reg_write(0x0100, {0}));
    }

    // RAW file 32 x n samples, value (f * 1000 + y * 100 + x) & 0x3FF with
    // f, y the frame / line of a 32x2 frame (written by sc_main).
    static uint16_t file_px(unsigned f, unsigned x, unsigned y) { return uint16_t((f * 1000 + y * 100 + x) & 0x3FF); }

    void reset_mid_frame() {
        const unsigned w = 64, h = 8;
        csi_up(4, w);
        auto mode = [&] {
            CHECK(reg16(0x034C, w));
            CHECK(reg16(0x034E, h));
            CHECK(reg16(0x0342, 4000));  // 20 us lines: the I2C reset lands inside a 160 us frame
            CHECK(reg16(0x0340, h + 4));
        };
        // Frame numbers on the link: FS and FE of each frame.
        std::vector<std::pair<uint8_t, uint16_t>> marks;
        sensor.packet_observer = [&](uint8_t dt, uint16_t field) {
            if (dt == bfm::DT_FS || dt == bfm::DT_FE) marks.emplace_back(dt, field);
        };
        mode();
        CHECK(reg_write(0x0100, {1}));
        CHECK(take_frame(w, h, 10, [&](unsigned x, unsigned y) { return CcsSensor::pattern(1, x, y, 10); }));
        for (unsigned t = 0; t < 3000 && sensor.in_frame(); ++t) wait(100, SC_NS);
        for (unsigned t = 0; t < 3000 && !sensor.in_frame(); ++t) wait(100, SC_NS);
        CHECK(sensor.in_frame());
        CHECK(reg_write(0x0103, {1}));
        CHECK(sensor.in_frame());                        // the reset landed inside frame 2
        CHECK_EQ(read16(0x034C), 640u);                  // registers at reset values at once
        CHECK_EQ(read8(0x0100), 0u);
        CHECK(take_frame(w, h, 10, [&](unsigned x, unsigned y) { return CcsSensor::pattern(2, x, y, 10); }));
        const uint32_t raw = rd(kCsi + FX1_CSI2_IRQ_STATUS);
        CHECK_EQ(raw & ((1u << IRQ_FRAME_LEVEL_ERR_VC0) | (1u << IRQ_FRAME_SYNC_ERR_VC0) | (1u << IRQ_FRAME_NUM_SEQ_ERR)), 0u);
        for (unsigned t = 0; t < 3000 && sensor.streaming(); ++t) wait(100, SC_NS);
        CHECK(!sensor.streaming());
        wait(400, SC_US);                                // no frame after the reset
        CHECK_EQ(rd(kCsi + FX1_CSI2_FRAME_COUNT), 2u);
        CHECK_EQ(sensor.frames_sent(), 0u);              // frame 2 started before the reset
        CHECK_EQ(read8(0x0005), 0u);
        CHECK(!irq.read());
        // Stream again: frame numbers restart at 1.
        mode();
        CHECK(reg_write(0x0100, {1}));
        CHECK(take_frame(w, h, 10, [&](unsigned x, unsigned y) { return CcsSensor::pattern(1, x, y, 10); }));
        CHECK(reg_write(0x0100, {0}));
        for (unsigned t = 0; t < 3000 && sensor.streaming(); ++t) wait(100, SC_NS);
        CHECK_EQ(sensor.frames_sent(), 1u);
        // FS 1, FE 1, FS 2, FE 2 (the frame the reset hit), FS 1, FE 1.
        const std::vector<std::pair<uint8_t, uint16_t>> want = {
            {bfm::DT_FS, 1}, {bfm::DT_FE, 1}, {bfm::DT_FS, 2}, {bfm::DT_FE, 2}, {bfm::DT_FS, 1}, {bfm::DT_FE, 1}};
        CHECK(marks == want);
        for (const auto& [dt, n] : marks) std::printf("  %s %u\n", dt == bfm::DT_FS ? "FS" : "FE", n);
    }

    // Wait for the stop that follows a stop request and for the restart; the
    // clock lane must reach LP-11 in between.
    bool stop_then_restart() {
        bool stopped = false, lp11 = false;
        for (unsigned t = 0; t < 20000 && !stopped; ++t) {
            stopped = !sensor.streaming();
            if (!stopped) wait(50, SC_NS);
        }
        for (unsigned t = 0; t < 20000 && !sensor.streaming(); ++t) {
            lp11 |= (rd(kCsi + FX1_CSI2_PHY_STATUS) & (FX1_CSI2_PHY_STATUS_CL_RXCLKACTIVEHS_MASK |
                                                      FX1_CSI2_PHY_STATUS_CL_STOPSTATE_MASK)) ==
                    FX1_CSI2_PHY_STATUS_CL_STOPSTATE_MASK;
            wait(50, SC_NS);
        }
        return stopped && lp11 && sensor.streaming();
    }
    void next_frame_start() {
        for (unsigned t = 0; t < 4000 && sensor.in_frame(); ++t) wait(100, SC_NS);
        for (unsigned t = 0; t < 4000 && !sensor.in_frame(); ++t) wait(100, SC_NS);
    }

    void restart_before_fe() {
        const unsigned w = 64, h = 8;
        csi_up(4, w);
        std::vector<std::pair<uint8_t, uint16_t>> marks;
        sensor.packet_observer = [&](uint8_t dt, uint16_t field) {
            if (dt == bfm::DT_FS || dt == bfm::DT_FE) marks.emplace_back(dt, field);
        };
        auto mode = [&] {
            CHECK(reg16(0x034C, w));
            CHECK(reg16(0x034E, h));
            CHECK(reg16(0x0342, 8000));  // 40 us lines: 320 us of frame data for the I2C writes
            CHECK(reg16(0x0340, h + 2));
        };
        auto px = [&](unsigned f) { return [f](unsigned x, unsigned y) { return CcsSensor::pattern(f, x, y, 10); }; };
        mode();
        CHECK(reg_write(0x0100, {1}));
        CHECK(take_frame(w, h, 10, px(1), 1000));
        // A. Stop and start again inside frame 2: one physical stop, numbers go on.
        next_frame_start();
        CHECK(sensor.in_frame());
        CHECK(reg_write(0x0100, {0}));
        CHECK(reg_write(0x0100, {1}));
        CHECK(sensor.in_frame());
        CHECK(take_frame(w, h, 10, px(2), 1000));
        CHECK(stop_then_restart());
        CHECK(take_frame(w, h, 10, px(3), 1000));
        // B. Reset, reconfigure and start inside frame 4: frame 4 whole, stop,
        //    restart with the new latch at frame number 1.
        next_frame_start();
        CHECK(sensor.in_frame());
        CHECK(reg_write(0x0103, {1}));
        mode();
        CHECK(reg_write(0x0100, {1}));
        CHECK(sensor.in_frame());
        CHECK(take_frame(w, h, 10, px(4), 1000));
        CHECK(stop_then_restart());
        CHECK(take_frame(w, h, 10, px(1), 1000));
        CHECK(reg_write(0x0100, {0}));
        for (unsigned t = 0; t < 20000 && sensor.streaming(); ++t) wait(50, SC_NS);
        CHECK(!sensor.streaming());
        const std::vector<std::pair<uint8_t, uint16_t>> want = {
            {bfm::DT_FS, 1}, {bfm::DT_FE, 1}, {bfm::DT_FS, 2}, {bfm::DT_FE, 2}, {bfm::DT_FS, 3}, {bfm::DT_FE, 3},
            {bfm::DT_FS, 4}, {bfm::DT_FE, 4}, {bfm::DT_FS, 1}, {bfm::DT_FE, 1}};
        CHECK(marks == want);
        CHECK_EQ(sensor.frames_sent(), 1u);  // since the reset: the restarted frame
        CHECK_EQ(rd(kCsi + FX1_CSI2_FRAME_COUNT), 5u);
        CHECK_EQ(rd(kCsi + FX1_CSI2_FRAME_DROP_COUNT), 0u);
    }

    // The file does not hold a whole number of 32x2 frames: no frame at all.
    void raw_refused() {
        csi_up(4, 32);
        CHECK(reg16(0x034C, 32));
        CHECK(reg16(0x034E, 2));
        CHECK(reg16(0x0342, 2000));
        CHECK(reg16(0x0340, 8));
        CHECK(reg_write(0x0100, {1}));
        wait(300, SC_US);
        CHECK(!sensor.streaming());
        CHECK_EQ(sensor.frames_sent(), 0u);
        CHECK(sensor.error_count() >= 1);
        CHECK_EQ(rd(kCsi + FX1_CSI2_FRAME_COUNT), 0u);
        CHECK_EQ(rd(kCsi + FX1_CSI2_PACKET_COUNT), 0u);  // nothing on the link, no pattern
        CHECK_EQ(read8(0x0100), 1u);                       // mode_select stays 1
    }

    // Two 32x2 frames in the file. 32x3 does not fit (128 % 96): the stream
    // stops at the frame end; 32x2 again resumes it with the next file frame.
    void raw_mode_change() {
        const unsigned w = 32;
        csi_up(4, w);
        CHECK(reg16(0x034C, w));
        CHECK(reg16(0x034E, 2));
        CHECK(reg16(0x0342, 2000));
        CHECK(reg16(0x0340, 8));
        CHECK(reg_write(0x0100, {1}));
        CHECK(take_frame(w, 2, 10, [&](unsigned x, unsigned y) { return file_px(0, x, y); }));
        CHECK(reg16(0x034E, 3));  // lands before the next frame start (80 us period)
        wait(200, SC_US);
        CHECK(!sensor.streaming());
        CHECK_EQ(sensor.frames_sent(), 1u);
        CHECK(sensor.error_count() >= 1);
        CHECK_EQ(rd(kCsi + FX1_CSI2_FRAME_COUNT), 1u);
        CHECK(reg16(0x034E, 2));
        CHECK(take_frame(w, 2, 10, [&](unsigned x, unsigned y) { return file_px(1, x, y); }));
        CHECK(reg_write(0x0100, {0}));
    }

    void run() {
        const std::map<std::string, std::function<void()>> cases = {
            {"reset_mid_frame", [this] { reset_mid_frame(); }},
            {"restart_before_fe", [this] { restart_before_fe(); }},
            {"raw_short", [this] { raw_refused(); }},
            {"raw_partial", [this] { raw_refused(); }},
            {"raw_mode_change", [this] { raw_mode_change(); }},
            {"stream", [this] { stream(4, 10); }},
            {"raw12_2lane", [this] { stream(2, 12); }},
            {"stream_noncontinuous", [this] { stream(4, 10); }},
            {"controls", [this] { controls(); }},
            {"raw_file", [this] { raw_file(); }},
        };
        const auto it = cases.find(which);
        if (it == cases.end()) {
            std::fprintf(stderr, "unknown case %s\n", which.c_str());
            ++g_fail;
        } else {
            it->second();
        }
        sc_core::sc_stop();
    }
};
}  // namespace

int sc_main(int argc, char** argv) {
    sc_core::sc_set_time_resolution(1, sc_core::SC_FS);
    const std::string which = argc > 1 ? argv[1] : "stream";
    const std::string dir = argc > 2 ? argv[2] : ".";
    // RAW fixtures: `samples` 16-bit little-endian values laid out as 32x2
    // frames, value (f * 1000 + y * 100 + x) & 0x3FF; `extra` raw bytes after.
    auto write_raw = [&](const std::string& name, unsigned samples, unsigned extra = 0) {
        g_raw_path = dir + "/fx1_csi2_sensor_" + name + ".raw";
        std::ofstream f(g_raw_path, std::ios::binary | std::ios::trunc);
        for (unsigned i = 0; i < samples; ++i) {
            const unsigned fr = i / 64, y = i % 64 / 32, x = i % 32;
            const uint16_t v = uint16_t((fr * 1000 + y * 100 + x) & 0x3FF);
            f.put(char(v & 0xFF));
            f.put(char(v >> 8));
        }
        for (unsigned i = 0; i < extra; ++i) f.put(char(0x42));
    };
    if (which == "raw_file" || which == "raw_mode_change") write_raw(which, 128);  // two frames
    if (which == "raw_short") write_raw(which, 10);                                // < one frame
    if (which == "raw_partial") write_raw(which, 96);                              // 1.5 frames
    if (which == "raw_load_errors") {
        // Refused when the sensor is built: no simulation runs.
        for (auto [name, bytes] : {std::pair<const char*, unsigned>{"empty", 0}, {"odd", 1}, {"odd129", 129}}) {
            write_raw(name, 0, bytes);
            bool refused = false;
            try {
                Tb tb((std::string("tb_") + name).c_str(), which);
            } catch (const sc_core::sc_report& r) {
                refused = std::string(r.what()).find("RAW file") != std::string::npos;
                std::printf("  %s (%u bytes): %s\n", name, bytes, r.what());
            }
            CHECK(refused);
        }
        std::printf("%s: %s\n", which.c_str(), g_fail ? "FAIL" : "PASS");
        return g_fail ? 1 : 0;
    }
    Tb tb("tb", which);
    sc_core::sc_start(50, sc_core::SC_MS);
    if (sc_core::sc_get_status() != sc_core::SC_STOPPED) {
        std::printf("case did not finish\n");
        ++g_fail;
    }
    std::printf("%s: %s\n", which.c_str(), g_fail ? "FAIL" : "PASS");
    return g_fail ? 1 : 0;
}
