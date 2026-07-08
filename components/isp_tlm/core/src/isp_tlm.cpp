#include "isp_tlm.h"

#include "isp_regmap.h"

#include <cstring>

namespace cdc::components {

isp_tlm::isp_tlm(sc_core::sc_module_name name, sc_core::sc_time access_latency)
    : sc_core::sc_module(name)
    , socket("socket")
    , reset_n("reset_n")
    , irq_out("irq_out")
    , access_latency_(access_latency)
{
    socket.register_b_transport(this, &isp_tlm::b_transport);
    socket.register_transport_dbg(this, &isp_tlm::transport_dbg);

    reset_state();

    SC_METHOD(update_irq_output);
    sensitive << reset_n;
    dont_initialize();
}

void isp_tlm::update_irq_output()
{
    irq_out.write(reset_n.read() && pipeline_.irq_level());
}

void isp_tlm::reset_state()
{
    pipeline_.reset_registers();
    raw_buffer_.clear();
    yuv_buffer_.clear();
}

void isp_tlm::allocate_buffers()
{
    const std::size_t raw_size =
        static_cast<std::size_t>(pipeline_.get_width()) * pipeline_.get_height();
    const std::size_t yuv_size =
        static_cast<std::size_t>(pipeline_.get_width()) * pipeline_.get_height() * 3 / 2;

    raw_buffer_.resize(raw_size);
    yuv_buffer_.resize(yuv_size);
}

void isp_tlm::b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay)
{
    delay += access_latency_;

    const std::uint64_t offset = trans.get_address();
    const unsigned len = trans.get_data_length();
    unsigned char* ptr = trans.get_data_ptr();

    if (len != sizeof(std::uint32_t) || (offset & 0x3u) != 0u) {
        trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
        return;
    }

    if (offset >= REG_MAX) {
        trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
        return;
    }

    if (trans.get_command() == tlm::TLM_READ_COMMAND) {
        std::uint32_t value = pipeline_.read_reg(static_cast<std::uint32_t>(offset));
        std::memcpy(ptr, &value, sizeof(value));
    } else if (trans.get_command() == tlm::TLM_WRITE_COMMAND) {
        std::uint32_t value = 0;
        std::memcpy(&value, ptr, sizeof(value));
        const bool start = pipeline_.write_reg(static_cast<std::uint32_t>(offset), value);
        update_irq_output();
        if (start) {
            trigger_processing();
        }
    } else {
        trans.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
        return;
    }

    trans.set_response_status(tlm::TLM_OK_RESPONSE);
}

unsigned int isp_tlm::transport_dbg(tlm::tlm_generic_payload& trans)
{
    const std::uint64_t offset = trans.get_address();
    const unsigned len = trans.get_data_length();
    unsigned char* ptr = trans.get_data_ptr();

    if (len != sizeof(std::uint32_t) || (offset & 0x3u) != 0u || offset >= REG_MAX) {
        return 0;
    }

    if (trans.get_command() == tlm::TLM_READ_COMMAND) {
        std::uint32_t value = pipeline_.read_reg(static_cast<std::uint32_t>(offset));
        std::memcpy(ptr, &value, sizeof(value));
    } else if (trans.get_command() == tlm::TLM_WRITE_COMMAND) {
        std::uint32_t value = 0;
        std::memcpy(&value, ptr, sizeof(value));
        const bool start = pipeline_.write_reg(static_cast<std::uint32_t>(offset), value);
        update_irq_output();
        if (start) {
            trigger_processing();
        }
    } else {
        return 0;
    }

    return sizeof(std::uint32_t);
}

void isp_tlm::trigger_processing()
{
    if (!pipeline_.is_enabled()) {
        return;
    }

    if (!pipeline_.has_valid_dimensions()) {
        pipeline_.mark_processing_error();
        update_irq_output();
        return;
    }

    pipeline_.mark_processing_started();
    update_irq_output();

    allocate_buffers();
    pipeline_.set_lsc_mem(nullptr);
    pipeline_.run(raw_buffer_.data(), yuv_buffer_);

    pipeline_.mark_processing_done();
    update_irq_output();
}

} // namespace cdc::components
