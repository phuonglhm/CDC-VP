// SPDX-License-Identifier: SHL-0.51
//
// SystemC side of the **inter-node** cross-check. It replays the shared CSV
// stimulus through the model's two-network `axi_noc` and writes the cycle
// trace that `rtl_crosscheck/run_mesh_crosscheck.sh` compares against a grid
// of the unmodified frozen `hw/floo_axi_router.sv`.
//
// Traced per node and per network: the local endpoint's inject `ready`, its
// eject `valid`, and the ejected flit reduced to channel, destination, `last`,
// and a payload tag. One line per node per cycle, so the grid size is a
// parameter rather than a column count.

#include "floo_noc_model/axi_noc.hpp"

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

constexpr unsigned mesh_x = 3;
constexpr unsigned mesh_y = 3;
constexpr unsigned num_nodes = mesh_x * mesh_y;
using noc_t = axi_noc<mesh_x, mesh_y>;

struct node_stimulus {
    bool rq_valid{};
    unsigned rq_ch{};
    unsigned rq_dst_x{};
    unsigned rq_dst_y{};
    bool rq_last{};
    std::uint64_t rq_tag{};
    bool rq_ej_ready{};
    bool rs_valid{};
    unsigned rs_dst_x{};
    unsigned rs_dst_y{};
    unsigned rs_tag{};
    bool rs_ej_ready{};
};

struct cycle_stimulus {
    unsigned cycle{};
    bool rst_n{};
    std::vector<node_stimulus> nodes;
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

std::vector<cycle_stimulus> read_stimuli(const std::string& path)
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
        "cycle,node,rst_n,"
        "rq_valid,rq_ch,rq_dst_x,rq_dst_y,rq_last,rq_tag,rq_ej_ready,"
        "rs_valid,rs_dst_x,rs_dst_y,rs_tag,rs_ej_ready";
    if (line != expected_header) {
        throw std::runtime_error(
            "unexpected stimulus header in " + path + ": " + line);
    }

