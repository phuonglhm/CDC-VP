/**
 * @file sc_blc.h
 * @brief SystemC Streaming Module for Black Level Correction (BLC)
 *
 * Implements per-channel offset subtraction and optional linear/non-linear
 * saturation for RAW Bayer data. Streaming architecture with pixel-by-pixel
 * processing using sc_fifo synchronization.
 *
 * Hardware shell features (Phase 3):
 *   - Optional clock binding via hw_params
 *   - Configurable cycles per pixel
 *   - Cycle counting for architecture metrics
 */
#ifndef SC_BLC_H
#define SC_BLC_H

#include <systemc>
using namespace sc_core;

#include <systemc>
#include <cstdint>
#include <algorithm>

#include "../../../blocks/blc/include/blc.h"
#include "../../tb_utils/sc_block_metrics.h"
#include "../../hw/isp_arch_config.h"

SC_MODULE(sc_blc) {
public:
    sc_core::sc_port<sc_fifo_in_if<std::uint16_t>> fifo_in;
    sc_core::sc_port<sc_fifo_out_if<std::uint16_t>> fifo_out;

    // Optional clock port for timed mode
    sc_in<bool> clk;

    SC_HAS_PROCESS(sc_blc);

    /**
     * @brief Constructor
     * @param name Module name
     * @param cfg BLC configuration
     * @param bayer Bayer CFA pattern
     * @param bit_depth Working bit depth
     * @param width Image width
     * @param height Image height
     * @param hw Pointer to hardware params (nullptr = untimed mode)
     */
    sc_blc(sc_core::sc_module_name name,
           const blc_config& cfg,
           cfa_types bayer,
           std::uint8_t bit_depth,
           std::uint32_t width,
           std::uint32_t height,
           const hw_params* hw = nullptr)
        : sc_module(name)
        , m_cfg(cfg)
        , m_bayer(bayer)
        , m_bit_depth(bit_depth)
        , m_width(width)
        , m_height(height)
        , m_hw(hw)
        , m_cycles_per_pixel(1)
        , m_metrics("blc") {
        SC_THREAD(process_stream);
    }

    // Block-level metrics collector
    sc_block_metrics<std::uint16_t> m_metrics;

    // Hardware configuration
    void set_hw_params(const hw_params* hw) { m_hw = hw; }
    void set_cycles_per_pixel(int cycles) { m_cycles_per_pixel = cycles; }

    // Architecture metrics (Phase 3)
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

    /**
     * @brief Compute per-pixel BLC offset based on Bayer position
     */
    std::uint16_t get_channel_offset(std::uint32_t row, std::uint32_t col) const;

    /**
     * @brief Compute per-pixel saturation range
     */
    std::uint16_t get_saturation_range(std::uint32_t row, std::uint32_t col) const;

    // Process a single pixel - pure functional, no timing
    std::uint16_t process_pixel(std::uint16_t pixel, std::uint32_t row, std::uint32_t col);

    const blc_config& m_cfg;
    cfa_types m_bayer;
    std::uint8_t m_bit_depth;
    std::uint32_t m_width;
    std::uint32_t m_height;

    // Hardware parameters (optional)
    const hw_params* m_hw;
    int m_cycles_per_pixel;

    // Architecture metrics (Phase 3)
    std::uint64_t m_active_cycles = 0;
    std::uint64_t m_starved_cycles = 0;
    std::uint64_t m_cycle_count = 0;

    // Pixel position for coordinate-based processing
    std::uint32_t m_row = 0;
    std::uint32_t m_col = 0;
};

#endif  // SC_BLC_H
