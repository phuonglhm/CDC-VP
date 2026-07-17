/**
 * @file sc_gc.cpp
 * @brief Implementation of sc_gc SystemC module
 *
 * Hardware shell pattern (Phase 3):
 *   - process_rgb_triplet(): Pure functional kernel
 *   - process_stream(): Hardware shell with timing
 */
#include "sc_gc.h"

int shift_amount(std::uint8_t bit_depth) {
    return static_cast<int>(gc_lut::GAMMA_LUT_BIT_DEPTH) - static_cast<int>(bit_depth);
}

void sc_gc::process_rgb_triplet(std::uint16_t r, std::uint16_t g, std::uint16_t b,
                               std::uint16_t& r_out, std::uint16_t& g_out, std::uint16_t& b_out) {
    if (!m_cfg.is_enable) {
        r_out = r;
        g_out = g;
        b_out = b;
        return;
    }

    const std::uint8_t bd = (m_cfg.bit_depth == 0u || m_cfg.bit_depth > 16u) ? 12u : m_cfg.bit_depth;
    const int shift = shift_amount(bd);
    const std::uint16_t max_lut_idx = static_cast<std::uint16_t>(m_lut_size - 1u);

    auto lookup = [&](std::uint16_t val) -> std::uint16_t {
        std::uint32_t idx;
        if (shift > 0) {
            idx = static_cast<std::uint32_t>(val) << shift;
        } else if (shift < 0) {
            idx = static_cast<std::uint32_t>(val) >> (-shift);
        } else {
            idx = val;
        }
        if (idx > max_lut_idx) idx = max_lut_idx;
        return m_lut[idx];
    };

    r_out = lookup(r);
    g_out = lookup(g);
    b_out = lookup(b);
}

void sc_gc::process_stream() {
    m_lut = gc_lut::get_lut();
    m_lut_size = gc_lut::get_lut_size();

    m_metrics.set_processing_unit(sc_block_metrics<std::uint16_t>::ProcessingUnit::PIXEL);
    m_metrics.set_cycles_per_pixel(2);

    // Check if timed mode is enabled
    bool timed_mode = (m_hw != nullptr) && m_hw->timed_mode;
    bool has_clock = (clk != nullptr);
    if (timed_mode) {
        m_cycles_per_pixel = m_hw->default_cycles_per_pixel;
    }

    while (true) {
        // Read RGB triplet
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
        m_metrics.begin_processing();
        std::uint16_t g = fifo_in->read();
        std::uint16_t b = fifo_in->read();

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
        m_metrics.record_output();
        fifo_out->write(g_out);
        m_metrics.record_output();
        fifo_out->write(b_out);
        m_metrics.end_processing();
        m_metrics.record_output();
    }
}
