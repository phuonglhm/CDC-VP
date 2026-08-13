// SPDX-License-Identifier: Apache-2.0
//
// The NEO DMA programming model (plan §11.5), as plain constants.
//
// Separated from the module so firmware, a test and the model itself all read
// the same offsets from one place, and so the register map is inspectable
// without elaborating SystemC. Phase 10 generates the firmware header from
// this; until then there is no second copy of these numbers anywhere.
//
// The layout is **frozen for Revision 1**. Unlisted offsets are reserved: they
// read as zero with `TLM_OK_RESPONSE` and drop writes, and they never alias an
// implemented register — an access at `DMA_CONTROL + 0x1000` answers zero
// rather than answering as offset zero.

#pragma once

#include <cstdint>

namespace cdc::components::tpu_v3::dma {

namespace reg {

inline constexpr std::uint64_t id = 0x000;              ///< RO identity
inline constexpr std::uint64_t version = 0x004;         ///< RO model revision
inline constexpr std::uint64_t control = 0x008;         ///< W1S START / ABORT
inline constexpr std::uint64_t status = 0x00C;          ///< RO BUSY, W1C rest
inline constexpr std::uint64_t src_addr_lo = 0x010;     ///< RW while idle
inline constexpr std::uint64_t src_addr_hi = 0x014;     ///< RW while idle
inline constexpr std::uint64_t dst_addr_lo = 0x018;     ///< RW while idle
inline constexpr std::uint64_t dst_addr_hi = 0x01C;     ///< RW while idle
inline constexpr std::uint64_t length = 0x020;          ///< RW while idle
inline constexpr std::uint64_t irq_enable = 0x024;      ///< RW
inline constexpr std::uint64_t error_cause = 0x028;     ///< RO latched
inline constexpr std::uint64_t bytes_done_lo = 0x02C;   ///< RO committed
inline constexpr std::uint64_t bytes_done_hi = 0x030;   ///< RO
inline constexpr std::uint64_t transfer_count = 0x034;  ///< RO accepted jobs
inline constexpr std::uint64_t error_count = 0x038;     ///< RO
inline constexpr std::uint64_t abort_count = 0x03C;     ///< RO
inline constexpr std::uint64_t overrun_count = 0x040;   ///< RO
inline constexpr std::uint64_t local_bytes_lo = 0x044;  ///< RO native bytes
inline constexpr std::uint64_t local_bytes_hi = 0x048;  ///< RO
inline constexpr std::uint64_t external_bytes_lo = 0x04C; ///< RO
inline constexpr std::uint64_t external_bytes_hi = 0x050; ///< RO

/// One past the last implemented register. Everything from here to the end of
/// the 64 KiB window is reserved.
inline constexpr std::uint64_t implemented_end = 0x054;

} // namespace reg

namespace control_bit {
/// Write-1-to-set. Snapshots the descriptor and starts the worker.
inline constexpr std::uint32_t start = 1u << 0;
/// Write-1-to-set. Abandons an active job without reporting completion.
inline constexpr std::uint32_t abort = 1u << 1;
inline constexpr std::uint32_t writable_mask = start | abort;
} // namespace control_bit

namespace status_bit {
/// Read-only. Set before the `START` write returns, cleared by the worker.
inline constexpr std::uint32_t busy = 1u << 0;
/// Write-1-to-clear. Successful completion.
inline constexpr std::uint32_t done = 1u << 1;
/// Write-1-to-clear. The job stopped on an error; `ERROR_CAUSE` says which.
inline constexpr std::uint32_t error = 1u << 2;
/// Write-1-to-clear, sticky. An explicit `ABORT` ended the job. Deliberately
/// **not** `DONE`: a job that was abandoned did not complete, and reporting it
/// as completion is the failure mode this bit exists to prevent.
inline constexpr std::uint32_t aborted = 1u << 3;
inline constexpr std::uint32_t w1c_mask = done | error | aborted;
} // namespace status_bit

namespace irq_enable_bit {
/// Enables the level interrupt for both successful completion and error. One
/// bit, not two: an errored job must wake firmware exactly as a completed one
/// does (`INTERFACE_CONTRACT.md` §4), so a configuration that could enable one
/// without the other would only invite a driver that never learns it failed.
inline constexpr std::uint32_t completion = 1u << 0;
inline constexpr std::uint32_t writable_mask = completion;
} // namespace irq_enable_bit

/// Latched first-error cause. The values are **stable for Revision 1**:
/// firmware and post-processing scripts compare against them, so a new
/// condition takes a new number rather than renumbering these.
enum class error_cause : std::uint32_t {
    none = 0,
    /// `LENGTH` was zero.
    invalid_length = 1,
    /// A span wrapped 64-bit arithmetic or ended above the 4 GiB RV32 limit.
    address_overflow = 2,
    /// Both endpoints local or both external. Revision 1 implements exactly
    /// local→external and external→local; the others are a later explicit
    /// revision, not behaviour to infer.
    unsupported_endpoints = 3,
    /// The source span crosses the core-SRAM boundary instead of lying wholly
    /// inside or wholly outside it.
    source_unmapped = 4,
    destination_unmapped = 5,
    /// A native local-SRAM read/write returned a non-`ok` status.
    local_read = 6,
    local_write = 7,
    /// An external TLM read/write returned a non-`OK` response.
    external_read = 8,
    external_write = 9,
    /// The model reached a state it does not define. Distinct from every guest
    /// -visible cause above so it can never be mistaken for one.
    internal = 10,
};

const char* to_string(error_cause cause) noexcept;

/// `"TP3"` and a block code of 4, matching the identity convention the Phase 3
/// register files use. A wrong-window decode is otherwise invisible: every
/// register file would answer and a driver would program the wrong engine.
inline constexpr std::uint32_t identity = 0x5450'3304u;

/// Programming-model revision, not the model's build. Phase 4 is the first.
inline constexpr std::uint32_t model_version = 0x0004'0000u;

// ── external frame limit ─────────────────────────────────────────────────────

/// The chip endpoint's bus width and burst limit (`INTERFACE_CONTRACT.md` §7).
inline constexpr std::uint64_t external_bus_bytes = 8;
inline constexpr std::uint64_t external_max_beats = 256;

/// Largest external payload legal at `address`.
///
/// The interconnect accepts a frame only while
/// `ceil((address % 8 + length) / 8) <= 256`, which rearranges to
/// `address % 8 + length <= 2048`. So 2048 bytes is reachable **only** at bus
/// alignment and the limit shrinks by one byte for every byte of lane offset —
/// a detail that is easy to state and easy to get wrong by assuming 2048
/// always fits.
constexpr std::uint64_t external_frame_limit(std::uint64_t address) noexcept
{
    const std::uint64_t lane = address % external_bus_bytes;
    return external_bus_bytes * external_max_beats - lane;
}

} // namespace cdc::components::tpu_v3::dma
