/**
 * @file metrics_demo_tb.cpp
 * @brief Standalone testbench for sc_metrics_wrapper
 *
 * Exercises sc_metrics_wrapper<uint16_t> between a Generic_Driver and a
 * sc_blc block. Dumps a CSV of per-token latency after sc_start().
 *
 * Expected output:
 *   output/metrics_blc_demo.csv
 *   output/metrics_blc_demo_summary.txt
 */

#include <systemc>
using namespace sc_core;

#include <iostream>
#include <vector>
#include <string>
#include <cstdint>
#include <cstdlib>

#include "tb_utils.h"
#include "sc_metrics_wrapper.h"
#include "../blocks/blc/sc_blc.h"
#include "../../blocks/blc/include/blc.h"
#include "../../core/include/isp_types.h"

int sc_main(int argc, char* argv[]) {
    std::cout << "==================================================" << std::endl;
    std::cout << "METRICS WRAPPER DEMO TESTBENCH" << std::endl;
    std::cout << "==================================================" << std::endl;

    const std::size_t N = 1024;  // 32x32 worth of pixels

    // 1. Toy input
    std::vector<uint16_t> input(N);
    for (std::size_t i = 0; i < N; ++i) input[i] = (uint16_t)(i * 7 % 4096);

    // 2. FIFOs
    sc_core::sc_fifo<uint16_t> in_fifo(64);    // driver -> wrapper
    sc_core::sc_fifo<uint16_t> out_fifo(64);   // wrapper -> blc.in
    sc_core::sc_fifo<uint16_t> sink(1024);     // blc.out sink

    // 3. BLC config
    blc_config cfg{};
    cfg.is_enable = true;
    cfg.b_offset = cfg.gb_offset = cfg.r_offset = cfg.gr_offset = 256;
    cfg.b_sat    = cfg.gb_sat    = cfg.r_sat    = cfg.gr_sat    = 4095;

    // 4. Driver pushes to in_fifo
    Generic_Driver<uint16_t> driver("driver", input);
    driver.fifo_out(in_fifo);

    // 5. Wrapper taps in_fifo, writes out_fifo
    sc_metrics_wrapper<uint16_t> wrap("blc_wrapper", "blc");
    wrap.fifo_in(in_fifo);
    wrap.fifo_out(out_fifo);

    // 6. sc_blc reads out_fifo, writes sink
    sc_blc blc("blc_inst", cfg, cfa_types::RGGB, /*bit_depth=*/12, 1280, 720, nullptr);
    blc.fifo_in(out_fifo);
    blc.fifo_out(sink);

    // Run simulation. Generic_Driver terminates when all tokens are sent;
    // sc_blc processes them and writes to sink. SystemC detects end-of-simulation.
    std::cout << "[TB] Starting simulation with N=" << N << " tokens" << std::endl;
    sc_start();
    std::cout << "[TB] Simulation completed at t="
              << sc_time_stamp().to_seconds() / 1e-9 << " ns" << std::endl;

    // 7. Dump metrics
    wrap.dump_metrics("output/metrics_blc_demo");

    std::cout << "[TB] mean_latency   = "
              << wrap.mean_latency().to_seconds() / 1e-9 << " ns" << std::endl;
    std::cout << "[TB] min_latency    = "
              << wrap.min_latency().to_seconds() / 1e-9 << " ns" << std::endl;
    std::cout << "[TB] max_latency    = "
              << wrap.max_latency().to_seconds() / 1e-9 << " ns" << std::endl;
    std::cout << "[TB] sample_count   = " << wrap.sample_count() << std::endl;

    return (wrap.sample_count() == N) ? 0 : 1;
}