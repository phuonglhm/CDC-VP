// SPDX-License-Identifier: Apache-2.0
//
// The job-to-Sauria-configuration translation.
//
// The headline check is that translating the golden case's GEMM reproduces the
// golden case's own `SHAPE` string. That is the one test in this file that
// could not have been written by reading my own code: the expected values come
// from `npu_demo_clean/cases/demo_gemm_64x64/case.env`, which the NPU team
// captured, and a translator that transposed `h_til`/`w_til` or misplaced K and
// N would still look self-consistent while failing it.

#include <array>
#include <iostream>
#include <string>

#include <systemc>

#include "tpu_v3/sauria/gemm_config.h"
#include "tpu_v3/sauria/sauria_geometry.h"

namespace sauria = cdc::components::tpu_v3::sauria;

namespace {

int failures = 0;

void check(bool condition, const std::string& what)
{
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << what << '\n';
    }
}

constexpr std::uint64_t kBase = 0;
constexpr std::uint64_t kCapacity = 16u * 1024 * 1024;
constexpr std::uint32_t kAllDatatypes = sauria::capability_bit::int8_int32;

sauria::job golden_job()
{
    // C[64 x 64] = A[64 x 256] . B[256 x 64], the shape recorded in the golden
    // case's manifest.
    sauria::job work;
    work.m = 64;
    work.n = 64;
    work.k = 256;
    work.a_address = 0;
    work.b_address = 0x4000;
    work.c_address = 0x8000;
    work.datatype = sauria::datatype_value::int8_int32;
    return work;
}

} // namespace

