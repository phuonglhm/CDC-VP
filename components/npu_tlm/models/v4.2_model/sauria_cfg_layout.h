// SAURIA packed core-config bit-layout — SINGLE SOURCE shared by the DECODER (tb)
// and the C ENCODER (driver, libsauria_cfg). The controller args[START_WORD..] carry
// the packed core config; this file defines the exact field order, per-field bit
// width, 32-bit word-alignment points, and the two special fields, ONCE. Both
// directions iterate the same table so they can never drift.
//
// Mirrors SAURIA config_helper.get_sauria_regs packing and the original sequential
// reader in tb_evaluate.cpp (decode_sauria_packed_config). Verified against the
// 10-demo suite.
//
// Section order: CONTROL -> (align) -> ACTIVATION -> (align) -> WEIGHT -> (align)
//                -> OUTPUT.  Widths ACT/WEI/OUT come from the target's IDX widths
//                (see sauria_targets.h); X/Y are the array geometry.

#ifndef SAURIA_CFG_LAYOUT_H
#define SAURIA_CFG_LAYOUT_H

#include <cstdint>
#include <cstddef>
#include <vector>

namespace sauria
{
    // Fields in packed order. Values double as indices into the flat field array
    // exchanged with the (de)coder; F_CFG_PER_ROW_OFF is consumed/emitted but not a
    // real config value (per-row weight offsets, currently zero).
    enum CfgFieldId : int
    {
        // CONTROL
        F_CFG_INCNTLIM = 0, F_CFG_ACT_REPS, F_CFG_WEI_REPS, F_CFG_THRES,
        // ACTIVATION
        F_CFG_XLIM, F_CFG_XSTEP, F_CFG_YLIM, F_CFG_YSTEP, F_CFG_CHLIM, F_CFG_CHSTEP,
        F_CFG_TIL_XLIM, F_CFG_TIL_XSTEP, F_CFG_TIL_YLIM, F_CFG_TIL_YSTEP,
        F_CFG_DIL_PAT, F_CFG_ROWS_ACTIVE, F_CFG_PER_ROW_OFF,
        // WEIGHT
        F_CFG_WLIM, F_CFG_WSTEP, F_CFG_KLIM, F_CFG_KSTEP, F_CFG_TIL_KLIM, F_CFG_TIL_KSTEP,
        F_CFG_COLS_ACTIVE, F_CFG_WALIGNED,
        // OUTPUT
        F_CFG_NCONTEXTS, F_CFG_CXLIM, F_CFG_CXSTEP, F_CFG_CKLIM, F_CFG_CKSTEP,
        F_CFG_TIL_CYLIM, F_CFG_TIL_CYSTEP, F_CFG_TIL_CKLIM, F_CFG_TIL_CKSTEP,
        F_CFG_INACTIVE_COLS, F_CFG_PRELOAD_EN,
        F_CFG_COUNT
    };

    // Width sources. Resolved per target at decode/encode time.
    enum CfgWidthKind : int
    {
        WK_ACT_IDX, WK_WEI_IDX, WK_OUT_IDX, WK_TH, WK_DILP, WK_PARAMS, WK_Y, WK_X, WK_ONE
    };

    enum CfgSpecial : int
    {
        SP_NONE = 0,
        SP_ROWS_ACTIVE_MSB, // Y bits, packed MSB-first (row j at bit Y-1-j)
        SP_SKIP_PER_ROW     // Y repeats of PARAMS_W bits, value always 0
    };

    struct CfgFieldSpec
    {
        CfgFieldId  field;
        CfgWidthKind wkind;
        bool        align_after; // align bit-cursor to next 32-bit word after this field
        CfgSpecial  special;
    };

