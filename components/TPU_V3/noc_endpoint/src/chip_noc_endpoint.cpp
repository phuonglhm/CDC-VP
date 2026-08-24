// SPDX-License-Identifier: Apache-2.0

#include "tpu_v3/noc/chip_noc_endpoint.h"

#include <algorithm>
#include <cstddef>
#include <sstream>
#include <stdexcept>
#include <utility>

#include "tpu_v3/address_map.h"
#include "tpu_v3/core/neo_payload_rules.h"

namespace cdc::components::tpu_v3::noc {

namespace {

/// Counts a call in and out of a transport, and remembers the high-water mark.
///
/// Decrementing is the guard's job rather than a line at the end of the
/// function, because both transports have early returns for refusals and one
/// of them can throw. It is deliberately **not** cleared by `reset()`: the
/// calls it counts are inside C++ stacks a reset cannot unwind, and zeroing it
/// under them would make their destructors underflow — the Phase 8 lesson
/// about reset releasing a port it does not own, one level down.
struct in_flight_guard {
    std::uint64_t& live;
    std::uint64_t& peak;

    in_flight_guard(std::uint64_t& live_ref, std::uint64_t& peak_ref)
        : live(live_ref)
        , peak(peak_ref)
    {
        ++live;
        if (live > peak) {
            peak = live;
        }
    }
    ~in_flight_guard() { --live; }
};

using core::check_common_payload_rules;
using core::common_payload_error;
using core::expand_byte_enables;
using core::payload_rule_error;
using core::ranges_overlap;

} // namespace

void chip_endpoint_config::validate(const std::string& context) const
{
    if (!address_map::valid_chip(chip)) {
        throw std::invalid_argument(
            context + ": chip " + std::to_string(chip) + " is outside 0.."
            + std::to_string(max_chips - 1)
            + ". The limit is the 3-bit FlooNoC manager id (decision record "
              "D2)");
    }
    if (bus_bytes == 0 || (bus_bytes & (bus_bytes - 1)) != 0) {
        throw std::invalid_argument(
            context + ": bus_bytes must be a non-zero power of two");
    }
    if (max_frame_bytes == 0 || max_frame_bytes % bus_bytes != 0) {
        throw std::invalid_argument(
            context + ": max_frame_bytes must be a non-zero multiple of "
                      "bus_bytes; a frame that is not a whole number of beats "
                      "has no meaning on this interconnect");
    }
    if (max_inbound_sram_bytes == 0
        || max_inbound_sram_bytes % bus_bytes != 0) {
        throw std::invalid_argument(
            context + ": max_inbound_sram_bytes must be a non-zero multiple of "
                      "bus_bytes. The splitter aligns chunk boundaries to the "
                      "bus, so a limit that is not a bus multiple makes later "
                      "chunks start mid-word -- and a limit smaller than a lane "
                      "offset underflows and emits a chunk larger than the "
                      "limit itself");
    }
    // The upper bound is not a preference, it is the native plane's own
    // maximum: `sram::neo_max_transfer_bytes` is one RVV register at VLEN=512,
    // the largest payload any single access can need (D7). A NEO-CORE's
    // external bridge refuses an inbound SRAM access above it and says why --
    // "the chip endpoint chunks oversized transfers before they reach a core;
    // one arriving here means that did not happen"
    // (`neo_external_bridge.cpp:142`). Accepting a larger limit here therefore
    // configures this endpoint to do exactly the thing that comment describes
    // as broken: it elaborates cleanly and then every inbound SRAM transfer is
    // refused downstream. Refused at construction instead
    // (`INTERFACE_CONTRACT.md` §10).
    if (max_inbound_sram_bytes > sram::neo_max_transfer_bytes) {
        throw std::invalid_argument(
            context + ": max_inbound_sram_bytes is "
            + std::to_string(max_inbound_sram_bytes) + ", above the native "
              "plane's maximum of "
            + std::to_string(sram::neo_max_transfer_bytes)
            + ". A NEO-CORE's external bridge refuses an inbound SRAM access "
              "longer than that, so this endpoint would split into chunks the "
              "core cannot accept and every inbound SRAM transfer would fail. "
              "The accepted range is a non-zero multiple of bus_bytes up to "
            + std::to_string(sram::neo_max_transfer_bytes));
    }
}

chip_noc_endpoint::chip_noc_endpoint(sc_core::sc_module_name name,
                                     chip_endpoint_config config)
    : sc_core::sc_module(name)
    , from_chip("from_chip")
    , to_chip("to_chip")
    , to_noc("to_noc")
    , from_noc("from_noc")
    , config_((config.validate(std::string("tpu_v3::chip_noc_endpoint[")
                               + std::string(name) + "]"),
               std::move(config)))
{
    from_chip.register_b_transport(this, &chip_noc_endpoint::chip_b_transport);
    from_chip.register_transport_dbg(this,
                                     &chip_noc_endpoint::chip_transport_dbg);
    from_noc.register_b_transport(this, &chip_noc_endpoint::noc_b_transport);
    from_noc.register_transport_dbg(this,
                                    &chip_noc_endpoint::noc_transport_dbg);
}

std::uint64_t chip_noc_endpoint::aperture_base() const noexcept
{
    return address_map::chip_base(config_.chip);
}

std::uint64_t chip_noc_endpoint::aperture_size() const noexcept
{
    return address_map::chip_aperture_stride;
}

std::uint64_t chip_noc_endpoint::beats_for(std::uint64_t address,
                                           std::uint64_t length) const noexcept
{
    if (length == 0) {
        return 0;
    }
    const std::uint64_t lane = address % config_.bus_bytes;
    return (lane + length + config_.bus_bytes - 1) / config_.bus_bytes;
}

std::uint64_t chip_noc_endpoint::frame_capacity_at(
    std::uint64_t address) const noexcept
{
    // The frame starts at the bus-aligned address below the request, so an
    // offset access reaches the limit sooner. This is the interconnect's own
    // arithmetic, restated where the splitting decision is made.
    const std::uint64_t lane = address % config_.bus_bytes;
    return config_.max_frame_bytes - lane;
}

bool chip_noc_endpoint::overlaps_aperture(std::uint64_t address,
                                          std::uint64_t length) const noexcept
{
    return ranges_overlap(address, length, aperture_base(), aperture_size());
}

bool chip_noc_endpoint::inside_aperture(std::uint64_t address,
                                        std::uint64_t length) const noexcept
{
    return address_map::contains(aperture_base(), aperture_size(), address,
                                 length);
}

bool chip_noc_endpoint::inside_a_core_sram(
    std::uint64_t address, std::uint64_t length) const noexcept
{
    for (core_id_t core = 0; core < cores_per_chip; ++core) {
        if (address_map::contains(
                address_map::core_sram_base(config_.chip, core),
                address_map::core_sram_window, address, length)) {
            return true;
        }
    }
    return false;
}

chip_noc_endpoint::decoded_region chip_noc_endpoint::decode_region(
    std::uint64_t address) const noexcept
{
    decoded_region out;
    const auto hit = [&out](std::uint64_t base, std::uint64_t size,
                            bool is_memory) {
        out.valid = true;
        out.base = base;
        out.size = size;
        out.is_memory = is_memory;
    };

    // ── global regions ───────────────────────────────────────────────────────
    if (address < address_map::boot_rom_base + address_map::boot_rom_size) {
        if (address >= address_map::boot_rom_base) {
            hit(address_map::boot_rom_base, address_map::boot_rom_size, true);
        }
        return out;
    }
    if (address >= address_map::global_control_base
        && address < address_map::global_control_base
            + address_map::global_control_size) {
        hit(address_map::global_control_base, address_map::global_control_size,
            false);
        return out;
    }
    if (address >= address_map::global_ram_base
        && address < address_map::global_ram_base
            + address_map::global_ram_window) {
        hit(address_map::global_ram_base, address_map::global_ram_window, true);
        return out;
    }

    // ── chip apertures, decoded down to the window ───────────────────────────
    if (address < address_map::chip_aperture_base) {
        return out;
    }
    const std::uint64_t from_chips = address - address_map::chip_aperture_base;
    const std::uint64_t chip = from_chips / address_map::chip_aperture_stride;
    if (chip >= max_chips) {
        return out;
    }
    const std::uint64_t chip_base =
        address_map::chip_base(static_cast<chip_id_t>(chip));
    const std::uint64_t offset = address - chip_base;

    if (offset < cores_per_chip * address_map::core_aperture_stride) {
        const std::uint64_t core = offset / address_map::core_aperture_stride;
        const std::uint64_t core_base =
            chip_base + core * address_map::core_aperture_stride;
        const std::uint64_t within =
            offset % address_map::core_aperture_stride;

        if (within < address_map::core_sram_window) {
            hit(core_base + address_map::core_sram_offset,
                address_map::core_sram_window, true);
            return out;
        }
        // The five 64 KiB AXI4-Lite windows sit contiguously from
        // `core_control_offset`: core, SA, DMA, Transform, counters. Anything
        // past them is the reserved hole, which decodes nowhere.
        if (within >= address_map::core_control_offset) {
            const std::uint64_t index =
                (within - address_map::core_control_offset)
                / address_map::core_control_size;
            if (index < address_map::regions_per_core - 1) {
                hit(core_base + address_map::core_control_offset
                        + index * address_map::core_control_size,
                    address_map::core_control_size, false);
            }
        }
        return out;
    }

    if (offset >= address_map::chip_control_offset
        && offset < address_map::chip_control_offset
            + address_map::chip_control_size) {
        hit(chip_base + address_map::chip_control_offset,
            address_map::chip_control_size, false);
        return out;
    }
    if (offset >= address_map::chip_counters_offset
        && offset < address_map::chip_counters_offset
            + address_map::chip_counters_size) {
        hit(chip_base + address_map::chip_counters_offset,
            address_map::chip_counters_size, false);
        return out;
    }
    return out;
}

bool chip_noc_endpoint::span_fits(const decoded_region& region,
                                  std::uint64_t address,
                                  std::uint64_t length) noexcept
{
    // A transfer that leaves one region for another has no single owner. The
    // interconnect refuses it outright, having touched nothing; splitting first
    // would commit the chunks that do decode and only then fail.
    //
    // The region has to be the *finest* one, not the enclosing chip aperture: a
    // write that runs from a core's SRAM into its `SA_CONTROL` window is inside
    // one aperture and is still two targets.
    return region.valid
        && address_map::contains(region.base, region.size, address, length);
}

bool chip_noc_endpoint::in_a_chip_aperture(std::uint64_t address,
                                           std::uint64_t length) noexcept
{
    return address_map::contains(
        address_map::chip_aperture_base,
        address_map::chip_aperture_stride * max_chips, address, length);
}

std::uint64_t chip_noc_endpoint::chunk_span(std::uint64_t address,
                                            std::uint64_t remaining,
                                            std::uint64_t limit,
                                            bool shape_reads) const noexcept
{
    if (limit == 0) {
        // "Do not split." Used for traffic whose semantics a split would
        // change -- MMIO, where AXI4-Lite is four bytes and several register
        // writes are not the same thing as one wide one.
        return remaining;
    }

    const std::uint64_t lane = address % config_.bus_bytes;

    if (!shape_reads) {
        // Bus-aligned except possibly the first chunk
        // (`INTERFACE_CONTRACT.md` §7). An unaligned start runs to the next
        // boundary and every chunk after it is aligned, so a transfer pays for
        // its lane offset once instead of on every chunk.
        const std::uint64_t first_frame = limit - lane;
        return std::min(remaining, lane == 0 ? limit : first_frame);
    }

    // **Every chunk must be a shape the interconnect will not widen** (D25).
    // `shape_of()` keeps a transfer narrow — and narrow is never widened —
    // exactly when the length is a power of two no wider than the bus and the
    // address is naturally aligned to it (`axi_lanes.hpp:88`). Anything else
    // becomes a full-width frame, and a full-width read against an `mmio`
    // target is refused when either end is not bus-aligned.
    //
    // So: naturally-aligned narrow steps until the address reaches a bus
    // boundary, then whole bus-aligned frames, then naturally-aligned narrow
    // steps again for whatever is left. The first and last groups cost at most
    // three transactions each, because 1 + 2 + 4 covers any gap below the bus
    // width.
    if (lane != 0 || remaining < config_.bus_bytes) {
        // The largest natural alignment this address has, capped by the bus
        // and then by what is left. `address & -address` isolates the lowest
        // set bit; address zero is maximally aligned and has none.
        std::uint64_t size = address == 0
            ? config_.bus_bytes
            : std::min<std::uint64_t>(address & (~address + 1),
                                      config_.bus_bytes);
        while (size > remaining) {
            size >>= 1;
        }
        return size;
    }

    // Bus-aligned with at least one whole bus word left: take whole words.
    // `limit` is validated to be a multiple of `bus_bytes`, so the minimum of
    // two multiples is one too, and the chunk is bus-aligned at both ends.
    return std::min(remaining - remaining % config_.bus_bytes, limit);
}

sc_core::sc_time chip_noc_endpoint::logical_time(
    const sc_core::sc_time& delay) noexcept
{
    return sc_core::sc_time_stamp() + delay;
}

void chip_noc_endpoint::sample_latency(bool outbound,
                                       const sc_core::sc_time& entry,
                                       const sc_core::sc_time& exit) noexcept
{
    // `sc_time` is unsigned, so a subtraction the wrong way round would not
    // give a negative result -- it would give an enormous one. Logical time
    // cannot move backwards (the endpoint hands back the unelapsed remainder
    // of an interrupted wait for exactly that reason), so this guard should
    // never fire; it is here because the failure it prevents is a metric that
    // is wrong by a factor of 2^64 rather than one that is slightly off.
    if (exit < entry) {
        return;
    }
    const sc_core::sc_time span = exit - entry;
    if (outbound) {
        outbound_latency_total_ += span;
        if (span > outbound_latency_max_) {
            outbound_latency_max_ = span;
        }
        ++outbound_latency_samples_;
    } else {
        inbound_latency_total_ += span;
        if (span > inbound_latency_max_) {
            inbound_latency_max_ = span;
        }
        ++inbound_latency_samples_;
    }
}

bool chip_noc_endpoint::absorb_incoming_delay(sc_core::sc_time& delay,
                                              std::uint64_t generation)
{
    if (delay == sc_core::SC_ZERO_TIME) {
        return true;
    }
    // Interruptible, and preserving the arrival instant: the same shape the
    // Phase 8 arbiters use, for the same two reasons. A bare `wait(delay)`
    // cannot be woken by `reset()`, and returning zero from an interrupted wait
    // would move a temporally decoupled caller into its own past.
    const sc_core::sc_time arrival = sc_core::sc_time_stamp() + delay;
    while (sc_core::sc_time_stamp() < arrival) {
        sc_core::wait(arrival - sc_core::sc_time_stamp(), reset_event_);
        if (generation_ != generation) {
            delay = arrival - sc_core::sc_time_stamp();
            return false;
        }
    }
    delay = sc_core::SC_ZERO_TIME;
    return true;
}

tlm::tlm_response_status chip_noc_endpoint::forward_chunked(
    tlm_utils::simple_initiator_socket<chip_noc_endpoint>& socket,
    tlm::tlm_generic_payload& trans, sc_core::sc_time& delay,
    std::uint64_t chunk_limit, bool shape_reads, bool outbound,
    std::uint64_t& committed, std::uint64_t& moved, std::uint64_t& chunks)
{
    const std::uint64_t generation = generation_;
    const std::uint64_t base = trans.get_address();
    const std::uint64_t total = trans.get_data_length();
    unsigned char* const data = trans.get_data_ptr();

    // The byte-enable pattern is expanded once, so a chunk can take its own
    // slice of it. TLM lets the pattern be shorter than the payload and repeat;
    // handing a chunk the raw pointer would apply the pattern from its start
    // again at every chunk boundary, which silently masks the wrong bytes.
    std::vector<unsigned char> enables;
    if (!expand_byte_enables(trans, enables)) {
        ++protocol_errors_;
        return tlm::TLM_BURST_ERROR_RESPONSE;
    }

    // **A read is staged; a write is not.**
    //
    // A failed read must leave the caller's buffer untouched
    // (`INTERFACE_CONTRACT.md` §1: "Errors are never converted to zero data").
    // Handing a chunk a direct pointer into that buffer cannot honour it,
    // because the downstream path writes the buffer *before* this component
    // sees the status. `noc_interconnect` does exactly that, deliberately and
    // in both timing modes: a failed access assigns zeroed `read_data`
    // (`noc_interconnect.cpp:1149` for a decode miss, `:1234` for a target
    // refusal) and then `unpack_read()` copies it into the caller's pointer
    // unconditionally, one line before the status is set — fast/bypass at
    // `:1369`, routed mesh at `:2204`. That is AXI-faithful (RRESP is per beat
    // and the data lanes are still driven), so the adaptation belongs here,
    // at the boundary whose job is to absorb what the interconnect imposes.
    //
    // A **write** needs none of this: the target only reads the caller's
    // buffer, so a failure cannot modify it.
    //
    // Staging is **zero-initialised, not seeded from the caller's buffer**, and
    // only the enabled bytes are ever published back. Seeding it was the first
    // implementation, on the reasoning that a successful read would then be
    // byte-for-byte what the direct pointer produced. It also made the
    // byte-enable rule below unfalsifiable: a disabled position held the
    // caller's own value either way, so publishing the whole staged chunk was
    // indistinguishable from publishing the enabled bytes, and the control
    // written for it could not fail. This project has had to correct that
    // shape three times (Phase 5 §7, Phase 7 §3a, Phase 8 §2a), so the
    // mechanism that cannot be tested is the one that goes.
    //
    // The difference this leaves, stated rather than left to be found: if a
    // downstream target reports success without delivering an enabled byte,
    // the caller now sees zero there instead of its own stale value. Both are
    // wrong and the target is what is wrong; neither is reachable through
    // `unpack_read()`, which writes every enabled byte its beat frame covers.
    const bool is_read = trans.get_command() == tlm::TLM_READ_COMMAND;
    std::vector<unsigned char> read_staging;

    committed = 0;
    moved = 0;
    chunks = 0;

    while (committed < total) {
        if (generation_ != generation) {
            // A reset arrived between chunks. The transfer stops here rather
            // than carrying work from a previous epoch into a new one; what has
            // already been transferred stays transferred and is reported (D23).
            return tlm::TLM_GENERIC_ERROR_RESPONSE;
        }

        const std::uint64_t address = base + committed;
        const std::uint64_t span =
            chunk_span(address, total - committed, chunk_limit, shape_reads);

        unsigned char* chunk_data = data + committed;
        if (is_read) {
            // `assign` keeps the capacity it already has, so this allocates on
            // the first chunk and only refills afterwards.
            read_staging.assign(static_cast<std::size_t>(span), 0);
            chunk_data = read_staging.data();
        }

        tlm::tlm_generic_payload chunk;
        chunk.set_command(trans.get_command());
        chunk.set_address(address);
        chunk.set_data_ptr(chunk_data);
        chunk.set_data_length(static_cast<unsigned>(span));
        chunk.set_streaming_width(static_cast<unsigned>(span));
        if (enables.empty()) {
            chunk.set_byte_enable_ptr(nullptr);
            chunk.set_byte_enable_length(0);
        } else {
            chunk.set_byte_enable_ptr(enables.data() + committed);
            chunk.set_byte_enable_length(static_cast<unsigned>(span));
        }
        chunk.set_dmi_allowed(false);
        chunk.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

        socket->b_transport(chunk, delay);

        // **Re-checked here, not only at the top of the loop.** A reset while
        // this call was blocked has already passed the check above; without
        // this the finished chunk would be counted into freshly cleared
        // counters, and a reset landing on the *last* chunk would return
        // `TLM_OK_RESPONSE` for a transfer the reset abandoned. Reset cannot
        // unwind a blocked call -- the Phase 8 lesson -- so the call finishes
        // and its result is discarded rather than pretended away.
        //
        // Staging makes "discarded" true of the **data** as well, which it
        // previously was not: a read abandoned here used to leave its bytes in
        // the caller's buffer while being counted in no chunk and no byte
        // total. Data present and accounting absent is the worse of the two
        // states to be in, and now neither happens.
        if (generation_ != generation) {
            return tlm::TLM_GENERIC_ERROR_RESPONSE;
        }

        ++chunks;
        if (outbound) {
            ++outbound_chunks_;
        } else {
            ++inbound_chunks_;
        }

        if (chunk.get_response_status() != tlm::TLM_OK_RESPONSE) {
            // The first failing chunk stops the transfer and its status is
            // returned. Bytes already moved stay moved: there is nothing to
            // roll them back with, and a rollback that cannot be performed is
            // worse promised than omitted.
            //
            // **Counted here, not at the caller.** A failure that moved
            // nothing is still a failure, and the caller's `moved > 0` test —
            // which is the right test for a *partial* completion — is exactly
            // the one that cannot see it. Recorded per direction and kept
            // apart from the `*_refused_` counters, because "the endpoint
            // refused this, having touched nothing" and "the target refused
            // this, possibly after touching something" are different events
            // with different owners. Without this, `chunks > 0 && bytes == 0`
            // is the only residue, and a legitimate fully byte-disabled write
            // produces the same residue while succeeding.
            if (outbound) {
                ++outbound_target_errors_;
            } else {
                ++inbound_target_errors_;
            }
            return chunk.get_response_status();
        }

        // Address progress and bytes *moved* are different quantities, and
        // conflating them makes a fully masked write report a full payload as
        // transferred while the target performed no access at all
        // (`INTERFACE_CONTRACT.md` §1).
        // The chunk succeeded, so a staged read is published to the caller
        // here — and **only the enabled bytes**. Copying the whole staged
        // chunk back would overwrite the disabled positions with staging
        // content, turning a correct masked read into a corrupting one:
        // `unpack_read()` skips disabled bytes (`axi_lanes.hpp:156`), so the
        // target never wrote them and the caller's own values are what belongs
        // there. Folded into the same pass that counts moved bytes, because it
        // is the same set of bytes by definition.
        std::uint64_t chunk_moved = 0;
        if (enables.empty()) {
            chunk_moved = span;
            if (is_read) {
                std::copy(read_staging.begin(),
                          read_staging.begin()
                              + static_cast<std::ptrdiff_t>(span),
                          data + committed);
            }
        } else {
            for (std::uint64_t i = 0; i < span; ++i) {
                if (enables[committed + i] == TLM_BYTE_ENABLED) {
                    ++chunk_moved;
                    if (is_read) {
                        data[committed + i] = read_staging[i];
                    }
                }
            }
        }
        moved += chunk_moved;

        // **Published with the chunk that moved them**, alongside the chunk
        // counter a few lines above and under the same generation guard.
        // Accumulating into the member counter only after the whole transfer
        // unwound made the two disagree for as long as a transfer was
        // suspended — a five-chunk transfer blocked in chunk three reported
        // "2 chunks, 0 bytes" to anything reading the counters, and a
        // simulation ending in flight lost the bytes entirely, so
        // endpoint-boundary conservation did not hold. It also made the reset
        // comment above false: those bytes were reported nowhere.
        if (outbound) {
            outbound_bytes_ += chunk_moved;
        } else {
            inbound_bytes_ += chunk_moved;
        }
        committed += span;
    }
    return tlm::TLM_OK_RESPONSE;
}

void chip_noc_endpoint::chip_b_transport(tlm::tlm_generic_payload& trans,
                                         sc_core::sc_time& delay)
{
    trans.set_dmi_allowed(false);
    ++outbound_transfers_;
    const in_flight_guard live(outbound_in_flight_, peak_outbound_in_flight_);

    // Captured before every early return, published only where the other
    // counters are (decision record D24). Taking it here rather than after the
    // checks costs one addition and means the measurement cannot drift away
    // from "when the chip handed us the transfer" as checks are added.
    const sc_core::sc_time entry = logical_time(delay);

    if (!check_common_payload_rules(trans)) {
        ++protocol_errors_;
        return;
    }
    const std::uint64_t length = trans.get_data_length();
    if (length == 0 || trans.get_data_ptr() == nullptr) {
        ++protocol_errors_;
        trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
        return;
    }

    // **Overlap, not containment.** A transfer that begins inside this chip's
    // aperture and runs past it is not contained by it, and one byte of local
    // traffic in the mesh is still local traffic in the mesh. Reaching here at
    // all means the chip fabric's decoder is wrong: it answers local addresses
    // itself and hands this port only what is outside.
    if (overlaps_aperture(trans.get_address(), length)) {
        ++outbound_local_refused_;
        trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
        return;
    }

    // **Checked before splitting.** A transfer spanning two architectural
    // regions has no single owner. The interconnect refuses it outright,
    // having touched nothing; splitting first would commit the chunks that do
    // decode and only then fail, turning a clean refusal into a partial side
    // effect nobody asked for.
    const auto region = decode_region(trans.get_address());
    if (!span_fits(region, trans.get_address(), length)) {
        ++outbound_span_refused_;
        trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
        return;
    }

    // **Only memory-like traffic is split.** An MMIO transfer is forwarded
    // whole even when it is longer than a frame: the interconnect refuses it,
    // having touched nothing, which is the right answer. Splitting it would
    // turn one illegal transaction into several *legal* four-byte register
    // writes and actually modify the device — the same reasoning that keeps
    // inbound MMIO whole, which the first version applied in one direction
    // only.
    const std::uint64_t chunk_limit =
        region.is_memory ? config_.max_frame_bytes : 0;

    // **Reads into a remote core's SRAM are shaped, nothing else is** (D25).
    //
    // Three conditions, and each excludes a case that would otherwise be
    // wrong:
    //
    //  * `is_read` — the interconnect's widening refusal is on
    //    `TLM_READ_COMMAND` only, and a write pays nothing for it;
    //  * `region.is_memory` — a remote MMIO window must reach the
    //    interconnect **whole** so it can be refused. Shaping it would turn
    //    one illegal transaction into several legal register reads, which is
    //    the same mistake as splitting an oversized MMIO write;
    //  * `in_a_chip_aperture` — this is the only destination registered
    //    `mmio` while containing memory. `GLOBAL_RAM` and `GLOBAL_BOOT_ROM`
    //    are `memory` targets that widen safely, and shaping reads to them
    //    would add up to six transactions per offset read on the bulk path
    //    for nothing. Note that `region.is_memory` alone does **not** answer
    //    this: it is the architectural region kind, and `CORE_SRAM` is memory
    //    inside an aperture that is not.
    const bool is_read = trans.get_command() == tlm::TLM_READ_COMMAND;
    const bool shape_reads = is_read && config_.chip_apertures_are_mmio
        && region.is_memory && in_a_chip_aperture(trans.get_address(), length);

    // Decided from the original span, not from how many chunks were attempted:
    // a transfer whose first chunk fails still needed splitting.
    const bool needed_splitting =
        chunk_span(trans.get_address(), length, chunk_limit, shape_reads)
        < length;

    const std::uint64_t generation = generation_;
    if (config_.downstream_spends_delay
        && !absorb_incoming_delay(delay, generation)) {
        trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
        return;
    }

    std::uint64_t committed = 0;
    std::uint64_t moved = 0;
    std::uint64_t chunks = 0;
    const auto status = forward_chunked(to_noc, trans, delay, chunk_limit,
                                        shape_reads, /*outbound=*/true,
                                        committed, moved, chunks);
    if (generation_ != generation) {
        // The reset cleared the counters; an old-epoch transfer must not
        // repopulate them. It still reports its own outcome to its caller.
        trans.set_response_status(status == tlm::TLM_OK_RESPONSE
                                      ? tlm::TLM_GENERIC_ERROR_RESPONSE
                                      : status);
        return;
    }

    // Bytes are published per chunk inside `forward_chunked`, so there is
    // nothing to accumulate here.
    sample_latency(/*outbound=*/true, entry, logical_time(delay));
    if (needed_splitting) {
        ++outbound_split_transfers_;
    }
    (void)chunks;
    // **Partial means bytes actually moved before it stopped.** A transfer
    // whose first or only chunk fails has committed nothing, and calling that a
    // partial completion overwrites both metrics with a byte count of zero —
    // which is exactly what an unsplit MMIO refusal does, and it is the common
    // case rather than a corner one.
    if (status != tlm::TLM_OK_RESPONSE && moved > 0) {
        ++outbound_partial_failures_;
        last_partial_bytes_ = moved;
    }
    trans.set_response_status(status);
}

void chip_noc_endpoint::noc_b_transport(tlm::tlm_generic_payload& trans,
                                        sc_core::sc_time& delay)
{
    trans.set_dmi_allowed(false);
    ++inbound_transfers_;
    const in_flight_guard live(inbound_in_flight_, peak_inbound_in_flight_);

    const sc_core::sc_time entry = logical_time(delay);

    if (!check_common_payload_rules(trans)) {
        ++protocol_errors_;
        return;
    }
    const std::uint64_t length = trans.get_data_length();
    if (length == 0 || trans.get_data_ptr() == nullptr) {
        ++protocol_errors_;
        trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
        return;
    }

    // **Rebase.** The interconnect calls a mapped target with an address
    // relative to that target's base; everything inside the chip uses the one
    // absolute address per resource of `ADDRESS_MAP.md` §4. The caller's view
    // is restored before returning, exactly as `noc_interconnect` does for its
    // own debug path.
    const std::uint64_t relative = trans.get_address();
    if (relative >= aperture_size()
        || length > aperture_size() - relative) {
        ++inbound_foreign_refused_;
        trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
        return;
    }
    const std::uint64_t absolute = aperture_base() + relative;
    trans.set_address(absolute);

    // **Only SRAM traffic is split.** A NEO-CORE's bridge refuses an inbound
    // SRAM access longer than the native limit, and a remote master writing a
    // kilobyte of tensor is a legitimate thing to do. Everything else is
    // forwarded whole -- AXI4-Lite is four bytes, and turning a wider access
    // into several register writes would invent a semantics the control plane
    // never agreed to. A `chunk_limit` of zero says exactly that; using the
    // transfer's own length instead would still split a four-byte register at
    // an odd bus lane into a zero-length chunk.
    const std::uint64_t limit = inside_a_core_sram(absolute, length)
        ? config_.max_inbound_sram_bytes
        : 0;

    const std::uint64_t generation = generation_;
    std::uint64_t committed = 0;
    std::uint64_t moved = 0;
    std::uint64_t chunks = 0;
    // Inbound is never shaped: it does not traverse the interconnect's
    // widening check at all -- it has already arrived -- and its limit exists
    // for the core's native plane, not for AXI.
    const auto status = forward_chunked(to_chip, trans, delay, limit,
                                        /*shape_reads=*/false,
                                        /*outbound=*/false, committed, moved,
                                        chunks);
    trans.set_address(relative);

    if (generation_ != generation) {
        trans.set_response_status(status == tlm::TLM_OK_RESPONSE
                                      ? tlm::TLM_GENERIC_ERROR_RESPONSE
                                      : status);
        return;
    }

    // Bytes are published per chunk inside `forward_chunked`.
    sample_latency(/*outbound=*/false, entry, logical_time(delay));
    if (status != tlm::TLM_OK_RESPONSE && moved > 0) {
        // Its own field, and only when something was actually moved. Sharing
        // one with the outbound metric let an inbound failure silently rewrite
        // a number an outbound completion had already reported.
        //
        // The **count** matters as much as the value, and the inbound side had
        // only the value: an inbound transfer failing after committed bytes
        // overwrote `last_inbound_partial_bytes_` and recorded nowhere how
        // often it happened, so a single report was indistinguishable from
        // twenty.
        ++inbound_partial_failures_;
        last_inbound_partial_bytes_ = moved;
    }
    (void)chunks;
    trans.set_response_status(status);
}

namespace {

/// A debug refusal, answered the way `INTERFACE_CONTRACT.md` §1 requires of
/// **any** TPU_V3 transport, `transport_dbg` included: a status on every
/// return path and DMI off. Returning a bare zero left the caller reading the
/// `TLM_INCOMPLETE_RESPONSE` it had set itself — which §1 says is a defect in
/// the target, not a condition for the caller to handle.
unsigned int refuse_debug(tlm::tlm_generic_payload& trans,
                          tlm::tlm_response_status status)
{
    trans.set_dmi_allowed(false);
    trans.set_response_status(status);
    return 0;
}

/// The payload-rule outcome, named the way `INTERFACE_CONTRACT.md` §1 names it.
///
/// §1 distinguishes an invalid **command** from an invalid **burst shape**, and
/// `check_common_payload_rules()` reports that distinction on the normal
/// transport path. The debug path folded both into `TLM_BURST_ERROR_RESPONSE`,
/// so a `TLM_IGNORE_COMMAND` debug access came back describing a burst it never
/// had. §8 relaxes timing for debug, not the error taxonomy — a caller that
/// switches between the two transports must not have to know which one it used
/// to read the status.
///
/// **Three outcomes, not two**, and the third is the one an earlier version of
/// this function got wrong. Zero length and a null data pointer are checked
/// separately from the payload rules, and both transports answer them with
/// `TLM_GENERIC_ERROR_RESPONSE`
/// (`chip_b_transport`, `noc_b_transport`). Folding them into the burst case
/// here — on the reasoning that a payload naming no bytes is a shape problem —
/// swapped one parity break for another: the *same* malformed payload came back
/// as a burst error through `transport_dbg` and a generic error through
/// `b_transport`, which is exactly the "status depends on which API you used"
/// the fix above exists to remove.
///
/// So this mirrors the normal path's taxonomy in full rather than reasoning
/// independently about what each condition ought to mean.
/// What a downstream debug target's silence means.
///
/// `transport_dbg` has no obligation to set a status — many targets just return
/// a byte count — so when one comes back `TLM_INCOMPLETE_RESPONSE` the endpoint
/// has to say what happened. It used to answer anything short of a full serve
/// with `TLM_GENERIC_ERROR_RESPONSE`, and `INTERFACE_CONTRACT.md` §1 reserves
/// that for "a target that decoded the access and refused it". A downstream
/// fabric with **no mapped target** decoded nothing, and the same address
/// through `b_transport` comes back `TLM_ADDRESS_ERROR_RESPONSE`.
///
/// The byte count is the only evidence available, and it is enough for the
/// distinction §1 actually draws:
///
///  * **nothing served** — no target claimed the address. That is a decode
///    miss, and decode misses are address errors;
///  * **part served** — something decoded it and stopped partway, which is a
///    refusal by a target that did decode.
///
/// A target that decodes an access and refuses all of it is the one case this
/// cannot separate from a decode miss, and it is the case §1 tells that target
/// to set its own status for. Guessing wrong for a target that stayed silent
/// when the contract told it to speak is the right way round.
tlm::tlm_response_status debug_outcome_status(unsigned int served,
                                              std::uint64_t requested)
{
    if (served == requested) {
        return tlm::TLM_OK_RESPONSE;
    }
    return served == 0 ? tlm::TLM_ADDRESS_ERROR_RESPONSE
                       : tlm::TLM_GENERIC_ERROR_RESPONSE;
}

tlm::tlm_response_status debug_refusal_status(
    const tlm::tlm_generic_payload& trans)
{
    switch (common_payload_error(trans)) {
    case payload_rule_error::command:
        return tlm::TLM_COMMAND_ERROR_RESPONSE;
    case payload_rule_error::burst:
        return tlm::TLM_BURST_ERROR_RESPONSE;
    case payload_rule_error::none:
        break;
    }
    // Reached only for the length and pointer checks the callers fold into the
    // same branch, which is what the normal path calls generic.
    return tlm::TLM_GENERIC_ERROR_RESPONSE;
}

} // namespace

unsigned int chip_noc_endpoint::chip_transport_dbg(
    tlm::tlm_generic_payload& trans)
{
    // No chunking, no counters, and no relaxation of the routing rules: a
    // debug access to this chip's own aperture is still the chip fabric's job
    // (`INTERFACE_CONTRACT.md` §8 relaxes timing, not rules).
    // A zero-length payload names no bytes. `common_payload_error()` checks the
    // command, the streaming width and the byte-enable shape and says nothing
    // about the length, so it is refused here rather than forwarded into a
    // downstream debug target that may act on it.
    trans.set_dmi_allowed(false);
    if (common_payload_error(trans) != payload_rule_error::none
        || trans.get_data_length() == 0 || trans.get_data_ptr() == nullptr) {
        return refuse_debug(trans, debug_refusal_status(trans));
    }
    if (overlaps_aperture(trans.get_address(), trans.get_data_length())) {
        return refuse_debug(trans, tlm::TLM_ADDRESS_ERROR_RESPONSE);
    }
    if (!span_fits(decode_region(trans.get_address()), trans.get_address(),
                   trans.get_data_length())) {
        return refuse_debug(trans, tlm::TLM_ADDRESS_ERROR_RESPONSE);
    }
    // A downstream debug target may leave the status alone — the stub in this
    // component's own gate does — so the endpoint states the outcome rather
    // than passing an untouched payload back to its caller.
    trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
    const unsigned int served = to_noc->transport_dbg(trans);
    if (trans.get_response_status() == tlm::TLM_INCOMPLETE_RESPONSE) {
        trans.set_response_status(
            debug_outcome_status(served, trans.get_data_length()));
    }
    trans.set_dmi_allowed(false);
    return served;
}

unsigned int chip_noc_endpoint::noc_transport_dbg(
    tlm::tlm_generic_payload& trans)
{
    trans.set_dmi_allowed(false);
    if (common_payload_error(trans) != payload_rule_error::none
        || trans.get_data_length() == 0 || trans.get_data_ptr() == nullptr) {
        return refuse_debug(trans, debug_refusal_status(trans));
    }
    // Relative on the way in, absolute inside the chip, restored on the way
    // out -- the same convention `b_transport` uses and the same one
    // `noc_interconnect::transport_dbg` applies to its own targets.
    const std::uint64_t relative = trans.get_address();
    const std::uint64_t length = trans.get_data_length();
    if (relative >= aperture_size() || length > aperture_size() - relative) {
        return refuse_debug(trans, tlm::TLM_ADDRESS_ERROR_RESPONSE);
    }
    trans.set_address(aperture_base() + relative);
    trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
    const unsigned int served = to_chip->transport_dbg(trans);
    trans.set_address(relative);
    if (trans.get_response_status() == tlm::TLM_INCOMPLETE_RESPONSE) {
        trans.set_response_status(debug_outcome_status(served, length));
    }
    trans.set_dmi_allowed(false);
    return served;
}

void chip_noc_endpoint::reset()
{
    ++generation_;
    // Wake a transfer that is waiting out an incoming quantum. It re-checks the
    // generation, hands back the unelapsed remainder and abandons; without this
    // it would sleep out the whole delay and only then notice.
    reset_event_.notify(sc_core::SC_ZERO_TIME);

    outbound_transfers_ = 0;
    outbound_chunks_ = 0;
    outbound_bytes_ = 0;
    outbound_split_transfers_ = 0;
    outbound_partial_failures_ = 0;
    outbound_target_errors_ = 0;
    outbound_local_refused_ = 0;
    outbound_span_refused_ = 0;

    inbound_transfers_ = 0;
    inbound_chunks_ = 0;
    inbound_bytes_ = 0;
    inbound_partial_failures_ = 0;
    inbound_target_errors_ = 0;
    inbound_foreign_refused_ = 0;

    protocol_errors_ = 0;
    last_partial_bytes_ = 0;
    last_inbound_partial_bytes_ = 0;

    // The peaks belong to the epoch and are restarted; `*_in_flight_` is not,
    // for the reason `in_flight_guard` records.
    //
    // **Restarted from the live count, not from zero.** A reset can land while
    // a transport is blocked downstream, and those calls are still in flight —
    // that is precisely why the live counters survive. Zeroing the peaks under
    // them published `peak < live`, a state the definition of a high-water mark
    // makes impossible, and it stayed wrong after the old calls drained: the
    // new epoch reported a peak of zero even though it had begun with transfers
    // already inside. The peak of an epoch that starts with `n` calls in flight
    // is at least `n`.
    peak_outbound_in_flight_ = outbound_in_flight_;
    peak_inbound_in_flight_ = inbound_in_flight_;

    outbound_latency_total_ = sc_core::SC_ZERO_TIME;
    outbound_latency_max_ = sc_core::SC_ZERO_TIME;
    outbound_latency_samples_ = 0;
    inbound_latency_total_ = sc_core::SC_ZERO_TIME;
    inbound_latency_max_ = sc_core::SC_ZERO_TIME;
    inbound_latency_samples_ = 0;
}

std::string chip_noc_endpoint::report() const
{
    std::ostringstream out;
    out << "chip_noc_endpoint[" << name() << "]\n"
        << "  chip " << config_.chip << ", aperture 0x" << std::hex
        << aperture_base() << " +0x" << aperture_size() << std::dec << '\n'
        << "  frame limit " << config_.max_frame_bytes << " B on a "
        << config_.bus_bytes << " B bus; inbound SRAM split at "
        << config_.max_inbound_sram_bytes << " B\n"
        << "  outbound: " << outbound_transfers_ << " transfers -> "
        << outbound_chunks_ << " chunks, " << outbound_bytes_ << " bytes ("
        << outbound_split_transfers_ << " split, "
        << outbound_partial_failures_ << " partial failures, "
        << outbound_target_errors_ << " target errors, "
        << outbound_local_refused_ << " local refused, "
        << outbound_span_refused_ << " multi-region refused)\n"
        // Symmetrical on purpose. The inbound line used to print transfers,
        // chunks, bytes and foreign refusals and nothing about work that
        // failed, so an inbound failure reached no report at all -- reachable
        // only through an accessor, from a test. The `last_*` values stay
        // accessor-only because they are "most recent" rather than totals; the
        // counts belong here.
        << "  inbound : " << inbound_transfers_ << " transfers -> "
        << inbound_chunks_ << " chunks, " << inbound_bytes_ << " bytes ("
        << inbound_partial_failures_ << " partial failures, "
        << inbound_target_errors_ << " target errors, "
        << inbound_foreign_refused_ << " foreign refused)\n"
        << "  in flight: outbound peak " << peak_outbound_in_flight_
        << ", inbound peak " << peak_inbound_in_flight_
        << " (transfers, not the interconnect's per-port transactions)\n"
        << "  latency : outbound " << outbound_latency_total_.to_string()
        << " over " << outbound_latency_samples_ << " transfers, max "
        << outbound_latency_max_.to_string() << "; inbound "
        << inbound_latency_total_.to_string() << " over "
        << inbound_latency_samples_ << " transfers, max "
        << inbound_latency_max_.to_string() << '\n'
        << "  latency and in-flight are per **transfer** and are not"
           " comparable with noc_interconnect::last_latency_cycles(port) or"
           " its per-port outstanding count, which are per transaction. One"
           " transfer is one thing the chip asked for and may be several"
           " transactions (decision record D24, corrected)\n"
        << "  protocol errors " << protocol_errors_
        << "; a target error is a refusal by the target, a refusal is one this"
           " endpoint made -- different events, different owners\n"
        << "  chunk counts are this component's split, never a transfer count;"
           " no counter here is per-core -- a chip is one aggregated manager"
           " (plan §4.4)\n";
    return out.str();
}

} // namespace cdc::components::tpu_v3::noc
