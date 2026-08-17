// SPDX-License-Identifier: Apache-2.0
//
// Translating a TPU_V3 matrix job into the Sauria configuration that runs it.
//
// This is the **only** new code in the configuration path, and that is
// deliberate. `TPU_V3_PHASE5_AUDIT.md` §4 records the layering:
//
// ```text
// SA_CONTROL (frozen TPU_V3 AXI4-Lite map)   <- firmware-visible, TPU_V3 owns it
//   -> job {M, N, K, addresses, strides}
//   -> SauriaLayerDesc                       <- this file
//   -> sauria_compute_core_fields()          <- the source's own encoder
//   -> sauria_encode_core_config()           <- the source's own packer
//   -> ConfigRegs host port                  <- the source's own distributor
//   -> the ~107 config signals
// ```
//
// Everything below this file is the path the source already uses and the golden
// cases were captured through. That is what makes a source-versus-adapter
// differential mean anything: both sides run the same configuration encoder, so
// a divergence is in the adapter rather than in a re-derived configuration that
// was subtly different from the start.
//
// ## GEMM is a 1x1 convolution, and the mapping is quoted, not inferred
//
// `SauriaLayerDesc` describes a convolution, so a matrix multiply has to be
// expressed as one. Getting that wrong is the easiest way to make Phase 5
// quietly incorrect — transposing `h_til` and `w_til`, or leaving `preload_en`
// at the demo's value, both produce plausible wrong numbers rather than a
// failure. So the mapping is taken from the golden case the plan names:
//
// ```text
// npu_demo_clean/cases/demo_gemm_64x64/case.env
//   DESC="1x1 conv / GeMM, Cin=256, Cout=64, 64x64 output on the 64x64 array."
//   SHAPE="1 1 1 1 256 64 1 64 64 64 1"
// ```
//
// whose eleven fields are `SauriaLayerDesc` in declaration order. For
// `C[M x N] = A[M x K] . B[K x N]`:
//
//   B_w, B_h    = 1, 1     the 1x1 kernel that makes this a GEMM
//   d, s        = 1, 1     no dilation, unit stride
//   c_til       = K        input channels
//   k_til       = N        output channels
//   h_til,w_til = 1, M     M as output spatial, laid out as one row
//   X_used,Y_used          the array actually in use
//   preload_en  = 0        D17 refuses C preload for all of Phase 5
//
// The demo's `preload_en` is 1 and this translator always emits 0. That is not
// an oversight: D17 refuses accumulation and C preload until the semantics of
// accumulating into a region a failed job partially wrote are settled, and
// `capability_bit::accumulate` is clear to match.

#pragma once

#include <array>
#include <cstdint>

#include "tpu_v3/sauria/sauria_matrix_if.h"

namespace cdc::components::tpu_v3::sauria {

/// The subset of `SauriaLayerDesc` this translator produces, in the source's
/// own field order.
///
/// Mirrored rather than including `driver/libsauria_cfg.h` here so that callers
/// which only want to *check* a translation — the tests, and anything reasoning
/// about a job before the engine exists — do not have to compile the Sauria
/// headers and inherit their build requirements. The adapter converts this to
/// the real `SauriaLayerDesc` in one obvious assignment, and a test asserts the
/// field order against the golden case's `SHAPE` string.
struct gemm_layer_desc {
    int kernel_width = 1;   ///< `B_w`
    int kernel_height = 1;  ///< `B_h`
    int dilation = 1;       ///< `d`
    int stride = 1;         ///< `s`
    int channels_in = 0;    ///< `c_til`  = K
    int channels_out = 0;   ///< `k_til`  = N
    int tile_height = 1;    ///< `h_til`
    int tile_width = 0;     ///< `w_til`  = M
    int columns_used = 0;   ///< `X_used`
    int rows_used = 0;      ///< `Y_used`
    int preload_enable = 0; ///< `preload_en`, always 0 in Phase 5 (D17)

    /// The eleven fields in the order `case.env`'s `SHAPE` string writes them,
    /// so a test can compare against the golden case directly.
    std::array<int, 11> shape() const
    {
        return {kernel_width, kernel_height, dilation,     stride,
                channels_in,  channels_out,  tile_height,  tile_width,
                columns_used, rows_used,     preload_enable};
    }
};

/// Element size in bytes for a datatype selector, or 0 if unknown.
std::uint32_t operand_element_bytes(std::uint32_t datatype) noexcept;
std::uint32_t result_element_bytes(std::uint32_t datatype) noexcept;

/// Check a job against everything that can be decided without running it.
///
/// Returns `submit_status::accepted` when the job is legal. Every refusal here
/// happens *before* any state changes, so a rejected job leaves C untouched —
/// unlike a job that fails partway, which leaves C partially written and is
/// reported through status (D17).
///
/// `sram_capacity` is the core-SRAM window the operands must fit inside.
submit_status validate_gemm(const job& work, std::uint32_t rows,
                            std::uint32_t columns, std::uint64_t sram_base,
                            std::uint64_t sram_capacity,
                            std::uint32_t supported_datatypes);

/// Translate a **validated** job. Calling this on a job `validate_gemm()`
/// rejected is a programming error, not a runtime one.
gemm_layer_desc describe_gemm(const job& work, std::uint32_t rows,
                              std::uint32_t columns);

/// Row stride actually used: the job's, or the tightly packed one when the job
/// asked for zero.
///
/// Exposed because the prefetch controller and the validator must agree about
/// it exactly — a validator that bounds-checked the packed stride while the
/// controller walked a padded one would pass a job that then reads out of the
/// window.
std::uint64_t effective_stride(std::uint32_t requested, std::uint32_t elements,
                               std::uint32_t element_bytes) noexcept;

} // namespace cdc::components::tpu_v3::sauria
