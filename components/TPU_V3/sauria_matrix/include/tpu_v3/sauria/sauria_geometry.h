// SPDX-License-Identifier: Apache-2.0
//
// The one place the Sauria array's template arguments are written down, and the
// compile-time proof that they are the profile's.
//
// ## The defect this exists to make impossible
//
// The array's geometry and datatypes are C++ template parameters with defaults:
//
// ```cpp
// template <int X_DIM = 32, int Y_DIM = 32, ...> class NpuTop;
// template <int X_DIM = 32, int Y_DIM = 64, ...> class SystolicArray;
// ```
//
// Two different defaults, neither of them the 64x64 Phase 5 requires. Nothing
// reads `sauria_targets.h` at run time. And INT8 is the model's default dtype,
// so it needs no `-D` flag — which means an adapter that simply forgot its
// template arguments would compile without a diagnostic, run a 32x64 INT8
// array, and produce plausible wrong numbers.
//
// Plan §16 asks for "a negative control that fails if an omitted parameter
// silently falls back to 32x32". A test cannot be that control on its own: by
// the time a test runs, the wrong array is already built, and a test that
// multiplies small matrices correctly on a 32x64 array passes. The control has
// to be at compile time, and it has to compare against something the build did
// not choose — which is what `sauria_profile.h` is, since its values are read
// out of the pinned source rather than written here.
//
// So: name the instantiation once, and assert every argument against the
// extracted profile. Getting a parameter wrong is then a compile error naming
// the parameter, not a wrong result.

#pragma once

#include <cstdint>

#include "tpu_v3/sauria/sauria_profile.h"

namespace cdc::components::tpu_v3::sauria {

// ── the element types, tied to the profile's widths ──────────────────────────
//
// Written as aliases and then checked, rather than derived from the widths by
// template metaprogramming. A `width_to_type<8>` would silently pick something
// for a width the profile changed to, and Phase 5's contract is that a changed
// width stops the build.

using activation_t = std::int8_t;
using weight_t = std::int8_t;
using accumulator_t = std::int32_t;

static_assert(activation_width_bits == 8 * static_cast<int>(sizeof(activation_t)),
              "the profile's activation width does not match activation_t; "
              "Phase 5 requires INT8 operands (plan §16)");
static_assert(weight_width_bits == 8 * static_cast<int>(sizeof(weight_t)),
              "the profile's weight width does not match weight_t; "
              "Phase 5 requires INT8 operands (plan §16)");
static_assert(accumulator_width_bits == 8 * static_cast<int>(sizeof(accumulator_t)),
              "the profile's accumulator width does not match accumulator_t; "
              "Phase 5 requires INT32 accumulation (plan §16)");
static_assert(operand_bytes == static_cast<int>(sizeof(activation_t)),
              "the profile's in-memory operand size disagrees with its "
              "element width");
static_assert(result_bytes == static_cast<int>(sizeof(accumulator_t)),
              "the profile's in-memory result size disagrees with its "
              "accumulator width");

// ── the geometry Phase 5 froze ───────────────────────────────────────────────
//
// Compared against literals as well as against the profile. The profile check
// catches "the source changed"; the literal check catches "the source changed
// *and* someone updated the recorded hash to match", which is the failure a
// hash alone cannot see.

static_assert(rows == 64,
              "Phase 5 is the 64x64 bring-up array. 128x128 is an NPU-team "
              "delivery behind a promotion gate that has not run (decision "
              "record D14), and 32x32 is the template default this build exists "
              "to refuse");
static_assert(columns == 64,
              "Phase 5 is the 64x64 bring-up array (decision record D14)");
static_assert(arithmetic_type == 0,
              "Phase 5 requires the integer PE arithmetic path");

// ── index widths ─────────────────────────────────────────────────────────────
//
// These reach the source as preprocessor definitions, so unlike the geometry
// they are not visible in a type. If the build forgot to pass them, the source
// compiles with its own defaults and nothing here would notice — so the adapter
// passes them *from these constants* and the source's macros are checked
// against them below.

static_assert(activation_index_width == 18,
              "int8_64x64 uses an 18-bit activation index (plan §16)");
static_assert(weight_index_width == 18,
              "int8_64x64 uses an 18-bit weight index (plan §16)");
static_assert(output_index_width == 17,
              "int8_64x64 uses a 17-bit output index (plan §16)");

#if defined(SAURIA_ACT_IDX_W)
static_assert(SAURIA_ACT_IDX_W == activation_index_width,
              "-DSAURIA_ACT_IDX_W does not match the profile's activation "
              "index width");
#endif
#if defined(SAURIA_WEI_IDX_W)
static_assert(SAURIA_WEI_IDX_W == weight_index_width,
              "-DSAURIA_WEI_IDX_W does not match the profile's weight index "
              "width");
#endif
#if defined(SAURIA_OUT_IDX_W)
static_assert(SAURIA_OUT_IDX_W == output_index_width,
              "-DSAURIA_OUT_IDX_W does not match the profile's output index "
              "width");
#endif

// ── the hygiene switches D17 makes mandatory ─────────────────────────────────
//
// Asserted rather than assumed, because both default the wrong way for a
// component build: `SAURIA_DEBUG` defaults to 1 in the source, and the trace
// writers were never under its control at all.

#if !defined(SAURIA_DEBUG) || SAURIA_DEBUG != 0
#error "The Sauria adapter must be built with -DSAURIA_DEBUG=0 (decision record D17)."
#endif
#if !defined(SAURIA_TRACE_FILES) || SAURIA_TRACE_FILES != 0
#error "The Sauria adapter must be built with -DSAURIA_TRACE_FILES=0 (decision record D17)."
#endif

/// The instance-name substring the source's remaining trace writers gate on.
///
/// They are unreachable from this adapter only because it never uses this name.
/// `sauria_matrix_adapter` refuses to construct a module whose hierarchical
/// name contains it, which turns "unreachable by accident" into "unreachable by
/// rule" — see `TPU_V3_PHASE5_AUDIT.md` §1.
inline constexpr const char* reserved_trace_instance_name = "NpuTop_std";

} // namespace cdc::components::tpu_v3::sauria
