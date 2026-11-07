// hw_capability.h -- which instructions the hardware (neo_lite RTL revision 1) can run, and why not. Pure C++17, no SystemC.
//
// hw_check(LayerInstr) gives a verdict and the reasons for one instruction of the model. The reason codes are THOSE OF THE HOST-SIDE
// TOOL (COMPILER_GUIDE.md, support matrix; has/has_host_neo.h): the rules here are its rules written out -- a code is given when a
// field of the instruction does not come back from the 82 configuration words (the encoder / decoder round trip) -- so the counts per
// code equal that table. A-* = fault or gap of the delivered RTL, B-* = the model goes beyond the hardware, P-* = note only.
//
//   verdict   NO         a B code that only the hardware or the model can remove (ACTLUT, REQ3, CINORD, HALO, TMAJ, POOLK, AVG, FA, LN, DMA)
//             PATCH      an A code (RTL fault or gap that a small RTL change removes) and nothing worse
//             TRANSFORM  only B codes the compiler removes (PAD, INDIM, EDGE, PADTAIL, STRIDE, FSCALE, ADDMAP, ADDTILE, POOLPAD, POOLFIT, FIELD)
//             OK         no A or B code
//
//   A-H1   idx 0..11 of the DFC cannot be written over AXI (df_controller_top.sv:139-140), MEASURED 12 / 82 wrong: every GEMM_FUSED
//   A-H3   FUSED_ATTN / LAYERNORM / AVGPOOL have an opcode number but no DFC path (neo_interface.sv:178,337-343; neo_dma_controller.sv:339-362)
//   A-H4   sbank is never loaded by the DFC (neo_dma_controller.sv:742 TODO): per-channel scale of GEMM_FUSED
//   B-ACTLUT   activation: only "none / table" fits the words, the table address and the built-in modes have no cell
//   B-REQ3     bias and shift arrays have no cell (scale, shift, bias would be three blocks of the sbank)
//   B-FSCALE   scale given as a float (no per-channel array)
//   B-PAD      padding of the input window (npu_pad2d is not connected in neo_top; DFC offsets are unsigned)
//   B-INDIM    input extent differs from the one the tile grid gives (follows from padding or an edge tile)
//   B-EDGE     edge tile: uniform grid only (neo_dma_pointer_generator.sv); tensors must be allocated to a tile multiple
//   B-PADTAIL  PAD_TAIL / other FLAGS: the edge tile is computed full size and written whole
//   B-CINORD   Cin split: the model's order differs from loop_order (c, k, space)
//   B-HALO     halo order (no such loop_order)             B-STRIDE  sx != sy
//   B-TMAJ     corner turn after the epilogue (no RTL)
//   B-ADDMAP   ADD parameters that cannot be one RESIDUAL (has::ResidualHw::map)      B-ADDTILE  ADD length not a multiple of 32
//   B-POOLPAD  padded pooling window            B-POOLFIT  a pooling field overflows its cell (8 / 10 bit) or the stride / mode cell
//   B-POOLK    window outside 2..5              B-AVG      Avg_Scale / Avg_Shift have no cell
//   B-FA, B-LN FUSED_ATTN, LAYERNORM (LN: the FSM stays in IDLE and never finishes, gvu_pipe_controller.sv)
//   B-DMA      slice / concat / COPY / upsample / reshape: no opcode (1-D DMA)        B-FIELD  any other field that does not come back
//   P-SPLIT    HAS_SKIP: GEMM_FUSED then ADD (a RESIDUAL)
//   P-H2       lut_init reads the wrong cell (neo_interface.sv:225-227): activation tables and Softmax (A-fault of the RTL, not in the host-side table)
//   P-POOLDENSE MAXPOOL stride 2 / 4 with window != stride: wrong write counter of the dense layout (MEASURED)
// Not checked: contiguity of the tile in DRAM.
// The same rules, on the register values of a program, are in tools/has/hw_check_program.py; the two print the same lines.
// With HAS_HW_CHECK=1, HasNpuTop prints one line per instruction it runs ("[hw-check] i=<n> op=<name> HW revision 1: <verdict> <codes>") and
// a summary table at exit. The knob changes nothing else.
#ifndef HAS_HW_CAPABILITY_H
#define HAS_HW_CAPABILITY_H

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>

