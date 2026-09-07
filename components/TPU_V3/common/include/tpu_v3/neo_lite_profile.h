// SPDX-License-Identifier: Apache-2.0
//
// The canonical Neo Lite C1/C2 profile (decision record D28, work package WP1).
//
// D28 freezes two executable target profiles and states the rule this file
// exists to enforce: "Changing a manifest, a result-row default or a
// configuration ID while the component still uses another value is a
// configuration defect and must be refused." A profile is therefore not a set
// of labels. It is the values that select behaviour, in one place, with the
// fields that follow from them expressed as functions so nobody can set them
// to something the others contradict.
//
// **This header describes C1 and C2. It does not implement either.** The
// current model is the D27 reference machine: one DMA channel, a 64-bit
// external path, a 64x64 MXU and VLEN 512. Asking to run as C1 or C2 today is
// refused by `profile_disagreements()` below, and that refusal is WP1's main
// visible effect. WP2 through WP5 build the behaviour; WP7 and WP10 promote it.
//
// Deliberately independent of `tpu_soc_config`: the plan requires the D27
// standalone runner not to acquire a full-system configuration dependency, so
// this is a core-level object that a standalone core, a report or a benchmark
// runner can each consume on its own.

#ifndef CDC_COMPONENTS_TPU_V3_NEO_LITE_PROFILE_H
#define CDC_COMPONENTS_TPU_V3_NEO_LITE_PROFILE_H

#include <cstdint>
#include <string>
#include <vector>

#include "tpu_v3/types.h"

namespace cdc::components::tpu_v3 {

/// Which profile a build or a run claims to be.
///
/// `none` is the honest answer for the current machine: it is a C2 *compute*
/// baseline and not an executable C2 profile, differing on SRAM capacity, bank
/// geometry, DMA channels, AXI width, burst size and clock.
enum class neo_lite_profile_id {
    none,
    c1,
    c2,
};

const char* to_string(neo_lite_profile_id id) noexcept;
bool parse_neo_lite_profile_id(const std::string& text,
                               neo_lite_profile_id& out);

/// What one DMA channel is for.
///
/// C2's roles come from the HAS. C1 has no approved register/role
/// specification, so D28 supplies a proposed mapping and requires it to be
/// labelled provisional rather than presented as sourced hardware — which is
/// what `roles_are_provisional` carries into every report that prints them.
enum class dma_channel_role {
    weight_read,
    ifmap_read,
    output_write,
    config_read,
    /// C1's channel 0: input, weight and configuration reads reused in
    /// sequence, because C1 has two channels rather than four.
    shared_input_read,
};

const char* to_string(dma_channel_role role) noexcept;

/// One profile, in full.
///
/// Every member here selects behaviour. Anything derivable from these is a
/// function below and has no storage, so a manifest cannot state a PE count
/// that disagrees with the geometry that produced it.
struct neo_lite_profile {
    neo_lite_profile_id id = neo_lite_profile_id::none;

    // ── ISA ──────────────────────────────────────────────────────────────
    unsigned xlen = 32;
    unsigned vlen_bits = 0;
    unsigned elen_bits = 64;
    std::string rvv_version = "1.0";
    /// The firmware ISA/ABI this profile's images must be built with.
    std::string firmware_march;
    std::string firmware_mabi = "ilp32d";

    // ── MXU ──────────────────────────────────────────────────────────────
    unsigned mxu_rows = 0;
    unsigned mxu_columns = 0;
    matrix_datatype mxu_datatype = matrix_datatype::int8_int32;
    /// The named target in the pinned Sauria manifest, never a template
    /// default: D28 forbids falling through to one because it proves neither
    /// the selected source nor its index widths.
    std::string mxu_source_profile;

    // ── clock ────────────────────────────────────────────────────────────
    double core_period_ns = 0.0;

    // ── memory ───────────────────────────────────────────────────────────
    /// Exact, and not to be rounded to a power of two: D28 states that
    /// rounding 768 KiB or 1536 KiB to 1 or 2 MiB produces a different
    /// profile.
    std::uint64_t backed_sram_bytes = 0;
    unsigned local_bank_width_bits = 0;
    unsigned local_bank_count = 0;

