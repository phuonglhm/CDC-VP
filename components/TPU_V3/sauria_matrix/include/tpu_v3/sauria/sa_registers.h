// SPDX-License-Identifier: Apache-2.0
//
// The frozen `SA_CONTROL` programming model (plan §11.4).
//
// A 32-bit AXI4-Lite target at `address_map::sa_control_offset` (0x0101_0000),
// 64 KiB. Bulk operand and result traffic never touches this plane: it goes
// through the native local-SRAM port, which is what keeps a 64-byte accelerator
// payload off the control fabric (D15).
//
// ## Frozen means firmware-visible
//
// Plan §11.4 requires the interface to stay geometry-independent so the 128x128
// promotion does not change firmware-visible control semantics. Nothing here
// mentions 64, and nothing should: `M`, `N` and `K` are the problem's shape and
// the engine's geometry is the implementation's. A register that encoded 64
// would have to change at promotion, which is exactly what the plan forbids.
//
// The layout deliberately mirrors `dma/dma_registers.h`: same identity/version
// at 0x000, same W1S control, same W1C status, same counter block. Two
// accelerators on one control plane that disagree about how to acknowledge
// completion is a needless thing for firmware to carry.

#pragma once

#include <cstdint>

namespace cdc::components::tpu_v3::sauria {

namespace reg {

inline constexpr std::uint64_t id = 0x000;            ///< RO identity
inline constexpr std::uint64_t version = 0x004;       ///< RO model revision
inline constexpr std::uint64_t control = 0x008;       ///< W1S START / ABORT
inline constexpr std::uint64_t status = 0x00C;        ///< RO BUSY, W1C rest

// ── the problem shape ────────────────────────────────────────────────────────
//
// C[M x N] = A[M x K] * B[K x N]. Written while idle; a write while `BUSY` is
// refused rather than latched, because a job whose shape changed halfway is not
// a job anyone can reason about.
inline constexpr std::uint64_t dim_m = 0x010;         ///< RW while idle
inline constexpr std::uint64_t dim_n = 0x014;         ///< RW while idle
inline constexpr std::uint64_t dim_k = 0x018;         ///< RW while idle

// ── operand and result placement ─────────────────────────────────────────────
//
// Core-SRAM addresses. 64-bit because `address_map` is, even though a NEO-CORE
// SRAM window is 16 MiB: an accelerator that silently truncated an address
// would be a defect that only appears once the map grows.
inline constexpr std::uint64_t a_addr_lo = 0x020;     ///< RW while idle
inline constexpr std::uint64_t a_addr_hi = 0x024;
inline constexpr std::uint64_t b_addr_lo = 0x028;
inline constexpr std::uint64_t b_addr_hi = 0x02C;
inline constexpr std::uint64_t c_addr_lo = 0x030;
inline constexpr std::uint64_t c_addr_hi = 0x034;

/// Row strides in **bytes**, so firmware need not know the element size to lay
/// out a padded tile. Zero means "tightly packed": the engine computes the
/// natural stride from the dimension and the datatype's element size.
inline constexpr std::uint64_t a_stride = 0x038;      ///< RW while idle
inline constexpr std::uint64_t b_stride = 0x03C;
inline constexpr std::uint64_t c_stride = 0x040;

// ── arithmetic ───────────────────────────────────────────────────────────────
//
// Selected explicitly, never defaulted. D6 permits INT8/INT32 only as an opt-in
// quantized extension, and a register that defaulted to it would make the
// reference path something a caller opts *out* of.
inline constexpr std::uint64_t datatype = 0x044;      ///< RW while idle

inline constexpr std::uint64_t irq_enable = 0x048;    ///< RW
inline constexpr std::uint64_t error_cause = 0x04C;   ///< RO latched

// ── counters ─────────────────────────────────────────────────────────────────
// Event counters are lifetime totals, retain their value across reset, and
// expose the low 32 bits. Native traffic counters instead describe the current
// reset epoch and are cleared by reset; this is what lets them reconcile with
// `neo_local_sram_fabric`, whose reset also starts a new epoch.
inline constexpr std::uint64_t job_count = 0x050;     ///< RO accepted jobs
inline constexpr std::uint64_t error_count = 0x054;   ///< RO
inline constexpr std::uint64_t abort_count = 0x058;   ///< RO

/// Bytes the engine has committed to the C region for the job that owns this
/// register. Non-atomic writeback means a failed job leaves C partially
/// updated, and this is the only thing that says how far it got (D17). ABORT or
/// active reset releases BUSY immediately but does not revoke that ownership: a
/// native write already accepted by the fabric may return later, so this count
/// can still rise until the next START claims it. Clearing ABORTED by W1C does
/// not freeze the count.
inline constexpr std::uint64_t c_bytes_done_lo = 0x05C;  ///< RO
inline constexpr std::uint64_t c_bytes_done_hi = 0x060;

/// The D17 timing split. Reported separately because only the middle term is
/// cycle-correlated with the Sauria source, and a single total would invite
/// exactly the claim D17 forbids. Each 32-bit nanosecond register saturates at
/// UINT32_MAX rather than wrapping; a saturated value is a lower bound.
inline constexpr std::uint64_t prefetch_ns = 0x064;   ///< RO
inline constexpr std::uint64_t compute_ns = 0x068;    ///< RO, source-correlated
inline constexpr std::uint64_t writeback_ns = 0x06C;  ///< RO

/// Native local-SRAM traffic, for reconciling against the fabric's own totals
/// the way Phase 4 reconciles the DMA's.
inline constexpr std::uint64_t local_requests = 0x070;  ///< RO
inline constexpr std::uint64_t local_bytes_lo = 0x074;  ///< RO
inline constexpr std::uint64_t local_bytes_hi = 0x078;
inline constexpr std::uint64_t overrun_count = 0x07C; ///< RO rejected STARTs

// ── engine identity, so a result can name the machine that produced it ───────
//
// Read-only and geometry-*reporting* rather than geometry-parameterised: these
// tell firmware what it is talking to without any control semantics depending
// on the answer.
inline constexpr std::uint64_t geometry = 0x080;      ///< RO rows<<16 | columns
inline constexpr std::uint64_t capability = 0x084;    ///< RO supported datatypes

/// One past the last implemented register. Anything from here to the end of the
/// 64 KiB window reads zero and ignores writes, like the DMA's reserved space.
inline constexpr std::uint64_t implemented_end = 0x088;

} // namespace reg

/// `'S','A','3','\0'`-ish: 0x54503353 spells "TP3S" little-endian, matching the
/// DMA's 0x54503304 family so a probe can tell TPU_V3 blocks apart from
/// whatever else is on the plane.
inline constexpr std::uint32_t identity_value = 0x54503353;

/// Bumped when the programming model changes in a way firmware can observe.
inline constexpr std::uint32_t model_version = 1;

namespace control_bit {
/// Latch the configured job and begin. Ignored while `BUSY`, and that is
/// reported as an overrun rather than silently queued.
inline constexpr std::uint32_t start = 1u << 0;
/// Abandon the running job. Clears `BUSY`, sets `ABORTED`, raises no interrupt.
/// Data already written to C stays written (D17).
inline constexpr std::uint32_t abort = 1u << 1;
inline constexpr std::uint32_t writable_mask = start | abort;
} // namespace control_bit

namespace status_bit {
inline constexpr std::uint32_t busy = 1u << 0;    ///< RO
inline constexpr std::uint32_t done = 1u << 1;    ///< W1C
inline constexpr std::uint32_t error = 1u << 2;   ///< W1C
inline constexpr std::uint32_t aborted = 1u << 3; ///< W1C
inline constexpr std::uint32_t w1c_mask = done | error | aborted;
} // namespace status_bit

namespace irq_enable_bit {
/// One level-sensitive completion interrupt, raised for `DONE` and for `ERROR`
/// alike and held until the status bit is acknowledged. An abort raises
/// nothing: firmware asked for it and already knows.
inline constexpr std::uint32_t completion = 1u << 0;
inline constexpr std::uint32_t writable_mask = completion;
} // namespace irq_enable_bit

/// Datatype selector values for `reg::datatype`.
///
/// The encoding is shared with `matrix_datatype` in `tpu_v3/types.h` by value,
/// not by include: this header is the firmware-visible ABI and must not depend
/// on a C++ enum's declaration order, which is free to change.
namespace datatype_value {
inline constexpr std::uint32_t bf16_fp32 = 0;
inline constexpr std::uint32_t fp16_fp32 = 1;
inline constexpr std::uint32_t int8_int32 = 2;
} // namespace datatype_value

/// Bits in `reg::capability`: which datatypes *this* engine can actually run.
///
/// It exists because the pinned v4.2 source has no BF16 profile in any
/// geometry, so the D6 reference path has no implementation behind it today.
/// Firmware must be able to discover that rather than select `bf16_fp32` and
/// receive a refusal it cannot distinguish from a bug.
namespace capability_bit {
inline constexpr std::uint32_t bf16_fp32 = 1u << 0;
inline constexpr std::uint32_t fp16_fp32 = 1u << 1;
inline constexpr std::uint32_t int8_int32 = 1u << 2;
/// Set when the engine can accumulate into C rather than overwrite it.
/// **Clear for the whole of Phase 5** — D17 refuses accumulation and C-preload
/// until the semantics of accumulating into a partially written region are
/// settled.
inline constexpr std::uint32_t accumulate = 1u << 8;
} // namespace capability_bit

/// Latched in `reg::error_cause`. Zero means no error.
///
/// Distinct causes rather than one "bad job" code: each of these sends whoever
/// reads it somewhere different, and a firmware author cannot guess which.
enum class error_cause : std::uint32_t {
    none = 0,
    /// `M`, `N` or `K` is zero.
    zero_dimension = 1,
    /// A dimension exceeds what the engine can sequence.
    dimension_too_large = 2,
    /// An operand or result region leaves the core-SRAM window.
    address_out_of_range = 3,
    /// A stride smaller than the row it describes, so rows would overlap.
    stride_too_small = 4,
    /// A, B and C overlap in a way that makes the result order-dependent.
    region_overlap = 5,
    /// `reg::datatype` names a datatype this engine does not implement; read
    /// `reg::capability` for what it does.
    datatype_unsupported = 6,
    /// The native local-SRAM port refused an operand read.
    operand_read_failed = 7,
    /// The native local-SRAM port refused a result write.
    result_write_failed = 8,
    /// `START` while `BUSY`.
    overrun = 9,
    /// The kept Control FSM raised its feeder-deadlock output.
    ///
    /// A distinct cause because it is the engine's own diagnosis, not the
    /// sequencer's inference: `Control` has an `o_feed_deadlock` port and this is
    /// what it means. Firmware's response differs from a timeout's — the job
    /// description is suspect, not the time budget.
    engine_deadlock = 10,
    /// The engine did not report done inside the sequencer's cycle bound.
    ///
    /// Separate from `engine_deadlock`: this one says only that the bound was
    /// reached, which can also mean the bound is too small for the problem.
    timeout = 11,
    /// The job was abandoned — `abort()`, or reset arriving mid-flight.
    ///
    /// Not a failure of the job description. `committed_bytes()` is the account
    /// of how much of C was written before it stopped (D17).
    aborted = 12,
};

} // namespace cdc::components::tpu_v3::sauria
