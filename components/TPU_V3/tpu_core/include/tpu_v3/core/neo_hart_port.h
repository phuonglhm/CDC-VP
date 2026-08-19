// SPDX-License-Identifier: Apache-2.0
//
// The hart's attachment point to the three NEO-CORE planes (plan §11.7).
//
// D15 splits the core into a 32-bit AXI4-Lite control plane, a native banked
// local-SRAM data plane and an external AXI4/NoC boundary. The hart does not
// speak three protocols: RISC-V VP++ exposes a single `CombinedMemoryInterface`
// — `instr_bus()` and `data_bus()` return the *same* TLM initiator socket —
// while `neo_local_sram_fabric` exposes only `sc_export<neo_local_sram_if>`
// and no TLM target at all. Nothing in the Phase 3 tree can bind to a hart
// without something in between, and this is it.
//
// What it does, per access, decided by absolute address alone:
//
//   core SRAM window   -> native local plane, as `neo_requester::cpu`
//   core-local MMIO    -> AXI4-Lite control fabric
//   anything else      -> external bridge, and from there the chip/global path
//
// Three properties are worth stating because each has a way of going wrong
// quietly:
//
// **It never waits.** It is a TLM target on the hart's path, so D16 binds it:
// the fabric behind it must be `annotated` in a full-system run. The port adds
// the fabric's charge to `delay` and returns.
//
// **It expands byte enables.** TLM's repeating pattern stops here
// (`INTERFACE_CONTRACT.md` §2); the native plane carries one strobe byte per
// data byte. The expansion lives in `neo_payload_rules.h`, shared with
// `neo_external_bridge`, because the two are the only components that do this
// and a second copy of the rule is how one of them ends up wrong.
//
// **It does not reassemble anything.** VP++ decomposes vector accesses per
// active element before the wrapper is reached (decision record D7), so this
// port sees element-wise traffic and must not infer a vector-instruction
// boundary from it. Its counters are named for what they measure.
//
// It is deliberately not a decoder "inside `tpu_core`": it owns response
// mapping, strobe expansion, straddle refusal and per-destination attribution,
// which is a component's worth of behaviour and a component's worth of tests.

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

/// Where one access went. Also the index space of the port's counters, so
/// "how much of this hart's traffic stayed in the core" is a number rather
/// than an inference.
enum class hart_destination : unsigned {
    local_sram = 0,
    control = 1,
    external = 2,
};

inline constexpr unsigned hart_destination_count = 3;

constexpr unsigned index_of(hart_destination destination) noexcept
{
    return static_cast<unsigned>(destination);
}

const char* to_string(hart_destination destination) noexcept;

/// The apertures this port decodes against — absolute, and supplied by the
/// composition from `address_map.h`. The port carries no address constant of
/// its own, for the same reason nothing else in TPU_V3 does.
struct hart_port_spec {
    /// `address_map::core_base(chip, core)` and the 32 MiB stride.
    std::uint64_t core_base = 0;
    std::uint64_t core_size = 0;
    /// The core SRAM window inside that aperture.
    std::uint64_t sram_base = 0;
    std::uint64_t sram_window = 0;
};

class neo_hart_port : public sc_core::sc_module {
public:
    neo_hart_port(sc_core::sc_module_name name, hart_port_spec spec,
                  neo_local_sram_fabric& fabric);

    /// Bind the hart's combined instruction/data initiator socket here.
    tlm_utils::simple_target_socket<neo_hart_port> from_hart;

    /// To the control fabric's `cpu` initiator port.
    tlm_utils::simple_initiator_socket<neo_hart_port> to_control;

    /// To `neo_external_bridge::local_outbound`. Not to a memory: the bridge
    /// is what refuses an outbound access that names this core, and binding
    /// past it would remove that check (plan §21).
    tlm_utils::simple_initiator_socket<neo_hart_port> to_external;

    const hart_port_spec& spec() const noexcept { return spec_; }

    /// True when the fabric behind this port blocks its requesters. A hart
    /// attached to an `arbitrated` fabric loses temporal decoupling on every
    /// local load and store (D16), so a platform that wants full-system speed
    /// has to be able to ask.
    bool blocks_on_arbitration() const noexcept
    {
        return fabric_.timing() == local_fabric_timing::arbitrated;
    }

    // ── counters ─────────────────────────────────────────────────────────────
    //
    // Named for what they measure (D7). These count *TLM requests arriving
    // from the hart*, not vector instructions, not hardware bus transactions,
    // and not physical bank beats — the fabric owns that last number and this
    // port does not relabel it.

    std::uint64_t requests(hart_destination destination) const;
    std::uint64_t bytes(hart_destination destination) const;

    /// Requests refused before they reached any destination.
    std::uint64_t protocol_errors() const noexcept { return protocol_errors_; }
    /// Refused for overlapping two planes at once.
    std::uint64_t straddle_errors() const noexcept { return straddle_errors_; }
    /// Refused because a single access larger than one RVV register cannot
    /// reach the native plane.
    std::uint64_t size_errors() const noexcept { return size_errors_; }
    /// Accepted, routed, and answered with an error by the destination.
    std::uint64_t destination_errors() const noexcept
    {
        return destination_errors_;
    }

    void reset();

    std::string report() const;

private:
    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay);
    unsigned int transport_dbg(tlm::tlm_generic_payload& trans);

    bool in_sram(std::uint64_t address, std::uint64_t length) const noexcept;
    bool in_core(std::uint64_t address, std::uint64_t length) const noexcept;

    /// Overlaps a plane boundary without lying inside either side of it.
    ///
    /// Refused rather than split. An access that covers the top of core SRAM
    /// and the bottom of the MMIO window decodes to two destinations at once,
    /// and there is no correct answer for which should have answered; the same
    /// reasoning `INTERFACE_CONTRACT.md` §7 applies at the NoC boundary.
    bool straddles_a_boundary(std::uint64_t address,
                              std::uint64_t length) const noexcept;

    hart_port_spec spec_;
    neo_local_sram_fabric& fabric_;

    std::uint64_t requests_[hart_destination_count] = {};
    std::uint64_t bytes_[hart_destination_count] = {};
    std::uint64_t protocol_errors_ = 0;
    std::uint64_t straddle_errors_ = 0;
    std::uint64_t size_errors_ = 0;
    std::uint64_t destination_errors_ = 0;
};

} // namespace cdc::components::tpu_v3::core
