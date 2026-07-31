// SPDX-License-Identifier: SHL-0.51
//
// SystemC side of the chimney **manager-side response** cross-check — Step A-1,
// the fourth and last chimney quadrant. It replays the shared CSV stimulus
// through the composed model and writes the cycle trace that
// `rtl_crosscheck/run_chimney_mgr_rsp_crosscheck.sh` compares against the
// unmodified frozen `hw/floo_axi_chimney.sv`.
//
// The model side is a composition, not a single module: `axi_chimney_request`
// owns the reorder-buffer counter bank and `axi_chimney_manager_response`
// decodes the incoming flit and drives the counter release. Signing the
// composition is the point — the counter bank alone is already signed by
// `run_rob_crosscheck.sh`, and what has never been compared is the wiring
// between the two.
//
// Traced: `floo_rsp_o.ready`, the AXI manager port's B and R channels, the
// manager-side `aw_ready`/`ar_ready`, and the per-id counters for both
// directions. Payload fields are qualified by their valid, for the reason given
// in the testbench header — the RTL's response union aliases B and R bits.

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

constexpr unsigned axi_id_bits = 3;
constexpr unsigned max_txns_per_id = 32;

struct stimulus {
    unsigned cycle{};
    bool rst_n{};
    bool aw_valid{};
    unsigned aw_id{};
    std::uint64_t aw_addr{};
    bool w_valid{};
    std::uint64_t w_data{};
    bool w_last{};
    bool ar_valid{};
    unsigned ar_id{};
    std::uint64_t ar_addr{};
    unsigned ar_len{};
    bool req_ready{};
    bool rsp_valid{};
    unsigned rsp_ch{};
    unsigned rsp_id{};
    std::uint64_t rsp_data{};
    unsigned rsp_resp{};
    bool rsp_last{};
    bool b_ready{};
    bool r_ready{};
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
        "cycle,rst_n,aw_valid,aw_id,aw_addr,w_valid,w_data,w_last,"
        "ar_valid,ar_id,ar_addr,ar_len,req_ready,"
        "rsp_valid,rsp_ch,rsp_id,rsp_data,rsp_resp,rsp_last,"
        "b_ready,r_ready";
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
        const auto f = split_csv(line);
        if (f.size() != 21) {
            throw std::runtime_error(
                path + ':' + std::to_string(line_number)
                + ": expected 21 CSV fields");
        }
        const auto number = [&](std::size_t index) {
            return parse_integer(f[index], path, line_number);
        };

