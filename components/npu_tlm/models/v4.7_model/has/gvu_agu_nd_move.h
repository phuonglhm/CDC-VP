// gvu_agu_nd_move.h -- MOVE operation of the vector unit (MODEL PROPOSAL: ELEM_WISE MOVE, gvu_opcode_t 4'd10): copies N rows
// of a Scratchpad port to another port, the source read with the AGU_ND source descriptor and the destination written with the
// destination descriptor (has/gvu_agu_nd.h). This is how slice / split / concat / permutation of row blocks run on the vector
// unit where an address alias cannot do it (inner axis, layout change, tensors that stay in the Scratchpad). Runs on the frozen
// ports has/rtl_ref_gvu_scratchpad.h and has/rtl_ref_gvu_vlsu.h through GvuVlsuNd; pure C++17, no SystemC.
// Schedule, as a controller state would drive it: one cycle with i_agu_addr_reset (both registers take their base), then one
// read request per cycle (an issue gate can insert idle cycles), and every row is written two cycles after its read (memory
// register + pipeline register of the bank). N rows without idle cycles take N + 3 cycles.
// Ports: rows of v32int8 (vbank0, vbank1 port 2, vbank2 port 1) or of v32int32 (vbank1 port 0, vbank2 port 0); source and
// destination have the same row width and lie in different banks (each bank has one physical port). All lanes are written.
#ifndef GVU_AGU_ND_MOVE_H
#define GVU_AGU_ND_MOVE_H

#include <cstdint>
#include <functional>
#include <stdexcept>
#include "has/rtl_ref_gvu_scratchpad.h"
#include "has/gvu_agu_nd_vlsu.h"

namespace rtl_ref_gvu
{
    enum MovePort : int { MV_V0_I8 = 0, MV_V1_I8, MV_V2_I8, MV_V1_I32, MV_V2_I32 };

    class GvuMove
    {
    public:
        struct Result { uint64_t cycles{0}, rows{0}, idle{0}; };
        GvuMove(GvuScratchpad &sp, const ScratchpadParams &p = ScratchpadParams()) : sp_(sp), P_(p), vlsu_(p) {}
        static unsigned agu_port(int mp) { return mp == MV_V0_I8 ? AguNdCtrl::VBANK0 : (mp == MV_V1_I8 || mp == MV_V1_I32) ? AguNdCtrl::VBANK1 : AguNdCtrl::VBANK2; }
        static bool wide(int mp) { return mp == MV_V1_I32 || mp == MV_V2_I32; }
        // issue(c): may the read of cycle c (counted from the first read cycle) go out; idle cycles keep the AGU_ND registers
        // unless idle_reload.
        Result run(int src, const AguNdDesc &ds, int dst, const AguNdDesc &dd, bool idle_reload = false,
                   const std::function<bool(uint64_t)> &issue = nullptr)
        {
            if (agu_port(src) == agu_port(dst) || wide(src) != wide(dst)) throw std::invalid_argument("GvuMove: ports");
            if (ds.length() != dd.length()) throw std::invalid_argument("GvuMove: lengths");
            const uint64_t N = ds.length();
            Result r;
            GvuVlsuNd::In in;
            in.ctrl.src_port = agu_port(src); in.ctrl.src_on = true;
            in.ctrl.dst_port = agu_port(dst); in.ctrl.dst_on = true;
            in.ctrl.idle_reload = idle_reload;
            in.src = ds; in.dst = dd;
            bool d0 = false, d1 = false;   // read issued one / two cycles ago
            uint64_t issued = 0, written = 0, c = 0;
            for (bool first = true; written < N; first = false, r.cycles++)
            {
                GvuVlsu::In &v = in.v;
                v = GvuVlsu::In{};
                setup(v, src); setup(v, dst);
                v.i_agu_addr_reset = first;
                const bool rd = !first && issued < N && (!issue || issue(c));
                if (!first && issued < N && !rd) r.idle++;
                if (!first) c++;
                set_req(v, src, rd, false);
                set_req(v, dst, false, d1);
                const GvuVlsu::Out o = vlsu_.eval(in);
                GvuScratchpad::In s = sp_in(o);
                const GvuScratchpad::Out so = sp_.out(s);
                if (d1) put_wdata(s, dst, rdata(so, src));
                sp_.tick(s);
                vlsu_.tick(in);
                if (rd) issued++;
                if (d1) written++;
                d1 = d0; d0 = rd;
            }
            r.rows = N;
            return r;
        }

    private:
        GvuScratchpad &sp_;
        ScratchpadParams P_;
        GvuVlsuNd vlsu_;

