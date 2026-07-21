#ifndef ISP_ARCH_METRICS_H
#define ISP_ARCH_METRICS_H

#include "isp_arch_config.h"

#include <systemc>

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <limits>
#include <optional>
#include <ostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace isp_tlm {

// Checked integer primitives used by every timing equation.  They throw
// instead of allowing a large image or profile value to wrap around.
inline std::uint64_t checked_add(std::uint64_t lhs, std::uint64_t rhs) {
    if (rhs > (std::numeric_limits<std::uint64_t>::max)() - lhs) {
        throw std::overflow_error("ISP metric addition overflow");
    }
    return lhs + rhs;
}

inline std::uint64_t checked_mul(std::uint64_t lhs, std::uint64_t rhs) {
    if (lhs != 0 && rhs > (std::numeric_limits<std::uint64_t>::max)() / lhs) {
        throw std::overflow_error("ISP metric multiplication overflow");
    }
    return lhs * rhs;
}

inline std::uint64_t ceil_div(std::uint64_t value, std::uint64_t divisor) {
    if (divisor == 0) {
        throw std::invalid_argument("ISP metric division by zero");
    }
    return value / divisor + (value % divisor == 0 ? 0 : 1);
}

inline std::uint64_t processing_beats(std::uint64_t logical_pixels,
                                      std::uint32_t pixels_per_cycle) {
    if (pixels_per_cycle == 0) {
        throw std::invalid_argument("pixels_per_cycle must be positive");
    }
    return ceil_div(logical_pixels, pixels_per_cycle);
}

inline std::uint64_t compute_cycles(std::uint64_t logical_pixels,
                                    std::uint32_t pixels_per_cycle,
                                    std::uint32_t pixel_initiation_interval_cycles) {
    if (pixel_initiation_interval_cycles == 0) {
        throw std::invalid_argument("pixel initiation interval must be positive");
    }
    return checked_mul(processing_beats(logical_pixels, pixels_per_cycle),
                       pixel_initiation_interval_cycles);
}

inline std::optional<std::uint64_t> memory_service_cycles(
    std::uint64_t logical_pixels, const std::optional<workload_profile>& workload,
    const std::optional<local_memory_service_profile>& memory) {
    if (!workload || !memory || !workload->available || !memory->available ||
        !workload->is_valid() || !memory->is_valid()) {
        return std::nullopt;
    }
    const auto demand_cycles = [&](std::uint32_t accesses_per_pixel,
                                   std::uint32_t ports) -> std::uint64_t {
        if (accesses_per_pixel == 0) {
            return 0;
        }
        if (ports == 0) {
            throw std::invalid_argument("memory ports must cover non-zero demand");
        }
        const std::uint64_t accesses =
            checked_mul(logical_pixels, accesses_per_pixel);
        return checked_mul(ceil_div(accesses, ports), memory->access_cycles);
    };
    const std::uint64_t reads =
        demand_cycles(workload->reads_per_pixel, memory->read_ports);
    const std::uint64_t writes =
        demand_cycles(workload->writes_per_pixel, memory->write_ports);
    return std::max(reads, writes);
}

inline std::uint64_t line_issue_interval_cycles(
    std::uint64_t logical_pixels, std::uint32_t pixels_per_cycle,
    std::uint32_t pixel_initiation_interval_cycles,
    const std::optional<workload_profile>& workload = std::nullopt,
    const std::optional<local_memory_service_profile>& memory = std::nullopt) {
    const std::uint64_t compute = compute_cycles(
        logical_pixels, pixels_per_cycle, pixel_initiation_interval_cycles);
    const auto service = memory_service_cycles(logical_pixels, workload, memory);
    return std::max(compute, service.value_or(0));
}

