// SPDX-License-Identifier: Apache-2.0
//
// Loading a translated job into the Sauria configuration registers.
//
// This closes the last link of the chain `TPU_V3_PHASE5_AUDIT.md` §4 describes:
//
// ```text
//   job {M, N, K, addresses, strides}
//     -> gemm_layer_desc                  (gemm_config.h)
//     -> SauriaLayerDesc                  (here)
//     -> sauria_compute_core_fields()     the source's own encoder
//     -> the V1 register sequence         (here)
//     -> ConfigRegs host port             (here)
//     -> the ~107 config signals
// ```
//
// `sauria_compute_core_fields()` is the source's and stays the source's: it
// computes every loop bound and step from the layer description, and rederiving
// those by hand would mean maintaining a second copy of arithmetic the NPU team
// owns.
//
// ## Why the *addresses* are not taken from the source's map, though
//
// The first version of this file drove the writes from `V4_CFG_MAP`, on the
// principle that a table shipped with the source beats a transcribed one. That
// was wrong three times over, and the reasons are worth keeping because each one
// looks like the safe choice until you check it.
//
//   1. **The two field enumerations are different types.** `RegMapEntry::field`
//      is `sauria::CfgField` (`config_map.h`), the union of every field either
//      profile has. `sauria_compute_core_fields()` fills an array indexed by
//      `sauria::CfgFieldId` (`sauria_cfg_layout.h`), the packed-bitstream field
//      list. They share a prefix convention and nothing else. Indexing the
//      second with the first compiles silently — both are enums with implicit
//      integer conversions — gives every register some unrelated field's value,
//      and reads past the end of the array for any `CfgField` beyond
//      `F_CFG_COUNT`. The out-of-bounds tail is where the stack garbage that
//      first exposed this came from; the in-range majority was quietly wrong
//      with no symptom at all.
//   2. **The pinned golden case runs the *other* profile.** `ConfigRegs`
//      defaults to `PROFILE_V1_SAURIA` and `tb_evaluate.cpp` never writes the
//      profile register, so `demo_gemm_64x64` was captured through the V1 map
//      and the V1 addressing behaviour — described in `npu_profile.h` as the
//      bit-exact path. `V4_CFG_MAP` is the legacy linear one.
//   3. **`V1_CFG_MAP` is not a function.** Several addresses appear twice in it
//      (`CFG_OUT_OFFSET + 0x24` is both `F_INACTIVE_COLS` and
//      `F_REQUANT_SCALE_A`), so which field an address means is decided by
//      `cfg_lookup`'s first-match order. A loader driven from the map would
//      depend on that ordering without saying so.
//
// And the values were never the raw fields anyway: the register takes
// `incntlim + 1`, `dil_pat` is truncated to its low 32 bits, and the row mask is
// carried one byte per host lane. So this file reproduces the sequence
// `tb_evaluate.cpp::apply_decoded_config_to_npu()` writes — the sequence the
// golden case was actually captured through — taking every address from the
// named constants in `sauria_types.h` rather than from literals.
//
// ## The write protocol is the testbench's
//
// `ConfigRegs` decodes `i_host_addr` and takes the value from lane 0 of the
// 4-lane host bus, gated by lane 0 of the mask. `tb_unified_smoke.cpp`'s `wr()`
// is the reference sequence and this reproduces it: drive address, data and
// mask, raise `wren` for one clock, lower it, and let two clocks pass before
// the next write.
//
// The two trailing waits are not padding. `ConfigRegs` is clocked and its
// outputs are `sc_signal`s, so a config value written on one edge reaches the
// feeders on the next; issuing the following write immediately would be
// harmless here but would make the loader's timing depend on ConfigRegs'
// internals, which is exactly the coupling this phase keeps paying for
// elsewhere.

#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "tpu_v3/sauria/gemm_config.h"

namespace cdc::components::tpu_v3::sauria {

/// One host-port register write.
///
/// The loader produces a *list* rather than driving signals itself, so the same
/// translation can be checked without a running simulation. The adapter then
/// plays the list out through the composition's host port in an `SC_THREAD`.
struct config_write {
    std::uint32_t address = 0;
    std::uint32_t value = 0;

    /// How the value reaches the four-lane host bus.
    enum class encoding {
        /// Value in lane 0, mask lane 0 only. The ordinary register write.
        lane0,
        /// Byte `i` of the value in lane `i`, all four lanes masked.
        ///
        /// Required for the bit-mask registers — `ConfigRegs` decodes
        /// `ROWS_ACTIVE` as eight active-row bits per lane — and it is also the
        /// only lossless way to carry a value at or above 2^24, because the host
        /// bus is floating point. The source's own testbench documents that trap
        /// against itself: `write_reg32_all` casts through `float`, so a mask of
        /// `0xFFFFFFFF` written that way arrives as something else entirely.
        byte_spread,
    };
    encoding how = encoding::lane0;

    /// The source's own name for the register, for a diagnostic that can say
    /// *which* configuration differed rather than which address did.
    const char* name = "";
};

/// Every register write needed to configure the engine for `work`.
///
/// The profile selector comes first, and is written explicitly rather than left
/// at its default: `ConfigRegs` decodes every following address through whichever
/// profile is selected, so a loader that relied on the reset value would silently
/// follow the source if that default ever changed.
///
/// `nsplit` is the one value here that `sauria_compute_core_fields()` does not
/// produce and the source's testbench never writes, so it comes from `rows`. Its
/// reset default is `Y_DIM / 2`, which splits the array in half between the two
/// lanes; leaving it there would route half the rows to lane B, whose operand
/// bank the prefetch controller does not fill. Writing `rows` means "one
/// unsplit array, all of it lane A", which is what a single-tile GEMM is.
std::vector<config_write> build_config_writes(const job& work,
                                              std::uint32_t rows,
                                              std::uint32_t columns);

/// Human-readable dump of a write list, for test diagnostics.
std::string describe_config_writes(const std::vector<config_write>& writes);

} // namespace cdc::components::tpu_v3::sauria
