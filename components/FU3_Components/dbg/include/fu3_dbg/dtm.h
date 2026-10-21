#pragma once
// FU3 DBG JTAG DTM (RISC-V Debug Spec 0.13.2 section 6): mức scan IR/DR, không mô hình từng xung TCK
#include <cstdint>

#include "fu3_dbg/dm.h"

namespace fu3_dbg {

class Dtm {
public:
    explicit Dtm(DebugModule& dm, std::uint32_t idcode = FU3_DBG_IDCODE) : dm_(dm), idcode_(idcode) {}

    void test_logic_reset();                   // IR = IDCODE, xoá kết quả DMI
    void scan_ir(std::uint8_t ir);             // 5 bit
    unsigned dr_length() const;                // Độ dài DR của IR hiện tại
    // Shift DR qua Update-DR
    // Trả giá trị Capture-DR 
    // Gọi từ SC_THREAD
    std::uint64_t scan_dr(std::uint64_t in);

private:
    DebugModule& dm_;
    const std::uint32_t idcode_;
    std::uint8_t ir_ = FU3_DBG_IR_IDCODE;
    std::uint64_t dmi_result_ = 0;             // {address, data, op} của truy cập DMI trước
};

}  // namespace fu3_dbg