inline std::optional<std::uint64_t> memory_wait_cycles(
    std::uint64_t logical_pixels, std::uint32_t pixels_per_cycle,
    std::uint32_t pixel_initiation_interval_cycles,
    const std::optional<workload_profile>& workload = std::nullopt,
    const std::optional<local_memory_service_profile>& memory = std::nullopt) {
    const auto service = memory_service_cycles(logical_pixels, workload, memory);
    if (!service) {
        return std::nullopt;
    }
    const std::uint64_t compute = compute_cycles(
        logical_pixels, pixels_per_cycle, pixel_initiation_interval_cycles);
    return *service > compute ? *service - compute : 0;
}

struct operation_counts {
    bool available = false;
    metric_provenance provenance = metric_provenance::unavailable;
    std::uint64_t additions = 0;
    std::uint64_t multiplications = 0;
    std::uint64_t comparisons = 0;
    std::uint64_t reads = 0;
    std::uint64_t writes = 0;
};

inline operation_counts modeled_operations(std::uint64_t logical_pixels,
                                           const std::optional<workload_profile>& workload) {
    operation_counts result;
    if (!workload || !workload->available || !workload->is_valid()) {
        return result;
    }
    result.available = true;
    result.provenance = metric_provenance::modeled;
    result.additions = checked_mul(logical_pixels, workload->additions_per_pixel);
    result.multiplications =
        checked_mul(logical_pixels, workload->multiplications_per_pixel);
    result.comparisons = checked_mul(logical_pixels, workload->comparisons_per_pixel);
    result.reads = checked_mul(logical_pixels, workload->reads_per_pixel);
    result.writes = checked_mul(logical_pixels, workload->writes_per_pixel);
    return result;
}

struct raw_frame_metrics {
    std::uint64_t frame_id = 0;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint64_t logical_pixels = 0;
    std::uint64_t input_bytes = 0;
    std::uint64_t output_bytes = 0;
    bool input_bytes_available = false;
    bool output_bytes_available = false;
    sc_core::sc_time cycle_period = sc_core::SC_ZERO_TIME;
    sc_core::sc_time first_input_time = sc_core::SC_ZERO_TIME;
    sc_core::sc_time first_output_time = sc_core::SC_ZERO_TIME;
    sc_core::sc_time last_output_time = sc_core::SC_ZERO_TIME;
    bool has_first_input = false;
    bool has_first_output = false;
    bool has_last_output = false;
};

struct raw_block_metrics {
    std::string name;
    bool enabled = true;
    std::uint64_t input_lines = 0;
    std::uint64_t input_logical_pixels = 0;
    std::uint64_t output_logical_pixels = 0;
    std::uint64_t accepted_input_beats = 0;
    std::uint64_t produced_output_beats = 0;
    std::uint64_t output_lines = 0;
    std::uint64_t logical_pixels = 0;
    std::uint64_t processing_beats = 0;
    std::uint64_t active_cycles = 0;
    std::uint64_t bypass_cycles = 0;
    std::uint64_t input_starved_cycles = 0;
    std::uint64_t output_blocked_cycles = 0;
    std::uint64_t completion_wait_cycles = 0;
    std::uint64_t memory_wait_cycles = 0;
    bool memory_wait_available = false;
    metric_provenance memory_wait_provenance = metric_provenance::unavailable;
    std::uint64_t issue_window_cycles = 0;
    std::uint64_t observation_cycles = 0;
    operation_counts operations{};
};

struct raw_link_metrics {
    std::string name;
    std::uint64_t published_lines = 0;
    std::uint64_t read_lines = 0;
    std::uint64_t released_lines = 0;
    std::uint64_t logical_bytes = 0;
    std::uint32_t occupancy_high_water = 0;
    std::uint64_t producer_wait_events = 0;
    std::uint64_t consumer_wait_events = 0;
    sc_core::sc_time producer_wait = sc_core::SC_ZERO_TIME;
    sc_core::sc_time consumer_wait = sc_core::SC_ZERO_TIME;
    std::uint64_t producer_wait_cycles = 0;
    std::uint64_t consumer_wait_cycles = 0;
};

