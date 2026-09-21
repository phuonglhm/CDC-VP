// libsauria_cfg — C config encoder for the SAURIA core (driver side, NO Python).
// Given a resolved layer/tile descriptor + a HW target, computes the packed core
// config field values (ports SAURIA config_helper.get_sauria_regs) and emits the
// args[22..] payload via the shared bit-layout (sauria_cfg_layout.h). Verified
// bit-exact against Python controller_args (see tools/test_encoder.cpp).
//
// Field VALUES here must match what cfg_decode() extracts from Python's packed args
// (that is the encode<->decode contract): rows_active is LSB-first (cfg_encode
// re-reverses to the MSB-first packing), cols_active is MSB-first as packed.
//
// SRAM element counts are HW-intrinsic: SRAMA_N=Y, SRAMB_N=X, SRAMC_N=Y
// (MEM*_W = dim*elem_w, MEM*_N = MEM*_W/elem_w = dim).

#ifndef LIBSAURIA_CFG_H
#define LIBSAURIA_CFG_H

#include <cstdint>
#include <vector>
#include "sauria_cfg_layout.h"
#include "sauria_targets.h"

namespace sauria
{
    // Resolved layer/tile descriptor. For an untiled workload the tile dims equal
    // the full output dims; leave C_w/C_h/C_c/A_c = 0 and they default to the tile.
    struct SauriaLayerDesc
    {
        int B_w, B_h;      // kernel spatial
        int d, s;          // dilation, stride
        int c_til;         // input channels in the tile (AB_c)
        int k_til;         // output channels in the tile (C_c tile)
        int h_til, w_til;  // output tile height/width
        int X_used, Y_used;
        int preload_en;
        // Full-tensor dims (for the DMA/tiling args[0..21]); 0 => single tile.
        int C_w, C_h, C_c, A_c;
    };

    static inline uint64_t iceil_div(int64_t a, int64_t b) { return (uint64_t)((a + b - 1) / b); }

