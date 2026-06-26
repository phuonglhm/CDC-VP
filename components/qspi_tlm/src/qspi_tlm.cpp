#include "qspi_tlm.h"

#include <algorithm>
#include <cstring>

namespace cdc::components {

namespace {

constexpr std::uint8_t kOpcodeRead = 0x03;
constexpr std::uint8_t kOpcodeFastRead = 0x0b;
constexpr std::uint8_t kOpcodeRdsr = 0x05;
constexpr std::uint8_t kOpcodeRdid = 0x9f;
constexpr std::uint8_t kOpcodeQuadRead = 0x6b;
constexpr std::uint8_t kOpcodeQuadIoRead = 0xeb;

} // namespace

qspi_tlm::qspi_tlm(sc_core::sc_module_name name)
    : sc_core::sc_module(name)
    , from_apb_socket("from_apb_socket")
    , to_flash_socket("to_flash_socket")
    , irq("irq")
    , reset_n("reset_n")
{
    from_apb_socket.register_b_transport(this, &qspi_tlm::b_transport);

    SC_METHOD(handle_reset);
    sensitive << reset_n.neg();
    dont_initialize();

    SC_METHOD(update_irq_output);
    sensitive << irq_update_event_;
    dont_initialize();
}

void qspi_tlm::b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay)
{
    const auto cmd = trans.get_command();
    const auto offset = static_cast<std::uint32_t>(trans.get_address());
    auto* ptr = trans.get_data_ptr();
    const auto len = trans.get_data_length();

    if (ptr == nullptr || len != sizeof(std::uint32_t)) {
        trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
        return;
    }

    const auto streaming_width = trans.get_streaming_width();
    if (trans.get_byte_enable_ptr() != nullptr ||
        (streaming_width != 0 && streaming_width < len)) {
        trans.set_response_status(tlm::TLM_BURST_ERROR_RESPONSE);
        return;
    }

    if (cmd == tlm::TLM_READ_COMMAND) {
        std::uint32_t value = 0;
        if (!read_reg(offset, value)) {
            trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
            return;
        }
        std::memcpy(ptr, &value, sizeof(value));
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
    } else if (cmd == tlm::TLM_WRITE_COMMAND) {
        std::uint32_t value = 0;
        std::memcpy(&value, ptr, sizeof(value));
        if (!write_reg(offset, value, delay)) {
            trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
            return;
        }
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
    } else {
        trans.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
        return;
    }

    delay += sc_core::sc_time(20, sc_core::SC_NS);
}

void qspi_tlm::handle_reset()
{
    reset_state();
}

void qspi_tlm::update_irq_output()
{
    irq.write(irq_level_);
}

void qspi_tlm::reset_state()
{
    ctrl_reg_ = 0;
    status_reg_ = 0;
    cmd_reg_ = 0;
    addr_reg_ = 0;
    len_reg_ = 0;
    cfg_reg_ = 1;
    int_en_reg_ = 0;
    int_status_reg_ = 0;
    while (!rx_fifo_.empty()) {
        rx_fifo_.pop();
    }
    irq_level_ = false;
    irq_update_event_.notify(sc_core::SC_ZERO_TIME);
}

bool qspi_tlm::read_reg(std::uint32_t offset, std::uint32_t& value)
{
    switch (offset) {
    case QSPI_CTRL:
        value = ctrl_reg_;
        return true;
    case QSPI_STATUS:
        update_status_rxne();
        value = status_reg_;
        return true;
    case QSPI_CMD:
        value = cmd_reg_;
        return true;
    case QSPI_ADDR:
        value = addr_reg_;
        return true;
    case QSPI_LEN:
        value = len_reg_;
        return true;
    case QSPI_CFG:
        value = cfg_reg_;
        return true;
    case QSPI_DATA: {
        value = 0;
        if (!rx_fifo_.empty()) {
            value = rx_fifo_.front();
            rx_fifo_.pop();
        }
        update_status_rxne();
        return true;
    }
    case QSPI_INT_EN:
        value = int_en_reg_;
        return true;
    case QSPI_INT_STATUS:
        value = int_status_reg_;
        return true;
    case QSPI_START:
        value = 0;
        return true;
    default:
        return false;
    }
}

