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
    ///
    /// **One socket for two initiators, so the bridge arbitrates between
    /// them.** That is not an optimisation, it is what makes the downstream
    /// invariant true: the chip fabric treats each core as one initiator and
    /// allows it one transaction at a time, and without arbitration here the
    /// hart and the DMA can both be inside this call the moment anything
    /// downstream blocks. It cannot happen while every target merely annotates
    /// delay, which is why it stayed invisible until a chip fabric in
    /// `arbitrated` mode existed — and it would have surfaced again at the
    /// first real NoC hop.
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

    /// Outbound requests that found the shared external port occupied by the
    /// other initiator. The back-pressure between a core's hart and its DMA,
    /// and the number that says whether a run exercised the arbiter at all.
    std::uint64_t outbound_conflicts() const noexcept
    {
        return outbound_conflicts_;
    }

    /// Grants the external port issued to one initiator. Rotating priority,
    /// so this is what makes fairness between the hart and the DMA measurable
    /// rather than claimed.
    std::uint64_t outbound_grants(outbound_initiator initiator) const;
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

    /// Clear the counters and abandon every **queued** outbound request; leave
    /// the external port alone if an initiator is still inside its downstream
    /// call.
    ///
    /// Undocumented until Phase 8 gave this component an arbiter, and the two
    /// populations are worth naming because they are treated differently. A
    /// request queued for the shared external port is woken, sees the new
    /// generation, and completes with `TLM_GENERIC_ERROR_RESPONSE` instead of
    /// being forwarded. A request already inside `external->b_transport()`
    /// **keeps the port**: reset cannot unwind a blocked C++ call, so releasing
    /// on its behalf would put both initiators on the socket at once. It
    /// releases the port itself when it unwinds.
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

    /// Block until this initiator owns the shared external port, then hold it
    /// across the downstream call.
    ///
    /// Returns false when a `reset()` abandoned the request **while it was
    /// still queued**; a request that has been granted the port always reaches
    /// its release, because ownership is a fact about a C++ call stack that a
    /// reset cannot revoke.
    ///
    /// Waiting here is safe in every configuration: the callers are the hart's
    /// own thread and the DMA's worker, both of which may `wait()`. The rule
    /// that a component on the NoC-reachable path must never wait applies to
    /// `inbound`, which does not pass through here.
    ///
    /// ## What it does with `delay`, and the one thing it does not model
    ///
    /// A contending request consumes its `delay` before queueing, so the
    /// arbiter orders contenders by when they actually arrive rather than by
    /// which process SystemC happened to run first. An **uncontended** request
    /// keeps its quantum and takes the free port immediately.
    ///
    /// That leaves one thing unmodelled, and it is the standard loosely-timed
    /// trade: a hart running ahead of simulated time can claim a free port
    /// before a DMA that is, in simulated time, earlier. Fixing it would mean
    /// consuming the delay on every outbound access, which synchronises the
    /// hart to global time on every instruction fetch and removes temporal
    /// decoupling entirely — the cost D16 refuses for the local plane, for the
    /// same reason. Arbitration order between a decoupled hart and a DMA is
    /// therefore approximate, and no fairness figure taken from it is a
    /// hardware claim.
    bool acquire_external(unsigned initiator, sc_core::sc_time& delay,
                          std::uint64_t request_generation);
    void release_external();
    unsigned select_outbound_waiter() const noexcept;

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

    // ── the shared external port ─────────────────────────────────────────────
    bool external_busy_ = false;
    bool external_waiting_[outbound_initiator_count] = {};
    /// Rotating priority: the next grant starts one past this.
    unsigned external_last_granted_ = outbound_initiator_count - 1;
    /// Notified whenever the port frees or a waiter appears, so a blocked
    /// initiator re-evaluates instead of polling.
    sc_core::sc_event external_changed_;
    std::uint64_t outbound_grants_[outbound_initiator_count] = {};
    std::uint64_t outbound_conflicts_ = 0;

    /// Bumped by `reset()`. A request carrying an older generation abandons
    /// itself at its next resume point rather than waiting for a grant that
    /// will never come — the defect the local SRAM fabric had once, and the
    /// same fix.
    std::uint64_t generation_ = 0;
};

// `to_tlm_response`, the payload rules and the byte-enable expansion now live
// in `neo_payload_rules.h`, shared with `neo_hart_port`. They were private to
// this file until Phase 7 added a second TLM-to-native converter; one copy of
// the expansion rule is the point, not the tidiness.

} // namespace cdc::components::tpu_v3::core
