// tb_neo_top_program_hw.cpp -- the register program of the hardware bench tb/tb_neo_top.sv (neo_lite RTL revision 1, "Test config" block) through the model.
//
// It writes EXACTLY the AXI-Lite writes of the bench (RESIDUAL 16x160x160 and MAXPOOL 3x3 stride 1 40x80x80) to a model of the data flow controller
// (df_controller_top / neo_interface / neo_dma_controller, vector unit side only), loads DRAM like the bench, lets the DFC model run every tile and compares
// the output DRAM image with the golden of the hardware team.
//
// What is modelled (READ from the RTL, see HW_RTL_ALIGNMENT.md, section 14.6):
//   - the DFC register file: word >= 4 -> control_regs[word - 4] (82 regs = 40 + 29 core + 13 vector unit), word 0 start, 2 interrupt enable, 3 clear;
//   - neo_interface (vector unit only): 14 configuration writes 0x04..0x38 (reg 1 = opcode of the unit, overridden by the DFC from df_ctrl opcode; regs 2..13 from
//     control_regs[69 + k]; the 14th lands on a register that does not exist), per tile CONTROL start / CONTROL clear done, at the end CONTROL start = 0 with bit 3
//     (force buffer swap, last_sync_2);
//   - neo_dma_controller (vector unit only): tile order D0, S0, D1, S1(+R0), D2, S2(+R1) ... S(N-1)(+R(N-2)), S(N)(+R(N-1)) with D = bring (vbank0 [, vbank1]),
//     R = send vbank2, S = start of the unit; 1-D commands per repetition (btt bytes, SRAM step = stride, DRAM step = btt), tile offsets as in neo_dma_controller.sv:331-364;
//   - the register file of the unit: rtl_ref_gvu::GvuConfigRegsNeo(14 registers, rev 1) -- start edge flips the four buffer selects, CONTROL bit 3 flips them again;
//   - the unit: MAXPOOL = rtl_ref_gvu::GvuTop (neo_rev 1) cycle by cycle; RESIDUAL = rtl_ref_gvu::residual_hw_lane (NOT GvuTop: GvuTop has no clocked RESIDUAL of
//     the hardware yet -- a gap of the model, HW_RTL_ALIGNMENT.md, section 14.6); the DMA side of every bank is the buffer the unit does not use (1 - select).
// NOT modelled: the timing of the DFC / DMA (only the order and the buffers), the AXI beats and the realigner, the clock-domain crossing, the SAURIA core.
//
// Negative controls (--neg noswap | poolswap | all): the test must see a difference, otherwise it proves nothing.
//   noswap   : the last CONTROL write carries bit 3 = 0 (no forced swap) -> the last tile is read from the wrong buffer
//   poolswap : POOLING_0 is decoded the way neo_lite RTL revision 0 did (C and OH exchanged)
//
// Build (from the root of the model tree):  g++ -std=c++17 -O2 -Wall -I. tools/has/tb_neo_top_program_hw.cpp -o tb_neo_top_program_hw
// Run:   ./tb_neo_top_program_hw [residual|maxpool|all] [--neg noswap|poolswap|all] [--dump] [--trace file] [--resdir d] [--mpdir d]
//   --trace file : instead of the built-in program, apply the writes of "W <addr hex> <data hex>" lines (a trace from the host side, opcode as in the file)

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <memory>
#include <string>
#include <vector>
#include "has/rtl_ref_gvu_config_regs_neo.h"
#include "has/rtl_ref_gvu_residual_hw.h"
#include "has/rtl_ref_gvu_top.h"

