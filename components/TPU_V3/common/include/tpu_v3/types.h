// SPDX-License-Identifier: Apache-2.0
//
// Architectural identity and enumeration types shared by every TPU_V3
// component. Deliberately free of SystemC and TLM: a configuration object and
// an address calculation must be testable, and reusable, without elaborating a
// simulation.

#pragma once

#include <cstdint>
#include <string>

namespace cdc::components::tpu_v3 {

/// Linear chip index within the SoC, `0 .. max_chips - 1`.
using chip_id_t = std::uint32_t;

/// Core index within a chip. Exactly two cores per chip are frozen, so this is
/// always 0 or 1.
using core_id_t = std::uint32_t;

/// MXU index within a core. Exactly two MXUs per core are frozen.
using mxu_id_t = std::uint32_t;

/// Architectural RISC-V hart id, visible to firmware through `mhartid`.
using hart_id_t = std::uint32_t;

/// Frozen counts (plan §4.1, §4.3). A component that finds a different value
/// in its configuration must refuse to construct, not adapt.
inline constexpr unsigned cores_per_chip = 2;
inline constexpr unsigned mxus_per_core = 2;
inline constexpr unsigned mxu_rows = 128;
inline constexpr unsigned mxu_columns = 128;

/// Upper bound on chips in this revision.
///
/// This is not an address-map choice. The frozen FlooNoC chimney manager id is
/// three bits, so `cdc::components::noc_interconnect` accepts at most eight
/// upstream initiators, and plan §4.4 gives each chip exactly one aggregated
/// initiator. Raising it is a NoC protocol change with its own cross-checks —
/// see `docs/TPU_V3_PHASE0_AUDIT.md` §5.2.
inline constexpr unsigned max_chips = 8;

/// `mhartid = chip_linear_id * 2 + core_id` (plan §11.6).
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

/// Which MXU implementation a core instantiates. Selectable by CMake and by
/// configuration; it must never change a firmware-visible architectural
/// result, only timing and reported fidelity (plan §14).
enum class mxu_backend {
    /// Functional analytical 128x128 model. The full-system default.
    fast,
    /// Sauria-derived detailed model. Scope is decided in Phase 9; until then
    /// selecting it is a configuration error, not a silent fallback.
    sauria,
};

/// MXU numeric contract (decision record D6).
///
/// This is an architectural property, not a backend detail: changing it
/// changes the results, so it is named in the configuration, in the report and
/// in the package manifest rather than being implied by whichever kernel got
/// compiled.
enum class mxu_arithmetic {
    /// The TPU_V3 reference path: BF16 operands, IEEE FP32 accumulation, with
    /// a fixed accumulation order so results are deterministic across hosts.
    /// Follows the public Cloud TPU description of TPU matrix multiplication.
    bf16_fp32,
    /// Optional quantized extension: INT8 operands, INT32 accumulation. It
    /// must be selected explicitly and never replaces the reference path.
    /// Not implemented before Phase 4.
    int8_int32,
};

/// Which NoC timing backend the platform asks `noc_interconnect` for.
enum class noc_timing {
    /// Calibrated no-contention estimate. Long runs, firmware work.
    fast,
    /// Cycle-stepped signed network. Contention and routing studies only.
    detailed,
};

const char* to_string(mxu_backend backend) noexcept;
const char* to_string(mxu_arithmetic arithmetic) noexcept;
const char* to_string(noc_timing timing) noexcept;

/// Parse a backend/timing name. Throws `std::invalid_argument` naming the
/// accepted values; there is no default-on-unknown, because a typo that
/// silently selects the fast backend would invalidate every number a detailed
/// run produced.
mxu_backend mxu_backend_from_string(const std::string& text);
mxu_arithmetic mxu_arithmetic_from_string(const std::string& text);
noc_timing noc_timing_from_string(const std::string& text);

} // namespace cdc::components::tpu_v3
