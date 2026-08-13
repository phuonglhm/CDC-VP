// SPDX-License-Identifier: Apache-2.0

#include "tpu_v3/core/neo_control_fabric.h"

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace cdc::components::tpu_v3::core {

namespace {

constexpr unsigned kMmioWidth = 4;

bool all_bytes_enabled(const tlm::tlm_generic_payload& trans)
{
    const unsigned char* enables = trans.get_byte_enable_ptr();
    if (enables == nullptr) {
        return true;
    }
    const unsigned int length = trans.get_byte_enable_length();
    if (length == 0) {
        return false;
    }
    for (unsigned int i = 0; i < length; ++i) {
        if (enables[i] != TLM_BYTE_ENABLED) {
            return false;
        }
    }
    return true;
}

std::string hex(std::uint64_t value)
{
    std::ostringstream out;
    out << "0x" << std::hex << std::setw(8) << std::setfill('0') << value;
    return out.str();
}

} // namespace

const char* to_string(control_initiator initiator) noexcept
{
    switch (initiator) {
    case control_initiator::cpu:
        return "cpu";
    case control_initiator::external_inbound:
        return "external_inbound";
    }
    return "unknown";
}

neo_control_fabric::neo_control_fabric(sc_core::sc_module_name name,
                                       std::vector<control_target_spec> targets)
    : sc_core::sc_module(name)
    , from("from", control_initiator_count)
    , to("to", targets.size())
    , targets_(std::move(targets))
    , forwarded_(targets_.size(), 0)
{
    if (targets_.empty()) {
        throw std::invalid_argument("tpu_v3::neo_control_fabric["
                                    + std::string(name)
                                    + "]: no control targets were given");
    }

    for (std::size_t i = 0; i < targets_.size(); ++i) {
        const auto& a = targets_[i];
        if (a.size == 0) {
            throw std::invalid_argument(
                "tpu_v3::neo_control_fabric[" + std::string(name)
                + "]: target '" + a.name + "' has zero size");
        }
        if (a.base % kMmioWidth != 0) {
            throw std::invalid_argument(
                "tpu_v3::neo_control_fabric[" + std::string(name)
                + "]: target '" + a.name + "' at " + hex(a.base)
                + " is not 4-byte aligned");
        }
        for (std::size_t j = i + 1; j < targets_.size(); ++j) {
            const auto& b = targets_[j];
            if (a.base < b.base + b.size && b.base < a.base + a.size) {
                // Not a runtime condition to resolve: there is no correct
                // answer for which of two overlapping targets should have
                // answered, so the map is refused during elaboration.
                throw std::invalid_argument(
                    "tpu_v3::neo_control_fabric[" + std::string(name)
                    + "]: control targets '" + a.name + "' at " + hex(a.base)
                    + " and '" + b.name + "' at " + hex(b.base) + " overlap");
            }
        }
    }

    for (unsigned i = 0; i < control_initiator_count; ++i) {
        from[i].register_b_transport(this, &neo_control_fabric::b_transport,
                                     static_cast<int>(i));
        from[i].register_transport_dbg(this,
                                       &neo_control_fabric::transport_dbg,
                                       static_cast<int>(i));
    }
}

int neo_control_fabric::decode(std::uint64_t address,
                               std::uint64_t length) const noexcept
{
    for (std::size_t i = 0; i < targets_.size(); ++i) {
        const auto& target = targets_[i];
        if (address < target.base) {
            continue;
        }
        const std::uint64_t offset = address - target.base;
        if (offset >= target.size) {
            continue;
        }
        // Folded so a length near 2^64 cannot wrap the check into a false
        // "inside" (plan §12 rule 5).
        if (length <= target.size - offset) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

bool neo_control_fabric::check_axi4lite_rules(tlm::tlm_generic_payload& trans)
{
    const auto command = trans.get_command();
    if (command != tlm::TLM_READ_COMMAND && command != tlm::TLM_WRITE_COMMAND) {
        trans.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
        ++protocol_errors_;
        return false;
    }

    // Four bytes, naturally aligned, full strobes. Refusing anything else is
    // what keeps accelerator bulk data off the control plane (D15): a 64-byte
    // vector payload is rejected here rather than quietly split into sixteen
    // register accesses that no RTL AXI4-Lite decoder would perform.
    if (trans.get_data_length() != kMmioWidth) {
        trans.set_response_status(tlm::TLM_BURST_ERROR_RESPONSE);
        ++protocol_errors_;
        return false;
    }
    if (trans.get_address() % kMmioWidth != 0) {
        trans.set_response_status(tlm::TLM_BURST_ERROR_RESPONSE);
        ++protocol_errors_;
        return false;
    }
    const unsigned int streaming = trans.get_streaming_width();
    if (streaming != 0 && streaming < trans.get_data_length()) {
        trans.set_response_status(tlm::TLM_BURST_ERROR_RESPONSE);
        ++protocol_errors_;
        return false;
    }
    if (!all_bytes_enabled(trans)) {
        trans.set_response_status(tlm::TLM_BURST_ERROR_RESPONSE);
        ++protocol_errors_;
        return false;
    }
    if (trans.get_data_ptr() == nullptr) {
        throw std::runtime_error(
            "tpu_v3::neo_control_fabric[" + std::string(name())
            + "]: a transaction carried a null data pointer -- this is a model "
              "or integration defect, not a guest fault");
    }
    return true;
}

void neo_control_fabric::b_transport(int id, tlm::tlm_generic_payload& trans,
                                     sc_core::sc_time& delay)
{
    const auto index = static_cast<unsigned>(id);
    ++requests_[index];
    trans.set_dmi_allowed(false);

    // AXI4-Lite here accepts at most one transaction per control initiator
    // (D15). A second one arriving while the first is outstanding means the
    // initiator re-entered the fabric — a register write whose target called
    // back through the same port, say — and that is a structural defect, not
    // a condition the initiator could handle if it were told.
    ++in_flight_[index];
    peak_in_flight_[index] = std::max(peak_in_flight_[index], in_flight_[index]);
    if (in_flight_[index] > 1) {
        --in_flight_[index];
        throw std::runtime_error(
            "tpu_v3::neo_control_fabric[" + std::string(name())
            + "]: initiator '" + to_string(static_cast<control_initiator>(index))
            + "' issued a second transaction while one was still outstanding. "
              "The AXI4-Lite control plane accepts one at a time (decision "
              "record D15) -- this is a model or integration defect");
    }

    // Clears the in-flight marker on every path out, including the one where
    // a target throws. Without it a single exception would make the port look
    // permanently busy and every later access a "defect".
    struct in_flight_guard {
        unsigned& counter;
        ~in_flight_guard() { --counter; }
    } guard{in_flight_[index]};

    if (!check_axi4lite_rules(trans)) {
        return;
    }

    const int target = decode(trans.get_address(), trans.get_data_length());
    if (target < 0) {
        trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
        ++decode_errors_;
        return;
    }

    ++forwarded_[static_cast<std::size_t>(target)];
    to[static_cast<std::size_t>(target)]->b_transport(trans, delay);

    // The target's status is propagated unchanged; the fabric only counts it.
    // Converting a refusal into an OK, or into zero data, is the failure mode
    // `INTERFACE_CONTRACT.md` §1 exists to prevent.
    if (trans.get_response_status() != tlm::TLM_OK_RESPONSE) {
        ++target_errors_;
    }
}

unsigned int neo_control_fabric::transport_dbg(int id,
                                               tlm::tlm_generic_payload& trans)
{
    (void)id;
    // Decode and bounds still apply; the width rules do not, because a debug
    // access is host-side loading rather than a modelled bus transaction
    // (`INTERFACE_CONTRACT.md` §8). No counter moves: a loader is not
    // workload traffic.
    const int target = decode(trans.get_address(), trans.get_data_length());
    if (target < 0) {
        return 0;
    }
    return to[static_cast<std::size_t>(target)]->transport_dbg(trans);
}

std::uint64_t
neo_control_fabric::requests(control_initiator initiator) const
{
    return requests_[static_cast<unsigned>(initiator)];
}

std::uint64_t neo_control_fabric::forwarded(std::size_t target_index) const
{
    if (target_index >= forwarded_.size()) {
        throw std::out_of_range("tpu_v3::neo_control_fabric: target index "
                                + std::to_string(target_index)
                                + " does not exist");
    }
    return forwarded_[target_index];
}

unsigned
neo_control_fabric::peak_in_flight(control_initiator initiator) const
{
    return peak_in_flight_[static_cast<unsigned>(initiator)];
}

void neo_control_fabric::reset()
{
    for (unsigned i = 0; i < control_initiator_count; ++i) {
        requests_[i] = 0;
        in_flight_[i] = 0;
        peak_in_flight_[i] = 0;
    }
    std::fill(forwarded_.begin(), forwarded_.end(), 0);
    protocol_errors_ = 0;
    decode_errors_ = 0;
    target_errors_ = 0;
}

std::string neo_control_fabric::report() const
{
    std::ostringstream out;
    out << name() << "  (32-bit AXI4-Lite, in order, no bursts or IDs, one "
                     "transaction per initiator)\n";
    for (unsigned i = 0; i < control_initiator_count; ++i) {
        out << "  initiator " << to_string(static_cast<control_initiator>(i))
            << " : " << requests_[i] << " requests, peak in flight "
            << peak_in_flight_[i] << '\n';
    }
    for (std::size_t i = 0; i < targets_.size(); ++i) {
        out << "  target " << targets_[i].name << " @ " << hex(targets_[i].base)
            << " : " << forwarded_[i] << " forwarded\n";
    }
    out << "  refused         : " << protocol_errors_ << " protocol, "
        << decode_errors_ << " decode, " << target_errors_
        << " by the target\n"
        << "  fidelity        : transaction level. AW/W/B/AR/R channel timing "
           "is not modelled and\n"
           "                    is not claimed (decision record D15).\n";
    return out.str();
}

} // namespace cdc::components::tpu_v3::core