bool qspi_tlm::write_reg(std::uint32_t offset, std::uint32_t value, sc_core::sc_time& delay)
{
    switch (offset) {
    case QSPI_CTRL:
        ctrl_reg_ = value & QSPI_CTRL_EN;
        return true;
    case QSPI_CMD:
        cmd_reg_ = value & 0xff;
        return true;
    case QSPI_ADDR:
        addr_reg_ = value & 0x00ffffffu;
        return true;
    case QSPI_LEN:
        len_reg_ = value;
        return true;
    case QSPI_CFG:
        cfg_reg_ = value;
        return true;
    case QSPI_INT_EN:
        int_en_reg_ = value & QSPI_INT_DONE;
        irq_level_ = (int_status_reg_ & int_en_reg_ & QSPI_INT_DONE) != 0;
        irq_update_event_.notify(sc_core::SC_ZERO_TIME);
        return true;
    case QSPI_INT_STATUS:
        int_status_reg_ &= ~(value & QSPI_INT_DONE);
        irq_level_ = (int_status_reg_ & int_en_reg_ & QSPI_INT_DONE) != 0;
        irq_update_event_.notify(sc_core::SC_ZERO_TIME);
        return true;
    case QSPI_START:
        if ((value & 0x1) != 0 && (ctrl_reg_ & QSPI_CTRL_EN) != 0) {
            execute_transfer(delay);
        }
        return true;
    case QSPI_STATUS:
    case QSPI_DATA:
        return true;
    default:
        return false;
    }
}

void qspi_tlm::execute_transfer(sc_core::sc_time& delay)
{
    status_reg_ |= QSPI_STATUS_BUSY;

    auto frame = build_frame();
    tlm::tlm_generic_payload flash_trans;
    flash_trans.set_command(tlm::TLM_IGNORE_COMMAND);
    flash_trans.set_address(0);
    flash_trans.set_data_ptr(frame.data());
    flash_trans.set_data_length(static_cast<unsigned>(frame.size()));
    flash_trans.set_streaming_width(static_cast<unsigned>(frame.size()));
    flash_trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

    to_flash_socket->b_transport(flash_trans, delay);

    while (!rx_fifo_.empty()) {
        rx_fifo_.pop();
    }

    const unsigned prefix_len = prefix_length_for_opcode(static_cast<std::uint8_t>(cmd_reg_));
    for (unsigned i = prefix_len; i < frame.size(); ++i) {
        rx_fifo_.push(frame[i]);
    }

    update_status_rxne();
    status_reg_ &= ~QSPI_STATUS_BUSY;
    int_status_reg_ |= QSPI_INT_DONE;
    irq_level_ = (int_status_reg_ & int_en_reg_ & QSPI_INT_DONE) != 0;
    irq_update_event_.notify(sc_core::SC_ZERO_TIME);

    const unsigned lanes = std::max<std::uint32_t>(1, cfg_reg_ & 0x7);
    const double cycles = static_cast<double>(frame.size() * 8) / lanes;
    delay += sc_core::sc_time(cycles, sc_core::SC_NS);
}

std::vector<unsigned char> qspi_tlm::build_frame() const
{
    const auto opcode = static_cast<std::uint8_t>(cmd_reg_);
    const unsigned prefix_len = prefix_length_for_opcode(opcode);
    std::vector<unsigned char> frame(prefix_len + len_reg_, 0);
    frame[0] = opcode;

    if (opcode_has_address(opcode)) {
        frame[1] = static_cast<unsigned char>((addr_reg_ >> 16) & 0xff);
        frame[2] = static_cast<unsigned char>((addr_reg_ >> 8) & 0xff);
        frame[3] = static_cast<unsigned char>(addr_reg_ & 0xff);
    }

    return frame;
}

unsigned qspi_tlm::prefix_length_for_opcode(std::uint8_t opcode) const
{
    if (!opcode_has_address(opcode)) {
        return 1;
    }
    return 1 + 3 + dummy_bytes_for_opcode(opcode);
}

unsigned qspi_tlm::dummy_bytes_for_opcode(std::uint8_t opcode) const
{
    if (opcode == kOpcodeFastRead || opcode == kOpcodeQuadRead || opcode == kOpcodeQuadIoRead) {
        const unsigned cfg_dummy = (cfg_reg_ >> 8) & 0xff;
        return std::max(1u, cfg_dummy / 8u);
    }
    const unsigned cfg_dummy = (cfg_reg_ >> 8) & 0xff;
    return cfg_dummy / 8u;
}

bool qspi_tlm::opcode_has_address(std::uint8_t opcode) const
{
    switch (opcode) {
    case kOpcodeRead:
    case kOpcodeFastRead:
    case kOpcodeQuadRead:
    case kOpcodeQuadIoRead:
        return true;
    case kOpcodeRdsr:
    case kOpcodeRdid:
    default:
        return false;
    }
}

void qspi_tlm::update_status_rxne()
{
    if (rx_fifo_.empty()) {
        status_reg_ &= ~QSPI_STATUS_RXNE;
    } else {
        status_reg_ |= QSPI_STATUS_RXNE;
    }
}

} // namespace cdc::components
