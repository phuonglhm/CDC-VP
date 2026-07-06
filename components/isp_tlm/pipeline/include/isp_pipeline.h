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

struct isp_config {
   blc_config blc;
   dpc_config dpc;
   lsc_config lsc;
   dg_config dg;
   bnr_config bnr;
   demosaic_config demosaic;
   awb_config awb;
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
   isp_pipeline();
   ~isp_pipeline();

   void set_dimensions(std::uint32_t width, std::uint32_t height);
   void set_lsc_mem(const float *lsc_mem);
   void set_input_format(std::uint8_t bit_depth, cfa_types bayer_pattern);

   void run(const std::uint16_t *raw_in, std::vector<std::uint8_t> &yuv_out, const isp_config &cfg);

   float get_awb_r_gain() const {
      return awb_r_gain_;
   }

   float get_awb_b_gain() const {
      return awb_b_gain_;
   }

private:
   std::uint32_t width_;
   std::uint32_t height_;
   std::uint8_t input_bit_depth_;
   cfa_types input_bayer_pattern_;
   std::uint8_t working_bit_depth_; // all blocks operate at 12-bit
   const float *lsc_mem_ptr_;
   float awb_r_gain_;
   float awb_b_gain_;

   blc_block blc_;
   dpc_block dpc_;
   lsc_block lsc_;
   dg_block dg_;
   bnr_block bnr_;
   demosaic_block demosaic_;
   awb_block awb_;
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
