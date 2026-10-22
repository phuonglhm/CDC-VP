#include "fx1_csi2/packet_analyzer.h"

#include "fx1_csi2/csi2_protocol.h"

namespace fx1::csi2 {

void PacketAnalyzer::reset() {
    st_ = St::Idle;
    clear_contexts();
}

void PacketAnalyzer::clear_contexts() { ctx_ = {}; }

void PacketAnalyzer::push(const MergedWord& w, const PaConfig& cfg, PaEvents& ev) {
    if (w.is_abort_marker()) {
        // A burst discarded by Lane Management, at its position in the stream:
        // the frames open now are marked in error; the channel is unknown.
        for (auto& c : ctx_)
            if (c.frame_active) c.pkt_err_in_frame = true;
        ev.pa_error(PaError::LaneAbort, -1, 0);
        st_ = St::Idle;
        return;
    }
    switch (st_) {
    case St::Idle:
        // Words without sob after a retired packet (e.g. the trail bytes the
        // PHY delivers) are ignored until the next burst (C16).
        if (w.sob) header(w, cfg, ev);
        break;
    case St::Payload:
        payload(w, 0, ev);
        break;
    case St::Discard:
        if (w.eob) st_ = St::Idle;
        break;
    }
}

void PacketAnalyzer::header(const MergedWord& w, const PaConfig& cfg, PaEvents& ev) {
    // C21: a header is checked only when all four bytes were received; a
    // shorter burst cannot be trusted (bytes not received are no evidence for
    // the ECC). It is dropped, reported as a truncated packet on an unknown
    // channel, and marks the open frames as for a discarded burst.
    if ((w.byte_valid & 0xF) != 0xF) {
        for (auto& c : ctx_)
            if (c.frame_active) c.pkt_err_in_frame = true;
        ev.pa_error(PaError::Wc, -1, 0);
        st_ = w.eob ? St::Idle : St::Discard;
        return;
    }
    uint8_t h[4];
    for (unsigned i = 0; i < 4; ++i) h[i] = static_cast<uint8_t>(w.data >> (8 * i));
    const HeaderCheck hc = check_header(h);
    if (hc.result == EccResult::Uncorrectable) {
        ev.pa_error(PaError::Ecc2, -1, 0);
        st_ = w.eob ? St::Idle : St::Discard;  // packet length unknown
        return;
    }
    if (hc.result == EccResult::Corrected) ev.pa_error(PaError::Ecc1, (hc.header24 >> 6) & 3, ctx_[(hc.header24 >> 6) & 1].line_count);
    vc_ = (hc.header24 >> 6) & 3;
    dt_ = hc.header24 & 0x3F;
    const uint16_t f2 = static_cast<uint16_t>(hc.header24 >> 8);
    supported_ = (cfg.supported_vc_mask >> vc_) & 1;
    Context& c = ctx_[vc_ & 1];

    if (dt_ < 0x10) {  // short packet
        st_ = St::Idle;
        if (!supported_) {
            ev.pa_vc_unsupported();
            return;
        }
        if (dt_ == 0x00) {  // Frame Start
            if (c.frame_active) ev.pa_error(PaError::FrameSync, vc_, c.line_count);
            c = Context{};
            c.frame_active = true;
            ev.pa_frame_start(vc_, f2);
        } else if (dt_ == 0x01) {  // Frame End
            if (!c.frame_active) {
                ev.pa_error(PaError::FrameSync, vc_, c.line_count);
            } else {
                if (c.pkt_err_in_frame) ev.pa_error(PaError::FrameLevel, vc_, c.line_count);
                c.frame_active = false;
                ev.pa_frame_end(vc_, f2);
            }
        } else if (dt_ >= 0x08) {
            ev.pa_gsp(vc_, dt_, f2);
        }
        // 0x02/0x03 (LS/LE) ignored without error; 0x04-0x07 reserved, ignored (C16).
        return;
    }

    // Long packet.
    remaining_ = f2;
    crc_ = 0xFFFF;
    footer_ = 0;
    footer_n_ = 0;
    first_beat_ = true;
    image_ = embedded_ = forward_ = false;
    if (!supported_) {
        ev.pa_vc_unsupported();  // dropped, no error; ECC and PACKET_COUNT still apply
    } else if (cfg.metadata_capture_en && dt_ == cfg.metadata_dt) {
        embedded_ = true;
    } else if (dt_ == 0x10 || dt_ == 0x11) {
        ev.pa_null_blank();
    } else if (dt_ == 0x2B || dt_ == 0x2C) {
        image_ = true;
    } else {
        ev.pa_error(PaError::UnsupportedDt, vc_, c.line_count);
    }
    if (image_ || embedded_) {
        if (!c.frame_active) {
            ev.pa_error(PaError::FrameSync, vc_, c.line_count);  // long packet with no frame open: not forwarded
        } else {
            forward_ = true;
        }
    }
    // C38: the first image line of the frame is marked on the first payload
    // beat actually delivered; an image packet without payload (WC = 0) does
    // not consume it.
    // The line count of the channel advances with that beat too (C38).
    sof_pending_ = image_ && forward_ && !c.first_image_seen;
    st_ = St::Payload;
    if (w.eob) truncated(ev);  // burst ended on the header word: payload and/or footer missing (C22)
}

// End of burst before the packet (payload and CRC footer) is complete (C22):
// reported as a truncated packet; no CRC verdict exists, so it is not a CRC error.
void PacketAnalyzer::truncated(PaEvents& ev) {
    Context& c = ctx_[vc_ & 1];
    if (supported_) {
        ev.pa_error(PaError::Wc, vc_, c.line_count);
        if (c.frame_active) c.pkt_err_in_frame = true;
    }
    finish(ev);
}

void PacketAnalyzer::payload(const MergedWord& w, unsigned first_byte, PaEvents& ev) {
    Context& c = ctx_[vc_ & 1];
    PayloadBeat b;
    unsigned nb = 0;
    bool done = false, crc_bad = false;
    for (unsigned i = first_byte; i < 4 && !done; ++i) {
        if (!(w.byte_valid >> i & 1)) break;
        const uint8_t byte = static_cast<uint8_t>(w.data >> (8 * i));
        if (remaining_ > 0) {
            b.bytes[nb++] = byte;
            crc_ = crc16_update(crc_, byte);
            --remaining_;
        } else {
            footer_ |= uint16_t(byte) << (8 * footer_n_);
            if (++footer_n_ == 2) {
                crc_bad = footer_ != crc_ && supported_;
                done = true;
            }
        }
    }
    if (nb > 0 && forward_) {
        b.byte_en = static_cast<uint8_t>((1u << nb) - 1);
        b.sol = first_beat_;
        b.eol = remaining_ == 0;
        b.sof = sof_pending_ && first_beat_;
        if (b.sof) c.first_image_seen = true;
        if (image_ && first_beat_) ++c.line_count;
        b.vc = vc_;
        b.dt = dt_;
        b.embedded = embedded_;
        b.raw10 = dt_ == 0x2B;
        ev.pa_beat(b);
    }
    if (nb > 0) first_beat_ = false;
    // C23: the payload beat (with EOL) is published before the integrity
    // result, and packet_done last (MAS: err_crc not before bp_eol).
    if (done) {
        if (crc_bad) {
            ev.pa_error(PaError::Crc, vc_, c.line_count);
            if (c.frame_active) c.pkt_err_in_frame = true;
        }
        finish(ev);
        return;
    }
    if (w.eob) truncated(ev);
}

void PacketAnalyzer::finish(PaEvents& ev) {
    ev.pa_packet_done();
    st_ = St::Idle;
}

}  // namespace fx1::csi2
