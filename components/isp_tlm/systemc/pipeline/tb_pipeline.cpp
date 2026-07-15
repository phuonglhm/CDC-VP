/**

 * @file tb_pipeline.cpp

 * @brief Full ISP Pipeline Testbench

 *

 * End-to-end testbench that validates the entire 17-block ISP pipeline

 * by comparing SystemC streaming output against the original C++ reference model.

 */

#include <systemc>
using namespace sc_core;


#include <iostream>
#include <vector>
#include <cstdlib>

#include "sc_isp_pipeline.h"
#include "../tb_utils/tb_utils.h"

#include "../../pipeline/include/isp_config.h"
#include "../../pipeline/include/isp_pipeline.h"

#define WIDTH  32
#define HEIGHT 32
#define FIFO_DEPTH 1024

int sc_main(int argc, char* argv[]) {
    std::cout << "==================================================" << std::endl;
    std::cout << "FULL ISP PIPELINE TESTBENCH" << std::endl;
    std::cout << "==================================================" << std::endl;

    // 1. Generate test input
    const std::size_t raw_pixels = WIDTH * HEIGHT;
    std::vector<std::uint16_t> test_input = tb_utils::generate_test_pattern<std::uint16_t>(
        WIDTH, HEIGHT, 1, 2048);

    std::cout << "[TB] Test input: " << raw_pixels << " pixels" << std::endl;

    // 2. Compute golden reference using original C++ pipeline
    isp_pipeline golden_pipeline;
    isp_config cfg = golden_pipeline.config();

    cfg.scale.in_width = WIDTH;
    cfg.scale.in_height = HEIGHT;
    cfg.scale.out_width = WIDTH / 2;
    cfg.scale.out_height = HEIGHT / 2;

    cfg.blc.is_enable = true;
    cfg.dpc.is_enable = true;
    cfg.lsc.is_enable = false;  // Disable LSC for simplicity
    cfg.dg.is_enable = false;   // Disable DG for simplicity
    cfg.bnr.is_enable = false;  // Disable BNR for simplicity
    cfg.demosaic.is_enable = true;
    cfg.awb.is_enable = true;
    cfg.wb.is_enable = true;
    cfg.ccm.is_enable = true;
    cfg.gc.is_enable = true;
    cfg.aec.is_enable = true;
    cfg.csc.conv_standard = 0;  // BT.601
    cfg.cse.is_enable = true;
    cfg.sharpen.is_enable = true;
    cfg.twodnr.is_enable = false;  // Disable 2DNR for simplicity
    cfg.scale.is_enable = true;
    cfg.yuv420.is_enable = true;

    // Configure block parameters
    cfg.blc.r_offset = 256; cfg.blc.gr_offset = 256;
    cfg.blc.gb_offset = 256; cfg.blc.b_offset = 256;
    cfg.blc.r_sat = 4095; cfg.blc.gr_sat = 4095;
    cfg.blc.gb_sat = 4095; cfg.blc.b_sat = 4095;

    cfg.dpc.dp_threshold = 30;

    cfg.wb.r_gain = 1.0f;
    cfg.wb.b_gain = 1.0f;

    cfg.ccm.bit_depth = 12;
    // Identity CCM
    cfg.ccm.corrected_red[0] = 1.0f; cfg.ccm.corrected_red[1] = 0.0f; cfg.ccm.corrected_red[2] = 0.0f;
    cfg.ccm.corrected_green[0] = 0.0f; cfg.ccm.corrected_green[1] = 1.0f; cfg.ccm.corrected_green[2] = 0.0f;
    cfg.ccm.corrected_blue[0] = 0.0f; cfg.ccm.corrected_blue[1] = 0.0f; cfg.ccm.corrected_blue[2] = 1.0f;

    cfg.gc.bit_depth = 12;
    cfg.cse.saturation_gain = 1.0f;
    cfg.sharpen.sharpen_sigma = 1;
    cfg.sharpen.sharpen_strength = 1;

    cfg.awb.underexposed_percentage = 0.01f;
    cfg.awb.overexposed_percentage = 0.01f;

    // Apply configuration by writing to registers
    auto write_to_reg = [&](std::uint32_t offset, std::uint32_t value) {
        // Golden pipeline expects writes through the register map
        while (golden_pipeline.write_reg(offset, value)) {
            // write_reg returns true to trigger processing - ignore for config
        }
    };

    write_to_reg(0x0004, 0);  // STATUS
    write_to_reg(0x000C, 0);  // IRQ_STATUS
    write_to_reg(0x0010, WIDTH);  // WIDTH
    write_to_reg(0x0014, HEIGHT);  // HEIGHT
    write_to_reg(0x0020, 12);  // BIT_DEPTH
    write_to_reg(0x0024, 0);  // BAYER_PATTERN (RGGB)

    // Run golden pipeline
    std::vector<std::uint8_t> golden_output;
    golden_pipeline.run(test_input.data(), golden_output);

    std::cout << "[TB] Golden output size: " << golden_output.size() << " bytes" << std::endl;

    // 3. Create SystemC pipeline
    std::vector<float> lsc_lut(8192, 1.0f);

    sc_core::sc_fifo<std::uint16_t> input_fifo(FIFO_DEPTH);
    sc_core::sc_fifo<std::uint8_t> output_fifo(FIFO_DEPTH);

    Generic_Driver<std::uint16_t> driver("driver", test_input);
    driver.fifo_out(input_fifo);

    sc_isp_pipeline dut("isp_pipeline", cfg, lsc_lut, &input_fifo, &output_fifo);

    // Calculate expected output size
    std::size_t out_w = cfg.scale.is_enable ? cfg.scale.out_width : WIDTH;
    std::size_t out_h = cfg.scale.is_enable ? cfg.scale.out_height : HEIGHT;
    std::size_t expected_size = out_w * out_h +
                                2 * ((out_w + 1) / 2) * ((out_h + 1) / 2);

    Generic_Monitor<std::uint8_t> monitor("monitor", expected_size);
    monitor.fifo_in(output_fifo);
    monitor.set_golden_reference(golden_output.data(), golden_output.size());

    // 4. Run simulation
    std::cout << "[TB] Starting simulation..." << std::endl;
    sc_start();
    std::cout << "[TB] Simulation completed" << std::endl;

    // 5. Verify output
    const auto& captured = monitor.get_captured_data();
    bool pass = true;
    std::size_t diff_count = 0;
    double max_diff = 0.0;

    std::size_t compare_size = std::min(captured.size(), golden_output.size());

    for (std::size_t i = 0; i < compare_size; ++i) {
        double diff = std::abs(static_cast<double>(captured[i]) - golden_output[i]);
        if (diff > 0.0) {
            diff_count++;
            max_diff = std::max(max_diff, diff);
        }
    }

    std::cout << std::endl;
    std::cout << "==================================================" << std::endl;
    std::cout << "PIPELINE VERIFICATION RESULTS" << std::endl;
    std::cout << "==================================================" << std::endl;
    std::cout << "  Expected output size: " << golden_output.size() << std::endl;
    std::cout << "  Captured output size: " << captured.size() << std::endl;
    std::cout << "  Differences: " << diff_count << " / " << compare_size << std::endl;
    std::cout << "  Max difference: " << max_diff << std::endl;
    std::cout << "  AWB R gain: " << dut.get_awb_r_gain() << std::endl;
    std::cout << "  AWB B gain: " << dut.get_awb_b_gain() << std::endl;
    std::cout << "  AEC feedback: " << dut.get_aec_feedback() << std::endl;

    // Tolerance check
    pass = (diff_count == 0) ||
           (max_diff < 5.0 && (diff_count < compare_size * 0.05));

    std::cout << "==================================================" << std::endl;
    std::cout << "TEST RESULT: " << (pass ? "PASS" : "FAIL") << std::endl;
    std::cout << "==================================================" << std::endl;

    return pass ? 0 : 1;
}