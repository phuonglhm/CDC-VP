#include "fu3_plic/plic.h"

namespace fu3_plic {
namespace {

constexpr std::uint32_t kPriorityMask  = 0x7777'7777;   // 8 mức => [2:0] mỗi nibble
constexpr std::uint32_t kThresholdMask = 0x7;
constexpr std::uint32_t kApertureSize  = 0x1'0000;      // Cửa sổ AP_PLIC 64 KiB

// CONFIG_0/1 (RO): giá trị tham số RTL hiện tại (HDS Table 5-1)
constexpr std::uint32_t kConfig0 = FU3_PLIC_SOURCES | (FU3_PLIC_TARGETS << 16);
constexpr std::uint32_t kConfig1 = FU3_PLIC_PRIORITIES | (FU3_PLIC_MAX_PENDING_COUNT << 16) | (1u << 24);

}  // namespace

Plic::Plic(sc_core::sc_module_name name) : sc_core::sc_module(name) {
    socket.register_b_transport(this, &Plic::b_transport);
    socket.register_transport_dbg(this, &Plic::transport_dbg);

    SC_METHOD(sample);
    sensitive << reset_n;
    for (auto& src : irq_sources) sensitive << src;

    // irq chỉ được lái từ đây 
    // bus thread chỉ notify changed_
    SC_METHOD(update);
    sensitive << changed_ << reset_n;
    for (auto& src : irq_sources) sensitive << src;
}

void Plic::reset_state() {
    el_ = ie_ = threshold_ = in_service_ = level_ip_ = 0;
    for (auto& p : priority_) p = 0;
    for (unsigned k = 0; k < kSources; ++k) edge_count_[k] = 0;
}

unsigned Plic::priority_of(unsigned bit) const {
    return (priority_[bit / 8] >> (4 * (bit % 8))) & 0x7;
}

std::uint32_t Plic::pending() const {
    std::uint32_t p = 0;
    for (unsigned k = 0; k < kSources; ++k) {
        const bool edge = (el_ >> k) & 1;
        // Level: theo mức nguồn.
        // Edge: còn sườn lên chưa claim. 
        // Đang in-service thì không pending.
        // const bool raised = edge ? edge_count_[k] > 0 : irq_sources[k].read();
      
        // Đọc mức nguồn trực tiếp làm mất xung IRQ 1 tick của TIMER FU3 trước khi CPU kịp claim
        const bool raised = edge ? edge_count_[k] > 0 : ((level_ip_ >> k) & 1);

        if (raised && !((in_service_ >> k) & 1)) 
            p |= 1u << k; 
        
    }
    return p;
}

unsigned Plic::best() const {
    const std::uint32_t eligible = pending() & ie_;
    unsigned id = 0, prio = threshold_;
    for (unsigned k = 0; k < kSources; ++k) {
        // Priority 0 không bao giờ ngắt
        // Ngang priority thì ID nhỏ thắng
        if (((eligible >> k) & 1) && priority_of(k) > prio) {
            prio = priority_of(k);
            id = k + 1;
        }
    }
    return id;
}

unsigned Plic::claim() {
    const unsigned id = best();
    if (id) {
        const unsigned k = id - 1;
        in_service_ |= 1u << k;
        level_ip_ &= ~(1u << k);   // Claim xoá pending đã chốt
        if (((el_ >> k) & 1) && edge_count_[k] > 0) --edge_count_[k];
        changed_.notify(sc_core::SC_ZERO_TIME);
    }
    return id;
}

void Plic::complete(std::uint32_t id) {
    // Complete ID không hợp lệ hoặc chưa claim thì bỏ qua
    if (id >= 1 && id <= kSources && ((in_service_ >> (id - 1)) & 1)) {
        in_service_ &= ~(1u << (id - 1));
        // Nguồn level vẫn đang lên thì pending lại ngay
        if (!((el_ >> (id - 1)) & 1) && irq_sources[id - 1].read()) 
            level_ip_ |= 1u << (id - 1);
        changed_.notify(sc_core::SC_ZERO_TIME);
    }
}

void Plic::sample() {
    if (!reset_n.read()) {
        reset_state();
    }
    for (unsigned k = 0; k < kSources; ++k) {
        const bool level = irq_sources[k].read();
        
        // Gateway edge: Đếm sườn lên và Bão hoà ở MAX_PENDING_COUNT
        if (reset_n.read() && ((el_ >> k) & 1) && level && !last_level_[k] && edge_count_[k] < kMaxPending)
            ++edge_count_[k];

        // Gateway level: nguồn lên khi chưa in-service => chốt pending
        if (reset_n.read() && !((el_ >> k) & 1) && level && !((in_service_ >> k) & 1))
            level_ip_ |= 1u << k;
        last_level_[k] = level;
    }
    changed_.notify(sc_core::SC_ZERO_TIME);
}

void Plic::update() {
    irq.write(reset_n.read() && best() != 0);
}

std::uint32_t Plic::read_register(std::uint32_t offset) const {
    switch (offset) {
        case FU3_PLIC_CONFIG_0_OFFSET:    return kConfig0;
        case FU3_PLIC_CONFIG_1_OFFSET:    return kConfig1;
        case FU3_PLIC_EL_OFFSET:          return el_;
        case FU3_PLIC_PRIORITY_0_OFFSET:  return priority_[0];
        case FU3_PLIC_PRIORITY_1_OFFSET:  return priority_[1];
        case FU3_PLIC_PRIORITY_2_OFFSET:  return priority_[2];
        case FU3_PLIC_PRIORITY_3_OFFSET:  return priority_[3];
        case FU3_PLIC_IE_0_OFFSET:        return ie_;
        case FU3_PLIC_THRESHOLD_0_OFFSET: return threshold_;
        case FU3_PLIC_ID_0_OFFSET:        return best();   // xem trước, không claim
        default:                          return 0;
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
    const unsigned len = tx.get_data_length();
    // APB4 32 bit: 1 beat 1/2/4 byte căn hàng (bus FU3 đã tách burst)
    if (len != 1 && len != 2 && len != 4) {
        tx.set_response_status(tlm::TLM_BURST_ERROR_RESPONSE);
        return;
    }
    if (addr % len != 0 || addr >= kApertureSize) {
        tx.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
        return;
    }

    const std::uint32_t word_offset = static_cast<std::uint32_t>(addr) & ~3u;
    const unsigned lane = static_cast<unsigned>(addr & 3);
    unsigned char* data = tx.get_data_ptr();
    const unsigned char* be = tx.get_byte_enable_ptr();
    const unsigned be_len = tx.get_byte_enable_length();

    if (tx.is_read()) {
        // Đọc ID_0 qua bus = claim
        // Debug read chỉ xem
        const std::uint32_t value = (word_offset == FU3_PLIC_ID_0_OFFSET && !backdoor && reset_n.read())
                                        ? claim() : read_register(word_offset);
        for (unsigned i = 0; i < len; ++i) data[i] = (value >> (8 * (lane + i))) & 0xFF;
    } else if (tx.is_write()) {
        std::uint32_t lanes = 0, value = 0;
        for (unsigned i = 0; i < len; ++i) {
            if (be && be_len && be[i % be_len] == TLM_BYTE_DISABLED) continue;   // PSTRB = 0
            lanes |= 0xFFu << (8 * (lane + i));
            value |= std::uint32_t(data[i]) << (8 * (lane + i));
        }
        auto merge = [&](std::uint32_t& reg, std::uint32_t mask) {
            reg = (reg & ~(lanes & mask)) | (value & lanes & mask);
        };
        // Đang giữ reset thì ghi không có tác dụng (trừ backdoor)
        // Offset không map: bỏ qua.
        if (backdoor || reset_n.read()) {
            switch (word_offset) {
                case FU3_PLIC_EL_OFFSET:          merge(el_, 0xFFFF'FFFF); break;
                case FU3_PLIC_PRIORITY_0_OFFSET:  merge(priority_[0], kPriorityMask); break;
                case FU3_PLIC_PRIORITY_1_OFFSET:  merge(priority_[1], kPriorityMask); break;
                case FU3_PLIC_PRIORITY_2_OFFSET:  merge(priority_[2], kPriorityMask); break;
                case FU3_PLIC_PRIORITY_3_OFFSET:  merge(priority_[3], kPriorityMask); break;
                case FU3_PLIC_IE_0_OFFSET:        merge(ie_, 0xFFFF'FFFF); break;
                case FU3_PLIC_THRESHOLD_0_OFFSET: merge(threshold_, kThresholdMask); break;
                case FU3_PLIC_ID_0_OFFSET:        complete(value & lanes); break;
                default: break;   // CONFIG_0/1 RO, offset không map
            }
            changed_.notify(sc_core::SC_ZERO_TIME);
        }
    } else {
        tx.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
        return;
    }
    tx.set_response_status(tlm::TLM_OK_RESPONSE);
}

}  // namespace fu3_plic
