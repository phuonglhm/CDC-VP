#include "fx1_csi2/sys_datapath.h"

namespace fx1::csi2 {

SysDatapath::SysDatapath(SysParams p)
    : p_(p), lb_(p.line_buffer_depth, p.full_watermark), fb_(p.stream_fifo_depth, p.dma_fifo_depth), dma_(p.dma) {}

void SysDatapath::reset(bool full) {
    cdc_flush();
    pa_.reset();
    b2p_.reset();
    tpg_.reset();
    mux_.reset();
    lb_.reset();
    fb_.reset();
    axis_.reset();
    fc_.reset(full);
    ctl_.reset();
    dma_.reset(full);
    ledger_.clear();
    discard_ = 3;
    stop_hold_ = false;
    lb_full_q_ = lb_ovf_q_ = false;
}

void SysDatapath::core_disable() {
    pa_.reset();
    cdc_flush();
    stop_hold_ = true;
    dma_.abort_all_dma();  // M16: core_enable 1 -> 0 aborts a capture in flight on the DMA path
}

// M19: one outcome per capture. Dropped by Control FSM (capture_dropped) ->
// dropped; DMA path not taking part -> delivered; otherwise the DMA outcome:
// buffer released (valid or with error) -> delivered, rewound or no claim -> dropped.
void SysDatapath::resolve_ledger() {
    for (const auto& [id, out] : dma_.take_outcomes())
        for (auto& e : ledger_)
            if (e.id == id && e.dma && e.out == DmaOutcome::None) {
                e.out = out;
                break;
            }
    while (!ledger_.empty()) {
        LedgerEntry& e = ledger_.front();
        if (!e.ended || (e.dma && e.out == DmaOutcome::None)) break;  // in Frame Start order
        const bool dropped =
            e.ctl_dropped || (e.dma && (e.out == DmaOutcome::Rewound || e.out == DmaOutcome::Refused));
        if (sys_ev_) {
            if (dropped) sys_ev_->sys_frame_dropped();
            else sys_ev_->sys_capture_done();
        }
        ledger_.pop_front();
    }
}

// ---------------------------------------------------------------- CDC FIFO (C25)

void SysDatapath::cdc_flush() {
    cdc_.clear();
    cdc_dropping_ = cdc_marker_pending_ = false;
}

void SysDatapath::cdc_push(const MergedWord& w, uint64_t t_ps) {
    if (stop_hold_) return;  // C37: the FIFO is held flushed until the stop has completed
    // A burst that met a full FIFO is lost from that word to its end and is
    // replaced, at its position in the stream, by an abort marker.
    if (cdc_dropping_) {
        ++cdc_lost_;
        if (w.eob) {
            cdc_dropping_ = false;
            cdc_marker_pending_ = true;
        }
        return;
    }
    if (cdc_marker_pending_ && cdc_.size() < p_.cdc_depth) {
        MergedWord m;
        m.sob = m.eob = true;
        cdc_.push_back({m, t_ps, 0});
        cdc_marker_pending_ = false;
    }
    if (cdc_marker_pending_ || cdc_.size() >= p_.cdc_depth) {
        ++cdc_lost_;
        if (!cdc_marker_pending_ && sys_ev_) sys_ev_->sys_cdc_overflow();
        if (w.eob) cdc_marker_pending_ = true;
        else cdc_dropping_ = true;
        return;
    }
    cdc_.push_back({w, t_ps, 0});
    if (cdc_.size() > cdc_max_) cdc_max_ = static_cast<unsigned>(cdc_.size());
}

// ---------------------------------------------------------------- Packet Analyzer events

void SysDatapath::pa_beat(const PayloadBeat& b) {
    // discard_gate: the bit of force_discard of the packet's channel suppresses
    // bp_valid and emb_valid only.
    if (discard_ >> (b.vc & 1) & 1) return;
    if (b.embedded) {
        fc_.emb_beat(b);
    } else {
        bp_ = b;
        bp_valid_ = true;
        fc_.image_beat(b);
    }
    if (pa_ev_) pa_ev_->pa_beat(b);
}

void SysDatapath::pa_error(PaError e, int vc, unsigned line) {
    const uint8_t bit = vc >= 0 ? static_cast<uint8_t>(1u << (vc & 1)) : 0;
    switch (e) {
    case PaError::Ecc2: ctl_in_.err_ecc2 = true; break;
    case PaError::LaneAbort: ctl_in_.err_lane_abort = true; break;
    case PaError::Crc: ctl_in_.err_crc |= bit; break;
    case PaError::Wc:
        if (vc < 0) ctl_in_.err_wc_unknown = true;
        else ctl_in_.err_wc |= bit;
        break;
    default: break;  // ECC1, unsupported DT, frame sync / frame level: not drop causes
    }
    if (pa_ev_) pa_ev_->pa_error(e, vc, line);
}

void SysDatapath::pa_packet_done() {
    if (pa_ev_) pa_ev_->pa_packet_done();
}

void SysDatapath::pa_frame_start(unsigned vc, uint16_t n) {
    fc_fs_ = true;
    fc_vc_ = static_cast<uint8_t>(vc);
    if (!mux_.active_sel()) fc_.frame_start(vc, n);  // Event Source Select
    if (pa_ev_) pa_ev_->pa_frame_start(vc, n);
}

void SysDatapath::pa_frame_end(unsigned vc, uint16_t n) {
    fc_fe_ = true;
    fc_vc_ = static_cast<uint8_t>(vc);
    if (!mux_.active_sel()) fc_.frame_end(vc);
    if (pa_ev_) pa_ev_->pa_frame_end(vc, n);
}

void SysDatapath::pa_gsp(uint8_t vc, uint8_t dt, uint16_t data) {
    if (pa_ev_) pa_ev_->pa_gsp(vc, dt, data);
}

void SysDatapath::pa_null_blank() {
    if (pa_ev_) pa_ev_->pa_null_blank();
}

void SysDatapath::pa_vc_unsupported() {
    if (pa_ev_) pa_ev_->pa_vc_unsupported();
}

// ---------------------------------------------------------------- one sys_clk edge

void SysDatapath::cycle(const SysConfig& cfg, uint64_t now_ps, uint64_t now_ns) {
    ++cycles_;
    cfg_ = &cfg;
    for (auto& e : cdc_)
        if (e.t_ps < now_ps && e.edges < 255) ++e.edges;
    if (cdc_marker_pending_ && !cdc_dropping_ && cdc_.size() < p_.cdc_depth) {
        MergedWord m;
        m.sob = m.eob = true;
        cdc_.push_back({m, now_ps, 0});
        cdc_marker_pending_ = false;
    }

    // Registered state at the start of the cycle. A latched stop (C37) holds
    // core_enable low for the blocks of this domain until it has completed.
    const bool core_en = cfg.core_enable && !stop_hold_;
    if (stop_hold_) cdc_flush();
    const bool active_sel = mux_.active_sel();
    const CtlOut co = ctl_.outputs(active_sel);
    discard_ = co.force_discard;

    // Downstream handshakes, from the consumers back to Line Buffer.
    const bool tready = axis_sink_ ? axis_sink_->axis_cycle(axis_.tvalid(), axis_.beat()) : true;
    const bool fbs_valid = fb_.stream_valid();
    const OutBeat fbs = fbs_valid ? fb_.stream_head() : OutBeat{};
    const bool fbs_take = fbs_valid && axis_.fbs_ready();
    const bool fbd_valid = fb_.dma_valid();
    const OutBeat fbd = fbd_valid ? fb_.dma_head() : OutBeat{};
    if (co.dma_abort_req) dma_.abort_all_ctl();  // first cycle of ST_HOLD
    const bool fbd_ready = dma_.beat(fbd_valid, fbd);
    const bool fb_ready = fb_.ready(cfg.fb);
    const bool lb_take = lb_.out_valid() && fb_ready;
    const PxBeat lb_head = lb_.out();

    // Pixel sources and TPG Mux (combinational).
    const PxBeat rx_px = b2p_.out();
    TpgMux::In mi;
    mi.rx = &rx_px;
    mi.tpg_valid = tpg_.px_valid();
    if (mi.tpg_valid) mi.tpg = tpg_.px();
    mi.lb_full = lb_.full();
    mi.lb_empty = lb_.empty();
    mi.tpg_enable = cfg.tpg.enable;
    mi.rx_ctx_clear = co.rx_ctx_clear;
    const bool tpg_ready = mux_.tpg_ready(mi);
    PxBeat wr;
    const bool wr_valid = mux_.output(mi, wr);

    // Packet Analyzer: at most one word of the asynchronous FIFO per cycle.
    FcConfig fcc = cfg.fc;
    fcc.core_enable = core_en;
    fcc.active_sel = active_sel;
    fcc.dual_vc = false;  // linear profile (C28)
    fcc.lead_vc = 0;
    fc_.begin_cycle(fcc, now_ns, co.capture_drop, co.drop_vc_mask);
    ctl_in_ = CtlIn{};
    bp_valid_ = false;
    fc_fs_ = fc_fe_ = false;
    if (core_en && !cdc_.empty() && cdc_.front().edges >= p_.cdc_latency_cycles) {
        const MergedWord w = cdc_.front().w;
        cdc_.pop_front();
        pa_.push(w, cfg.pa, *this);
    }
    const TestPatternGenerator::Events tev = tpg_.clock(cfg.tpg, core_en, tpg_ready, mi.lb_empty);
    if (active_sel) {
        if (tev.fs) fc_.frame_start(0, 0);
        if (tev.fe) fc_.frame_end(0);
    }
    const FcCycleOut fo = fc_.end_cycle();

    // Register updates.
    axis_.clock(tready, fbs_valid, fbs, co.asm_abort_req, co.asm_abort_mask, cfg.fb.axis_enable);
    bool fb_ovf = false;
    if (co.fb_flush) fb_.reset();
    else fb_ovf = fb_.clock(cfg.fb, lb_take ? &lb_head : nullptr, fbs_take, fbd_valid && fbd_ready);
    bool lb_ovf = false;
    // M18: the entry carries the frame id of its capture (after this cycle's frame events).
    if (wr_valid) wr.tag = fc_.cur_frame_id(wr.vc & 1u);
    if (co.lb_flush) lb_.reset();
    else lb_ovf = lb_.clock(wr_valid ? &wr : nullptr, fb_ready);
    b2p_.clock(bp_valid_ ? &bp_ : nullptr);
    mi.fc_fs = fc_fs_;
    mi.fc_fe = fc_fe_;
    mi.fc_vc = fc_vc_;
    mi.tpg_fs = tev.fs;
    mi.tpg_eof = tev.eof;
    mux_.clock(mi);

    ctl_in_.lb_full = mi.lb_full;
    ctl_in_.lb_overflow = lb_ovf;
    ctl_in_.fb_overflow = fb_ovf;
    ctl_in_.core_enable = core_en;
    ctl_in_.axis_enable = cfg.fb.axis_enable;
    ctl_in_.active_sel = active_sel;
    ctl_in_.capture_start = fo.capture_start;
    ctl_in_.capture_end = fo.capture_end;
    ctl_in_.capture_dropped = fo.capture_dropped;
    ctl_in_.ev_fs = fo.ev_fs;
    ctl_in_.ev_vc = fo.ev_vc;
    ctl_in_.lead_vc = 0;
    ctl_.clock(ctl_in_);  // frame_drop_pulse: counting is done by the capture ledger (M19)
    // DMA claim and capture boundaries, ends before starts (a sync close and the
    // next lead Frame Start share a cycle).
    for (unsigned s = 0; s < 2; ++s)
        if (fo.capture_end[s]) {
            const uint16_t id = fc_.slot_frame_id(s);
            for (auto& e : ledger_)
                if (e.id == id && !e.ended) {
                    e.ended = true;
                    e.ctl_dropped = fo.capture_dropped[s];
                }
            dma_.capture_end(id);
        }
    for (unsigned s = 0; s < 2; ++s)
        if (fo.capture_start[s]) {
            LedgerEntry e;
            e.id = fc_.slot_frame_id(s);
            e.dma = dma_.capture_start(e.id, cfg.dma, core_en, active_sel);
            ledger_.push_back(e);
        }
    dma_.cycle_end(b2p_.out().count == 0 && lb_.empty() && fb_.dma_side_empty());
    if (sys_ev_)
        for (unsigned s = 0; s < 2; ++s)  // TIMESTAMP (C30) before any outcome of this cycle
            if (fo.capture_start[s]) sys_ev_->sys_capture_start(fc_.slot_frame_id(s), fc_.slot_timestamp(s));
    resolve_ledger();
    // The stop has completed once every capture has ended (as dropped), the
    // drop sequence has brought Control FSM to ST_IDLE and the stream master
    // has finished the abort it requested (its last abort beat accepted, or
    // nothing to terminate): Finalize Sequencer back in PASSTHROUGH. A stop
    // that issued no abort finds it there already.
    if (stop_hold_ && ctl_.state() == ControlFsm::State::Idle && !fc_.slot_open(0) && !fc_.slot_open(1) &&
        axis_.state() == AxisMaster::State::Passthrough)
        stop_hold_ = false;

    // Events towards the register file.
    if (!sys_ev_) return;
    const bool lbf = mi.lb_full && !active_sel;
    if (lbf && !lb_full_q_) sys_ev_->sys_line_buffer_full();
    lb_full_q_ = lbf;
    if (lb_ovf && !lb_ovf_q_) sys_ev_->sys_line_buffer_overflow();
    lb_ovf_q_ = lb_ovf;
    if (fb_ovf) sys_ev_->sys_fmtbridge_overflow();
    if (b2p_.out().count && b2p_.out().err_wc_align) sys_ev_->sys_err_wc_align();
    for (unsigned vc = 0; vc < 2; ++vc) {
        if (fo.err_frame_num[vc]) sys_ev_->sys_frame_num_err(vc);
        if (fo.published[vc]) sys_ev_->sys_published(vc, fc_.published(vc));
        if (fo.err_resolution[vc]) sys_ev_->sys_resolution_mismatch(vc);
    }
}

bool SysDatapath::quiescent(const SysConfig& cfg) const {
    if (stop_hold_ || !dma_.quiescent()) return false;
    return cdc_.empty() && !cdc_marker_pending_ && b2p_.quiescent() && tpg_.quiescent(cfg.tpg, cfg.core_enable) &&
           mux_.quiescent(cfg.tpg.enable, lb_.empty()) && lb_.empty() && fb_.quiescent() && axis_.quiescent() &&
           fc_.quiescent() && ctl_.quiescent(cfg.core_enable, cfg.fb.axis_enable) && !lb_full_q_ && !lb_ovf_q_;
}

}  // namespace fx1::csi2
