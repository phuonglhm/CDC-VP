#include "fu3_plic/plic.h"

namespace fu3_plic {
namespace {

constexpr std::uint32_t kPriorityMask = 0x7;           // MAX_PRIO 7 => 3 bit
constexpr std::uint32_t kApertureSize = 0x1'0000;      // Cửa sổ AP_PLIC 64 KiB

bool is_priority(std::uint32_t offset) {
    return offset <= FU3_PLIC_PRIORITY_OFFSET(kSources) && offset % 4 == 0;
}

bool is_mapped(std::uint32_t offset) {
    return is_priority(offset) || offset == FU3_PLIC_PENDING_OFFSET || offset == FU3_PLIC_ENABLE_OFFSET ||
           offset == FU3_PLIC_THRESHOLD_OFFSET || offset == FU3_PLIC_CLAIM_OFFSET;
}

}  // namespace

Plic::Plic(sc_core::sc_module_name name) : sc_core::sc_module(name) {
    socket.register_b_transport(this, &Plic::b_transport);
    socket.register_transport_dbg(this, &Plic::transport_dbg);

    SC_METHOD(sample);
    sensitive << reset_n;
    for (auto& src : irq_sources) sensitive << src;

    // irq chỉ được lái từ đây, bus thread chỉ notify changed_
    SC_METHOD(update);
    sensitive << changed_ << reset_n;
}

void Plic::reset_state() {
    for (auto& p : priority_) p = 0;
    ie_ = threshold_ = ip_ = in_service_ = 0;
}

unsigned Plic::best() const {
    const std::uint32_t eligible = ip_ & ie_;
    unsigned id = 0, prio = threshold_;
    // So sánh tường minh priority > threshold: threshold 7 chặn hết (Common v2.0: threshold corner case)
    // Ngang priority thì ID nhỏ thắng
    for (unsigned n = 1; n <= kSources; ++n) {
        if (((eligible >> n) & 1) && priority_[n] > prio) {
            prio = priority_[n];
            id = n;
        }
    }
    return id;
}

unsigned Plic::claim() {
    const unsigned id = best();
    if (id) {
        ip_ &= ~(1u << id);   // Claim xoá pending, không xoá IP ngoại vi
        in_service_ |= 1u << id;
        changed_.notify(sc_core::SC_ZERO_TIME);
    }
    return id;
}

void Plic::complete(std::uint32_t id) {
    // Chỉ nhận ID 1-23 đang được claim, còn lại không làm gì
    if (id >= 1 && id <= kSources && ((in_service_ >> id) & 1)) {
        in_service_ &= ~(1u << id);
        // Nguồn level vẫn lên thì pending lại ngay
        if (irq_sources[id - 1].read()) ip_ |= 1u << id;
        changed_.notify(sc_core::SC_ZERO_TIME);
    }
}

void Plic::sample() {
    if (!reset_n.read()) {
        reset_state();
    } else {
        // Gateway level: nguồn lên khi chưa in-service => chốt pending tới khi claim
        // Giữ được xung 1 tick của TIMER FU3
        for (unsigned n = 1; n <= kSources; ++n)
            if (irq_sources[n - 1].read() && !((in_service_ >> n) & 1)) ip_ |= 1u << n;
    }
    changed_.notify(sc_core::SC_ZERO_TIME);
}

void Plic::update() {
    irq.write(reset_n.read() && best() != 0);
}

std::uint32_t Plic::read_register(std::uint32_t offset) const {
    if (is_priority(offset)) return priority_[offset / 4];   // ID 0 luôn 0
    switch (offset) {
        case FU3_PLIC_PENDING_OFFSET:   return ip_;
        case FU3_PLIC_ENABLE_OFFSET:    return ie_;
        case FU3_PLIC_THRESHOLD_OFFSET: return threshold_;
        case FU3_PLIC_CLAIM_OFFSET:     return best();   // xem trước, không claim
        default:                        return 0;
    }
}

void Plic::b_transport(tlm::tlm_generic_payload& tx, sc_core::sc_time&) {
    access(tx, false);
}

unsigned Plic::transport_dbg(tlm::tlm_generic_payload& tx) {
    access(tx, true);
    return tx.is_response_ok() ? tx.get_data_length() : 0;
}

void Plic::access(tlm::tlm_generic_payload& tx, bool backdoor) {
    const std::uint64_t addr = tx.get_address();
    // Common v2.0 APB access contract: chỉ word 32 bit căn hàng, PSTRB = 0xF
    if (tx.get_data_length() != 4) {
        tx.set_response_status(tlm::TLM_BURST_ERROR_RESPONSE);
        return;
    }
    if (addr % 4 != 0 || addr >= kApertureSize) {
        tx.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
        return;
    }
    const unsigned char* be = tx.get_byte_enable_ptr();
    const unsigned be_len = tx.get_byte_enable_length();
    bool partial = false;
    for (unsigned i = 0; be && be_len && i < 4; ++i) partial |= be[i % be_len] == TLM_BYTE_DISABLED;

    const auto offset = static_cast<std::uint32_t>(addr);
    // Offset không map hoặc ghi thiếu byte => PSLVERR
    if (!is_mapped(offset) || (tx.is_write() && partial)) {
        tx.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
        return;
    }

    unsigned char* data = tx.get_data_ptr();
    if (tx.is_read()) {
        // Đọc CLAIM qua bus = claim, debug read chỉ xem
        const std::uint32_t value = (offset == FU3_PLIC_CLAIM_OFFSET && !backdoor && reset_n.read())
                                        ? claim() : read_register(offset);
        for (unsigned i = 0; i < 4; ++i) data[i] = (value >> (8 * i)) & 0xFF;
    } else if (tx.is_write()) {
        std::uint32_t value = 0;
        for (unsigned i = 0; i < 4; ++i) value |= std::uint32_t(data[i]) << (8 * i);
        // Đang giữ reset thì ghi không có tác dụng (trừ backdoor)
        if (backdoor || reset_n.read()) {
            if (is_priority(offset)) {
                if (offset != 0) priority_[offset / 4] = value & kPriorityMask;
            } else if (offset == FU3_PLIC_ENABLE_OFFSET) {
                ie_ = value & FU3_PLIC_ENABLE_MASK;
            } else if (offset == FU3_PLIC_THRESHOLD_OFFSET) {
                threshold_ = value & kPriorityMask;
            } else if (offset == FU3_PLIC_CLAIM_OFFSET) {
                complete(value);
            }   // PENDING là RO: ghi bị bỏ qua
            changed_.notify(sc_core::SC_ZERO_TIME);
        }
    } else {
        tx.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
        return;
    }
    tx.set_response_status(tlm::TLM_OK_RESPONSE);
}

}  // namespace fu3_plic
