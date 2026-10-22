#include "fx1_csi2/stream_out.h"

namespace fx1::csi2 {

// ================================================================ Block 8

FormatBridge::FormatBridge(unsigned stream_depth, unsigned dma_depth) : sdepth_(stream_depth), ddepth_(dma_depth) {}

void FormatBridge::reset() {
    rep_ = Formatter{};
    r16_ = Formatter{};
    dt_hold_raw10_ = err_hold_ = false;
    vc_hold_ = 0;
    tag_hold_ = 0;
    sfifo_.clear();
    dfifo_.clear();
}

bool FormatBridge::ready(const FbConfig& cfg) const {
    const bool flush_cycle = rep_.flush_req() || r16_.flush_req();
    const bool s_af = sdepth_ - stream_count() <= 1, d_af = ddepth_ - dma_count() <= 1;
    return !flush_cycle && !(cfg.axis_enable && s_af) && !(cfg.dma_enable && d_af);
}

bool FormatBridge::quiescent() const {
    return sfifo_.empty() && dfifo_.empty() && !rep_.flush_req() && !r16_.flush_req();
}

bool FormatBridge::Formatter::step(const uint8_t* bytes, unsigned n, bool in_sof, bool in_sol, bool in_eol,
                                   bool err_hold, uint8_t vc, uint16_t tag, OutBeat& beat) {
    if (flush_req()) {
        // Line-end flush, the cycle after the input beat that ended the line
        // (fb_ready is low, so there is no intake on this cycle).
        beat = OutBeat{};
        for (unsigned i = 0; i < cnt; ++i) beat.data |= uint64_t(buf[i]) << (8 * i);
        beat.keep = static_cast<uint8_t>((1u << cnt) - 1);
        beat.eol = true;
        beat.sof = sof_pend;
        beat.sol = sol_pend;
        beat.err = err_hold;
        beat.vc = vc;
        beat.tag = tag;
        buf = {};
        cnt = 0;
        sof_pend = sol_pend = eol_pend = false;
        return true;
    }
    const bool eol_n = in_eol || eol_pend, sof_n = in_sof || sof_pend, sol_n = in_sol || sol_pend;
    for (unsigned i = 0; i < n; ++i) buf[cnt + i] = bytes[i];  // new bytes above the residual
    const unsigned total = cnt + n;
    if (total < 8) {
        cnt = total;
        eol_pend = eol_n;
        sof_pend = sof_n;
        sol_pend = sol_n;
        return false;
    }
    beat = OutBeat{};
    for (unsigned i = 0; i < 8; ++i) beat.data |= uint64_t(buf[i]) << (8 * i);
    beat.keep = 0xFF;
    beat.eol = eol_n && total == 8;
    beat.sof = sof_n;
    beat.sol = sol_n;
    beat.err = err_hold && beat.eol;
    beat.vc = vc;
    beat.tag = tag;
    for (unsigned i = 8; i < total; ++i) buf[i - 8] = buf[i];
    for (unsigned i = total - 8; i < buf.size(); ++i) buf[i] = 0;
    cnt = total - 8;
    eol_pend = beat.eol ? false : eol_n;
    sof_pend = sol_pend = false;
    return true;
}

bool FormatBridge::clock(const FbConfig& cfg, const PxBeat* take, bool stream_pop, bool dma_pop) {
    // Shared line hold registers: an input beat that starts (ends) a line uses
    // its own Data Type / VC (error flag) on this cycle.
    bool raw10 = dt_hold_raw10_, err = err_hold_;
    uint8_t vc = vc_hold_;
    uint16_t tag = tag_hold_;
    if (take && take->sol) {
        raw10 = take->raw10;
        vc = take->vc;
        tag = take->tag;
        err = false;
    }
    if (take && take->eol) err = take->err_wc_align;

    uint8_t rep_bytes[6] = {}, r16_bytes[8] = {};
    unsigned rep_n = 0, r16_n = 0;
    if (take) {
        const auto& p = take->px;
        if (raw10) {
            if (take->count == 4) {  // RAW10 count 2 does not occur (intake 0, safe default)
                for (unsigned i = 0; i < 4; ++i) rep_bytes[i] = static_cast<uint8_t>(p[i] >> 2);
                rep_bytes[4] = static_cast<uint8_t>((p[3] & 3) << 6 | (p[2] & 3) << 4 | (p[1] & 3) << 2 | (p[0] & 3));
                rep_n = 5;
            }
        } else {
            for (unsigned u = 0; u < take->count / 2u; ++u) {
                rep_bytes[3 * u] = static_cast<uint8_t>(p[2 * u] >> 4);
                rep_bytes[3 * u + 1] = static_cast<uint8_t>(p[2 * u + 1] >> 4);
                rep_bytes[3 * u + 2] = static_cast<uint8_t>((p[2 * u + 1] & 0xF) << 4 | (p[2 * u] & 0xF));
            }
            rep_n = 3u * (take->count / 2u);
        }
        for (unsigned i = 0; i < take->count; ++i) {  // RAW16: {4'b0, pixel}, little endian
            r16_bytes[2 * i] = static_cast<uint8_t>(p[i]);
            r16_bytes[2 * i + 1] = static_cast<uint8_t>(p[i] >> 8 & 0x0F);
        }
        r16_n = 2u * take->count;
    }
    const bool in_sof = take && take->sof, in_sol = take && take->sol, in_eol = take && take->eol;
    OutBeat rep_beat, r16_beat;
    const bool rep_valid = rep_.step(rep_bytes, rep_n, in_sof, in_sol, in_eol, err, vc, tag, rep_beat);
    const bool r16_valid = r16_.step(r16_bytes, r16_n, in_sof, in_sol, in_eol, err, vc, tag, r16_beat);
    if (take) {
        dt_hold_raw10_ = raw10;
        vc_hold_ = vc;
        tag_hold_ = tag;
        err_hold_ = err;
    }

    if (stream_pop && !sfifo_.empty()) sfifo_.pop_front();
    if (dma_pop && !dfifo_.empty()) dfifo_.pop_front();
    bool overflow = false;
    auto write = [&](std::deque<OutBeat>& f, unsigned depth, const OutBeat& b) {
        if (f.size() >= depth) overflow = true;  // beat lost
        else f.push_back(b);
    };
    // Format Mux on the whole bundle; the stream path carries no sol.
    if (cfg.axis_enable && (cfg.stream_raw16 ? r16_valid : rep_valid)) {
        OutBeat b = cfg.stream_raw16 ? r16_beat : rep_beat;
        b.sol = false;
        write(sfifo_, sdepth_, b);
    }
    if (cfg.dma_enable && (cfg.dma_raw16 ? r16_valid : rep_valid)) write(dfifo_, ddepth_, cfg.dma_raw16 ? r16_beat : rep_beat);
    return overflow;
}

// ================================================================ Block 12

void AxisMaster::reset() {
    st_ = State::Passthrough;
    out_valid_ = skid_valid_ = false;
    line_open_ = false;
    line_tid_ = abort_mask_ = 0;
    pass_ = {true, true};
    abort_done_ = false;
}

bool AxisMaster::clock(bool tready, bool fbs_valid, const OutBeat& fbs, bool abort_req, uint8_t abort_mask,
                       bool axis_enable) {
    abort_done_ = false;
    const bool sink = st_ != State::Passthrough;
    const bool ready = fbs_ready();
    const bool s_out = out_valid_, s_skid = skid_valid_;
    // Linear profile (C29): TID = channel of the beat, vc_exposure_map not applied.
    const uint8_t tid = fbs.vc & 3;
    // Frame Gate: a pass bit changes only on a beat carrying sof.
    std::array<bool, 2> pass_next = pass_;
    if (fbs_valid && fbs.sof) {
        if (tid == 0) pass_next[0] = !sink;
        else if (tid == 1) pass_next[1] = !sink && pass_[0];
    }
    const bool gate_pass = tid < 2 ? pass_next[tid] : true;
    const bool taken = fbs_valid && ready;
    const bool in_valid = fbs_valid && axis_enable && gate_pass;
    const bool abort_start = st_ == State::Passthrough && abort_req;
    if (abort_start) pass_ = {false, false};
    else if (taken) pass_ = pass_next;

    const bool hs = s_out && tready;
    const bool abort_hs = st_ == State::Inject && tready;
    bool accepted = false;
    AxisBeat in{fbs.data, fbs.keep, fbs.eol, static_cast<uint8_t>((fbs.sof ? 1 : 0) | (fbs.err ? 2 : 0)), tid};
    if (st_ != State::Inject) {
        if (hs) out_valid_ = false;
        if (skid_valid_ && !out_valid_) {
            out_ = skid_;
            out_valid_ = true;
            skid_valid_ = false;
        }
        if (st_ == State::Passthrough && taken && in_valid) {  // sink mode discards every beat taken
            accepted = true;
            if (!out_valid_) {
                out_ = in;
                out_valid_ = true;
            } else {
                skid_ = in;
                skid_valid_ = true;
            }
        }
    }
    // Finalize Sequencer.
    const bool line_open_next = abort_hs ? false : (accepted ? !in.tlast : line_open_);
    if (accepted) line_tid_ = tid;
    const uint8_t mask_next =
        static_cast<uint8_t>((abort_mask_ | (abort_req ? abort_mask : 0)) & ~(abort_hs ? (1u << out_.tid) : 0u) & 3u);
    const bool todo = line_open_next || mask_next != 0;
    const uint8_t inj_tid = line_open_next ? line_tid_ : ((mask_next & 1) ? 0 : 1);
    const bool drained = !s_skid && (!s_out || tready);
    auto load_abort = [&] {
        out_ = AxisBeat{0, 0, true, 2, inj_tid};  // null transfer: TKEEP 0, TLAST, TUSER[1]
        out_valid_ = true;
    };
    switch (st_) {
    case State::Passthrough:
        if (abort_start) st_ = State::Drain;
        break;
    case State::Drain:
        if (drained) {
            if (todo) {
                load_abort();
                st_ = State::Inject;
            } else {
                st_ = State::Passthrough;
                abort_done_ = true;
            }
        }
        break;
    case State::Inject:
        if (abort_hs) {
            if (todo) {
                load_abort();
            } else {
                out_valid_ = false;
                st_ = State::Passthrough;
                abort_done_ = true;
            }
        }
        break;
    }
    line_open_ = line_open_next;
    abort_mask_ = mask_next;
    return taken;
}

}  // namespace fx1::csi2
