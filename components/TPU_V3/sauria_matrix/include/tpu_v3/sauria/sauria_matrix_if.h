// SPDX-License-Identifier: Apache-2.0
//
// `sauria_matrix_if` — the geometry-aware matrix-engine interface (plan §11.4).
//
// Plan §11.4 says to "define a geometry-aware `sauria_matrix_if` before
// extracting source", and that instruction is worth taking literally. An
// interface written after the adapter is a description of whatever the adapter
// turned out to do; written before, it is a constraint the adapter has to meet.
//
// ## "Geometry-aware" is not "geometry-parameterised"
//
// The engine *reports* its geometry; nothing in the control flow depends on it.
// `job` carries M, N and K — the problem's shape — and never a tile size, a row
// count or a 64. That is what lets the 128x128 promotion happen without
// changing firmware-visible semantics, which §11.4 requires explicitly.
//
// The distinction shows up in one place and it is the important one: a caller
// asks for `C[M x N] = A[M x K] * B[K x N]` and the engine decides how many
// passes of its array that takes. A caller that had to know 64 would have to be
// rewritten at promotion, and every stored workload with it.
//
// ## What this interface deliberately does not offer
//
// * **No backing pointer.** Operands are named by core-SRAM address and read
//   through the native local-SRAM port. `core_sram` exposes no `data()` and
//   this interface exposes no way to ask for one, which is what makes "no
//   accelerator bypasses arbitration" structural (INTERFACE_CONTRACT.md).
// * **No accumulation and no C preload.** Decision record D17 refuses both for
//   the whole of Phase 5: writeback is not atomic, so accumulating into a
//   region a failed job partially wrote has no settled meaning, and an engine
//   that silently accumulated into torn data would be the worst answer to that.
//   `capability_bit::accumulate` is clear and `start()` refuses the request.
// * **No synchronous result.** `start()` enqueues and returns; completion is
//   asynchronous and observed through status or the level-sensitive interrupt.
//   Plan §11.4 requires exactly this — MMIO start must not block the control
//   plane while a matrix multiply runs.

#pragma once

#include <cstdint>
#include <string>

#include "tpu_v3/sauria/sa_registers.h"

namespace cdc::components::tpu_v3::sauria {

/// What the caller wants computed: `C[M x N] = A[M x K] * B[K x N]`.
///
/// Strides are in bytes and zero means "tightly packed", so a caller laying out
/// a padded tile does not have to know the element size and a caller that is
/// not padding does not have to compute one.
struct job {
    std::uint32_t m = 0;
    std::uint32_t n = 0;
    std::uint32_t k = 0;

    std::uint64_t a_address = 0;
    std::uint64_t b_address = 0;
    std::uint64_t c_address = 0;

    std::uint32_t a_stride_bytes = 0;
    std::uint32_t b_stride_bytes = 0;
    std::uint32_t c_stride_bytes = 0;

    std::uint32_t datatype = datatype_value::int8_int32;
};

/// What the engine is, reported rather than configured.
///
/// `rows`/`columns` are here so a report can name the array that produced a
/// result — D14 requires geometry, datatype and source revision in every report
/// and manifest — and for no other purpose. Nothing in `submit()` or the job
/// consults them.
struct engine_identity {
    std::uint32_t rows = 0;
    std::uint32_t columns = 0;
    /// Bitmask of `capability_bit`.
    std::uint32_t capability = 0;
    /// The pinned, patched source the engine was built from. Empty is a defect,
    /// not a default: a result whose source cannot be named is not reportable.
    std::string source_revision;
};

/// The outcome of *submitting* a job, which is not the outcome of running one.
///
/// `accepted` means the descriptor was legal and the engine started. Everything
/// else is a refusal made before any state changed, so a rejected job leaves C
/// untouched — unlike a job that fails partway, which is reported through
/// status and leaves C partially written (D17).
enum class submit_status : std::uint32_t {
    accepted = 0,
    busy,
    invalid_dimension,
    /// The problem is larger than one pass of the array. Distinct from
    /// `invalid_dimension` because it is not a malformed job: it is a legal
    /// GEMM this engine cannot yet tile, and a caller's response — split it —
    /// is different from the response to a zero dimension.
    dimension_exceeds_array,
    invalid_address,
    invalid_stride,
    region_overlap,
    datatype_unsupported,
    accumulation_unsupported,
    /// The job fits one array pass, but K makes A or B larger than this
    /// engine's explicitly instantiated private staging store.
    staging_capacity_exceeded,
    /// The hardware reset line is asserted, so the clocked modules this engine
    /// configures are being held in reset.
    ///
    /// A distinct status because it is not a malformed job and the caller's
    /// response is different: retry once reset deasserts. Refusing is what
    /// makes the outcome *defined* — a job admitted here would write its
    /// configuration into modules that cannot latch it, and then run with a
    /// configuration that was silently dropped, which reads as an arithmetic
    /// defect rather than a reset-timing one.
    engine_in_reset,
};

const char* to_string(submit_status status) noexcept;

/// The matrix engine, as everything above it sees it.
class sauria_matrix_if {
public:
    virtual ~sauria_matrix_if() = default;

    /// Enqueue `work` and return immediately.
    ///
    /// Never blocks and never computes on the caller's stack: plan §11.4 makes
    /// MMIO start an enqueue, because the control plane is shared and a matrix
    /// multiply is not a register write.
    virtual submit_status submit(const job& work) = 0;

    /// Abandon the running job. Data already written to C stays written; the
    /// committed byte count is what says how far it got (D17).
    virtual void abort() = 0;

    /// Abandon the current epoch. The NEO-CORE reset path calls this together
    /// with the local-fabric reset; bytes already committed to C stay written.
    virtual void reset() = 0;

    virtual bool busy() const = 0;

    /// Latched cause of the last failure, or `error_cause::none`.
    virtual error_cause last_error() const = 0;

    /// Bytes committed to C by the job that owns the count. Non-atomic
    /// writeback makes this the only account of a partial result.
    virtual std::uint64_t committed_bytes() const = 0;

    virtual engine_identity identity() const = 0;

    /// The D17 timing split, in nanoseconds of simulated time.
    ///
    /// Three terms rather than a total, because only `compute` is
    /// cycle-correlated with the Sauria source; the other two are this
    /// adapter's own traffic through a fabric the source never had. A single
    /// number would be quoted as though all of it meant something.
    struct timing {
        std::uint64_t prefetch_ns = 0;
        std::uint64_t compute_ns = 0;
        std::uint64_t writeback_ns = 0;
    };
    virtual timing last_timing() const = 0;

    /// Native-port traffic in the current reset epoch.
    virtual std::uint64_t local_requests() const = 0;
    virtual std::uint64_t local_bytes() const = 0;
};

} // namespace cdc::components::tpu_v3::sauria
