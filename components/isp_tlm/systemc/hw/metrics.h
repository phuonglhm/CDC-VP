/**
 * @file metrics.h
 * @brief Architecture metrics collection for ISP pipeline
 *
 * Provides metrics collection at multiple levels:
 *   - Frame metrics: end-to-end timing
 *   - Block metrics: per-block processing
 *   - Link metrics: inter-block communication
 *   - Bottleneck classification
 */

#ifndef ISP_ARCH_METRICS_H
#define ISP_ARCH_METRICS_H

#include <systemc>
using namespace sc_core;

#include <cstdint>
#include <vector>
#include <string>
#include <array>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>

#include "isp_arch_config.h"
#include "power.h"

// ============================================================================
// Frame Metrics
// ============================================================================
struct frame_metrics {
    std::uint64_t frame_id = 0;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint64_t total_pixels = 0;

    // Timing
    sc_time first_input_time = SC_ZERO_TIME;
    sc_time first_output_time = SC_ZERO_TIME;
    sc_time last_output_time = SC_ZERO_TIME;

    // Computed
    sc_time frame_time() const {
        if (last_output_time > first_input_time) {
            return last_output_time - first_input_time;
        }
        return SC_ZERO_TIME;
    }

    double frame_time_us() const {
        return frame_time().to_double() / 1e-3;
    }

    double fps(float clk_mhz) const {
        double ft = frame_time_us();
        return (ft > 0.0) ? (1e6 / ft) : 0.0;
    }

    // Bandwidth
    std::uint64_t input_bytes = 0;
    std::uint64_t output_bytes = 0;
    double input_bandwidth_mbps = 0.0;
    double output_bandwidth_mbps = 0.0;

    // Reset
    void reset() {
        frame_id = 0;
        first_input_time = SC_ZERO_TIME;
        first_output_time = SC_ZERO_TIME;
        last_output_time = SC_ZERO_TIME;
        input_bytes = 0;
        output_bytes = 0;
        input_bandwidth_mbps = 0.0;
        output_bandwidth_mbps = 0.0;
    }
};

// ============================================================================
// Block Cycle Counters
// ============================================================================
struct block_cycle_counters {
    std::uint64_t active_cycles = 0;
    std::uint64_t idle_cycles = 0;
    std::uint64_t starved_cycles = 0;    // Waiting for input
    std::uint64_t blocked_cycles = 0;     // Waiting for output
    std::uint64_t memory_wait_cycles = 0; // Waiting for memory
    std::uint64_t bypass_cycles = 0;      // Processing bypassed
    std::uint64_t stall_cycles = 0;       // Any stall condition

    double utilization() const {
        std::uint64_t total = active_cycles + idle_cycles + starved_cycles +
                             blocked_cycles + memory_wait_cycles + bypass_cycles;
        return (total > 0) ? static_cast<double>(active_cycles) / total : 0.0;
    }

    void reset() { *this = block_cycle_counters{}; }
};

// ============================================================================
// Block Performance Metrics
// ============================================================================
struct block_perf_metrics {
    std::string block_name;
    std::uint64_t frame_id = 0;

    // Throughput
    std::uint64_t input_beats = 0;
    std::uint64_t output_beats = 0;
    double throughput_hz = 0.0;
    double effective_ii = 0.0;  // Effective initiation interval

    // Latency
    sc_time mean_latency = SC_ZERO_TIME;
    sc_time min_latency = SC_ZERO_TIME;
    sc_time max_latency = SC_ZERO_TIME;
    double latency_stddev = 0.0;

    // Cycles
    block_cycle_counters cycles;
    double block_utilization = 0.0;

    // Memory
    std::uint64_t memory_reads = 0;
    std::uint64_t memory_writes = 0;
    std::uint64_t memory_wait_cycles = 0;

    // Operations
    std::uint64_t ops_add = 0;
    std::uint64_t ops_mul = 0;
    std::uint64_t ops_cmp = 0;

    // Power estimation
    power_values power;
    double energy_nj = 0.0;

    void reset() {
        *this = block_perf_metrics{block_name};
    }
};

