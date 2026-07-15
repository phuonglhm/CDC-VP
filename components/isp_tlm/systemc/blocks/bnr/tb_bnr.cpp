/**

 * @file tb_bnr.cpp

 * @brief Testbench for Bayer Noise Reduction (BNR) SystemC Module

 */

#include <systemc>
using namespace sc_core;


#include <iostream>
#include <vector>

#include "sc_bnr.h"
#include "../../tb_utils/tb_utils.h"

#define WIDTH  32
#define HEIGHT 32
#define FIFO_DEPTH (WIDTH * 4)

int sc_main(int argc, char* argv[]) {
    std::cout << "==================================================" << std::endl;
    std::cout << "Testbench: Bayer Noise Reduction (BNR)" << std::endl;
    std::cout << "==================================================" << std::endl;

    const std::size_t raw_pixels = WIDTH * HEIGHT;

    // Generate test input
    std::vector<std::uint16_t> test_input = tb_utils::generate_test_pattern<std::uint16_t>(
        WIDTH, HEIGHT, 1, 1024);

    // Golden reference
    bnr_block golden_block;
    std::vector<std::uint16_t> golden_output(raw_pixels);

    bnr_config cfg;
    cfg.is_enable = true;
    cfg.filter_window = 3;
    cfg.r_std_dev_s = 1.5f;
    cfg.r_std_dev_r = 0.1f;
    cfg.g_std_dev_s = 1.5f;
    cfg.g_std_dev_r = 0.1f;
    cfg.b_std_dev_s = 1.5f;
    cfg.b_std_dev_r = 0.1f;

    golden_block.process(test_input.data(), golden_output.data(),
                       WIDTH, HEIGHT, cfg, cfa_types::RGGB, 12);

    // SystemC
    sc_core::sc_fifo<std::uint16_t> input_fifo(FIFO_DEPTH);
    sc_core::sc_fifo<std::uint16_t> output_fifo(FIFO_DEPTH);

    Generic_Driver<std::uint16_t> driver("driver", test_input);
    driver.fifo_out(input_fifo);

    sc_bnr dut("bnr_dut", cfg, cfa_types::RGGB, 12, WIDTH, HEIGHT);
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
    std::size_t diff_count = 0;
    double total_diff = 0.0;

    for (std::size_t i = 0; i < raw_pixels; ++i) {
        if (captured[i] != golden_output[i]) {
            diff_count++;
            total_diff += std::abs(static_cast<double>(captured[i]) - golden_output[i]);
        }
    }

    double avg_diff = (diff_count > 0) ? (total_diff / diff_count) : 0.0;
    std::cout << "Differences: " << diff_count << " / " << raw_pixels << std::endl;
    std::cout << "Average difference: " << avg_diff << std::endl;

    pass = (diff_count == 0);
    std::cout << "TEST RESULT: " << (pass ? "PASS" : "FAIL") << std::endl;
    return pass ? 0 : 1;
}
