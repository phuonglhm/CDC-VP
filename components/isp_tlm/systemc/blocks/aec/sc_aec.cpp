/**
 * @file sc_aec.cpp
 * @brief Implementation of sc_aec SystemC module
 */
#include "sc_aec.h"
#include <cmath>

constexpr std::uint8_t BIT_RANGE_8 = 255;

void sc_aec::process_stream() {
    while (true) {
        double m2 = 0.0;
        double m3 = 0.0;
        double img_size = static_cast<double>(m_width) * m_height;

        int shift = (m_bit_depth > 8) ? m_bit_depth - 8 : 0;

        const std::size_t pixels = static_cast<std::size_t>(m_width) * m_height;

        for (std::size_t i = 0; i < pixels; ++i) {
            std::uint16_t r = fifo_in->read();
            std::uint16_t g = fifo_in->read();
            std::uint16_t b = fifo_in->read();

            double r_val = static_cast<double>(r >> shift);
            double g_val = static_cast<double>(g >> shift);
            double b_val = static_cast<double>(b >> shift);
            double y = 0.299 * r_val + 0.587 * g_val + 0.144 * b_val;

            y = std::clamp(y, 0.0, static_cast<double>(BIT_RANGE_8));
            y -= m_cfg.center_illuminance;

            m2 += y * y;
            m3 += y * y * y;

            // Pass through
            fifo_out->write(r);
            fifo_out->write(g);
            fifo_out->write(b);
        }

        m2 /= img_size;
        m3 /= img_size;

        double skewness = 0.0;
        if (m2 > 1e-6) {
            skewness = m3 * std::sqrt(img_size * (img_size - 1)) / std::pow(m2, 1.5) / (img_size - 2);
        }

        if (!m_cfg.is_enable) {
            m_ae_feedback = 0;
        } else if (skewness < -m_cfg.histogram_skewness) {
            m_ae_feedback = -1;
        } else if (skewness > m_cfg.histogram_skewness) {
            m_ae_feedback = 1;
        } else {
            m_ae_feedback = 0;
        }

        std::cout << "[AEC] Computed feedback: " << m_ae_feedback << " (skewness=" << skewness << ")" << std::endl;
    }
}
