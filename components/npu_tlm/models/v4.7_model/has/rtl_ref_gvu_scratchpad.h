// rtl_ref_gvu_scratchpad.h -- Scratchpad of the vector unit, ported line by line from the RTL (neo_core/gvu/scratchpad):
// gvu_scratchpad.sv, sram_single_port.sv, sram_dual_port.sv, with the bank depths of gvu_pkg.sv. Pure C++17, no SystemC.
// The models of the same memory in the instruction path are has/gvu_scratchpad.h (flat byte memory) and has/gvu_sp_map.h
// (ports and capacity); nothing here is used by it.
// Four banks, each built from two memories (buffer 0 and buffer 1). Per bank a select input (i_*_buf_sel) gives one buffer
// to the accelerator ports and the other one to the DMA side:
//   bank    word      depth  bytes / buffer  memory        accelerator ports
//   sbank   32 bits     768        3 072     dual port     port 0 int32 (word address), port 1 int8 (byte address)
//   vbank0  256 bits    800       25 600     single port   port 0 v32int8
//   vbank1  1024 bits    32        4 096     single port   port 0 v32int32, port 1 v32int16, port 2 v32int8 (i_vbank1_port_sel)
//   vbank2  1024 bits   256       32 768     single port   port 0 v32int32, port 1 v32int8 (i_vbank2_port_sel)
// The narrow ports address parts of a word: the low address bits pick the part (written through a shifted mask, read by
// slicing the word); the part select of a read is the low address bits delayed by the read latency of the bank, so it
// belongs to the access that produced the data (narrow_sel_delayed; false takes it from the address input of the cycle
// in which the data is used, as earlier RTL revisions did).
// Read data: the memory registers its output (one clock) and each bank adds a pipeline register (SBANK_PIPE, VBANK*_PIPE,
// default 1), so data follows the address by two clocks. A read in the cycle of a write to the same word on the other port
// of sbank returns the old contents; an access with wen = 1 does not update the read data.
// Two points of the RTL:
//   Byte mask of the memories: one mask bit per byte ("mem[addr][i*8 +: 8] <= i_wdata[i*8 +: 8]"). Earlier RTL revisions
//     indexed with i instead of 8 * i, so that mask bit i wrote bits [i, i + 8) of the word and a word of DW bits could
//     only be changed in its low DW / 8 + 7 bits; wmask_as_written = true reproduces that.
//   DMA side: the dma_* signals of every bank are declared and never driven ("TODO: define DMA access for each bank"),
//     and o_dma_rdata has no driver. The buffer not given to the accelerator is therefore never accessed; this port has
//     no DMA access. An address at or above the depth of a bank is not written; its read value is not ported.
#ifndef RTL_REF_GVU_SCRATCHPAD_H
#define RTL_REF_GVU_SCRATCHPAD_H

#include <array>
#include <cstdint>
#include <cstdlib>
#include <vector>
#include "has/rtl_ref_gvu_fu.h"

namespace rtl_ref_gvu
{
    // Rows of vbank#1: 32 in the earlier RTL drops, 200 (25 KB) in gvu_pkg.sv of neo_lite@9b704bb. Env HAS_GVU_VBANK1_DEPTH=200 selects the new
    // depth; unset = 32 (nothing changes).
    inline int vbank1_depth_default()
    {
        static const int v = [] { const char *e = std::getenv("HAS_GVU_VBANK1_DEPTH"); const int d = e ? std::atoi(e) : 0; return d > 0 ? d : 32; }();
        return v;
    }

