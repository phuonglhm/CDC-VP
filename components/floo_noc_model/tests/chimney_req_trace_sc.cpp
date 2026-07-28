// SPDX-License-Identifier: SHL-0.51
//
// Model side of the chimney request-path cross-check. It replays the same AXI
// beat list through the packing functions in `axi_chimney_pack.hpp` and emits
// the same flit trace the RTL testbench records.
//
// Scope: flit content and per-beat ordering. The model has no timed chimney
// yet, so nothing here claims anything about chimney timing or about its
// internal request arbitration; the stimulus is deliberately one beat at a
// time so the flit order is unambiguous.

#include "floo_noc_model/axi_chimney_pack.hpp"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using namespace floo::model;

// Matches `hw/test/floo_test_pkg.sv`: x at bit 16, y at bit 20, two bits each.
constexpr unsigned xy_x_offset = 16;
constexpr unsigned xy_y_offset = 20;
constexpr unsigned xy_bits = 2;

struct beat {
    unsigned seq{};
    unsigned axi_ch{};
    std::uint64_t id{};
    std::uint64_t addr{};
    unsigned len{};
    unsigned atop{};
    std::uint64_t data{};
    std::uint64_t strb{};
    bool last{};
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

std::vector<beat> read_beats(const std::string& path)
{
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("cannot open stimulus file: " + path);
    }

    std::string line;
    if (!std::getline(input, line)) {
        throw std::runtime_error("empty stimulus file: " + path);
    }
    if (line != "seq,axi_ch,id,addr,len,atop,data,strb,last") {
        throw std::runtime_error("unexpected stimulus header: " + line);
    }

    std::vector<beat> beats;
    while (std::getline(input, line)) {
        if (line.empty()) {
            continue;
        }
        const auto fields = split_csv(line);
        if (fields.size() != 9) {
            throw std::runtime_error("expected 9 stimulus fields: " + line);
        }
        beat entry;
        entry.seq = static_cast<unsigned>(std::stoul(fields[0]));
        entry.axi_ch = static_cast<unsigned>(std::stoul(fields[1]));
        entry.id = std::stoull(fields[2], nullptr, 10);
        entry.addr = std::stoull(fields[3], nullptr, 16);
        entry.len = static_cast<unsigned>(std::stoul(fields[4]));
        entry.atop = static_cast<unsigned>(std::stoul(fields[5], nullptr, 16));
        entry.data = std::stoull(fields[6], nullptr, 16);
        entry.strb = std::stoull(fields[7], nullptr, 16);
        entry.last = std::stoul(fields[8]) != 0;
        beats.push_back(entry);
    }
    return beats;
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
                  << " <stimulus.csv> <actual.csv> [expected.csv]\n";
        return 2;
    }

    try {
        const auto beats = read_beats(argv[1]);

        std::ofstream trace(argv[2]);
        if (!trace) {
            throw std::runtime_error(
                "cannot create trace file: " + std::string(argv[2]));
        }
        trace << "seq,axi_ch,dst_x,dst_y,src_x,src_y,last,atop,rob_req,rob_idx,"
                 "pa,pb,pc\n";

        const coordinate here{1, 1};
        chimney_destination destination{
            xy_addr_offsets{xy_x_offset, xy_bits, xy_y_offset, xy_bits}};

        unsigned emitted = 0;
        for (const auto& entry : beats) {
            axi_req_flit flit{};
            std::uint64_t pa = 0;
            std::uint64_t pb = 0;
            std::uint64_t pc = 0;

            if (entry.axi_ch == static_cast<unsigned>(axi_channel::aw)) {
                axi_aw_chan aw{};
                aw.id = entry.id;
                aw.addr = entry.addr;
                aw.len = static_cast<std::uint8_t>(entry.len);
                aw.size = 3;
                aw.burst = 1;  // BURST_INCR
                aw.atop = static_cast<std::uint8_t>(entry.atop);

                const auto dst = destination.decode_request(aw.addr).value();
                destination.accept_aw(dst);
                flit = pack_aw(aw, here, dst);
                pa = aw.id;
                pb = aw.addr;
                pc = aw.len;
            } else if (entry.axi_ch == static_cast<unsigned>(axi_channel::w)) {
                axi_w_chan w{};
                w.data = entry.data;
                w.strb = entry.strb;
                w.last = entry.last;

                flit = pack_w(w, here, destination.write_destination());
                pa = w.data;
                pb = w.strb;
                pc = w.last ? 1 : 0;
            } else if (entry.axi_ch == static_cast<unsigned>(axi_channel::ar)) {
                axi_ar_chan ar{};
                ar.id = entry.id;
                ar.addr = entry.addr;
                ar.len = static_cast<std::uint8_t>(entry.len);
                ar.size = 3;
                ar.burst = 1;

                const auto dst = destination.decode_request(ar.addr).value();
                flit = pack_ar(ar, here, dst);
                pa = ar.id;
                pb = ar.addr;
                pc = ar.len;
            } else {
                throw std::runtime_error(
                    "unknown AXI channel in stimulus: "
                    + std::to_string(entry.axi_ch));
            }

            trace << emitted << ',' << flit.hdr.axi_ch.to_uint() << ','
                  << flit.hdr.dst_id.x.to_uint() << ','
                  << flit.hdr.dst_id.y.to_uint() << ','
                  << flit.hdr.src_id.x.to_uint() << ','
                  << flit.hdr.src_id.y.to_uint() << ','
                  << static_cast<unsigned>(flit.hdr.last) << ','
                  << static_cast<unsigned>(flit.hdr.atop) << ','
                  << static_cast<unsigned>(flit.hdr.rob_req) << ','
                  << flit.hdr.rob_idx.to_uint() << ','
                  << std::hex << pa << ',' << pb << ',' << pc << std::dec
                  << '\n';
            ++emitted;
        }
        trace.close();

        if (argc == 4 && !compare_trace(argv[2], argv[3])) {
            return 1;
        }
        std::cout << "chimney_req_trace_sc PASS (" << emitted << " flits)\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "chimney_req_trace_sc: " << error.what() << '\n';
        return 2;
    }
}
