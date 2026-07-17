/**
 * @file sc_ccm.cpp
 * @brief Implementation of sc_ccm SystemC module
 *
 * Hardware shell pattern (Phase 3):
 *   - process_rgb_triplet(): Pure functional kernel (matrix multiply)
 *   - process_stream(): Hardware shell with timing
 */
#include "sc_ccm.h"
#include <cmath>

std::uint16_t sc_ccm::max_for_bit_depth(std::uint8_t bit_depth) const {
    if (bit_depth == 0) bit_depth = 8;
    if (bit_depth >= 16) return 0xFFFFu;
    return static_cast<std::uint16_t>((1u << bit_depth) - 1u);
}

float sc_ccm::clamp01(float value) const {
    return std::max(0.0f, std::min(1.0f, value));
}

std::uint16_t sc_ccm::quantize(float value, std::uint16_t max_value) const {
    const float scaled = clamp01(value) * static_cast<float>(max_value);
    return static_cast<std::uint16_t>(std::lround(scaled));
}

void sc_ccm::process_rgb_triplet(std::uint16_t r_in, std::uint16_t g_in, std::uint16_t b_in,
                                 std::uint16_t& r_out, std::uint16_t& g_out, std::uint16_t& b_out) {
    if (!m_cfg.is_enable) {
        r_out = r_in;
        g_out = g_in;
        b_out = b_in;
        return;
    }

    const std::uint16_t max_value = max_for_bit_depth(m_cfg.bit_depth);
    const float inv_max = 1.0f / static_cast<float>(max_value);

    const float rgb[3] = {
        static_cast<float>(std::min(r_in, max_value)) * inv_max,
        static_cast<float>(std::min(g_in, max_value)) * inv_max,
        static_cast<float>(std::min(b_in, max_value)) * inv_max,
    };

    r_out = quantize(
        m_cfg.corrected_red[0] * rgb[0] +
        m_cfg.corrected_red[1] * rgb[1] +
        m_cfg.corrected_red[2] * rgb[2], max_value);

    g_out = quantize(
        m_cfg.corrected_green[0] * rgb[0] +
        m_cfg.corrected_green[1] * rgb[1] +
        m_cfg.corrected_green[2] * rgb[2], max_value);

    b_out = quantize(
        m_cfg.corrected_blue[0] * rgb[0] +
        m_cfg.corrected_blue[1] * rgb[1] +
        m_cfg.corrected_blue[2] * rgb[2], max_value);
}

void sc_ccm::process_stream() {
    m_metrics.set_processing_unit(sc_block_metrics<std::uint16_t>::ProcessingUnit::PIXEL);
    m_metrics.set_cycles_per_pixel(2);

    // Check if timed mode is enabled
    bool timed_mode = (m_hw != nullptr) && m_hw->timed_mode;
    bool has_clock = (clk != nullptr);
    if (timed_mode) {
        m_cycles_per_pixel = m_hw->default_cycles_per_pixel;
        m_metrics.set_cycles_per_pixel(m_cycles_per_pixel);
    }

    while (true) {
        // Read RGB triplet (need 3 tokens)
        if (fifo_in->num_available() < 3) {
            if (timed_mode) {
                ++m_starved_cycles;
                ++m_cycle_count;
                if (has_clock) {
                    wait(clk->posedge_event());
                } else {
                    wait();
                }
            } else {
                wait(fifo_in->data_written_event());
            }
            continue;
        }

        std::uint16_t r = fifo_in->read();
        std::uint16_t g = fifo_in->read();
        std::uint16_t b = fifo_in->read();
        m_metrics.begin_processing();

        // Process - pure functional kernel
        std::uint16_t r_out, g_out, b_out;
        process_rgb_triplet(r, g, b, r_out, g_out, b_out);

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

        // Check for output backpressure (need 3 tokens free)
        if (fifo_out->num_free() < 3) {
            if (timed_mode) {
                ++m_cycle_count;
                if (has_clock) {
                    wait(clk->posedge_event());
                } else {
                    wait();
                }
            } else {
                wait(fifo_out->data_read_event());
            }
        }

        fifo_out->write(r_out);
        fifo_out->write(g_out);
        fifo_out->write(b_out);

        m_metrics.end_processing();
        m_metrics.record_output();
    }
}
