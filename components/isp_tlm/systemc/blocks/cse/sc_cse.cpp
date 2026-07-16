/**
 * @file sc_cse.cpp
 * @brief Implementation of sc_cse SystemC module
 *
 * Hardware shell pattern (Phase 3):
 *   - process_yuv_triplet(): Pure functional kernel
 *   - process_stream(): Hardware shell with timing
 */
#include "sc_cse.h"

constexpr std::int32_t CHROMA_OFFSET = 128;

std::uint8_t sc_cse::clip_to_uint8(std::int32_t value) {
    if (value < 0) return 0;
    if (value > 255) return 255;
    return static_cast<std::uint8_t>(value);
}

void sc_cse::process_yuv_triplet(std::uint8_t y_in, std::uint8_t u_in, std::uint8_t v_in,
                               std::uint8_t& y_out, std::uint8_t& u_out, std::uint8_t& v_out) {
    if (!m_cfg.is_enable) {
        y_out = y_in;
        u_out = u_in;
        v_out = v_in;
        return;
    }

    std::int32_t u_centered = static_cast<std::int32_t>(u_in) - CHROMA_OFFSET;
    std::int32_t v_centered = static_cast<std::int32_t>(v_in) - CHROMA_OFFSET;

    std::int32_t u_enhanced = static_cast<std::int32_t>(u_centered * m_cfg.saturation_gain);
    std::int32_t v_enhanced = static_cast<std::int32_t>(v_centered * m_cfg.saturation_gain);

    y_out = y_in;
    u_out = clip_to_uint8(u_enhanced + CHROMA_OFFSET);
    v_out = clip_to_uint8(v_enhanced + CHROMA_OFFSET);
}

void sc_cse::process_stream() {
    m_metrics.set_processing_unit(sc_block_metrics<std::uint8_t>::ProcessingUnit::PIXEL);
    m_metrics.set_cycles_per_pixel(1);

    // Check if timed mode is enabled
    bool timed_mode = (m_hw != nullptr) && m_hw->timed_mode;
    if (timed_mode) {
        m_cycles_per_pixel = m_hw->default_cycles_per_pixel;
    }

    while (true) {
        // Read YUV triplet
        std::uint8_t y = fifo_in->read();
        m_metrics.begin_processing();
        std::uint8_t u = fifo_in->read();
        std::uint8_t v = fifo_in->read();

        std::uint8_t y_out, u_out, v_out;
        process_yuv_triplet(y, u, v, y_out, u_out, v_out);

        // Hardware shell: timing
        if (timed_mode) {
            for (int i = 0; i < m_cycles_per_pixel; ++i) {
                wait();
                ++m_cycle_count;
                ++m_active_cycles;
            }
        } else {
            ++m_cycle_count;
            ++m_active_cycles;
        }

        fifo_out->write(y_out);
        m_metrics.record_output();
        fifo_out->write(u_out);
        m_metrics.record_output();
        fifo_out->write(v_out);
        m_metrics.end_processing();
        m_metrics.record_output();
    }
}
