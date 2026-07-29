// SPDX-License-Identifier: Apache-2.0
//
// Contract test for the abstract AXI endpoint transactors.
//
// These are a model-side abstraction, not an RTL block, so this test checks
// that they compose the RTL-signed rules correctly: flit assembly, AW/W
// coupling, response routing back to the requester, AXI ID restoration, and
// the NoRoB ordering rule.

#include "floo_noc_model/axi_endpoint.hpp"

#include <iostream>
#include <string>

namespace {

using namespace floo::model;

int failures = 0;

void check(bool condition, const std::string& message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

} // namespace

int sc_main(int, char**)
{
    const coordinate manager_node{1, 1};
    const coordinate memory_node{3, 2};
    const coordinate other_node{0, 3};

    reference_address_map map{{
        endpoint_region{0x8000'0000, 0x1000, memory_node},
        endpoint_region{0x9000'0000, 0x1000, other_node},
    }};

    axi_manager_endpoint manager{manager_node, chimney_destination{map}};
    axi_subordinate_endpoint memory{memory_node};

    // ---- A write: AW then W, one packet ---------------------------------

    axi_transaction write{};
    write.is_write = true;
    write.id = 5;
    write.addr = 0x8000'0040;
    write.data = {0xDEAD'BEEF};
    write.strb = 0xFF;
    check(manager.offer(write), "an idle manager must accept a write");

    check(manager.has_request(), "the write must produce request flits");
    const auto aw = manager.take_request();
    const auto w = manager.take_request();
    check(!manager.has_request(), "a single-beat write is exactly two flits");

    check(aw.hdr.axi_ch == static_cast<unsigned>(axi_channel::aw),
          "the first flit must be the AW");
    check(!aw.hdr.last, "AW must not close the packet");
    check(aw.hdr.dst_id == memory_node, "AW must target the decoded endpoint");
    check(w.hdr.axi_ch == static_cast<unsigned>(axi_channel::w),
          "the second flit must be the W");
    check(w.hdr.last, "the final W beat must close the packet");
    check(w.hdr.dst_id == memory_node,
          "W must follow the AW's destination, not decode its own");

    memory.accept_request(aw);
    check(!memory.has_write(),
          "an AW alone must not yet be answerable: its W is still coming");
    memory.accept_request(w);
    check(memory.has_write(), "the completed AW/W pair must be answerable");

    const auto b = memory.respond_write(0);
    check(b.hdr.axi_ch == static_cast<unsigned>(axi_channel::b),
          "the write answer must be a B flit");
    check(b.hdr.dst_id == manager_node,
          "the response must route back to the requester");
    check(b.b.id == write.id,
          "the manager's original AXI id must be restored");

    const auto write_done = manager.accept_response(b);
    check(write_done.is_write && write_done.id == write.id,
          "the completion must identify the original transaction");
    check(manager.outstanding(write.id) == 0,
          "completing must release the ordering gate");

    // ---- A read ----------------------------------------------------------

    axi_transaction read{};
    read.is_write = false;
    read.id = 2;
    read.addr = 0x8000'0080;
    check(manager.offer(read), "the manager must accept a read");

    const auto ar = manager.take_request();
    check(!manager.has_request(), "a read is a single flit");
    check(ar.hdr.axi_ch == static_cast<unsigned>(axi_channel::ar),
          "the read must produce an AR flit");
    check(ar.hdr.last, "AR is its own packet");

    memory.accept_request(ar);
    check(memory.has_read(), "the AR must be answerable immediately");
    const auto r = memory.respond_read(0x1234'5678, 0);
    check(r.hdr.dst_id == manager_node, "the R must route back");
    check(r.r.id == read.id, "the read's AXI id must be restored");
    check(r.hdr.last, "every R flit closes its packet");

    const auto read_done = manager.accept_response(r);
    check(!read_done.is_write && read_done.data.front() == 0x1234'5678,
          "the read completion must carry the data");

    // ---- The NoRoB ordering rule -----------------------------------------

    axi_transaction first{};
    first.is_write = false;
    first.id = 7;
    first.addr = 0x8000'0000;  // memory_node
    check(manager.offer(first), "a fresh id must be admitted");

    axi_transaction same_target{};
    same_target.is_write = false;
    same_target.id = 7;
    same_target.addr = 0x8000'0100;  // still memory_node
    check(manager.offer(same_target),
          "reusing an id for the same destination must not stall");

    axi_transaction other_target{};
    other_target.is_write = false;
    other_target.id = 7;
    other_target.addr = 0x9000'0000;  // other_node
    check(!manager.offer(other_target),
          "reusing an id for a different destination must stall");

    // Drain the two in-flight reads, then the id is free again.
    memory.accept_request(manager.take_request());
    memory.accept_request(manager.take_request());
    manager.accept_response(memory.respond_read(0xAA, 0));
    check(!manager.offer(other_target),
          "one outstanding transaction still locks the destination");
    manager.accept_response(memory.respond_read(0xBB, 0));
    check(manager.offer(other_target),
          "a fully drained id must admit a new destination");

    if (failures == 0) {
        std::cout << "PASS: AXI endpoint transactors\n";
        return 0;
    }
    std::cerr << "FAIL: " << failures << " endpoint checks failed\n";
    return 1;
}
