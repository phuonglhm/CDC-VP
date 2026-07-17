/**
 * @file sc_dpc.cpp
 * @brief Implementation of sc_dpc SystemC module
 *
 * Defective Pixel Correction (DPC) - Frame-level processing.
 *
 * Hardware shell pattern (Phase 3):
 *   - Frame-level processing with timing
 *   - Optional clock binding via hw_params
 *   - In timed_mode: uses clk->posedge_event() for proper timing
 *   - In untimed mode: no wait() needed, fifo operations block naturally
 */
#include "sc_dpc.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace {

inline std::uint16_t mirror_get(const std::uint16_t* img,
                                int row, int col,
                                std::uint32_t width, std::uint32_t height) {
    if (row < 0) row = -row;
    else if (row >= static_cast<int>(height)) row = 2 * static_cast<int>(height) - 2 - row;

    if (col < 0) col = -col;
    else if (col >= static_cast<int>(width)) col = 2 * static_cast<int>(width) - 2 - col;

    return img[static_cast<std::size_t>(row) * width + static_cast<std::size_t>(col)];
}

inline std::int32_t iabs(std::int32_t v) { return v < 0 ? -v : v; }

inline std::uint16_t corrected_3p(std::uint32_t a, std::uint32_t b) {
    return static_cast<std::uint16_t>((a + b) / 2u);
}

} // anonymous namespace

