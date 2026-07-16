/**
 * @file sc_2dnr.h
 * @brief SystemC Streaming Module for 2D Noise Reduction (2DNR)
 *
 * Implements patch-based denoising on Y channel.
 *
 * Hardware shell features (Phase 3):
 *   - Optional hw_params for timing
 *   - Frame-level processing metrics
 *   - Architecture metrics
 */
#ifndef SC_2DNR_H
#define SC_2DNR_H

#include <systemc>
using namespace sc_core;

#include <systemc>
#include <cstdint>
#include <vector>

#include "../../../blocks/2dnr/include/2dnr.h"
#include "../../tb_utils/sc_block_metrics.h"
#include "../../hw/isp_arch_config.h"

SC_MODULE(sc_2dnr) {
public:
    sc_core::sc_port<sc_fifo_in_if<std::uint8_t>> fifo_in;
    sc_core::sc_port<sc_fifo_out_if<std::uint8_t>> fifo_out;

    // Optional clock port for timed mode
    sc_in<bool> clk;

    SC_HAS_PROCESS(sc_2dnr);

    sc_2dnr(sc_core::sc_module_name name,
            const twodnr_config& cfg,
            std::uint32_t width,
            std::uint32_t height,
            const hw_params* hw = nullptr)
        : sc_module(name)
        , m_cfg(cfg)
        , m_width(width)
        , m_height(height)
        , m_hw(hw)
        , m_metrics("2dnr") {
        SC_THREAD(process_stream);
    }

    sc_block_metrics<std::uint8_t> m_metrics;

    // Hardware configuration
    void set_hw_params(const hw_params* hw) { m_hw = hw; }

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

    const twodnr_config& m_cfg;
    std::uint32_t m_width;
    std::uint32_t m_height;

    // Hardware parameters
    const hw_params* m_hw;

    // Architecture metrics
    std::uint64_t m_active_cycles = 0;
    std::uint64_t m_starved_cycles = 0;
    std::uint64_t m_cycle_count = 0;
};

#endif  // SC_2DNR_H