// ============================================================================
// Link/Stream Metrics
// ============================================================================
struct link_metrics {
    std::string link_name;

    // Transfer stats
    std::uint64_t transfer_count = 0;
    std::uint64_t total_bits = 0;
    double bandwidth_hz = 0.0;

    // Occupancy
    std::uint32_t max_occupancy = 0;
    double avg_occupancy = 0.0;
    double occupancy_utilization = 0.0;

    // Backpressure
    std::uint64_t backpressure_events = 0;
    std::uint64_t backpressure_cycles = 0;

    // Starvation
    std::uint64_t starvation_events = 0;
    std::uint64_t starvation_cycles = 0;

    // Cycles
    std::uint64_t empty_cycles = 0;
    std::uint64_t full_cycles = 0;
    std::uint64_t active_cycles = 0;

    void reset() {
        *this = link_metrics{link_name};
    }
};

// ============================================================================
// Bottleneck Classification
// ============================================================================
enum class bottleneck_type {
    NONE,
    INPUT_BANDWIDTH,
    COMPUTE_II,          // Initiation interval limited
    MEMORY_PORT,          // Memory bandwidth limited
    DOWNSTREAM_BACKPRESSURE,
    OUTPUT_BANDWIDTH,
    FRAME_BARRIER        // Statistical blocks waiting
};

struct bottleneck_report {
    bottleneck_type type = bottleneck_type::NONE;
    std::string location;           // Which block/link
    double severity = 0.0;          // 0.0 - 1.0
    std::string description;
    std::vector<std::string> trace; // Backtrace of blocking chain
};

// Forward declaration for power config
struct block_power_config;
block_power_config get_power_config_for_block(const std::string& block_name);

// ============================================================================
// Architecture Metrics Collector
// ============================================================================
class arch_metrics_collector {
public:
    explicit arch_metrics_collector(const std::string& output_dir = "output/arch_metrics")
        : m_output_dir(output_dir)
        , m_total_cycles(0)
        , m_frame_count(0) {}

    // Frame management
    void start_frame(std::uint64_t frame_id, std::uint32_t width, std::uint32_t height) {
        m_current_frame.reset();
        m_current_frame.frame_id = frame_id;
        m_current_frame.width = width;
        m_current_frame.height = height;
        m_current_frame.total_pixels = static_cast<std::uint64_t>(width) * height;
        m_frame_count = frame_id + 1;
    }

    void record_first_input(const sc_time& t) {
        m_current_frame.first_input_time = t;
    }

    void record_first_output(const sc_time& t) {
        if (m_current_frame.first_output_time == SC_ZERO_TIME) {
            m_current_frame.first_output_time = t;
        }
    }

    void record_last_output(const sc_time& t) {
        m_current_frame.last_output_time = t;
    }

    void end_frame() {
        m_frames.push_back(m_current_frame);
    }

    // Block metrics
    void record_block_metrics(const block_perf_metrics& metrics) {
        m_block_metrics.push_back(metrics);
    }

    void update_block_cycles(const std::string& block,
                            const block_cycle_counters& cycles) {
        for (auto& bm : m_block_metrics) {
            if (bm.block_name == block) {
                bm.cycles = cycles;
                bm.block_utilization = cycles.utilization();
                return;
            }
        }
        // Not found, add new
        block_perf_metrics new_bm;
        new_bm.block_name = block;
        new_bm.cycles = cycles;
        m_block_metrics.push_back(new_bm);
    }

    // Link metrics
    void record_link_metrics(const link_metrics& metrics) {
        m_link_metrics.push_back(metrics);
    }

    // Total cycles
    void set_total_cycles(std::uint64_t cycles) { m_total_cycles = cycles; }

    // Bottleneck analysis
    bottleneck_report analyze_bottleneck() const;

    // Dump methods
    void dump_frame_metrics() const;
    void dump_block_metrics() const;
    void dump_link_metrics() const;
    void dump_bottleneck_report() const;
    void dump_all() const {
        dump_frame_metrics();
        dump_block_metrics();
        dump_link_metrics();
        dump_bottleneck_report();
    }

