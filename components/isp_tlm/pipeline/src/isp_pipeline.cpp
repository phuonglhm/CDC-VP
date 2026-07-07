#include "isp_pipeline.h"

#include <cstring>

isp_pipeline::isp_pipeline()
    : width_(0)
    , height_(0)
    , input_bit_depth_(12)
    , input_bayer_pattern_(cfa_types::RGGB)
    , working_bit_depth_(12)
    , lsc_mem_ptr_(nullptr)
    , awb_r_gain_(1.0f)
    , awb_b_gain_(1.0f)
    , bit_depth_(12)
    , bayer_pattern_(0)
    , processing_done_(false)
    , ctrl_(0)
    , irq_enable_(0)
    , irq_status_(0)
    , scratch_frame_addr_(0)
    , src_size_bytes_(0)
    , dst_size_bytes_(0)
    , stride_(0)
    , format_(0)
    , op_mode_(0)
    , weights_frame_addr_(0)
    , param_frame_addr_(0)
    , lut_addr_(0)
    , aec_feedback_(0) {
}

isp_pipeline::~isp_pipeline() {
}

void isp_pipeline::set_dimensions(std::uint32_t width, std::uint32_t height) {
   width_ = width;
   height_ = height;

   const std::size_t raw_pixels = static_cast<std::size_t>(width_) * height_;
   const std::size_t rgb_pixels = raw_pixels * 3u;
   const std::size_t yuv_pixels = raw_pixels * 3u;

   raw_buf_.resize(raw_pixels);
   blc_out_.resize(raw_pixels);
   dpc_out_.resize(raw_pixels);
   lsc_out_.resize(raw_pixels);
   dg_out_.resize(raw_pixels);
   bnr_out_.resize(raw_pixels);
   demosaic_out_.resize(rgb_pixels);
   wb_out_.resize(rgb_pixels);
   ccm_out_.resize(rgb_pixels);
   gc_out_.resize(rgb_pixels);
   csc_out_.resize(yuv_pixels);
   cse_out_.resize(yuv_pixels);
   sharpen_out_.resize(yuv_pixels);
   twodnr_out_.resize(yuv_pixels);
   scale_out_.resize(yuv_pixels);
   final_out_.resize(yuv_pixels);
}

void isp_pipeline::set_lsc_mem(const float *lsc_mem) {
   lsc_mem_ptr_ = lsc_mem;
}

void isp_pipeline::set_input_format(std::uint8_t bit_depth, cfa_types bayer_pattern) {
   input_bit_depth_ = bit_depth;
   input_bayer_pattern_ = bayer_pattern;
   working_bit_depth_ = bit_depth;
}

// ── Register-file access ───────────────────────────────────────────────────────

