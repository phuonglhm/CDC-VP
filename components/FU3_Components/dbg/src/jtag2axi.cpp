#include "fu3_dbg/jtag2axi.h"

#include <fu3_bus/access.h>

namespace fu3_dbg {

void Jtag2Axi::test_logic_reset() {
    ir_ = FU3_DBG_IR_CMD_IDCODE;
    address_ = rdata_ = status_ = 0;
}

void Jtag2Axi::scan_ir(std::uint8_t ir) {
    ir_ = ir & 0x1F;
}

unsigned Jtag2Axi::dr_length() const {
    switch (ir_) {
        case FU3_DBG_IR_CMD_IDCODE:
        case FU3_DBG_IR_CMD_ADDR:
        case FU3_DBG_IR_CMD_STATUS:
            return 32;
        case FU3_DBG_IR_CMD_DATA:
            return 34;
        default:
            return 1;   // CMD_BYPASS và IR không dùng
    }
}

std::uint64_t Jtag2Axi::scan_dr(std::uint64_t in) {
    switch (ir_) {
        case FU3_DBG_IR_CMD_IDCODE:
            return idcode_;
        case FU3_DBG_IR_CMD_ADDR: {
            const std::uint32_t out = address_;
            address_ = static_cast<std::uint32_t>(in);
            return out;
        }
        case FU3_DBG_IR_CMD_DATA: {
            // Capture: dữ liệu của lần đọc trước. Update: write hoặc read tại address rồi address += 4,
            // không bit nào thì không truy cập (lấy kết quả mà không đọc thêm)
            const std::uint32_t out = rdata_;
            if (in & FU3_DBG_DATA_WRITE)
                access(true, static_cast<std::uint32_t>(in));
            else if (in & FU3_DBG_DATA_READ)
                access(false, 0);
            return out;
        }
        case FU3_DBG_IR_CMD_STATUS: {
            const std::uint32_t out = status_;
            status_ &= ~(static_cast<std::uint32_t>(in) & FU3_DBG_STATUS_ERRORS);   // W1C
            return out;
        }
        default:
            return 0;   // BYPASS: 1 bit 0
    }
}

void Jtag2Axi::access(bool write, std::uint32_t wdata) {
    if (address_ % 4) {
        status_ |= FU3_DBG_STATUS_ALIGN;
        return;
    }
    unsigned char bytes[4];
    for (unsigned i = 0; i < 4; ++i) bytes[i] = (wdata >> (8 * i)) & 0xFF;
    tlm::tlm_generic_payload tx;
    tx.set_command(write ? tlm::TLM_WRITE_COMMAND : tlm::TLM_READ_COMMAND);
    tx.set_address(address_);
    tx.set_data_ptr(bytes);
    tx.set_data_length(4);
    tx.set_streaming_width(4);
    tx.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
    fu3_bus::ScopedAccess scoped(tx, 4);
    sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
    axi->b_transport(tx, delay);

    if (!tx.is_response_ok()) {
        status_ |= tx.get_response_status() == tlm::TLM_ADDRESS_ERROR_RESPONSE ? FU3_DBG_STATUS_DECERR
                                                                               : FU3_DBG_STATUS_SLVERR;
        return;
    }
    if (!write) {
        rdata_ = 0;
        for (unsigned i = 0; i < 4; ++i) rdata_ |= std::uint32_t(bytes[i]) << (8 * i);
    }
    address_ += 4;
}

}  // namespace fu3_dbg
