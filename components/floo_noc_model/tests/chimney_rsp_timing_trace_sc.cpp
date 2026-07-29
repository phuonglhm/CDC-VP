// SPDX-License-Identifier: SHL-0.51
//
// SystemC side of the chimney **response-path and subordinate-side timing**
// cross-check. It replays the shared CSV stimulus through the composed model
// and writes the cycle trace that
// `rtl_crosscheck/run_chimney_rsp_timing_crosscheck.sh` compares against the
// unmodified frozen `hw/floo_axi_chimney.sv`.
//
// Traced: the inbound `req` link's `ready`, the reissued AXI request boundary,
// the `axi_out` response `ready` signals, the outgoing `rsp` link's `valid` and
// flit, and the composed state — both metadata FIFO occupancies and every
// response-arbiter register.

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

constexpr unsigned out_id_width = 3;
constexpr unsigned max_txns = 32;

struct stimulus {
    unsigned cycle{};
    bool rst_n{};
    bool req_valid{};
    unsigned req_ch{};
    unsigned req_id{};
    std::uint64_t req_addr{};
    bool req_last{};
    std::uint64_t req_data{};
    unsigned req_src_x{};
    unsigned req_src_y{};
    bool aw_ready{};
    bool w_ready{};
    bool ar_ready{};
    bool b_valid{};
    unsigned b_id{};
    unsigned b_resp{};
    bool r_valid{};
    unsigned r_id{};
    std::uint64_t r_data{};
    unsigned r_resp{};
    bool r_last{};
    bool rsp_ready{};
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
        "cycle,rst_n,req_valid,req_ch,req_id,req_addr,req_last,req_data,"
        "req_src_x,req_src_y,aw_ready,w_ready,ar_ready,"
        "b_valid,b_id,b_resp,r_valid,r_id,r_data,r_resp,r_last,rsp_ready";
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
        if (f.size() != 22) {
            throw std::runtime_error(
                path + ':' + std::to_string(line_number)
                + ": expected 22 CSV fields");
        }
        const auto number = [&](std::size_t index) {
            return parse_integer(f[index], path, line_number);
        };

        stimulus row{};
        row.cycle = static_cast<unsigned>(number(0));
        row.rst_n = number(1) != 0;
        row.req_valid = number(2) != 0;
        row.req_ch = static_cast<unsigned>(number(3));
        row.req_id = static_cast<unsigned>(number(4));
        row.req_addr = number(5);
        row.req_last = number(6) != 0;
        row.req_data = number(7);
        row.req_src_x = static_cast<unsigned>(number(8));
        row.req_src_y = static_cast<unsigned>(number(9));
        row.aw_ready = number(10) != 0;
        row.w_ready = number(11) != 0;
        row.ar_ready = number(12) != 0;
        row.b_valid = number(13) != 0;
        row.b_id = static_cast<unsigned>(number(14));
        row.b_resp = static_cast<unsigned>(number(15));
        row.r_valid = number(16) != 0;
        row.r_id = static_cast<unsigned>(number(17));
        row.r_data = number(18);
        row.r_resp = static_cast<unsigned>(number(19));
        row.r_last = number(20) != 0;
        row.rsp_ready = number(21) != 0;
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

    sc_core::sc_signal<axi_req_flit> req_data{"req_data"};
    sc_core::sc_signal<bool> req_valid{"req_valid"};
    sc_core::sc_signal<bool> req_ready{"req_ready"};

    sc_core::sc_signal<axi_aw_chan> axi_aw{"axi_aw"};
    sc_core::sc_signal<bool> axi_aw_valid{"axi_aw_valid"};
    sc_core::sc_signal<bool> axi_aw_ready{"axi_aw_ready"};
    sc_core::sc_signal<axi_w_chan> axi_w{"axi_w"};
    sc_core::sc_signal<bool> axi_w_valid{"axi_w_valid"};
    sc_core::sc_signal<bool> axi_w_ready{"axi_w_ready"};
    sc_core::sc_signal<axi_ar_chan> axi_ar{"axi_ar"};
    sc_core::sc_signal<bool> axi_ar_valid{"axi_ar_valid"};
    sc_core::sc_signal<bool> axi_ar_ready{"axi_ar_ready"};

    sc_core::sc_signal<axi_b_chan> axi_b{"axi_b"};
    sc_core::sc_signal<bool> axi_b_valid{"axi_b_valid"};
    sc_core::sc_signal<bool> axi_b_ready{"axi_b_ready"};
    sc_core::sc_signal<axi_r_chan> axi_r{"axi_r"};
    sc_core::sc_signal<bool> axi_r_valid{"axi_r_valid"};
    sc_core::sc_signal<bool> axi_r_ready{"axi_r_ready"};

    sc_core::sc_signal<axi_rsp_flit> rsp_data{"rsp_data"};
    sc_core::sc_signal<bool> rsp_valid{"rsp_valid"};
    sc_core::sc_signal<bool> rsp_ready{"rsp_ready"};

