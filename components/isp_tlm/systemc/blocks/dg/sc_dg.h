/**
 * @file sc_dg.h
 * @brief SystemC Streaming Module for Digital Gain (DG)
 *
 * Implements digital gain multiplication for RAW Bayer data.
 * Streaming architecture with pixel-by-pixel processing.
 *
 * Hardware shell features (Phase 3):
 *   - Optional hw_params for timing
 *   - Configurable cycles per pixel
 *   - Architecture metrics
 */
#ifndef SC_DG_H
#define SC_DG_H

#include <systemc>
using namespace sc_core;

#include <systemc>
#include <cstdint>
#include <algorithm>

#include "../../../blocks/dg/include/dg.h"
#include "../../tb_utils/sc_block_metrics.h"
#include "../../hw/isp_arch_config.h"

SC_MODULE(sc_dg) {
public:
    sc_core::sc_port<sc_fifo_in_if<std::uint16_t>> fifo_in;
    sc_core::sc_port<sc_fifo_out_if<std::uint16_t>> fifo_out;

    // Optional clock port for timed mode (pointer for optional binding)
    sc_in<bool>* clk = nullptr;

    SC_HAS_PROCESS(sc_dg);

    sc_dg(sc_core::sc_module_name name,
          const dg_config& cfg,
          std::uint8_t bit_depth = 12,
          const hw_params* hw = nullptr)
        : sc_module(name)
        , m_cfg(cfg)
        , m_bit_depth(bit_depth)
        , m_hw(hw)
        , m_cycles_per_pixel(1)
        , m_metrics("dg") {
        SC_THREAD(process_stream);
    }

    // Block-level metrics collector
    sc_block_metrics<std::uint16_t> m_metrics;

    // Hardware configuration
    void set_hw_params(const hw_params* hw) { m_hw = hw; }
    void set_cycles_per_pixel(int cycles) { m_cycles_per_pixel = cycles; }

    // Architecture metrics
    std::uint64_t active_cycles() const { return m_active_cycles; }
    std::uint64_t starved_cycles() const { return m_starved_cycles; }
    std::uint64_t total_cycles() const { return m_cycle_count; }
    double utilization() const {
        return (m_cycle_count > 0) ?
            static_cast<double>(m_active_cycles) / m_cycle_count : 0.0;
    }

    void reset_counters() {
        m_active_cycles = 0;
        m_starved_cycles = 0;
        m_cycle_count = 0;
    }

private:
    void process_stream();

    // Process a single pixel - pure functional
    std::uint16_t process_pixel(std::uint16_t pixel);

    const dg_config& m_cfg;
    std::uint8_t m_bit_depth;

    // Hardware parameters
    const hw_params* m_hw;
    int m_cycles_per_pixel;

    // Architecture metrics
    std::uint64_t m_active_cycles = 0;
    std::uint64_t m_starved_cycles = 0;
    std::uint64_t m_cycle_count = 0;
};

#endif  // SC_DG_H