    // Compute the packed core-config field values. `t` supplies X/Y and IDX widths;
    // DILP_W/PARAMS_W/TH_W are the fixed SAURIA constants.
    inline void sauria_compute_core_fields(const SauriaLayerDesc &c, const SauriaTarget &t,
                                           uint64_t f[F_CFG_COUNT])
    {
        for (int i = 0; i < F_CFG_COUNT; i++)
            f[i] = 0;

        const int X = t.X, Y = t.Y;
        const int SRAMA_N = Y, SRAMB_N = X, SRAMC_N = Y;
        const int DILP_W = 64;

        const int B_w = c.B_w, B_h = c.B_h, d = c.d, s = c.s;
        const int c_til = c.c_til, k_til = c.k_til, h_til = c.h_til, w_til = c.w_til;
        const int X_used = c.X_used, Y_used = c.Y_used;

        const int B_w_eff = 1 + (B_w - 1) * d;
        const int B_h_eff = 1 + (B_h - 1) * d;
        const int C_w_eff_til = 1 + (w_til - 1) * s;
        const int C_h_eff_til = 1 + (h_til - 1) * s;
        const int A_w_til = C_w_eff_til + B_w_eff - 1;
        const int A_h_til = C_h_eff_til + B_h_eff - 1;

        const uint64_t N_cswitch =
            iceil_div(w_til, Y_used) * (uint64_t)h_til * iceil_div(k_til, X_used);

        const bool waligned = ((k_til % SRAMB_N) == 0) && (X_used == SRAMB_N);

        // Dilation pattern: MSB-first over DILP_W (index i at bit DILP_W-1-i).
        uint64_t dil_pat = 0;
        for (int i = 0; i < DILP_W; i++)
            if ((i % d == 0) && (i / d < B_w))
                dil_pat |= (1ULL << (DILP_W - 1 - i));

        // CONTROL
        f[F_CFG_INCNTLIM] = (uint64_t)(B_w * B_h * c_til - 1);
        f[F_CFG_ACT_REPS] = iceil_div(k_til, X_used);
        f[F_CFG_WEI_REPS] = iceil_div(w_til, Y_used) * (uint64_t)h_til;
        f[F_CFG_THRES]    = 0;

        // ACTIVATION
        uint64_t xlim;
        if (s == 1 && d == 1 && B_w == 1 && B_h == 1 &&
            (c_til % SRAMA_N == 0) && (Y_used % SRAMA_N == 0))
            xlim = (uint64_t)Y_used;
        else
            xlim = (uint64_t)((1 + (Y_used - 1) * s) + B_w_eff + 1 - (B_w_eff % 2) + SRAMA_N);
        f[F_CFG_XLIM]     = xlim;
        f[F_CFG_XSTEP]    = (uint64_t)SRAMA_N;
        f[F_CFG_YLIM]     = (uint64_t)(A_w_til * B_h_eff);
        f[F_CFG_YSTEP]    = (uint64_t)(A_w_til * d);
        f[F_CFG_CHLIM]    = (uint64_t)(A_w_til * A_h_til * c_til);
        f[F_CFG_CHSTEP]   = (uint64_t)(A_w_til * A_h_til);
        f[F_CFG_TIL_XLIM] = iceil_div(w_til, Y_used) * (uint64_t)(Y_used * s);
        f[F_CFG_TIL_XSTEP] = (uint64_t)(Y_used * s);
        f[F_CFG_TIL_YLIM] = (uint64_t)h_til * (uint64_t)(A_w_til * s);
        f[F_CFG_TIL_YSTEP] = (uint64_t)(A_w_til * s);
        f[F_CFG_DIL_PAT]  = dil_pat;
        // LSB-first mask; cfg_encode re-reverses to the MSB-first packing.
        f[F_CFG_ROWS_ACTIVE] = (Y_used >= 64) ? ~0ULL : ((1ULL << Y_used) - 1ULL);
        // F_CFG_PER_ROW_OFF stays 0 (skipped by the layout).

        // WEIGHT
        f[F_CFG_WLIM]     = (uint64_t)(k_til * B_w * B_h * c_til);
        f[F_CFG_WSTEP]    = (uint64_t)k_til;
        f[F_CFG_KLIM]     = waligned ? 1ULL : (uint64_t)(SRAMB_N + 1);
        f[F_CFG_KSTEP]    = (uint64_t)SRAMB_N;
        f[F_CFG_TIL_KLIM] = (uint64_t)k_til;
        f[F_CFG_TIL_KSTEP] = (uint64_t)X_used;
        // MSB-first mask, packed as-is (cfg_decode does NOT reverse cols_active).
        {
            uint64_t xm = (X_used >= 64) ? ~0ULL : ((1ULL << X_used) - 1ULL);
            f[F_CFG_COLS_ACTIVE] = (X >= 64) ? xm : (xm << (X - X_used));
        }
        f[F_CFG_WALIGNED] = waligned ? 1ULL : 0ULL;

        // OUTPUT
        f[F_CFG_NCONTEXTS]  = N_cswitch;
        f[F_CFG_CXLIM]      = (uint64_t)(Y_used + SRAMC_N);
        f[F_CFG_CXSTEP]     = (uint64_t)SRAMC_N;
        f[F_CFG_CKLIM]      = (uint64_t)(w_til * h_til * X_used);
        f[F_CFG_CKSTEP]     = (uint64_t)(w_til * h_til);
        f[F_CFG_TIL_CYLIM]  = (uint64_t)(w_til * h_til);
        f[F_CFG_TIL_CYSTEP] = (uint64_t)Y_used;
        f[F_CFG_TIL_CKLIM]  = (uint64_t)(w_til * h_til * k_til);
        f[F_CFG_TIL_CKSTEP] = (uint64_t)(w_til * h_til * X_used);
        f[F_CFG_INACTIVE_COLS] = (uint64_t)(X - X_used);
        f[F_CFG_PRELOAD_EN] = (uint64_t)c.preload_en;
    }

