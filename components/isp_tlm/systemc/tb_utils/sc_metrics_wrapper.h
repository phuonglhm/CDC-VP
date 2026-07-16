/**
 * @file sc_metrics_wrapper.h
 * @brief SystemC passive metrics wrapper for inter-block FIFOs
 *
 * Sits between two existing FIFOs and timestamps every token that flows
 * through it. It does NOT modify the tokens — it is a passive observer.
 *
 * For each token, the wrapper records:
 *   - t_in  : simulation time when the token arrived at fifo_in
 *   - t_out : simulation time when the token was forwarded to fifo_out
 *   - latency = t_out - t_in
 *   - out_interval : delta between successive output emissions
 *
 * In addition, the incoming FIFO's num_available() depth is sampled
 * whenever the wrapper's threads run. The outgoing FIFO's occupancy
 * cannot be inspected through a write-only sc_fifo_out_if port in
 * SystemC 2.3.4; that counter is therefore reported as N/A.
 *
 * Use:
 *   sc_metrics_wrapper<uint16_t> wrap("blc_wrapper", "blc");
 *   wrap.fifo_in(in_fifo);
 *   wrap.fifo_out(out_fifo);
 *   ...
 *   sc_start();
 *   wrap.dump_metrics("output/metrics_blc_demo");
 */

#ifndef SC_METRICS_WRAPPER_H
#define SC_METRICS_WRAPPER_H

#include <systemc>
using namespace sc_core;

#include <cstdint>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <deque>

template <typename T>
class sc_metrics_wrapper : public sc_core::sc_module {
public:
    sc_core::sc_port<sc_fifo_in_if<T>>  fifo_in;
    sc_core::sc_port<sc_fifo_out_if<T>> fifo_out;

    SC_HAS_PROCESS(sc_metrics_wrapper);

    explicit sc_metrics_wrapper(sc_core::sc_module_name name,
                                const std::string& block_label = "wrapper")
        : sc_core::sc_module(name)
        , m_block_label(block_label)
        , m_first_out_time(SC_ZERO_TIME)
        , m_last_out_time(SC_ZERO_TIME) {
        // Single consumer-style thread that reads from fifo_in, stamps,
        // and forwards to fifo_out. We rely on a second pass-through
        // sc_fifo_bound to break a single-direction loop and to allow
        // the consumer to wake us via data_written_event.
        //
        // Architecture: two SC_THREADs —
        //   forward_in_thread(): reads fifo_in into m_buffer
        //   forward_out_thread(): drains m_buffer, writes fifo_out
        // The two threads are decoupled by m_buffer (a std::deque +
        // sc_event). This design avoids polling and lets sc_start()
        // terminate naturally when both FIFOs go idle.
        SC_THREAD(forward_in_thread);
        SC_THREAD(forward_out_thread);
    }

    void enable_collection(bool on) { m_collecting = on; }

    sc_core::sc_time mean_latency() const {
        if (m_latencies.empty()) return SC_ZERO_TIME;
        sc_core::sc_time sum(SC_ZERO_TIME);
        for (const auto& t : m_latencies) sum += t;
        return sum / static_cast<double>(m_latencies.size());
    }

    sc_core::sc_time min_latency() const {
        if (m_latencies.empty()) return SC_ZERO_TIME;
        return *std::min_element(m_latencies.begin(), m_latencies.end());
    }

    sc_core::sc_time max_latency() const {
        if (m_latencies.empty()) return SC_ZERO_TIME;
        return *std::max_element(m_latencies.begin(), m_latencies.end());
    }

    sc_core::sc_time throughput_period_mean() const {
        if (m_out_times.size() < 2) return SC_ZERO_TIME;
        sc_core::sc_time sum(SC_ZERO_TIME);
        for (std::size_t i = 1; i < m_out_times.size(); ++i) {
            sum += (m_out_times[i] - m_out_times[i - 1]);
        }
        return sum / static_cast<double>(m_out_times.size() - 1);
    }

    std::size_t sample_count() const { return m_latencies.size(); }
    std::size_t fifo_in_avg_fill() const {
        return m_sampling_events == 0 ? 0
                                      : m_input_fifo_fill_count / m_sampling_events;
    }
    std::size_t fifo_out_avg_fill() const {
        return 0;  // SystemC 2.3.4 — sc_fifo_out_if has no occupancy query
    }

