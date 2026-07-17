/**
 * @file sc_dpc.h
 * @brief SystemC Streaming Module for Defective Pixel Correction (DPC)
 *
 * Implements defect pixel detection using gradient analysis on a 5x5 window.
 * Uses internal line buffers to handle boundary conditions without global lookups.
 * Streaming architecture with sc_fifo synchronization.
 *
 * Hardware shell features (Phase 3):
 *   - Optional hw_params for timing
 *   - Frame-level processing metrics
 *   - Architecture metrics
 */
#ifndef SC_DPC_H
#define SC_DPC_H

#include <systemc>
using namespace sc_core;

#include <systemc>
#include <cstdint>
#include <algorithm>
#include <deque>

#include "../../../blocks/dpc/include/dpc.h"
#include "../../tb_utils/sc_block_metrics.h"
#include "../../hw/isp_arch_config.h"

SC_MODULE(sc_dpc) {
public:
    sc_core::sc_port<sc_fifo_in_if<std::uint16_t>> fifo_in;
    sc_core::sc_port<sc_fifo_out_if<std::uint16_t>> fifo_out;

    // Optional clock port for timed mode (pointer for optional binding)
    sc_in<bool>* clk = nullptr;

    SC_HAS_PROCESS(sc_dpc);

    sc_dpc(sc_core::sc_module_name name,
           const dpc_config& cfg,
           std::uint32_t width,
           std::uint32_t height,
           const hw_params* hw = nullptr)
        : sc_module(name)
        , m_cfg(cfg)
        , m_width(width)
        , m_height(height)
        , m_hw(hw)
        , m_metrics("dpc") {
        SC_THREAD(process_stream);
    }

    // Block-level metrics collector
    sc_block_metrics<std::uint16_t> m_metrics;

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

    const dpc_config& m_cfg;
    std::uint32_t m_width;
    std::uint32_t m_height;

    // Hardware parameters
    const hw_params* m_hw;

    // Architecture metrics
    std::uint64_t m_active_cycles = 0;
    std::uint64_t m_starved_cycles = 0;
    std::uint64_t m_cycle_count = 0;
};

#endif  // SC_DPC_H
