// SPDX-License-Identifier: Apache-2.0
//
// The Phase 5 configuration gate, checked from the running binary.
//
// Most of this gate is already enforced at compile time by
// `sauria_geometry.h` — if the geometry, widths or index widths were wrong, or
// the hygiene definitions were missing, this file would not have compiled. That
// is the point, and it is why this test is short.
//
// What it adds is the part a `static_assert` cannot express: that the two
// independent statements of the contract agree. The generated profile came out
// of the pinned source; the frozen numbers below come out of plan §16. A test
// that only re-read the generated header would be comparing it with itself.

#include <cstring>
#include <iostream>
#include <string>

#include <systemc>

#include "tpu_v3/sauria/sa_registers.h"
#include "tpu_v3/sauria/sauria_geometry.h"
#include "tpu_v3/sauria/sauria_matrix_if.h"
#include "tpu_v3/sauria/sauria_profile.h"

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

template <typename T>
void check_equal(const T& got, const T& want, const std::string& what)
{
    if (!(got == want)) {
        ++failures;
        std::cerr << "FAIL: " << what << " is " << got << ", expected " << want
                  << '\n';
    }
}

} // namespace

int sc_main(int, char*[])
{
    // Plan §16: "Record and enforce X=64, Y=64, INT8 activation/weight, INT32
    // accumulation/output, index widths 18/18/17". Written as literals here on
    // purpose — this is the plan's copy of the contract, and the profile is the
    // source's. They must agree without either being derived from the other.
    check_equal(sauria::columns, 64, "profile columns (X)");
    check_equal(sauria::rows, 64, "profile rows (Y)");
    check_equal(sauria::activation_width_bits, 8, "activation width");
    check_equal(sauria::weight_width_bits, 8, "weight width");
    check_equal(sauria::accumulator_width_bits, 32, "accumulator width");
    check_equal(sauria::arithmetic_type, 0, "PE arithmetic type (0 = integer)");
    check_equal(sauria::activation_index_width, 18, "activation index width");
    check_equal(sauria::weight_index_width, 18, "weight index width");
    check_equal(sauria::output_index_width, 17, "output index width");
    check_equal(sauria::operand_bytes, 1, "operand bytes");
    check_equal(sauria::result_bytes, 4, "result bytes");

    check(std::strcmp(sauria::profile_name, "int8_64x64") == 0,
          std::string("profile name is '") + sauria::profile_name
              + "', expected 'int8_64x64'");

    // Provenance must be present and must be a full digest. An empty or short
    // hash means the extraction ran but recorded nothing usable, which would
    // make every later claim about "the source that produced this" unverifiable.
    check(std::strlen(sauria::source_base_hash) == 64,
          "source_base_hash is not a full SHA-256");
    check(std::strlen(sauria::source_patched_hash) == 64,
          "source_patched_hash is not a full SHA-256");
    check(std::strlen(sauria::hygiene_patch_hash) == 64,
          "hygiene_patch_hash is not a full SHA-256");
    check(std::strlen(sauria::golden_case_hash) == 64,
          "golden_case_hash is not a full SHA-256");
    check(std::strcmp(sauria::source_base_hash, sauria::source_patched_hash) != 0,
          "the base and patched hashes are identical, so the hygiene patch "
          "changed nothing — decision record D17 requires it to remove the "
          "source's static trace state");
    check(std::strlen(sauria::hygiene_patch) > 0, "hygiene patch is unnamed");

    std::cout << "sauria profile   : " << sauria::profile_name << '\n'
              << "geometry         : " << sauria::columns << 'x' << sauria::rows
              << "  (X x Y)\n"
              << "arithmetic       : INT" << sauria::activation_width_bits
              << " x INT" << sauria::weight_width_bits << " -> INT"
              << sauria::accumulator_width_bits << '\n'
              << "index widths     : " << sauria::activation_index_width << '/'
              << sauria::weight_index_width << '/'
              << sauria::output_index_width << '\n'
              << "source (base)    : " << sauria::source_base_hash << '\n'
              << "source (patched) : " << sauria::source_patched_hash << '\n'
              << "hygiene patch    : " << sauria::hygiene_patch << '\n';

    // ── the frozen register map (plan §11.4) ─────────────────────────────────
    //
    // Offsets are checked as literals because they are an ABI: firmware built
    // against this map must keep working, so a change here has to be a
    // deliberate edit to both sides rather than a silent renumber when a
    // register is inserted.
    check_equal(sauria::reg::id, std::uint64_t{0x000}, "reg::id");
    check_equal(sauria::reg::control, std::uint64_t{0x008}, "reg::control");
    check_equal(sauria::reg::status, std::uint64_t{0x00C}, "reg::status");
    check_equal(sauria::reg::dim_m, std::uint64_t{0x010}, "reg::dim_m");
    check_equal(sauria::reg::datatype, std::uint64_t{0x044}, "reg::datatype");
    check_equal(sauria::reg::geometry, std::uint64_t{0x080}, "reg::geometry");
    check_equal(sauria::reg::implemented_end, std::uint64_t{0x088},
                "reg::implemented_end");
    check(sauria::identity_value == 0x54503353u, "identity value");

    // The map must fit its window, or the decoder would alias into the next
    // region of the address map.
    check(sauria::reg::implemented_end <= 0x10000,
          "the register map overflows the 64 KiB SA_CONTROL window");

    // Every implemented register is word-aligned: the control plane is 32-bit
    // AXI4-Lite and a misaligned register would be unreachable by a compliant
    // master.
    for (std::uint64_t offset :
         {sauria::reg::id, sauria::reg::version, sauria::reg::control,
          sauria::reg::status, sauria::reg::dim_m, sauria::reg::dim_n,
          sauria::reg::dim_k, sauria::reg::a_addr_lo, sauria::reg::c_stride,
          sauria::reg::datatype, sauria::reg::irq_enable,
          sauria::reg::error_cause, sauria::reg::c_bytes_done_lo,
          sauria::reg::prefetch_ns, sauria::reg::compute_ns,
          sauria::reg::writeback_ns, sauria::reg::geometry,
          sauria::reg::capability}) {
        check(offset % 4 == 0,
              "register at " + std::to_string(offset) + " is not word-aligned");
    }

    // Status bits: BUSY is read-only and the rest are write-1-to-clear. A BUSY
    // that firmware could clear would let it declare the engine idle while a
    // job was still writing to C.
    check((sauria::status_bit::w1c_mask & sauria::status_bit::busy) == 0,
          "BUSY must not be write-1-to-clear");
    check((sauria::status_bit::w1c_mask
           & (sauria::status_bit::done | sauria::status_bit::error
              | sauria::status_bit::aborted))
              == sauria::status_bit::w1c_mask,
          "DONE, ERROR and ABORTED must all be write-1-to-clear");

    // D17 refuses accumulation and C preload for all of Phase 5, and the
    // capability register is how firmware discovers that rather than finding
    // out from a refusal it cannot distinguish from a defect.
    check((sauria::capability_bit::accumulate & 0xffu) == 0,
          "the accumulate capability bit must not collide with the datatype "
          "bits");
    check(sauria::to_string(sauria::submit_status::accumulation_unsupported)
              != std::string("unknown submit status"),
          "accumulation_unsupported has no diagnostic");

    // Every submit_status must describe itself: these are what a firmware
    // author reads when a job is refused.
    for (auto status : {sauria::submit_status::accepted,
                        sauria::submit_status::busy,
                        sauria::submit_status::invalid_dimension,
                        sauria::submit_status::invalid_address,
                        sauria::submit_status::invalid_stride,
                        sauria::submit_status::region_overlap,
                        sauria::submit_status::datatype_unsupported,
                        sauria::submit_status::accumulation_unsupported,
                        sauria::submit_status::staging_capacity_exceeded}) {
        check(sauria::to_string(status) != std::string("unknown submit status"),
              "a submit_status has no diagnostic");
    }

    if (failures != 0) {
        std::cerr << failures << " checks failed\n";
        return 1;
    }
    std::cout << "sauria configuration gate PASS\n";
    return 0;
}
