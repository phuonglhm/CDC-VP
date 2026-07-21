/**
 * @file sc_wb.cpp
 * @brief Implementation of sc_wb SystemC module
 *
 * Hardware shell pattern (Phase 3):
 *   - process_pixel_triplet(): Pure functional kernel
 *   - process_stream(): Hardware shell with timing
 */
#include "sc_wb.h"
#include "../awb/sc_awb.h"

namespace {
    std::uint16_t scale_and_clip(std::uint16_t value, float gain, std::uint16_t max_value) {
        const float scaled = static_cast<float>(value) * gain;
        if (scaled <= 0.0f) return 0;
        if (scaled >= static_cast<float>(max_value)) return max_value;
        return static_cast<std::uint16_t>(std::lround(scaled));
    }
}

void sc_wb::process_pixel_triplet(std::uint16_t r_in, std::uint16_t g_in, std::uint16_t b_in,
                                  std::uint16_t& r_out, std::uint16_t& g_out, std::uint16_t& b_out) {
    if (!m_cfg.is_enable) {
        r_out = r_in;
        g_out = g_in;
        b_out = b_in;
    } else {
        // Combine static WB gain with AWB-computed latched gains
        float r_gain = m_cfg.r_gain;
        float b_gain = m_cfg.b_gain;
        if (m_awb != nullptr) {
            r_gain *= get_latched_r_gain();
            b_gain *= get_latched_b_gain();
        }
        r_out = scale_and_clip(r_in, r_gain, 4095);
        g_out = g_in;  // Green channel unchanged
        b_out = scale_and_clip(b_in, b_gain, 4095);
    }
}

void sc_wb::process_stream() {
    
    

    // Check if timed mode is enabled
    bool timed_mode = (m_hw != nullptr) && m_hw->timed_mode;
    bool has_clock = (clk != nullptr);
    if (timed_mode) {
        
        m_cycles_per_pixel = m_hw->default_cycles_per_pixel;
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
        

        std::uint16_t r_out, g_out, b_out;
        process_pixel_triplet(r, g, b, r_out, g_out, b_out);

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

        
        
    }
}
