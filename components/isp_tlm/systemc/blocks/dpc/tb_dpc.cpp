/**

 * @file tb_dpc.cpp

 * @brief Testbench for Defective Pixel Correction (DPC) SystemC Module

 */

#include <systemc>
using namespace sc_core;


#include <iostream>
#include <vector>
#include <cstdlib>

#include "sc_dpc.h"
#include "../../tb_utils/tb_utils.h"

#define WIDTH  64
#define HEIGHT 64
#define FIFO_DEPTH (WIDTH * 4)

int sc_main(int argc, char* argv[]) {
    std::cout << "==================================================" << std::endl;
    std::cout << "Testbench: Defective Pixel Correction (DPC)" << std::endl;
    std::cout << "==================================================" << std::endl;

    const std::size_t raw_pixels = WIDTH * HEIGHT;

    // Generate test input pattern with some edge/corner pixels
    std::vector<std::uint16_t> test_input(raw_pixels);
    for (std::size_t i = 0; i < raw_pixels; ++i) {
        test_input[i] = static_cast<std::uint16_t>(1000 + (i % 2000));
    }

    // Inject artificial defect at center
    test_input[WIDTH * (HEIGHT/2) + WIDTH/2] = 0;

    std::cout << "[TB] Generated test pattern: " << raw_pixels << " pixels" << std::endl;

    // Compute golden reference
    dpc_block golden_block;
    std::vector<std::uint16_t> golden_output(raw_pixels);

    dpc_config cfg;
    cfg.is_enable = true;
    cfg.dp_threshold = 30;

    golden_block.process(test_input.data(), golden_output.data(), WIDTH, HEIGHT, cfg);

    std::cout << "[TB] Golden reference computed" << std::endl;

    // Create SystemC module hierarchy
    sc_core::sc_fifo<std::uint16_t> input_fifo(FIFO_DEPTH);
    sc_core::sc_fifo<std::uint16_t> output_fifo(FIFO_DEPTH);

    Generic_Driver<std::uint16_t> driver("driver", test_input);
    driver.fifo_out(input_fifo);

    sc_dpc dut("dpc_dut", cfg, WIDTH, HEIGHT);
    dut.fifo_in(input_fifo);
    dut.fifo_out(output_fifo);

    Generic_Monitor<std::uint16_t> monitor("monitor", raw_pixels);
    monitor.fifo_in(output_fifo);
    monitor.set_golden_reference(golden_output.data(), golden_output.size());

    std::cout << "[TB] Starting simulation..." << std::endl;

    sc_start();

    std::cout << "[TB] Simulation completed" << std::endl;

    // Verification
    const auto& captured = monitor.get_captured_data();
    bool pass = true;
    std::size_t mismatch_count = 0;

    for (std::size_t i = 0; i < raw_pixels; ++i) {
        if (captured[i] != golden_output[i]) {
            if (mismatch_count < 5) {
                std::cerr << "[TB] Mismatch at pixel " << i << std::endl;
            }
            mismatch_count++;
            pass = false;
        }
    }

    std::cout << std::endl;
    std::cout << "==================================================" << std::endl;
    std::cout << "TEST RESULT: " << (pass ? "PASS" : "FAIL") << std::endl;
    if (!pass) {
        std::cout << "Mismatches: " << mismatch_count << " / " << raw_pixels << std::endl;
    }
    std::cout << "==================================================" << std::endl;

    return pass ? 0 : 1;
}
