/**

 * @file tb_aec.cpp

 * @brief Testbench for Auto Exposure Control (AEC) SystemC Module

 */

#include <systemc>
using namespace sc_core;


#include <iostream>
#include <vector>

#include "sc_aec.h"
#include "../../tb_utils/tb_utils.h"

#define WIDTH  32
#define HEIGHT 32
#define FIFO_DEPTH (WIDTH * 4)

int sc_main(int argc, char* argv[]) {
    std::cout << "==================================================" << std::endl;
    std::cout << "Testbench: Auto Exposure Control (AEC)" << std::endl;
    std::cout << "==================================================" << std::endl;

    const std::size_t rgb_pixels = WIDTH * HEIGHT;

    // Generate RGB test pattern
    std::vector<std::uint16_t> test_input(rgb_pixels * 3);
    for (std::size_t i = 0; i < rgb_pixels; ++i) {
        test_input[i * 3 + 0] = 2000;
        test_input[i * 3 + 1] = 2000;
        test_input[i * 3 + 2] = 2000;
    }

    // Golden reference
    aec_block golden_block;
    aec_config cfg;
    cfg.is_enable = true;
    cfg.center_illuminance = 128;
    cfg.histogram_skewness = 0.5f;

    std::int32_t golden_feedback;
    {
        aec_config temp_cfg = cfg;
        golden_block.process(test_input.data(), WIDTH, HEIGHT, temp_cfg, 12);
        golden_feedback = temp_cfg.ae_feedback;
    }

    // SystemC
    sc_core::sc_fifo<std::uint16_t> input_fifo(FIFO_DEPTH);
    sc_core::sc_fifo<std::uint16_t> output_fifo(FIFO_DEPTH);

    Generic_Driver<std::uint16_t> driver("driver", test_input);
    driver.fifo_out(input_fifo);

    sc_aec dut("aec_dut", cfg, WIDTH, HEIGHT, 12);
    dut.fifo_in(input_fifo);
    dut.fifo_out(output_fifo);

    Generic_Monitor<std::uint16_t> monitor("monitor", rgb_pixels * 3);
    monitor.fifo_in(output_fifo);

    std::cout << "[TB] Starting simulation..." << std::endl;
    sc_start();
    std::cout << "[TB] Simulation completed" << std::endl;

    std::cout << "Computed feedback: " << dut.get_ae_feedback() << std::endl;
    std::cout << "Golden feedback:   " << golden_feedback << std::endl;

    bool pass = (dut.get_ae_feedback() == golden_feedback);
    std::cout << "TEST RESULT: " << (pass ? "PASS" : "FAIL") << std::endl;
    return pass ? 0 : 1;
}