#include "has/has_mmio_compat.h"
#include "has/rtl_ref_gvu_residual_hw.h"

namespace has
{
    enum HwCode : int
    {
        HW_A_H1, HW_A_H3, HW_A_H4,
        HW_B_ACTLUT, HW_B_REQ3, HW_B_FSCALE, HW_B_PAD, HW_B_INDIM, HW_B_EDGE, HW_B_PADTAIL, HW_B_CINORD, HW_B_HALO, HW_B_STRIDE, HW_B_TMAJ,
        HW_B_ADDMAP, HW_B_ADDTILE, HW_B_POOLPAD, HW_B_POOLFIT, HW_B_POOLK, HW_B_AVG, HW_B_FA, HW_B_LN, HW_B_DMA, HW_B_FIELD,
        HW_P_SPLIT, HW_P_H2, HW_P_POOLDENSE,
        HW_N_CODES
    };
    inline const char *hw_code_name(int c)
    {
        static const char *n[HW_N_CODES] = {"A-H1", "A-H3", "A-H4",
                                            "B-ACTLUT", "B-REQ3", "B-FSCALE", "B-PAD", "B-INDIM", "B-EDGE", "B-PADTAIL", "B-CINORD", "B-HALO", "B-STRIDE", "B-TMAJ",
                                            "B-ADDMAP", "B-ADDTILE", "B-POOLPAD", "B-POOLFIT", "B-POOLK", "B-AVG", "B-FA", "B-LN", "B-DMA", "B-FIELD",
                                            "P-SPLIT", "P-H2", "P-POOLDENSE"};
        return n[c];
    }
    // severity per code: 'A' RTL fault / gap, 'T' removed by the compiler, 'N' removed only by the hardware or the model, 'I' note only
    inline char hw_code_severity(int c)
    {
        //                                     A-H1 A-H3 A-H4 ACTLUT REQ3 FSCALE PAD INDIM EDGE PADTAIL CINORD HALO STRIDE TMAJ ADDMAP ADDTILE POOLPAD POOLFIT POOLK AVG FA LN DMA FIELD SPLIT H2 POOLDENSE
        static const char s[HW_N_CODES + 1] = "AAA" "NN" "TTTTT" "NN" "T" "N" "TT" "TT" "N" "NNNN" "T" "III";
        return s[c];
    }

    struct HwVerdict
    {
        uint32_t codes{0};   // bit per HwCode
        const char *verdict() const
        {
            bool no = false, pa = false, tr = false;
            for (int c = 0; c < HW_N_CODES; c++)
                if (codes & (1u << c)) { const char s = hw_code_severity(c); no |= s == 'N'; pa |= s == 'A'; tr |= s == 'T'; }
            return no ? "NO" : (pa ? "PATCH" : (tr ? "TRANSFORM" : "OK"));
        }
        std::string codes_str() const
        {
            std::string r;
            for (int c = 0; c < HW_N_CODES; c++)
                if (codes & (1u << c)) { if (!r.empty()) r += ","; r += hw_code_name(c); }
            return r;
        }
    };

    inline const char *hw_op_name(const LayerInstr &li)
    {
        switch (li.opcode)
        {
        case 0x12: return "GEMM_FUSED";
        case 0x13: return "FUSED_ATTN";
        case 0x14: return "LAYERNORM";
        case 0x16: return "DMA_TENSOR";
        case 0x15:
            switch (li.mode)
            {
            case 0: return "ADD";
            case 1: return "MAX_POOL";
            case 5: return "AVG_POOL";
            case 6: return "SLICE";
            case 7: return "CONCAT";
            default: return "ELEM_WISE";
            }
        default: return "OP";
        }
    }

