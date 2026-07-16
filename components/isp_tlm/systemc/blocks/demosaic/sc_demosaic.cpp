/**
 * @file sc_demosaic.cpp
 * @brief Implementation of sc_demosaic SystemC module
 *
 * CFA (Bayer) to RGB demosaic.
 *
 * Reads the entire RAW frame from fifo_in, applies the 5x5 high-quality
 * linear demosaic identical to `demosaic_block::process`, and writes the
 * resulting RGB triplets out to fifo_out (3 tokens per input pixel).
 *
 * Mirror padding is done locally (a small `mirror_get` helper) instead of
 * using the global `isp_utils::get_pixel_mirror`.
 */
#include "sc_demosaic.h"
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace {

constexpr int WINDOW_SIZE = 5;
constexpr int HALF_WINDOW = WINDOW_SIZE / 2;

inline std::uint16_t mirror_get(const std::uint16_t* img, int row, int col,
                                std::uint32_t width, std::uint32_t height) {
    // Mirror convention from the original `isp_utils::get_pixel_mirror`:
    //   r < 0     -> r = -r           (so -1 -> 0, -2 -> 2, -3 -> 4 ...)
    //   r >= h    -> r = 2*h-2-r
    if (row < 0) row = -row;
    else if (row >= static_cast<int>(height)) row = 2 * static_cast<int>(height) - 2 - row;
    if (col < 0) col = -col;
    else if (col >= static_cast<int>(width)) col = 2 * static_cast<int>(width) - 2 - col;
    return img[static_cast<std::size_t>(row) * width + static_cast<std::size_t>(col)];
}

inline bayer_channel channel_at(int row, int col, cfa_types bayer) {
    const bool is_even_row = (row & 1) == 0;
    const bool is_even_col = (col & 1) == 0;
    switch (bayer) {
    case cfa_types::RGGB:
        return is_even_row ? (is_even_col ? bayer_channel::R : bayer_channel::GR)
                           : (is_even_col ? bayer_channel::GB : bayer_channel::B);
    case cfa_types::GRBG:
        return is_even_row ? (is_even_col ? bayer_channel::GR : bayer_channel::R)
                           : (is_even_col ? bayer_channel::B : bayer_channel::GB);
    case cfa_types::BGGR:
        return is_even_row ? (is_even_col ? bayer_channel::B : bayer_channel::GB)
                           : (is_even_col ? bayer_channel::GR : bayer_channel::R);
    case cfa_types::GBRG:
        return is_even_row ? (is_even_col ? bayer_channel::GB : bayer_channel::B)
                           : (is_even_col ? bayer_channel::R : bayer_channel::GR);
    }
    return bayer_channel::R;
}

} // anonymous namespace

