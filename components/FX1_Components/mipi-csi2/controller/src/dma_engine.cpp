#include "fx1_csi2/dma_engine.h"

#include <algorithm>

namespace fx1::csi2 {

namespace {
unsigned popcount8(uint8_t v) {
    unsigned n = 0;
    for (; v; v &= static_cast<uint8_t>(v - 1)) ++n;
    return n;
}
}  // namespace

DmaEngine::Capture* DmaEngine::find(uint16_t id) {
    if (id == 0) return nullptr;
    for (auto& c : caps_)
        if (c.id == id) return &c;
    return nullptr;
}

std::vector<std::pair<uint16_t, DmaOutcome>> DmaEngine::take_outcomes() {
    std::vector<std::pair<uint16_t, DmaOutcome>> v;
    v.swap(outcomes_);
    return v;
}

bool DmaEngine::busy() const {
    if (inflight_ > 0) return true;
    for (const auto& c : caps_)
        if (c.claimed && !c.finalised) return true;
    return false;
}

void DmaEngine::update_busy() {
    const bool b = busy();
    if (b != busy_) {
        busy_ = b;
        if (ev_) ev_->dma_busy(b);
    }
}

bool DmaEngine::quiescent() const {
    for (const auto& c : caps_)
        if (c.ended && !c.intake_done) return false;  // waits for the pipeline to drain (cycle_end)
    return outcomes_.empty();
}

// ---------------------------------------------------------------- reset

void DmaEngine::reset(bool full) {
    ++epoch_;
    // M13: a side being written is finalised without event: rewound to
    // HW_OWNED once every write issued before the reset has its response
    // (soft_reset); resetn / full_reset reset the ownership registers.
    if (!full)
        for (const auto& c : caps_)
            if (c.claimed && !c.finalised) {
                if (inflight_ > 0) stale_rewind_.push_back(c.side);
                else if (ev_) ev_->dma_set_buffer(c.side, BufState::HwOwned, 0);
            }
    if (full) {
        stale_rewind_.clear();
        last_done_side_ = 1;
    }
    caps_.clear();
    queue_.clear();
    fifo_beats_ = 0;
    intake_ = 0;
    outcomes_.clear();
    update_busy();
}

// ---------------------------------------------------------------- claim

bool DmaEngine::capture_start(uint16_t id, const DmaConfig& cfg, bool core_enable, bool tpg) {
    if (!cfg.enable) return false;  // M10: the DMA path does not take part
    Capture c;
    c.id = id;
    c.tpg = tpg;
    c.cfg = cfg;
    c.stride = uint32_t(cfg.bytes_per_line) + cfg.line_padding;
    const bool allowed = cfg.start && !cfg.abort && core_enable && !(ev_ && ev_->dma_claim_blocked());
    int side = -1;
    if (allowed && ev_) {
        // M11: the side after the last completed one, else the other; a side
        // still being written by an earlier capture is not free.
        for (unsigned k = 0; k < 2 && side < 0; ++k) {
            const unsigned s = (last_done_side_ + 1 + k) & 1;
            bool writing = false;
            for (const auto& o : caps_) writing |= o.claimed && !o.finalised && o.side == s;
            if (!writing && ev_->dma_buf_state(s) == BufState::HwOwned) side = static_cast<int>(s);
        }
    }
    // M04: base of the side and line stride aligned to the 8-byte beat.
    if (side >= 0 && ((cfg.base[side] & 7u) || (c.stride & 7u))) side = -1;
    if (side < 0) {
        c.finalised = true;  // refused: dropped (M10, M04, M11), its beats are discarded
        outcomes_.push_back({id, DmaOutcome::Refused});
        caps_.push_back(c);
        return true;
    }
    c.claimed = true;
    c.side = static_cast<unsigned>(side);
    caps_.push_back(c);
    if (ev_) {
        ev_->dma_set_buffer(c.side, BufState::HwOwned, id);  // FRAME_ID at the claim
        ev_->dma_set_active_buffer(c.side);
    }
    update_busy();
    return true;
}

void DmaEngine::capture_end(uint16_t id) {
    if (Capture* c = find(id)) c->ended = true;
}

// ---------------------------------------------------------------- abort

void DmaEngine::start_abort(Capture& c, Cause cause) {
    if (!c.claimed || c.finalised || c.abort != Cause::None) return;
    c.abort = cause;
    // M16 (1): no further write of this capture; data not issued is discarded.
    for (auto it = queue_.begin(); it != queue_.end();) {
        if (it->capture == c.id) {
            fifo_beats_ -= it->beats;
            it = queue_.erase(it);
        } else {
            ++it;
        }
    }
    c.queued = 0;
    if (c.has_open) {
        fifo_beats_ -= c.open.beats;
        c.has_open = false;
    }
    if ((cause == Cause::Layout || cause == Cause::Bounds || cause == Cause::Overflow) && ev_) ev_->dma_overflow(epoch_);
    try_finalise(c);  // (2) drain: waits for the write in flight, (3) finalise once
}

void DmaEngine::abort_all_ctl() {
    for (auto& c : caps_) start_abort(c, Cause::Ctl);
    update_busy();
}

void DmaEngine::abort_all_dma() {
    for (auto& c : caps_) start_abort(c, Cause::Dma);
    update_busy();
}

// ---------------------------------------------------------------- finalisation

void DmaEngine::try_finalise(Capture& c) {
    if (!c.claimed || c.finalised || !ev_) return;
    if (c.inflight > 0) return;
    if (c.abort == Cause::None) {
        if (!c.intake_done || c.queued > 0 || c.has_open) return;
        // M12: every write of the capture answered OKAY: SW_OWNED_VALID, then the event.
        ev_->dma_set_buffer(c.side, BufState::SwOwnedValid, c.id);
        ev_->dma_buf_done(true, epoch_);
        outcomes_.push_back({c.id, DmaOutcome::Valid});
    } else if (c.cfg.err_policy_release) {
        // M07 policy 1: release with error, BUFFER_ERROR_INFO before the event.
        ev_->dma_set_last_valid_line(c.side, c.last_valid);
        ev_->dma_set_buffer(c.side, BufState::SwOwnedError, c.id);
        ev_->dma_buf_done(false, epoch_);
        outcomes_.push_back({c.id, DmaOutcome::ErrorReleased});
    } else {
        // M07 policy 0: discard and rewind; the side stays HW_OWNED.
        ev_->dma_set_buffer(c.side, BufState::HwOwned, c.id);
        outcomes_.push_back({c.id, DmaOutcome::Rewound});
    }
    c.finalised = true;
    last_done_side_ = c.side;
}

// ---------------------------------------------------------------- intake

void DmaEngine::close_open(Capture& c) {
    if (!c.has_open) return;
    c.has_open = false;
    DmaBurst& b = c.open;
    // M05: before each burst, line offset + burst end within DMA_BUFFER_SIZE
    // (64-bit arithmetic); the address must also stay inside 32 bits.
    const uint64_t line_off = uint64_t(c.line) * c.stride;
    const uint64_t end = line_off + c.open_line_off + b.data.size();
    const uint64_t last_addr = uint64_t(c.cfg.base[c.side]) + end - 1;
    if (end > c.cfg.size || last_addr > 0xFFFFFFFFull) {
        fifo_beats_ -= b.beats;
        start_abort(c, Cause::Bounds);
        return;
    }
    if (queue_.size() >= p_.cmd_fifo) {
        fifo_beats_ -= b.beats;
        start_abort(c, Cause::Overflow);
        return;
    }
    queue_.push_back(b);
    ++c.queued;
}

bool DmaEngine::beat(bool valid, const OutBeat& in) {
    if (!valid) return true;
    // A beat of capture N: every beat of the captures opened before N has
    // passed (the pipeline keeps the order). Only those are closed, in Frame
    // Start order; a later capture whose Frame End has been decoded can still
    // have all its data behind this beat (review M4a F1). A beat whose capture
    // is unknown (orphan frame, or a capture already forgotten) closes nothing.
    Capture* c = find(in.tag);
    if (c)
        for (auto& o : caps_) {
            if (&o == c) break;
            if (o.ended && !o.intake_done) {
                o.intake_done = true;
                close_open(o);
                try_finalise(o);
            }
        }
    intake_ = in.tag;
    if (!c || !c->claimed || c->finalised || c->abort != Cause::None) {
        update_busy();
        return true;  // no buffer for this capture: accepted and discarded
    }
    const unsigned max_beats = std::min<unsigned>(unsigned(c->cfg.max_burst_len) + 1, p_.max_burst_beats);
    if (c->tpg && (fifo_beats_ >= p_.raw_fifo_beats || queue_.size() >= p_.cmd_fifo))
        return false;  // the generator can be stalled: never lose a beat on the TPG path
    c->started = true;
    const unsigned n = popcount8(in.keep);
    // M03: a line longer than bytes_per_line is a layout error.
    if (c->line_bytes + n > c->cfg.bytes_per_line) {
        start_abort(*c, Cause::Layout);
        update_busy();
        return true;
    }
    // Beat lost on a full RAW data FIFO (M20): the capture is aborted, the
    // DMA keeps fbd_ready high and discards the rest of it (M20).
    if (fifo_beats_ >= p_.raw_fifo_beats) {
        start_abort(*c, Cause::Overflow);
        update_busy();
        return true;
    }
    if (!c->has_open) {
        c->open = DmaBurst{};
        c->open.addr = uint64_t(c->cfg.base[c->side]) + uint64_t(c->line) * c->stride + c->line_bytes;
        c->open.side = c->side;
        c->open.capture = c->id;
        c->open.epoch = epoch_;
        c->open.line = c->line;
        c->open.qos = c->cfg.qos;
        c->open.prot = c->cfg.prot;
        c->open_line_off = c->line_bytes;
        c->has_open = true;
    }
    for (unsigned i = 0; i < n; ++i) c->open.data.push_back(static_cast<uint8_t>(in.data >> (8 * i)));
    ++c->open.beats;
    ++fifo_beats_;
    max_fifo_beats_ = std::max(max_fifo_beats_, fifo_beats_);
    c->line_bytes += n;
    // M06: close at max_burst_len + 1 beats, at a 4 KiB boundary and at the end of a line.
    const uint64_t next = c->open.addr + uint64_t(c->open.beats) * 8;
    if (in.eol) c->open.line_end = true;
    if (in.eol || c->open.beats >= max_beats || (next & 0xFFF) == 0) close_open(*c);
    if (in.eol && c->abort == Cause::None) {
        ++c->line;
        c->line_bytes = 0;
    }
    update_busy();
    return true;
}

void DmaEngine::cycle_end(bool pipe_empty) {
    if (pipe_empty)
        for (auto& c : caps_)
            if (c.ended && !c.intake_done) {
                c.intake_done = true;
                close_open(c);
                try_finalise(c);
            }
    // Forget captures that are over: finalised (or refused) and no beat to come.
    while (!caps_.empty() && caps_.front().intake_done && (caps_.front().finalised || !caps_.front().claimed))
        caps_.pop_front();
    update_busy();
}

// ---------------------------------------------------------------- worker side

bool DmaEngine::take_burst(DmaBurst& b) {
    if (queue_.empty()) return false;
    b = queue_.front();
    queue_.pop_front();
    fifo_beats_ -= b.beats;
    ++inflight_;
    ++issued_;
    if (Capture* c = find(b.capture)) {
        --c->queued;
        ++c->inflight;
    }
    return true;
}

void DmaEngine::burst_done(const DmaBurst& b, bool ok, uint8_t resp) {
    if (inflight_ > 0) --inflight_;
    if (b.epoch != epoch_) {
        // A write issued before a reset (M13): drained without event.
        if (inflight_ == 0) {
            for (unsigned s : stale_rewind_)
                if (ev_) ev_->dma_set_buffer(s, BufState::HwOwned, 0);
            stale_rewind_.clear();
        }
        update_busy();
        return;
    }
    Capture* c = find(b.capture);
    if (c) {
        if (c->inflight > 0) --c->inflight;
        if (!ok) {
            if (ev_) ev_->dma_axi_error(static_cast<uint8_t>(b.side), resp, static_cast<uint32_t>(b.addr), epoch_);
            start_abort(*c, Cause::Axi);
        } else if (b.line_end) {
            // M08: last line whose every write was OKAY, also when the answer
            // of a write already issued arrives after an abort (lines are
            // written in order, one write at a time).
            c->last_valid = b.line;
        }
        try_finalise(*c);
    }
    update_busy();
}

}  // namespace fx1::csi2
