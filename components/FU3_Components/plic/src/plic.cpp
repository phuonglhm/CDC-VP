#include "fu3_plic/plic.h"

namespace fu3_plic {
namespace {

constexpr std::uint32_t kPriorityMask = 0x1F;          // MAX_PRIO 31 => 5 bit
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
    sensitive << reset_n << clock_gate_changed_;
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
    // Only an ID 1-31 that is in service completes; anything else is ignored
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
    } else if (clock_enabled_) {
        // Gated clock: the gateways capture nothing; a level still high is
        // captured on the first sample after the clock is enabled again.
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
    const unsigned length = tx.get_data_length();
    const unsigned lane = static_cast<unsigned>(addr % 4);
    // Decision Q02-g (Common v2.1 "Full" strobes): byte and halfword accesses
    // inside one word are served on their lanes; byte enables select lanes.
    if (length == 0 || length == 3 || length > 4) {
        tx.set_response_status(tlm::TLM_BURST_ERROR_RESPONSE);
        return;
    }
    if (lane % length || addr >= kApertureSize) {   // misaligned or outside the window

        tx.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
        return;
    }
    std::uint32_t lanes = 0;   // byte lanes this access touches
    const unsigned char* be = tx.get_byte_enable_ptr();
    const unsigned be_len = tx.get_byte_enable_length();
    for (unsigned i = 0; i < length; ++i)
        if (!be || !be_len || be[i % be_len] != TLM_BYTE_DISABLED) lanes |= 0xFFu << (8 * (lane + i));

    const auto offset = static_cast<std::uint32_t>(addr - lane);
    // Unmapped offset => PSLVERR. CLAIM/COMPLETE has side effects: full word only.
    if (!is_mapped(offset) || (offset == FU3_PLIC_CLAIM_OFFSET && lanes != 0xFFFF'FFFFu)) {
        tx.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
        return;
    }

    unsigned char* data = tx.get_data_ptr();
    if (tx.is_read()) {
        // A bus read of CLAIM claims; a debug read only looks
        const std::uint32_t value = (offset == FU3_PLIC_CLAIM_OFFSET && !backdoor && reset_n.read())
                                        ? claim() : read_register(offset);
        for (unsigned i = 0; i < length; ++i) data[i] = (value >> (8 * (lane + i))) & 0xFF;
    } else if (tx.is_write()) {
        std::uint32_t written = 0;
        for (unsigned i = 0; i < length; ++i) written |= std::uint32_t(data[i]) << (8 * (lane + i));
        // Merge the enabled lanes into the register; no enabled lane: no effect
        const std::uint32_t value = (read_register(offset) & ~lanes) | (written & lanes);
        // Writes are ignored while held in reset (except the debug path)
        if (lanes && (backdoor || reset_n.read())) {
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
