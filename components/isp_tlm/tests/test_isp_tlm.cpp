#include <cstdint>
#include <iostream>
#include <vector>
#include <cmath>
#include <cstring>

#include <systemc>
#include <tlm>

// Include RAW domain blocks
#include "blocks/blc.h"
#include "blocks/dpc.h"
#include "blocks/lsc.h"
#include "blocks/dg.h"
#include "blocks/bnr.h"
#include "blocks/demosaic.h"
#include "blocks/awb.h"

// Include RGB domain blocks
#include "blocks/wb.h"
#include "blocks/ccm.h"
#include "blocks/gc.h"
#include "blocks/rgb_to_yuv.h"

// Include YUV domain blocks
#include "blocks/cse.h"
#include "blocks/sharpen.h"
#include "blocks/2dnr.h"
#include "blocks/scale.h"
#include "blocks/yuv420.h"

// Include SystemC TLM wrapper headers
#include "isp_tlm.h"
#include "tlm_probe.h"

// Macro helper for simple assertions in tests
#define ASSERT_EQUAL(val, expected)                                                                          \
   do {                                                                                                      \
      if ((val) != (expected)) {                                                                             \
         std::cerr << "Assertion failed: " << #val << " (" << (val) << ") != " << #expected << " ("          \
                   << (expected) << ") at line " << __LINE__ << std::endl;                                   \
         std::exit(1);                                                                                       \
      }                                                                                                      \
   } while (0)

#define ASSERT_NEAR(val, expected, tol)                                                                      \
   do {                                                                                                      \
      if (std::abs((val) - (expected)) > (tol)) {                                                            \
         std::cerr << "Assertion failed: " << #val << " (" << (val) << ") != " << #expected << " ("          \
                   << (expected) << ") within tolerance " << #tol << " at line " << __LINE__ << std::endl;   \
         std::exit(1);                                                                                       \
      }                                                                                                      \
   } while (0)

// ============================================================================
// 1. RAW Domain Block Unit Tests
// ============================================================================

void test_blc_block() {
   std::cout << "[Test] Running BLC Block Unit Test..." << std::endl;
   blc_block block;

   std::vector<uint16_t> in = {100, 200, 150, 250};
   std::vector<uint16_t> out(4, 0);

   blc_config cfg;
   cfg.is_enable = true;
   cfg.is_linear = false;
   cfg.r_offset = 10;
   cfg.gr_offset = 20;
   cfg.gb_offset = 15;
   cfg.b_offset = 30;

   block.process(in.data(), out.data(), 2, 2, cfg, cfa_types::RGGB, 12);

   ASSERT_EQUAL(out[0], 90);
   ASSERT_EQUAL(out[1], 180);
   ASSERT_EQUAL(out[2], 135);
   ASSERT_EQUAL(out[3], 220);

   cfg.is_enable = false;
   in = {100, 200, 150, 250};
   block.process(in.data(), out.data(), 2, 2, cfg, cfa_types::RGGB, 12);
   ASSERT_EQUAL(out[0], 100);
   ASSERT_EQUAL(out[1], 200);

   std::cout << "[Test] BLC Block Unit Test Passed." << std::endl;
}

void test_dpc_block() {
   std::cout << "[Test] Running DPC Block Unit Test..." << std::endl;
   dpc_block block;

   uint32_t w = 5, h = 5;
   std::vector<uint16_t> in(w * h, 100);
   in[2 * w + 2] = 1000;
   std::vector<uint16_t> out(w * h, 0);

   dpc_config cfg;
   cfg.is_enable = true;
   cfg.dp_threshold = 200;

   block.process(in.data(), out.data(), w, h, cfg);
   ASSERT_EQUAL(out[2 * w + 2], 100);

   cfg.is_enable = false;
   block.process(in.data(), out.data(), w, h, cfg);
   ASSERT_EQUAL(out[2 * w + 2], 1000);

   std::cout << "[Test] DPC Block Unit Test Passed." << std::endl;
}

