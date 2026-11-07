// has_profile_neo.h -- run profiles "neo" and "neo_as_rtl": the knobs of the model that follow neo_lite RTL revision 1, set in one place.
//
//   neo         REV 1 of the RTL (HAS_NEO_REV=1) with every neo_lite knob on and every model proposal the RTL does not have off
//               (4-D DMA, COPY, attention requant on GvuTop, lane LayerNorm, average pool, corner turn, AGU_ND).
//   neo_as_rtl  neo + the knobs that imitate behaviour of the RTL that revision 1 still has (rounding half away from zero, wrapping
//               16-bit narrowing and zero-point adder, Softmax stage 3 as delivered, arithmetic through the ported functional units).
//
// How a profile is applied. The knobs are environment variables, and most of them are read once, at the first use, into a static of
// the part concerned. A profile therefore sets the variables BEFORE any of those parts is used (HasNpuTop construction, or the
// start of the testbench). Order of priority, highest first:
//   1. a HAS_* variable the caller has set (kept, the log line says "env")
//   2. the table of the profile (kNeoKnobs, kNeoAsRtlExtra)
//   3. the default in the code
// HAS_PROFILE=neo|neo_as_rtl selects the profile from the environment (see resolve_profile() in has_npu_top.h). Nothing here is
// reached when no profile of this file is selected.
// verify_env_profile() reads the cached values back and reports a knob that was read before the profile set it.
#ifndef HAS_PROFILE_NEO_H
#define HAS_PROFILE_NEO_H

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "has/neo_rev.h"
#include "has/dma_dfc_1d.h"
#include "has/gvu_regs_neo_shadow.h"
#include "has/rtl_ref_gvu_residual_hw.h"
#include "has/rtl_ref_gvu_scratchpad.h"
#include "has/gvu_top_bridge.h"
#include "has/gvu_top_stream.h"
#include "has/gvu_rtl_arith.h"

namespace has
{
    namespace profile_neo
    {
        struct EnvKnob
        {
            const char *name;
            const char *value;
            const char *why;   // reason, with the RTL line (neo_lite RTL revision 1, rtl/src/...) or the measurement
        };

        // ---- profile neo ----
        static const EnvKnob kNeoKnobs[] = {
            {"HAS_NEO_REV", "1", "follow revision 1: Reg 12 OH/C, Reg 0 bit 3 forced swap, dense max pool s2/s4, pool feeder without reset (gvu_config_regs.sv:305,227; gvu_vlsu.sv:252)"},
            {"HAS_GVU_REGS_NEO", "1", "register map of 14 byte-addressed registers, 14 writes per command (gvu_pkg.sv; df_controller_top.sv:44)"},
            {"HAS_GVU_NEO_PP", "1", "four buffer-select flops toggle on the pipe_en edge (gvu_config_regs.sv:221)"},
            {"HAS_GVU_NEO_CDC_LAT", "4", "mux + clock-domain crossing of neo_top, MEASURED 3.8 to 4.1 core cycles (Verilator)"},
            {"HAS_GVU_NEO_WRITE_LAT", "1", "one register write per cycle (neo_interface.sv), MEASURED"},
            {"HAS_GVU_VBANK1_DEPTH", "200", "VBANK1_DEPTH 200 (gvu_pkg.sv)"},
            {"HAS_GVU_RESIDUAL_HW", "1", "ADD through RESIDUAL (opcode 6), arithmetic shift as in revision 1 (gvu_vector_shift.sv:11,21)"},
            {"HAS_DMA_DFC_1D", "1", "one-dimensional DFC commands, cost 19/15/7 MEASURED (neo_dma_controller.sv)"},
            {"HAS_GVU_OUT_VBANK2", "1", "GEMM_FUSED result vbank2 -> DRAM by DMA_SEND_VBANK2 (neo_dma_controller.sv:841)"},
            {"HAS_BUS_STRICT", "1", "scale/shift into sbank and Softmax output A cross the DMA (neo_dma_controller.sv:742)"},
            // proposals of the model that the RTL does not have: off
            {"HAS_GVU_AGU_ND", "0", "AGU_ND registers 16..24 are a proposal; the RTL has 14 registers (gvu_pkg.sv)"},
            {"HAS_DMA_ND", "0", "4-D DMA descriptors are a proposal; the DMA is 1-D, the DFC loops over at most 2-D"},
            {"HAS_DMA_TENSOR", "0", "no DMA-only opcode (neo_opcode_t in df_controller_pkg.sv)"},
            {"HAS_DMA_ND_MERGE", "0", "proposal"},
            {"HAS_DMA_RESHAPE", "0", "upsample / reshape on the DMA are a proposal; the DMA is 1-D and has no DMA-only opcode"},
            {"HAS_GVU_FA", "0", "attention requant on GvuTop is a proposal (it needs a change of Reg 3); the RTL limit is 614 scale words"},
            {"HAS_GVU_FA_OVERLAP", "0", "proposal P-FA5"},
            {"HAS_GVU_LN", "0", "the RTL has no LayerNorm path: opcode LAYERNORM leaves the FSM in IDLE (gvu_pipe_controller.sv); LN runs on the functional model"},
            {"HAS_GVU_AVG", "0", "AVG_POOL is not wired (A7a)"},
            {"HAS_OUT_TRANSPOSE", "0", "corner turn after the epilogue is a proposal"},
            {"HAS_FEEDER_PAD", "0", "padding by the IFMAP feeder is not in the RTL"},
        };

