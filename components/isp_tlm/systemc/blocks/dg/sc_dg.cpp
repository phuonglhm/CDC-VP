/**
 * @file sc_dg.cpp
 * @brief Implementation of sc_dg SystemC module
 *
 * Hardware shell pattern (Phase 3):
 *   - process_pixel(): Pure functional kernel
 *   - process_stream(): Hardware shell with timing
 */
#include "sc_dg.h"

std::uint16_t sc_dg::process_pixel(std::uint16_t pixel) {
    std::uint32_t bit_range = (1u << m_bit_depth) - 1;

    std::uint16_t gain_idx = m_cfg.current_gain;
    gain_idx = (gain_idx >= kGainArraySize) ? kGainArraySize - 1 : gain_idx;
    float gain_multiplier = kGainArray[gain_idx];

    float val = static_cast<float>(pixel) * gain_multiplier;
    return static_cast<std::uint16_t>(std::clamp(val, 0.0f, static_cast<float>(bit_range)));
}

void sc_dg::process_stream() {
    m_metrics.set_processing_unit(sc_block_metrics<std::uint16_t>::ProcessingUnit::PIXEL);
    m_metrics.set_cycles_per_pixel(1);

    bool timed_mode = (m_hw != nullptr) && m_hw->timed_mode;
    bool has_clock = (clk != nullptr);
    if (timed_mode) {
        m_metrics.set_cycles_per_pixel(m_hw->default_cycles_per_pixel);
        m_cycles_per_pixel = m_hw->default_cycles_per_pixel;
    }

    while (true) {
        if (timed_mode) {
            while (fifo_in->num_available() == 0) {
                ++m_starved_cycles;
                ++m_cycle_count;
                if (has_clock) {
                    wait(clk->posedge_event());
                } else {
                    wait();
                }
            }
            if (has_clock) {
                wait(clk->posedge_event());
            } else {
                wait();
            }
        }

        std::uint16_t pixel = fifo_in->read();
        m_metrics.begin_processing();

        std::uint16_t out;
        if (!m_cfg.is_enable) {
            out = pixel;
        } else {
            out = process_pixel(pixel);
        }

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

        if (timed_mode) {
            while (fifo_out->num_free() == 0) {
                ++m_cycle_count;
                if (has_clock) {
                    wait(clk->posedge_event());
                } else {
                    wait();
                }
            }
        }

        fifo_out->write(out);
        m_metrics.end_processing();
        m_metrics.record_output();
    }
}
