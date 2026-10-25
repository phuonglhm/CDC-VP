// gvu_regs_dfc.h -- the register sequences with which the data flow controller configures and starts the vector unit (HAS_GVU_REGS=1),
// one function per operation. They only build the list of writes (byte address, data); has::GvuRegMaster issues them, the register
// file is rtl_ref_gvu::GvuConfigRegs (gvu_config_regs.sv). Register map (gvu_pkg.sv gvu_cfg_reg_t, word index = byte address / 4):
//   0 CONTROL [0] enable [1] done [2] clear done (write 1) [6:3] opcode    1 LUT [0] act reset   2 SCRATCHPAD [2] vbank1 buffer
//   3 GEMM_FUSED [0] skip activation [15:8] output zero point (int8)       7 POOLING_0 mode / stride / W tiles / C / OH    8 POOLING_1 moves
//   4 token length [8:0] / tiles [17:9] (residual add: rows of the run, 1)  9..15 residual add: scales, shifts + rounding + width, zero points,
//   first rows of A / B / sum, lane mask of the last row (register proposal for the hardware, not in the RTL drop)
//   16..24 AGU_ND (proposal, has/gvu_agu_nd.h): AGU_ND_CTRL, source descriptor (17..20), destination descriptor (21..24)
// The enable bit clears itself when the unit reports pipe_done (Reg 0 bit 0), and bit 1 (done) is set; the DFC sees pipe_done on a wire
// (df_controller_top.sv: gvu_pipe_done) and clears the flag with a write of Reg 0 bit 2.
#ifndef HAS_GVU_REGS_DFC_H
#define HAS_GVU_REGS_DFC_H

#include <array>
#include <cstdint>
#include <vector>
#include "has/gvu_reg_master.h"
#include "has/gvu_agu_nd.h"

namespace has
{
    namespace gvu_regs_dfc
    {
        typedef GvuRegMaster::W W;
        enum Reg : unsigned { CONTROL = 0, LUT = 1, SCRATCHPAD = 2, GEMM_FUSED = 3, SOFTMAX_0 = 4, POOLING_0 = 7, POOLING_1 = 8,
                              RES_SCALE_A = 9, RES_SCALE_B = 10, RES_SCALE_OUT = 11, RES_SHIFT = 12, RES_ZP = 13, RES_ROW = 14, RES_LASTMASK = 15 };
        enum Op : unsigned { OP_GEMM_FUSED = 2, OP_RESIDUAL = 6, OP_MAXPOOL = 7 };
        inline uint32_t addr(unsigned reg) { return reg * 4u; }
        inline uint32_t control(unsigned op, bool en) { return (op << 3) | (en ? 1u : 0u); }