    std::vector<cycle_stimulus> result;
    unsigned line_number = 1;
    while (std::getline(input, line)) {
        ++line_number;
        if (line.empty()) {
            continue;
        }
        const auto f = split_csv(line);
        if (f.size() != 15) {
            throw std::runtime_error(
                path + ':' + std::to_string(line_number)
                + ": expected 15 CSV fields");
        }
        const auto number = [&](std::size_t index) {
            return parse_integer(f[index], path, line_number);
        };

        const auto cycle = static_cast<unsigned>(number(0));
        const auto node = static_cast<unsigned>(number(1));
        if (node >= num_nodes) {
            throw std::runtime_error(
                path + ':' + std::to_string(line_number) + ": node out of range");
        }
        if (node == 0) {
            cycle_stimulus entry{};
            entry.cycle = cycle;
            entry.rst_n = number(2) != 0;
            entry.nodes.resize(num_nodes);
            result.push_back(entry);
        }
        if (result.empty() || result.back().nodes.size() != num_nodes) {
            throw std::runtime_error(
                path + ':' + std::to_string(line_number)
                + ": stimulus node out of order");
        }

        node_stimulus row{};
        row.rq_valid = number(3) != 0;
        row.rq_ch = static_cast<unsigned>(number(4));
        row.rq_dst_x = static_cast<unsigned>(number(5));
        row.rq_dst_y = static_cast<unsigned>(number(6));
        row.rq_last = number(7) != 0;
        row.rq_tag = number(8);
        row.rq_ej_ready = number(9) != 0;
        row.rs_valid = number(10) != 0;
        row.rs_dst_x = static_cast<unsigned>(number(11));
        row.rs_dst_y = static_cast<unsigned>(number(12));
        row.rs_tag = static_cast<unsigned>(number(13));
        row.rs_ej_ready = number(14) != 0;
        result.back().nodes[node] = row;
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
    unsigned reported = 0;
    for (std::size_t index = 0; index < count; ++index) {
        const std::string lhs =
            index < actual_lines.size() ? actual_lines[index] : "<missing>";
        const std::string rhs =
            index < expected_lines.size() ? expected_lines[index] : "<missing>";
        if (lhs != rhs) {
            match = false;
            if (reported < 20) {
                std::cerr << "trace mismatch at line " << index + 1
                          << ": actual='" << lhs << "' expected='" << rhs
                          << "'\n";
                ++reported;
            }
        }
    }
    return match;
}

unsigned encode_dest(const coordinate& value)
{
    return (value.y.to_uint() << 2) | value.x.to_uint();
}

void run(const std::vector<cycle_stimulus>& stimuli, std::ostream& trace)
{
    sc_core::sc_signal<bool> clk{"clk"};
    sc_core::sc_signal<bool> rst_n{"rst_n"};

    noc_t noc{"noc"};
    noc.i_clk(clk);
    noc.i_rst_n(rst_n);

    trace << "cycle,node,"
             "pre_rq_inj_ready,pre_rq_ej_valid,pre_rq_ej_ch,pre_rq_ej_dst,"
             "pre_rq_ej_last,pre_rq_ej_tag,"
             "pre_rs_inj_ready,pre_rs_ej_valid,pre_rs_ej_dst,pre_rs_ej_tag,"
             "post_rq_inj_ready,post_rq_ej_valid,post_rq_ej_ch,post_rq_ej_dst,"
             "post_rq_ej_last,post_rq_ej_tag,"
             "post_rs_inj_ready,post_rs_ej_valid,post_rs_ej_dst,post_rs_ej_tag\n";

    // The ejected flit reduced to one comparable number, matching the RTL side.
    const auto req_tag_of = [](const axi_req_flit& flit) -> std::uint64_t {
        switch (static_cast<axi_channel>(flit.hdr.axi_ch.to_uint())) {
        case axi_channel::aw: return flit.aw.addr & 0xFFFF'FFFFull;
        case axi_channel::w:  return flit.w.data & 0xFFFF'FFFFull;
        default:              return flit.ar.addr & 0xFFFF'FFFFull;
        }
    };

    const auto emit_node = [&](std::ostream& out, unsigned node) {
        auto& req = noc.req(node);
        auto& rsp = noc.rsp(node);
        const auto req_flit = req.eject_data.read();
        const auto rsp_flit = rsp.eject_data.read();
        out << static_cast<unsigned>(req.inject_ready.read()) << ','
            << static_cast<unsigned>(req.eject_valid.read()) << ','
            << req_flit.hdr.axi_ch.to_uint() << ','
            << encode_dest(req_flit.hdr.dst_id) << ','
            << static_cast<unsigned>(req_flit.hdr.last) << ','
            << req_tag_of(req_flit) << ','
            << static_cast<unsigned>(rsp.inject_ready.read()) << ','
            << static_cast<unsigned>(rsp.eject_valid.read()) << ','
            << encode_dest(rsp_flit.hdr.dst_id) << ',' << rsp_flit.b.id;
    };

    clk.write(false);
    rst_n.write(false);
    for (unsigned node = 0; node < num_nodes; ++node) {
        noc.req(node).inject_valid.write(false);
        noc.rsp(node).inject_valid.write(false);
        noc.req(node).eject_ready.write(false);
        noc.rsp(node).eject_ready.write(false);
    }
    sc_core::sc_start(sc_core::SC_ZERO_TIME);

    for (const auto& row : stimuli) {
        clk.write(false);
        rst_n.write(row.rst_n);

        for (unsigned node = 0; node < num_nodes; ++node) {
            const auto& spec = row.nodes[node];
            auto& req = noc.req(node);
            auto& rsp = noc.rsp(node);

            axi_req_flit req_flit{};
            req_flit.hdr.axi_ch = spec.rq_ch;
            req_flit.hdr.dst_id = coordinate{spec.rq_dst_x, spec.rq_dst_y};
            req_flit.hdr.src_id =
                coordinate{node % mesh_x, node / mesh_x};
            req_flit.hdr.last = spec.rq_last;
            switch (static_cast<axi_channel>(spec.rq_ch)) {
            case axi_channel::aw: req_flit.aw.addr = spec.rq_tag; break;
            case axi_channel::w:  req_flit.w.data = spec.rq_tag; break;
            default:              req_flit.ar.addr = spec.rq_tag; break;
            }
            req.inject_data.write(req_flit);
            req.inject_valid.write(spec.rq_valid);
            req.eject_ready.write(spec.rq_ej_ready);

            axi_rsp_flit rsp_flit{};
            rsp_flit.hdr.axi_ch = static_cast<unsigned>(axi_channel::b);
            rsp_flit.hdr.dst_id = coordinate{spec.rs_dst_x, spec.rs_dst_y};
            rsp_flit.hdr.src_id =
                coordinate{node % mesh_x, node / mesh_x};
            rsp_flit.hdr.last = true;
            rsp_flit.b.id = spec.rs_tag;
            rsp.inject_data.write(rsp_flit);
            rsp.inject_valid.write(spec.rs_valid);
            rsp.eject_ready.write(spec.rs_ej_ready);
        }
        sc_core::sc_start(sc_core::sc_time(1, sc_core::SC_NS));

        std::vector<std::string> pre(num_nodes);
        for (unsigned node = 0; node < num_nodes; ++node) {
            std::ostringstream line;
            emit_node(line, node);
            pre[node] = line.str();
        }

        clk.write(true);
        sc_core::sc_start(sc_core::sc_time(1, sc_core::SC_NS));

        for (unsigned node = 0; node < num_nodes; ++node) {
            trace << row.cycle << ',' << node << ',' << pre[node] << ',';
            emit_node(trace, node);
            trace << '\n';
        }

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
        std::cout << "mesh_trace_sc PASS\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "mesh_trace_sc: " << error.what() << '\n';
        return 2;
    }
}