void test_lsc_block() {
   std::cout << "[Test] Running LSC Block Unit Test..." << std::endl;
   lsc_block block;

   uint32_t w = 4, h = 4;
   std::vector<uint16_t> in(w * h, 100);
   std::vector<uint16_t> out(w * h, 0);

   lsc_config cfg;
   cfg.is_enable = true;
   cfg.grid_width = 1;
   cfg.grid_height = 1;

   std::vector<float> lsc_mem(16, 1.0f);
   lsc_mem[0] = 2.0f;
   lsc_mem[1] = 2.0f;
   lsc_mem[2] = 2.0f;
   lsc_mem[3] = 2.0f;
   lsc_mem[4] = 1.5f;
   lsc_mem[5] = 1.5f;
   lsc_mem[6] = 1.5f;
   lsc_mem[7] = 1.5f;

   block.process(in.data(), out.data(), w, h, cfg, lsc_mem.data(), cfa_types::RGGB, 12);

   ASSERT_EQUAL(out[0 * w + 0], 200);
   ASSERT_EQUAL(out[0 * w + 1], 150);

   std::cout << "[Test] LSC Block Unit Test Passed." << std::endl;
}

void test_dg_block() {
   std::cout << "[Test] Running DG Block Unit Test..." << std::endl;
   dg_block block;

   uint32_t w = 2, h = 2;
   std::vector<uint16_t> in = {100, 100, 100, 100};
   std::vector<uint16_t> out(4, 0);

   dg_config cfg;
   cfg.is_enable = true;
   cfg.is_auto = false;
   cfg.current_gain = 2;
   cfg.ae_feedback = 0;

   block.process(in.data(), out.data(), w, h, cfg, 12);
   ASSERT_EQUAL(out[0], 400);

   in = {2000, 2000, 2000, 2000};
   block.process(in.data(), out.data(), w, h, cfg, 12);
   ASSERT_EQUAL(out[0], 4095);

   std::cout << "[Test] DG Block Unit Test Passed." << std::endl;
}

void test_bnr_block() {
   std::cout << "[Test] Running BNR Block Unit Test..." << std::endl;
   bnr_block block;

   uint32_t w = 4, h = 4;
   std::vector<uint16_t> in(w * h, 500);
   std::vector<uint16_t> out(w * h, 0);

   bnr_config cfg;
   cfg.is_enable = true;
   cfg.filter_window = 5;
   cfg.r_std_dev_s = 1.0f;
   cfg.r_std_dev_r = 0.1f;
   cfg.g_std_dev_s = 1.0f;
   cfg.g_std_dev_r = 0.1f;
   cfg.b_std_dev_s = 1.0f;
   cfg.b_std_dev_r = 0.1f;

   block.process(in.data(), out.data(), w, h, cfg, cfa_types::RGGB, 12);
   ASSERT_NEAR(static_cast<float>(out[0]), 500.0f, 5.0f);

   cfg.is_enable = false;
   block.process(in.data(), out.data(), w, h, cfg, cfa_types::RGGB, 12);
   ASSERT_EQUAL(out[0], 500);

   std::cout << "[Test] BNR Block Unit Test Passed." << std::endl;
}

void test_demosaic_block() {
   std::cout << "[Test] Running Demosaic Block Unit Test..." << std::endl;
   demosaic_block block;

   uint32_t w = 2, h = 2;
   std::vector<uint16_t> in = {100, 200, 150, 250};
   std::vector<uint16_t> out(12, 0);

   demosaic_config cfg;
   cfg.is_enable = false;

   block.process(in.data(), out.data(), w, h, cfg, cfa_types::RGGB, 12);

   ASSERT_EQUAL(out[0], 100);
   ASSERT_EQUAL(out[1], 100);
   ASSERT_EQUAL(out[2], 100);
   ASSERT_EQUAL(out[3], 200);
   ASSERT_EQUAL(out[4], 200);
   ASSERT_EQUAL(out[5], 200);

   std::cout << "[Test] Demosaic Block Unit Test Passed." << std::endl;
}