    // Power estimation methods
    void set_power_estimator(power_estimator* est) { m_power_estimator = est; }
    void calculate_block_power();
    void dump_power_metrics() const;
    pipeline_power_summary get_power_summary() const { return m_power_summary; }

    // Accessors
    const std::vector<frame_metrics>& frames() const { return m_frames; }
    const std::vector<block_perf_metrics>& blocks() const { return m_block_metrics; }
    const std::vector<link_metrics>& links() const { return m_link_metrics; }
    std::uint64_t total_cycles() const { return m_total_cycles; }
    std::uint64_t frame_count() const { return m_frame_count; }

private:
    std::string m_output_dir;
    std::uint64_t m_total_cycles;
    std::uint64_t m_frame_count;
    power_estimator* m_power_estimator = nullptr;
    pipeline_power_summary m_power_summary;

    frame_metrics m_current_frame;
    std::vector<frame_metrics> m_frames;
    std::vector<block_perf_metrics> m_block_metrics;
    std::vector<link_metrics> m_link_metrics;
};

// ============================================================================
// Implementation
// ============================================================================
inline bottleneck_report arch_metrics_collector::analyze_bottleneck() const {
    bottleneck_report report;

    // Find block with lowest utilization
    double min_util = 1.0;
    std::string bottleneck_block;
    bottleneck_type bt = bottleneck_type::NONE;

    for (const auto& bm : m_block_metrics) {
        if (bm.block_utilization < min_util) {
            min_util = bm.block_utilization;
            bottleneck_block = bm.block_name;

            if (bm.cycles.starved_cycles > bm.cycles.active_cycles) {
                bt = bottleneck_type::INPUT_BANDWIDTH;
            } else if (bm.cycles.blocked_cycles > bm.cycles.active_cycles) {
                bt = bottleneck_type::DOWNSTREAM_BACKPRESSURE;
            } else if (bm.cycles.memory_wait_cycles > 0) {
                bt = bottleneck_type::MEMORY_PORT;
            } else {
                bt = bottleneck_type::COMPUTE_II;
            }
        }
    }

    if (min_util < 1.0) {
        report.type = bt;
        report.location = bottleneck_block;
        report.severity = 1.0 - min_util;
        report.description = "Block " + bottleneck_block + " has lowest utilization";
    }

    return report;
}

inline void arch_metrics_collector::dump_frame_metrics() const {
    std::string path = m_output_dir + "/frame_metrics.csv";
    std::ofstream ofs(path);
    if (!ofs) return;

    ofs << "frame_id,width,height,pixels,first_input_us,first_output_us,"
        << "last_output_us,frame_time_us,fps,input_bytes,output_bytes\n";

    for (const auto& f : m_frames) {
        ofs << f.frame_id << ','
            << f.width << ','
            << f.height << ','
            << f.total_pixels << ','
            << std::fixed << std::setprecision(3)
            << f.first_input_time.to_double() / 1e-3 << ','
            << f.first_output_time.to_double() / 1e-3 << ','
            << f.last_output_time.to_double() / 1e-3 << ','
            << f.frame_time_us() << ','
            << f.fps(200.0) << ','
            << f.input_bytes << ','
            << f.output_bytes << '\n';
    }
    ofs.close();
}

inline void arch_metrics_collector::dump_block_metrics() const {
    std::string path = m_output_dir + "/block_metrics.csv";
    std::ofstream ofs(path);
    if (!ofs) return;

    ofs << "block,frame_id,input_beats,output_beats,mean_latency_ns,"
        << "min_latency_ns,max_latency_ns,utilization,"
        << "active_cycles,idle_cycles,starved_cycles,blocked_cycles,"
        << "memory_wait_cycles,bypass_cycles\n";

    for (const auto& bm : m_block_metrics) {
        ofs << bm.block_name << ','
            << bm.frame_id << ','
            << bm.input_beats << ','
            << bm.output_beats << ','
            << std::fixed << std::setprecision(3)
            << bm.mean_latency.to_double() / 1e-9 << ','
            << bm.min_latency.to_double() / 1e-9 << ','
            << bm.max_latency.to_double() / 1e-9 << ','
            << std::setprecision(4) << bm.block_utilization << ','
            << bm.cycles.active_cycles << ','
            << bm.cycles.idle_cycles << ','
            << bm.cycles.starved_cycles << ','
            << bm.cycles.blocked_cycles << ','
            << bm.cycles.memory_wait_cycles << ','
            << bm.cycles.bypass_cycles << '\n';
    }
    ofs.close();
}

