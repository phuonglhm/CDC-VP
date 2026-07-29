// SPDX-License-Identifier: Apache-2.0
//
// End-to-end AXI over the two-network NoC.
//
// This is the vertical slice: an AXI transaction leaves a manager endpoint as
// request flits, is routed across the `req` mesh, is absorbed by a subordinate
// endpoint, and its response is routed back across the separate `rsp` mesh.
//
// What is RTL-signed underneath: the routers (214 cycles), their input FIFOs
// (133 cycles), the wormhole arbiters (152 cycles), flit assembly (16 + 8
// flits), the `NoRoB` ordering rule (127 cycles), and the chimney request
// path's timing (141 cycles). What is not: the endpoint transactors, which
// have no RTL counterpart, and inter-node timing, which needs a mesh-level
// cross-check against the FlooGen-generated top.
//
// The latency checks below are deliberately *structural* rather than exact.
// Every router registers its input (`InFifoDepth = 2`, a spill register), so a
// flit costs at least one cycle per hop and a round trip at least twice the
// Manhattan distance. Asserting that, and that a farther endpoint really does
// cost more than a nearer one, is what distinguishes "the mesh was traversed"
// from "something short-circuited and the test still passed".

#include "floo_noc_model/axi_endpoint.hpp"
#include "floo_noc_model/axi_noc.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

using namespace floo::model;

constexpr unsigned mesh_width = 4;
constexpr unsigned mesh_height = 4;
using noc_t = axi_noc<mesh_width, mesh_height>;

int failures = 0;

void check(bool condition, const std::string& message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

unsigned manhattan(const coordinate& lhs, const coordinate& rhs)
{
    const auto dx = std::abs(
        static_cast<int>(lhs.x.to_uint()) - static_cast<int>(rhs.x.to_uint()));
    const auto dy = std::abs(
        static_cast<int>(lhs.y.to_uint()) - static_cast<int>(rhs.y.to_uint()));
    return static_cast<unsigned>(dx + dy);
}

/// A subordinate endpoint plus the queue of answers waiting for the `rsp`
/// network to accept them.
struct memory_agent {
    coordinate node;
    unsigned index;
    axi_subordinate_endpoint endpoint;
    std::vector<axi_rsp_flit> outbox;
    std::uint64_t stored{};

    memory_agent(const coordinate& id)
        : node(id)
        , index(noc_t::node_index(id))
        , endpoint(id)
    {
    }
};

} // namespace

