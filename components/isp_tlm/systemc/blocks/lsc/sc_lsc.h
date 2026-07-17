/**
 * @file sc_lsc.h
 * @brief SystemC Streaming Module for Lens Shading Correction (LSC)
 *
 * Implements lens shading correction using bilinear interpolation over a gain grid.
 * Uses internal line buffers for streaming operation.
 *
 * Hardware shell features (Phase 3):
 *   - Optional hw_params for timing
 *   - Configurable cycles per pixel
 *   - Architecture metrics
 */
#ifndef SC_LSC_H
#define SC_LSC_H

#include <systemc>
using namespace sc_core;

#include <systemc>
#include <cstdint>
#include <vector>
#include <deque>

#include "../../../blocks/lsc/include/lsc.h"
#include "../../../blocks/blc/include/blc.h"
#include "../../tb_utils/sc_block_metrics.h"
#include "../../hw/isp_arch_config.h"

SC_MODULE(sc_lsc) {
public:
    sc_core::sc_port<sc_fifo_in_if<std::uint16_t>> fifo_in;
    sc_core::sc_port<sc_fifo_out_if<std::uint16_t>> fifo_out;

    // Optional clock port for timed mode (pointer for optional binding)
    sc_in<bool>* clk = nullptr;

    SC_HAS_PROCESS(sc_lsc);

    /**
     * @brief Constructor
     * @param name Module name
     * @param cfg LSC configuration
     * @param lsc_lut Pointer to LSC LUT data
     * @param bayer Bayer CFA pattern
     * @param bit_depth Working bit depth
     * @param width Image width
     * @param height Image height
     * @param hw Optional hardware parameters for timed mode
     */
    sc_lsc(sc_core::sc_module_name name,
           const lsc_config& cfg,
           const std::vector<float>& lsc_lut,
           cfa_types bayer,
           std::uint8_t bit_depth,
           std::uint32_t width,
           std::uint32_t height,
           const hw_params* hw = nullptr)
        : sc_module(name), m_cfg(cfg), m_lsc_lut(lsc_lut), m_bayer(bayer),
          m_bit_depth(bit_depth), m_width(width), m_height(height),
          m_hw(hw), m_cycles_per_pixel(1), m_metrics("lsc") {
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

    // Process single pixel - pure functional
    std::uint16_t process_pixel(std::uint16_t pixel);

    /**
     * @brief Get LSC gain for a specific pixel position and channel
     */
    float get_lsc_gain(std::uint32_t row, std::uint32_t col, bayer_channel channel);

    /**
     * @brief Determine Bayer channel at given position
     */
    bayer_channel get_bayer_channel(std::uint32_t row, std::uint32_t col);

    const lsc_config& m_cfg;
    const std::vector<float>& m_lsc_lut;
    cfa_types m_bayer;
    std::uint8_t m_bit_depth;
    std::uint32_t m_width;
    std::uint32_t m_height;

    std::uint32_t m_row = 0;
    std::uint32_t m_col = 0;

    // Hardware parameters
    const hw_params* m_hw;
    int m_cycles_per_pixel;

    // Architecture metrics
    std::uint64_t m_active_cycles = 0;
    std::uint64_t m_starved_cycles = 0;
    std::uint64_t m_cycle_count = 0;
};

#endif  // SC_LSC_H