std::uint32_t isp_pipeline::read_reg(std::uint32_t offset) {
   switch (offset) {
      // ── Global/Common Registers (0x0000 - 0x00FF)
   case cdc::components::REG_CTRL:
      return ctrl_;
   case cdc::components::REG_STATUS:
      return (processing_done_ ? cdc::components::STATUS_DONE : 0) |
             (ctrl_ & 0x1 ? cdc::components::STATUS_IDLE : 0);
   case cdc::components::REG_IRQ_ENABLE:
      return irq_enable_;
   case cdc::components::REG_IRQ_STATUS:
      return irq_status_;
   case cdc::components::REG_WIDTH:
      return width_;
   case cdc::components::REG_HEIGHT:
      return height_;
   case cdc::components::REG_STRIDE:
      return stride_;
   case cdc::components::REG_FORMAT:
      return format_;
   case cdc::components::REG_OP_MODE:
      return op_mode_;
   case cdc::components::REG_BIT_DEPTH:
      return bit_depth_;
   case cdc::components::REG_BAYER_PATTERN:
      return bayer_pattern_;

   // ── Buffer Descriptors (0x0100 - 0x01FF)
   case cdc::components::REG_SRC_ADDR:
      return raw_frame_addr_;
   case cdc::components::REG_DST_ADDR:
      return yuv_frame_addr_;
   case cdc::components::REG_SCRATCH_ADDR:
      return scratch_frame_addr_;
   case cdc::components::REG_SRC_SIZE_BYTES:
      return src_size_bytes_;
   case cdc::components::REG_DST_SIZE_BYTES:
      return dst_size_bytes_;
   case cdc::components::REG_WEIGHTS_ADDR:
      return weights_frame_addr_;
   case cdc::components::REG_PARAM_ADDR:
      return param_frame_addr_;

   // ── Block Parameters in order of flow ──

   // 1. BLC
   case cdc::components::REG_BLC_ENABLE:
      return static_cast<std::uint32_t>(config_.blc.is_enable);
   case cdc::components::REG_BLC_LINEAR:
      return static_cast<std::uint32_t>(config_.blc.is_linear);
   case cdc::components::REG_BLC_R_OFFSET:
      return config_.blc.r_offset;
   case cdc::components::REG_BLC_GR_OFFSET:
      return config_.blc.gr_offset;
   case cdc::components::REG_BLC_GB_OFFSET:
      return config_.blc.gb_offset;
   case cdc::components::REG_BLC_B_OFFSET:
      return config_.blc.b_offset;
   case cdc::components::REG_BLC_R_SAT:
      return config_.blc.r_sat;
   case cdc::components::REG_BLC_GR_SAT:
      return config_.blc.gr_sat;
   case cdc::components::REG_BLC_GB_SAT:
      return config_.blc.gb_sat;
   case cdc::components::REG_BLC_B_SAT:
      return config_.blc.b_sat;

   // 2. DPC
   case cdc::components::REG_DPC_ENABLE:
      return static_cast<std::uint32_t>(config_.dpc.is_enable);
   case cdc::components::REG_DPC_THRESH:
      return config_.dpc.dp_threshold;

   // 3. LSC
   case cdc::components::REG_LSC_ENABLE:
      return static_cast<std::uint32_t>(config_.lsc.is_enable);
   case cdc::components::REG_LSC_GRID_W:
      return config_.lsc.grid_width;
   case cdc::components::REG_LSC_GRID_H:
      return config_.lsc.grid_height;

   // 4. DG
   case cdc::components::REG_DG_ENABLE:
      return static_cast<std::uint32_t>(config_.dg.is_enable);
   case cdc::components::REG_DG_GAIN:
      return static_cast<std::uint32_t>(config_.dg.current_gain);
   case cdc::components::REG_DG_AUTO:
      return static_cast<std::uint32_t>(config_.dg.is_auto);

   // 5. BNR
   case cdc::components::REG_BNR_ENABLE:
      return static_cast<std::uint32_t>(config_.bnr.is_enable);
   case cdc::components::REG_BNR_WINDOW:
      return static_cast<std::uint32_t>(config_.bnr.filter_window);

   // 6. Demosaic
   case cdc::components::REG_DEMOSAIC_ENABLE:
      return static_cast<std::uint32_t>(config_.demosaic.is_enable);

   // 7. AWB
   case cdc::components::REG_AWB_ENABLE:
      return static_cast<std::uint32_t>(config_.awb.is_enable);
   case cdc::components::REG_AWB_ALGORITHM:
      return static_cast<std::uint32_t>(config_.awb.algorithm);
   case cdc::components::REG_AWB_R_GAIN:
      return awb_r_gain_raw_;
   case cdc::components::REG_AWB_B_GAIN:
      return awb_b_gain_raw_;
   case cdc::components::REG_AWB_UNDER_PCT: {
      std::uint32_t raw;
      std::memcpy(&raw, &config_.awb.underexposed_percentage, sizeof(float));
      return raw;
   }
   case cdc::components::REG_AWB_OVER_PCT: {
      std::uint32_t raw;
      std::memcpy(&raw, &config_.awb.overexposed_percentage, sizeof(float));
      return raw;
   }
   case cdc::components::REG_AWB_PERCENT: {
      std::uint32_t raw;
      std::memcpy(&raw, &config_.awb.percentage, sizeof(float));
      return raw;
   }

   // 8. WB
   case cdc::components::REG_WB_ENABLE:
      return static_cast<std::uint32_t>(config_.wb.is_enable);
   case cdc::components::REG_WB_R_GAIN: {
      std::uint32_t raw;
      std::memcpy(&raw, &config_.wb.r_gain, sizeof(float));
      return raw;
   }
   case cdc::components::REG_WB_B_GAIN: {
      std::uint32_t raw;
      std::memcpy(&raw, &config_.wb.b_gain, sizeof(float));
      return raw;
   }

   // 9. CCM
   case cdc::components::REG_CCM_ENABLE:
      return static_cast<std::uint32_t>(config_.ccm.is_enable);
   case cdc::components::REG_CCM_MATRIX00:
      return ccm_matrix_raw_[0];
   case cdc::components::REG_CCM_MATRIX01:
      return ccm_matrix_raw_[1];
   case cdc::components::REG_CCM_MATRIX02:
      return ccm_matrix_raw_[2];
   case cdc::components::REG_CCM_MATRIX10:
      return ccm_matrix_raw_[3];
   case cdc::components::REG_CCM_MATRIX11:
      return ccm_matrix_raw_[4];
   case cdc::components::REG_CCM_MATRIX12:
      return ccm_matrix_raw_[5];
   case cdc::components::REG_CCM_MATRIX20:
      return ccm_matrix_raw_[6];
   case cdc::components::REG_CCM_MATRIX21:
      return ccm_matrix_raw_[7];
   case cdc::components::REG_CCM_MATRIX22:
      return ccm_matrix_raw_[8];

   // 10. GC
   case cdc::components::REG_GC_ENABLE:
      return static_cast<std::uint32_t>(config_.gc.is_enable);
   case cdc::components::REG_GC_LUT_ADDR:
      return lut_addr_;
   case cdc::components::REG_GC_LUT_DATA: {
      const std::vector<std::uint16_t> *active_lut = nullptr;
      if (bit_depth_ == 8)
         active_lut = &config_.gc.gamma_lut_8;
      else if (bit_depth_ == 10)
         active_lut = &config_.gc.gamma_lut_10;
      else if (bit_depth_ == 12)
         active_lut = &config_.gc.gamma_lut_12;
      else if (bit_depth_ == 14)
         active_lut = &config_.gc.gamma_lut_14;

      if (active_lut && lut_addr_ < active_lut->size()) {
         return (*active_lut)[lut_addr_];
      }
      return 0;
   }

   // 11. AEC
   case cdc::components::REG_AEC_ENABLE:
      return static_cast<std::uint32_t>(config_.aec.is_enable);
   case cdc::components::REG_AEC_FEEDBACK:
      return static_cast<std::uint32_t>(aec_feedback_);
   case cdc::components::REG_AEC_CENTER_ILLUM:
      return static_cast<std::uint32_t>(config_.aec.center_illuminance);
   case cdc::components::REG_AEC_SKEWNESS: {
      std::uint32_t raw;
      std::memcpy(&raw, &config_.aec.histogram_skewness, sizeof(float));
      return raw;
   }

   // 12. CSC
   case cdc::components::REG_CSC_ENABLE:
      return static_cast<std::uint32_t>(config_.csc.is_enable);
   case cdc::components::REG_CSC_STANDARD:
      return static_cast<std::uint32_t>(config_.csc.conv_standard);

   // 13. CSE
   case cdc::components::REG_CSE_ENABLE:
      return static_cast<std::uint32_t>(config_.cse.is_enable);
   case cdc::components::REG_CSE_SAT_GAIN: {
      std::uint32_t raw;
      std::memcpy(&raw, &config_.cse.saturation_gain, sizeof(float));
      return raw;
   }

   // 14. Sharpen
   case cdc::components::REG_SHARPEN_ENABLE:
      return static_cast<std::uint32_t>(config_.sharpen.is_enable);
   case cdc::components::REG_SHARPEN_SIGMA:
      return static_cast<std::uint32_t>(config_.sharpen.sharpen_sigma);
   case cdc::components::REG_SHARPEN_STRENGTH:
      return config_.sharpen.sharpen_strength;

   // 15. 2DNR
   case cdc::components::REG_2DNR_ENABLE:
      return static_cast<std::uint32_t>(config_.twodnr.is_enable);
   case cdc::components::REG_2DNR_WINDOW:
      return static_cast<std::uint32_t>(config_.twodnr.window_size);
   case cdc::components::REG_2DNR_PATCH:
      return static_cast<std::uint32_t>(config_.twodnr.patch_size);
   case cdc::components::REG_2DNR_WTS:
      return config_.twodnr.wts;

   // 16. Scale
   case cdc::components::REG_SCALE_ENABLE:
      return static_cast<std::uint32_t>(config_.scale.is_enable);
   case cdc::components::REG_SCALE_OUT_W:
      return config_.scale.out_width;
   case cdc::components::REG_SCALE_OUT_H:
      return config_.scale.out_height;

   // 17. YUV420
   case cdc::components::REG_YUV420_ENABLE:
      return static_cast<std::uint32_t>(config_.yuv420.is_enable);

   default:
      return 0;
   }
}

