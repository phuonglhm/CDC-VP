/**
 * @file sc_2dnr.cpp
 * @brief Implementation of sc_2dnr SystemC module
 */
#include "sc_2dnr.h"
#include <algorithm>
#include <cmath>

namespace {
    constexpr std::size_t WEIGHT_LUT_SIZE = 256;

    std::vector<float> build_weight_lut(std::uint16_t wts) {
        std::vector<float> lut(WEIGHT_LUT_SIZE);
        if (wts == 0) {
            lut[0] = 1.0f;
            for (std::size_t i = 1; i < WEIGHT_LUT_SIZE; ++i)
                lut[i] = 0.0f;
            return lut;
        }
        for (std::size_t i = 0; i < WEIGHT_LUT_SIZE; ++i) {
            lut[i] = static_cast<float>(std::exp(-static_cast<double>(i) / static_cast<double>(wts)));
        }
        return lut;
    }

    std::vector<float> build_patch_gaussian_kernel(std::uint8_t patch_size) {
        const int radius = static_cast<int>(patch_size) / 2;
        const int size = static_cast<int>(patch_size);
        std::vector<float> kernel(static_cast<std::size_t>(size * size));
        float sigma = static_cast<float>(patch_size) / 6.0f;
        if (sigma < 0.5f) sigma = 0.5f;
        const float sigma_sq = sigma * sigma;
        float sum = 0.0f;
        for (int dy = -radius; dy <= radius; ++dy) {
            for (int dx = -radius; dx <= radius; ++dx) {
                float value = std::exp(-static_cast<float>(dx * dx + dy * dy) / (2.0f * sigma_sq));
                kernel[static_cast<std::size_t>((dy + radius) * size + (dx + radius))] = value;
                sum += value;
            }
        }
        for (float& k : kernel) k /= sum;
        return kernel;
    }

    float compute_patch_distance(const std::uint8_t* y_work, std::uint32_t width, std::uint32_t height,
                                int py, int px, int sy, int sx,
                                const std::vector<float>& patch_gaussian, int patch_radius) {
        float dist = 0.0f;
        const int size = patch_radius * 2 + 1;
        for (int dy = -patch_radius; dy <= patch_radius; ++dy) {
            int ry = py + dy;
            ry = std::max(0, std::min(static_cast<int>(height) - 1, ry));
            int ry2 = sy + dy;
            ry2 = std::max(0, std::min(static_cast<int>(height) - 1, ry2));
            for (int dx = -patch_radius; dx <= patch_radius; ++dx) {
                int rx = px + dx;
                rx = std::max(0, std::min(static_cast<int>(width) - 1, rx));
                int rx2 = sx + dx;
                rx2 = std::max(0, std::min(static_cast<int>(width) - 1, rx2));
                const std::size_t idx1 = (static_cast<std::size_t>(ry) * width + static_cast<std::size_t>(rx)) * 3u;
                const std::size_t idx2 = (static_cast<std::size_t>(ry2) * width + static_cast<std::size_t>(rx2)) * 3u;
                const int diff = static_cast<int>(y_work[idx1]) - static_cast<int>(y_work[idx2]);
                dist += static_cast<float>(diff * diff) * patch_gaussian[static_cast<std::size_t>((dy + patch_radius) * size + (dx + patch_radius))];
            }
        }
        const std::size_t patch_area = static_cast<std::size_t>(patch_radius * 2 + 1) * static_cast<std::size_t>(patch_radius * 2 + 1);
        return dist / static_cast<float>(patch_area);
    }

    std::uint8_t get_pixel_clamped(const std::uint8_t* buf, std::uint32_t width,
                                   std::uint32_t height, int y, int x) {
        y = std::max(0, std::min(static_cast<int>(height) - 1, y));
        x = std::max(0, std::min(static_cast<int>(width) - 1, x));
        return buf[(static_cast<std::size_t>(y) * width + static_cast<std::size_t>(x)) * 3u];
    }
}