    struct ScratchpadParams
    {
        int NUM_LANE{32};
        int SBANK_DEPTH{768}, VBANK0_DEPTH{800}, VBANK1_DEPTH{vbank1_depth_default()}, VBANK2_DEPTH{256};   // gvu_pkg.sv
        bool SBANK_PIPE{true}, VBANK0_PIPE{true}, VBANK1_PIPE{true}, VBANK2_PIPE{true};
        bool wmask_as_written{false};
        bool narrow_sel_delayed{true};
        static int clog2(int d) { int w = 0; while ((1 << w) < d) w++; return w; }
        int SBANK_AW() const { return clog2(SBANK_DEPTH); }
        int VBANK0_AW() const { return clog2(VBANK0_DEPTH); }
        int VBANK1_AW() const { return clog2(VBANK1_DEPTH); }
        int VBANK2_AW() const { return clog2(VBANK2_DEPTH); }
        // Bytes of one buffer of each bank.
        uint32_t vbank_bytes(int bank) const
        {
            return bank == 0 ? static_cast<uint32_t>(VBANK0_DEPTH * NUM_LANE) : static_cast<uint32_t>((bank == 1 ? VBANK1_DEPTH : VBANK2_DEPTH) * 4 * NUM_LANE);
        }
        uint32_t sbank_bytes() const { return static_cast<uint32_t>(SBANK_DEPTH) * 4u; }
        int read_latency(int bank) const { return 1 + ((bank == 0 ? VBANK0_PIPE : bank == 1 ? VBANK1_PIPE : bank == 2 ? VBANK2_PIPE : SBANK_PIPE) ? 1 : 0); } // bank 3 = sbank
    };

    typedef std::vector<uint8_t> SpBytes;                                // least significant byte first

    // ---- sram_single_port.sv / sram_dual_port.sv -----------------------------------------------------------------------
    // One access per port and clock: en && wen writes through the mask, en && !wen loads o_rdata with the word before
    // the edge. Dual port: both ports see the contents before the edge; when both write the same bit, port 1 wins.
    class SpSram
    {
    public:
        struct Access { bool en{false}, wen{false}; unsigned addr{0}; SpBytes wmask; SpBytes wdata; };   // wmask: bit i = wmask[i / 8] >> (i % 8)
        SpSram(int depth, int word_bytes, int ports, bool wmask_as_written)
            : depth_(depth), wb_(word_bytes), as_written_(wmask_as_written), mem_(static_cast<size_t>(depth) * static_cast<size_t>(word_bytes), 0)
        {
            for (int p = 0; p < ports; p++) rdata_.push_back(SpBytes(static_cast<size_t>(word_bytes), 0));
        }
        const SpBytes &o_rdata(int port) const { return rdata_[static_cast<size_t>(port)]; }
        void tick(const Access *acc, int n)
        {
            std::vector<SpBytes> rd;
            for (int p = 0; p < n; p++)
                rd.push_back((acc[p].en && !acc[p].wen && static_cast<int>(acc[p].addr) < depth_) ? word(acc[p].addr) : rdata_[static_cast<size_t>(p)]);
            for (int p = 0; p < n; p++)
                if (acc[p].en && acc[p].wen && static_cast<int>(acc[p].addr) < depth_) write(acc[p]);
            for (int p = 0; p < n; p++) rdata_[static_cast<size_t>(p)] = rd[static_cast<size_t>(p)];
        }
        SpBytes word(unsigned addr) const
        {
            const size_t o = static_cast<size_t>(addr) * static_cast<size_t>(wb_);
            return SpBytes(mem_.begin() + static_cast<long>(o), mem_.begin() + static_cast<long>(o) + wb_);
        }
        void poke(unsigned addr, const SpBytes &d) { for (int i = 0; i < wb_ && i < static_cast<int>(d.size()); i++) mem_[static_cast<size_t>(addr) * static_cast<size_t>(wb_) + static_cast<size_t>(i)] = d[static_cast<size_t>(i)]; }

