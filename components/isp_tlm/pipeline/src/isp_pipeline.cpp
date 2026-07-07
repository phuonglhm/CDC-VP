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
    , processing_done_(false) {
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
   case REG_ISP_ENABLE:       return 0;                    // write-only
   case REG_STATUS:           return processing_done_ ? STATUS_DONE : 0;
   case REG_WIDTH:            return width_;
   case REG_HEIGHT:           return height_;
   case REG_BIT_DEPTH:        return bit_depth_;
   case REG_BAYER_PATTERN:     return bayer_pattern_;

   case REG_BLC_ENABLE:   return static_cast<std::uint32_t>(config_.blc.is_enable);
   case REG_BLC_R_OFFSET:  return config_.blc.r_offset;
   case REG_BLC_GR_OFFSET: return config_.blc.gr_offset;
   case REG_BLC_GB_OFFSET: return config_.blc.gb_offset;
   case REG_BLC_B_OFFSET:  return config_.blc.b_offset;

   case REG_WB_ENABLE: return static_cast<std::uint32_t>(config_.wb.is_enable);
   case REG_WB_R_GAIN: return awb_r_gain_raw_;
   case REG_WB_B_GAIN: return awb_b_gain_raw_;

   case REG_CCM_ENABLE:       return static_cast<std::uint32_t>(config_.ccm.is_enable);
   case REG_CCM_MATRIX00: return ccm_matrix_raw_[0];
   case REG_CCM_MATRIX01: return ccm_matrix_raw_[1];
   case REG_CCM_MATRIX02: return ccm_matrix_raw_[2];
   case REG_CCM_MATRIX10: return ccm_matrix_raw_[3];
   case REG_CCM_MATRIX11: return ccm_matrix_raw_[4];
   case REG_CCM_MATRIX12: return ccm_matrix_raw_[5];
   case REG_CCM_MATRIX20: return ccm_matrix_raw_[6];
   case REG_CCM_MATRIX21: return ccm_matrix_raw_[7];
   case REG_CCM_MATRIX22: return ccm_matrix_raw_[8];

   case REG_GC_ENABLE:  return static_cast<std::uint32_t>(config_.gc.is_enable);

   case REG_CSC_ENABLE:    return static_cast<std::uint32_t>(config_.csc.is_enable);
   case REG_CSC_STANDARD: return static_cast<std::uint32_t>(config_.csc.conv_standard);

   case REG_CSE_ENABLE:      return static_cast<std::uint32_t>(config_.cse.is_enable);
   case REG_CSE_SAT_GAIN: {
      std::uint32_t raw;
      std::memcpy(&raw, &config_.cse.saturation_gain, sizeof(float));
      return raw;
   }

   case REG_SHARPEN_ENABLE:    return static_cast<std::uint32_t>(config_.sharpen.is_enable);
   case REG_SHARPEN_SIGMA:    return static_cast<std::uint32_t>(config_.sharpen.sharpen_sigma);
   case REG_SHARPEN_STRENGTH: return config_.sharpen.sharpen_strength;

   case REG_2DNR_ENABLE:   return static_cast<std::uint32_t>(config_.twodnr.is_enable);
   case REG_2DNR_WINDOW:   return static_cast<std::uint32_t>(config_.twodnr.window_size);
   case REG_2DNR_PATCH:   return static_cast<std::uint32_t>(config_.twodnr.patch_size);
   case REG_2DNR_WTS:     return config_.twodnr.wts;

   case REG_SCALE_ENABLE: return static_cast<std::uint32_t>(config_.scale.is_enable);
   case REG_SCALE_OUT_W:  return config_.scale.out_width;
   case REG_SCALE_OUT_H:  return config_.scale.out_height;

   case REG_YUV420_ENABLE: return static_cast<std::uint32_t>(config_.yuv420.is_enable);

   case REG_DPC_ENABLE:      return static_cast<std::uint32_t>(config_.dpc.is_enable);
   case REG_DPC_THRESH:      return config_.dpc.dp_threshold;
   case REG_LSC_ENABLE:     return static_cast<std::uint32_t>(config_.lsc.is_enable);
   case REG_LSC_GRID_W:     return config_.lsc.grid_width;
   case REG_LSC_GRID_H:     return config_.lsc.grid_height;
   case REG_DG_ENABLE:      return static_cast<std::uint32_t>(config_.dg.is_enable);
   case REG_DG_GAIN:        return static_cast<std::uint32_t>(config_.dg.current_gain);
   case REG_BNR_ENABLE:     return static_cast<std::uint32_t>(config_.bnr.is_enable);
   case REG_BNR_WINDOW:     return static_cast<std::uint32_t>(config_.bnr.filter_window);
   case REG_DEMOSAIC_ENABLE: return static_cast<std::uint32_t>(config_.demosaic.is_enable);

   case REG_AWB_ENABLE:    return static_cast<std::uint32_t>(config_.awb.is_enable);
   case REG_AWB_ALGORITHM:  return static_cast<std::uint32_t>(config_.awb.algorithm);
   case REG_AWB_R_GAIN:     return awb_r_gain_raw_;
   case REG_AWB_B_GAIN:     return awb_b_gain_raw_;

   case REG_RAW_FRAME_ADDR: return raw_frame_addr_;
   case REG_YUV_FRAME_ADDR: return yuv_frame_addr_;

   default: return 0;
   }
}

