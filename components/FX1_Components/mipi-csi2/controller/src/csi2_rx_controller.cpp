#include "fx1_csi2/csi2_rx_controller.h"

#include <cstring>

// Field masks of the generated register map (positions only). Access types and
// reset values below are written by hand from the workbook text, so that the
// CSR contract test (driven by a table generated from the workbook) checks
// them independently.
#define FM(reg, field) FX1_CSI2_##reg##_##field##_MASK

namespace fx1::csi2 {

namespace {
bool is1(const sc_dt::sc_logic& v) { return v == sc_dt::SC_LOGIC_1; }

// RW bits per register.
constexpr uint32_t kCoreConfigRw = FM(CORE_CONFIG, CORE_ENABLE) | FM(CORE_CONFIG, SOFT_RESET);
constexpr uint32_t kProtocolRw = FM(PROTOCOL_CONFIG, ACTIVE_LANES) | FM(PROTOCOL_CONFIG, VC_EXPOSURE_MAP);
constexpr uint32_t kPhyRw = FM(PHY_CONFIG, PHY_ENABLE) | FM(PHY_CONFIG, CL_ENABLE) | FM(PHY_CONFIG, DL_ENABLE) |
                            FM(PHY_CONFIG, FORCERXMODE);
constexpr uint32_t kMetadataRw = FM(METADATA_DT_CONFIG, METADATA_DT) | FM(METADATA_DT_CONFIG, METADATA_CAPTURE_EN) |
                                 FM(METADATA_DT_CONFIG, FNUM_CHECK_EN) | FM(METADATA_DT_CONFIG, EMB_EXTRACT_EN);
constexpr uint32_t kStreamRw = FM(STREAM_CONFIG, AXIS_ENABLE) | FM(STREAM_CONFIG, STREAM_FORMAT);
constexpr uint32_t kDmaControlRw = FM(DMA_CONTROL, DMA_ENABLE) | FM(DMA_CONTROL, DMA_START) | FM(DMA_CONTROL, DMA_ABORT) |
                                   FM(DMA_CONTROL, DMA_FORMAT) | FM(DMA_CONTROL, ERR_FRAME_POLICY);
constexpr uint32_t kDmaAttrRw = FM(DMA_ATTR, AWQOS) | FM(DMA_ATTR, MAX_BURST_LEN) | FM(DMA_ATTR, AWPROT) |
                                FM(DMA_ATTR, MAX_OUTSTANDING);
constexpr uint32_t kLinePitchRw = FM(LINE_PITCH, BYTES_PER_LINE) | FM(LINE_PITCH, LINE_PADDING);
constexpr uint32_t kTpgControlRw = FM(TPG_CONTROL, TPG_ENABLE) | FM(TPG_CONTROL, PATTERN) |
                                   FM(TPG_CONTROL, TPG_DUAL_VC_EN) | FM(TPG_CONTROL, TPG_PIXEL_FORMAT);
constexpr uint32_t kEmbCfgRw = FM(EMB_EXTRACT_CFG0, LEN) | FM(EMB_EXTRACT_CFG0, BYTE_OFFSET) | FM(EMB_EXTRACT_CFG0, LINE_IDX);
constexpr uint32_t kIrqBits = (1u << IRQ_COUNT) - 1;

// Reset values (workbook "Memory Map" sheet), written out by hand.
constexpr uint32_t kCoreConfigReset = 0x00000001u;
constexpr uint32_t kProtocolReset = 0x00000003u;
constexpr uint32_t kPhyConfigReset = 0x00000078u;
constexpr uint32_t kMetadataReset = 0x00000012u;
constexpr uint32_t kStreamReset = 0x00000003u;
constexpr uint32_t kDmaControlReset = 0x00000010u;

// RO bits a datapath block may write through hw_set_ro, per register word.
uint32_t hw_owned_mask(uint32_t off) {
    switch (off) {
    case FX1_CSI2_PROTOCOL_CONFIG: return FM(PROTOCOL_CONFIG, ACTIVE_LANES_APPLIED);
    // Live levels of the sys_clk domain (line_buffer_full, active_src,
    // src_switch_pending, linebuf_fill) are read from the datapath, not stored.
    case FX1_CSI2_CORE_STATUS: return FM(CORE_STATUS, NULL_BLANK_SEEN) | FM(CORE_STATUS, VC_UNSUPPORTED_SEEN);
    case FX1_CSI2_BUFFER_STATUS: return FM(BUFFER_STATUS, LINEBUF_OVERFLOW) | FM(BUFFER_STATUS, META_OVERFLOW);
    case FX1_CSI2_LANE_ALIGN_STATUS:
        return FM(LANE_ALIGN_STATUS, LM_FSM_STATE) | FM(LANE_ALIGN_STATUS, ABORT_SYNC_ERR) |
               FM(LANE_ALIGN_STATUS, ABORT_FIFO_FULL) | FM(LANE_ALIGN_STATUS, ABORT_UNARMED) |
               FM(LANE_ALIGN_STATUS, DESKEW_BURST_SEEN) | FM(LANE_ALIGN_STATUS, ABORT_LANE_SYNC_SEEN);
    case FX1_CSI2_VC0_IMAGE_INFO1: case FX1_CSI2_VC1_IMAGE_INFO1:
        return FM(VC0_IMAGE_INFO1, BYTE_COUNT) | FM(VC0_IMAGE_INFO1, LINE_COUNT);
    case FX1_CSI2_VC0_IMAGE_INFO2: case FX1_CSI2_VC1_IMAGE_INFO2:
        return FM(VC0_IMAGE_INFO2, DATA_TYPE) | FM(VC0_IMAGE_INFO2, WIDTH_MISMATCH) | FM(VC0_IMAGE_INFO2, HEIGHT_MISMATCH);
    case FX1_CSI2_EMBEDDED_DATA_INFO_VC0:
        return FM(EMBEDDED_DATA_INFO_VC0, EMBEDDED_WORD_COUNT_VC0) | FM(EMBEDDED_DATA_INFO_VC0, EMBEDDED_LINE_COUNT_VC0);
    case FX1_CSI2_EMBEDDED_DATA_INFO_VC1:
        return FM(EMBEDDED_DATA_INFO_VC1, EMBEDDED_WORD_COUNT_VC1) | FM(EMBEDDED_DATA_INFO_VC1, EMBEDDED_LINE_COUNT_VC1);
    case FX1_CSI2_DMA_STATUS: return FM(DMA_STATUS, ACTIVE_BUFFER);
    case FX1_CSI2_BUFFER_ERROR_INFO:
        return FM(BUFFER_ERROR_INFO, BUF0_LAST_VALID_LINE) | FM(BUFFER_ERROR_INFO, BUF1_LAST_VALID_LINE);
    case FX1_CSI2_ERROR_CAPTURE:
        return FM(ERROR_CAPTURE, FAULT_LINE_NUMBER) | FM(ERROR_CAPTURE, FAULT_FRAME_ID) | FM(ERROR_CAPTURE, FAULT_VC);
    case FX1_CSI2_AXI_FAULT_TRACE: return FM(AXI_FAULT_TRACE, AWID) | FM(AXI_FAULT_TRACE, RESP);
    case FX1_CSI2_AXI_FAULT_ADDR:
    case FX1_CSI2_EMB_EXTRACT_VC0_VAL0: case FX1_CSI2_EMB_EXTRACT_VC0_VAL1:
    case FX1_CSI2_EMB_EXTRACT_VC1_VAL0: case FX1_CSI2_EMB_EXTRACT_VC1_VAL1:
        return 0xFFFFFFFFu;
    default: return 0;
    }
}

bool byte_enabled(const tlm::tlm_generic_payload& tx, unsigned i) {
    const unsigned char* be = tx.get_byte_enable_ptr();
    const unsigned n = tx.get_byte_enable_length();
    return be == nullptr || be[i % n] == TLM_BYTE_ENABLED;
}

uint32_t sat_add(uint32_t a, uint32_t n, uint32_t max) { return (max - a < n) ? max : a + n; }
}  // namespace

Csi2RxController::Csi2RxController(sc_core::sc_module_name nm, DphyRxWrapper& phy, Csi2ControllerConfig cfg)
    : sc_core::sc_module(nm),
      w_(phy),
      cfg_(std::move(cfg)),
      adapter_("phy_adapter", phy, cfg_.phy),
      lm_(cfg_.max_lanes),
      sys_(cfg_.sys) {
    if (cfg_.max_lanes != 1 && cfg_.max_lanes != 2 && cfg_.max_lanes != 4)
        SC_REPORT_FATAL("fx1/csi2/ctrl", "MAX_LANES must be 1, 2 or 4");
    if (cfg_.exposure_count != 1)
        SC_REPORT_FATAL("fx1/csi2/ctrl", "only the linear profile (exposure_count = 1) is built (D09)");
    csr_socket.register_b_transport(this, &Csi2RxController::b_transport);
    csr_socket.register_transport_dbg(this, &Csi2RxController::transport_dbg);
    if (cfg_.sys_clk_period <= sc_core::SC_ZERO_TIME || cfg_.mm_clk_period <= sc_core::SC_ZERO_TIME)
        SC_REPORT_FATAL("fx1/csi2/ctrl", "sys_clk and mm_aclk periods must be positive");
    sys_.set_pa_events(&glue_);
    sys_.set_sys_events(&sys_glue_);
    sys_.dma().set_events(&dma_glue_);
    reset_registers();
    SC_METHOD(irq_method);
    sensitive << irq_ev_;
    SC_METHOD(resetn_method);
    sensitive << resetn;
    SC_METHOD(byteclk_monitor);
    sensitive << w_.rxbyteclkhs.posedge_event();
    dont_initialize();
    SC_METHOD(stopstate_monitor);
    for (auto& s : w_.stopstatedata) sensitive << s;
    dont_initialize();
    SC_THREAD(sys_thread);
    SC_THREAD(dma_thread);
}

// ---------------------------------------------------------------- reset

void Csi2RxController::reset_registers() {
    core_config_ = kCoreConfigReset;
    protocol_config_ = kProtocolReset;
    const uint32_t built = (1u << cfg_.max_lanes) - 1;
    const uint32_t lane_rw = (built << FX1_CSI2_PHY_CONFIG_DL_ENABLE_SHIFT) | (built << FX1_CSI2_PHY_CONFIG_FORCERXMODE_SHIFT);
    phy_config_ = kPhyConfigReset & ~((FM(PHY_CONFIG, DL_ENABLE) | FM(PHY_CONFIG, FORCERXMODE)) & ~lane_rw);
    metadata_dt_config_ = kMetadataReset;
    stream_config_ = kStreamReset;
    expected_resolution_ = global_irq_enable_ = irq_enable_ = metadata_record_id_ = 0;
    dma_control_ = kDmaControlReset;
    dma_buf0_ = dma_buf1_ = dma_size_ = dma_attr_ = line_pitch_ = 0;
    tpg_control_ = tpg_resolution_ = tpg_blanking_ = emb_cfg0_ = emb_cfg1_ = 0;
    ro_.fill(0);
    irq_status_ = 0;
    irq_set_valid_.fill(false);
    cnt_ = Counters{};
    gsp_.clear();
    ts_lo_ = ts_hi_ = ts_hi_shadow_ = 0;
    buf_ = {BufState::HwOwned, BufState::HwOwned};
    lane_sot_err_ = lane_sot_sync_err_ = 0;
    update_sys_config();
}

// soft_reset scope (C10): datapath status, counters, IRQ_STATUS, FIFO and
// latched capture context are cleared; configuration, the timestamp counter,
// ownership of the ping-pong sides (M15) with their FRAME_ID and
// BUFFER_ERROR_INFO, and the PHY-derived live fields are kept.
void Csi2RxController::clear_datapath_status() {
    const uint32_t keep_frame_id = ro_[FX1_CSI2_FRAME_ID / 4];
    const uint32_t keep_err_info = ro_[FX1_CSI2_BUFFER_ERROR_INFO / 4];
    const uint32_t keep_dma_status = ro_[FX1_CSI2_DMA_STATUS / 4];
    ro_.fill(0);
    ro_[FX1_CSI2_FRAME_ID / 4] = keep_frame_id;
    ro_[FX1_CSI2_BUFFER_ERROR_INFO / 4] = keep_err_info;
    ro_[FX1_CSI2_DMA_STATUS / 4] = keep_dma_status;
    irq_status_ = 0;
    irq_set_valid_.fill(false);
    cnt_ = Counters{};
    gsp_.clear();
    ts_lo_ = ts_hi_ = ts_hi_shadow_ = 0;
    lane_sot_err_ = lane_sot_sync_err_ = 0;
}

void Csi2RxController::start_reset(bool full) {
    ++epoch_;
    sys_.reset(full);  // sys_clk domain (the frame_id counter survives soft_reset)
    sys_wake_ev_.notify(sc_core::SC_ZERO_TIME);
    if (full) {
        lm_.reset();  // asynchronous reset path, no byte clock needed
        bc_reset_pending_ = false;
        reset_registers();
        ts_base_ = sc_core::sc_time_stamp();
        apply_phy_config();       // phy_enable = 0 after the register reset
        adapter_.request_reset();
    } else {
        clear_datapath_status();
        bc_reset_pending_ = true;  // C20: Lane Management resets on the next byte-clock edge
    }
    disable_drain_pending_ = false;  // a reset supersedes a pending abort
    if (capture_in_flight_ || reset_drain_pending_) {  // a new reset keeps an unfinished drain
        reset_drain_pending_ = true;
        drain_req_ev_.notify(sc_core::SC_ZERO_TIME);
    }
    update_irq();
}

void Csi2RxController::disable_cause() {
    if (capture_in_flight_ && !disable_drain_pending_ && !reset_drain_pending_) {
        disable_drain_pending_ = true;
        drain_req_ev_.notify(sc_core::SC_ZERO_TIME);
    }
}

void Csi2RxController::hw_drain_complete() {
    reset_drain_pending_ = false;
    disable_drain_pending_ = false;
}

bool Csi2RxController::reset_in_progress() const {
    // C20: a soft reset is complete once the byte-clock domain has applied it;
    // with the byte clock stopped it cannot be, and is not waited for (MAS).
    const bool bc_wait = bc_reset_pending_ && is1(w_.rxclkactivehs.read());
    // C37: a core_enable 1 -> 0 still being processed in the sys_clk domain.
    return (core_config_ & FM(CORE_CONFIG, SOFT_RESET)) || adapter_.reset_pulse_active() || reset_drain_pending_ ||
           disable_drain_pending_ || bc_wait || sys_.stop_pending();
}

bool Csi2RxController::suppressed() const {
    return in_reset_ || (core_config_ & FM(CORE_CONFIG, SOFT_RESET)) || adapter_.reset_pulse_active();
}

void Csi2RxController::resetn_method() {
    if (!resetn.read()) {
        if (!in_reset_) {
            in_reset_ = true;
            ++epoch_;
            reset_registers();
            lm_.reset();
            sys_.reset(true);
            bc_reset_pending_ = false;
            // Writes already issued cannot be recalled (M13): a capture in
            // flight turns into a reset drain that outlives the reset pin
            // until the DMA reports completion; an abort drain is superseded.
            disable_drain_pending_ = false;
            if (capture_in_flight_ || reset_drain_pending_) {
                reset_drain_pending_ = true;
                drain_req_ev_.notify(sc_core::SC_ZERO_TIME);
            }
            apply_phy_config();
            adapter_.request_reset();
        }
    } else {
        // Release (or a model started out of reset): the power-up PHY reset pulse.
        in_reset_ = false;
        ts_base_ = sc_core::sc_time_stamp();
        adapter_.request_reset();
    }
    sys_wake_ev_.notify(sc_core::SC_ZERO_TIME);
    update_irq();
}

void Csi2RxController::apply_phy_config() {
    const uint32_t v = phy_config_;
    adapter_.set_control(v & FM(PHY_CONFIG, PHY_ENABLE), v & FM(PHY_CONFIG, CL_ENABLE),
                         static_cast<uint8_t>((v & FM(PHY_CONFIG, DL_ENABLE)) >> FX1_CSI2_PHY_CONFIG_DL_ENABLE_SHIFT),
                         static_cast<uint8_t>((v & FM(PHY_CONFIG, FORCERXMODE)) >> FX1_CSI2_PHY_CONFIG_FORCERXMODE_SHIFT));
}

// ---------------------------------------------------------------- interrupts

void Csi2RxController::hw_raise_irq(unsigned bit, uint32_t source_epoch) {
    if (bit >= IRQ_COUNT) {
        SC_REPORT_ERROR("fx1/csi2/ctrl", "IRQ bit out of range");
        return;
    }
    if (source_epoch != epoch_ || suppressed()) return;  // C08
    irq_status_ |= 1u << bit;
    irq_set_time_[bit] = sc_core::sc_time_stamp();
    irq_set_valid_[bit] = true;
    update_irq();
}

void Csi2RxController::irq_method() {
    const bool on = !in_reset_ && (global_irq_enable_ & FM(GLOBAL_IRQ_ENABLE, GLOBAL_ENABLE)) &&
                    (irq_status_ & irq_enable_ & kIrqBits);
    irq.write(on);
}

void Csi2RxController::byteclk_monitor() {
    // soft_reset reaches Lane Management in the rxbyteclkhs domain: it acts on
    // byte-clock edges only (MAS Block 2); a request shorter than a byte-clock
    // period is held until an edge applies it (C20).
    if (in_reset_ || (core_config_ & FM(CORE_CONFIG, SOFT_RESET)) || bc_reset_pending_) {
        lm_.reset();
        bc_reset_pending_ = false;
        return;
    }
    const unsigned enabled = adapter_.applied_lane_mask() & ((1u << cfg_.max_lanes) - 1);
    const bool quiet = suppressed();
    PpiSample in;
    in.skewcal = is1(w_.rxskewcalhs.read());
    for (unsigned i = 0; i < cfg_.max_lanes; ++i) {
        const sc_dt::sc_logic v = w_.rxvalidhs[i].read();
        in.valid[i] = is1(v);
        in.active[i] = is1(w_.rxactivehs[i].read());
        in.sync[i] = is1(w_.rxsynchs[i].read());
        in.errsync[i] = is1(w_.errsotsynchs[i].read());
        const sc_dt::sc_lv<8> d = w_.rxdatahs[i].read();
        if (d.is_01()) in.data[i] = static_cast<uint8_t>(d.to_uint());
        if ((enabled >> i & 1) && (!(v == sc_dt::SC_LOGIC_0 || v == sc_dt::SC_LOGIC_1) || (in.valid[i] && !d.is_01())))
            ++ppi_x_;  // X/Z on valid or on valid data: counted, treated as 0
        if (quiet || !(enabled >> i & 1)) continue;
        if (is1(w_.errsoths[i].read())) {
            lane_sot_err_ |= 1u << i;
            hw_raise_irq(IRQ_SOT_ERROR);
        }
        if (in.errsync[i]) {
            lane_sot_sync_err_ |= 1u << i;
            hw_raise_irq(IRQ_SOT_SYNC_ERROR);
        }
    }
    lm_.clock(in, protocol_config_ & FM(PROTOCOL_CONFIG, ACTIVE_LANES), glue_);
}

// ---------------------------------------------------------------- LM / PA event routing

// Configuration of the sys_clk domain, rebuilt after every CSR write (the MAS
// delivers each multi-bit field into sys_clk as a complete value).
void Csi2RxController::update_sys_config() {
    SysConfig& c = sys_cfg_;
    const uint32_t md = metadata_dt_config_;
    c.core_enable = core_config_ & FM(CORE_CONFIG, CORE_ENABLE);
    c.pa.metadata_dt = static_cast<uint8_t>(md & FM(METADATA_DT_CONFIG, METADATA_DT));
    c.pa.metadata_capture_en = md & FM(METADATA_DT_CONFIG, METADATA_CAPTURE_EN);
    c.pa.supported_vc_mask = 0x1;  // D09 linear profile
    c.fc.metadata_capture_en = c.pa.metadata_capture_en;
    c.fc.fnum_check_en = md & FM(METADATA_DT_CONFIG, FNUM_CHECK_EN);
    c.fc.emb_extract_en = md & FM(METADATA_DT_CONFIG, EMB_EXTRACT_EN);
    c.fc.expected_width = static_cast<uint16_t>(expected_resolution_);
    c.fc.expected_height = static_cast<uint16_t>(expected_resolution_ >> 16);
    c.fc.record_id = metadata_record_id_;
    c.fc.emb_cfg = {emb_cfg0_, emb_cfg1_};
    c.fb.axis_enable = stream_config_ & FM(STREAM_CONFIG, AXIS_ENABLE);
    c.fb.stream_raw16 = !(stream_config_ & FM(STREAM_CONFIG, STREAM_FORMAT));  // workbook: 0 = RAW16 (C27)
    c.fb.dma_enable = dma_control_ & FM(DMA_CONTROL, DMA_ENABLE);
    c.fb.dma_raw16 = dma_control_ & FM(DMA_CONTROL, DMA_FORMAT);  // M01: 1 = RAW16
    c.tpg.enable = tpg_control_ & FM(TPG_CONTROL, TPG_ENABLE);
    c.tpg.horizontal = tpg_control_ & FM(TPG_CONTROL, PATTERN);
    c.tpg.raw10 = tpg_control_ & FM(TPG_CONTROL, TPG_PIXEL_FORMAT);
    c.tpg.width = static_cast<uint16_t>(tpg_resolution_);
    c.tpg.height = static_cast<uint16_t>(tpg_resolution_ >> 16);
    c.tpg.hblank = static_cast<uint16_t>(tpg_blanking_);
    c.tpg.vblank = static_cast<uint16_t>(tpg_blanking_ >> 16);
    c.tpg.cfa = 0;  // no field in workbook v1.4 (D11)
    DmaConfig& d = c.dma;
    d.enable = dma_control_ & FM(DMA_CONTROL, DMA_ENABLE);
    d.start = dma_control_ & FM(DMA_CONTROL, DMA_START);
    d.abort = dma_control_ & FM(DMA_CONTROL, DMA_ABORT);
    d.err_policy_release = dma_control_ & FM(DMA_CONTROL, ERR_FRAME_POLICY);
    d.base = {dma_buf0_, dma_buf1_};
    d.size = dma_size_;
    d.bytes_per_line = static_cast<uint16_t>((line_pitch_ & FM(LINE_PITCH, BYTES_PER_LINE)) >> FX1_CSI2_LINE_PITCH_BYTES_PER_LINE_SHIFT);
    d.line_padding = static_cast<uint8_t>((line_pitch_ & FM(LINE_PITCH, LINE_PADDING)) >> FX1_CSI2_LINE_PITCH_LINE_PADDING_SHIFT);
    d.qos = static_cast<uint8_t>((dma_attr_ & FM(DMA_ATTR, AWQOS)) >> FX1_CSI2_DMA_ATTR_AWQOS_SHIFT);
    d.max_burst_len = static_cast<uint8_t>((dma_attr_ & FM(DMA_ATTR, MAX_BURST_LEN)) >> FX1_CSI2_DMA_ATTR_MAX_BURST_LEN_SHIFT);
    d.prot = static_cast<uint8_t>((dma_attr_ & FM(DMA_ATTR, AWPROT)) >> FX1_CSI2_DMA_ATTR_AWPROT_SHIFT);
    d.max_outstanding = static_cast<uint8_t>((dma_attr_ & FM(DMA_ATTR, MAX_OUTSTANDING)) >> FX1_CSI2_DMA_ATTR_MAX_OUTSTANDING_SHIFT);
}

uint64_t Csi2RxController::timestamp_ns() const {
    return static_cast<uint64_t>((sc_core::sc_time_stamp() - ts_base_).to_seconds() * 1e9 + 0.5);
}

// D05: most recent error (C33). Frame and line from Frame Context: the
// capture the channel belongs to and its completed lines; for an unknown
// channel (fault_vc = 3) the newest capture and line 0xFFFF (MAS Block 11).
void Csi2RxController::error_capture(int vc) {
    const FrameContext& fc = sys_.frame_context();
    const uint16_t fid = vc < 0 ? fc.newest_frame_id() : fc.cur_frame_id(static_cast<unsigned>(vc));
    const uint32_t line = vc < 0 ? 0xFFFFu : fc.cur_line(static_cast<unsigned>(vc));
    const uint32_t v = (line & 0xFFFFu) | (uint32_t(fid & 0x3FFF) << FX1_CSI2_ERROR_CAPTURE_FAULT_FRAME_ID_SHIFT) |
                       (uint32_t(vc < 0 ? 3 : vc & 3) << FX1_CSI2_ERROR_CAPTURE_FAULT_VC_SHIFT);
    if (!suppressed()) ro_[FX1_CSI2_ERROR_CAPTURE / 4] = v;
}

void Csi2RxController::Glue::lm_word(const MergedWord& w) {
    if (!(c.core_config_ & FM(CORE_CONFIG, CORE_ENABLE))) return;  // CDC FIFO held flushed
    c.sys_.cdc_push(w, static_cast<uint64_t>(sc_core::sc_time_stamp() / sc_core::sc_time(1, sc_core::SC_PS)));
    c.sys_wake_ev_.notify(sc_core::SC_ZERO_TIME);
}

void Csi2RxController::Glue::lm_abort(bool sync_err, bool fifo_full, bool unarmed, uint8_t lane_sync_seen) {
    const uint32_t causes = FM(LANE_ALIGN_STATUS, ABORT_SYNC_ERR) | FM(LANE_ALIGN_STATUS, ABORT_FIFO_FULL) |
                            FM(LANE_ALIGN_STATUS, ABORT_UNARMED) | FM(LANE_ALIGN_STATUS, ABORT_LANE_SYNC_SEEN);
    const uint32_t v = (sync_err ? FM(LANE_ALIGN_STATUS, ABORT_SYNC_ERR) : 0) |
                       (fifo_full ? FM(LANE_ALIGN_STATUS, ABORT_FIFO_FULL) : 0) |
                       (unarmed ? FM(LANE_ALIGN_STATUS, ABORT_UNARMED) : 0) |
                       (uint32_t(lane_sync_seen & 0xF) << FX1_CSI2_LANE_ALIGN_STATUS_ABORT_LANE_SYNC_SEEN_SHIFT);
    if (c.suppressed()) return;
    c.hw_set_ro(FX1_CSI2_LANE_ALIGN_STATUS, causes, v);
    c.hw_count(Counter::LaneAbort);
    c.hw_raise_irq(IRQ_LANE_ALIGN_ABORT);
}

void Csi2RxController::Glue::lm_deskew_burst() {
    c.hw_set_ro(FX1_CSI2_LANE_ALIGN_STATUS, FM(LANE_ALIGN_STATUS, DESKEW_BURST_SEEN), ~0u);
}

void Csi2RxController::Glue::lm_state(LmState s) {
    c.hw_set_ro(FX1_CSI2_LANE_ALIGN_STATUS, FM(LANE_ALIGN_STATUS, LM_FSM_STATE), static_cast<uint32_t>(s));
}

void Csi2RxController::Glue::lm_lanes_applied(unsigned enc) {
    c.hw_set_ro(FX1_CSI2_PROTOCOL_CONFIG, FM(PROTOCOL_CONFIG, ACTIVE_LANES_APPLIED),
                enc << FX1_CSI2_PROTOCOL_CONFIG_ACTIVE_LANES_APPLIED_SHIFT);
}

void Csi2RxController::Glue::pa_beat(const PayloadBeat& b) {
    if (c.sink_) c.sink_->beat(b);
}

void Csi2RxController::Glue::pa_error(PaError e, int vc, unsigned) {
    switch (e) {
    case PaError::Ecc1:
        c.hw_raise_irq(IRQ_ECC_1BIT);
        c.hw_count(Counter::Ecc1);
        break;
    case PaError::Ecc2:
        c.hw_raise_irq(IRQ_ECC_2BIT);
        c.hw_count(Counter::Ecc2);
        c.error_capture(-1);
        break;
    case PaError::Crc:
        c.hw_raise_irq(IRQ_CRC_ERROR);
        c.hw_count(Counter::Crc);
        c.error_capture(vc);
        break;
    case PaError::Wc:
        c.hw_raise_irq(IRQ_WC_CORRUPTION);
        c.error_capture(vc);
        break;
    case PaError::UnsupportedDt: c.hw_raise_irq(IRQ_UNSUPPORTED_DT); break;
    case PaError::FrameSync:
        c.hw_raise_irq(vc == 1 ? IRQ_FRAME_SYNC_ERR_VC1 : IRQ_FRAME_SYNC_ERR_VC0);
        c.error_capture(vc);
        break;
    case PaError::FrameLevel: c.hw_raise_irq(vc == 1 ? IRQ_FRAME_LEVEL_ERR_VC1 : IRQ_FRAME_LEVEL_ERR_VC0); break;
    case PaError::LaneAbort: c.error_capture(-1); break;  // IRQ 19 raised by Lane Management
    }
}

void Csi2RxController::Glue::pa_packet_done() { c.hw_count(Counter::Packet); }

void Csi2RxController::Glue::pa_frame_start(unsigned vc, uint16_t n) {
    if (c.sink_) c.sink_->frame_start(vc, n);
}

void Csi2RxController::Glue::pa_frame_end(unsigned vc, uint16_t n) {
    if (c.sink_) c.sink_->frame_end(vc, n);
}

void Csi2RxController::Glue::pa_gsp(uint8_t vc, uint8_t dt, uint16_t data) { c.hw_gsp_push(vc, dt, data); }

void Csi2RxController::Glue::pa_null_blank() {
    if (!c.suppressed()) c.hw_set_ro(FX1_CSI2_CORE_STATUS, FM(CORE_STATUS, NULL_BLANK_SEEN), ~0u);
}

void Csi2RxController::Glue::pa_vc_unsupported() {
    if (!c.suppressed()) c.hw_set_ro(FX1_CSI2_CORE_STATUS, FM(CORE_STATUS, VC_UNSUPPORTED_SEEN), ~0u);
}

// ---------------------------------------------------------------- sys_clk domain

void Csi2RxController::SysGlue::sys_line_buffer_full() { c.hw_raise_irq(IRQ_LINE_BUFFER_FULL); }

void Csi2RxController::SysGlue::sys_line_buffer_overflow() {
    if (c.suppressed()) return;
    c.ro_[FX1_CSI2_BUFFER_STATUS / 4] |= FM(BUFFER_STATUS, LINEBUF_OVERFLOW);  // sticky, cleared by soft_reset
    c.hw_raise_irq(IRQ_BUFFER_OVERFLOW);
}

void Csi2RxController::SysGlue::sys_cdc_overflow() {
    // C25: a word lost between Lane Management and Packet Analyzer; the burst is
    // replaced by an abort marker (lane abort at its position in the stream).
    SC_REPORT_WARNING("fx1/csi2/ctrl", "receive FIFO overflow: sys_clk slower than the merged word rate");
    c.hw_raise_irq(IRQ_BUFFER_OVERFLOW);
}

void Csi2RxController::SysGlue::sys_frame_dropped() {
    c.hw_count(Counter::FrameDrop);
    c.hw_raise_irq(IRQ_FRAME_DROPPED);
}

void Csi2RxController::SysGlue::sys_capture_start(uint16_t, uint64_t timestamp_ns) {
    c.hw_latch_timestamp_ns(timestamp_ns);  // C30: latched when the capture opens
}

void Csi2RxController::SysGlue::sys_capture_done() {
    c.hw_count(Counter::Frame);  // D06 with dma_enable = 0 (DMA path in M4)
    c.hw_raise_irq(IRQ_FRAME_RECEIVED);
}

void Csi2RxController::SysGlue::sys_published(unsigned vc, const FrameInfo& f) {
    const bool v1 = vc & 1;
    c.hw_set_ro(v1 ? FX1_CSI2_VC1_IMAGE_INFO1 : FX1_CSI2_VC0_IMAGE_INFO1, ~0u,
                f.byte_count | uint32_t(f.line_count) << FX1_CSI2_VC0_IMAGE_INFO1_LINE_COUNT_SHIFT);
    c.hw_set_ro(v1 ? FX1_CSI2_VC1_IMAGE_INFO2 : FX1_CSI2_VC0_IMAGE_INFO2,
                FM(VC0_IMAGE_INFO2, DATA_TYPE) | FM(VC0_IMAGE_INFO2, WIDTH_MISMATCH) | FM(VC0_IMAGE_INFO2, HEIGHT_MISMATCH),
                (f.data_type & 0x3Fu) | (f.width_mismatch ? FM(VC0_IMAGE_INFO2, WIDTH_MISMATCH) : 0) |
                    (f.height_mismatch ? FM(VC0_IMAGE_INFO2, HEIGHT_MISMATCH) : 0));
    if (v1) {
        c.hw_set_ro(FX1_CSI2_EMBEDDED_DATA_INFO_VC1,
                    FM(EMBEDDED_DATA_INFO_VC1, EMBEDDED_WORD_COUNT_VC1) | FM(EMBEDDED_DATA_INFO_VC1, EMBEDDED_LINE_COUNT_VC1),
                    f.emb_words | uint32_t(f.emb_lines) << FX1_CSI2_EMBEDDED_DATA_INFO_VC1_EMBEDDED_LINE_COUNT_VC1_SHIFT);
    } else {
        c.hw_set_ro(FX1_CSI2_EMBEDDED_DATA_INFO_VC0,
                    FM(EMBEDDED_DATA_INFO_VC0, EMBEDDED_WORD_COUNT_VC0) | FM(EMBEDDED_DATA_INFO_VC0, EMBEDDED_LINE_COUNT_VC0),
                    f.emb_words | uint32_t(f.emb_lines) << FX1_CSI2_EMBEDDED_DATA_INFO_VC0_EMBEDDED_LINE_COUNT_VC0_SHIFT);
    }
    c.hw_set_ro(v1 ? FX1_CSI2_EMB_EXTRACT_VC1_VAL0 : FX1_CSI2_EMB_EXTRACT_VC0_VAL0, ~0u, f.extract[0]);
    c.hw_set_ro(v1 ? FX1_CSI2_EMB_EXTRACT_VC1_VAL1 : FX1_CSI2_EMB_EXTRACT_VC0_VAL1, ~0u, f.extract[1]);
}

void Csi2RxController::SysGlue::sys_resolution_mismatch(unsigned) { c.hw_raise_irq(IRQ_RESOLUTION_MISMATCH); }
void Csi2RxController::SysGlue::sys_err_wc_align() { c.hw_raise_irq(IRQ_ERR_WC_ALIGN); }
void Csi2RxController::SysGlue::sys_fmtbridge_overflow() { c.hw_raise_irq(IRQ_FMTBRIDGE_OVERFLOW); }
void Csi2RxController::SysGlue::sys_frame_num_err(unsigned) { c.hw_raise_irq(IRQ_FRAME_NUM_SEQ_ERR); }

// ---------------------------------------------------------------- DMA register side

void Csi2RxController::DmaGlue::dma_set_buffer(unsigned side, BufState s, uint16_t frame_id) {
    if (frame_id) c.hw_set_buffer(side, s, frame_id);
    else c.buf_[side & 1] = s;  // FRAME_ID unchanged
}

void Csi2RxController::DmaGlue::dma_set_last_valid_line(unsigned side, uint16_t line) {
    const uint32_t mask = side ? FM(BUFFER_ERROR_INFO, BUF1_LAST_VALID_LINE) : FM(BUFFER_ERROR_INFO, BUF0_LAST_VALID_LINE);
    const unsigned shift = side ? FX1_CSI2_BUFFER_ERROR_INFO_BUF1_LAST_VALID_LINE_SHIFT : FX1_CSI2_BUFFER_ERROR_INFO_BUF0_LAST_VALID_LINE_SHIFT;
    c.hw_set_ro(FX1_CSI2_BUFFER_ERROR_INFO, mask, uint32_t(line) << shift);
}

void Csi2RxController::DmaGlue::dma_set_active_buffer(unsigned side) {
    c.hw_set_ro(FX1_CSI2_DMA_STATUS, FM(DMA_STATUS, ACTIVE_BUFFER), uint32_t(side & 1) << FX1_CSI2_DMA_STATUS_ACTIVE_BUFFER_SHIFT);
}

// dma_busy (M17); the end of busy is the end of any reset or disable drain.
void Csi2RxController::DmaGlue::dma_busy(bool busy) {
    c.capture_in_flight_ = busy;
    if (!busy) c.hw_drain_complete();
}

void Csi2RxController::DmaGlue::dma_buf_done(bool ok, uint32_t) { c.hw_raise_irq(ok ? IRQ_BUF_DONE_OK : IRQ_BUF_DONE_ERR); }

void Csi2RxController::DmaGlue::dma_overflow(uint32_t) { c.hw_raise_irq(IRQ_BUFFER_OVERFLOW); }

void Csi2RxController::DmaGlue::dma_axi_error(uint8_t awid, uint8_t resp, uint32_t addr, uint32_t) {
    c.hw_set_ro(FX1_CSI2_AXI_FAULT_TRACE, FM(AXI_FAULT_TRACE, AWID) | FM(AXI_FAULT_TRACE, RESP),
                uint32_t(awid) << FX1_CSI2_AXI_FAULT_TRACE_AWID_SHIFT | uint32_t(resp & 3) << FX1_CSI2_AXI_FAULT_TRACE_RESP_SHIFT);
    c.hw_set_ro(FX1_CSI2_AXI_FAULT_ADDR, ~0u, addr);
    c.hw_count(resp == 3 ? Counter::AxiDecerr : Counter::AxiSlverr);
    c.hw_raise_irq(IRQ_AXI_WRITE_ERROR);
}

// DMA worker in the mm_aclk domain (P03, M21): one blocking TLM write per AXI
// burst; the W channel takes one mm_aclk cycle per beat before the write is
// sent, the target adds its own latency. Serial: one write outstanding.
void Csi2RxController::dma_thread() {
    for (;;) {
        DmaBurst b;
        while (!sys_.dma().take_burst(b)) wait(dma_wake_ev_);
        wait(cfg_.mm_clk_period * double(b.beats));
        tlm::tlm_generic_payload p;
        Csi2AxiExtension ext;
        ext.awid = static_cast<uint8_t>(b.side);
        ext.awlen = static_cast<uint8_t>(b.beats - 1);
        ext.awqos = b.qos;
        ext.awprot = b.prot;
        const unsigned tail = static_cast<unsigned>(b.data.size() % 8);
        ext.last_wstrb = tail ? static_cast<uint8_t>((1u << tail) - 1) : 0xFF;
        p.set_command(tlm::TLM_WRITE_COMMAND);
        p.set_address(b.addr);
        p.set_data_ptr(b.data.data());
        p.set_data_length(static_cast<unsigned>(b.data.size()));
        p.set_streaming_width(static_cast<unsigned>(b.data.size()));
        p.set_byte_enable_ptr(nullptr);
        p.set_dmi_allowed(false);
        p.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        p.set_extension(&ext);
        sc_core::sc_time d = sc_core::SC_ZERO_TIME;
        if (dma_socket.size() > 0) dma_socket->b_transport(p, d);
        else p.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
        p.clear_extension(&ext);
        if (d != sc_core::SC_ZERO_TIME) wait(d);
        const tlm::tlm_response_status r = p.get_response_status();
        // AXI BRESP: OKAY 0, SLVERR 2, DECERR 3 (address error).
        const uint8_t resp = r == tlm::TLM_OK_RESPONSE ? 0 : r == tlm::TLM_ADDRESS_ERROR_RESPONSE ? 3 : 2;
        sys_.dma().burst_done(b, resp == 0, resp);
        sys_.resolve_ledger();  // completion events in the same timestamp as the ownership update (M12)
        sys_wake_ev_.notify(sc_core::SC_ZERO_TIME);
    }
}

// sys_clk edges on the grid k * period. While the datapath is quiescent and
// held, the thread sleeps until an input (merged word, CSR write, reset)
// wakes it (C26); this does not change any cycle the datapath would execute.
void Csi2RxController::sys_thread() {
    const uint64_t period = cfg_.sys_clk_period.value();
    for (;;) {
        const bool held = in_reset_ || (core_config_ & FM(CORE_CONFIG, SOFT_RESET));
        if (held || sys_.quiescent(sys_cfg_)) {
            wait(sys_wake_ev_);
            continue;
        }
        const uint64_t now = sc_core::sc_time_stamp().value();
        uint64_t next = (now + period - 1) / period * period;
        if (any_edge_ && next <= last_edge_.value()) next = last_edge_.value() + period;
        if (next > now) wait(sc_core::sc_time::from_value(next - now));
        last_edge_ = sc_core::sc_time_stamp();
        any_edge_ = true;
        if (in_reset_ || (core_config_ & FM(CORE_CONFIG, SOFT_RESET))) continue;  // reset during the wait
        sys_.cycle(sys_cfg_, static_cast<uint64_t>(last_edge_ / sc_core::sc_time(1, sc_core::SC_PS)), timestamp_ns());
        if (sys_.dma().burst_ready()) dma_wake_ev_.notify(sc_core::SC_ZERO_TIME);
    }
}

void Csi2RxController::stopstate_monitor() {
    const unsigned lanes = adapter_.applied_lane_mask();
    for (unsigned i = 0; i < cfg_.max_lanes; ++i) {
        const bool now = is1(w_.stopstatedata[i].read());
        if (now && !stop_prev_[i] && (lanes >> i & 1)) hw_raise_irq(IRQ_STOP_STATE);
        stop_prev_[i] = now;
    }
}

// ---------------------------------------------------------------- hw_* interface

void Csi2RxController::hw_count(Counter c, uint32_t n) {
    switch (c) {
    case Counter::Packet: cnt_.packet = sat_add(cnt_.packet, n, 0xFFFFFFFFu); break;
    case Counter::Frame: cnt_.frame = sat_add(cnt_.frame, n, 0xFFFFFFFFu); break;
    case Counter::Crc: cnt_.crc = sat_add(cnt_.crc, n, 0xFFFFFFFFu); break;
    case Counter::FrameDrop: cnt_.frame_drop = sat_add(cnt_.frame_drop, n, 0xFFFFFFFFu); break;
    case Counter::Ecc1: cnt_.ecc1 = static_cast<uint16_t>(sat_add(cnt_.ecc1, n, 0xFFFFu)); break;
    case Counter::Ecc2: cnt_.ecc2 = static_cast<uint16_t>(sat_add(cnt_.ecc2, n, 0xFFFFu)); break;
    case Counter::LaneAbort: cnt_.lane_abort = static_cast<uint16_t>(sat_add(cnt_.lane_abort, n, 0xFFFFu)); break;
    case Counter::AxiSlverr: cnt_.axi_slverr = static_cast<uint16_t>(sat_add(cnt_.axi_slverr, n, 0xFFFFu)); break;
    case Counter::AxiDecerr: cnt_.axi_decerr = static_cast<uint16_t>(sat_add(cnt_.axi_decerr, n, 0xFFFFu)); break;
    }
}

void Csi2RxController::hw_set_ro(uint32_t offset, uint32_t mask, uint32_t value) {
    const uint32_t owned = offset < 0x100 && !(offset & 3) ? hw_owned_mask(offset) : 0;
    if (mask & ~owned) {
        SC_REPORT_ERROR("fx1/csi2/ctrl", "hw_set_ro: bits not owned by the datapath");
        return;
    }
    uint32_t& r = ro_[offset / 4];
    r = (r & ~mask) | (value & mask);
}

bool Csi2RxController::hw_gsp_push(uint8_t vc, uint8_t dt, uint16_t data) {
    if (gsp_.size() >= cfg_.gsp_fifo_depth) {
        ro_[FX1_CSI2_BUFFER_STATUS / 4] |= FM(BUFFER_STATUS, SP_FIFO_OVERFLOW);
        hw_raise_irq(IRQ_BUFFER_OVERFLOW);
        return false;
    }
    gsp_.push_back((uint32_t(data) << FX1_CSI2_GENERIC_SHORT_PACKET_DATA_SHIFT) |
                   (uint32_t(vc & 3) << FX1_CSI2_GENERIC_SHORT_PACKET_VIRTUAL_CHANNEL_SHIFT) | (dt & 0x3Fu));
    return true;
}

void Csi2RxController::hw_latch_timestamp() { hw_latch_timestamp_ns(timestamp_ns()); }

void Csi2RxController::hw_latch_timestamp_ns(uint64_t ns) {
    ts_lo_ = static_cast<uint32_t>(ns);
    ts_hi_ = static_cast<uint32_t>(ns >> 32);
}

void Csi2RxController::hw_set_buffer(unsigned side, BufState s, uint16_t frame_id) {
    if (side > 1) return;
    buf_[side] = s;
    const uint32_t mask = side ? FM(FRAME_ID, FRAME_ID_BUF1) : FM(FRAME_ID, FRAME_ID_BUF0);
    const unsigned shift = side ? FX1_CSI2_FRAME_ID_FRAME_ID_BUF1_SHIFT : FX1_CSI2_FRAME_ID_FRAME_ID_BUF0_SHIFT;
    uint32_t& r = ro_[FX1_CSI2_FRAME_ID / 4];
    r = (r & ~mask) | ((uint32_t(frame_id) << shift) & mask);
}

// ---------------------------------------------------------------- register file

uint32_t Csi2RxController::csr_read(uint32_t word, bool side_effects) {
    const uint32_t ro = ro_[word / 4];
    switch (word) {
    case FX1_CSI2_IP_VERSION: return cfg_.ip_version;
    case FX1_CSI2_CORE_CONFIG: return core_config_ & kCoreConfigRw;
    case FX1_CSI2_PROTOCOL_CONFIG: {
        const uint32_t enc = cfg_.max_lanes == 1 ? 0u : cfg_.max_lanes == 2 ? 1u : 3u;
        return (protocol_config_ & kProtocolRw) | (enc << FX1_CSI2_PROTOCOL_CONFIG_MAX_LANES_SHIFT) |
               (ro & FM(PROTOCOL_CONFIG, ACTIVE_LANES_APPLIED));
    }
    case FX1_CSI2_PHY_CONFIG: return phy_config_;
    case FX1_CSI2_METADATA_DT_CONFIG: return metadata_dt_config_;
    case FX1_CSI2_STREAM_CONFIG: return stream_config_;
    case FX1_CSI2_EXPECTED_RESOLUTION: return expected_resolution_;
    case FX1_CSI2_CORE_STATUS: {
        const uint32_t dt = metadata_dt_config_ & FM(METADATA_DT_CONFIG, METADATA_DT);
        const bool conflict = (metadata_dt_config_ & FM(METADATA_DT_CONFIG, METADATA_CAPTURE_EN)) &&
                              (dt == 0x10 || dt == 0x11 || dt == 0x2B || dt == 0x2C);
        const TpgMux& mux = sys_.tpg_mux();
        return ro | (reset_in_progress() ? FM(CORE_STATUS, RESET_IN_PROGRESS) : 0) |
               (sys_.line_buffer().full() ? FM(CORE_STATUS, LINE_BUFFER_FULL) : 0) |
               (mux.active_sel() ? FM(CORE_STATUS, ACTIVE_SRC) : 0) |
               (mux.switch_pending() ? FM(CORE_STATUS, SRC_SWITCH_PENDING) : 0) |
               (!gsp_.empty() ? FM(CORE_STATUS, SP_FIFO_NOT_EMPTY) : 0) |
               (gsp_.size() >= cfg_.gsp_fifo_depth ? FM(CORE_STATUS, SP_FIFO_FULL) : 0) |
               (conflict ? FM(CORE_STATUS, DT_CONFLICT) : 0);
    }
    case FX1_CSI2_PHY_STATUS:
        // cl_ulps stays 0: the model exports no ulpsactivenotclk (contract §2.1).
        return (is1(w_.stopstateclk.read()) ? FM(PHY_STATUS, CL_STOPSTATE) : 0) |
               (is1(w_.rxclkactivehs.read()) ? FM(PHY_STATUS, CL_RXCLKACTIVEHS) : 0) |
               (is1(w_.rxulpsclknot.read()) ? FM(PHY_STATUS, CL_ULPSCLKNOT) : 0) |
               (adapter_.init_done() ? FM(PHY_STATUS, INIT_DONE) : 0);
    case FX1_CSI2_LANE_STATUS: {
        uint32_t v = 0;
        const bool skew = is1(w_.rxskewcalhs.read());  // one common signal (contract §2.1)
        for (unsigned i = 0; i < cfg_.max_lanes; ++i) {
            const unsigned base = 8 * i;
            if (is1(w_.stopstatedata[i].read())) v |= 1u << base;
            if (lane_sot_err_ >> i & 1) v |= 1u << (base + 1);
            if (lane_sot_sync_err_ >> i & 1) v |= 1u << (base + 2);
            if (skew && (adapter_.applied_lane_mask() >> i & 1)) v |= 1u << (base + 3);
        }
        return v;
    }
    case FX1_CSI2_PACKET_COUNT: return cnt_.packet;
    case FX1_CSI2_FRAME_COUNT: return cnt_.frame;
    case FX1_CSI2_BUFFER_STATUS: return ro | (sys_.line_buffer().fill() & FM(BUFFER_STATUS, LINEBUF_FILL));
    case FX1_CSI2_LANE_ALIGN_STATUS: return ro;
    case FX1_CSI2_GLOBAL_IRQ_ENABLE: return global_irq_enable_;
    case FX1_CSI2_IRQ_STATUS: return irq_status_;
    case FX1_CSI2_IRQ_ENABLE: return irq_enable_;
    case FX1_CSI2_ECC_ERROR_COUNT: return cnt_.ecc1 | (uint32_t(cnt_.ecc2) << 16);
    case FX1_CSI2_CRC_ERROR_COUNT: return cnt_.crc;
    case FX1_CSI2_FRAME_DROP_COUNT: return cnt_.frame_drop;
    case FX1_CSI2_LANE_ABORT_COUNT: return cnt_.lane_abort;
    case FX1_CSI2_GENERIC_SHORT_PACKET: {
        if (gsp_.empty()) return 0;
        const uint32_t v = gsp_.front();
        if (side_effects) gsp_.pop_front();  // D12: bus read pops, debug read peeks
        return v;
    }
    case FX1_CSI2_METADATA_RECORD_ID: return metadata_record_id_;
    case FX1_CSI2_TIMESTAMP_LO:
        if (side_effects) ts_hi_shadow_ = ts_hi_;  // C06
        return ts_lo_;
    case FX1_CSI2_TIMESTAMP_HI: return ts_hi_shadow_;
    case FX1_CSI2_DMA_CONTROL: return dma_control_;
    case FX1_CSI2_DMA_BUFFER0_ADDR: return dma_buf0_;
    case FX1_CSI2_DMA_BUFFER1_ADDR: return dma_buf1_;
    case FX1_CSI2_DMA_BUFFER_SIZE: return dma_size_;
    case FX1_CSI2_DMA_ATTR: return dma_attr_;
    case FX1_CSI2_DMA_STATUS:
        return (ro & FM(DMA_STATUS, ACTIVE_BUFFER)) |
               ((capture_in_flight_ || reset_drain_pending_) ? FM(DMA_STATUS, DMA_BUSY) : 0);  // M17
    case FX1_CSI2_LINE_PITCH: return line_pitch_;
    case FX1_CSI2_BUFFER_OWNERSHIP:
        return (uint32_t(buf_[0]) << FX1_CSI2_BUFFER_OWNERSHIP_BUF0_STATE_SHIFT) |
               (uint32_t(buf_[1]) << FX1_CSI2_BUFFER_OWNERSHIP_BUF1_STATE_SHIFT);
    case FX1_CSI2_BUFFER_RELEASE: return 0;  // WO
    case FX1_CSI2_TPG_CONTROL: return tpg_control_;
    case FX1_CSI2_AXI_ERROR_COUNT: return cnt_.axi_slverr | (uint32_t(cnt_.axi_decerr) << 16);
    case FX1_CSI2_TPG_RESOLUTION: return tpg_resolution_;
    case FX1_CSI2_TPG_BLANKING: return tpg_blanking_;
    case FX1_CSI2_EMB_EXTRACT_CFG0: return emb_cfg0_;
    case FX1_CSI2_EMB_EXTRACT_CFG1: return emb_cfg1_;
    // Latched hardware words.
    case FX1_CSI2_VC0_IMAGE_INFO1: case FX1_CSI2_VC0_IMAGE_INFO2: case FX1_CSI2_VC1_IMAGE_INFO1:
    case FX1_CSI2_VC1_IMAGE_INFO2: case FX1_CSI2_EMBEDDED_DATA_INFO_VC0: case FX1_CSI2_EMBEDDED_DATA_INFO_VC1:
    case FX1_CSI2_FRAME_ID: case FX1_CSI2_BUFFER_ERROR_INFO: case FX1_CSI2_ERROR_CAPTURE:
    case FX1_CSI2_AXI_FAULT_TRACE: case FX1_CSI2_AXI_FAULT_ADDR: case FX1_CSI2_EMB_EXTRACT_VC0_VAL0:
    case FX1_CSI2_EMB_EXTRACT_VC0_VAL1: case FX1_CSI2_EMB_EXTRACT_VC1_VAL0: case FX1_CSI2_EMB_EXTRACT_VC1_VAL1:
        return ro;
    default: return 0;  // reserved offset inside the decoded range (D07)
    }
}

void Csi2RxController::csr_write(uint32_t word, uint32_t value, uint32_t lanes) {
    switch (word) {
    case FX1_CSI2_CORE_CONFIG: {
        if (lanes & value & FM(CORE_CONFIG, FULL_RESET)) {  // WO command, D-PHY + datapath + registers
            start_reset(true);
            break;
        }
        const uint32_t old = core_config_;
        core_config_ = merge(old, value, lanes, kCoreConfigRw);
        const uint32_t soft = FM(CORE_CONFIG, SOFT_RESET), en = FM(CORE_CONFIG, CORE_ENABLE);
        if ((core_config_ & soft) && !(old & soft)) start_reset(false);
        if ((old & en) && !(core_config_ & en)) {
            disable_cause();      // M16/M17: drain flag while a capture is in flight
            sys_.core_disable();  // Packet FSM held idle, contexts cleared, receive FIFO flushed; DMA abort
            sys_.resolve_ledger();
        }
        break;
    }
    case FX1_CSI2_PROTOCOL_CONFIG: protocol_config_ = merge(protocol_config_, value, lanes, kProtocolRw); break;
    case FX1_CSI2_PHY_CONFIG: {
        const uint32_t built = (1u << cfg_.max_lanes) - 1;
        const uint32_t rw = (kPhyRw & ~(FM(PHY_CONFIG, DL_ENABLE) | FM(PHY_CONFIG, FORCERXMODE))) |
                            (built << FX1_CSI2_PHY_CONFIG_DL_ENABLE_SHIFT) |
                            (built << FX1_CSI2_PHY_CONFIG_FORCERXMODE_SHIFT);
        phy_config_ = merge(phy_config_, value, lanes, rw);
        if (lanes & value & FM(PHY_CONFIG, DPHY_RST_REQ)) adapter_.request_reset();
        apply_phy_config();
        break;
    }
    case FX1_CSI2_METADATA_DT_CONFIG: metadata_dt_config_ = merge(metadata_dt_config_, value, lanes, kMetadataRw); break;
    case FX1_CSI2_STREAM_CONFIG: {
        const uint32_t old = stream_config_;
        stream_config_ = merge(old, value, lanes, kStreamRw);
        // MAS Block 8: stream_format is not shadowed; changed only while the path is disabled.
        if (((old ^ stream_config_) & FM(STREAM_CONFIG, STREAM_FORMAT)) &&
            ((old | stream_config_) & FM(STREAM_CONFIG, AXIS_ENABLE)))
            SC_REPORT_WARNING("fx1/csi2/ctrl", "STREAM_CONFIG.stream_format changed while axis_enable = 1: unsupported");
        break;
    }
    case FX1_CSI2_EXPECTED_RESOLUTION: expected_resolution_ = merge(expected_resolution_, value, lanes, ~0u); break;
    case FX1_CSI2_GLOBAL_IRQ_ENABLE:
        global_irq_enable_ = merge(global_irq_enable_, value, lanes, FM(GLOBAL_IRQ_ENABLE, GLOBAL_ENABLE));
        break;
    case FX1_CSI2_IRQ_STATUS: {
        // RW1C; an event at the same simulation time keeps its flag (C02).
        const uint32_t clr = value & lanes & kIrqBits;
        const sc_core::sc_time now = sc_core::sc_time_stamp();
        for (unsigned b = 0; b < IRQ_COUNT; ++b)
            if ((clr >> b & 1) && !(irq_set_valid_[b] && irq_set_time_[b] == now)) irq_status_ &= ~(1u << b);
        break;
    }
    case FX1_CSI2_IRQ_ENABLE: irq_enable_ = merge(irq_enable_, value, lanes, kIrqBits); break;
    case FX1_CSI2_METADATA_RECORD_ID: metadata_record_id_ = merge(metadata_record_id_, value, lanes, ~0u); break;
    case FX1_CSI2_DMA_CONTROL: {
        const uint32_t old = dma_control_;
        dma_control_ = merge(old, value, lanes, kDmaControlRw);
        // M09 / MAS Block 8: dma_format is not latched; changed only with dma_enable = 0 and the DMA idle.
        if (((old ^ dma_control_) & FM(DMA_CONTROL, DMA_FORMAT)) &&
            (((old | dma_control_) & FM(DMA_CONTROL, DMA_ENABLE)) || capture_in_flight_))
            SC_REPORT_WARNING("fx1/csi2/ctrl", "DMA_CONTROL.dma_format changed while the DMA is enabled or busy: unsupported");
        const uint32_t en = FM(DMA_CONTROL, DMA_ENABLE), ab = FM(DMA_CONTROL, DMA_ABORT);
        if (((old & en) && !(dma_control_ & en)) || (!(old & ab) && (dma_control_ & ab))) {
            disable_cause();
            sys_.dma().abort_all_dma();  // M16
            sys_.resolve_ledger();
        }
        break;
    }
    case FX1_CSI2_DMA_BUFFER0_ADDR: dma_buf0_ = merge(dma_buf0_, value, lanes, ~0u); break;
    case FX1_CSI2_DMA_BUFFER1_ADDR: dma_buf1_ = merge(dma_buf1_, value, lanes, ~0u); break;
    case FX1_CSI2_DMA_BUFFER_SIZE: dma_size_ = merge(dma_size_, value, lanes, ~0u); break;
    case FX1_CSI2_DMA_ATTR: dma_attr_ = merge(dma_attr_, value, lanes, kDmaAttrRw); break;
    case FX1_CSI2_LINE_PITCH: line_pitch_ = merge(line_pitch_, value, lanes, kLinePitchRw); break;
    case FX1_CSI2_BUFFER_RELEASE:
        for (unsigned s = 0; s < 2; ++s) {
            const uint32_t bit = s ? FM(BUFFER_RELEASE, RELEASE_BUF1) : FM(BUFFER_RELEASE, RELEASE_BUF0);
            if ((lanes & value & bit) && buf_[s] != BufState::HwOwned) {  // M11: no effect on HW_OWNED
                buf_[s] = BufState::HwOwned;
                release_ev_.notify(sc_core::SC_ZERO_TIME);
            }
        }
        break;
    case FX1_CSI2_TPG_CONTROL: {
        const uint32_t dual = FM(TPG_CONTROL, TPG_DUAL_VC_EN);
        const bool was = tpg_control_ & dual;
        tpg_control_ = merge(tpg_control_, value, lanes, kTpgControlRw);
        if ((tpg_control_ & dual) && !was)  // C28: stored and read back, no effect in the linear build
            SC_REPORT_WARNING("fx1/csi2/ctrl", "TPG_CONTROL.tpg_dual_vc_en set: not supported by the linear profile, VC0 only");
        break;
    }
    case FX1_CSI2_TPG_RESOLUTION: tpg_resolution_ = merge(tpg_resolution_, value, lanes, ~0u); break;
    case FX1_CSI2_TPG_BLANKING: tpg_blanking_ = merge(tpg_blanking_, value, lanes, ~0u); break;
    case FX1_CSI2_EMB_EXTRACT_CFG0: emb_cfg0_ = merge(emb_cfg0_, value, lanes, kEmbCfgRw); break;
    case FX1_CSI2_EMB_EXTRACT_CFG1: emb_cfg1_ = merge(emb_cfg1_, value, lanes, kEmbCfgRw); break;
    default: break;  // RO registers and reserved offsets: writes ignored (C01, D07)
    }
    update_sys_config();
    sys_wake_ev_.notify(sc_core::SC_ZERO_TIME);
    update_irq();
}

// ---------------------------------------------------------------- transport

void Csi2RxController::b_transport(tlm::tlm_generic_payload& tx, sc_core::sc_time& delay) {
    tx.set_dmi_allowed(false);  // registers have side effects
    if (!tx.is_read() && !tx.is_write()) {
        tx.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
        return;
    }
    const uint64_t addr = tx.get_address();
    const unsigned len = tx.get_data_length();
    if (!tx.get_data_ptr() || len == 0 || tx.get_streaming_width() < len) {
        tx.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
        return;
    }
    if (tx.get_byte_enable_ptr() && tx.get_byte_enable_length() == 0) {
        tx.set_response_status(tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE);
        return;
    }
    // A register access takes effect at its annotated time (same rule as the
    // FX1 DMA and ISP models); the returned delay carries the access latency.
    if (delay != sc_core::SC_ZERO_TIME) {
        if (sc_core::sc_get_current_process_handle().proc_kind() != sc_core::SC_THREAD_PROC_) {
            SC_REPORT_ERROR(name(), "CSR access with a non-zero annotated delay from a method process");
            tx.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
            return;
        }
        wait(delay);
        delay = sc_core::SC_ZERO_TIME;
    }
    if (addr >= FX1_CSI2_CSR_DECODED_BYTES || addr + len > FX1_CSI2_CSR_DECODED_BYTES) {
        tx.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);  // DECERR, no side effect (D07)
        return;
    }
    const uint32_t lane0 = static_cast<uint32_t>(addr & 3);
    if ((len != 1 && len != 2 && len != 4) || lane0 + len > 4) {
        tx.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);  // SLVERR before any side effect (C04)
        return;
    }
    const uint32_t word = static_cast<uint32_t>(addr & ~uint64_t(3));
    unsigned char* data = tx.get_data_ptr();
    if (tx.is_read()) {
        const uint32_t v = in_reset_ ? 0 : csr_read(word, true);
        for (unsigned i = 0; i < len; ++i)
            if (byte_enabled(tx, i)) data[i] = static_cast<unsigned char>(v >> (8 * (lane0 + i)));
    } else {
        uint32_t v = 0, lanes = 0;
        for (unsigned i = 0; i < len; ++i)
            if (byte_enabled(tx, i)) {
                v |= uint32_t(data[i]) << (8 * (lane0 + i));
                lanes |= 0xFFu << (8 * (lane0 + i));
            }
        if (!in_reset_) csr_write(word, v, lanes);  // register file held in reset
    }
    delay += cfg_.csr_latency;
    tx.set_response_status(tlm::TLM_OK_RESPONSE);
}

unsigned Csi2RxController::transport_dbg(tlm::tlm_generic_payload& tx) {
    // Side-effect-free reads only (C05).
    if (!tx.is_read() || !tx.get_data_ptr()) return 0;
    const uint64_t addr = tx.get_address();
    const unsigned len = tx.get_data_length();
    if (addr >= FX1_CSI2_CSR_DECODED_BYTES || (len != 1 && len != 2 && len != 4) || (addr & 3) + len > 4) return 0;
    const uint32_t v = csr_read(static_cast<uint32_t>(addr & ~uint64_t(3)), false);
    for (unsigned i = 0; i < len; ++i) tx.get_data_ptr()[i] = static_cast<unsigned char>(v >> (8 * ((addr & 3) + i)));
    tx.set_response_status(tlm::TLM_OK_RESPONSE);
    return len;
}

}  // namespace fx1::csi2