void test_awb_block_bypass() {
   std::cout << "[Test] Running AWB Block - Bypass Test..." << std::endl;
   awb_block block;

   std::vector<std::uint16_t> in = {100, 200, 150, 250, 100, 200, 150, 250};
   awb_config cfg;
   cfg.is_enable = false;

   block.process(in.data(), 4, 2, cfg, 12);

   ASSERT_EQUAL(cfg.r_gain_out, 1.0f);
   ASSERT_EQUAL(cfg.b_gain_out, 1.0f);

   std::cout << "[Test] AWB Block - Bypass Test Passed." << std::endl;
}

void test_awb_block_gray_world() {
   std::cout << "[Test] Running AWB Block - Gray World Test..." << std::endl;
   awb_block block;

   std::vector<std::uint16_t> in = {500, 500, 500, 500, 500, 500, 500, 500,
                                    500, 500, 500, 500, 500, 500, 500, 500};
   awb_config cfg;
   cfg.is_enable = true;

   block.process(in.data(), 4, 4, cfg, 12);

   ASSERT_NEAR(cfg.r_gain_out, 1.0f, 0.01f);
   ASSERT_NEAR(cfg.b_gain_out, 1.0f, 0.01f);

   std::cout << "[Test] AWB Block - Gray World Test Passed." << std::endl;
}

void test_awb_block_colored_image() {
   std::cout << "[Test] Running AWB Block - Colored Image Test..." << std::endl;
   awb_block block;

   std::vector<std::uint16_t> in(16, 500);

   for (std::size_t i = 0; i < 4; ++i) {
      for (std::size_t j = 0; j < 4; ++j) {
         in[i * 4 + j] = 1000;
      }
   }
   for (std::size_t i = 0; i < 2; ++i) {
      for (std::size_t j = 0; j < 2; ++j) {
         in[i * 4 + j] = 500;
      }
   }
   for (std::size_t i = 2; i < 4; ++i) {
      for (std::size_t j = 2; j < 4; ++j) {
         in[i * 4 + j] = 200;
      }
   }

   awb_config cfg;
   cfg.is_enable = true;

   block.process(in.data(), 4, 4, cfg, 12);

   std::cout << "  AWB computed R gain: " << cfg.r_gain_out << std::endl;
   std::cout << "  AWB computed B gain: " << cfg.b_gain_out << std::endl;

   ASSERT_EQUAL(cfg.r_gain_out > 0.0f, true);
   ASSERT_EQUAL(cfg.b_gain_out > 0.0f, true);

   std::cout << "[Test] AWB Block - Colored Image Test Passed." << std::endl;
}

// ============================================================================
// 2. RGB Domain Block Unit Tests
// ============================================================================

void test_wb_block_bypass() {
   std::cout << "[Test] Running WB Block - Bypass Test..." << std::endl;
   wb_block block;

   std::vector<std::uint16_t> in = {100, 200, 150, 100, 200, 150};
   std::vector<std::uint16_t> out(6, 0);

   wb_config cfg;
   cfg.is_enable = false;

   block.process(in.data(), out.data(), 2, 1, cfg);
   ASSERT_EQUAL(out[0], 100);
   ASSERT_EQUAL(out[1], 200);
   ASSERT_EQUAL(out[2], 150);

   std::cout << "[Test] WB Block - Bypass Test Passed." << std::endl;
}

void test_wb_block_hand_computable() {
   std::cout << "[Test] Running WB Block - Hand-Computable Test..." << std::endl;
   wb_block block;

   std::vector<std::uint16_t> in = {100, 200, 150, 100, 200, 150, 100, 200, 150, 100, 200, 150};
   std::vector<std::uint16_t> out(12, 0);

   wb_config cfg;
   cfg.is_enable = true;
   cfg.r_gain = 2.0f;
   cfg.b_gain = 0.5f;

   block.process(in.data(), out.data(), 2, 2, cfg);

   ASSERT_EQUAL(out[0], 200); // R: 100 * 2.0 = 200
   ASSERT_EQUAL(out[1], 200); // G: pass-through
   ASSERT_EQUAL(out[2], 75);  // B: 150 * 0.5 = 75
   ASSERT_EQUAL(out[3], 200); // R: 100 * 2.0 = 200
   ASSERT_EQUAL(out[4], 200); // G: pass-through
   ASSERT_EQUAL(out[5], 75);  // B: 150 * 0.5 = 75

   std::cout << "[Test] WB Block - Hand-Computable Test Passed." << std::endl;
}

