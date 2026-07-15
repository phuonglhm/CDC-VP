/**
 * @file sc_yuv420.cpp
 * @brief Implementation of sc_yuv420 SystemC module
 */
#include "sc_yuv420.h"

void sc_yuv420::process_stream() {
    if (!m_cfg.is_enable) {
        // Bypass mode: output YUV444 (interleaved)
        const std::size_t pixels = static_cast<std::size_t>(m_width) * m_height;
        for (std::size_t i = 0; i < pixels * 3; ++i) {
            fifo_out->write(fifo_in->read());
        }
        return;
    }

    const std::uint32_t half_width = (m_width + 1) / 2;
    const std::uint32_t half_height = (m_height + 1) / 2;
    const std::size_t pixels = static_cast<std::size_t>(m_width) * m_height;

    // Read entire interleaved YUV444 frame (3 bytes per pixel)
    std::vector<std::uint8_t> yuv444(pixels * 3u);
    for (std::size_t i = 0; i < yuv444.size(); ++i) {
        yuv444[i] = fifo_in->read();
    }

    // Output Y plane (full resolution)
    for (std::uint32_t y = 0; y < m_height; ++y) {
        for (std::uint32_t x = 0; x < m_width; ++x) {
            const std::size_t src_idx = (static_cast<std::size_t>(y) * m_width + x) * 3u;
            fifo_out->write(yuv444[src_idx]);  // Y
        }
    }

    // Output U/V plane (subsampled by 2x2 averaging)
    for (std::uint32_t y = 0; y < half_height; ++y) {
        for (std::uint32_t x = 0; x < half_width; ++x) {
            const std::uint32_t src_y = y * 2;
            const std::uint32_t src_x = x * 2;

            std::uint32_t sum_u = 0;
            std::uint32_t sum_v = 0;
            std::uint32_t count = 0;

            for (std::uint32_t dy = 0; dy < 2 && src_y + dy < m_height; ++dy) {
                for (std::uint32_t dx = 0; dx < 2 && src_x + dx < m_width; ++dx) {
                    const std::size_t idx = (static_cast<std::size_t>(src_y + dy) * m_width +
                                              (src_x + dx)) * 3u;
                    sum_u += yuv444[idx + 1u];
                    sum_v += yuv444[idx + 2u];
                    ++count;
                }
            }

            fifo_out->write(static_cast<std::uint8_t>(sum_u / count));
            fifo_out->write(static_cast<std::uint8_t>(sum_v / count));
        }
    }
}
