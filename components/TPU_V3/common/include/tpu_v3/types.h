// SPDX-License-Identifier: Apache-2.0
//
// Architectural identity and enumeration types shared by every TPU_V3
// component. Deliberately free of SystemC and TLM: a configuration object and
// an address calculation must be testable, and reusable, without elaborating a
// simulation.
//
// Rebaselined by decision records D14 (NEO-CORE composition) and D15
// (control/local-data/external interconnect split). The Phase 1 vocabulary —
// one SVM and two 128x128 MXUs per core — is gone from here; it survives only
// in the Phase 0–2 audit documents, which are historical evidence and are not
// rewritten.

#pragma once

#include <cstdint>
#include <string>

namespace cdc::components::tpu_v3 {

/// Linear chip index within the SoC, `0 .. max_chips - 1`.
using chip_id_t = std::uint32_t;

/// Core index within a chip. Exactly two NEO-COREs per chip are frozen, so
/// this is always 0 or 1.
using core_id_t = std::uint32_t;

/// Architectural RISC-V hart id, visible to firmware through `mhartid`.
using hart_id_t = std::uint32_t;

/// Frozen counts (plan §4.1, decision record D14). A component that finds a
/// different value in its configuration must refuse to construct, not adapt.
inline constexpr unsigned cores_per_chip = 2;
inline constexpr unsigned sa_per_core = 1;
inline constexpr unsigned dma_per_core = 1;
inline constexpr unsigned transform_per_core = 1;

/// Sauria matrix-engine geometry. Two named pairs exist and no others: the
/// verified v4.2 bring-up array, and the architectural destination that only
/// the NPU team's promotion gate may unlock (D14).
inline constexpr unsigned sa_bringup_rows = 64;
inline constexpr unsigned sa_bringup_columns = 64;
inline constexpr unsigned sa_target_rows = 128;
inline constexpr unsigned sa_target_columns = 128;

/// Upper bound on chips in this revision.
///
/// This is not an address-map choice. The frozen FlooNoC chimney manager id is
/// three bits, so `cdc::components::noc_interconnect` accepts at most eight
/// upstream initiators, and plan §4.4 gives each chip exactly one aggregated
/// initiator. Raising it is a NoC protocol change with its own cross-checks —
/// see `docs/TPU_V3_PHASE0_AUDIT.md` §5.2.
inline constexpr unsigned max_chips = 8;

/// `mhartid = chip_linear_id * 2 + core_id` (plan §11.8).
constexpr hart_id_t hart_id_of(chip_id_t chip, core_id_t core) noexcept
{
    return static_cast<hart_id_t>(chip * cores_per_chip + core);
}

constexpr chip_id_t chip_of_hart(hart_id_t hart) noexcept
{
    return static_cast<chip_id_t>(hart / cores_per_chip);
}

constexpr core_id_t core_of_hart(hart_id_t hart) noexcept
{
    return static_cast<core_id_t>(hart % cores_per_chip);
}

/// Matrix-engine numeric contract (decision record D6).
///
/// This is an architectural property, not a backend detail: changing it
/// changes the results, so it is named in the configuration, in the report and
/// in the package manifest rather than being implied by whichever kernel got
/// compiled. A bring-up run on a datatype the v4.2 source happens to support
/// must report *that* datatype and must never be presented as the reference.
enum class matrix_datatype {
    /// The TPU_V3 reference path: BF16 operands, IEEE FP32 accumulation, with
    /// a fixed accumulation order so results are deterministic across hosts.
    /// Follows the public Cloud TPU description of TPU matrix multiplication.
    bf16_fp32,
    /// A datatype the verified v4.2 bring-up configuration supports. Useful
    /// integration evidence only; it is not BF16 equivalence (D6, D14).
    fp16_fp32,
    /// Optional quantized extension: INT8 operands, INT32 accumulation. It
    /// must be selected explicitly and never replaces the reference path.
    int8_int32,
};

/// How a logical core-SRAM address maps onto physical banks (D15).
///
/// Only one mapping exists today. It is an enumeration rather than an implied
/// constant because the mapping is a physical-implementation choice that the
/// SRAM macro and floorplan may change, and a report that does not name it is
/// not interpretable.
enum class bank_mapping {
    /// Bank index = (address / bytes_per_beat) % bank_count. Consecutive beats
    /// of one sequential burst land on consecutive banks.
    low_order_interleaved,
};

/// Per-bank arbitration policy on the native local-data plane (D15).
enum class arbitration_policy {
    /// Deterministic rotating priority. Deterministic is the requirement, not
    /// a preference: an arbiter whose outcome depends on host scheduling makes
    /// every contention measurement unreproducible.
    round_robin,
};

/// Which NoC timing backend the platform asks `noc_interconnect` for.
enum class noc_timing {
    /// Calibrated no-contention estimate. Long runs, firmware work.
    fast,
    /// Cycle-stepped signed network. Contention and routing studies only.
    detailed,
};

const char* to_string(matrix_datatype datatype) noexcept;

/// What this datatype's arithmetic actually is, and whether it is the TPU_V3
/// reference path.
///
/// It exists because the report used to print the datatype's *name* followed by
/// a hard-coded "(BF16 operands, IEEE FP32 accumulation)" — so an `fp16_fp32`
/// bring-up run printed a BF16 claim next to the word `fp16_fp32`, which is
/// precisely what D6 forbids ("do not claim BF16 equivalence from an FP16
/// bring-up run") and what D14 requires reports to get right. The note is
/// derived from the datatype so the two cannot disagree again.
const char* matrix_datatype_note(matrix_datatype datatype) noexcept;
const char* to_string(bank_mapping mapping) noexcept;
const char* to_string(arbitration_policy policy) noexcept;
const char* to_string(noc_timing timing) noexcept;

/// Parse an enumeration name. Throws `std::invalid_argument` naming the
/// accepted values; there is no default-on-unknown, because a typo that
/// silently selected another datatype or arbiter would invalidate every number
/// the run produced.
matrix_datatype matrix_datatype_from_string(const std::string& text);
bank_mapping bank_mapping_from_string(const std::string& text);
arbitration_policy arbitration_policy_from_string(const std::string& text);
noc_timing noc_timing_from_string(const std::string& text);

} // namespace cdc::components::tpu_v3