    private:
        int depth_, wb_;
        bool as_written_;
        std::vector<uint8_t> mem_;
        std::vector<SpBytes> rdata_;
        static bool bit(const SpBytes &v, int i) { return static_cast<size_t>(i / 8) < v.size() && ((v[static_cast<size_t>(i / 8)] >> (i % 8)) & 1); }
        void write(const Access &a)
        {
            uint8_t *w = &mem_[static_cast<size_t>(a.addr) * static_cast<size_t>(wb_)];
            if (!as_written_)
            {
                for (int i = 0; i < wb_; i++) if (bit(a.wmask, i)) w[i] = static_cast<size_t>(i) < a.wdata.size() ? a.wdata[static_cast<size_t>(i)] : 0;
                return;
            }
            // mask bit i writes bits [i, i + 8): bit k is written when one of the mask bits k - 7 .. k is set
            const int DW = 8 * wb_;
            for (int k = 0; k < wb_ + 7 && k < DW; k++)
            {
                bool en = false;
                for (int i = (k >= 7 ? k - 7 : 0); i <= k && i < wb_; i++) en = en || bit(a.wmask, i);
                if (!en) continue;
                const uint8_t m = static_cast<uint8_t>(1u << (k % 8));
                w[k / 8] = static_cast<uint8_t>((w[k / 8] & ~m) | (bit(a.wdata, k) ? m : 0));
            }
        }
    };

    // ---- gvu_scratchpad.sv ---------------------------------------------------------------------------------------------
    class GvuScratchpad
    {
    public:
        struct In
        {
            // sbank: port 0 int32 (word address, 4-bit byte mask), port 1 int8 (byte address, 1-bit mask)
            bool i_sbank_buf_sel{false};
            bool i_sbank_en_0{false}, i_sbank_wen_0{false}; unsigned i_sbank_addr_0{0}, i_sbank_wmask_0{0}; uint32_t i_sbank_wdata_0{0};
            bool i_sbank_en_1{false}, i_sbank_wen_1{false}; unsigned i_sbank_addr_1{0}; bool i_sbank_wmask_1{false}; uint8_t i_sbank_wdata_1{0};
            // vbank0: v32int8, one mask bit per lane
            bool i_vbank0_buf_sel{false}, i_vbank0_en{false}, i_vbank0_wen{false}; unsigned i_vbank0_addr{0}; SpBytes i_vbank0_wmask, i_vbank0_wdata;
            // vbank1: port 0 v32int32 (mask 4 bits per lane), port 1 v32int16 (2 bits per lane), port 2 v32int8 (1 bit per lane)
            bool i_vbank1_buf_sel{false}; unsigned i_vbank1_port_sel{0};
            bool i_vbank1_en_0{false}, i_vbank1_wen_0{false}; unsigned i_vbank1_addr_0{0}; SpBytes i_vbank1_wmask_0, i_vbank1_wdata_0;
            bool i_vbank1_en_1{false}, i_vbank1_wen_1{false}; unsigned i_vbank1_addr_1{0}; SpBytes i_vbank1_wmask_1, i_vbank1_wdata_1;
            bool i_vbank1_en_2{false}, i_vbank1_wen_2{false}; unsigned i_vbank1_addr_2{0}; SpBytes i_vbank1_wmask_2, i_vbank1_wdata_2;
            // vbank2: port 0 v32int32, port 1 v32int8
            bool i_vbank2_buf_sel{false}, i_vbank2_port_sel{false};
            bool i_vbank2_en_0{false}, i_vbank2_wen_0{false}; unsigned i_vbank2_addr_0{0}; SpBytes i_vbank2_wmask_0, i_vbank2_wdata_0;
            bool i_vbank2_en_1{false}, i_vbank2_wen_1{false}; unsigned i_vbank2_addr_1{0}; SpBytes i_vbank2_wmask_1, i_vbank2_wdata_1;
        };
        struct Out
        {
            uint32_t o_sbank_rdata_0; uint8_t o_sbank_rdata_1;
            SpBytes o_vbank0_rdata, o_vbank1_rdata_0, o_vbank1_rdata_1, o_vbank1_rdata_2, o_vbank2_rdata_0, o_vbank2_rdata_1;
        };

