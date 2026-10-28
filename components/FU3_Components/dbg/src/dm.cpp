#include "fu3_dbg/dm.h"

namespace fu3_dbg {
namespace {

constexpr std::uint32_t kDataCount = 2;
constexpr std::uint32_t kWindow = 0x1000;              // Cửa sổ DM 4 KiB (DmAddrMask 0xFFF)
constexpr std::uint32_t kDmiEnd = FU3_DBG_DM_OFFSET(0x80);   // 7 bit địa chỉ DMI

}  // namespace

DebugModule::DebugModule(sc_core::sc_module_name name, std::vector<DebugHart*> harts)
    : sc_core::sc_module(name), harts_(std::move(harts)), state_(harts_.size()) {
    socket.register_b_transport(this, &DebugModule::b_transport);
    socket.register_transport_dbg(this, &DebugModule::transport_dbg);
    reset_dm();
    SC_METHOD(update);
    sensitive << changed_;
    SC_THREAD(domain_reset);
}

void DebugModule::domain_reset() {
    if (!reset_n.size())
        return;
    while (true) {
        if (!reset_n->read()) {
            reset_dm();
            for (auto& s : state_) s.havereset = true;
        }
        wait(reset_n->value_changed_event());
    }
}

void DebugModule::reset_dm() {
    for (auto* h : harts_) h->set_halt_request(false);
    for (auto& s : state_) s.resumeack = false;   // havereset giữ tới ackhavereset
    dmactive_ = ndmreset_ = false;
    hartsel_ = 0;
    data_[0] = data_[1] = 0;
    cmderr_ = 0;
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
    if (h->unavailable())
        s |= FU3_DBG_DMSTATUS_ANYUNAVAIL | FU3_DBG_DMSTATUS_ALLUNAVAIL;
    else
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
    if (!h || h->unavailable() || !h->halted()) {
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
        case FU3_DBG_HALTSUM0: {
            std::uint32_t s = 0;
            for (unsigned i = 0; i < harts_.size() && i < 32; ++i)
                if (harts_[i]->halted()) s |= 1u << i;
            return s;
        }
        default:
            return 0;   // hartinfo, command, abstractauto, nextdm, sbcs (không SBA), không map
    }
}

void DebugModule::dmi_write(std::uint8_t address, std::uint32_t v) {
    if (reset_n.size() && !reset_n->read())
        return;   // held in DBG-domain reset
    if (address == FU3_DBG_DMCONTROL) {
        write_dmcontrol(v);
        return;
    }
    if (!dmactive_) return;
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
        default:
            break;   // RO và không map
    }
}

void DebugModule::b_transport(tlm::tlm_generic_payload& tx, sc_core::sc_time&) {
    access(tx);
}

unsigned DebugModule::transport_dbg(tlm::tlm_generic_payload& tx) {
    access(tx);   // Đọc DMI không có side effect
    return tx.is_response_ok() ? tx.get_data_length() : 0;
}

void DebugModule::access(tlm::tlm_generic_payload& tx) {
    const std::uint64_t addr = tx.get_address();
    if (tx.get_data_length() != 4) {
        tx.set_response_status(tlm::TLM_BURST_ERROR_RESPONSE);
        return;
    }
    if (addr % 4 || addr >= kWindow) {
        tx.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
        return;
    }
    const auto offset = static_cast<std::uint32_t>(addr);
    unsigned char* data = tx.get_data_ptr();
    // Thanh ghi DMI n tại offset 4n, phần còn lại của 4 KiB đọc 0, ghi bỏ qua
    const bool dmi = offset < kDmiEnd;
    const auto reg = static_cast<std::uint8_t>(offset / 4);
    if (tx.is_read()) {
        const std::uint32_t v = dmi ? dmi_read(reg) : 0;
        for (unsigned i = 0; i < 4; ++i) data[i] = (v >> (8 * i)) & 0xFF;
    } else if (tx.is_write()) {
        std::uint32_t v = 0;
        for (unsigned i = 0; i < 4; ++i) v |= std::uint32_t(data[i]) << (8 * i);
        if (dmi) dmi_write(reg, v);
    } else {
        tx.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
        return;
    }
    tx.set_response_status(tlm::TLM_OK_RESPONSE);
}

}  // namespace fu3_dbg