void sc_demosaic::process_stream() {
    // Frame-level: measure from first read to last write
    m_metrics.set_processing_unit(sc_block_metrics<std::uint16_t>::ProcessingUnit::FRAME);
    m_metrics.begin_processing();

    const std::size_t total = static_cast<std::size_t>(m_width) * static_cast<std::size_t>(m_height);
    const std::uint32_t bit_range = (1u << m_bit_depth) - 1;

    // 1) Read entire RAW frame
    std::vector<std::uint16_t> raw(total);
    for (std::size_t i = 0; i < total; ++i) {
        raw[i] = fifo_in->read();
    }

    // 2) Bypass mode: replicate single channel into R=G=B triplet
    if (!m_cfg.is_enable) {
        for (std::size_t i = 0; i < total; ++i) {
            fifo_out->write(raw[i]);
            fifo_out->write(raw[i]);
            fifo_out->write(raw[i]);
        }
        return;
    }

    // 3) Demosaic pass
    std::vector<std::uint16_t> output(total * 3u);
    const int W = static_cast<int>(m_width);
    const int H = static_cast<int>(m_height);

    for (int r = 0; r < H; ++r) {
        for (int c = 0; c < W; ++c) {
            const std::uint32_t out_idx = 3u * (static_cast<std::uint32_t>(r) * m_width +
                                                static_cast<std::uint32_t>(c));

            float W5[WINDOW_SIZE][WINDOW_SIZE];
            for (int i = 0; i < WINDOW_SIZE; ++i) {
                for (int j = 0; j < WINDOW_SIZE; ++j) {
                    W5[i][j] = static_cast<float>(
                        mirror_get(raw.data(), r + i - HALF_WINDOW, c + j - HALF_WINDOW,
                                   m_width, m_height));
                }
            }

            const bayer_channel channel = channel_at(r, c, m_bayer);

            float rout = 0.0f, gout = 0.0f, bout = 0.0f;

            if (channel == bayer_channel::R) {
                rout = W5[2][2];
                gout = (4.0f * W5[2][2] - W5[0][2] - W5[2][0] - W5[4][2] - W5[2][4] +
                        2.0f * (W5[1][2] + W5[3][2] + W5[2][1] + W5[2][3])) / 8.0f;
                bout = (6.0f * W5[2][2] - 1.5f * (W5[0][2] + W5[2][0] + W5[4][2] + W5[2][4]) +
                        2.0f * (W5[1][1] + W5[1][3] + W5[3][1] + W5[3][3])) / 8.0f;
            } else if (channel == bayer_channel::B) {
                bout = W5[2][2];
                gout = (4.0f * W5[2][2] - W5[0][2] - W5[2][0] - W5[4][2] - W5[2][4] +
                        2.0f * (W5[1][2] + W5[3][2] + W5[2][1] + W5[2][3])) / 8.0f;
                rout = (6.0f * W5[2][2] - 1.5f * (W5[0][2] + W5[2][0] + W5[4][2] + W5[2][4]) +
                        2.0f * (W5[1][1] + W5[1][3] + W5[3][1] + W5[3][3])) / 8.0f;
            } else if (channel == bayer_channel::GR) {
                gout = W5[2][2];
                rout = (5.0f * W5[2][2] - W5[2][0] - W5[1][1] - W5[3][1] - W5[1][3] - W5[3][3] - W5[2][4] +
                        0.5f * (W5[0][2] + W5[4][2]) + 4.0f * (W5[2][1] + W5[2][3])) / 8.0f;
                bout = (5.0f * W5[2][2] - W5[0][2] - W5[1][1] - W5[1][3] - W5[4][2] - W5[3][1] - W5[3][3] +
                        0.5f * (W5[2][0] + W5[2][4]) + 4.0f * (W5[1][2] + W5[3][2])) / 8.0f;
            } else { // GB
                gout = W5[2][2];
                bout = (5.0f * W5[2][2] - W5[2][0] - W5[1][1] - W5[3][1] - W5[1][3] - W5[3][3] - W5[2][4] +
                        0.5f * (W5[0][2] + W5[4][2]) + 4.0f * (W5[2][1] + W5[2][3])) / 8.0f;
                rout = (5.0f * W5[2][2] - W5[0][2] - W5[1][1] - W5[1][3] - W5[4][2] - W5[3][1] - W5[3][3] +
                        0.5f * (W5[2][0] + W5[2][4]) + 4.0f * (W5[1][2] + W5[3][2])) / 8.0f;
            }

            const float br = static_cast<float>(bit_range);
            output[out_idx + 0] = static_cast<std::uint16_t>(std::clamp(rout, 0.0f, br));
            output[out_idx + 1] = static_cast<std::uint16_t>(std::clamp(gout, 0.0f, br));
            output[out_idx + 2] = static_cast<std::uint16_t>(std::clamp(bout, 0.0f, br));
        }
    }

    // 4) Stream out RGB triplets
    for (std::size_t i = 0; i < output.size(); ++i) {
        fifo_out->write(output[i]);
    }

    m_metrics.end_processing();
    for (std::size_t i = 0; i < output.size(); ++i) m_metrics.record_output();
}