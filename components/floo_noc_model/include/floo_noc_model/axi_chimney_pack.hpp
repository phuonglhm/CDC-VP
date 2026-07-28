// SPDX-License-Identifier: SHL-0.51
//
// Single-AXI chimney flit assembly and destination resolution.
//
// Source of truth: `hw/floo_axi_chimney.sv` at the frozen revision. Each
// packing function below mirrors one `always_comb` block of that file, and the
// destination rules mirror its `gen_route` generate block over
// `hw/floo_id_translation.sv`.
//
// Scope: vertical slice v0, so `RouteAlgo = XYRouting`, unicast, and no
// collectives. `dst_id = id_out` because the source-routing table is not used.
//
// `floo_id_translation` has two destination-decode modes under XY routing, and
// the frozen tree uses both, so the model implements both:
//
//   UseIdTable = 1  a system-address-map lookup. This is what
//                   `floogen/examples/axi_mesh_xy.yml` selects.
//   UseIdTable = 0  bit-field extraction straight from the address:
//                     id.x = addr[XYAddrOffsetX +: $bits(id.x)]
//                     id.y = addr[XYAddrOffsetY +: $bits(id.y)]
//                     id.port_id = '0    (the RTL notes it is unsupported)
//                   This is what `hw/test/floo_test_pkg.sv` selects, and
//                   therefore what the upstream chimney testbench exercises.
//
// **Verification status: not yet RTL cross-checked.** The rules here were read
// out of the RTL rather than measured against it. Isolating the packing alone
// is not possible, because it is inline `always_comb` inside the chimney
// rather than a separate module; a real cross-check has to instantiate the
// whole chimney, which also drags in the meta buffer and the reorder buffers.
// Until that harness exists, treat everything in this header as
// cycle-approximate and unproven, exactly like any other block before its
// cross-check.

#pragma once

#include "floo_noc_model/axi_types.hpp"
#include "floo_noc_model/floo_types.hpp"
#include "floo_noc_model/reference_model.hpp"

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <systemc>

