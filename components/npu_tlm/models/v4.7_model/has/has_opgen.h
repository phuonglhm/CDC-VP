// has_opgen.h -- HAS_DMA_OPGEN=1: the tensor-operation register file of the ReDMA of neo_lite RTL revision 2 (redma_opgen_top.sv) in the host port.
//
// The CPU programs a descriptor (op, two base addresses, three loop counts, strides, pad / replication fields) into a register file inside the DMA
// block of the configuration window and starts it; the sequencer (redma_opgen.sv) turns it into a stream of segments (raddr, waddr, len, fill) that the
// engines of the DMA move one after the other. COPY covers slice, concat, reshape and transpose (three nested loops, a stride per loop), FILL writes a
// byte, PAD2D builds padded rows, UPSAMPLE repeats groups and rows. The data flow controller never writes this register file.
//
// Window of the model: 0x40000A00 .. 0x40000AFF. The offset inside the window is the offset inside the DMA block of neo_top (0x00..0x3F conf_regs,
// 0x80..0xC4 the tensor register file), so firmware for the FPGA uses cfg_base + 0x30000 + offset and firmware for the model uses 0x40000A00 + offset.
// Addresses in the descriptor are DRAM addresses (DMA_RADDR_OFFSET = DMA_WADDR_OFFSET = 0); an address at or above dram_base is a system bus address
// and has dram_base subtracted, any other value is taken as an offset into the DRAM vector.
//
// Model of the sequence, not of the clocks: START hands the descriptor to the engine when the data flow controller is idle (HasNpuTop::dfc), the engine
// is busy for the cost of the descriptor in core cycles, the bytes are moved at the end, then DONE (CTRL bit 2) rises. No interrupt (the hardware has
// none for this register file). What it does not model: AXI timing beyond the cost formula, DRAM latency and bandwidth (the memory is ideal, as in the
// bench of the hardware), the clock-domain crossings and the arbiter of the DMA configuration port, local memories as source or destination.
//
// Cost (system clocks of the DMA, measured on the RTL with an ideal memory): COPY segment 10 + max(bs, bd + (sm > dm)), FILL segment 5 + bd, with
// bs / bd the number of 16-byte words the source / destination range touches and sm / dm the addresses mod 16; a descriptor is the sum of its segments;
// every register write costs 3. The sum is converted to core cycles with HAS_OPGEN_CLK_RATIO (default 4.17: the system clock is faster than the core clock).
// HAS_OPGEN_FLOOR=1 keeps the descriptor at no less than one core cycle per 16-byte word moved (the data rate the other DMA paths of the model use).
// Env: HAS_DMA_OPGEN (0/1, default 0; acts with HAS_NEO_REV >= 2), HAS_OPGEN_CFG_WRITE (3), HAS_OPGEN_CLK_RATIO (4.17), HAS_OPGEN_FLOOR (0).
// Pure C++17, no SystemC.
#ifndef HAS_OPGEN_H
#define HAS_OPGEN_H

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <vector>
#include "driver/libneo_cfg.h"
#include "has/neo_rev.h"

namespace has
{
    namespace opgen
    {
        enum Op : uint32_t { COPY = 0, FILL = 1, PAD2D = 2, UPSAMPLE = 3 };

        struct Desc
        {
            uint32_t op{0}, raddr{0}, waddr{0}, n0{0}, n1{0}, n2{0}, grp{0}, row{0}, rs0{0}, ws0{0}, rs1{0}, ws1{0}, rs2{0}, ws2{0};
            uint32_t padl{0}, padr{0}, full{0}, pt{0}, pb{0}, rep0{0}, rep1{0}, fill{0};
        };
        struct Segment
        {
            bool fill{false};
            uint32_t raddr{0}, waddr{0}, len{0};
            uint8_t fill_val{0};
            bool last{false};
        };