    // Full convenience: descriptor -> packed args[22..] payload (one flat bitstream,
    // regions word-aligned exactly like SAURIA). Returns the payload words; the
    // caller places them at controller_args[start_word..].
    inline std::vector<uint32_t> sauria_encode_core_config(const SauriaLayerDesc &c,
                                                           const SauriaTarget &t)
    {
        uint64_t f[F_CFG_COUNT];
        sauria_compute_core_fields(c, t, f);
        CfgWidths w;
        w.idx_a = t.idx_a; w.idx_w = t.idx_w; w.idx_o = t.idx_o;
        w.X = t.X; w.Y = t.Y;
        // Per-row weight offsets: lwoffs[j] = j*s for active rows (j<Y_used), else 0
        // (SAURIA sauria_lib: rows_active_arr * arange(Y) * s). The core discards
        // them, but they must be packed to match Python's args bit-for-bit.
        std::vector<uint64_t> lwoffs((size_t)t.Y, 0);
        for (int j = 0; j < t.Y; j++)
            if (j < c.Y_used)
                lwoffs[(size_t)j] = (uint64_t)(j * c.s);
        std::vector<uint32_t> args;
        cfg_encode(f, 0, w, args, lwoffs.data()); // start_word 0: payload only
        return args;
    }

    // Port of execution_model.get_tiling_loops -> loop_order (the `decision`).
    inline int sauria_loop_order(const SauriaLayerDesc &c)
    {
        int C_w = c.C_w ? c.C_w : c.w_til, C_h = c.C_h ? c.C_h : c.h_til;
        int C_c = c.C_c ? c.C_c : c.k_til, A_c = c.A_c ? c.A_c : c.c_til;
        int B_w_eff = 1 + (c.B_w - 1) * c.d, B_h_eff = 1 + (c.B_h - 1) * c.d;
        int A_w_til = (1 + (c.w_til - 1) * c.s) + B_w_eff - 1;
        int A_h_til = (1 + (c.h_til - 1) * c.s) + B_h_eff - 1;

        int c_it = A_c / c.c_til, k_it = C_c / c.k_til;
        int w_it = C_w / c.w_til, h_it = C_h / c.h_til;
        double A_w = (double)c.c_til * A_h_til * A_w_til;
        double B_w = (double)c.k_til * c.c_til * c.B_h * c.B_w * 1.5;
        double C_wt = (double)c.k_til * c.w_til * c.h_til * (c.preload_en ? 2 : 1);
        if (k_it == 1) A_w = 0;
        if (w_it == 1 && h_it == 1) B_w = 0;
        if (c_it == 1) C_wt = 0;
        // argmax([B, C, A]); numpy argmax returns first max on ties.
        double v[3] = {B_w, C_wt, A_w};
        int best = 0;
        for (int i = 1; i < 3; i++) if (v[i] > v[best]) best = i;
        return best;
    }

    inline uint32_t set_bits(uint32_t x, int msb, int lsb, uint32_t val)
    {
        uint32_t nbits = (uint32_t)(msb - lsb + 1);
        uint32_t mask = (nbits >= 32) ? 0xFFFFFFFFu : (((1u << nbits) - 1u) << lsb);
        return (x & ~mask) | ((val << lsb) & mask);
    }

