// SPDX-License-Identifier: Apache-2.0

#include "tpu_v3/core/neo_local_sram_fabric.h"

#include <algorithm>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace cdc::components::tpu_v3::core {

namespace {

[[noreturn]] void model_defect(const std::string& where,
                               const std::string& what)
{
    // Deliberately not a guest-visible error status. A request from an
    // unattached requester, or one carrying a null buffer, is a defect in the
    // model or in the integration, and turning it into a response status would
    // let it be "handled" by a requester that cannot possibly fix it. Decision
    // record D13 settled the same question on the CPU side.
    throw std::runtime_error("tpu_v3::neo_local_sram_fabric[" + where
                             + "]: " + what);
}

} // namespace

const char* to_string(local_fabric_timing timing) noexcept
{
    switch (timing) {
    case local_fabric_timing::annotated:
        return "annotated";
    case local_fabric_timing::arbitrated:
        return "arbitrated";
    }
    return "unknown";
}

neo_local_sram_fabric::neo_local_sram_fabric(
    sc_core::sc_module_name name, local_sram_fabric_config config,
    sram::core_sram& sram, std::vector<neo_requester> requesters,
    local_fabric_timing timing, sc_core::sc_time cycle)
    : sc_core::sc_module(name)
    , native_port("native_port")
    , config_((config.validate(std::string(name)), std::move(config)))
    , sram_(sram)
    , timing_(timing)
    , cycle_(cycle)
{
    if (cycle_ <= sc_core::SC_ZERO_TIME) {
        model_defect(std::string(name),
                     "the fabric clock period must be greater than zero; a "
                     "zero-length beat makes bank occupancy unobservable and "
                     "arbitration untestable");
    }
    if (requesters.empty()) {
        model_defect(std::string(name),
                     "no requesters were attached. A fabric with no named "
                     "owners cannot attribute anything it carries");
    }
    for (const auto requester : requesters) {
        attached_[sram::index_of(requester)] = true;
    }

    // The logical SRAM is one address space mapped onto physical banks (D15).
    // A capacity that does not cover a whole stripe would leave banks no
    // address selects; the configuration validator refuses that, so by here
    // every bank is reachable.
    banks_.reserve(config_.bank_count);
    for (unsigned i = 0; i < config_.bank_count; ++i) {
        banks_.push_back(std::make_unique<bank_state>());
    }

    native_port.bind(*this);
}

bool neo_local_sram_fabric::is_attached(neo_requester requester) const noexcept
{
    return attached_[sram::index_of(requester)];
}

const requester_counters&
neo_local_sram_fabric::counters(neo_requester requester) const
{
    return counters_[sram::index_of(requester)];
}

std::uint64_t neo_local_sram_fabric::bank_grants(unsigned bank,
                                                 neo_requester requester) const
{
    if (bank >= banks_.size()) {
        throw std::out_of_range("tpu_v3::neo_local_sram_fabric: bank "
                                + std::to_string(bank) + " does not exist");
    }
    return banks_[bank]->grants[sram::index_of(requester)];
}

unsigned neo_local_sram_fabric::bank_of(std::uint64_t address) const noexcept
{
    const std::uint64_t offset = address - sram_.config().base_address;
    return static_cast<unsigned>((offset / config_.bytes_per_beat())
                                 % config_.bank_count);
}

std::uint32_t
neo_local_sram_fabric::beats_for(std::uint64_t address,
                                 std::uint32_t size) const noexcept
{
    if (size == 0) {
        return 0;
    }
    const std::uint64_t base = sram_.config().base_address;
    const std::uint64_t bpb = config_.bytes_per_beat();
    const std::uint64_t first = (address - base) / bpb;
    const std::uint64_t last = (address - base + size - 1) / bpb;
    return static_cast<std::uint32_t>(last - first + 1);
}

unsigned
neo_local_sram_fabric::select_waiter(const bank_state& bank) const noexcept
{
    // Rotating priority, starting one past the last requester this bank
    // granted. Deterministic is the requirement: an arbiter whose outcome
    // depends on host scheduling makes every contention measurement
    // unreproducible (D15).
    for (unsigned step = 1; step <= neo_requester_count; ++step) {
        const unsigned candidate =
            (bank.last_granted + step) % neo_requester_count;
        if (bank.waiting[candidate]) {
            return candidate;
        }
    }
    return neo_requester_count;
}

neo_status neo_local_sram_fabric::move_beat(const neo_local_request& request,
                                            std::uint64_t address,
                                            std::uint32_t offset,
                                            std::uint32_t bytes)
{
    const unsigned char* strobes =
        request.strobes != nullptr ? request.strobes + offset : nullptr;
    if (request.command == neo_command::write) {
        return sram_.write(address, bytes, request.data + offset, strobes);
    }
    return sram_.read(address, bytes, request.data + offset, strobes);
}