        // Line-by-line port of redma_opgen.sv with LATCH_DESC = 0: address accumulators (no multiplier), counters of the widths of the RTL
        // (n0 / n1 / n2 16 bit, the row counter 17 bit, pad 8 bit, replication 4 bit), the segment of the cycle in cur(), the update in advance().
        class Seq
        {
        public:
            void begin(const Desc &d)
            {
                d_ = d;
                ra2_ = ra1_ = ra0_ = d.raddr;
                wa2_ = wa1_ = wa0_ = d.waddr;
                c0_ = c2_ = 0; c1_ = 0; ch_ = cv_ = 0;
                ph_ = d.padl != 0 ? 0 : 1;
                run_ = true;
            }
            bool running() const { return run_; }
            Segment cur() const
            {
                Segment s;
                s.raddr = ra1_; s.waddr = wa1_; s.len = d_.row;
                switch (d_.op & 3u)
                {
                case COPY: s.raddr = ra0_; s.waddr = wa0_; s.last = c2_last() && c1_last() && c0_last(); break;
                case FILL: s.fill = true; s.waddr = wa0_; s.last = c2_last() && c1_last() && c0_last(); break;
                case PAD2D:
                    if (row_is_pad()) { s.fill = true; s.waddr = wa1_; s.len = d_.full; }
                    else if (ph_ == 0) { s.fill = true; s.waddr = wa1_; s.len = d_.padl; }
                    else if (ph_ == 1) { s.raddr = ra1_; s.waddr = wa1_ + d_.padl; s.len = d_.row; }
                    else { s.fill = true; s.waddr = wa1_ + d_.padl + d_.row; s.len = d_.padr; }
                    s.last = c2_last() && or_last() && pad_row_done();
                    break;
                default:
                    if (d_.rep0 <= 1u) { s.raddr = ra1_; s.waddr = wa1_; s.len = d_.row; s.last = c2_last() && c1_last() && ch_last(); }
                    else { s.raddr = ra0_; s.waddr = wa0_; s.len = d_.grp; s.last = c2_last() && c1_last() && ch_last() && c0_last() && cv_last(); }
                }
                s.fill_val = static_cast<uint8_t>(d_.fill);
                return s;
            }
            // one segment accepted: update the loops
            void advance()
            {
                switch (d_.op & 3u)
                {
                case COPY:
                case FILL:
                    if (c0_last())
                    {
                        c0_ = 0;
                        if (c1_last())
                        {
                            c1_ = 0;
                            ra2_ += d_.rs2; wa2_ += d_.ws2;
                            ra1_ = ra2_; wa1_ = wa2_; ra0_ = ra2_; wa0_ = wa2_;
                            if (c2_last()) run_ = false; else c2_ = (c2_ + 1) & 0xFFFFu;
                        }
                        else
                        {
                            c1_ = (c1_ + 1) & 0x1FFFFu;
                            ra1_ += d_.rs1; wa1_ += d_.ws1;
                            ra0_ = ra1_; wa0_ = wa1_;
                        }
                    }
                    else { c0_ = (c0_ + 1) & 0xFFFFu; ra0_ += d_.rs0; wa0_ += d_.ws0; }
                    break;
                case PAD2D:
                    if (!pad_row_done()) ph_ = ph_ == 0 ? 1 : 2;
                    else
                    {
                        ph_ = has_l() ? 0 : 1;
                        wa1_ += d_.ws1;
                        if (!row_is_pad()) ra1_ += d_.rs1;
                        if (or_last())
                        {
                            c1_ = 0;
                            ra2_ += d_.rs2; wa2_ += d_.ws2;
                            ra1_ = ra2_; wa1_ = wa2_;
                            if (c2_last()) run_ = false; else c2_ = (c2_ + 1) & 0xFFFFu;
                        }
                        else c1_ = (c1_ + 1) & 0x1FFFFu;
                    }
                    break;
                default:
                    if (d_.rep0 <= 1u)
                    {
                        wa1_ += d_.ws1;
                        if (ch_last())
                        {
                            ch_ = 0;
                            ra1_ += d_.rs1;
                            if (c1_last())
                            {
                                c1_ = 0;
                                ra2_ += d_.rs2; wa2_ += d_.ws2;
                                ra1_ = ra2_; wa1_ = wa2_;
                                if (c2_last()) run_ = false; else c2_ = (c2_ + 1) & 0xFFFFu;
                            }
                            else c1_ = (c1_ + 1) & 0x1FFFFu;
                        }
                        else ch_ = (ch_ + 1) & 0xFu;
                    }
                    else
                    {
                        wa0_ += d_.grp;
                        if (cv_last())
                        {
                            cv_ = 0;
                            ra0_ += d_.grp;
                            if (c0_last())
                            {
                                c0_ = 0;
                                ra0_ = ra1_;
                                wa1_ += d_.ws1;
                                wa0_ = wa1_;
                                if (ch_last())
                                {
                                    ch_ = 0;
                                    ra1_ += d_.rs1;
                                    ra0_ = ra1_;
                                    if (c1_last())
                                    {
                                        c1_ = 0;
                                        ra2_ += d_.rs2; wa2_ += d_.ws2;
                                        ra1_ = ra2_; wa1_ = wa2_; ra0_ = ra2_; wa0_ = wa2_;
                                        if (c2_last()) run_ = false; else c2_ = (c2_ + 1) & 0xFFFFu;
                                    }
                                    else c1_ = (c1_ + 1) & 0x1FFFFu;
                                }
                                else ch_ = (ch_ + 1) & 0xFu;
                            }
                            else c0_ = (c0_ + 1) & 0xFFFFu;
                        }
                        else cv_ = (cv_ + 1) & 0xFu;
                    }
                }
            }

