// SPDX-License-Identifier: Apache-2.0

#include "tpu_v3/chip/chip_local_fabric.h"

#include <algorithm>
#include <memory>
#include <sstream>
#include <stdexcept>

#include "tpu_v3/address_map.h"
#include "tpu_v3/core/neo_payload_rules.h"

namespace cdc::components::tpu_v3::chip {

namespace {

constexpr unsigned index_of(chip_initiator initiator) noexcept
{
    return static_cast<unsigned>(initiator);
}

constexpr unsigned index_of(chip_destination destination) noexcept
{
    return static_cast<unsigned>(destination);
}

/// A model or integration defect, not a runtime condition. Same posture as the
/// local SRAM fabric: unattributable or duplicated traffic corrupts every
/// counter downstream of it, so it stops the simulation instead of being
/// counted as somebody.
[[noreturn]] void model_defect(const std::string& who, const std::string& what)
{
    throw std::runtime_error("tpu_v3::chip_local_fabric[" + who + "]: " + what);
}

} // namespace

const char* to_string(chip_initiator initiator) noexcept
{
    switch (initiator) {
    case chip_initiator::core0:
        return "core0";
    case chip_initiator::core1:
        return "core1";
    case chip_initiator::external_inbound:
        return "external_inbound";
    }
    return "unknown";
}

const char* to_string(chip_destination destination) noexcept
{
    switch (destination) {
    case chip_destination::core0:
        return "core0";
    case chip_destination::core1:
        return "core1";
    case chip_destination::chip_control:
        return "chip_control";
    case chip_destination::chip_counters:
        return "chip_counters";
    case chip_destination::outside:
        return "outside";
    case chip_destination::none:
        return "refused";
    }
    return "unknown";
}

const char* to_string(chip_fabric_timing timing) noexcept
{
    switch (timing) {
    case chip_fabric_timing::annotated:
        return "annotated";
    case chip_fabric_timing::arbitrated:
        return "arbitrated";
    }
    return "unknown";
}

void chip_aperture_spec::validate(const std::string& context) const
{
    struct window {
        const char* name;
        std::uint64_t base;
        std::uint64_t size;
    };
    const window windows[] = {
        {"core[0]", core_base[0], core_size},
        {"core[1]", core_base[1], core_size},
        {"control", control_base, control_size},
        {"counters", counters_base, counters_size},
    };

    if (chip_size == 0) {
        throw std::invalid_argument(context + ": chip_size must be non-zero");
    }
    for (const auto& w : windows) {
        if (w.size == 0) {
            throw std::invalid_argument(context + ": " + w.name
                                        + " has zero size");
        }
        if (!address_map::contains(chip_base, chip_size, w.base, w.size)) {
            std::ostringstream out;
            out << context << ": " << w.name << " [0x" << std::hex << w.base
                << ", +0x" << w.size << ") leaves the chip aperture [0x"
                << chip_base << ", +0x" << chip_size << ')';
            throw std::invalid_argument(out.str());
        }
    }
    for (std::size_t i = 0; i < std::size(windows); ++i) {
        for (std::size_t j = i + 1; j < std::size(windows); ++j) {
            if (core::ranges_overlap(windows[i].base, windows[i].size,
                                     windows[j].base, windows[j].size)) {
                std::ostringstream out;
                out << context << ": " << windows[i].name << " and "
                    << windows[j].name << " overlap. There is no correct answer"
                    << " for which should answer an address in both";
                throw std::invalid_argument(out.str());
            }
        }
    }
}

chip_aperture_spec chip_aperture_of(chip_id_t chip)
{
    if (!address_map::valid_chip(chip)) {
        throw std::invalid_argument(
            "tpu_v3::chip_aperture_of: chip " + std::to_string(chip)
            + " is outside 0.." + std::to_string(max_chips - 1)
            + ". The limit is the 3-bit FlooNoC manager id (decision record "
              "D2)");
    }

    chip_aperture_spec spec;
    spec.chip_base = address_map::chip_base(chip);
    spec.chip_size = address_map::chip_aperture_stride;
    for (unsigned core = 0; core < cores_per_chip; ++core) {
        spec.core_base[core] = address_map::core_base(chip, core);
    }
    spec.core_size = address_map::core_aperture_stride;
    spec.control_base = address_map::chip_control(chip);
    spec.control_size = address_map::chip_control_size;
    spec.counters_base = address_map::chip_counters(chip);
    spec.counters_size = address_map::chip_counters_size;
    return spec;
}

chip_local_fabric::chip_local_fabric(sc_core::sc_module_name name,
                                     chip_aperture_spec spec,
                                     chip_fabric_timing timing,
                                     sc_core::sc_time cycle)
    : sc_core::sc_module(name)
    , from("from", chip_initiator_count)
    , to_core("to_core", cores_per_chip)
    , to_control("to_control")
    , to_counters("to_counters")
    , external("external")
    , spec_((spec.validate(std::string("tpu_v3::chip_local_fabric[")
                           + std::string(name) + "]"),
             spec))
    , timing_(timing)
    , cycle_(cycle)
{
    if (cycle_ <= sc_core::SC_ZERO_TIME) {
        throw std::invalid_argument(
            std::string("tpu_v3::chip_local_fabric[") + std::string(name)
            + "]: the fabric clock period must be positive; a zero period "
              "makes every port free at every instant and removes the "
              "arbitration this component exists to model");
    }

    for (unsigned i = 0; i < chip_initiator_count; ++i) {
        from[i].register_b_transport(this, &chip_local_fabric::b_transport,
                                     static_cast<int>(i));
        from[i].register_transport_dbg(this, &chip_local_fabric::transport_dbg,
                                       static_cast<int>(i));
    }
    for (unsigned i = 0; i < chip_port_count; ++i) {
        ports_.push_back(std::make_unique<port_state>());
    }
}

bool chip_local_fabric::in_region(std::uint64_t base, std::uint64_t size,
                                  std::uint64_t address,
                                  std::uint64_t length) const noexcept
{
    return address_map::contains(base, size, address, length);
}

chip_destination chip_local_fabric::decode(std::uint64_t address,
                                           std::uint64_t length) const noexcept
{
    if (length == 0) {
        return chip_destination::none;
    }

    struct window {
        chip_destination destination;
        std::uint64_t base;
        std::uint64_t size;
    };
    const window windows[] = {
        {chip_destination::core0, spec_.core_base[0], spec_.core_size},
        {chip_destination::core1, spec_.core_base[1], spec_.core_size},
        {chip_destination::chip_control, spec_.control_base,
         spec_.control_size},
        {chip_destination::chip_counters, spec_.counters_base,
         spec_.counters_size},
    };

    for (const auto& w : windows) {
        if (in_region(w.base, w.size, address, length)) {
            return w.destination;
        }
        // Touches the window without lying inside it. Such a transfer decodes
        // to two destinations at once and there is no correct answer for which
        // should answer, so it is refused rather than routed by whichever
        // comparison happened to run first.
        if (core::ranges_overlap(address, length, w.base, w.size)) {
            return chip_destination::none;
        }
    }

    // Inside the chip aperture but in none of its windows: a hole. `outside`
    // would send it to the mesh, where the chip's own aperture is this node's
    // and NoLoopback would deadlock it.
    if (core::ranges_overlap(address, length, spec_.chip_base,
                             spec_.chip_size)) {
        return chip_destination::none;
    }

    return chip_destination::outside;
}

chip_destination chip_local_fabric::route(chip_initiator initiator,
                                          tlm::tlm_generic_payload& trans)
{
    const chip_destination destination
        = decode(trans.get_address(), trans.get_data_length());

    if (destination == chip_destination::none) {
        ++decode_refused_;
        trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
        return chip_destination::none;
    }

    if (initiator == chip_initiator::external_inbound) {
        if (destination == chip_destination::outside) {
            // The endpoint handed this chip an address it does not own.
            // Forwarding it back out would turn one mis-route into a loop.
            ++inbound_foreign_refused_;
            trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
            return chip_destination::none;
        }
        return destination;
    }

    // An outbound access from a core naming that same core's aperture. The
    // core's own external bridge already refuses this, so reaching here means
    // its decoder is wrong; the containment rule is enforced twice on purpose.
    const auto own = initiator == chip_initiator::core0
        ? chip_destination::core0
        : chip_destination::core1;
    if (destination == own) {
        ++self_refused_;
        trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
        return chip_destination::none;
    }

    return destination;
}

tlm_utils::simple_initiator_socket<chip_local_fabric>&
chip_local_fabric::port_of(chip_destination destination)
{
    switch (destination) {
    case chip_destination::core0:
        return to_core[0];
    case chip_destination::core1:
        return to_core[1];
    case chip_destination::chip_control:
        return to_control;
    case chip_destination::chip_counters:
        return to_counters;
    case chip_destination::outside:
        return external;
    case chip_destination::none:
        break;
    }
    model_defect(std::string(name()), "port_of() was asked for a refused "
                                      "destination");
}

unsigned chip_local_fabric::select_waiter(const port_state& port) const noexcept
{
    // Rotating priority, starting one past the last initiator this port
    // granted. Deterministic is the requirement: an arbiter whose outcome
    // depends on host scheduling makes every contention measurement
    // unreproducible.
    for (unsigned step = 1; step <= chip_initiator_count; ++step) {
        const unsigned candidate
            = (port.last_granted + step) % chip_initiator_count;
        if (port.waiting[candidate]) {
            return candidate;
        }
    }
    return chip_initiator_count;
}

void chip_local_fabric::charge_annotated(chip_destination destination,
                                         sc_core::sc_time& delay)
{
    port_state& port = *ports_[index_of(destination)];

    // Where this initiator actually is in time. Under temporal decoupling that
    // is later than `sc_time_stamp()` by whatever delay it is still carrying.
    const sc_core::sc_time arrival = sc_core::sc_time_stamp() + delay;
    sc_core::sc_time start = arrival;
    if (port.busy_until > start) {
        ++port.conflicts;
        start = port.busy_until;
    }
    port.busy_until = start + cycle_;
    delay += (start - arrival) + cycle_;
}

bool chip_local_fabric::acquire_port(chip_initiator initiator,
                                     chip_destination destination,
                                     sc_core::sc_time& delay,
                                     std::uint64_t request_generation)
{
    const auto abandoned = [&] { return generation_ != request_generation; };

    const unsigned me = index_of(initiator);
    port_state& port = *ports_[index_of(destination)];

    // An arbitrated initiator has to be where it says it is before it can
    // contend with anyone: arbitration is about who holds a port *now*, and an
    // initiator still carrying an unconsumed quantum is not here yet.
    //
    // **Interruptible**, for the reason `neo_external_bridge` records at the
    // same place: a request catching up on its quantum has not registered as a
    // waiter, so it is invisible to the arbiter, and a bare `wait(delay)`
    // cannot be woken by `reset()` either — it would sleep out the whole
    // remaining quantum before noticing it had been abandoned. Waiting on the
    // timeout *or* the port's event preserves the arrival instant exactly and
    // lets a reset land.
    if (delay != sc_core::SC_ZERO_TIME) {
        const sc_core::sc_time arrival = sc_core::sc_time_stamp() + delay;
        while (sc_core::sc_time_stamp() < arrival) {
            sc_core::wait(arrival - sc_core::sc_time_stamp(), port.changed);
            if (abandoned()) {
                // Hand back the unelapsed remainder, for the reason
                // `neo_external_bridge` records at the same place: a caller's
                // logical time is `sc_time_stamp() + delay`, and a decoupled
                // initiator that sets its keeper from the returned value would
                // move backwards if this returned zero.
                delay = arrival - sc_core::sc_time_stamp();
                return false;
            }
        }
        delay = sc_core::SC_ZERO_TIME;
    }

    port.waiting[me] = true;
    // A new waiter can change who the rotating priority selects, so whoever is
    // already blocked has to re-evaluate.
    port.changed.notify(sc_core::SC_ZERO_TIME);

    if (port.busy || select_waiter(port) != me) {
        // Counted once per request, not once per wake-up: this is "the request
        // found its port occupied", the back-pressure event, not the number of
        // times the process happened to be rescheduled.
        ++port.conflicts;
        while (port.busy || select_waiter(port) != me) {
            sc_core::wait(port.changed);
            if (abandoned()) {
                // `reset()` already cleared `waiting[]` and released the ports,
                // so there is nothing to unwind here — touching port state now
                // would corrupt whoever was granted next.
                return false;
            }
        }
    }

    port.waiting[me] = false;
    port.busy = true;
    port.last_granted = me;
    ++port.grants[me];

    sc_core::wait(cycle_);

    if (abandoned()) {
        // Granted, and now not going to be used. **This request must release
        // the port**, because it owns it and `reset()` deliberately no longer
        // clears `busy` — see `reset()` for why. Leaving it held would wedge
        // the port for the rest of the simulation.
        release_port(destination);
        return false;
    }

    // Still held. The caller forwards downstream and then calls
    // `release_port()`; the port covers the whole transaction, which is the
    // only version of "one at a time" a target can observe.
    return true;
}

void chip_local_fabric::release_port(chip_destination destination)
{
    port_state& port = *ports_[index_of(destination)];
    port.busy = false;
    port.changed.notify(sc_core::SC_ZERO_TIME);
}

void chip_local_fabric::b_transport(int id, tlm::tlm_generic_payload& trans,
                                    sc_core::sc_time& delay)
{
    const auto initiator = static_cast<chip_initiator>(id);
    const unsigned me = static_cast<unsigned>(id);

    // Revision 1 allows one transaction in flight per initiator. Enforced
    // rather than assumed: two processes sharing an initiator identity
    // interleave under one name, and from then on the arbiter sees one
    // contender where there are two and every counter stays plausible.
    ++in_flight_[me];
    peak_in_flight_[me] = std::max(peak_in_flight_[me], in_flight_[me]);
    if (in_flight_[me] > 1) {
        --in_flight_[me];
        model_defect(std::string(name()),
                     std::string("initiator '") + to_string(initiator)
                         + "' issued a second transaction while one was still "
                           "in flight");
    }

    const std::uint64_t request_generation = generation_;
    ++requests_[me];

    if (!core::check_common_payload_rules(trans)) {
        ++protocol_errors_;
        --in_flight_[me];
        return;
    }
    if (trans.get_data_length() != 0 && trans.get_data_ptr() == nullptr) {
        ++protocol_errors_;
        trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
        --in_flight_[me];
        return;
    }

    // `route()` has already set the response status and counted the refusal
    // under the reason that applies; there is no `routed_` column for a
    // request that went nowhere.
    const chip_destination destination = route(initiator, trans);
    if (destination == chip_destination::none) {
        --in_flight_[me];
        return;
    }

    bool holds_port = false;
    if (timing_ == chip_fabric_timing::arbitrated) {
        if (!acquire_port(initiator, destination, delay, request_generation)) {
            trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
            --in_flight_[me];
            return;
        }
        holds_port = true;
    } else {
        charge_annotated(destination, delay);
    }

    port_of(destination)->b_transport(trans, delay);

    // **Always released by whoever took it, reset or no reset.**
    //
    // Ownership of a port is a fact about a C++ call stack, not model state a
    // reset can revoke: `reset()` cannot cancel a `b_transport()` already
    // blocked inside the target. An earlier version made this conditional on
    // the generation and had `reset()` clear `busy` instead, which handed the
    // port to another core while the first one was still inside the downstream
    // call — two concurrent transactions in one target, which is the invariant
    // this arbiter exists to hold.
    if (holds_port) {
        release_port(destination);
    }

    // A reset while the request was downstream starts a new counter epoch. The
    // response still reaches its owner, but an old-generation request must not
    // write its bytes into the freshly cleared counters.
    if (generation_ != request_generation) {
        --in_flight_[me];
        return;
    }

    ++routed_[me][index_of(destination)];
    if (destination == chip_destination::outside) {
        ++outbound_requests_;
    } else if ((initiator == chip_initiator::core0
                && destination == chip_destination::core1)
               || (initiator == chip_initiator::core1
                   && destination == chip_destination::core0)) {
        ++local_bypass_;
    }

    if (trans.is_response_ok()) {
        bytes_[me] += trans.get_data_length();
    } else {
        ++target_errors_;
    }

    --in_flight_[me];
}

unsigned int chip_local_fabric::transport_dbg(int id,
                                              tlm::tlm_generic_payload& trans)
{
    // No arbitration, no latency, no counters — and no relaxation of the
    // decode or the routing rules: a debug access to a foreign address fails
    // like any other (`INTERFACE_CONTRACT.md` §8).
    const auto initiator = static_cast<chip_initiator>(id);
    if (core::common_payload_error(trans) != core::payload_rule_error::none) {
        return 0;
    }
    // `common_payload_error()` checks the command, the streaming width and the
    // byte-enable shape; it says nothing about the data pointer. A non-empty
    // transaction with a null pointer is a malformed payload and forwarding it
    // hands a downstream debug target a null to dereference. `b_transport`
    // already refuses it, and `INTERFACE_CONTRACT.md` §8 gives the debug path
    // no relaxation of the rules — only of timing.
    if (trans.get_data_length() != 0 && trans.get_data_ptr() == nullptr) {
        return 0;
    }

    const chip_destination destination
        = decode(trans.get_address(), trans.get_data_length());
    if (destination == chip_destination::none) {
        return 0;
    }
    if (initiator == chip_initiator::external_inbound
        && destination == chip_destination::outside) {
        return 0;
    }
    const auto own = initiator == chip_initiator::core0
        ? chip_destination::core0
        : chip_destination::core1;
    if (initiator != chip_initiator::external_inbound && destination == own) {
        return 0;
    }

    return port_of(destination)->transport_dbg(trans);
}

std::uint64_t chip_local_fabric::requests(chip_initiator initiator) const
{
    return requests_[index_of(initiator)];
}

std::uint64_t chip_local_fabric::bytes(chip_initiator initiator) const
{
    return bytes_[index_of(initiator)];
}

std::uint64_t chip_local_fabric::routed(chip_initiator initiator,
                                        chip_destination destination) const
{
    if (destination == chip_destination::none) {
        return 0;
    }
    return routed_[index_of(initiator)][index_of(destination)];
}

std::uint64_t
chip_local_fabric::port_conflicts(chip_destination destination) const
{
    if (destination == chip_destination::none) {
        return 0;
    }
    return ports_[index_of(destination)]->conflicts;
}

std::uint64_t chip_local_fabric::port_grants(chip_destination destination,
                                             chip_initiator initiator) const
{
    if (destination == chip_destination::none) {
        return 0;
    }
    return ports_[index_of(destination)]->grants[index_of(initiator)];
}

unsigned chip_local_fabric::peak_in_flight(chip_initiator initiator) const
{
    return peak_in_flight_[index_of(initiator)];
}

void chip_local_fabric::reset()
{
    ++generation_;

    for (auto& port : ports_) {
        port->busy_until = sc_core::SC_ZERO_TIME;
        for (unsigned i = 0; i < chip_initiator_count; ++i) {
            port->waiting[i] = false;
            port->grants[i] = 0;
        }
        port->last_granted = chip_initiator_count - 1;
        port->conflicts = 0;

        // **`busy` is deliberately not cleared.**
        //
        // If a request is inside this port's `b_transport()` right now, it
        // still owns the port and this call cannot take it back: reset does not
        // unwind a blocked C++ call. Clearing the flag would let another core
        // into the same target alongside it. The owner releases the port when
        // its downstream call returns — or, if it was abandoned between the
        // grant and the forward, from `acquire_port()` — and a request arriving
        // after this reset waits for that, correctly, because the port really
        // is busy.

        // Wake the queued waiters before their flags are gone. Each re-checks
        // the generation, abandons its request and returns; clearing
        // `waiting[]` without notifying would leave one unselectable and
        // unwoken, waiting for a grant no arbiter can issue.
        port->changed.notify(sc_core::SC_ZERO_TIME);
    }

    for (unsigned i = 0; i < chip_initiator_count; ++i) {
        requests_[i] = 0;
        bytes_[i] = 0;
        peak_in_flight_[i] = 0;
        for (unsigned p = 0; p < chip_port_count; ++p) {
            routed_[i][p] = 0;
        }
    }

    local_bypass_ = 0;
    outbound_requests_ = 0;
    self_refused_ = 0;
    inbound_foreign_refused_ = 0;
    decode_refused_ = 0;
    protocol_errors_ = 0;
    target_errors_ = 0;
}

std::string chip_local_fabric::report() const
{
    std::ostringstream out;
    out << "chip_local_fabric[" << name() << "]\n"
        << "  aperture 0x" << std::hex << spec_.chip_base << " +0x"
        << spec_.chip_size << std::dec << ", timing " << to_string(timing_)
        << ", cycle " << cycle_ << '\n';
    for (unsigned i = 0; i < chip_initiator_count; ++i) {
        const auto initiator = static_cast<chip_initiator>(i);
        out << "  " << to_string(initiator) << ": " << requests_[i]
            << " requests, " << bytes_[i] << " bytes";
        if (peak_in_flight_[i] > 0) {
            out << ", peak in flight " << peak_in_flight_[i];
        }
        out << '\n';
    }
    out << "  local bypass " << local_bypass_ << ", outbound "
        << outbound_requests_ << '\n'
        << "  refused: " << self_refused_ << " self, "
        << inbound_foreign_refused_ << " inbound-foreign, " << decode_refused_
        << " decode, " << protocol_errors_ << " protocol, " << target_errors_
        << " target\n";
    return out.str();
}

} // namespace cdc::components::tpu_v3::chip
