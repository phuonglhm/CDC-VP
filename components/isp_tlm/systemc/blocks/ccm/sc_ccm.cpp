/**
 * @file sc_ccm.cpp
 * @brief Implementation of sc_ccm SystemC module
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

void sc_ccm::process_stream() {
    while (true) {
        // Read RGB triplet
        std::uint16_t r = fifo_in->read();
        std::uint16_t g = fifo_in->read();
        std::uint16_t b = fifo_in->read();

        if (!m_cfg.is_enable) {
            fifo_out->write(r);
            fifo_out->write(g);
            fifo_out->write(b);
        } else {
            const std::uint16_t max_value = max_for_bit_depth(m_cfg.bit_depth);
            const float inv_max = 1.0f / static_cast<float>(max_value);

            const float rgb[3] = {
                static_cast<float>(std::min(r, max_value)) * inv_max,
                static_cast<float>(std::min(g, max_value)) * inv_max,
                static_cast<float>(std::min(b, max_value)) * inv_max,
            };

            std::uint16_t r_out = quantize(
                m_cfg.corrected_red[0] * rgb[0] +
                m_cfg.corrected_red[1] * rgb[1] +
                m_cfg.corrected_red[2] * rgb[2], max_value);

            std::uint16_t g_out = quantize(
                m_cfg.corrected_green[0] * rgb[0] +
                m_cfg.corrected_green[1] * rgb[1] +
                m_cfg.corrected_green[2] * rgb[2], max_value);

            std::uint16_t b_out = quantize(
                m_cfg.corrected_blue[0] * rgb[0] +
                m_cfg.corrected_blue[1] * rgb[1] +
                m_cfg.corrected_blue[2] * rgb[2], max_value);

            fifo_out->write(r_out);
            fifo_out->write(g_out);
            fifo_out->write(b_out);
        }
    }
}
