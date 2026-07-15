/**
 * @file sc_cse.cpp
 * @brief Implementation of sc_cse SystemC module
 */
#include "sc_cse.h"

constexpr std::int32_t CHROMA_OFFSET = 128;

std::uint8_t sc_cse::clip_to_uint8(std::int32_t value) {
    if (value < 0) return 0;
    if (value > 255) return 255;
    return static_cast<std::uint8_t>(value);
}

void sc_cse::process_stream() {
    while (true) {
        // Read YUV triplet
        std::uint8_t y = fifo_in->read();
        std::uint8_t u = fifo_in->read();
        std::uint8_t v = fifo_in->read();

        if (!m_cfg.is_enable) {
            fifo_out->write(y);
            fifo_out->write(u);
            fifo_out->write(v);
        } else {
            std::int32_t u_centered = static_cast<std::int32_t>(u) - CHROMA_OFFSET;
            std::int32_t v_centered = static_cast<std::int32_t>(v) - CHROMA_OFFSET;

            std::int32_t u_enhanced = static_cast<std::int32_t>(u_centered * m_cfg.saturation_gain);
            std::int32_t v_enhanced = static_cast<std::int32_t>(v_centered * m_cfg.saturation_gain);

            fifo_out->write(y);
            fifo_out->write(clip_to_uint8(u_enhanced + CHROMA_OFFSET));
            fifo_out->write(clip_to_uint8(v_enhanced + CHROMA_OFFSET));
        }
    }
}
