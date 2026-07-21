/**
 * @file sc_sharpen.cpp
 * @brief Implementation of sc_sharpen SystemC module
 */
#include "sc_sharpen.h"
#include <algorithm>
#include <cmath>

std::uint8_t clip_to_uint8(float value) {
    if (value <= 0.0f) return 0;
    if (value >= 255.0f) return 255;
    return static_cast<std::uint8_t>(std::lround(value));
}

std::vector<float> sc_sharpen::create_gaussian_kernel(std::uint8_t sigma) {
    const int radius = (sigma > 0) ? static_cast<int>(std::ceil(sigma * 3.0)) : 1;
    const int size = 2 * radius + 1;
    m_radius = radius;

    std::vector<float> kernel(static_cast<std::size_t>(size * size));
    const float sigma_sq = static_cast<float>(sigma * sigma);
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

void sc_sharpen::process_stream() {
    // Check if timed mode is enabled
    bool timed_mode = (m_hw != nullptr) && m_hw->timed_mode;
    bool has_clock = (clk != nullptr);

    while (true) {

        if (!m_cfg.is_enable || m_cfg.sharpen_strength == 0) {
            // Bypass mode
            while (true) {
                if (timed_mode) {
                    if (fifo_in->num_available() < 3) {
                        ++m_starved_cycles;
                        ++m_cycle_count;
                        if (has_clock) {
                            wait(clk->posedge_event());
                        } else {
                            wait();
                        }
                        continue;
                    }
                } else {
                    if (fifo_in->num_available() < 3) {
                        wait(fifo_in->data_written_event());
                        continue;
                    }
                }
                std::uint8_t v1 = fifo_in->read();
                std::uint8_t v2 = fifo_in->read();
                std::uint8_t v3 = fifo_in->read();

                if (timed_mode) {
                    ++m_active_cycles;
                    ++m_cycle_count;
                    if (has_clock) {
                        wait(clk->posedge_event());
                    } else {
                        wait();
                    }
                    ++m_cycle_count;
                }

                if (timed_mode) {
                    if (fifo_out->num_free() < 3) {
                        ++m_cycle_count;
                        if (has_clock) {
                            wait(clk->posedge_event());
                        } else {
                            wait();
                        }
                    }
                } else {
                    if (fifo_out->num_free() < 3) {
                        wait(fifo_out->data_read_event());
                    }
                }
                fifo_out->write(v1);
                fifo_out->write(v2);
                fifo_out->write(v3);
                if (timed_mode) {
                    ++m_cycle_count;
                }
            }
        }

        // Create Gaussian kernel
        m_kernel = create_gaussian_kernel(m_cfg.sharpen_sigma);
        const int kernel_size = 2 * m_radius + 1;

        // Read entire frame for Gaussian filtering
        std::vector<std::uint8_t> y_plane(m_width * m_height * 3);
        for (std::size_t i = 0; i < m_width * m_height * 3; ++i) {
            if (timed_mode) {
                if (fifo_in->num_available() < 1) {
                    ++m_starved_cycles;
                    ++m_cycle_count;
                    if (has_clock) {
                        wait(clk->posedge_event());
                    } else {
                        wait();
                    }
                    --i;  // Retry this read
                    continue;
                }
            } else {
                if (fifo_in->num_available() < 1) {
                    wait(fifo_in->data_written_event());
                    --i;  // Retry this read
                    continue;
                }
            }
            y_plane[i] = fifo_in->read();
            if (timed_mode) {
                ++m_active_cycles;
                ++m_cycle_count;
            }
        }

        // Synchronize after read phase in timed mode
        if (timed_mode) {
            if (has_clock) {
                wait(clk->posedge_event());
            } else {
                wait();
            }
            ++m_cycle_count;
        }

        // Compute low-pass (Gaussian blurred) Y
        std::vector<float> y_lp(m_width * m_height);
        for (std::uint32_t y = 0; y < m_height; ++y) {
            for (std::uint32_t x = 0; x < m_width; ++x) {
                float sum = 0.0f;
                for (int dy = -m_radius; dy <= m_radius; ++dy) {
                    int sy = y + dy;
                    sy = std::max(0, std::min(static_cast<int>(m_height) - 1, sy));
                    for (int dx = -m_radius; dx <= m_radius; ++dx) {
                        int sx = x + dx;
                        sx = std::max(0, std::min(static_cast<int>(m_width) - 1, sx));
                        const float k = m_kernel[static_cast<std::size_t>((dy + m_radius) * kernel_size + (dx + m_radius))];
                        const std::size_t idx = (static_cast<std::size_t>(sy) * m_width + static_cast<std::size_t>(sx)) * 3u;
                        sum += static_cast<float>(y_plane[idx]) * k;
                    }
                }
                y_lp[static_cast<std::size_t>(y) * m_width + x] = sum;
            }
        }

        // Apply sharpening and output
        for (std::uint32_t y = 0; y < m_height; ++y) {
            for (std::uint32_t x = 0; x < m_width; ++x) {
                const std::size_t p = static_cast<std::size_t>(y) * m_width + x;
                const std::size_t i = p * 3u;

                const float y_in = static_cast<float>(y_plane[i]);
                const float y_blurred = y_lp[p];
                const float y_sharp = y_in + (y_in - y_blurred) * static_cast<float>(m_cfg.sharpen_strength);

                if (timed_mode) {
                    if (fifo_out->num_free() < 3) {
                        ++m_cycle_count;
                        if (has_clock) {
                            wait(clk->posedge_event());
                        } else {
                            wait();
                        }
                    }
                } else {
                    if (fifo_out->num_free() < 3) {
                        wait(fifo_out->data_read_event());
                    }
                }
                fifo_out->write(clip_to_uint8(y_sharp));
                fifo_out->write(y_plane[i + 1u]);  // U unchanged
                fifo_out->write(y_plane[i + 2u]);  // V unchanged
                if (timed_mode) {
                    ++m_active_cycles;
                    ++m_cycle_count;
                }
            }
        }

        
    }
}
