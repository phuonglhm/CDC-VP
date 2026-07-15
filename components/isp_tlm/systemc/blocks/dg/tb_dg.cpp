/**

 * @file tb_dg.cpp

 * @brief Testbench for Digital Gain (DG) SystemC Module

 */

#include <systemc>
using namespace sc_core;


#include <iostream>
#include <vector>

#include "sc_dg.h"
#include "../../tb_utils/tb_utils.h"

#define WIDTH  64
#define HEIGHT 64
#define FIFO_DEPTH (WIDTH * 4)

int sc_main(int argc, char* argv[]) {
    std::cout << "==================================================" << std::endl;
    std::cout << "Testbench: Digital Gain (DG)" << std::endl;
    std::cout << "==================================================" << std::endl;

    const std::size_t raw_pixels = WIDTH * HEIGHT;

    std::vector<std::uint16_t> test_input = tb_utils::generate_test_pattern<std::uint16_t>(
        WIDTH, HEIGHT, 1, 1024);

    // Golden reference
    dg_block golden_block;
    std::vector<std::uint16_t> golden_output(raw_pixels);

    dg_config cfg;
    cfg.is_enable = true;
    cfg.current_gain = 3;  // 8x gain

    golden_block.process(test_input.data(), golden_output.data(), WIDTH, HEIGHT, cfg, 12);

    // SystemC
    sc_core::sc_fifo<std::uint16_t> input_fifo(FIFO_DEPTH);
    sc_core::sc_fifo<std::uint16_t> output_fifo(FIFO_DEPTH);

    Generic_Driver<std::uint16_t> driver("driver", test_input);
    driver.fifo_out(input_fifo);

    sc_dg dut("dg_dut", cfg, 12);
    dut.fifo_in(input_fifo);
    dut.fifo_out(output_fifo);

    Generic_Monitor<std::uint16_t> monitor("monitor", raw_pixels);
    monitor.fifo_in(output_fifo);
    monitor.set_golden_reference(golden_output.data(), golden_output.size());

    std::cout << "[TB] Starting simulation..." << std::endl;
    sc_start();
    std::cout << "[TB] Simulation completed" << std::endl;

    const auto& captured = monitor.get_captured_data();
    bool pass = true;
    for (std::size_t i = 0; i < raw_pixels && pass; ++i) {
        if (captured[i] != golden_output[i]) pass = false;
    }

    std::cout << "TEST RESULT: " << (pass ? "PASS" : "FAIL") << std::endl;
    return pass ? 0 : 1;
}