        // GEMM_FUSED layer: output zero point (low byte, the register has 8 bits), skip activation, the unit's table flag cleared when the
        // layer brings a new activation table after an earlier one (reset 1 then 0), then the start (enable + opcode).
        inline std::vector<W> layer_begin(int zp, bool skip_act, bool table_reset)
        {
            std::vector<W> s;
            s.push_back(W{addr(GEMM_FUSED), (static_cast<uint32_t>(zp) & 0xFFu) << 8 | (skip_act ? 1u : 0u), 0xF});
            if (table_reset) { s.push_back(W{addr(LUT), 1u, 0xF}); s.push_back(W{addr(LUT), 0u, 0xF}); }
            s.push_back(W{addr(CONTROL), control(OP_GEMM_FUSED, true), 0xF});
            return s;
        }
        // End of an operation: clear the done flag (the opcode bits of the write are not looked at by the unit).
        inline std::vector<W> clear_done() { return {W{addr(CONTROL), 1u << 2, 0xF}}; }
        // One max pool run: window mode (k - 2), stride, W tiles, C, OH (POOLING_0), tile moves (POOLING_1), start.
        inline std::vector<W> pool_run(unsigned mode, unsigned stride, unsigned num_w_tile, unsigned c, unsigned oh, unsigned c_move, unsigned h_move)
        {
            std::vector<W> s;
            s.push_back(W{addr(POOLING_0), (mode & 3u) | (stride & 7u) << 2 | (num_w_tile & 0xFFu) << 8 | (c & 0xFFu) << 16 | (oh & 0xFFu) << 24, 0xF});
            s.push_back(W{addr(POOLING_1), (h_move & 0x3FFu) | (c_move & 0x3FFu) << 10, 0xF});
            s.push_back(W{addr(CONTROL), control(OP_MAXPOOL, true), 0xF});
            return s;
        }
        // Residual add: the parameters of one add and the values of the registers they go to.
        struct AddRegs
        {
            int32_t scale_a{0}, scale_b{0}, scale_out{0};
            unsigned shift_a{0}, shift_b{0}, shift_out{0}, round{0};
            bool out_int16{false};
            int zp_a{0}, zp_b{0}, zp_out{0};
            unsigned a_row{0}, b_row{0}, sum_row{0};
        };
        // One residual add run of `rows` rows (at most 128): the first run of a job writes the whole configuration (Reg 9..13, Reg 4, Reg 14,
        // Reg 15) before the start; a later run of the same job writes Reg 4 only when the number of rows changes and Reg 15 only when the lane
        // mask of the last row changes, then the start (enable + opcode RESIDUAL).
        inline std::vector<W> add_run(bool first, const AddRegs &c, unsigned rows, uint32_t last_mask, unsigned prev_rows, uint32_t prev_mask)
        {
            std::vector<W> s;
            if (first)
            {
                s.push_back(W{addr(RES_SCALE_A), static_cast<uint32_t>(c.scale_a), 0xF});
                s.push_back(W{addr(RES_SCALE_B), static_cast<uint32_t>(c.scale_b), 0xF});
                s.push_back(W{addr(RES_SCALE_OUT), static_cast<uint32_t>(c.scale_out), 0xF});
                s.push_back(W{addr(RES_SHIFT), (c.shift_a & 0xFFu) | (c.shift_b & 0xFFu) << 8 | (c.shift_out & 0xFFu) << 16 | (c.round & 7u) << 24 |
                                                   (c.out_int16 ? 1u << 27 : 0u), 0xF});
                s.push_back(W{addr(RES_ZP), (static_cast<uint32_t>(c.zp_a) & 0xFFu) | (static_cast<uint32_t>(c.zp_b) & 0xFFu) << 8 |
                                                (static_cast<uint32_t>(c.zp_out) & 0xFFu) << 16, 0xF});
            }
            if (first || rows != prev_rows) s.push_back(W{addr(SOFTMAX_0), (rows & 0x1FFu) | 1u << 9, 0xF});
            if (first) s.push_back(W{addr(RES_ROW), (c.a_row & 0x3FFu) | (c.b_row & 0x3FFu) << 10 | (c.sum_row & 0x3FFu) << 20, 0xF});
            if (first || last_mask != prev_mask) s.push_back(W{addr(RES_LASTMASK), last_mask, 0xF});
            s.push_back(W{addr(CONTROL), control(OP_RESIDUAL, true), 0xF});
            return s;
        }

        // AGU_ND (Reg 16..24): the words of AGU_ND_CTRL and of the two descriptors that differ from the ones the DFC wrote last (shadow,
        // reset value 0); the caller puts them ahead of the sequence of the operation, so they land before its Reg 0 start.
        inline std::vector<W> agu_nd(const rtl_ref_gvu::AguNdCtrl &c, const rtl_ref_gvu::AguNdDesc &src, const rtl_ref_gvu::AguNdDesc &dst,
                                     std::array<uint32_t, 9> &shadow)
        {
            namespace R = rtl_ref_gvu;
            std::array<uint32_t, 9> v{};
            v[0] = R::AguNdRegs::ctrl(c);
            const std::array<uint32_t, 4> a = R::AguNdRegs::pack(src), b = R::AguNdRegs::pack(dst);
            for (unsigned k = 0; k < 4; k++) { v[1 + k] = a[k]; v[5 + k] = b[k]; }
            std::vector<W> s;
            for (unsigned k = 0; k < 9; k++)
                if (v[k] != shadow[k]) { s.push_back(W{addr(R::AguNdRegs::CTRL + k), v[k], 0xF}); shadow[k] = v[k]; }
            return s;
        }
    }
}

#endif // HAS_GVU_REGS_DFC_H
