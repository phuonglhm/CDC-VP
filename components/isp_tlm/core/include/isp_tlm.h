#pragma once

#include <systemc>
#include <tlm>
#include <tlm_core/tlm_2/tlm_generic_payload/tlm_gp.h>
#include <tlm_utils/simple_target_socket.h>
#include <tlm_utils/simple_initiator_socket.h>

#include <cstdint>
#include <vector>

#include "isp_pipeline.h"
#include "isp_regmap.h"

namespace cdc::components {

class isp_tlm : public sc_core::sc_module {
public:
   SC_HAS_PROCESS(isp_tlm);

   tlm_utils::simple_target_socket<isp_tlm> socket;
   tlm_utils::simple_initiator_socket<isp_tlm> dma_socket;
   sc_core::sc_in<bool> reset_n;
   sc_core::sc_out<bool> irq_out;

   explicit isp_tlm(sc_core::sc_module_name name,
                    sc_core::sc_time access_latency = sc_core::sc_time(10, sc_core::SC_NS));

   std::uint16_t *get_raw_buffer() {
      allocate_buffers();
      return raw_buffer_.data();
   }

   std::uint8_t *get_yuv_buffer() {
      allocate_buffers();
      return yuv_buffer_.data();
   }

   std::size_t get_raw_buffer_size() const {
      return raw_buffer_.size();
   }

   std::size_t get_yuv_buffer_size() const {
      return yuv_buffer_.size();
   }

private:
   void allocate_buffers();
   void b_transport(tlm::tlm_generic_payload &trans, sc_core::sc_time &delay);
   unsigned int transport_dbg(tlm::tlm_generic_payload &trans);
   void dma_read();
   void dma_write();
   void processing_thread();
   void update_irq_output();
   void trigger_processing();

   isp_pipeline pipeline_;

   std::vector<std::uint16_t> raw_buffer_;
   std::vector<std::uint8_t> yuv_buffer_;

   sc_core::sc_time access_latency_;
   sc_core::sc_event processing_event_;
   bool irq_level_;

   static constexpr std::size_t MAX_WIDTH = 4096;
   static constexpr std::size_t MAX_HEIGHT = 4096;
};

} // namespace cdc::components
