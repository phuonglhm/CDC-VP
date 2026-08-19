// SPDX-License-Identifier: Apache-2.0

#include "tpu_v3/core/neo_hart_port.h"

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

} // namespace

const char* to_string(hart_destination destination) noexcept
{
    switch (destination) {
    case hart_destination::local_sram:
        return "local_sram";
    case hart_destination::control:
        return "control";
    case hart_destination::external:
        return "external";
    }
    return "unknown";
}

neo_hart_port::neo_hart_port(sc_core::sc_module_name name, hart_port_spec spec,
                             neo_local_sram_fabric& fabric)
    : sc_core::sc_module(name)
    , from_hart("from_hart")
    , to_control("to_control")
    , to_external("to_external")
    , spec_(std::move(spec))
    , fabric_(fabric)
{
    if (spec_.core_size == 0 || spec_.sram_window == 0) {
        throw std::invalid_argument("tpu_v3::neo_hart_port["
                                    + std::string(name)
                                    + "]: the core aperture and SRAM window "
                                      "must both be non-empty");
    }
    if (!address_map::contains(spec_.core_base, spec_.core_size,
                               spec_.sram_base, spec_.sram_window)) {
        throw std::invalid_argument(
            "tpu_v3::neo_hart_port[" + std::string(name) + "]: the SRAM window "
            + hex(spec_.sram_base) + '+' + hex(spec_.sram_window)
            + " is not inside the core aperture " + hex(spec_.core_base) + '+'
            + hex(spec_.core_size));
    }
    if (!fabric_.is_attached(sram::neo_requester::cpu)) {
        // Same reasoning as the bridge's `external_inbound` check: a requester
        // the fabric does not know is refused on its first transaction, so a
        // platform wired this way would elaborate, boot, and die on the hart's
        // first local load. Elaboration is the cheaper place to find out.
        throw std::invalid_argument(
            "tpu_v3::neo_hart_port[" + std::string(name)
            + "]: the local SRAM fabric does not have 'cpu' among its attached "
              "requesters, so the hart's local traffic would have no owner");
    }

    from_hart.register_b_transport(this, &neo_hart_port::b_transport);
    from_hart.register_transport_dbg(this, &neo_hart_port::transport_dbg);
}

bool neo_hart_port::in_sram(std::uint64_t address,
                            std::uint64_t length) const noexcept
{
    return address_map::contains(spec_.sram_base, spec_.sram_window, address,
                                 length);
}

bool neo_hart_port::in_core(std::uint64_t address,
                            std::uint64_t length) const noexcept
{
    return address_map::contains(spec_.core_base, spec_.core_size, address,
                                 length);
}

bool neo_hart_port::straddles_a_boundary(std::uint64_t address,
                                         std::uint64_t length) const noexcept
{
    // Two boundaries matter, and the test is overlap-without-containment at
    // each. Touching a region but not lying inside it means the access also
    // reaches something else, and there is no correct answer for which
    // destination should have answered it.
    const bool touches_sram
        = ranges_overlap(address, length, spec_.sram_base, spec_.sram_window);
    if (touches_sram && !in_sram(address, length)) {
        return true;
    }
    const bool touches_core
        = ranges_overlap(address, length, spec_.core_base, spec_.core_size);
    if (touches_core && !in_core(address, length)) {
        return true;
    }
    return false;
}

void neo_hart_port::b_transport(tlm::tlm_generic_payload& trans,
                                sc_core::sc_time& delay)
{
    trans.set_dmi_allowed(false);

    const std::uint64_t address = trans.get_address();
    const unsigned int length = trans.get_data_length();
    const auto command = trans.get_command();

    if (!check_common_payload_rules(trans)) {
        ++protocol_errors_;
        return;
    }
    if (length == 0 || trans.get_data_ptr() == nullptr) {
        // Not a guest fault. A hart cannot ask for zero bytes through the VP++
        // memory interface, so this is the wrapper or a test constructing a
        // malformed payload, and decision record D13's rule applies: a
        // protocol error is a model defect and must not be handed back as
        // something firmware could have caused.
        throw std::runtime_error(
            "tpu_v3::neo_hart_port[" + std::string(name())
            + "]: a hart transaction had zero length or a null data pointer -- "
              "this is a model or integration defect, not a guest fault");
    }

    if (straddles_a_boundary(address, length)) {
        trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
        ++straddle_errors_;
        return;
    }

    if (in_sram(address, length)) {
        if (length > sram::neo_max_transfer_bytes) {
            // One RVV register at VLEN=512 is the largest a single access can
            // need (D7). Anything wider is refused rather than split: this
            // port must not invent a transfer boundary, and splitting here
            // would fabricate arbitration events the hart never caused.
            trans.set_response_status(tlm::TLM_BURST_ERROR_RESPONSE);
            ++size_errors_;
            return;
        }

        std::vector<unsigned char> enables;
        if (!expand_byte_enables(trans, enables)) {
            trans.set_response_status(tlm::TLM_BURST_ERROR_RESPONSE);
            ++protocol_errors_;
            return;
        }

        sram::neo_local_request request;
        request.requester = sram::neo_requester::cpu;
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
        ++requests_[index_of(hart_destination::local_sram)];
        bytes_[index_of(hart_destination::local_sram)] += response.bytes;
        if (response.status != sram::neo_status::ok) {
            ++destination_errors_;
        }
        return;
    }

    if (in_core(address, length)) {
        // Core-local MMIO. The control fabric owns the AXI4-Lite width,
        // alignment and strobe rules; this port does not pre-judge them, so a
        // misaligned register access gets the same refusal wherever it came
        // from.
        ++requests_[index_of(hart_destination::control)];
        to_control->b_transport(trans, delay);
        if (trans.get_response_status() == tlm::TLM_OK_RESPONSE) {
            bytes_[index_of(hart_destination::control)] += length;
        } else {
            ++destination_errors_;
        }
        return;
    }

    // Everything else leaves the core. Instruction fetch from global boot ROM
    // arrives here on the very first cycle, which is why this path is
    // architecturally required and not an optimisation (D15).
    ++requests_[index_of(hart_destination::external)];
    to_external->b_transport(trans, delay);
    if (trans.get_response_status() == tlm::TLM_OK_RESPONSE) {
        bytes_[index_of(hart_destination::external)] += length;
    } else {
        ++destination_errors_;
    }
}

