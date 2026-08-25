// SPDX-License-Identifier: Apache-2.0
//
// Configuration validation tests.
//
// The positive cases here are cheap; the negative ones are the point. Plan
// §11.1 requires construction to *reject* a wrong frozen value, and a
// validator that silently accepted a 128x128 geometry would let a whole
// measurement campaign report the verified 64x64 bring-up array under the name
// of a machine the NPU team has not delivered.
//
// Rebaselined for D14/D15: one Sauria matrix engine, one independent DMA and
// one ImageTransform engine per NEO-CORE, and a local-SRAM fabric whose
// physical parameters have no architectural default.

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
    // D15 leaves the physical fabric parameters open, so the schema has no
    // default and every configuration — including a test's — has to state
    // them. That is the intended friction: it is what stops an illustrative
    // datapath width becoming an architectural constant by accident.
    for (auto& core : config.chip.core) {
        core.local_sram_fabric = tpu::provisional_local_sram_fabric();
    }
    return config;
}

void defaults_are_the_frozen_architecture()
{
    const tpu::tpu_soc_config config = good_config();
    config.validate();

    const auto& core = config.chip.core[0];
    CHECK(config.chip.cores == 2);

    // D14: one matrix engine, one DMA, one ImageTransform engine per core.
    CHECK(core.sa.count_per_core == 1);
    CHECK(core.dma.count_per_core == 1);
    CHECK(core.transform.count_per_core == 1);

    // The verified v4.2 bring-up array. 128x128 is the destination, not the
    // default, and nothing may report the one as the other.
    CHECK(core.sa.rows == 64);
    CHECK(core.sa.columns == 64);
    CHECK(core.sa.geometry() == "64x64");
    CHECK(!core.sa.is_target_geometry());
    CHECK(core.sa.source_revision.empty());

    // Decision record D6: BF16 operands, IEEE FP32 accumulation. This
    // supersedes the temporary INT8 + FP32 proposal of Phase 0 (P0-7).
    CHECK(core.sa.datatype == tpu::matrix_datatype::bf16_fp32);

    // Phase 6 built Im2Col, but the platform does not compose/link it until
    // Phase 7. Col2Im has no source and remains unavailable in every phase.
    CHECK(!core.transform.im2col_available);
    CHECK(!core.transform.col2im_available);

    CHECK(core.rvv.xlen == 32);
    CHECK(core.rvv.vlen == 512);
    CHECK(core.rvv.elen == 64);
    CHECK(core.rvv.vlenb() == 64);
    CHECK(core.rvv.version == "1.0");

    // D6: the reference configuration instantiates the full 16 MiB window,
    // superseding the temporary 4 MiB of P0-6.
    CHECK(core.sram_size_bytes == am::core_sram_default_capacity);
    CHECK(am::core_sram_default_capacity == am::core_sram_window);
    CHECK(am::core_sram_default_capacity == 16u * 1024 * 1024);

    CHECK(config.harts() == 2);
    CHECK(config.matrix_engines() == 2);
    CHECK(config.dma_engines() == 2);
    CHECK(config.transform_engines() == 2);
}

void derived_counts_scale_with_chips()
{
    tpu::tpu_soc_config config = good_config();
    config.mesh_x = 4;
    config.mesh_y = 4;
    config.chips = 8;
    config.validate();

    CHECK(config.harts() == 16);
    CHECK(config.matrix_engines() == 16);
    CHECK(config.mesh_nodes() == 16);

    // 16 x 16 MiB of core SRAM plus 256 MiB of global RAM. This is logical
    // address space; D6 requires it to be sparsely backed, so it is not a host
    // allocation and `test_sparse_memory` is what proves that separately.
    CHECK(config.logical_memory_bytes()
          == 16ull * am::core_sram_default_capacity
                 + am::global_ram_default_capacity);

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
            c.chip.core[0].sa.count_per_core = 2;
            c.validate();
        },
        "sa.count_per_core"));

    CHECK(rejected_naming(
        [] {
            tpu::tpu_soc_config c = good_config();
            c.chip.core[0].dma.count_per_core = 0;
            c.validate();
        },
        "dma.count_per_core"));

    CHECK(rejected_naming(
        [] {
            tpu::tpu_soc_config c = good_config();
            c.chip.core[1].transform.count_per_core = 2;
            c.validate();
        },
        "transform.count_per_core"));

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

