/**

 * @file tb_demosaic.cpp

 * @brief Testbench for Demosaic SystemC Module

 */

#include <systemc>
using namespace sc_core;


#include <iostream>
#include <vector>

#include "sc_demosaic.h"
#include "../../tb_utils/tb_utils.h"

#define WIDTH  32
#define HEIGHT 32
#define FIFO_DEPTH (WIDTH * 4)

int sc_main(int argc, char* argv[]) {
    std::cout << "==================================================" << std::endl;
    std::cout << "Testbench: Demosaic (CFA to RGB)" << std::endl;
    std::cout << "==================================================" << std::endl;

    const std::size_t raw_pixels = WIDTH * HEIGHT;
    const std::size_t rgb_pixels = raw_pixels * 3;

    // Generate RAW Bayer test pattern
    std::vector<std::uint16_t> test_input(raw_pixels);
    for (std::size_t i = 0; i < raw_pixels; ++i) {
        test_input[i] = static_cast<std::uint16_t>(1000 + (i % 2000));
    }

    // Golden reference
    demosaic_block golden_block;
    std::vector<std::uint16_t> golden_output(rgb_pixels);

    demosaic_config cfg;
    cfg.is_enable = true;

    golden_block.process(test_input.data(), golden_output.data(),
                      WIDTH, HEIGHT, cfg, cfa_types::RGGB, 12);

    // SystemC
    sc_core::sc_fifo<std::uint16_t> input_fifo(FIFO_DEPTH);
    sc_core::sc_fifo<std::uint16_t> output_fifo(FIFO_DEPTH);

    Generic_Driver<std::uint16_t> driver("driver", test_input);
    driver.fifo_out(input_fifo);

    sc_demosaic dut("demosaic_dut", cfg, cfa_types::RGGB, 12, WIDTH, HEIGHT);
    dut.fifo_in(input_fifo);
    dut.fifo_out(output_fifo);

    Generic_Monitor<std::uint16_t> monitor("monitor", rgb_pixels);
    monitor.fifo_in(output_fifo);
    monitor.set_golden_reference(golden_output.data(), golden_output.size());

    std::cout << "[TB] Starting simulation..." << std::endl;
    sc_start();
    std::cout << "[TB] Simulation completed" << std::endl;

    const auto& captured = monitor.get_captured_data();
    bool pass = true;
    std::size_t diff_count = 0;

    for (std::size_t i = 0; i < rgb_pixels; ++i) {
        if (captured[i] != golden_output[i]) {
            diff_count++;
            pass = false;
        }
    }

    std::cout << "Differences: " << diff_count << " / " << rgb_pixels << std::endl;
    std::cout << "TEST RESULT: " << (pass ? "PASS" : "FAIL") << std::endl;
    return pass ? 0 : 1;
}
