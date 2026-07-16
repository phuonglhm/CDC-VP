/**
 * @file sc_blc.cpp
 * @brief Implementation of sc_blc SystemC module
 *
 * Hardware shell pattern (Phase 3):
 *   - process_pixel(): Pure functional kernel - unchanged algorithm
 *   - process_stream(): Hardware shell - timing, buffering, control
 *
 * When m_hw->timed_mode == true, adds wait() cycles for real timing.
 * When m_hw == nullptr or timed_mode == false, behaves as before (untimed).
 */
#include "sc_blc.h"

std::uint16_t sc_blc::process_pixel(std::uint16_t pixel, std::uint32_t row, std::uint32_t col) {
    std::uint32_t bit_range = (1u << m_bit_depth) - 1;

    std::uint16_t offset = get_channel_offset(row, col);
    std::uint16_t sat_diff = get_saturation_range(row, col);

    int32_t val = static_cast<int32_t>(pixel) - static_cast<int32_t>(offset);
    if (val < 0) val = 0;

    if (m_cfg.is_linear) {
        val = static_cast<int32_t>(
            static_cast<double>(val) / sat_diff * bit_range);
    }

    if (val > static_cast<int32_t>(bit_range))
        val = bit_range;

    return static_cast<std::uint16_t>(val);
}

void sc_blc::process_stream() {
    m_metrics.set_processing_unit(sc_block_metrics<std::uint16_t>::ProcessingUnit::PIXEL);
    m_metrics.set_cycles_per_pixel(1);

    // Check if timed mode is enabled
    bool timed_mode = (m_hw != nullptr) && m_hw->timed_mode;
    if (timed_mode) {
        m_metrics.set_cycles_per_pixel(m_hw->default_cycles_per_pixel);
        m_cycles_per_pixel = m_hw->default_cycles_per_pixel;
    }

    while (true) {
        // Check for input availability
        if (fifo_in->num_available() == 0) {
            // Input starved - count cycles if timed mode
            if (timed_mode) {
                ++m_starved_cycles;
                ++m_cycle_count;
            }
            wait();  // Wait for data
            continue;
        }

        std::uint16_t pixel = fifo_in->read();
        m_metrics.begin_processing();

        std::uint16_t out;

        if (!m_cfg.is_enable) {
            // Bypass mode: pass through unchanged
            out = pixel;
        } else {
            // Process with BLC - pure functional kernel
            out = process_pixel(pixel, m_row, m_col);
        }

        // Update position
        m_col++;
        if (m_col >= m_width) {
            m_col = 0;
            m_row++;
            if (m_row >= m_height) {
                m_row = 0;  // Frame complete - reset for next frame
            }
        }

        // Hardware shell: timing
        if (timed_mode) {
            // Add wait cycles for processing
            for (int i = 0; i < m_cycles_per_pixel; ++i) {
                wait();  // Wait for clock edge
                ++m_cycle_count;
                ++m_active_cycles;
            }
        } else {
            // Untimed mode - count as active
            ++m_cycle_count;
            ++m_active_cycles;
        }

        // Check for output backpressure
        if (fifo_out->num_free() == 0) {
            // Output blocked - wait for space
            if (timed_mode) {
                ++m_cycle_count;
            }
            wait();  // Wait for FIFO space
        }

        fifo_out->write(out);
        m_metrics.end_processing();
        m_metrics.record_output();
    }
}

std::uint16_t sc_blc::get_channel_offset(std::uint32_t row, std::uint32_t col) const {
    bool is_even_row = (row & 1) == 0;
    bool is_even_col = (col & 1) == 0;

    switch (m_bayer) {
    case cfa_types::BGGR:
        if (is_even_row) return is_even_col ? m_cfg.b_offset : m_cfg.gb_offset;
        else return is_even_col ? m_cfg.gr_offset : m_cfg.r_offset;

    case cfa_types::GBRG:
        if (is_even_row) return is_even_col ? m_cfg.gb_offset : m_cfg.b_offset;
        else return is_even_col ? m_cfg.r_offset : m_cfg.gr_offset;

    case cfa_types::GRBG:
        if (is_even_row) return is_even_col ? m_cfg.gr_offset : m_cfg.r_offset;
        else return is_even_col ? m_cfg.b_offset : m_cfg.gb_offset;

    case cfa_types::RGGB:
    default:
        if (is_even_row) return is_even_col ? m_cfg.r_offset : m_cfg.gr_offset;
        else return is_even_col ? m_cfg.gb_offset : m_cfg.b_offset;
    }
}

std::uint16_t sc_blc::get_saturation_range(std::uint32_t row, std::uint32_t col) const {
    bool is_even_row = (row & 1) == 0;
    bool is_even_col = (col & 1) == 0;

    std::uint16_t b_sat_diff = (m_cfg.b_sat > m_cfg.b_offset) ?
                               (m_cfg.b_sat - m_cfg.b_offset) : 1;
    std::uint16_t gb_sat_diff = (m_cfg.gb_sat > m_cfg.gb_offset) ?
                                (m_cfg.gb_sat - m_cfg.gb_offset) : 1;
    std::uint16_t r_sat_diff = (m_cfg.r_sat > m_cfg.r_offset) ?
                               (m_cfg.r_sat - m_cfg.r_offset) : 1;
    std::uint16_t gr_sat_diff = (m_cfg.gr_sat > m_cfg.gr_offset) ?
                                (m_cfg.gr_sat - m_cfg.gr_offset) : 1;

    switch (m_bayer) {
    case cfa_types::BGGR:
        if (is_even_row) return is_even_col ? b_sat_diff : gb_sat_diff;
        else return is_even_col ? gr_sat_diff : r_sat_diff;

    case cfa_types::GBRG:
        if (is_even_row) return is_even_col ? gb_sat_diff : b_sat_diff;
        else return is_even_col ? r_sat_diff : gr_sat_diff;

    case cfa_types::GRBG:
        if (is_even_row) return is_even_col ? gr_sat_diff : r_sat_diff;
        else return is_even_col ? b_sat_diff : gb_sat_diff;

    case cfa_types::RGGB:
    default:
        if (is_even_row) return is_even_col ? r_sat_diff : gr_sat_diff;
        else return is_even_col ? gb_sat_diff : b_sat_diff;
    }
}