        explicit GvuScratchpad(const ScratchpadParams &p = ScratchpadParams())
            : P_(p), L_(p.NUM_LANE),
              sbank_{SpSram(p.SBANK_DEPTH, 4, 2, p.wmask_as_written), SpSram(p.SBANK_DEPTH, 4, 2, p.wmask_as_written)},
              vbank0_{SpSram(p.VBANK0_DEPTH, p.NUM_LANE, 1, p.wmask_as_written), SpSram(p.VBANK0_DEPTH, p.NUM_LANE, 1, p.wmask_as_written)},
              vbank1_{SpSram(p.VBANK1_DEPTH, 4 * p.NUM_LANE, 1, p.wmask_as_written), SpSram(p.VBANK1_DEPTH, 4 * p.NUM_LANE, 1, p.wmask_as_written)},
              vbank2_{SpSram(p.VBANK2_DEPTH, 4 * p.NUM_LANE, 1, p.wmask_as_written), SpSram(p.VBANK2_DEPTH, 4 * p.NUM_LANE, 1, p.wmask_as_written)}
        {
            acc_sbank_rdata_r_[0].assign(4, 0); acc_sbank_rdata_r_[1].assign(4, 0);
            acc_vbank0_rdata_r_.assign(static_cast<size_t>(L_), 0);
            acc_vbank1_mux_rdata_r_.assign(static_cast<size_t>(4 * L_), 0);
            acc_vbank2_mux_rdata_r_.assign(static_cast<size_t>(4 * L_), 0);
        }
        const ScratchpadParams &params() const { return P_; }
        // Clears the delayed part selects of the narrow read ports. Not a port of the RTL: has/rtl_ref_gvu_top.h calls it on
        // reset and at every change of the controller state.
        void reset_addr_delays() { sel_s1_[0] = sel_s1_[1] = sel_v11_[0] = sel_v11_[1] = sel_v12_[0] = sel_v12_[1] = sel_v21_[0] = sel_v21_[1] = 0; }
        // Memory of buffer `buf` of a bank (0, 1, 2 = vbank, 3 = sbank): direct access for test benches.
        SpSram &sram(int bank, int buf) { return (bank == 0 ? vbank0_ : bank == 1 ? vbank1_ : bank == 2 ? vbank2_ : sbank_)[static_cast<size_t>(buf)]; }

        // Outputs in a cycle with the inputs `i` (the part selects and, without a pipeline register, the buffer selects
        // are combinational).
        Out out(const In &i) const
        {
            Out o;
            const SpBytes s0 = P_.SBANK_PIPE ? acc_sbank_rdata_r_[0] : sbank_[sel(i.i_sbank_buf_sel)].o_rdata(0);
            const SpBytes s1 = P_.SBANK_PIPE ? acc_sbank_rdata_r_[1] : sbank_[sel(i.i_sbank_buf_sel)].o_rdata(1);
            o.o_sbank_rdata_0 = static_cast<uint32_t>(s0[0]) | (static_cast<uint32_t>(s0[1]) << 8) | (static_cast<uint32_t>(s0[2]) << 16) | (static_cast<uint32_t>(s0[3]) << 24);
            o.o_sbank_rdata_1 = s1[nsel(sel_s1_, P_.SBANK_PIPE, i.i_sbank_addr_1 & 3u)];
            o.o_vbank0_rdata = P_.VBANK0_PIPE ? acc_vbank0_rdata_r_ : vbank0_[sel(i.i_vbank0_buf_sel)].o_rdata(0);
            const SpBytes v1 = P_.VBANK1_PIPE ? acc_vbank1_mux_rdata_r_ : vbank1_[sel(i.i_vbank1_buf_sel)].o_rdata(0);
            o.o_vbank1_rdata_0 = v1;
            o.o_vbank1_rdata_1 = slice(v1, 2 * L_ * static_cast<int>(nsel(sel_v11_, P_.VBANK1_PIPE, i.i_vbank1_addr_1 & 1u)), 2 * L_);
            o.o_vbank1_rdata_2 = slice(v1, L_ * static_cast<int>(nsel(sel_v12_, P_.VBANK1_PIPE, i.i_vbank1_addr_2 & 3u)), L_);
            const SpBytes v2 = P_.VBANK2_PIPE ? acc_vbank2_mux_rdata_r_ : vbank2_[sel(i.i_vbank2_buf_sel)].o_rdata(0);
            o.o_vbank2_rdata_0 = v2;
            o.o_vbank2_rdata_1 = slice(v2, L_ * static_cast<int>(nsel(sel_v21_, P_.VBANK2_PIPE, i.i_vbank2_addr_1 & 3u)), L_);
            return o;
        }

