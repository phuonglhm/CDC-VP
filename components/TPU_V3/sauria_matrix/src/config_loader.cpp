// SPDX-License-Identifier: Apache-2.0

#include "tpu_v3/sauria/config_loader.h"

#include <iomanip>
#include <sstream>

#include "tpu_v3/sauria/sauria_geometry.h"

// The source's own encoder and address table. Included here and nowhere else,
// so everything above this file stays free of the Sauria headers.
#include "config_map.h"
#include "driver/libsauria_cfg.h"
#include "sauria_targets.h"

namespace cdc::components::tpu_v3::sauria {

namespace {

/// The pinned profile, looked up by name rather than constructed.
///
/// `sauria_targets.h` calls itself the single source of truth for geometry and
/// index widths, and `sauria_compute_core_fields()` reads X, Y and the three
/// index widths straight out of this structure. Building a `SauriaTarget` here
/// by hand would reintroduce exactly the transcription the configuration gate
/// exists to prevent.
const ::sauria::SauriaTarget& pinned_target()
{
    const ::sauria::SauriaTarget* target =
        ::sauria::sauria_find_target(profile_name);
    // The configuration gate already proved this profile exists and matches
    // Phase 5's parameters, at configure time. Reaching here with a null would
    // mean the compiled-in profile name and the compiled-in target table came
    // from different sources.
    if (target == nullptr) {
        SC_REPORT_FATAL("sauria_config_loader",
                        "the pinned profile is absent from sauria_targets.h");
    }
    return *target;
}

} // namespace

std::vector<config_write> build_config_writes(const job& work,
                                              std::uint32_t rows,
                                              std::uint32_t columns)
{
    const gemm_layer_desc gemm = describe_gemm(work, rows, columns);

    // The mirrored description becomes the source's own, field for field. This
    // assignment is the reason `gemm_layer_desc` keeps the source's field order
    // and names: a reader can check it against `SauriaLayerDesc` by eye.
    ::sauria::SauriaLayerDesc desc{};
    desc.B_w = gemm.kernel_width;
    desc.B_h = gemm.kernel_height;
    desc.d = gemm.dilation;
    desc.s = gemm.stride;
    desc.c_til = gemm.channels_in;
    desc.k_til = gemm.channels_out;
    desc.h_til = gemm.tile_height;
    desc.w_til = gemm.tile_width;
    desc.X_used = gemm.columns_used;
    desc.Y_used = gemm.rows_used;
    desc.preload_en = gemm.preload_enable;

    // Single tile: the full-tensor dimensions equal the tile's. Phase 5 refuses
    // a job larger than one pass of the array (`dimension_exceeds_array`), so
    // there is never more than one tile to describe.
    desc.C_w = gemm.tile_width;
    desc.C_h = gemm.tile_height;
    desc.C_c = gemm.channels_out;
    desc.A_c = gemm.channels_in;

    // Zero-initialised, and that is not defensive habit — it is required.
    //
    // `sauria_compute_core_fields()` does not populate every entry: the fields
    // belonging to blocks Phase 5 excludes are left exactly as it found them. An
    // uninitialised array therefore hands stack garbage to the register writes,
    // and because the garbage differs between calls the *same job* configures
    // the engine differently each time. That was observed: three registers, one
    // reading 17685856.
    std::uint64_t fields[::sauria::F_CFG_COUNT] = {};
    ::sauria::sauria_compute_core_fields(desc, pinned_target(), fields);

    std::vector<config_write> writes;
    writes.reserve(48);

    // Local helpers, so the sequence below reads as a list of registers rather
    // than as a list of `push_back`s.
    const auto reg = [&writes](std::uint32_t address, std::uint64_t value,
                               const char* name) {
        writes.push_back({address, static_cast<std::uint32_t>(value),
                          config_write::encoding::lane0, name});
    };
    const auto mask_reg = [&writes](std::uint32_t address, std::uint64_t value,
                                    const char* name) {
        writes.push_back({address, static_cast<std::uint32_t>(value),
                          config_write::encoding::byte_spread, name});
    };

    // Profile first — it selects which decode every following address lands in.
    reg(::sauria::CFG_PROFILE_ADDR,
        static_cast<std::uint32_t>(::sauria::PROFILE_V1_SAURIA), "PROFILE");

    // ── CONTROL ──────────────────────────────────────────────────────────────
    //
    // The `+ 1` is the source's, not a correction: the encoder's field is
    // `B_w * B_h * c_til - 1` and the register holds the *count*. The
    // testbench's `sysc_act_read_count = cfg.incntlim + 1` is where this comes
    // from, and getting it wrong costs exactly one activation read per context.
    reg(::sauria::CFG_CON_OFFSET + 0x00,
        fields[::sauria::F_CFG_INCNTLIM] + 1, "CON.INCNTLIM");
    reg(::sauria::CFG_CON_OFFSET + 0x04,
        fields[::sauria::F_CFG_ACT_REPS], "CON.ACT_REPS");
    reg(::sauria::CFG_CON_OFFSET + 0x08,
        fields[::sauria::F_CFG_WEI_REPS], "CON.WEI_REPS");
    // See the header: the encoder has no opinion about this one and the default
    // is a half-split array.
    reg(::sauria::CFG_CON_OFFSET + 0x14, rows, "CON.NSPLIT");

    // ── ACTIVATION ───────────────────────────────────────────────────────────
    mask_reg(::sauria::CFG_ACT_OFFSET + 0x00,
             fields[::sauria::F_CFG_ROWS_ACTIVE], "ACT.ROWS_ACTIVE");
    reg(::sauria::CFG_ACT_OFFSET + 0x04,
        fields[::sauria::F_CFG_INCNTLIM] + 1, "ACT.INCNTLIM");
    reg(::sauria::CFG_ACT_OFFSET + 0x08,
        fields[::sauria::F_CFG_XSTEP], "ACT.INCNTSTEP");
    reg(::sauria::CFG_ACT_OFFSET + 0x0C,
        fields[::sauria::F_CFG_XLIM], "ACT.OUTCNTLIM");
    reg(::sauria::CFG_ACT_OFFSET + 0x10,
        fields[::sauria::F_CFG_XSTEP], "ACT.OUTCNTSTEP");
    // `ConfigRegs` keeps only the low 32 bits of the dilation pattern. For a 1x1
    // kernel the pattern is a single set bit at the top of a 64-bit word, so the
    // low half is zero — which the feeder reads as "no dilation mask", the right
    // answer here. Truncating is the source's behaviour and is reproduced rather
    // than worked around.
    reg(::sauria::CFG_ACT_OFFSET + 0x28,
        fields[::sauria::F_CFG_DIL_PAT] & 0xFFFFFFFFULL, "ACT.DIL_PAT_LOW32");

    reg(::sauria::ACT_XLIM, fields[::sauria::F_CFG_XLIM], "ACT.XLIM");
    reg(::sauria::ACT_XSTEP, fields[::sauria::F_CFG_XSTEP], "ACT.XSTEP");
    reg(::sauria::ACT_YLIM, fields[::sauria::F_CFG_YLIM], "ACT.YLIM");
    reg(::sauria::ACT_YSTEP, fields[::sauria::F_CFG_YSTEP], "ACT.YSTEP");
    reg(::sauria::ACT_CHLIM, fields[::sauria::F_CFG_CHLIM], "ACT.CHLIM");
    reg(::sauria::ACT_CHSTEP, fields[::sauria::F_CFG_CHSTEP], "ACT.CHSTEP");
    reg(::sauria::ACT_TIL_XLIM, fields[::sauria::F_CFG_TIL_XLIM],
        "ACT.TIL_XLIM");
    reg(::sauria::ACT_TIL_XSTEP, fields[::sauria::F_CFG_TIL_XSTEP],
        "ACT.TIL_XSTEP");
    reg(::sauria::ACT_TIL_YLIM, fields[::sauria::F_CFG_TIL_YLIM],
        "ACT.TIL_YLIM");
    reg(::sauria::ACT_TIL_YSTEP, fields[::sauria::F_CFG_TIL_YSTEP],
        "ACT.TIL_YSTEP");

    // ── WEIGHT ───────────────────────────────────────────────────────────────
    reg(::sauria::CFG_WEI_OFFSET + 0x04, fields[::sauria::F_CFG_WLIM],
        "WEI.INCNTLIM");
    reg(::sauria::CFG_WEI_OFFSET + 0x08, fields[::sauria::F_CFG_WSTEP],
        "WEI.INCNTSTEP");
    reg(::sauria::WEI_WLIM, fields[::sauria::F_CFG_WLIM], "WEI.WLIM");
    reg(::sauria::WEI_WSTEP, fields[::sauria::F_CFG_WSTEP], "WEI.WSTEP");
    reg(::sauria::WEI_KLIM, fields[::sauria::F_CFG_KLIM], "WEI.KLIM");
    reg(::sauria::WEI_KSTEP, fields[::sauria::F_CFG_KSTEP], "WEI.KSTEP");
    reg(::sauria::WEI_TIL_XLIM, fields[::sauria::F_CFG_TIL_KLIM],
        "WEI.TIL_KLIM");
    reg(::sauria::WEI_TIL_XSTEP, fields[::sauria::F_CFG_TIL_KSTEP],
        "WEI.TIL_KSTEP");
    // Byte-spread, unlike the testbench's plain write. The column mask reaches
    // `0xFFFFFFFF` at 64 active columns, which a lane-0 write cannot carry
    // losslessly. Nothing in the kept modules reads this register — `i_wei_cols_active`
    // is declared on the weight feeder and never referenced — so this changes no
    // behaviour today; it is written correctly so that it does not become a
    // silent divergence if the feeder starts using it.
    mask_reg(::sauria::WEI_COLS_ACTIVE, fields[::sauria::F_CFG_COLS_ACTIVE],
             "WEI.COLS_ACTIVE");
    reg(::sauria::WEI_WALIGNED, fields[::sauria::F_CFG_WALIGNED],
        "WEI.WALIGNED");

    // ── OUTPUT / PSM ─────────────────────────────────────────────────────────
    reg(::sauria::NCONTEXTS, fields[::sauria::F_CFG_NCONTEXTS],
        "OUT.NCONTEXTS");
    reg(::sauria::CFG_OUT_OFFSET + 0x04, fields[::sauria::F_CFG_CXLIM],
        "OUT.CXLIM");
    reg(::sauria::CFG_OUT_OFFSET + 0x08, fields[::sauria::F_CFG_CXSTEP],
        "OUT.CXSTEP");
    reg(::sauria::CFG_OUT_OFFSET + 0x0C, fields[::sauria::F_CFG_CKLIM],
        "OUT.CKLIM");
    reg(::sauria::CFG_OUT_OFFSET + 0x10, fields[::sauria::F_CFG_CKSTEP],
        "OUT.CKSTEP");
    reg(::sauria::TIL_CYLIM, fields[::sauria::F_CFG_TIL_CYLIM],
        "OUT.TIL_CYLIM");
    reg(::sauria::TIL_CYSTEP, fields[::sauria::F_CFG_TIL_CYSTEP],
        "OUT.TIL_CYSTEP");
    reg(::sauria::TIL_CKLIM, fields[::sauria::F_CFG_TIL_CKLIM],
        "OUT.TIL_CKLIM");
    reg(::sauria::TIL_CKSTEP, fields[::sauria::F_CFG_TIL_CKSTEP],
        "OUT.TIL_CKSTEP");
    reg(::sauria::INACTIVE_COLS, fields[::sauria::F_CFG_INACTIVE_COLS],
        "OUT.INACTIVE_COLS");
    reg(::sauria::PRELOAD_EN, fields[::sauria::F_CFG_PRELOAD_EN],
        "OUT.PRELOAD_EN");

    // Registers belonging to excluded blocks are absent rather than zeroed.
    //
    // The OBP configuration and its requantisation scale and shift live in this
    // address range, and the encoder has no fields for them because it does not
    // configure that block either. Writing them would configure something that
    // is not in the composition, and would leave a register trace implying it is.

    // ── base addresses ───────────────────────────────────────────────────────
    //
    // *Staging-store* indices, not core-SRAM addresses. The controllers place
    // each tile at the start of its store, so the engine always reads from zero;
    // where the tile came from in core SRAM is the prefetch controller's business
    // and is deliberately invisible here. This is the one place the adapter's
    // addressing differs from `NpuTop`'s, and it is a consequence of D17: with
    // staging, the array never sees a system address at all.
    reg(::sauria::CFG_ACT_BASE_ADDR, 0, "ACT.BASE_ADDR");
    reg(::sauria::CFG_WEI_BASE_ADDR, 0, "WEI.BASE_ADDR");
    reg(::sauria::CFG_OUT_BASE_ADDR, 0, "OUT.BASE_ADDR");

    return writes;
}

std::string describe_config_writes(const std::vector<config_write>& writes)
{
    std::ostringstream out;
    for (const auto& write : writes) {
        out << "  " << std::setw(18) << std::left << write.name << " @0x"
            << std::right << std::hex << std::setw(4) << std::setfill('0')
            << write.address << std::setfill(' ') << std::dec << " = "
            << write.value
            << (write.how == config_write::encoding::byte_spread
                    ? "  (byte-spread)"
                    : "")
            << '\n';
    }
    return out.str();
}

} // namespace cdc::components::tpu_v3::sauria