int sc_main(int, char*[])
{
    const auto rows = static_cast<std::uint32_t>(sauria::rows);
    const auto columns = static_cast<std::uint32_t>(sauria::columns);

    // ── the golden case's own SHAPE, field for field ─────────────────────────
    //
    // case.env: SHAPE="1 1 1 1 256 64 1 64 64 64 1"
    //
    // Every field matches except the last: the demo enables C preload and Phase
    // 5 refuses it (D17), so this translator emits 0 there. That difference is
    // asserted rather than tolerated — if it ever became 1 by accident, an
    // engine would accumulate into a region a failed job had partially written.
    {
        const auto desc = sauria::describe_gemm(golden_job(), rows, columns);
        const std::array<int, 11> expected{1, 1, 1, 1, 256, 64, 1, 64, 64, 64, 0};
        const auto actual = desc.shape();

        static const char* names[] = {
            "B_w",   "B_h",    "d",       "s",       "c_til (K)", "k_til (N)",
            "h_til", "w_til (M)", "X_used", "Y_used", "preload_en"};
        for (std::size_t i = 0; i < expected.size(); ++i) {
            check(actual[i] == expected[i],
                  std::string("golden SHAPE field ") + names[i] + " is "
                      + std::to_string(actual[i]) + ", expected "
                      + std::to_string(expected[i]));
        }
    }

    // K and N must not be interchangeable. A square GEMM would hide a swap, so
    // this one is deliberately not square.
    {
        sauria::job work = golden_job();
        work.m = 32;
        work.n = 16;
        work.k = 128;
        const auto desc = sauria::describe_gemm(work, rows, columns);
        check(desc.channels_in == 128, "c_til must be K");
        check(desc.channels_out == 16, "k_til must be N");
        check(desc.tile_width == 32, "w_til must be M");
        check(desc.tile_height == 1, "h_til must be 1 for a GEMM");
    }

    // ── strides ──────────────────────────────────────────────────────────────
    check(sauria::effective_stride(0, 64, 1) == 64,
          "a zero stride must mean tightly packed");
    check(sauria::effective_stride(0, 64, 4) == 256,
          "a zero stride must account for the element size");
    check(sauria::effective_stride(128, 64, 1) == 128,
          "a stated stride must be taken literally, so padded tiles work");

    // ── refusals ─────────────────────────────────────────────────────────────
    auto validate = [&](const sauria::job& work) {
        return sauria::validate_gemm(work, rows, columns, kBase, kCapacity,
                                     kAllDatatypes);
    };

    check(validate(golden_job()) == sauria::submit_status::accepted,
          "the golden case's own job was refused");

    for (auto zero : {&sauria::job::m, &sauria::job::n, &sauria::job::k}) {
        sauria::job work = golden_job();
        work.*zero = 0;
        check(validate(work) == sauria::submit_status::invalid_dimension,
              "a zero dimension was accepted");
    }

    {   // Larger than one pass of the array: legal GEMM, not a malformed one.
        sauria::job work = golden_job();
        work.n = columns + 1;
        check(validate(work) == sauria::submit_status::dimension_exceeds_array,
              "an N wider than the array was not refused distinctly");
        work = golden_job();
        work.m = rows + 1;
        check(validate(work) == sauria::submit_status::dimension_exceeds_array,
              "an M taller than the array was not refused distinctly");
    }

    {   // A stride narrower than its row makes consecutive rows overlap.
        sauria::job work = golden_job();
        work.a_stride_bytes = 128;   // K = 256 INT8 bytes
        check(validate(work) == sauria::submit_status::invalid_stride,
              "a stride narrower than the row was accepted");
    }

    {   // Out of the window.
        sauria::job work = golden_job();
        work.c_address = kCapacity - 16;
        check(validate(work) == sauria::submit_status::invalid_address,
              "a C region running past the window was accepted");
    }

    {   // C over an operand makes the result depend on access order.
        sauria::job work = golden_job();
        work.c_address = work.a_address;
        check(validate(work) == sauria::submit_status::region_overlap,
              "C overlapping A was accepted");
        work = golden_job();
        work.c_address = work.b_address;
        check(validate(work) == sauria::submit_status::region_overlap,
              "C overlapping B was accepted");
    }

    {   // A and B may overlap: both are read-only, so order cannot matter.
        sauria::job work = golden_job();
        work.b_address = work.a_address;
        check(validate(work) == sauria::submit_status::accepted,
              "two read-only operands sharing storage were refused");
    }

    {   // The pinned engine has no BF16 profile, so asking for the D6 reference
        // path must be refused *by capability* rather than silently downgraded.
        sauria::job work = golden_job();
        work.datatype = sauria::datatype_value::bf16_fp32;
        check(validate(work) == sauria::submit_status::datatype_unsupported,
              "bf16_fp32 was accepted by an engine whose source has no BF16 "
              "profile");
    }

    {   // A **padded** tile that ends exactly at the window boundary is legal.
        //
        // The last row occupies only its own bytes, not a whole stride, so the
        // footprint is `(rows - 1) * stride + row_bytes`. The naive
        // `rows * stride` overstates it by the final row's padding and would
        // refuse this job.
        //
        // `m` must be greater than 1 and the stride must exceed the row for the
        // two formulas to differ at all — an earlier version of this check used
        // `m = 1`, where they are equal, so it passed against both and proved
        // nothing.
        sauria::job work = golden_job();
        work.m = 2;
        work.n = 64;
        work.k = 256;
        const std::uint64_t c_stride = 512;            // 64 x 4 bytes, padded x2
        const std::uint64_t c_row = 64 * 4;
        work.c_stride_bytes = static_cast<std::uint32_t>(c_stride);
        work.c_address = kCapacity - (c_stride + c_row);   // exact fit
        work.a_address = 0;
        work.b_address = 0x10000;
        check(validate(work) == sauria::submit_status::accepted,
              "a padded region ending exactly at the window boundary was "
              "refused; the footprint is counting the last row's padding");
    }

    if (failures != 0) {
        std::cerr << failures << " checks failed\n";
        return 1;
    }
    std::cout << "gemm config translation PASS (golden SHAPE reproduced)\n";
    return 0;
}
