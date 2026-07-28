// SPDX-License-Identifier: SHL-0.51
//
// SystemC side of the `stream_fifo_optimal_wrap` cross-check. It replays the
// shared CSV stimulus through the model and writes the cycle trace that
// `rtl_crosscheck/run_stream_fifo_crosscheck.sh` compares against the
// unmodified common_cells RTL.
//
// Only the signals that `hw/floo_router.sv` actually connects are traced:
// `ready_o`, `valid_o`, and `data_o`. `usage_o` is left unconnected by the
// frozen router and is undefined ('x) in the depth-2 branch, so it is not part
// of the compared contract.

#include "floo_noc_model/stream_fifo.hpp"

#include <systemc>

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using data_t = sc_dt::sc_uint<64>;

struct stimulus {
    unsigned cycle{};
    bool rst_n{};
    bool valid{};
    bool ready{};
    std::uint64_t data{};
};

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

std::uint64_t parse_integer(
    const std::string& value,
    int base,
    const std::string& path,
    unsigned line_number)
{
    std::size_t parsed = 0;
    const auto result = std::stoull(value, &parsed, base);
    if (parsed != value.size()) {
        throw std::runtime_error(
            path + ':' + std::to_string(line_number)
            + ": invalid integer '" + value + "'");
    }
    return result;
}

std::vector<stimulus> read_stimuli(const std::string& path)
{
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("cannot open stimulus file: " + path);
    }

    std::string line;
    if (!std::getline(input, line)) {
        throw std::runtime_error("empty stimulus file: " + path);
    }

    const std::string expected_header = "cycle,rst_n,valid_i,ready_i,data_i";
    if (line != expected_header) {
        throw std::runtime_error(
            "unexpected stimulus header in " + path + ": " + line);
    }

    std::vector<stimulus> result;
    unsigned line_number = 1;
    while (std::getline(input, line)) {
        ++line_number;
        if (line.empty()) {
            continue;
        }

        const auto fields = split_csv(line);
        if (fields.size() != 5) {
            throw std::runtime_error(
                path + ':' + std::to_string(line_number)
                + ": expected 5 CSV fields");
        }

        result.push_back({
            static_cast<unsigned>(parse_integer(fields[0], 10, path, line_number)),
            parse_integer(fields[1], 10, path, line_number) != 0,
            parse_integer(fields[2], 10, path, line_number) != 0,
            parse_integer(fields[3], 10, path, line_number) != 0,
            parse_integer(fields[4], 16, path, line_number),
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
                      << ": actual='" << lhs
                      << "' expected='" << rhs << "'\n";
            match = false;
        }
    }
    return match;
}

template <unsigned Depth>
void run(const std::vector<stimulus>& stimuli, std::ostream& trace)
{
    sc_core::sc_signal<bool> clk{"clk"};
    sc_core::sc_signal<bool> rst_n{"rst_n"};
    sc_core::sc_signal<data_t> in_data{"in_data"};
    sc_core::sc_signal<bool> in_valid{"in_valid"};
    sc_core::sc_signal<bool> in_ready{"in_ready"};
    sc_core::sc_signal<data_t> out_data{"out_data"};
    sc_core::sc_signal<bool> out_valid{"out_valid"};
    sc_core::sc_signal<bool> out_ready{"out_ready"};
    sc_core::sc_signal<unsigned> occupancy{"occupancy"};

    floo::model::stream_fifo_optimal_wrap<data_t, Depth> dut{"dut"};
    dut.i_clk(clk);
    dut.i_rst_n(rst_n);
    dut.i_data(in_data);
    dut.i_valid(in_valid);
    dut.o_ready(in_ready);
    dut.o_data(out_data);
    dut.o_valid(out_valid);
    dut.i_ready(out_ready);
    dut.o_occupancy(occupancy);

    trace << "cycle,pre_ready,pre_valid,pre_data,"
             "post_ready,post_valid,post_data\n";

    clk.write(false);
    rst_n.write(false);
    in_valid.write(false);
    out_ready.write(false);
    sc_core::sc_start(sc_core::SC_ZERO_TIME);

    for (const auto& row : stimuli) {
        clk.write(false);
        rst_n.write(row.rst_n);
        in_data.write(data_t(row.data));
        in_valid.write(row.valid);
        out_ready.write(row.ready);
        sc_core::sc_start(sc_core::sc_time(1, sc_core::SC_NS));

        // Pre-edge: what the environment sees while deciding this cycle's
        // handshake. In both RTL branches these outputs are register functions,
        // so any dependence on the current inputs shows up as a mismatch here.
        const unsigned pre_ready = in_ready.read();
        const unsigned pre_valid = out_valid.read();
        const std::uint64_t pre_data = out_data.read().to_uint64();

        clk.write(true);
        sc_core::sc_start(sc_core::sc_time(1, sc_core::SC_NS));

        trace << row.cycle << ','
              << pre_ready << ',' << pre_valid << ','
              << std::hex << pre_data << std::dec << ','
              << static_cast<unsigned>(in_ready.read()) << ','
              << static_cast<unsigned>(out_valid.read()) << ','
              << std::hex << out_data.read().to_uint64() << std::dec << '\n';

        clk.write(false);
        sc_core::sc_start(sc_core::sc_time(1, sc_core::SC_NS));
    }
}

} // namespace

int sc_main(int argc, char* argv[])
{
    if (argc < 4 || argc > 5) {
        std::cerr << "usage: " << argv[0]
                  << " <stimulus.csv> <actual.csv> <depth> [expected.csv]\n";
        return 2;
    }

    try {
        const auto stimuli = read_stimuli(argv[1]);
        const std::string depth_arg = argv[3];

        std::ofstream trace(argv[2]);
        if (!trace) {
            throw std::runtime_error(
                "cannot create SystemC trace file: " + std::string(argv[2]));
        }

        if (depth_arg == "2") {
            run<2>(stimuli, trace);
        } else if (depth_arg == "4") {
            run<4>(stimuli, trace);
        } else {
            throw std::runtime_error(
                "unsupported depth '" + depth_arg + "' (expected 2 or 4)");
        }
        trace.close();

        if (argc == 5 && !compare_trace(argv[2], argv[4])) {
            return 1;
        }
        std::cout << "fifo_trace_sc PASS (depth " << depth_arg << ")\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "fifo_trace_sc: " << error.what() << '\n';
        return 2;
    }
}
