// has_host_neo.h -- LayerInstr <-> the configuration register file of neo_lite (NEO_REGISTER_FILE.md, neo_lite RTL revision 1).
//
// plan():   LayerInstr -> the 82 words the CPU writes (driver/libneo_cfg.h) + "ext" lines.
// decode(): the 82 words (+ ext lines) -> LayerInstr. decode_regs() reads the words ONLY; it does not call the encoder.
//
// What the hardware file cannot hold travels as ext lines, one per LayerInstr field whose value the words do not give back:
//   X <field> <value> <code>   the hardware cannot run this layer as given (code A-* = hardware fault / gap, B-* = the model goes beyond the hardware);
//                              a replay tool for the RTL / FPGA must REFUSE a trace that has an X line.
//   Y <field> <value> <code>   provenance only (code P-*): the words already carry the operation; a replay tool ignores the line.
// A layer is "as given on the hardware" when it has no X line and no A-* issue. Pure C++17, no SystemC.
#ifndef HAS_HOST_NEO_H
#define HAS_HOST_NEO_H

#include <algorithm>
#include <cstdint>
#include <numeric>
#include <set>
#include <sstream>
#include <string>
#include <vector>
#include "driver/libneo_cfg.h"
#include "has/has_mmio_compat.h"
#include "has/rtl_ref_gvu_residual_hw.h"

namespace has
{
    namespace host_neo
    {
        struct FieldDef
        {
            const char *name;
            int64_t (*get)(const LayerInstr &);
            void (*set)(LayerInstr &, int64_t);
        };
#define HN_F(NAME, PATH) {NAME, [](const LayerInstr &l) -> int64_t { return static_cast<int64_t>(l.PATH); }, \
                          [](LayerInstr &l, int64_t v) { l.PATH = static_cast<decltype(l.PATH)>(v); }},
        // Every instruction field except the provenance ones left out on purpose: host_seq, default_tiling, from_float_scale.
        inline const std::vector<FieldDef> &fields()
        {
            static const std::vector<FieldDef> f = {
                HN_F("opcode", opcode) HN_F("last_of_host", last_of_host) HN_F("in_addr", in_addr) HN_F("w_addr", w_addr) HN_F("out_addr", out_addr)
                HN_F("bias_addr", bias_addr) HN_F("in_c", in_c) HN_F("in_h", in_h) HN_F("in_w", in_w) HN_F("cin_t", cin_t)
                HN_F("geom.cout", geom.cout) HN_F("geom.oh", geom.oh) HN_F("geom.ow", geom.ow) HN_F("geom.kh", geom.kh) HN_F("geom.kw", geom.kw)
                HN_F("geom.sy", geom.sy) HN_F("geom.sx", geom.sx) HN_F("geom.dil", geom.dil) HN_F("geom.pad_t", geom.pad_t) HN_F("geom.pad_l", geom.pad_l)
                HN_F("geom.pad_b", geom.pad_b) HN_F("geom.pad_r", geom.pad_r) HN_F("geom.cout_t", geom.cout_t) HN_F("geom.h_t", geom.h_t)
                HN_F("geom.w_t", geom.w_t) HN_F("geom.sa_y", geom.sa_y) HN_F("geom.order", geom.order) HN_F("geom.y_used", geom.y_used)
                HN_F("geom.pad_tail", geom.pad_tail) HN_F("act", act) HN_F("lut_addr", lut_addr) HN_F("per_channel", per_channel)
                HN_F("scale_addr", scale_addr) HN_F("shift_addr", shift_addr) HN_F("bcast_S", bcast_S) HN_F("bcast_s", bcast_s) HN_F("zp_out", zp_out)
                HN_F("pad_val", pad_val) HN_F("flags", flags) HN_F("tmaj_c0", tmaj_c0) HN_F("mode", mode) HN_F("a_addr", a_addr) HN_F("b_addr", b_addr)
                HN_F("n", n) HN_F("c", c) HN_F("h", h) HN_F("w", w) HN_F("pool_k", pool_k) HN_F("pool_s", pool_s) HN_F("pool_p", pool_p)
                HN_F("pool_mode", pool_mode) HN_F("avg_scale", avg_scale) HN_F("avg_shift", avg_shift) HN_F("add.zpA", add.zpA) HN_F("add.zpB", add.zpB)
                HN_F("add.zpO", add.zpO) HN_F("add.SA", add.SA) HN_F("add.SB", add.SB) HN_F("add.SO", add.SO) HN_F("add.sA", add.sA) HN_F("add.sB", add.sB)
                HN_F("add.sO", add.sO) HN_F("v_addr", v_addr) HN_F("param_addr", param_addr) HN_F("mask_addr", mask_addr) HN_F("rows", rows)
                HN_F("seq_len", seq_len) HN_F("head_dim", head_dim) HN_F("v_stride", v_stride)};
            return f;
        }
#undef HN_F

