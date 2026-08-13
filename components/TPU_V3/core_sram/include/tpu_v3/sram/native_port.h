// SPDX-License-Identifier: Apache-2.0
//
// The native local-data interface of a NEO-CORE (decision record D15).
//
// D15 is explicit that the local data plane is **not** AXI and must not be
// implemented or described as a full AXI data crossbar. It is a small
// RTL-realizable request/response interface into physically banked core SRAM:
//
//   request   valid / ready, requester identity, absolute byte address,
//             read-or-write, transfer size, write data, write strobes
//   response  valid / ready, read data, explicit error status
//
// The types below are that interface at transaction level. The signal names an
// RTL implementation uses may follow the RTL coding standard, but dropping
// ready/back-pressure or error propagation is not a legal simplification, and
// neither is replacing the status with "returns zero on failure".
//
// Requester identity is carried in the request rather than being implicit in a
// dedicated port. D15 permits either and requires it to be explicit after
// arbitration; carrying it makes every counter attributable by construction
// and makes an unregistered requester a detectable error rather than a silent
// one.

#pragma once

#include <cstdint>

#include <systemc>

namespace cdc::components::tpu_v3::sram {

/// The named requesters on one NEO-CORE's local data plane (D15).
///
/// This list is closed on purpose. "Whoever happens to call" is not an
/// ownership model: every counter, every arbitration decision and every
/// response is attributed to one of these, and a fabric that accepted an
/// unnamed requester would produce metrics nothing could be traced back to.
enum class neo_requester : unsigned {
    /// VP++ local load/store and instruction fetch that lands in core SRAM.
    cpu = 0,
    /// The independent NEO DMA's local port (Phase 4).
    dma = 1,
    /// The Sauria matrix engine's operand/result path (Phase 5).
    sa = 2,
    /// The ImageTransform engine's tensor port (Phase 6).
    transform = 3,
    /// Authorized inbound chip/NoC traffic, arriving through the external
    /// bridge. It is a normal requester and is arbitrated like one; the whole
    /// point of naming it is that remote traffic must not bypass arbitration.
    external_inbound = 4,
};

inline constexpr unsigned neo_requester_count = 5;

constexpr unsigned index_of(neo_requester requester) noexcept
{
    return static_cast<unsigned>(requester);
}

const char* to_string(neo_requester requester) noexcept;

enum class neo_command {
    read,
    write,
};

const char* to_string(neo_command command) noexcept;

/// Explicit response status. There is no "returns zero on failure" state:
/// `INTERFACE_CONTRACT.md` §1 forbids converting an error into zero data, and
/// a status enumeration is what makes that rule checkable.
enum class neo_status {
    ok,
    /// Outside the decoded core SRAM window entirely.
    decode_error,
    /// Inside the window but above the instantiated capacity. A distinct
    /// status from `decode_error` because the two mean different things to a
    /// driver: one is a wrong pointer, the other is a configuration too small
    /// for the workload — and D6 forbids aliasing it down into valid storage.
    capacity_error,
    /// Zero-length, or larger than the 64-byte maximum a single access can
    /// need (one RVV register at VLEN=512).
    size_error,
    /// A reset arrived while this request was waiting for, or holding, a bank.
    ///
    /// Reset abandons in-flight work (`ARCHITECTURE.md` §6), and a requester
    /// blocked in a call has to be told so rather than left waiting for a
    /// grant the arbiter will never issue. Any beats that completed before the
    /// reset stay completed and are reported in `bytes`.
    aborted,
};

const char* to_string(neo_status status) noexcept;

/// The largest payload any single access can need: one RVV register at
/// VLEN=512. A target must accept any size from 1 to this, and — decision
/// record D7 — must never *require* the largest, because VP++ decomposes
/// vector accesses per active element before a fabric ever sees them.
inline constexpr std::uint32_t neo_max_transfer_bytes = 64;

struct neo_local_request {
    neo_requester requester = neo_requester::cpu;
    neo_command command = neo_command::read;

