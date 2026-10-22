#include "fx1_csi2/lane_management.h"

namespace fx1::csi2 {

namespace {
bool decode_lanes(unsigned enc, unsigned& n) {
    switch (enc & 3) {
    case 0: n = 1; return true;
    case 1: n = 2; return true;
    case 3: n = 4; return true;
    default: return false;  // 10 reserved: not adopted (C13)
    }
}
}  // namespace

LaneManagement::LaneManagement(unsigned max_lanes, unsigned skew_depth, unsigned af_margin)
    : max_(max_lanes), depth_(skew_depth), margin_(af_margin) {}

void LaneManagement::clear_burst() {
    for (auto& f : fifo_) f.clear();
    sync_seen_.fill(false);
    deskew_seen_.fill(false);
    eot_.fill(false);
}

void LaneManagement::reset() {
    ++resets_;
    st_ = LmState::Idle;
    n_ = 1;
    n_enc_ = 0;
    clear_burst();
    pos_ = 0;
    have_pending_ = false;
    first_ = true;
}

void LaneManagement::emit(uint32_t data, uint8_t bv, bool eob, LmEvents& ev) {
    MergedWord w;
    w.data = data;
    w.byte_valid = bv;
    w.sob = first_;
    w.eob = eob;
    first_ = false;
    ev.lm_word(w);
}

// Lane Merging: bytes of one read cycle, lane order. A completed word is held
// one word so that the last word of the burst carries merged_eob; in flush
// mode (ST_DRAIN) a cycle without bytes means the FIFOs are drained.
void LaneManagement::merge(const uint8_t* b, unsigned k, bool flush, LmEvents& ev) {
    for (unsigned i = 0; i < k; ++i) {
        word_[pos_++] = b[i];
        if (pos_ == 4) {
            const uint32_t w = word_[0] | word_[1] << 8 | word_[2] << 16 | uint32_t(word_[3]) << 24;
            if (have_pending_) emit(pending_, 0xF, false, ev);
            pending_ = w;
            have_pending_ = true;
            pos_ = 0;
        }
    }
    if (flush && k == 0 && !packer_empty()) {
        if (pos_ > 0) {
            uint32_t w = 0;
            for (unsigned i = 0; i < pos_; ++i) w |= uint32_t(word_[i]) << (8 * i);
            if (have_pending_) emit(pending_, 0xF, false, ev);
            emit(w, static_cast<uint8_t>((1u << pos_) - 1), true, ev);
        } else {
            emit(pending_, 0xF, true, ev);
        }
        pos_ = 0;
        have_pending_ = false;
        first_ = true;
    }
}

void LaneManagement::clock(const PpiSample& in, unsigned active_lanes_enc, LmEvents& ev) {
    const LmState s = st_;
    const bool burst_clr = s == LmState::Idle || s == LmState::Abort;
    const bool wr_en_ok = s == LmState::Arming || s == LmState::Aligned;
    const bool rd_en_ok = s == LmState::Aligned || s == LmState::Drain;
    const bool flush = s == LmState::Drain;

    // Condition Reduce on the registered flags and the live inputs, masked by
    // the adopted lane set; lane slices above MAX_LANES are held inactive.
    bool burst_open = false, armed = n_ <= max_, any_sync = false, any_deskew = false, errsync = false, fifo_full = false;
    uint8_t sync_mask = 0;
    for (unsigned i = 0; i < n_; ++i) {
        if (!lane_on(i)) continue;
        burst_open |= in.active[i];
        armed &= sync_seen_[i];
        any_sync |= sync_seen_[i];
        any_deskew |= deskew_seen_[i];
        errsync |= in.errsync[i];
        fifo_full |= fifo_[i].size() >= depth_ - margin_;
        if (sync_seen_[i]) sync_mask |= 1u << i;
    }
    const bool burst_closed = !burst_open;
    const bool deskew_burst = any_deskew && !any_sync;

    // Read Control: one common read, each FIFO pops only when non-empty. A
    // lane that has finished (eot) does not block the final partial word.
    bool all_ready = true, any_data = false;
    for (unsigned i = 0; i < n_; ++i) {
        if (!lane_on(i)) continue;
        const bool ne = !fifo_[i].empty();
        any_data |= ne;
        all_ready &= ne || eot_[i];
    }
    uint8_t got[4];
    unsigned k = 0;
    if (rd_en_ok && all_ready && any_data) {
        for (unsigned i = 0; i < n_; ++i) {
            if (lane_on(i) && !fifo_[i].empty()) {
                got[k++] = fifo_[i].front();
                fifo_[i].pop_front();
            }
        }
    }
    merge(got, k, flush, ev);

    // Write Control: valid AND (sync_seen OR live sync) AND wr_en_ok.
    for (unsigned i = 0; i < n_; ++i) {
        if (!lane_on(i)) continue;
        if (in.valid[i] && (sync_seen_[i] || in.sync[i]) && wr_en_ok) {
            if (fifo_[i].size() < depth_) fifo_[i].push_back(in.data[i]);
            else ++dropped_;
        }
    }

    // Lane Event Capture, clear-dominant (MAS), except in the cycle that opens
    // a burst from ST_IDLE: this PHY asserts rxactivehs and rxsynchs on the
    // same rxbyteclkhs edge, so clearing then would lose the sync pulse and
    // abort every burst as unarmed (spec_decisions C18).
    const bool opening = s == LmState::Idle && burst_open;
    if (burst_clr && !opening) {
        sync_seen_.fill(false);
        deskew_seen_.fill(false);
        eot_.fill(false);
    } else {
        for (unsigned i = 0; i < n_; ++i) {
            if (!lane_on(i)) continue;
            sync_seen_[i] = sync_seen_[i] || in.sync[i];
            deskew_seen_[i] = deskew_seen_[i] || in.skewcal;
            if (sync_seen_[i] && !in.active[i]) eot_[i] = true;
        }
    }

    // Burst FSM.
    LmState next = s;
    switch (s) {
    case LmState::Idle:
        if (burst_open) next = LmState::Arming;
        break;
    case LmState::Arming:
        if (errsync) next = LmState::Abort;  // priority 1
        else if (armed) next = LmState::Aligned;  // priority 2
        else if (fifo_full || (burst_closed && !deskew_burst)) next = LmState::Abort;
        else if (burst_closed && deskew_burst) next = LmState::Idle;
        break;
    case LmState::Aligned:
        if (burst_closed) next = LmState::Drain;
        break;
    case LmState::Drain:
        if (k == 0 && packer_empty()) {
            bool empty = true;
            for (unsigned i = 0; i < n_; ++i) empty &= fifo_[i].empty();
            if (empty) next = LmState::Idle;
        }
        break;
    case LmState::Abort:
        if (burst_closed) next = LmState::Idle;
        break;
    }

    if (s == LmState::Arming && next == LmState::Abort) {
        ev.lm_abort(errsync, fifo_full, burst_closed && !deskew_burst, sync_mask);
        MergedWord m;  // abort marker: sob, eob, byte_valid 0000, data 0
        m.sob = m.eob = true;
        ev.lm_word(m);
    }
    if (s == LmState::Arming && next == LmState::Idle) ev.lm_deskew_burst();
    if (next == LmState::Idle || next == LmState::Abort) {
        for (auto& f : fifo_) f.clear();  // held cleared
        pos_ = 0;
        have_pending_ = false;
        first_ = true;
    }
    // cfg_apply: the lane count is adopted only in ST_IDLE.
    if (next == LmState::Idle) {
        unsigned n;
        if (decode_lanes(active_lanes_enc, n) && (n != n_ || (active_lanes_enc & 3) != n_enc_)) {
            n_ = n;
            n_enc_ = active_lanes_enc & 3;
            ev.lm_lanes_applied(n_enc_);
        }
    }
    if (next != s) ev.lm_state(next);
    st_ = next;
}

}  // namespace fx1::csi2
