// SPDX-License-Identifier: Apache-2.0
//
// The config load: a job becomes the register writes that configure the engine.
//
// The values come from the source's own encoder and its own address table, so
// this test does not re-check the arithmetic of the loop bounds — doing that
// would mean writing a second encoder and comparing it against the first, which
// proves whichever one I wrote twice. What it checks is the part this
// repository is responsible for: that the writes are well-formed, ordered
// correctly, complete, and carry the values the encoder produced.

#include <cstdint>
#include <iostream>
#include <set>
#include <string>

#include "tpu_v3/sauria/config_loader.h"
#include "tpu_v3/sauria/sauria_geometry.h"

// The source's own address table. `config_loader.h` deliberately does not
// pull the Sauria headers in — that is what keeps everything above it free of
// them — so a test that wants to check the writes against the table has to
// include it here.
#include "config_map.h"

namespace sauria_tpu = cdc::components::tpu_v3::sauria;

namespace {

int failures = 0;

void check(bool condition, const std::string& what)
{
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << what << '\n';
    }
}

sauria_tpu::job golden_job()
{
    // demo_gemm_64x64: C[64 x 64] = A[64 x 256] . B[256 x 64].
    sauria_tpu::job work;
    work.m = 64;
    work.n = 64;
    work.k = 256;
    work.a_address = 0;
    work.b_address = 0x4000;
    work.c_address = 0x8000;
    work.datatype = sauria_tpu::datatype_value::int8_int32;
    return work;
}

} // namespace