        struct Ext { std::string field; int64_t value{0}; std::string code; bool provenance() const { return code.compare(0, 2, "P-") == 0; } };

        // Instruction content the comparison and the words agree on: tile sizes clamped to the tensor, Y_used as the tile iterator uses it.
        inline LayerInstr normalize(LayerInstr l)
        {
            l.host_seq = 0; l.default_tiling = false; l.from_float_scale = false;
            // MAX_POOL / AVG_POOL have no second operand: B_ADDR is a sticky register of the v4.5 host interface that keeps the value of an earlier instruction
            if (l.opcode == 0x15 && (l.mode == 1 || l.mode == 5)) l.b_addr = 0;
            if (l.opcode == 0x12)
            {
                LayerGeom &g = l.geom;
                // A tile larger than the tensor is the tensor, except for the channels of a PAD_TAIL layer (computed at the full cout_t).
                if (!g.pad_tail) g.cout_t = std::min(g.cout_t, g.cout);
                g.h_t = std::min(g.h_t, g.oh); g.w_t = std::min(g.w_t, g.ow);
                if (l.cin_t <= 0) l.cin_t = l.in_c;
                // Y_USED unset (0) and its effective value are one thing only when every tile has the full width; with a column edge the
                // tile iterator chooses Y_used per tile, so the written value stays as it is.
                if (g.ow % g.w_t == 0 && g.w_t > 0)
                {
                    const int wt = g.w_t;
                    if (g.pad_tail) g.y_used = (g.y_used > 0 && g.y_used <= g.sa_y) ? g.y_used : g.sa_y;
                    else g.y_used = (g.y_used > 0 && g.y_used <= g.sa_y && wt % g.y_used == 0) ? g.y_used : std::gcd(wt, g.sa_y);
                }
            }
            return l;
        }

        struct Plan
        {
            neo::Image img;
            std::vector<Ext> ext;
            std::vector<std::string> issues;   // sorted, unique: A-* / B-* codes (X lines and rule codes)
            std::string error;                 // non-empty: the layer could not be mapped at all
            bool as_given() const { return error.empty() && issues.empty(); }
            bool only_class_a() const
            {
                if (!error.empty() || issues.empty()) return false;
                for (const std::string &s : issues) if (s[0] != 'A') return false;
                return true;
            }
        };

        struct Decoded { LayerInstr li; std::vector<std::string> errors; int pool_oh_reg{-1}; };   // pool_oh_reg: OH word of a pooling layer, checked by decode() once ext is applied

        namespace detail
        {
            using neo::Image;
            inline uint32_t u(int64_t v) { return static_cast<uint32_t>(v); }
            inline int sext8(uint32_t v) { return static_cast<int8_t>(v & 0xFFu); }
            inline int cdiv(int a, int b) { return (a + b - 1) / b; }
            inline bool exact(uint64_t a, uint64_t b, uint64_t &q) { if (b == 0 || a % b) return false; q = a / b; return true; }
            // tb_neo_top.sv / gvu_pkg.sv capacity of one vbank0 buffer (rows of 32 lanes); an assumption of the mapper, not a register
            constexpr int VBANK0_ROWS = 800;