        private:
            Desc d_;
            uint32_t ra2_{0}, wa2_{0}, ra1_{0}, wa1_{0}, ra0_{0}, wa0_{0};
            uint32_t c0_{0}, c1_{0}, c2_{0}, ch_{0}, cv_{0}, ph_{0};
            bool run_{false};
            uint32_t pt_n1() const { return (d_.pt + (d_.n1 & 0xFFFFu)) & 0x1FFFFu; }
            uint32_t h_out() const { return (pt_n1() + d_.pb) & 0x1FFFFu; }
            bool row_is_pad() const { return c1_ < d_.pt || c1_ >= pt_n1(); }
            bool has_l() const { return d_.padl != 0; }
            bool has_r() const { return d_.padr != 0; }
            bool c2_last() const { return c2_ == ((d_.n2 - 1u) & 0xFFFFu); }
            bool c1_last() const { return c1_ == ((d_.n1 - 1u) & 0x1FFFFu); }
            bool c0_last() const { return c0_ == ((d_.n0 - 1u) & 0xFFFFu); }
            bool ch_last() const { return ch_ == ((d_.rep1 - 1u) & 0xFu); }
            bool cv_last() const { return cv_ == ((d_.rep0 - 1u) & 0xFu); }
            bool or_last() const { return c1_ == ((h_out() - 1u) & 0x1FFFFu); }
            bool pad_row_done() const { return row_is_pad() ? true : (ph_ == 2 || (ph_ == 1 && !has_r())); }
        };

        // Reason a descriptor is refused (the sequencer of the hardware would run away or emit an empty segment); nullptr = acceptable.
        inline const char *check(const Desc &d)
        {
            if (d.n2 == 0) return "n2 = 0";
            const uint32_t op = d.op & 3u;
            if (op == COPY || op == FILL)
                return d.n0 == 0 ? "n0 = 0" : d.n1 == 0 ? "n1 = 0" : d.row == 0 ? "ROW_BYTES = 0" : nullptr;
            if (op == PAD2D)
            {
                if (((d.pt + d.n1 + d.pb) & 0x1FFFFu) == 0) return "no output row";
                if (d.n1 != 0 && d.row == 0) return "ROW_BYTES = 0";
                if (d.pt + d.pb != 0 && d.full == 0) return "FULL_BYTES = 0 with pad rows";
                return nullptr;
            }
            if (d.n1 == 0) return "n1 = 0";
            if (d.rep0 == 0 || d.rep1 == 0) return "replication factor 0";
            if (d.rep0 == 1) return d.row == 0 ? "ROW_BYTES = 0" : nullptr;
            return d.n0 == 0 ? "n0 = 0 with rep0 > 1" : d.grp == 0 ? "GRP_BYTES = 0" : nullptr;
        }