void neo_local_sram_fabric::b_access(const neo_local_request& request,
                                     neo_local_response& response,
                                     sc_core::sc_time& delay)
{
    response = neo_local_response{};

    const unsigned me = sram::index_of(request.requester);

    if (!is_attached(request.requester)) {
        model_defect(std::string(name()),
                     std::string("an access arrived from '")
                         + sram::to_string(request.requester)
                         + "', which is not one of this fabric's attached "
                           "requesters. Unattributable traffic corrupts every "
                           "counter that follows");
    }

    // One request in flight per requester (D15). A blocking call gives that
    // for free only while one process drives one requester; two processes
    // sharing an identity overlap the moment the first waits for a bank, and
    // from then on their beats interleave under one name.
    if (in_flight_[me]) {
        model_defect(std::string(name()),
                     std::string("requester '")
                         + sram::to_string(request.requester)
                         + "' issued a second request while one was still in "
                           "flight. Revision 1 allows exactly one per "
                           "requester (decision record D15) -- two processes "
                           "are sharing a requester identity, which is an "
                           "integration defect");
    }
    in_flight_[me] = true;
    // Released on every path out, including the one where the storage throws.
    // Without it a single exception would make the port look permanently busy
    // and every later access a "defect".
    struct in_flight_guard {
        bool& flag;
        ~in_flight_guard() { flag = false; }
    } guard{in_flight_[me]};

    // Captured before any path can wait. In arbitrated mode even consuming the
    // caller's temporal-decoupling delay is a wait point, so capturing this in
    // `charge_arbitrated()` after that wait would let a reset during the delay
    // disappear: the resumed request would adopt the new generation and run as
    // if it had been issued after reset.
    const std::uint64_t request_generation = generation_;

    auto& counters = counters_[me];
    // Counted whatever the outcome, so `request_count >= error_count` holds
    // and the two are a ratio of the same thing.
    ++counters.request_count;

    const neo_status status = sram_.classify(request.address, request.size);
    if (status != neo_status::ok) {
        // Nothing is transferred and the caller's buffer is untouched. The
        // status says which of the three it was, because a driver reacts
        // differently to a wrong pointer, an oversized payload and a capacity
        // the configuration is too small for.
        response.status = status;
        ++counters.error_count;
        return;
    }
    if (request.data == nullptr) {
        model_defect(std::string(name()),
                     "a non-empty request carried a null data pointer");
    }

    if (timing_ == local_fabric_timing::arbitrated) {
        charge_arbitrated(request, response, delay, request_generation);
    } else {
        charge_annotated(request, response, delay);
    }

    // A reset starts a new counter epoch. The response still reports partial
    // beats/bytes to its owner, but an old-generation request must not write
    // those values (or its abort error) back into the freshly cleared counters.
    // Otherwise reset can leave `request_count == 0, error_count == 1`, breaking
    // both "reset clears counters" and D7's request/error invariant.
    if (generation_ != request_generation) {
        return;
    }

    auto& after = counters_[me];
    after.physical_beat_count += response.beats;
    after.bank_conflict_count += response.bank_conflicts;
    after.arbitration_event_count += response.beats;
    after.transferred_bytes += response.bytes;
    after.total_latency += response.latency;
    if (response.status != neo_status::ok) {
        ++after.error_count;
    }
}

void neo_local_sram_fabric::charge_annotated(const neo_local_request& request,
                                             neo_local_response& response,
                                             sc_core::sc_time& delay)
{
    const std::uint32_t bpb = config_.bytes_per_beat();

    // Where this requester actually is in time. Under temporal decoupling that
    // is not `sc_time_stamp()`: an initiator running ahead in its quantum has
    // an unconsumed `delay`, and charging contention against global time
    // instead would compare its future against another requester's past.
    const sc_core::sc_time arrival = sc_core::sc_time_stamp() + delay;
    sc_core::sc_time cursor = arrival;

    std::uint32_t done = 0;
    while (done < request.size) {
        const std::uint64_t address = request.address + done;
        const std::uint32_t within = static_cast<std::uint32_t>(address % bpb);
        const std::uint32_t bytes =
            std::min<std::uint32_t>(request.size - done, bpb - within);

        bank_state& bank = *banks_[bank_of(address)];
        if (bank.busy_until > cursor) {
            ++response.bank_conflicts;
        }
        const sc_core::sc_time start = std::max(cursor, bank.busy_until);
        cursor = start + cycle_;
        bank.busy_until = cursor;
        ++bank.grants[sram::index_of(request.requester)];

        const neo_status beat = move_beat(request, address, done, bytes);
        if (beat != neo_status::ok) {
            model_defect(std::string(name()),
                         std::string("a classified access failed at the "
                                     "storage with status ")
                             + sram::to_string(beat));
        }

        ++response.beats;
        // Bytes moved, not bytes named: a masked beat transfers fewer.
        response.bytes += sram::enabled_byte_count(
            request.strobes != nullptr ? request.strobes + done : nullptr,
            bytes);
        done += bytes;
    }

    response.latency =
        (cursor - arrival) + cycle_ * static_cast<double>(config_.pipeline_stages);
    delay += response.latency;
}