    namespace hw_detail
    {
        inline int cdiv(int a, int b) { return (a + b - 1) / b; }
        // fields of a pooling instruction as the words give them back (has_host_neo.h decode_regs): window 2..5, stride 3 bit, W tiles 8 bit,
        // rows of a channel tile times W tiles 10 bit; the padded size comes back instead of the size
        inline void pool_codes(int h, int w, int k, int s, int p, int pool_mode, bool avg, uint32_t &codes)
        {
            auto set = [&codes](int c) { codes |= 1u << c; };
            const int hp = h + 2 * p, wp = w + 2 * p, nw = cdiv(wp, 32);
            if (k < 2 || k > 5) set(HW_B_POOLK);
            if (s < 0 || s > 7) set(HW_B_POOLFIT);
            if (pool_mode != 0) set(HW_B_POOLFIT);
            if (wp != w) set(HW_B_POOLPAD);
            const int n8 = nw & 0xFF;
            const int h_back = n8 ? ((nw * hp) & 0x3FF) / n8 : 0;
            if (h_back != h) set(p > 0 ? HW_B_POOLPAD : HW_B_POOLFIT);
            if (p > 0) set(HW_B_POOLPAD);
            (void)avg;
        }
        // the ADD parameters of the words (RES0..2) give back zpO (int8), sO (6 bit), SA, SB; zpA, zpB, sA, sB must be 0 and SO 1
        inline bool add_words_differ(const AddQ &a)
        {
            return a.zpA != 0 || a.zpB != 0 || a.sA != 0 || a.sB != 0 || a.SO != 1u || a.zpO < -128 || a.zpO > 127 || a.sO < 0 || a.sO > 63;
        }
    }

    // `skip_add` is the ADD half of an HAS_SKIP split, given as the instruction that follows the GEMM (the caller passes it as an ordinary ADD).
    inline HwVerdict hw_check(const LayerInstr &li)
    {
        using namespace hw_detail;
        HwVerdict v;
        auto set = [&v](int c) { v.codes |= 1u << c; };
        switch (li.opcode)
        {
        case 0x12:
        {
            const LayerGeom &g = li.geom;
            set(HW_A_H1);
            if (li.per_channel) set(HW_A_H4);
            if (!li.last_of_host) set(HW_P_SPLIT);
            // normalized as the host-side tool does: tile sizes clamped to the tensor (the channel tile not for PAD_TAIL)
            const int cout_t = g.pad_tail ? g.cout_t : std::min(g.cout_t, g.cout), h_t = std::min(g.h_t, g.oh), w_t = std::min(g.w_t, g.ow);
            if (g.cout > 0 && g.oh > 0 && g.ow > 0 && cout_t > 0 && h_t > 0 && w_t > 0)
            {
                if (g.cout % cout_t != 0 || g.oh % h_t != 0 || g.ow % w_t != 0) set(HW_B_EDGE);
                const int Cw = cdiv(g.ow, w_t) * w_t, Ch = cdiv(g.oh, h_t) * h_t;
                const int Aw = (Cw - 1) * g.sx + g.ekw(), Ah = (Ch - 1) * g.sx + g.ekh();
                if (li.in_w != Aw || li.in_h != Ah) set(HW_B_INDIM);
            }
            if (g.pad_t != 0 || g.pad_l != 0 || g.pad_b != 0 || g.pad_r != 0 || li.pad_val != 0) set(HW_B_PAD);
            if (li.flags != 0) set(HW_B_PADTAIL);
            if (g.sx != g.sy) set(HW_B_STRIDE);
            if (g.kw == 1 && g.kh > 1 && g.dil > 1) set(HW_B_FIELD);   // the dilation pattern of one column does not come back
            if (li.cin_t > 0 && li.cin_t < li.in_c) set(HW_B_CINORD);
            if (li.tmaj_c0 >= 0) set(HW_B_TMAJ);
            if (li.act != 0 || li.lut_addr != 0) set(HW_B_ACTLUT);
            if (li.act == 2 || li.act == 3) set(HW_P_H2);
            if (li.bias_addr != 0 || li.shift_addr != 0) set(HW_B_REQ3);
            if (!li.per_channel && (li.bcast_S != 1u || li.bcast_s != 0)) set(HW_B_FSCALE);
            break;
        }
        case 0x15:
            if (li.mode == 0)
            {
                if (li.n % 32 != 0) set(HW_B_ADDTILE);
                rtl_ref_gvu::ResidualHwParams p;
                const bool ok = rtl_ref_gvu::ResidualHw::map(li.add.zpA, li.add.zpB, li.add.zpO, int64_t(li.add.SA), int64_t(li.add.SB), int64_t(li.add.SO),
                                                             li.add.sA, li.add.sB, li.add.sO, 1, true, &p);
                if (!ok && add_words_differ(li.add)) set(HW_B_ADDMAP);
            }
            else if (li.mode == 1 || li.mode == 5)
            {
                pool_codes(li.h, li.w, li.pool_k, li.pool_s, li.pool_p, li.pool_mode, li.mode == 5, v.codes);
                if (li.b_addr != 0) set(HW_B_POOLFIT);   // B_ADDR left over from an earlier instruction: the words do not give it back
                if (li.mode == 5) { set(HW_A_H3); if (li.avg_scale != 0 || li.avg_shift != 0) set(HW_B_AVG); }
                if (li.mode == 1 && (li.pool_s == 2 || li.pool_s == 4) && li.pool_k != li.pool_s) set(HW_P_POOLDENSE);
            }
            else if (li.mode >= 6 && li.mode <= 9) set(HW_B_DMA);   // slice, concat, upsample, reshape
            break;
        case 0x16: set(HW_B_DMA); break;
        case 0x13: set(HW_A_H3); set(HW_B_FA); set(HW_P_H2); break;
        case 0x14: set(HW_A_H3); set(HW_B_LN); break;
        default: break;
        }
        return v;
    }

