#include "fu3_dbg/dm.h"

#include <fu3_bus/access.h>

namespace fu3_dbg {
namespace {

constexpr std::uint32_t kDataCount = 2;
constexpr std::uint32_t kSbRwMask = FU3_DBG_SBCS_SBREADONADDR | FU3_DBG_SBCS_SBACCESS(7) |
                                    FU3_DBG_SBCS_SBAUTOINCREMENT | FU3_DBG_SBCS_SBREADONDATA;

}  // namespace

DebugModule::DebugModule(sc_core::sc_module_name name, std::vector<DebugHart*> harts)
    : sc_core::sc_module(name), harts_(std::move(harts)), state_(harts_.size()) {
    reset_dm();
    SC_METHOD(update);
    sensitive << changed_;
}

void DebugModule::reset_dm() {
    for (auto* h : harts_) h->set_halt_request(false);
    for (auto& s : state_) s.resumeack = false;   // havereset giữ tới ackhavereset
    dmactive_ = ndmreset_ = false;
    hartsel_ = 0;
    data_[0] = data_[1] = 0;
    cmderr_ = 0;
    sbcs_ = FU3_DBG_SBCS_SBACCESS(2);
    sbaddress_ = sbdata_ = 0;
    changed_.notify(sc_core::SC_ZERO_TIME);
}

void DebugModule::notify_hart_reset(unsigned hart) {
    if (hart < state_.size()) 
        state_[hart].havereset = true;
}

void DebugModule::update() {
    ndmreset.write(ndmreset_);
}

std::uint32_t DebugModule::dmstatus() const {
    std::uint32_t s = FU3_DBG_DMSTATUS_AUTHENTICATED | FU3_DBG_DMSTATUS_VERSION;
    const DebugHart* h = selected();
    if (!h) 
        return s | FU3_DBG_DMSTATUS_ANYNONEXISTENT | FU3_DBG_DMSTATUS_ALLNONEXISTENT;

    // hasel = 0: chỉ 1 hart được chọn nên any = all
    const HartState& st = state_[hartsel_];
    s |= h->halted() ? (FU3_DBG_DMSTATUS_ANYHALTED | FU3_DBG_DMSTATUS_ALLHALTED)
                     : (FU3_DBG_DMSTATUS_ANYRUNNING | FU3_DBG_DMSTATUS_ALLRUNNING);
    if (st.resumeack) 
        s |= FU3_DBG_DMSTATUS_ANYRESUMEACK | FU3_DBG_DMSTATUS_ALLRESUMEACK;
    if (st.havereset) 
        s |= FU3_DBG_DMSTATUS_ANYHAVERESET | FU3_DBG_DMSTATUS_ALLHAVERESET;
    return s;
}

void DebugModule::write_dmcontrol(std::uint32_t v) {
    // dmactive = 0: DM về giá trị reset, mọi field khác bị bỏ qua
    if (!(v & FU3_DBG_DMCONTROL_DMACTIVE)) {
        reset_dm();
        return;
    }
    dmactive_ = true;
    hartsel_ = ((v >> 16) & 0x3FF) | (((v >> 6) & 0x3FF) << 10);

    const bool ndm = v & FU3_DBG_DMCONTROL_NDMRESET;
    if (ndm && !ndmreset_)
        for (auto& s : state_) 
            s.havereset = true;
    ndmreset_ = ndm;
    changed_.notify(sc_core::SC_ZERO_TIME);

    DebugHart* h = selected();
    if (!h) 
        return;
    HartState& st = state_[hartsel_];
    const bool haltreq = v & FU3_DBG_DMCONTROL_HALTREQ;
    h->set_halt_request(haltreq);
    if (!haltreq && (v & FU3_DBG_DMCONTROL_RESUMEREQ)) {
        st.resumeack = false;
        if (h->halted()) {
            h->resume();
            st.resumeack = !h->halted();
        }
    }
    if (v & FU3_DBG_DMCONTROL_ACKHAVERESET) 
        st.havereset = false;
}

void DebugModule::execute(std::uint32_t cmd) {
    const unsigned cmdtype = cmd >> 24;
    const unsigned aarsize = (cmd >> 20) & 7;
    // Chỉ Access Register 32 bit, không program buffer
    if (cmdtype != 0 || (cmd & FU3_DBG_CMD_POSTEXEC) || ((cmd & FU3_DBG_CMD_TRANSFER) && aarsize != 2)) {
        cmderr_ = FU3_DBG_CMDERR_NOTSUPPORTED;
        return;
    }
    DebugHart* h = selected();
    if (!h || !h->halted()) {
        cmderr_ = FU3_DBG_CMDERR_HALTRESUME;
        return;
    }
    if (cmd & FU3_DBG_CMD_TRANSFER) {
        const auto regno = static_cast<std::uint16_t>(cmd & 0xFFFF);
        if (!h->access_register(regno, cmd & FU3_DBG_CMD_WRITE, data_[0])) {
            cmderr_ = FU3_DBG_CMDERR_EXCEPTION;
            return;
        }
    }
}

void DebugModule::sb_access(bool write) {
    if (((sbcs_ >> 17) & 7) != 2) {   // Cổng DBG chỉ có beat 32-bit
        sbcs_ |= FU3_DBG_SBERROR_SIZE << FU3_DBG_SBCS_SBERROR_SHIFT;
        return;
    }
    if (sbaddress_ % 4) {
        sbcs_ |= FU3_DBG_SBERROR_ALIGNMENT << FU3_DBG_SBCS_SBERROR_SHIFT;
        return;
    }
    unsigned char bytes[4];
    for (unsigned i = 0; i < 4; ++i) bytes[i] = (sbdata_ >> (8 * i)) & 0xFF;
    tlm::tlm_generic_payload tx;
    tx.set_command(write ? tlm::TLM_WRITE_COMMAND : tlm::TLM_READ_COMMAND);
    tx.set_address(sbaddress_);
    tx.set_data_ptr(bytes);
    tx.set_data_length(4);
    tx.set_streaming_width(4);
    tx.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
    fu3_bus::ScopedAccess access(tx, 4);
    sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
    sba->b_transport(tx, delay);

    if (!tx.is_response_ok()) {
        const unsigned err = tx.get_response_status() == tlm::TLM_ADDRESS_ERROR_RESPONSE ? FU3_DBG_SBERROR_BADADDR
                                                                                           : FU3_DBG_SBERROR_OTHER;
        sbcs_ |= err << FU3_DBG_SBCS_SBERROR_SHIFT;
        return;
    }
    if (!write) {
        sbdata_ = 0;
        for (unsigned i = 0; i < 4; ++i)
            sbdata_ |= std::uint32_t(bytes[i]) << (8 * i);
    }
    if (sbcs_ & FU3_DBG_SBCS_SBAUTOINCREMENT) sbaddress_ += 4;
}

std::uint32_t DebugModule::dmi_read(std::uint8_t address) {
    switch (address) {
        case FU3_DBG_DATA0:      
            return data_[0];
        case FU3_DBG_DATA1:      
            return data_[1];
        case FU3_DBG_DMCONTROL:
            return ((hartsel_ & 0x3FF) << 16) | (((hartsel_ >> 10) & 0x3FF) << 6) |
                   (ndmreset_ ? FU3_DBG_DMCONTROL_NDMRESET : 0) | (dmactive_ ? FU3_DBG_DMCONTROL_DMACTIVE : 0);
        case FU3_DBG_DMSTATUS:   
            return dmstatus();
        case FU3_DBG_ABSTRACTCS: 
            return (cmderr_ << FU3_DBG_ABSTRACTCS_CMDERR_SHIFT) | kDataCount;
        case FU3_DBG_SBCS:
            return sbcs_ | FU3_DBG_SBCS_SBVERSION | FU3_DBG_SBCS_SBASIZE_32 | FU3_DBG_SBCS_SBACCESS32;
        case FU3_DBG_SBADDRESS0: 
            return sbaddress_;
        case FU3_DBG_SBDATA0: {
            const std::uint32_t v = sbdata_;
            if (dmactive_ && !(sbcs_ & FU3_DBG_SBCS_SBERROR_MASK) && (sbcs_ & FU3_DBG_SBCS_SBREADONDATA))
                sb_access(false);
            return v;
        }
        case FU3_DBG_HALTSUM0: {
            std::uint32_t s = 0;
            for (unsigned i = 0; i < harts_.size() && i < 32; ++i)
                if (harts_[i]->halted()) s |= 1u << i;
            return s;
        }
        default:                 
            return 0;   // hartinfo, command, abstractauto, nextdm, không map
    }
}

void DebugModule::dmi_write(std::uint8_t address, std::uint32_t v) {
    if (address == FU3_DBG_DMCONTROL) {
        write_dmcontrol(v);
        return;
    }
    if (!dmactive_) return;
    const bool sb_ok = !(sbcs_ & FU3_DBG_SBCS_SBERROR_MASK);
    switch (address) {
        case FU3_DBG_DATA0:      
            data_[0] = v; 
            break;
        case FU3_DBG_DATA1:      
            data_[1] = v; 
            break;
        case FU3_DBG_ABSTRACTCS: 
            cmderr_ &= ~((v >> FU3_DBG_ABSTRACTCS_CMDERR_SHIFT) & 7); 
            break;
        case FU3_DBG_COMMAND:
            if (cmderr_ == FU3_DBG_CMDERR_NONE) 
                execute(v);
            break;
        case FU3_DBG_SBCS:
            sbcs_ = (sbcs_ & ~kSbRwMask & ~(v & FU3_DBG_SBCS_SBERROR_MASK)) | (v & kSbRwMask);
            break;
        case FU3_DBG_SBADDRESS0:
            sbaddress_ = v;
            if (sb_ok && (sbcs_ & FU3_DBG_SBCS_SBREADONADDR)) 
                sb_access(false);
            break;
        case FU3_DBG_SBDATA0:
            sbdata_ = v;
            if (sb_ok) 
                sb_access(true);
            break;
        default: 
            break;   // RO và không map
    }
}

}  // namespace fu3_dbg
