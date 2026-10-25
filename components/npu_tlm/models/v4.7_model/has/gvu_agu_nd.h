// gvu_agu_nd.h -- multi-dimensional address mode of the AGU of the vector load / store unit (AGU_ND, code 3 of agu_mode_t), as
// the MAS describes it for reshape / slice / split / concat ("only alters the method of generating read/write addresses ... 4D
// stride address mode, each dimension has its own stride and repetition factors"). The hardware RTL has FIXED / INCR / POOLING
// only (code 3 unused; vlsu_stride_t / vlsu_rep_t declared in gvu_pkg.sv and not used). This is a MODEL PROPOSAL for the
// hardware, written in the style of the RTL ports (eval / tick, pure C++17, no SystemC). Nothing in the instruction path uses it.
//
// One unit drives the address register of one Scratchpad port, in units of that port (rows of 32 lanes):
//   addr = base + i0*s0 + i1*s1 + i2*s2 + i3*s3,   0 <= ik < nk,   i0 innermost
// The counters step once per access of the port (rd | wr), innermost first; the offsets are accumulated (no multiplier), as the
// pooling offsets of gvu_vlsu.sv. The address output is the register, like the other modes: the first access of a program uses
// the base loaded by i_reset (i_agu_addr_reset of the controller) before it.
//   access, not the last one of the program : next address
//   access, last one (o_last)               : stop = 0 -> back to base (base input of that cycle); stop = 1 -> keeps the address
//   no access                               : idle_reload = 0 -> keeps the address and the counters (proposal: an idle cycle
//                                             inside a stream does not lose the position, see B18); idle_reload = 1 -> base,
//                                             counters 0 (what AGU_INCR does)
//   i_reset                                 : base, counters 0, whatever the access is
// INCR is the program n0 = 2^AW, s0 = 1 (idle_reload = 1); FIXED is n0 = n1 = n2 = n3 = 1; the pooling read address of vbank0 is
// n = (window rows, W tiles, OH, C), s = (num_w_tile, 1, h_move, c_move).
//
// Registers (proposal; GVU_REG_AW 4 -> 5, Reg 9..15 are left to the residual add registers):
//   Reg 16 AGU_ND_CTRL  [2:0] source port, [3] source on, [6:4] destination port, [7] destination on, [8] idle_reload, [9] stop
//                       ports: 0 sbank port 0, 1 sbank port 1, 2 vbank0, 3 vbank1, 4 vbank2
//   Reg 17..20 source descriptor, Reg 21..24 destination descriptor, each:
//     +0 [11:0] base, [21:12] n0 - 1, [31:22] n1 - 1
//     +1 [9:0] n2 - 1, [19:10] n3 - 1
//     +2 [15:0] s0, [31:16] s1   (two's complement)
//     +3 [15:0] s2, [31:16] s3
#ifndef GVU_AGU_ND_H
#define GVU_AGU_ND_H

#include <array>
#include <cstdint>

namespace rtl_ref_gvu
{
    constexpr int AGU_ND = 3;   // fourth code of agu_mode_t

    struct AguNdDesc
    {
        unsigned base{0};
        std::array<unsigned, 4> n{{1, 1, 1, 1}};   // repetitions, 1..1024
        std::array<int, 4> s{{1, 0, 0, 0}};        // strides in port rows, -32768..32767
        unsigned length() const { return n[0] * n[1] * n[2] * n[3]; }
        // Address of access k of the program on an aw-bit register (k counted from the base load, the program repeating as with
        // stop = 0): the digits of k in the radices n0..n3. The closed form of what GvuAguNd steps to; the model uses it where
        // the access count of a port is not one per request (the pooling write of vbank2 steps once per output row).
        unsigned addr_at(uint64_t k, int aw) const
        {
            k %= length();
            int64_t a = base;
            for (size_t j = 0; j < 4; j++) { a += static_cast<int64_t>(k % n[j]) * s[j]; k /= n[j]; }
            return static_cast<unsigned>(a) & (aw >= 32 ? 0xFFFFFFFFu : (1u << aw) - 1u);
        }
    };

    struct AguNdCtrl
    {
        enum Port : unsigned { SBANK_0 = 0, SBANK_1 = 1, VBANK0 = 2, VBANK1 = 3, VBANK2 = 4 };
        unsigned src_port{VBANK0}, dst_port{VBANK2};
        bool src_on{false}, dst_on{false}, idle_reload{false}, stop{false};
    };

