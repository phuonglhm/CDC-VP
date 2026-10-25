// gvu_agu_nd_vlsu.h -- the vector load / store unit with the AGU_ND mode (has/gvu_agu_nd.h) WITHOUT changing the frozen port
// has/rtl_ref_gvu_vlsu.h. GvuVlsuNd holds one rtl_ref_gvu::GvuVlsu and one GvuAguNd per Scratchpad port register (sbank port 0,
// sbank port 1, vbank0, vbank1, vbank2, each on the width of that register). A port whose AGU mode input is AGU_ND (3) and that
// AGU_ND_CTRL binds to the source or destination descriptor is handed to the frozen unit as AGU_FIXED with the start address =
// the address the AGU_ND register takes at the clock edge of this cycle: in AGU_FIXED the frozen register loads its start
// address at every edge (access or not, and on i_agu_addr_reset), so its address output is the AGU_ND register, cycle for cycle.
// The rest of the unit (enables, write masks, port selects, pooling) is the frozen one. A port in mode 3 that no descriptor
// binds is passed through unchanged (the RTL as delivered: the register keeps its value).
// When a port reads with one descriptor and writes with the other in two states of one operation (vbank0 of the residual add:
// operand A read in RESIDUAL_ST1, result written in RESIDUAL_ST2), the controller picks the descriptor per state with In::sel;
// in the cycle of i_agu_addr_reset it gives the pick of the NEXT state, so the register loads the base of the new descriptor.
// GvuVlsuNdPort is the same unit behind the interface of GvuVlsu (eval / tick / reset_pooling on a GvuVlsu::In), with the
// AGU_ND configuration given by bind() once per cycle; while nothing is bound it calls the frozen unit directly.
#ifndef GVU_AGU_ND_VLSU_H
#define GVU_AGU_ND_VLSU_H

#include <array>
#include <cstdint>
#include "has/rtl_ref_gvu_vlsu.h"
#include "has/gvu_agu_nd.h"

namespace rtl_ref_gvu
{
    class GvuVlsuNd
    {
    public:
        struct In
        {
            GvuVlsu::In v;           // as for GvuVlsu; agu mode AGU_ND (3) selects the descriptor of the port
            AguNdCtrl ctrl;          // AGU_ND_CTRL
            AguNdDesc src, dst;      // AGU_SRC_0..3, AGU_DST_0..3
            std::array<uint8_t, 5> sel{{0, 0, 0, 0, 0}};   // per port: 0 as AGU_ND_CTRL binds it, 1 source, 2 destination
        };
        explicit GvuVlsuNd(const ScratchpadParams &p = ScratchpadParams())
            : vlsu_(p), nd_{{GvuAguNd(p.SBANK_AW()), GvuAguNd(p.SBANK_AW() + 2), GvuAguNd(p.VBANK0_AW()), GvuAguNd(p.VBANK1_AW() + 2),
                             GvuAguNd(p.VBANK2_AW() + 2)}}
        {
        }
        GvuVlsu::Out eval(const In &i) const { return vlsu_.eval(lower(i)); }
        void tick(const In &i)
        {
            const GvuVlsu::In v = lower(i);
            vlsu_.tick(v);
            for (unsigned p = 0; p < 5; p++)
            {
                const AguNdDesc *d = desc(i, p);
                if (d) nd_[p].tick(nd_in(i, p, *d));
            }
        }
        // AGU_ND unit of a port (0 sbank port 0, 1 sbank port 1, 2 vbank0, 3 vbank1, 4 vbank2) and its outputs in this cycle.
        const GvuAguNd &nd(unsigned port) const { return nd_[port]; }
        GvuVlsu &frozen() { return vlsu_; }
        const GvuVlsu &frozen() const { return vlsu_; }
        GvuAguNd::Out nd_out(const In &i, unsigned port) const
        {
            const AguNdDesc *d = desc(i, port);
            return d ? nd_[port].eval(nd_in(i, port, *d)) : GvuAguNd::Out{};
        }

    private:
        GvuVlsu vlsu_;
        std::array<GvuAguNd, 5> nd_;