    // THE layout. Order + widths + alignment are authoritative here.
    static const CfgFieldSpec SAURIA_CFG_LAYOUT[] = {
        // CONTROL
        {F_CFG_INCNTLIM,  WK_ACT_IDX, false, SP_NONE},
        {F_CFG_ACT_REPS,  WK_OUT_IDX, false, SP_NONE},
        {F_CFG_WEI_REPS,  WK_OUT_IDX, false, SP_NONE},
        {F_CFG_THRES,     WK_TH,      true,  SP_NONE},
        // ACTIVATION
        {F_CFG_XLIM,      WK_ACT_IDX, false, SP_NONE},
        {F_CFG_XSTEP,     WK_ACT_IDX, false, SP_NONE},
        {F_CFG_YLIM,      WK_ACT_IDX, false, SP_NONE},
        {F_CFG_YSTEP,     WK_ACT_IDX, false, SP_NONE},
        {F_CFG_CHLIM,     WK_ACT_IDX, false, SP_NONE},
        {F_CFG_CHSTEP,    WK_ACT_IDX, false, SP_NONE},
        {F_CFG_TIL_XLIM,  WK_ACT_IDX, false, SP_NONE},
        {F_CFG_TIL_XSTEP, WK_ACT_IDX, false, SP_NONE},
        {F_CFG_TIL_YLIM,  WK_ACT_IDX, false, SP_NONE},
        {F_CFG_TIL_YSTEP, WK_ACT_IDX, false, SP_NONE},
        {F_CFG_DIL_PAT,   WK_DILP,    false, SP_NONE},
        {F_CFG_ROWS_ACTIVE, WK_Y,     false, SP_ROWS_ACTIVE_MSB},
        {F_CFG_PER_ROW_OFF, WK_PARAMS, true, SP_SKIP_PER_ROW},
        // WEIGHT
        {F_CFG_WLIM,      WK_WEI_IDX, false, SP_NONE},
        {F_CFG_WSTEP,     WK_WEI_IDX, false, SP_NONE},
        {F_CFG_KLIM,      WK_WEI_IDX, false, SP_NONE},
        {F_CFG_KSTEP,     WK_WEI_IDX, false, SP_NONE},
        {F_CFG_TIL_KLIM,  WK_WEI_IDX, false, SP_NONE},
        {F_CFG_TIL_KSTEP, WK_WEI_IDX, false, SP_NONE},
        {F_CFG_COLS_ACTIVE, WK_X,     false, SP_NONE},
        {F_CFG_WALIGNED,  WK_ONE,     true,  SP_NONE},
        // OUTPUT
        {F_CFG_NCONTEXTS, WK_OUT_IDX, false, SP_NONE},
        {F_CFG_CXLIM,     WK_OUT_IDX, false, SP_NONE},
        {F_CFG_CXSTEP,    WK_OUT_IDX, false, SP_NONE},
        {F_CFG_CKLIM,     WK_OUT_IDX, false, SP_NONE},
        {F_CFG_CKSTEP,    WK_OUT_IDX, false, SP_NONE},
        {F_CFG_TIL_CYLIM, WK_OUT_IDX, false, SP_NONE},
        {F_CFG_TIL_CYSTEP, WK_OUT_IDX, false, SP_NONE},
        {F_CFG_TIL_CKLIM, WK_OUT_IDX, false, SP_NONE},
        {F_CFG_TIL_CKSTEP, WK_OUT_IDX, false, SP_NONE},
        {F_CFG_INACTIVE_COLS, WK_PARAMS, false, SP_NONE},
        {F_CFG_PRELOAD_EN, WK_ONE,    false, SP_NONE},
    };
    static const int SAURIA_CFG_LAYOUT_N =
        (int)(sizeof(SAURIA_CFG_LAYOUT) / sizeof(SAURIA_CFG_LAYOUT[0]));

    // Per-target width parameters (from sauria_targets.h fields).
    struct CfgWidths
    {
        uint32_t idx_a, idx_w, idx_o; // ACT/WEI/OUT packed-config index widths
        uint32_t X, Y;                // array geometry
        uint32_t th_w = 2;            // negligence threshold width
        uint32_t dilp_w = 64;         // dilation pattern width
        uint32_t params_w = 8;        // general params width
    };

    inline uint32_t cfg_resolve_width(CfgWidthKind k, const CfgWidths &w)
    {
        switch (k)
        {
        case WK_ACT_IDX: return w.idx_a;
        case WK_WEI_IDX: return w.idx_w;
        case WK_OUT_IDX: return w.idx_o;
        case WK_TH:      return w.th_w;
        case WK_DILP:    return w.dilp_w;
        case WK_PARAMS:  return w.params_w;
        case WK_Y:       return w.Y;
        case WK_X:       return w.X;
        case WK_ONE:     return 1;
        }
        return 0;
    }