int sc_main(int, char*[])
{
    const auto rows = static_cast<std::uint32_t>(sauria_tpu::rows);
    const auto columns = static_cast<std::uint32_t>(sauria_tpu::columns);
    const auto writes = sauria_tpu::build_config_writes(golden_job(), rows, columns);

    check(!writes.empty(), "no config writes were produced");

    // The profile selector must come first. `ConfigRegs` decodes every other
    // address differently depending on it, so a field written before the
    // profile lands in whichever register the *previous* profile mapped —
    // silently, and with a plausible value.
    check(writes.front().address == ::sauria::CFG_PROFILE_ADDR,
          "the profile selector is not the first write");
    // V1, not V4. The pinned golden case was captured through `ConfigRegs`'
    // default profile, which is `PROFILE_V1_SAURIA` — `tb_evaluate.cpp` never
    // writes the profile register — and `npu_profile.h` calls that the bit-exact
    // path. The loader writes it explicitly rather than relying on the default.
    check(writes.front().value
              == static_cast<std::uint32_t>(::sauria::PROFILE_V1_SAURIA),
          "the profile written is not the v1 SAURIA profile");

    std::set<std::uint32_t> addresses;
    for (const auto& write : writes) {
        addresses.insert(write.address);
    }

    // Every register the kept modules read must be written. Named individually
    // rather than swept from `V1_CFG_MAP`, because that table is not a function
    // from address to field — `CFG_OUT_OFFSET + 0x24` appears in it twice, once
    // as `F_INACTIVE_COLS` and once as `F_REQUANT_SCALE_A` — so "covered by the
    // map" is not a property a test can check against it.
    for (auto address :
         {::sauria::CFG_CON_OFFSET + 0x00u, ::sauria::CFG_CON_OFFSET + 0x04u,
          ::sauria::CFG_CON_OFFSET + 0x08u, ::sauria::CFG_CON_OFFSET + 0x14u,
          ::sauria::CFG_ACT_OFFSET + 0x00u, ::sauria::ACT_XLIM,
          ::sauria::ACT_XSTEP, ::sauria::ACT_YLIM, ::sauria::ACT_YSTEP,
          ::sauria::ACT_CHLIM, ::sauria::ACT_CHSTEP, ::sauria::ACT_TIL_XLIM,
          ::sauria::ACT_TIL_XSTEP, ::sauria::ACT_TIL_YLIM,
          ::sauria::ACT_TIL_YSTEP, ::sauria::WEI_WLIM, ::sauria::WEI_WSTEP,
          ::sauria::WEI_KLIM, ::sauria::WEI_KSTEP, ::sauria::WEI_TIL_XLIM,
          ::sauria::WEI_TIL_XSTEP, ::sauria::WEI_WALIGNED,
          ::sauria::NCONTEXTS, ::sauria::TIL_CYLIM, ::sauria::TIL_CYSTEP,
          ::sauria::TIL_CKLIM, ::sauria::TIL_CKSTEP, ::sauria::PRELOAD_EN}) {
        check(addresses.count(address) != 0,
              "config address 0x" + std::to_string(address)
                  + " is never written; the register keeps its reset value, "
                    "which is a wrong result rather than a failure");
    }

    // The lane-B OBP configuration and its requantisation scale and shift are
    // *not* written. Phase 5 excludes that block, and configuring something
    // absent would leave a register trace implying it is present.
    //
    // Only the lane-B triple can be checked this way, and that is the duplicate
    // addressing again rather than an oversight: `CFG_OUT_OFFSET + 0x20`, `+0x24`
    // and `+0x28` each appear twice in `V1_CFG_MAP`, first as `TIL_CKSTEP`,
    // `INACTIVE_COLS` and `PRELOAD_EN` and again as the lane-A OBP registers.
    // First match wins in `cfg_lookup`, so those three addresses *are* PSM
    // configuration and must be written. The lane-B triple at `+0x30`, `+0x34`
    // and `+0x38` has no alias, so it is unambiguously OBP.
    for (auto address : {::sauria::CFG_OUT_OFFSET + 0x30u,
                         ::sauria::CFG_OUT_OFFSET + 0x34u,
                         ::sauria::CFG_OUT_OFFSET + 0x38u}) {
        check(addresses.count(address) == 0,
              "an excluded block's register is configured; OBP is not part of "
              "the Phase 5 composition");
    }

    // `ROWS_ACTIVE` and `COLS_ACTIVE` use the byte-spread host encoding. This
    // does not mean ROWS_ACTIVE is a writable 64-bit register: ConfigRegs has
    // four host lanes and its `byte_idx < 4` guard leaves rows 32..63 at their
    // power-on value (true) for Y_DIM=64. The adapter relies on zero-filled
    // inactive staging rows and M-bounded writeback for edge jobs; the Phase 5
    // audit records that source limitation explicitly. At 64 active columns the
    // low mask also reaches 0xFFFFFFFF, which one float host lane cannot carry
    // without loss. The source's own testbench documents that separate trap.
    for (const auto& write : writes) {
        if (write.address == ::sauria::CFG_ACT_OFFSET + 0x00u
            || write.address == ::sauria::WEI_COLS_ACTIVE) {
            check(write.how == sauria_tpu::config_write::encoding::byte_spread,
                  std::string(write.name)
                      + " is a bit mask and must be written byte-spread");
        }
    }

    for (const auto& write : writes) {
        if (write.address == ::sauria::CFG_ACT_OFFSET + 0x00u) {
            check(write.value == 0xFFFFFFFFu,
                  "the 64-row source mask must expose exactly its writable "
                  "low 32 bits; rows 32..63 are source reset state, not a "
                  "successfully programmed host value");
        }
    }

    // The activation read count is the *count*, not the encoder's limit. The
    // encoder's `INCNTLIM` field is `B_w * B_h * c_til - 1`; the register holds
    // one more than that, which for this 1x1 job is K. Getting it wrong costs
    // exactly one activation read per context, which is a plausible wrong answer.
    for (const auto& write : writes) {
        if (write.address == ::sauria::CFG_CON_OFFSET + 0x00u) {
            check(write.value == golden_job().k,
                  "CON.INCNTLIM is not the activation read count K");
        }
    }

    // The three base addresses are staging-store indices and must be zero: with
    // D17 staging, the array never sees a core-SRAM address, and a non-zero one
    // here would read outside the staged tile.
    for (auto address : {::sauria::CFG_ACT_BASE_ADDR, ::sauria::CFG_WEI_BASE_ADDR,
                         ::sauria::CFG_OUT_BASE_ADDR}) {
        bool seen = false;
        for (const auto& write : writes) {
            if (write.address == address) {
                seen = true;
                check(write.value == 0,
                      "a base address is not the staging store's origin; with "
                      "tile staging the engine must always read from zero");
            }
        }
        check(seen, "a base address register is never written");
    }

    // No address written twice with different values: the later write would
    // silently win, and which one is "the" configuration would depend on the
    // order this file happens to build the list in.
    for (std::size_t i = 0; i < writes.size(); ++i) {
        for (std::size_t j = i + 1; j < writes.size(); ++j) {
            if (writes[i].address == writes[j].address) {
                check(writes[i].value == writes[j].value,
                      "address 0x" + std::to_string(writes[i].address)
                          + " is written twice with different values");
            }
        }
    }

    // Determinism: the same job must produce byte-identical configuration.
    //
    // Checked because the encoder is the source's and this repository does not
    // own its internals — if it ever acquired state that survived a call, the
    // differential would start failing intermittently, which is the worst way to
    // find out.
    {
        const auto again =
            sauria_tpu::build_config_writes(golden_job(), rows, columns);
        check(again.size() == writes.size(),
              "the same job produced a different number of config writes");
        bool identical = again.size() == writes.size();
        for (std::size_t i = 0; identical && i < again.size(); ++i) {
            identical = again[i].address == writes[i].address
                        && again[i].value == writes[i].value;
        }
        check(identical,
              "the same job produced different configuration on a second call; "
              "the encoder is carrying state between calls");
    }

    // A job that differs must configure differently. Without this, a loader
    // that ignored its argument entirely would pass everything above.
    //
    // The count is asserted, not just "something differs", because a partially
    // wired loader satisfies "something differs" easily.
    //
    // An earlier version of this comment recorded an unresolved anomaly: with
    // the layer description pinned to a constant, three registers still
    // responded to K. That is now explained rather than open. The loader was
    // indexing the encoder's `CfgFieldId` array with a `CfgField`, two unrelated
    // enumerations, so most registers carried some other field's value and the
    // ones past `F_CFG_COUNT` read off the end of the array. The three
    // "K-sensitive" registers were stack garbage. With the loader reading the
    // right fields, the count below is a property of the encoder.
    {
        sauria_tpu::job other = golden_job();
        other.k = 128;
        const auto other_writes =
            sauria_tpu::build_config_writes(other, rows, columns);
        bool differs = false;
        for (std::size_t i = 0;
             i < writes.size() && i < other_writes.size(); ++i) {
            if (writes[i].value != other_writes[i].value) {
                differs = true;
                break;
            }
        }
        std::size_t differing = 0;
        for (std::size_t i = 0;
             i < writes.size() && i < other_writes.size(); ++i) {
            if (writes[i].value != other_writes[i].value) {
                ++differing;
            }
        }
        check(differs, "halving K produced an identical configuration; the "
                       "loader is not reading the job");
        // Measured on the pinned source: K = 256 versus K = 128 moves exactly
        // five registers, and each one is nameable. `INCNTLIM` is
        // `B_w * B_h * c_til - 1` and reaches two addresses (CON and ACT);
        // `WLIM` is `k_til * B_w * B_h * c_til` and also reaches two
        // (WEI.INCNTLIM and WEI.WLIM); `CHLIM` is `A_w_til * A_h_til * c_til`.
        // Everything else in this job is a function of M and N.
        check(differing == 5,
              "halving K changed " + std::to_string(differing)
                  + " registers, expected 5. Either the loader has stopped "
                    "propagating part of the job, or the pinned encoder "
                    "changed what depends on K");
    }

    std::cout << "config writes : " << writes.size() << '\n'
              << sauria_tpu::describe_config_writes(writes);

    if (failures != 0) {
        std::cerr << failures << " checks failed\n";
        return 1;
    }
    std::cout << "config loader PASS\n";
    return 0;
}
