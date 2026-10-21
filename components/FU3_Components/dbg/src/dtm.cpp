#include "fu3_dbg/dtm.h"

namespace fu3_dbg {
namespace {

constexpr unsigned kDmiBits = FU3_DBG_DTMCS_ABITS + 34;
constexpr std::uint32_t kDtmcs = FU3_DBG_DTMCS_VERSION | (FU3_DBG_DTMCS_ABITS << 4) | (1u << 12);   // idle = 1

}  // namespace

void Dtm::test_logic_reset() {
    ir_ = FU3_DBG_IR_IDCODE;
    dmi_result_ = 0;
}

void Dtm::scan_ir(std::uint8_t ir) {
    ir_ = ir & 0x1F;
}

unsigned Dtm::dr_length() const {
    switch (ir_) {
        case FU3_DBG_IR_IDCODE:
        case FU3_DBG_IR_DTMCS: 
            return 32;
        case FU3_DBG_IR_DMI:   
            return kDmiBits;
        default:               
            return 1;   // BYPASS và IR không dùng
    }
}

std::uint64_t Dtm::scan_dr(std::uint64_t in) {
    switch (ir_) {
        case FU3_DBG_IR_IDCODE:
            return idcode_;
        case FU3_DBG_IR_DTMCS:
            // DMI xong ngay trong Update-DR nên không có lỗi sticky cho dmireset xoá
            // dmihardreset bỏ kết quả DMI đang giữ
            if (in & FU3_DBG_DTMCS_DMIHARDRESET) 
                dmi_result_ = 0;
            return kDtmcs;
        case FU3_DBG_IR_DMI: {
            const std::uint64_t out = dmi_result_;
            const auto op = static_cast<unsigned>(in & 3);
            const auto data = static_cast<std::uint32_t>(in >> 2);
            const auto address = static_cast<std::uint8_t>((in >> 34) & 0x7F);
            if (op == FU3_DBG_DMI_OP_READ) {
                dmi_result_ = (std::uint64_t(address) << 34) | (std::uint64_t(dm_.dmi_read(address)) << 2);
            } else if (op == FU3_DBG_DMI_OP_WRITE) {
                dm_.dmi_write(address, data);
                dmi_result_ = (std::uint64_t(address) << 34) | (std::uint64_t(data) << 2);
            }
            return out;   // op = 0: thành công
        }
        default:
            return 0;     // BYPASS: 1 bit 0
    }
}

}  // namespace fu3_dbg