    // Register layout of the proposal (Reg 16..24).
    struct AguNdRegs
    {
        static constexpr unsigned CTRL = 16, SRC = 17, DST = 21, COUNT = 9;
        static uint32_t ctrl(const AguNdCtrl &c)
        {
            return (c.src_port & 7u) | (c.src_on ? 1u << 3 : 0u) | ((c.dst_port & 7u) << 4) | (c.dst_on ? 1u << 7 : 0u) |
                   (c.idle_reload ? 1u << 8 : 0u) | (c.stop ? 1u << 9 : 0u);
        }
        static AguNdCtrl ctrl(uint32_t r)
        {
            AguNdCtrl c;
            c.src_port = r & 7u; c.src_on = (r >> 3) & 1u; c.dst_port = (r >> 4) & 7u; c.dst_on = (r >> 7) & 1u;
            c.idle_reload = (r >> 8) & 1u; c.stop = (r >> 9) & 1u;
            return c;
        }
        static std::array<uint32_t, 4> pack(const AguNdDesc &d)
        {
            auto s16 = [](int v) { return static_cast<uint32_t>(v) & 0xFFFFu; };
            return {{(d.base & 0xFFFu) | (((d.n[0] - 1u) & 0x3FFu) << 12) | (((d.n[1] - 1u) & 0x3FFu) << 22),
                     ((d.n[2] - 1u) & 0x3FFu) | (((d.n[3] - 1u) & 0x3FFu) << 10),
                     s16(d.s[0]) | (s16(d.s[1]) << 16),
                     s16(d.s[2]) | (s16(d.s[3]) << 16)}};
        }
        static AguNdDesc unpack(const std::array<uint32_t, 4> &r)
        {
            auto s16 = [](uint32_t v) { return static_cast<int>(static_cast<int16_t>(static_cast<uint16_t>(v & 0xFFFFu))); };
            AguNdDesc d;
            d.base = r[0] & 0xFFFu;
            d.n = {{((r[0] >> 12) & 0x3FFu) + 1u, ((r[0] >> 22) & 0x3FFu) + 1u, (r[1] & 0x3FFu) + 1u, ((r[1] >> 10) & 0x3FFu) + 1u}};
            d.s = {{s16(r[2]), s16(r[2] >> 16), s16(r[3]), s16(r[3] >> 16)}};
            return d;
        }
    };

    class GvuAguNd
    {
    public:
        struct In
        {
            bool i_req{false};     // access of the port in this cycle (rd | wr)
            bool i_reset{false};   // i_agu_addr_reset
            AguNdDesc d;
            bool idle_reload{false}, stop{false};
        };
        struct Out
        {
            unsigned o_addr{0};    // address of an access in this cycle
            bool o_last{false};    // this access is the last one of the program
        };
        explicit GvuAguNd(int aw = 10) : aw_(aw) {}
        int aw() const { return aw_; }
        Out eval(const In &i) const
        {
            Out o;
            o.o_addr = addr_r_;
            o.o_last = i.i_req && !i.i_reset && last(i.d);
            return o;
        }
        // Address register after the clock edge of this cycle (the value eval() gives in the next cycle).
        unsigned next_addr(const In &i) const { State s = st_; step(s, i); return s.addr; }
        void tick(const In &i) { step(st_, i); addr_r_ = st_.addr; }

    private:
        struct State
        {
            unsigned addr{0}, base{0};
            std::array<unsigned, 4> cnt{{0, 0, 0, 0}};
            std::array<int64_t, 4> off{{0, 0, 0, 0}};
        };
        int aw_;
        State st_;
        unsigned addr_r_{0};
        unsigned mask() const { return aw_ >= 32 ? 0xFFFFFFFFu : (1u << aw_) - 1u; }
        bool last(const AguNdDesc &d) const
        {
            for (int k = 0; k < 4; k++) if (st_.cnt[static_cast<size_t>(k)] + 1u < d.n[static_cast<size_t>(k)]) return false;
            return true;
        }
        void load(State &s, unsigned base) const
        {
            s.base = base;
            s.cnt = {{0, 0, 0, 0}};
            s.off = {{0, 0, 0, 0}};
            s.addr = base & mask();
        }
        void step(State &s, const In &i) const
        {
            if (i.i_reset) { load(s, i.d.base); return; }
            if (!i.i_req) { if (i.idle_reload) load(s, i.d.base); return; }
            for (size_t k = 0; k < 4; k++)
            {
                if (s.cnt[k] + 1u < i.d.n[k])
                {
                    s.cnt[k]++;
                    s.off[k] += i.d.s[k];
                    int64_t a = s.base;
                    for (size_t j = 0; j < 4; j++) a += s.off[j];
                    s.addr = static_cast<unsigned>(a) & mask();
                    return;
                }
                s.cnt[k] = 0;
                s.off[k] = 0;
            }
            // end of the program
            if (i.stop)
            {
                for (size_t k = 0; k < 4; k++) { s.cnt[k] = i.d.n[k] - 1u; s.off[k] = static_cast<int64_t>(i.d.n[k] - 1u) * i.d.s[k]; }
                return;
            }
            load(s, i.d.base);
        }
    };
} // namespace rtl_ref_gvu

#endif // GVU_AGU_ND_H
