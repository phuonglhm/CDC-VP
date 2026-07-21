/**
 * @file sc_bnr.cpp
 * @brief Implementation of sc_bnr SystemC module
 *
 * Bayer Noise Reduction (BNR) - Frame-level processing.
 *
 * Hardware shell pattern (Phase 3):
 *   - Frame-level processing with timing
 *   - Architecture metrics
 */
#include "sc_bnr.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace {

inline float mirror_get(const float* img, int row, int col,
                        std::uint32_t width, std::uint32_t height) {
    if (row < 0) row = -row;
    else if (row >= static_cast<int>(height)) row = 2 * static_cast<int>(height) - 2 - row;

    if (col < 0) col = -col;
    else if (col >= static_cast<int>(width)) col = 2 * static_cast<int>(width) - 2 - col;

    return img[static_cast<std::size_t>(row) * width + static_cast<std::size_t>(col)];
}

std::vector<float> make_spatial_kernel(int size, float std_dev, int stride) {
    std_dev = std::max(std_dev, 1e-5f);
    if (size % 2 == 0) size += 1;
    if (size <= 0) size = 3;

    std::vector<float> kern(static_cast<std::size_t>(size) * size);
    float sum = 0.0f;
    int half = (size - 1) / 2;
    for (int i = 0; i < size; ++i) {
        for (int j = 0; j < size; ++j) {
            float d2 = static_cast<float>((stride * (i - half)) * (stride * (i - half)) +
                                          (stride * (j - half)) * (stride * (j - half)));
            float val = std::exp(-d2 / (2.0f * std_dev * std_dev));
            kern[static_cast<std::size_t>(i) * size + j] = val;
            sum += val;
        }
    }
    if (sum > 0.0f) {
        for (float& k : kern) k /= sum;
    }
    return kern;
}

void joint_bilateral_filter(const std::vector<float>& in_img,
                            const std::vector<float>& guide_img,
                            std::vector<float>& out_img,
                            std::uint32_t w, std::uint32_t h,
                            int spatial_size,
                            float stddev_s, float stddev_r,
                            int stride) {
    stddev_s = std::max(stddev_s, 1e-5f);
    stddev_r = std::max(stddev_r, 1e-5f);
    if (spatial_size % 2 == 0) spatial_size += 1;
    if (spatial_size <= 0) spatial_size = 3;

    const std::vector<float> s_kern = make_spatial_kernel(spatial_size, stddev_s, stride);
    const int half = (spatial_size - 1) / 2;

    for (int r = 0; r < static_cast<int>(h); ++r) {
        for (int c = 0; c < static_cast<int>(w); ++c) {
            const float guide_center = guide_img[static_cast<std::size_t>(r) * w + c];
            float sum_val = 0.0f;
            float sum_w = 0.0f;

            for (int i = 0; i < spatial_size; ++i) {
                int nr = r + i - half;
                for (int j = 0; j < spatial_size; ++j) {
                    int nc = c + j - half;
                    const float guide_neigh = mirror_get(guide_img.data(), nr, nc, w, h);
                    const float in_neigh = mirror_get(in_img.data(), nr, nc, w, h);

                    const float range_diff = guide_center - guide_neigh;
                    const float range_w = std::exp(-(range_diff * range_diff) /
                                                  (2.0f * stddev_r * stddev_r));
                    const float spatial_w = s_kern[static_cast<std::size_t>(i) * spatial_size + j];

                    const float weight = spatial_w * range_w;
                    sum_val += weight * in_neigh;
                    sum_w += weight;
                }
            }

            out_img[static_cast<std::size_t>(r) * w + c] =
                (sum_w > 0.0f) ? (sum_val / sum_w) : guide_center;
        }
    }
}

inline bayer_channel channel_at(int row, int col, cfa_types bayer) {
    const bool is_even_row = (row & 1) == 0;
    const bool is_even_col = (col & 1) == 0;
    switch (bayer) {
    case cfa_types::RGGB:
        return is_even_row ? (is_even_col ? bayer_channel::R : bayer_channel::GR)
                           : (is_even_col ? bayer_channel::GB : bayer_channel::B);
    case cfa_types::BGGR:
        return is_even_row ? (is_even_col ? bayer_channel::B : bayer_channel::GB)
                           : (is_even_col ? bayer_channel::GR : bayer_channel::R);
    case cfa_types::GRBG:
        return is_even_row ? (is_even_col ? bayer_channel::GR : bayer_channel::R)
                           : (is_even_col ? bayer_channel::B : bayer_channel::GB);
    case cfa_types::GBRG:
        return is_even_row ? (is_even_col ? bayer_channel::GB : bayer_channel::B)
                           : (is_even_col ? bayer_channel::R : bayer_channel::GR);
    }
    return bayer_channel::R;
}

} // anonymous namespace

