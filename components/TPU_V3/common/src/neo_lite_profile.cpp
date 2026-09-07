// SPDX-License-Identifier: Apache-2.0

#include "tpu_v3/neo_lite_profile.h"

#include <limits>
#include <sstream>
#include <stdexcept>

namespace cdc::components::tpu_v3 {
namespace {

[[noreturn]] void reject(const std::string& context, const char* field,
                         const std::string& value, const std::string& why)
{
    std::ostringstream message;
    message << context << ": " << field << " = " << value << " is invalid; "
            << why;
    throw std::invalid_argument(message.str());
}

void require(const std::string& context, const char* field, std::uint64_t value,
             std::uint64_t expected, const char* why)
{
    if (value != expected) {
        reject(context, field, std::to_string(value),
               std::string(why) + " (D28 requires "
                   + std::to_string(expected) + ")");
    }
}

} // namespace

const char* to_string(neo_lite_profile_id id) noexcept
{
    switch (id) {
    case neo_lite_profile_id::none: return "none";
    case neo_lite_profile_id::c1: return "neo_lite_c1";
    case neo_lite_profile_id::c2: return "neo_lite_c2";
    }
    return "unknown";
}

bool parse_neo_lite_profile_id(const std::string& text,
                               neo_lite_profile_id& out)
{
    for (auto candidate : {neo_lite_profile_id::none, neo_lite_profile_id::c1,
                           neo_lite_profile_id::c2}) {
        if (text == to_string(candidate)) {
            out = candidate;
            return true;
        }
    }
    // The CMake option spells them C1/C2; accept those too so a build option
    // and a runtime flag do not need two vocabularies.
    if (text == "C1") {
        out = neo_lite_profile_id::c1;
        return true;
    }
    if (text == "C2") {
        out = neo_lite_profile_id::c2;
        return true;
    }
    return false;
}

const char* to_string(dma_channel_role role) noexcept
{
    switch (role) {
    case dma_channel_role::weight_read: return "weight_read";
    case dma_channel_role::ifmap_read: return "ifmap_read";
    case dma_channel_role::output_write: return "output_write";
    case dma_channel_role::config_read: return "config_read";
    case dma_channel_role::shared_input_read: return "shared_input_read";
    }
    return "unknown";
}

std::string neo_lite_profile::mxu_geometry() const
{
    return std::to_string(mxu_rows) + "x" + std::to_string(mxu_columns);
}

void neo_lite_profile::validate(const std::string& context) const
{
    if (id == neo_lite_profile_id::none) {
        reject(context, "id", "none",
               "there is no profile to validate; `none` names the absence of "
               "one, not a third target");
    }

    // Common to both profiles.
    require(context, "xlen", xlen, 32, "D28 freezes RV32");
    require(context, "elen_bits", elen_bits, 64, "D28 freezes ELEN");
    if (rvv_version != "1.0") {
        reject(context, "rvv_version", rvv_version,
               "the frozen vector specification is RISC-V V 1.0");
    }
    if (mxu_datatype != matrix_datatype::int8_int32) {
        reject(context, "mxu_datatype", to_string(mxu_datatype),
               "both Neo Lite profiles are INT8 multiply with INT32 "
               "accumulation; the arithmetic is a fixed profile capability, "
               "not a free knob");
    }
    require(context, "dma_controllers", dma_controllers, 1,
            "the core owns one DMA controller however many channel contexts "
            "it holds");
    require(context, "transform_count", transform_count, 1,
            "one Im2Col Transform block per core");
    if (!im2col_available || col2im_available) {
        reject(context, "transform capability",
               std::string("im2col=") + (im2col_available ? "1" : "0")
                   + " col2im=" + (col2im_available ? "1" : "0"),
               "D18 leaves Col2Im unavailable and D28 does not reopen it");
    }
    if (core_period_ns != 1.25) {
        reject(context, "core_period_ns", std::to_string(core_period_ns),
               "both profiles run at 800 MHz, a canonical 1.25 ns period");
    }
    require(context, "dma_burst_beats", dma_burst_beats, 8,
            "a normal maximum burst is exactly eight beats");

    // Byte-addressability, before anything divides by eight. D28 requires a
    // width that is not byte-addressable to be rejected rather than truncated.
    for (auto [field, bits] :
         {std::pair<const char*, unsigned>{"vlen_bits", vlen_bits},
          {"external_axi_width_bits", external_axi_width_bits},
          {"local_bank_width_bits", local_bank_width_bits}}) {
        if (bits == 0 || bits % 8 != 0) {
            reject(context, field, std::to_string(bits),
                   "a width must be a non-zero whole number of bytes, or the "
                   "byte figures derived from it are a truncation rather than "
                   "a conversion");
        }
    }

    // The two profiles, exactly.
    if (id == neo_lite_profile_id::c1) {
        require(context, "vlen_bits", vlen_bits, 256, "C1 is VLEN 256");
        require(context, "mxu_rows", mxu_rows, 32, "C1 is a 32x32 array");
        require(context, "mxu_columns", mxu_columns, 32, "C1 is a 32x32 array");
        require(context, "backed_sram_bytes", backed_sram_bytes, 786432,
                "C1 backs exactly 768 KiB; rounding it to 1 MiB is a different "
                "profile");
        require(context, "local_bank_width_bits", local_bank_width_bits, 128,
                "C1 has 4 banks of 128 bits");
        require(context, "local_bank_count", local_bank_count, 4,
                "C1 has 4 banks of 128 bits");
        require(context, "dma_channels", dma_channels(), 2,
                "C1's controller owns two channel contexts");
        require(context, "external_axi_width_bits", external_axi_width_bits,
                128, "C1's external path is 128 bits");
        if (mxu_source_profile != "int8_32x32") {
            reject(context, "mxu_source_profile", mxu_source_profile,
                   "C1's array must come from the named int8_32x32 target in "
                   "the pinned manifest; a C++ template default is forbidden "
                   "because it proves neither the target nor its index widths");
        }
        if (firmware_march != "rv32gcv_zvl256b") {
            reject(context, "firmware_march", firmware_march,
                   "C1 firmware is built for a 256-bit minimum vector length");
        }
        if (!dma_roles_are_provisional) {
            reject(context, "dma_roles_are_provisional", "false",
                   "D28 supplies C1's channel mapping as a proposal and "
                   "requires it to be labelled provisional until a C1 "
                   "register/role specification is approved");
        }
    } else {
        require(context, "vlen_bits", vlen_bits, 512, "C2 is VLEN 512");
        require(context, "mxu_rows", mxu_rows, 64, "C2 is a 64x64 array");
        require(context, "mxu_columns", mxu_columns, 64, "C2 is a 64x64 array");
        require(context, "backed_sram_bytes", backed_sram_bytes, 1572864,
                "C2 backs exactly 1536 KiB; rounding it to 2 MiB is a "
                "different profile");
        require(context, "local_bank_width_bits", local_bank_width_bits, 256,
                "C2 has 8 banks of 256 bits");
        require(context, "local_bank_count", local_bank_count, 8,
                "C2 has 8 banks of 256 bits");
        require(context, "dma_channels", dma_channels(), 4,
                "C2's controller owns four channel contexts");
        require(context, "external_axi_width_bits", external_axi_width_bits,
                256, "C2's external path is 256 bits");
        if (mxu_source_profile != "int8_64x64") {
            reject(context, "mxu_source_profile", mxu_source_profile,
                   "C2 retains the verified int8_64x64 profile");
        }
        if (firmware_march != "rv32gcv_zvl512b") {
            reject(context, "firmware_march", firmware_march,
                   "C2 firmware is built for a 512-bit minimum vector length");
        }
        if (dma_roles_are_provisional) {
            reject(context, "dma_roles_are_provisional", "true",
                   "C2's channel roles come from the HAS and are not "
                   "provisional");
        }
    }

    // The derivations, checked rather than assumed.
    if (vlenb() * 8u != vlen_bits) {
        reject(context, "vlenb", std::to_string(vlenb()),
               "vlenb must be VLEN/8 exactly");
    }
    // The stripe and the SRAM must fit together: a capacity that is not a
    // whole number of physical stripes cannot be backed by the banks that
    // serve it (D28's capacity gate).
    const std::uint64_t stripe = local_stripe_bytes();
    if (stripe == 0 || backed_sram_bytes % stripe != 0) {
        reject(context, "backed_sram_bytes", std::to_string(backed_sram_bytes),
               "the backed capacity must be a whole number of physical "
               "stripes (" + std::to_string(stripe) + " bytes)");
    }
    // Overflow in either derivation. Both are small today; the check exists
    // because D28 asks for it and because the next profile may not be.
    if (mxu_rows != 0
        && pe_count() / mxu_rows != mxu_columns) {
        reject(context, "pe_count", "overflow",
               "rows * columns does not fit the derivation's type");
    }
    if (dma_burst_beats != 0
        && dma_burst_bytes() / dma_burst_beats
            != external_axi_width_bits / 8u) {
        reject(context, "dma_burst_bytes", "overflow",
               "width/8 * beats does not fit the derivation's type");
    }
}

neo_lite_profile neo_lite_c1()
{
    neo_lite_profile profile;
    profile.id = neo_lite_profile_id::c1;
    profile.vlen_bits = 256;
    profile.firmware_march = "rv32gcv_zvl256b";
    profile.mxu_rows = 32;
    profile.mxu_columns = 32;
    profile.mxu_source_profile = "int8_32x32";
    profile.core_period_ns = 1.25;
    profile.backed_sram_bytes = 768u * 1024u;
    profile.local_bank_width_bits = 128;
    profile.local_bank_count = 4;
    // D28's proposed mapping, and the reason it is flagged: no C1
    // register/role specification has been approved, so CH0 does the input,
    // weight and configuration reads in sequence.
    profile.dma_channel_roles = {dma_channel_role::shared_input_read,
                                 dma_channel_role::output_write};
    profile.dma_roles_are_provisional = true;
    profile.external_axi_width_bits = 128;
    return profile;
}

neo_lite_profile neo_lite_c2()
{
    neo_lite_profile profile;
    profile.id = neo_lite_profile_id::c2;
    profile.vlen_bits = 512;
    profile.firmware_march = "rv32gcv_zvl512b";
    profile.mxu_rows = 64;
    profile.mxu_columns = 64;
    profile.mxu_source_profile = "int8_64x64";
    profile.core_period_ns = 1.25;
    profile.backed_sram_bytes = 1536u * 1024u;
    profile.local_bank_width_bits = 256;
    profile.local_bank_count = 8;
    // The HAS roles, in channel order.
    profile.dma_channel_roles = {
        dma_channel_role::weight_read, dma_channel_role::ifmap_read,
        dma_channel_role::output_write, dma_channel_role::config_read};
    profile.external_axi_width_bits = 256;
    return profile;
}

neo_lite_profile neo_lite_profile_for(neo_lite_profile_id id)
{
    switch (id) {
    case neo_lite_profile_id::c1: return neo_lite_c1();
    case neo_lite_profile_id::c2: return neo_lite_c2();
    case neo_lite_profile_id::none:
        break;
    }
    throw std::invalid_argument(
        "neo_lite_profile_for: `none` names the absence of a profile, so there "
        "is no factory for it. The D27 reference machine is not a Neo Lite "
        "profile and must not be given one's identity (D28)");
}

std::vector<std::string> profile_disagreements(const neo_lite_profile& profile,
                                               const live_core_identity& live)
{
    std::vector<std::string> problems;
    const auto compare = [&](const char* field, std::uint64_t actual,
                             std::uint64_t expected, const char* owner) {
        if (actual != expected) {
            problems.push_back(
                std::string(field) + ": the machine reports "
                + std::to_string(actual) + ", "
                + to_string(profile.id) + " requires "
                + std::to_string(expected) + " (" + owner + ")");
        }
    };

    compare("vlenb", live.vlenb, profile.vlenb(),
            "WP9 builds the VP++ profile that changes this");
    compare("mxu_rows", live.mxu_rows, profile.mxu_rows,
            "WP8 extracts the named source profile");
    compare("mxu_columns", live.mxu_columns, profile.mxu_columns,
            "WP8 extracts the named source profile");
    compare("dma_controllers", live.dma_controllers, profile.dma_controllers,
            "the core owns exactly one");
    compare("dma_channels", live.dma_channels, profile.dma_channels(),
            "WP4 builds the multi-channel controller");
    compare("external_axi_width_bits", live.external_axi_width_bits,
            profile.external_axi_width_bits,
            "WP3 parameterises the external path");
    compare("backed_sram_bytes", live.backed_sram_bytes,
            profile.backed_sram_bytes, "WP2 allows an exact capacity");
    compare("local_bank_width_bits", live.local_bank_width_bits,
            profile.local_bank_width_bits, "a configuration value today");
    compare("local_bank_count", live.local_bank_count, profile.local_bank_count,
            "a configuration value today");

    if (live.mxu_source_profile != profile.mxu_source_profile) {
        problems.push_back(
            "mxu_source_profile: the machine reports '"
            + (live.mxu_source_profile.empty() ? std::string("(none)")
                                               : live.mxu_source_profile)
            + "', " + to_string(profile.id) + " requires '"
            + profile.mxu_source_profile
            + "' (WP8; a template default is forbidden)");
    }
    if (live.core_period_ns != profile.core_period_ns) {
        problems.push_back(
            "core_period_ns: the machine reports "
            + std::to_string(live.core_period_ns) + ", "
            + to_string(profile.id) + " requires "
            + std::to_string(profile.core_period_ns)
            + " (WP5 propagates it to every timing consumer)");
    }
    return problems;
}

} // namespace cdc::components::tpu_v3