void test_wb_block_overflow_clip() {
   std::cout << "[Test] Running WB Block - Overflow Clipping Test..." << std::endl;
   wb_block block;

   std::vector<std::uint16_t> in = {2000, 1000, 1000, 2000, 1000, 1000};
   std::vector<std::uint16_t> out(6, 0);

   wb_config cfg;
   cfg.is_enable = true;
   cfg.r_gain = 3.0f;
   cfg.b_gain = 0.5f;

   block.process(in.data(), out.data(), 2, 1, cfg);

   ASSERT_EQUAL(out[0], 4095); // 2000 * 3 = 6000, clipped to 4095
   ASSERT_EQUAL(out[1], 1000); // G: pass-through
   ASSERT_EQUAL(out[2], 500);  // B: 1000 * 0.5 = 500

   std::cout << "[Test] WB Block - Overflow Clipping Test Passed." << std::endl;
}

void test_ccm_block_bypass() {
   std::cout << "[Test] Running CCM Block - Bypass Test..." << std::endl;
   ccm_block block;

   std::vector<std::uint16_t> in = {100, 200, 150};
   std::vector<std::uint16_t> out(3, 0);

   ccm_config cfg;
   cfg.is_enable = false;

   block.process(in.data(), out.data(), 1, 1, cfg);
   ASSERT_EQUAL(out[0], 100);
   ASSERT_EQUAL(out[1], 200);
   ASSERT_EQUAL(out[2], 150);

   std::cout << "[Test] CCM Block - Bypass Test Passed." << std::endl;
}

void test_ccm_block_identity_matrix() {
   std::cout << "[Test] Running CCM Block - Identity Matrix Test..." << std::endl;
   ccm_block block;

   std::vector<std::uint16_t> in = {100, 200, 150};
   std::vector<std::uint16_t> out(3, 0);

   ccm_config cfg;
   cfg.is_enable = true;
   cfg.bit_depth = 12;

   block.process(in.data(), out.data(), 1, 1, cfg);
   ASSERT_EQUAL(out[0], 100);
   ASSERT_EQUAL(out[1], 200);
   ASSERT_EQUAL(out[2], 150);

   std::cout << "[Test] CCM Block - Identity Matrix Test Passed." << std::endl;
}

void test_ccm_block_clipping() {
   std::cout << "[Test] Running CCM Block - Clipping Test..." << std::endl;
   ccm_block block;

   std::vector<std::uint16_t> in = {4000, 4000, 4000};
   std::vector<std::uint16_t> out(3, 0);

   ccm_config cfg;
   cfg.is_enable = true;
   cfg.bit_depth = 12;
   cfg.corrected_red[0] = 2.0f;
   cfg.corrected_red[1] = 0.0f;
   cfg.corrected_red[2] = 0.0f;

   block.process(in.data(), out.data(), 1, 1, cfg);
   ASSERT_EQUAL(out[0], 4095);
   ASSERT_EQUAL(out[1], 4000);
   ASSERT_EQUAL(out[2], 4000);

   std::cout << "[Test] CCM Block - Clipping Test Passed." << std::endl;
}

void test_gc_block_bypass() {
   std::cout << "[Test] Running GC Block - Bypass Test..." << std::endl;
   gc_block block;

   std::vector<std::uint16_t> in = {10, 128, 255};
   std::vector<std::uint16_t> out(3, 0);

   gc_config cfg;
   cfg.is_enable = false;

   block.process(in.data(), out.data(), 1, 1, cfg);
   ASSERT_EQUAL(out[0], 10);
   ASSERT_EQUAL(out[1], 128);
   ASSERT_EQUAL(out[2], 255);

   std::cout << "[Test] GC Block - Bypass Test Passed." << std::endl;
}