inline void arch_metrics_collector::dump_link_metrics() const {
    std::string path = m_output_dir + "/link_metrics.csv";
    std::ofstream ofs(path);
    if (!ofs) return;

    ofs << "link,transfer_count,total_bits,bandwidth_hz,"
        << "max_occupancy,avg_occupancy,occupancy_util,"
        << "backpressure_events,backpressure_cycles,"
        << "starvation_events,starvation_cycles,"
        << "empty_cycles,full_cycles\n";

    for (const auto& lm : m_link_metrics) {
        ofs << lm.link_name << ','
            << lm.transfer_count << ','
            << lm.total_bits << ','
            << std::fixed << std::setprecision(2) << lm.bandwidth_hz << ','
            << lm.max_occupancy << ','
            << std::setprecision(4) << lm.avg_occupancy << ','
            << lm.occupancy_utilization << ','
            << lm.backpressure_events << ','
            << lm.backpressure_cycles << ','
            << lm.starvation_events << ','
            << lm.starvation_cycles << ','
            << lm.empty_cycles << ','
            << lm.full_cycles << '\n';
    }
    ofs.close();
}

inline void arch_metrics_collector::dump_bottleneck_report() const {
    bottleneck_report report = analyze_bottleneck();

    std::string path = m_output_dir + "/bottleneck_report.txt";
    std::ofstream ofs(path);
    if (!ofs) return;

    ofs << "=== Bottleneck Analysis Report ===\n\n";

    if (report.type == bottleneck_type::NONE) {
        ofs << "No significant bottleneck detected.\n";
    } else {
        ofs << "Dominant bottleneck: " << report.location << "\n"
            << "Type: ";
        switch (report.type) {
            case bottleneck_type::INPUT_BANDWIDTH: ofs << "Input Bandwidth Limited\n"; break;
            case bottleneck_type::COMPUTE_II: ofs << "Compute II Limited\n"; break;
            case bottleneck_type::MEMORY_PORT: ofs << "Memory Port Limited\n"; break;
            case bottleneck_type::DOWNSTREAM_BACKPRESSURE: ofs << "Downstream Backpressure\n"; break;
            case bottleneck_type::OUTPUT_BANDWIDTH: ofs << "Output Bandwidth Limited\n"; break;
            case bottleneck_type::FRAME_BARRIER: ofs << "Frame Barrier Limited\n"; break;
            default: ofs << "Unknown\n";
        }
        ofs << "Severity: " << std::fixed << std::setprecision(2)
            << (report.severity * 100.0) << "%\n"
            << "Description: " << report.description << "\n";
    }

    ofs << "\n=== Per-Block Utilization ===\n";
    for (const auto& bm : m_block_metrics) {
        ofs << bm.block_name << ": "
             << std::fixed << std::setprecision(1)
             << (bm.block_utilization * 100.0) << "%\n";
    }

    ofs.close();
}

inline void arch_metrics_collector::calculate_block_power() {
    if (!m_power_estimator) return;

    for (auto& bm : m_block_metrics) {
        // Get appropriate power config based on block name
        block_power_config cfg = get_power_config_for_block(bm.block_name);

        // Calculate power based on block metrics
        std::uint64_t total_cycles = bm.cycles.active_cycles + bm.cycles.idle_cycles +
                                     bm.cycles.starved_cycles + bm.cycles.blocked_cycles;
        std::uint64_t memory_accesses = bm.memory_reads + bm.memory_writes;

        bm.power = m_power_estimator->calculate_power(
            cfg,
            bm.cycles.active_cycles,
            total_cycles,
            memory_accesses,
            bm.input_beats
        );

        // Scale by utilization
        bm.power = m_power_estimator->scaled_power(bm.power, bm.block_utilization);

        // Calculate energy
        if (!m_frames.empty()) {
            double frame_time_us = m_frames.back().frame_time_us();
            bm.energy_nj = m_power_estimator->energy_per_frame(bm.power, frame_time_us);
        }

        // Add to summary
        m_power_summary.add_block(bm.block_name, bm.power);
    }
}