void isp_pipeline::write_reg(std::uint32_t offset, std::uint32_t value) {
   switch (offset) {
   // ── Global/Common Registers (0x0000 - 0x00FF)
   case cdc::components::REG_CTRL:
      ctrl_ = value;
      break;
   case cdc::components::REG_IRQ_ENABLE:
      irq_enable_ = value;
      break;
   case cdc::components::REG_IRQ_STATUS:
      irq_status_ &= ~value; // Write 1 to clear
      break;
   case cdc::components::REG_WIDTH:
      width_ = value & 0xFFFF;
      break;
   case cdc::components::REG_HEIGHT:
      height_ = value & 0xFFFF;
      break;
   case cdc::components::REG_STRIDE:
      stride_ = value;
      break;
   case cdc::components::REG_FORMAT:
      format_ = value;
      break;
   case cdc::components::REG_OP_MODE:
      op_mode_ = value;
      break;
   case cdc::components::REG_BIT_DEPTH:
      bit_depth_ = value & 0xFF;
      break;
   case cdc::components::REG_BAYER_PATTERN:
      bayer_pattern_ = value & 0xFF;
      break;

   // ── Buffer Descriptors (0x0100 - 0x01FF)
   case cdc::components::REG_SRC_ADDR:
      raw_frame_addr_ = value;
      break;
   case cdc::components::REG_DST_ADDR:
      yuv_frame_addr_ = value;
      break;
   case cdc::components::REG_SCRATCH_ADDR:
      scratch_frame_addr_ = value;
      break;
   case cdc::components::REG_SRC_SIZE_BYTES:
      src_size_bytes_ = value;
      break;
   case cdc::components::REG_DST_SIZE_BYTES:
      dst_size_bytes_ = value;
      break;
   case cdc::components::REG_WEIGHTS_ADDR:
      weights_frame_addr_ = value;
      break;
   case cdc::components::REG_PARAM_ADDR:
      param_frame_addr_ = value;
      break;

   // ── Block Parameters ──

   // 1. BLC
   case cdc::components::REG_BLC_ENABLE:
      config_.blc.is_enable = (value & 0x1) != 0;
      break;
   case cdc::components::REG_BLC_LINEAR:
      config_.blc.is_linear = (value & 0x1) != 0;
      break;
   case cdc::components::REG_BLC_R_OFFSET:
      config_.blc.r_offset = static_cast<std::uint16_t>(value & 0xFFFF);
      break;
   case cdc::components::REG_BLC_GR_OFFSET:
      config_.blc.gr_offset = static_cast<std::uint16_t>(value & 0xFFFF);
      break;
   case cdc::components::REG_BLC_GB_OFFSET:
      config_.blc.gb_offset = static_cast<std::uint16_t>(value & 0xFFFF);
      break;
   case cdc::components::REG_BLC_B_OFFSET:
      config_.blc.b_offset = static_cast<std::uint16_t>(value & 0xFFFF);
      break;
   case cdc::components::REG_BLC_R_SAT:
      config_.blc.r_sat = static_cast<std::uint16_t>(value & 0xFFFF);
      break;
   case cdc::components::REG_BLC_GR_SAT:
      config_.blc.gr_sat = static_cast<std::uint16_t>(value & 0xFFFF);
      break;
   case cdc::components::REG_BLC_GB_SAT:
      config_.blc.gb_sat = static_cast<std::uint16_t>(value & 0xFFFF);
      break;
   case cdc::components::REG_BLC_B_SAT:
      config_.blc.b_sat = static_cast<std::uint16_t>(value & 0xFFFF);
      break;

   // 2. DPC
   case cdc::components::REG_DPC_ENABLE:
      config_.dpc.is_enable = (value & 0x1) != 0;
      break;
   case cdc::components::REG_DPC_THRESH:
      config_.dpc.dp_threshold = static_cast<std::uint16_t>(value & 0xFFFF);
      break;

   // 3. LSC
   case cdc::components::REG_LSC_ENABLE:
      config_.lsc.is_enable = (value & 0x1) != 0;
      break;
   case cdc::components::REG_LSC_GRID_W:
      config_.lsc.grid_width = static_cast<std::uint16_t>(value & 0xFFFF);
      break;
   case cdc::components::REG_LSC_GRID_H:
      config_.lsc.grid_height = static_cast<std::uint16_t>(value & 0xFFFF);
      break;

   // 4. DG
   case cdc::components::REG_DG_ENABLE:
      config_.dg.is_enable = (value & 0x1) != 0;
      break;
   case cdc::components::REG_DG_GAIN:
      config_.dg.current_gain = static_cast<std::uint8_t>(value & 0xFF);
      break;
   case cdc::components::REG_DG_AUTO:
      config_.dg.is_auto = (value & 0x1) != 0;
      break;

   // 5. BNR
   case cdc::components::REG_BNR_ENABLE:
      config_.bnr.is_enable = (value & 0x1) != 0;
      break;
   case cdc::components::REG_BNR_WINDOW:
      config_.bnr.filter_window = static_cast<std::uint8_t>(value & 0xFF);
      break;

   // 6. Demosaic
   case cdc::components::REG_DEMOSAIC_ENABLE:
      config_.demosaic.is_enable = (value & 0x1) != 0;
      break;

   // 7. AWB
   case cdc::components::REG_AWB_ENABLE:
      config_.awb.is_enable = (value & 0x1) != 0;
      break;
   case cdc::components::REG_AWB_ALGORITHM:
      config_.awb.algorithm = static_cast<std::uint8_t>(value & 0xFF);
      break;
   case cdc::components::REG_AWB_R_GAIN:
      awb_r_gain_raw_ = value;
      std::memcpy(&config_.awb.r_gain_out, &value, sizeof(float));
      break;
   case cdc::components::REG_AWB_B_GAIN:
      awb_b_gain_raw_ = value;
      std::memcpy(&config_.awb.b_gain_out, &value, sizeof(float));
      break;
   case cdc::components::REG_AWB_UNDER_PCT:
      std::memcpy(&config_.awb.underexposed_percentage, &value, sizeof(float));
      break;
   case cdc::components::REG_AWB_OVER_PCT:
      std::memcpy(&config_.awb.overexposed_percentage, &value, sizeof(float));
      break;
   case cdc::components::REG_AWB_PERCENT:
      std::memcpy(&config_.awb.percentage, &value, sizeof(float));
      break;

   // 8. WB
   case cdc::components::REG_WB_ENABLE:
      config_.wb.is_enable = (value & 0x1) != 0;
      break;
   case cdc::components::REG_WB_R_GAIN:
      std::memcpy(&config_.wb.r_gain, &value, sizeof(float));
      break;
   case cdc::components::REG_WB_B_GAIN:
      std::memcpy(&config_.wb.b_gain, &value, sizeof(float));
      break;

   // 9. CCM
   case cdc::components::REG_CCM_ENABLE:
      config_.ccm.is_enable = (value & 0x1) != 0;
      break;
   case cdc::components::REG_CCM_MATRIX00:
      ccm_matrix_raw_[0] = value;
      std::memcpy(&config_.ccm.corrected_red[0], &value, sizeof(float));
      break;
   case cdc::components::REG_CCM_MATRIX01:
      ccm_matrix_raw_[1] = value;
      std::memcpy(&config_.ccm.corrected_red[1], &value, sizeof(float));
      break;
   case cdc::components::REG_CCM_MATRIX02:
      ccm_matrix_raw_[2] = value;
      std::memcpy(&config_.ccm.corrected_red[2], &value, sizeof(float));
      break;
   case cdc::components::REG_CCM_MATRIX10:
      ccm_matrix_raw_[3] = value;
      std::memcpy(&config_.ccm.corrected_green[0], &value, sizeof(float));
      break;
   case cdc::components::REG_CCM_MATRIX11:
      ccm_matrix_raw_[4] = value;
      std::memcpy(&config_.ccm.corrected_green[1], &value, sizeof(float));
      break;
   case cdc::components::REG_CCM_MATRIX12:
      ccm_matrix_raw_[5] = value;
      std::memcpy(&config_.ccm.corrected_green[2], &value, sizeof(float));
      break;
   case cdc::components::REG_CCM_MATRIX20:
      ccm_matrix_raw_[6] = value;
      std::memcpy(&config_.ccm.corrected_blue[0], &value, sizeof(float));
      break;
   case cdc::components::REG_CCM_MATRIX21:
      ccm_matrix_raw_[7] = value;
      std::memcpy(&config_.ccm.corrected_blue[1], &value, sizeof(float));
      break;
   case cdc::components::REG_CCM_MATRIX22:
      ccm_matrix_raw_[8] = value;
      std::memcpy(&config_.ccm.corrected_blue[2], &value, sizeof(float));
      break;

   // 10. GC
   case cdc::components::REG_GC_ENABLE:
      config_.gc.is_enable = (value & 0x1) != 0;
      break;
   case cdc::components::REG_GC_LUT_ADDR:
      lut_addr_ = value & 0x3FFF;
      break;
   case cdc::components::REG_GC_LUT_DATA: {
      std::vector<std::uint16_t> *active_lut = nullptr;
      std::size_t expected_size = 0;
      if (bit_depth_ == 8) {
         active_lut = &config_.gc.gamma_lut_8;
         expected_size = 256;
      } else if (bit_depth_ == 10) {
         active_lut = &config_.gc.gamma_lut_10;
         expected_size = 1024;
      } else if (bit_depth_ == 12) {
         active_lut = &config_.gc.gamma_lut_12;
         expected_size = 4096;
      } else if (bit_depth_ == 14) {
         active_lut = &config_.gc.gamma_lut_14;
         expected_size = 16384;
      }

      if (active_lut) {
         if (active_lut->size() != expected_size) {
            active_lut->resize(expected_size, 0);
         }
         if (lut_addr_ < expected_size) {
            (*active_lut)[lut_addr_] = static_cast<std::uint16_t>(value & 0xFFFF);
            lut_addr_++;
         }
      }
      break;
   }

   // 11. AEC
   case cdc::components::REG_AEC_ENABLE:
      config_.aec.is_enable = (value & 0x1) != 0;
      break;
   case cdc::components::REG_AEC_CENTER_ILLUM:
      config_.aec.center_illuminance = static_cast<std::uint8_t>(value & 0xFF);
      break;
   case cdc::components::REG_AEC_SKEWNESS:
      std::memcpy(&config_.aec.histogram_skewness, &value, sizeof(float));
      break;

   // 12. CSC
   case cdc::components::REG_CSC_ENABLE:
      config_.csc.is_enable = (value & 0x1) != 0;
      break;
   case cdc::components::REG_CSC_STANDARD:
      config_.csc.conv_standard = static_cast<std::uint8_t>(value & 0xFF);
      break;

   // 13. CSE
   case cdc::components::REG_CSE_ENABLE:
      config_.cse.is_enable = (value & 0x1) != 0;
      break;
   case cdc::components::REG_CSE_SAT_GAIN:
      std::memcpy(&config_.cse.saturation_gain, &value, sizeof(float));
      break;

   // 14. Sharpen
   case cdc::components::REG_SHARPEN_ENABLE:
      config_.sharpen.is_enable = (value & 0x1) != 0;
      break;
   case cdc::components::REG_SHARPEN_SIGMA:
      config_.sharpen.sharpen_sigma = static_cast<std::uint8_t>(value & 0xFF);
      break;
   case cdc::components::REG_SHARPEN_STRENGTH:
      config_.sharpen.sharpen_strength = static_cast<std::uint16_t>(value & 0xFFFF);
      break;

   // 15. 2DNR
   case cdc::components::REG_2DNR_ENABLE:
      config_.twodnr.is_enable = (value & 0x1) != 0;
      break;
   case cdc::components::REG_2DNR_WINDOW:
      config_.twodnr.window_size = static_cast<std::uint8_t>(value & 0xFF);
      break;
   case cdc::components::REG_2DNR_PATCH:
      config_.twodnr.patch_size = static_cast<std::uint8_t>(value & 0xFF);
      break;
   case cdc::components::REG_2DNR_WTS:
      config_.twodnr.wts = static_cast<std::uint16_t>(value & 0xFFFF);
      break;

   // 16. Scale
   case cdc::components::REG_SCALE_ENABLE:
      config_.scale.is_enable = (value & 0x1) != 0;
      break;
   case cdc::components::REG_SCALE_OUT_W:
      config_.scale.out_width = static_cast<std::uint16_t>(value & 0xFFFF);
      break;
   case cdc::components::REG_SCALE_OUT_H:
      config_.scale.out_height = static_cast<std::uint16_t>(value & 0xFFFF);
      break;

   // 17. YUV420
   case cdc::components::REG_YUV420_ENABLE:
      config_.yuv420.is_enable = (value & 0x1) != 0;
      break;

   default:
      break;
   }
}