void the_matrix_geometry_contract_is_enforced()
{
    // The whole point of D14's two-stage plan. Asking for the destination
    // geometry today must fail: the NPU team has not delivered the 128x128
    // source, so its promotion gate cannot have passed, and quietly running
    // the 64x64 array under that name is the outcome the rule exists to stop.
    CHECK(rejected_naming(
        [] {
            tpu::tpu_soc_config c = good_config();
            c.chip.core[0].sa.rows = 128;
            c.chip.core[0].sa.columns = 128;
            c.validate();
        },
        "promotion gate"));

    // An unnamed geometry has neither a source nor golden tests.
    CHECK(rejected_naming(
        [] {
            tpu::tpu_soc_config c = good_config();
            c.chip.core[0].sa.rows = 32;
            c.chip.core[0].sa.columns = 32;
            c.validate();
        },
        "sa.geometry"));

    // Half a promotion is still not 128x128.
    CHECK(rejected_naming(
        [] {
            tpu::tpu_soc_config c = good_config();
            c.chip.core[1].sa.columns = 128;
            c.validate();
        },
        "sa.geometry"));

    // Arithmetic is *not* refused here any more, and the change is deliberate.
    //
    // `int8_int32` used to be rejected as unimplemented. Phase 5 implements
    // exactly it — the verified v4.2 `int8_64x64` profile — and D6 permits it
    // as an opt-in quantized extension, so a schema that refuses the only
    // arithmetic the source provides would make the phase unconfigurable.
    // What D6 actually forbids is INT8 being read *as* the BF16 reference, and
    // that is checked below on the report, where it can be broken.
    tpu::tpu_soc_config quantized = good_config();
    quantized.chip.core[0].sa.datatype = tpu::matrix_datatype::int8_int32;
    quantized.chip.core[1].sa.datatype = tpu::matrix_datatype::int8_int32;
    quantized.validate();

    // Every non-reference datatype must be *named* and must be *disclaimed* in
    // the same breath. The report used to print the datatype followed by a
    // hard-coded "(BF16 operands, IEEE FP32 accumulation)", so an FP16 run
    // printed a BF16 claim beside the word `fp16_fp32` and the old test, which
    // only looked for the name, passed anyway.
    for (auto datatype : {tpu::matrix_datatype::fp16_fp32,
                          tpu::matrix_datatype::int8_int32}) {
        tpu::tpu_soc_config bringup = good_config();
        bringup.chip.core[0].sa.datatype = datatype;
        bringup.chip.core[1].sa.datatype = datatype;
        bringup.validate();

        const std::string report = tpu::describe(bringup);
        CHECK(report.find(tpu::to_string(datatype)) != std::string::npos);
        CHECK(report.find("NOT the BF16 reference path") != std::string::npos);
        // The specific regression: no BF16 claim anywhere in a non-BF16 report.
        CHECK(report.find("BF16 operands") == std::string::npos);
    }

    // The two cores of a chip must agree about everything the report reads from
    // core 0. Until this check existed a caller could give core 1 a different
    // datatype, source revision or geometry and the report would describe only
    // core 0 — so half the matrix work ran on an arithmetic nothing mentioned.
    //
    // Each case changes **core 1 only**, which is exactly what the tests above
    // could not catch: they assign both cores together, so a per-core hole is
    // invisible to them by construction.
    CHECK(rejected_naming(
        [] {
            tpu::tpu_soc_config c = good_config();
            c.chip.core[1].sa.datatype = tpu::matrix_datatype::int8_int32;
            c.validate();
        },
        "chip.core1.sa.datatype"));
    CHECK(rejected_naming(
        [] {
            tpu::tpu_soc_config c = good_config();
            c.chip.core[1].sa.source_revision = "some-other-revision";
            c.validate();
        },
        "chip.core1.sa.source_revision"));
    CHECK(rejected_naming(
        [] {
            tpu::tpu_soc_config c = good_config();
            c.chip.core[1].sa.rows = tpu::sa_target_rows;
            c.chip.core[1].sa.columns = tpu::sa_target_columns;
            c.validate();
        },
        "chip.core1.sa"));

    // Agreeing on a non-default value is still fine — the rule is agreement,
    // not "must be the default".
    tpu::tpu_soc_config agreed = good_config();
    agreed.chip.core[0].sa.source_revision = "v4.2-pinned";
    agreed.chip.core[1].sa.source_revision = "v4.2-pinned";
    agreed.validate();

    // ...and the reference path still says so.
    tpu::tpu_soc_config reference = good_config();
    const std::string reference_report = tpu::describe(reference);
    CHECK(reference_report.find("bf16_fp32") != std::string::npos);
    CHECK(reference_report.find("BF16 operands, IEEE FP32 accumulation")
          != std::string::npos);
    CHECK(reference_report.find("NOT the BF16 reference path")
          == std::string::npos);
}

