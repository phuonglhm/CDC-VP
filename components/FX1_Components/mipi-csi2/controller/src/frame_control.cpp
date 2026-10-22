#include "fx1_csi2/frame_control.h"

namespace fx1::csi2 {

namespace {
unsigned popcount4(uint8_t v) { return (v & 1) + (v >> 1 & 1) + (v >> 2 & 1) + (v >> 3 & 1); }
template <typename T> T sat_inc(T v, T max) { return v == max ? v : static_cast<T>(v + 1); }
}  // namespace

// ================================================================ Block 9

void FrameContext::reset(bool full) {
    ch_ = {};
    slot_ = {};
    geo_ = {};
    emb_ = {};
    pub_ = {};
    last_alloc_ = 0;
    any_alloc_ = false;
    prev_active_sel_ = false;
    out_ = FcCycleOut{};
    if (full) frame_id_cnt_ = 0;  // the timestamp counter is the controller's (C07)
}

void FrameContext::begin_cycle(const FcConfig& cfg, uint64_t now_ns, bool capture_drop, uint8_t drop_vc_mask) {
    out_ = FcCycleOut{};
    cfg_ = cfg;
    now_ns_ = now_ns;
    if (cfg.active_sel != prev_active_sel_) {
        for (auto& s : slot_) s.wait_lag = false;
        prev_active_sel_ = cfg.active_sel;
    }
    if (capture_drop) {
        for (unsigned vc = 0; vc < 2; ++vc) {
            if (!(drop_vc_mask >> vc & 1)) continue;
            const Channel& c = ch_[vc];
            if (!c.orphan && slot_[c.slot].open) slot_[c.slot].dropped = true;
            else if (any_alloc_ && slot_[last_alloc_].open) slot_[last_alloc_].dropped = true;
        }
    }
    if (!cfg.core_enable) {
        // Channels detached without a record; every open capture ends as dropped.
        for (auto& c : ch_) {
            c.active = false;
            c.orphan = true;
        }
        for (auto& s : slot_)
            if (s.open) s.dropped = true;
    }
}

void FrameContext::open_channel(unsigned vc) {
    Geometry& g = geo_[vc];
    g = Geometry{};
    g.exp_w = cfg_.expected_width;  // sampled at ctx_open
    g.exp_h = cfg_.expected_height;
    Embedded& e = emb_[vc];
    e.words = 0;
    e.lines = 0;
    e.byte_ptr = 0;
}

void FrameContext::close_channel(unsigned vc, bool sync) {
    Channel& c = ch_[vc];
    const Geometry& g = geo_[vc];
    const Embedded& e = emb_[vc];
    const bool ev_rx = !cfg_.active_sel;
    const bool height_mismatch = g.line_count != g.exp_h;
    const bool abort = !c.orphan && slot_[c.slot].dropped;  // ctx_abort
    if (ev_rx && !c.orphan && !abort && !sync && (g.width_mismatch || height_mismatch)) out_.err_resolution[vc] = true;
    if (ev_rx && !c.orphan && !abort) {
        FrameInfo& p = pub_[vc];
        p.line_count = g.line_count;
        p.byte_count = g.byte_count_last;
        p.data_type = g.dt;
        p.width_mismatch = g.width_mismatch;
        p.height_mismatch = height_mismatch;
        p.emb_words = e.words;
        p.emb_lines = e.lines;
        p.extract = e.extract;
        out_.published[vc] = true;
    }
    c.active = false;
}

void FrameContext::frame_start(unsigned vc, uint16_t frame_number) {
    if (!cfg_.core_enable) return;  // no frame event is taken while core_enable is low
    vc &= 1;
    out_.ev_fs = true;
    out_.ev_vc = static_cast<uint8_t>(vc);
    Channel& c = ch_[vc];
    if (c.active) close_channel(vc, true);  // Frame End lost: ctx_sync_close
    const bool ev_rx = !cfg_.active_sel;
    c.fnum_err = false;
    if (ev_rx) {
        if (cfg_.fnum_check_en && frame_number != 0 && frame_number != 1 &&
            frame_number != static_cast<uint16_t>(c.last_fnum + 1)) {
            out_.err_frame_num[vc] = true;
            c.fnum_err = true;
        }
        c.last_fnum = frame_number;
    }
    if (vc == (cfg_.lead_vc & 1u)) {
        for (auto& s : slot_)
            if (s.open) s.wait_lag = false;
        // Allocate a free slot, alternating between the two.
        const unsigned first = any_alloc_ ? (last_alloc_ + 1) & 1 : 0;
        int free_slot = -1;
        for (unsigned i = 0; i < 2 && free_slot < 0; ++i)
            if (!slot_[(first + i) & 1].open) free_slot = static_cast<int>((first + i) & 1);
        if (free_slot >= 0) {
            Slot& s = slot_[free_slot];
            frame_id_cnt_ = static_cast<uint16_t>((frame_id_cnt_ + 1) & 0x3FFF);
            if (frame_id_cnt_ == 0) frame_id_cnt_ = 1;  // 0 is never assigned to a capture
            s = Slot{};
            s.open = true;
            s.wait_lag = cfg_.dual_vc;
            s.frame_id = frame_id_cnt_;
            s.timestamp = now_ns_;
            s.record_id = cfg_.record_id;
            out_.capture_start[free_slot] = true;
            last_alloc_ = static_cast<unsigned>(free_slot);
            any_alloc_ = true;
            c.slot = static_cast<uint8_t>(free_slot);
            c.orphan = false;
        } else {
            c.orphan = true;  // both slots in use
        }
    } else if (cfg_.dual_vc && any_alloc_ && slot_[last_alloc_].open && slot_[last_alloc_].wait_lag) {
        c.slot = static_cast<uint8_t>(last_alloc_);
        c.orphan = false;
        slot_[last_alloc_].wait_lag = false;
    } else {
        c.orphan = true;
    }
    c.active = true;
    open_channel(vc);
}

void FrameContext::frame_end(unsigned vc) {
    if (!cfg_.core_enable) return;
    vc &= 1;
    if (ch_[vc].active) close_channel(vc, false);  // a Frame End with no frame open is ignored
}

void FrameContext::image_beat(const PayloadBeat& b) {
    if (cfg_.active_sel || !cfg_.core_enable) return;  // ev_rx
    Geometry& g = geo_[b.vc & 1];
    if (b.sol) g.byte_acc = 0;
    g.byte_acc = static_cast<uint16_t>(g.byte_acc + popcount4(b.byte_en));
    if (b.eol) {
        g.byte_count_last = g.byte_acc;
        g.line_count = sat_inc<uint16_t>(g.line_count, 0xFFFF);
        g.dt = b.dt;
        const uint32_t w = g.exp_w;
        const uint32_t expect = b.raw10 ? (w * 5) >> 2 : (w * 3) >> 1;
        if (g.byte_count_last != expect) g.width_mismatch = true;  // sticky until ctx_open
    }
}

void FrameContext::emb_beat(const PayloadBeat& b) {
    if (cfg_.active_sel || !cfg_.core_enable || !cfg_.metadata_capture_en) return;
    Embedded& e = emb_[b.vc & 1];
    if (b.sol) e.byte_ptr = 0;
    const unsigned line_idx = e.lines;  // embedded lines completed so far in the frame
    e.words = sat_inc<uint16_t>(e.words, 0xFFFF);
    unsigned n = 0;
    for (unsigned i = 0; i < 4; ++i) {
        if (!(b.byte_en >> i & 1)) continue;
        const uint32_t p = e.byte_ptr + n++;
        if (!cfg_.emb_extract_en) continue;
        for (unsigned m = 0; m < 2; ++m) {
            const uint32_t d = cfg_.emb_cfg[m];
            unsigned len = d & 7;
            if (len > 4) len = 4;  // C32
            const uint32_t off = d >> 8 & 0xFFFF, line = d >> 24 & 0xFF;
            if (len == 0 || line != line_idx || p < off || p >= off + len) continue;
            if (p == off) e.extract[m] = 0;
            e.extract[m] |= uint32_t(b.bytes[i]) << (8 * (p - off));
        }
    }
    e.byte_ptr += n;
    if (b.eol) e.lines = sat_inc<uint8_t>(e.lines, 0xFF);
}

bool FrameContext::attached_active(unsigned s) const {
    for (const auto& c : ch_)
        if (!c.orphan && c.slot == s && c.active) return true;
    return false;
}

const FcCycleOut& FrameContext::end_cycle() {
    // A capture ends once neither of its channels is open; at most one slot per cycle.
    for (unsigned s = 0; s < 2; ++s) {
        Slot& sl = slot_[s];
        if (sl.open && !sl.wait_lag && !attached_active(s)) {
            out_.capture_end[s] = true;
            out_.capture_dropped[s] = sl.dropped;
            out_.capture_done = !sl.dropped;
            sl.open = false;
            break;
        }
    }
    return out_;
}

uint16_t FrameContext::cur_frame_id(unsigned vc) const {
    const Channel& c = ch_[vc & 1];
    return c.orphan ? 0 : slot_[c.slot].frame_id;
}

bool FrameContext::quiescent() const {
    for (unsigned s = 0; s < 2; ++s)
        if (slot_[s].open && !slot_[s].wait_lag && !attached_active(s)) return false;
    if (!cfg_.core_enable && (ch_[0].active || ch_[1].active || slot_[0].open || slot_[1].open)) return false;
    return true;
}

// ================================================================ Block 10

void ControlFsm::reset() {
    st_ = State::Idle;
    hold_cnt_ = 0;
    discard_ = 3;
    lead_seen_ = drop_now_q_ = core_enable_q_ = axis_enable_q_ = stream_stop_q_ = drop_pend_ = false;
    cap_open_ = {};
    dma_mark_ = {};
    last_slot_ = 0;
}

CtlOut ControlFsm::outputs(bool active_sel) const {
    CtlOut o;
    o.force_discard = discard_;
    o.lb_flush = o.fb_flush = st_ == State::Flush;
    o.rx_ctx_clear = st_ == State::Flush && !active_sel;
    o.asm_abort_req = st_ == State::Flush || stream_stop_q_;
    o.asm_abort_mask = 0x1;  // linear profile: the exposure in use is TID 0 (C29)
    o.capture_drop = drop_now_q_ && core_enable_q_;
    o.drop_vc_mask = 0x3;
    o.dma_abort_req = st_ == State::Hold && hold_cnt_ == 0;
    return o;
}

bool ControlFsm::quiescent(bool core_enable, bool axis_enable) const {
    if (drop_now_q_ || stream_stop_q_ || drop_pend_ || axis_enable != axis_enable_q_ || core_enable != core_enable_q_)
        return false;
    switch (st_) {
    case State::Idle: return !core_enable;
    case State::Run: return core_enable;
    case State::Skip: return core_enable && !lead_seen_;
    default: return false;
    }
}

bool ControlFsm::clock(const CtlIn& in) {
    // Cause Collector: sensor-path causes only while active_sel = 0; err_crc /
    // err_wc ignored on a channel already discarded.
    const bool rx = !in.active_sel;
    const bool c_ecc2 = rx && in.err_ecc2, c_lane = rx && in.err_lane_abort;
    const bool c_crc = rx && (in.err_crc & ~discard_ & 3);
    const bool c_wc = rx && ((in.err_wc & ~discard_ & 3) || in.err_wc_unknown);
    const bool c_lbfull = rx && in.lb_full, c_lbovf = rx && in.lb_overflow, c_fbovf = in.fb_overflow;
    const bool c_core = !in.core_enable;
    const bool abort_any = c_ecc2 || c_crc || c_wc || c_lane || c_lbfull || c_lbovf || c_fbovf || c_core;
    const bool midline = c_lbfull || c_lbovf || c_fbovf;
    const bool stream_stop = axis_enable_q_ && !in.axis_enable;

    // Capture Watch.
    const bool lead_start = in.capture_start[0] || in.capture_start[1];
    const bool cap_busy = cap_open_[0] || cap_open_[1] || lead_start;
    const bool lag_fs = in.ev_fs && (in.ev_vc & 1) != (in.lead_vc & 1);
    std::array<bool, 2> drop_end{};
    for (unsigned s = 0; s < 2; ++s) drop_end[s] = in.capture_end[s] && (in.capture_dropped[s] || dma_mark_[s]);
    const bool frame_drop_pulse = drop_end[0] || drop_end[1] || drop_pend_;
    drop_pend_ = drop_end[0] && drop_end[1];
    for (unsigned s = 0; s < 2; ++s) {
        if (in.capture_start[s]) dma_mark_[s] = false;
        else if (in.dma_drop && last_slot_ == s) dma_mark_[s] = true;
        cap_open_[s] = in.capture_start[s] ? true : in.capture_end[s] ? false : cap_open_[s];
    }
    if (in.capture_start[1]) last_slot_ = 1;
    else if (in.capture_start[0]) last_slot_ = 0;

    // Drop FSM.
    const bool drop_now = st_ == State::Run && abort_any;
    const bool go_run = st_ == State::Skip && in.core_enable && (lead_start || lead_seen_);
    const bool set = st_ == State::Idle || st_ == State::Hold || st_ == State::Flush || drop_now;
    const unsigned lead = in.lead_vc & 1u;
    uint8_t clr = 0;
    for (unsigned x = 0; x < 2; ++x)
        if ((go_run && lead == x) || (lag_fs && !(discard_ >> lead & 1) && lead != x)) clr |= uint8_t(1u << x);
    discard_ = set ? 3 : static_cast<uint8_t>(discard_ & ~clr & 3);
    if ((st_ == State::Hold || st_ == State::Flush) && lead_start) lead_seen_ = true;
    else if (st_ == State::Run) lead_seen_ = false;

    State nx = st_;
    switch (st_) {
    case State::Idle:
        if (in.core_enable) nx = State::Skip;
        break;
    case State::Run:
        if (abort_any && (cap_busy || midline)) {
            nx = State::Hold;
            hold_cnt_ = 0;
        } else if (abort_any && in.core_enable) {
            nx = State::Skip;
        } else if (!in.core_enable) {
            nx = State::Idle;
        }
        break;
    case State::Hold:
        if (hold_cnt_ == kHoldCycles - 1) nx = State::Flush;
        else ++hold_cnt_;
        break;
    case State::Flush:
        nx = in.core_enable ? State::Skip : State::Idle;
        break;
    case State::Skip:
        if (!in.core_enable) nx = State::Idle;  // taken first
        else if (lead_start || lead_seen_) nx = State::Run;
        break;
    }
    st_ = nx;
    drop_now_q_ = drop_now;
    core_enable_q_ = in.core_enable;
    axis_enable_q_ = in.axis_enable;
    stream_stop_q_ = stream_stop;
    return frame_drop_pulse;
}

}  // namespace fx1::csi2