struct raw_pipeline_metrics {
    raw_frame_metrics frame;
    std::vector<raw_block_metrics> blocks;
    std::vector<raw_link_metrics> links;
};

struct frame_metric_values {
    bool first_output_latency_available = false;
    bool frame_cycles_available = false;
    std::uint64_t input_bytes = 0;
    std::uint64_t output_bytes = 0;
    bool input_bytes_available = false;
    bool output_bytes_available = false;
    std::uint64_t first_output_latency_cycles = 0;
    std::uint64_t frame_cycles = 0;
    bool achieved_pixels_per_cycle_available = false;
    bool input_bandwidth_available = false;
    bool output_bandwidth_available = false;
    double achieved_pixels_per_cycle = 0.0;
    double input_bandwidth_mbps = 0.0;
    double output_bandwidth_mbps = 0.0;
    double first_output_latency_us = 0.0;
    bool first_output_latency_us_available = false;
};

struct block_metric_values {
    std::string name;
    std::uint64_t input_lines = 0;
    std::uint64_t output_lines = 0;
    std::uint64_t logical_pixels = 0;
    std::uint64_t processing_beats = 0;
    bool enabled = true;
    std::uint64_t input_logical_pixels = 0;
    std::uint64_t output_logical_pixels = 0;
    std::uint64_t accepted_input_beats = 0;
    std::uint64_t produced_output_beats = 0;
    std::uint64_t active_cycles = 0;
    std::uint64_t bypass_cycles = 0;
    std::uint64_t input_starved_cycles = 0;
    std::uint64_t output_blocked_cycles = 0;
    std::uint64_t completion_wait_cycles = 0;
    std::uint64_t memory_wait_cycles = 0;
    bool memory_wait_available = false;
    metric_provenance memory_wait_provenance = metric_provenance::unavailable;
    double utilization = 0.0;
    bool effective_ii_available = false;
    double effective_ii = 0.0;
    operation_counts operations{};
};

struct link_metric_values {
    std::string name;
    std::uint64_t published_lines = 0;
    std::uint64_t read_lines = 0;
    std::uint64_t released_lines = 0;
    std::uint64_t logical_bytes = 0;
    std::uint32_t occupancy_high_water = 0;
    std::uint64_t producer_wait_events = 0;
    std::uint64_t consumer_wait_events = 0;
    sc_core::sc_time producer_wait = sc_core::SC_ZERO_TIME;
    sc_core::sc_time consumer_wait = sc_core::SC_ZERO_TIME;
    std::uint64_t producer_wait_cycles = 0;
    std::uint64_t consumer_wait_cycles = 0;
};

struct bottleneck_candidate {
    std::string type;
    std::string location;
    double severity = 0.0;
    std::string evidence;
};

struct pipeline_metrics {
    frame_metric_values frame;
    std::vector<block_metric_values> blocks;
    std::vector<link_metric_values> links;
    std::vector<bottleneck_candidate> bottlenecks;
};

inline std::vector<bottleneck_candidate> rank_bottlenecks(
    const raw_pipeline_metrics& raw) {
    std::vector<bottleneck_candidate> result;
    for (const auto& block : raw.blocks) {
        const long double denominator =
            1.0L + static_cast<long double>(block.active_cycles) +
            static_cast<long double>(block.bypass_cycles) +
            static_cast<long double>(block.input_starved_cycles) +
            static_cast<long double>(block.output_blocked_cycles) +
            static_cast<long double>(block.completion_wait_cycles) +
            static_cast<long double>(block.memory_wait_cycles);
        const auto add = [&](const char* type, std::uint64_t evidence,
                             const char* field) {
            if (evidence != 0) {
                const double severity =
                    static_cast<double>(static_cast<long double>(evidence) /
                                        denominator);
                result.push_back({type, block.name, std::min(1.0, severity),
                                  block.name + ": " + field + "=" +
                                      std::to_string(evidence)});
            }
        };
        add("input_starvation", block.input_starved_cycles,
            "input_starved_cycles");
        add("downstream_credit", block.output_blocked_cycles,
            "output_blocked_cycles");
        if (block.memory_wait_available) {
            add("memory_port", block.memory_wait_cycles, "memory_wait_cycles");
        }
        if (block.processing_beats != 0 &&
            block.issue_window_cycles > block.processing_beats) {
            result.push_back({"compute_ii", block.name,
                              std::min(1.0, static_cast<double>(
                                                 block.issue_window_cycles -
                                                 block.processing_beats) /
                                             static_cast<double>(
                                                 block.issue_window_cycles)),
                              block.name + ": observed_issue_window_cycles=" +
                                  std::to_string(block.issue_window_cycles)});
        }
    }
    std::sort(result.begin(), result.end(),
              [](const auto& lhs, const auto& rhs) {
                  return lhs.severity > rhs.severity;
              });
    return result;
}