namespace
{
    // ---- constants of tb_neo_top.sv / neo_addr_pkg / df_controller_pkg / sauria_pkg (int8 32x32) ----
    constexpr uint32_t NEO_OFFSET = 0x80000000u, CONTROLLER_OFFSET = NEO_OFFSET + 0u, DRAM_OFFSET = 0x40000000u;
    constexpr unsigned N_CTRL_REGS = 40, N_SAURIA_REGS = 2 + 17 + 5 + 5;                 // TOTAL_REGS_CON + ACT + WEI + OUT
    constexpr unsigned CONTROLLER_REG_BASE = 4;
    constexpr unsigned GVU_REG_BASE_TB = CONTROLLER_REG_BASE + N_CTRL_REGS + N_SAURIA_REGS;   // 73, as tb_neo_top.sv
    constexpr unsigned GVU_REG_BASE_IF = N_CTRL_REGS + N_SAURIA_REGS;                          // 69, as neo_interface.sv
    constexpr unsigned GVU_CFG_REGS = 13, N_REGS = N_CTRL_REGS + N_SAURIA_REGS + GVU_CFG_REGS;
    constexpr unsigned OP_RESIDUAL_DF = 4, OP_MAXPOOL_DF = 5;                                  // df_ctrl_pkg::neo_opcode_t
    constexpr unsigned GVU_OP_RESIDUAL = 6, GVU_OP_MAXPOOL = 7;                                // gvu_pkg::gvu_opcode_t
    constexpr uint32_t RESIDUAL_A = 0x00100000u, RESIDUAL_B = 0x00200000u, RESIDUAL_C = 0x00300000u, MAXPOOL_IN = 0x00100000u, MAXPOOL_OUT = 0x00200000u;
    constexpr int VBANK0_BYTES = 800 * 32, VBANK1_BYTES = 200 * 128, VBANK2_BYTES = 256 * 128;   // gvu_pkg.sv depths (VBANK1 200 as neo_lite RTL revision 0 and later)

    struct Wr { uint32_t addr, data; };

    // cfg_data_in of the bench is ONE variable: a partial assignment keeps the other bits of the write before
    struct ProgBuilder
    {
        std::vector<Wr> w;
        uint32_t cd{0};
        void set(uint64_t v) { cd = static_cast<uint32_t>(v); }
        void bits(int hi, int lo, uint64_t v)
        {
            const uint32_t m = static_cast<uint32_t>(((hi - lo + 1) >= 32 ? 0xFFFFFFFFull : ((1ull << (hi - lo + 1)) - 1ull)) << lo);
            cd = (cd & ~m) | (static_cast<uint32_t>(v << lo) & m);
        }
        void put_word(unsigned word) { w.push_back({CONTROLLER_OFFSET + word * 4u, cd}); }
        void put_ctrl(unsigned i) { put_word(CONTROLLER_REG_BASE + i); }
        void put_gvu(unsigned reg) { put_word(GVU_REG_BASE_TB + reg - 1u); }
    };

    // tb_neo_top.sv:443-697, OPCODE = RESIDUAL (C 16, H 160, W 160, C_TILE 1, H_TILE 80)
    std::vector<Wr> residual_program()
    {
        constexpr unsigned C = 16, H = 160, W = 160, C_TILE = 1, H_TILE = H / 2, W_TILE = W, NUM_TILE = C * H * W / (C_TILE * H_TILE * W_TILE);
        constexpr unsigned seq_len = C_TILE * H_TILE * W_TILE / 32;
        constexpr int64_t bias = 266712933412ll;
        ProgBuilder b;
        b.set(OP_RESIDUAL_DF); b.put_ctrl(39);
        b.bits(30, 30, 0); b.bits(29, 24, 30); b.bits(23, 16, static_cast<uint8_t>(-125)); b.bits(15, 0, seq_len); b.put_gvu(7);
        b.set(1073741824u); b.put_gvu(8);
        b.set(1055666676u); b.put_gvu(9);
        b.set(static_cast<uint32_t>(static_cast<uint64_t>(bias) & 0xFFFFFFFFu)); b.put_gvu(10);
        b.set(static_cast<uint32_t>(static_cast<uint64_t>(bias) >> 32)); b.put_gvu(11);
        b.set(NUM_TILE - 1); b.put_ctrl(21);
        b.set(C_TILE * H_TILE * W_TILE); b.put_ctrl(24);       // vbank0 btt
        b.set(0); b.put_ctrl(25);                              // vbank0 stride
        b.set(0); b.put_ctrl(26);                              // vbank0 rep lim
        b.set(C_TILE * H_TILE * W_TILE); b.put_ctrl(27);       // vbank0 tile move
        b.set(C_TILE * H_TILE * W_TILE); b.put_ctrl(28);       // vbank1 btt
        b.set(C_TILE * H_TILE * W_TILE); b.put_ctrl(29);       // vbank1 tile move
        b.set(C_TILE * H_TILE * W_TILE); b.put_ctrl(30);       // vbank2 btt
        b.set(0); b.put_ctrl(31);
        b.set(0); b.put_ctrl(32);
        b.set(C_TILE * H_TILE * W_TILE); b.put_ctrl(33);       // vbank2 tile move
        b.set(DRAM_OFFSET + RESIDUAL_A); b.put_ctrl(35);
        b.set(DRAM_OFFSET + RESIDUAL_B); b.put_ctrl(36);
        b.set(DRAM_OFFSET + RESIDUAL_C); b.put_ctrl(37);
        b.set(1); b.put_word(2);                               // enable the interrupt
        b.set(1); b.put_word(0);                               // enable the FSM
        b.set(1); b.put_word(3);                               // after the interrupt: clear
        return b.w;
    }