        stimulus row{};
        row.cycle = static_cast<unsigned>(number(0));
        row.rst_n = number(1) != 0;
        row.aw_valid = number(2) != 0;
        row.aw_id = static_cast<unsigned>(number(3));
        row.aw_addr = number(4);
        row.w_valid = number(5) != 0;
        row.w_data = number(6);
        row.w_last = number(7) != 0;
        row.ar_valid = number(8) != 0;
        row.ar_id = static_cast<unsigned>(number(9));
        row.ar_addr = number(10);
        row.ar_len = static_cast<unsigned>(number(11));
        row.req_ready = number(12) != 0;
        row.rsp_valid = number(13) != 0;
        row.rsp_ch = static_cast<unsigned>(number(14));
        row.rsp_id = static_cast<unsigned>(number(15));
        row.rsp_data = number(16);
        row.rsp_resp = static_cast<unsigned>(number(17));
        row.rsp_last = number(18) != 0;
        row.b_ready = number(19) != 0;
        row.r_ready = number(20) != 0;
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

/// The chimney derives a flit's destination from the request address, using
/// `RouteCfg.XYAddrOffsetX = 16` and `XYAddrOffsetY = 20`. The model takes the
/// destination as a port, so the harness applies the same decode rather than
/// hard-coding a node — a stimulus that changes its target address then moves
/// both sides together.
coordinate dest_of(std::uint64_t address)
{
    return coordinate{static_cast<unsigned>((address >> 16) & 0x3),
                      static_cast<unsigned>((address >> 20) & 0x3)};
}

void run(const std::vector<stimulus>& stimuli, std::ostream& trace)
{
    sc_core::sc_signal<bool> clk{"clk"};
    sc_core::sc_signal<bool> rst_n{"rst_n"};
    sc_core::sc_signal<coordinate> node_id{"node_id"};

    sc_core::sc_signal<axi_aw_chan> aw{"aw"};
    sc_core::sc_signal<bool> aw_valid{"aw_valid"};
    sc_core::sc_signal<bool> aw_ready{"aw_ready"};
    sc_core::sc_signal<coordinate> aw_dest{"aw_dest"};
    sc_core::sc_signal<axi_w_chan> w{"w"};
    sc_core::sc_signal<bool> w_valid{"w_valid"};
    sc_core::sc_signal<bool> w_ready{"w_ready"};
    sc_core::sc_signal<axi_ar_chan> ar{"ar"};
    sc_core::sc_signal<bool> ar_valid{"ar_valid"};
    sc_core::sc_signal<bool> ar_ready{"ar_ready"};
    sc_core::sc_signal<coordinate> ar_dest{"ar_dest"};

    sc_core::sc_signal<axi_req_flit> req_data{"req_data"};
    sc_core::sc_signal<bool> req_valid{"req_valid"};
    sc_core::sc_signal<bool> req_ready{"req_ready"};

    sc_core::sc_signal<axi_rsp_flit> rsp_data{"rsp_data"};
    sc_core::sc_signal<bool> rsp_valid{"rsp_valid"};
    sc_core::sc_signal<bool> rsp_ready{"rsp_ready"};

    sc_core::sc_signal<bool> b_rob_ready{"b_rob_ready"};
    sc_core::sc_signal<bool> r_rob_ready{"r_rob_ready"};
    sc_core::sc_signal<bool> b_pop{"b_pop"};
    sc_core::sc_signal<unsigned> b_pop_id{"b_pop_id"};
    sc_core::sc_signal<bool> r_pop{"r_pop"};
    sc_core::sc_signal<unsigned> r_pop_id{"r_pop_id"};
    sc_core::sc_signal<bool> r_pop_last{"r_pop_last"};

    sc_core::sc_signal<bool> b_ready{"b_ready"};
    sc_core::sc_signal<bool> r_ready{"r_ready"};

    sc_core::sc_signal<axi_b_chan> axi_b{"axi_b"};
    sc_core::sc_signal<bool> axi_b_valid{"axi_b_valid"};
    sc_core::sc_signal<axi_r_chan> axi_r{"axi_r"};
    sc_core::sc_signal<bool> axi_r_valid{"axi_r_valid"};

    axi_chimney_request<axi_id_bits, max_txns_per_id> request{"request"};
    axi_chimney_manager_response unpacker{"unpacker"};

    request.i_clk(clk);
    request.i_rst_n(rst_n);
    request.i_node_id(node_id);
    request.i_aw_valid(aw_valid);
    request.o_aw_ready(aw_ready);
    request.i_aw(aw);
    request.i_aw_dest(aw_dest);
    request.i_w_valid(w_valid);
    request.o_w_ready(w_ready);
    request.i_w(w);
    request.i_ar_valid(ar_valid);
    request.o_ar_ready(ar_ready);
    request.i_ar(ar);
    request.i_ar_dest(ar_dest);
    request.o_req_data(req_data);
    request.o_req_valid(req_valid);
    request.i_req_ready(req_ready);
    request.i_b_pop(b_pop);
    request.i_b_pop_id(b_pop_id);
    request.i_r_pop(r_pop);
    request.i_r_pop_id(r_pop_id);
    request.i_r_pop_last(r_pop_last);
    request.o_b_rsp_ready(b_rob_ready);
    request.o_r_rsp_ready(r_rob_ready);
    request.i_b_rsp_ready(b_ready);
    request.i_r_rsp_ready(r_ready);

    unpacker.i_rsp_data(rsp_data);
    unpacker.i_rsp_valid(rsp_valid);
    unpacker.o_rsp_ready(rsp_ready);
    unpacker.i_b_rob_ready(b_rob_ready);
    unpacker.i_r_rob_ready(r_rob_ready);
    unpacker.o_b_pop(b_pop);
    unpacker.o_b_pop_id(b_pop_id);
    unpacker.o_r_pop(r_pop);
    unpacker.o_r_pop_id(r_pop_id);
    unpacker.o_r_pop_last(r_pop_last);
    unpacker.o_axi_b(axi_b);
    unpacker.o_axi_b_valid(axi_b_valid);
    unpacker.o_axi_r(axi_r);
    unpacker.o_axi_r_valid(axi_r_valid);

    trace << "cycle,"
             "pre_rsp_ready,pre_b_valid,pre_b_id,pre_b_resp,"
             "pre_r_valid,pre_r_id,pre_r_data,pre_r_resp,"
             "pre_r_last,pre_aw_ready,pre_ar_ready,"
             "post_rsp_ready,post_b_valid,post_b_id,post_b_resp,"
             "post_r_valid,post_r_id,post_r_data,post_r_resp,"
             "post_r_last,post_aw_ready,post_ar_ready,"
             "r_cnt1,r_cnt2,b_cnt1\n";

    // Payload fields are only meaningful while their valid is high; see the
    // file header.
    const auto emit = [&](std::ostream& out) {
        const bool b_v = axi_b_valid.read();
        const bool r_v = axi_r_valid.read();
        const auto b = axi_b.read();
        const auto r = axi_r.read();
        out << static_cast<unsigned>(rsp_ready.read()) << ','
            << static_cast<unsigned>(b_v) << ','
            << (b_v ? static_cast<unsigned>(b.id) : 0u) << ','
            << (b_v ? static_cast<unsigned>(b.resp) : 0u) << ','
            << static_cast<unsigned>(r_v) << ','
            << (r_v ? static_cast<unsigned>(r.id) : 0u) << ','
            << (r_v ? static_cast<std::uint32_t>(r.data) : 0u) << ','
            << (r_v ? static_cast<unsigned>(r.resp) : 0u) << ','
            << (r_v ? static_cast<unsigned>(r.last) : 0u) << ','
            << static_cast<unsigned>(aw_ready.read()) << ','
            << static_cast<unsigned>(ar_ready.read());
    };

    clk.write(false);
    rst_n.write(false);
    node_id.write(coordinate{2, 2});
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
        aw_dest.write(dest_of(row.aw_addr));

        axi_w_chan w_beat{};
        w_beat.data = row.w_data;
        w_beat.strb = 0xFF;
        w_beat.last = row.w_last;
        w.write(w_beat);
        w_valid.write(row.w_valid);

        axi_ar_chan ar_beat{};
        ar_beat.id = row.ar_id;
        ar_beat.addr = row.ar_addr;
        ar_beat.len = row.ar_len;
        ar_beat.size = 3;
        ar_beat.burst = 1;
        ar.write(ar_beat);
        ar_valid.write(row.ar_valid);
        ar_dest.write(dest_of(row.ar_addr));

        req_ready.write(row.req_ready);

        // Only the channel the header names is populated, matching the RTL,
        // where `floo_rsp_chan_t` is a union and the two payloads alias.
        axi_rsp_flit flit{};
        flit.hdr.axi_ch = row.rsp_ch;
        flit.hdr.src_id = coordinate{1, 2};
        flit.hdr.dst_id = coordinate{2, 2};
        flit.hdr.last = true;
        flit.hdr.rob_req = true;
        flit.hdr.rob_idx = 0;
        if (static_cast<axi_channel>(row.rsp_ch) == axi_channel::b) {
            flit.b.id = row.rsp_id;
            flit.b.resp = static_cast<std::uint8_t>(row.rsp_resp);
        } else {
            flit.r.id = row.rsp_id;
            flit.r.data = row.rsp_data;
            flit.r.resp = static_cast<std::uint8_t>(row.rsp_resp);
            flit.r.last = row.rsp_last;
        }
        rsp_data.write(flit);
        rsp_valid.write(row.rsp_valid);

        b_ready.write(row.b_ready);
        r_ready.write(row.r_ready);

        sc_core::sc_start(sc_core::sc_time(1, sc_core::SC_NS));

        std::ostringstream pre;
        emit(pre);

        clk.write(true);
        sc_core::sc_start(sc_core::sc_time(1, sc_core::SC_NS));

        trace << row.cycle << ',' << pre.str() << ',';
        emit(trace);
        trace << ',' << request.r_outstanding(1) << ','
              << request.r_outstanding(2) << ',' << request.b_outstanding(1)
              << '\n';

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
        std::cout << "chimney_mgr_rsp_trace_sc PASS\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "chimney_mgr_rsp_trace_sc: " << error.what() << '\n';
        return 2;
    }
}
