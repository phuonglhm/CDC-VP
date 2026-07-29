// SPDX-License-Identifier: SHL-0.51
//
// SystemC side of the chimney request-path **timing** cross-check. It replays
// the shared CSV stimulus through the composed model and writes the cycle
// trace that `rtl_crosscheck/run_chimney_timing_crosscheck.sh` compares against
// the unmodified frozen `hw/floo_axi_chimney.sv`.
//
// Traced: the AXI manager port's three `ready` signals and the `req` link's
// `valid` plus the flit it carries, sampled both before and after the clock
// edge, and the composed state that decides them — `aw_w_sel_q` and every
// register of the request wormhole arbiter.

#include "floo_noc_model/axi_chimney.hpp"

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

// `hw/test/floo_test_pkg.sv`: InIdWidth 3, MaxTxnsPerId 32, and the XY address
// fields at bit 16 (x) and bit 20 (y), two bits each.
constexpr unsigned axi_id_bits = 3;
constexpr unsigned max_txns_per_id = 32;
constexpr unsigned xy_offset_x = 16;
constexpr unsigned xy_offset_y = 20;
constexpr unsigned xy_field_bits = 2;

struct stimulus {
    unsigned cycle{};
    bool rst_n{};
    bool aw_valid{};
    unsigned aw_id{};
    std::uint64_t aw_addr{};
    bool w_valid{};
    bool w_last{};
    std::uint64_t w_data{};
    bool ar_valid{};
    unsigned ar_id{};
    std::uint64_t ar_addr{};
    bool req_ready{};
};

/// `floo_id_translation.sv` with `UseIdTable = 0` under XY routing:
/// `id.x = addr[XYAddrOffsetX +: $bits(id.x)]`, likewise for y, `port_id = 0`.
coordinate decode_dest(std::uint64_t addr)
{
    const unsigned mask = (1u << xy_field_bits) - 1u;
    return coordinate{
        static_cast<unsigned>((addr >> xy_offset_x) & mask),
        static_cast<unsigned>((addr >> xy_offset_y) & mask)};
}

