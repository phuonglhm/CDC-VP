// SPDX-License-Identifier: Apache-2.0
//
// NEO external bridge — the bidirectional AXI4/NoC boundary of one NEO-CORE
// (decision record D15, plan §11.7).
//
// Two directions, and the interesting rules are on opposite sides of it.
//
// **Inbound.** A remote chip, the sibling core or the host loader names this
// core's SRAM and registers by exactly the same addresses the core itself uses
// (`ADDRESS_MAP.md` §4). The bridge decodes them back into the local planes:
// MMIO onto the AXI4-Lite control fabric, SRAM data into the native fabric as
// the named `external_inbound` requester. It never reaches storage directly.
// That is the whole point — D15 says inbound traffic "never bypasses normal
// arbitration", and the way to make that checkable is that every inbound byte
// appears in the fabric's `external_inbound` counters. `core_sram` exposes no
// backing pointer, so the totals must reconcile, and
// `test_neo_external_bridge` checks that they do.
//
// **Outbound.** An address outside this core goes out. An address *inside*
// this core must never go out: local containment is a decode consequence, not
// a routing decision made per transaction (plan §9.2, `ARCHITECTURE.md` §3),
// and it is also what makes the FlooNoC NoLoopback constraint survivable. A
// local address arriving on the outbound port therefore means the core-local
// decoder upstream is wrong, so the bridge refuses it and counts it rather
// than forwarding it into the mesh where it would deadlock or, worse, work.
//
// ## Blocking
//
// Inbound SRAM traffic goes through the native fabric, and an `arbitrated`
// fabric blocks its requester while another one holds the bank. That is
// correct back-pressure for a local requester and *not* acceptable for a path
// that starts at the NoC, where the mesh clock is advanced by one process
// (`INTERFACE_CONTRACT.md` §3). A bridge attached to the detailed NoC must
// therefore front an `annotated` fabric; `blocks_on_arbitration()` reports
// which it got, and the platform is what has to choose correctly.

#pragma once

#include <cstdint>
#include <string>

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

#include "tpu_v3/core/neo_local_sram_fabric.h"
#include "tpu_v3/core/neo_payload_rules.h"

namespace cdc::components::tpu_v3::core {

/// The apertures the bridge decodes against. All absolute, all from
/// `address_map.h`; the bridge carries no address constant of its own.
struct core_aperture_spec {
    /// `address_map::core_base(chip, core)` and the 32 MiB stride.
    std::uint64_t core_base = 0;
    std::uint64_t core_size = 0;
    /// The core SRAM window inside that aperture.
    std::uint64_t sram_base = 0;
    std::uint64_t sram_window = 0;
};

/// The named initiators that leave a NEO-CORE (D15). Closed, like every other
/// requester list in this core: outbound traffic with no owner would be the
/// one path whose metrics nothing could be traced back to.
enum class outbound_initiator : unsigned {
    /// VP++ instruction fetch and global data. Architecturally required — the
    /// reset PC is in `GLOBAL_BOOT_ROM`, outside the core.
    cpu = 0,
    /// The independent NEO DMA's external port; the bulk mover.
    dma = 1,
};

inline constexpr unsigned outbound_initiator_count = 2;

const char* to_string(outbound_initiator initiator) noexcept;

class neo_external_bridge : public sc_core::sc_module {
public:
    neo_external_bridge(sc_core::sc_module_name name, core_aperture_spec spec,
                        neo_local_sram_fabric& fabric);

    /// From the chip-local fabric or NoC endpoint into this core.
    tlm_utils::simple_target_socket<neo_external_bridge> inbound;
    /// Inbound MMIO, re-issued on the control plane. Bind to the control
    /// fabric's `external_inbound` initiator port.
    tlm_utils::simple_initiator_socket<neo_external_bridge> inbound_control;

