/**
 * @file sc_lsc.cpp
 * @brief Implementation of sc_lsc SystemC module
 *
 * Hardware shell pattern (Phase 3):
 *   - process_pixel(): Pure functional kernel
 *   - process_stream(): Hardware shell with timing
 */
#include "sc_lsc.h"
#include <cmath>

namespace {
    inline float bilinear_interpolate(float G00, float G10, float G01, float G11,
                                     float dx, float dy) {
        return (1.0f - dx) * (1.0f - dy) * G00 +
               dx * (1.0f - dy) * G10 +
               (1.0f - dx) * dy * G01 +
               dx * dy * G11;
    }
}

bayer_channel sc_lsc::get_bayer_channel(std::uint32_t row, std::uint32_t col) {
    bool is_even_row = (row & 1) == 0;
    bool is_even_col = (col & 1) == 0;

    switch (m_bayer) {
    case cfa_types::RGGB:
        return is_even_row ? (is_even_col ? bayer_channel::R : bayer_channel::GR)
                           : (is_even_col ? bayer_channel::GB : bayer_channel::B);
    case cfa_types::GRBG:
        return is_even_row ? (is_even_col ? bayer_channel::GR : bayer_channel::R)
                           : (is_even_col ? bayer_channel::B : bayer_channel::GB);
    case cfa_types::BGGR:
        return is_even_row ? (is_even_col ? bayer_channel::B : bayer_channel::GB)
                           : (is_even_col ? bayer_channel::GR : bayer_channel::R);
    case cfa_types::GBRG:
        return is_even_row ? (is_even_col ? bayer_channel::GB : bayer_channel::B)
                           : (is_even_col ? bayer_channel::R : bayer_channel::GR);
    }
    return bayer_channel::R;
}

float sc_lsc::get_lsc_gain(std::uint32_t row, std::uint32_t col, bayer_channel channel) {
    if (!m_cfg.is_enable || m_lsc_lut.empty()) {
        return 1.0f;
    }

    if (m_cfg.grid_width == 0 || m_cfg.grid_height == 0) {
        return 1.0f;
    }

    float box_w = static_cast<float>(m_width) / m_cfg.grid_width;
    float box_h = static_cast<float>(m_height) / m_cfg.grid_height;

    int box_idx = static_cast<int>(col / box_w);
    int box_idy = static_cast<int>(row / box_h);

    if (box_idx >= static_cast<int>(m_cfg.grid_width)) box_idx = m_cfg.grid_width - 1;
    if (box_idy >= static_cast<int>(m_cfg.grid_height)) box_idy = m_cfg.grid_height - 1;

    float dx = (static_cast<float>(col) - box_idx * box_w) / box_w;
    float dy = (static_cast<float>(row) - box_idy * box_h) / box_h;
    dx = std::clamp(dx, 0.0f, 1.0f);
    dy = std::clamp(dy, 0.0f, 1.0f);

    std::uint32_t nodes_per_channel = (m_cfg.grid_width + 1) * (m_cfg.grid_height + 1);
    std::uint32_t pitch = m_cfg.grid_width + 1;

    const float* grid = m_lsc_lut.data() + static_cast<std::size_t>(channel) * nodes_per_channel;

    float G00 = grid[box_idy * pitch + box_idx];
    float G10 = grid[box_idy * pitch + (box_idx + 1)];
    float G01 = grid[(box_idy + 1) * pitch + box_idx];
    float G11 = grid[(box_idy + 1) * pitch + (box_idx + 1)];

    return bilinear_interpolate(G00, G10, G01, G11, dx, dy);
}

std::uint16_t sc_lsc::process_pixel(std::uint16_t pixel) {
    if (!m_cfg.is_enable || m_lsc_lut.empty()) {
        return pixel;
    }

    bayer_channel channel = get_bayer_channel(m_row, m_col);
    float gain = get_lsc_gain(m_row, m_col, channel);

    std::uint32_t bit_range = (1u << m_bit_depth) - 1;
    float val = static_cast<float>(pixel) * gain;
    return static_cast<std::uint16_t>(
        std::clamp(val, 0.0f, static_cast<float>(bit_range)));
}

void sc_lsc::process_stream() {
    m_metrics.set_processing_unit(sc_block_metrics<std::uint16_t>::ProcessingUnit::PIXEL);
    m_metrics.set_cycles_per_pixel(1);

    // Check if timed mode is enabled
    bool timed_mode = (m_hw != nullptr) && m_hw->timed_mode;
    bool has_clock = (clk != nullptr);
    if (timed_mode) {
        m_cycles_per_pixel = m_hw->default_cycles_per_pixel;
    }

    while (true) {
        if (timed_mode) {
            if (has_clock) {
                wait(clk->posedge_event());
            } else {
                wait();
            }
        }

        std::uint16_t pixel = fifo_in->read();
        m_metrics.begin_processing();

        std::uint16_t output = process_pixel(pixel);

        // Hardware shell: timing
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

        fifo_out->write(output);
        m_metrics.end_processing();
        m_metrics.record_output();

        m_col++;
        if (m_col >= m_width) {
            m_col = 0;
            m_row++;
            if (m_row >= m_height) {
                m_row = 0;  // Frame complete - reset for next frame
            }
        }
    }
}
