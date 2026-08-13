// SPDX-License-Identifier: Apache-2.0
//
// NEO control fabric — the 32-bit AXI4-Lite control plane of one NEO-CORE
// (decision record D15, plan §11.7).
//
// One narrow, in-order decoder from a small set of named control initiators to
// the core's register files. No bursts, no AXI IDs, at most one transaction in
// flight per initiator, 4-byte naturally aligned accesses with full strobes.
//
// ## Why it is separate from the data plane
//
// Because they are different structures in RTL, not because they are different
// C++ classes. AXI4-Lite is the right shape for software-visible control and
// the wrong shape for the high-bandwidth path, where five channels, IDs and a
// central crossbar buy nothing. D15 froze that split, so a component that
// routed accelerator bulk data through here would be building the thing the
// decision exists to avoid — which is why a payload that is not exactly four
// bytes is refused rather than split.
//
// ## What it models
//
// Transactions, not channels. There is no AW/W/B/AR/R timing here and the
// model does not claim any. What it does model exactly is the *rules*: the
// widths, the alignment, the strobes, the single-transaction limit and the
// response propagation, because those are the parts firmware and an RTL
// integrator can both be wrong about.

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

namespace cdc::components::tpu_v3::core {

/// The named initiators on the control plane (D15). VP++ programming
/// registers, and authorized inbound remote traffic arriving through the
/// external bridge. Both are arbitrated and counted; neither is special.
enum class control_initiator : unsigned {
    cpu = 0,
    external_inbound = 1,
};

inline constexpr unsigned control_initiator_count = 2;

const char* to_string(control_initiator initiator) noexcept;

/// One decoded control target.
struct control_target_spec {
    std::uint64_t base = 0;
    std::uint64_t size = 0;
    std::string name;
};

class neo_control_fabric : public sc_core::sc_module {
public:
    /// `targets` are decoded in the order given and must not overlap; the
    /// constructor throws `std::invalid_argument` naming both regions if they
    /// do. An overlapping control map is not a runtime condition to resolve —
    /// there is no correct answer for which target should have answered.
    neo_control_fabric(sc_core::sc_module_name name,
                       std::vector<control_target_spec> targets);

    /// One per `control_initiator`, indexed by its value.
    sc_core::sc_vector<
        tlm_utils::simple_target_socket_tagged<neo_control_fabric>>
        from;

    /// One per entry in `targets`, in the order they were given.
    sc_core::sc_vector<tlm_utils::simple_initiator_socket<neo_control_fabric>>
        to;

    const std::vector<control_target_spec>& targets() const noexcept
    {
        return targets_;
    }

    /// Index of the target decoding `[address, length)`, or `-1`.
    int decode(std::uint64_t address, std::uint64_t length) const noexcept;

    // ── counters ─────────────────────────────────────────────────────────────

    std::uint64_t requests(control_initiator initiator) const;
    std::uint64_t forwarded(std::size_t target_index) const;

    /// Refused for violating an MMIO rule: wrong width, misaligned, partial
    /// strobes, a wrapped streaming transfer, or a command that is neither
    /// read nor write.
    std::uint64_t protocol_errors() const noexcept { return protocol_errors_; }
    /// Refused because nothing decodes the address.
    std::uint64_t decode_errors() const noexcept { return decode_errors_; }
    /// Refused because the target itself said no. Counted apart from the two
    /// above because it means something different: the access was legal and
    /// the target declined it.
    std::uint64_t target_errors() const noexcept { return target_errors_; }

    /// The largest number of transactions ever in flight on one initiator
    /// port. AXI4-Lite here accepts one; the counter exists so a test can show
    /// the limit is enforced rather than merely never exercised.
    unsigned peak_in_flight(control_initiator initiator) const;

    void reset();

    std::string report() const;

private:
    void b_transport(int id, tlm::tlm_generic_payload& trans,
                     sc_core::sc_time& delay);
    unsigned int transport_dbg(int id, tlm::tlm_generic_payload& trans);

    /// The AXI4-Lite access rules. Sets the response status and returns false
    /// when the access is refused.
    bool check_axi4lite_rules(tlm::tlm_generic_payload& trans);

    std::vector<control_target_spec> targets_;

    std::uint64_t requests_[control_initiator_count] = {};
    unsigned in_flight_[control_initiator_count] = {};
    unsigned peak_in_flight_[control_initiator_count] = {};
    std::vector<std::uint64_t> forwarded_;

    std::uint64_t protocol_errors_ = 0;
    std::uint64_t decode_errors_ = 0;
    std::uint64_t target_errors_ = 0;
};

} // namespace cdc::components::tpu_v3::core
