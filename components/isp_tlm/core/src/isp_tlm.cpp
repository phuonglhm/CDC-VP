#include "isp_tlm.h"

#include "isp_regmap.h"

#include <cstring>

namespace cdc::components {

isp_tlm::isp_tlm(sc_core::sc_module_name name, sc_core::sc_time access_latency)
    : sc_core::sc_module(name)
    , socket("socket")
    , dma_socket("dma_socket")
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
        static_cast<std::size_t>(pipeline_.read_reg(REG_WIDTH)) * pipeline_.read_reg(REG_HEIGHT);
    const std::size_t yuv_size =
        static_cast<std::size_t>(pipeline_.read_reg(REG_WIDTH)) * pipeline_.read_reg(REG_HEIGHT) * 3 / 2;

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

void isp_tlm::dma_read() {
   std::uint32_t src_addr = pipeline_.read_reg(REG_SRC_ADDR);
   if (src_addr == 0) return;

   std::size_t size = raw_buffer_.size() * sizeof(std::uint16_t);
   if (size == 0) return;

   std::uint64_t local_addr = src_addr;
   if (local_addr >= 0x80000000ULL && local_addr < 0x90000000ULL) {
      local_addr -= 0x80000000ULL;
   }

   tlm::tlm_generic_payload trans;
   sc_core::sc_time delay = sc_core::SC_ZERO_TIME;

   trans.set_command(tlm::TLM_READ_COMMAND);
   trans.set_address(local_addr);
   trans.set_data_ptr(reinterpret_cast<unsigned char *>(raw_buffer_.data()));
   trans.set_data_length(size);
   trans.set_streaming_width(size);
   trans.set_byte_enable_ptr(nullptr);
   trans.set_dmi_allowed(false);
   trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

   dma_socket->b_transport(trans, delay);
   if (trans.is_response_error()) {
      SC_REPORT_ERROR("ISP_DMA", "DMA Read from DRAM failed!");
   }
}

void isp_tlm::dma_write() {
   std::uint32_t dest_addr = pipeline_.read_reg(REG_DST_ADDR);
   if (dest_addr == 0) return;

   std::size_t size = yuv_buffer_.size();
   if (size == 0) return;

   std::uint64_t local_addr = dest_addr;
   if (local_addr >= 0x80000000ULL && local_addr < 0x90000000ULL) {
      local_addr -= 0x80000000ULL;
   }

   tlm::tlm_generic_payload trans;
   sc_core::sc_time delay = sc_core::SC_ZERO_TIME;

   trans.set_command(tlm::TLM_WRITE_COMMAND);
   trans.set_address(local_addr);
   trans.set_data_ptr(reinterpret_cast<unsigned char *>(yuv_buffer_.data()));
   trans.set_data_length(size);
   trans.set_streaming_width(size);
   trans.set_byte_enable_ptr(nullptr);
   trans.set_dmi_allowed(false);
   trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

   dma_socket->b_transport(trans, delay);
   if (trans.is_response_error()) {
      SC_REPORT_ERROR("ISP_DMA", "DMA Write to DRAM failed!");
   }
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
    dma_read();
    pipeline_.run(raw_buffer_.data(), yuv_buffer_);
    dma_write();

    pipeline_.mark_processing_done();
    update_irq_output();
}

} // namespace cdc::components