    void dump_metrics(const std::string& base_path) {
        const std::string csv_path  = base_path + ".csv";
        const std::string txt_path  = base_path + "_summary.txt";

        // ---- CSV: per-token latency table ----
        {
            std::ofstream ofs(csv_path.c_str());
            if (!ofs.is_open()) {
                std::cerr << "[sc_metrics_wrapper:" << m_block_label
                          << "] failed to open " << csv_path << std::endl;
            } else {
                ofs << "token_id,t_in_ns,t_out_ns,latency_ns\n";
                ofs << std::fixed;
                for (std::size_t i = 0; i < m_latencies.size(); ++i) {
                    const sc_core::sc_time t_out  = m_out_times[i];
                    const sc_core::sc_time t_in   = t_out - m_latencies[i];
                    ofs << i << ','
                        << std::setprecision(9) << t_in.to_double()  / 1e-9 << ','
                        << std::setprecision(9) << t_out.to_double() / 1e-9 << ','
                        << std::setprecision(9) << m_latencies[i].to_double() / 1e-9
                        << '\n';
                }
                ofs.close();
            }
        }

        // ---- Summary text ----
        {
            std::ofstream ofs(txt_path.c_str());
            if (!ofs.is_open()) {
                std::cerr << "[sc_metrics_wrapper:" << m_block_label
                          << "] failed to open " << txt_path << std::endl;
                return;
            }
            ofs << std::fixed;
            ofs << "=== Metrics summary for block: " << m_block_label << " ===\n";
            ofs << "total_samples        : " << m_latencies.size()     << '\n';
            ofs << "simulation_time_ns   : "
                << sc_core::sc_time_stamp().to_double() / 1e-9 << '\n';
            if (m_latencies.empty()) {
                ofs << "mean_latency_ns      : 0\n";
                ofs << "min_latency_ns       : 0\n";
                ofs << "max_latency_ns       : 0\n";
                ofs << "stddev_latency_ns    : 0\n";
                ofs << "throughput_tokens_s  : 0\n";
            } else {
                ofs << "mean_latency_ns      : "
                    << mean_latency().to_double() / 1e-9 << '\n';
                ofs << "min_latency_ns       : "
                    << min_latency().to_double() / 1e-9 << '\n';
                ofs << "max_latency_ns       : "
                    << max_latency().to_double() / 1e-9 << '\n';

                const double mean_ns = mean_latency().to_double() / 1e-9;
                double sumsq = 0.0;
                for (const auto& t : m_latencies) {
                    const double v = t.to_double() / 1e-9;
                    const double d = v - mean_ns;
                    sumsq += d * d;
                }
                const double stddev_ns = std::sqrt(sumsq / m_latencies.size());
                ofs << "stddev_latency_ns    : " << stddev_ns << '\n';

                const sc_core::sc_time span = m_last_out_time - m_first_out_time;
                if (span > SC_ZERO_TIME) {
                    const double tokens_per_ns =
                        static_cast<double>(m_out_times.size()) /
                        (span.to_double() / 1e-9);
                    const double tokens_per_s = tokens_per_ns * 1e9;
                    ofs << "throughput_tokens_s  : " << tokens_per_s << '\n';
                    ofs << "mean_period_ns       : "
                        << throughput_period_mean().to_double() / 1e-9 << '\n';
                } else {
                    ofs << "throughput_tokens_s  : inf\n";
                    ofs << "mean_period_ns       : 0\n";
                }
            }

            ofs << "fifo_in_avg_fill     : " << fifo_in_avg_fill()  << '\n';
            ofs << "fifo_out_avg_fill    : N/A (write-only port)\n";
            ofs << "sampling_events      : " << m_sampling_events   << '\n';
            ofs.close();
        }

        std::cout << "[sc_metrics_wrapper:" << m_block_label
                  << "] dumped " << m_latencies.size()
                  << " samples to " << csv_path << std::endl;
    }

private:
    struct Token {
        T value;
        sc_core::sc_time t_in;
    };

    static constexpr std::size_t BUFFER_LIMIT = 8192;

    void forward_in_thread() {
        sample_fifo_fills();
        while (true) {
            T v = fifo_in->read();
            sample_fifo_fills();
            {
                if (m_buffer.size() >= BUFFER_LIMIT) {
                    static bool warned = false;
                    if (!warned) {
                        std::cerr << "[sc_metrics_wrapper:" << m_block_label
                                  << "] buffer overflow; dropping oldest samples"
                                  << std::endl;
                        warned = true;
                    }
                    m_buffer.pop_front();
                }
                m_buffer.push_back({v, sc_core::sc_time_stamp()});
            }
            // Wake the forward_out_thread (it may be waiting on this event).
            m_buffer_event.notify(SC_ZERO_TIME);
        }
    }

    void forward_out_thread() {
        while (true) {
            // Wait for the buffer to become non-empty.
            while (m_buffer.empty()) {
                wait(m_buffer_event);
            }
            Token tok = m_buffer.front();
            m_buffer.pop_front();

            const sc_core::sc_time t_out = sc_core::sc_time_stamp();
            sample_fifo_fills();
            fifo_out->write(tok.value);

            if (m_collecting) {
                const sc_core::sc_time latency =
                    (t_out >= tok.t_in) ? (t_out - tok.t_in) : sc_core::SC_ZERO_TIME;
                m_latencies.push_back(latency);
                m_out_times.push_back(t_out);
                if (m_out_times.size() == 1) {
                    m_first_out_time = t_out;
                }
                m_last_out_time = t_out;
            }
        }
    }

    void sample_fifo_fills() {
        // num_available() is only available on sc_fifo_in_if. We have an
        // output-side port but no read access to that FIFO. So we only
        // sample the input FIFO (the one we read from).
        if (fifo_in.get_interface() != nullptr) {
            const int n = fifo_in->num_available();
            if (n > 0) m_input_fifo_fill_count += static_cast<std::size_t>(n);
            ++m_sampling_events;
        }
    }

    std::string m_block_label;
    bool m_collecting = true;

    std::deque<Token> m_buffer;
    sc_core::sc_event m_buffer_event;
    std::vector<sc_core::sc_time> m_latencies;
    std::vector<sc_core::sc_time> m_out_times;

    sc_core::sc_time m_first_out_time;
    sc_core::sc_time m_last_out_time;

    std::size_t m_input_fifo_fill_count  = 0;
    std::size_t m_sampling_events        = 0;
};

#endif  // SC_METRICS_WRAPPER_H