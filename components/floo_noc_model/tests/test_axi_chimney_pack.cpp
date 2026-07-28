// SPDX-License-Identifier: Apache-2.0
//
// Contract test for the chimney flit assembly, destination resolution, and the
// AW/W select FSM.
//
// Every expectation is read out of `hw/floo_axi_chimney.sv` at the frozen
// revision, not out of a run. This is a contract test, not an equivalence
// proof: the packing has no RTL cross-check yet, so a failure here means the
// model drifted from what the RTL text says, and a pass does not certify
// equivalence.

#include "floo_noc_model/axi_chimney_pack.hpp"

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
    const coordinate here{1, 1};
    const coordinate there{3, 2};

    // ---- Request packing -------------------------------------------------

    axi_aw_chan aw{};
    aw.id = 5;
    aw.addr = 0x8000'1000;
    aw.len = 3;
    aw.atop = 0;

    const auto aw_flit = pack_aw(aw, here, there);
    check(!aw_flit.hdr.last,
          "AW must not terminate the packet: the W burst does");
    check(aw_flit.hdr.axi_ch == static_cast<unsigned>(axi_channel::aw),
          "AW flit must be tagged as the AW channel");
    check(aw_flit.hdr.dst_id == there, "AW must carry the decoded destination");
    check(aw_flit.hdr.src_id == here, "AW must carry this chimney's id");
    check(!aw_flit.hdr.atop, "a non-atomic AW must clear the atop flag");
    check(aw_flit.aw == aw, "AW payload must pass through unchanged");

    axi_aw_chan atomic_aw = aw;
    atomic_aw.atop = 0x30;
    check(pack_aw(atomic_aw, here, there).hdr.atop,
          "hdr.atop is a flag derived from atop != ATOP_NONE");

    axi_w_chan w{};
    w.data = 0xDEAD'BEEF;
    w.strb = 0xFF;
    w.last = false;

    const rob_tag aw_tag{true, 7};
    const auto w_flit = pack_w(w, here, there, aw_tag);
    check(!w_flit.hdr.last, "a non-final W beat must not terminate the packet");
    check(w_flit.hdr.rob_req == aw_tag.rob_req
              && w_flit.hdr.rob_idx == aw_tag.rob_idx,
          "W carries the AW's reorder tag, not one of its own");
    check(w_flit.hdr.axi_ch == static_cast<unsigned>(axi_channel::w),
          "W flit must be tagged as the W channel");

    axi_w_chan last_w = w;
    last_w.last = true;
    check(pack_w(last_w, here, there, aw_tag).hdr.last,
          "the final W beat must terminate the packet");

    axi_ar_chan ar{};
    ar.id = 2;
    ar.addr = 0x8000'2000;
    const auto ar_flit = pack_ar(ar, here, there);
    check(ar_flit.hdr.last, "AR is a single-flit packet");
    check(ar_flit.hdr.axi_ch == static_cast<unsigned>(axi_channel::ar),
          "AR flit must be tagged as the AR channel");
    check(ar_flit.ar == ar, "AR payload must pass through unchanged");

    // ---- Response packing ------------------------------------------------

    // The requester sat at `there` and used AXI id 9; downstream the chimney
    // reissued the transaction under a different id.
    response_meta meta{};
    meta.src_id = there;
    meta.axi_id = 9;
    meta.rob_req = false;
    meta.rob_idx = 0;

    axi_b_chan b{};
    b.id = 0;  // downstream id, must be overwritten
    b.resp = 1;

    const auto b_flit = pack_b(b, here, meta);
    check(b_flit.hdr.last, "B is a single-flit packet");
    check(b_flit.hdr.dst_id == there,
          "a response routes back to the requester's src_id");
    check(b_flit.hdr.src_id == here, "B must carry this chimney's id");
    check(b_flit.b.id == meta.axi_id,
          "the manager's original AXI id must be restored into the B payload");
    check(b_flit.b.resp == b.resp, "the B response code must pass through");

    axi_r_chan r{};
    r.id = 0;
    r.data = 0x1234;
    r.last = false;  // mid-burst AXI beat

    const auto r_flit = pack_r(r, here, meta);
    check(r_flit.hdr.last,
          "every R flit terminates its packet: R bursts are not wormholed");
    check(r_flit.r.last == false,
          "the AXI r.last beat flag is independent of the flit's hdr.last");
    check(r_flit.r.id == meta.axi_id,
          "the manager's original AXI id must be restored into the R payload");

    // ---- Destination resolution ------------------------------------------

    chimney_destination destination{reference_address_map{{
        endpoint_region{0x8000'0000, 0x1000, coordinate{3, 2}},
        endpoint_region{0x8000'1000, 0x1000, coordinate{0, 3}},
    }}};

    const auto first = destination.decode_request(0x8000'0000);
    check(first.has_value() && *first == coordinate(3, 2),
          "an address in the first region must decode to its endpoint");
    const auto second = destination.decode_request(0x8000'1FFF);
    check(second.has_value() && *second == coordinate(0, 3),
          "the last address of a region still belongs to it");
    check(!destination.decode_request(0x9000'0000).has_value(),
          "an unmapped address must not decode");

    // Offset mode: the coordinate comes straight out of address bit fields,
    // as `hw/test/floo_test_pkg.sv` configures it (x at bit 16, y at bit 20).
    chimney_destination offsets{xy_addr_offsets{16, 2, 20, 2}};
    check(offsets.decode_request(0x0032'0000).value() == coordinate(2, 3),
          "offset mode must extract x and y from their address fields");
    check(offsets.decode_request(0x0000'0000).value() == coordinate(0, 0),
          "a zero address decodes to the origin in offset mode");
    check(!offsets.uses_id_table(),
          "offset mode must not report itself as table-based");
    check(destination.uses_id_table(),
          "table mode must report itself as table-based");

    // W follows the latched AW destination, never its own decode.
    destination.accept_aw(coordinate{0, 3});
    check(destination.write_destination() == coordinate(0, 3),
          "W must use the destination latched when the AW was accepted");
    destination.accept_aw(coordinate{3, 2});
    check(destination.write_destination() == coordinate(3, 2),
          "a later AW must relatch the write destination");

    // ---- AW/W select FSM --------------------------------------------------

    aw_w_select select;
    check(select.expects_aw(), "the select FSM must reset to AW");

    select.clock(/*aw_accepted=*/true, /*w_accepted=*/false, /*w_last=*/false);
    check(select.expects_w(), "an accepted AW must hand the channel to W");

    select.clock(false, true, false);
    check(select.expects_w(), "a non-final W beat must keep the channel");

    select.clock(false, true, true);
    check(select.expects_aw(), "the final W beat must return the channel");

    // A single-beat burst: the AW and its only W are accepted in the same
    // cycle. The RTL evaluates the `w.last` assignment second, so the FSM ends
    // that cycle back at AW.
    select.reset();
    select.clock(true, true, true);
    check(select.expects_aw(),
          "a same-cycle AW and final W must leave the FSM at AW");

    if (failures == 0) {
        std::cout << "PASS: chimney packing, destinations, AW/W select\n";
        return 0;
    }
    std::cerr << "FAIL: " << failures << " chimney checks failed\n";
    return 1;
}
