// SPDX-License-Identifier: SHL-0.51
//
// SystemC side of the five-port router cross-check. It replays the shared CSV
// stimulus through the model and writes the cycle trace that
// `rtl_crosscheck/run_router_crosscheck.sh` compares against the unmodified
// frozen `hw/floo_router.sv` in the parameter set frozen in
// `docs/P0_SCOPE.md`.
//
// Traced: per-port `ready_o` and `valid_o` masks and per-output `data_o`
// payloads, sampled before and after the clock edge, plus the one-hot route
// mask per input.
//
// The measured counter block is instantiated alongside the router here on
// purpose. It observes the same signals but drives nothing, so the cross-check
// passing with it attached is the evidence that attaching counters does not
// perturb the modeled datapath. Its report goes to stdout, never into the
// compared trace.

#include "floo_noc_model/floo_router.hpp"
#include "floo_noc_model/floo_types.hpp"
#include "floo_noc_model/noc_counters.hpp"

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

constexpr unsigned num_ports = 5;
constexpr unsigned router_x = 1;
constexpr unsigned router_y = 1;

struct stimulus {
    unsigned cycle{};
    bool rst_n{};
    unsigned valid_mask{};
    unsigned last_mask{};
    unsigned ready_mask{};
    unsigned dst[num_ports]{};
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

    const std::string expected_header =
        "cycle,rst_n,valid_i,last_i,ready_i,dst0,dst1,dst2,dst3,dst4";
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

