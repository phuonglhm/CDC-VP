#include "noc_soc_top.h"

#include <cmath>
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
    bool mode_seen = false;
    bool timing_seen = false;
    bool help = false;
};

const char* usage =
    "usage: noc_soc --mode survey|firmware [--fw image.elf] "
    "[--noc-timing detailed|fast] [--sim-us N] [-c config.yaml]";

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
            args.firmware, args.sim_us);
        sc_core::sc_start();
    } catch (const std::exception& error) {
        std::cerr << "noc_soc: " << error.what() << '\n'
                  << usage << '\n';
        return 1;
    }

    return 0;
}