        struct Params
        {
            uint64_t a_copy{10}, a_fill{5}, cfg_write{3}, max_seg{16777216};
            double ratio{4.17};
            bool floor_{false};
            static uint64_t env_u(const char *n, uint64_t d) { const char *e = std::getenv(n); return e ? static_cast<uint64_t>(std::atoll(e)) : d; }
            static Params from_env()
            {
                Params p;
                p.cfg_write = env_u("HAS_OPGEN_CFG_WRITE", p.cfg_write);
                if (const char *e = std::getenv("HAS_OPGEN_CLK_RATIO")) { const double v = std::atof(e); if (v > 0.0) p.ratio = v; }
                p.floor_ = env_u("HAS_OPGEN_FLOOR", 0) != 0;
                return p;
            }
            // system clocks one segment keeps the DMA busy (period to the next segment)
            uint64_t seg_sys(const Segment &s) const
            {
                const uint64_t sm = s.raddr % 16u, dm = s.waddr % 16u;
                const uint64_t bs = (sm + s.len + 15u) / 16u, bd = (dm + s.len + 15u) / 16u;
                return s.fill ? a_fill + bd : a_copy + std::max(bs, bd + (sm > dm ? 1u : 0u));
            }
        };

        struct Stats
        {
            uint64_t ops{0}, segments{0}, bytes{0}, sys_cycles{0}, core_cycles{0}, cfg_writes{0};
            uint64_t bad_desc{0}, out_of_range{0}, legacy_start{0}, bad_access{0}, overlap_start{0}, write_while_busy{0}, seg_overlap{0};
        };

        class Port
        {
        public:
            static constexpr uint32_t BASE = 0x40000A00u, SIZE = 0x100u;
            static_assert(neo::addr::OPGEN_BASE == 0x80u && neo::addr::OG_ID == 0x44u && neo::addr::CR_CONTROL == 0u, "window layout = block DMA of neo_top");
            static_assert(BASE + neo::addr::OPGEN_BASE + neo::addr::OG_ID == 0x40000AC4u, "the tensor register file starts at the window base + OPGEN_BASE");

            Port() : on_(env_on()), par_(Params::from_env()) {}
            explicit Port(bool on, const Params &p) : on_(on), par_(p) {}
            static bool env_on()
            {
                const char *e = std::getenv("HAS_DMA_OPGEN");
                return e && std::atoi(e) != 0 && neo_rev() >= NEO_REV_2;
            }
            bool on() const { return on_; }
            bool owns(uint32_t a) const { return on_ && a >= BASE && a < BASE + SIZE; }
            bool active() const { return run_; }
            const Stats &stats() const { return st_; }
            Params &params() { return par_; }
            void set_dram(std::vector<uint8_t> *d) { dram_ = d; }
            void set_dram_base(uint32_t b) { dram_base_ = b; }
            void on_segment(std::function<void(const Segment &)> f) { hook_ = std::move(f); }
            void soft_reset()
            {
                regs_ = Regs(); pend_n_ = 0; done_ = run_ = false; remaining_ = 0; cfg_since_start_ = 0;
                st_ = Stats();
            }

            // Host write at a model address in the window. dfc_busy: the data flow controller is executing an instruction (counted, not refused).
            void write(uint32_t a, uint32_t v, bool dfc_busy = false)
            {
                const uint32_t off = a - BASE;
                st_.cfg_writes++; cfg_since_start_++;
                if (off & neo::addr::OPGEN_BASE)
                {
                    const uint32_t r = (off >> neo::addr::OG_ADDR_LO) & 0x1Fu, o = r * 4u;
                    if (o == neo::addr::OG_CTRL)
                    {
                        if (v & 1u)
                        {
                            if (dfc_busy || run_) st_.overlap_start++;
                            if (pend_n_ < (run_ ? 1u : 2u)) pend_n_++;
                            done_ = false;   // the hardware holds one request while busy; the first of two back-to-back STARTs is taken at once
                        }
                        if (v & 2u) done_ = false;
                        return;
                    }
                    uint32_t *p = reg_ptr(o);
                    if (!p) return;   // ID and addresses past it: write ignored
                    if (run_ || pend_n_ > 0)
                    {
                        if (st_.write_while_busy++ == 0)
                            std::fprintf(stderr, "[opgen] descriptor register written while the engine is busy (the hardware would change the running operation; the model keeps the one it started)\n");
                    }
                    *p = v;
                    return;
                }
                // conf_regs: register file only; START is not executed
                const uint32_t o = off & 0x3Fu;
                if (o == neo::addr::CR_CONTROL)
                {
                    regs_.cr_wz = (v >> 8) & 1u;
                    if (v & 3u && st_.legacy_start++ == 0)
                        std::fprintf(stderr, "[opgen] one-run descriptor START (conf_regs) is not executed by the model\n");
                }
                else if (o == neo::addr::CR_INTR_EN) regs_.cr_ien = v & 3u;
                else if (o == neo::addr::CR_INTR_STATUS) regs_.cr_ist &= ~(v & 3u);
                else if (o == neo::addr::CR_RADDR) regs_.cr_r = v;
                else if (o == neo::addr::CR_WADDR) regs_.cr_w = v;
                else if (o == neo::addr::CR_BTT) regs_.cr_btt = v;
                else st_.bad_access++;
            }

