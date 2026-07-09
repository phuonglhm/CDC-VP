#include "isp_pipeline.h"
#include "dg.h"

#include <cstring>
#include <iostream>

namespace {

constexpr std::uint32_t CTRL_ENABLE = 1u << 0;
constexpr std::uint32_t CTRL_START = 1u << 1;
constexpr std::uint32_t CTRL_SOFT_RESET = 1u << 2;
constexpr std::uint32_t CTRL_IRQ_EN = 1u << 3;
constexpr std::uint32_t CTRL_STICKY_MASK = CTRL_ENABLE | CTRL_IRQ_EN;

constexpr std::uint32_t IRQ_DONE = 1u << 0;
constexpr std::uint32_t IRQ_ERROR = 1u << 1;
constexpr std::uint32_t IRQ_MASK = IRQ_DONE | IRQ_ERROR;

std::uint32_t float_to_reg(float value) {
   std::uint32_t raw = 0;
   std::memcpy(&raw, &value, sizeof(raw));
   return raw;
}

float reg_to_float(std::uint32_t value) {
   float raw = 0.0f;
   std::memcpy(&raw, &value, sizeof(raw));
   return raw;
}

cfa_types cfa_from_reg(std::uint32_t value) {
   switch (value & 0x3u) {
   case 0:
      return cfa_types::RGGB;
   case 1:
      return cfa_types::GRBG;
   case 2:
      return cfa_types::BGGR;
   case 3:
      return cfa_types::GBRG;
   default:
      return cfa_types::RGGB;
   }
}

} // namespace

using namespace cdc::components;

isp_pipeline::isp_pipeline()
    : config_{}
    , ctrl_(0)
    , irq_enable_(0)
    , irq_status_(0)
    , processing_done_(false)
    , processing_busy_(false)
    , processing_error_(false)
    , src_addr_(0)
    , dst_addr_(0)
    , scratch_addr_(0)
    , src_size_bytes_(0)
    , dst_size_bytes_(0)
    , weights_addr_(0)
    , param_addr_(0)
    , stride_(0)
    , format_(0)
    , op_mode_(0)
    , gc_gamma_(0)
    , gc_lut_addr_(0)
    , gc_lut_data_(0)
    , lsc_lut_addr_(0)
    , csc_enable_(0)
    , width_(0)
    , height_(0)
    , input_bit_depth_(12)
    , input_bayer_pattern_(cfa_types::RGGB)
    , working_bit_depth_(12)
    , awb_r_gain_(1.0f)
    , awb_b_gain_(1.0f) {
   lsc_sram_.resize(8192);
   reset_registers();
}

isp_pipeline::~isp_pipeline() {
}

