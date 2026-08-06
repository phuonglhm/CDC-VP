#include "noc_soc_top.h"

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

#include <systemc>

namespace {

using cdc::platforms::noc_soc::noc_soc_mode;
using cdc::platforms::noc_soc::noc_timing_mode;

struct options {
    std::string config_path = "platforms/noc_soc/configs/default.yaml";
    std::string firmware;
    noc_soc_mode mode = noc_soc_mode::survey;
    noc_timing_mode timing = noc_timing_mode::detailed;
    double sim_us = 0.0;
    bool measure_baseline = false;
    std::string metrics_path;
    int uart0_socket = -1;
    bool uart0_wait = false;
    std::string uart0_rx_file;
    std::uint64_t uart0_rx_delay_us = 0;
    bool mode_seen = false;
    bool timing_seen = false;
    bool uart0_socket_seen = false;
    bool uart0_rx_file_seen = false;
    bool uart0_rx_delay_seen = false;
    bool metrics_path_seen = false;
    bool help = false;
};

const char* usage =
    "usage: noc_soc --mode survey|firmware [--fw image.elf] "
    "[--noc-timing detailed|fast] [--sim-us N] [--noc-baseline] "
    "[--noc-metrics FILE] "
    "[--uart0-socket PORT [--uart0-wait]] "
    "[--uart0-rx-file FILE [--uart0-rx-delay-us N]] "
    "[-c config.yaml]";

std::string require_value(
    int& index, int argc, char* argv[], const std::string& option)
{
    if (index + 1 >= argc) {
        throw std::invalid_argument(option + " requires a value");
    }
    return argv[++index];
}

double parse_sim_us(const std::string& text)
{
    std::size_t consumed = 0;
    const double value = std::stod(text, &consumed);
    if (consumed != text.size() || !std::isfinite(value) || value < 0.0) {
        throw std::invalid_argument(
            "--sim-us must be a finite, non-negative number");
    }
    return value;
}

std::uint64_t parse_unsigned(
    const std::string& text, const std::string& option,
    std::uint64_t maximum)
{
    if (text.empty() ||
        text.find_first_not_of("0123456789") != std::string::npos) {
        throw std::invalid_argument(
            option + " must be an unsigned decimal number");
    }
    std::size_t consumed = 0;
    std::uint64_t value = 0;
    try {
        value = std::stoull(text, &consumed);
    } catch (const std::exception&) {
        throw std::invalid_argument(option + " is out of range");
    }
    if (consumed != text.size() || value > maximum) {
        throw std::invalid_argument(option + " is out of range");
    }
    return value;
}

options parse_options(int argc, char* argv[])
{
    options result;
    for (int index = 1; index < argc; ++index) {
        const std::string arg = argv[index];
        if (arg == "-h" || arg == "--help") {
            result.help = true;
        } else if (arg == "-c" || arg == "--config") {
            result.config_path = require_value(index, argc, argv, arg);
        } else if (arg == "--mode") {
            if (result.mode_seen) {
                throw std::invalid_argument("--mode may be specified only once");
            }
            const auto value = require_value(index, argc, argv, arg);
            if (value == "survey") {
                result.mode = noc_soc_mode::survey;
            } else if (value == "firmware") {
                result.mode = noc_soc_mode::firmware;
            } else {
                throw std::invalid_argument(
                    "--mode must be 'survey' or 'firmware'");
            }
            result.mode_seen = true;
        } else if (arg == "--fw") {
            result.firmware = require_value(index, argc, argv, arg);
        } else if (arg == "--noc-timing") {
            if (result.timing_seen) {
                throw std::invalid_argument(
                    "--noc-timing may be specified only once");
            }
            const auto value = require_value(index, argc, argv, arg);
            if (value == "detailed") {
                result.timing = noc_timing_mode::detailed;
            } else if (value == "fast") {
                result.timing = noc_timing_mode::fast;
            } else {
                throw std::invalid_argument(
                    "--noc-timing must be 'detailed' or 'fast'");
            }
            result.timing_seen = true;
        } else if (arg == "--noc-baseline") {
            result.measure_baseline = true;
        } else if (arg == "--noc-metrics") {
            if (result.metrics_path_seen) {
                throw std::invalid_argument(
                    "--noc-metrics may be specified only once");
            }
            result.metrics_path = require_value(index, argc, argv, arg);
            if (result.metrics_path.empty()) {
                throw std::invalid_argument(
                    "--noc-metrics requires a non-empty path");
            }
            result.metrics_path_seen = true;
            result.measure_baseline = true;
        } else if (arg == "--uart0-socket") {
            if (result.uart0_socket_seen) {
                throw std::invalid_argument(
                    "--uart0-socket may be specified only once");
            }
            result.uart0_socket = static_cast<int>(parse_unsigned(
                require_value(index, argc, argv, arg), arg, 65535));
            result.uart0_socket_seen = true;
        } else if (arg == "--uart0-wait") {
            result.uart0_wait = true;
        } else if (arg == "--uart0-rx-file") {
            if (result.uart0_rx_file_seen) {
                throw std::invalid_argument(
                    "--uart0-rx-file may be specified only once");
            }
            result.uart0_rx_file = require_value(index, argc, argv, arg);
            result.uart0_rx_file_seen = true;
        } else if (arg == "--uart0-rx-delay-us") {
            if (result.uart0_rx_delay_seen) {
                throw std::invalid_argument(
                    "--uart0-rx-delay-us may be specified only once");
            }
            result.uart0_rx_delay_us = parse_unsigned(
                require_value(index, argc, argv, arg), arg,
                1'000'000'000'000ull);
            result.uart0_rx_delay_seen = true;
        } else if (arg == "--sim-us") {
            result.sim_us =
                parse_sim_us(require_value(index, argc, argv, arg));
        } else {
            throw std::invalid_argument("unknown option '" + arg + "'");
        }
    }

    if (result.help) {
        return result;
    }
    if (!result.mode_seen) {
        throw std::invalid_argument(
            "--mode is required: choose 'survey' or 'firmware'");
    }
    if (result.mode == noc_soc_mode::survey && !result.firmware.empty()) {
        throw std::invalid_argument("survey mode rejects --fw");
    }
    if (result.mode == noc_soc_mode::firmware && result.firmware.empty()) {
        throw std::invalid_argument(
            "firmware mode requires --fw <image.elf>");
    }
    // The baseline report is written from the firmware-mode path, and survey
    // mode owns its own synthetic traffic, so measuring there would classify
    // the probe's transactions as if they were a workload's.
    if (result.measure_baseline && result.mode != noc_soc_mode::firmware) {
        throw std::invalid_argument(
            "--noc-baseline requires --mode firmware");
    }
    if (result.metrics_path_seen
        && result.timing != noc_timing_mode::detailed) {
        throw std::invalid_argument(
            "--noc-metrics requires --noc-timing detailed");
    }
    if (result.uart0_wait && !result.uart0_socket_seen) {
        throw std::invalid_argument(
            "--uart0-wait requires --uart0-socket");
    }
    if (result.uart0_rx_delay_seen && !result.uart0_rx_file_seen) {
        throw std::invalid_argument(
            "--uart0-rx-delay-us requires --uart0-rx-file");
    }
    if ((result.uart0_socket_seen || result.uart0_rx_file_seen) &&
        result.mode != noc_soc_mode::firmware) {
        throw std::invalid_argument(
            "UART0 host input requires --mode firmware");
    }
    return result;
}

} // namespace

int sc_main(int argc, char* argv[])
{
    try {
        const auto args = parse_options(argc, argv);
        if (args.help) {
            std::cout << usage << '\n';
            return 0;
        }

        cdc::platforms::noc_soc::noc_soc_top top(
            "noc_soc", args.config_path, args.mode, args.timing,
            args.firmware, args.sim_us, args.measure_baseline,
            args.metrics_path);
        if (args.uart0_socket_seen) {
            top.set_uart0_socket(
                static_cast<std::uint16_t>(args.uart0_socket),
                args.uart0_wait);
        }
        if (args.uart0_rx_file_seen) {
            top.set_uart0_rx_file(
                args.uart0_rx_file, args.uart0_rx_delay_us);
        }
        sc_core::sc_start();
    } catch (const std::exception& error) {
        std::cerr << "noc_soc: " << error.what() << '\n'
                  << usage << '\n';
        return 1;
    }

    return 0;
}
