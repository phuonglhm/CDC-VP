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
#include <stdexcept>
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
    write.strb = {0xFF};  // one beat, every lane enabled
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

    // ---- R2-F4: exact response codes, B and R ------------------------------
    //
    // The encoding itself is pinned by `static_assert` in `test_axi_types`.
    // What is checked here is that a code survives the journey: crafted into a
    // response flit, through the endpoint, out in the completion. A model that
    // discards the code, or maps `EXOKAY` to failure, or swaps `SLVERR` and
    // `DECERR`, fails here.
    {
        using axi_pkg::axi_resp;
        using axi_pkg::to_bits;

        // Fresh endpoints: the checks above deliberately leave a transaction
        // in flight, and reusing them here would take those flits by mistake.
        axi_manager_endpoint mgr{manager_node, chimney_destination{map}};
        axi_subordinate_endpoint mem{memory_node};

        const struct { axi_resp code; const char* name; bool error; } cases[] = {
            {axi_resp::okay,   "OKAY",   false},
            {axi_resp::exokay, "EXOKAY", false},
            {axi_resp::slverr, "SLVERR", true},
            {axi_resp::decerr, "DECERR", true},
        };

        for (const auto& item : cases) {
            // ---- B ---------------------------------------------------------
            axi_transaction write_txn{};
            write_txn.is_write = true;
            write_txn.id = 2;
            write_txn.addr = 0x8000'0000;
            write_txn.data = {0x1234};
            write_txn.strb = {0xFF};
            check(mgr.offer(write_txn), "the B-code write must be offered");
            mem.accept_request(mgr.take_request());
            mem.accept_request(mgr.take_request());
            const auto b_done =
                mgr.accept_response(mem.respond_write(to_bits(item.code)));
            check(b_done.resp == to_bits(item.code),
                  std::string("a B response must carry ") + item.name
                      + " through to the completion");
            check(axi_pkg::is_error(static_cast<axi_resp>(b_done.resp))
                      == item.error,
                  std::string(item.name)
                      + " must be classified as error/success correctly");

            // ---- R ---------------------------------------------------------
            axi_transaction read_txn{};
            read_txn.is_write = false;
            read_txn.id = 2;
            read_txn.addr = 0x8000'0000;
            read_txn.read_beats = 1;
            check(mgr.offer(read_txn), "the R-code read must be offered");
            mem.accept_request(mgr.take_request());
            const auto r_done =
                mgr.accept_response(mem.respond_read(0x99, to_bits(item.code)));
            check(r_done.resp == to_bits(item.code),
                  std::string("an R response must carry ") + item.name
                      + " through to the completion");
        }

        // ---- a multi-beat burst whose *middle* beat fails ------------------
        //
        // The failure must survive a later OKAY. Reporting the final beat's
        // code — which the endpoint used to do — turns a partial failure into
        // a clean success, and nothing downstream can tell.
        axi_transaction burst{};
        burst.is_write = false;
        burst.id = 2;
        burst.addr = 0x8000'0000;
        burst.read_beats = 3;
        check(mgr.offer(burst), "the burst read must be offered");
        mem.accept_request(mgr.take_request());

        const auto beats = mem.respond_read_burst({0xA, 0xB, 0xC},
                                                     to_bits(axi_resp::okay));
        check(beats.size() == 3, "the burst must produce three R beats");

        auto middle = beats[1];
        middle.r.resp = to_bits(axi_resp::slverr);

        (void)mgr.accept_response(beats[0]);
        const auto mid_done = mgr.accept_response(middle);
        check(mid_done.resp == to_bits(axi_resp::slverr),
              "the error must be visible as soon as the failing beat arrives");
        const auto burst_done = mgr.accept_response(beats[2]);
        check(burst_done.resp == to_bits(axi_resp::slverr),
              "a later OKAY beat must not erase an earlier SLVERR");
        check(burst_done.data.size() == 3,
              "the burst must still deliver every beat it received");

        // The next burst starts clean: the accumulated code must not leak.
        axi_transaction after{};
        after.is_write = false;
        after.id = 2;
        after.addr = 0x8000'0000;
        after.read_beats = 1;
        check(mgr.offer(after), "a following read must be offered");
        mem.accept_request(mgr.take_request());
        const auto clean =
            mgr.accept_response(mem.respond_read(0x5, to_bits(axi_resp::okay)));
        check(clean.resp == to_bits(axi_resp::okay),
              "a previous burst's error must not leak into the next response");
    }

    // ---- R3-F1: AxLEN is 8 bits, so 256 beats is the ceiling ---------------
    //
    // `static_cast<std::uint8_t>(257 - 1)` is 0. A 257-beat read used to be
    // accepted, arrive as `ARLEN = 0`, be answered with a single beat, and
    // complete OK with the caller's buffer mostly untouched.
    {
        axi_manager_endpoint edge{manager_node, chimney_destination{map}};

        axi_transaction max_read{};
        max_read.is_write = false;
        max_read.id = 4;
        max_read.addr = 0x8000'0000;
        max_read.read_beats = axi_pkg::max_burst_beats;
        check(edge.offer(max_read), "a 256-beat read must be accepted");
        check(edge.peek_request().ar.len == 255,
              "a 256-beat read must emit ARLEN = 255");
        while (edge.has_request()) {
            (void)edge.take_request();
        }

        axi_manager_endpoint edge_w{manager_node, chimney_destination{map}};
        axi_transaction max_write{};
        max_write.is_write = true;
        max_write.id = 4;
        max_write.addr = 0x8000'0000;
        max_write.data.assign(axi_pkg::max_burst_beats, 0);
        max_write.strb.assign(axi_pkg::max_burst_beats, 0xFF);
        check(edge_w.offer(max_write), "a 256-beat write must be accepted");
        check(edge_w.peek_request().aw.len == 255,
              "a 256-beat write must emit AWLEN = 255");

        // One beat too many, both directions, and nothing may change.
        axi_manager_endpoint over{manager_node, chimney_destination{map}};
        axi_transaction too_long_read = max_read;
        too_long_read.read_beats = axi_pkg::max_burst_beats + 1;
        bool threw = false;
        try {
            (void)over.offer(too_long_read);
        } catch (const std::invalid_argument&) {
            threw = true;
        }
        check(threw, "a 257-beat read must be rejected");
        check(!over.has_request(),
              "a rejected 257-beat read must queue no request flit");
        check(over.outstanding(too_long_read.id) == 0,
              "a rejected 257-beat read must not move the ordering counter");

        axi_transaction too_long_write{};
        too_long_write.is_write = true;
        too_long_write.id = 4;
        too_long_write.addr = 0x8000'0000;
        too_long_write.data.assign(axi_pkg::max_burst_beats + 1, 0);
        too_long_write.strb.assign(axi_pkg::max_burst_beats + 1, 0xFF);
        threw = false;
        try {
            (void)over.offer(too_long_write);
        } catch (const std::invalid_argument&) {
            threw = true;
        }
        check(threw, "a 257-beat write must be rejected");
        check(!over.has_request(),
              "a rejected 257-beat write must queue no request flit");
    }

    // ---- F5: a refused offer must change nothing ---------------------------
    //
    // Fill the read metadata buffer, then offer one more read. The refusal has
    // to be complete: no request flit queued, no counter moved. An earlier
    // version appended the flits first and only then discovered the buffer was
    // full, leaving a request in the queue that no response could ever match.
    {
        constexpr unsigned small_max_txns = 2;
        axi_manager_endpoint tight{
            manager_node, chimney_destination{map}, /*out_id_width=*/3,
            /*max_txns=*/small_max_txns, /*max_txns_per_id=*/32};

        axi_transaction read{};
        read.is_write = false;
        read.id = 3;
        read.addr = 0x8000'0000;
        read.read_beats = 1;

        for (unsigned index = 0; index < small_max_txns; ++index) {
            check(tight.offer(read),
                  "the metadata buffer must accept up to its capacity");
        }

        // Drain the request flits so `has_request()` below is unambiguous:
        // anything it reports afterwards was queued by the refused offer.
        while (tight.has_request()) {
            (void)tight.take_request();
        }
        const auto outstanding_before = tight.outstanding(read.id);

        // Caught rather than allowed to escape: an uncaught exception aborts
        // the run with a SystemC wrapper message, which is unstable evidence
        // for a negative control to match on. The assertion below says exactly
        // what went wrong.
        bool refused = false;
        bool offer_threw = false;
        try {
            refused = !tight.offer(read);
        } catch (const std::exception&) {
            offer_threw = true;
        }
        check(!offer_threw,
              "a full metadata buffer must refuse the offer, not throw: "
              "capacity is checked before any state changes");
        check(refused,
              "an offer against a full metadata buffer must be refused");
        check(!tight.has_request(),
              "a refused offer must queue no request flit");
        check(tight.outstanding(read.id) == outstanding_before,
              "a refused offer must not move the ordering counter");

        // A malformed write must throw *and* leave the endpoint untouched.
        axi_manager_endpoint clean{
            manager_node, chimney_destination{map}, 3, 32, 32};
        axi_transaction bad{};
        bad.is_write = true;
        bad.id = 1;
        bad.addr = 0x8000'0000;
        bad.data = {0x11, 0x22};
        bad.strb = {0xFF};  // one strobe for two beats: malformed
        bool threw = false;
        try {
            (void)clean.offer(bad);
        } catch (const std::invalid_argument&) {
            threw = true;
        }
        check(threw, "a write without one WSTRB per beat must be rejected");
        check(!clean.has_request(),
              "a rejected malformed write must queue no request flit");
        check(clean.outstanding(bad.id) == 0,
              "a rejected malformed write must not move the ordering counter");
    }

    if (failures == 0) {
        std::cout << "PASS: AXI endpoint transactors\n";
        return 0;
    }
    std::cerr << "FAIL: " << failures << " endpoint checks failed\n";
    return 1;
}