            uint32_t read(uint32_t a)
            {
                const uint32_t off = a - BASE;
                if (off & neo::addr::OPGEN_BASE)
                {
                    const uint32_t o = ((off >> neo::addr::OG_ADDR_LO) & 0x1Fu) * 4u;
                    if (o == neo::addr::OG_CTRL) return (done_ ? 4u : 0u) | ((run_ || pend_n_ > 0) ? 2u : 0u);
                    if (o == neo::addr::OG_ID) return neo::addr::OG_ID_VALUE;
                    const uint32_t *p = reg_ptr(o);
                    return p ? *p : 0u;
                }
                const uint32_t o = off & 0x3Fu;
                if (o == neo::addr::CR_CONTROL) return (neo::addr::CR_ID << 24) | (regs_.cr_wz << 8);
                if (o == neo::addr::CR_INTR_EN) return regs_.cr_ien;
                if (o == neo::addr::CR_INTR_STATUS) return regs_.cr_ist;
                if (o == neo::addr::CR_RADDR) return regs_.cr_r;
                if (o == neo::addr::CR_WADDR) return regs_.cr_w;
                if (o == neo::addr::CR_BTT) return regs_.cr_btt;
                st_.bad_access++;
                return neo::addr::CR_BAD_READ;
            }

            // One core clock at which the data flow controller is idle. True = the engine used this clock.
            bool step()
            {
                if (!on_) return false;
                if (!run_ && pend_n_ > 0) begin_run();
                if (!run_) return false;
                if (remaining_ > 0) remaining_--;
                if (remaining_ == 0) finish();
                return true;
            }

        private:
            struct Regs
            {
                uint32_t op{0}, raddr{0}, waddr{0}, cnt01{0}, cnt2{0}, grpb{0}, rowb{0}, rs0{0}, ws0{0}, rs1{0}, ws1{0}, rs2{0}, ws2{0}, padl{0}, padr{0}, full{0};
                uint32_t cr_wz{0}, cr_ien{0}, cr_ist{0}, cr_r{0}, cr_w{0}, cr_btt{0};
            };
            bool on_{false};
            Params par_;
            Regs regs_;
            Stats st_;
            std::vector<uint8_t> *dram_{nullptr};
            uint32_t dram_base_{0};
            std::function<void(const Segment &)> hook_;
            unsigned pend_n_{0};
            bool done_{false}, run_{false};
            uint64_t remaining_{0}, cfg_since_start_{0};
            Desc cur_;