    /// Absolute platform byte address. There is one address per resource
    /// (`ADDRESS_MAP.md` §4): the local core, the sibling core, a remote chip
    /// and the host loader all name core SRAM the same way, and only the
    /// *path* differs.
    std::uint64_t address = 0;

    /// 1 .. `neo_max_transfer_bytes`.
    std::uint32_t size = 0;

    /// Read destination or write source; `size` bytes.
    unsigned char* data = nullptr;

    /// Exactly `size` bytes, one per data byte, non-zero meaning enabled — or
    /// null for "all enabled". Arbitrary patterns including non-contiguous
    /// ones are legal on a memory-like target (`INTERFACE_CONTRACT.md` §2).
    ///
    /// **Exactly `size`, not TLM's repeating pattern.** TLM lets
    /// `byte_enable_length` be shorter than `data_length`, in which case the
    /// pattern repeats; `wstrb` in RTL has one bit per byte of the transfer
    /// and no such rule. Expanding the pattern is therefore the *adapter's*
    /// job, and an adapter that forwarded the raw TLM pointer would read past
    /// the end of the caller's array and apply whatever it found there.
    const unsigned char* strobes = nullptr;
};

/// Bytes `strobes` actually enables over `size` — `size` itself when there is
/// no strobe pattern.
///
/// This, not `size`, is what a "transferred bytes" counter must accumulate
/// (decision record D7 names its counters for what they measure). A masked
/// write moves fewer bytes than it names, and a counter that ignored the mask
/// would report bandwidth the model never carried.
inline std::uint32_t enabled_byte_count(const unsigned char* strobes,
                                        std::uint32_t size) noexcept
{
    if (strobes == nullptr) {
        return size;
    }
    std::uint32_t enabled = 0;
    for (std::uint32_t i = 0; i < size; ++i) {
        if (strobes[i] != 0) {
            ++enabled;
        }
    }
    return enabled;
}

struct neo_local_response {
    neo_status status = neo_status::ok;

    /// Bytes actually transferred. Decode/capacity/size errors transfer nothing
    /// and leave the caller's buffer untouched. `aborted` is the deliberate
    /// exception: reset may arrive after earlier beats completed, and those
    /// partial bytes stay transferred and are reported to the requester.
    std::uint32_t bytes = 0;

    /// Physical bank accesses this one logical request was split into.
    ///
    /// Kept separate from the request count on purpose (D7): if the fabric
    /// splits a payload across beats or banks it records one request and the
    /// actual number of beats, and neither number may be presented as the
    /// other. A beat count is not a hardware bus transaction count either.
    std::uint32_t beats = 0;

    /// Beats that found their bank occupied and had to wait for it.
    std::uint32_t bank_conflicts = 0;

    /// What the fabric charged this request, arbitration included.
    sc_core::sc_time latency = sc_core::SC_ZERO_TIME;
};

/// The native port, as a SystemC interface so a requester can bind to it.
///
/// `b_access` is blocking in the ready/valid sense: it returns when the
/// response is complete. Whether it also blocks in simulated time depends on
/// the fabric's timing mode — see `local_fabric_timing`.
class neo_local_sram_if : public virtual sc_core::sc_interface {
public:
    virtual void b_access(const neo_local_request& request,
                          neo_local_response& response,
                          sc_core::sc_time& delay) = 0;

    /// Host-side loading and test setup. Bypasses arbitration and latency but
    /// **not** decode or bounds checks, advances no simulated time, and
    /// updates no performance counter — a loader is not workload traffic, and
    /// counting it would corrupt every metric that follows
    /// (`INTERFACE_CONTRACT.md` §8). Returns bytes transferred, 0 on refusal.
    virtual std::uint32_t dbg_access(const neo_local_request& request) = 0;
};

} // namespace cdc::components::tpu_v3::sram