    // tb_neo_top.sv:443-697, OPCODE = MAXPOOL (C 40, H 80, W 80, POOLING_3X3, stride 1, C_TILE 2)
    std::vector<Wr> maxpool_program()
    {
        constexpr unsigned C = 40, H = 80, W = 80, K = 3, STRIDE = 1, MODE = 1 /* POOLING_3X3 */, C_TILE = 2, H_TILE = H, W_TILE = W;
        constexpr unsigned OH = (H - K) / STRIDE + 1, OW = (W - K) / STRIDE + 1, NUM_TILE = C * H * W / (C_TILE * H_TILE * W_TILE);
        constexpr unsigned OC_TILE = C_TILE, OH_TILE = (H_TILE - K) / STRIDE + 1, OW_TILE = (W_TILE - K) / STRIDE + 1;
        constexpr unsigned nw = W / 32 + (W % 32 != 0), now = OW / 32 + (OW % 32 != 0);
        ProgBuilder b;
        b.set(OP_MAXPOOL_DF); b.put_ctrl(39);
        b.bits(1, 0, MODE); b.bits(4, 2, STRIDE); b.bits(15, 8, nw); b.bits(23, 16, OH); b.bits(31, 24, C_TILE); b.put_gvu(12);
        b.bits(9, 0, nw * STRIDE); b.bits(25, 16, nw * H); b.put_gvu(13);
        b.set(NUM_TILE - 1); b.put_ctrl(21);
        b.set(W); b.put_ctrl(24);
        b.set(nw * 32); b.put_ctrl(25);
        b.set(C_TILE * H - 1); b.put_ctrl(26);
        b.set(C_TILE * H_TILE * W_TILE); b.put_ctrl(27);
        b.set(OW); b.put_ctrl(30);
        b.set(now * 32); b.put_ctrl(31);
        b.set(OC_TILE * OH - 1); b.put_ctrl(32);
        b.set(OC_TILE * OH_TILE * OW_TILE); b.put_ctrl(33);
        b.set(DRAM_OFFSET + MAXPOOL_IN); b.put_ctrl(35);
        b.set(DRAM_OFFSET + MAXPOOL_OUT); b.put_ctrl(37);
        b.set(1); b.put_word(2);
        b.set(1); b.put_word(0);
        b.set(1); b.put_word(3);
        return b.w;
    }

    struct Dram
    {
        std::vector<uint8_t> m = std::vector<uint8_t>(8u << 20, 0);
        uint64_t bad{0};
        bool ok(uint32_t a, size_t n) const { return a >= DRAM_OFFSET && static_cast<uint64_t>(a - DRAM_OFFSET) + n <= m.size(); }
        void rd(uint32_t a, uint8_t *d, size_t n) { if (!ok(a, n)) { bad++; std::memset(d, 0, n); return; } std::memcpy(d, &m[a - DRAM_OFFSET], n); }
        void wr(uint32_t a, const uint8_t *d, size_t n) { if (!ok(a, n)) { bad++; return; } std::memcpy(&m[a - DRAM_OFFSET], d, n); }
    };