        static int &mode(GvuVlsu::In &v, unsigned p)
        {
            return p == 0 ? v.i_sbank_agu_mode_0 : p == 1 ? v.i_sbank_agu_mode_1 : p == 2 ? v.i_vbank0_agu_mode : p == 3 ? v.i_vbank1_agu_mode : v.i_vbank2_agu_mode;
        }
        static int mode(const GvuVlsu::In &v, unsigned p)
        {
            return p == 0 ? v.i_sbank_agu_mode_0 : p == 1 ? v.i_sbank_agu_mode_1 : p == 2 ? v.i_vbank0_agu_mode : p == 3 ? v.i_vbank1_agu_mode : v.i_vbank2_agu_mode;
        }
        static unsigned &start(GvuVlsu::In &v, unsigned p)
        {
            return p == 0 ? v.i_sbank_start_addr_0 : p == 1 ? v.i_sbank_start_addr_1 : p == 2 ? v.i_vbank0_start_addr : p == 3 ? v.i_vbank1_start_addr : v.i_vbank2_start_addr;
        }
        static bool req(const GvuVlsu::In &v, unsigned p)
        {
            return p == 0 ? (v.i_sbank_rd_0 || v.i_sbank_wr_0) : p == 1 ? (v.i_sbank_rd_1 || v.i_sbank_wr_1) : p == 2 ? (v.i_vbank0_rd || v.i_vbank0_wr)
                 : p == 3 ? (v.i_vbank1_rd || v.i_vbank1_wr) : (v.i_vbank2_rd || v.i_vbank2_wr);
        }
        // Descriptor bound to port p when its mode is AGU_ND: the one sel names, else the source if both name the same port.
        static const AguNdDesc *desc(const In &i, unsigned p)
        {
            if ((mode(i.v, p) & 3) != AGU_ND) return nullptr;
            const bool s = i.ctrl.src_on && i.ctrl.src_port == p, d = i.ctrl.dst_on && i.ctrl.dst_port == p;
            if (i.sel[p] == 1) return s ? &i.src : nullptr;
            if (i.sel[p] == 2) return d ? &i.dst : nullptr;
            if (s) return &i.src;
            if (d) return &i.dst;
            return nullptr;
        }
        static GvuAguNd::In nd_in(const In &i, unsigned p, const AguNdDesc &d)
        {
            GvuAguNd::In n;
            n.i_req = req(i.v, p);
            n.i_reset = i.v.i_agu_addr_reset;
            n.d = d;
            n.idle_reload = i.ctrl.idle_reload;
            n.stop = i.ctrl.stop;
            return n;
        }
        GvuVlsu::In lower(const In &i) const
        {
            GvuVlsu::In v = i.v;
            for (unsigned p = 0; p < 5; p++)
            {
                const AguNdDesc *d = desc(i, p);
                if (!d) continue;
                mode(v, p) = AGU_FIXED;
                start(v, p) = nd_[p].next_addr(nd_in(i, p, *d));
            }
            return v;
        }
    };

    class GvuVlsuNdPort
    {
    public:
        explicit GvuVlsuNdPort(const ScratchpadParams &p = ScratchpadParams()) : u_(p) {}
        // AGU_ND configuration of this cycle (AGU_ND_CTRL, AGU_SRC_0..3, AGU_DST_0..3, the controller's pick per port).
        void bind(const AguNdCtrl &c, const AguNdDesc &src, const AguNdDesc &dst, const std::array<uint8_t, 5> &sel = {{0, 0, 0, 0, 0}})
        {
            cfg_.ctrl = c; cfg_.src = src; cfg_.dst = dst; cfg_.sel = sel;
            on_ = c.src_on || c.dst_on;
        }
        GvuVlsu::Out eval(const GvuVlsu::In &v) const
        {
            if (!on_) return u_.frozen().eval(v);
            GvuVlsuNd::In i = cfg_;
            i.v = v;
            return u_.eval(i);
        }
        void tick(const GvuVlsu::In &v)
        {
            if (!on_) { u_.frozen().tick(v); return; }
            GvuVlsuNd::In i = cfg_;
            i.v = v;
            u_.tick(i);
        }
        void reset_pooling() { u_.frozen().reset_pooling(); }
        GvuVlsu &frozen() { return u_.frozen(); }
        const GvuVlsu &frozen() const { return u_.frozen(); }
        const GvuVlsuNd &unit() const { return u_; }

    private:
        GvuVlsuNd u_;
        GvuVlsuNd::In cfg_;
        bool on_{false};
    };
} // namespace rtl_ref_gvu

#endif // GVU_AGU_ND_VLSU_H
