/**

 * @file tb_lsc.cpp

 * @brief Testbench for Lens Shading Correction (LSC) SystemC Module

 */

#include <systemc>
using namespace sc_core;


#include <iostream>
#include <vector>

#include "sc_lsc.h"
#include "../../tb_utils/tb_utils.h"

#define WIDTH  64
#define HEIGHT 64
#define FIFO_DEPTH (WIDTH * 4)

int sc_main(int argc, char* argv[]) {
    std::cout << "==================================================" << std::endl;
    std::cout << "Testbench: Lens Shading Correction (LSC)" << std::endl;
    std::cout << "==================================================" << std::endl;

    const std::size_t raw_pixels = WIDTH * HEIGHT;

    // Generate test input
    std::vector<std::uint16_t> test_input = tb_utils::generate_test_pattern<std::uint16_t>(
        WIDTH, HEIGHT, 0, 2048);

    // Create LSC LUT
    std::uint32_t grid_w = 16;
    std::uint32_t grid_h = 12;
    std::uint32_t nx = grid_w + 1;
    std::uint32_t ny = grid_h + 1;
    std::uint32_t nodes_per_channel = nx * ny;
    std::vector<float> lsc_lut(nodes_per_channel * 4, 1.0f);

    // Fill with simple radial gain
    float center_x = WIDTH / 2.0f;
    float center_y = HEIGHT / 2.0f;
    for (std::uint32_t ch = 0; ch < 4; ++ch) {
        for (std::uint32_t y = 0; y < ny; ++y) {
            for (std::uint32_t x = 0; x < nx; ++x) {
                float px = x * (WIDTH / grid_w);
                float py = y * (HEIGHT / grid_h);
                float dx = (px - center_x) / center_x;
                float dy = (py - center_y) / center_y;
                float dist2 = dx * dx + dy * dy;
                std::size_t idx = ch * nodes_per_channel + y * nx + x;
                lsc_lut[idx] = 1.0f + 0.3f * dist2;
            }
        }
    }

    // Golden reference
    lsc_block golden_block;
    std::vector<std::uint16_t> golden_output(raw_pixels);

    lsc_config cfg;
    cfg.is_enable = true;
    cfg.grid_width = grid_w;
    cfg.grid_height = grid_h;

    golden_block.process(test_input.data(), golden_output.data(),
                       WIDTH, HEIGHT, cfg, lsc_lut.data(),
                       cfa_types::RGGB, 12);

    // SystemC
    sc_core::sc_fifo<std::uint16_t> input_fifo(FIFO_DEPTH);
    sc_core::sc_fifo<std::uint16_t> output_fifo(FIFO_DEPTH);

    Generic_Driver<std::uint16_t> driver("driver", test_input);
    driver.fifo_out(input_fifo);

    sc_lsc dut("lsc_dut", cfg, lsc_lut, cfa_types::RGGB, 12, WIDTH, HEIGHT);
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
