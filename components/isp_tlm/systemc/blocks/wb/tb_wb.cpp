/**

 * @file tb_wb.cpp

 * @brief Testbench for White Balance (WB) SystemC Module

 */

#include <systemc>
using namespace sc_core;


#include <iostream>
#include <vector>

#include "sc_wb.h"
#include "../../tb_utils/tb_utils.h"

#define WIDTH  32
#define HEIGHT 32
#define FIFO_DEPTH (WIDTH * 4)

int sc_main(int argc, char* argv[]) {
    std::cout << "==================================================" << std::endl;
    std::cout << "Testbench: White Balance (WB)" << std::endl;
    std::cout << "==================================================" << std::endl;

    const std::size_t rgb_pixels = WIDTH * HEIGHT;

    // Generate RGB test pattern (3 channels interleaved)
    std::vector<std::uint16_t> test_input(rgb_pixels * 3);
    for (std::size_t i = 0; i < rgb_pixels; ++i) {
        test_input[i * 3 + 0] = 1000;
        test_input[i * 3 + 1] = 1500;
        test_input[i * 3 + 2] = 800;
    }

    // Golden reference
    wb_block golden_block;
    std::vector<std::uint16_t> golden_output(rgb_pixels * 3);

    wb_config cfg;
    cfg.is_enable = true;
    cfg.r_gain = 1.5f;
    cfg.b_gain = 2.0f;

    golden_block.process(test_input.data(), golden_output.data(), WIDTH, HEIGHT, cfg);

    // SystemC
    sc_core::sc_fifo<std::uint16_t> input_fifo(FIFO_DEPTH);
    sc_core::sc_fifo<std::uint16_t> output_fifo(FIFO_DEPTH);

    Generic_Driver<std::uint16_t> driver("driver", test_input);
    driver.fifo_out(input_fifo);

    sc_wb dut("wb_dut", cfg);
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
