// Packet Analyzer (MAS v1.3 Block No. 3): header ECC, payload CRC-16, Data
// Type routing, per-VC Frame Supervisor, generic short packets.
//
// Functional model of the sys_clk domain (spec_decisions C17): push() takes
// one entry of the asynchronous FIFO from Lane Management and produces at
// most one payload beat, as the cut-through RTL does per word. Plain C++, so
// protocol cases are unit-tested without the PHY.
#pragma once

#include <array>
#include <cstdint>

#include "fx1_csi2/lane_management.h"

namespace fx1::csi2 {

// One payload beat towards Byte-to-Pixel Unpacking (bp_*) or Frame Context &
// Metadata Capture (emb_*): the payload bytes of one 32-bit word, CRC removed.
struct PayloadBeat {
    std::array<uint8_t, 4> bytes{};
    uint8_t byte_en = 0;      // contiguous from bit 0 except on the first beat of no packet: always from bit 0
    bool sof = false;         // first image line of the frame (image only)
    bool sol = false, eol = false;  // first / last payload beat of the long packet
    uint8_t vc = 0;
    uint8_t dt = 0;
    bool embedded = false;    // emb_* interface, else bp_*
    bool raw10 = false;       // bp_dt_sel
};

enum class PaError { Ecc1, Ecc2, Crc, Wc, UnsupportedDt, FrameSync, FrameLevel, LaneAbort };

class PaEvents {
public:
    virtual ~PaEvents() = default;
    virtual void pa_beat(const PayloadBeat& b) = 0;
    // vc < 0: channel unknown (ECC2, lane abort). line: line count of the channel.
    virtual void pa_error(PaError e, int vc, unsigned line) = 0;
    virtual void pa_packet_done() = 0;  // long packet whose header passed ECC
    virtual void pa_frame_start(unsigned vc, uint16_t frame_number) = 0;
    virtual void pa_frame_end(unsigned vc, uint16_t frame_number) = 0;
    virtual void pa_gsp(uint8_t vc, uint8_t dt, uint16_t data) = 0;
    virtual void pa_null_blank() = 0;
    virtual void pa_vc_unsupported() = 0;
};

// VP-only read-only view of what the Packet Analyzer parses (bandwidth
// measurement, plan CSI_ISP_BW): every long-packet payload byte as it is
// received, before the forward decision of this analyzer and before any
// discard gate downstream, so a capture dropped in mid-frame still shows the
// bytes that arrived. Header and CRC footer bytes are not payload. Changes no
// event, state or timing; default: none.
enum class RxPacketClass {
    Image,          // RAW10 / RAW12 (DT 0x2B / 0x2C), channel supported
    Embedded,       // metadata DT with capture enabled
    NullBlank,      // DT 0x10 / 0x11
    UnsupportedDt,  // other long DT on a supported channel
    UnsupportedVc,  // channel not supported: dropped without error
};
class RxPayloadObserver {
public:
    virtual ~RxPayloadObserver() = default;
    // A long-packet header passed ECC: class, channel, DT, word count, and
    // whether its payload is forwarded (a frame of the channel is open).
    virtual void rx_long_header(RxPacketClass cls, unsigned vc, uint8_t dt, uint16_t wc, bool forwarded) = 0;
    virtual void rx_payload_bytes(unsigned n) = 0;  // payload bytes of that packet, as parsed
    // The packet ended: CRC footer checked (crc_ok) or burst ended early (truncated).
    virtual void rx_packet_end(bool truncated, bool crc_ok) = 0;
    // A header that could not be decoded (truncated or ECC uncorrectable):
    // its channel, type and length are unknown, its payload is not counted.
    virtual void rx_unclassified() = 0;
    virtual void rx_frame_start(unsigned vc, uint16_t frame_number) = 0;  // FS of a supported channel
    virtual void rx_frame_end(unsigned vc, uint16_t frame_number) = 0;    // FE closing an open frame
};

struct PaConfig {
    uint8_t metadata_dt = 0x12;
    bool metadata_capture_en = false;
    uint8_t supported_vc_mask = 0x1;  // D09 linear profile: {VC0}
};

class PacketAnalyzer {
public:
    void reset();                     // resetn / soft_reset: FSM idle, contexts cleared
    void clear_contexts();            // core_enable = 0
    void push(const MergedWord& w, const PaConfig& cfg, PaEvents& ev);

    struct Context {
        bool frame_active = false, first_image_seen = false, pkt_err_in_frame = false;
        uint16_t line_count = 0;
    };
    const Context& context(unsigned vc) const { return ctx_[vc & 1]; }
    void set_rx_observer(RxPayloadObserver* o) { rx_obs_ = o; }  // VP-only, see RxPayloadObserver

private:
    enum class St { Idle, Payload, Discard };
    void header(const MergedWord& w, const PaConfig& cfg, PaEvents& ev);
    void payload(const MergedWord& w, unsigned first_byte, PaEvents& ev);
    void finish(PaEvents& ev);
    void truncated(PaEvents& ev);

    St st_ = St::Idle;
    std::array<Context, 2> ctx_{};
    // current long packet
    uint8_t vc_ = 0, dt_ = 0;
    uint16_t remaining_ = 0;
    uint16_t crc_ = 0xFFFF;
    uint16_t footer_ = 0;
    unsigned footer_n_ = 0;
    bool supported_ = false, forward_ = false, image_ = false, embedded_ = false, first_beat_ = false;
    bool sof_pending_ = false;
    RxPayloadObserver* rx_obs_ = nullptr;
    bool rx_open_ = false;  // a long packet reported to rx_obs_ has not ended yet
};

}  // namespace fx1::csi2