void test_gc_block_identity_lut() {
   std::cout << "[Test] Running GC Block - Identity LUT Test..." << std::endl;
   gc_block block;

   std::vector<std::uint16_t> identity_lut(256);
   for (std::size_t i = 0; i < 256; ++i) {
      identity_lut[i] = static_cast<std::uint16_t>(i);
   }

   std::vector<std::uint16_t> in = {10, 128, 200};
   std::vector<std::uint16_t> out(3, 0);

   gc_config cfg;
   cfg.is_enable = true;
   cfg.bit_depth = 8;
   cfg.gamma_lut_8 = identity_lut;

   block.process(in.data(), out.data(), 1, 1, cfg);
   ASSERT_EQUAL(out[0], 10);
   ASSERT_EQUAL(out[1], 128);
   ASSERT_EQUAL(out[2], 200);

   std::cout << "[Test] GC Block - Identity LUT Test Passed." << std::endl;
}

void test_csc_block_gray_image() {
   std::cout << "[Test] Running CSC Block - Gray Image Test..." << std::endl;
   csc_block block;

   std::vector<std::uint16_t> in = {0, 0, 0, 2048, 2048, 2048};
   std::vector<std::uint8_t> out(6, 0);

   csc_config cfg;
   cfg.conv_standard = 1;

   block.process(in.data(), out.data(), 2, 1, cfg);

   ASSERT_EQUAL(out[0], 0);
   ASSERT_EQUAL(out[1], 128);
   ASSERT_EQUAL(out[2], 128);

   std::cout << "[Test] CSC Block - Gray Image Test Passed." << std::endl;
}

// ============================================================================
// 3. YUV Domain Block Unit Tests
// ============================================================================

void test_cse_block_bypass() {
   std::cout << "[Test] Running CSE Block - Bypass Test..." << std::endl;
   cse_block block;

   std::vector<std::uint8_t> in = {100, 100, 200};
   std::vector<std::uint8_t> out(3, 0);

   cse_config cfg;
   cfg.is_enable = false;

   block.process(in.data(), out.data(), 1, 1, cfg);
   ASSERT_EQUAL(out[0], 100);
   ASSERT_EQUAL(out[1], 100);
   ASSERT_EQUAL(out[2], 200);

   std::cout << "[Test] CSE Block - Bypass Test Passed." << std::endl;
}

void test_cse_block_saturation_gain() {
   std::cout << "[Test] Running CSE Block - Saturation Gain Test..." << std::endl;
   cse_block block;

   std::vector<std::uint8_t> in = {128, 128, 128};
   std::vector<std::uint8_t> out(3, 0);

   cse_config cfg;
   cfg.is_enable = true;
   cfg.saturation_gain = 2.0f;

   block.process(in.data(), out.data(), 1, 1, cfg);
   ASSERT_EQUAL(out[0], 128);
   ASSERT_EQUAL(out[1], 128);
   ASSERT_EQUAL(out[2], 128);

   std::cout << "[Test] CSE Block - Saturation Gain Test Passed." << std::endl;
}

void test_sharpen_block_bypass() {
   std::cout << "[Test] Running Sharpen Block - Bypass Test..." << std::endl;
   sharpen_block block;

   std::vector<std::uint8_t> in = {100, 100, 200};
   std::vector<std::uint8_t> out(3, 0);

   sharpen_config cfg;
   cfg.is_enable = false;

   block.process(in.data(), out.data(), 1, 1, cfg);
   ASSERT_EQUAL(out[0], 100);
   ASSERT_EQUAL(out[1], 100);
   ASSERT_EQUAL(out[2], 200);

   std::cout << "[Test] Sharpen Block - Bypass Test Passed." << std::endl;
}

void test_sharpen_block_flat_region() {
   std::cout << "[Test] Running Sharpen Block - Flat Region Test..." << std::endl;
   sharpen_block block;

   std::vector<std::uint8_t> in(27, 128);
   std::vector<std::uint8_t> out(27, 0);

   sharpen_config cfg;
   cfg.is_enable = true;
   cfg.sharpen_sigma = 1;
   cfg.sharpen_strength = 1;

   block.process(in.data(), out.data(), 3, 3, cfg);
   ASSERT_NEAR(static_cast<float>(out[0]), 128.0f, 1.0f);

   std::cout << "[Test] Sharpen Block - Flat Region Test Passed." << std::endl;
}