namespace floo::model {

/// A flit on the `req` physical channel. The RTL uses a packed union, so only
/// the payload named by `hdr.axi_ch` is meaningful; this struct keeps the same
/// contract without claiming a bit-accurate layout.
struct axi_req_flit {
    flit_header hdr{};
    axi_aw_chan aw{};
    axi_w_chan w{};
    axi_ar_chan ar{};
};

/// A flit on the `rsp` physical channel. Same union contract as above.
struct axi_rsp_flit {
    flit_header hdr{};
    axi_b_chan b{};
    axi_r_chan r{};
};

/// Reorder-buffer fields the chimney stamps into a request flit.
///
/// Beware the default: `hw/floo_rob_wrapper.sv` drives `ax_rob_req_o = 1'b1`
/// even in its `NoRoB` branch, with `ax_rob_idx_o = '0`. So "reorder buffer
/// disabled" does **not** mean `rob_req = 0` on the wire; it means every
/// request flit carries `rob_req = 1` and index 0. The default below matches
/// that, not the intuitive reading.
struct rob_tag {
    bool rob_req{true};
    unsigned rob_idx{0};
};

/// Mirrors `floo_axi_chimney.sv`'s AW assembly.
///
/// Note `hdr.last = 1'b0`: an AW never ends a packet. The following W burst
/// does, which is what couples AW and W into one wormhole-routed packet.
inline axi_req_flit pack_aw(
    const axi_aw_chan& aw,
    const coordinate& src_id,
    const coordinate& dst_id,
    const rob_tag& tag = {})
{
    axi_req_flit flit{};
    flit.hdr.rob_req = tag.rob_req;
    flit.hdr.rob_idx = tag.rob_idx;
    flit.hdr.dst_id = dst_id;
    flit.hdr.collective_mask = 0;
    flit.hdr.src_id = src_id;
    flit.hdr.last = false;
    flit.hdr.axi_ch = static_cast<unsigned>(axi_channel::aw);
    // `atop` is the "this AW is atomic" flag, not the ATOP code itself.
    flit.hdr.atop = aw.atop != 0;
    flit.hdr.collective_op = static_cast<unsigned>(collect_op::unicast);
    flit.aw = aw;
    return flit;
}

/// Mirrors the W assembly.
///
/// Two details that a hand-written model easily gets wrong: the W flit carries
/// the **AW's** reorder-buffer tag, not one of its own, and `hdr.last` comes
/// from the AXI `w.last` beat flag, so the packet ends with the burst.
inline axi_req_flit pack_w(
    const axi_w_chan& w,
    const coordinate& src_id,
    const coordinate& dst_id,
    const rob_tag& aw_tag = {})
{
    axi_req_flit flit{};
    flit.hdr.rob_req = aw_tag.rob_req;
    flit.hdr.rob_idx = aw_tag.rob_idx;
    flit.hdr.dst_id = dst_id;
    flit.hdr.collective_mask = 0;
    flit.hdr.src_id = src_id;
    flit.hdr.last = w.last;
    flit.hdr.axi_ch = static_cast<unsigned>(axi_channel::w);
    flit.hdr.collective_op = static_cast<unsigned>(collect_op::unicast);
    flit.w = w;
    return flit;
}

/// Mirrors the AR assembly. `hdr.last = 1'b1`: an AR is a single-flit packet.
inline axi_req_flit pack_ar(
    const axi_ar_chan& ar,
    const coordinate& src_id,
    const coordinate& dst_id,
    const rob_tag& tag = {})
{
    axi_req_flit flit{};
    flit.hdr.rob_req = tag.rob_req;
    flit.hdr.rob_idx = tag.rob_idx;
    flit.hdr.dst_id = dst_id;
    flit.hdr.collective_mask = 0;
    flit.hdr.src_id = src_id;
    flit.hdr.last = true;
    flit.hdr.axi_ch = static_cast<unsigned>(axi_channel::ar);
    flit.hdr.collective_op = static_cast<unsigned>(collect_op::unicast);
    flit.ar = ar;
    return flit;
}

/// Metadata the chimney retains per outstanding request and replays into the
/// response flit. Mirrors what `aw_out_hdr_out`/`ar_out_hdr_out` carry out of
/// `floo_meta_buffer.sv`.
struct response_meta {
    /// Where the request came from; the response routes back to it.
    coordinate src_id{};
    /// The manager's original AXI ID, restored into the response payload.
    std::uint64_t axi_id{};
    bool rob_req{false};
    unsigned rob_idx{0};
    bool atop{false};
};

/// Mirrors the B assembly. `hdr.last = 1'b1`, the destination is the
/// requester's `src_id`, and the payload ID is restored from the retained
/// metadata rather than taken from the downstream subordinate.
inline axi_rsp_flit pack_b(
    const axi_b_chan& b,
    const coordinate& src_id,
    const response_meta& meta)
{
    axi_rsp_flit flit{};
    flit.hdr.rob_req = meta.rob_req;
    flit.hdr.rob_idx = meta.rob_idx;
    flit.hdr.dst_id = meta.src_id;
    flit.hdr.collective_mask = 0;
    flit.hdr.src_id = src_id;
    flit.hdr.last = true;
    flit.hdr.axi_ch = static_cast<unsigned>(axi_channel::b);
    flit.hdr.atop = meta.atop;
    flit.hdr.collective_op = static_cast<unsigned>(collect_op::unicast);
    flit.b = b;
    flit.b.id = meta.axi_id;
    return flit;
}

/// Mirrors the R assembly. The RTL comments that `hdr.last` is tied high
/// because there is no reason to wormhole-route an R burst, so every R beat is
/// its own packet even mid-burst.
inline axi_rsp_flit pack_r(
    const axi_r_chan& r,
    const coordinate& src_id,
    const response_meta& meta)
{
    axi_rsp_flit flit{};
    flit.hdr.rob_req = meta.rob_req;
    flit.hdr.rob_idx = meta.rob_idx;
    flit.hdr.dst_id = meta.src_id;
    flit.hdr.collective_mask = 0;
    flit.hdr.src_id = src_id;
    flit.hdr.last = true;
    flit.hdr.axi_ch = static_cast<unsigned>(axi_channel::r);
    flit.hdr.atop = meta.atop;
    flit.hdr.collective_op = static_cast<unsigned>(collect_op::unicast);
    flit.r = r;
    flit.r.id = meta.axi_id;
    return flit;
}

/// XY coordinate extraction from an address, mirroring `floo_id_translation`
/// with `UseIdTable = 0` and `RouteAlgo = XYRouting`.
///
/// `port_id` is tied to zero: the RTL comments that multiple local ports are
/// "not supported at the moment" in this mode.
struct xy_addr_offsets {
    unsigned x_offset{};
    unsigned x_bits{};
    unsigned y_offset{};
    unsigned y_bits{};

