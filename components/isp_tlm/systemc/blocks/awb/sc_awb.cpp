/**
 * @file sc_awb.cpp
 * @brief Implementation of sc_awb SystemC module
 */
#include "sc_awb.h"
#include <algorithm>

void sc_awb::precompute_gains(const std::uint16_t* rgb, std::size_t pixels) {
    if (rgb == nullptr || pixels == 0) return;
    const std::uint32_t max_value = (1u << m_bit_depth) - 1u;
    const float under_thresh = static_cast<float>(max_value) * m_cfg.underexposed_percentage;
    const float over_thresh  = static_cast<float>(max_value) * (1.0f - m_cfg.overexposed_percentage);

    std::uint64_t sum_r = 0, sum_g = 0, sum_b = 0;
    std::uint32_t count = 0;
    for (std::size_t i = 0; i < pixels; ++i) {
        const std::uint16_t r = rgb[i * 3u];
        const std::uint16_t g = rgb[i * 3u + 1u];
        const std::uint16_t b = rgb[i * 3u + 2u];
        const float lum = 0.299f * r + 0.587f * g + 0.114f * b;
        if (lum >= under_thresh && lum <= over_thresh) {
            sum_r += r;
            sum_g += g;
            sum_b += b;
            ++count;
        }
    }

    if (count == 0 || !m_cfg.is_enable) {
        m_r_gain = 1.0f;
        m_b_gain = 1.0f;
    } else {
        const float avg_r = static_cast<float>(sum_r) / count;
        const float avg_g = static_cast<float>(sum_g) / count;
        const float avg_b = static_cast<float>(sum_b) / count;
        const float avg_r_norm = avg_r / static_cast<float>(max_value);
        const float avg_g_norm = avg_g / static_cast<float>(max_value);
        const float avg_b_norm = avg_b / static_cast<float>(max_value);

        if (avg_r_norm > 0.01f && avg_g_norm > 0.01f && avg_b_norm > 0.01f) {
            m_r_gain = std::clamp(avg_g_norm / avg_r_norm, 0.25f, 4.0f);
            m_b_gain = std::clamp(avg_g_norm / avg_b_norm, 0.25f, 4.0f);
        } else {
            m_r_gain = 1.0f;
            m_b_gain = 1.0f;
        }
    }
    std::cout << "[AWB] precompute gains: R=" << m_r_gain << " B=" << m_b_gain << std::endl;
}

bool sc_awb::precompute_gains_from_bayer(const std::uint16_t* bayer, std::size_t pixels) {
    if (bayer == nullptr || pixels == 0) return false;

    // The raw Bayer input is at the sensor's input bit-depth (typically
    // 16-bit). The pipeline operates in a 12-bit working range after the
    // input normalizer, so we rescale the input down to 12-bit. The
    // under/over-exposed thresholds in `m_cfg` are expressed against
    // the working bit-depth, so the shift must compensate for the
    // difference between sensor and working bit-depths.
    const std::uint32_t max_input_value = (1u << 12) - 1u;  // 12-bit working
    const std::uint32_t shift = (m_input_bit_depth > 12)
                                    ? (m_input_bit_depth - 12)
                                    : 0;
    const float under_thresh = static_cast<float>(max_input_value) * m_cfg.underexposed_percentage;
    const float over_thresh  = static_cast<float>(max_input_value) * (1.0f - m_cfg.overexposed_percentage);

    // Determine (dx, dy) for R and B positions in the 2x2 mosaic.
    // For RGGB:        R=(0,0) G=(0,1)/(1,0) B=(1,1)
    // For BGGR:        B=(0,0) G=(0,1)/(1,0) R=(1,1)
    // GRBG/GBRG: swapped horizontally from RGGB/BGGR.
    int r_dx = 0, r_dy = 0, b_dx = 1, b_dy = 1;
    switch (m_bayer_pattern) {
    case cfa_types::RGGB: r_dx = 0; r_dy = 0; b_dx = 1; b_dy = 1; break;
    case cfa_types::BGGR: r_dx = 1; r_dy = 1; b_dx = 0; b_dy = 0; break;
    case cfa_types::GRBG: r_dx = 1; r_dy = 0; b_dx = 0; b_dy = 1; break;
    case cfa_types::GBRG: r_dx = 0; r_dy = 1; b_dx = 1; b_dy = 0; break;
    }

    std::uint64_t sum_r = 0, sum_g = 0, sum_b = 0;
    std::uint32_t cnt_r = 0, cnt_g = 0, cnt_b = 0;

    const std::size_t H = m_height;
    const std::size_t W = m_width;
    for (std::size_t y = 0; y + 1 < H; y += 2) {
        for (std::size_t x = 0; x + 1 < W; x += 2) {
            const std::uint16_t r12 = static_cast<std::uint16_t>(
                bayer[(y + r_dy) * W + (x + r_dx)] >> shift);
            const std::uint16_t b12 = static_cast<std::uint16_t>(
                bayer[(y + b_dy) * W + (x + b_dx)] >> shift);
            const std::uint16_t g112 = static_cast<std::uint16_t>(
                bayer[y * W + x + (r_dx ^ 1)] >> shift);
            const std::uint16_t g212 = static_cast<std::uint16_t>(
                bayer[(y + 1) * W + x + (r_dx ^ 1)] >> shift);

            // Luminance for raw Bayer: each 2x2 block has 1 R, 1 B, 2 G
            // samples. Use weights 0.5 / 1.0 / 0.5 so the formula
            // corresponds to a per-channel average rather than Rec.601.
            const float lum = 0.5f * r12 + (0.5f * (g112 + g212)) + 0.5f * b12;

            if (lum >= under_thresh && lum <= over_thresh) {
                sum_r += r12;
                sum_g += static_cast<std::uint32_t>(g112 + g212);
                sum_b += b12;
                ++cnt_r;
                cnt_g += 2;
                ++cnt_b;
            }
        }
    }
    (void)pixels;

    if (cnt_r == 0 || cnt_g == 0 || cnt_b == 0 || !m_cfg.is_enable) {
        m_r_gain = 1.0f;
        m_b_gain = 1.0f;
    } else {
        const float avg_r = static_cast<float>(sum_r) / cnt_r;
        const float avg_g = static_cast<float>(sum_g) / cnt_g;
        const float avg_b = static_cast<float>(sum_b) / cnt_b;
        const float avg_r_norm = avg_r / static_cast<float>(max_input_value);
        const float avg_g_norm = avg_g / static_cast<float>(max_input_value);
        const float avg_b_norm = avg_b / static_cast<float>(max_input_value);

        if (avg_r_norm > 0.01f && avg_g_norm > 0.01f && avg_b_norm > 0.01f) {
            m_r_gain = std::clamp(avg_g_norm / avg_r_norm, 0.25f, 4.0f);
            m_b_gain = std::clamp(avg_g_norm / avg_b_norm, 0.25f, 4.0f);
        } else {
            m_r_gain = 1.0f;
            m_b_gain = 1.0f;
        }
    }
    std::cout << "[AWB] precompute (Bayer) gains: R=" << m_r_gain
              << " B=" << m_b_gain << std::endl;
    return true;
}