        // ---- profile neo_as_rtl: on top of neo ----
        static const EnvKnob kNeoAsRtlExtra[] = {
            {"HAS_GVU_SOFTMAX_AS_RTL", "1", "Softmax stage 3 shifts by E, as revision 1 does"},
            {"HAS_ROUND_MODE", "2", "both round-shift units are fixed to RND_NEAR_AWAY (gvu_pipe_controller.sv:364,368); model enum 2 = HALF_AWAY"},
            {"HAS_REQ_NARROW", "1", "narrowing is NARROW_WRAP, not saturating (gvu_pipe_controller.sv, Round1)"},
            {"HAS_ZP_ADD_WRAP16", "1", "the 16-bit zero-point adder wraps (functional units unchanged between revision 0 and revision 1)"},
            {"HAS_ARITH_RTL", "1", "arithmetic that stays on the functional model goes through the ported units (rtl_ref_gvu_fu.h)"},
        };

        inline bool set_env(const char *name, const char *value)
        {
#ifdef _WIN32
            return _putenv_s(name, value) == 0;
#else
            return setenv(name, value, 1) == 0;
#endif
        }

        template <size_t N>
        inline void apply_table(const EnvKnob (&t)[N], const char *profile, std::FILE *log, int &set, int &kept)
        {
            for (const EnvKnob &k : t)
            {
                const char *cur = std::getenv(k.name);
                if (cur && *cur)
                {
                    kept++;
                    if (log) std::fprintf(log, "[profile %s] %s=%s (env; profile value %s)\n", profile, k.name, cur, k.value);
                }
                else
                {
                    set_env(k.name, k.value);
                    set++;
                    if (log) std::fprintf(log, "[profile %s] %s=%s (profile) -- %s\n", profile, k.name, k.value, k.why);
                }
            }
        }

        // Profile name -> 0 none, 1 neo, 2 neo_as_rtl, -1 unknown name.
        inline int parse(const char *p)
        {
            if (!p || !*p) return 0;
            if (!std::strcmp(p, "neo")) return 1;
            if (!std::strcmp(p, "neo_as_rtl")) return 2;
            return -1;
        }
        // HAS_PROFILE of the environment, same codes.
        inline int from_env() { return parse(std::getenv("HAS_PROFILE")); }
    }