void isp_pipeline::write_reg(std::uint32_t offset, std::uint32_t value) {
   switch (offset) {
   case REG_ISP_ENABLE:
      break;

   case REG_WIDTH:
      width_ = value & 0xFFFF;
      break;

   case REG_HEIGHT:
      height_ = value & 0xFFFF;
      break;

   case REG_BIT_DEPTH:
      bit_depth_ = value & 0xFF;
      break;

   case REG_BAYER_PATTERN:
      bayer_pattern_ = value & 0xFF;
      break;

   case REG_BLC_ENABLE:
      config_.blc.is_enable = (value & 0x1) != 0;
      break;

   case REG_BLC_R_OFFSET:
      config_.blc.r_offset = value;
      break;

   case REG_BLC_GR_OFFSET:
      config_.blc.gr_offset = value;
      break;

   case REG_BLC_GB_OFFSET:
      config_.blc.gb_offset = value;
      break;

   case REG_BLC_B_OFFSET:
      config_.blc.b_offset = value;
      break;

   case REG_WB_ENABLE:
      config_.wb.is_enable = (value & 0x1) != 0;
      break;

   case REG_WB_R_GAIN:
      awb_r_gain_raw_ = value;
      std::memcpy(&config_.wb.r_gain, &value, sizeof(float));
      break;

   case REG_WB_B_GAIN:
      awb_b_gain_raw_ = value;
      std::memcpy(&config_.wb.b_gain, &value, sizeof(float));
      break;

   case REG_CCM_ENABLE:
      config_.ccm.is_enable = (value & 0x1) != 0;
      break;

   case REG_CCM_MATRIX00: ccm_matrix_raw_[0] = value; break;
   case REG_CCM_MATRIX01: ccm_matrix_raw_[1] = value; break;
   case REG_CCM_MATRIX02: ccm_matrix_raw_[2] = value; break;
   case REG_CCM_MATRIX10: ccm_matrix_raw_[3] = value; break;
   case REG_CCM_MATRIX11: ccm_matrix_raw_[4] = value; break;
   case REG_CCM_MATRIX12: ccm_matrix_raw_[5] = value; break;
   case REG_CCM_MATRIX20: ccm_matrix_raw_[6] = value; break;
   case REG_CCM_MATRIX21: ccm_matrix_raw_[7] = value; break;
   case REG_CCM_MATRIX22: ccm_matrix_raw_[8] = value; break;

   case REG_GC_ENABLE:
      config_.gc.is_enable = (value & 0x1) != 0;
      break;

   case REG_CSC_ENABLE:
      config_.csc.is_enable = (value & 0x1) != 0;
      break;

   case REG_CSC_STANDARD:
      config_.csc.conv_standard = static_cast<std::uint8_t>(value);
      break;

   case REG_CSE_ENABLE:
      config_.cse.is_enable = (value & 0x1) != 0;
      break;

   case REG_CSE_SAT_GAIN:
      config_.cse.saturation_gain = 0.0f;
      std::memcpy(&config_.cse.saturation_gain, &value, sizeof(float));
      break;

   case REG_SHARPEN_ENABLE:
      config_.sharpen.is_enable = (value & 0x1) != 0;
      break;

   case REG_SHARPEN_SIGMA:
      config_.sharpen.sharpen_sigma = static_cast<std::uint8_t>(value & 0xFF);
      break;

   case REG_SHARPEN_STRENGTH:
      config_.sharpen.sharpen_strength = value & 0xFFFF;
      break;

   case REG_2DNR_ENABLE:
      config_.twodnr.is_enable = (value & 0x1) != 0;
      break;

   case REG_2DNR_WINDOW:
      config_.twodnr.window_size = static_cast<std::uint8_t>(value & 0xFF);
      break;

   case REG_2DNR_PATCH:
      config_.twodnr.patch_size = static_cast<std::uint8_t>(value & 0xFF);
      break;

   case REG_2DNR_WTS:
      config_.twodnr.wts = value & 0xFFFF;
      break;

   case REG_SCALE_ENABLE:
      config_.scale.is_enable = (value & 0x1) != 0;
      break;

   case REG_SCALE_OUT_W:
      config_.scale.out_width = value;
      break;

   case REG_SCALE_OUT_H:
      config_.scale.out_height = value;
      break;

   case REG_YUV420_ENABLE:
      config_.yuv420.is_enable = (value & 0x1) != 0;
      break;

   case REG_DPC_ENABLE:
      config_.dpc.is_enable = (value & 0x1) != 0;
      break;

   case REG_DPC_THRESH:
      config_.dpc.dp_threshold = value;
      break;

   case REG_LSC_ENABLE:
      config_.lsc.is_enable = (value & 0x1) != 0;
      break;

   case REG_LSC_GRID_W:
      config_.lsc.grid_width = value;
      break;

   case REG_LSC_GRID_H:
      config_.lsc.grid_height = value;
      break;

   case REG_DG_ENABLE:
      config_.dg.is_enable = (value & 0x1) != 0;
      break;

   case REG_DG_GAIN:
      config_.dg.current_gain = static_cast<std::uint8_t>(value & 0xFF);
      break;

   case REG_BNR_ENABLE:
      config_.bnr.is_enable = (value & 0x1) != 0;
      break;

   case REG_BNR_WINDOW:
      config_.bnr.filter_window = static_cast<std::uint8_t>(value & 0xFF);
      break;

   case REG_DEMOSAIC_ENABLE:
      config_.demosaic.is_enable = (value & 0x1) != 0;
      break;

   case REG_AWB_ENABLE:
      config_.awb.is_enable = (value & 0x1) != 0;
      break;

   case REG_AWB_ALGORITHM:
      config_.awb.algorithm = static_cast<std::uint8_t>(value & 0xFF);
      break;

   case REG_AWB_R_GAIN:
      awb_r_gain_raw_ = value;
      std::memcpy(&config_.awb.r_gain_out, &value, sizeof(float));
      break;

   case REG_AWB_B_GAIN:
      awb_b_gain_raw_ = value;
      std::memcpy(&config_.awb.b_gain_out, &value, sizeof(float));
      break;

   case REG_RAW_FRAME_ADDR:
      raw_frame_addr_ = value;
      break;

   case REG_YUV_FRAME_ADDR:
      yuv_frame_addr_ = value;
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
   const std::uint32_t src_max  = (1u << input_bit_depth_) - 1u;
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
   const cfa_types    cfa = input_bayer_pattern_;

   blc_.process(raw_buf_.data(), blc_out_.data(), width_, height_, config_.blc, cfa, bd);
   dpc_.process(blc_out_.data(), dpc_out_.data(), width_, height_, config_.dpc);
   lsc_.process(dpc_out_.data(), lsc_out_.data(), width_, height_, config_.lsc, lsc_mem_ptr_, cfa, bd);
   dg_.process(lsc_out_.data(), dg_out_.data(), width_, height_, config_.dg, bd);
   bnr_.process(dg_out_.data(), bnr_out_.data(), width_, height_, config_.bnr, cfa, bd);

   demosaic_.process(bnr_out_.data(), demosaic_out_.data(), width_, height_, config_.demosaic, cfa, bd);

   awb_config awb_cfg = config_.awb;
   if (config_.awb.is_enable) {
      awb_.process(demosaic_out_.data(), width_, height_, awb_cfg, bd);
      awb_r_gain_ = awb_cfg.r_gain_out;
      awb_b_gain_ = awb_cfg.b_gain_out;
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
         if (v < 0) v = 0;
         if (v > 255) v = 255;
         csc_out_[p] = static_cast<std::uint8_t>(v);
      }
   }

   cse_.process(csc_out_.data(), cse_out_.data(), width_, height_, config_.cse);
   sharpen_.process(cse_out_.data(), sharpen_out_.data(), width_, height_, config_.sharpen);
   twodnr_.process(sharpen_out_.data(), twodnr_out_.data(), width_, height_, config_.twodnr);

   if (config_.scale.is_enable) {
      scale_.process(twodnr_out_.data(), scale_out_.data(), width_, height_,
                     config_.scale.out_width, config_.scale.out_height, config_.scale);
      if (config_.yuv420.is_enable) {
         yuv420_.process(scale_out_.data(), final_out_.data(),
                        config_.scale.out_width, config_.scale.out_height, config_.yuv420);
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
