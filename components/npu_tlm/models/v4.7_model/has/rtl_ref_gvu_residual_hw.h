// rtl_ref_gvu_residual_hw.h -- RESIDUAL (opcode 6) of the vector unit as the hardware builds it (gvu_top.sv, gvu_pipe_controller.sv, gvu_config_regs.sv Reg 7..11).
// Pure C++17, no SystemC. Per lane: s = a * Sa (+|-) b * Sb  (64 bit), s += bias (64 bit), shift right by sh, clamp to [-256, 255], + out_z, clamp to [-128, 127].
// Measured on the RTL (ModelSim): the shift of the delivered RTL (FUs/gvu_vector_shift.sv) is LOGICAL for negative values (logical_shift = true);
// the intended and patched behaviour is the arithmetic shift (logical_shift = false, default). One 32-lane vector per cycle, done = 13 + vectors cycles.
// Knobs (env): HAS_GVU_RESIDUAL_HW=1 routes the ADD of HasElemwise through this unit when its parameters are expressible; HAS_GVU_RESIDUAL_HW_SHIFT_LOGICAL=1 selects the delivered shift.
#ifndef RTL_REF_GVU_RESIDUAL_HW_H
#define RTL_REF_GVU_RESIDUAL_HW_H

#include <cstdint>
#include <cstdlib>

namespace rtl_ref_gvu
{
    struct ResidualHwParams
    {
        int32_t sa{1}, sb{1};     // dequant scales of tensor A and B (Reg 8, 9)
        int64_t bias{0};          // total bias (Reg 10 / 11)
        int sh{0};                // output shift 0..63 (Reg 7 [29:24])
        int z{0};                 // output zero point (Reg 7 [23:16])
        bool sub{false};          // Reg 7 [30]
        bool logical_shift{false};
    };

    constexpr int RESIDUAL_HW_FIXED_LATENCY = 13;   // done = 13 + vectors (measured R = 1, 2, 8, 32, 256)

    inline int residual_hw_lane(int a, int b, const ResidualHwParams &p)
    {
        const uint64_t pa = static_cast<uint64_t>(static_cast<int64_t>(a) * p.sa);
        const uint64_t pb = static_cast<uint64_t>(static_cast<int64_t>(b) * p.sb);
        const uint64_t s = (p.sub ? pa - pb : pa + pb) + static_cast<uint64_t>(p.bias);
        int64_t x = p.logical_shift ? static_cast<int64_t>(s >> p.sh) : (static_cast<int64_t>(s) >> p.sh);
        x = x < -256 ? -256 : (x > 255 ? 255 : x);
        int64_t y = x + p.z;
        y = y < -128 ? -128 : (y > 127 ? 127 : y);
        return static_cast<int>(y);
    }

    inline uint64_t residual_hw_cycles(uint64_t elements) { return RESIDUAL_HW_FIXED_LATENCY + (elements + 31) / 32; }

    struct ResidualHw
    {
        static bool on() { static const bool v = [] { const char *e = std::getenv("HAS_GVU_RESIDUAL_HW"); return e && std::atoi(e) != 0; }(); return v; }
        static bool logical() { static const bool v = [] { const char *e = std::getenv("HAS_GVU_RESIDUAL_HW_SHIFT_LOGICAL"); return e && std::atoi(e) != 0; }(); return v; }

        // Parameters of the model's ADD (HAS_IFACE ELEM_WISE ADD: Requant((A - zpA) * SA + (B - zpB) * SB, SO, sO, zpO)) as registers of the hardware.
        // Expressible when there is no per-operand shift, the zero point is subtracted first, the rounding is floor or half-up (the bias carries the half),
        // and the scales cannot saturate the model's int32 intermediate. round_mode: 0 FLOOR, 1 HALF_UP (has/gvu_quant.h).
        static bool map(int zpA, int zpB, int zpO, int64_t SA, int64_t SB, int64_t SO, int sA, int sB, int sO, int round_mode, bool sub_before, ResidualHwParams *out)
        {
            if (!sub_before || (round_mode != 0 && round_mode != 1) || sO < 0 || sO > 63 || sA < 0 || sB < 0) return false;
            if (SA < 0 || SB < 0 || SO < 0 || SO > 0x7FFFFFFF || SA > 0x7FFFFFFF || SB > 0x7FFFFFFF) return false;
            // exact reductions: (x - zp) * S >> s == (x - zp) * (S >> s) when S is a multiple of 2^s; X * SO >> sO == X * (SO >> t) >> (sO - t) for SO = 2^t * q, t <= sO (rounding included)
            if (sA > 0 && sA < 31 && (SA & ((static_cast<int64_t>(1) << sA) - 1)) == 0) { SA >>= sA; sA = 0; }
            if (sB > 0 && sB < 31 && (SB & ((static_cast<int64_t>(1) << sB) - 1)) == 0) { SB >>= sB; sB = 0; }
            if (sA != 0 || sB != 0) return false;
            if (SO > 0) { int t = 0; while (t < sO && (SO & (static_cast<int64_t>(1) << t)) == 0) t++; SO >>= t; sO -= t; }
            if (SA > 4000000 || SB > 4000000) return false;
            if (zpO < -128 || zpO > 127) return false;
            const int64_t sa = SA * SO, sb = SB * SO;
            if (sa > 0x7FFFFFFF || sb > 0x7FFFFFFF) return false;
            if (SO > 0 && (SA > 0x7FFFFFFF / SO || SB > 0x7FFFFFFF / SO)) return false;
            ResidualHwParams p;
            p.sa = static_cast<int32_t>(sa); p.sb = static_cast<int32_t>(sb);
            p.bias = ((round_mode == 1 && sO > 0) ? (static_cast<int64_t>(1) << (sO - 1)) : 0) - (static_cast<int64_t>(zpA) * SA + static_cast<int64_t>(zpB) * SB) * SO;
            p.sh = sO; p.z = zpO; p.sub = false; p.logical_shift = logical();
            *out = p;
            return true;
        }
    };
}

#endif
