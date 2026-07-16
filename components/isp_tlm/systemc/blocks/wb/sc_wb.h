/**
 * @file sc_wb.h
 * @brief SystemC Streaming Module for White Balance (WB)
 *
 * Implements channel-specific gain multiplication (R and B channels).
 * Uses PixelRGB struct for RGB interleaved data streaming.
 *
 * Hardware shell features (Phase 3):
 *   - Optional hw_params for timing
 *   - Configurable cycles per pixel
 *   - Architecture metrics
 */
#ifndef SC_WB_H
#define SC_WB_H

#include <systemc>
using namespace sc_core;

#include <systemc>
#include <cstdint>
#include <algorithm>

#include "../../../blocks/wb/include/wb.h"
#include "../../tb_utils/sc_block_metrics.h"
#include "../../hw/isp_arch_config.h"
#include "../awb/sc_awb.h"

SC_MODULE(sc_wb) {
public:
    sc_core::sc_port<sc_fifo_in_if<std::uint16_t>> fifo_in;
    sc_core::sc_port<sc_fifo_out_if<std::uint16_t>> fifo_out;

    // Optional clock port for timed mode
    sc_in<bool> clk;

    SC_HAS_PROCESS(sc_wb);

    sc_wb(sc_core::sc_module_name name,
         const wb_config& cfg,
         const hw_params* hw = nullptr)
        : sc_module(name)
        , m_cfg(cfg)
        , m_awb(nullptr)
        , m_hw(hw)
        , m_cycles_per_pixel(1)
        , m_metrics("wb") {
        SC_THREAD(process_stream);
    }

    sc_block_metrics<std::uint16_t> m_metrics;

    // Allow sc_isp_pipeline to wire up the upstream AWB
    void bind_awb(class sc_awb* awb) { m_awb = awb; }

    // Frame-latched gains accessors
    float get_latched_r_gain() const {
        return m_awb ? m_awb->get_latched_r_gain() : 1.0f;
    }
    float get_latched_b_gain() const {
        return m_awb ? m_awb->get_latched_b_gain() : 1.0f;
    }

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

    // Process RGB triplet - pure functional
    void process_pixel_triplet(std::uint16_t r_in, std::uint16_t g_in, std::uint16_t b_in,
                              std::uint16_t& r_out, std::uint16_t& g_out, std::uint16_t& b_out);

    const wb_config& m_cfg;
    class sc_awb* m_awb;

    // Hardware parameters
    const hw_params* m_hw;
    int m_cycles_per_pixel;

    // Architecture metrics
    std::uint64_t m_active_cycles = 0;
    std::uint64_t m_starved_cycles = 0;
    std::uint64_t m_cycle_count = 0;
};

#endif  // SC_WB_H
