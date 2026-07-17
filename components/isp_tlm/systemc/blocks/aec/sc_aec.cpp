/**
 * @file sc_aec.cpp
 * @brief Implementation of sc_aec SystemC module
 */
#include "sc_aec.h"
#include <cmath>

constexpr std::uint8_t BIT_RANGE_8 = 255;

void sc_aec::process_stream() {
    // Check if timed mode is enabled
    bool timed_mode = (m_hw != nullptr) && m_hw->timed_mode;
    bool has_clock = (clk != nullptr);
    if (timed_mode) {
        m_metrics.set_cycles_per_pixel(m_hw->default_cycles_per_pixel);
    }

    while (true) {
        double m2 = 0.0;
        double m3 = 0.0;
        double img_size = static_cast<double>(m_width) * m_height;

        int shift = (m_bit_depth > 8) ? m_bit_depth - 8 : 0;

        const std::size_t pixels = static_cast<std::size_t>(m_width) * m_height;

        for (std::size_t i = 0; i < pixels; ++i) {
            if (timed_mode) {
                if (fifo_in->num_available() < 3) {
                    ++m_starved_cycles;
                    ++m_cycle_count;
                    if (has_clock) {
                        wait(clk->posedge_event());
                    } else {
                        wait();
                    }
                    --i;  // Retry this iteration
                    continue;
                }
            }

            std::uint16_t r = fifo_in->read();
            std::uint16_t g = fifo_in->read();
            std::uint16_t b = fifo_in->read();

            if (timed_mode) {
                ++m_active_cycles;
                ++m_cycle_count;
            }

            double r_val = static_cast<double>(r >> shift);
            double g_val = static_cast<double>(g >> shift);
            double b_val = static_cast<double>(b >> shift);
            double y = 0.299 * r_val + 0.587 * g_val + 0.144 * b_val;

            y = std::clamp(y, 0.0, static_cast<double>(BIT_RANGE_8));
            y -= m_cfg.center_illuminance;

            m2 += y * y;
            m3 += y * y * y;

            // Pass through
            if (timed_mode) {
                if (fifo_out->num_free() < 3) {
                    ++m_cycle_count;
                    if (has_clock) {
                        wait(clk->posedge_event());
                    } else {
                        wait();
                    }
                }
            }
            fifo_out->write(r);
            fifo_out->write(g);
            fifo_out->write(b);
            if (timed_mode) {
                ++m_cycle_count;
            }
        }

        // Synchronize at end of frame in timed mode
        if (timed_mode) {
            if (has_clock) {
                wait(clk->posedge_event());
            } else {
                wait();
            }
            ++m_cycle_count;
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
