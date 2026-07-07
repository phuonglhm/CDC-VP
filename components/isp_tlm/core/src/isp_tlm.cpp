#include "isp_tlm.h"

#include <cstring>

namespace cdc::components {

isp_tlm::isp_tlm(sc_core::sc_module_name name, sc_core::sc_time access_latency)
    : sc_core::sc_module(name)
    , socket("socket")
    , reset_n("reset_n")
    , irq_out("irq_out")
    , access_latency_(access_latency)
    , irq_level_(false) {
   socket.register_b_transport(this, &isp_tlm::b_transport);
   socket.register_transport_dbg(this, &isp_tlm::transport_dbg);

   SC_THREAD(processing_thread);
   sensitive << processing_event_;
   dont_initialize();

   SC_METHOD(update_irq_output);
   sensitive << reset_n;
   dont_initialize();
}

void isp_tlm::update_irq_output() {
   if (!reset_n.read()) {
      irq_level_ = false;
   }
   irq_out.write(irq_level_);
}

void isp_tlm::allocate_buffers() {
   const std::uint32_t w = pipeline_.read_reg(REG_WIDTH);
   const std::uint32_t h = pipeline_.read_reg(REG_HEIGHT);
   const std::size_t raw_size = static_cast<std::size_t>(w) * h;
   const std::size_t yuv_size = static_cast<std::size_t>(w) * h * 3 / 2;
   if (raw_buffer_.size() == raw_size) {
      return;
   }
   raw_buffer_.resize(raw_size);
   yuv_buffer_.resize(yuv_size);
}

void isp_tlm::b_transport(tlm::tlm_generic_payload &trans, sc_core::sc_time &delay) {
   delay += access_latency_;

   const std::uint64_t offset = trans.get_address();
   const unsigned      len    = trans.get_data_length();
   unsigned char      *ptr    = trans.get_data_ptr();

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

      // CTRL lives here in the TLM shell (bit 0 controls enable, bit 1 triggers processing)
      if (offset == REG_CTRL) {
         pipeline_.write_reg(static_cast<std::uint32_t>(offset), value);
         if (value & 0x2) { // bit 1: START
            trigger_processing();
         }
      } else {
         pipeline_.write_reg(static_cast<std::uint32_t>(offset), value);
      }
   } else {
      trans.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
      return;
   }

   trans.set_response_status(tlm::TLM_OK_RESPONSE);
}

void isp_tlm::dma_read() {
   std::uint32_t src_addr = pipeline_.read_reg(REG_SRC_ADDR);
   if (src_addr == 0)
      return;

   std::size_t size = raw_buffer_.size() * sizeof(std::uint16_t);
   if (size == 0)
      return;

   std::uint64_t local_addr = src_addr;
   if (local_addr >= 0x80000000ULL && local_addr < 0x90000000ULL) {
      local_addr -= 0x80000000ULL;
   }

   tlm::tlm_generic_payload trans;
   sc_core::sc_time delay = sc_core::SC_ZERO_TIME;

   trans.set_command(tlm::TLM_READ_COMMAND);
   trans.set_address(local_addr);
   trans.set_data_ptr(reinterpret_cast<unsigned char *>(raw_buffer_.data()));
   trans.set_data_length(static_cast<unsigned>(size));
   trans.set_streaming_width(static_cast<unsigned>(size));
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
   if (dest_addr == 0)
      return;

   std::size_t size = yuv_buffer_.size();
   if (size == 0)
      return;

   std::uint64_t local_addr = dest_addr;
   if (local_addr >= 0x80000000ULL && local_addr < 0x90000000ULL) {
      local_addr -= 0x80000000ULL;
   }

   tlm::tlm_generic_payload trans;
   sc_core::sc_time delay = sc_core::SC_ZERO_TIME;

   trans.set_command(tlm::TLM_WRITE_COMMAND);
   trans.set_address(local_addr);
   trans.set_data_ptr(reinterpret_cast<unsigned char *>(yuv_buffer_.data()));
   trans.set_data_length(static_cast<unsigned>(size));
   trans.set_streaming_width(static_cast<unsigned>(size));
   trans.set_byte_enable_ptr(nullptr);
   trans.set_dmi_allowed(false);
   trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

   dma_socket->b_transport(trans, delay);
   if (trans.is_response_error()) {
      SC_REPORT_ERROR("ISP_DMA", "DMA Write to DRAM failed!");
   }
}

unsigned int isp_tlm::transport_dbg(tlm::tlm_generic_payload &trans) {
   const std::uint64_t offset = trans.get_address();
   const unsigned      len    = trans.get_data_length();
   unsigned char      *ptr    = trans.get_data_ptr();

   if (len != sizeof(std::uint32_t) || (offset & 0x3u) != 0u || offset >= REG_MAX) {
      return 0;
   }

   if (trans.get_command() == tlm::TLM_READ_COMMAND) {
      std::uint32_t value = pipeline_.read_reg(static_cast<std::uint32_t>(offset));
      std::memcpy(ptr, &value, sizeof(value));
   } else if (trans.get_command() == tlm::TLM_WRITE_COMMAND) {
      std::uint32_t value = 0;
      std::memcpy(&value, ptr, sizeof(value));
      pipeline_.write_reg(static_cast<std::uint32_t>(offset), value);
   } else {
      return 0;
   }

   return sizeof(std::uint32_t);
}

void isp_tlm::trigger_processing() {
   const std::uint32_t w = pipeline_.read_reg(REG_WIDTH);
   const std::uint32_t h = pipeline_.read_reg(REG_HEIGHT);
   if (w == 0 || h == 0) {
      return;
   }

   raw_buffer_.resize(static_cast<std::size_t>(w) * h);
   yuv_buffer_.resize(static_cast<std::size_t>(w) * h * 3 / 2);

   dma_read();

   pipeline_.run(raw_buffer_.data(), yuv_buffer_);

   dma_write();

   irq_level_ = true;
}

void isp_tlm::processing_thread() {
   while (true) {
      wait();

      if (!reset_n.read()) {
         pipeline_.reset();
         irq_level_ = false;
         raw_buffer_.clear();
         yuv_buffer_.clear();
         continue;
      }

      trigger_processing();
   }
}

} // namespace cdc::components