            inline sauria::CfgWidths widths()
            {
                const sauria::SauriaTarget &t = neo::target();
                sauria::CfgWidths w; w.idx_a = t.idx_a; w.idx_w = t.idx_w; w.idx_o = t.idx_o; w.X = t.X; w.Y = t.Y; return w;
            }

            inline bool add_map(const LayerInstr &l, rtl_ref_gvu::ResidualHwParams &p)
            {
                // round mode HALF_UP (has/gvu_quant.h default); zero point subtracted first
                return rtl_ref_gvu::ResidualHw::map(l.add.zpA, l.add.zpB, l.add.zpO, l.add.SA, l.add.SB, l.add.SO, l.add.sA, l.add.sB, l.add.sO, 1, true, &p);
            }

            inline std::string code_for(const std::string &f, const LayerInstr &o, bool add_ok)
            {
                const auto is = [&](std::initializer_list<const char *> l) { for (const char *s : l) if (f == s) return true; return false; };
                if (f == "last_of_host") return "P-SPLIT";
                if (o.opcode == 0x13) return "B-FA";
                if (o.opcode == 0x14) return "B-LN";
                if (o.opcode == 0x16 || (o.opcode == 0x15 && (o.mode == 6 || o.mode == 7))) return "B-DMA";
                if (o.opcode == 0x15)
                {
                    if (o.mode == 0)
                    {
                        if (f.compare(0, 4, "add.") == 0) return add_ok ? "P-ADDQ" : "B-ADDMAP";
                        if (f == "n") return "B-ADDTILE";
                        return "B-FIELD";
                    }
                    if (is({"avg_scale", "avg_shift"})) return "B-AVG";
                    if (f == "pool_k") return "B-POOLK";
                    if (is({"c", "h", "w", "pool_p"})) return o.pool_p > 0 ? "B-POOLPAD" : "B-POOLFIT";
                    return "B-POOLFIT";
                }
                if (is({"geom.pad_t", "geom.pad_l", "geom.pad_b", "geom.pad_r", "pad_val"})) return "B-PAD";
                if (is({"in_h", "in_w"})) return "B-INDIM";
                if (is({"geom.cout", "geom.oh", "geom.ow", "geom.cout_t", "geom.h_t", "geom.w_t", "geom.y_used"})) return "B-EDGE";
                if (is({"geom.sy", "geom.sx"})) return "B-STRIDE";
                if (f == "geom.pad_tail" || f == "flags") return "B-PADTAIL";
                if (f == "tmaj_c0") return "B-TMAJ";
                if (is({"shift_addr", "bias_addr"})) return "B-REQ3";
                if (is({"per_channel", "bcast_S", "bcast_s", "scale_addr"})) return "B-FSCALE";
                if (is({"act", "lut_addr"})) return "B-ACTLUT";
                if (f == "geom.order") return o.cin_t > 0 && o.cin_t < o.in_c ? "B-CINORD" : "B-HALO";
                return "B-FIELD";
            }
        } // namespace detail

