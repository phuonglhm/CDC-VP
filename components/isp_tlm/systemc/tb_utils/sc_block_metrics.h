/**
 * @file sc_block_metrics.h
 * @brief SystemC block-level metrics collector for processing latency and throughput
 *
 * Measures timing INSIDE a processing block — from the moment the block reads
 * its final input token to the moment it writes its final output token for
 * each processing unit (pixel, row, or frame).
 *
 * Usage (point/per-pixel blocks):
 *
 *   // In block header (.h):
 *   #include "sc_block_metrics.h"
 *   SC_MODULE(sc_blc) {
 *       sc_block_metrics<std::uint16_t> m_metrics;
 *       void dump_metrics(const std::string& path);
 *   };
 *
 *   // In block implementation (.cpp):
 *   SC_THREAD(blc_thread) {
 *       while (true) {
 *           std::uint16_t p = fifo_in->read();       // read pixel
 *           m_metrics.begin_processing();              // t_start
 *           std::uint16_t out = process(p);
 *           fifo_out->write(out);                      // write pixel
 *           m_metrics.end_processing();                // t_end, push latency
 *       }
 *   }
 *
 *   // After sc_start():
 *   m_metrics.dump_metrics("output/metrics_blc");
 *
 * Usage (frame-buffered blocks):
 *
 *   SC_THREAD(dpc_thread) {
 *       // Frame-level processing
 *       m_metrics.begin_processing();                  // t_start = before read loop
 *       std::vector<uint16_t> frame(W*H);
 *       for (int i = 0; i < W*H; ++i) frame[i] = fifo_in->read();
 *       std::vector<uint16_t> output = process_frame(frame);
 *       for (int i = 0; i < output.size(); ++i) fifo_out->write(output[i]);
 *       m_metrics.end_processing();                    // t_end = after write loop
 *   }
 *
 * Metrics collected:
 *   - Block processing latency (per-pixel or per-frame)
 *   - Output timestamp vector (for boundary throughput at block output)
 *   - FIFO input fill level sampling (for bottleneck detection)
 *   - Hardware parameters (for real-time conversion)
 *
 * NOTE: This measures INSIDE the block. Use sc_metrics_wrapper for
 * inter-block boundary throughput measurement.
 */

#ifndef SC_BLOCK_METRICS_H
#define SC_BLOCK_METRICS_H

#include <systemc>
using namespace sc_core;

#include <cstddef>
#include <cstdint>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cmath>
#include <iostream>

#include "hardware_params.h"

template <typename T>
class sc_block_metrics {
public:
    explicit sc_block_metrics(const std::string& block_name,
                              const hw_params* hw = nullptr)
        : m_block_name(block_name)
        , m_hw(hw)
        , m_processing_unit(ProcessingUnit::PIXEL)
        , m_cycles_per_pixel(1)
        , m_collecting(true)
        , m_sampling_events(0)
        , m_input_fifo_fill_count(0) {}

    void set_hw(const hw_params* hw) { m_hw = hw; }

    enum class ProcessingUnit { PIXEL, ROW, FRAME };
    void set_processing_unit(ProcessingUnit u) { m_processing_unit = u; }
    void set_cycles_per_pixel(int c) { m_cycles_per_pixel = c; }

    void enable_collection(bool on) { m_collecting = on; }

    // Begin/end processing — for point blocks call per-pixel,
    // for frame blocks call once per frame.
    void begin_processing() {
        if (!m_collecting) return;
        m_t_start = sc_time_stamp();
    }

    void end_processing() {
        if (!m_collecting) return;
        m_t_end = sc_time_stamp();
        if (m_t_end >= m_t_start) {
            m_latencies.push_back(m_t_end - m_t_start);
        } else {
            m_latencies.push_back(SC_ZERO_TIME);
        }
    }

    // Record each output token timestamp — used to compute boundary throughput
    void record_output() {
        if (!m_collecting) return;
        m_out_times.push_back(sc_time_stamp());
    }

    // Record FIFO input fill — call whenever the block's input FIFO
    // can be inspected (i.e., when the block reads from it).
    void sample_fifo_in_fill(int n_available) {
        if (n_available > 0) m_input_fifo_fill_count += static_cast<std::size_t>(n_available);
        ++m_sampling_events;
    }

    // -- Accessors for pipeline-level aggregation --
    std::size_t sample_count() const { return m_latencies.size(); }

    sc_time mean_latency() const {
        if (m_latencies.empty()) return SC_ZERO_TIME;
        sc_time sum(SC_ZERO_TIME);
        for (const auto& t : m_latencies) sum += t;
        return sum / static_cast<double>(m_latencies.size());
    }

    sc_time min_latency() const {
        if (m_latencies.empty()) return SC_ZERO_TIME;
        return *std::min_element(m_latencies.begin(), m_latencies.end());
    }

    sc_time max_latency() const {
        if (m_latencies.empty()) return SC_ZERO_TIME;
        return *std::max_element(m_latencies.begin(), m_latencies.end());
    }

    double stddev_latency() const {
        if (m_latencies.size() < 2) return 0.0;
        const double mean_ns = mean_latency().to_seconds() / 1e-9;
        double sumsq = 0.0;
        for (const auto& t : m_latencies) {
            const double v = t.to_seconds() / 1e-9;
            const double d = v - mean_ns;
            sumsq += d * d;
        }
        return std::sqrt(sumsq / m_latencies.size());
    }

    sc_time throughput_period_mean() const {
        if (m_out_times.size() < 2) return SC_ZERO_TIME;
        sc_time sum(SC_ZERO_TIME);
        for (std::size_t i = 1; i < m_out_times.size(); ++i) {
            sum += (m_out_times[i] - m_out_times[i - 1]);
        }
        return sum / static_cast<double>(m_out_times.size() - 1);
    }