inline void arch_metrics_collector::dump_power_metrics() const {
    std::string path = m_output_dir + "/power_metrics.csv";
    std::ofstream ofs(path);
    if (!ofs) return;

    ofs << "block,utilization,static_mw,dynamic_mw,memory_mw,total_mw,energy_nj\n";

    for (const auto& bm : m_block_metrics) {
        ofs << bm.block_name << ','
            << std::fixed << std::setprecision(4) << bm.block_utilization << ','
            << std::setprecision(3) << bm.power.static_mw << ','
            << bm.power.dynamic_mw << ','
            << bm.power.memory_mw << ','
            << bm.power.total_mw << ','
            << bm.power.total_mw * m_power_summary.frame_time_us / 1000.0 << '\n';
    }

    // Summary
    ofs << "\n# Summary\n";
    ofs << "# Total static: " << m_power_summary.total.static_mw << " mW\n";
    ofs << "# Total dynamic: " << m_power_summary.total.dynamic_mw << " mW\n";
    ofs << "# Total memory: " << m_power_summary.total.memory_mw << " mW\n";
    ofs << "# Total power: " << m_power_summary.total.total_mw << " mW\n";

    ofs.close();

    // Also dump text summary
    std::string txt_path = m_output_dir + "/power_summary.txt";
    std::ofstream txt(txt_path);
    if (!txt) return;

    txt << "=== Power Estimation Summary ===\n\n";
    txt << "Total Power: " << std::fixed << std::setprecision(2)
        << m_power_summary.total.total_mw << " mW\n";
    txt << "  Static:  " << m_power_summary.total.static_mw << " mW\n";
    txt << "  Dynamic: " << m_power_summary.total.dynamic_mw << " mW\n";
    txt << "  Memory:  " << m_power_summary.total.memory_mw << " mW\n\n";

    if (m_power_summary.frame_time_us > 0) {
        txt << "Frame Energy: " << m_power_summary.frame_energy_nj << " nJ\n";
        txt << "Average Power: " << m_power_summary.avg_power_mw << " mW\n";
    }

    txt.close();
}

// Helper to get power config for a block
inline block_power_config get_power_config_for_block(const std::string& block_name) {
    if (block_name == "blc") return block_power_defaults::blc();
    if (block_name == "dg") return block_power_defaults::dg();
    if (block_name == "wb") return block_power_defaults::wb();
    if (block_name == "ccm") return block_power_defaults::ccm();
    if (block_name == "gc") return block_power_defaults::point_op();
    if (block_name == "csc") return block_power_defaults::csc();
    if (block_name == "cse") return block_power_defaults::point_op();
    if (block_name == "dpc") return block_power_defaults::dpc();
    if (block_name == "bnr") return block_power_defaults::bnr();
    if (block_name == "demosaic") return block_power_defaults::demosaic();
    if (block_name == "sharpen") return block_power_defaults::sharpen();
    if (block_name == "2dnr") return block_power_defaults::tdnoise_reduction();
    if (block_name == "lsc") return block_power_defaults::lsc();
    if (block_name == "scale") return block_power_defaults::scale();
    if (block_name == "yuv420") return block_power_defaults::yuv420();
    if (block_name == "awb") return block_power_defaults::awb();
    if (block_name == "aec") return block_power_defaults::aec();
    if (block_name == "normalizer") return block_power_defaults::normalizer();

    // Default
    block_power_config cfg;
    cfg.name = block_name;
    return cfg;
}

#endif  // ISP_ARCH_METRICS_H