        // ---------------- words -> LayerInstr (no encoder call) ----------------
        inline Decoded decode_regs(const neo::Image &im)
        {
            using namespace detail;
            Decoded d; LayerInstr &li = d.li; std::vector<std::string> &er = d.errors;
            const uint32_t op = im.reg[neo::OPCODE];
            const auto need = [&](bool ok, const char *what) { if (!ok) er.push_back(what); return ok; };
            if (op == neo::GEMM_FUSED)
            {
                li.opcode = 0x12;
                uint64_t q;
                const uint32_t xl = im.reg[neo::XY_LIM] & 0xFFFFu, yl = im.reg[neo::XY_LIM] >> 16, cl = im.reg[neo::CK_LIM] & 0xFFFFu, kl = im.reg[neo::CK_LIM] >> 16;
                LayerGeom &g = li.geom;
                g.w_t = static_cast<int>(im.reg[neo::P_XSTEP]);
                if (!need(g.w_t > 0, "gemm: x step 0")) return d;
                g.ow = static_cast<int>((xl + 1u) * im.reg[neo::P_XSTEP]);
                if (!need(exact(im.reg[neo::P_YSTEP], static_cast<uint64_t>(g.ow), q), "gemm: y step not a multiple of ow")) return d;
                g.h_t = static_cast<int>(q); g.oh = static_cast<int>((yl + 1u) * q);
                if (!need(exact(im.reg[neo::P_KSTEP], static_cast<uint64_t>(g.ow) * g.oh, q), "gemm: k step not a multiple of ow*oh")) return d;
                g.cout_t = static_cast<int>(q); g.cout = static_cast<int>((kl + 1u) * q);
                if (!need(exact(im.reg[neo::I_XSTEP], static_cast<uint64_t>(g.w_t), q), "gemm: ifmap x step")) return d;
                g.sx = g.sy = static_cast<int>(q);
                const uint32_t aw = im.reg[neo::D_IY_STEP];
                if (!need(aw > 0 && exact(im.reg[neo::D_IC_STEP], aw, q), "gemm: ifmap c step")) return d;
                const uint32_t ah = static_cast<uint32_t>(q);
                li.in_w = static_cast<int>(aw); li.in_h = static_cast<int>(ah);
                if (!need(exact(im.reg[neo::I_CSTEP], static_cast<uint64_t>(aw) * ah, q), "gemm: ifmap tile c step")) return d;
                li.cin_t = static_cast<int>(q); li.in_c = static_cast<int>((cl + 1u) * q);
                if (!need(exact(im.reg[neo::W_CSTEP], static_cast<uint64_t>(g.cout_t) * li.cin_t, q), "gemm: weight c step")) return d;
                const int kk = static_cast<int>(q);   // kw * kh
                std::vector<uint32_t> core(im.reg.begin() + neo::IDX_CORE, im.reg.begin() + neo::IDX_GVU);
                uint64_t f[sauria::F_CFG_COUNT];
                sauria::cfg_decode(core, 0, widths(), f);
                const uint64_t pat = f[sauria::F_CFG_DIL_PAT];
                int ones = 0, first = -1, second = -1;
                for (int i = 0; i < 64; i++) if ((pat >> (63 - i)) & 1ull) { ones++; if (first < 0) first = i; else if (second < 0) second = i; }
                if (!need(ones > 0 && kk % ones == 0, "gemm: dilation pattern")) return d;
                g.kw = ones; g.kh = kk / ones; g.dil = ones > 1 ? second - first : 1;
                if (!need(static_cast<int>(f[sauria::F_CFG_TIL_CYSTEP]) > 0, "gemm: Y_used 0")) return d;
                g.y_used = static_cast<int>(f[sauria::F_CFG_TIL_CYSTEP]);
                need(static_cast<int>(f[sauria::F_CFG_TIL_KSTEP]) == g.cout_t, "gemm: core X_used != tile channels");
                need(f[sauria::F_CFG_WLIM] == static_cast<uint64_t>(g.cout_t) * kk * li.cin_t, "gemm: core WLIM");
                need(f[sauria::F_CFG_INCNTLIM] + 1 == static_cast<uint64_t>(kk) * li.cin_t, "gemm: core INCNTLIM");
                const uint32_t lo = (im.reg[neo::CTRL_BITS] >> 16) & 3u;
                g.order = lo >= 2 ? 1 : 0;
                li.in_addr = im.reg[neo::START_A]; li.w_addr = im.reg[neo::START_B]; li.out_addr = im.reg[neo::START_C];
                li.scale_addr = im.reg[neo::START_SB]; li.per_channel = li.scale_addr != 0;
                const uint32_t gm = im.gvu(neo::G_GEMM);
                li.act = (gm & 1u) ? 0 : 2; li.zp_out = sext8(gm >> 8);
            }
            else if (op == neo::RESIDUAL)
            {
                li.opcode = 0x15; li.mode = 0;
                li.n = (im.reg[neo::G_TILE_LIM] + 1u) * im.reg[neo::V0_BTT];
                li.a_addr = im.reg[neo::START_V0]; li.b_addr = im.reg[neo::START_V1]; li.out_addr = im.reg[neo::START_V2];
                const uint32_t r0 = im.gvu(neo::G_RES0);
                need((r0 & 0xFFFFu) * 32u == im.reg[neo::V0_BTT], "residual: sequence length != tile rows");
                li.add.zpO = sext8(r0 >> 16); li.add.sO = static_cast<int>((r0 >> 24) & 0x3Fu);
                li.add.SA = im.gvu(neo::G_RES1); li.add.SB = im.gvu(neo::G_RES2); li.add.SO = 1;
            }
            else if (op == neo::MAXPOOL || op == neo::AVGPOOL)
            {
                li.opcode = 0x15; li.mode = op == neo::MAXPOOL ? 1 : 5;
                const uint32_t p0 = im.gvu(neo::G_POOL0), p1 = im.gvu(neo::G_POOL1);
                li.pool_k = static_cast<int>(p0 & 3u) + 2; li.pool_s = static_cast<int>((p0 >> 2) & 7u);
                const uint32_t num_w = (p0 >> 8) & 0xFFu, oh_r = (p0 >> 16) & 0xFFu, ct = p0 >> 24, c_move = (p1 >> 16) & 0x3FFu;
                li.w = static_cast<int>(im.reg[neo::V0_BTT]);
                if (need(num_w > 0, "pool: no W tiles")) li.h = static_cast<int>(c_move / num_w);
                li.c = static_cast<int>((im.reg[neo::G_TILE_LIM] + 1u) * ct);
                li.a_addr = im.reg[neo::START_V0]; li.out_addr = im.reg[neo::START_V2];
                d.pool_oh_reg = static_cast<int>(oh_r);
            }
            else if (op == neo::FUSED_ATTN)
            {
                li.opcode = 0x13; li.a_addr = im.reg[neo::START_V0]; li.b_addr = im.reg[neo::START_SB];
                li.v_addr = im.reg[neo::START_V1]; li.out_addr = im.reg[neo::START_V2];
            }
            else if (op == neo::LAYERNORM)
            {
                li.opcode = 0x14; li.in_addr = im.reg[neo::START_V0]; li.out_addr = im.reg[neo::START_V2];
            }
            else if (op == neo::NONE) li.opcode = 0x16;
            else er.push_back("opcode word has no meaning");
            return d;
        }

