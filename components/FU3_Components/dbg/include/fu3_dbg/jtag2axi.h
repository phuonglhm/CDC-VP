#pragma once
// FU3 DBG cầu JTAG2AXI4 (HAS v2.0 3.1): JTAG IEEE 1149.1, lệnh CMD_ADDR, CMD_DATA, CMD_BYPASS, CMD_STATUS, CMD_IDCODE
// Master AXI4 32-bit trên SYSBUS_1 (cổng DBG của bus FU3)
// Mức scan IR/DR, không mô hình từng xung TCK
#include <cstdint>
#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>

#include "fu3_dbg/registers.h"

namespace fu3_dbg {

class Jtag2Axi : public sc_core::sc_module {
public:
    tlm_utils::simple_initiator_socket<Jtag2Axi> axi{"axi"};   // -> bus initiator DBG

    explicit Jtag2Axi(sc_core::sc_module_name name, std::uint32_t idcode = FU3_DBG_IDCODE)
        : sc_core::sc_module(name), idcode_(idcode) {}

    void test_logic_reset();          // IR = CMD_IDCODE, xoá address, data, status
    void scan_ir(std::uint8_t ir);    // 5 bit
    unsigned dr_length() const;       // Độ dài DR của IR hiện tại
    // Shift DR qua Update-DR, trả giá trị Capture-DR
    // Gọi từ SC_THREAD vì truy cập AXI có thể wait
    std::uint64_t scan_dr(std::uint64_t in);

private:
    const std::uint32_t idcode_;
    std::uint8_t ir_ = FU3_DBG_IR_CMD_IDCODE;
    std::uint32_t address_ = 0, rdata_ = 0, status_ = 0;

    void access(bool write, std::uint32_t wdata);
};

}  // namespace fu3_dbg
