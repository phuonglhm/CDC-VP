// SPDX-License-Identifier: Apache-2.0
//
// The payload rules every TPU_V3 component that converts a TLM transaction
// into a native local-plane request has to apply, in one place.
//
// There are two such components: `neo_external_bridge`, which converts inbound
// remote traffic, and `neo_hart_port`, which converts the hart's own. They are
// on opposite sides of the core and they apply the *same* rules, so the rules
// live here rather than in two anonymous namespaces.
//
// That is not tidiness. `INTERFACE_CONTRACT.md` §2 singles out the
// byte-enable expansion as a rule whose violation "reads past the end of the
// initiator's array and applies whatever it finds — a wrong result and an
// overread at once", and the bridge shipped that exact defect once already.
// Two copies of a rule like that diverge in one direction only: the copy with
// the test stays right and the copy without it does not.

#pragma once

#include <cstdint>
#include <vector>

#include <tlm>

#include "tpu_v3/sram/native_port.h"

namespace cdc::components::tpu_v3::core {

/// True when `[a, a+la)` and `[b, b+lb)` share a byte.
///
/// Written as a subtraction on whichever side cannot underflow, so a length
/// near 2^64 cannot wrap the comparison into a false answer (plan §12 rule 5).
/// `address_map::contains()` is not enough for a containment rule: a transfer
/// that starts inside a region and ends outside it is not *contained*, and
/// treating that as "not local" is precisely how local traffic escapes into
/// the mesh.
inline bool ranges_overlap(std::uint64_t a, std::uint64_t la, std::uint64_t b,
                           std::uint64_t lb) noexcept
{
    if (la == 0 || lb == 0) {
        return false;
    }
    if (a >= b) {
        return (a - b) < lb;
    }
    return (b - a) < la;
}

/// Expand TLM's byte-enable pattern into one byte per data byte.
///
/// TLM lets `byte_enable_length` be shorter than `data_length`, in which case
/// the pattern repeats. The native local plane has no such rule — `wstrb` is
/// one bit per byte of the transfer — so expanding it is the adapter's job.
/// Forwarding the raw pointer, as the first version of the bridge did, reads
/// past the end of the caller's array for any payload longer than the pattern
/// and applies whatever happens to be there.
///
/// Returns false for a malformed pattern: a non-null pointer with zero length
/// describes no bytes at all and cannot be repeated into anything.
inline bool expand_byte_enables(const tlm::tlm_generic_payload& trans,
                                std::vector<unsigned char>& expanded)
{
    expanded.clear();
    const unsigned char* pattern = trans.get_byte_enable_ptr();
    if (pattern == nullptr) {
        return true;
    }
    const unsigned int pattern_length = trans.get_byte_enable_length();
    if (pattern_length == 0) {
        return false;
    }
    const unsigned int length = trans.get_data_length();
    expanded.resize(length);
    for (unsigned int i = 0; i < length; ++i) {
        expanded[i] = pattern[i % pattern_length];
    }
    return true;
}

enum class payload_rule_error {
    none,
    command,
    burst,
};

/// The payload rules a memory-like target shares with every other TPU_V3
/// target (`INTERFACE_CONTRACT.md` §1). Kept side-effect free so the same
/// rules govern both normal and debug transport.
inline payload_rule_error
common_payload_error(const tlm::tlm_generic_payload& trans) noexcept
{
    const auto command = trans.get_command();
    if (command != tlm::TLM_READ_COMMAND && command != tlm::TLM_WRITE_COMMAND) {
        return payload_rule_error::command;
    }
    // A wrapped streaming transfer repeats the payload over a narrower window.
    // Nothing in TPU_V3 implements that, so it is refused rather than served
    // as if the field were absent.
    const unsigned int streaming = trans.get_streaming_width();
    if (streaming != 0 && streaming < trans.get_data_length()) {
        return payload_rule_error::burst;
    }
    if (trans.get_byte_enable_ptr() != nullptr
        && trans.get_byte_enable_length() == 0) {
        return payload_rule_error::burst;
    }
    return payload_rule_error::none;
}

/// Normal transport reports the exact protocol error through the TLM response.
inline bool check_common_payload_rules(tlm::tlm_generic_payload& trans)
{
    switch (common_payload_error(trans)) {
    case payload_rule_error::none:
        return true;
    case payload_rule_error::command:
        trans.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
        return false;
    case payload_rule_error::burst:
        trans.set_response_status(tlm::TLM_BURST_ERROR_RESPONSE);
        return false;
    }
    return false;
}

/// Map a native local-plane status onto a TLM response.
///
/// Kept in one place because the mapping is a policy, not a detail: a decode
/// miss and a capacity overrun are different conditions and must not collapse
/// into one generic error, and neither may become `TLM_OK_RESPONSE` with zero
/// data (`INTERFACE_CONTRACT.md` §1).
inline tlm::tlm_response_status
to_tlm_response(sram::neo_status status) noexcept
{
    switch (status) {
    case sram::neo_status::ok:
        return tlm::TLM_OK_RESPONSE;
    case sram::neo_status::decode_error:
        return tlm::TLM_ADDRESS_ERROR_RESPONSE;
    case sram::neo_status::capacity_error:
        // The address decoded; the target refused it because nothing is backed
        // there. That is `TLM_GENERIC_ERROR_RESPONSE` by
        // `INTERFACE_CONTRACT.md` §1 — an address error would tell the
        // initiator the map is wrong when the capacity is what is too small.
        return tlm::TLM_GENERIC_ERROR_RESPONSE;
    case sram::neo_status::size_error:
        return tlm::TLM_BURST_ERROR_RESPONSE;
    case sram::neo_status::aborted:
        // The access was legal and the target abandoned it because a reset
        // arrived. Generic, not address: the map is right and the transfer
        // simply did not survive.
        return tlm::TLM_GENERIC_ERROR_RESPONSE;
    }
    return tlm::TLM_GENERIC_ERROR_RESPONSE;
}

} // namespace cdc::components::tpu_v3::core
