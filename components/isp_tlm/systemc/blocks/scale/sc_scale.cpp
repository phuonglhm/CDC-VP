/**
 * @file sc_scale.cpp
 * @brief Implementation of sc_scale SystemC module
 */
#include "sc_scale.h"
#include <algorithm>

std::uint8_t sc_scale::interpolate_bilinear_yuv(const std::vector<std::uint8_t>& data,
                                                  std::uint32_t channel, float x, float y) {
    int x0 = static_cast<int>(x);
    int y0 = static_cast<int>(y);
    int x1 = std::min(x0 + 1, static_cast<int>(m_in_width) - 1);
    int y1 = std::min(y0 + 1, static_cast<int>(m_in_height) - 1);

    float x_frac = x - x0;
    float y_frac = y - y0;

    const std::size_t stride = m_in_width * 3u;

    std::uint8_t p00 = data[static_cast<std::size_t>(y0) * stride + x0 * 3u + channel];
    std::uint8_t p10 = data[static_cast<std::size_t>(y0) * stride + x1 * 3u + channel];
    std::uint8_t p01 = data[static_cast<std::size_t>(y1) * stride + x0 * 3u + channel];
    std::uint8_t p11 = data[static_cast<std::size_t>(y1) * stride + x1 * 3u + channel];

    float top = p00 * (1.0f - x_frac) + p10 * x_frac;
    float bottom = p01 * (1.0f - x_frac) + p11 * x_frac;

    int result = static_cast<int>(top * (1.0f - y_frac) + bottom * y_frac);
    if (result < 0) return 0;
    if (result > 255) return 255;
    return static_cast<std::uint8_t>(result);
}

void sc_scale::process_stream() {
    // Check if timed mode is enabled
    bool timed_mode = (m_hw != nullptr) && m_hw->timed_mode;
    bool has_clock = (clk != nullptr);

    while (true) {

        if (!m_cfg.is_enable || (m_in_width == m_cfg.out_width && m_in_height == m_cfg.out_height)) {
            // Bypass mode
            const std::size_t in_pixels = static_cast<std::size_t>(m_in_width) * m_in_height;
            for (std::size_t i = 0; i < in_pixels * 3; ++i) {
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
                std::uint8_t val = fifo_in->read();

                if (timed_mode) {
                    if (fifo_out->num_free() < 1) {
                        ++m_cycle_count;
                        if (has_clock) {
                            wait(clk->posedge_event());
                        } else {
                            wait();
                        }
                    }
                } else {
                    if (fifo_out->num_free() < 1) {
                        wait(fifo_out->data_read_event());
                    }
                }
                fifo_out->write(val);
                if (timed_mode) {
                    ++m_active_cycles;
                    ++m_cycle_count;
                }
            }
            
            const std::size_t out_pixels = static_cast<std::size_t>(m_in_width) * m_in_height;
            continue;
        }

        // Read entire input frame
        std::vector<std::uint8_t> input(static_cast<std::size_t>(m_in_width) * m_in_height * 3u);
        for (std::size_t i = 0; i < input.size(); ++i) {
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
            input[i] = fifo_in->read();
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

        float x_scale = static_cast<float>(m_in_width) / static_cast<float>(m_cfg.out_width);
        float y_scale = static_cast<float>(m_in_height) / static_cast<float>(m_cfg.out_height);

        const std::size_t out_pixels = static_cast<std::size_t>(m_cfg.out_width) * m_cfg.out_height;

        for (std::uint32_t dy = 0; dy < m_cfg.out_height; ++dy) {
            for (std::uint32_t dx = 0; dx < m_cfg.out_width; ++dx) {
                float src_x = dx * x_scale;
                float src_y = dy * y_scale;

                for (std::uint32_t c = 0; c < 3; ++c) {
                    if (timed_mode) {
                        if (fifo_out->num_free() < 1) {
                            ++m_cycle_count;
                            if (has_clock) {
                                wait(clk->posedge_event());
                            } else {
                                wait();
                            }
                        }
                    } else {
                        if (fifo_out->num_free() < 1) {
                            wait(fifo_out->data_read_event());
                        }
                    }
                    std::uint8_t value = interpolate_bilinear_yuv(input, c, src_x, src_y);
                    fifo_out->write(value);
                    if (timed_mode) {
                        ++m_active_cycles;
                        ++m_cycle_count;
                    }
                }
            }
        }
        
    }
}
