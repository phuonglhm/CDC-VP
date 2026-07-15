/**

 * @file tb_cse.cpp

 * @brief Testbench for Color Saturation Enhancement (CSE) SystemC Module

 */

#include <systemc>
using namespace sc_core;


#include <iostream>
#include <vector>

#include "sc_cse.h"
#include "../../tb_utils/tb_utils.h"

#define WIDTH  32
#define HEIGHT 32
#define FIFO_DEPTH (WIDTH * 4)

int sc_main(int argc, char* argv[]) {
    std::cout << "==================================================" << std::endl;
    std::cout << "Testbench: Color Saturation Enhancement (CSE)" << std::endl;
    std::cout << "==================================================" << std::endl;

    const std::size_t yuv_pixels = WIDTH * HEIGHT;

    // Generate YUV test pattern (YUV444)
    std::vector<std::uint8_t> test_input(yuv_pixels * 3);
    for (std::size_t i = 0; i < yuv_pixels; ++i) {
        test_input[i * 3 + 0] = 128;  // Y
        test_input[i * 3 + 1] = 128;  // U
        test_input[i * 3 + 2] = 128;  // V
    }

    // Golden reference
    cse_block golden_block;
    std::vector<std::uint8_t> golden_output(yuv_pixels * 3);

    cse_config cfg;
    cfg.is_enable = true;
    cfg.saturation_gain = 1.5f;

    golden_block.process(test_input.data(), golden_output.data(), WIDTH, HEIGHT, cfg);

    // SystemC
    sc_core::sc_fifo<std::uint8_t> input_fifo(FIFO_DEPTH);
    sc_core::sc_fifo<std::uint8_t> output_fifo(FIFO_DEPTH);

    Generic_Driver<std::uint8_t> driver("driver", test_input);
    driver.fifo_out(input_fifo);

    sc_cse dut("cse_dut", cfg);
    dut.fifo_in(input_fifo);
    dut.fifo_out(output_fifo);

    Generic_Monitor<std::uint8_t> monitor("monitor", yuv_pixels * 3);
    monitor.fifo_in(output_fifo);
    monitor.set_golden_reference(golden_output.data(), golden_output.size());

    std::cout << "[TB] Starting simulation..." << std::endl;
    sc_start();
    std::cout << "[TB] Simulation completed" << std::endl;

    const auto& captured = monitor.get_captured_data();
    bool pass = true;
    for (std::size_t i = 0; i < yuv_pixels * 3 && pass; ++i) {
        if (captured[i] != golden_output[i]) pass = false;
    }

    std::cout << "TEST RESULT: " << (pass ? "PASS" : "FAIL") << std::endl;
    return pass ? 0 : 1;
}