    struct Options { bool noswap{false}, poolswap{false}; int rev{1}; };

    struct Counters
    {
        uint64_t dfc_writes{0}, gvu_writes{0}, tiles{0}, dma_cmds{0}, dma_bytes{0}, pipe_en_edges{0}, forced_swaps{0}, gvu_cycles{0}, range_errors{0}, ignored_writes{0};
        bool interrupt{false};
    };

    class NeoTopModel
    {
    public:
        NeoTopModel(Dram &d, const Options &o) : dram_(d), opt_(o), gv_(14, static_cast<unsigned>(o.rev))
        {
            regs_.fill(0u);
            bufs_[0][0].assign(VBANK0_BYTES, 0); bufs_[0][1].assign(VBANK0_BYTES, 0);
            bufs_[1][0].assign(VBANK1_BYTES, 0); bufs_[1][1].assign(VBANK1_BYTES, 0);
            bufs_[2][0].assign(VBANK2_BYTES, 0); bufs_[2][1].assign(VBANK2_BYTES, 0);
            rtl_ref_gvu::GvuTopParams tp;
            tp.neo_rev = opt_.rev;
            top_.reset(new rtl_ref_gvu::GvuTop(tp));
            reset_top();
        }

        // one AXI-Lite write of the host to the configuration window of neo_top (only the DFC window exists in this model)
        void write(uint32_t addr, uint32_t data)
        {
            const uint32_t off = addr - NEO_OFFSET;
            if (addr < NEO_OFFSET || ((off >> 16) & 0xFu) != 0u) { c_.ignored_writes++; return; }
            const unsigned word = (off & 0xFFFFu) >> 2;
            c_.dfc_writes++;
            if (word == 0) { if (data & 1u) run(); }
            else if (word == 2) irq_en_ = data & 1u;
            else if (word == 3) { if (data & 1u) c_.interrupt = false; }
            else if (word >= CONTROLLER_REG_BASE && word - CONTROLLER_REG_BASE < N_REGS) regs_[word - CONTROLLER_REG_BASE] = data;
            else c_.ignored_writes++;
        }
        const Counters &counters() const { return c_; }
        const std::string &error() const { return err_; }
        uint64_t forced() const { return gv_.forced_swaps(); }
        uint64_t edges() const { return gv_.enable_edges(); }

    private:
        struct Link { uint32_t btt, stride, rep_lim, move; };

        void reset_top()
        {
            rtl_ref_gvu::GvuTop::In i;
            for (int c = 0; c < 5; c++) top_->tick(i, false);
            top_->tick(i, true);
        }

        // ---- the AXI-Lite master towards the register file of the vector unit (gvu_core.sv, axi_lite_2ram): one write = one edge here ----
        void gvu_wr(unsigned byte_addr, uint32_t data)
        {
            rtl_ref_gvu::GvuConfigNeoIn in;
            in.byte_addr = byte_addr; in.wren = true; in.wdata = data; in.wmask = 0xFFFFFFFFu;
            gv_.tick(in);
            c_.gvu_writes++;
        }
        void gvu_idle(bool pipe_done)
        {
            rtl_ref_gvu::GvuConfigNeoIn in;
            in.gvu_pipe_done = pipe_done;
            gv_.tick(in);
        }

