/**

 * @file tb_scale.cpp

 * @brief Testbench for Scaling SystemC Module

 */

#include <systemc>
using namespace sc_core;


#include <iostream>
#include <vector>

#include "sc_scale.h"
#include "../../tb_utils/tb_utils.h"

#define IN_WIDTH  32
#define IN_HEIGHT 32
#define OUT_WIDTH 16
#define OUT_HEIGHT 16
#define FIFO_DEPTH (IN_WIDTH * 4)

int sc_main(int argc, char* argv[]) {
    std::cout << "==================================================" << std::endl;
    std::cout << "Testbench: Scaling (32x32 -> 16x16)" << std::endl;
    std::cout << "==================================================" << std::endl;

    const std::size_t in_pixels = IN_WIDTH * IN_HEIGHT;
    const std::size_t out_pixels = OUT_WIDTH * OUT_HEIGHT;

    // Generate YUV test pattern
    std::vector<std::uint8_t> test_input(in_pixels * 3);
    for (std::size_t i = 0; i < in_pixels; ++i) {
        test_input[i * 3 + 0] = static_cast<std::uint8_t>((i * 17) % 256);
        test_input[i * 3 + 1] = 128;
        test_input[i * 3 + 2] = 128;
    }

    // Golden reference
    scale_block golden_block;
    std::vector<std::uint8_t> golden_output(out_pixels * 3);

    scale_config cfg;
    cfg.is_enable = true;
    cfg.out_width = OUT_WIDTH;
    cfg.out_height = OUT_HEIGHT;

    golden_block.process(test_input.data(), golden_output.data(),
                       IN_WIDTH, IN_HEIGHT, OUT_WIDTH, OUT_HEIGHT, cfg);

    // SystemC
    sc_core::sc_fifo<std::uint8_t> input_fifo(FIFO_DEPTH);
    sc_core::sc_fifo<std::uint8_t> output_fifo(FIFO_DEPTH);

    Generic_Driver<std::uint8_t> driver("driver", test_input);
    driver.fifo_out(input_fifo);

    sc_scale dut("scale_dut", cfg, IN_WIDTH, IN_HEIGHT);
    dut.fifo_in(input_fifo);
    dut.fifo_out(output_fifo);

    Generic_Monitor<std::uint8_t> monitor("monitor", out_pixels * 3);
    monitor.fifo_in(output_fifo);
    monitor.set_golden_reference(golden_output.data(), golden_output.size());

    std::cout << "[TB] Starting simulation..." << std::endl;
    sc_start();
    std::cout << "[TB] Simulation completed" << std::endl;

    const auto& captured = monitor.get_captured_data();
    bool pass = true;
    for (std::size_t i = 0; i < out_pixels * 3 && pass; ++i) {
        if (captured[i] != golden_output[i]) pass = false;
    }

    std::cout << "TEST RESULT: " << (pass ? "PASS" : "FAIL") << std::endl;
    return pass ? 0 : 1;
}