// ── Pipeline run ──────────────────────────────────────────────────────────────

void isp_pipeline::run(const std::uint16_t *raw_in, std::vector<std::uint8_t> &yuv_out) {
   if (width_ == 0 || height_ == 0 || raw_in == nullptr) {
      yuv_out.clear();
      return;
   }

   const std::size_t raw_pixels = static_cast<std::size_t>(width_) * height_;
   const std::size_t rgb_pixels = raw_pixels * 3u;
   const std::size_t yuv_pixels = raw_pixels * 3u;

   const std::uint32_t work_max = (1u << working_bit_depth_) - 1u;
   const std::uint32_t src_max = (1u << input_bit_depth_) - 1u;
   if (src_max == 0)
      return;
   const std::uint64_t scale_num = work_max;
   const std::uint64_t scale_den = src_max;

   std::memcpy(raw_buf_.data(), raw_in, raw_pixels * sizeof(std::uint16_t));
   for (std::size_t p = 0; p < raw_pixels; ++p) {
      const std::uint64_t v = static_cast<std::uint64_t>(raw_buf_[p]);
      const std::uint64_t scaled = (v * scale_num) / scale_den;
      raw_buf_[p] = static_cast<std::uint16_t>(scaled > work_max ? work_max : scaled);
   }

   const std::uint8_t bd = working_bit_depth_;
   const cfa_types cfa = input_bayer_pattern_;

   blc_.process(raw_buf_.data(), blc_out_.data(), width_, height_, config_.blc, cfa, bd);
   dpc_.process(blc_out_.data(), dpc_out_.data(), width_, height_, config_.dpc);
   lsc_.process(dpc_out_.data(), lsc_out_.data(), width_, height_, config_.lsc, lsc_mem_ptr_, cfa, bd);

   // DG auto-stepping feedback
   if (config_.dg.is_auto) {
      if (aec_feedback_ < 0) {
         if (config_.dg.current_gain < 9) { // 9 is kGainArraySize - 1
            config_.dg.current_gain++;
         }
      } else if (aec_feedback_ > 0) {
         if (config_.dg.current_gain > 0) {
            config_.dg.current_gain--;
         }
      }
   }
   dg_.process(lsc_out_.data(), dg_out_.data(), width_, height_, config_.dg, bd);
   bnr_.process(dg_out_.data(), bnr_out_.data(), width_, height_, config_.bnr, cfa, bd);

   demosaic_.process(bnr_out_.data(), demosaic_out_.data(), width_, height_, config_.demosaic, cfa, bd);

   awb_config awb_cfg = config_.awb;
   if (config_.awb.is_enable) {
      awb_.process(demosaic_out_.data(), width_, height_, awb_cfg, bd);
      awb_r_gain_ = awb_cfg.r_gain_out;
      awb_b_gain_ = awb_cfg.b_gain_out;
      config_.awb.r_gain_out = awb_cfg.r_gain_out;
      config_.awb.b_gain_out = awb_cfg.b_gain_out;
      std::memcpy(&awb_r_gain_raw_, &awb_cfg.r_gain_out, sizeof(float));
      std::memcpy(&awb_b_gain_raw_, &awb_cfg.b_gain_out, sizeof(float));
   } else {
      awb_r_gain_ = 1.0f;
      awb_b_gain_ = 1.0f;
   }

   wb_config wb_cfg = config_.wb;
   wb_cfg.r_gain = config_.wb.is_enable ? config_.wb.r_gain : 1.0f;
   wb_cfg.b_gain = config_.wb.is_enable ? config_.wb.b_gain : 1.0f;
   if (config_.awb.is_enable) {
      wb_cfg.r_gain *= awb_r_gain_;
      wb_cfg.b_gain *= awb_b_gain_;
   }
   wb_.process(demosaic_out_.data(), wb_out_.data(), width_, height_, wb_cfg, bd);

   ccm_.process(wb_out_.data(), ccm_out_.data(), width_, height_, config_.ccm, bd);
   gc_.process(ccm_out_.data(), gc_out_.data(), width_, height_, config_.gc, bd);

   // AEC (Auto Exposure Control)
   if (config_.aec.is_enable) {
      aec_.process(gc_out_.data(), width_, height_, config_.aec, bd);
      aec_feedback_ = config_.aec.ae_feedback;
   } else {
      aec_feedback_ = 0;
   }

   if (config_.csc.is_enable) {
      csc_.process(gc_out_.data(), csc_out_.data(), width_, height_, config_.csc, bd);
   } else {
      const std::int32_t shift_to_8bit = static_cast<std::int32_t>(bd) - 8;
      for (std::size_t p = 0; p < rgb_pixels; ++p) {
         const std::int32_t raw = static_cast<std::int32_t>(gc_out_[p]);
         std::int32_t v;
         if (shift_to_8bit > 0) {
            v = (raw + (1 << (shift_to_8bit - 1))) >> shift_to_8bit;
         } else if (shift_to_8bit < 0) {
            v = raw << (-shift_to_8bit);
         } else {
            v = raw;
         }
         if (v < 0)
            v = 0;
         if (v > 255)
            v = 255;
         csc_out_[p] = static_cast<std::uint8_t>(v);
      }
   }

   cse_.process(csc_out_.data(), cse_out_.data(), width_, height_, config_.cse);
   sharpen_.process(cse_out_.data(), sharpen_out_.data(), width_, height_, config_.sharpen);
   twodnr_.process(sharpen_out_.data(), twodnr_out_.data(), width_, height_, config_.twodnr);

   if (config_.scale.is_enable) {
      scale_.process(twodnr_out_.data(), scale_out_.data(), width_, height_, config_.scale.out_width,
                     config_.scale.out_height, config_.scale);
      if (config_.yuv420.is_enable) {
         yuv420_.process(scale_out_.data(), final_out_.data(), config_.scale.out_width,
                         config_.scale.out_height, config_.yuv420);
      } else {
         final_out_ = scale_out_;
      }
   } else if (config_.yuv420.is_enable) {
      yuv420_.process(twodnr_out_.data(), final_out_.data(), width_, height_, config_.yuv420);
   } else {
      final_out_ = twodnr_out_;
   }

   yuv_out = final_out_;
   processing_done_ = true;
}