        // ---- DMA: 1-D commands between DRAM and the buffer of the bank that the unit does not use ----
        int dma_buf(int bank) const
        {
            const rtl_ref_gvu::GvuConfigNeoOut o = gv_.out();
            const bool sel = bank == 0 ? o.vbank0_buf_sel : bank == 1 ? o.vbank1_buf_sel : o.vbank2_buf_sel;
            return sel ? 0 : 1;
        }
        void bring(int bank, uint32_t dram_tile_off, const Link &l)
        {
            std::vector<uint8_t> &b = bufs_[bank][dma_buf(bank)];
            uint32_t local = 0;
            for (uint32_t r = 0; r <= l.rep_lim; r++)
            {
                if (local + l.btt > b.size()) { c_.range_errors++; break; }
                dram_.rd(dram_tile_off + r * l.btt, &b[local], l.btt);
                local += l.stride;
                c_.dma_cmds++; c_.dma_bytes += l.btt;
            }
        }
        void send(uint32_t dram_tile_off, const Link &l)
        {
            std::vector<uint8_t> &b = bufs_[2][dma_buf(2)];
            uint32_t local = 0;
            for (uint32_t r = 0; r <= l.rep_lim; r++)
            {
                if (local + l.btt > b.size()) { c_.range_errors++; break; }
                dram_.wr(dram_tile_off + r * l.btt, &b[local], l.btt);
                local += l.stride;
                c_.dma_cmds++; c_.dma_bytes += l.btt;
            }
        }

        // ---- the unit: runs the operation that the register file holds, returns when it reports pipe_done ----
        void run_unit()
        {
            const rtl_ref_gvu::GvuConfigNeoOut o = gv_.out();
            const int a0 = o.vbank0_buf_sel ? 1 : 0, a1 = o.vbank1_buf_sel ? 1 : 0, a2 = o.vbank2_buf_sel ? 1 : 0;
            if (o.op == GVU_OP_RESIDUAL)
            {
                rtl_ref_gvu::ResidualHwParams p;
                p.sa = o.residual_scale_a; p.sb = o.residual_scale_b; p.bias = o.residual_bias; p.sh = static_cast<int>(o.residual_out_shift);
                p.z = o.residual_out_z; p.sub = o.residual_sub_mode; p.logical_shift = false;
                const size_t n = static_cast<size_t>(o.residual_sequence_len) * 32u;
                if (n > bufs_[0][a0].size() || n > bufs_[1][a1].size() || n > bufs_[2][a2].size()) { c_.range_errors++; gvu_idle(true); return; }
                for (size_t i = 0; i < n; i++)
                    bufs_[2][a2][i] = static_cast<uint8_t>(rtl_ref_gvu::residual_hw_lane(static_cast<int8_t>(bufs_[0][a0][i]), static_cast<int8_t>(bufs_[1][a1][i]), p));
                c_.gvu_cycles += rtl_ref_gvu::residual_hw_cycles(n);
                gvu_idle(true);
            }
            else if (o.op == GVU_OP_MAXPOOL)
            {
                for (int r = 0; r < VBANK0_BYTES / 32; r++)
                    top_->scratchpad().sram(0, 0).poke(static_cast<unsigned>(r), rtl_ref_gvu::SpBytes(bufs_[0][a0].begin() + r * 32, bufs_[0][a0].begin() + (r + 1) * 32));
                rtl_ref_gvu::GvuTop::In i;
                i.i_gvu_op = rtl_ref_gvu::GVU_OP_MAXPOOL;
                i.i_pooling_mode = static_cast<int>(o.pooling_mode);
                i.i_pooling_stride = o.pooling_stride;
                i.i_pooling_num_w_tile = o.pooling_num_w_tile;
                i.i_pooling_c = opt_.poolswap ? o.pooling_oh : o.pooling_c;      // poolswap: the decoding of neo_lite RTL revision 0 (C and OH exchanged)
                i.i_pooling_oh = opt_.poolswap ? o.pooling_c : o.pooling_oh;
                i.i_pooling_c_move = o.pooling_c_move;
                i.i_pooling_h_move = o.pooling_h_move;
                i.i_pipeline_en = true;
                bool done = false;
                uint64_t c = 0;
                const uint64_t guard = 400000;
                for (; c < guard && !done; c++) done = top_->tick(i, true).o_maxpool_pipe_done;
                c_.gvu_cycles += c;
                gvu_idle(true);                                                   // pipe_done: the enable clears itself, the done flag is set
                i.i_pipeline_en = false;
                for (int q = 0; q < 4; q++) top_->tick(i, true);
                if (!done) { c_.range_errors++; reset_top(); }
                for (int w = 0; w < VBANK2_BYTES / 128; w++)
                {
                    const rtl_ref_gvu::SpBytes word = top_->scratchpad().sram(2, 0).word(static_cast<unsigned>(w));
                    for (int k = 0; k < 128 && k < static_cast<int>(word.size()); k++) bufs_[2][a2][static_cast<size_t>(w) * 128 + static_cast<size_t>(k)] = word[static_cast<size_t>(k)];
                }
            }
            else { err_ = "unit started with an opcode that this model does not run"; gvu_idle(true); }
        }