            uint32_t *reg_ptr(uint32_t o)
            {
                switch (o)
                {
                case neo::addr::OG_OP: return &regs_.op;
                case neo::addr::OG_RADDR: return &regs_.raddr;
                case neo::addr::OG_WADDR: return &regs_.waddr;
                case neo::addr::OG_CNT01: return &regs_.cnt01;
                case neo::addr::OG_CNT2: return &regs_.cnt2;
                case neo::addr::OG_GRPB: return &regs_.grpb;
                case neo::addr::OG_ROWB: return &regs_.rowb;
                case neo::addr::OG_RS0: return &regs_.rs0;
                case neo::addr::OG_WS0: return &regs_.ws0;
                case neo::addr::OG_RS1: return &regs_.rs1;
                case neo::addr::OG_WS1: return &regs_.ws1;
                case neo::addr::OG_RS2: return &regs_.rs2;
                case neo::addr::OG_WS2: return &regs_.ws2;
                case neo::addr::OG_PADL: return &regs_.padl;
                case neo::addr::OG_PADR: return &regs_.padr;
                case neo::addr::OG_FULL: return &regs_.full;
                default: return nullptr;
                }
            }
            Desc decode() const
            {
                Desc d;
                d.op = regs_.op & 3u;
                d.pt = (regs_.op >> 2) & 0xFFu; d.pb = (regs_.op >> 10) & 0xFFu;
                d.rep0 = (regs_.op >> 18) & 0xFu; d.rep1 = (regs_.op >> 22) & 0xFu; d.fill = regs_.op >> 24;
                d.raddr = regs_.raddr; d.waddr = regs_.waddr;
                d.n0 = regs_.cnt01 & 0xFFFFu; d.n1 = regs_.cnt01 >> 16; d.n2 = regs_.cnt2 & 0xFFFFu;
                d.grp = regs_.grpb; d.row = regs_.rowb;
                d.rs0 = regs_.rs0; d.ws0 = regs_.ws0; d.rs1 = regs_.rs1; d.ws1 = regs_.ws1; d.rs2 = regs_.rs2; d.ws2 = regs_.ws2;
                d.padl = regs_.padl; d.padr = regs_.padr; d.full = regs_.full;
                return d;
            }
            bool to_dram(uint32_t a, uint32_t len, uint64_t &off) const
            {
                off = (dram_base_ != 0 && a >= dram_base_) ? a - dram_base_ : a;
                return dram_ && off + len <= dram_->size();
            }
            void refuse(const char *why)
            {
                st_.bad_desc++;
                std::fprintf(stderr, "[opgen] descriptor refused: %s\n", why);
                if (pend_n_) pend_n_--;
                done_ = true;   // the sequencer never starts; DONE rises so firmware does not wait for ever
            }
            // first pass: validate every segment, add up the cost
            void begin_run()
            {
                cur_ = decode();
                if (const char *why = check(cur_)) { refuse(why); return; }
                Seq q; q.begin(cur_);
                uint64_t n = 0, sys = 0, words = 0;
                while (q.running())
                {
                    const Segment s = q.cur();
                    uint64_t o;
                    if (s.len == 0) { refuse("empty segment"); return; }
                    if (!to_dram(s.waddr, s.len, o) || (!s.fill && !to_dram(s.raddr, s.len, o)))
                    {
                        st_.out_of_range++; refuse("address outside the DRAM vector"); return;
                    }
                    if (++n > par_.max_seg) { refuse("too many segments"); return; }
                    sys += par_.seg_sys(s);
                    words += (s.waddr % 16u + s.len + 15u) / 16u;
                    q.advance();
                }
                sys += cfg_since_start_ * par_.cfg_write;
                uint64_t core = static_cast<uint64_t>(std::ceil(static_cast<double>(sys) / par_.ratio));
                if (par_.floor_) core = std::max(core, words);
                core = std::max<uint64_t>(core, 1);
                st_.ops++; st_.segments += n; st_.sys_cycles += sys; st_.core_cycles += core;
                cfg_since_start_ = 0;
                remaining_ = core; run_ = true;
                if (pend_n_) pend_n_--;
            }
            // second pass: move the bytes, segment after segment (a segment whose ranges overlap is undefined in the hardware: counted)
            void finish()
            {
                Seq q; q.begin(cur_);
                while (q.running())
                {
                    const Segment s = q.cur();
                    uint64_t wo, ro = 0;
                    to_dram(s.waddr, s.len, wo);
                    if (s.fill) std::memset(dram_->data() + wo, s.fill_val, s.len);
                    else
                    {
                        to_dram(s.raddr, s.len, ro);
                        if (ro != wo && ro < wo + s.len && wo < ro + s.len && st_.seg_overlap++ == 0)
                            std::fprintf(stderr, "[opgen] a segment reads and writes overlapping ranges: the hardware result depends on the timing; the model copies as memmove\n");
                        std::memmove(dram_->data() + wo, dram_->data() + ro, s.len);
                    }
                    st_.bytes += s.len;
                    if (hook_) hook_(s);
                    q.advance();
                }
                run_ = false; done_ = true;
            }
        };
    } // namespace opgen
} // namespace has

#endif // HAS_OPGEN_H
