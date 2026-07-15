/**
 * @file sc_csc.cpp
 * @brief Implementation of sc_csc SystemC module
 */
#include "sc_csc.h"

std::uint8_t sc_csc::clip_to_uint8(std::int32_t value) {
    if (value < 0) return 0;
    if (value > 255) return 255;
    return static_cast<std::uint8_t>(value);
}

void sc_csc::process_stream() {
    while (true) {
        // Read RGB triplet (12-bit values)
        std::uint16_t r_raw = fifo_in->read();
        std::uint16_t g_raw = fifo_in->read();
        std::uint16_t b_raw = fifo_in->read();

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

        std::uint8_t y = clip_to_uint8(y_raw);
        std::uint8_t u = clip_to_uint8(u_raw + 128);
        std::uint8_t v = clip_to_uint8(v_raw + 128);

        fifo_out->write(y);
        fifo_out->write(u);
        fifo_out->write(v);
    }
}