void test_twodnr_block_bypass() {
   std::cout << "[Test] Running 2DNR Block - Bypass Test..." << std::endl;
   twodnr_block block;

   std::vector<std::uint8_t> in = {100, 100, 200};
   std::vector<std::uint8_t> out(3, 0);

   twodnr_config cfg;
   cfg.is_enable = false;

   block.process(in.data(), out.data(), 1, 1, cfg);
   ASSERT_EQUAL(out[0], 100);
   ASSERT_EQUAL(out[1], 100);
   ASSERT_EQUAL(out[2], 200);

   std::cout << "[Test] 2DNR Block - Bypass Test Passed." << std::endl;
}

void test_twodnr_block_uniform_image() {
   std::cout << "[Test] Running 2DNR Block - Uniform Image Test..." << std::endl;
   twodnr_block block;

   std::vector<std::uint8_t> in(5 * 5 * 3, 128);
   std::vector<std::uint8_t> out(5 * 5 * 3, 0);

   twodnr_config cfg;
   cfg.is_enable = true;
   cfg.window_size = 5;
   cfg.patch_size = 3;
   cfg.wts = 10;

   block.process(in.data(), out.data(), 5, 5, cfg);
   ASSERT_NEAR(static_cast<float>(out[0]), 128.0f, 1.0f);

   std::cout << "[Test] 2DNR Block - Uniform Image Test Passed." << std::endl;
}

// ============================================================================
// 4. Pipeline Integration Test
// ============================================================================

void test_pipeline_end_to_end() {
   std::cout << "[Test] Running End-to-End Pipeline Test..." << std::endl;

   isp_pipeline pipeline;
   pipeline.set_dimensions(4, 4);

   std::vector<std::uint16_t> raw_in(16);
   for (std::size_t i = 0; i < 16; ++i) {
      raw_in[i] = 512;
   }

   std::vector<std::uint8_t> yuv_out;
   isp_config cfg;

   cfg.blc.is_enable = true;
   cfg.blc.r_offset = 10;
   cfg.blc.gr_offset = 10;
   cfg.blc.gb_offset = 10;
   cfg.blc.b_offset = 10;
   cfg.blc.r_sat = 4095;
   cfg.blc.gr_sat = 4095;
   cfg.blc.gb_sat = 4095;
   cfg.blc.b_sat = 4095;
   cfg.blc.is_linear = false;

   cfg.dpc.is_enable = false;
   cfg.lsc.is_enable = false;
   cfg.dg.is_enable = false;
   cfg.bnr.is_enable = false;
   cfg.demosaic.is_enable = true;
   cfg.wb.is_enable = true;
   cfg.wb.r_gain = 1.0f;
   cfg.wb.b_gain = 1.0f;
   cfg.ccm.is_enable = true;
   cfg.ccm.bit_depth = 12;
   cfg.gc.is_enable = false;
   cfg.csc.conv_standard = 1;
   cfg.cse.is_enable = false;
   cfg.sharpen.is_enable = false;
   cfg.twodnr.is_enable = false;
   cfg.scale.is_enable = false;
   cfg.yuv420.is_enable = false;

   pipeline.run(raw_in.data(), yuv_out, cfg);

   ASSERT_EQUAL(yuv_out.empty(), false);
   std::cout << "Pipeline output size: " << yuv_out.size() << " bytes" << std::endl;

   std::cout << "[Test] End-to-End Pipeline Test Passed." << std::endl;
}

// ============================================================================
// 5. SystemC TLM Smoke Test
// ============================================================================

class isp_smoke_tb : public sc_core::sc_module {
public:
   cdc::components::isp_tlm &isp;
   cdc::test::tlm_probe probe;

   SC_HAS_PROCESS(isp_smoke_tb);

   explicit isp_smoke_tb(sc_core::sc_module_name name, cdc::components::isp_tlm &isp_ref)
       : sc_module(name)
       , isp(isp_ref)
       , probe("probe") {
      probe.socket.bind(isp.socket);
      SC_THREAD(run_smoke_test);
   }

