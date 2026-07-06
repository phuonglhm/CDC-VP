#include "isp_tlm.h"

#include <cstring>

namespace cdc::components {

isp_tlm::isp_tlm(sc_core::sc_module_name name, sc_core::sc_time access_latency)
    : sc_core::sc_module(name)
    , socket("socket")
    , reset_n("reset_n")
    , irq_out("irq_out")
    , access_latency_(access_latency)
    , width_(0)
    , height_(0)
    , bit_depth_(12)
    , bayer_pattern_(0)
    , processing_done_(false)
    , irq_level_(false) {
   socket.register_b_transport(this, &isp_tlm::b_transport);
   socket.register_transport_dbg(this, &isp_tlm::transport_dbg);

   std::memset(reg_file_, 0, sizeof(reg_file_));

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
   const std::size_t raw_size = static_cast<std::size_t>(width_) * height_;
   const std::size_t yuv_size = static_cast<std::size_t>(width_) * height_ * 3 / 2;

   raw_buffer_.resize(raw_size);
   yuv_buffer_.resize(yuv_size);
}

void isp_tlm::b_transport(tlm::tlm_generic_payload &trans, sc_core::sc_time &delay) {
   delay += access_latency_;

   const std::uint64_t offset = trans.get_address();
   const unsigned len = trans.get_data_length();
   unsigned char *ptr = trans.get_data_ptr();

   if (len != sizeof(std::uint32_t) || (offset & 0x3u) != 0u) {
      trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
      return;
   }

   if (offset >= REG_MAX) {
      trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
      return;
   }

   if (trans.get_command() == tlm::TLM_READ_COMMAND) {
      std::uint32_t value = read_reg(static_cast<std::uint32_t>(offset));
      std::memcpy(ptr, &value, sizeof(value));
   } else if (trans.get_command() == tlm::TLM_WRITE_COMMAND) {
      std::uint32_t value = 0;
      std::memcpy(&value, ptr, sizeof(value));
      write_reg(static_cast<std::uint32_t>(offset), value);
   } else {
      trans.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
      return;
   }

   trans.set_response_status(tlm::TLM_OK_RESPONSE);
}

void isp_tlm::dma_read() {
   std::uint32_t src_addr = reg_file_[REG_RAW_FRAME_ADDR / 4];
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
   std::uint32_t dest_addr = reg_file_[REG_YUV_FRAME_ADDR / 4];
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

unsigned int isp_tlm::transport_dbg(tlm::tlm_generic_payload &trans) {
   const std::uint64_t offset = trans.get_address();
   const unsigned len = trans.get_data_length();
   unsigned char *ptr = trans.get_data_ptr();

   if (len != sizeof(std::uint32_t) || (offset & 0x3u) != 0u || offset >= REG_MAX) {
      return 0;
   }

   if (trans.get_command() == tlm::TLM_READ_COMMAND) {
      std::uint32_t value = read_reg(static_cast<std::uint32_t>(offset));
      std::memcpy(ptr, &value, sizeof(value));
   } else if (trans.get_command() == tlm::TLM_WRITE_COMMAND) {
      std::uint32_t value = 0;
      std::memcpy(&value, ptr, sizeof(value));
      write_reg(static_cast<std::uint32_t>(offset), value);
   } else {
      return 0;
   }

   return sizeof(std::uint32_t);
}

std::uint32_t isp_tlm::read_reg(std::uint32_t offset) {
   switch (offset) {
   case REG_STATUS:
      return (processing_done_ ? STATUS_DONE : 0);

   default:
      if (offset < REG_MAX) {
         return reg_file_[offset / 4];
      }
      return 0;
   }
}

void isp_tlm::write_reg(std::uint32_t offset, std::uint32_t value) {
   switch (offset) {
   case REG_ISP_ENABLE:
      reg_file_[REG_ISP_ENABLE / 4] = value;
      break;

   case REG_TRIGGER:
      reg_file_[REG_TRIGGER / 4] = value;
      if (value & 0x1) {
         trigger_processing();
      }
      break;

   case REG_WIDTH:
      width_ = value & 0xFFFF;
      reg_file_[REG_WIDTH / 4] = width_;
      break;

   case REG_HEIGHT:
      height_ = value & 0xFFFF;
      reg_file_[REG_HEIGHT / 4] = height_;
      break;

   case REG_BIT_DEPTH:
      bit_depth_ = value & 0xFF;
      reg_file_[REG_BIT_DEPTH / 4] = bit_depth_;
      break;

   case REG_BAYER_PATTERN:
      bayer_pattern_ = value & 0xFF;
      reg_file_[REG_BAYER_PATTERN / 4] = bayer_pattern_;
      break;

   case REG_BLC_ENABLE:
      reg_file_[REG_BLC_ENABLE / 4] = value;
      update_config_from_regs();
      break;
   case REG_BLC_R_OFFSET:
      reg_file_[REG_BLC_R_OFFSET / 4] = value;
      update_config_from_regs();
      break;
   case REG_BLC_GR_OFFSET:
      reg_file_[REG_BLC_GR_OFFSET / 4] = value;
      update_config_from_regs();
      break;
   case REG_BLC_GB_OFFSET:
      reg_file_[REG_BLC_GB_OFFSET / 4] = value;
      update_config_from_regs();
      break;
   case REG_BLC_B_OFFSET:
      reg_file_[REG_BLC_B_OFFSET / 4] = value;
      update_config_from_regs();
      break;

   case REG_WB_ENABLE:
      reg_file_[REG_WB_ENABLE / 4] = value;
      update_config_from_regs();
      break;
   case REG_WB_R_GAIN: {
      reg_file_[REG_WB_R_GAIN / 4] = value;
      float gain;
      std::memcpy(&gain, &value, sizeof(float));
      config_.wb.r_gain = gain;
      break;
   }
   case REG_WB_B_GAIN: {
      reg_file_[REG_WB_B_GAIN / 4] = value;
      float gain;
      std::memcpy(&gain, &value, sizeof(float));
      config_.wb.b_gain = gain;
      break;
   }

   case REG_CCM_ENABLE:
      reg_file_[REG_CCM_ENABLE / 4] = value;
      update_config_from_regs();
      break;

   case REG_GC_ENABLE:
      reg_file_[REG_GC_ENABLE / 4] = value;
      update_config_from_regs();
      break;

   case REG_CSC_STANDARD:
      reg_file_[REG_CSC_STANDARD / 4] = value;
      config_.csc.conv_standard = static_cast<std::uint8_t>(value);
      break;

   case REG_CSE_ENABLE:
      reg_file_[REG_CSE_ENABLE / 4] = value;
      update_config_from_regs();
      break;
   case REG_CSE_SAT_GAIN: {
      reg_file_[REG_CSE_SAT_GAIN / 4] = value;
      float gain;
      std::memcpy(&gain, &value, sizeof(float));
      config_.cse.saturation_gain = gain;
      break;
   }

   case REG_SHARPEN_ENABLE:
      reg_file_[REG_SHARPEN_ENABLE / 4] = value;
      update_config_from_regs();
      break;
   case REG_SHARPEN_SIGMA:
      reg_file_[REG_SHARPEN_SIGMA / 4] = value;
      update_config_from_regs();
      break;
   case REG_SHARPEN_STRENGTH:
      reg_file_[REG_SHARPEN_STRENGTH / 4] = value;
      update_config_from_regs();
      break;

   case REG_2DNR_ENABLE:
      reg_file_[REG_2DNR_ENABLE / 4] = value;
      update_config_from_regs();
      break;
   case REG_2DNR_WINDOW:
      reg_file_[REG_2DNR_WINDOW / 4] = value;
      update_config_from_regs();
      break;
   case REG_2DNR_PATCH:
      reg_file_[REG_2DNR_PATCH / 4] = value;
      update_config_from_regs();
      break;
   case REG_2DNR_WTS:
      reg_file_[REG_2DNR_WTS / 4] = value;
      update_config_from_regs();
      break;

   case REG_SCALE_ENABLE:
      reg_file_[REG_SCALE_ENABLE / 4] = value;
      update_config_from_regs();
      break;
   case REG_SCALE_OUT_W:
      reg_file_[REG_SCALE_OUT_W / 4] = value;
      update_config_from_regs();
      break;
   case REG_SCALE_OUT_H:
      reg_file_[REG_SCALE_OUT_H / 4] = value;
      update_config_from_regs();
      break;

   case REG_YUV420_ENABLE:
      reg_file_[REG_YUV420_ENABLE / 4] = value;
      update_config_from_regs();
      break;

   case REG_DPC_ENABLE:
   case REG_DPC_THRESH:
   case REG_LSC_ENABLE:
   case REG_LSC_GRID_W:
   case REG_LSC_GRID_H:
   case REG_DG_ENABLE:
   case REG_DG_GAIN:
   case REG_BNR_ENABLE:
   case REG_BNR_WINDOW:
   case REG_DEMOSAIC_ENABLE:
   case REG_AWB_ENABLE:
   case REG_AWB_ALGORITHM:
   case REG_AWB_R_GAIN:
   case REG_AWB_B_GAIN:
      reg_file_[offset / 4] = value;
      update_config_from_regs();
      break;

   default:
      if (offset < REG_MAX) {
         reg_file_[offset / 4] = value;
      }
      break;
   }
}

void isp_tlm::update_config_from_regs() {
   config_.blc.is_enable = (reg_file_[REG_BLC_ENABLE / 4] & 0x1) != 0;
   config_.blc.r_offset = reg_file_[REG_BLC_R_OFFSET / 4];
   config_.blc.gr_offset = reg_file_[REG_BLC_GR_OFFSET / 4];
   config_.blc.gb_offset = reg_file_[REG_BLC_GB_OFFSET / 4];
   config_.blc.b_offset = reg_file_[REG_BLC_B_OFFSET / 4];
   config_.blc.r_sat = 4095;
   config_.blc.gr_sat = 4095;
   config_.blc.gb_sat = 4095;
   config_.blc.b_sat = 4095;
   config_.blc.is_linear = false;

   config_.dpc.is_enable = (reg_file_[REG_DPC_ENABLE / 4] & 0x1) != 0;
   config_.dpc.dp_threshold = reg_file_[REG_DPC_THRESH / 4];

   config_.lsc.is_enable = (reg_file_[REG_LSC_ENABLE / 4] & 0x1) != 0;
   config_.lsc.grid_width = reg_file_[REG_LSC_GRID_W / 4];
   config_.lsc.grid_height = reg_file_[REG_LSC_GRID_H / 4];

   config_.dg.is_enable = (reg_file_[REG_DG_ENABLE / 4] & 0x1) != 0;
   config_.dg.current_gain = reg_file_[REG_DG_GAIN / 4] & 0xFF;

   config_.bnr.is_enable = (reg_file_[REG_BNR_ENABLE / 4] & 0x1) != 0;
   config_.bnr.filter_window = reg_file_[REG_BNR_WINDOW / 4] & 0xFF;

   config_.demosaic.is_enable = (reg_file_[REG_DEMOSAIC_ENABLE / 4] & 0x1) != 0;

   config_.awb.is_enable = (reg_file_[REG_AWB_ENABLE / 4] & 0x1) != 0;
   config_.awb.algorithm = reg_file_[REG_AWB_ALGORITHM / 4] & 0xFF;
   float awb_r_gain, awb_b_gain;
   std::memcpy(&awb_r_gain, &reg_file_[REG_AWB_R_GAIN / 4], sizeof(float));
   std::memcpy(&awb_b_gain, &reg_file_[REG_AWB_B_GAIN / 4], sizeof(float));
   config_.awb.r_gain_out = awb_r_gain;
   config_.awb.b_gain_out = awb_b_gain;

   config_.wb.is_enable = (reg_file_[REG_WB_ENABLE / 4] & 0x1) != 0;

   config_.ccm.is_enable = (reg_file_[REG_CCM_ENABLE / 4] & 0x1) != 0;

   config_.gc.is_enable = (reg_file_[REG_GC_ENABLE / 4] & 0x1) != 0;

   config_.cse.is_enable = (reg_file_[REG_CSE_ENABLE / 4] & 0x1) != 0;

   config_.sharpen.is_enable = (reg_file_[REG_SHARPEN_ENABLE / 4] & 0x1) != 0;
   config_.sharpen.sharpen_sigma = reg_file_[REG_SHARPEN_SIGMA / 4] & 0xFF;
   config_.sharpen.sharpen_strength = reg_file_[REG_SHARPEN_STRENGTH / 4] & 0xFFFF;

   config_.twodnr.is_enable = (reg_file_[REG_2DNR_ENABLE / 4] & 0x1) != 0;
   config_.twodnr.window_size = reg_file_[REG_2DNR_WINDOW / 4] & 0xFF;
   config_.twodnr.patch_size = reg_file_[REG_2DNR_PATCH / 4] & 0xFF;
   config_.twodnr.wts = reg_file_[REG_2DNR_WTS / 4] & 0xFFFF;

   config_.scale.is_enable = (reg_file_[REG_SCALE_ENABLE / 4] & 0x1) != 0;
   config_.scale.out_width = reg_file_[REG_SCALE_OUT_W / 4];
   config_.scale.out_height = reg_file_[REG_SCALE_OUT_H / 4];

   config_.yuv420.is_enable = (reg_file_[REG_YUV420_ENABLE / 4] & 0x1) != 0;
}

void isp_tlm::trigger_processing() {
   if (width_ == 0 || height_ == 0) {
      return;
   }

   pipeline_.set_dimensions(width_, height_);

   // Map register-level bayer_pattern (0..3) to cfa_types enum
   cfa_types bayer_pattern = cfa_types::RGGB;
   switch (bayer_pattern_) {
   case 0:
      bayer_pattern = cfa_types::RGGB;
      break;
   case 1:
      bayer_pattern = cfa_types::GRBG;
      break;
   case 2:
      bayer_pattern = cfa_types::BGGR;
      break;
   case 3:
      bayer_pattern = cfa_types::GBRG;
      break;
   default:
      bayer_pattern = cfa_types::RGGB;
      break;
   }
   pipeline_.set_input_format(static_cast<std::uint8_t>(bit_depth_), bayer_pattern);
   pipeline_.set_lsc_mem(nullptr);

   update_config_from_regs();

   const std::size_t raw_size = static_cast<std::size_t>(width_) * height_;
   const std::size_t yuv_size = static_cast<std::size_t>(width_) * height_ * 3 / 2;

   raw_buffer_.resize(raw_size);
   yuv_buffer_.resize(yuv_size);

   dma_read();

   pipeline_.run(raw_buffer_.data(), yuv_buffer_, config_);

   dma_write();

   processing_done_ = true;
   irq_level_ = true;
}

void isp_tlm::processing_thread() {
   while (true) {
      wait();

      if (!reset_n.read()) {
         width_ = 0;
         height_ = 0;
         processing_done_ = false;
         irq_level_ = false;
         raw_buffer_.clear();
         yuv_buffer_.clear();
         std::memset(reg_file_, 0, sizeof(reg_file_));
         continue;
      }

      trigger_processing();
   }
}

} // namespace cdc::components
