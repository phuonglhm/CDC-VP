#include "fx1_csi2/pixel_path.h"

#include <algorithm>

namespace fx1::csi2 {

// ================================================================ Block 4

void ByteToPixel::reset() {
    carry_ = {};
    carry_cnt_ = 0;
    sof_pend_ = sol_pend_ = false;
    out_ = PxBeat{};
}

void ByteToPixel::clock(const PayloadBeat* bp) {
    const bool valid = bp != nullptr;
    if (valid) {
        raw10_ = bp->raw10;  // constant within a line (bp_dt_sel)
        vc_ = bp->vc;
    }
    const unsigned k = raw10_ ? 5 : 3;
    const bool line_start = valid && bp->sol, line_end = valid && bp->eol;
    // Work buffer: carry (not read on a line start) then the enabled bytes of
    // bp_data at their positions; every byte at or above work_cnt is 0.
    const unsigned carry_in = line_start ? 0 : carry_cnt_;
    std::array<uint8_t, 8> work{};
    for (unsigned i = 0; i < carry_in; ++i) work[i] = carry_[i];
    unsigned new_bytes = 0;
    if (valid)
        for (unsigned i = 0; i < 4; ++i)
            if (bp->byte_en >> i & 1) {
                work[carry_in + i] = bp->bytes[i];
                ++new_bytes;
            }
    const unsigned work_cnt = carry_in + new_bytes;
    const unsigned units = work_cnt / k, rest = work_cnt % k;
    const bool pad = line_end && units == 0;  // the end of a line always carries pixels
    const unsigned units_out = units + (pad ? 1 : 0);

    PxBeat o;
    o.vc = vc_;
    o.raw10 = raw10_;
    for (unsigned u = 0; u < units_out; ++u) {
        const uint8_t* b = &work[k * u];
        if (raw10_) {
            for (unsigned i = 0; i < 4; ++i) o.px[i] = static_cast<uint16_t>(b[i] << 2 | (b[4] >> (2 * i) & 3));
        } else {
            o.px[2 * u] = static_cast<uint16_t>(b[0] << 4 | (b[2] & 0xF));
            o.px[2 * u + 1] = static_cast<uint16_t>(b[1] << 4 | b[2] >> 4);
        }
    }
    o.count = static_cast<uint8_t>(units_out * (raw10_ ? 4 : 2));

    // Carry Buffer Update: the remainder never crosses a line end.
    carry_ = {};
    if (!line_end)
        for (unsigned i = 0; i < rest; ++i) carry_[i] = work[k * units + i];
    carry_cnt_ = line_end ? 0 : rest;

    // Sync Pending Logic: flags only on a cycle that emits pixels.
    const bool emit = units_out != 0;
    const bool sof_n = (valid && bp->sof) || sof_pend_;
    const bool sol_n = line_start || sol_pend_;
    o.sof = emit && sof_n;
    o.sol = emit && sol_n;
    o.eol = line_end;
    o.err_wc_align = line_end && (rest != 0 || units == 0);
    sof_pend_ = o.sof ? false : sof_n;
    sol_pend_ = o.sol ? false : sol_n;
    if (!emit) {
        o.px = {};
        o.count = 0;
    }
    out_ = o;
}

// ================================================================ Block 5

void TestPatternGenerator::reset() {
    st_ = State::Idle;
    x_ = y_ = 0;
    blank_cnt_ = vblank_lines_ = 0;
    eof_pend_ = false;
    // The shadows are re-latched in CALC_THRESHOLDS before the next frame.
}

bool TestPatternGenerator::quiescent(const TpgConfig& cfg, bool core_enable) const {
    return st_ == State::Idle && !(cfg.enable && core_enable && (cfg.width & ~3u) >= 8 && cfg.height >= 8);
}

bool TestPatternGenerator::px_valid() const {
    return (st_ == State::Running || st_ == State::Stopping) && blank_cnt_ == 0 && !eof_pend_;
}

PxBeat TestPatternGenerator::px() const {
    PxBeat o;
    o.count = 4;
    o.raw10 = raw10_;
    o.vc = 0;
    o.sof = x_ == 0 && y_ == 0;
    o.sol = x_ == 0;
    o.eol = x_ == w_ - 4;
    const unsigned b = (y_ & 1u) ^ (cfa_ >> 1 & 1u);  // odd row (Bayer Site Select)
    for (unsigned n = 0; n < 4; ++n) {
        const uint32_t coord = horizontal_ ? y_ : x_ + n;
        unsigned band = 0;
        for (uint32_t t : thr_) band += coord >= t;
        const bool r_on = !(band >> 1 & 1), g_on = !(band >> 2 & 1), b_on = !(band & 1);
        const unsigned a = (n & 1u) ^ (cfa_ & 1u);  // odd column
        const bool on = (!a && !b) ? r_on : (a && b) ? b_on : g_on;
        const uint16_t pix12 = on ? 0xFFF : 0;
        o.px[n] = raw10_ ? static_cast<uint16_t>(pix12 >> 2) : pix12;
    }
    return o;
}

TestPatternGenerator::Events TestPatternGenerator::clock(const TpgConfig& cfg, bool core_enable, bool px_ready,
                                                         bool linebuf_empty) {
    Events ev;
    const bool run = cfg.enable && core_enable;
    switch (st_) {
    case State::Idle:
        blank_cnt_ = vblank_lines_ = 0;
        eof_pend_ = false;
        // Dimension guard on the truncated width; level-sensitive start.
        if (run && linebuf_empty && (cfg.width & ~3u) >= 8 && cfg.height >= 8) st_ = State::Calc;
        return ev;
    case State::Calc: {
        w_ = static_cast<uint16_t>(cfg.width & ~3u);
        h_ = cfg.height;
        hblank_ = cfg.hblank;
        vblank_ = cfg.vblank;
        horizontal_ = cfg.horizontal;
        raw10_ = cfg.raw10;
        cfa_ = cfg.cfa & 3;
        const uint32_t base = horizontal_ ? (h_ >> 3) : (w_ >> 3);
        for (unsigned k = 0; k < 7; ++k) thr_[k] = (k + 1) * base;
        x_ = y_ = 0;
        eof_pend_ = false;
        st_ = State::Running;
        return ev;
    }
    case State::Running:
    case State::Stopping:
        break;
    }
    const bool accept = px_valid() && px_ready;
    bool line_done = false, frame_done = false;
    if (accept) {
        ev.fs = x_ == 0 && y_ == 0;
        if (x_ == w_ - 4) {
            line_done = true;
            x_ = 0;
            if (y_ == h_ - 1) {
                frame_done = true;
                ev.fe = true;
                y_ = 0;
            } else {
                ++y_;
            }
        } else {
            x_ = static_cast<uint16_t>(x_ + 4);
        }
    }
    ev.eof = frame_done;
    // Control FSM, on the registered state.
    State nx = st_;
    if (st_ == State::Running) {
        if (!run) nx = State::Stopping;
        else if (eof_pend_ && blank_cnt_ == 0 && linebuf_empty) nx = State::Calc;
    } else {
        if (run) nx = State::Running;
        else if (eof_pend_) nx = State::Idle;  // the cycle after the last beat; vertical blanking skipped
    }
    // Blanking Generator.
    const uint32_t period = (uint32_t(w_) >> 2) + hblank_;
    const uint32_t blank_nxt = line_done ? hblank_ : (blank_cnt_ ? blank_cnt_ - 1 : 0);
    const uint32_t vblank_nxt = frame_done ? vblank_ : vblank_lines_;
    const bool reload = blank_nxt == 0 && vblank_nxt != 0;
    blank_cnt_ = reload ? period : blank_nxt;
    vblank_lines_ = reload ? vblank_nxt - 1 : vblank_nxt;
    if (frame_done) eof_pend_ = true;
    st_ = nx;
    return ev;
}

// ================================================================ Block 6

void TpgMux::reset() {
    st_ = State::Streaming;
    active_ = armed_ = tpg_frame_active_ = false;
    vc_active_ = {};
}

bool TpgMux::quiescent(bool tpg_enable, bool lb_empty) const {
    const bool mismatch = tpg_enable != active_;
    switch (st_) {
    case State::Streaming: return !mismatch;
    case State::Draining: {
        // drain_done with neither source presenting a beat
        const bool rx_active = vc_active_[0] || vc_active_[1];
        const bool outgoing_active = active_ ? tpg_frame_active_ : rx_active;
        const bool incoming_idle = tpg_enable ? !tpg_frame_active_ : !rx_active;
        return !(!outgoing_active && incoming_idle && (!active_ || lb_empty));
    }
    case State::Acquiring: return !mismatch;  // waits for the first Start of Frame
    }
    return true;
}

bool TpgMux::tpg_ready(const In& in) const {
    // While RX is selected the generator is held at its first beat, except to
    // finish a frame it has already started.
    return active_ ? !in.lb_full : (!in.tpg_enable || tpg_frame_active_);
}

bool TpgMux::incoming_sof(const In& in) const {
    return active_ ? (in.tpg_valid && in.tpg.sof) : (in.rx && in.rx->count && in.rx->sof);
}

bool TpgMux::output(const In& in, PxBeat& out) const {
    if (armed_ && !incoming_sof(in)) return false;  // output_mask
    if (active_) {
        if (!(in.tpg_valid && tpg_ready(in))) return false;  // a TPG beat is forwarded once, when accepted
        out = in.tpg;
        out.count = 4;
        out.err_wc_align = false;
        return true;
    }
    if (!in.rx || in.rx->count == 0) return false;
    out = *in.rx;
    return true;
}

void TpgMux::clock(const In& in) {
    const bool rx_active = vc_active_[0] || vc_active_[1];
    const bool outgoing_active = active_ ? tpg_frame_active_ : rx_active;
    const bool outgoing_valid = active_ ? in.tpg_valid : (in.rx && in.rx->count);
    const bool outgoing_idle = !outgoing_active && !outgoing_valid;
    const bool incoming_idle = in.tpg_enable ? !tpg_frame_active_ : !rx_active;
    const bool mismatch = in.tpg_enable != active_;
    const bool drain_done = outgoing_idle && incoming_idle && (!active_ || in.lb_empty);
    const bool isof = incoming_sof(in);
    bool write_en = false;
    switch (st_) {
    case State::Streaming:
        if (mismatch) st_ = State::Draining;
        break;
    case State::Draining:
        if (drain_done) {
            write_en = true;
            st_ = State::Acquiring;
        }
        break;
    case State::Acquiring:
        if (isof) st_ = State::Streaming;  // evaluated first
        else if (mismatch) st_ = State::Draining;
        break;
    }
    armed_ = write_en ? true : isof ? false : armed_;
    if (write_en) active_ = in.tpg_enable;
    for (unsigned v = 0; v < 2; ++v) {
        if (in.fc_fs && in.fc_vc == v) vc_active_[v] = true;
        else if (in.rx_ctx_clear || (in.fc_fe && in.fc_vc == v)) vc_active_[v] = false;
    }
    tpg_frame_active_ = in.tpg_fs ? true : in.tpg_eof ? false : tpg_frame_active_;
}

// ================================================================ Block 7

LineBuffer::LineBuffer(unsigned depth, unsigned watermark) : depth_(depth), wm_(watermark), mem_(depth) {}

void LineBuffer::reset() {
    wr_ = rd_ = fill_ = 0;
    inflight_ = false;
    out_valid_ = skid_valid_ = false;
}

bool LineBuffer::clock(const PxBeat* wr, bool fb_ready) {
    // Write Control: the last slot is reserved for an end-of-line entry.
    const bool write_en = wr && wr->count > 0;
    const bool full_hard = fill_ == depth_, full_normal = fill_ + 1 >= depth_;
    const bool accept = write_en && (!full_normal || (wr->eol && !full_hard));
    // Read Control & FWFT output stage.
    const bool consume = out_valid_ && fb_ready;
    const int occupancy = int(out_valid_) + int(skid_valid_);
    const int space = 2 - occupancy + int(consume) - int(inflight_);
    const int unread = int(fill_) - occupancy - int(inflight_);
    const bool rd_en = unread > 0 && space > 0;
    if (consume) {  // release before absorption
        out_valid_ = skid_valid_;
        out_ = skid_;
        skid_valid_ = false;
    }
    if (inflight_) {  // r_data holds the entry addressed last cycle
        const PxBeat& r = mem_[inflight_addr_];
        if (!out_valid_) {
            out_ = r;
            out_valid_ = true;
        } else {
            skid_ = r;
            skid_valid_ = true;
        }
    }
    inflight_ = rd_en;
    if (rd_en) {
        inflight_addr_ = rd_;
        rd_ = (rd_ + 1) % depth_;
    }
    if (accept) {
        mem_[wr_] = *wr;
        wr_ = (wr_ + 1) % depth_;
    }
    fill_ = fill_ + (accept ? 1 : 0) - (consume ? 1 : 0);
    max_fill_ = std::max(max_fill_, fill_);
    return write_en && !accept;
}

}  // namespace fx1::csi2