void sc_bnr::process_stream() {
    bool timed_mode = (m_hw != nullptr) && m_hw->timed_mode;
    bool has_clock = (clk != nullptr);

    while (true) {
        
        

        const std::size_t total = static_cast<std::size_t>(m_width) * static_cast<std::size_t>(m_height);
        const std::uint32_t bit_range = (1u << m_bit_depth) - 1;
        const float scale = 1.0f / static_cast<float>(bit_range);

        if (timed_mode) {
            while (fifo_in->num_available() < total) {
                ++m_starved_cycles;
                ++m_cycle_count;
                if (has_clock) {
                    wait(clk->posedge_event());
                } else {
                    wait();
                }
            }
        }

        if (!m_cfg.is_enable) {
            // Bypass
            for (std::size_t i = 0; i < total; ++i) {
                if (timed_mode) {
                    if (has_clock) {
                        wait(clk->posedge_event());
                    } else {
                        wait();
                    }
                }

                fifo_out->write(fifo_in->read());
                if (timed_mode) {
                    ++m_active_cycles;
                    ++m_cycle_count;
                }
            }
            
            continue;
        }

        // 1) Read entire frame
        std::vector<std::uint16_t> raw(total);
        for (std::size_t i = 0; i < total; ++i) {
            if (timed_mode) {
                if (has_clock) {
                    wait(clk->posedge_event());
                } else {
                    wait();
                }
            }

            raw[i] = fifo_in->read();
            if (timed_mode) {
                ++m_active_cycles;
                ++m_cycle_count;
            }
        }

        std::vector<float> norm_in(total);
        for (std::size_t i = 0; i < total; ++i) {
            norm_in[i] = static_cast<float>(raw[i]) * scale;
        }

        const int W = static_cast<int>(m_width);
        const int H = static_cast<int>(m_height);

        // 2) Hamilton-Adams green-channel interpolation (guide image)
        std::vector<float> kern_filt_g(total);
        for (int i = 0; i < H; ++i) {
            for (int j = 0; j < W; ++j) {
                bayer_channel channel = channel_at(i, j, m_bayer);
                float green_est;
                if (channel == bayer_channel::GR || channel == bayer_channel::GB) {
                    green_est = norm_in[static_cast<std::size_t>(i) * m_width + j];
                } else {
                    const float g_n  = mirror_get(norm_in.data(), i - 1, j    , m_width, m_height);
                    const float g_s  = mirror_get(norm_in.data(), i + 1, j    , m_width, m_height);
                    const float g_e  = mirror_get(norm_in.data(), i    , j + 1, m_width, m_height);
                    const float g_w  = mirror_get(norm_in.data(), i    , j - 1, m_width, m_height);
                    const float d_ne = mirror_get(norm_in.data(), i - 1, j + 1, m_width, m_height);
                    const float d_nw = mirror_get(norm_in.data(), i - 1, j - 1, m_width, m_height);
                    const float d_se = mirror_get(norm_in.data(), i + 1, j + 1, m_width, m_height);
                    const float d_sw = mirror_get(norm_in.data(), i + 1, j - 1, m_width, m_height);

                    const float center   = norm_in[static_cast<std::size_t>(i) * m_width + j];
                    const float card_avg = 0.25f * (g_n + g_s + g_e + g_w);
                    const float diag_avg = 0.25f * (d_ne + d_nw + d_se + d_sw);
                    green_est = center + 0.5f * (card_avg - diag_avg);
                }
                kern_filt_g[static_cast<std::size_t>(i) * m_width + j] =
                    std::clamp(green_est, 0.0f, 1.0f);

                if (timed_mode) {
                    ++m_active_cycles;
                    ++m_cycle_count;
                    if (has_clock) {
                        wait(clk->posedge_event());
                    } else {
                        wait();
                    }
                }
            }
        }

        // 3) Extract sub-images at R / B positions
        std::vector<float> interp_g = norm_in;
        const std::uint32_t sub_w = m_width / 2;
        const std::uint32_t sub_h = m_height / 2;
        std::vector<float> in_img_r(static_cast<std::size_t>(sub_w) * sub_h);
        std::vector<float> in_img_b(static_cast<std::size_t>(sub_w) * sub_h);
        std::vector<float> interp_g_at_r(static_cast<std::size_t>(sub_w) * sub_h);
        std::vector<float> interp_g_at_b(static_cast<std::size_t>(sub_w) * sub_h);

        for (std::uint32_t i = 0; i < sub_h; ++i) {
            for (std::uint32_t j = 0; j < sub_w; ++j) {
                std::uint32_t r_idx = 0, b_idx = 0;
                switch (m_bayer) {
                case cfa_types::RGGB:
                    r_idx = (2 * i) * m_width + (2 * j);
                    b_idx = (2 * i + 1) * m_width + (2 * j + 1);
                    break;
                case cfa_types::BGGR:
                    r_idx = (2 * i + 1) * m_width + (2 * j + 1);
                    b_idx = (2 * i) * m_width + (2 * j);
                    break;
                case cfa_types::GRBG:
                    r_idx = (2 * i) * m_width + (2 * j + 1);
                    b_idx = (2 * i + 1) * m_width + (2 * j);
                    break;
                case cfa_types::GBRG:
                    r_idx = (2 * i + 1) * m_width + (2 * j);
                    b_idx = (2 * i) * m_width + (2 * j + 1);
                    break;
                }

                in_img_r[i * sub_w + j] = norm_in[r_idx];
                in_img_b[i * sub_w + j] = norm_in[b_idx];

                interp_g[r_idx] = kern_filt_g[r_idx];
                interp_g[b_idx] = kern_filt_g[b_idx];

                interp_g_at_r[i * sub_w + j] = kern_filt_g[r_idx];
                interp_g_at_b[i * sub_w + j] = kern_filt_g[b_idx];
            }
        }

        // 4) Joint bilateral filtering on R / G / B sub-images
        const int filt_size_g = m_cfg.filter_window;
        const int filt_size_r = (m_cfg.filter_window + 1) / 2;
        const int filt_size_b = (m_cfg.filter_window + 1) / 2;

        std::vector<float> out_img_r(static_cast<std::size_t>(sub_w) * sub_h);
        std::vector<float> out_img_g(total);
        std::vector<float> out_img_b(static_cast<std::size_t>(sub_w) * sub_h);

        joint_bilateral_filter(in_img_r, interp_g_at_r, out_img_r, sub_w, sub_h,
                               filt_size_r, m_cfg.r_std_dev_s, m_cfg.r_std_dev_r, 2);
        joint_bilateral_filter(interp_g, interp_g, out_img_g, m_width, m_height,
                               filt_size_g, m_cfg.g_std_dev_s, m_cfg.g_std_dev_r, 1);
        joint_bilateral_filter(in_img_b, interp_g_at_b, out_img_b, sub_w, sub_h,
                               filt_size_b, m_cfg.b_std_dev_s, m_cfg.b_std_dev_r, 2);

        // 5) Reconstruct Bayer grid
        std::vector<float> bnr_out = out_img_g;
        for (std::uint32_t i = 0; i < sub_h; ++i) {
            for (std::uint32_t j = 0; j < sub_w; ++j) {
                std::uint32_t r_idx = 0, b_idx = 0;
                switch (m_bayer) {
                case cfa_types::RGGB:
                    r_idx = (2 * i) * m_width + (2 * j);
                    b_idx = (2 * i + 1) * m_width + (2 * j + 1);
                    break;
                case cfa_types::BGGR:
                    r_idx = (2 * i + 1) * m_width + (2 * j + 1);
                    b_idx = (2 * i) * m_width + (2 * j);
                    break;
                case cfa_types::GRBG:
                    r_idx = (2 * i) * m_width + (2 * j + 1);
                    b_idx = (2 * i + 1) * m_width + (2 * j);
                    break;
                case cfa_types::GBRG:
                    r_idx = (2 * i + 1) * m_width + (2 * j);
                    b_idx = (2 * i) * m_width + (2 * j + 1);
                    break;
                }
                bnr_out[r_idx] = out_img_r[i * sub_w + j];
                bnr_out[b_idx] = out_img_b[i * sub_w + j];
            }
        }

        // 6) Rescale & stream out
        for (std::size_t i = 0; i < total; ++i) {
            const float val = bnr_out[i] * static_cast<float>(bit_range);
            const std::uint16_t output = static_cast<std::uint16_t>(
                std::clamp(val, 0.0f, static_cast<float>(bit_range)));
            fifo_out->write(output);
            
            if (timed_mode) {
                ++m_active_cycles;
                ++m_cycle_count;
                if (has_clock) {
                    wait(clk->posedge_event());
                } else {
                    wait();
                }
            }
        }

        
    }
}
