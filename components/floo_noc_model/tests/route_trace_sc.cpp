// SPDX-License-Identifier: SHL-0.51

#include "floo_noc_model/floo_types.hpp"
#include "floo_noc_model/xy_route_select.hpp"

#include <systemc>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

struct stimulus {
    unsigned cycle{};
    bool rst_n{};
    unsigned router_x{};
    unsigned router_y{};
    unsigned dst_x{};
    unsigned dst_y{};
    unsigned dst_port{};
    bool last{};
    bool valid{};
    bool ready{};
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

unsigned as_unsigned(
    const std::string& value,
    const std::string& path,
    unsigned line_number)
{
    std::size_t parsed = 0;
    const auto result = std::stoul(value, &parsed, 0);
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

    const std::string expected_header =
        "cycle,rst_n,router_x,router_y,dst_x,dst_y,dst_port,last,valid,ready";
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
        if (fields.size() != 10) {
            throw std::runtime_error(
                path + ':' + std::to_string(line_number)
                + ": expected 10 CSV fields");
        }

        std::vector<unsigned> values;
        values.reserve(fields.size());
        for (const auto& field : fields) {
            values.push_back(as_unsigned(field, path, line_number));
        }

        result.push_back({
            values[0],
            values[1] != 0,
            values[2],
            values[3],
            values[4],
            values[5],
            values[6],
            values[7] != 0,
            values[8] != 0,
            values[9] != 0,
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

} // namespace

int sc_main(int argc, char* argv[])
{
    if (argc < 3 || argc > 4) {
        std::cerr << "usage: " << argv[0]
                  << " <stimulus.csv> <actual.csv> [expected.csv]\n";
        return 2;
    }

    try {
        const auto stimuli = read_stimuli(argv[1]);

        sc_core::sc_signal<bool> clk{"clk"};
        sc_core::sc_signal<bool> rst_n{"rst_n"};
        sc_core::sc_signal<floo::model::coordinate> router_id{"router_id"};
        sc_core::sc_signal<floo::model::test_flit> flit{"flit"};
        sc_core::sc_signal<bool> valid{"valid"};
        sc_core::sc_signal<bool> ready{"ready"};
        sc_core::sc_signal<floo::model::test_flit> flit_out{"flit_out"};
        sc_core::sc_signal<sc_dt::sc_uint<3>> route{"route"};
        sc_core::sc_signal<bool> locked{"locked"};

        floo::model::xy_route_select<floo::model::test_flit> dut{"dut"};
        dut.i_clk(clk);
        dut.i_rst_n(rst_n);
        dut.i_router_id(router_id);
        dut.i_flit(flit);
        dut.i_valid(valid);
        dut.i_ready(ready);
        dut.o_flit(flit_out);
        dut.o_route(route);
        dut.o_locked(locked);

        std::ofstream trace(argv[2]);
        if (!trace) {
            throw std::runtime_error(
                "cannot create SystemC trace file: " + std::string(argv[2]));
        }
        trace << "cycle,pre_route,pre_locked,post_route,post_locked\n";

        clk.write(false);
        rst_n.write(false);
        valid.write(false);
        ready.write(false);
        sc_core::sc_start(sc_core::SC_ZERO_TIME);

        for (const auto& row : stimuli) {
            floo::model::test_flit current_flit{};
            current_flit.hdr.dst_id =
                floo::model::coordinate(row.dst_x, row.dst_y, row.dst_port);
            current_flit.hdr.last = row.last;

            clk.write(false);
            rst_n.write(row.rst_n);
            router_id.write(
                floo::model::coordinate(row.router_x, row.router_y));
            flit.write(current_flit);
            valid.write(row.valid);
            ready.write(row.ready);
            sc_core::sc_start(sc_core::sc_time(1, sc_core::SC_NS));

            // Pre-edge, before the registers move. `route` is combinational
            // here, so this column is what catches an output that wrongly
            // depends on the clock rather than on the presented header.
            const auto pre_route = route.read().to_uint();
            const auto pre_locked = static_cast<unsigned>(locked.read());

            clk.write(true);
            sc_core::sc_start(sc_core::sc_time(1, sc_core::SC_NS));

            trace << row.cycle << ',' << pre_route << ',' << pre_locked << ','
                  << route.read().to_uint() << ','
                  << static_cast<unsigned>(locked.read()) << '\n';

            clk.write(false);
            sc_core::sc_start(sc_core::sc_time(1, sc_core::SC_NS));
        }

        trace.close();
        if (argc == 4 && !compare_trace(argv[2], argv[3])) {
            return 1;
        }
        std::cout << "route_trace_sc PASS\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "route_trace_sc: " << error.what() << '\n';
        return 2;
    }
}
