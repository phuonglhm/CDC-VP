// SPDX-License-Identifier: SHL-0.51
//
// Model side of the chimney response-path cross-check. It replays the same
// transaction list through the meta buffer and the response packing functions,
// and emits the same flit trace the RTL testbench records.
//
// What this exercises: that a response routes back to the requester's `src_id`
// and restores the manager's original AXI ID, and that the metadata is
// retained in order per direction.
//
// Transactions are processed in batches: every request in a batch is pushed
// before any response pops. With one transaction in flight the FIFOs never
// hold more than one entry, and a negative control that swapped the read and
// write buffers still passed, so the batching is what gives the ordering and
// the read/write separation any coverage at all. The downstream ID the chimney reissues
// under is checked too, since the testbench echoes back whatever the RTL chose.

#include "floo_noc_model/meta_buffer.hpp"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using namespace floo::model;

// Matches `hw/test/floo_test_pkg.sv`.
constexpr unsigned out_id_width = 3;
constexpr std::size_t max_txns = 32;
// Must match `BatchSize` in the RTL testbench.
constexpr std::size_t batch_size = 3;

struct transaction {
    unsigned seq{};
    unsigned axi_ch{};
    unsigned src_x{};
    unsigned src_y{};
    std::uint64_t id{};
    std::uint64_t addr{};
    std::uint64_t data{};
    unsigned resp{};
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

std::vector<transaction> read_transactions(const std::string& path)
{
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("cannot open stimulus file: " + path);
    }
    std::string line;
    if (!std::getline(input, line)) {
        throw std::runtime_error("empty stimulus file: " + path);
    }
    if (line != "seq,axi_ch,src_x,src_y,id,addr,data,resp") {
        throw std::runtime_error("unexpected stimulus header: " + line);
    }

    std::vector<transaction> result;
    while (std::getline(input, line)) {
        if (line.empty()) {
            continue;
        }
        const auto fields = split_csv(line);
        if (fields.size() != 8) {
            throw std::runtime_error("expected 8 stimulus fields: " + line);
        }
        transaction entry;
        entry.seq = static_cast<unsigned>(std::stoul(fields[0]));
        entry.axi_ch = static_cast<unsigned>(std::stoul(fields[1]));
        entry.src_x = static_cast<unsigned>(std::stoul(fields[2]));
        entry.src_y = static_cast<unsigned>(std::stoul(fields[3]));
        entry.id = std::stoull(fields[4]);
        entry.addr = std::stoull(fields[5], nullptr, 16);
        entry.data = std::stoull(fields[6], nullptr, 16);
        entry.resp = static_cast<unsigned>(std::stoul(fields[7]));
        result.push_back(entry);
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

} // namespace

int sc_main(int argc, char* argv[])
{
    if (argc < 3 || argc > 4) {
        std::cerr << "usage: " << argv[0]
                  << " <stimulus.csv> <actual.csv> [expected.csv]\n";
        return 2;
    }

    try {
        const auto transactions = read_transactions(argv[1]);

        std::ofstream trace(argv[2]);
        if (!trace) {
            throw std::runtime_error(
                "cannot create trace file: " + std::string(argv[2]));
        }
        trace << "seq,axi_ch,dst_x,dst_y,src_x,src_y,last,atop,rob_req,rob_idx,"
                 "rsp_id,rsp_field\n";

        const coordinate here{1, 1};
        meta_buffer buffer{out_id_width, max_txns};

        unsigned emitted = 0;
        for (std::size_t base = 0; base < transactions.size();
             base += batch_size) {
            const auto limit =
                std::min(base + batch_size, transactions.size());

            // Push every request in the batch first.
            for (std::size_t index = base; index < limit; ++index) {
                const auto& entry = transactions[index];
                response_meta meta{};
                meta.src_id = coordinate(entry.src_x, entry.src_y);
                meta.axi_id = entry.id;
                // `floo_rob_wrapper.sv` drives `rob_req` high even with NoRoB.
                meta.rob_req = true;
                meta.rob_idx = 0;
                meta.atop = false;

                if (entry.axi_ch == static_cast<unsigned>(axi_channel::aw)) {
                    buffer.push_write(meta);
                } else {
                    buffer.push_read(meta);
                }
            }

            // Then answer them in issue order, popping per direction.
            for (std::size_t index = base; index < limit; ++index) {
                const auto& entry = transactions[index];
                const bool is_write =
                    entry.axi_ch == static_cast<unsigned>(axi_channel::aw);

                axi_rsp_flit flit{};
                std::uint64_t rsp_id = 0;
                std::uint64_t rsp_field = 0;

                if (is_write) {
                    const auto stored = buffer.pop_write();
                    axi_b_chan b{};
                    b.id = buffer.downstream_id();
                    b.resp = static_cast<std::uint8_t>(entry.resp);
                    flit = pack_b(b, here, stored);
                    rsp_id = flit.b.id;
                    rsp_field = flit.b.resp;
                } else {
                    const auto stored = buffer.pop_read();
                    axi_r_chan r{};
                    r.id = buffer.downstream_id();
                    r.data = entry.data;
                    r.resp = static_cast<std::uint8_t>(entry.resp);
                    r.last = true;
                    flit = pack_r(r, here, stored);
                    rsp_id = flit.r.id;
                    rsp_field = flit.r.data;
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
                      << std::hex << rsp_id << ',' << rsp_field << std::dec
                      << '\n';
                ++emitted;
            }
        }
        trace.close();

        if (argc == 4 && !compare_trace(argv[2], argv[3])) {
            return 1;
        }
        std::cout << "chimney_rsp_trace_sc PASS (" << emitted << " flits)\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "chimney_rsp_trace_sc: " << error.what() << '\n';
        return 2;
    }
}