    // ── DMA ──────────────────────────────────────────────────────────────
    /// Always one. D28 is explicit that "DMA channels" does not mean two or
    /// four independent DMA blocks: the core owns one controller and one
    /// external AXI boundary, and the channels are contexts inside it.
    unsigned dma_controllers = 1;
    std::vector<dma_channel_role> dma_channel_roles;
    bool dma_roles_are_provisional = false;

    // ── external path ────────────────────────────────────────────────────
    unsigned external_axi_width_bits = 0;
    /// A normal maximum burst is exactly eight beats.
    unsigned dma_burst_beats = 8;

    // ── Transform ────────────────────────────────────────────────────────
    unsigned transform_count = 1;
    bool im2col_available = true;
    bool col2im_available = false;

    // ── derived, never configured ────────────────────────────────────────

    unsigned dma_channels() const noexcept
    {
        return static_cast<unsigned>(dma_channel_roles.size());
    }
    std::uint64_t pe_count() const noexcept
    {
        return static_cast<std::uint64_t>(mxu_rows) * mxu_columns;
    }
    /// `1 / core period`, in MHz.
    double core_frequency_mhz() const noexcept
    {
        return core_period_ns > 0.0 ? 1000.0 / core_period_ns : 0.0;
    }
    std::uint64_t dma_burst_bytes() const noexcept
    {
        return static_cast<std::uint64_t>(external_axi_width_bits / 8)
            * dma_burst_beats;
    }
    /// VLEN in bytes, as firmware reads it from the CSR.
    unsigned vlenb() const noexcept { return vlen_bits / 8; }
    /// Bytes covered by one pass over every local bank.
    std::uint64_t local_stripe_bytes() const noexcept
    {
        return static_cast<std::uint64_t>(local_bank_width_bits / 8)
            * local_bank_count;
    }

    std::string mxu_geometry() const;

    /// Throws `std::invalid_argument` naming the field, the value and why.
    ///
    /// Validation covers the exact D28 values, the derivations above, and the
    /// arithmetic that produces them: D28 requires a width that is not
    /// byte-addressable and an overflow in either derivation to be rejected.
    void validate(const std::string& context) const;
};

/// The two profiles, exactly as D28 freezes them.
neo_lite_profile neo_lite_c1();
neo_lite_profile neo_lite_c2();
/// The profile a name selects. Throws for `none` and for an unknown name:
/// there is no factory for "not a profile".
neo_lite_profile neo_lite_profile_for(neo_lite_profile_id id);

/// What a live machine reports about itself.
///
/// Filled from instantiated components — the hart's `vlenb` CSR, the matrix
/// engine's identity, the fabric's configuration — never from the values that
/// were requested. D28's promotion gate requires exactly that distinction.
struct live_core_identity {
    unsigned vlenb = 0;
    unsigned mxu_rows = 0;
    unsigned mxu_columns = 0;
    std::string mxu_source_profile;
    unsigned dma_controllers = 0;
    unsigned dma_channels = 0;
    unsigned external_axi_width_bits = 0;
    std::uint64_t backed_sram_bytes = 0;
    unsigned local_bank_width_bits = 0;
    unsigned local_bank_count = 0;
    double core_period_ns = 0.0;
};

/// Every way a live machine fails to be the profile it claims.
///
/// Empty means the machine may carry that profile's identity. A non-empty
/// result must refuse the run **before** simulation: a row labelled with a
/// profile the components do not implement is the defect D28 names, and it is
/// worse than an unlabelled row because an aggregation groups by that label.
///
/// Returns every disagreement rather than the first, so one run says what the
/// whole gap is instead of revealing it one work package at a time.
std::vector<std::string> profile_disagreements(const neo_lite_profile& profile,
                                               const live_core_identity& live);

} // namespace cdc::components::tpu_v3

#endif // CDC_COMPONENTS_TPU_V3_NEO_LITE_PROFILE_H
