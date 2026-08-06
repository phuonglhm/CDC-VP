// SPDX-License-Identifier: Apache-2.0
//
// Deterministic, drainable benchmark for the production FlooNoC TLM wrapper.
//
// Unlike the firmware platform, this harness owns every traffic source.  It
// can therefore implement the signed measurement window:
//
//   warm-up -> reset statistics -> measure -> stop injection -> drain -> report
//
// It deliberately uses noc_interconnect rather than a second analytical NoC
// model, so every [M] counter in its JSON comes from the same detailed datapath
// used by noc_soc.

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

#include <floo_noc_model/noc_interconnect.h>
#include <floo_noc_model/noc_metrics.hpp>

namespace {

using cdc::components::noc_interconnect;
using node = noc_interconnect::node;
using transaction_metrics = floo::model::transaction_metrics;

constexpr std::uint64_t kRamBase = 0x8000'0000;
constexpr std::uint64_t kRamSize = 0x1'0000;
constexpr double kClockPeriodNs = 1.0;
constexpr unsigned kMaximumManagers = 3;

#ifndef NOC_SOC_BUILD_TYPE
#define NOC_SOC_BUILD_TYPE "unknown"
#endif
constexpr const char* kBuildType = NOC_SOC_BUILD_TYPE;

struct options {
    std::string workload = "quiet";
    unsigned width = 4;
    unsigned height = 4;
    unsigned transactions = 64;
    unsigned warmup_transactions = 4;
    unsigned workers_per_manager = 1;
    unsigned injection_gap_cycles = 100;
    unsigned read_percent = 50;
    unsigned burst_bytes = 8;
    std::uint32_t seed = 1;
    std::string metrics_path;
    bool gap_seen = false;
    bool help = false;
};

const char* kUsage =
    "usage: noc_benchmark --workload quiet|hotspot|contention|fairness "
    "[--topology 2x2|3x3|4x4|4x2|2x4] [--transactions N] "
    "[--warmup-transactions N] [--workers-per-manager 1..8] "
    "[--injection-gap-cycles N] "
    "[--read-percent 0..100] [--burst-bytes 4|8|16|32|64] "
    "[--seed N] --noc-metrics FILE";

unsigned parse_unsigned(
    const std::string& text, const std::string& option,
    unsigned maximum = std::numeric_limits<unsigned>::max())
{
    if (text.empty()
        || text.find_first_not_of("0123456789") != std::string::npos) {
        throw std::invalid_argument(option + " requires an unsigned integer");
    }
    std::size_t consumed = 0;
    const auto value = std::stoull(text, &consumed);
    if (consumed != text.size() || value > maximum) {
        throw std::invalid_argument(option + " is out of range");
    }
    return static_cast<unsigned>(value);
}

std::string require_value(
    int& index, int argc, char* argv[], const std::string& option)
{
    if (index + 1 >= argc) {
        throw std::invalid_argument(option + " requires a value");
    }
    return argv[++index];
}

void parse_topology(const std::string& text, unsigned& width, unsigned& height)
{
    const auto split = text.find('x');
    if (split == std::string::npos || text.find('x', split + 1)
        != std::string::npos) {
        throw std::invalid_argument("--topology must use WIDTHxHEIGHT");
    }
    width = parse_unsigned(text.substr(0, split), "--topology");
    height = parse_unsigned(text.substr(split + 1), "--topology");
    const bool supported =
        (width == 2 && height == 2)
        || (width == 3 && height == 3)
        || (width == 4 && height == 4)
        || (width == 4 && height == 2)
        || (width == 2 && height == 4);
    if (!supported) {
        throw std::invalid_argument(
            "--topology is outside the verified noc_interconnect set");
    }
}

options parse_options(int argc, char* argv[])
{
    options result;
    for (int index = 1; index < argc; ++index) {
        const std::string arg = argv[index];
        if (arg == "-h" || arg == "--help") {
            result.help = true;
        } else if (arg == "--workload") {
            result.workload = require_value(index, argc, argv, arg);
        } else if (arg == "--topology") {
            parse_topology(
                require_value(index, argc, argv, arg),
                result.width, result.height);
        } else if (arg == "--transactions") {
            result.transactions = parse_unsigned(
                require_value(index, argc, argv, arg), arg, 1'000'000);
        } else if (arg == "--warmup-transactions") {
            result.warmup_transactions = parse_unsigned(
                require_value(index, argc, argv, arg), arg, 100'000);
        } else if (arg == "--workers-per-manager") {
            result.workers_per_manager = parse_unsigned(
                require_value(index, argc, argv, arg), arg, 8);
        } else if (arg == "--injection-gap-cycles") {
            result.injection_gap_cycles = parse_unsigned(
                require_value(index, argc, argv, arg), arg, 1'000'000);
            result.gap_seen = true;
        } else if (arg == "--read-percent") {
            result.read_percent = parse_unsigned(
                require_value(index, argc, argv, arg), arg, 100);
        } else if (arg == "--burst-bytes") {
            result.burst_bytes = parse_unsigned(
                require_value(index, argc, argv, arg), arg, 64);
        } else if (arg == "--seed") {
            result.seed = static_cast<std::uint32_t>(parse_unsigned(
                require_value(index, argc, argv, arg), arg));
        } else if (arg == "--noc-metrics") {
            result.metrics_path = require_value(index, argc, argv, arg);
        } else {
            throw std::invalid_argument("unknown option '" + arg + "'");
        }
    }
    if (result.help) {
        return result;
    }
    const bool workload_ok =
        result.workload == "quiet" || result.workload == "hotspot"
        || result.workload == "contention" || result.workload == "fairness";
    if (!workload_ok) {
        throw std::invalid_argument(
            "--workload must be quiet, hotspot, contention or fairness");
    }
    if (result.transactions == 0) {
        throw std::invalid_argument("--transactions must be positive");
    }
    if (result.workers_per_manager == 0) {
        throw std::invalid_argument(
            "--workers-per-manager must be in the range 1..8");
    }
    if (result.metrics_path.empty()) {
        throw std::invalid_argument("--noc-metrics requires a non-empty path");
    }
    const bool burst_ok =
        result.burst_bytes == 4 || result.burst_bytes == 8
        || result.burst_bytes == 16 || result.burst_bytes == 32
        || result.burst_bytes == 64;
    if (!burst_ok) {
        throw std::invalid_argument(
            "--burst-bytes must be 4, 8, 16, 32 or 64");
    }
    if (!result.gap_seen) {
        result.injection_gap_cycles =
            result.workload == "quiet" ? 100u : 0u;
    }
    return result;
}

unsigned manager_count_for(const std::string& workload)
{
    if (workload == "contention") {
        return 2;
    }
    if (workload == "fairness") {
        return 3;
    }
    return 1;
}

std::vector<node> manager_nodes(
    unsigned width, unsigned height, node target, unsigned count)
{
    const std::array<node, 8> preferred{{
        {0, 0},
        {0, height - 1},
        {width - 1, 0},
        {width - 1, height - 1},
        {width / 2, 0},
        {0, height / 2},
        {width - 1, height / 2},
        {width / 2, height - 1},
    }};
    std::vector<node> result;
    for (const auto candidate : preferred) {
        if ((candidate.x == target.x && candidate.y == target.y)
            || std::find_if(
                result.begin(), result.end(),
                [candidate](node placed) {
                    return placed.x == candidate.x
                        && placed.y == candidate.y;
                }) != result.end()) {
            continue;
        }
        result.push_back(candidate);
        if (result.size() == count) {
            return result;
        }
    }
    throw std::invalid_argument(
        "topology has too few distinct nodes for the workload");
}

class benchmark_ram : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<benchmark_ram> socket;

    explicit benchmark_ram(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
        , socket("socket")
        , storage_(kRamSize, 0)
    {
        socket.register_b_transport(this, &benchmark_ram::b_transport);
    }

private:
    std::vector<unsigned char> storage_;

    void b_transport(
        tlm::tlm_generic_payload& trans, sc_core::sc_time& delay)
    {
        const auto address = trans.get_address();
        const auto length = trans.get_data_length();
        delay += sc_core::sc_time(2, sc_core::SC_NS);
        if (address > storage_.size()
            || length > storage_.size() - static_cast<std::size_t>(address)) {
            trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
            return;
        }
        for (unsigned index = 0; index < length; ++index) {
            auto& stored =
                storage_[static_cast<std::size_t>(address) + index];
            if (trans.is_write()) {
                stored = trans.get_data_ptr()[index];
            } else {
                trans.get_data_ptr()[index] = stored;
            }
        }
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
    }
};

struct benchmark_control {
    sc_core::sc_event progress;
    sc_core::sc_event start_measurement;
    unsigned manager_count = 0;
    unsigned warmup_done = 0;
    unsigned measured_done = 0;
    bool measurement_active = false;
    std::uint64_t offered_transactions = 0;
    std::uint64_t offered_bytes = 0;
    std::uint64_t measurement_start_cycle = 0;
    std::array<unsigned, kMaximumManagers> workers_done{};
    std::array<std::uint64_t, kMaximumManagers> manager_end_cycle{};
};

class benchmark_source : public sc_core::sc_module {
public:
    tlm_utils::simple_initiator_socket<benchmark_source> socket;
    benchmark_control* control = nullptr;
    const options* config = nullptr;
    unsigned port = 0;

    SC_HAS_PROCESS(benchmark_source);

    explicit benchmark_source(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
        , socket("socket")
    {
        SC_THREAD(worker_zero);
        SC_THREAD(worker_one);
        SC_THREAD(worker_two);
        SC_THREAD(worker_three);
        SC_THREAD(worker_four);
        SC_THREAD(worker_five);
        SC_THREAD(worker_six);
        SC_THREAD(worker_seven);
    }

private:
    static std::uint32_t random_word(std::uint32_t& state)
    {
        std::uint32_t value = state;
        value ^= value << 13;
        value ^= value >> 17;
        value ^= value << 5;
        state = value == 0 ? 0x9E37'79B9u : value;
        return state;
    }

    void transact(
        unsigned worker, unsigned ordinal, bool measured,
        std::uint32_t& random_state)
    {
        const bool write =
            random_word(random_state) % 100 >= config->read_percent;
        const std::uint64_t lane =
            (static_cast<std::uint64_t>(port) * 0x2000
             + static_cast<std::uint64_t>(worker) * 0x400
             + static_cast<std::uint64_t>(ordinal) * 64)
            % (kRamSize - config->burst_bytes);
        std::vector<unsigned char> bytes(config->burst_bytes);
        for (unsigned index = 0; index < bytes.size(); ++index) {
            bytes[index] = static_cast<unsigned char>(
                random_word(random_state)
                ^ (port * 37u + worker * 13u + ordinal + index));
        }

        tlm::tlm_generic_payload payload;
        payload.set_command(
            write ? tlm::TLM_WRITE_COMMAND : tlm::TLM_READ_COMMAND);
        payload.set_address(kRamBase + lane);
        payload.set_data_ptr(bytes.data());
        payload.set_data_length(static_cast<unsigned>(bytes.size()));
        payload.set_streaming_width(static_cast<unsigned>(bytes.size()));
        payload.set_byte_enable_ptr(nullptr);
        payload.set_byte_enable_length(0);
        payload.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

        if (measured) {
            ++control->offered_transactions;
            control->offered_bytes += bytes.size();
        }
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
        socket->b_transport(payload, delay);
        if (delay != sc_core::SC_ZERO_TIME) {
            wait(delay);
        }
        if (!payload.is_response_ok()) {
            throw std::runtime_error(
                "noc_benchmark: RAM transaction was rejected");
        }
    }

    void run_worker(unsigned worker)
    {
        if (worker >= config->workers_per_manager) {
            return;
        }
        std::uint32_t random_state =
            config->seed
            ^ (0xA511'E9B3u * static_cast<std::uint32_t>(port + 1))
            ^ (0x63D8'35A5u * static_cast<std::uint32_t>(worker + 1));
        if (random_state == 0) random_state = 1;
        for (unsigned index = 0; index < config->warmup_transactions; ++index) {
            transact(worker, index, false, random_state);
        }
        ++control->warmup_done;
        control->progress.notify(sc_core::SC_ZERO_TIME);
        wait(control->start_measurement);

        // Warm-up must change neither the measured traffic sequence nor its
        // counters.  Use a separate deterministic stream at the signed window
        // boundary so `--warmup-transactions 0` and `N` produce identical
        // measured evidence for the same seed/config.
        random_state =
            config->seed
            ^ (0xC2B2'AE35u * static_cast<std::uint32_t>(port + 1))
            ^ (0x27D4'EB2Fu * static_cast<std::uint32_t>(worker + 1));
        if (random_state == 0) random_state = 1;
        for (unsigned index = 0; index < config->transactions; ++index) {
            transact(
                worker, config->warmup_transactions + index, true,
                random_state);
            if (index + 1 != config->transactions
                && config->injection_gap_cycles != 0) {
                wait(sc_core::sc_time(
                    config->injection_gap_cycles, sc_core::SC_NS));
            }
        }
        ++control->measured_done;
        ++control->workers_done[port];
        if (control->workers_done[port] == config->workers_per_manager) {
            control->manager_end_cycle[port] =
                static_cast<std::uint64_t>(
                    sc_core::sc_time_stamp()
                    / sc_core::sc_time(kClockPeriodNs, sc_core::SC_NS));
        }
        control->progress.notify(sc_core::SC_ZERO_TIME);
    }

    void worker_zero() { run_worker(0); }
    void worker_one() { run_worker(1); }
    void worker_two() { run_worker(2); }
    void worker_three() { run_worker(3); }
    void worker_four() { run_worker(4); }
    void worker_five() { run_worker(5); }
    void worker_six() { run_worker(6); }
    void worker_seven() { run_worker(7); }
};

struct mesh_summary {
    double peak_utilisation = 0.0;
    double peak_stall = 0.0;
    unsigned input_high_water = 0;
    unsigned output_high_water = 0;
};

mesh_summary summarise_mesh(
    const floo::model::mesh_counter_snapshot& mesh,
    std::uint64_t measurement_cycles)
{
    mesh_summary result{};
    for (const auto& router : mesh.routers) {
        for (unsigned port = 0;
             port < floo::model::router_counter_snapshot::num_ports; ++port) {
            result.input_high_water = std::max(
                result.input_high_water,
                router.input_buffers[port].high_water);
            result.output_high_water = std::max(
                result.output_high_water,
                router.output_buffers[port].high_water);
            if (port == floo::model::to_port(floo::model::direction::eject)) {
                continue;
            }
            const auto& output = router.outputs[port];
            const double utilisation = measurement_cycles == 0
                ? 0.0
                : static_cast<double>(output.accepted_flits)
                    / static_cast<double>(measurement_cycles);
            const double stall = output.busy_cycles == 0
                ? 0.0
                : static_cast<double>(output.stall_cycles)
                    / static_cast<double>(output.busy_cycles);
            result.peak_utilisation =
                std::max(result.peak_utilisation, utilisation);
            result.peak_stall = std::max(result.peak_stall, stall);
        }
    }
    return result;
}

std::string json_escape(const std::string& value)
{
    std::ostringstream escaped;
    escaped << '"';
    for (const unsigned char byte : value) {
        switch (byte) {
        case '"': escaped << "\\\""; break;
        case '\\': escaped << "\\\\"; break;
        case '\n': escaped << "\\n"; break;
        case '\r': escaped << "\\r"; break;
        case '\t': escaped << "\\t"; break;
        default:
            if (byte < 0x20) {
                escaped << "\\u00" << std::hex << std::setw(2)
                        << std::setfill('0') << static_cast<unsigned>(byte)
                        << std::dec << std::setfill(' ');
            } else {
                escaped << static_cast<char>(byte);
            }
        }
    }
    escaped << '"';
    return escaped.str();
}

std::string generated_utc()
{
    const auto now = std::chrono::system_clock::now();
    const std::time_t epoch = std::chrono::system_clock::to_time_t(now);
    const std::tm* converted = std::gmtime(&epoch);
    if (converted == nullptr) {
        return "unavailable";
    }
    std::ostringstream text;
    text << std::put_time(converted, "%Y-%m-%dT%H:%M:%SZ");
    return text.str();
}

void write_metric(
    std::ostream& out, double value, const char* unit, const char* source,
    std::uint64_t samples = 0, const char* formula = nullptr)
{
    out << "{\"value\":" << std::setprecision(12) << value
        << ",\"unit\":" << json_escape(unit)
        << ",\"source\":" << json_escape(source);
    if (samples != 0) {
        out << ",\"samples\":" << samples;
    }
    if (formula != nullptr) {
        out << ",\"formula\":" << json_escape(formula);
    }
    out << '}';
}

void write_null_metric(
    std::ostream& out, const char* unit, const char* source)
{
    out << "{\"value\":null,\"unit\":" << json_escape(unit)
        << ",\"source\":" << json_escape(source) << '}';
}

void write_transaction_metrics(
    std::ostream& out, const transaction_metrics& metrics,
    const std::string& name = {})
{
    out << '{';
    if (!name.empty()) {
        out << "\"name\":" << json_escape(name) << ',';
    }
    out << "\"transactions\":";
    write_metric(
        out, static_cast<double>(metrics.transactions()), "transactions", "M",
        metrics.transactions());
    out << ",\"payload_bytes\":";
    write_metric(
        out, static_cast<double>(metrics.payload_bytes()), "bytes", "M",
        metrics.transactions());
    out << ",\"latency_cycles\":{";
    const auto& latency = metrics.latency();
    const std::array<const char*, 6> names{{
        "min", "mean", "p50", "p95", "p99", "max"}};
    if (metrics.transactions() == 0) {
        for (unsigned index = 0; index < names.size(); ++index) {
            if (index != 0) out << ',';
            out << json_escape(names[index]) << ':';
            write_null_metric(out, "cycles", "M");
        }
    } else {
        const std::array<double, 6> values{{
            static_cast<double>(latency.min()),
            latency.mean(),
            static_cast<double>(latency.percentile(50, 100)),
            static_cast<double>(latency.percentile(95, 100)),
            static_cast<double>(latency.percentile(99, 100)),
            static_cast<double>(latency.max()),
        }};
        for (unsigned index = 0; index < names.size(); ++index) {
            if (index != 0) out << ',';
            out << json_escape(names[index]) << ':';
            write_metric(
                out, values[index], "cycles", "M", metrics.transactions());
        }
    }
    out << "}}";
}

void write_mesh(
    std::ostream& out, const char* channel,
    const floo::model::mesh_counter_snapshot& mesh)
{
    out << "{\"channel\":" << json_escape(channel)
        << ",\"width\":" << mesh.width
        << ",\"height\":" << mesh.height
        << ",\"routers\":[";
    for (std::size_t index = 0; index < mesh.routers.size(); ++index) {
        if (index != 0) out << ',';
        const auto& router = mesh.routers[index];
        out << "{\"x\":" << index % mesh.width
            << ",\"y\":" << index / mesh.width
            << ",\"counted_cycles\":" << router.counted_cycles;
        const auto write_ports = [&](
            const char* name, const auto& ports, const auto& buffers) {
            out << ',' << json_escape(name) << ":[";
            for (unsigned port = 0;
                 port < floo::model::router_counter_snapshot::num_ports;
                 ++port) {
                if (port != 0) out << ',';
                out << "{\"port\":"
                    << json_escape(floo::model::to_string(
                           floo::model::direction_from_port(port)))
                    << ",\"accepted_flits\":"
                    << ports[port].accepted_flits
                    << ",\"accepted_packets\":"
                    << ports[port].accepted_packets
                    << ",\"stall_cycles\":" << ports[port].stall_cycles
                    << ",\"busy_cycles\":" << ports[port].busy_cycles
                    << ",\"occupancy_sum\":"
                    << buffers[port].occupancy_sum
                    << ",\"occupancy_high_water\":"
                    << buffers[port].high_water << '}';
            }
            out << ']';
        };
        write_ports("inputs", router.inputs, router.input_buffers);
        write_ports("outputs", router.outputs, router.output_buffers);
        out << '}';
    }
    out << "]}";
}

std::uint64_t boundary_flits(
    const floo::model::mesh_counter_snapshot& mesh, bool input)
{
    const unsigned eject =
        floo::model::to_port(floo::model::direction::eject);
    std::uint64_t total = 0;
    for (const auto& router : mesh.routers) {
        total += input
            ? router.inputs[eject].accepted_flits
            : router.outputs[eject].accepted_flits;
    }
    return total;
}

double jain_fairness(const std::vector<double>& delivered_rates)
{
    double sum = 0.0;
    double squares = 0.0;
    for (const double rate : delivered_rates) {
        sum += rate;
        squares += rate * rate;
    }
    return delivered_rates.empty() || squares == 0.0
        ? 0.0
        : sum * sum
            / (static_cast<double>(delivered_rates.size()) * squares);
}

class benchmark_top : public sc_core::sc_module {
public:
    SC_HAS_PROCESS(benchmark_top);

    benchmark_top(sc_core::sc_module_name name, options config)
        : sc_core::sc_module(name)
        , config_(std::move(config))
        , manager_count_(manager_count_for(config_.workload))
        , target_node_{config_.width - 1, config_.height - 1}
        , manager_nodes_(manager_nodes(
              config_.width, config_.height, target_node_, manager_count_))
        , noc_(
              "noc", config_.width, config_.height, 1, manager_count_,
              sc_core::sc_time(kClockPeriodNs, sc_core::SC_NS),
              noc_interconnect::default_max_outstanding_per_port,
              noc_interconnect::timing_mode::detailed)
        , ram_("ram")
        , manager_stats_(manager_count_)
    {
        control_.manager_count =
            manager_count_ * config_.workers_per_manager;
        for (unsigned index = 0; index < manager_count_; ++index) {
            auto source = std::make_unique<benchmark_source>(
                sc_core::sc_gen_unique_name("source"));
            source->control = &control_;
            source->config = &config_;
            source->port = index;
            noc_.place_initiator(index, manager_nodes_[index]);
            if (index == 0) {
                source->socket.bind(noc_.target_socket);
            } else {
                source->socket.bind(noc_.cpu_port(index));
            }
            sources_.push_back(std::move(source));
        }
        noc_.add_target(
                kRamBase, kRamSize, target_node_,
                noc_interconnect::target_kind::memory)
            .bind(ram_.socket);
        noc_.set_completion_observer(
            [this](const noc_interconnect::completion& completion) {
                if (!control_.measurement_active) {
                    return;
                }
                all_stats_.add(
                    completion.length, completion.latency_cycles);
                if (completion.port < manager_stats_.size()) {
                    manager_stats_[completion.port].add(
                        completion.length, completion.latency_cycles);
                }
            });
        SC_THREAD(control);
        SC_THREAD(watchdog);
    }

private:
    options config_;
    unsigned manager_count_;
    node target_node_;
    std::vector<node> manager_nodes_;
    noc_interconnect noc_;
    benchmark_ram ram_;
    benchmark_control control_;
    std::vector<std::unique_ptr<benchmark_source>> sources_;
    transaction_metrics all_stats_;
    std::vector<transaction_metrics> manager_stats_;
    std::uint64_t warmup_cycles_ = 0;
    std::uint64_t start_cycle_ = 0;
    std::uint64_t end_cycle_ = 0;
    bool complete_ = false;

    static std::uint64_t modeled_cycle()
    {
        return static_cast<std::uint64_t>(
            sc_core::sc_time_stamp()
            / sc_core::sc_time(kClockPeriodNs, sc_core::SC_NS));
    }

    void wait_for_progress(unsigned benchmark_control::*field, unsigned value)
    {
        while (control_.*field != value) {
            wait(control_.progress);
        }
    }

    void wait_for_drain()
    {
        constexpr unsigned limit = 100'000;
        for (unsigned cycle = 0; cycle < limit; ++cycle) {
            if (noc_.mesh_quiescent() && noc_.wrapper_idle()) {
                return;
            }
            wait(sc_core::sc_time(1, sc_core::SC_NS));
        }
        throw std::runtime_error(
            "noc_benchmark: network did not drain within 100000 cycles");
    }

    void require_consistent(
        const noc_interconnect::detailed_counters& counters) const
    {
        if (!all_stats_.valid()
            || std::any_of(
                manager_stats_.begin(), manager_stats_.end(),
                [](const transaction_metrics& stats) {
                    return !stats.valid();
                })) {
            throw std::runtime_error(
                "noc_benchmark: metric overflow or invalid histogram");
        }
        std::uint64_t transactions = 0;
        std::uint64_t bytes = 0;
        for (const auto& manager : manager_stats_) {
            transactions += manager.transactions();
            bytes += manager.payload_bytes();
        }
        if (transactions != all_stats_.transactions()
            || bytes != all_stats_.payload_bytes()) {
            throw std::runtime_error(
                "noc_benchmark: manager buckets do not reconcile");
        }
        if (control_.offered_transactions != all_stats_.transactions()
            || control_.offered_bytes != all_stats_.payload_bytes()) {
            throw std::runtime_error(
                "noc_benchmark: offered and delivered traffic differ");
        }
        if (!noc_.mesh_quiescent() || !noc_.wrapper_idle()) {
            throw std::runtime_error(
                "noc_benchmark: incomplete measurement was accepted");
        }
        for (const auto* mesh : {&counters.request, &counters.response}) {
            if (boundary_flits(*mesh, true)
                != boundary_flits(*mesh, false)) {
                throw std::runtime_error(
                    "noc_benchmark: injected/ejected flits do not conserve");
            }
        }
    }

    void write_json(
        const noc_interconnect::detailed_counters& counters) const
    {
        const std::string temporary = config_.metrics_path + ".tmp";
        std::ofstream out(temporary, std::ios::trunc);
        if (!out) {
            throw std::runtime_error(
                "noc_benchmark: cannot create metrics output");
        }
        const std::uint64_t counted_cycles = end_cycle_ - start_cycle_;
        const std::uint64_t mesh_active_cycles =
            counters.request.routers.empty()
                ? 0 : counters.request.routers.front().counted_cycles;
        const auto request =
            summarise_mesh(counters.request, counted_cycles);
        const auto response =
            summarise_mesh(counters.response, counted_cycles);
        const double seconds =
            static_cast<double>(counted_cycles) * kClockPeriodNs * 1e-9;
        const double throughput = seconds == 0.0 ? 0.0
            : static_cast<double>(all_stats_.transactions()) / seconds / 1e6;
        const double bandwidth = seconds == 0.0 ? 0.0
            : static_cast<double>(all_stats_.payload_bytes()) / seconds / 1e9;
        double hop_sum = 0.0;
        for (unsigned index = 0; index < manager_count_; ++index) {
            const unsigned hops =
                static_cast<unsigned>(
                    std::max(manager_nodes_[index].x, target_node_.x)
                    - std::min(manager_nodes_[index].x, target_node_.x)
                    + std::max(manager_nodes_[index].y, target_node_.y)
                    - std::min(manager_nodes_[index].y, target_node_.y));
            hop_sum += static_cast<double>(hops)
                * static_cast<double>(manager_stats_[index].transactions());
        }
        const double average_hops = all_stats_.transactions() == 0 ? 0.0
            : hop_sum / static_cast<double>(all_stats_.transactions());
        std::vector<double> manager_rates;
        manager_rates.reserve(manager_count_);
        for (unsigned index = 0; index < manager_count_; ++index) {
            const auto end = control_.manager_end_cycle[index];
            const auto active_cycles =
                end > control_.measurement_start_cycle
                ? end - control_.measurement_start_cycle : 0;
            manager_rates.push_back(
                active_cycles == 0 ? 0.0
                : static_cast<double>(manager_stats_[index].payload_bytes())
                    / static_cast<double>(active_cycles));
        }

        out << "{\n\"schema\":\"floo-noc-metrics-v1\",\n"
            << "\"provenance\":{"
            << "\"generated_utc\":" << json_escape(generated_utc()) << ','
            << "\"git_revision\":null,\"git_dirty\":null,"
            << "\"source_patch_sha256\":null,"
            << "\"platform_binary\":\"unavailable\","
            << "\"platform_sha256\":null,\"firmware\":\"none\","
            << "\"firmware_sha256\":null,"
            << "\"build_type\":" << json_escape(kBuildType) << ','
            << "\"host_cc\":\"unavailable\","
            << "\"host_cxx\":\"unavailable\","
            << "\"systemc_version\":" << json_escape(sc_core::sc_version())
            << "},\n\"configuration\":{"
            << "\"timing_mode\":\"detailed\","
            << "\"topology\":{\"width\":" << config_.width
            << ",\"height\":" << config_.height << "},"
            << "\"routing\":\"XY\","
            << "\"port_order\":[\"North\",\"East\",\"South\",\"West\","
               "\"Eject\"],"
            << "\"clock_period_ns\":" << kClockPeriodNs << ','
            << "\"input_fifo_depth\":2,\"output_fifo_depth\":2,"
            << "\"max_outstanding_per_manager\":"
            << noc_interconnect::default_max_outstanding_per_port
            << ",\"managers\":[";
        for (unsigned index = 0; index < manager_count_; ++index) {
            if (index != 0) out << ',';
            out << "{\"name\":\"manager" << index
                << "\",\"x\":" << manager_nodes_[index].x
                << ",\"y\":" << manager_nodes_[index].y << '}';
        }
        out << "],\"targets\":[{\"name\":\"ram\",\"x\":"
            << target_node_.x << ",\"y\":" << target_node_.y << "}]},\n"
            << "\"workload\":{\"name\":" << json_escape(config_.workload)
            << ",\"kind\":\"synthetic\",\"seed\":" << config_.seed
            << ",\"transactions_per_worker\":" << config_.transactions
            << ",\"workers_per_manager\":" << config_.workers_per_manager
            << ",\"injection_gap_cycles\":"
            << config_.injection_gap_cycles
            << ",\"read_percent\":" << config_.read_percent
            << ",\"burst_bytes\":" << config_.burst_bytes
            << ",\"offered_transactions\":"
            << control_.offered_transactions << "},\n"
            << "\"measurement_window\":{\"warmup_cycles\":"
            << warmup_cycles_ << ",\"start_cycle\":" << start_cycle_
            << ",\"end_cycle\":" << end_cycle_
            << ",\"counted_cycles\":" << counted_cycles
            << ",\"drained\":true},\n"
            << "\"transaction_metrics\":{\"global\":";
        write_transaction_metrics(out, all_stats_);
        out << ",\"classes\":[";
        for (unsigned index = 0; index < manager_count_; ++index) {
            if (index != 0) out << ',';
            write_transaction_metrics(
                out, manager_stats_[index],
                "manager_" + std::to_string(index));
        }
        for (unsigned index = 0; index < manager_count_; ++index) {
            out << ',';
            write_transaction_metrics(
                out, manager_stats_[index],
                "flow_manager" + std::to_string(index) + "_ram");
        }
        out << "]},\n\"traffic_accounting\":{"
            << "\"offered_transactions\":";
        write_metric(
            out, static_cast<double>(control_.offered_transactions),
            "transactions", "M", control_.offered_transactions);
        out << ",\"delivered_transactions\":";
        write_metric(
            out, static_cast<double>(all_stats_.transactions()),
            "transactions", "M", all_stats_.transactions());
        out << "},\n\"physical_meshes\":[";
        write_mesh(out, "request", counters.request);
        out << ',';
        write_mesh(out, "response", counters.response);
        out << "],\n\"derived_metrics\":{"
            << "\"transaction_throughput_mtrans_s\":";
        write_metric(
            out, throughput, "Mtrans/s", "D", all_stats_.transactions(),
            "delivered transactions / counted cycles / clock period");
        out << ",\"payload_bandwidth_gb_s\":";
        write_metric(
            out, bandwidth, "GB/s", "D", all_stats_.transactions(),
            "delivered payload bytes / counted cycles / clock period");
        out << ",\"request_peak_link_utilisation\":";
        write_metric(
            out, request.peak_utilisation * 100.0, "%", "D", 0,
            "accepted flits / counted cycles");
        out << ",\"response_peak_link_utilisation\":";
        write_metric(
            out, response.peak_utilisation * 100.0, "%", "D", 0,
            "accepted flits / counted cycles");
        out << ",\"request_peak_stall_ratio\":";
        write_metric(
            out, request.peak_stall * 100.0, "%", "D", 0,
            "stall cycles / busy cycles");
        out << ",\"response_peak_stall_ratio\":";
        write_metric(
            out, response.peak_stall * 100.0, "%", "D", 0,
            "stall cycles / busy cycles");
        out << ",\"jain_fairness\":";
        write_metric(
            out, jain_fairness(manager_rates), "ratio", "D",
            manager_count_,
            "square(sum manager delivered byte rates) / "
            "(manager count * sum square(manager delivered byte rates)); "
            "rate = bytes / manager active cycles");
        out << ",\"average_hop_count\":";
        write_metric(
            out, average_hops, "hops", "D", all_stats_.transactions(),
            "sum(manager transactions * Manhattan distance) / "
            "delivered transactions");
        out << ",\"clock_active_ratio\":";
        write_metric(
            out,
            counted_cycles == 0 ? 0.0
                : static_cast<double>(mesh_active_cycles)
                    / static_cast<double>(counted_cycles),
            "ratio", "D", counted_cycles,
            "mesh active cycles / modeled measurement cycles");
        out << ",\"clock_gating_ratio\":";
        write_metric(
            out,
            counted_cycles == 0 ? 0.0
                : 1.0 - static_cast<double>(mesh_active_cycles)
                    / static_cast<double>(counted_cycles),
            "ratio", "D", counted_cycles,
            "1 - clock active ratio");
        out << "},\n\"availability\":{\"router_counters\":true,"
               "\"area\":false,\"power\":false,\"energy_per_flit\":false},\n"
            << "\"warnings\":["
            << "\"injection-gap is a post-completion source gap; each worker "
               "is one blocking issuer and workers_per_manager is explicit\","
            << "\"area, power and energy are unavailable pending calibrated "
               "RTL evidence\"]\n}\n";
        out.flush();
        if (!out) {
            std::remove(temporary.c_str());
            throw std::runtime_error(
                "noc_benchmark: failed while writing metrics output");
        }
        out.close();
        if (std::rename(temporary.c_str(), config_.metrics_path.c_str()) != 0) {
            std::remove(temporary.c_str());
            throw std::runtime_error(
                "noc_benchmark: cannot atomically publish metrics output");
        }
    }

    void control()
    {
        wait_for_progress(
            &benchmark_control::warmup_done, control_.manager_count);
        wait_for_drain();
        warmup_cycles_ = modeled_cycle();
        noc_.reset_detailed_counters();
        start_cycle_ = modeled_cycle();
        control_.measurement_start_cycle = start_cycle_;
        control_.measurement_active = true;
        control_.start_measurement.notify(sc_core::SC_ZERO_TIME);

        wait_for_progress(
            &benchmark_control::measured_done, control_.manager_count);
        control_.measurement_active = false;
        wait_for_drain();
        end_cycle_ = modeled_cycle();
        const auto counters = noc_.detailed_counter_snapshot();
        require_consistent(counters);
        write_json(counters);
        complete_ = true;
        std::cout
            << "noc_benchmark PASS"
            << " workload=" << config_.workload
            << " topology=" << config_.width << 'x' << config_.height
            << " managers=" << manager_count_
            << " workers_per_manager=" << config_.workers_per_manager
            << " offered=" << control_.offered_transactions
            << " delivered=" << all_stats_.transactions()
            << " drained=yes metrics=" << config_.metrics_path << '\n';
        sc_core::sc_stop();
    }

    void watchdog()
    {
        wait(sc_core::sc_time(100, sc_core::SC_MS));
        if (!complete_) {
            throw std::runtime_error(
                "noc_benchmark: watchdog expired before drain/report");
        }
    }
};

} // namespace

int sc_main(int argc, char* argv[])
{
    try {
        options config = parse_options(argc, argv);
        if (config.help) {
            std::cout << kUsage << '\n';
            return 0;
        }
        benchmark_top top("noc_benchmark", std::move(config));
        sc_core::sc_start();
    } catch (const std::exception& error) {
        std::cerr << "noc_benchmark: " << error.what() << '\n'
                  << kUsage << '\n';
        return 1;
    }
    return 0;
}