        void run()
        {
            const unsigned opcode = regs_[39] & 15u;
            if (opcode != OP_RESIDUAL_DF && opcode != OP_MAXPOOL_DF) { err_ = "unsupported df_controller opcode"; return; }
            const bool residual = opcode == OP_RESIDUAL_DF;
            if ((regs_[21] >> 18) & 1u) { err_ = "tile_lim bit 18 (start from register) is not modelled"; return; }
            const uint32_t n_tiles = regs_[21] + 1u;
            const Link l0{regs_[24], regs_[25], regs_[26], regs_[27]}, l1{regs_[28], 0u, 0u, regs_[29]}, l2{regs_[30], regs_[31], regs_[32], regs_[33]};
            uint32_t off0 = regs_[35], off1 = regs_[36], off2 = regs_[37];

            // neo_interface GVU_SEND_ARGS: 14 writes to 0x04 .. 0x38; the first carries the opcode of the unit
            for (unsigned k = 0; k <= GVU_CFG_REGS; k++)
            {
                uint32_t d = k == 0 ? (residual ? GVU_OP_RESIDUAL : GVU_OP_MAXPOOL) : (k < GVU_CFG_REGS ? regs_[GVU_REG_BASE_IF + k] : 0u);
                gvu_wr(0x04u + 4u * k, d);
            }
            auto bring_tile = [&]() { bring(0, off0, l0); if (residual) bring(1, off1, l1); };
            auto send_tile = [&]() { send(off2, l2); off2 += l2.move; };

            bring_tile();                                                         // D0
            for (uint32_t k = 0; k < n_tiles; k++)
            {
                const uint64_t e0 = gv_.enable_edges();
                gvu_wr(0x00u, 1u);                                                // S(k): CONTROL start (bit 3 = last_sync_2 = 0)
                if (gv_.enable_edges() != e0 + 1) { err_ = "start write did not raise the enable"; return; }
                run_unit();
                gvu_wr(0x00u, 4u);                                                // clear done after the interrupt
                c_.tiles++;
                off0 += l0.move; off1 += l1.move;                                 // advance
                if (k >= 1) send_tile();                                          // R(k-1)
                if (k + 1 < n_tiles) bring_tile();                                // D(k+1)
            }
            gvu_wr(0x00u, opt_.noswap ? 0u : 8u);                                 // S(N): start = 0, bit 3 = last_sync_2 (force buffer swap)
            send_tile();                                                          // R(N-1)
            c_.pipe_en_edges = gv_.enable_edges();
            c_.forced_swaps = gv_.forced_swaps();
            if (irq_en_) c_.interrupt = true;
        }

        Dram &dram_;
        Options opt_;
        std::array<uint32_t, N_REGS> regs_;
        bool irq_en_{false};
        rtl_ref_gvu::GvuConfigRegsNeo gv_;
        std::unique_ptr<rtl_ref_gvu::GvuTop> top_;
        std::vector<uint8_t> bufs_[3][2];
        Counters c_;
        std::string err_;
    };

    bool read_ints(const std::string &path, std::vector<int> &v, size_t expect)
    {
        std::ifstream f(path);
        if (!f) { std::fprintf(stderr, "cannot open %s\n", path.c_str()); return false; }
        int x;
        while (f >> x) v.push_back(x);
        if (expect && v.size() != expect) { std::fprintf(stderr, "%s: %zu values, expected %zu\n", path.c_str(), v.size(), expect); return false; }
        return true;
    }