void an_available_transform_must_name_its_source()
{
    // D18: no value in source_revision can make the missing Col2Im real.
    CHECK(rejected_naming(
        [] {
            tpu::tpu_soc_config c = good_config();
            c.chip.core[0].transform.col2im_available = true;
            c.chip.core[0].transform.source_revision = "invented-col2im";
            c.validate();
        },
        "transform.col2im_available"));

    CHECK(rejected_naming(
        [] {
            tpu::tpu_soc_config c = good_config();
            c.chip.core[0].transform.im2col_available = true;
            c.chip.core[0].transform.source_revision = "wrong-revision";
            c.validate();
        },
        "transform.source_revision"));

    tpu::tpu_soc_config sourced = good_config();
    for (auto& core : sourced.chip.core) {
        core.transform.im2col_available = true;
        core.transform.source_revision = tpu::im2col_source_revision;
    }
    sourced.validate();
}

void the_open_fabric_parameters_have_no_default()
{
    // D15's rule, made mechanical: the schema must not carry a physical value
    // that nobody chose. A default-constructed configuration is therefore
    // *invalid*, and the message has to say why rather than complain about a
    // range.
    CHECK(rejected_naming(
        [] {
            tpu::tpu_soc_config c;
            c.validate();
        },
        "no architectural default"));

    CHECK(rejected_naming(
        [] {
            tpu::tpu_soc_config c = good_config();
            c.chip.core[0].local_sram_fabric.bank_count = 0;
            c.validate();
        },
        "local_sram_banks"));

    CHECK(rejected_naming(
        [] {
            tpu::tpu_soc_config c = good_config();
            c.chip.core[0].local_sram_fabric.pipeline_stages = 0;
            c.validate();
        },
        "local_sram_pipeline_stages"));

    // Physical sanity, once a value has been stated.
    CHECK(rejected_naming(
        [] {
            tpu::tpu_soc_config c = good_config();
            c.chip.core[0].local_sram_fabric.data_width_bits = 12;
            c.validate();
        },
        "multiple of 8"));

    CHECK(rejected_naming(
        [] {
            tpu::tpu_soc_config c = good_config();
            c.chip.core[0].local_sram_fabric.data_width_bits = 24;
            c.validate();
        },
        "power of two"));

    CHECK(rejected_naming(
        [] {
            tpu::tpu_soc_config c = good_config();
            c.chip.core[0].local_sram_fabric.bank_count = 6;
            c.validate();
        },
        "power of two"));

    CHECK(rejected_naming(
        [] {
            tpu::tpu_soc_config c = good_config();
            c.chip.core[0].local_sram_fabric.max_outstanding_per_requester = 2;
            c.validate();
        },
        "max_outstanding_per_requester"));

    // A capacity smaller than one bank stripe would leave banks no address
    // selects, so the arbitration a report described would have been measured
    // on a structure the configuration does not describe.
    CHECK(rejected_naming(
        [] {
            tpu::tpu_soc_config c = good_config();
            for (auto& core : c.chip.core) {
                core.local_sram_fabric.data_width_bits = 1024;
                core.local_sram_fabric.bank_count = 64;
                core.sram_size_bytes = am::core_sram_min_capacity;
            }
            c.validate();
        },
        "bank stripe"));

    const auto fabric = tpu::provisional_local_sram_fabric();
    CHECK(fabric.bytes_per_beat() == 16);
    CHECK(fabric.stripe_bytes() == 64);
}

