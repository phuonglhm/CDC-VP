// SPDX-License-Identifier: SHL-0.51
//
// SystemC side of the AXI sizing cross-check. It evaluates the model's mirror
// of the FlooNoC sizing functions over the same configuration list the RTL
// testbench reads, so the arithmetic is compared against the real
// `floo_pkg`/`axi_pkg` functions rather than against a transcription.
//
// This runner needs no simulation: the functions under test are pure. It still
// enters through `sc_main` because it links against SystemC for `sc_dt`.

#include "floo_noc_model/axi_types.hpp"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using floo::model::axi_cfg;
using floo::model::axi_channel;
using floo::model::physical_channel;

std::vector<std::string> split_csv(const std::string& line)
{
    std::vector<std::string> fields;
    std::istringstream stream(line);
    std::string field;
    while (std::getline(stream, field, ',')) {
        fields.push_back(field);
    }
    return fields;
}

std::vector<axi_cfg> read_configs(const std::string& path)
{
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("cannot open config file: " + path);
    }

    std::string line;
    if (!std::getline(input, line)) {
        throw std::runtime_error("empty config file: " + path);
    }
    if (line != "addr,data,user,in_id,out_id") {
        throw std::runtime_error("unexpected config header: " + line);
    }

    std::vector<axi_cfg> result;
    while (std::getline(input, line)) {
        if (line.empty()) {
            continue;
        }
        const auto fields = split_csv(line);
        if (fields.size() != 5) {
            throw std::runtime_error("expected 5 config fields: " + line);
        }
        result.push_back(axi_cfg{
            static_cast<unsigned>(std::stoul(fields[0])),
            static_cast<unsigned>(std::stoul(fields[1])),
            static_cast<unsigned>(std::stoul(fields[2])),
            static_cast<unsigned>(std::stoul(fields[3])),
            static_cast<unsigned>(std::stoul(fields[4])),
        });
    }
    return result;
}

std::vector<std::string> read_nonempty_lines(const std::string& path)
{
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("cannot open trace file: " + path);
    }
    std::vector<std::string> result;
    std::string line;
    while (std::getline(input, line)) {
        if (!line.empty()) {
            result.push_back(line);
        }
    }
    return result;
}

bool compare_trace(const std::string& actual, const std::string& expected)
{
    const auto actual_lines = read_nonempty_lines(actual);
    const auto expected_lines = read_nonempty_lines(expected);
    const auto count = std::max(actual_lines.size(), expected_lines.size());
    bool match = true;
    for (std::size_t index = 0; index < count; ++index) {
        const std::string lhs =
            index < actual_lines.size() ? actual_lines[index] : "<missing>";
        const std::string rhs =
            index < expected_lines.size() ? expected_lines[index] : "<missing>";
        if (lhs != rhs) {
            std::cerr << "trace mismatch at line " << index + 1
                      << ": actual='" << lhs << "' expected='" << rhs << "'\n";
            match = false;
        }
    }
    return match;
}

} // namespace

int sc_main(int argc, char* argv[])
{
    if (argc < 3 || argc > 4) {
        std::cerr << "usage: " << argv[0]
                  << " <configs.csv> <actual.csv> [expected.csv]\n";
        return 2;
    }

    try {
        const auto configs = read_configs(argv[1]);

        std::ofstream trace(argv[2]);
        if (!trace) {
            throw std::runtime_error(
                "cannot create trace file: " + std::string(argv[2]));
        }

        trace << "addr,data,user,in_id,out_id,"
                 "w_aw,w_w,w_ar,w_b,w_r,map_aw,map_w,map_ar,map_b,map_r,"
                 "max_req,max_rsp,rsvd_aw,rsvd_w,rsvd_ar,rsvd_b,rsvd_r\n";

        const axi_channel order[] = {axi_channel::aw, axi_channel::w,
                                     axi_channel::ar, axi_channel::b,
                                     axi_channel::r};

        for (const auto& cfg : configs) {
            trace << cfg.addr_width << ',' << cfg.data_width << ','
                  << cfg.user_width << ',' << cfg.in_id_width << ','
                  << cfg.out_id_width << ',';
            for (const auto channel : order) {
                trace << floo::model::get_axi_chan_width(cfg, channel) << ',';
            }
            for (const auto channel : order) {
                trace << static_cast<unsigned>(
                    floo::model::axi_chan_mapping(channel)) << ',';
            }
            trace << floo::model::get_max_axi_payload_bits(
                         cfg, physical_channel::req)
                  << ','
                  << floo::model::get_max_axi_payload_bits(
                         cfg, physical_channel::rsp);
            for (const auto channel : order) {
                trace << ',' << floo::model::get_axi_rsvd_bits(cfg, channel);
            }
            trace << '\n';
        }
        trace.close();

        if (argc == 4 && !compare_trace(argv[2], argv[3])) {
            return 1;
        }
        std::cout << "axi_sizing_trace PASS (" << configs.size()
                  << " configurations)\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "axi_sizing_trace: " << error.what() << '\n';
        return 2;
    }
}
