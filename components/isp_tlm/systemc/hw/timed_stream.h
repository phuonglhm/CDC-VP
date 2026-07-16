/**
 * @file timed_stream.h
 * @brief Timed stream wrapper with bandwidth limiting
 *
 * Wraps sc_fifo ports to model DMA bandwidth limitations.
 * Used for input/output bandwidth simulation in Phase 4.
 */

#ifndef TIMED_STREAM_H
#define TIMED_STREAM_H

#include <systemc>
using namespace sc_core;

#include <cstdint>
#include <queue>

// ============================================================================
// Stream Configuration
// ============================================================================
struct stream_config {
    std::string name = "stream";
    std::uint32_t bus_width_bits = 64;      // Bus width in bits
    std::uint32_t max_burst = 16;          // Maximum burst length
    float bandwidth_limit_mbps = 0.0f;      // 0 = unlimited
    std::uint32_t read_latency_cycles = 1;  // Read latency
    std::uint32_t write_latency_cycles = 1; // Write latency
    bool enable_throttling = false;         // Enable bandwidth throttling
};

// ============================================================================
// Timed Input Stream (wraps read operations)
// ============================================================================
template <typename T>
class timed_input_stream : public sc_module {
public:
    sc_port<sc_fifo_in_if<T>> fifo_in;

    SC_HAS_PROCESS(timed_input_stream);

    timed_input_stream(sc_module_name name, const stream_config& cfg = stream_config{})
        : sc_module(name), m_cfg(cfg), m_cycle_count(0) {
        m_cfg.name = name;
        SC_THREAD(process_reads);
    }

    void set_config(const stream_config& cfg) { m_cfg = cfg; }

    // Metrics
    std::uint64_t total_reads() const { return m_total_reads; }
    std::uint64_t starved_cycles() const { return m_starved_cycles; }
    double bandwidth_mbps(float clk_mhz) const {
        if (m_cycle_count == 0) return 0.0;
        double bytes_per_cycle = static_cast<double>(m_total_reads * sizeof(T)) / m_cycle_count;
        return bytes_per_cycle * clk_mhz * 1e6 / 1e6;
    }
    double utilization() const {
        return (m_cycle_count > 0) ?
            static_cast<double>(m_total_reads) / m_cycle_count : 0.0;
    }

    void reset_counters() {
        m_total_reads = 0;
        m_starved_cycles = 0;
        m_cycle_count = 0;
    }

private:
    stream_config m_cfg;
    std::uint64_t m_total_reads = 0;
    std::uint64_t m_starved_cycles = 0;
    std::uint64_t m_cycle_count = 0;

    void process_reads() {
        while (true) {
            wait();

            ++m_cycle_count;

            // Check for bandwidth limiting
            if (m_cfg.enable_throttling && m_cfg.bandwidth_limit_mbps > 0) {
                // Calculate cycles needed based on bandwidth limit
                // bytes_per_cycle = bandwidth_mbps / (clk_mhz * 1e6) * 1e6
                // Simplified: just add wait based on ratio
                std::uint32_t bytes_per_pixel = sizeof(T);
                std::uint32_t cycles_per_pixel = (m_cfg.bus_width_bits / 8) / bytes_per_pixel;
                if (cycles_per_pixel > 0) {
                    wait(cycles_per_pixel - 1);  // -1 because we already waited once
                }
            }

            // Try to read
            if (fifo_in->num_available() > 0) {
                T data = fifo_in->read();
                ++m_total_reads;
            } else {
                ++m_starved_cycles;
            }
        }
    }
};

// ============================================================================
// Timed Output Stream (wraps write operations)
// ============================================================================
template <typename T>
class timed_output_stream : public sc_module {
public:
    sc_port<sc_fifo_out_if<T>> fifo_out;

    SC_HAS_PROCESS(timed_output_stream);