        stimulus row;
        row.cycle = parse_integer(fields[0], 10, path, line_number);
        row.rst_n = parse_integer(fields[1], 10, path, line_number) != 0;
        row.valid_mask = parse_integer(fields[2], 16, path, line_number);
        row.last_mask = parse_integer(fields[3], 16, path, line_number);
        row.ready_mask = parse_integer(fields[4], 16, path, line_number);
        for (unsigned port = 0; port < num_ports; ++port) {
            row.dst[port] =
                parse_integer(fields[5 + port], 16, path, line_number);
        }
        result.push_back(row);
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

namespace {

/// `OutFifoDepth` is a template parameter so one runner covers both RTL
/// branches: `2`, which every generated FlooNoC router has, and `0`, the
/// `gen_no_out_fifo` bypass.
template <unsigned OutFifoDepth>
int run(int argc, char* argv[], const std::vector<stimulus>& stimuli)
{
    {
        sc_core::sc_signal<bool> clk{"clk"};
        sc_core::sc_signal<bool> rst_n{"rst_n"};
        sc_core::sc_signal<floo::model::coordinate> router_id{"router_id"};
        sc_core::sc_vector<sc_core::sc_signal<flit_t>> in_data{
            "in_data", num_ports};
        sc_core::sc_vector<sc_core::sc_signal<bool>> in_valid{
            "in_valid", num_ports};
        sc_core::sc_vector<sc_core::sc_signal<bool>> in_ready{
            "in_ready", num_ports};
        sc_core::sc_vector<sc_core::sc_signal<flit_t>> out_data{
            "out_data", num_ports};
        sc_core::sc_vector<sc_core::sc_signal<bool>> out_valid{
            "out_valid", num_ports};
        sc_core::sc_vector<sc_core::sc_signal<bool>> out_ready{
            "out_ready", num_ports};
        sc_core::sc_vector<sc_core::sc_signal<unsigned>> occupancy{
            "occupancy", num_ports};
        sc_core::sc_vector<sc_core::sc_signal<unsigned>> output_occupancy{
            "output_occupancy", num_ports};
        sc_core::sc_vector<sc_core::sc_signal<unsigned>> selected{
            "selected", num_ports};
        sc_core::sc_vector<sc_core::sc_signal<bool>> locked{
            "locked", num_ports};

        floo::model::floo_router<flit_t, 2, OutFifoDepth> dut{"dut"};
        floo::model::router_counters<flit_t, num_ports> counters{"counters"};
        counters.i_clk(clk);
        counters.i_rst_n(rst_n);
        dut.i_clk(clk);
        dut.i_rst_n(rst_n);
        dut.i_router_id(router_id);
        for (unsigned port = 0; port < num_ports; ++port) {
            dut.i_data[port](in_data[port]);
            dut.i_valid[port](in_valid[port]);
            dut.o_ready[port](in_ready[port]);
            dut.o_data[port](out_data[port]);
            dut.o_valid[port](out_valid[port]);
            dut.i_ready[port](out_ready[port]);
            dut.o_input_occupancy[port](occupancy[port]);
            dut.o_output_occupancy[port](output_occupancy[port]);
            dut.o_output_selected[port](selected[port]);
            dut.o_output_locked[port](locked[port]);

            counters.i_in_data[port](in_data[port]);
            counters.i_in_valid[port](in_valid[port]);
            counters.i_in_ready[port](in_ready[port]);
            counters.i_out_data[port](out_data[port]);
            counters.i_out_valid[port](out_valid[port]);
            counters.i_out_ready[port](out_ready[port]);
            counters.i_input_occupancy[port](occupancy[port]);
            counters.i_output_occupancy[port](output_occupancy[port]);
        }

        std::ofstream trace(argv[2]);
        if (!trace) {
            throw std::runtime_error(
                "cannot create SystemC trace file: " + std::string(argv[2]));
        }

        trace << "cycle,pre_ready,pre_valid";
        for (unsigned out = 0; out < num_ports; ++out) {
            trace << ",pre_d" << out;
        }
        trace << ",post_ready,post_valid";
        for (unsigned out = 0; out < num_ports; ++out) {
            trace << ",post_d" << out;
        }
        for (unsigned in = 0; in < num_ports; ++in) {
            trace << ",mask" << in;
        }
        trace << '\n';

        const auto mask_of = [&](const auto& vector) {
            unsigned mask = 0;
            for (unsigned port = 0; port < num_ports; ++port) {
                if (vector[port].read()) {
                    mask |= 1u << port;
                }
            }
            return mask;
        };

        const auto write_outputs = [&](unsigned ready, unsigned valid,
                                       const std::uint64_t* payloads) {
            trace << std::hex << ready << ',' << valid;
            for (unsigned out = 0; out < num_ports; ++out) {
                trace << ',' << payloads[out];
            }
            trace << std::dec;
        };

        clk.write(false);
        rst_n.write(false);
        router_id.write(floo::model::coordinate(router_x, router_y));
        sc_core::sc_start(sc_core::SC_ZERO_TIME);

        for (const auto& row : stimuli) {
            clk.write(false);
            rst_n.write(row.rst_n);
            for (unsigned port = 0; port < num_ports; ++port) {
                flit_t flit{};
                flit.hdr.dst_id = floo::model::coordinate(
                    row.dst[port] & 0x3u, (row.dst[port] >> 2) & 0x3u);
                flit.hdr.src_id =
                    floo::model::coordinate(router_x, router_y);
                flit.hdr.last = ((row.last_mask >> port) & 1u) != 0;
                flit.payload = (row.cycle + 1) * 16 + port;
                in_data[port].write(flit);
                in_valid[port].write(((row.valid_mask >> port) & 1u) != 0);
                out_ready[port].write(((row.ready_mask >> port) & 1u) != 0);
            }
            sc_core::sc_start(sc_core::sc_time(1, sc_core::SC_NS));

            const unsigned pre_ready = mask_of(in_ready);
            const unsigned pre_valid = mask_of(out_valid);
            std::uint64_t pre_payload[num_ports];
            for (unsigned out = 0; out < num_ports; ++out) {
                pre_payload[out] = out_data[out].read().payload.to_uint64();
            }

            clk.write(true);
            sc_core::sc_start(sc_core::sc_time(1, sc_core::SC_NS));

            std::uint64_t post_payload[num_ports];
            for (unsigned out = 0; out < num_ports; ++out) {
                post_payload[out] = out_data[out].read().payload.to_uint64();
            }

            trace << row.cycle << ',';
            write_outputs(pre_ready, pre_valid, pre_payload);
            trace << ',';
            write_outputs(mask_of(in_ready), mask_of(out_valid), post_payload);
            for (unsigned in = 0; in < num_ports; ++in) {
                trace << ',' << std::hex
                      << (1u << dut.route_index(in)) << std::dec;
            }
            trace << '\n';

            clk.write(false);
            sc_core::sc_start(sc_core::sc_time(1, sc_core::SC_NS));
        }

        trace.close();

        // Counters are reported outside the compared trace.
        counters.report(std::cout);

        if (argc == 5 && !compare_trace(argv[2], argv[4])) {
            return 1;
        }
        std::cout << "router_trace_sc PASS (out-fifo depth " << OutFifoDepth
                  << ")\n";
        return 0;
    }
}

} // namespace

int sc_main(int argc, char* argv[])
{
    if (argc < 4 || argc > 5) {
        std::cerr << "usage: " << argv[0]
                  << " <stimulus.csv> <actual.csv> <out-fifo-depth>"
                     " [expected.csv]\n";
        return 2;
    }

    try {
        const auto stimuli = read_stimuli(argv[1]);
        const std::string depth = argv[3];
        if (depth == "2") {
            return run<2>(argc, argv, stimuli);
        }
        if (depth == "0") {
            return run<0>(argc, argv, stimuli);
        }
        std::cerr << "unsupported out-fifo depth '" << depth
                  << "' (expected 0 or 2)\n";
        return 2;
    } catch (const std::exception& error) {
        std::cerr << "router_trace_sc: " << error.what() << '\n';
        return 2;
    }
}