void sc_2dnr::process_stream() {
    // Frame-level: measure from first read to last write
    m_metrics.set_processing_unit(sc_block_metrics<std::uint8_t>::ProcessingUnit::FRAME);
    m_metrics.begin_processing();

    if (!m_cfg.is_enable || m_cfg.window_size < 3 || m_cfg.patch_size < 3) {
        while (true) {
            fifo_out->write(fifo_in->read());
            fifo_out->write(fifo_in->read());
            fifo_out->write(fifo_in->read());
        }
    }

    // Read entire frame
    std::vector<std::uint8_t> frame(m_width * m_height * 3);
    for (std::size_t i = 0; i < m_width * m_height * 3; ++i) {
        frame[i] = fifo_in->read();
    }

    const std::vector<float> weight_lut = build_weight_lut(m_cfg.wts);
    const std::vector<float> patch_gaussian = build_patch_gaussian_kernel(m_cfg.patch_size);
    const int patch_radius = static_cast<int>(m_cfg.patch_size) / 2;
    const int window_radius = static_cast<int>(m_cfg.window_size) / 2;
    const std::size_t lut_max_idx = WEIGHT_LUT_SIZE - 1;

    std::vector<std::uint8_t> output(frame.size());

    for (std::uint32_t py = 0; py < m_height; ++py) {
        for (std::uint32_t px = 0; px < m_width; ++px) {
            float weighted_sum = 0.0f;
            float weight_sum = 0.0f;

            for (int wy = -window_radius; wy <= window_radius; ++wy) {
                for (int wx = -window_radius; wx <= window_radius; ++wx) {
                    int sy = static_cast<int>(py) + wy;
                    int sx = static_cast<int>(px) + wx;
                    if (sy < 0 || sy >= static_cast<int>(m_height) ||
                        sx < 0 || sx >= static_cast<int>(m_width))
                        continue;

                    float dist = 0.0f;
                    for (int dy = -patch_radius; dy <= patch_radius; ++dy) {
                        for (int dx = -patch_radius; dx <= patch_radius; ++dx) {
                            int ry = py + dy;
                            int rx = px + dx;
                            int ry2 = sy + dy;
                            int rx2 = sx + dx;
                            ry = std::max(0, std::min(static_cast<int>(m_height) - 1, ry));
                            rx = std::max(0, std::min(static_cast<int>(m_width) - 1, rx));
                            ry2 = std::max(0, std::min(static_cast<int>(m_height) - 1, ry2));
                            rx2 = std::max(0, std::min(static_cast<int>(m_width) - 1, rx2));
                            const std::size_t idx1 = (static_cast<std::size_t>(ry) * m_width + rx) * 3u;
                            const std::size_t idx2 = (static_cast<std::size_t>(ry2) * m_width + rx2) * 3u;
                            const int diff = static_cast<int>(frame[idx1]) - static_cast<int>(frame[idx2]);
                            dist += static_cast<float>(diff * diff) * patch_gaussian[static_cast<std::size_t>((dy + patch_radius) * static_cast<int>(m_cfg.patch_size) + (dx + patch_radius))];
                        }
                    }
                    const std::size_t patch_area = static_cast<std::size_t>(patch_radius * 2 + 1) * static_cast<std::size_t>(patch_radius * 2 + 1);
                    dist /= static_cast<float>(patch_area);

                    const std::size_t lut_idx = static_cast<std::size_t>(std::min<float>(dist, static_cast<float>(lut_max_idx)));
                    const float weight = weight_lut[lut_idx];

                    const std::uint8_t search_pixel = get_pixel_clamped(frame.data(), m_width, m_height, sy, sx);
                    weighted_sum += static_cast<float>(search_pixel) * weight;
                    weight_sum += weight;
                }
            }

            const std::size_t p = static_cast<std::size_t>(py) * m_width + static_cast<std::size_t>(px);
            const std::size_t i = p * 3u;

            if (weight_sum > 0.0f) {
                const float y_denoised = weighted_sum / weight_sum;
                output[i] = static_cast<std::uint8_t>(std::lround(std::max(0.0f, std::min(255.0f, y_denoised))));
            } else {
                output[i] = frame[i];
            }
            output[i + 1u] = frame[i + 1u];  // U unchanged
            output[i + 2u] = frame[i + 2u];  // V unchanged
        }
    }

    // Output
    for (std::size_t i = 0; i < output.size(); ++i) {
        fifo_out->write(output[i]);
    }
    m_metrics.end_processing();
    for (std::size_t i = 0; i < 3 * m_width * m_height; ++i) m_metrics.record_output();
}