    timed_output_stream(sc_module_name name, const stream_config& cfg = stream_config{})
        : sc_module(name), m_cfg(cfg), m_cycle_count(0) {
        m_cfg.name = name;
        SC_THREAD(process_writes);
    }

    void set_config(const stream_config& cfg) { m_cfg = cfg; }

    // Metrics
    std::uint64_t total_writes() const { return m_total_writes; }
    std::uint64_t blocked_cycles() const { return m_blocked_cycles; }
    double bandwidth_mbps(float clk_mhz) const {
        if (m_cycle_count == 0) return 0.0;
        double bytes_per_cycle = static_cast<double>(m_total_writes * sizeof(T)) / m_cycle_count;
        return bytes_per_cycle * clk_mhz * 1e6 / 1e6;
    }
    double utilization() const {
        return (m_cycle_count > 0) ?
            static_cast<double>(m_total_writes) / m_cycle_count : 0.0;
    }

    void reset_counters() {
        m_total_writes = 0;
        m_blocked_cycles = 0;
        m_cycle_count = 0;
    }

private:
    stream_config m_cfg;
    std::uint64_t m_total_writes = 0;
    std::uint64_t m_blocked_cycles = 0;
    std::uint64_t m_cycle_count = 0;
    T m_pending_data;
    bool m_has_pending = false;

    void process_writes() {
        while (true) {
            wait();

            ++m_cycle_count;

            // Try to write
            if (m_has_pending) {
                if (fifo_out->nb_write(m_pending_data)) {
                    ++m_total_writes;
                    m_has_pending = false;
                } else {
                    ++m_blocked_cycles;
                }
            } else {
                // No pending data, idle
                wait();
            }
        }
    }

public:
    // Non-blocking write with pending
    bool nb_write_with_limit(const T& data) {
        if (!m_has_pending) {
            if (fifo_out->nb_write(data)) {
                ++m_total_writes;
                return true;
            } else {
                m_pending_data = data;
                m_has_pending = true;
            }
        }
        return false;
    }

    // Force write (blocking)
    void write_with_limit(const T& data) {
        fifo_out->write(data);
        ++m_total_writes;
    }
};

// ============================================================================
// Bandwidth Monitor (for measuring stream bandwidth)
// ============================================================================
template <typename T>
class bandwidth_monitor : public sc_module {
public:
    sc_port<sc_fifo_in_if<T>> fifo_in;

    SC_HAS_PROCESS(bandwidth_monitor);

    bandwidth_monitor(sc_module_name name)
        : sc_module(name), m_cycle_count(0), m_total_tokens(0) {
        SC_THREAD(monitor_process);
    }

    // Metrics
    std::uint64_t total_tokens() const { return m_total_tokens; }
    std::uint64_t total_cycles() const { return m_cycle_count; }
    double bandwidth_mbps(float clk_mhz) const {
        if (m_cycle_count == 0) return 0.0;
        double bytes_per_cycle = static_cast<double>(m_total_tokens * sizeof(T)) / m_cycle_count;
        return bytes_per_cycle * clk_mhz * 1e6 / 1e6;
    }
    double avg_occupancy() const {
        return m_cycle_count > 0 ?
            static_cast<double>(m_total_occupancy) / m_cycle_count : 0.0;
    }
    std::uint32_t max_occupancy() const { return m_max_occupancy; }

    void reset() {
        m_cycle_count = 0;
        m_total_tokens = 0;
        m_total_occupancy = 0;
        m_max_occupancy = 0;
    }

private:
    std::uint64_t m_cycle_count = 0;
    std::uint64_t m_total_tokens = 0;
    std::uint64_t m_total_occupancy = 0;
    std::uint32_t m_max_occupancy = 0;

    void monitor_process() {
        while (true) {
            wait();

            ++m_cycle_count;
            std::uint32_t available = fifo_in->num_available();
            m_total_occupancy += available;
            if (available > m_max_occupancy) {
                m_max_occupancy = available;
            }
        }
    }
};

#endif  // TIMED_STREAM_H