    // HAS_HW_CHECK=1: one line per instruction the DFC takes, a table at exit.
    class HwCheck
    {
    public:
        static bool on() { static const bool v = [] { const char *e = std::getenv("HAS_HW_CHECK"); return e && std::atoi(e) != 0; }(); return v; }
        static void instr(const LayerInstr &li)
        {
            HwCheck &h = inst();
            const HwVerdict v = hw_check(li);
            std::printf("[hw-check] i=%u op=%s HW revision 1: %s%s%s\n", li.host_seq ? li.host_seq - 1 : 0u, hw_op_name(li), v.verdict(), v.codes ? " " : "", v.codes_str().c_str());
            h.count(v);
        }

    private:
        uint64_t n_{0}, ok_{0}, tr_{0}, pa_{0}, no_{0}, per_code_[HW_N_CODES]{};
        static HwCheck &inst() { static HwCheck h; return h; }
        HwCheck() { std::atexit(&HwCheck::report); }
        void count(const HwVerdict &v)
        {
            n_++;
            const std::string s = v.verdict();
            if (s == "OK") ok_++; else if (s == "TRANSFORM") tr_++; else if (s == "PATCH") pa_++; else no_++;
            for (int c = 0; c < HW_N_CODES; c++) if (v.codes & (1u << c)) per_code_[c]++;
        }
        static void report()
        {
            const HwCheck &h = inst();
            std::printf("[hw-check] summary: %llu instructions, OK %llu, TRANSFORM %llu, PATCH %llu, NO %llu\n", (unsigned long long)h.n_, (unsigned long long)h.ok_,
                        (unsigned long long)h.tr_, (unsigned long long)h.pa_, (unsigned long long)h.no_);
            for (int c = 0; c < HW_N_CODES; c++)
                if (h.per_code_[c]) std::printf("[hw-check]   %-10s %c %llu\n", hw_code_name(c), hw_code_severity(c), (unsigned long long)h.per_code_[c]);
            std::printf("[hw-check] not checked: contiguity of the tile in DRAM\n");
        }
    };
}

#endif // HAS_HW_CAPABILITY_H
