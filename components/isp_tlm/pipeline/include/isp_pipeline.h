#pragma once

#include <cstdint>
#include <vector>

#include "blc.h"
#include "dpc.h"
#include "lsc.h"
#include "dg.h"
#include "bnr.h"
#include "demosaic.h"
#include "awb.h"
#include "aec.h"
#include "wb.h"
#include "ccm.h"
#include "gc.h"
#include "rgb_to_yuv.h"
#include "cse.h"
#include "sharpen.h"
#include "2dnr.h"
#include "scale.h"
#include "yuv420.h"
#include "isp_types.h"
#include "isp_regmap.h"

struct isp_config {
   blc_config blc;
   dpc_config dpc;
   lsc_config lsc;
   dg_config dg;
   bnr_config bnr;
   demosaic_config demosaic;
   awb_config awb;
   aec_config aec;
   wb_config wb;
   ccm_config ccm;
   gc_config gc;
   csc_config csc;
   cse_config cse;
   sharpen_config sharpen;
   twodnr_config twodnr;
   scale_config scale;
   yuv420_config yuv420;
};

class isp_pipeline {
public:
   static constexpr float OPTICAL_CENTER_X = 1292.15f;
   static constexpr float OPTICAL_CENTER_Y = 638.02f;

   isp_pipeline();
   ~isp_pipeline();

   void run(const std::uint16_t *raw_in, std::vector<std::uint8_t> &yuv_out);

   void reset_registers();
   std::uint32_t read_reg(std::uint32_t offset) const;
   bool write_reg(std::uint32_t offset, std::uint32_t value);

   bool irq_level() const;
   bool is_enabled() const;
   bool has_valid_dimensions() const;
   void mark_processing_started();
   void mark_processing_done();
   void mark_processing_error();

   const isp_config &config() const {
      return config_;
   }

   // Direct accessors used by the SystemC testbench to prime the AWB
   // from the reference pipeline's WB output (a 12-bit RGB frame that
   // already matches what the SystemC streaming AWB would see after
   // demosaic + WB).
   const std::vector<std::uint16_t>& wb_output() const { return wb_out_; }

   // Final R/B AWB gains the reference pipeline used.
   float awb_r_gain() const { return awb_r_gain_; }
   float awb_b_gain() const { return awb_b_gain_; }

private:
   std::uint32_t status_reg() const;
   std::uint32_t bayer_pattern_reg() const;

   void set_dimensions(std::uint32_t width, std::uint32_t height);
   void set_input_format(std::uint8_t bit_depth, cfa_types bayer_pattern);

   isp_config config_;

   std::uint32_t ctrl_;
   std::uint32_t irq_enable_;
   std::uint32_t irq_status_;
   bool processing_done_;
   bool processing_busy_;
   bool processing_error_;

   std::uint32_t src_addr_;
   std::uint32_t dst_addr_;
   std::uint32_t scratch_addr_;
   std::uint32_t src_size_bytes_;
   std::uint32_t dst_size_bytes_;
   std::uint32_t weights_addr_;
   std::uint32_t param_addr_;
   std::uint32_t stride_;
   std::uint32_t format_;
   std::uint32_t op_mode_;
   std::uint32_t gc_gamma_;
   std::uint32_t gc_lut_addr_;
   std::uint32_t gc_lut_data_;
   std::uint32_t lsc_lut_addr_;
   std::uint32_t csc_enable_;

   std::uint32_t width_;
   std::uint32_t height_;
   std::uint8_t input_bit_depth_;
   cfa_types input_bayer_pattern_;
   std::uint8_t working_bit_depth_; // all blocks operate at 12-bit
   std::vector<float> lsc_sram_;
   float awb_r_gain_;
   float awb_b_gain_;

   blc_block blc_;
   dpc_block dpc_;
   lsc_block lsc_;
   dg_block dg_;
   bnr_block bnr_;
   demosaic_block demosaic_;
   awb_block awb_;
   aec_block aec_;
   wb_block wb_;
   ccm_block ccm_;
   gc_block gc_;
   csc_block csc_;
   cse_block cse_;
   sharpen_block sharpen_;
   twodnr_block twodnr_;
   scale_block scale_;
   yuv420_block yuv420_;

   std::vector<std::uint16_t> raw_buf_;
   std::vector<std::uint16_t> blc_out_;
   std::vector<std::uint16_t> dpc_out_;
   std::vector<std::uint16_t> lsc_out_;
   std::vector<std::uint16_t> dg_out_;
   std::vector<std::uint16_t> bnr_out_;
   std::vector<std::uint16_t> demosaic_out_;
   std::vector<std::uint16_t> wb_out_;
   std::vector<std::uint16_t> ccm_out_;
   std::vector<std::uint16_t> gc_out_;
   std::vector<std::uint8_t> csc_out_;
   std::vector<std::uint8_t> cse_out_;
   std::vector<std::uint8_t> sharpen_out_;
   std::vector<std::uint8_t> twodnr_out_;
   std::vector<std::uint8_t> scale_out_;
   std::vector<std::uint8_t> final_out_;
};