void neo_local_sram_fabric::charge_arbitrated(const neo_local_request& request,
                                              neo_local_response& response,
                                              sc_core::sc_time& delay,
                                              std::uint64_t request_generation)
{
    const auto abandoned = [&] { return generation_ != request_generation; };

    // An arbitrated requester has to be where it says it is before it can
    // contend with anyone: arbitration is about who holds a bank *now*, and a
    // requester still carrying an unconsumed quantum is not here yet.
    if (delay != sc_core::SC_ZERO_TIME) {
        sc_core::wait(delay);
        delay = sc_core::SC_ZERO_TIME;
        if (abandoned()) {
            response.status = neo_status::aborted;
            return;
        }
    }

    const std::uint32_t bpb = config_.bytes_per_beat();
    const unsigned me = sram::index_of(request.requester);
    const sc_core::sc_time begin = sc_core::sc_time_stamp();

    // A reset while this request is blocked abandons it. Every point where
    // the process can resume re-checks this: waiting for a grant, holding a
    // bank, and draining the pipeline. Without it, `reset()` clearing
    // `waiting[]` would leave a blocked requester unselectable and unwoken —
    // waiting forever for a grant no arbiter can issue.
    std::uint32_t done = 0;
    while (done < request.size) {
        const std::uint64_t address = request.address + done;
        const std::uint32_t within = static_cast<std::uint32_t>(address % bpb);
        const std::uint32_t bytes =
            std::min<std::uint32_t>(request.size - done, bpb - within);

        bank_state& bank = *banks_[bank_of(address)];

        bank.waiting[me] = true;
        // A new waiter can change who the rotating priority selects, so the
        // requesters already blocked have to re-evaluate.
        bank.changed.notify(sc_core::SC_ZERO_TIME);

        if (bank.busy || select_waiter(bank) != me) {
            // Counted once per beat, not once per wake-up: this is "the beat
            // found its bank occupied", which is the back-pressure event, not
            // the number of times the process happened to be rescheduled.
            ++response.bank_conflicts;
            while (bank.busy || select_waiter(bank) != me) {
                sc_core::wait(bank.changed);
                if (abandoned()) {
                    // `reset()` already cleared `waiting[]` and released the
                    // banks, so there is nothing to unwind here — touching
                    // bank state now would corrupt whoever was granted next.
                    response.status = neo_status::aborted;
                    response.latency = sc_core::sc_time_stamp() - begin;
                    return;
                }
            }
        }

        bank.waiting[me] = false;
        bank.busy = true;
        bank.last_granted = me;
        ++bank.grants[me];

        sc_core::wait(cycle_);

        if (abandoned()) {
            // The beat is dropped rather than written. Reset abandons
            // in-flight work; silently completing it would be worse than
            // either alternative (`ARCHITECTURE.md` §6).
            response.status = neo_status::aborted;
            response.latency = sc_core::sc_time_stamp() - begin;
            return;
        }

        const neo_status beat = move_beat(request, address, done, bytes);
        if (beat != neo_status::ok) {
            model_defect(std::string(name()),
                         std::string("a classified access failed at the "
                                     "storage with status ")
                             + sram::to_string(beat));
        }

        bank.busy = false;
        bank.changed.notify(sc_core::SC_ZERO_TIME);

        ++response.beats;
        // Bytes moved, not bytes named: a masked beat transfers fewer.
        response.bytes += sram::enabled_byte_count(
            request.strobes != nullptr ? request.strobes + done : nullptr,
            bytes);
        done += bytes;
    }

    if (config_.pipeline_stages > 0) {
        sc_core::wait(cycle_ * static_cast<double>(config_.pipeline_stages));
        if (abandoned()) {
            response.status = neo_status::aborted;
        }
    }
    response.latency = sc_core::sc_time_stamp() - begin;
}

