/**

 * @file tb_blc.cpp

 * @brief Testbench for Black Level Correction (BLC) SystemC Module

 *

 * Validates sc_blc streaming module by comparing output against golden

 * reference computed by the original blc_block.

 */

#include <systemc>
using namespace sc_core;


#include <iostream>
#include <vector>
#include <cstdlib>
#include <ctime>

#include "sc_blc.h"
#include "../../tb_utils/tb_utils.h"

#define WIDTH  64
#define HEIGHT 64
#define FIFO_DEPTH (WIDTH * 4)

int sc_main(int argc, char* argv[]) {
    std::cout << "==================================================" << std::endl;
    std::cout << "Testbench: Black Level Correction (BLC)" << std::endl;
    std::cout << "==================================================" << std::endl;

    const std::size_t raw_pixels = WIDTH * HEIGHT;

    // Generate test input pattern (ramp)
    std::vector<std::uint16_t> test_input = tb_utils::generate_test_pattern<std::uint16_t>(
        WIDTH, HEIGHT, 1, 2048);

    std::cout << "[TB] Generated test pattern: " << raw_pixels << " pixels" << std::endl;

    // Compute golden reference using original C++ block
    blc_block golden_block;
    std::vector<std::uint16_t> golden_output(raw_pixels);

    blc_config cfg;
    cfg.is_enable = true;
    cfg.is_linear = true;
    cfg.r_offset = 256;
    cfg.gr_offset = 256;
    cfg.gb_offset = 256;
    cfg.b_offset = 256;
    cfg.r_sat = 4095;
    cfg.gr_sat = 4095;
    cfg.gb_sat = 4095;
    cfg.b_sat = 4095;

    golden_block.process(test_input.data(), golden_output.data(),
                        WIDTH, HEIGHT, cfg, cfa_types::RGGB, 12);

    std::cout << "[TB] Golden reference computed" << std::endl;

    // Print sample values for verification
    std::cout << "[TB] Sample golden values (first 5): ";
    for (int i = 0; i < 5; ++i) {
        std::cout << golden_output[i] << " ";
    }
    std::cout << std::endl;

    // Create SystemC module hierarchy
    sc_core::sc_fifo<std::uint16_t> input_fifo(FIFO_DEPTH);
    sc_core::sc_fifo<std::uint16_t> output_fifo(FIFO_DEPTH);

    Generic_Driver<std::uint16_t> driver("driver", test_input);
    driver.fifo_out(input_fifo);

    sc_blc dut("blc_dut", cfg, cfa_types::RGGB, 12, 640, 480, nullptr);
    dut.fifo_in(input_fifo);
    dut.fifo_out(output_fifo);

    Generic_Monitor<std::uint16_t> monitor("monitor", raw_pixels);
    monitor.fifo_in(output_fifo);
    monitor.set_golden_reference(golden_output.data(), golden_output.size());

    std::cout << "[TB] Starting simulation..." << std::endl;

    sc_core::sc_time sim_time(0, sc_core::SC_NS);
    sc_start(sim_time);

    sc_start();

    std::cout << "[TB] Simulation completed" << std::endl;
    std::cout << "[TB] Final simulation time: " << sc_core::sc_time_stamp().to_seconds() << " s" << std::endl;

    // Verification
    const auto& captured = monitor.get_captured_data();
    bool pass = true;
    std::size_t mismatch_count = 0;

    for (std::size_t i = 0; i < raw_pixels; ++i) {
        if (captured[i] != golden_output[i]) {
            if (mismatch_count < 5) {
                std::cerr << "[TB] Mismatch at pixel " << i
                          << ": captured=" << captured[i]
                          << " expected=" << golden_output[i] << std::endl;
            }
            mismatch_count++;
            pass = false;
        }
    }

    std::cout << std::endl;
    std::cout << "==================================================" << std::endl;
    if (pass) {
        std::cout << "TEST RESULT: PASS" << std::endl;
    } else {
        std::cout << "TEST RESULT: FAIL" << std::endl;
        std::cout << "Mismatches: " << mismatch_count << " / " << raw_pixels << std::endl;
    }
    std::cout << "==================================================" << std::endl;

    return pass ? 0 : 1;
}
