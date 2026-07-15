/**

 * @file tb_yuv420.cpp

 * @brief Testbench for YUV420 Conversion SystemC Module

 */

#include <systemc>
using namespace sc_core;


#include <iostream>
#include <vector>

#include "sc_yuv420.h"
#include "../../tb_utils/tb_utils.h"

#define WIDTH  32
#define HEIGHT 32
#define FIFO_DEPTH (WIDTH * 4)

int sc_main(int argc, char* argv[]) {
    std::cout << "==================================================" << std::endl;
    std::cout << "Testbench: YUV420 Conversion" << std::endl;
    std::cout << "==================================================" << std::endl;

    const std::size_t yuv444_pixels = WIDTH * HEIGHT;
    const std::size_t yuv420_size = static_cast<std::size_t>(WIDTH) * HEIGHT +
                                   2 * ((WIDTH + 1) / 2) * ((HEIGHT + 1) / 2);

    // Generate YUV444 test pattern
    std::vector<std::uint8_t> test_input(yuv444_pixels * 3);
    for (std::size_t i = 0; i < yuv444_pixels; ++i) {
        test_input[i * 3 + 0] = static_cast<std::uint8_t>((i * 17) % 256);  // Y
        test_input[i * 3 + 1] = static_cast<std::uint8_t>(100 + (i * 7) % 50);  // U
        test_input[i * 3 + 2] = static_cast<std::uint8_t>(150 + (i * 11) % 50);  // V
    }

    // Golden reference
    yuv420_block golden_block;
    std::vector<std::uint8_t> golden_output(yuv420_size);

    yuv420_config cfg;
    cfg.is_enable = true;

    golden_block.process(test_input.data(), golden_output.data(), WIDTH, HEIGHT, cfg);

    // SystemC
    sc_core::sc_fifo<std::uint8_t> input_fifo(FIFO_DEPTH);
    sc_core::sc_fifo<std::uint8_t> output_fifo(FIFO_DEPTH);

    Generic_Driver<std::uint8_t> driver("driver", test_input);
    driver.fifo_out(input_fifo);

    sc_yuv420 dut("yuv420_dut", cfg, WIDTH, HEIGHT);
    dut.fifo_in(input_fifo);
    dut.fifo_out(output_fifo);

    Generic_Monitor<std::uint8_t> monitor("monitor", yuv420_size);
    monitor.fifo_in(output_fifo);
    monitor.set_golden_reference(golden_output.data(), golden_output.size());

    std::cout << "[TB] Starting simulation..." << std::endl;
    sc_start();
    std::cout << "[TB] Simulation completed" << std::endl;

    const auto& captured = monitor.get_captured_data();
    bool pass = true;
    for (std::size_t i = 0; i < yuv420_size && pass; ++i) {
        if (captured[i] != golden_output[i]) pass = false;
    }

    std::cout << "TEST RESULT: " << (pass ? "PASS" : "FAIL") << std::endl;
    return pass ? 0 : 1;
}
