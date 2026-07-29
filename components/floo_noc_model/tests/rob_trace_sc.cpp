// SPDX-License-Identifier: SHL-0.51
//
// SystemC side of the `NoRoB` ordering cross-check. It replays the shared CSV
// stimulus through the model and writes the cycle trace that
// `rtl_crosscheck/run_rob_crosscheck.sh` compares against the unmodified
// frozen `hw/floo_rob_wrapper.sv`.
//
// Traced: the module's handshake boundary plus the three internal signals that
// decide admission (`in_flight`, `prev_dest`, `counter_full`) and the whole
// counter bank, sampled both before and after the clock edge. Exporting the
// bank matters because `full_o` is a global OR: a model that got the per-ID
// counters right but the OR wrong would still agree on the boundary for many
// cycles.

#include "floo_noc_model/rob_order_gate.hpp"

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

using namespace floo::model;

constexpr unsigned axi_id_bits = 2;
constexpr unsigned max_ro_txns_per_id = 4;
constexpr unsigned num_ids = 1u << axi_id_bits;

struct stimulus {
    unsigned cycle{};
    bool rst_n{};
    bool ax_valid{};
    unsigned ax_id{};
    unsigned ax_dest{};
    bool ax_ready{};
    bool rsp_valid{};
    unsigned rsp_id{};
    bool rsp_last{};
    bool rsp_ready{};
};

/// The stimulus carries destinations as a 4-bit code. The mapping onto a
/// coordinate only has to be injective: the RTL compares `dest_t` for
/// equality and never interprets it.
coordinate decode_dest(unsigned code)
{
    return coordinate{code & 0x3u, (code >> 2) & 0x3u};
}

unsigned encode_dest(const coordinate& value)
{
    return (value.y.to_uint() << 2) | value.x.to_uint();
}

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
    const std::string& value, const std::string& path, unsigned line_number)
{
    std::size_t parsed = 0;
    const auto result = std::stoul(value, &parsed, 10);
    if (parsed != value.size()) {
        throw std::runtime_error(
            path + ':' + std::to_string(line_number) + ": invalid integer '"
            + value + "'");
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
        "cycle,rst_n,ax_valid,ax_id,ax_dest,ax_ready,"
        "rsp_valid,rsp_id,rsp_last,rsp_ready";
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

        stimulus row{};
        row.cycle = parse_integer(fields[0], path, line_number);
        row.rst_n = parse_integer(fields[1], path, line_number) != 0;
        row.ax_valid = parse_integer(fields[2], path, line_number) != 0;
        row.ax_id = parse_integer(fields[3], path, line_number);
        row.ax_dest = parse_integer(fields[4], path, line_number);
        row.ax_ready = parse_integer(fields[5], path, line_number) != 0;
        row.rsp_valid = parse_integer(fields[6], path, line_number) != 0;
        row.rsp_id = parse_integer(fields[7], path, line_number);
        row.rsp_last = parse_integer(fields[8], path, line_number) != 0;
        row.rsp_ready = parse_integer(fields[9], path, line_number) != 0;

        if (row.ax_id >= num_ids || row.rsp_id >= num_ids) {
            throw std::runtime_error(
                path + ':' + std::to_string(line_number)
                + ": AXI ID outside the counter bank");
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
                      << ": actual='" << lhs << "' expected='" << rhs << "'\n";
            match = false;
        }
    }
    return match;
}

