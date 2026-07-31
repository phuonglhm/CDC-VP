// SPDX-License-Identifier: Apache-2.0
//
// Contract test for the single-AXI channel types and the FlooNoC sizing
// arithmetic.
//
// The absolute widths below are computed by hand from the frozen
// `axi_pkg` field constants for the reference configuration in
// `floogen/examples/axi_mesh_xy.yml` (addr 48, data 64, user 1, in-id 4,
// out-id 2). They are independently confirmed by
// `rtl_crosscheck/run_axi_sizing_crosscheck.sh`, which evaluates the real
// `floo_pkg` functions over the same configuration.

#include "floo_noc_model/axi_types.hpp"

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

void check_equal(unsigned actual, unsigned expected, const std::string& what)
{
    if (actual != expected) {
        std::cerr << "FAIL: " << what << " expected " << expected << ", got "
                  << actual << '\n';
        ++failures;
    }
}

} // namespace

// The exact `xRESP` encoding, checked at compile time against `axi_pkg.sv`:
//
//   localparam RESP_OKAY   = 2'b00;
//   localparam RESP_EXOKAY = 2'b01;
//   localparam RESP_SLVERR = 2'b10;
//   localparam RESP_DECERR = 2'b11;
//
// A `static_assert` rather than a runtime check because there is nothing to
// discover at runtime: if these values are ever edited, the build should stop.
// The literal `1` was once used in the wrapper and called SLVERR in a comment;
// it is EXOKAY, so every downstream failure was reported as a successful
// exclusive access.
static_assert(static_cast<unsigned>(floo::model::axi_pkg::axi_resp::okay) == 0b00,
              "RESP_OKAY is 2'b00");
static_assert(static_cast<unsigned>(floo::model::axi_pkg::axi_resp::exokay) == 0b01,
              "RESP_EXOKAY is 2'b01");
static_assert(static_cast<unsigned>(floo::model::axi_pkg::axi_resp::slverr) == 0b10,
              "RESP_SLVERR is 2'b10");
static_assert(static_cast<unsigned>(floo::model::axi_pkg::axi_resp::decerr) == 0b11,
              "RESP_DECERR is 2'b11");
static_assert(!floo::model::axi_pkg::is_error(floo::model::axi_pkg::axi_resp::okay)
              && !floo::model::axi_pkg::is_error(floo::model::axi_pkg::axi_resp::exokay),
              "OKAY and EXOKAY are both success codes");
static_assert(floo::model::axi_pkg::is_error(floo::model::axi_pkg::axi_resp::slverr)
              && floo::model::axi_pkg::is_error(floo::model::axi_pkg::axi_resp::decerr),
              "SLVERR and DECERR are both errors");

int sc_main(int, char**)
{
    // The FlooGen reference XY-mesh configuration.
    constexpr axi_cfg reference{48, 64, 1, 4, 2};

    // Address/control field widths shared by AW and AR:
    //   len 8 + size 3 + burst 2 + lock 1 + cache 4 + prot 3 + qos 4
    //   + region 4 = 29, plus atop 6 on AW only.
    check_equal(get_axi_chan_width(reference, axi_channel::aw),
                4 + 48 + 29 + 6 + 1, "aw width");
    check_equal(get_axi_chan_width(reference, axi_channel::ar),
                4 + 48 + 29 + 1, "ar width");
    // W: data + strobe(data/8) + last + user.
    check_equal(get_axi_chan_width(reference, axi_channel::w),
                64 + 8 + 1 + 1, "w width");
    // B: id + resp + user.
    check_equal(get_axi_chan_width(reference, axi_channel::b),
                4 + 2 + 1, "b width");
    // R: id + data + resp + last + user.
    check_equal(get_axi_chan_width(reference, axi_channel::r),
                4 + 64 + 2 + 1 + 1, "r width");

    // AW, W and AR ride `req`; B and R ride `rsp`.
    check(axi_chan_mapping(axi_channel::aw) == physical_channel::req,
          "AW must map to the req channel");
    check(axi_chan_mapping(axi_channel::w) == physical_channel::req,
          "W must map to the req channel");
    check(axi_chan_mapping(axi_channel::ar) == physical_channel::req,
          "AR must map to the req channel");
    check(axi_chan_mapping(axi_channel::b) == physical_channel::rsp,
          "B must map to the rsp channel");
    check(axi_chan_mapping(axi_channel::r) == physical_channel::rsp,
          "R must map to the rsp channel");

    // A physical channel is one bit wider than its widest payload: FlooNoC
    // always reserves at least one spare bit.
    const unsigned widest_req = 4 + 48 + 29 + 6 + 1;  // AW dominates
    const unsigned widest_rsp = 4 + 64 + 2 + 1 + 1;   // R dominates
    check_equal(get_max_axi_payload_bits(reference, physical_channel::req),
                widest_req + 1, "req channel width");
    check_equal(get_max_axi_payload_bits(reference, physical_channel::rsp),
                widest_rsp + 1, "rsp channel width");

    // Reserved bits pad each channel out to its physical channel width. The
    // widest payload on each side still gets exactly one spare bit.
    check_equal(get_axi_rsvd_bits(reference, axi_channel::aw), 1,
                "aw reserved bits");
    check_equal(get_axi_rsvd_bits(reference, axi_channel::r), 1,
                "r reserved bits");
    for (const auto channel : {axi_channel::aw, axi_channel::w,
                               axi_channel::ar, axi_channel::b,
                               axi_channel::r}) {
        const unsigned total =
            get_axi_chan_width(reference, channel)
            + get_axi_rsvd_bits(reference, channel);
        check_equal(total,
                    get_max_axi_payload_bits(
                        reference, axi_chan_mapping(channel)),
                    "payload plus reserved must fill the physical channel");
    }

    // A wider out-id must not change any width: FlooNoC sizes the flits from
    // InIdWidth only.
    constexpr axi_cfg wider_out_id{48, 64, 1, 4, 8};
    for (const auto channel : {axi_channel::aw, axi_channel::w,
                               axi_channel::ar, axi_channel::b,
                               axi_channel::r}) {
        check_equal(get_axi_chan_width(wider_out_id, channel),
                    get_axi_chan_width(reference, channel),
                    "OutIdWidth must not affect the flit sizing");
    }

    // The channel payload structs compare by value.
    axi_aw_chan aw{};
    aw.id = 3;
    aw.addr = 0x8000'0000;
    aw.len = 7;
    axi_aw_chan same = aw;
    check(aw == same, "identical AW payloads must compare equal");
    same.len = 8;
    check(!(aw == same), "a differing AW field must break equality");

    axi_r_chan r{};
    r.id = 3;
    r.data = 0xDEAD'BEEF;
    r.last = true;
    axi_r_chan r_copy = r;
    check(r == r_copy, "identical R payloads must compare equal");
    r_copy.last = false;
    check(!(r == r_copy), "a differing R field must break equality");

    if (failures == 0) {
        std::cout << "PASS: single-AXI channel types and sizing\n";
        return 0;
    }
    std::cerr << "FAIL: " << failures << " AXI type checks failed\n";
    return 1;
}