void capacities_are_range_and_alignment_checked()
{
    CHECK(rejected_naming(
        [] {
            tpu::tpu_soc_config c = good_config();
            c.chip.core[0].sram_size_bytes = am::core_sram_max_capacity * 2;
            c.chip.core[1].sram_size_bytes = am::core_sram_max_capacity * 2;
            c.validate();
        },
        "sram_size_bytes"));

    CHECK(rejected_naming(
        [] {
            tpu::tpu_soc_config c = good_config();
            c.chip.core[0].sram_size_bytes = 0x300000; // 3 MiB
            c.chip.core[1].sram_size_bytes = 0x300000;
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

    // Mismatched SRAM sizes between the two cores in a chip: the enumeration
    // uses core 0's value, so accepting this would map core 1 wrongly. Both
    // values here are individually legal, so this reaches the cross-core rule
    // rather than tripping the range check first.
    CHECK(rejected_naming(
        [] {
            tpu::tpu_soc_config c = good_config();
            c.chip.core[1].sram_size_bytes =
                am::core_sram_default_capacity / 2;
            c.validate();
        },
        "same core SRAM capacity"));

    // The extremes must be accepted, or the bounds are wrong rather than the
    // configuration.
    tpu::tpu_soc_config low = good_config();
    low.chip.core[0].sram_size_bytes = am::core_sram_min_capacity;
    low.chip.core[1].sram_size_bytes = am::core_sram_min_capacity;
    low.global_ram_size_bytes = am::global_ram_min_capacity;
    low.validate();

    tpu::tpu_soc_config high = good_config();
    high.chip.core[0].sram_size_bytes = am::core_sram_max_capacity;
    high.chip.core[1].sram_size_bytes = am::core_sram_max_capacity;
    high.global_ram_size_bytes = am::global_ram_max_capacity;
    high.validate();

    // The DMA's chunk bound follows the smallest downstream limit (plan §9.3),
    // so a request for more must be refused rather than silently clamped.
    CHECK(rejected_naming(
        [] {
            tpu::tpu_soc_config c = good_config();
            c.chip.core[0].dma.max_burst_bytes = 4096;
            c.validate();
        },
        "dma.max_burst_bytes"));
}

void noc_limits_are_enforced_at_configuration_time()
{
    // Nine entries exceed the retained Revision 1 config/address-map schema.
    // D27 explicitly rejects the old claim that this is an RTL NoC-node limit.
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

void enum_parsing_round_trips_and_rejects_typos()
{
    CHECK(tpu::matrix_datatype_from_string("bf16_fp32")
          == tpu::matrix_datatype::bf16_fp32);
    CHECK(tpu::matrix_datatype_from_string("int8_int32")
          == tpu::matrix_datatype::int8_int32);
    CHECK(tpu::bank_mapping_from_string("low_order_interleaved")
          == tpu::bank_mapping::low_order_interleaved);
    CHECK(tpu::arbitration_policy_from_string("round_robin")
          == tpu::arbitration_policy::round_robin);
    CHECK(tpu::noc_timing_from_string("fast") == tpu::noc_timing::fast);
    CHECK(tpu::noc_timing_from_string("detailed") == tpu::noc_timing::detailed);

    CHECK(std::string(tpu::to_string(tpu::matrix_datatype::bf16_fp32))
          == "bf16_fp32");
    CHECK(std::string(tpu::to_string(tpu::bank_mapping::low_order_interleaved))
          == "low_order_interleaved");
    CHECK(std::string(tpu::to_string(tpu::noc_timing::detailed)) == "detailed");

    // A typo must not silently select a mode: "detaild" quietly meaning "fast"
    // would mislabel every latency figure in the run.
    CHECK(rejected_naming([] { tpu::noc_timing_from_string("detaild"); },
                          "detaild"));
    CHECK(rejected_naming([] { tpu::matrix_datatype_from_string("bf16"); },
                          "bf16"));
    CHECK(rejected_naming([] { tpu::noc_timing_from_string("FAST"); }, "FAST"));
    CHECK(rejected_naming([] { tpu::bank_mapping_from_string("xor"); },
                          "low_order_interleaved"));
}

void description_reports_what_it_ran()
{
    tpu::tpu_soc_config config = good_config();
    config.name = "single_chip";
    config.validate();

    const std::string text = tpu::describe(config);
    CHECK(text.find("single_chip") != std::string::npos);
    CHECK(text.find("VLEN=512") != std::string::npos);
    CHECK(text.find("vlenb=64") != std::string::npos);
    CHECK(text.find("hart 0") != std::string::npos);
    CHECK(text.find("hart 1") != std::string::npos);

    // The geometry must be named, and must not be the promotion target.
    CHECK(text.find("64x64 bring-up") != std::string::npos);
    CHECK(text.find("matrix source        : not integrated") != std::string::npos);

    // The timing backend and the arithmetic must be named in any output that
    // could be quoted: neither a latency nor a numeric result is interpretable
    // without them.
    CHECK(text.find("NoC timing backend") != std::string::npos);
    CHECK(text.find("matrix datatype      : bf16_fp32") != std::string::npos);
    CHECK(text.find("reference capacity") != std::string::npos);

    // D15's open physical values must appear with their provisional label
    // wherever they appear at all.
    CHECK(text.find("128-bit x 4 banks") != std::string::npos);
    CHECK(text.find("low_order_interleaved") != std::string::npos);
    CHECK(text.find("provisional") != std::string::npos);

    // Logical memory is address space, not a host commitment (D6).
    CHECK(text.find("sparsely page-backed") != std::string::npos);

    const std::string map = tpu::describe_address_map(config);
    CHECK(map.find("chip0.core1.sa_control") != std::string::npos);
    CHECK(map.find("chip0.core1.dma_control") != std::string::npos);
    CHECK(map.find("chip0.core1.transform_control") != std::string::npos);
    CHECK(map.find("chip0.core0.sram") != std::string::npos);
    CHECK(map.find("global.ram") != std::string::npos);
    // The legacy names must be gone from generated output, which is the
    // firmware-visible half of the Phase 3 migration gate.
    CHECK(map.find("svm") == std::string::npos);
    CHECK(map.find("mxu") == std::string::npos);
    // The reference core SRAM fully backs its window, so nothing is annotated;
    // global RAM at the 256 MiB default does not, so it is.
    CHECK(map.find("global.ram  [backed 256 MiB of 1 GiB") != std::string::npos);

    tpu::tpu_soc_config bringup = good_config();
    bringup.chip.core[0].sram_size_bytes = 1024 * 1024;
    bringup.chip.core[1].sram_size_bytes = 1024 * 1024;
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
    the_matrix_geometry_contract_is_enforced();
    an_available_transform_must_name_its_source();
    the_open_fabric_parameters_have_no_default();
    capacities_are_range_and_alignment_checked();
    noc_limits_are_enforced_at_configuration_time();
    enum_parsing_round_trips_and_rejects_typos();
    description_reports_what_it_ran();

    if (failures != 0) {
        std::cerr << failures << " check(s) failed\n";
        return 1;
    }
    std::cout << "test_architecture_config: all checks passed\n";
    return 0;
}