        static void setup(GvuVlsu::In &v, int mp)
        {
            const unsigned p = agu_port(mp);
            if (p == AguNdCtrl::VBANK0) { v.i_vbank0_agu_mode = AGU_ND; v.i_vbank0_lane_wmask = 0xFFFFFFFFu; }
            else if (p == AguNdCtrl::VBANK1) { v.i_vbank1_agu_mode = AGU_ND; v.i_vbank1_port_sel = wide(mp) ? 0u : 2u; v.i_vbank1_lane_wmask = 0xFFFFFFFFu; }
            else { v.i_vbank2_agu_mode = AGU_ND; v.i_vbank2_port_sel = !wide(mp); v.i_vbank2_lane_wmask = 0xFFFFFFFFu; }
        }
        static void set_req(GvuVlsu::In &v, int mp, bool rd, bool wr)
        {
            const unsigned p = agu_port(mp);
            if (p == AguNdCtrl::VBANK0) { v.i_vbank0_rd = v.i_vbank0_rd || rd; v.i_vbank0_wr = v.i_vbank0_wr || wr; }
            else if (p == AguNdCtrl::VBANK1) { v.i_vbank1_rd = v.i_vbank1_rd || rd; v.i_vbank1_wr = v.i_vbank1_wr || wr; }
            else { v.i_vbank2_rd = v.i_vbank2_rd || rd; v.i_vbank2_wr = v.i_vbank2_wr || wr; }
        }
        static SpBytes lanes(uint32_t m) { return SpBytes{static_cast<uint8_t>(m), static_cast<uint8_t>(m >> 8), static_cast<uint8_t>(m >> 16), static_cast<uint8_t>(m >> 24)}; }
        // Scratchpad inputs from the unit's outputs, as gvu_top.sv connects them (write data filled in later).
        static GvuScratchpad::In sp_in(const GvuVlsu::Out &o)
        {
            GvuScratchpad::In s;
            s.i_vbank0_en = o.o_vbank0_en; s.i_vbank0_wen = o.o_vbank0_wen; s.i_vbank0_addr = o.o_vbank0_addr; s.i_vbank0_wmask = lanes(o.o_vbank0_wmask);
            s.i_vbank1_port_sel = o.o_vbank1_port_sel;
            s.i_vbank1_en_0 = o.o_vbank1_en_0; s.i_vbank1_wen_0 = o.o_vbank1_wen_0; s.i_vbank1_addr_0 = o.o_vbank1_addr_0; s.i_vbank1_wmask_0 = o.o_vbank1_wmask_0;
            s.i_vbank1_en_1 = o.o_vbank1_en_1; s.i_vbank1_wen_1 = o.o_vbank1_wen_1; s.i_vbank1_addr_1 = o.o_vbank1_addr_1; s.i_vbank1_wmask_1 = o.o_vbank1_wmask_1;
            s.i_vbank1_en_2 = o.o_vbank1_en_2; s.i_vbank1_wen_2 = o.o_vbank1_wen_2; s.i_vbank1_addr_2 = o.o_vbank1_addr_2; s.i_vbank1_wmask_2 = lanes(o.o_vbank1_wmask_2);
            s.i_vbank2_port_sel = o.o_vbank2_port_sel;
            s.i_vbank2_en_0 = o.o_vbank2_en_0; s.i_vbank2_wen_0 = o.o_vbank2_wen_0; s.i_vbank2_addr_0 = o.o_vbank2_addr_0; s.i_vbank2_wmask_0 = o.o_vbank2_wmask_0;
            s.i_vbank2_en_1 = o.o_vbank2_en_1; s.i_vbank2_wen_1 = o.o_vbank2_wen_1; s.i_vbank2_addr_1 = o.o_vbank2_addr_1; s.i_vbank2_wmask_1 = lanes(o.o_vbank2_wmask_1);
            return s;
        }
        static SpBytes rdata(const GvuScratchpad::Out &so, int mp)
        {
            switch (mp)
            {
            case MV_V0_I8: return so.o_vbank0_rdata;
            case MV_V1_I8: return so.o_vbank1_rdata_2;
            case MV_V2_I8: return so.o_vbank2_rdata_1;
            case MV_V1_I32: return so.o_vbank1_rdata_0;
            default: return so.o_vbank2_rdata_0;
            }
        }
        static void put_wdata(GvuScratchpad::In &s, int mp, const SpBytes &d)
        {
            switch (mp)
            {
            case MV_V0_I8: s.i_vbank0_wdata = d; break;
            case MV_V1_I8: s.i_vbank1_wdata_2 = d; break;
            case MV_V2_I8: s.i_vbank2_wdata_1 = d; break;
            case MV_V1_I32: s.i_vbank1_wdata_0 = d; break;
            default: s.i_vbank2_wdata_0 = d; break;
            }
        }
    };
} // namespace rtl_ref_gvu

#endif // GVU_AGU_ND_MOVE_H