    axi_chimney_response<out_id_width, max_txns> dut{"dut"};
    dut.i_clk(clk);
    dut.i_rst_n(rst_n);
    dut.i_node_id(node_id);
    dut.i_req_data(req_data);
    dut.i_req_valid(req_valid);
    dut.o_req_ready(req_ready);
    dut.o_axi_aw(axi_aw);
    dut.o_axi_aw_valid(axi_aw_valid);
    dut.i_axi_aw_ready(axi_aw_ready);
    dut.o_axi_w(axi_w);
    dut.o_axi_w_valid(axi_w_valid);
    dut.i_axi_w_ready(axi_w_ready);
    dut.o_axi_ar(axi_ar);
    dut.o_axi_ar_valid(axi_ar_valid);
    dut.i_axi_ar_ready(axi_ar_ready);
    dut.i_axi_b(axi_b);
    dut.i_axi_b_valid(axi_b_valid);
    dut.o_axi_b_ready(axi_b_ready);
    dut.i_axi_r(axi_r);
    dut.i_axi_r_valid(axi_r_valid);
    dut.o_axi_r_ready(axi_r_ready);
    dut.o_rsp_data(rsp_data);
    dut.o_rsp_valid(rsp_valid);
    dut.i_rsp_ready(rsp_ready);

    trace << "cycle,"
             "pre_req_ready,pre_aw_valid,pre_aw_id,pre_w_valid,pre_ar_valid,"
             "pre_ar_id,pre_b_ready,pre_r_ready,pre_rsp_valid,"
             "pre_rsp_ch,pre_rsp_dst,pre_rsp_id,"
             "post_req_ready,post_aw_valid,post_aw_id,post_w_valid,"
             "post_ar_valid,post_ar_id,post_b_ready,post_r_ready,"
             "post_rsp_valid,post_rsp_ch,post_rsp_dst,post_rsp_id,"
             "aw_meta,ar_meta,arb_valid_q,arb_last_q,arb_rr_q,arb_lock_q,"
             "arb_req_q\n";

    const auto emit = [&](std::ostream& out) {
        const auto flit = rsp_data.read();
        const auto channel =
            static_cast<axi_channel>(flit.hdr.axi_ch.to_uint());
        const std::uint64_t id =
            channel == axi_channel::b ? flit.b.id : flit.r.id;
        out << static_cast<unsigned>(req_ready.read()) << ','
            << static_cast<unsigned>(axi_aw_valid.read()) << ','
            << axi_aw.read().id << ','
            << static_cast<unsigned>(axi_w_valid.read()) << ','
            << static_cast<unsigned>(axi_ar_valid.read()) << ','
            << axi_ar.read().id << ','
            << static_cast<unsigned>(axi_b_ready.read()) << ','
            << static_cast<unsigned>(axi_r_ready.read()) << ','
            << static_cast<unsigned>(rsp_valid.read()) << ','
            << flit.hdr.axi_ch.to_uint() << ','
            << ((flit.hdr.dst_id.y.to_uint() << 2)
                | flit.hdr.dst_id.x.to_uint())
            << ',' << id;
    };

    clk.write(false);
    rst_n.write(false);
    node_id.write(coordinate{2, 2});
    sc_core::sc_start(sc_core::SC_ZERO_TIME);

    for (const auto& row : stimuli) {
        clk.write(false);
        rst_n.write(row.rst_n);

        axi_req_flit flit{};
        flit.hdr.axi_ch = row.req_ch;
        flit.hdr.src_id = coordinate{row.req_src_x, row.req_src_y};
        flit.hdr.dst_id = coordinate{2, 2};
        flit.hdr.last = row.req_last;
        flit.hdr.rob_req = true;
        flit.hdr.rob_idx = 0;
        flit.aw.id = row.req_id;
        flit.aw.addr = row.req_addr;
        flit.aw.size = 3;
        flit.aw.burst = 1;
        flit.w.data = row.req_data;
        flit.w.strb = 0xFF;
        flit.w.last = row.req_last;
        flit.ar.id = row.req_id;
        flit.ar.addr = row.req_addr;
        flit.ar.size = 3;
        flit.ar.burst = 1;
        req_data.write(flit);
        req_valid.write(row.req_valid);

        axi_aw_ready.write(row.aw_ready);
        axi_w_ready.write(row.w_ready);
        axi_ar_ready.write(row.ar_ready);

        axi_b_chan b{};
        b.id = row.b_id;
        b.resp = static_cast<std::uint8_t>(row.b_resp);
        axi_b.write(b);
        axi_b_valid.write(row.b_valid);

        axi_r_chan r{};
        r.id = row.r_id;
        r.data = row.r_data;
        r.resp = static_cast<std::uint8_t>(row.r_resp);
        r.last = row.r_last;
        axi_r.write(r);
        axi_r_valid.write(row.r_valid);

        rsp_ready.write(row.rsp_ready);
        sc_core::sc_start(sc_core::sc_time(1, sc_core::SC_NS));

        std::ostringstream pre;
        emit(pre);

        clk.write(true);
        sc_core::sc_start(sc_core::sc_time(1, sc_core::SC_NS));

        trace << row.cycle << ',' << pre.str() << ',';
        emit(trace);
        trace << ',' << dut.aw_meta_occupancy() << ','
              << dut.ar_meta_occupancy() << ',' << dut.arb_valid_q() << ','
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
        std::cout << "chimney_rsp_timing_trace_sc PASS\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "chimney_rsp_timing_trace_sc: " << error.what() << '\n';
        return 2;
    }
}