unsigned int neo_hart_port::transport_dbg(tlm::tlm_generic_payload& trans)
{
    // Debug transport bypasses timing and workload counters, not payload
    // validity and not decode (`INTERFACE_CONTRACT.md` §8). A loader is not
    // workload traffic, so nothing here touches the counters.
    if (common_payload_error(trans) != payload_rule_error::none) {
        return 0;
    }

    const std::uint64_t address = trans.get_address();
    const unsigned int length = trans.get_data_length();
    if (length == 0 || trans.get_data_ptr() == nullptr) {
        return 0;
    }
    if (straddles_a_boundary(address, length)) {
        return 0;
    }

    if (in_sram(address, length)) {
        if (length > sram::neo_max_transfer_bytes) {
            return 0;
        }
        std::vector<unsigned char> enables;
        if (!expand_byte_enables(trans, enables)) {
            return 0;
        }

        sram::neo_local_request request;
        request.requester = sram::neo_requester::cpu;
        request.command = trans.get_command() == tlm::TLM_WRITE_COMMAND
            ? sram::neo_command::write
            : sram::neo_command::read;
        request.address = address;
        request.size = length;
        request.data = trans.get_data_ptr();
        request.strobes = enables.empty() ? nullptr : enables.data();
        return fabric_.dbg_access(request);
    }

    if (in_core(address, length)) {
        return to_control->transport_dbg(trans);
    }
    return to_external->transport_dbg(trans);
}

std::uint64_t neo_hart_port::requests(hart_destination destination) const
{
    return requests_[index_of(destination)];
}

std::uint64_t neo_hart_port::bytes(hart_destination destination) const
{
    return bytes_[index_of(destination)];
}

void neo_hart_port::reset()
{
    for (unsigned i = 0; i < hart_destination_count; ++i) {
        requests_[i] = 0;
        bytes_[i] = 0;
    }
    protocol_errors_ = 0;
    straddle_errors_ = 0;
    size_errors_ = 0;
    destination_errors_ = 0;
}

std::string neo_hart_port::report() const
{
    std::ostringstream out;
    out << "neo_hart_port[" << name() << "]\n"
        << "  core aperture      " << hex(spec_.core_base) << " + "
        << hex(spec_.core_size) << '\n'
        << "  core SRAM window   " << hex(spec_.sram_base) << " + "
        << hex(spec_.sram_window) << '\n'
        << "  fabric timing      " << to_string(fabric_.timing());
    if (blocks_on_arbitration()) {
        // Worth saying out loud rather than leaving to be deduced from the
        // mode name: this is the configuration in which the hart gives up
        // temporal decoupling on every local access (D16).
        out << "  (blocks its requesters; no temporal decoupling)";
    }
    out << '\n';
    for (unsigned i = 0; i < hart_destination_count; ++i) {
        const auto destination = static_cast<hart_destination>(i);
        out << "  " << std::setw(11) << std::left << to_string(destination)
            << std::right << "  requests " << requests_[i] << ", bytes "
            << bytes_[i] << '\n';
    }
    out << "  refused: protocol " << protocol_errors_ << ", straddle "
        << straddle_errors_ << ", size " << size_errors_
        << "; destination errors " << destination_errors_ << '\n'
        << "  counts are TLM requests from the hart, at VP++ element-wise "
           "granularity (D7)\n";
    return out.str();
}

} // namespace cdc::components::tpu_v3::core