        // One clock edge.
        void tick(const In &i)
        {
            // pipeline registers take the memory outputs of before the edge
            acc_sbank_rdata_r_[0] = sbank_[sel(i.i_sbank_buf_sel)].o_rdata(0);
            acc_sbank_rdata_r_[1] = sbank_[sel(i.i_sbank_buf_sel)].o_rdata(1);
            acc_vbank0_rdata_r_ = vbank0_[sel(i.i_vbank0_buf_sel)].o_rdata(0);
            acc_vbank1_mux_rdata_r_ = vbank1_[sel(i.i_vbank1_buf_sel)].o_rdata(0);
            acc_vbank2_mux_rdata_r_ = vbank2_[sel(i.i_vbank2_buf_sel)].o_rdata(0);
            // part selects of the narrow ports: shift registers as deep as the read latency
            sel_s1_[1] = sel_s1_[0]; sel_s1_[0] = i.i_sbank_addr_1 & 3u;
            sel_v11_[1] = sel_v11_[0]; sel_v11_[0] = i.i_vbank1_addr_1 & 1u;
            sel_v12_[1] = sel_v12_[0]; sel_v12_[0] = i.i_vbank1_addr_2 & 3u;
            sel_v21_[1] = sel_v21_[0]; sel_v21_[0] = i.i_vbank2_addr_1 & 3u;

            // sbank: port 1 addresses a byte of the word
            SpSram::Access s[2];
            s[0].en = i.i_sbank_en_0; s[0].wen = i.i_sbank_wen_0; s[0].addr = i.i_sbank_addr_0 & amask(P_.SBANK_AW());
            s[0].wmask = SpBytes{static_cast<uint8_t>(i.i_sbank_wmask_0 & 0xFu)};
            s[0].wdata = SpBytes{static_cast<uint8_t>(i.i_sbank_wdata_0), static_cast<uint8_t>(i.i_sbank_wdata_0 >> 8), static_cast<uint8_t>(i.i_sbank_wdata_0 >> 16), static_cast<uint8_t>(i.i_sbank_wdata_0 >> 24)};
            const unsigned sbank_word_addr_1 = i.i_sbank_addr_1 & 3u;
            s[1].en = i.i_sbank_en_1; s[1].wen = i.i_sbank_wen_1; s[1].addr = (i.i_sbank_addr_1 >> 2) & amask(P_.SBANK_AW());
            s[1].wmask = SpBytes{static_cast<uint8_t>((i.i_sbank_wmask_1 ? 1u : 0u) << sbank_word_addr_1)};
            s[1].wdata.assign(4, 0); s[1].wdata[sbank_word_addr_1] = i.i_sbank_wdata_1;
            SpSram::Access idle[2];
            sbank_[sel(i.i_sbank_buf_sel)].tick(s, 2);
            sbank_[1 - sel(i.i_sbank_buf_sel)].tick(idle, 2);            // DMA side: never driven

            SpSram::Access v0;
            v0.en = i.i_vbank0_en; v0.wen = i.i_vbank0_wen; v0.addr = i.i_vbank0_addr & amask(P_.VBANK0_AW()); v0.wmask = i.i_vbank0_wmask; v0.wdata = i.i_vbank0_wdata;
            vbank0_[sel(i.i_vbank0_buf_sel)].tick(&v0, 1);
            vbank0_[1 - sel(i.i_vbank0_buf_sel)].tick(idle, 1);

            SpSram::Access v1;
            if (i.i_vbank1_port_sel == 0)
            {
                v1.en = i.i_vbank1_en_0; v1.wen = i.i_vbank1_wen_0; v1.addr = i.i_vbank1_addr_0 & amask(P_.VBANK1_AW()); v1.wmask = i.i_vbank1_wmask_0; v1.wdata = i.i_vbank1_wdata_0;
            }
            else if (i.i_vbank1_port_sel == 1)
            {
                const int w = static_cast<int>(i.i_vbank1_addr_1 & 1u);
                v1.en = i.i_vbank1_en_1; v1.wen = i.i_vbank1_wen_1; v1.addr = (i.i_vbank1_addr_1 >> 1) & amask(P_.VBANK1_AW());
                v1.wmask = place(i.i_vbank1_wmask_1, (2 * L_ / 8) * w, 4 * L_ / 8); v1.wdata = place(i.i_vbank1_wdata_1, 2 * L_ * w, 4 * L_);
            }
            else
            {
                const int w = static_cast<int>(i.i_vbank1_addr_2 & 3u);
                v1.en = i.i_vbank1_en_2; v1.wen = i.i_vbank1_wen_2; v1.addr = (i.i_vbank1_addr_2 >> 2) & amask(P_.VBANK1_AW());
                v1.wmask = place(i.i_vbank1_wmask_2, (L_ / 8) * w, 4 * L_ / 8); v1.wdata = place(i.i_vbank1_wdata_2, L_ * w, 4 * L_);
            }
            vbank1_[sel(i.i_vbank1_buf_sel)].tick(&v1, 1);
            vbank1_[1 - sel(i.i_vbank1_buf_sel)].tick(idle, 1);

            SpSram::Access v2;
            if (!i.i_vbank2_port_sel)
            {
                v2.en = i.i_vbank2_en_0; v2.wen = i.i_vbank2_wen_0; v2.addr = i.i_vbank2_addr_0 & amask(P_.VBANK2_AW()); v2.wmask = i.i_vbank2_wmask_0; v2.wdata = i.i_vbank2_wdata_0;
            }
            else
            {
                const int w = static_cast<int>(i.i_vbank2_addr_1 & 3u);
                v2.en = i.i_vbank2_en_1; v2.wen = i.i_vbank2_wen_1; v2.addr = (i.i_vbank2_addr_1 >> 2) & amask(P_.VBANK2_AW());
                v2.wmask = place(i.i_vbank2_wmask_1, (L_ / 8) * w, 4 * L_ / 8); v2.wdata = place(i.i_vbank2_wdata_1, L_ * w, 4 * L_);
            }
            vbank2_[sel(i.i_vbank2_buf_sel)].tick(&v2, 1);
            vbank2_[1 - sel(i.i_vbank2_buf_sel)].tick(idle, 1);
        }

