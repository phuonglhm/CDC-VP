/**

 * @file tb_awb.cpp

 * @brief Testbench for Auto White Balance (AWB) SystemC Module

 */

#include <systemc>
using namespace sc_core;


#include <iostream>
#include <vector>

#include "sc_awb.h"
#include "../../tb_utils/tb_utils.h"

#define WIDTH  32
#define HEIGHT 32
#define FIFO_DEPTH (WIDTH * 4)

int sc_main(int argc, char* argv[]) {
    std::cout << "==================================================" << std::endl;
    std::cout << "Testbench: Auto White Balance (AWB)" << std::endl;
    std::cout << "==================================================" << std::endl;

    const std::size_t rgb_pixels = WIDTH * HEIGHT;

    // Generate RGB test pattern with known color cast
    std::vector<std::uint16_t> test_input(rgb_pixels * 3);
    for (std::size_t i = 0; i < rgb_pixels; ++i) {
        test_input[i * 3 + 0] = 2000;  // R high
        test_input[i * 3 + 1] = 1500;  // G medium
        test_input[i * 3 + 2] = 1000;  // B low
    }

    // Golden reference
    awb_block golden_block;
    awb_config cfg;
    cfg.is_enable = true;
    cfg.underexposed_percentage = 0.01f;
    cfg.overexposed_percentage = 0.01f;

    float golden_r_gain, golden_b_gain;
    {
        awb_config temp_cfg = cfg;
        golden_block.process(test_input.data(), WIDTH, HEIGHT, temp_cfg, 12);
        golden_r_gain = temp_cfg.r_gain_out;
        golden_b_gain = temp_cfg.b_gain_out;
    }

    // SystemC
    sc_core::sc_fifo<std::uint16_t> input_fifo(FIFO_DEPTH);
    sc_core::sc_fifo<std::uint16_t> output_fifo(FIFO_DEPTH);

    Generic_Driver<std::uint16_t> driver("driver", test_input);
    driver.fifo_out(input_fifo);

    sc_awb dut("awb_dut", cfg, WIDTH, HEIGHT, 12);
    dut.fifo_in(input_fifo);
    dut.fifo_out(output_fifo);

    Generic_Monitor<std::uint16_t> monitor("monitor", rgb_pixels * 3);
    monitor.fifo_in(output_fifo);

    std::cout << "[TB] Starting simulation..." << std::endl;
    sc_start();
    std::cout << "[TB] Simulation completed" << std::endl;

    std::cout << "Computed gains: R=" << dut.get_r_gain() << " B=" << dut.get_b_gain() << std::endl;
    std::cout << "Golden gains:   R=" << golden_r_gain << " B=" << golden_b_gain << std::endl;

    bool pass = true;
    if (std::abs(dut.get_r_gain() - golden_r_gain) > 0.01f) pass = false;
    if (std::abs(dut.get_b_gain() - golden_b_gain) > 0.01f) pass = false;

    std::cout << "TEST RESULT: " << (pass ? "PASS" : "FAIL") << std::endl;
    return pass ? 0 : 1;
}
