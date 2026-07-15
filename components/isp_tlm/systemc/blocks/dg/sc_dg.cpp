/**
 * @file sc_dg.cpp
 * @brief Implementation of sc_dg SystemC module
 */
#include "sc_dg.h"

void sc_dg::process_stream() {
    std::uint32_t bit_range = (1u << m_bit_depth) - 1;

    while (true) {
        std::uint16_t pixel = fifo_in->read();
        std::uint16_t output;

        if (!m_cfg.is_enable) {
            output = pixel;
        } else {
            std::uint16_t gain_idx = m_cfg.current_gain;
            gain_idx = (gain_idx >= kGainArraySize) ? kGainArraySize - 1 : gain_idx;
            float gain_multiplier = kGainArray[gain_idx];

            float val = static_cast<float>(pixel) * gain_multiplier;
            output = static_cast<std::uint16_t>(std::clamp(val, 0.0f, static_cast<float>(bit_range)));
        }

        fifo_out->write(output);
    }
}