    struct CaseResult { bool ran{false}; uint64_t miss{0}, total{0}, last_tile_miss{0}, other_tile_miss{0}, expect_last{0}; Counters c; std::string err; };

    CaseResult run_case(bool residual, const Options &opt, const std::string &dir, bool dump, const std::vector<Wr> *trace)
    {
        CaseResult r;
        Dram dram;
        const size_t n_in = residual ? 16u * 160u * 160u : 40u * 80u * 80u;
        const size_t n_out = residual ? n_in : 40u * 78u * 78u;
        const size_t tile_out = residual ? 12800u : 12168u;
        std::vector<int> a, b, gold;
        if (!read_ints(dir + (residual ? "/input_a.txt" : "/input.txt"), a, n_in)) { r.err = "input"; return r; }
        if (residual && !read_ints(dir + "/input_b.txt", b, n_in)) { r.err = "input_b"; return r; }
        if (!read_ints(dir + "/golden_output.txt", gold, n_out)) { r.err = "golden"; return r; }
        // data_init_block of the bench
        for (size_t i = 0; i < n_in; i++)
        {
            dram.m[(residual ? RESIDUAL_A : MAXPOOL_IN) + i] = static_cast<uint8_t>(a[i]);
            if (residual) dram.m[RESIDUAL_B + i] = static_cast<uint8_t>(b[i]);
        }
        const std::vector<Wr> prog = trace ? *trace : (residual ? residual_program() : maxpool_program());
        if (dump) for (const Wr &w : prog) std::printf("W %08x %08x\n", w.addr, w.data);
        NeoTopModel m(dram, opt);
        for (const Wr &w : prog) m.write(w.addr, w.data);
        r.ran = true; r.err = m.error(); r.c = m.counters();
        if (!r.err.empty()) return r;
        const uint32_t out = residual ? RESIDUAL_C : MAXPOOL_OUT;
        const size_t n_tiles = n_out / tile_out;
        // without the forced swap the last tile is read from the buffer of the tile before: it differs from the golden exactly where the golden of the
        // last two tiles differ (this is the exact expectation of the noswap control)
        for (size_t i = 0; i < tile_out; i++) if (gold[(n_tiles - 2) * tile_out + i] != gold[(n_tiles - 1) * tile_out + i]) r.expect_last++;
        for (size_t i = 0; i < n_out; i++)
        {
            const int got = static_cast<int8_t>(dram.m[out + i]);
            r.total++;
            if (got != gold[i])
            {
                r.miss++;
                (i / tile_out == n_tiles - 1 ? r.last_tile_miss : r.other_tile_miss)++;
                if (r.miss <= 3) std::printf("  miss %zu: got %d expected %d (tile %zu)\n", i, got, gold[i], i / tile_out);
            }
        }
        if (dram.bad) r.err = "DRAM accesses out of range: " + std::to_string(dram.bad);
        return r;
    }

    void print_case(const char *name, const char *tag, const CaseResult &r, const Options &o)
    {
        const Counters &c = r.c;
        std::printf("hwbench %-8s %-9s rev %d: miss %llu / %llu (last tile %llu, other tiles %llu)  tiles %llu  dfc_writes %llu  gvu_writes %llu  dma_cmds %llu  bytes %llu  pipe_en_edges %llu  forced_swaps %llu  unit_cycles %llu  range_errors %llu%s%s\n",
                    name, tag, o.rev, static_cast<unsigned long long>(r.miss), static_cast<unsigned long long>(r.total), static_cast<unsigned long long>(r.last_tile_miss),
                    static_cast<unsigned long long>(r.other_tile_miss), static_cast<unsigned long long>(c.tiles), static_cast<unsigned long long>(c.dfc_writes),
                    static_cast<unsigned long long>(c.gvu_writes), static_cast<unsigned long long>(c.dma_cmds), static_cast<unsigned long long>(c.dma_bytes),
                    static_cast<unsigned long long>(c.pipe_en_edges), static_cast<unsigned long long>(c.forced_swaps), static_cast<unsigned long long>(c.gvu_cycles),
                    static_cast<unsigned long long>(c.range_errors), r.err.empty() ? "" : "  ERROR: ", r.err.c_str());
    }
}

