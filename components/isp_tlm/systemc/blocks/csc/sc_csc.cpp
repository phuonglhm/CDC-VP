/**
 * @file sc_csc.cpp
 * @brief Implementation of sc_csc SystemC module
 *
 * Hardware shell pattern (Phase 3):
 *   - process_rgb_triplet(): Pure functional kernel
 *   - process_stream(): Hardware shell with timing
 */
#include "sc_csc.h"

std::uint8_t sc_csc::clip_to_uint8(std::int32_t value) {
    if (value < 0) return 0;
    if (value > 255) return 255;
    return static_cast<std::uint8_t>(value);
}

void sc_csc::process_rgb_triplet(std::uint16_t r_raw, std::uint16_t g_raw, std::uint16_t b_raw,
                               std::uint8_t& y, std::uint8_t& u, std::uint8_t& v) {
    // Normalize 12-bit input to 8-bit range [0, 255]
    std::int32_t r = r_raw >> 4;
    std::int32_t g = g_raw >> 4;
    std::int32_t b = b_raw >> 4;

    std::int32_t y_raw, u_raw, v_raw;

    if (m_cfg.conv_standard == 1) {
        // BT.709
        y_raw = (54 * r + 183 * g + 18 * b) >> 8;
        u_raw = (-29 * r - 99 * g + 128 * b) >> 8;
        v_raw = (128 * r - 116 * g - 12 * b) >> 8;
    } else {
        // BT.601
        y_raw = (77 * r + 150 * g + 29 * b) >> 8;
        u_raw = (-43 * r - 84 * g + 128 * b) >> 8;
        v_raw = (128 * r - 107 * g - 21 * b) >> 8;
    }

    y = clip_to_uint8(y_raw);
    u = clip_to_uint8(u_raw + 128);
    v = clip_to_uint8(v_raw + 128);
}

void sc_csc::process_stream() {
    
    

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

        std::uint16_t r_raw = fifo_in->read();
        
        std::uint16_t g_raw = fifo_in->read();
        std::uint16_t b_raw = fifo_in->read();

        std::uint8_t y, u, v;
        process_rgb_triplet(r_raw, g_raw, b_raw, y, u, v);

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

        fifo_out->write(y);
        
        fifo_out->write(u);
        
        fifo_out->write(v);
        
        
    }
}