    // Sets the variables of the profile that are not set yet. Call before the first use of any knob. 0 on success, -1 for an unknown name.
    inline int apply_env_profile(const char *p, std::FILE *log = nullptr)
    {
        const int id = profile_neo::parse(p);
        if (id <= 0) return id < 0 ? -1 : 0;
        int set = 0, kept = 0;
        profile_neo::apply_table(profile_neo::kNeoKnobs, p, log, set, kept);
        if (id == 2) profile_neo::apply_table(profile_neo::kNeoAsRtlExtra, p, log, set, kept);
        if (log) std::fprintf(log, "[profile %s] %d variables set, %d kept from the environment\n", p, set, kept);
        return 0;
    }

    // Reads back the values the parts have cached and compares them with the variables now in the environment. A difference means a
    // part read its knob before the profile set it (the number of such knobs is returned and each one is printed).
    inline int verify_env_profile(std::FILE *log = nullptr)
    {
        struct Chk { const char *name; int (*get)(); };
        static const Chk chk[] = {
            {"HAS_NEO_REV", [] { return int(neo_rev()); }},
            {"HAS_GVU_REGS_NEO", [] { return int(GvuRegsNeoKnobs::on()); }},
            {"HAS_GVU_NEO_PP", [] { return int(GvuRegsNeoKnobs::pp_on()); }},
            {"HAS_GVU_NEO_CDC_LAT", [] { return int(GvuRegsNeoKnobs::cdc_lat()); }},
            {"HAS_GVU_NEO_WRITE_LAT", [] { return int(GvuRegsNeoKnobs::write_lat()); }},
            {"HAS_GVU_VBANK1_DEPTH", [] { return rtl_ref_gvu::vbank1_depth_default(); }},
            {"HAS_GVU_RESIDUAL_HW", [] { return int(rtl_ref_gvu::ResidualHw::on()); }},
            {"HAS_GVU_RESIDUAL_HW_SHIFT_LOGICAL", [] { return int(rtl_ref_gvu::ResidualHw::logical()); }},
            {"HAS_DMA_DFC_1D", [] { return int(DmaDfc1d::on()); }},
            {"HAS_GVU_OUT_VBANK2", [] { return int(DmaDfc1d::out_vbank2()); }},
            {"HAS_GVU_AGU_ND", [] { return GvuTopBridge::agu_nd_level(); }},
            {"HAS_GVU_AVG", [] { return int(GvuTopStream::avg_on()); }},
            {"HAS_GVU_LN", [] { return int(GvuTopStream::ln_on()); }},
            {"HAS_GVU_SOFTMAX_AS_RTL", [] { return int(GvuTopStream::softmax_as_rtl()); }},
            {"HAS_ARITH_RTL", [] { return int(RtlArith::on()); }},
        };
        int bad = 0;
        for (const Chk &c : chk)
        {
            const char *e = std::getenv(c.name);
            if (!e) continue;
            const int want = std::atoi(e), have = c.get();
            // vbank1 depth and the latency knobs are clamped by their readers (0 -> 32; < 1 -> 1): compare after the same clamp
            const bool ok = have == want || (!std::strcmp(c.name, "HAS_GVU_VBANK1_DEPTH") && want <= 0 && have == 32) ||
                            (!std::strcmp(c.name, "HAS_GVU_NEO_WRITE_LAT") && want < 1 && have == 1) ||
                            (!std::strcmp(c.name, "HAS_GVU_NEO_CDC_LAT") && want < 0 && have == 0) ||
                            (!std::strcmp(c.name, "HAS_NEO_REV") && want < 0 && have == 0) ||
                            (!std::strcmp(c.name, "HAS_NEO_REV") && want > int(NEO_REV_MAX) && have == int(NEO_REV_MAX));
            if (!ok)
            {
                bad++;
                if (log) std::fprintf(log, "[profile] WARNING: %s=%d in the environment but the part reads %d (read before the profile was applied)\n", c.name, want, have);
            }
        }
        if (log) std::fprintf(log, "[profile] verify: %d knobs differ from the environment\n", bad);
        return bad;
    }
}

#endif // HAS_PROFILE_NEO_H