    private:
        ScratchpadParams P_;
        int L_;
        std::array<SpSram, 2> sbank_, vbank0_, vbank1_, vbank2_;
        std::array<SpBytes, 2> acc_sbank_rdata_r_;
        SpBytes acc_vbank0_rdata_r_, acc_vbank1_mux_rdata_r_, acc_vbank2_mux_rdata_r_;
        unsigned sel_s1_[2]{0, 0}, sel_v11_[2]{0, 0}, sel_v12_[2]{0, 0}, sel_v21_[2]{0, 0};
        // delayed part select: latency 2 with the pipeline register of the bank, 1 without
        unsigned nsel(const unsigned *q, bool pipe, unsigned now) const { return P_.narrow_sel_delayed ? q[pipe ? 1 : 0] : now; }
        static size_t sel(bool buf_sel) { return buf_sel ? 1u : 0u; }      // ~buf_sel: buffer 0 serves the accelerator
        static unsigned amask(int aw) { return (1u << aw) - 1u; }
        static SpBytes slice(const SpBytes &d, int off, int len)
        {
            SpBytes r(static_cast<size_t>(len), 0);
            for (int k = 0; k < len && off + k < static_cast<int>(d.size()); k++) r[static_cast<size_t>(k)] = d[static_cast<size_t>(off + k)];
            return r;
        }
        // d << (8 * off) on a field of `total` bytes
        static SpBytes place(const SpBytes &d, int off, int total)
        {
            SpBytes r(static_cast<size_t>(total), 0);
            for (int k = 0; k < static_cast<int>(d.size()) && off + k < total; k++) r[static_cast<size_t>(off + k)] = d[static_cast<size_t>(k)];
            return r;
        }
    };
} // namespace rtl_ref_gvu

#endif // RTL_REF_GVU_SCRATCHPAD_H