int main(int argc, char **argv)
{
    std::string which = "all", neg, resdir = "tools/has/gvu_top_vectors/residual_tile_rev1", mpdir = "tools/has/neo_top_sim/vectors/maxpool_multi_s1_k3_rev1", trace_path;
    bool dump = false;
    for (int i = 1; i < argc; i++)
    {
        const std::string a = argv[i];
        if (a == "residual" || a == "maxpool" || a == "all") which = a;
        else if (a == "--dump") dump = true;
        else if (a == "--neg" && i + 1 < argc) neg = argv[++i];
        else if (a == "--trace" && i + 1 < argc) trace_path = argv[++i];
        else if (a == "--resdir" && i + 1 < argc) resdir = argv[++i];
        else if (a == "--mpdir" && i + 1 < argc) mpdir = argv[++i];
        else { std::fprintf(stderr, "unknown argument %s\n", a.c_str()); return 2; }
    }
    std::vector<Wr> trace;
    if (!trace_path.empty())
    {
        std::ifstream f(trace_path);
        std::string t; unsigned a, d;
        char line[256];
        while (f.getline(line, sizeof line)) if (std::sscanf(line, "W %x %x", &a, &d) == 2) trace.push_back({a, d});
        if (trace.empty()) { std::fprintf(stderr, "no writes in %s\n", trace_path.c_str()); return 2; }
        (void)t;
    }
    bool pass = true;
    Options o;
    for (int pass_no = 0; pass_no < 2; pass_no++)
    {
        if (pass_no == 1 && neg.empty()) break;
        for (int s = 0; s < 2; s++)
        {
            const bool residual = s == 0;
            if ((residual && which == "maxpool") || (!residual && which == "residual")) continue;
            std::vector<std::pair<const char *, Options>> runs;
            if (pass_no == 0) runs.push_back({"normal", o});
            else
            {
                Options n1 = o; n1.noswap = true;
                Options n2 = o; n2.poolswap = true;
                if (neg == "noswap" || neg == "all") runs.push_back({"noswap", n1});
                if ((neg == "poolswap" || neg == "all") && !residual) runs.push_back({"poolswap", n2});
            }
            for (auto &ru : runs)
            {
                const CaseResult r = run_case(residual, ru.second, residual ? resdir : mpdir, dump && pass_no == 0, trace.empty() ? nullptr : &trace);
                print_case(residual ? "RESIDUAL" : "MAXPOOL", ru.first, r, ru.second);
                if (!r.ran || !r.err.empty()) { pass = false; continue; }
                if (pass_no == 0) { if (r.miss != 0) pass = false; }
                else if (ru.second.noswap)
                {   // the last tile only, and most of it
                    const uint64_t tile = residual ? 12800 : 12168, want = residual ? 12000 : 11000;
                    const bool ok = r.other_tile_miss == 0 && r.last_tile_miss == r.expect_last && r.last_tile_miss > 0;
                    std::printf("  negative noswap: %s (exact expectation: only the last tile, %llu outputs = positions where the golden of tile N-2 and N-1 differ; "
                                "the threshold of prediction P-L4.7 (>= %llu of %llu) is %s)\n",
                                ok ? "DETECTED" : "NOT as expected -- stop and report", static_cast<unsigned long long>(r.expect_last), static_cast<unsigned long long>(want),
                                static_cast<unsigned long long>(tile), r.last_tile_miss >= want ? "met" : "NOT met");
                    if (!ok) pass = false;
                }
                else
                {
                    const bool ok = r.miss * 10 >= r.total * 9;
                    std::printf("  negative poolswap: %s (expected: >= 90 %% of the outputs differ)\n", ok ? "DETECTED as predicted" : "NOT as predicted -- stop and report");
                    if (!ok) pass = false;
                }
            }
        }
    }
    std::printf("RESULT: %s\n", pass ? "PASS" : "FAIL");
    return pass ? 0 : 1;
}
