// SPDX-License-Identifier: Apache-2.0
//
// Configuration validation tests.
//
// The positive cases here are cheap; the negative ones are the point. Plan
// §11.1 requires construction to *reject* a wrong frozen value, and a
// validator that silently accepts `mxu.rows = 64` would let a whole
// measurement campaign run on a model that is not the specified machine.

#include "tpu_v3/address_map.h"
#include "tpu_v3/architecture_config.h"
#include "tpu_v3/types.h"

#include <iostream>
#include <stdexcept>
#include <string>

namespace tpu = cdc::components::tpu_v3;
namespace am = cdc::components::tpu_v3::address_map;

namespace {

int failures = 0;

#define CHECK(cond)                                                           \
    do {                                                                      \
        if (!(cond)) {                                                        \
            std::cerr << "CHECK failed: " #cond " @ " << __FILE__ << ':'      \
                      << __LINE__ << '\n';                                    \
            ++failures;                                                       \
        }                                                                     \
    } while (0)

// Reports whether validation rejected the configuration, and requires the
// message to name the offending field: a bare "invalid configuration" is not
// actionable, and plan §19 asks for messages that are.
template <typename Fn>
bool rejected_naming(Fn&& fn, const std::string& needle)
{
    try {
        fn();
    } catch (const std::invalid_argument& error) {
        const std::string what = error.what();
        if (what.find(needle) != std::string::npos) {
            return true;
        }
        std::cerr << "  rejected, but the message does not mention '" << needle
                  << "': " << what << '\n';
        return false;
    } catch (const std::exception& error) {
        std::cerr << "  wrong exception type: " << error.what() << '\n';
        return false;
    }
    std::cerr << "  accepted, but should have been rejected (" << needle
              << ")\n";
    return false;
}

tpu::tpu_soc_config good_config()
{
    tpu::tpu_soc_config config;
    config.name = "test";
    config.mesh_x = 2;
    config.mesh_y = 2;
    config.chips = 1;
    return config;
}

void defaults_are_the_frozen_architecture()
{
    const tpu::tpu_soc_config config = good_config();
    config.validate();

    const auto& core = config.chip.core[0];
    CHECK(config.chip.cores == 2);
    CHECK(core.mxu.rows == 128);
    CHECK(core.mxu.columns == 128);
    CHECK(core.mxu.count_per_core == 2);
    CHECK(core.mxu.backend == tpu::mxu_backend::fast);
    // Decision record D6: BF16 operands, IEEE FP32 accumulation. This
    // supersedes the temporary INT8 + FP32 proposal of Phase 0 (P0-7).
    CHECK(core.mxu.arithmetic == tpu::mxu_arithmetic::bf16_fp32);
    CHECK(core.rvv.xlen == 32);
    CHECK(core.rvv.vlen == 512);
    CHECK(core.rvv.elen == 64);
    CHECK(core.rvv.vlenb() == 64);
    CHECK(core.rvv.version == "1.0");
    // D6: the reference configuration instantiates the full 16 MiB window,
    // superseding the temporary 4 MiB of P0-6.
    CHECK(core.svm_size_bytes == am::svm_default_capacity);
    CHECK(am::svm_default_capacity == am::svm_window);
    CHECK(am::svm_default_capacity == 16u * 1024 * 1024);

    CHECK(config.harts() == 2);
    CHECK(config.mxus() == 4);
}

void derived_counts_scale_with_chips()
{
    tpu::tpu_soc_config config = good_config();
    config.mesh_x = 4;
    config.mesh_y = 4;
    config.chips = 8;
    config.validate();

    CHECK(config.harts() == 16);
    CHECK(config.mxus() == 32);
    CHECK(config.mesh_nodes() == 16);

    // Chips take low node indices row-major; the globals take the last node,
    // which at 8 chips on a 4x4 mesh is comfortably clear of every chip.
    CHECK(config.chip_node(0).x == 0 && config.chip_node(0).y == 0);
    CHECK(config.chip_node(4).x == 0 && config.chip_node(4).y == 1);
    CHECK(config.chip_node(7).x == 3 && config.chip_node(7).y == 1);
    CHECK(config.global_node().x == 3 && config.global_node().y == 3);
}

void frozen_values_are_rejected()
{
    CHECK(rejected_naming(
        [] {
            tpu::tpu_soc_config c = good_config();
            c.chip.cores = 1;
            c.validate();
        },
        "cores"));

    CHECK(rejected_naming(
        [] {
            tpu::tpu_soc_config c = good_config();
            c.chip.core[0].mxu.rows = 64;
            c.validate();
        },
        "mxu.rows"));

    CHECK(rejected_naming(
        [] {
            tpu::tpu_soc_config c = good_config();
            c.chip.core[1].mxu.columns = 256;
            c.validate();
        },
        "mxu.columns"));

    CHECK(rejected_naming(
        [] {
            tpu::tpu_soc_config c = good_config();
            c.chip.core[0].mxu.count_per_core = 1;
            c.validate();
        },
        "mxu.count_per_core"));

    CHECK(rejected_naming(
        [] {
            tpu::tpu_soc_config c = good_config();
            c.chip.core[0].rvv.xlen = 64;
            c.validate();
        },
        "rvv.xlen"));

    CHECK(rejected_naming(
        [] {
            tpu::tpu_soc_config c = good_config();
            c.chip.core[0].rvv.vlen = 256;
            c.validate();
        },
        "rvv.vlen"));

    CHECK(rejected_naming(
        [] {
            tpu::tpu_soc_config c = good_config();
            c.chip.core[0].rvv.elen = 32;
            c.validate();
        },
        "rvv.elen"));

    CHECK(rejected_naming(
        [] {
            tpu::tpu_soc_config c = good_config();
            c.chip.core[0].rvv.version = "0.10";
            c.validate();
        },
        "rvv.version"));

    // A wrong value in core 1 must be caught too: validating only core 0 is a
    // plausible bug that every other test here would miss.
    CHECK(rejected_naming(
        [] {
            tpu::tpu_soc_config c = good_config();
            c.chip.core[1].rvv.vlen = 128;
            c.validate();
        },
        "core1"));
}

void capacities_are_range_and_alignment_checked()
{
    CHECK(rejected_naming(
        [] {
            tpu::tpu_soc_config c = good_config();
            c.chip.core[0].svm_size_bytes = am::svm_max_capacity * 2;
            c.chip.core[1].svm_size_bytes = am::svm_max_capacity * 2;
            c.validate();
        },
        "svm_size_bytes"));

    CHECK(rejected_naming(
        [] {
            tpu::tpu_soc_config c = good_config();
            c.chip.core[0].svm_size_bytes = 0x300000; // 3 MiB
            c.chip.core[1].svm_size_bytes = 0x300000;
            c.validate();
        },
        "power of two"));

    CHECK(rejected_naming(
        [] {
            tpu::tpu_soc_config c = good_config();
            c.global_ram_size_bytes = am::global_ram_max_capacity * 2;
            c.validate();
        },
        "global_ram_size_bytes"));

    // Mismatched SVM sizes between the two cores in a chip: the enumeration
    // uses core 0's value, so accepting this would map core 1 wrongly. Both
    // values here are individually legal, so this reaches the cross-core rule
    // rather than tripping the range check first.
    CHECK(rejected_naming(
        [] {
            tpu::tpu_soc_config c = good_config();
            c.chip.core[1].svm_size_bytes = am::svm_default_capacity / 2;
            c.validate();
        },
        "same SVM capacity"));

    // The extremes must be accepted, or the bounds are wrong rather than the
    // configuration.
    tpu::tpu_soc_config low = good_config();
    low.chip.core[0].svm_size_bytes = am::svm_min_capacity;
    low.chip.core[1].svm_size_bytes = am::svm_min_capacity;
    low.global_ram_size_bytes = am::global_ram_min_capacity;
    low.validate();

    tpu::tpu_soc_config high = good_config();
    high.chip.core[0].svm_size_bytes = am::svm_max_capacity;
    high.chip.core[1].svm_size_bytes = am::svm_max_capacity;
    high.global_ram_size_bytes = am::global_ram_max_capacity;
    high.validate();
}

void noc_limits_are_enforced_at_configuration_time()
{
    // Nine chips: the frozen chimney manager id is 3 bits.
    CHECK(rejected_naming(
        [] {
            tpu::tpu_soc_config c = good_config();
            c.mesh_x = 4;
            c.mesh_y = 4;
            c.chips = tpu::max_chips + 1;
            c.validate();
        },
        "chips"));

    // A mesh size make_noc() does not instantiate.
    CHECK(rejected_naming(
        [] {
            tpu::tpu_soc_config c = good_config();
            c.mesh_x = 5;
            c.mesh_y = 5;
            c.validate();
        },
        "make_noc"));

    CHECK(rejected_naming(
        [] {
            tpu::tpu_soc_config c = good_config();
            c.mesh_x = 3;
            c.mesh_y = 2;
            c.validate();
        },
        "2x2, 3x3, 4x4, 4x2 and 2x4"));

    // Four chips on a 2x2 mesh leaves no node for the global targets, and
    // noc_interconnect refuses a target on a node that hosts an initiator.
    CHECK(rejected_naming(
        [] {
            tpu::tpu_soc_config c = good_config();
            c.mesh_x = 2;
            c.mesh_y = 2;
            c.chips = 4;
            c.validate();
        },
        "NoLoopback"));

    // Three chips on a 2x2 mesh is the largest that fits, and must pass.
    tpu::tpu_soc_config three = good_config();
    three.chips = 3;
    three.validate();
    CHECK(three.global_node().x == 1 && three.global_node().y == 1);
}

void sauria_backend_is_refused_until_phase_9()
{
    // Selecting an unimplemented backend must fail loudly. Falling back to the
    // fast model would produce results labelled "detailed" that are not.
    CHECK(rejected_naming(
        [] {
            tpu::tpu_soc_config c = good_config();
            c.chip.core[0].mxu.backend = tpu::mxu_backend::sauria;
            c.validate();
        },
        "Phase 9"));
}

void quantized_arithmetic_is_refused_until_it_exists()
{
    // Same reasoning as the backend: a configuration asking for INT8/INT32
    // must not quietly get BF16/FP32 results. The extension is optional and
    // additive (D6); it never replaces the reference path.
    CHECK(rejected_naming(
        [] {
            tpu::tpu_soc_config c = good_config();
            c.chip.core[0].mxu.arithmetic = tpu::mxu_arithmetic::int8_int32;
            c.validate();
        },
        "mxu.arithmetic"));

    CHECK(tpu::mxu_arithmetic_from_string("bf16_fp32")
          == tpu::mxu_arithmetic::bf16_fp32);
    CHECK(tpu::mxu_arithmetic_from_string("int8_int32")
          == tpu::mxu_arithmetic::int8_int32);
    CHECK(std::string(tpu::to_string(tpu::mxu_arithmetic::bf16_fp32))
          == "bf16_fp32");
    CHECK(rejected_naming([] { tpu::mxu_arithmetic_from_string("bf16"); },
                          "bf16"));
}

void enum_parsing_round_trips_and_rejects_typos()
{
    CHECK(tpu::mxu_backend_from_string("fast") == tpu::mxu_backend::fast);
    CHECK(tpu::mxu_backend_from_string("sauria") == tpu::mxu_backend::sauria);
    CHECK(tpu::noc_timing_from_string("fast") == tpu::noc_timing::fast);
    CHECK(tpu::noc_timing_from_string("detailed") == tpu::noc_timing::detailed);

    CHECK(std::string(tpu::to_string(tpu::mxu_backend::fast)) == "fast");
    CHECK(std::string(tpu::to_string(tpu::noc_timing::detailed)) == "detailed");

    // A typo must not silently select a backend: "detaild" quietly meaning
    // "fast" would mislabel every latency figure in the run.
    CHECK(rejected_naming([] { tpu::noc_timing_from_string("detaild"); },
                          "detaild"));
    CHECK(rejected_naming([] { tpu::mxu_backend_from_string("FAST"); }, "FAST"));
    CHECK(rejected_naming([] { tpu::mxu_backend_from_string(""); },
                          "accepted values"));
}

void description_reports_what_it_ran()
{
    tpu::tpu_soc_config config = good_config();
    config.name = "single_chip";
    config.validate();

    const std::string text = tpu::describe(config);
    CHECK(text.find("single_chip") != std::string::npos);
    CHECK(text.find("128x128") != std::string::npos);
    CHECK(text.find("VLEN=512") != std::string::npos);
    CHECK(text.find("vlenb=64") != std::string::npos);
    CHECK(text.find("hart 0") != std::string::npos);
    CHECK(text.find("hart 1") != std::string::npos);
    // The timing backend and the arithmetic must be named in any output that
    // could be quoted: neither a latency nor a numeric result is interpretable
    // without them.
    CHECK(text.find("NoC timing backend") != std::string::npos);
    CHECK(text.find("MXU arithmetic       : bf16_fp32") != std::string::npos);
    CHECK(text.find("reference capacity") != std::string::npos);

    const std::string map = tpu::describe_address_map(config);
    CHECK(map.find("chip0.core1.mxu1_control") != std::string::npos);
    CHECK(map.find("global.ram") != std::string::npos);
    // The reference SVM fully backs its window, so nothing is annotated;
    // global RAM at the 256 MiB default does not, so it is.
    CHECK(map.find("global.ram  [backed 256 MiB of 1 GiB") != std::string::npos);

    tpu::tpu_soc_config bringup = good_config();
    bringup.chip.core[0].svm_size_bytes = 1024 * 1024;
    bringup.chip.core[1].svm_size_bytes = 1024 * 1024;
    bringup.validate();
    const std::string small = tpu::describe(bringup);
    CHECK(small.find("bring-up configuration") != std::string::npos);
    const std::string small_map = tpu::describe_address_map(bringup);
    CHECK(small_map.find("[backed 1 MiB of 16 MiB") != std::string::npos);
}

} // namespace

int main()
{
    defaults_are_the_frozen_architecture();
    derived_counts_scale_with_chips();
    frozen_values_are_rejected();
    capacities_are_range_and_alignment_checked();
    noc_limits_are_enforced_at_configuration_time();
    sauria_backend_is_refused_until_phase_9();
    quantized_arithmetic_is_refused_until_it_exists();
    enum_parsing_round_trips_and_rejects_typos();
    description_reports_what_it_ran();

    if (failures != 0) {
        std::cerr << failures << " check(s) failed\n";
        return 1;
    }
    std::cout << "test_architecture_config: all checks passed\n";
    return 0;
}