   void run_smoke_test() {
      using namespace cdc::components;
      using namespace cdc::test;

      std::cout << "\n--- SystemC TLM Smoke Test ---" << std::endl;

      wait(sc_core::SC_ZERO_TIME);

      std::uint32_t value = 1;
      probe.write(REG_ISP_ENABLE, &value, 4);

      wait(sc_core::SC_ZERO_TIME);

      std::uint32_t read_val = 0;
      probe.read(REG_ISP_ENABLE, &read_val, 4);
      if (read_val == 1) {
         std::cout << "  Register read/write: PASS" << std::endl;
      }

      std::cout << "SystemC TLM smoke test completed." << std::endl;
      sc_core::sc_stop();
   }
};

int sc_main(int argc, char *argv[]) {
   using namespace sc_core;
   using namespace cdc::components;

   std::cout << "========================================" << std::endl;
   std::cout << "   ISP TLM Pipeline Execution" << std::endl;
   std::cout << "========================================" << std::endl;

   isp_tlm isp("isp");
   sc_signal<bool> reset_n("reset_n");
   sc_signal<bool> irq("irq");
   isp.reset_n(reset_n);
   isp.irq_out(irq);

   cdc::test::tlm_probe probe("probe");
   probe.socket.bind(isp.socket);

   reset_n.write(true);
   sc_start(SC_ZERO_TIME);

   // 1. Configure ISP dimensions
   std::uint32_t width = 2592;
   std::uint32_t height = 1536;
   probe.write(REG_WIDTH, &width, 4);
   probe.write(REG_HEIGHT, &height, 4);

   // Set Bit depth (12-bit) and Bayer Pattern (RGGB = 0)
   std::uint32_t bayer_pattern = 0; // RGGB
   std::uint32_t bit_depth = 12;
   probe.write(REG_BAYER_PATTERN, &bayer_pattern, 4);
   probe.write(REG_BIT_DEPTH, &bit_depth, 4);

   // 2. Enable needed ISP blocks
   std::uint32_t enable = 1;
   probe.write(REG_ISP_ENABLE, &enable, 4);

   // Enable Demosaic
   probe.write(0x080, &enable, 4); // REG_DEMOSAIC_ENABLE (assumed 0x080 based on README example)

   // Enable AWB & WB
   probe.write(REG_AWB_ENABLE, &enable, 4);
   probe.write(REG_WB_ENABLE, &enable, 4);

   // Enable CCM (Color Correction)
   probe.write(REG_CCM_ENABLE, &enable, 4);
   float identity_ccm[9] = {1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0};
   for (int i = 0; i < 9; ++i) {
      probe.write(REG_CCM_MATRIX00 + i * 4, &identity_ccm[i], 4);
   }

   // Enable CSC (Color Space Conversion) to BT.709
   std::uint32_t csc_standard = 1;
   probe.write(REG_CSC_STANDARD, &csc_standard, 4);

   // Enable YUV420 Output
   probe.write(REG_YUV420_ENABLE, &enable, 4);

   // 3. Load RAW Image
   std::string input_path = "../input/ColorChecker_2592x1536_12bits_RGGB.raw";
   std::string output_path = "../input/output.yuv";

   FILE *fp_in = fopen(input_path.c_str(), "rb");
   if (!fp_in) {
      std::cerr << "Error: Could not open input file " << input_path << std::endl;
      return 1;
   }

   // Read directly into ISP's raw buffer
   size_t read_bytes = fread(isp.get_raw_buffer(), 1, width * height * 2, fp_in);
   fclose(fp_in);
   std::cout << "Read " << read_bytes << " bytes from " << input_path << std::endl;

   // 4. Trigger Processing
   std::cout << "Triggering ISP pipeline..." << std::endl;
   probe.write(REG_TRIGGER, &enable, 4);
   sc_start(1, SC_MS); // Run simulation forward so the module finishes

   // 5. Save YUV Output
   FILE *fp_out = fopen(output_path.c_str(), "wb");
   if (!fp_out) {
      std::cerr << "Error: Could not open output file " << output_path << std::endl;
      return 1;
   }

   // YUV420 size = width * height * 1.5
   size_t yuv_size = width * height * 3 / 2;
   fwrite(isp.get_yuv_buffer(), 1, yuv_size, fp_out);
   fclose(fp_out);

   std::cout << "Successfully saved YUV output to " << output_path << std::endl;

   return 0;
}