inline std::uint64_t time_to_cycles(const sc_core::sc_time& duration,
                                    const sc_core::sc_time& cycle_period) {
    if (duration <= sc_core::SC_ZERO_TIME ||
        cycle_period <= sc_core::SC_ZERO_TIME) {
        return 0;
    }
    const std::uint64_t duration_ticks =
        static_cast<std::uint64_t>(duration.value());
    const std::uint64_t period_ticks =
        static_cast<std::uint64_t>(cycle_period.value());
    return ceil_div(duration_ticks, period_ticks);
}

inline pipeline_metrics derive_metrics(const raw_pipeline_metrics& raw) {
    pipeline_metrics result;
    result.bottlenecks = rank_bottlenecks(raw);
    const auto& frame = raw.frame;
    const bool valid_clock = frame.cycle_period > sc_core::SC_ZERO_TIME;
    const bool ordered_first =
        !frame.has_first_input || !frame.has_first_output ||
        frame.first_output_time >= frame.first_input_time;
    const bool ordered_frame =
        !frame.has_first_input || !frame.has_last_output ||
        frame.last_output_time >= frame.first_input_time;
    const bool valid_first_latency =
        valid_clock && frame.has_first_input && frame.has_first_output &&
        ordered_first;
    const bool valid_frame = valid_clock && frame.has_first_input &&
                             frame.has_last_output && ordered_frame;
    const std::uint64_t first_latency =
        valid_first_latency
            ? time_to_cycles(frame.first_output_time - frame.first_input_time,
                             frame.cycle_period)
            : 0;
    const std::uint64_t elapsed =
        valid_frame
            ? time_to_cycles(frame.last_output_time - frame.first_input_time,
                             frame.cycle_period)
            : 0;
    result.frame.first_output_latency_available = valid_first_latency;
    result.frame.frame_cycles_available = valid_frame;
    result.frame.first_output_latency_cycles = first_latency;
    result.frame.first_output_latency_us_available = valid_first_latency;
    result.frame.first_output_latency_us =
        valid_first_latency
            ? (frame.first_output_time - frame.first_input_time).to_seconds() / 1e-6
            : 0.0;
    result.frame.frame_cycles = elapsed;
    result.frame.input_bytes = frame.input_bytes;
    result.frame.output_bytes = frame.output_bytes;
    result.frame.input_bytes_available = frame.input_bytes_available;
    result.frame.output_bytes_available = frame.output_bytes_available;
    const double frame_us =
        valid_frame
            ? (frame.last_output_time - frame.first_input_time).to_seconds() / 1e-6
            : 0.0;
    const bool positive_elapsed = frame_us > 0.0;
    result.frame.achieved_pixels_per_cycle_available = elapsed != 0;
    result.frame.input_bandwidth_available =
        positive_elapsed && frame.input_bytes_available;
    result.frame.output_bandwidth_available =
        positive_elapsed && frame.output_bytes_available;
    if (elapsed != 0) {
        result.frame.achieved_pixels_per_cycle =
            static_cast<double>(frame.logical_pixels) / static_cast<double>(elapsed);
    }
    if (positive_elapsed && frame.input_bytes_available) {
        result.frame.input_bandwidth_mbps =
            static_cast<double>(frame.input_bytes) * 8.0 / frame_us;
    }
    if (positive_elapsed && frame.output_bytes_available) {
        result.frame.output_bandwidth_mbps =
            static_cast<double>(frame.output_bytes) * 8.0 / frame_us;
    }

    result.blocks.reserve(raw.blocks.size());
    for (const auto& block : raw.blocks) {
        block_metric_values derived;
        derived.name = block.name;
        derived.input_lines = block.input_lines;
        derived.output_lines = block.output_lines;
        derived.logical_pixels = block.logical_pixels;
        derived.processing_beats = block.processing_beats;
        derived.enabled = block.enabled;
        derived.input_logical_pixels = block.input_logical_pixels;
        derived.output_logical_pixels = block.output_logical_pixels;
        derived.accepted_input_beats = block.accepted_input_beats;
        derived.produced_output_beats = block.produced_output_beats;
        derived.active_cycles = block.active_cycles;
        derived.bypass_cycles = block.bypass_cycles;
        derived.input_starved_cycles = block.input_starved_cycles;
        derived.output_blocked_cycles = block.output_blocked_cycles;
        derived.completion_wait_cycles = block.completion_wait_cycles;
        derived.memory_wait_cycles = block.memory_wait_cycles;
        derived.memory_wait_available = block.memory_wait_available;
        derived.memory_wait_provenance = block.memory_wait_provenance;
        derived.operations = block.operations;
        if (block.enabled && block.observation_cycles != 0) {
            derived.utilization = std::min(
                1.0, static_cast<double>(block.active_cycles) /
                         static_cast<double>(block.observation_cycles));
        }
        if (block.processing_beats != 0 && block.issue_window_cycles != 0) {
            derived.effective_ii_available = true;
            derived.effective_ii = static_cast<double>(block.issue_window_cycles) /
                                   static_cast<double>(block.processing_beats);
        }
        result.blocks.push_back(std::move(derived));
    }

    result.links.reserve(raw.links.size());
    for (const auto& link : raw.links) {
        link_metric_values derived;
        derived.name = link.name;
        derived.published_lines = link.published_lines;
        derived.read_lines = link.read_lines;
        derived.released_lines = link.released_lines;
        derived.logical_bytes = link.logical_bytes;
        derived.occupancy_high_water = link.occupancy_high_water;
        derived.producer_wait_events = link.producer_wait_events;
        derived.consumer_wait_events = link.consumer_wait_events;
        derived.producer_wait = link.producer_wait;
        derived.consumer_wait = link.consumer_wait;
        derived.producer_wait_cycles =
            time_to_cycles(link.producer_wait, frame.cycle_period);
        derived.consumer_wait_cycles =
            time_to_cycles(link.consumer_wait, frame.cycle_period);
        result.links.push_back(std::move(derived));
    }
    return result;
}