    // ---- bit-cursor primitives (identical semantics to the tb reader) ----
    inline uint64_t cfg_read_bits(const std::vector<uint32_t> &args, uint32_t start_word,
                                  size_t &bitpos, uint32_t width)
    {
        uint64_t value = 0;
        for (uint32_t b = 0; b < width; b++)
        {
            size_t abs_bit = bitpos + b;
            uint32_t word_idx = start_word + (uint32_t)(abs_bit / 32);
            uint32_t bit_idx = (uint32_t)(abs_bit % 32);
            uint64_t bit_val = (word_idx < args.size())
                                   ? ((args[word_idx] >> bit_idx) & 0x1ULL)
                                   : 0ULL;
            value |= (bit_val << b);
        }
        bitpos += width;
        return value;
    }

    inline void cfg_write_bits(std::vector<uint32_t> &args, uint32_t start_word,
                               size_t &bitpos, uint32_t width, uint64_t value)
    {
        for (uint32_t b = 0; b < width; b++)
        {
            size_t abs_bit = bitpos + b;
            uint32_t word_idx = start_word + (uint32_t)(abs_bit / 32);
            uint32_t bit_idx = (uint32_t)(abs_bit % 32);
            if (word_idx >= args.size())
                args.resize(word_idx + 1, 0u);
            if ((value >> b) & 0x1ULL)
                args[word_idx] |= (1u << bit_idx);
            else
                args[word_idx] &= ~(1u << bit_idx);
        }
        bitpos += width;
    }

    inline void cfg_align(size_t &bitpos)
    {
        if (bitpos % 32 != 0)
            bitpos = ((bitpos / 32) + 1) * 32;
    }

    inline uint64_t cfg_bitrev(uint64_t v, uint32_t n)
    {
        uint64_t r = 0;
        for (uint32_t j = 0; j < n; j++)
            if ((v >> (n - 1 - j)) & 0x1ULL)
                r |= (0x1ULL << j);
        return r;
    }

    // DECODE: args[start_word..] -> flat field array out[F_CFG_COUNT].
    // rows_active is returned LSB-first (bit-reversed from the MSB-first packing).
    inline void cfg_decode(const std::vector<uint32_t> &args, uint32_t start_word,
                           const CfgWidths &w, uint64_t out[F_CFG_COUNT])
    {
        for (int i = 0; i < F_CFG_COUNT; i++)
            out[i] = 0;
        size_t bitpos = 0;
        for (int i = 0; i < SAURIA_CFG_LAYOUT_N; i++)
        {
            const CfgFieldSpec &f = SAURIA_CFG_LAYOUT[i];
            uint32_t width = cfg_resolve_width(f.wkind, w);
            if (f.special == SP_SKIP_PER_ROW)
            {
                for (uint32_t y = 0; y < w.Y; y++)
                    (void)cfg_read_bits(args, start_word, bitpos, width);
            }
            else if (f.special == SP_ROWS_ACTIVE_MSB)
            {
                uint64_t raw = cfg_read_bits(args, start_word, bitpos, width);
                out[f.field] = cfg_bitrev(raw, width);
            }
            else
            {
                out[f.field] = cfg_read_bits(args, start_word, bitpos, width);
            }
            if (f.align_after)
                cfg_align(bitpos);
        }
    }

    // ENCODE: flat field array in[F_CFG_COUNT] -> packed bits appended to args at
    // start_word. rows_active is given LSB-first and re-packed MSB-first. The Y
    // per-row weight offsets are taken from per_row_vals[0..Y-1] (each PARAMS_W
    // bits); pass nullptr to write zeros. args is grown as needed.
    inline void cfg_encode(const uint64_t in[F_CFG_COUNT], uint32_t start_word,
                           const CfgWidths &w, std::vector<uint32_t> &args,
                           const uint64_t *per_row_vals = nullptr)
    {
        size_t bitpos = 0;
        for (int i = 0; i < SAURIA_CFG_LAYOUT_N; i++)
        {
            const CfgFieldSpec &f = SAURIA_CFG_LAYOUT[i];
            uint32_t width = cfg_resolve_width(f.wkind, w);
            if (f.special == SP_SKIP_PER_ROW)
            {
                for (uint32_t y = 0; y < w.Y; y++)
                    cfg_write_bits(args, start_word, bitpos, width,
                                   per_row_vals ? per_row_vals[y] : 0);
            }
            else if (f.special == SP_ROWS_ACTIVE_MSB)
            {
                cfg_write_bits(args, start_word, bitpos, width,
                               cfg_bitrev(in[f.field], width));
            }
            else
            {
                cfg_write_bits(args, start_word, bitpos, width, in[f.field]);
            }
            if (f.align_after)
                cfg_align(bitpos);
        }
    }

} // namespace sauria

#endif // SAURIA_CFG_LAYOUT_H
