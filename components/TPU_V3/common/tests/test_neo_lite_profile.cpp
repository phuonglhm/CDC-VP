// SPDX-License-Identifier: Apache-2.0
//
// The canonical Neo Lite profile (D28, WP1).
//
// Every expected value below is transcribed from D28's table rather than read
// from the factory it checks. A test that asks the code what it produced and
// then agrees with it would pass through any edit, including the one that
// rounds 768 KiB to 1 MiB — which D28 singles out as producing a different
// profile rather than a rounded one.

#include "tpu_v3/neo_lite_profile.h"

#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace tpu = cdc::components::tpu_v3;

namespace {

int failures = 0;

#define CHECK_MSG(cond, msg)                                                  \
    do {                                                                      \
        if (!(cond)) {                                                        \
            std::cerr << "CHECK failed: " << (msg) << " @ " << __FILE__       \
                      << ':' << __LINE__ << '\n';                             \
            ++failures;                                                       \
        }                                                                     \
    } while (0)

using tpu::dma_channel_role;
using tpu::live_core_identity;
using tpu::neo_lite_profile;
using tpu::neo_lite_profile_id;

/// D28's table, written out. C1 first.
void c1_holds_exactly_the_decided_values()
{
    const auto p = tpu::neo_lite_c1();
    CHECK_MSG(p.id == neo_lite_profile_id::c1, "C1 has the wrong id");
    CHECK_MSG(p.xlen == 32 && p.elen_bits == 64 && p.rvv_version == "1.0",
              "C1's ISA basics drifted");
    CHECK_MSG(p.vlen_bits == 256, "C1 is VLEN 256");
    CHECK_MSG(p.vlenb() == 32, "C1 reads vlenb 32");
    CHECK_MSG(p.firmware_march == "rv32gcv_zvl256b", "C1 firmware ISA drifted");
    CHECK_MSG(p.mxu_rows == 32 && p.mxu_columns == 32, "C1 is a 32x32 array");
    CHECK_MSG(p.pe_count() == 1024, "C1 has 1024 PEs");
    CHECK_MSG(p.mxu_source_profile == "int8_32x32",
              "C1 must name the pinned source target, not a template default");
    CHECK_MSG(p.core_period_ns == 1.25, "C1 runs at a 1.25 ns period");
    CHECK_MSG(p.backed_sram_bytes == 786432,
              "C1 backs exactly 768 KiB (786432 bytes), never 1 MiB");
    CHECK_MSG(p.local_bank_width_bits == 128 && p.local_bank_count == 4,
              "C1 has 4 banks of 128 bits");
    CHECK_MSG(p.dma_controllers == 1 && p.dma_channels() == 2,
              "C1 has one controller with two channel contexts");
    CHECK_MSG(p.external_axi_width_bits == 128, "C1's external path is 128 bits");
    CHECK_MSG(p.dma_burst_beats == 8 && p.dma_burst_bytes() == 128,
              "C1's normal maximum burst is 8 x 16 bytes");
    CHECK_MSG(p.transform_count == 1 && p.im2col_available
                  && !p.col2im_available,
              "C1 has one Im2Col Transform and no Col2Im");

    // The provisional flag is a requirement, not a note: D28 refuses to let
    // C1's channel mapping be presented as sourced hardware.
    CHECK_MSG(p.dma_roles_are_provisional,
              "C1's channel roles must be labelled provisional until a C1 "
              "register/role specification is approved");
    const std::vector<dma_channel_role> c1_roles{
        dma_channel_role::shared_input_read, dma_channel_role::output_write};
    CHECK_MSG(p.dma_channel_roles == c1_roles,
              "C1's proposed mapping is CH0 shared input read, CH1 output write");
}

void c2_holds_exactly_the_decided_values()
{
    const auto p = tpu::neo_lite_c2();
    CHECK_MSG(p.id == neo_lite_profile_id::c2, "C2 has the wrong id");
    CHECK_MSG(p.vlen_bits == 512 && p.vlenb() == 64, "C2 is VLEN 512");
    CHECK_MSG(p.firmware_march == "rv32gcv_zvl512b", "C2 firmware ISA drifted");
    CHECK_MSG(p.mxu_rows == 64 && p.mxu_columns == 64, "C2 is a 64x64 array");
    CHECK_MSG(p.pe_count() == 4096, "C2 has 4096 PEs");
    CHECK_MSG(p.mxu_source_profile == "int8_64x64",
              "C2 retains the verified source profile");
    CHECK_MSG(p.core_period_ns == 1.25, "C2 runs at a 1.25 ns period");
    CHECK_MSG(p.backed_sram_bytes == 1572864,
              "C2 backs exactly 1536 KiB (1572864 bytes), never 2 MiB");
    CHECK_MSG(p.local_bank_width_bits == 256 && p.local_bank_count == 8,
              "C2 has 8 banks of 256 bits");
    CHECK_MSG(p.dma_controllers == 1 && p.dma_channels() == 4,
              "C2 has one controller with four channel contexts");
    CHECK_MSG(p.external_axi_width_bits == 256, "C2's external path is 256 bits");
    CHECK_MSG(p.dma_burst_bytes() == 256,
              "C2's normal maximum burst is 8 x 32 bytes");
    CHECK_MSG(!p.dma_roles_are_provisional,
              "C2's channel roles come from the HAS and are not provisional");
    const std::vector<dma_channel_role> c2_roles{
        dma_channel_role::weight_read, dma_channel_role::ifmap_read,
        dma_channel_role::output_write, dma_channel_role::config_read};
    CHECK_MSG(p.dma_channel_roles == c2_roles,
              "C2's HAS roles are CH0 weight, CH1 IFmap, CH2 output, CH3 config");
}

void both_profiles_validate()
{
    tpu::neo_lite_c1().validate("C1");
    tpu::neo_lite_c2().validate("C2");
}

void derived_fields_follow_their_inputs()
{
    auto p = tpu::neo_lite_c2();
    CHECK_MSG(p.core_frequency_mhz() == 800.0,
              "1.25 ns must derive 800 MHz");
    CHECK_MSG(p.local_stripe_bytes() == 256, "8 banks x 32 bytes is 256");

    // Change an input, and every derived figure moves with it. That is the
    // property having no stored derived fields buys.
    p.mxu_columns = 32;
    CHECK_MSG(p.pe_count() == 2048, "PE count did not follow the geometry");
    p.external_axi_width_bits = 128;
    CHECK_MSG(p.dma_burst_bytes() == 128,
              "burst bytes did not follow the external width");
}

bool refuses(const neo_lite_profile& profile, const char* what)
{
    try {
        profile.validate("mutation");
    } catch (const std::invalid_argument&) {
        return true;
    }
    std::cerr << "CHECK failed: validate accepted " << what << '\n';
    ++failures;
    return false;
}

/// One field swapped at a time. Each is a value that would produce a machine
/// D28 does not authorise, so each must be refused rather than validated.
void a_single_swapped_field_is_refused()
{
    {
        auto p = tpu::neo_lite_c1();
        p.backed_sram_bytes = 1024u * 1024u;
        refuses(p, "C1 with its SRAM rounded up to 1 MiB");
    }
    {
        auto p = tpu::neo_lite_c2();
        p.backed_sram_bytes = 2u * 1024u * 1024u;
        refuses(p, "C2 with its SRAM rounded up to 2 MiB");
    }
    {
        auto p = tpu::neo_lite_c1();
        p.vlen_bits = 512;
        refuses(p, "C1 carrying C2's VLEN");
    }
    {
        auto p = tpu::neo_lite_c1();
        p.mxu_rows = 64;
        p.mxu_columns = 64;
        refuses(p, "C1 carrying C2's array");
    }
    {
        auto p = tpu::neo_lite_c1();
        p.mxu_source_profile = "int8_64x64";
        refuses(p, "C1 pointing at C2's source target");
    }
    {
        auto p = tpu::neo_lite_c1();
        p.mxu_source_profile.clear();
        refuses(p, "C1 with no named source target");
    }
    {
        auto p = tpu::neo_lite_c1();
        p.dma_roles_are_provisional = false;
        refuses(p, "C1 presenting its proposed channel roles as sourced");
    }
    {
        auto p = tpu::neo_lite_c2();
        p.dma_channel_roles.pop_back();
        refuses(p, "C2 with three channels");
    }
    {
        auto p = tpu::neo_lite_c2();
        p.dma_controllers = 4;
        refuses(p, "four DMA controllers rather than four channels");
    }
    {
        auto p = tpu::neo_lite_c2();
        p.core_period_ns = 10.0;
        refuses(p, "a Neo Lite profile at the reference machine's clock");
    }
    {
        auto p = tpu::neo_lite_c2();
        p.mxu_datatype = tpu::matrix_datatype::bf16_fp32;
        refuses(p, "a Neo Lite profile with BF16 arithmetic");
    }
    {
        auto p = tpu::neo_lite_c2();
        p.col2im_available = true;
        refuses(p, "a profile advertising Col2Im");
    }
    {
        auto p = tpu::neo_lite_c2();
        p.firmware_march = "rv32gcv_zvl256b";
        refuses(p, "C2 built against C1's firmware ISA");
    }
}

/// D28 requires a width that is not byte-addressable to be rejected, and the
/// capacity to be a whole number of physical stripes.
void widths_and_capacities_must_be_whole()
{
    {
        auto p = tpu::neo_lite_c2();
        p.external_axi_width_bits = 255;
        refuses(p, "an external width that is not a whole number of bytes");
    }
    {
        auto p = tpu::neo_lite_c2();
        p.vlen_bits = 0;
        refuses(p, "a zero VLEN");
    }
    {
        auto p = tpu::neo_lite_c2();
        p.local_bank_width_bits = 12;
        refuses(p, "a bank width that is not a whole number of bytes");
    }
}

/// `none` is the absence of a profile, and must behave like one everywhere.
void none_is_not_a_third_profile()
{
    neo_lite_profile empty;
    CHECK_MSG(empty.id == neo_lite_profile_id::none,
              "a default-constructed profile must be `none`");
    refuses(empty, "the absence of a profile as though it were one");

    bool threw = false;
    try {
        tpu::neo_lite_profile_for(neo_lite_profile_id::none);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    CHECK_MSG(threw, "there must be no factory for `none`");
}

void names_round_trip_in_both_spellings()
{
    for (auto id : {neo_lite_profile_id::none, neo_lite_profile_id::c1,
                    neo_lite_profile_id::c2}) {
        neo_lite_profile_id parsed{};
        CHECK_MSG(tpu::parse_neo_lite_profile_id(tpu::to_string(id), parsed)
                      && parsed == id,
                  "a profile name did not round-trip");
    }
    neo_lite_profile_id parsed{};
    // The CMake option spells them C1/C2; one vocabulary, two spellings.
    CHECK_MSG(tpu::parse_neo_lite_profile_id("C1", parsed)
                  && parsed == neo_lite_profile_id::c1,
              "the build option's spelling of C1 is not accepted");
    CHECK_MSG(tpu::parse_neo_lite_profile_id("C2", parsed)
                  && parsed == neo_lite_profile_id::c2,
              "the build option's spelling of C2 is not accepted");
    CHECK_MSG(!tpu::parse_neo_lite_profile_id("c3", parsed),
              "an unknown profile name was accepted");
    CHECK_MSG(!tpu::parse_neo_lite_profile_id("", parsed),
              "an empty profile name was accepted");
}

live_core_identity live_for(const neo_lite_profile& p)
{
    live_core_identity live;
    live.vlenb = p.vlenb();
    live.mxu_rows = p.mxu_rows;
    live.mxu_columns = p.mxu_columns;
    live.mxu_source_profile = p.mxu_source_profile;
    live.dma_controllers = p.dma_controllers;
    live.dma_channels = p.dma_channels();
    live.external_axi_width_bits = p.external_axi_width_bits;
    live.backed_sram_bytes = p.backed_sram_bytes;
    live.local_bank_width_bits = p.local_bank_width_bits;
    live.local_bank_count = p.local_bank_count;
    live.core_period_ns = p.core_period_ns;
    return live;
}

/// The gate that stops a label outrunning the machine.
void a_matching_machine_carries_the_profile()
{
    for (auto factory : {&tpu::neo_lite_c1, &tpu::neo_lite_c2}) {
        const auto p = factory();
        CHECK_MSG(tpu::profile_disagreements(p, live_for(p)).empty(),
                  "a machine matching its profile was rejected");
    }
}

void every_disagreement_is_reported_not_just_the_first()
{
    const auto c2 = tpu::neo_lite_c2();
    auto live = live_for(c2);
    live.vlenb = 32;
    live.dma_channels = 1;
    live.external_axi_width_bits = 64;
    const auto problems = tpu::profile_disagreements(c2, live);
    CHECK_MSG(problems.size() == 3,
              "expected three disagreements, got "
                  + std::to_string(problems.size())
                  + "; one run must say what the whole gap is rather than "
                    "revealing it one work package at a time");
}

/// The state that matters today: the D27 reference machine asked to be C1 or
/// C2. Every profile field it does not implement must be named.
void the_current_machine_is_neither_profile()
{
    live_core_identity reference;
    reference.vlenb = 64;
    reference.mxu_rows = 64;
    reference.mxu_columns = 64;
    reference.mxu_source_profile = "int8_64x64";
    reference.dma_controllers = 1;
    reference.dma_channels = 1;
    reference.external_axi_width_bits = 64;
    reference.backed_sram_bytes = 16u * 1024u * 1024u;
    reference.local_bank_width_bits = 128;
    reference.local_bank_count = 4;
    reference.core_period_ns = 10.0;

    const auto against_c2
        = tpu::profile_disagreements(tpu::neo_lite_c2(), reference);
    CHECK_MSG(!against_c2.empty(),
              "the reference machine was accepted as C2; it shares only the "
              "MXU and VLEN with it");
    const auto against_c1
        = tpu::profile_disagreements(tpu::neo_lite_c1(), reference);
    CHECK_MSG(against_c1.size() > against_c2.size(),
              "the reference machine must disagree with C1 in more ways than "
              "with C2: it already has C2's array and VLEN");
}

} // namespace

int main()
{
    c1_holds_exactly_the_decided_values();
    c2_holds_exactly_the_decided_values();
    both_profiles_validate();
    derived_fields_follow_their_inputs();
    a_single_swapped_field_is_refused();
    widths_and_capacities_must_be_whole();
    none_is_not_a_third_profile();
    names_round_trip_in_both_spellings();
    a_matching_machine_carries_the_profile();
    every_disagreement_is_reported_not_just_the_first();
    the_current_machine_is_neither_profile();

    if (failures != 0) {
        std::cerr << failures << " check(s) failed\n";
        return 1;
    }
    std::cout << "test_neo_lite_profile: all checks passed\n";
    return 0;
}