inline const char* metric_availability(bool available) noexcept {
    return available ? "available" : "unavailable";
}
inline const char* provenance_name(metric_provenance provenance) noexcept {
    switch (provenance) {
        case metric_provenance::modeled: return "modeled";
        case metric_provenance::measured: return "measured";
        default: return "unavailable";
    }
}

inline bool write_metrics(std::ostream& output, const pipeline_metrics& metrics) {
    output << "frame_metric,value,availability\n"
           << "first_output_latency_cycles," << metrics.frame.first_output_latency_cycles
           << ',' << metric_availability(metrics.frame.first_output_latency_available) << "\n"
           << "first_output_latency_us," << metrics.frame.first_output_latency_us
           << ',' << metric_availability(metrics.frame.first_output_latency_us_available) << "\n"
           << "frame_cycles," << metrics.frame.frame_cycles << ','
           << metric_availability(metrics.frame.frame_cycles_available) << "\n"
           << "achieved_pixels_per_cycle," << metrics.frame.achieved_pixels_per_cycle
           << ',' << metric_availability(
                          metrics.frame.achieved_pixels_per_cycle_available) << "\n"
           << "input_bandwidth_mbps," << metrics.frame.input_bandwidth_mbps << ','
           << metric_availability(metrics.frame.input_bandwidth_available) << "\n"
           << "output_bandwidth_mbps," << metrics.frame.output_bandwidth_mbps << ','
           << metric_availability(metrics.frame.output_bandwidth_available) << "\n\n"
           << "input_bytes," << metrics.frame.input_bytes << ','
           << metric_availability(metrics.frame.input_bytes_available) << "\n"
           << "output_bytes," << metrics.frame.output_bytes << ','
           << metric_availability(metrics.frame.output_bytes_available) << "\n"
           << "block,name,input_lines,output_lines,logical_pixels,processing_beats,"
              "input_logical_pixels,output_logical_pixels,accepted_input_beats,"
              "produced_output_beats,enabled,active_cycles,bypass_cycles,"
              "input_starved_cycles,output_blocked_cycles,completion_wait_cycles,"
              "memory_wait_cycles,memory_wait_availability,memory_wait_provenance,"
              "utilization,effective_ii,effective_ii_availability,additions,"
              "multiplications,comparisons,reads,writes,operations_provenance\n";
    for (const auto& block : metrics.blocks) {
        output << "block," << block.name << ',' << block.input_lines << ','
               << block.output_lines << ',' << block.logical_pixels << ','
               << block.processing_beats << ',' << block.input_logical_pixels << ','
               << block.output_logical_pixels << ',' << block.accepted_input_beats << ','
               << block.produced_output_beats << ',' << (block.enabled ? 1 : 0) << ','
               << block.active_cycles << ',' << block.bypass_cycles << ','
               << block.input_starved_cycles << ',' << block.output_blocked_cycles << ','
               << block.completion_wait_cycles << ',' << block.memory_wait_cycles << ','
               << metric_availability(block.memory_wait_available) << ','
               << provenance_name(block.memory_wait_provenance) << ','
               << block.utilization << ',' << block.effective_ii << ','
               << metric_availability(block.effective_ii_available) << ','
               << block.operations.additions << ',' << block.operations.multiplications << ','
               << block.operations.comparisons << ',' << block.operations.reads << ','
               << block.operations.writes << ','
               << provenance_name(block.operations.provenance) << "\n";
    }
    output << "\nlink,name,published_lines,read_lines,released_lines,logical_bytes,"
              "occupancy_high_water,producer_wait_events,consumer_wait_events,"
              "producer_wait_seconds,consumer_wait_seconds,producer_wait_cycles,"
              "consumer_wait_cycles\n";
    for (const auto& link : metrics.links) {
        output << "link," << link.name << ',' << link.published_lines << ','
               << link.read_lines << ',' << link.released_lines << ','
               << link.logical_bytes << ',' << link.occupancy_high_water << ','
               << link.producer_wait_events << ',' << link.consumer_wait_events << ','
               << link.producer_wait.to_seconds() << ','
               << link.consumer_wait.to_seconds() << ',' << link.producer_wait_cycles
               << ',' << link.consumer_wait_cycles << "\n";
    }
    output << "\nbottleneck,type,location,severity,evidence\n";
    for (const auto& bottleneck : metrics.bottlenecks) {
        output << "bottleneck," << bottleneck.type << ',' << bottleneck.location
               << ',' << bottleneck.severity << ',' << bottleneck.evidence << "\n";
    }
    return static_cast<bool>(output);
}

// The only filesystem side effect in this header.  In-memory derivation never
// opens files or creates directories.
inline bool write_metrics(const pipeline_metrics& metrics, const std::string& path) {
    std::ofstream output(path);
    if (!output) {
        return false;
    }
    return write_metrics(output, metrics);
}

} // namespace isp_tlm

#endif // ISP_ARCH_METRICS_H