std::uint32_t neo_local_sram_fabric::dbg_access(const neo_local_request& request)
{
    // No arbitration, no latency, no counters — and no relaxation of decode or
    // bounds: a debug write outside the region fails like any other
    // (`INTERFACE_CONTRACT.md` §8).
    if (sram_.classify(request.address, request.size) != neo_status::ok) {
        return 0;
    }
    if (request.data == nullptr) {
        return 0;
    }

    const neo_status status =
        request.command == neo_command::write
            ? sram_.debug_write(request.address, request.size, request.data,
                                request.strobes)
            : sram_.debug_read(request.address, request.size, request.data,
                               request.strobes);
    return status == neo_status::ok ? request.size : 0;
}

std::uint64_t neo_local_sram_fabric::total_requests() const noexcept
{
    std::uint64_t total = 0;
    for (const auto& entry : counters_) {
        total += entry.request_count;
    }
    return total;
}

std::uint64_t neo_local_sram_fabric::total_beats() const noexcept
{
    std::uint64_t total = 0;
    for (const auto& entry : counters_) {
        total += entry.physical_beat_count;
    }
    return total;
}

std::uint64_t neo_local_sram_fabric::total_bytes() const noexcept
{
    std::uint64_t total = 0;
    for (const auto& entry : counters_) {
        total += entry.transferred_bytes;
    }
    return total;
}

std::uint64_t neo_local_sram_fabric::total_bank_conflicts() const noexcept
{
    std::uint64_t total = 0;
    for (const auto& entry : counters_) {
        total += entry.bank_conflict_count;
    }
    return total;
}

void neo_local_sram_fabric::reset()
{
    // Bumped first: a requester that wakes on the notification below must see
    // the new generation and abandon itself rather than re-enter arbitration
    // against freshly cleared state.
    ++generation_;

    for (auto& entry : counters_) {
        entry = requester_counters{};
    }
    for (auto& bank : banks_) {
        bank->busy_until = sc_core::SC_ZERO_TIME;
        bank->busy = false;
        bank->last_granted = neo_requester_count - 1;
        for (unsigned i = 0; i < neo_requester_count; ++i) {
            bank->waiting[i] = false;
            bank->grants[i] = 0;
        }
        // Wake everyone. Clearing `waiting[]` without this is what left a
        // blocked requester both unselectable and unwoken.
        bank->changed.notify(sc_core::SC_ZERO_TIME);
    }

    // `in_flight_` is deliberately *not* cleared. The processes that own those
    // flags are still inside `b_access` and their guards will release them as
    // they unwind; clearing here would let a second request from the same
    // requester start while the first is still returning.
}

std::string neo_local_sram_fabric::report() const
{
    std::ostringstream out;
    out << name() << '\n'
        << "  geometry        : " << config_.data_width_bits << "-bit x "
        << config_.bank_count << " banks (" << config_.bytes_per_beat()
        << " bytes/beat, " << config_.stripe_bytes() << "-byte stripe), "
        << to_string(config_.mapping) << '\n'
        << "  arbitration     : " << to_string(config_.arbitration) << ", "
        << to_string(timing_) << " timing, " << config_.pipeline_stages
        << " pipeline stages, beat = " << cycle_ << '\n'
        << "  attached        :";
    for (unsigned i = 0; i < neo_requester_count; ++i) {
        if (attached_[i]) {
            out << ' ' << sram::to_string(static_cast<neo_requester>(i));
        }
    }
    out << "\n  requester          requests   beats  conflicts      bytes  "
           "errors   latency\n";
    for (unsigned i = 0; i < neo_requester_count; ++i) {
        if (!attached_[i]) {
            continue;
        }
        const auto& c = counters_[i];
        out << "  " << sram::to_string(static_cast<neo_requester>(i));
        for (std::size_t pad = std::string(
                 sram::to_string(static_cast<neo_requester>(i)))
                 .size();
             pad < 19; ++pad) {
            out << ' ';
        }
        out << c.request_count << "  " << c.physical_beat_count << "  "
            << c.bank_conflict_count << "  " << c.transferred_bytes << "  "
            << c.error_count << "  " << c.total_latency << '\n';
    }
    out << "  granularity     : VP++ element-wise granularity — one request "
           "usually carries one\n"
           "                    active element with the current backend, "
           "which is a property of\n"
           "                    that backend and not an invariant. A beat or "
           "arbitration-event\n"
           "                    count is not a hardware bus transaction "
           "count and is not\n"
           "                    evidence of equivalence with TPU hardware "
           "(decision record D7).\n";
    return out.str();
}

} // namespace cdc::components::tpu_v3::core