void run(const std::vector<stimulus>& stimuli, std::ostream& trace)
{
    sc_core::sc_signal<bool> clk{"clk"};
    sc_core::sc_signal<bool> rst_n{"rst_n"};
    sc_core::sc_signal<bool> ax_valid{"ax_valid"};
    sc_core::sc_signal<bool> ax_ready_o{"ax_ready_o"};
    sc_core::sc_signal<unsigned> ax_id{"ax_id"};
    sc_core::sc_signal<coordinate> ax_dest{"ax_dest"};
    sc_core::sc_signal<bool> ax_valid_o{"ax_valid_o"};
    sc_core::sc_signal<bool> ax_ready{"ax_ready"};
    sc_core::sc_signal<bool> rob_req{"rob_req"};
    sc_core::sc_signal<unsigned> rob_idx{"rob_idx"};
    sc_core::sc_signal<bool> rsp_valid{"rsp_valid"};
    sc_core::sc_signal<bool> rsp_ready_o{"rsp_ready_o"};
    sc_core::sc_signal<unsigned> rsp_id{"rsp_id"};
    sc_core::sc_signal<bool> rsp_last{"rsp_last"};
    sc_core::sc_signal<bool> rsp_valid_o{"rsp_valid_o"};
    sc_core::sc_signal<bool> rsp_ready{"rsp_ready"};

    no_rob_gate<axi_id_bits, max_ro_txns_per_id> dut{"dut"};
    dut.i_clk(clk);
    dut.i_rst_n(rst_n);
    dut.i_ax_valid(ax_valid);
    dut.o_ax_ready(ax_ready_o);
    dut.i_ax_id(ax_id);
    dut.i_ax_dest(ax_dest);
    dut.o_ax_valid(ax_valid_o);
    dut.i_ax_ready(ax_ready);
    dut.o_ax_rob_req(rob_req);
    dut.o_ax_rob_idx(rob_idx);
    dut.i_rsp_valid(rsp_valid);
    dut.o_rsp_ready(rsp_ready_o);
    dut.i_rsp_id(rsp_id);
    dut.i_rsp_last(rsp_last);
    dut.o_rsp_valid(rsp_valid_o);
    dut.i_rsp_ready(rsp_ready);

    trace << "cycle,"
             "pre_ax_ready,pre_ax_valid,pre_rsp_ready,pre_rsp_valid,"
             "pre_in_flight,pre_prev_dest,pre_full,"
             "post_ax_ready,post_ax_valid,post_rsp_ready,post_rsp_valid,"
             "post_in_flight,post_prev_dest,post_full,"
             "rob_req,rob_idx";
    for (unsigned id = 0; id < num_ids; ++id) {
        trace << ",cnt" << id;
    }
    for (unsigned id = 0; id < num_ids; ++id) {
        trace << ",sel" << id;
    }
    trace << '\n';

    clk.write(false);
    rst_n.write(false);
    sc_core::sc_start(sc_core::SC_ZERO_TIME);

    for (const auto& row : stimuli) {
        clk.write(false);
        rst_n.write(row.rst_n);
        ax_valid.write(row.ax_valid);
        ax_id.write(row.ax_id);
        ax_dest.write(decode_dest(row.ax_dest));
        ax_ready.write(row.ax_ready);
        rsp_valid.write(row.rsp_valid);
        rsp_id.write(row.rsp_id);
        rsp_last.write(row.rsp_last);
        rsp_ready.write(row.rsp_ready);
        sc_core::sc_start(sc_core::sc_time(1, sc_core::SC_NS));

        const unsigned pre_ax_ready = ax_ready_o.read();
        const unsigned pre_ax_valid = ax_valid_o.read();
        const unsigned pre_rsp_ready = rsp_ready_o.read();
        const unsigned pre_rsp_valid = rsp_valid_o.read();
        const unsigned pre_in_flight = dut.in_flight();
        const unsigned pre_prev_dest = encode_dest(dut.prev_dest());
        const unsigned pre_full = dut.counter_full();

        clk.write(true);
        sc_core::sc_start(sc_core::sc_time(1, sc_core::SC_NS));

        trace << row.cycle << ',' << pre_ax_ready << ',' << pre_ax_valid << ','
              << pre_rsp_ready << ',' << pre_rsp_valid << ',' << pre_in_flight
              << ',' << pre_prev_dest << ',' << pre_full << ','
              << static_cast<unsigned>(ax_ready_o.read()) << ','
              << static_cast<unsigned>(ax_valid_o.read()) << ','
              << static_cast<unsigned>(rsp_ready_o.read()) << ','
              << static_cast<unsigned>(rsp_valid_o.read()) << ','
              << static_cast<unsigned>(dut.in_flight()) << ','
              << encode_dest(dut.prev_dest()) << ','
              << static_cast<unsigned>(dut.counter_full()) << ','
              << static_cast<unsigned>(rob_req.read()) << ','
              << rob_idx.read();
        for (unsigned id = 0; id < num_ids; ++id) {
            trace << ',' << dut.outstanding(id);
        }
        for (unsigned id = 0; id < num_ids; ++id) {
            trace << ',' << encode_dest(dut.select_of(id));
        }
        trace << '\n';

        clk.write(false);
        sc_core::sc_start(sc_core::sc_time(1, sc_core::SC_NS));
    }
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

        std::ofstream trace(argv[2]);
        if (!trace) {
            throw std::runtime_error(
                "cannot create SystemC trace file: " + std::string(argv[2]));
        }
        run(stimuli, trace);
        trace.close();

        if (argc == 4 && !compare_trace(argv[2], argv[3])) {
            return 1;
        }
        std::cout << "rob_trace_sc PASS\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "rob_trace_sc: " << error.what() << '\n';
        return 2;
    }
}