void isp_pipeline::set_dimensions(std::uint32_t width, std::uint32_t height) {
   width_ = width;
   height_ = height;
   config_.scale.in_width = static_cast<std::uint16_t>(width_);
   config_.scale.in_height = static_cast<std::uint16_t>(height_);

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

void isp_pipeline::set_input_format(std::uint8_t bit_depth, cfa_types bayer_pattern) {
   input_bit_depth_ = bit_depth;
   input_bayer_pattern_ = bayer_pattern;
   // All processing blocks operate at 12-bit working precision
   working_bit_depth_ = 12;
   config_.ccm.bit_depth = working_bit_depth_;
   config_.gc.bit_depth = working_bit_depth_;
   config_.csc.bit_depth = working_bit_depth_;
}

void isp_pipeline::reset_registers() {
   config_ = isp_config{};

   ctrl_ = 0;
   irq_enable_ = 0;
   irq_status_ = 0;
   processing_done_ = false;
   processing_busy_ = false;
   processing_error_ = false;

   src_addr_ = 0;
   dst_addr_ = 0;
   scratch_addr_ = 0;
   src_size_bytes_ = 0;
   dst_size_bytes_ = 0;
   weights_addr_ = 0;
   param_addr_ = 0;
   stride_ = 0;
   format_ = 0;
   op_mode_ = 0;
   gc_gamma_ = 0;
   gc_lut_addr_ = 0;
   gc_lut_data_ = 0;
   lsc_lut_addr_ = 0;
   csc_enable_ = 0;

   awb_r_gain_ = 1.0f;
   awb_b_gain_ = 1.0f;

   set_dimensions(0, 0);
   set_input_format(12, cfa_types::RGGB);

   config_.blc.r_sat = 4095;
   config_.blc.gr_sat = 4095;
   config_.blc.gb_sat = 4095;
   config_.blc.b_sat = 4095;

   config_.bnr.filter_window = 3;
   config_.bnr.r_std_dev_s = 1.5f;
   config_.bnr.r_std_dev_r = 0.1f;
   config_.bnr.g_std_dev_s = 1.5f;
   config_.bnr.g_std_dev_r = 0.1f;
   config_.bnr.b_std_dev_s = 1.5f;
   config_.bnr.b_std_dev_r = 0.1f;

   config_.dg.current_gain = 1;
   config_.aec.center_illuminance = 128;

   // default lsc lut
   config_.lsc.grid_width = 16;
   config_.lsc.grid_height = 12;
   std::uint32_t nx = 16 + 1;
   std::uint32_t ny = 12 + 1;
   float cx = (nx - 1) / 2.0f;
   float cy = (ny - 1) / 2.0f;
   std::uint32_t addr = 0;
   for (int ch = 0; ch < 4; ++ch) {
      for (std::uint32_t y = 0; y < ny; ++y) {
         for (std::uint32_t x = 0; x < nx; ++x) {
            float dx = (static_cast<float>(x) - cx) / cx;
            float dy = (static_cast<float>(y) - cy) / cy;
            float dist2 = dx * dx + dy * dy;
            lsc_sram_[addr++] = 1.0f + 0.5f * dist2;
         }
      }
   }
}

std::uint32_t isp_pipeline::status_reg() const {
   return (processing_busy_ ? STATUS_BUSY : 0) | (processing_done_ ? STATUS_DONE : 0) |
          (processing_error_ ? STATUS_ERROR : 0) | (!processing_busy_ ? STATUS_IDLE : 0);
}

std::uint32_t isp_pipeline::bayer_pattern_reg() const {
   switch (input_bayer_pattern_) {
   case cfa_types::RGGB:
      return 0;
   case cfa_types::GRBG:
      return 1;
   case cfa_types::BGGR:
      return 2;
   case cfa_types::GBRG:
      return 3;
   }
   return 0;
}

bool isp_pipeline::irq_level() const {
   return (ctrl_ & CTRL_IRQ_EN) != 0u && (irq_status_ & irq_enable_) != 0u;
}

bool isp_pipeline::is_enabled() const {
   return (ctrl_ & CTRL_ENABLE) != 0u;
}

bool isp_pipeline::has_valid_dimensions() const {
   return width_ != 0 && height_ != 0;
}

void isp_pipeline::mark_processing_started() {
   processing_busy_ = true;
   processing_done_ = false;
   processing_error_ = false;
   irq_status_ &= ~(IRQ_DONE | IRQ_ERROR);
}

void isp_pipeline::mark_processing_done() {
   processing_busy_ = false;
   processing_done_ = true;
   processing_error_ = false;
   irq_status_ |= IRQ_DONE;
}

void isp_pipeline::mark_processing_error() {
   processing_busy_ = false;
   processing_error_ = true;
   irq_status_ |= IRQ_ERROR;
}

std::uint32_t isp_pipeline::read_reg(std::uint32_t offset) const {
   switch (offset) {
   case REG_CTRL:
      return ctrl_;
   case REG_STATUS:
      return status_reg();
   case REG_IRQ_ENABLE:
      return irq_enable_;
   case REG_IRQ_STATUS:
      return irq_status_;

   case REG_SRC_ADDR:
      return src_addr_;
   case REG_DST_ADDR:
      return dst_addr_;
   case REG_SCRATCH_ADDR:
      return scratch_addr_;
   case REG_SRC_SIZE_BYTES:
      return src_size_bytes_;
   case REG_DST_SIZE_BYTES:
      return dst_size_bytes_;
   case REG_WEIGHTS_ADDR:
      return weights_addr_;
   case REG_PARAM_ADDR:
      return param_addr_;

   case REG_WIDTH:
      return width_;
   case REG_HEIGHT:
      return height_;
   case REG_STRIDE:
      return stride_;
   case REG_FORMAT:
      return format_;
   case REG_OP_MODE:
      return op_mode_;
   case REG_BIT_DEPTH:
      return input_bit_depth_;
   case REG_BAYER_PATTERN:
      return bayer_pattern_reg();

   case REG_BLC_ENABLE:
      return config_.blc.is_enable ? 1u : 0u;
   case REG_BLC_LINEAR:
      return config_.blc.is_linear ? 1u : 0u;
   case REG_BLC_R_OFFSET:
      return config_.blc.r_offset;
   case REG_BLC_GR_OFFSET:
      return config_.blc.gr_offset;
   case REG_BLC_GB_OFFSET:
      return config_.blc.gb_offset;
   case REG_BLC_B_OFFSET:
      return config_.blc.b_offset;
   case REG_BLC_R_SAT:
      return config_.blc.r_sat;
   case REG_BLC_GR_SAT:
      return config_.blc.gr_sat;
   case REG_BLC_GB_SAT:
      return config_.blc.gb_sat;
   case REG_BLC_B_SAT:
      return config_.blc.b_sat;

   case REG_DPC_ENABLE:
      return config_.dpc.is_enable ? 1u : 0u;
   case REG_DPC_THRESH:
      return config_.dpc.dp_threshold;

   case REG_LSC_ENABLE:
      return config_.lsc.is_enable ? 1u : 0u;
   case REG_LSC_GRID_W:
      return config_.lsc.grid_width;
   case REG_LSC_GRID_H:
      return config_.lsc.grid_height;
   case REG_LSC_LUT_ADDR:
      return lsc_lut_addr_;
   case REG_LSC_LUT_DATA:
      if (lsc_lut_addr_ < lsc_sram_.size()) {
         std::uint32_t val;
         std::memcpy(&val, &lsc_sram_[lsc_lut_addr_], sizeof(float));
         return val;
      }
      return 0;

   case REG_DG_ENABLE:
      return config_.dg.is_enable ? 1u : 0u;
   case REG_DG_GAIN:
      return config_.dg.current_gain;
   case REG_DG_AUTO:
      return config_.dg.is_auto ? 1u : 0u;

   case REG_BNR_ENABLE:
      return config_.bnr.is_enable ? 1u : 0u;
   case REG_BNR_WINDOW:
      return config_.bnr.filter_window;
   case REG_BNR_R_STD_DEV_S:
      return float_to_reg(config_.bnr.r_std_dev_s);
   case REG_BNR_R_STD_DEV_R:
      return float_to_reg(config_.bnr.r_std_dev_r);
   case REG_BNR_G_STD_DEV_S:
      return float_to_reg(config_.bnr.g_std_dev_s);
   case REG_BNR_G_STD_DEV_R:
      return float_to_reg(config_.bnr.g_std_dev_r);
   case REG_BNR_B_STD_DEV_S:
      return float_to_reg(config_.bnr.b_std_dev_s);
   case REG_BNR_B_STD_DEV_R:
      return float_to_reg(config_.bnr.b_std_dev_r);

   case REG_DEMOSAIC_ENABLE:
      return config_.demosaic.is_enable ? 1u : 0u;

   case REG_AWB_ENABLE:
      return config_.awb.is_enable ? 1u : 0u;
   case REG_AWB_ALGORITHM:
      return config_.awb.algorithm;
   case REG_AWB_R_GAIN:
      return float_to_reg(awb_r_gain_);
   case REG_AWB_B_GAIN:
      return float_to_reg(awb_b_gain_);
   case REG_AWB_UNDER_PCT:
      return float_to_reg(config_.awb.underexposed_percentage);
   case REG_AWB_OVER_PCT:
      return float_to_reg(config_.awb.overexposed_percentage);
   case REG_AWB_PERCENT:
      return float_to_reg(config_.awb.percentage);

   case REG_WB_ENABLE:
      return config_.wb.is_enable ? 1u : 0u;
   case REG_WB_R_GAIN:
      return float_to_reg(config_.wb.r_gain);
   case REG_WB_B_GAIN:
      return float_to_reg(config_.wb.b_gain);

   case REG_CCM_ENABLE:
      return config_.ccm.is_enable ? 1u : 0u;
   case REG_CCM_MATRIX00:
      return float_to_reg(config_.ccm.corrected_red[0]);
   case REG_CCM_MATRIX01:
      return float_to_reg(config_.ccm.corrected_red[1]);
   case REG_CCM_MATRIX02:
      return float_to_reg(config_.ccm.corrected_red[2]);
   case REG_CCM_MATRIX10:
      return float_to_reg(config_.ccm.corrected_green[0]);
   case REG_CCM_MATRIX11:
      return float_to_reg(config_.ccm.corrected_green[1]);
   case REG_CCM_MATRIX12:
      return float_to_reg(config_.ccm.corrected_green[2]);
   case REG_CCM_MATRIX20:
      return float_to_reg(config_.ccm.corrected_blue[0]);
   case REG_CCM_MATRIX21:
      return float_to_reg(config_.ccm.corrected_blue[1]);
   case REG_CCM_MATRIX22:
      return float_to_reg(config_.ccm.corrected_blue[2]);

   case REG_GC_ENABLE:
      return config_.gc.is_enable ? 1u : 0u;
   case REG_GC_GAMMA:
      return gc_gamma_;
   case REG_GC_LUT_ADDR:
      return gc_lut_addr_;
   case REG_GC_LUT_DATA:
      return gc_lut_data_;

   case REG_AEC_ENABLE:
      return config_.aec.is_enable ? 1u : 0u;
   case REG_AEC_FEEDBACK:
      return static_cast<std::uint32_t>(config_.aec.ae_feedback);
   case REG_AEC_CENTER_ILLUM:
      return config_.aec.center_illuminance;
   case REG_AEC_SKEWNESS:
      return float_to_reg(config_.aec.histogram_skewness);

   case REG_CSC_ENABLE:
      return csc_enable_;
   case REG_CSC_STANDARD:
      return config_.csc.conv_standard;

   case REG_CSE_ENABLE:
      return config_.cse.is_enable ? 1u : 0u;
   case REG_CSE_SAT_GAIN:
      return float_to_reg(config_.cse.saturation_gain);

   case REG_SHARPEN_ENABLE:
      return config_.sharpen.is_enable ? 1u : 0u;
   case REG_SHARPEN_SIGMA:
      return config_.sharpen.sharpen_sigma;
   case REG_SHARPEN_STRENGTH:
      return config_.sharpen.sharpen_strength;

   case REG_2DNR_ENABLE:
      return config_.twodnr.is_enable ? 1u : 0u;
   case REG_2DNR_WINDOW:
      return config_.twodnr.window_size;
   case REG_2DNR_PATCH:
      return config_.twodnr.patch_size;
   case REG_2DNR_WTS:
      return config_.twodnr.wts;

   case REG_SCALE_ENABLE:
      return config_.scale.is_enable ? 1u : 0u;
   case REG_SCALE_OUT_W:
      return config_.scale.out_width;
   case REG_SCALE_OUT_H:
      return config_.scale.out_height;

   case REG_YUV420_ENABLE:
      return config_.yuv420.is_enable ? 1u : 0u;

   default:
      return 0;
   }
}

bool isp_pipeline::write_reg(std::uint32_t offset, std::uint32_t value) {
   switch (offset) {
   case REG_CTRL:
      if (value & CTRL_SOFT_RESET) {
         reset_registers();
         return false;
      }
      {
         std::uint32_t next_ctrl = value & CTRL_STICKY_MASK;
         if (value & CTRL_START) {
            next_ctrl |= ctrl_ & CTRL_STICKY_MASK;
         }
         ctrl_ = next_ctrl;
      }
      if (value & CTRL_START) {
         return true;
      }
      return false;

   case REG_STATUS:
      if (value & STATUS_DONE) {
         processing_done_ = false;
         irq_status_ &= ~IRQ_DONE;
      }
      if (value & STATUS_ERROR) {
         processing_error_ = false;
         irq_status_ &= ~IRQ_ERROR;
      }
      return false;

   case REG_IRQ_ENABLE:
      irq_enable_ = value & IRQ_MASK;
      return false;

   case REG_IRQ_STATUS:
      irq_status_ &= ~(value & IRQ_MASK);
      return false;

   case REG_SRC_ADDR:
      src_addr_ = value;
      return false;
   case REG_DST_ADDR:
      dst_addr_ = value;
      return false;
   case REG_SCRATCH_ADDR:
      scratch_addr_ = value;
      return false;
   case REG_SRC_SIZE_BYTES:
      src_size_bytes_ = value;
      return false;
   case REG_DST_SIZE_BYTES:
      dst_size_bytes_ = value;
      return false;
   case REG_WEIGHTS_ADDR:
      weights_addr_ = value;
      return false;
   case REG_PARAM_ADDR:
      param_addr_ = value;
      return false;

   case REG_WIDTH:
      set_dimensions(value & 0xFFFFu, height_);
      return false;
   case REG_HEIGHT:
      set_dimensions(width_, value & 0xFFFFu);
      return false;
   case REG_STRIDE:
      stride_ = value;
      return false;
   case REG_FORMAT:
      format_ = value;
      return false;
   case REG_OP_MODE:
      op_mode_ = value;
      return false;
   case REG_BIT_DEPTH:
      set_input_format(static_cast<std::uint8_t>(value & 0xFFu), input_bayer_pattern_);
      return false;
   case REG_BAYER_PATTERN:
      set_input_format(input_bit_depth_, cfa_from_reg(value));
      return false;

   case REG_BLC_ENABLE:
      config_.blc.is_enable = (value & 0x1u) != 0u;
      return false;
   case REG_BLC_LINEAR:
      config_.blc.is_linear = (value & 0x1u) != 0u;
      return false;
   case REG_BLC_R_OFFSET:
      config_.blc.r_offset = value & 0xFFFFu;
      return false;
   case REG_BLC_GR_OFFSET:
      config_.blc.gr_offset = value & 0xFFFFu;
      return false;
   case REG_BLC_GB_OFFSET:
      config_.blc.gb_offset = value & 0xFFFFu;
      return false;
   case REG_BLC_B_OFFSET:
      config_.blc.b_offset = value & 0xFFFFu;
      return false;
   case REG_BLC_R_SAT:
      config_.blc.r_sat = value & 0xFFFFu;
      return false;
   case REG_BLC_GR_SAT:
      config_.blc.gr_sat = value & 0xFFFFu;
      return false;
   case REG_BLC_GB_SAT:
      config_.blc.gb_sat = value & 0xFFFFu;
      return false;
   case REG_BLC_B_SAT:
      config_.blc.b_sat = value & 0xFFFFu;
      return false;

   case REG_DPC_ENABLE:
      config_.dpc.is_enable = (value & 0x1u) != 0u;
      return false;
   case REG_DPC_THRESH:
      config_.dpc.dp_threshold = value & 0xFFFFu;
      return false;

   case REG_LSC_ENABLE:
      config_.lsc.is_enable = (value & 0x1u) != 0u;
      return false;
   case REG_LSC_GRID_W:
      config_.lsc.grid_width = value & 0xFFFFu;
      return false;
   case REG_LSC_GRID_H:
      config_.lsc.grid_height = value & 0xFFFFu;
      return false;
   case REG_LSC_LUT_ADDR:
      lsc_lut_addr_ = value;
      return false;
   case REG_LSC_LUT_DATA:
      if (lsc_lut_addr_ < lsc_sram_.size()) {
         float f_val;
         std::memcpy(&f_val, &value, sizeof(float));
         lsc_sram_[lsc_lut_addr_] = f_val;
         lsc_lut_addr_++;
      }
      return false;

   case REG_DG_ENABLE:
      config_.dg.is_enable = (value & 0x1u) != 0u;
      return false;
   case REG_DG_GAIN:
      config_.dg.current_gain = value & 0xFFFFu;
      return false;
   case REG_DG_AUTO:
      config_.dg.is_auto = (value & 0x1u) != 0u;
      return false;

   case REG_BNR_ENABLE:
      config_.bnr.is_enable = (value & 0x1u) != 0u;
      return false;
   case REG_BNR_WINDOW:
      config_.bnr.filter_window = value & 0xFFu;
      return false;
   case REG_BNR_R_STD_DEV_S:
      config_.bnr.r_std_dev_s = reg_to_float(value);
      return false;
   case REG_BNR_R_STD_DEV_R:
      config_.bnr.r_std_dev_r = reg_to_float(value);
      return false;
   case REG_BNR_G_STD_DEV_S:
      config_.bnr.g_std_dev_s = reg_to_float(value);
      return false;
   case REG_BNR_G_STD_DEV_R:
      config_.bnr.g_std_dev_r = reg_to_float(value);
      return false;
   case REG_BNR_B_STD_DEV_S:
      config_.bnr.b_std_dev_s = reg_to_float(value);
      return false;
   case REG_BNR_B_STD_DEV_R:
      config_.bnr.b_std_dev_r = reg_to_float(value);
      return false;

   case REG_DEMOSAIC_ENABLE:
      config_.demosaic.is_enable = (value & 0x1u) != 0u;
      return false;

   case REG_AWB_ENABLE:
      config_.awb.is_enable = (value & 0x1u) != 0u;
      return false;
   case REG_AWB_ALGORITHM:
      config_.awb.algorithm = value & 0xFFu;
      return false;
   case REG_AWB_R_GAIN:
      awb_r_gain_ = reg_to_float(value);
      config_.awb.r_gain_out = awb_r_gain_;
      return false;
   case REG_AWB_B_GAIN:
      awb_b_gain_ = reg_to_float(value);
      config_.awb.b_gain_out = awb_b_gain_;
      return false;
   case REG_AWB_UNDER_PCT:
      config_.awb.underexposed_percentage = reg_to_float(value);
      return false;
   case REG_AWB_OVER_PCT:
      config_.awb.overexposed_percentage = reg_to_float(value);
      return false;
   case REG_AWB_PERCENT:
      config_.awb.percentage = reg_to_float(value);
      return false;

   case REG_WB_ENABLE:
      config_.wb.is_enable = (value & 0x1u) != 0u;
      return false;
   case REG_WB_R_GAIN:
      config_.wb.r_gain = reg_to_float(value);
      return false;
   case REG_WB_B_GAIN:
      config_.wb.b_gain = reg_to_float(value);
      return false;

   case REG_CCM_ENABLE:
      config_.ccm.is_enable = (value & 0x1u) != 0u;
      return false;
   case REG_CCM_MATRIX00:
      config_.ccm.corrected_red[0] = reg_to_float(value);
      return false;
   case REG_CCM_MATRIX01:
      config_.ccm.corrected_red[1] = reg_to_float(value);
      return false;
   case REG_CCM_MATRIX02:
      config_.ccm.corrected_red[2] = reg_to_float(value);
      return false;
   case REG_CCM_MATRIX10:
      config_.ccm.corrected_green[0] = reg_to_float(value);
      return false;
   case REG_CCM_MATRIX11:
      config_.ccm.corrected_green[1] = reg_to_float(value);
      return false;
   case REG_CCM_MATRIX12:
      config_.ccm.corrected_green[2] = reg_to_float(value);
      return false;
   case REG_CCM_MATRIX20:
      config_.ccm.corrected_blue[0] = reg_to_float(value);
      return false;
   case REG_CCM_MATRIX21:
      config_.ccm.corrected_blue[1] = reg_to_float(value);
      return false;
   case REG_CCM_MATRIX22:
      config_.ccm.corrected_blue[2] = reg_to_float(value);
      return false;

   case REG_GC_ENABLE:
      config_.gc.is_enable = (value & 0x1u) != 0u;
      return false;
   case REG_GC_GAMMA:
      gc_gamma_ = value;
      return false;
   case REG_GC_LUT_ADDR:
      gc_lut_addr_ = value;
      return false;
   case REG_GC_LUT_DATA:
      gc_lut_data_ = value;
      return false;

   case REG_AEC_ENABLE:
      config_.aec.is_enable = (value & 0x1u) != 0u;
      return false;
   case REG_AEC_FEEDBACK:
      config_.aec.ae_feedback = static_cast<std::int32_t>(value);
      return false;
   case REG_AEC_CENTER_ILLUM:
      config_.aec.center_illuminance = static_cast<std::uint8_t>(value & 0xFFu);
      return false;
   case REG_AEC_SKEWNESS:
      config_.aec.histogram_skewness = reg_to_float(value);
      return false;

   case REG_CSC_ENABLE:
      csc_enable_ = value & 0x1u;
      return false;
   case REG_CSC_STANDARD:
      config_.csc.conv_standard = value & 0xFFu;
      return false;

   case REG_CSE_ENABLE:
      config_.cse.is_enable = (value & 0x1u) != 0u;
      return false;
   case REG_CSE_SAT_GAIN:
      config_.cse.saturation_gain = reg_to_float(value);
      return false;

   case REG_SHARPEN_ENABLE:
      config_.sharpen.is_enable = (value & 0x1u) != 0u;
      return false;
   case REG_SHARPEN_SIGMA:
      config_.sharpen.sharpen_sigma = value & 0xFFu;
      return false;
   case REG_SHARPEN_STRENGTH:
      config_.sharpen.sharpen_strength = value & 0xFFFFu;
      return false;

   case REG_2DNR_ENABLE:
      config_.twodnr.is_enable = (value & 0x1u) != 0u;
      return false;
   case REG_2DNR_WINDOW:
      config_.twodnr.window_size = value & 0xFFu;
      return false;
   case REG_2DNR_PATCH:
      config_.twodnr.patch_size = value & 0xFFu;
      return false;
   case REG_2DNR_WTS:
      config_.twodnr.wts = value & 0xFFFFu;
      return false;

   case REG_SCALE_ENABLE:
      config_.scale.is_enable = (value & 0x1u) != 0u;
      return false;
   case REG_SCALE_OUT_W:
      config_.scale.out_width = value & 0xFFFFu;
      return false;
   case REG_SCALE_OUT_H:
      config_.scale.out_height = value & 0xFFFFu;
      return false;

   case REG_YUV420_ENABLE:
      config_.yuv420.is_enable = (value & 0x1u) != 0u;
      return false;

   default:
      return false;
   }
}

void isp_pipeline::run(const std::uint16_t *raw_in, std::vector<std::uint8_t> &yuv_out) {
   if (width_ == 0 || height_ == 0 || raw_in == nullptr) {
      yuv_out.clear();
      return;
   }

   const isp_config cfg = config_;

   const std::size_t raw_pixels = static_cast<std::size_t>(width_) * height_;
   const std::size_t rgb_pixels = raw_pixels * 3u;
   const std::size_t yuv_pixels = raw_pixels * 3u;

   // Normalize input to 12-bit working range (0..4095).
   // For 12-bit input: no change. For 16-bit input: shift right by 4.
   const std::uint32_t work_max = (1u << working_bit_depth_) - 1u;
   const std::uint32_t src_max = (1u << input_bit_depth_) - 1u;
   if (src_max == 0)
      return;
   // Pre-compute scale = work_max / src_max as Q16 fixed point for speed
   // but keep it readable using 64-bit math
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

   blc_.process(raw_buf_.data(), blc_out_.data(), width_, height_, cfg.blc, cfa, bd);
   dpc_.process(blc_out_.data(), dpc_out_.data(), width_, height_, cfg.dpc);
   lsc_.process(dpc_out_.data(), lsc_out_.data(), width_, height_, cfg.lsc, lsc_sram_.data(), cfa, bd);
   dg_.process(lsc_out_.data(), dg_out_.data(), width_, height_, cfg.dg, bd);
   bnr_.process(dg_out_.data(), bnr_out_.data(), width_, height_, cfg.bnr, cfa, bd);

   demosaic_.process(bnr_out_.data(), demosaic_out_.data(), width_, height_, cfg.demosaic, cfa, bd);

   awb_config awb_cfg = cfg.awb;
   if (cfg.awb.is_enable) {
      awb_.process(demosaic_out_.data(), width_, height_, awb_cfg, bd);
      awb_r_gain_ = awb_cfg.r_gain_out;
      awb_b_gain_ = awb_cfg.b_gain_out;
   } else {
      awb_r_gain_ = 1.0f;
      awb_b_gain_ = 1.0f;
   }

   wb_config wb_cfg = cfg.wb;
   wb_cfg.r_gain = cfg.wb.is_enable ? cfg.wb.r_gain : 1.0f;
   wb_cfg.b_gain = cfg.wb.is_enable ? cfg.wb.b_gain : 1.0f;
   if (cfg.awb.is_enable) {
      wb_cfg.r_gain *= awb_r_gain_;
      wb_cfg.b_gain *= awb_b_gain_;
   }
   wb_.process(demosaic_out_.data(), wb_out_.data(), width_, height_, wb_cfg);

   ccm_.process(wb_out_.data(), ccm_out_.data(), width_, height_, cfg.ccm);
   gc_.process(ccm_out_.data(), gc_out_.data(), width_, height_, cfg.gc);

   aec_config aec_cfg = cfg.aec;
   aec_.process(gc_out_.data(), width_, height_, aec_cfg, bd);

   csc_.process(gc_out_.data(), csc_out_.data(), width_, height_, cfg.csc);
   cse_.process(csc_out_.data(), cse_out_.data(), width_, height_, cfg.cse);
   sharpen_.process(cse_out_.data(), sharpen_out_.data(), width_, height_, cfg.sharpen);
   twodnr_.process(sharpen_out_.data(), twodnr_out_.data(), width_, height_, cfg.twodnr);

   if (cfg.scale.is_enable) {
      scale_.process(twodnr_out_.data(), scale_out_.data(), width_, height_, cfg.scale.out_width,
                     cfg.scale.out_height, cfg.scale);
      if (cfg.yuv420.is_enable) {
         yuv420_.process(scale_out_.data(), final_out_.data(), cfg.scale.out_width, cfg.scale.out_height,
                         cfg.yuv420);
      } else {
         final_out_ = scale_out_;
      }
   } else if (cfg.yuv420.is_enable) {
      yuv420_.process(twodnr_out_.data(), final_out_.data(), width_, height_, cfg.yuv420);
   } else {
      final_out_ = twodnr_out_;
   }

   yuv_out = final_out_;

   // Update feedback/dynamic state registers at end of frame
   if (cfg.awb.is_enable) {
      config_.awb.r_gain_out = awb_r_gain_;
      config_.awb.b_gain_out = awb_b_gain_;
   }

   if (aec_cfg.is_enable) {
      config_.aec.ae_feedback = aec_cfg.ae_feedback;
      if (cfg.dg.is_auto) {
         if (aec_cfg.ae_feedback < 0 && config_.dg.current_gain < kGainArraySize) {
            config_.dg.current_gain++;
         } else if (aec_cfg.ae_feedback > 0 && config_.dg.current_gain > 0) {
            config_.dg.current_gain--;
         }
      }
   }

   std::cout << "current gain: " << config_.dg.current_gain << std::endl;
   std::cout << "aec feedback: " << config_.aec.ae_feedback << std::endl;
}