void sc_awb::process_stream() {
    // Check if timed mode is enabled
    bool timed_mode = (m_hw != nullptr) && m_hw->timed_mode;
    bool has_clock = (clk != nullptr);
    if (timed_mode) {
        m_metrics.set_cycles_per_pixel(m_hw->default_cycles_per_pixel);
    }

    while (true) {
        // Frame-level: AWB reads entire frame for stats, then passes through
        m_metrics.set_processing_unit(sc_block_metrics<std::uint16_t>::ProcessingUnit::FRAME);
        m_metrics.begin_processing();

        // First pass: accumulate statistics
        const std::uint32_t max_value = (1u << m_bit_depth) - 1u;
        const float under_thresh = max_value * m_cfg.underexposed_percentage;
        const float over_thresh = max_value * (1.0f - m_cfg.overexposed_percentage);

        std::uint64_t sum_r = 0, sum_g = 0, sum_b = 0;
        std::uint32_t count = 0;

        const std::size_t pixels = static_cast<std::size_t>(m_width) * m_height;

        // Read all pixels and compute statistics
        for (std::size_t i = 0; i < pixels; ++i) {
            if (timed_mode) {
                if (fifo_in->num_available() < 3) {
                    ++m_starved_cycles;
                    ++m_cycle_count;
                    if (has_clock) {
                        wait(clk->posedge_event());
                    } else {
                        wait();
                    }
                    --i;  // Retry this iteration
                    continue;
                }
            }

            std::uint16_t r = fifo_in->read();
            std::uint16_t g = fifo_in->read();
            std::uint16_t b = fifo_in->read();

            if (timed_mode) {
                ++m_active_cycles;
                ++m_cycle_count;
            }

            const float lum = 0.299f * r + 0.587f * g + 0.114f * b;
            if (lum >= under_thresh && lum <= over_thresh) {
                sum_r += r;
                sum_g += g;
                sum_b += b;
                ++count;
            }

            // Pass through immediately for streaming
            if (timed_mode) {
                if (fifo_out->num_free() < 3) {
                    ++m_cycle_count;
                    if (has_clock) {
                        wait(clk->posedge_event());
                    } else {
                        wait();
                    }
                }
            }
            fifo_out->write(r);
            fifo_out->write(g);
            fifo_out->write(b);
            if (timed_mode) {
                ++m_cycle_count;
            }
        }

        // Synchronize at end of frame in timed mode
        if (timed_mode) {
            if (has_clock) {
                wait(clk->posedge_event());
            } else {
                wait();
            }
            ++m_cycle_count;
        }

        // Compute gains (mirrors `precompute_gains`)
        if (count == 0 || !m_cfg.is_enable) {
            m_r_gain = 1.0f;
            m_b_gain = 1.0f;
        } else {
            const float avg_r = static_cast<float>(sum_r) / count;
            const float avg_g = static_cast<float>(sum_g) / count;
            const float avg_b = static_cast<float>(sum_b) / count;

            const float avg_r_norm = avg_r / static_cast<float>(max_value);
            const float avg_g_norm = avg_g / static_cast<float>(max_value);
            const float avg_b_norm = avg_b / static_cast<float>(max_value);

            if (avg_r_norm > 0.01f && avg_g_norm > 0.01f && avg_b_norm > 0.01f) {
                m_r_gain = std::clamp(avg_g_norm / avg_r_norm, 0.25f, 4.0f);
                m_b_gain = std::clamp(avg_g_norm / avg_b_norm, 0.25f, 4.0f);
            } else {
                m_r_gain = 1.0f;
                m_b_gain = 1.0f;
            }
        }

        std::cout << "[AWB] Computed gains: R=" << m_r_gain << " B=" << m_b_gain << std::endl;

        // Latch gains at end-of-frame for downstream blocks (safe to read)
        m_latched_r_gain = m_r_gain;
        m_latched_b_gain = m_b_gain;

        m_metrics.end_processing();
        for (std::size_t i = 0; i < 3 * m_width * m_height; ++i) m_metrics.record_output();
    }
}