        // ---------------- LayerInstr -> words + ext ----------------
        inline Plan plan(const LayerInstr &orig)
        {
            using namespace detail;
            Plan p;
            const LayerInstr l = normalize(orig);
            neo::Image &im = p.img;
            bool add_ok = false;
            if (l.opcode == 0x12)
            {
                const LayerGeom &g = l.geom;
                if (g.cout <= 0 || g.oh <= 0 || g.ow <= 0 || g.cout_t <= 0 || g.h_t <= 0 || g.w_t <= 0 || l.cin_t <= 0 ||
                    g.kh <= 0 || g.kw <= 0 || g.sy <= 0 || g.sx <= 0 || g.dil <= 0 || g.sa_y <= 0) { p.error = "bad GEMM geometry"; return p; }
                const int nx = cdiv(g.ow, g.w_t), ny = cdiv(g.oh, g.h_t), nk = cdiv(g.cout, g.cout_t), nc = cdiv(l.in_c, l.cin_t);
                sauria::SauriaLayerDesc d{};
                d.B_w = g.kw; d.B_h = g.kh; d.d = g.dil; d.s = g.sx; d.c_til = l.cin_t; d.k_til = g.cout_t; d.h_til = g.h_t; d.w_til = g.w_t;
                d.X_used = g.cout_t; d.preload_en = nc > 1 ? 1 : 0;
                d.Y_used = TileIter(g).at(0).y_used;   // the first (full) tile: the hardware configures the core once per layer
                d.C_w = nx * g.w_t; d.C_h = ny * g.h_t; d.C_c = nk * g.cout_t; d.A_c = nc * l.cin_t;
                const uint32_t dram[3] = {l.in_addr, l.w_addr, l.out_addr};
                const int lo = nc > 1 ? 1 : (g.order == 1 ? 2 : 0);   // HW_RTL_ALIGNMENT.md, section 14.5: model order 0 / 1 = hardware loop_order 0 / 2; Cin split = 1
                im = neo::encode_gemm(d, dram, l.per_channel ? l.scale_addr : 0u, l.zp_out, l.act == 0, lo);
                p.issues.push_back("A-H1");
                if (l.per_channel) p.issues.push_back("A-H4");
                if (nc > 1) p.issues.push_back("B-CINORD");
            }
            else if (l.opcode == 0x15 && l.mode == 0)
            {
                if (l.n == 0) { p.error = "ADD of 0 elements"; return p; }
                const int rows = cdiv(static_cast<int>(l.n), 32);
                int tr = 1;
                for (int t = std::min(rows, VBANK0_ROWS); t >= 1; t--) if (rows % t == 0) { tr = t; break; }
                const uint32_t T = static_cast<uint32_t>(tr) * 32u;
                im.reg[neo::OPCODE] = neo::RESIDUAL; im.gvu(neo::G_OPCODE) = neo::GVU_RESIDUAL;
                im.reg[neo::G_TILE_LIM] = static_cast<uint32_t>(rows / tr - 1);
                im.reg[neo::V0_BTT] = im.reg[neo::V0_MOVE] = im.reg[neo::V1_BTT] = im.reg[neo::V1_MOVE] = im.reg[neo::V2_BTT] = im.reg[neo::V2_MOVE] = T;
                im.reg[neo::START_V0] = l.a_addr; im.reg[neo::START_V1] = l.b_addr; im.reg[neo::START_V2] = l.out_addr;
                rtl_ref_gvu::ResidualHwParams hp;
                add_ok = add_map(l, hp);
                if (add_ok)
                {
                    im.gvu(neo::G_RES0) = static_cast<uint32_t>(tr) | (static_cast<uint32_t>(hp.z) & 0xFFu) << 16 | (static_cast<uint32_t>(hp.sh) & 0x3Fu) << 24;
                    im.gvu(neo::G_RES1) = static_cast<uint32_t>(hp.sa); im.gvu(neo::G_RES2) = static_cast<uint32_t>(hp.sb);
                    im.gvu(neo::G_RES3) = static_cast<uint32_t>(static_cast<uint64_t>(hp.bias)); im.gvu(neo::G_RES4) = static_cast<uint32_t>(static_cast<uint64_t>(hp.bias) >> 32);
                }
                else
                {
                    im.gvu(neo::G_RES0) = static_cast<uint32_t>(tr) | (static_cast<uint32_t>(l.add.zpO) & 0xFFu) << 16 | (static_cast<uint32_t>(l.add.sO) & 0x3Fu) << 24;
                    im.gvu(neo::G_RES1) = l.add.SA; im.gvu(neo::G_RES2) = l.add.SB;
                }
            }
            else if (l.opcode == 0x15 && (l.mode == 1 || l.mode == 5))
            {
                const int hp = l.h + 2 * l.pool_p, wp = l.w + 2 * l.pool_p, k = l.pool_k, s = l.pool_s;
                if (l.c <= 0 || hp < k || wp < k || s <= 0 || k <= 0) { p.error = "bad pooling geometry"; return p; }
                const int nw = cdiv(wp, 32), oh = (hp - k) / s + 1, ow = (wp - k) / s + 1, now = cdiv(ow, 32);
                int ct = 1;
                for (int t = std::min(l.c, 255); t >= 1; t--) if (l.c % t == 0 && t * hp * nw <= VBANK0_ROWS) { ct = t; break; }
                im.reg[neo::OPCODE] = l.mode == 1 ? neo::MAXPOOL : neo::AVGPOOL; im.gvu(neo::G_OPCODE) = l.mode == 1 ? neo::GVU_MAXPOOL : neo::GVU_AVGPOOL;
                im.gvu(neo::G_POOL0) = static_cast<uint32_t>((k - 2) & 3) | (static_cast<uint32_t>(s) & 7u) << 2 | (static_cast<uint32_t>(nw) & 0xFFu) << 8 |
                                       (static_cast<uint32_t>(oh) & 0xFFu) << 16 | (static_cast<uint32_t>(ct) & 0xFFu) << 24;
                im.gvu(neo::G_POOL1) = (static_cast<uint32_t>(nw * s) & 0x3FFu) | (static_cast<uint32_t>(nw * hp) & 0x3FFu) << 16;
                im.reg[neo::G_TILE_LIM] = static_cast<uint32_t>(l.c / ct - 1);
                im.reg[neo::V0_BTT] = static_cast<uint32_t>(wp); im.reg[neo::V0_STRIDE] = static_cast<uint32_t>(nw * 32);
                im.reg[neo::V0_REP] = static_cast<uint32_t>(ct * hp - 1); im.reg[neo::V0_MOVE] = static_cast<uint32_t>(ct * hp * wp);
                im.reg[neo::V2_BTT] = static_cast<uint32_t>(ow); im.reg[neo::V2_STRIDE] = static_cast<uint32_t>(now * 32);
                im.reg[neo::V2_REP] = static_cast<uint32_t>(ct * oh - 1); im.reg[neo::V2_MOVE] = static_cast<uint32_t>(ct * oh * ow);
                im.reg[neo::START_V0] = l.a_addr; im.reg[neo::START_V2] = l.out_addr;
                if (l.mode == 5) p.issues.push_back("A-H3");
            }
            else if (l.opcode == 0x13)
            {
                im.reg[neo::OPCODE] = neo::FUSED_ATTN;
                im.reg[neo::START_V0] = l.a_addr; im.reg[neo::START_SB] = l.b_addr; im.reg[neo::START_V1] = l.v_addr; im.reg[neo::START_V2] = l.out_addr;
                p.issues.push_back("A-H3");
            }
            else if (l.opcode == 0x14)
            {
                im.reg[neo::OPCODE] = neo::LAYERNORM; im.reg[neo::START_V0] = l.in_addr; im.reg[neo::START_V2] = l.out_addr;
                p.issues.push_back("A-H3");
            }
            else im.reg[neo::OPCODE] = neo::NONE;

            // ext = every field the words do not give back
            const LayerInstr base = decode_regs(im).li;   // raw: Y_used is read from the words (normalize() would recompute it without pad_tail)
            for (const FieldDef &f : fields())
            {
                const int64_t want = f.get(l), got = f.get(base);
                if (want != got) p.ext.push_back(Ext{f.name, want, code_for(f.name, l, add_ok)});
            }
            for (const Ext &e : p.ext) if (!e.provenance()) p.issues.push_back(e.code);
            std::sort(p.issues.begin(), p.issues.end());
            p.issues.erase(std::unique(p.issues.begin(), p.issues.end()), p.issues.end());
            return p;
        }

