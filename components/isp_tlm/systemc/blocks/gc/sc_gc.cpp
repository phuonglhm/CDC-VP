/**
 * @file sc_gc.cpp
 * @brief Implementation of sc_gc SystemC module
 */
#include "sc_gc.h"

int shift_amount(std::uint8_t bit_depth) {
    return static_cast<int>(gc_lut::GAMMA_LUT_BIT_DEPTH) - static_cast<int>(bit_depth);
}

void sc_gc::process_stream() {
    m_lut = gc_lut::get_lut();
    m_lut_size = gc_lut::get_lut_size();

    const std::uint8_t bd = (m_cfg.bit_depth == 0u || m_cfg.bit_depth > 16u) ? 12u : m_cfg.bit_depth;
    const int shift = shift_amount(bd);
    const std::uint16_t max_lut_idx = static_cast<std::uint16_t>(m_lut_size - 1u);

    while (true) {
        // Read RGB triplet
        std::uint16_t r = fifo_in->read();
        std::uint16_t g = fifo_in->read();
        std::uint16_t b = fifo_in->read();

        if (!m_cfg.is_enable) {
            fifo_out->write(r);
            fifo_out->write(g);
            fifo_out->write(b);
        } else {
            // LUT lookup with bit depth adjustment
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

            fifo_out->write(lookup(r));
            fifo_out->write(lookup(g));
            fifo_out->write(lookup(b));
        }
    }
}
