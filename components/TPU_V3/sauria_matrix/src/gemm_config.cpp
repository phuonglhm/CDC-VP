// SPDX-License-Identifier: Apache-2.0

#include "tpu_v3/sauria/gemm_config.h"

#include <limits>

namespace cdc::components::tpu_v3::sauria {

namespace {

/// Overflow-safe "does `[base, base + length)` fit inside the window".
bool fits(std::uint64_t address, std::uint64_t length, std::uint64_t base,
          std::uint64_t capacity)
{
    if (address < base) {
        return false;
    }
    const std::uint64_t offset = address - base;
    return offset <= capacity && length <= capacity - offset;
}

bool overlaps(std::uint64_t a, std::uint64_t a_len, std::uint64_t b,
              std::uint64_t b_len)
{
    if (a_len == 0 || b_len == 0) {
        return false;
    }
    return a < b + b_len && b < a + a_len;
}

/// Bytes a matrix occupies given its row count and row stride.
///
/// The last row needs only its own elements, not a full stride, so this is
/// `(rows - 1) * stride + row_bytes` rather than `rows * stride`. Using the
/// simpler product would refuse a perfectly legal tile that ends exactly at the
/// window boundary.
std::uint64_t footprint(std::uint64_t rows, std::uint64_t stride,
                        std::uint64_t row_bytes)
{
    return rows == 0 ? 0 : (rows - 1) * stride + row_bytes;
}

} // namespace

std::uint32_t operand_element_bytes(std::uint32_t datatype) noexcept
{
    switch (datatype) {
    case datatype_value::int8_int32:
        return 1;
    case datatype_value::fp16_fp32:
        return 2;
    case datatype_value::bf16_fp32:
        return 2;
    default:
        return 0;
    }
}

std::uint32_t result_element_bytes(std::uint32_t datatype) noexcept
{
    switch (datatype) {
    case datatype_value::int8_int32:
    case datatype_value::fp16_fp32:
    case datatype_value::bf16_fp32:
        return 4;
    default:
        return 0;
    }
}

std::uint64_t effective_stride(std::uint32_t requested, std::uint32_t elements,
                               std::uint32_t element_bytes) noexcept
{
    // Zero means "tightly packed" so firmware laying out a dense matrix does
    // not have to know the element size. Any other value is taken literally,
    // including one larger than the row, which is how a padded tile is
    // described.
    return requested != 0 ? requested
                          : static_cast<std::uint64_t>(elements) * element_bytes;
}

submit_status validate_gemm(const job& work, std::uint32_t rows,
                            std::uint32_t columns, std::uint64_t sram_base,
                            std::uint64_t sram_capacity,
                            std::uint32_t supported_datatypes)
{
    // Datatype first: every size below depends on it, so a validator that
    // checked dimensions first would compute footprints from an element size it
    // had not established.
    const std::uint32_t operand_bytes = operand_element_bytes(work.datatype);
    const std::uint32_t result_bytes = result_element_bytes(work.datatype);
    if (operand_bytes == 0 || result_bytes == 0) {
        return submit_status::datatype_unsupported;
    }
    const std::uint32_t datatype_bit = 1u << work.datatype;
    if ((supported_datatypes & datatype_bit) == 0) {
        return submit_status::datatype_unsupported;
    }

    if (work.m == 0 || work.n == 0 || work.k == 0) {
        return submit_status::invalid_dimension;
    }
    // The array computes N output channels across its columns and M positions
    // down its rows. Larger problems need tiling, which Phase 5 does not
    // implement — and refusing is the point: a silently truncated GEMM is the
    // failure this phase exists to make impossible.
    if (work.n > columns || work.m > rows) {
        return submit_status::dimension_exceeds_array;
    }

    const std::uint64_t a_row_bytes =
        static_cast<std::uint64_t>(work.k) * operand_bytes;
    const std::uint64_t b_row_bytes =
        static_cast<std::uint64_t>(work.n) * operand_bytes;
    const std::uint64_t c_row_bytes =
        static_cast<std::uint64_t>(work.n) * result_bytes;

    const std::uint64_t a_stride =
        effective_stride(work.a_stride_bytes, work.k, operand_bytes);
    const std::uint64_t b_stride =
        effective_stride(work.b_stride_bytes, work.n, operand_bytes);
    const std::uint64_t c_stride =
        effective_stride(work.c_stride_bytes, work.n, result_bytes);

    // A stride narrower than the row it describes would make consecutive rows
    // overlap, so the "matrix" would not be one.
    if (a_stride < a_row_bytes || b_stride < b_row_bytes
        || c_stride < c_row_bytes) {
        return submit_status::invalid_stride;
    }

    const std::uint64_t a_bytes = footprint(work.m, a_stride, a_row_bytes);
    const std::uint64_t b_bytes = footprint(work.k, b_stride, b_row_bytes);
    const std::uint64_t c_bytes = footprint(work.m, c_stride, c_row_bytes);

    if (!fits(work.a_address, a_bytes, sram_base, sram_capacity)
        || !fits(work.b_address, b_bytes, sram_base, sram_capacity)
        || !fits(work.c_address, c_bytes, sram_base, sram_capacity)) {
        return submit_status::invalid_address;
    }

    // C overlapping an operand makes the result depend on the order in which
    // the engine happens to read and write, which is not a contract anyone can
    // rely on. A and B may overlap each other freely — both are read-only.
    if (overlaps(work.c_address, c_bytes, work.a_address, a_bytes)
        || overlaps(work.c_address, c_bytes, work.b_address, b_bytes)) {
        return submit_status::region_overlap;
    }

    return submit_status::accepted;
}

gemm_layer_desc describe_gemm(const job& work, std::uint32_t rows,
                              std::uint32_t columns)
{
    gemm_layer_desc desc;
    // 1x1 kernel, unit stride, no dilation: this is what turns a convolution
    // descriptor into a matrix multiply. See the header for the golden case
    // this mapping is quoted from.
    desc.kernel_width = 1;
    desc.kernel_height = 1;
    desc.dilation = 1;
    desc.stride = 1;

    desc.channels_in = static_cast<int>(work.k);   // c_til
    desc.channels_out = static_cast<int>(work.n);  // k_til
    desc.tile_height = 1;                          // h_til
    desc.tile_width = static_cast<int>(work.m);    // w_til

    // How much of the array this problem uses, which is the problem's shape and
    // not the array's. `validate_gemm` has already refused `n > columns` and
    // `m > rows`, so these never exceed the geometry.
    //
    // Passing the full `columns`/`rows` instead would be wrong in a way that is
    // invisible at exactly 64x64 — where the golden case lives — and wrong
    // everywhere else: `X_used` sets the column mask and the weight feeder's
    // `til_kstep`, so claiming 64 columns for a 32-channel problem makes the
    // feeder fetch 64 channels' worth of weights per step and read past the tile.
    desc.columns_used = static_cast<int>(work.n);
    desc.rows_used = static_cast<int>(work.m);

    (void)rows;
    (void)columns;

    // Always zero. D17 refuses accumulation and C preload for the whole of
    // Phase 5, and `capability_bit::accumulate` is clear to match. The golden
    // case ships `preload_en = 1`; that difference is deliberate and is the one
    // field where this translator does not follow the demo.
    desc.preload_enable = 0;

    return desc;
}

} // namespace cdc::components::tpu_v3::sauria