void sc_dpc::process_stream() {
    bool timed_mode = (m_hw != nullptr) && m_hw->timed_mode;
    bool has_clock = (clk != nullptr);

    while (true) {
        m_metrics.set_processing_unit(sc_block_metrics<std::uint16_t>::ProcessingUnit::FRAME);
        m_metrics.begin_processing();

        // Check if enough data is available (need W*H pixels)
        const std::size_t total = static_cast<std::size_t>(m_width) * static_cast<std::size_t>(m_height);

        if (timed_mode) {
            // In timed mode, count cycles while waiting for frame
            while (fifo_in->num_available() < total) {
                ++m_starved_cycles;
                ++m_cycle_count;
                if (has_clock) {
                    wait(clk->posedge_event());
                } else {
                    wait();
                }
            }
        } else {
            // In untimed mode: wait on FIFO event until we have enough data
            while (fifo_in->num_available() < total) {
                wait(fifo_in->data_written_event());
            }
        }

        // 1) Read the entire frame
        std::vector<std::uint16_t> frame(total);
        for (std::size_t i = 0; i < total; ++i) {
            if (timed_mode) {
                if (has_clock) {
                    wait(clk->posedge_event());
                } else {
                    wait();
                }
            }
            frame[i] = fifo_in->read();
            if (timed_mode) {
                ++m_active_cycles;
                ++m_cycle_count;
            }
        }

        if (!m_cfg.is_enable) {
            for (std::size_t i = 0; i < total; ++i) {
                if (timed_mode) {
                    if (has_clock) {
                        wait(clk->posedge_event());
                    } else {
                        wait();
                    }
                }
                fifo_out->write(frame[i]);
                if (timed_mode) {
                    ++m_active_cycles;
                    ++m_cycle_count;
                }
            }
            m_metrics.end_processing();
            m_metrics.record_output();
            continue;
        }

        // 2) Apply DPC over the whole frame
        std::vector<std::uint16_t> output(total);
        const std::uint16_t* in = frame.data();
        std::uint16_t* out = output.data();

        const int W = static_cast<int>(m_width);
        const int H = static_cast<int>(m_height);
        const int thr = static_cast<int>(m_cfg.dp_threshold);

        for (int i = 0; i < H; ++i) {
            for (int j = 0; j < W; ++j) {
                const std::uint16_t P = in[i * W + j];

                // 5x5 stride-2 sampling
                const std::uint16_t N0 = mirror_get(in, i - 2, j - 2, m_width, m_height);
                const std::uint16_t N1 = mirror_get(in, i - 2, j    , m_width, m_height);
                const std::uint16_t N2 = mirror_get(in, i - 2, j + 2, m_width, m_height);
                const std::uint16_t N3 = mirror_get(in, i    , j - 2, m_width, m_height);
                const std::uint16_t N4 = mirror_get(in, i    , j + 2, m_width, m_height);
                const std::uint16_t N5 = mirror_get(in, i + 2, j - 2, m_width, m_height);
                const std::uint16_t N6 = mirror_get(in, i + 2, j    , m_width, m_height);
                const std::uint16_t N7 = mirror_get(in, i + 2, j + 2, m_width, m_height);

                std::uint16_t n_min = N0;
                if (N1 < n_min) n_min = N1;
                if (N2 < n_min) n_min = N2;
                if (N3 < n_min) n_min = N3;
                if (N4 < n_min) n_min = N4;
                if (N5 < n_min) n_min = N5;
                if (N6 < n_min) n_min = N6;
                if (N7 < n_min) n_min = N7;

                std::uint16_t n_max = N0;
                if (N1 > n_max) n_max = N1;
                if (N2 > n_max) n_max = N2;
                if (N3 > n_max) n_max = N3;
                if (N4 > n_max) n_max = N4;
                if (N5 > n_max) n_max = N5;
                if (N6 > n_max) n_max = N6;
                if (N7 > n_max) n_max = N7;

                const bool cond1 = (P < n_min) || (P > n_max);

                const int p_int = static_cast<int>(P);
                const bool cond2 =
                    (iabs(p_int - static_cast<int>(N0)) > thr) &&
                    (iabs(p_int - static_cast<int>(N1)) > thr) &&
                    (iabs(p_int - static_cast<int>(N2)) > thr) &&
                    (iabs(p_int - static_cast<int>(N3)) > thr) &&
                    (iabs(p_int - static_cast<int>(N4)) > thr) &&
                    (iabs(p_int - static_cast<int>(N5)) > thr) &&
                    (iabs(p_int - static_cast<int>(N6)) > thr) &&
                    (iabs(p_int - static_cast<int>(N7)) > thr);

                if (cond1 && cond2) {
                    const std::int32_t iP = static_cast<std::int32_t>(P);
                    const std::int32_t G_v  = iabs(2 * iP - static_cast<std::int32_t>(N1) - static_cast<std::int32_t>(N6));
                    const std::int32_t G_h  = iabs(2 * iP - static_cast<std::int32_t>(N3) - static_cast<std::int32_t>(N4));
                    const std::int32_t G_ld = iabs(2 * iP - static_cast<std::int32_t>(N2) - static_cast<std::int32_t>(N5));
                    const std::int32_t G_rd = iabs(2 * iP - static_cast<std::int32_t>(N0) - static_cast<std::int32_t>(N7));

                    std::int32_t min_grad = G_v;
                    if (G_h  < min_grad) min_grad = G_h;
                    if (G_ld < min_grad) min_grad = G_ld;
                    if (G_rd < min_grad) min_grad = G_rd;

                    if (min_grad == G_v)       out[i * W + j] = corrected_3p(N1, N6);
                    else if (min_grad == G_h)  out[i * W + j] = corrected_3p(N3, N4);
                    else if (min_grad == G_ld) out[i * W + j] = corrected_3p(N2, N5);
                    else                       out[i * W + j] = corrected_3p(N0, N7);
                } else {
                    out[i * W + j] = P;
                }

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

        // 3) Stream out
        for (std::size_t i = 0; i < total; ++i) {
            if (timed_mode) {
                if (has_clock) {
                    wait(clk->posedge_event());
                } else {
                    wait();
                }
            }
            fifo_out->write(out[i]);
            m_metrics.record_output();
            if (timed_mode) {
                ++m_active_cycles;
                ++m_cycle_count;
            }
        }

        m_metrics.end_processing();
    }
}
