#pragma once

#include <systemc>
#include <tlm>
#include <tlm_core/tlm_2/tlm_generic_payload/tlm_gp.h>
#include <tlm_utils/simple_target_socket.h>
#include <tlm_utils/simple_initiator_socket.h>

#include <cstdint>
#include <vector>

#include "isp_pipeline.h"

namespace cdc::components {

constexpr std::uint32_t REG_ISP_ENABLE = 0x000;
constexpr std::uint32_t REG_STATUS = 0x004;
constexpr std::uint32_t REG_TRIGGER = 0x008;
constexpr std::uint32_t REG_WIDTH = 0x00C;
constexpr std::uint32_t REG_HEIGHT = 0x010;
constexpr std::uint32_t REG_BIT_DEPTH = 0x014;
constexpr std::uint32_t REG_BAYER_PATTERN = 0x018;

constexpr std::uint32_t REG_BLC_ENABLE = 0x020;
constexpr std::uint32_t REG_BLC_R_OFFSET = 0x024;
constexpr std::uint32_t REG_BLC_GR_OFFSET = 0x028;
constexpr std::uint32_t REG_BLC_GB_OFFSET = 0x02C;
constexpr std::uint32_t REG_BLC_B_OFFSET = 0x030;

constexpr std::uint32_t REG_DPC_ENABLE = 0x040;
constexpr std::uint32_t REG_DPC_THRESH = 0x044;

constexpr std::uint32_t REG_LSC_ENABLE = 0x050;
constexpr std::uint32_t REG_LSC_GRID_W = 0x054;
constexpr std::uint32_t REG_LSC_GRID_H = 0x058;

constexpr std::uint32_t REG_DG_ENABLE = 0x060;
constexpr std::uint32_t REG_DG_GAIN = 0x064;

constexpr std::uint32_t REG_BNR_ENABLE = 0x070;
constexpr std::uint32_t REG_BNR_WINDOW = 0x074;

constexpr std::uint32_t REG_DEMOSAIC_ENABLE = 0x080;

constexpr std::uint32_t REG_AWB_ENABLE = 0x085;
constexpr std::uint32_t REG_AWB_ALGORITHM = 0x086;
constexpr std::uint32_t REG_AWB_R_GAIN = 0x087;
constexpr std::uint32_t REG_AWB_B_GAIN = 0x088;

constexpr std::uint32_t REG_WB_ENABLE = 0x090;
constexpr std::uint32_t REG_WB_R_GAIN = 0x094;
constexpr std::uint32_t REG_WB_B_GAIN = 0x098;

constexpr std::uint32_t REG_CCM_ENABLE = 0x0A0;
constexpr std::uint32_t REG_CCM_MATRIX00 = 0x0A4;
constexpr std::uint32_t REG_CCM_MATRIX01 = 0x0A8;
constexpr std::uint32_t REG_CCM_MATRIX02 = 0x0AC;
constexpr std::uint32_t REG_CCM_MATRIX10 = 0x0B0;
constexpr std::uint32_t REG_CCM_MATRIX11 = 0x0B4;
constexpr std::uint32_t REG_CCM_MATRIX12 = 0x0B8;
constexpr std::uint32_t REG_CCM_MATRIX20 = 0x0BC;
constexpr std::uint32_t REG_CCM_MATRIX21 = 0x0C0;
constexpr std::uint32_t REG_CCM_MATRIX22 = 0x0C4;

constexpr std::uint32_t REG_GC_ENABLE = 0x0D0;

constexpr std::uint32_t REG_CSC_ENABLE    = 0x0DC;
constexpr std::uint32_t REG_CSC_STANDARD = 0x0E0;

constexpr std::uint32_t REG_CSE_ENABLE = 0x0F0;
constexpr std::uint32_t REG_CSE_SAT_GAIN = 0x0F4;

constexpr std::uint32_t REG_SHARPEN_ENABLE = 0x100;
constexpr std::uint32_t REG_SHARPEN_SIGMA = 0x104;
constexpr std::uint32_t REG_SHARPEN_STRENGTH = 0x108;

constexpr std::uint32_t REG_2DNR_ENABLE = 0x110;
constexpr std::uint32_t REG_2DNR_WINDOW = 0x114;
constexpr std::uint32_t REG_2DNR_PATCH = 0x118;
constexpr std::uint32_t REG_2DNR_WTS = 0x11C;

constexpr std::uint32_t REG_SCALE_ENABLE = 0x120;
constexpr std::uint32_t REG_SCALE_OUT_W = 0x124;
constexpr std::uint32_t REG_SCALE_OUT_H = 0x128;

constexpr std::uint32_t REG_YUV420_ENABLE = 0x130;

constexpr std::uint32_t REG_LUT_ADDR = 0x200;
constexpr std::uint32_t REG_LUT_DATA = 0x204;

constexpr std::uint32_t REG_RAW_FRAME_ADDR = 0x300;
constexpr std::uint32_t REG_YUV_FRAME_ADDR = 0x304;

constexpr std::uint32_t REG_MAX = 0x400;

constexpr std::uint32_t STATUS_DONE = 0x01;
constexpr std::uint32_t STATUS_BUSY = 0x02;

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
      return raw_buffer_.data();
   }

   std::uint8_t *get_yuv_buffer() {
      return yuv_buffer_.data();
   }

   std::size_t get_raw_buffer_size() const {
      return raw_buffer_.size();
   }

   std::size_t get_yuv_buffer_size() const {
      return yuv_buffer_.size();
   }

   std::uint32_t get_width() const {
      return width_;
   }

   std::uint32_t get_height() const {
      return height_;
   }

   void allocate_buffers();

private:
   void b_transport(tlm::tlm_generic_payload &trans, sc_core::sc_time &delay);
   unsigned int transport_dbg(tlm::tlm_generic_payload &trans);
   void dma_read();
   void dma_write();
   void processing_thread();
   void update_irq_output();

   std::uint32_t read_reg(std::uint32_t offset);
   void write_reg(std::uint32_t offset, std::uint32_t value);
   void update_config_from_regs();
   void trigger_processing();

   isp_pipeline pipeline_;
   isp_config config_;

   std::uint32_t reg_file_[REG_MAX / 4];

   std::uint32_t width_;
   std::uint32_t height_;
   std::uint32_t bit_depth_;
   std::uint32_t bayer_pattern_;

   std::vector<std::uint16_t> raw_buffer_;
   std::vector<std::uint8_t> yuv_buffer_;

   sc_core::sc_time access_latency_;
   sc_core::sc_event processing_event_;
   bool processing_done_;
   bool irq_level_;

   static constexpr std::size_t MAX_WIDTH = 4096;
   static constexpr std::size_t MAX_HEIGHT = 4096;
};

} // namespace cdc::components