    coordinate decode(std::uint64_t address) const
    {
        const auto field = [&](unsigned offset, unsigned bits) {
            if (bits == 0 || bits >= 64) {
                throw std::invalid_argument(
                    "FlooNoC XY address field width out of range");
            }
            return static_cast<unsigned>(
                (address >> offset) & ((1ull << bits) - 1));
        };
        return coordinate(field(x_offset, x_bits), field(y_offset, y_bits), 0);
    }
};

/// Destination resolution, mirroring the chimney's `gen_route` block.
///
/// AW and AR decode their own address. W does not decode anything: it reuses
/// the ID latched when the preceding AW was accepted, which is what keeps a
/// write burst on one route. B and R route back to the requester's `src_id`.
class chimney_destination {
public:
    /// `UseIdTable = 1`: decode through the system address map.
    explicit chimney_destination(reference_address_map address_map)
        : address_map_(std::move(address_map))
    {
    }

    /// `UseIdTable = 0`: extract the coordinate from address bit fields.
    explicit chimney_destination(xy_addr_offsets offsets)
        : offsets_(offsets)
    {
    }

    /// Mirrors `floo_id_translation`. In table mode an unmapped address yields
    /// no value, and the RTL asserts `DecodeError` for that case. In offset
    /// mode every address decodes, because it is a bit-field extraction.
    std::optional<coordinate> decode_request(std::uint64_t address) const
    {
        if (offsets_.has_value()) {
            return offsets_->decode(address);
        }
        if (!address_map_.has_value()) {
            throw std::logic_error("chimney_destination has no decode mode");
        }
        return address_map_->decode(address);
    }

    /// Latches the AW destination, mirroring
    /// `FFL(axi_aw_id_q, id_out[AxiAw], aw_valid && aw_ready, '0)`.
    void accept_aw(const coordinate& destination)
    {
        latched_aw_destination_ = destination;
    }

    /// The destination a W flit uses: the previous AW's ID.
    const coordinate& write_destination() const
    {
        return latched_aw_destination_;
    }

    bool uses_id_table() const { return address_map_.has_value(); }

private:
    std::optional<reference_address_map> address_map_;
    std::optional<xy_addr_offsets> offsets_;
    coordinate latched_aw_destination_{};
};

/// Mirrors the chimney's `aw_w_sel_q` state:
///
///   if (aw_valid && aw_ready)                    -> SelW
///   if (w_valid && w_ready && w.last)            -> SelAw
///   reset value                                   = SelAw
///
/// This is what serialises an AW and its W burst onto the shared `req`
/// channel, and what stops a second AW from overtaking an in-flight burst.
class aw_w_select {
public:
    enum class selection { aw, w };

    selection state() const { return state_; }
    bool expects_aw() const { return state_ == selection::aw; }
    bool expects_w() const { return state_ == selection::w; }

    void reset() { state_ = selection::aw; }

    /// One clock of the select FSM. Both handshakes are evaluated in the same
    /// cycle, in the RTL's order: a `w.last` acceptance wins, so a single-beat
    /// burst returns to `SelAw` immediately.
    void clock(bool aw_accepted, bool w_accepted, bool w_last)
    {
        if (aw_accepted) {
            state_ = selection::w;
        }
        if (w_accepted && w_last) {
            state_ = selection::aw;
        }
    }

private:
    selection state_{selection::aw};
};

} // namespace floo::model
