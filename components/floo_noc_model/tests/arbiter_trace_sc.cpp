// SPDX-License-Identifier: SHL-0.51
//
// SystemC side of the wormhole-arbiter cross-check. It replays the shared CSV
// stimulus through the model and writes the cycle trace that
// `rtl_crosscheck/run_wormhole_arbiter_crosscheck.sh` compares against the
// unmodified frozen RTL.
//
// Traced: the handshake outputs plus the arbiter's registered state, sampled
// both before and after the clock edge.

#include "floo_noc_model/floo_types.hpp"
#include "floo_noc_model/wormhole_arbiter.hpp"

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

using flit_t = floo::model::test_flit;

constexpr unsigned stimulus_routes = 5;

struct stimulus {
    unsigned cycle{};
    bool rst_n{};
    bool ready{};
    unsigned valid_mask{};
    unsigned last_mask{};
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

unsigned parse_integer(
    const std::string& value,
    int base,
    const std::string& path,
    unsigned line_number)
{
    std::size_t parsed = 0;
    const auto result = std::stoul(value, &parsed, base);
    if (parsed != value.size()) {
        throw std::runtime_error(
            path + ':' + std::to_string(line_number)
            + ": invalid integer '" + value + "'");
    }
    return static_cast<unsigned>(result);
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

    const std::string expected_header = "cycle,rst_n,ready_i,valid_i,last_i";
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
            parse_integer(fields[0], 10, path, line_number),
            parse_integer(fields[1], 10, path, line_number) != 0,
            parse_integer(fields[2], 10, path, line_number) != 0,
            parse_integer(fields[3], 16, path, line_number),
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

template <unsigned NumRoutes>
void run(const std::vector<stimulus>& stimuli, std::ostream& trace)
{
    static_assert(NumRoutes <= stimulus_routes,
                  "stimulus carries only five routes");

    sc_core::sc_signal<bool> clk{"clk"};
    sc_core::sc_signal<bool> rst_n{"rst_n"};
    sc_core::sc_vector<sc_core::sc_signal<flit_t>> in_data{
        "in_data", NumRoutes};
    sc_core::sc_vector<sc_core::sc_signal<bool>> in_valid{
        "in_valid", NumRoutes};
    sc_core::sc_vector<sc_core::sc_signal<bool>> in_ready{
        "in_ready", NumRoutes};
    sc_core::sc_signal<flit_t> out_data{"out_data"};
    sc_core::sc_signal<bool> out_valid{"out_valid"};
    sc_core::sc_signal<bool> out_ready{"out_ready"};
    sc_core::sc_signal<unsigned> selected{"selected"};
    sc_core::sc_signal<bool> locked{"locked"};

    floo::model::wormhole_arbiter<flit_t, NumRoutes> dut{"dut"};
    dut.i_clk(clk);
    dut.i_rst_n(rst_n);
    for (unsigned route = 0; route < NumRoutes; ++route) {
        dut.i_data[route](in_data[route]);
        dut.i_valid[route](in_valid[route]);
        dut.o_ready[route](in_ready[route]);
    }
    dut.o_data(out_data);
    dut.o_valid(out_valid);
    dut.i_ready(out_ready);
    dut.o_selected(selected);
    dut.o_locked(locked);

    trace << "cycle,pre_ready,pre_valid,pre_data,pre_selected,"
             "post_ready,post_valid,post_data,post_selected,"
             "valid_q,last_q,rr_q,lock_q,req_q\n";

    const auto ready_mask = [&]() {
        unsigned mask = 0;
        for (unsigned route = 0; route < NumRoutes; ++route) {
            if (in_ready[route].read()) {
                mask |= 1u << route;
            }
        }
        return mask;
    };

    clk.write(false);
    rst_n.write(false);
    out_ready.write(false);
    sc_core::sc_start(sc_core::SC_ZERO_TIME);

    for (const auto& row : stimuli) {
        clk.write(false);
        rst_n.write(row.rst_n);
        out_ready.write(row.ready);
        for (unsigned route = 0; route < NumRoutes; ++route) {
            flit_t flit{};
            flit.hdr.last = ((row.last_mask >> route) & 1u) != 0;
            flit.payload = (row.cycle + 1) * 16 + route;
            in_data[route].write(flit);
            in_valid[route].write(((row.valid_mask >> route) & 1u) != 0);
        }
        sc_core::sc_start(sc_core::sc_time(1, sc_core::SC_NS));

        const unsigned pre_ready = ready_mask();
        const unsigned pre_valid = out_valid.read();
        const std::uint64_t pre_data = out_data.read().payload.to_uint64();
        const unsigned pre_selected = selected.read();

        clk.write(true);
        sc_core::sc_start(sc_core::sc_time(1, sc_core::SC_NS));

        trace << row.cycle << ','
              << std::hex << pre_ready << std::dec << ','
              << pre_valid << ','
              << std::hex << pre_data << std::dec << ','
              << pre_selected << ','
              << std::hex << ready_mask() << std::dec << ','
              << static_cast<unsigned>(out_valid.read()) << ','
              << std::hex << out_data.read().payload.to_uint64() << std::dec
              << ',' << selected.read() << ','
              << std::hex << dut.valid_q() << std::dec << ','
              << static_cast<unsigned>(dut.last_q()) << ','
              << dut.rr_q() << ','
              << static_cast<unsigned>(dut.lock_q()) << ','
              << std::hex << dut.req_q() << std::dec << '\n';

        clk.write(false);
        sc_core::sc_start(sc_core::sc_time(1, sc_core::SC_NS));
    }
}

} // namespace

int sc_main(int argc, char* argv[])
{
    if (argc < 4 || argc > 5) {
        std::cerr << "usage: " << argv[0]
                  << " <stimulus.csv> <actual.csv> <num-routes>"
                     " [expected.csv]\n";
        return 2;
    }

    try {
        const auto stimuli = read_stimuli(argv[1]);
        const std::string routes_arg = argv[3];

        std::ofstream trace(argv[2]);
        if (!trace) {
            throw std::runtime_error(
                "cannot create SystemC trace file: " + std::string(argv[2]));
        }

        if (routes_arg == "5") {
            run<5>(stimuli, trace);
        } else if (routes_arg == "4") {
            run<4>(stimuli, trace);
        } else if (routes_arg == "2") {
            run<2>(stimuli, trace);
        } else {
            throw std::runtime_error(
                "unsupported route count '" + routes_arg
                + "' (expected 2, 4, or 5)");
        }
        trace.close();

        if (argc == 5 && !compare_trace(argv[2], argv[4])) {
            return 1;
        }
        std::cout << "arbiter_trace_sc PASS (routes " << routes_arg << ")\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "arbiter_trace_sc: " << error.what() << '\n';
        return 2;
    }
}