    std::size_t fifo_in_avg_fill() const {
        return m_sampling_events == 0 ? 0
                                      : m_input_fifo_fill_count / m_sampling_events;
    }

    float throughput_tokens_s() const {
        if (m_out_times.size() < 2) return 0.0f;
        const sc_time span = m_out_times.back() - m_out_times.front();
        if (span <= SC_ZERO_TIME) return 0.0f;
        return static_cast<float>(m_out_times.size()) /
               (span.to_seconds());
    }

    // Dump CSV (per-unit latency + output timestamps) and text summary
    void dump_metrics(const std::string& base_path) {
        const std::string csv_path  = base_path + ".csv";
        const std::string txt_path  = base_path + "_summary.txt";

        dump_csv(csv_path);
        dump_summary(txt_path);

        std::cout << "[sc_block_metrics:" << m_block_name
                  << "] dumped " << m_latencies.size()
                  << " samples to " << csv_path << std::endl;
    }

private:
    void dump_csv(const std::string& csv_path) {
        std::ofstream ofs(csv_path.c_str());
        if (!ofs.is_open()) {
            std::cerr << "[sc_block_metrics:" << m_block_name
                      << "] failed to open " << csv_path << std::endl;
            return;
        }
        ofs << "unit_id,t_start_ns,t_end_ns,latency_ns,out_time_ns\n";
        ofs << std::fixed;
        for (std::size_t i = 0; i < m_latencies.size(); ++i) {
            const sc_time t_end  = m_out_times.size() > i ? m_out_times[i] : SC_ZERO_TIME;
            const sc_time t_start = m_t_start;  // overwritten below
            // Recompute t_start from latency: t_start = t_end - latency
            sc_time t_in = (t_end >= m_latencies[i]) ? (t_end - m_latencies[i]) : SC_ZERO_TIME;
            ofs << i << ','
                << std::setprecision(9) << t_in.to_seconds() / 1e-9 << ','
                << std::setprecision(9) << t_end.to_seconds() / 1e-9 << ','
                << std::setprecision(9) << m_latencies[i].to_seconds() / 1e-9 << ','
                << std::setprecision(9) << t_end.to_seconds() / 1e-9
                << '\n';
        }
        ofs.close();
    }

    void dump_summary(const std::string& txt_path) {
        std::ofstream ofs(txt_path.c_str());
        if (!ofs.is_open()) {
            std::cerr << "[sc_block_metrics:" << m_block_name
                      << "] failed to open " << txt_path << std::endl;
            return;
        }
        ofs << std::fixed;
        ofs << "=== Block metrics: " << m_block_name << " ===\n";
        ofs << "total_samples        : " << m_latencies.size() << '\n';
        ofs << "simulation_time_ns   : "
            << sc_time_stamp().to_seconds() / 1e-9 << '\n';
        ofs << "processing_unit      : ";
        switch (m_processing_unit) {
            case ProcessingUnit::PIXEL: ofs << "pixel\n"; break;
            case ProcessingUnit::ROW:   ofs << "row\n";   break;
            case ProcessingUnit::FRAME: ofs << "frame\n"; break;
        }
        ofs << "cycles_per_pixel     : " << m_cycles_per_pixel << '\n';

        if (m_hw) {
            ofs << "hw_clk_mhz          : " << m_hw->clk_mhz << '\n';
            ofs << "hw_bus_width_bits   : " << m_hw->bus_width_bits << '\n';
            ofs << "hw_pixel_bits       : " << m_hw->pixel_bits << '\n';
            ofs << "hw_tokens_per_cycle : " << m_hw->tokens_per_cycle() << '\n';
            ofs << "hw_cycle_ns         : " << m_hw->cycle_ns() << '\n';
        }

        if (m_latencies.empty()) {
            ofs << "mean_latency_ns     : 0\n";
            ofs << "min_latency_ns      : 0\n";
            ofs << "max_latency_ns      : 0\n";
            ofs << "stddev_latency_ns   : 0\n";
            ofs << "throughput_tokens_s : 0\n";
        } else {
            ofs << "mean_latency_ns     : "
                << mean_latency().to_seconds() / 1e-9 << '\n';
            ofs << "min_latency_ns      : "
                << min_latency().to_seconds() / 1e-9 << '\n';
            ofs << "max_latency_ns      : "
                << max_latency().to_seconds() / 1e-9 << '\n';
            ofs << "stddev_latency_ns   : " << stddev_latency() << '\n';

            if (m_out_times.size() >= 2) {
                const sc_time span = m_out_times.back() - m_out_times.front();
                if (span > SC_ZERO_TIME) {
                    const double tokens_per_ns =
                        static_cast<double>(m_out_times.size()) /
                        (span.to_seconds());
                    ofs << "throughput_tokens_s : " << tokens_per_ns << '\n';
                    ofs << "mean_period_ns      : "
                        << throughput_period_mean().to_seconds() / 1e-9 << '\n';
                }
            }
        }

        ofs << "fifo_in_avg_fill    : " << fifo_in_avg_fill() << '\n';
        ofs << "sampling_events     : " << m_sampling_events << '\n';
        ofs.close();
    }

    std::string m_block_name;
    const hw_params* m_hw;

    ProcessingUnit m_processing_unit;
    int m_cycles_per_pixel;

    bool m_collecting;

    sc_time m_t_start;
    sc_time m_t_end;

    std::vector<sc_time> m_latencies;    // latencies per processing unit
    std::vector<sc_time> m_out_times;    // output timestamps (for throughput)
    std::size_t m_input_fifo_fill_count;
    std::size_t m_sampling_events;
};

#endif  // SC_BLOCK_METRICS_H
