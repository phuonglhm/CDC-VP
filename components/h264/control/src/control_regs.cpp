#include <h264/control/control_regs.h>
#include <algorithm>

namespace h264 {
ControlRegs::ControlRegs(sc_core::sc_module_name name, ResetDomain& reset, SequenceParameters params)
    : sc_module(name), reset_(reset), params_(params) {
    socket.register_b_transport(this, &ControlRegs::transport);
    SC_METHOD(drive_irq);
    sensitive << irq_changed_;
}
void ControlRegs::drive_irq() { irq.write(irq_level_ && (values_[0] & reg::GIE)); }
void ControlRegs::set_irq(bool level) {
    irq_level_ = level;
    irq_changed_.notify(sc_core::SC_ZERO_TIME);
}
void ControlRegs::reset() {
    values_.fill(0); status_ = words_ = 0; pending_ = false;
    values_[reg::SPARA0/4] = 0x12c; // Table 20-3: COMPAT=1, N=2, M=12.
    completed_frames_ = 0;
    set_irq(false);
}
bool ControlRegs::take_start() {
    if (!pending_) return false;
    pending_ = false;
    return true;
}
FrameConfig ControlRegs::config() const {
    FrameConfig c;
    c.width = (values_[reg::FMSIZE/4] >> 16) & 0x7ff;
    c.height = values_[reg::FMSIZE/4] & 0x7ff;
    c.cmb = values_[reg::CMB/4]; c.refm = values_[reg::REFM/4]; c.nal = values_[reg::NAL/4];
    c.dfcon = values_[reg::DFCON/4]; c.spara0 = values_[reg::SPARA0/4];
    c.spara1 = values_[reg::SPARA1/4]; c.spara2 = values_[reg::SPARA2/4];
    c.sequence.nal_capacity_bytes = params_.nal_capacity_bytes;
    c.sequence.qp = (c.spara0 >> 21) & 63;
    c.sequence.gop_m = c.spara0 & 15;
    c.sequence.gop_n = (c.spara0 >> 4) & 15;
    c.sequence.frame_address_mode = (c.spara0 & (1u<<9)) != 0;
    c.sequence.frame_count = c.spara1 & 0xffff;
    c.sequence.cmb_frames = (c.spara1 >> 25) & 127;
    c.sequence.log2_fn = (c.spara1 >> 20) & 15;
    c.sequence.log2_poc = (c.spara1 >> 16) & 15;
    c.sequence.force_log = (c.spara1 & (1u<<24)) != 0;
    auto signed_field=[](unsigned value,unsigned bits) {
        return (value & (1u<<(bits-1))) ? int(value)-int(1u<<bits) : int(value);
    };
    c.filter_enabled = (c.dfcon & 1) != 0;
    c.alpha = signed_field((c.dfcon>>6)&31,5);
    c.beta = signed_field((c.dfcon>>1)&31,5);
    c.slice_qp_delta = signed_field(c.spara2&63,6);
    c.crop_bottom = (c.spara2>>6)&0x7ff;
    c.completed_frames = completed_frames_;
    const unsigned remaining = c.sequence.frame_count > completed_frames_ ? c.sequence.frame_count-completed_frames_ : 0;
    c.activation_frames = std::min(remaining,c.sequence.frame_address_mode ? 1u : c.sequence.cmb_frames);
    return c;
}
void ControlRegs::begin() {
    const unsigned total = values_[reg::SPARA1/4]&0xffff;
    if (completed_frames_ >= total) completed_frames_ = 0;
    status_ = reg::BUSY | completed_frames_; words_ = 0; set_irq(false);
}
void ControlRegs::frame_complete() {
    ++completed_frames_;
    status_ = (status_ & ~0xffffu) | (completed_frames_ & 0xffffu);
}
void ControlRegs::finish(bool success, uint32_t words) {
    status_ = (status_ & 0xffffu) | (success ? reg::NORMAL : reg::ERROR);
    words_ = words;
    set_irq((values_[0] & reg::GIE) != 0);
}
void ControlRegs::nal_words_accepted(uint64_t generation,uint32_t words) {
    if (!reset_.valid(generation) || !busy()) throw std::runtime_error("stale NAL progress");
    if (words > params_.nal_capacity_bytes/4 - words_) throw std::runtime_error("NAL capacity exceeded");
    words_ += words;
}
void ControlRegs::transport(tlm::tlm_generic_payload& tx, sc_core::sc_time& delay) {
    consume_delay(delay);
    tx.set_dmi_allowed(false);
    const auto a = tx.get_address();
    if (reset_.active) { tx.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE); return; }
    if ((a & 3) || a > reg::STM_LEN) { tx.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE); return; }
    if (!tx.get_data_ptr() || tx.get_data_length() != 4 || tx.get_streaming_width() < 4) {
        tx.set_response_status(tlm::TLM_BURST_ERROR_RESPONSE); return;
    }
    auto* be = tx.get_byte_enable_ptr();
    const unsigned bel = tx.get_byte_enable_length();
    if (be && !bel) { tx.set_response_status(tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE); return; }
    if (be) for (unsigned i=0; i<4; ++i) {
        if (be[i%bel] != 0 && be[i%bel] != 0xff) {
            tx.set_response_status(tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE); return;
        }
    }
    if (tx.is_read()) {
        uint32_t v = a == reg::STAT ? status_ : a == reg::STM_LEN ? words_ : values_[a/4];
        unsigned char bytes[4]; store_le(bytes, v);
        for (unsigned i=0; i<4; ++i) if (!be || be[i%bel]) tx.get_data_ptr()[i] = bytes[i];
        // Clear once at the completed register access, provided some byte was read.
        bool any = false;
        for (unsigned i=0; i<4; ++i) any |= !be || be[i%bel];
        if (any && (a == reg::STAT || a == reg::STM_LEN)) set_irq(false);
    } else if (tx.is_write()) {
        if (a == reg::STAT || a == reg::STM_LEN || (a != reg::SCON && (busy() || enabled()))) {
            tx.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE); return;
        }
        uint32_t v = values_[a/4];
        for (unsigned i=0; i<4; ++i) if (!be || be[i%bel])
            v = (v & ~(0xffu << (8*i))) | uint32_t(tx.get_data_ptr()[i]) << (8*i);
        if (a == reg::SCON) {
            v &= 3;
            const bool rise = !(values_[0] & reg::ENABLE) && (v & reg::ENABLE);
            if (rise && busy()) { tx.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE); return; }
            values_[0] = v;
            irq_changed_.notify(sc_core::SC_ZERO_TIME); // Mask output, retain pending event.
            if (rise) {
                begin(); pending_ = true; start_event.notify(sc_core::SC_ZERO_TIME);
            }
        } else {
            if (a == reg::FMSIZE) v &= 0x07ff07ffu;
            if (a == reg::SPARA2) v &= 0x1ffffu;
            if (a == reg::DFCON) v &= 0x7ffu;
            if (a == reg::SPARA0) v &= 0x07fff3ffu;
            const auto changed=values_[a/4]^v;
            if ((changed && (a == reg::FMSIZE || a == reg::REFM || a == reg::SPARA2 || a == reg::DFCON)) ||
                (a == reg::SPARA0 && (changed&0x07e003ffu)) ||
                (a == reg::SPARA1 && (changed&0x01ffffffu))) completed_frames_ = 0;
            values_[a/4] = v;
        }
    } else { tx.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE); return; }
    tx.set_response_status(tlm::TLM_OK_RESPONSE);
}
}
