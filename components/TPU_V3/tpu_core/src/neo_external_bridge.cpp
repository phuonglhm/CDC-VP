// SPDX-License-Identifier: Apache-2.0

#include "tpu_v3/core/neo_external_bridge.h"

#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <vector>

#include "tpu_v3/address_map.h"

namespace cdc::components::tpu_v3::core {

namespace {

std::string hex(std::uint64_t value)
{
    std::ostringstream out;
    out << "0x" << std::hex << std::setw(8) << std::setfill('0') << value;
    return out.str();
}

/// True when `[a, a+la)` and `[b, b+lb)` share a byte.
///
/// Written as a subtraction on whichever side cannot underflow, so a length
/// near 2^64 cannot wrap the comparison into a false answer (plan §12 rule 5).
/// `contains()` is not enough for the containment rule: a transfer that starts
/// inside this core and ends outside it is not *contained*, and treating that
/// as "not local" is precisely how local traffic escapes into the mesh.
bool ranges_overlap(std::uint64_t a, std::uint64_t la, std::uint64_t b,
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
/// Forwarding the raw pointer, as the first version of this file did, reads
/// past the end of the caller's array for any payload longer than the pattern
/// and applies whatever happens to be there.
///
/// Returns false for a malformed pattern: a non-null pointer with zero length
/// describes no bytes at all and cannot be repeated into anything.
bool expand_byte_enables(const tlm::tlm_generic_payload& trans,
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
/// target (`INTERFACE_CONTRACT.md` §1). Kept side-effect free so the same rules
/// govern both normal and debug transport.
payload_rule_error
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
bool check_common_payload_rules(tlm::tlm_generic_payload& trans)
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

} // namespace

tlm::tlm_response_status to_tlm_response(sram::neo_status status) noexcept
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

neo_external_bridge::neo_external_bridge(sc_core::sc_module_name name,
                                         core_aperture_spec spec,
                                         neo_local_sram_fabric& fabric)
    : sc_core::sc_module(name)
    , inbound("inbound")
    , inbound_control("inbound_control")
    , local_outbound("local_outbound")
    , external("external")
    , spec_(std::move(spec))
    , fabric_(fabric)
{
    if (spec_.core_size == 0 || spec_.sram_window == 0) {
        throw std::invalid_argument("tpu_v3::neo_external_bridge["
                                    + std::string(name)
                                    + "]: the core aperture and SRAM window "
                                      "must both be non-empty");
    }
    if (!address_map::contains(spec_.core_base, spec_.core_size,
                               spec_.sram_base, spec_.sram_window)) {
        throw std::invalid_argument(
            "tpu_v3::neo_external_bridge[" + std::string(name)
            + "]: the SRAM window " + hex(spec_.sram_base) + '+'
            + hex(spec_.sram_window) + " is not inside the core aperture "
            + hex(spec_.core_base) + '+' + hex(spec_.core_size));
    }
    if (!fabric_.is_attached(sram::neo_requester::external_inbound)) {
        // Without this the bridge would be a requester the fabric refuses on
        // its first transaction, which is a worse failure than one during
        // elaboration: the platform would build, run and then die on the
        // first remote access.
        throw std::invalid_argument(
            "tpu_v3::neo_external_bridge[" + std::string(name)
            + "]: the local SRAM fabric does not have 'external_inbound' "
              "among its attached requesters, so inbound remote traffic would "
              "have no owner");
    }

    inbound.register_b_transport(this,
                                 &neo_external_bridge::inbound_b_transport);
    inbound.register_transport_dbg(this,
                                   &neo_external_bridge::inbound_transport_dbg);
    local_outbound.register_b_transport(
        this, &neo_external_bridge::outbound_b_transport);
    local_outbound.register_transport_dbg(
        this, &neo_external_bridge::outbound_transport_dbg);
}

bool neo_external_bridge::in_sram(std::uint64_t address,
                                  std::uint64_t length) const noexcept
{
    return address_map::contains(spec_.sram_base, spec_.sram_window, address,
                                 length);
}

bool neo_external_bridge::in_core(std::uint64_t address,
                                  std::uint64_t length) const noexcept
{
    return address_map::contains(spec_.core_base, spec_.core_size, address,
                                 length);
}

bool neo_external_bridge::overlaps_core(std::uint64_t address,
                                        std::uint64_t length) const noexcept
{
    return ranges_overlap(address, length, spec_.core_base, spec_.core_size);
}

bool neo_external_bridge::straddles_a_boundary(
    std::uint64_t address, std::uint64_t length) const noexcept
{
    // Touching a region without lying inside it. Either boundary counts: the
    // SRAM/MMIO edge inside the core, and the edge of the core aperture
    // itself.
    if (ranges_overlap(address, length, spec_.sram_base, spec_.sram_window)
        && !in_sram(address, length)) {
        return true;
    }
    return overlaps_core(address, length) && !in_core(address, length);
}

void neo_external_bridge::inbound_b_transport(tlm::tlm_generic_payload& trans,
                                              sc_core::sc_time& delay)
{
    trans.set_dmi_allowed(false);

    const std::uint64_t address = trans.get_address();
    const unsigned int length = trans.get_data_length();
    const auto command = trans.get_command();

    if (!check_common_payload_rules(trans)) {
        ++inbound_rejected_;
        return;
    }
    if (length == 0 || trans.get_data_ptr() == nullptr) {
        throw std::runtime_error(
            "tpu_v3::neo_external_bridge[" + std::string(name())
            + "]: an inbound transaction had zero length or a null data "
              "pointer -- this is a model or integration defect, not a guest "
              "fault");
    }

    // A transfer that overlaps a region without lying inside it decodes to two
    // targets at once, and there is no correct answer for which should answer.
    // Refused rather than routed by whichever test happened to run first.
    if (straddles_a_boundary(address, length)) {
        trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
        ++inbound_rejected_;
        return;
    }

    if (in_sram(address, length)) {
        if (length > sram::neo_max_transfer_bytes) {
            // The chip endpoint chunks oversized transfers before they reach a
            // core (`INTERFACE_CONTRACT.md` §7); one arriving here means that
            // did not happen, so it is refused rather than silently split.
            trans.set_response_status(tlm::TLM_BURST_ERROR_RESPONSE);
            ++inbound_rejected_;
            return;
        }

        std::vector<unsigned char> enables;
        if (!expand_byte_enables(trans, enables)) {
            trans.set_response_status(tlm::TLM_BURST_ERROR_RESPONSE);
            ++inbound_rejected_;
            return;
        }

        // Through the fabric as a named requester, arbitrated like any other.
        // There is no path from here to the storage that skips this call, and
        // `core_sram` hands out no pointer that would let one exist.
        sram::neo_local_request request;
        request.requester = sram::neo_requester::external_inbound;
        request.command = command == tlm::TLM_WRITE_COMMAND
            ? sram::neo_command::write
            : sram::neo_command::read;
        request.address = address;
        request.size = length;
        request.data = trans.get_data_ptr();
        request.strobes = enables.empty() ? nullptr : enables.data();

        sram::neo_local_response response;
        fabric_.b_access(request, response, delay);

        trans.set_response_status(to_tlm_response(response.status));
        ++inbound_sram_requests_;
        inbound_sram_bytes_ += response.bytes;
        if (response.status != sram::neo_status::ok) {
            ++inbound_rejected_;
        }
        return;
    }

    if (in_core(address, length)) {
        // Core-local MMIO. Re-issued on the AXI4-Lite control plane, which
        // applies its own width, alignment and strobe rules; the bridge does
        // not pre-judge them, so a remote master gets exactly the same
        // refusals the local core would.
        ++inbound_mmio_requests_;
        inbound_control->b_transport(trans, delay);
        if (trans.get_response_status() != tlm::TLM_OK_RESPONSE) {
            ++inbound_rejected_;
        }
        return;
    }

    trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
    ++inbound_rejected_;
}

unsigned int
neo_external_bridge::inbound_transport_dbg(tlm::tlm_generic_payload& trans)
{
    const std::uint64_t address = trans.get_address();
    const unsigned int length = trans.get_data_length();
    // The debug path bypasses arbitration and latency, not the payload rules
    // (`INTERFACE_CONTRACT.md` §8). That includes command, streaming width and
    // byte-enable shape; otherwise debug loading and simulated traffic see two
    // different devices.
    if (common_payload_error(trans) != payload_rule_error::none || length == 0
        || trans.get_data_ptr() == nullptr) {
        return 0;
    }
    const auto command = trans.get_command();
    if (straddles_a_boundary(address, length)) {
        return 0;
    }

    if (in_sram(address, length)) {
        if (length == 0 || length > sram::neo_max_transfer_bytes
            || trans.get_data_ptr() == nullptr) {
            return 0;
        }
        std::vector<unsigned char> enables;
        if (!expand_byte_enables(trans, enables)) {
            return 0;
        }
        sram::neo_local_request request;
        request.requester = sram::neo_requester::external_inbound;
        request.command = command == tlm::TLM_WRITE_COMMAND
            ? sram::neo_command::write
            : sram::neo_command::read;
        request.address = address;
        request.size = length;
        request.data = trans.get_data_ptr();
        request.strobes = enables.empty() ? nullptr : enables.data();
        return fabric_.dbg_access(request);
    }
    if (in_core(address, length)) {
        return inbound_control->transport_dbg(trans);
    }
    return 0;
}

void neo_external_bridge::outbound_b_transport(tlm::tlm_generic_payload& trans,
                                               sc_core::sc_time& delay)
{
    trans.set_dmi_allowed(false);

    // The outbound bridge is still a TPU_V3 target. Do not rely on the external
    // target to reject a malformed transaction: that would make correctness
    // depend on what happened to be bound on the far side and would count a
    // protocol error as forwarded traffic here.
    if (!check_common_payload_rules(trans)) {
        return;
    }
    if (trans.get_data_length() == 0 || trans.get_data_ptr() == nullptr) {
        throw std::runtime_error(
            "tpu_v3::neo_external_bridge[" + std::string(name())
            + "]: an outbound transaction had zero length or a null data "
              "pointer -- this is a model or integration defect, not a guest "
              "fault");
    }

    // **Overlap**, not containment. A transfer that begins inside this core
    // and runs past its aperture is not *contained* by it, and the first
    // version of this check therefore forwarded it — one byte of local traffic
    // in the mesh is still local traffic in the mesh. Local containment is a
    // decode consequence, not a per-transaction routing decision (plan §9.2):
    // an address touching this core arriving on the outbound port means the
    // core-local decoder upstream is wrong, and handing it to the interconnect
    // would either deadlock against NoLoopback or — worse — work, and hide the
    // bug.
    if (overlaps_core(trans.get_address(), trans.get_data_length())) {
        trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
        ++outbound_local_refused_;
        return;
    }

    ++outbound_requests_;
    external->b_transport(trans, delay);
}

unsigned int
neo_external_bridge::outbound_transport_dbg(tlm::tlm_generic_payload& trans)
{
    if (common_payload_error(trans) != payload_rule_error::none
        || trans.get_data_length() == 0 || trans.get_data_ptr() == nullptr) {
        return 0;
    }
    if (overlaps_core(trans.get_address(), trans.get_data_length())) {
        return 0;
    }
    return external->transport_dbg(trans);
}

void neo_external_bridge::reset()
{
    inbound_sram_requests_ = 0;
    inbound_sram_bytes_ = 0;
    inbound_mmio_requests_ = 0;
    inbound_rejected_ = 0;
    outbound_requests_ = 0;
    outbound_local_refused_ = 0;
}

std::string neo_external_bridge::report() const
{
    std::ostringstream out;
    out << name() << '\n'
        << "  core aperture   : " << hex(spec_.core_base) << " + "
        << hex(spec_.core_size) << ", SRAM window " << hex(spec_.sram_base)
        << " + " << hex(spec_.sram_window) << '\n'
        << "  inbound         : " << inbound_sram_requests_ << " SRAM ("
        << inbound_sram_bytes_ << " bytes, arbitrated as 'external_inbound'), "
        << inbound_mmio_requests_ << " MMIO, " << inbound_rejected_
        << " refused\n"
        << "  outbound        : " << outbound_requests_ << " forwarded, "
        << outbound_local_refused_
        << " refused for naming an address inside this core\n"
        << "  arbitration     : "
        << (blocks_on_arbitration()
                ? "the local fabric is 'arbitrated', so an inbound access may "
                  "block while another\n                    requester holds a "
                  "bank. Do not attach this to the detailed NoC."
                : "the local fabric is 'annotated', so no inbound access "
                  "blocks the caller.")
        << '\n';
    return out.str();
}

} // namespace cdc::components::tpu_v3::core