    /// From inside the core towards chip/global/remote memory, one socket per
    /// named outbound initiator and indexed by `outbound_initiator`.
    ///
    /// A vector rather than one socket because a NEO-CORE has exactly two
    /// things that leave it — the hart, which must reach global boot ROM for
    /// its first fetch, and the DMA, which is the bulk mover (D15) — and
    /// `INTERFACE_CONTRACT.md` §5 requires every in-flight request to carry an
    /// identifiable owner. One shared socket would have made outbound traffic
    /// the one path in the core with no owner, and SystemC would have refused
    /// the second bind anyway.
    sc_core::sc_vector<
        tlm_utils::simple_target_socket_tagged<neo_external_bridge>>
        local_outbound;
    /// To the chip-local fabric or NoC endpoint.
    tlm_utils::simple_initiator_socket<neo_external_bridge> external;

    const core_aperture_spec& aperture() const noexcept { return spec_; }

    bool blocks_on_arbitration() const noexcept
    {
        return fabric_.timing() == local_fabric_timing::arbitrated;
    }

    // ── counters ─────────────────────────────────────────────────────────────

    std::uint64_t inbound_sram_requests() const noexcept
    {
        return inbound_sram_requests_;
    }
    std::uint64_t inbound_sram_bytes() const noexcept
    {
        return inbound_sram_bytes_;
    }
    std::uint64_t inbound_mmio_requests() const noexcept
    {
        return inbound_mmio_requests_;
    }
    std::uint64_t inbound_rejected() const noexcept
    {
        return inbound_rejected_;
    }
    std::uint64_t outbound_requests() const noexcept
    {
        return outbound_requests_;
    }
    /// Outbound requests attributed to one initiator.
    std::uint64_t outbound_requests(outbound_initiator initiator) const;
    /// Outbound accesses refused for naming an address inside this core.
    ///
    /// This should stay at zero in a correct system. It is counted rather than
    /// asserted so an integration bug shows up as a number in the report
    /// instead of taking the simulation down, and so a negative test can prove
    /// the refusal happens at all.
    std::uint64_t outbound_local_refused() const noexcept
    {
        return outbound_local_refused_;
    }

    void reset();

    std::string report() const;

private:
    void inbound_b_transport(tlm::tlm_generic_payload& trans,
                             sc_core::sc_time& delay);
    unsigned int inbound_transport_dbg(tlm::tlm_generic_payload& trans);
    void outbound_b_transport(int id, tlm::tlm_generic_payload& trans,
                              sc_core::sc_time& delay);
    unsigned int outbound_transport_dbg(int id,
                                        tlm::tlm_generic_payload& trans);

    bool in_sram(std::uint64_t address, std::uint64_t length) const noexcept;
    bool in_core(std::uint64_t address, std::uint64_t length) const noexcept;

    /// Touches this core at all, even by one byte. This — not containment —
    /// is what the outbound containment rule tests: a transfer that starts
    /// inside the aperture and runs past it is not contained, and forwarding
    /// it puts local traffic in the mesh just the same.
    bool overlaps_core(std::uint64_t address,
                       std::uint64_t length) const noexcept;

    /// Overlaps a region without lying inside it, at either the SRAM/MMIO
    /// edge or the edge of the core aperture. Such a transfer decodes to two
    /// targets at once and there is no correct answer for which should
    /// answer, so it is refused rather than routed by whichever test ran
    /// first.
    bool straddles_a_boundary(std::uint64_t address,
                              std::uint64_t length) const noexcept;

    core_aperture_spec spec_;
    neo_local_sram_fabric& fabric_;

    std::uint64_t inbound_sram_requests_ = 0;
    std::uint64_t inbound_sram_bytes_ = 0;
    std::uint64_t inbound_mmio_requests_ = 0;
    std::uint64_t inbound_rejected_ = 0;
    std::uint64_t outbound_requests_ = 0;
    std::uint64_t outbound_by_initiator_[outbound_initiator_count] = {};
    std::uint64_t outbound_local_refused_ = 0;
};

// `to_tlm_response`, the payload rules and the byte-enable expansion now live
// in `neo_payload_rules.h`, shared with `neo_hart_port`. They were private to
// this file until Phase 7 added a second TLM-to-native converter; one copy of
// the expansion rule is the point, not the tidiness.

} // namespace cdc::components::tpu_v3::core
