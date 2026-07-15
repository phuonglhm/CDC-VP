/**
 * @file sc_blc.cpp
 * @brief Implementation of sc_blc SystemC module
 */
#include "sc_blc.h"

void sc_blc::process_stream() {
    std::uint32_t bit_range = (1u << m_bit_depth) - 1;

    std::uint16_t b_sat_diff = (m_cfg.b_sat > m_cfg.b_offset) ?
                               (m_cfg.b_sat - m_cfg.b_offset) : 1;
    std::uint16_t gb_sat_diff = (m_cfg.gb_sat > m_cfg.gb_offset) ?
                                (m_cfg.gb_sat - m_cfg.gb_offset) : 1;
    std::uint16_t r_sat_diff = (m_cfg.r_sat > m_cfg.r_offset) ?
                               (m_cfg.r_sat - m_cfg.r_offset) : 1;
    std::uint16_t gr_sat_diff = (m_cfg.gr_sat > m_cfg.gr_offset) ?
                                (m_cfg.gr_sat - m_cfg.gr_offset) : 1;

    while (true) {
        if (!m_cfg.is_enable) {
            // Bypass mode: read and write unchanged
            std::uint16_t pixel = fifo_in->read();
            fifo_out->write(pixel);
        } else {
            // Process with BLC
            std::uint16_t pixel = fifo_in->read();

            bool is_even_row = (m_row & 1) == 0;
            bool is_even_col = (m_col & 1) == 0;

            std::uint16_t offset = get_channel_offset(m_row, m_col);
            std::uint16_t sat_diff = get_saturation_range(m_row, m_col);

            int32_t val = static_cast<int32_t>(pixel) - static_cast<int32_t>(offset);
            if (val < 0) val = 0;

            if (m_cfg.is_linear) {
                val = static_cast<int32_t>(
                    static_cast<double>(val) / sat_diff * bit_range);
            }

            if (val > static_cast<int32_t>(bit_range))
                val = bit_range;

            fifo_out->write(static_cast<std::uint16_t>(val));
        }

        // Update pixel position
        m_col++;
        if (m_col == 0) m_row++;  // Overflow case
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
