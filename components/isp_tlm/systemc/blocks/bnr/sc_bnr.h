/**
 * @file sc_bnr.h
 * @brief SystemC Streaming Module for Bayer Noise Reduction (BNR)
 *
 * Implements joint bilateral filtering for noise reduction on RAW Bayer data.
 * Uses internal line buffers and performs streaming processing with boundary padding.
 *
 * Hardware shell features (Phase 3):
 *   - Optional hw_params for timing
 *   - Architecture metrics
 */
#ifndef SC_BNR_H
#define SC_BNR_H

#include <systemc>
using namespace sc_core;

#include <systemc>
#include <cstdint>
#include <vector>
#include <deque>

#include "../../../blocks/bnr/include/bnr.h"
#include "../../tb_utils/hardware_params.h"

SC_MODULE(sc_bnr) {
public:
    sc_core::sc_port<sc_fifo_in_if<std::uint16_t>> fifo_in;
    sc_core::sc_port<sc_fifo_out_if<std::uint16_t>> fifo_out;

    // Optional clock port for timed mode (pointer for optional binding)
    sc_in<bool>* clk = nullptr;

    SC_HAS_PROCESS(sc_bnr);

    sc_bnr(sc_core::sc_module_name name,
           const bnr_config& cfg,
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
        , m_hw(hw) {
        SC_THREAD(process_stream);
    }


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

    void shift_buffers();
    std::uint16_t get_mirrored_pixel(int row, int col);

    const bnr_config& m_cfg;
    cfa_types m_bayer;
    std::uint8_t m_bit_depth;
    std::uint32_t m_width;
    std::uint32_t m_height;

    // Hardware parameters
    const hw_params* m_hw;

    // Architecture metrics
    std::uint64_t m_active_cycles = 0;
    std::uint64_t m_starved_cycles = 0;
    std::uint64_t m_cycle_count = 0;

    // Line buffer for window operation (3 rows: previous, current, next)
    static constexpr int BUFFER_ROWS = 3;
    static constexpr int HALF_WINDOW = 1;
    std::deque<std::uint16_t> m_line_buffer[BUFFER_ROWS];
    int m_buffer_row_idx = 0;

    // Normalized image buffer for bilateral filter
    std::vector<float> m_norm_image;
    std::vector<float> m_interp_green;

    // Statistics
    std::uint32_t m_row = 0;
    std::uint32_t m_col = 0;
};

#endif  // SC_BNR_H
