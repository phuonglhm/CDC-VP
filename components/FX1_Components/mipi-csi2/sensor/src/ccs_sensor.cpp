#include "fx1_csi2_sensor/ccs_sensor.h"

#include <algorithm>
#include <fstream>

#include "fx1_csi2_bfm/csi2_packet.h"

namespace fx1::csi2 {

CcsSensor::CcsSensor(sc_core::sc_module_name name, bfm::DphyTxBfm& tx, CcsSensorConfig cfg)
    : sc_core::sc_module(name), tx_(tx), cfg_(std::move(cfg)) {
    if (cfg_.max_lanes != 1 && cfg_.max_lanes != 2 && cfg_.max_lanes != 4)
        SC_REPORT_FATAL("fx1/csi2/sensor", "max_lanes must be 1, 2 or 4");
    if (!cfg_.raw_file.empty()) {
        std::ifstream f(cfg_.raw_file, std::ios::binary);
        if (!f) SC_REPORT_ERROR("fx1/csi2/sensor", ("cannot open RAW file " + cfg_.raw_file).c_str());
        std::vector<char> bytes((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        if (bytes.empty() || bytes.size() % 2)
            SC_REPORT_ERROR("fx1/csi2/sensor", ("RAW file " + cfg_.raw_file + " has " + std::to_string(bytes.size()) +
                                                " bytes: it must hold a whole, nonzero number of 16-bit samples")
                                                   .c_str());
        raw_.resize(bytes.size() / 2);
        for (size_t i = 0; i < raw_.size(); ++i)
            raw_[i] = static_cast<uint16_t>(uint8_t(bytes[2 * i]) | uint8_t(bytes[2 * i + 1]) << 8);
    }
    reset_registers();
    SC_THREAD(stream_thread);
}

void CcsSensor::reset_registers() {
    mode_select_ = 0;
    lane_mode_ = static_cast<uint8_t>(cfg_.max_lanes - 1);
    data_format_ = 0x0A0A;
    x_size_ = 640;
    y_size_ = 480;
    line_length_ = 640 + 128;
    frame_length_ = 480 + 20;
    frames_ = 0;
    fnum_ = 0;
    ++reset_epoch_;
    reg_ev_.notify(sc_core::SC_ZERO_TIME);
}

uint8_t CcsSensor::reg_read(uint16_t i) const {
    auto hi = [](uint16_t v) { return static_cast<uint8_t>(v >> 8); };
    auto lo = [](uint16_t v) { return static_cast<uint8_t>(v); };
    switch (i) {
    case 0x0000: return hi(kModelId);
    case 0x0001: return lo(kModelId);
    case 0x0002: return 0x01;
    case 0x0005: return static_cast<uint8_t>(frames_);
    case 0x0100: return mode_select_;
    case 0x0112: return hi(data_format_);
    case 0x0113: return lo(data_format_);
    case 0x0114: return lane_mode_;
    case 0x0340: return hi(frame_length_);
    case 0x0341: return lo(frame_length_);
    case 0x0342: return hi(line_length_);
    case 0x0343: return lo(line_length_);
    case 0x034C: return hi(x_size_);
    case 0x034D: return lo(x_size_);
    case 0x034E: return hi(y_size_);
    case 0x034F: return lo(y_size_);
    default: return 0;
    }
}

void CcsSensor::reg_write(uint16_t i, uint8_t v) {
    auto set_hi = [v](uint16_t& r) { r = static_cast<uint16_t>((r & 0x00FF) | uint16_t(v) << 8); };
    auto set_lo = [v](uint16_t& r) { r = static_cast<uint16_t>((r & 0xFF00) | v); };
    switch (i) {
    case 0x0100:
        if (!(v & 1) && streaming_) stop_req_ = true;  // latched: the stream stops even if 1 is written again
        mode_select_ = v & 1;
        break;
    case 0x0103:
        if (v & 1) {
            if (streaming_) stop_req_ = true;
            reset_registers();
        }
        break;
    case 0x0112: set_hi(data_format_); break;
    case 0x0113: set_lo(data_format_); break;
    case 0x0114: lane_mode_ = v & 3; break;
    case 0x0340: set_hi(frame_length_); break;
    case 0x0341: set_lo(frame_length_); break;
    case 0x0342: set_hi(line_length_); break;
    case 0x0343: set_lo(line_length_); break;
    case 0x034C: set_hi(x_size_); break;
    case 0x034D: set_lo(x_size_); break;
    case 0x034E: set_hi(y_size_); break;
    case 0x034F: set_lo(y_size_); break;
    default: break;  // read-only or unknown: ignored
    }
    reg_ev_.notify(sc_core::SC_ZERO_TIME);
}

// ---- I2C: first two bytes of a write set the index; data auto-increments.
bool CcsSensor::i2c_start(bool read) {
    if (!read) wbytes_ = 0;
    return true;
}

bool CcsSensor::i2c_write(uint8_t b) {
    if (wbytes_ == 0) index_ = static_cast<uint16_t>(uint16_t(b) << 8);
    else if (wbytes_ == 1) index_ = static_cast<uint16_t>(index_ | b);
    else reg_write(index_++, b);
    ++wbytes_;
    return true;
}

uint8_t CcsSensor::i2c_read(bool) { return reg_read(index_++); }

void CcsSensor::i2c_stop() {}

// ---- streaming

bool CcsSensor::latch_mode(Mode& m) {
    m.lanes = unsigned(lane_mode_) + 1;
    m.bits = data_format_ == 0x0A0A ? 10 : data_format_ == 0x0C0C ? 12 : 0;
    m.width = x_size_;
    m.height = y_size_;
    m.line_length = std::max<unsigned>(line_length_, m.width);
    m.frame_length = std::max<unsigned>(frame_length_, m.height);
    const unsigned unit = m.bits == 10 ? 4 : 2;
    const bool ok = (m.lanes == 1 || m.lanes == 2 || m.lanes == 4) && m.lanes <= cfg_.max_lanes && m.bits && m.width &&
                    m.height && m.width % unit == 0;
    if (!ok) {
        SC_REPORT_WARNING("fx1/csi2/sensor", "mode cannot be sent (lanes, csi_data_format or output size): no frame");
        return false;
    }
    const size_t frame_px = size_t(m.width) * m.height;
    if (!cfg_.raw_file.empty() && raw_.size() % frame_px) {
        SC_REPORT_WARNING("fx1/csi2/sensor", ("RAW file " + cfg_.raw_file + " holds " + std::to_string(raw_.size()) +
                                              " samples, not a whole number of " + std::to_string(m.width) + "x" +
                                              std::to_string(m.height) + " frames: no frame")
                                                 .c_str());
        return false;
    }
    return true;
}

// frame: the frame's CSI-2 number, 1 for the first frame after a reset.
std::vector<uint16_t> CcsSensor::line_pixels(const Mode& m, unsigned frame, unsigned y) const {
    std::vector<uint16_t> px(m.width);
    const size_t frame_px = size_t(m.width) * m.height;
    const size_t nframes = raw_.size() / frame_px;  // whole and nonzero: checked by latch_mode
    if (!cfg_.raw_file.empty()) {
        const size_t base = ((frame - 1) % nframes) * frame_px + size_t(y) * m.width;
        for (unsigned x = 0; x < m.width; ++x) px[x] = static_cast<uint16_t>(raw_[base + x] & ((1u << m.bits) - 1));
    } else {
        for (unsigned x = 0; x < m.width; ++x) px[x] = pattern(frame, x, y, m.bits);
    }
    return px;
}

void CcsSensor::send_packet(const std::vector<uint8_t>& pkt, unsigned lanes) {
    // The tap decodes the header actually sent: DI (VC, DT), then the 16-bit field.
    if (packet_observer && pkt.size() >= 4) packet_observer(pkt[0] & 0x3F, static_cast<uint16_t>(pkt[1] | pkt[2] << 8));
    const auto split = bfm::distribute_lanes(pkt, lanes);
    std::vector<bfm::LaneBurst> lb(lanes);
    for (unsigned i = 0; i < lanes; ++i) lb[i].bytes = split[i];
    tx_.send_burst(lb);
    wait(300, sc_core::SC_NS);  // LP-11 between bursts: end of transmission seen by the receiver
}

void CcsSensor::stream_thread() {
    for (;;) {
        while (!(mode_select_ & 1)) wait(reg_ev_);
        Mode m;
        if (!latch_mode(m)) {
            ++errors_;
            wait(reg_ev_);  // retried after the next register write
            continue;
        }
        streaming_ = true;
        bool clock_on = false;
        while ((mode_select_ & 1) && !stop_req_) {
            if (!clock_on) {
                tx_.clock_start();
                wait(1, sc_core::SC_US);
                clock_on = true;
            }
            // Everything this frame depends on is latched here: m (above or at
            // the previous frame end), its number and the reset epoch.
            const sc_core::sc_time line_t(double(m.line_length) / cfg_.pixel_rate_hz, sc_core::SC_SEC);
            const sc_core::sc_time t0 = sc_core::sc_time_stamp();
            fnum_ = fnum_ == 0xFFFF ? 1 : static_cast<uint16_t>(fnum_ + 1);  // CSI-2 frame number 1..65535
            const uint16_t fn = fnum_;
            const uint64_t epoch = reset_epoch_;
            const uint8_t dt = m.bits == 10 ? bfm::DT_RAW10 : bfm::DT_RAW12;
            in_frame_ = true;
            send_packet(bfm::short_packet(0, bfm::DT_FS, fn), m.lanes);
            for (unsigned y = 0; y < m.height; ++y) {
                const sc_core::sc_time tl = sc_core::sc_time_stamp();
                const auto px = line_pixels(m, fn, y);
                send_packet(bfm::long_packet(0, dt, m.bits == 10 ? bfm::pack_raw10(px) : bfm::pack_raw12(px)), m.lanes);
                const sc_core::sc_time next = tl + line_t;
                if (next > sc_core::sc_time_stamp()) wait(next - sc_core::sc_time_stamp());
            }
            send_packet(bfm::short_packet(0, bfm::DT_FE, fn), m.lanes);
            in_frame_ = false;
            if (epoch == reset_epoch_) ++frames_;  // not counted after a software_reset during the frame
            if (!cfg_.continuous_clock) {  // non-continuous clock between frames (P16)
                tx_.clock_stop();
                clock_on = false;
            }
            const sc_core::sc_time end = t0 + line_t * double(m.frame_length);
            if (end > sc_core::sc_time_stamp()) wait(end - sc_core::sc_time_stamp());
            // A stop request (mode_select 0, software_reset) takes effect here,
            // at the frame end, whatever mode_select reads now.
            if (!stop_req_ && (mode_select_ & 1) && !latch_mode(m)) {
                ++errors_;
                break;
            }
        }
        if (clock_on) tx_.clock_stop();
        streaming_ = false;
        stop_req_ = false;
        wait(standby_min());  // clock lane in LP-11 before any restart
    }
}

}  // namespace fx1::csi2
