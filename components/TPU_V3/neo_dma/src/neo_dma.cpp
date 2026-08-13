// SPDX-License-Identifier: Apache-2.0

#include "tpu_v3/dma/neo_dma.h"

#include <algorithm>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace cdc::components::tpu_v3::dma {

namespace {

constexpr unsigned kMmioWidth = 4;
/// One RVV register at VLEN=512 — the largest a single native access may be.
constexpr std::uint32_t kLocalChunkMax = sram::neo_max_transfer_bytes;
/// Every RV32-visible address is below 4 GiB (plan §12 rule 1).
constexpr std::uint64_t kAddressLimit = 0x1'0000'0000ull;

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

std::uint32_t low32(std::uint64_t value)
{
    return static_cast<std::uint32_t>(value & 0xFFFF'FFFFull);
}

std::uint32_t high32(std::uint64_t value)
{
    return static_cast<std::uint32_t>(value >> 32);
}

} // namespace

const char* to_string(error_cause cause) noexcept
{
    switch (cause) {
    case error_cause::none:
        return "none";
    case error_cause::invalid_length:
        return "invalid_length";
    case error_cause::address_overflow:
        return "address_overflow";
    case error_cause::unsupported_endpoints:
        return "unsupported_endpoints";
    case error_cause::source_unmapped:
        return "source_unmapped";
    case error_cause::destination_unmapped:
        return "destination_unmapped";
    case error_cause::local_read:
        return "local_read";
    case error_cause::local_write:
        return "local_write";
    case error_cause::external_read:
        return "external_read";
    case error_cause::external_write:
        return "external_write";
    case error_cause::internal:
        return "internal";
    }
    return "unknown";
}

void neo_dma_config::validate(const std::string& context) const
{
    const auto reject = [&](const std::string& field, const std::string& value,
                            const std::string& expected) {
        throw std::invalid_argument("tpu_v3::neo_dma: " + context + '.' + field
                                    + " = " + value + " is not accepted; "
                                    + expected);
    };

    if (control_size < reg::implemented_end
        || (control_size & (control_size - 1)) != 0) {
        reject("control_size", std::to_string(control_size),
               "it must be a power of two large enough to hold the register "
               "map (" + std::to_string(reg::implemented_end) + " bytes)");
    }
    if (control_base % control_size != 0) {
        reject("control_base", hex(control_base),
               "the window must be naturally aligned (plan §12 rule 2)");
    }
    if (sram_window == 0 || (sram_window & (sram_window - 1)) != 0) {
        reject("sram_window", std::to_string(sram_window),
               "it must be a non-zero power of two");
    }
    if (sram_base % sram_window != 0) {
        reject("sram_base", hex(sram_base),
               "the core SRAM window must be naturally aligned");
    }
    if (max_burst_bytes < external_bus_bytes
        || max_burst_bytes > external_bus_bytes * external_max_beats
        || (max_burst_bytes & (max_burst_bytes - 1)) != 0) {
        reject("max_burst_bytes", std::to_string(max_burst_bytes),
               "it must be a power of two between "
                   + std::to_string(external_bus_bytes) + " and "
                   + std::to_string(external_bus_bytes * external_max_beats)
                   + "; the interconnect refuses a longer frame outright "
                     "(INTERFACE_CONTRACT.md §7)");
    }
    if (chunk_latency < sc_core::SC_ZERO_TIME) {
        reject("chunk_latency", chunk_latency.to_string(),
               "it cannot be negative");
    }
}

neo_dma::neo_dma(sc_core::sc_module_name name, neo_dma_config config)
    : sc_core::sc_module(name)
    , control("control")
    , local("local")
    , external("external")
    , irq("irq")
    , config_((config.validate(std::string(name)), std::move(config)))
{
    control.register_b_transport(this, &neo_dma::b_transport);
    control.register_transport_dbg(this, &neo_dma::transport_dbg);

    staging_.resize(static_cast<std::size_t>(config_.max_burst_bytes));

    SC_THREAD(worker);

    // Deliberately *not* `dont_initialize()`: the initial run at time zero is
    // what drives the line low before anything can look at it. A reset value
    // left to the default-constructed signal would make "no interrupt yet" and
    // "interrupt already pending" the same observation.
    SC_METHOD(drive_irq);
    sensitive << irq_event_;
}

void neo_dma::drive_irq()
{
    // Level, and gated by the enable. An aborted job is deliberately absent:
    // firmware asked for the abort, so there is nothing to wake it for.
    const bool pending =
        (status_ & (status_bit::done | status_bit::error)) != 0;
    const bool enabled = (irq_enable_ & irq_enable_bit::completion) != 0;
    irq.write(pending && enabled);
}

endpoint_kind neo_dma::classify(std::uint64_t address,
                                std::uint64_t length) const noexcept
{
    if (length == 0) {
        return endpoint_kind::external;
    }
    if (address_map::contains(config_.sram_base, config_.sram_window, address,
                              length)) {
        return endpoint_kind::local;
    }
    // Overlap without containment. Computed as a subtraction on whichever side
    // cannot underflow, so a length near 2^64 cannot wrap the comparison.
    const std::uint64_t base = config_.sram_base;
    const std::uint64_t window = config_.sram_window;
    const bool overlaps = (address >= base) ? (address - base) < window
                                            : (base - address) < length;
    return overlaps ? endpoint_kind::straddling : endpoint_kind::external;
}

// ── control plane ────────────────────────────────────────────────────────────

bool neo_dma::check_control_rules(tlm::tlm_generic_payload& trans)
{
    const auto command = trans.get_command();
    if (command != tlm::TLM_READ_COMMAND && command != tlm::TLM_WRITE_COMMAND) {
        trans.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
        return false;
    }
    if (trans.get_data_length() != kMmioWidth
        || trans.get_address() % kMmioWidth != 0) {
        trans.set_response_status(tlm::TLM_BURST_ERROR_RESPONSE);
        return false;
    }
    const unsigned int streaming = trans.get_streaming_width();
    if (streaming != 0 && streaming < trans.get_data_length()) {
        trans.set_response_status(tlm::TLM_BURST_ERROR_RESPONSE);
        return false;
    }
    if (!all_bytes_enabled(trans)) {
        trans.set_response_status(tlm::TLM_BURST_ERROR_RESPONSE);
        return false;
    }
    if (trans.get_data_ptr() == nullptr) {
        // Answered rather than thrown, unlike the Phase 3 register files. This
        // target is reachable from a remote master through the external
        // bridge, and taking the simulation down on a malformed remote payload
        // would turn a diagnosable protocol error into a crash with no
        // response to trace it by.
        trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
        return false;
    }
    // Absolute decode over the whole 64 KiB window. Reserved offsets are
    // inside it and answer as reserved; `control_base + 0x1000` is therefore
    // not offset zero.
    const std::uint64_t address = trans.get_address();
    if (address < config_.control_base
        || address - config_.control_base >= config_.control_size
        || config_.control_size - (address - config_.control_base)
            < kMmioWidth) {
        trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
        return false;
    }
    return true;
}

std::uint32_t neo_dma::read_register(std::uint64_t offset) const
{
    switch (offset) {
    case reg::id:
        return identity;
    case reg::version:
        return model_version;
    case reg::control:
        // Both bits are write-1-to-set and self-clearing, so there is nothing
        // to read back. Zero, rather than a stale echo that a driver might
        // mistake for "still starting".
        return 0;
    case reg::status:
        return status_;
    case reg::src_addr_lo:
        return low32(src_addr_);
    case reg::src_addr_hi:
        return high32(src_addr_);
    case reg::dst_addr_lo:
        return low32(dst_addr_);
    case reg::dst_addr_hi:
        return high32(dst_addr_);
    case reg::length:
        return length_;
    case reg::irq_enable:
        return irq_enable_;
    case reg::error_cause:
        return static_cast<std::uint32_t>(error_cause_);
    case reg::bytes_done_lo:
        return low32(bytes_done_);
    case reg::bytes_done_hi:
        return high32(bytes_done_);
    case reg::transfer_count:
        return low32(transfer_count_);
    case reg::error_count:
        return low32(error_count_);
    case reg::abort_count:
        return low32(abort_count_);
    case reg::overrun_count:
        return low32(overrun_count_);
    case reg::local_bytes_lo:
        return low32(local_bytes_);
    case reg::local_bytes_hi:
        return high32(local_bytes_);
    case reg::external_bytes_lo:
        return low32(external_bytes_);
    case reg::external_bytes_hi:
        return high32(external_bytes_);
    default:
        // Reserved. Zero with an OK response is a *defined* state, so a
        // firmware register sweep does not have to know the implementation
        // status of every offset (`ADDRESS_MAP.md` §6).
        return 0;
    }
}

bool neo_dma::write_register(std::uint64_t offset, std::uint32_t value)
{
    const bool is_descriptor = offset == reg::src_addr_lo
        || offset == reg::src_addr_hi || offset == reg::dst_addr_lo
        || offset == reg::dst_addr_hi || offset == reg::length;

    if (is_descriptor && busy()) {
        // Refused, not queued and not applied. The active job holds its own
        // snapshot, so applying this would change nothing about the transfer
        // in flight while making the registers describe a job that is not
        // running — the worst of both.
        return false;
    }

    switch (offset) {
    case reg::control: {
        const bool wants_start = (value & control_bit::start) != 0;
        const bool wants_abort = (value & control_bit::abort) != 0;

        if (wants_start && wants_abort) {
            // No defined ordering between the two in one access, so there is
            // no correct outcome to pick. Refused with no state change.
            return false;
        }
        if (wants_abort) {
            if (busy()) {
                // Advancing the generation is what actually stops the worker;
                // clearing BUSY here is what makes the stop visible before it
                // has unwound.
                ++generation_;
                ++abort_count_;
                status_ &= ~status_bit::busy;
                status_ |= status_bit::aborted;
                update_irq();
            }
            return true;
        }
        if (wants_start) {
            if (busy()) {
                ++overrun_count_;
                return false;
            }
            // Snapshot, mark busy, wake the worker, return. No data has moved
            // and none will before this access completes.
            active_.source = src_addr_;
            active_.destination = dst_addr_;
            active_.length = length_;

            status_ &= ~(status_bit::done | status_bit::error
                         | status_bit::aborted);
            status_ |= status_bit::busy;
            error_cause_ = error_cause::none;
            bytes_done_ = 0;
            // This job now owns the register. Any worker still unwinding from
            // an earlier one stops publishing here.
            bytes_done_generation_ = generation_;
            ++transfer_count_;
            update_irq();
            start_pending_ = true;
            start_event_.notify(sc_core::SC_ZERO_TIME);
        }
        return true;
    }
    case reg::status:
        // Write-1-to-clear on DONE, ERROR and ABORTED. BUSY is read-only: a
        // driver clearing it would tell the model a running job had stopped.
        status_ &= ~(value & status_bit::w1c_mask);
        update_irq();
        return true;
    case reg::src_addr_lo:
        src_addr_ = (src_addr_ & 0xFFFF'FFFF'0000'0000ull) | value;
        return true;
    case reg::src_addr_hi:
        src_addr_ = (src_addr_ & 0xFFFF'FFFFull)
            | (static_cast<std::uint64_t>(value) << 32);
        return true;
    case reg::dst_addr_lo:
        dst_addr_ = (dst_addr_ & 0xFFFF'FFFF'0000'0000ull) | value;
        return true;
    case reg::dst_addr_hi:
        dst_addr_ = (dst_addr_ & 0xFFFF'FFFFull)
            | (static_cast<std::uint64_t>(value) << 32);
        return true;
    case reg::length:
        length_ = value;
        return true;
    case reg::irq_enable:
        irq_enable_ = value & irq_enable_bit::writable_mask;
        update_irq();
        return true;
    default:
        // Read-only and reserved offsets drop the write. Defined, not an
        // error, for the same reason a reserved read answers zero.
        return true;
    }
}

void neo_dma::b_transport(tlm::tlm_generic_payload& trans,
                          sc_core::sc_time& delay)
{
    trans.set_dmi_allowed(false);
    if (!check_control_rules(trans)) {
        return;
    }

    const std::uint64_t offset = trans.get_address() - config_.control_base;
    unsigned char* data = trans.get_data_ptr();

    if (trans.get_command() == tlm::TLM_READ_COMMAND) {
        const std::uint32_t value = read_register(offset);
        std::memcpy(data, &value, sizeof(value));
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
        return;
    }

    std::uint32_t value = 0;
    std::memcpy(&value, data, sizeof(value));
    const bool accepted = write_register(offset, value);
    trans.set_response_status(accepted ? tlm::TLM_OK_RESPONSE
                                       : tlm::TLM_GENERIC_ERROR_RESPONSE);

    // No wait() anywhere on this path: a register access is annotated, never
    // waited (`INTERFACE_CONTRACT.md` §3), and a start write in particular
    // must return before any data moves. The worker owns every long operation.
    (void)delay;
}

unsigned int neo_dma::transport_dbg(tlm::tlm_generic_payload& trans)
{
    const auto command = trans.get_command();
    if (command != tlm::TLM_READ_COMMAND && command != tlm::TLM_WRITE_COMMAND) {
        return 0;
    }
    const std::uint64_t address = trans.get_address();
    const unsigned int length = trans.get_data_length();
    if (length != kMmioWidth || address % kMmioWidth != 0
        || trans.get_data_ptr() == nullptr || address < config_.control_base
        || address - config_.control_base >= config_.control_size
        || config_.control_size - (address - config_.control_base) < length) {
        return 0;
    }

    const std::uint64_t offset = address - config_.control_base;
    if (command == tlm::TLM_WRITE_COMMAND) {
        // Deliberately **side-effect free**: a debug write does not start,
        // abort or acknowledge anything. A loader is not firmware, and a
        // host-side register poke that launched a transfer would make the
        // model's behaviour depend on how a test happened to set it up
        // (`INTERFACE_CONTRACT.md` §8).
        return 0;
    }

    const std::uint32_t value = read_register(offset);
    std::memcpy(trans.get_data_ptr(), &value, sizeof(value));
    return length;
}

// ── worker ───────────────────────────────────────────────────────────────────

error_cause neo_dma::validate_descriptor(const descriptor& job) const noexcept
{
    if (job.length == 0) {
        return error_cause::invalid_length;
    }
    // Overflow first, so the classification below never runs on a wrapped
    // span. Both ends are checked against the RV32 limit, not just 2^64.
    if (job.source > kAddressLimit - job.length
        || job.destination > kAddressLimit - job.length) {
        return error_cause::address_overflow;
    }

    const endpoint_kind source = classify(job.source, job.length);
    const endpoint_kind destination = classify(job.destination, job.length);

    if (source == endpoint_kind::straddling) {
        return error_cause::source_unmapped;
    }
    if (destination == endpoint_kind::straddling) {
        return error_cause::destination_unmapped;
    }
    if (source == destination) {
        // Both local or both external. Revision 1 implements exactly one of
        // each; local-to-local would also need an overlap contract that does
        // not exist, since the staging buffer makes a forward-overlapping copy
        // read bytes it has already overwritten.
        return error_cause::unsupported_endpoints;
    }
    return error_cause::none;
}

bool neo_dma::local_access(sram::neo_command command, std::uint64_t address,
                           std::uint32_t size, unsigned char* data,
                           std::uint64_t generation, bool destination)
{
    std::uint32_t done = 0;
    while (done < size) {
        const std::uint32_t piece = std::min(size - done, kLocalChunkMax);

        sram::neo_local_request request;
        request.requester = sram::neo_requester::dma;
        request.command = command;
        request.address = address + done;
        request.size = piece;
        request.data = data + done;

        sram::neo_local_response response;
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
        // Captured before the call: a reset landing while this is in flight
        // opens a new measurement window, and this request belongs to the old
        // one.
        const std::uint64_t traffic = traffic_epoch_;
        local->b_access(request, response, delay);

        // Record the boundary result *before* consuming the delay. A reset
        // landing inside that wait must not be able to hide a memory effect
        // that has already happened.
        if (traffic_epoch_ == traffic) {
            ++local_requests_;
            local_bytes_ += response.bytes;
        }
        if (destination && bytes_done_generation_ == generation) {
            // Whatever the *status*: the fabric reports `aborted` with the
            // bytes it had already moved, and those bytes are in memory, so
            // refusing to count them would make `BYTES_DONE` under-report a
            // commit — the same class of lie as over-reporting one.
            //
            // The guard is register ownership, not epoch sameness. A reset
            // that interrupts this job does not hand `BYTES_DONE` to anyone
            // else, so the bytes this access already committed still belong
            // in it. Only a new `START` claims the register, and from then on
            // this worker publishes nothing — an old job's bytes in a new
            // job's count is what plan §11.5 forbids.
            bytes_done_ += response.bytes;
        }

        if (response.status != sram::neo_status::ok) {
            return false;
        }
        if (delay != sc_core::SC_ZERO_TIME) {
            sc_core::wait(delay);
        }
        if (superseded(generation)) {
            return false;
        }
        done += piece;
    }
    return true;
}

bool neo_dma::external_access(tlm::tlm_command command, std::uint64_t address,
                              std::uint32_t size, unsigned char* data,
                              std::uint64_t generation, bool destination)
{
    tlm::tlm_generic_payload trans;
    trans.set_command(command);
    trans.set_address(address);
    trans.set_data_ptr(data);
    trans.set_data_length(size);
    trans.set_streaming_width(size);
    trans.set_byte_enable_ptr(nullptr);
    trans.set_byte_enable_length(0);
    trans.set_dmi_allowed(false);
    trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

    sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
    const std::uint64_t traffic = traffic_epoch_;
    external->b_transport(trans, delay);

    const bool counted = traffic_epoch_ == traffic;
    if (counted) {
        ++external_requests_;
    }
    const bool ok = trans.get_response_status() == tlm::TLM_OK_RESPONSE;
    if (ok) {
        // Published before the delay is consumed, for the same reason as the
        // local side: the target has already changed and a reset inside the
        // wait must not be able to hide it.
        if (counted) {
            external_bytes_ += size;
        }
        if (destination && bytes_done_generation_ == generation) {
            // Only while this job still owns the register; see `local_access`.
            bytes_done_ += size;
        }
    }
    if (!ok) {
        return false;
    }
    if (delay != sc_core::SC_ZERO_TIME) {
        sc_core::wait(delay);
    }
    return !superseded(generation);
}

error_cause neo_dma::run_transfer(const descriptor& job,
                                  std::uint64_t generation)
{
    const bool local_source =
        classify(job.source, job.length) == endpoint_kind::local;
    const std::uint64_t traffic = traffic_epoch_;

    std::uint64_t done = 0;
    while (done < job.length) {
        const std::uint64_t source = job.source + done;
        const std::uint64_t destination = job.destination + done;
        const std::uint64_t external_address =
            local_source ? destination : source;

        // One legal external frame, and never more than the configured burst
        // cap. 2048 bytes is reachable only at bus alignment; at a lane offset
        // the frame is shorter, which is exactly the case an implementation
        // that assumed 2048 always fits would get wrong.
        const std::uint64_t limit =
            std::min(config_.max_burst_bytes,
                     external_frame_limit(external_address));
        const auto chunk =
            static_cast<std::uint32_t>(std::min(job.length - done, limit));

        unsigned char* buffer = staging_.data();

        if (local_source) {
            if (!local_access(sram::neo_command::read, source, chunk, buffer,
                              generation, /*destination=*/false)) {
                return superseded(generation) ? error_cause::none
                                              : error_cause::local_read;
            }
            if (!external_access(tlm::TLM_WRITE_COMMAND, destination, chunk,
                                 buffer, generation, /*destination=*/true)) {
                return superseded(generation) ? error_cause::none
                                              : error_cause::external_write;
            }
        } else {
            if (!external_access(tlm::TLM_READ_COMMAND, source, chunk, buffer,
                                 generation, /*destination=*/false)) {
                return superseded(generation) ? error_cause::none
                                              : error_cause::external_read;
            }
            if (!local_access(sram::neo_command::write, destination, chunk,
                              buffer, generation, /*destination=*/true)) {
                return superseded(generation) ? error_cause::none
                                              : error_cause::local_write;
            }
        }

        // `bytes_done_` is *not* assigned here. It is accumulated by the
        // destination access itself, as each piece lands. Assigning it at the
        // end of a chunk would leave a window — the annotated delay of the
        // last destination access — in which memory had changed and the
        // register said it had not, which is exactly what a reset arriving in
        // that window would freeze into the record.
        done += chunk;
        if (traffic_epoch_ == traffic) {
            ++chunks_completed_;
        }

        if (config_.chunk_latency != sc_core::SC_ZERO_TIME) {
            sc_core::wait(config_.chunk_latency);
            if (superseded(generation)) {
                return error_cause::none;
            }
        }
    }
    return error_cause::none;
}

void neo_dma::worker()
{
    for (;;) {
        while (!start_pending_) {
            sc_core::wait(start_event_);
        }
        start_pending_ = false;

        if (!busy()) {
            // Aborted or reset between the request and this pickup. Nothing to
            // do, and nothing to publish.
            continue;
        }

        const std::uint64_t generation = generation_;
        const descriptor job = active_;

        // Validation is the worker's first action, not the register write's,
        // so an invalid descriptor reaches firmware through the same status
        // and IRQ path as a downstream failure.
        error_cause cause = validate_descriptor(job);
        if (cause == error_cause::none) {
            cause = run_transfer(job, generation);
        }

        if (superseded(generation)) {
            // Reset or abort won. Whoever bumped the generation already
            // published the state it wanted; writing ours would overwrite a
            // new epoch with an old job's outcome.
            continue;
        }

        finish(cause == error_cause::none ? status_bit::done
                                          : status_bit::error,
               cause, generation);
    }
}

void neo_dma::finish(std::uint32_t bit, error_cause cause,
                     std::uint64_t generation)
{
    if (superseded(generation)) {
        return;
    }
    status_ &= ~status_bit::busy;
    status_ |= bit;
    if (bit == status_bit::error) {
        error_cause_ = cause;
        ++error_count_;
    }
    update_irq();
}

void neo_dma::update_irq()
{
    // Immediate, not delta-delayed. The line is combinational from the status
    // bits, so it must settle within one delta of the write that changed them;
    // a delta notification would put the method one delta later and the signal
    // update one delta after that, so firmware acknowledging an interrupt
    // would still read it asserted on the very next cycle.
    irq_event_.notify();
}

void neo_dma::reset()
{
    // Advance the epoch first: a worker resuming after this must see a new
    // generation and abandon its job rather than publish into the state below.
    ++generation_;

    if (busy()) {
        ++abort_count_;
        // Committed bytes stay committed and stay reported. Nothing is rolled
        // back — the destination really does hold them — and pretending
        // otherwise would be a lie about memory. The register also keeps its
        // owner, so an access still in flight can still report into it.
    } else {
        bytes_done_ = 0;
        bytes_done_generation_ = generation_;
    }

    status_ = 0;
    error_cause_ = error_cause::none;
    active_ = descriptor{};
    // A request that was never picked up is abandoned with everything else; a
    // later START sets the flag again.
    start_pending_ = false;

    // Path traffic counters are cleared; job-lifecycle event counters are not.
    //
    // The split is forced by what these numbers are compared against.
    // `LOCAL_BYTES` and the request counts are reconciled with
    // `neo_local_sram_fabric`'s own counters, and that fabric clears its
    // counters on reset. In Phase 7 both are reset together by the core's
    // hierarchical reset path, so a DMA that kept lifetime traffic totals
    // would permanently disagree with a fabric that did not — conservation
    // would break at the first reset and stay broken.
    //
    // The event counters have no counterpart on the fabric and measure this
    // engine's own history: `ABORT_COUNT` is incremented by this very call, so
    // zeroing the block here would erase the fact it just recorded.
    ++traffic_epoch_;
    local_bytes_ = 0;
    external_bytes_ = 0;
    local_requests_ = 0;
    external_requests_ = 0;
    chunks_completed_ = 0;

    update_irq();
}

std::string neo_dma::report() const
{
    std::ostringstream out;
    out << name() << "  (independent NEO DMA, one descriptor slot)\n"
        << "  control window  : " << hex(config_.control_base) << " + "
        << hex(config_.control_size) << '\n'
        << "  local SRAM      : " << hex(config_.sram_base) << " + "
        << hex(config_.sram_window)
        << "  (requester 'dma' on the native plane)\n"
        << "  external frames : <= " << config_.max_burst_bytes
        << " bytes, further bounded by the 8-byte/256-beat formula at a lane "
           "offset\n"
        << "  status          : 0x" << std::hex << status_ << std::dec
        << (busy() ? "  BUSY" : "") << "  cause "
        << to_string(error_cause_) << '\n'
        << "  jobs            : " << transfer_count_ << " accepted, "
        << error_count_ << " errored, " << abort_count_ << " aborted, "
        << overrun_count_ << " rejected while busy\n"
        << "  bytes           : " << bytes_done_
        << " committed at the destination, " << local_bytes_ << " on the "
        << "native plane, " << external_bytes_ << " on the external path\n"
        << "  transactions    : " << chunks_completed_ << " chunks, "
        << local_requests_ << " native requests, " << external_requests_
        << " external transactions\n"
        << "  note            : a chunk count is not a byte count and neither "
           "is a hardware bus\n"
           "                    transaction count (decision record D7). On "
           "success both byte\n"
           "                    totals equal LENGTH; on error or abort each "
           "equals only its own\n"
           "                    successful boundary transactions.\n";
    return out.str();
}

} // namespace cdc::components::tpu_v3::dma
