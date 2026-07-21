/**
 * @file sc_blc.cpp
 * @brief Implementation of sc_blc SystemC module
 *
 * Black Level Correction (BLC) - Pixel-wise processing.
 *
 * Hardware shell features:
 *   - Optional clock binding via hw_params
 *   - In timed_mode: uses clk->posedge_event() for proper timing
 *   - In untimed mode: no wait() needed, fifo operations block naturally
 */
#include "sc_blc.h"

#include <cstdint>

namespace {

inline std::uint16_t apply_blc(std::uint16_t pixel,
                                std::uint16_t offset,
                                std::uint16_t sat) {
    std::int32_t v = static_cast<std::int32_t>(pixel) - static_cast<std::int32_t>(offset);
    if (v < 0) v = 0;
    if (v > static_cast<std::int32_t>(sat)) v = static_cast<std::int32_t>(sat);
    return static_cast<std::uint16_t>(v);
}

}  // anonymous namespace

void sc_blc::process_stream() {
    
    

    // Check if timed mode is enabled
    bool timed_mode = (m_hw != nullptr) && m_hw->timed_mode;
    // Use clock if available, otherwise use simple wait()
    bool has_clock = (clk != nullptr);

    if (timed_mode) {
        
        m_cycles_per_pixel = m_hw->default_cycles_per_pixel;
    }

    while (true) {
        // In timed mode: wait for clock edge or just advance time
        if (timed_mode) {
            if (has_clock) {
                wait(clk->posedge_event());
            } else {
                wait();  // Just advance simulation time
            }
        }

        // Read pixel - blocks until data available
        std::uint16_t pixel = fifo_in->read();
        

        // Update row/col for bayer pattern
        if (++m_col >= m_width) {
            m_col = 0;
            ++m_row;
        }

        std::uint16_t out;
        if (!m_cfg.is_enable) {
            out = pixel;
        } else {
            out = process_pixel(pixel, m_row, m_col);
        }

        // In timed mode: add processing delay
        if (timed_mode) {
            for (int i = 0; i < m_cycles_per_pixel; ++i) {
                if (has_clock) {
                    wait(clk->posedge_event());
                } else {
                    wait();
                }
                ++m_cycle_count;
                ++m_active_cycles;
            }
        } else {
            ++m_cycle_count;
            ++m_active_cycles;
        }

        fifo_out->write(out);
        
        
    }
}

std::uint16_t sc_blc::get_channel_offset(std::uint32_t row, std::uint32_t col) const {
    bool is_even_row = (row & 1) == 0;
    bool is_even_col = (col & 1) == 0;

    switch (m_bayer) {
    case cfa_types::BGGR:
        if (is_even_row) return is_even_col ? m_cfg.b_offset : m_cfg.gb_offset;
        else return is_even_col ? m_cfg.r_offset : m_cfg.gr_offset;

    case cfa_types::GBRG:
        if (is_even_row) return is_even_col ? m_cfg.gb_offset : m_cfg.b_offset;
        else return is_even_col ? m_cfg.r_offset : m_cfg.gr_offset;

    case cfa_types::GRBG:
        if (is_even_row) return is_even_col ? m_cfg.gr_offset : m_cfg.r_offset;
        else return is_even_col ? m_cfg.gb_offset : m_cfg.b_offset;

    case cfa_types::RGGB:
    default:
        if (is_even_row) return is_even_col ? m_cfg.r_offset : m_cfg.gr_offset;
        else return is_even_col ? m_cfg.gb_offset : m_cfg.b_offset;
    }
}

std::uint16_t sc_blc::process_pixel(std::uint16_t raw, std::uint32_t row, std::uint32_t col) {
    std::uint16_t offset = get_channel_offset(row, col);
    std::uint16_t sat = get_channel_saturation(row, col);
    return apply_blc(raw, offset, sat);
}

std::uint16_t sc_blc::get_channel_saturation(std::uint32_t row, std::uint32_t col) const {
    bool is_even_row = (row & 1) == 0;
    bool is_even_col = (col & 1) == 0;

    switch (m_bayer) {
    case cfa_types::BGGR:
        if (is_even_row) return is_even_col ? m_cfg.b_sat : m_cfg.gb_sat;
        else return is_even_col ? m_cfg.r_sat : m_cfg.gr_sat;

    case cfa_types::GBRG:
        if (is_even_row) return is_even_col ? m_cfg.gb_sat : m_cfg.b_sat;
        else return is_even_col ? m_cfg.r_sat : m_cfg.gr_sat;

    case cfa_types::GRBG:
        if (is_even_row) return is_even_col ? m_cfg.gr_sat : m_cfg.r_sat;
        else return is_even_col ? m_cfg.gb_sat : m_cfg.b_sat;

    case cfa_types::RGGB:
    default:
        if (is_even_row) return is_even_col ? m_cfg.r_sat : m_cfg.gr_sat;
        else return is_even_col ? m_cfg.gb_sat : m_cfg.b_sat;
    }
}
