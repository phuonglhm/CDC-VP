/**
 * @file sc_wb.cpp
 * @brief Implementation of sc_wb SystemC module
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

void sc_wb::process_stream() {
    while (true) {
        // Read RGB triplet
        std::uint16_t r = fifo_in->read();
        std::uint16_t g = fifo_in->read();
        std::uint16_t b = fifo_in->read();

        std::uint16_t r_out, g_out, b_out;

        if (!m_cfg.is_enable) {
            r_out = r;
            g_out = g;
            b_out = b;
        } else {
            // Combine the static WB gain (from tuning) with the dynamic
            // AWB-computed gain (if AWB is wired up). AWB finishes its
            // statistics after reading the entire frame, so the WB applies
            // those gains with a one-frame latency on the first frame;
            // from the second frame onward the streaming output matches
            // what a single-pass C++ reference would produce.
            float r_gain = m_cfg.r_gain;
            float b_gain = m_cfg.b_gain;
            if (m_awb != nullptr) {
                r_gain *= m_awb->get_r_gain();
                b_gain *= m_awb->get_b_gain();
            }
            r_out = scale_and_clip(r, r_gain, 4095);
            g_out = g;  // Green channel unchanged
            b_out = scale_and_clip(b, b_gain, 4095);
        }

        fifo_out->write(r_out);
        fifo_out->write(g_out);
        fifo_out->write(b_out);
    }
}