int sc_main(int, char**)
{
    const coordinate manager_node{0, 0};
    const coordinate near_node{1, 0};   // one hop
    const coordinate far_node{3, 3};    // six hops

    reference_address_map map{{
        endpoint_region{0x8000'0000, 0x1000, near_node},
        endpoint_region{0x9000'0000, 0x1000, far_node},
    }};

    sc_core::sc_signal<bool> clk{"clk"};
    sc_core::sc_signal<bool> rst_n{"rst_n"};

    noc_t noc{"noc"};
    noc.i_clk(clk);
    noc.i_rst_n(rst_n);

    const unsigned manager_index = noc_t::node_index(manager_node);
    axi_manager_endpoint manager{manager_node, chimney_destination{map}};

    std::vector<memory_agent> memories;
    memories.emplace_back(near_node);
    memories.emplace_back(far_node);

    auto& manager_req = noc.req(manager_index);
    auto& manager_rsp = noc.rsp(manager_index);

    std::vector<axi_completion> completions;
    std::vector<unsigned> completed_at;
    unsigned now = 0;

    clk.write(false);
    rst_n.write(false);
    manager_req.inject_valid.write(false);
    manager_rsp.eject_ready.write(true);
    for (auto& memory : memories) {
        noc.req(memory.index).eject_ready.write(true);
        noc.rsp(memory.index).inject_valid.write(false);
    }
    sc_core::sc_start(sc_core::SC_ZERO_TIME);

    const auto step = [&](unsigned cycles) {
        for (unsigned cycle = 0; cycle < cycles; ++cycle) {
            ++now;
            clk.write(false);

            const bool has_request = manager.has_request();
            if (has_request) {
                manager_req.inject_data.write(manager.peek_request());
            }
            manager_req.inject_valid.write(has_request);
            manager_rsp.eject_ready.write(true);

            for (auto& memory : memories) {
                const bool has_response = !memory.outbox.empty();
                if (has_response) {
                    noc.rsp(memory.index).inject_data.write(memory.outbox.front());
                }
                noc.rsp(memory.index).inject_valid.write(has_response);
                noc.req(memory.index).eject_ready.write(true);
            }
            sc_core::sc_start(sc_core::sc_time(1, sc_core::SC_NS));

            // Sample what the DUT presents for this whole cycle.
            const bool request_accepted =
                has_request && manager_req.inject_ready.read();
            const bool response_arrived = manager_rsp.eject_valid.read();
            const auto arrived_response = manager_rsp.eject_data.read();

            std::vector<bool> response_accepted;
            std::vector<bool> request_arrived;
            std::vector<axi_req_flit> arrived_request;
            for (auto& memory : memories) {
                response_accepted.push_back(
                    !memory.outbox.empty()
                    && noc.rsp(memory.index).inject_ready.read());
                request_arrived.push_back(
                    noc.req(memory.index).eject_valid.read());
                arrived_request.push_back(
                    noc.req(memory.index).eject_data.read());
            }

            clk.write(true);
            sc_core::sc_start(sc_core::sc_time(1, sc_core::SC_NS));

            if (request_accepted) {
                manager.take_request();
            }
            for (std::size_t slot = 0; slot < memories.size(); ++slot) {
                auto& memory = memories[slot];
                if (response_accepted[slot]) {
                    memory.outbox.erase(memory.outbox.begin());
                }
                if (!request_arrived[slot]) {
                    continue;
                }
                memory.endpoint.accept_request(arrived_request[slot]);
                const auto channel = static_cast<axi_channel>(
                    arrived_request[slot].hdr.axi_ch.to_uint());
                if (channel == axi_channel::w) {
                    memory.stored = arrived_request[slot].w.data;
                }
                if (memory.endpoint.has_write()) {
                    memory.outbox.push_back(memory.endpoint.respond_write(0));
                } else if (memory.endpoint.has_read()) {
                    memory.outbox.push_back(
                        memory.endpoint.respond_read(memory.stored, 0));
                }
            }
            if (response_arrived) {
                completions.push_back(manager.accept_response(arrived_response));
                completed_at.push_back(now);
            }

            clk.write(false);
            sc_core::sc_start(sc_core::sc_time(1, sc_core::SC_NS));
        }
    };

    step(4);
    rst_n.write(true);
    step(4);

    // ---- A write to the near endpoint, then a read of the same location ---

    const unsigned near_offered = now;
    axi_transaction write{};
    write.is_write = true;
    write.id = 1;
    write.addr = 0x8000'0040;
    write.data = {0xDEAD'BEEF};
    write.strb = 0xFF;
    check(manager.offer(write), "the manager must accept the write");
    step(60);

    check(completions.size() == 1, "the write must complete end to end");
    if (completions.empty()) {
        std::cerr << "FAIL: nothing completed, the rest cannot be judged\n";
        return 1;
    }
    check(completions[0].is_write, "the completion must be the write's");
    check(completions[0].id == write.id,
          "the manager's original AXI id must be restored across the NoC");
    check(memories[0].stored == write.data.front(),
          "the write data must have reached the near memory node");
    check(memories[1].stored == 0,
          "the far memory must not have seen a transaction addressed elsewhere");

    const unsigned near_latency = completed_at[0] - near_offered;
    check(near_latency >= 2 * manhattan(manager_node, near_node),
          "a round trip cannot be faster than one cycle per hop each way");

    axi_transaction read{};
    read.is_write = false;
    read.id = 2;
    read.addr = 0x8000'0040;
    check(manager.offer(read), "the manager must accept the read");
    step(60);

    check(completions.size() == 2, "the read must complete end to end");
    if (completions.size() == 2) {
        check(!completions[1].is_write, "the second completion is the read's");
        check(completions[1].data.front() == write.data.front(),
              "the read must return what the write stored");
        check(completions[1].id == read.id, "the read's AXI id must survive");
    }

    // ---- The same transaction to the far endpoint -------------------------
    //
    // Issued only once the near one has drained. With `MaxUniqueIds = 1` the
    // chimney's metadata is a plain in-order FIFO (`fifo_v3` in the
    // `gen_no_atop_fifos` branch of `hw/floo_meta_buffer.sv`), so it assumes
    // responses return in request order. Two endpoints at different distances
    // do not guarantee that, which is a real constraint of the frozen
    // configuration rather than a limitation of this test.

    const unsigned far_offered = now;
    axi_transaction far_read{};
    far_read.is_write = false;
    far_read.id = 4;
    far_read.addr = 0x9000'0000;
    check(manager.offer(far_read), "the manager must accept the far read");
    step(80);

    check(completions.size() == 3, "the far read must complete end to end");
    if (completions.size() == 3) {
        const unsigned far_latency = completed_at[2] - far_offered;
        check(far_latency >= 2 * manhattan(manager_node, far_node),
              "the far round trip cannot beat one cycle per hop each way");
        check(far_latency > near_latency,
              "a six-hop endpoint must cost more than a one-hop endpoint");
        std::cout << "near " << manhattan(manager_node, near_node)
                  << " hops: " << near_latency << " cycles; far "
                  << manhattan(manager_node, far_node)
                  << " hops: " << far_latency << " cycles\n";
    }

    // ---- Several transactions in flight to one destination ----------------

    const std::size_t before = completions.size();
    for (unsigned index = 0; index < 3; ++index) {
        axi_transaction burst{};
        burst.is_write = false;
        burst.id = 5 + index;
        burst.addr = 0x8000'0000 + index * 8;
        check(manager.offer(burst), "a fresh id must be admitted");
    }
    step(80);
    check(completions.size() == before + 3,
          "every outstanding read must complete");

    // ---- The ordering rule, end to end ------------------------------------

    axi_transaction first{};
    first.is_write = false;
    first.id = 9;
    first.addr = 0x8000'0000;
    check(manager.offer(first), "a fresh id must be admitted");

    axi_transaction elsewhere{};
    elsewhere.is_write = false;
    elsewhere.id = 9;
    elsewhere.addr = 0x9000'0000;  // the far endpoint
    check(!manager.offer(elsewhere),
          "the same id to a different destination must stall while in flight");
    step(60);
    check(manager.offer(elsewhere),
          "once drained, the id must be admitted to the new destination");

    if (failures == 0) {
        std::cout << "PASS: AXI end to end over the two-network NoC\n";
        return 0;
    }
    std::cerr << "FAIL: " << failures << " NoC checks failed\n";
    return 1;
}