    // Full controller args[0..21] + core payload at [22..]. dram_bases = {act,wei,out}.
    // Ports config_helper.get_controller_regs (WXfer_op=True path). args[18..20] are
    // the caller-supplied DRAM bases (not derived).
    inline std::vector<uint32_t> sauria_encode_controller_args(const SauriaLayerDesc &c,
                                                              const SauriaTarget &t,
                                                              const uint32_t dram_bases[3])
    {
        const int Bw = c.B_w, Bh = c.B_h, s = c.s, d = c.d;
        const int Cw_til = c.w_til, Ch_til = c.h_til, Ck_til = c.k_til, Ac_til = c.c_til;
        const int Cw = c.C_w ? c.C_w : Cw_til, Ch = c.C_h ? c.C_h : Ch_til;
        const int Ck = c.C_c ? c.C_c : Ck_til, Ac = c.A_c ? c.A_c : Ac_til;
        const int B_w_eff = 1 + (Bw - 1) * d, B_h_eff = 1 + (Bh - 1) * d;
        const int Aw = (1 + (Cw - 1) * s) + B_w_eff - 1;
        const int Ah = (1 + (Ch - 1) * s) + B_h_eff - 1;
        const int Aw_til = (1 + (Cw_til - 1) * s) + B_w_eff - 1;
        const int Ah_til = (1 + (Ch_til - 1) * s) + B_h_eff - 1;

        std::vector<uint32_t> core = sauria_encode_core_config(c, t);
        std::vector<uint32_t> a(22 + core.size(), 0u);

        a[0] = set_bits(a[0], 15, 0, (uint32_t)(Cw / Cw_til - 1));
        a[0] = set_bits(a[0], 31, 16, (uint32_t)(Ch / Ch_til - 1));
        a[1] = set_bits(a[1], 15, 0, (uint32_t)(Ac / Ac_til - 1));
        a[1] = set_bits(a[1], 31, 16, (uint32_t)(Ck / Ck_til - 1));
        a[2] = (uint32_t)(Cw_til);
        a[3] = (uint32_t)(Ch_til * Cw);
        a[4] = (uint32_t)(Ck_til * Cw * Ch);
        a[5] = (uint32_t)(s * Cw_til);
        a[6] = (uint32_t)(s * Ch_til * Aw);
        a[7] = (uint32_t)(Ac_til * Aw * Ah);
        a[8] = (uint32_t)(Ck_til * Bw * Bh * Ac);       // WXfer
        a[9] = (uint32_t)(Ck_til * Bw * Bh * Ac_til);   // WXfer
        // dma ifmaps limits/ett depend on tile-vs-full spatial coverage
        uint32_t dma_ify_lim, dma_ifc_lim, dma_ifett;
        if (Aw == Aw_til && Ah == Ah_til) { dma_ify_lim = 0; dma_ifc_lim = 0; dma_ifett = (uint32_t)(Ac_til * Aw * Ah); }
        else if (Aw == Aw_til)            { dma_ify_lim = 0; dma_ifc_lim = (uint32_t)(Ac_til - 1); dma_ifett = (uint32_t)(Ah_til * Aw); }
        else                              { dma_ify_lim = (uint32_t)(Ah_til - 1); dma_ifc_lim = (uint32_t)(Ac_til - 1); dma_ifett = (uint32_t)Aw_til; }
        a[10] = dma_ify_lim;
        a[11] = dma_ifc_lim;
        a[12] = (uint32_t)Cw;                 // dma_psums_y_step
        a[13] = (uint32_t)(Ch * Cw);          // dma_psums_k_step
        a[14] = (uint32_t)Aw;                 // dma_ifmaps_y_step
        a[15] = (uint32_t)(Ah * Aw);          // dma_ifmaps_c_step
        a[16] = (uint32_t)Ck_til;             // dma_weights_w_step (WXfer)
        a[17] = dma_ifett;
        a[18] = dram_bases[0];
        a[19] = dram_bases[1];
        a[20] = dram_bases[2];

        int loop_order = sauria_loop_order(c);
        a[21] = set_bits(a[21], 17, 16, (uint32_t)loop_order);
        a[21] = set_bits(a[21], 23, 23, (uint32_t)(Cw == Cw_til));
        a[21] = set_bits(a[21], 24, 24, (uint32_t)(Ch == Ch_til));
        a[21] = set_bits(a[21], 25, 25, 1u);   // Ck_eq (WXfer_op path)
        a[21] = set_bits(a[21], 31, 31, 1u);   // WXfer_op

        for (size_t i = 0; i < core.size(); i++)
            a[22 + i] = core[i];
        return a;
    }

} // namespace sauria

#endif // LIBSAURIA_CFG_H
