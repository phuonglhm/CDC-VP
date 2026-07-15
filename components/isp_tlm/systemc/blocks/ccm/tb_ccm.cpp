/**

 * @file tb_ccm.cpp

 * @brief Testbench for Color Correction Matrix (CCM) SystemC Module

 */

#include <systemc>
using namespace sc_core;


#include <iostream>
#include <vector>

#include "sc_ccm.h"
#include "../../tb_utils/tb_utils.h"

#define WIDTH  32
#define HEIGHT 32
#define FIFO_DEPTH (WIDTH * 4)

int sc_main(int argc, char* argv[]) {
    std::cout << "==================================================" << std::endl;
    std::cout << "Testbench: Color Correction Matrix (CCM)" << std::endl;
    std::cout << "==================================================" << std::endl;

    const std::size_t rgb_pixels = WIDTH * HEIGHT;

    // Generate RGB test pattern
    std::vector<std::uint16_t> test_input = tb_utils::generate_rgb_test_pattern(WIDTH, HEIGHT, 1);

    // Golden reference
    ccm_block golden_block;
    std::vector<std::uint16_t> golden_output(rgb_pixels * 3);

    ccm_config cfg;
    cfg.is_enable = true;
    cfg.bit_depth = 12;
    // Identity matrix (no change)
    cfg.corrected_red[0] = 1.0f; cfg.corrected_red[1] = 0.0f; cfg.corrected_red[2] = 0.0f;
    cfg.corrected_green[0] = 0.0f; cfg.corrected_green[1] = 1.0f; cfg.corrected_green[2] = 0.0f;
    cfg.corrected_blue[0] = 0.0f; cfg.corrected_blue[1] = 0.0f; cfg.corrected_blue[2] = 1.0f;

    golden_block.process(test_input.data(), golden_output.data(), WIDTH, HEIGHT, cfg);

    // SystemC
    sc_core::sc_fifo<std::uint16_t> input_fifo(FIFO_DEPTH);
    sc_core::sc_fifo<std::uint16_t> output_fifo(FIFO_DEPTH);

    Generic_Driver<std::uint16_t> driver("driver", test_input);
    driver.fifo_out(input_fifo);

    sc_ccm dut("ccm_dut", cfg);
    dut.fifo_in(input_fifo);
    dut.fifo_out(output_fifo);

    Generic_Monitor<std::uint16_t> monitor("monitor", rgb_pixels * 3);
    monitor.fifo_in(output_fifo);
    monitor.set_golden_reference(golden_output.data(), golden_output.size());

    std::cout << "[TB] Starting simulation..." << std::endl;
    sc_start();
    std::cout << "[TB] Simulation completed" << std::endl;

    const auto& captured = monitor.get_captured_data();
    bool pass = true;
    for (std::size_t i = 0; i < rgb_pixels * 3 && pass; ++i) {
        if (captured[i] != golden_output[i]) pass = false;
    }

    std::cout << "TEST RESULT: " << (pass ? "PASS" : "FAIL") << std::endl;
    return pass ? 0 : 1;
}