unsigned encode_dest(const coordinate& value)
{
    return (value.y.to_uint() << xy_field_bits) | value.x.to_uint();
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

std::uint64_t parse_integer(
    const std::string& value, const std::string& path, unsigned line_number)
{
    std::size_t parsed = 0;
    const auto result = std::stoull(value, &parsed, 10);
    if (parsed != value.size()) {
        throw std::runtime_error(
            path + ':' + std::to_string(line_number) + ": invalid integer '"
            + value + "'");
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

    const std::string expected_header =
        "cycle,rst_n,aw_valid,aw_id,aw_addr,w_valid,w_last,w_data,"
        "ar_valid,ar_id,ar_addr,req_ready";
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
        if (fields.size() != 12) {
            throw std::runtime_error(
                path + ':' + std::to_string(line_number)
                + ": expected 12 CSV fields");
        }

        stimulus row{};
        row.cycle = static_cast<unsigned>(parse_integer(fields[0], path, line_number));
        row.rst_n = parse_integer(fields[1], path, line_number) != 0;
        row.aw_valid = parse_integer(fields[2], path, line_number) != 0;
        row.aw_id = static_cast<unsigned>(parse_integer(fields[3], path, line_number));
        row.aw_addr = parse_integer(fields[4], path, line_number);
        row.w_valid = parse_integer(fields[5], path, line_number) != 0;
        row.w_last = parse_integer(fields[6], path, line_number) != 0;
        row.w_data = parse_integer(fields[7], path, line_number);
        row.ar_valid = parse_integer(fields[8], path, line_number) != 0;
        row.ar_id = static_cast<unsigned>(parse_integer(fields[9], path, line_number));
        row.ar_addr = parse_integer(fields[10], path, line_number);
        row.req_ready = parse_integer(fields[11], path, line_number) != 0;

        if (row.aw_id >= (1u << axi_id_bits)
            || row.ar_id >= (1u << axi_id_bits)) {
            throw std::runtime_error(
                path + ':' + std::to_string(line_number)
                + ": AXI ID wider than InIdWidth");
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
    sc_core::sc_signal<coordinate> node_id{"node_id"};

    sc_core::sc_signal<bool> aw_valid{"aw_valid"};
    sc_core::sc_signal<bool> aw_ready{"aw_ready"};
    sc_core::sc_signal<axi_aw_chan> aw{"aw"};
    sc_core::sc_signal<coordinate> aw_dest{"aw_dest"};
    sc_core::sc_signal<bool> w_valid{"w_valid"};
    sc_core::sc_signal<bool> w_ready{"w_ready"};
    sc_core::sc_signal<axi_w_chan> w{"w"};
    sc_core::sc_signal<bool> ar_valid{"ar_valid"};
    sc_core::sc_signal<bool> ar_ready{"ar_ready"};
    sc_core::sc_signal<axi_ar_chan> ar{"ar"};
    sc_core::sc_signal<coordinate> ar_dest{"ar_dest"};

    sc_core::sc_signal<axi_req_flit> req_data{"req_data"};
    sc_core::sc_signal<bool> req_valid{"req_valid"};
    sc_core::sc_signal<bool> req_ready{"req_ready"};

    sc_core::sc_signal<bool> b_pop{"b_pop", false};
    sc_core::sc_signal<unsigned> b_pop_id{"b_pop_id", 0};
    sc_core::sc_signal<bool> r_pop{"r_pop", false};
    sc_core::sc_signal<unsigned> r_pop_id{"r_pop_id", 0};

    axi_chimney_request<axi_id_bits, max_txns_per_id> dut{"dut"};
    dut.i_clk(clk);
    dut.i_rst_n(rst_n);
    dut.i_node_id(node_id);
    dut.i_aw_valid(aw_valid);
    dut.o_aw_ready(aw_ready);
    dut.i_aw(aw);
    dut.i_aw_dest(aw_dest);
    dut.i_w_valid(w_valid);
    dut.o_w_ready(w_ready);
    dut.i_w(w);
    dut.i_ar_valid(ar_valid);
    dut.o_ar_ready(ar_ready);
    dut.i_ar(ar);
    dut.i_ar_dest(ar_dest);
    dut.o_req_data(req_data);
    dut.o_req_valid(req_valid);
    dut.i_req_ready(req_ready);
    dut.i_b_pop(b_pop);
    dut.i_b_pop_id(b_pop_id);
    dut.i_r_pop(r_pop);
    dut.i_r_pop_id(r_pop_id);

    trace << "cycle,"
             "pre_aw_ready,pre_w_ready,pre_ar_ready,pre_req_valid,"
             "pre_ch,pre_dst,pre_last,pre_payload,"
             "post_aw_ready,post_w_ready,post_ar_ready,post_req_valid,"
             "post_ch,post_dst,post_last,post_payload,"
             "sel_aw,arb_valid_q,arb_last_q,arb_rr_q,arb_lock_q,arb_req_q\n";

    // The flit payload is reduced to one comparable number per channel: an AW
    // and an AR contribute their id and address, a W its data and strobe. The
    // full payload is already signed by the two content cross-checks; what
    // this trace has to pin is which flit is on the link in which cycle.
    const auto payload_of = [](const axi_req_flit& flit) -> std::uint64_t {
        switch (static_cast<axi_channel>(flit.hdr.axi_ch.to_uint())) {
        case axi_channel::aw:
            return (flit.aw.id << 32) | (flit.aw.addr & 0xFFFF'FFFFull);
        case axi_channel::w:
            return (flit.w.strb << 32) | (flit.w.data & 0xFFFF'FFFFull);
        case axi_channel::ar:
            return (flit.ar.id << 32) | (flit.ar.addr & 0xFFFF'FFFFull);
        default:
            return 0;
        }
    };

    const auto emit_link = [&](std::ostream& out) {
        const auto flit = req_data.read();
        out << flit.hdr.axi_ch.to_uint() << ','
            << encode_dest(flit.hdr.dst_id) << ','
            << static_cast<unsigned>(flit.hdr.last) << ','
            << payload_of(flit);
    };

    clk.write(false);
    rst_n.write(false);
    node_id.write(coordinate{0, 0});
    sc_core::sc_start(sc_core::SC_ZERO_TIME);

    for (const auto& row : stimuli) {
        clk.write(false);
        rst_n.write(row.rst_n);

        axi_aw_chan aw_beat{};
        aw_beat.id = row.aw_id;
        aw_beat.addr = row.aw_addr;
        aw_beat.size = 3;
        aw_beat.burst = 1;
        aw.write(aw_beat);
        aw_valid.write(row.aw_valid);
        aw_dest.write(decode_dest(row.aw_addr));

        axi_w_chan w_beat{};
        w_beat.data = row.w_data;
        w_beat.strb = 0xFF;
        w_beat.last = row.w_last;
        w.write(w_beat);
        w_valid.write(row.w_valid);

        axi_ar_chan ar_beat{};
        ar_beat.id = row.ar_id;
        ar_beat.addr = row.ar_addr;
        ar_beat.size = 3;
        ar_beat.burst = 1;
        ar.write(ar_beat);
        ar_valid.write(row.ar_valid);
        ar_dest.write(decode_dest(row.ar_addr));

        req_ready.write(row.req_ready);
        sc_core::sc_start(sc_core::sc_time(1, sc_core::SC_NS));

        std::ostringstream pre;
        pre << static_cast<unsigned>(aw_ready.read()) << ','
            << static_cast<unsigned>(w_ready.read()) << ','
            << static_cast<unsigned>(ar_ready.read()) << ','
            << static_cast<unsigned>(req_valid.read()) << ',';
        emit_link(pre);

        clk.write(true);
        sc_core::sc_start(sc_core::sc_time(1, sc_core::SC_NS));

        trace << row.cycle << ',' << pre.str() << ','
              << static_cast<unsigned>(aw_ready.read()) << ','
              << static_cast<unsigned>(w_ready.read()) << ','
              << static_cast<unsigned>(ar_ready.read()) << ','
              << static_cast<unsigned>(req_valid.read()) << ',';
        emit_link(trace);
        trace << ',' << static_cast<unsigned>(dut.selects_aw()) << ','
              << dut.arb_valid_q() << ','
              << static_cast<unsigned>(dut.arb_last_q()) << ','
              << dut.arb_rr_q() << ','
              << static_cast<unsigned>(dut.arb_lock_q()) << ','
              << dut.arb_req_q() << '\n';

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
        std::cout << "chimney_timing_trace_sc PASS\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "chimney_timing_trace_sc: " << error.what() << '\n';
        return 2;
    }
}