        inline LayerInstr decode(const neo::Image &im, const std::vector<Ext> &ext, std::vector<std::string> *errors = nullptr)
        {
            Decoded d = decode_regs(im);
            for (const Ext &e : ext)
                for (const FieldDef &f : fields()) if (e.field == f.name) { f.set(d.li, e.value); break; }
            if (d.pool_oh_reg >= 0)
            {
                const int hp = d.li.h + 2 * d.li.pool_p;
                if (d.li.pool_s <= 0 || hp < d.li.pool_k || ((hp - d.li.pool_k) / d.li.pool_s + 1) % 256 != d.pool_oh_reg)
                    d.errors.push_back("pool: OH register != window arithmetic");
            }
            if (errors) *errors = d.errors;
            return normalize(d.li);
        }

        // text form of one ext line (the trace format of NEO_REGISTER_FILE.md)
        inline std::string ext_line(const Ext &e)
        {
            std::ostringstream o; o << (e.provenance() ? "Y " : "X ") << e.field << ' ' << e.value << ' ' << e.code; return o.str();
        }
        inline bool parse_ext_line(const std::string &s, Ext &e)
        {
            std::istringstream is(s); std::string tag;
            if (!(is >> tag >> e.field >> e.value >> e.code) || (tag != "X" && tag != "Y")) return false;
            return (tag == "Y") == e.provenance();
        }
    } // namespace host_neo
} // namespace has

#endif // HAS_HOST_NEO_H
