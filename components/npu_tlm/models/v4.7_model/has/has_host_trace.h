// has_host_trace.h -- register trace of a program for neo_lite (NEO_REGISTER_FILE.md).
//
// A trace is the sequence of CPU writes that runs a program on neo_lite RTL revision 1 (82 configuration words + interrupt enable + START per instruction,
// at the addresses of the hardware map), plus the lines the model adds:
//   # neo_trace v1 neo_lite RTL revision 1 words=82 cfg_base=<hex> dram_base=<hex>
//   D in|golden|out <file>                 DRAM images of the program (names only)
//   H <host step as in mmio.txt>          a step the host CPU does between two instructions (for example upsample2x), kept in place
//   I <n> <kind> host_seq=<k> hw_runnable=<0|1> issues=<A-..;B-..|->
//   W <addr> <value>                       the 84 CPU writes of the instruction (hex)
//   X <field> <value> <code>               the hardware cannot run the instruction as given (code A-* / B-*): REFUSED by a replay for the hardware
//   Y <field> <value> <code>               provenance only (code P-*); Y legacy_<hex addr> <word> P-FLOAT keeps a v4.5 float register word
// The words of an instruction that has no X line are the trace that runs on neo_lite RTL revision 1 once the GEMM fault of the delivered RTL (COMPILER_GUIDE.md, support matrix) is fixed.
// Pure C++17, no SystemC.
#ifndef HAS_HOST_TRACE_H
#define HAS_HOST_TRACE_H

#include <cstdio>
#include <cstring>
#include <istream>
#include <map>
#include <ostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>
#include "has/has_host_neo.h"
#include "has/has_host_neo_port.h"

namespace has
{
    namespace host_trace
    {
        struct Config { uint32_t cfg_base{0x80000000u}, dram_base{0x40000000u}; };   // tb_neo_top.sv: NEO_OFFSET, DRAM_OFFSET

        inline bool is_addr_word(unsigned i)
        {
            return i == neo::START_A || i == neo::START_B || i == neo::START_C || i == neo::START_SB || i == neo::START_V0 || i == neo::START_V1 || i == neo::START_V2;
        }

        inline const char *kind_of(const LayerInstr &l)
        {
            switch (l.opcode)
            {
            case 0x12: return "GEMM_FUSED";
            case 0x13: return "FUSED_ATTN";
            case 0x14: return "LAYERNORM";
            case 0x15: return l.mode == 0 ? "ADD" : l.mode == 1 ? "MAX_POOL" : l.mode == 5 ? "AVG_POOL" : l.mode == 6 ? "SLICE" : l.mode == 7 ? "CONCAT" : "ELEM_OTHER";
            case 0x16: return "DMA_TENSOR";
            }
            return "OTHER";
        }

        // ---------------- writer ----------------
        inline void write_header(std::ostream &os, const Config &c)
        {
            char b[160];
            std::snprintf(b, sizeof b, "# neo_trace v1 neo_lite RTL revision 1 words=%u cfg_base=%x dram_base=%x\n", neo::N_REGS, c.cfg_base, c.dram_base);
            os << b;
        }

        // One instruction. `legacy` = v4.5 float register words kept as provenance (only for an instruction that took the float path).
        inline void write_item(std::ostream &os, const Config &c, int index, const LayerInstr &li, const host_neo::Plan &p, const std::map<uint32_t, uint32_t> &legacy,
                               const std::vector<std::string> &host = std::vector<std::string>())
        {
            for (const std::string &h : host) os << h << '\n';
            std::string iss;
            for (const std::string &s : p.issues) iss += (iss.empty() ? "" : ";") + s;
            char b[160];
            std::snprintf(b, sizeof b, "I %d %s host_seq=%u hw_runnable=%d issues=%s\n", index, kind_of(li), li.host_seq, p.as_given() ? 1 : 0, iss.empty() ? "-" : iss.c_str());
            os << b;
            neo::Image im = p.img;
            for (unsigned i = 0; i < neo::N_REGS; i++) if (is_addr_word(i)) im.reg[i] += c.dram_base;
            for (const auto &w : neo::write_sequence(im, c.cfg_base))
            {
                std::snprintf(b, sizeof b, "W %x %x\n", w.first, w.second);
                os << b;
            }
            for (const host_neo::Ext &e : p.ext) os << host_neo::ext_line(e) << '\n';
            for (const auto &kv : legacy)
            {
                std::snprintf(b, sizeof b, "Y legacy_%x %u P-FLOAT\n", kv.first, kv.second);
                os << b;
            }
        }

        // ---------------- reader ----------------
        struct Item
        {
            int index{0}, line{0};
            std::string kind, issues;
            uint32_t host_seq{0};
            bool hw_runnable{false}, saw_ien{false}, saw_start{false};
            int writes{0};
            neo::Image img;
            std::vector<host_neo::Ext> ext;          // X and Y lines of the model (not the legacy words)
            std::map<uint32_t, uint32_t> legacy;
            std::vector<std::string> host_before;    // "H ..." lines that stand before the instruction
            LayerInstr li;
            std::vector<std::string> errors;
        };
        struct Trace
        {
            Config cfg;
            std::vector<std::string> dram;           // "D ..." lines
            std::vector<std::string> host_tail;      // "H ..." lines after the last instruction
            std::vector<Item> items;
            std::string error;
        };

        inline bool read_trace(std::istream &is, Trace &t)
        {
            std::string line;
            int ln = 0;
            Item *cur = nullptr;
            std::vector<std::string> pending_h;
            const auto fail = [&](const std::string &m) { t.error = "line " + std::to_string(ln) + ": " + m; return false; };
            const auto finish = [&](Item &it) -> bool {
                if (it.writes != static_cast<int>(neo::N_REGS) + 2 || !it.saw_ien || !it.saw_start) { ln = it.line; return fail("instruction " + std::to_string(it.index) + " does not have the 84 writes"); }
                for (unsigned i = 0; i < neo::N_REGS; i++) if (is_addr_word(i)) it.img.reg[i] -= t.cfg.dram_base;
                it.li = host_neo::decode(it.img, it.ext, &it.errors);
                it.li.host_seq = it.host_seq;
                return true;
            };
            while (std::getline(is, line))
            {
                ln++;
                if (!line.empty() && line.back() == '\r') line.pop_back();
                if (line.empty()) continue;
                if (line[0] == '#')
                {
                    unsigned long cb, db;
                    if (const char *p = std::strstr(line.c_str(), "cfg_base=")) if (std::sscanf(p, "cfg_base=%lx dram_base=%lx", &cb, &db) == 2) { t.cfg.cfg_base = static_cast<uint32_t>(cb); t.cfg.dram_base = static_cast<uint32_t>(db); }
                    continue;
                }
                std::istringstream ls(line);
                std::string tag; ls >> tag;
                if (tag == "D") { t.dram.push_back(line); continue; }
                if (tag == "H") { pending_h.push_back(line); continue; }
                if (tag == "I")
                {
                    if (cur && !finish(*cur)) return false;
                    t.items.emplace_back();
                    cur = &t.items.back();
                    cur->host_before.swap(pending_h);
                    std::string a, b, c;
                    if (!(ls >> cur->index >> cur->kind >> a >> b >> c)) return fail("bad I line");
                    cur->line = ln;
                    cur->host_seq = static_cast<uint32_t>(std::strtoul(a.c_str() + (a.compare(0, 9, "host_seq=") == 0 ? 9 : 0), nullptr, 10));
                    cur->hw_runnable = b == "hw_runnable=1";
                    cur->issues = c.compare(0, 7, "issues=") == 0 ? c.substr(7) : c;
                    continue;
                }
                if (!cur) return fail("line before the first I line");
                if (tag == "W")
                {
                    uint32_t a, v;
                    if (!(ls >> std::hex >> a >> v)) return fail("bad W line");
                    if (a == t.cfg.cfg_base + neo::IEN) cur->saw_ien = true;
                    else if (a == t.cfg.cfg_base + neo::CTRL) cur->saw_start = true;
                    else if (!neo::apply_write(cur->img, t.cfg.cfg_base, a, v)) return fail("write outside the configuration register file");
                    cur->writes++;
                    continue;
                }
                if (tag == "X" || tag == "Y")
                {
                    host_neo::Ext e;
                    if (!host_neo::parse_ext_line(line, e)) return fail("bad " + tag + " line (a code must start with A-/B- for X and P- for Y)");
                    if (e.field.compare(0, 7, "legacy_") == 0) cur->legacy[static_cast<uint32_t>(std::strtoul(e.field.c_str() + 7, nullptr, 16))] = static_cast<uint32_t>(e.value);
                    else cur->ext.push_back(e);
                    continue;
                }
                return fail("unknown line tag '" + tag + "'");
            }
            if (cur && !finish(*cur)) return false;
            t.host_tail = pending_h;
            return true;
        }

        // First X line of the trace (the hardware cannot run that instruction as given); nullptr when there is none.
        inline const host_neo::Ext *first_x(const Trace &t, const Item **owner = nullptr)
        {
            for (const Item &it : t.items) for (const host_neo::Ext &e : it.ext) if (!e.provenance()) { if (owner) *owner = &it; return &e; }
            return nullptr;
        }

        // ---------------- the v4.5 host protocol from a trace ----------------
        typedef std::vector<std::pair<uint32_t, uint32_t>> Writes;

        // HAS_HOST=neo: an instruction as the CPU writes it for the neo window of HasNpuTop (has/has_host_neo_port.h).
        // win = the window in the address space the lines are for (model 0x40000800, or VP offset 0x10800 for the firmware of the CDC wrapper),
        // marker / bell = addresses of the staging flag and of PUSH_A, dram_add = base added to the words that hold DRAM addresses (0x80000000 for a wrapper).
        // only: a firmware list (VP offsets) carries no v4.5 lines, an instruction the hardware cannot run is left out with a comment.
        struct NeoEmit { uint32_t win{NeoPort::BASE}, marker{NeoPort::MARKER}, bell{mmio::PUSH_A}, dram_add{0}; bool only{false}; std::string force_kind; };   // force_kind: also send this kind of instruction through the window (to see it refused)
        inline void emit_neo_item(const Item &it, const NeoEmit &e, std::vector<std::string> &o)
        {
            const auto W = [&](uint32_t a, uint32_t v) { char b[40]; std::snprintf(b, sizeof b, "W %x %x", a, v); o.emplace_back(b); };
            W(e.marker, 0);
            for (unsigned i = 0; i < neo::N_REGS; i++) W(e.win + neo::reg_byte(i), it.img.reg[i] + (is_addr_word(i) ? e.dram_add : 0u));
            W(e.win + neo::IEN, 1); W(e.win + neo::CTRL, 1); W(e.bell, NeoPort::BELL);
        }

        // The lines of a v4.5 program (mmio.txt: "W <addr> <value>" register writes, "H ..." host steps) that give back exactly the instructions of the trace.
        // A GEMM with last_of_host = false and the ADD after it become ONE push with HAS_SKIP. False (and `err`) when something cannot be said in that protocol.
        inline bool emit_mmio(const Trace &t, std::vector<std::string> &o, std::string &err, const NeoEmit *neo = nullptr)
        {
            namespace M = has::mmio;
            const std::vector<Item> &items = t.items;
            const auto W = [&](uint32_t a, uint32_t v) { char b[40]; std::snprintf(b, sizeof b, "W %x %x", a, v); o.emplace_back(b); };
            const auto X = [&](int i, int64_t v) { W(M::EXT_BASE + 4u * static_cast<uint32_t>(i), static_cast<uint32_t>(v)); };
            for (size_t k = 0; k < items.size(); k++)
            {
                const Item &it = items[k];
                const LayerInstr &l = it.li;
                const LayerGeom &g = l.geom;
                for (const std::string &h : it.host_before) o.push_back(h);
                if (neo && (it.hw_runnable || (!neo->force_kind.empty() && it.kind == neo->force_kind))) { emit_neo_item(it, *neo, o); continue; }   // runs as given on the hardware: through the neo window
                if (neo && neo->only)
                {
                    o.push_back("# instruction " + std::to_string(it.index) + " (" + it.kind + ") is not run by neo_lite as given: " + it.issues);
                    if (l.opcode == 0x12 && !l.last_of_host) k++;
                    continue;
                }
                if (l.opcode == 0x12)
                {
                    if (g.sy != g.sx) { err = "instruction " + std::to_string(it.index) + ": stride X != Y has no place in the v4.5 registers"; return false; }
                    const bool skip = !l.last_of_host;
                    W(M::IN_ADDR, l.in_addr); W(M::W_ADDR, l.w_addr); W(M::OUT_ADDR, l.out_addr); W(M::BIAS_ADDR, l.bias_addr);
                    W(M::KH, uint32_t(g.kh)); W(M::KW, uint32_t(g.kw)); W(M::STRIDE, uint32_t(g.sx)); W(M::PAD, uint32_t(g.pad_t));
                    W(M::ACT_TYPE, uint32_t(l.act)); W(M::HAS_SKIP, skip ? 1u : 0u);
                    if (skip)
                    {
                        if (k + 1 >= items.size() || items[k + 1].li.opcode != 0x15 || items[k + 1].li.mode != 0) { err = "split GEMM without its ADD"; return false; }
                        W(M::SKIP_ADDR, items[k + 1].li.b_addr);
                    }
                    X(M::X_IN_C, l.in_c); X(M::X_IN_H, l.in_h); X(M::X_IN_W, l.in_w); X(M::X_OUT_C, g.cout); X(M::X_OUT_H, g.oh); X(M::X_OUT_W, g.ow);
                    X(M::X_TILE_COUT, g.cout_t); X(M::X_TILE_H, g.h_t); X(M::X_TILE_W, g.w_t);
                    X(M::X_PAD_T, g.pad_t); X(M::X_PAD_L, g.pad_l); X(M::X_PAD_B, g.pad_b); X(M::X_PAD_R, g.pad_r);
                    if (l.per_channel) { X(M::X_SCALE_ADDR, l.scale_addr); X(M::X_SHIFT_ADDR, l.shift_addr); }
                    else
                    {
                        if (it.legacy.empty()) { err = "instruction " + std::to_string(it.index) + ": broadcast scale without the v4.5 float words (Y legacy_*)"; return false; }
                        for (const auto &kv : it.legacy) W(kv.first, kv.second);
                    }
                    if (l.act >= 2 || l.lut_addr) X(M::X_LUT_ADDR, l.lut_addr);
                    X(M::X_ZP_OUT, l.zp_out); X(M::X_FLAGS, l.flags);
                    if (l.cin_t != l.in_c) X(M::X_TILE_CIN, l.cin_t);
                    // Y_USED is the value the core is configured with. The model's own default (register unset) is the gcd rule, so a value equal to it is not written:
                    // an unset register and an explicit one are the same configuration of the hardware but not the same to every part of the model.
                    if (g.y_used && !(!g.pad_tail && g.y_used == std::gcd(std::min(g.w_t, g.ow), g.sa_y))) X(M::X_Y_USED, g.y_used);
                    if (l.pad_val) X(M::X_PAD_VALUE, l.pad_val & 0xFF);
                    if (g.dil != 1) X(M::X_DILATION, g.dil);
                    if (l.tmaj_c0 >= 0) X(M::X_TMAJ_C0, l.tmaj_c0);
                    W(M::PUSH_A, 0x12);
                    if (skip) k++;   // the ADD of the pair is the second half of this push
                }
                else if (l.opcode == 0x15 && l.mode == 0)
                {
                    W(M::A_ADDR, l.a_addr); W(M::B_ADDR, l.b_addr); W(M::OUT_ADDR, l.out_addr); W(M::LEN, l.n); W(M::MODE_PACK, 0); W(M::A_LEN, l.n); W(M::B_LEN, l.n);
                    if (!it.legacy.empty()) for (const auto &kv : it.legacy) W(kv.first, kv.second);
                    else
                    {
                        const int64_t v[9] = {l.add.zpA, l.add.zpB, l.add.zpO, l.add.SA, l.add.sA, l.add.SB, l.add.sB, l.add.SO, l.add.sO};
                        for (int i = 0; i < 9; i++) X(M::X_ZP_A + i, v[i]);
                    }
                    W(M::PUSH_A, 0x15);
                }
                else if (l.opcode == 0x15 && (l.mode == 1 || l.mode == 5))
                {
                    W(M::A_ADDR, l.a_addr); W(M::OUT_ADDR, l.out_addr); W(M::MODE_PACK, uint32_t(l.mode)); W(M::STRIDE, uint32_t(l.pool_s));
                    X(M::X_IN_C, l.c); X(M::X_IN_H, l.h); X(M::X_IN_W, l.w); X(M::X_POOL_K, l.pool_k); X(M::X_POOL_P, l.pool_p);
                    if (l.mode == 1) X(M::X_POOL_MODE, l.pool_mode);
                    else { X(M::X_SO, l.avg_scale); X(M::X_SHO, l.avg_shift); }
                    W(M::PUSH_A, 0x15);
                }
                else if (l.opcode == 0x15 && (l.mode == 6 || l.mode == 7))
                {
                    W(M::A_ADDR, l.a_addr); W(M::B_ADDR, l.b_addr); W(M::OUT_ADDR, l.out_addr); W(M::MODE_PACK, uint32_t(l.mode)); W(M::STRIDE, uint32_t(l.pool_s));
                    if (l.mode == 7) { X(M::X_OUT_C, l.c); X(M::X_OUT_H, l.h); X(M::X_OUT_W, l.w); } else { X(M::X_IN_C, l.c); X(M::X_IN_H, l.h); X(M::X_IN_W, l.w); }
                    X(M::X_POOL_MODE, l.pool_mode); X(M::X_POOL_P, l.pool_p); X(M::X_POOL_K, l.pool_k);
                    W(M::PUSH_A, 0x15);
                }
                else if (l.opcode == 0x16)
                {
                    W(M::A_ADDR, l.a_addr); W(M::OUT_ADDR, l.out_addr); W(M::LEN, l.n); W(M::MODE_PACK, uint32_t(l.mode));
                    W(M::PUSH_A, 0x16);
                }
                else if (l.opcode == 0x13)
                {
                    W(M::OUT_ADDR, l.out_addr); W(M::A_ADDR, l.a_addr); W(M::B_ADDR, l.b_addr); W(M::V_ADDR, l.v_addr); W(M::LEN, uint32_t(l.seq_len)); W(M::R458, uint32_t(l.head_dim));
                    X(M::X_PARAM_ADDR, l.param_addr); X(M::X_ROWS, l.rows);
                    if (l.mask_addr) X(M::X_MASK_ADDR, l.mask_addr);
                    X(M::X_FLAGS, l.flags);
                    if (l.flags & 4u) X(M::X_IN_C, l.v_stride);
                    W(M::PUSH_A, 0x13);
                }
                else if (l.opcode == 0x14)
                {
                    W(M::OUT_ADDR, l.out_addr); W(M::IN_ADDR, l.in_addr); W(M::LEN, uint32_t(l.seq_len));
                    X(M::X_ROWS, l.rows); X(M::X_PARAM_ADDR, l.param_addr); X(M::X_FLAGS, l.flags);
                    W(M::PUSH_A, 0x14);
                }
                else { err = "instruction " + std::to_string(it.index) + ": opcode " + std::to_string(l.opcode) + " has no v4.5 form"; return false; }
            }
            for (const std::string &h : t.host_tail) o.push_back(h);
            return true;
        }

        // The "W" lines of a program as register writes.
        inline Writes writes_of(const std::vector<std::string> &lines)
        {
            Writes w;
            for (const std::string &l : lines)
            {
                std::istringstream is(l);
                std::string tag; uint32_t a, v;
                if ((is >> tag) && tag == "W" && (is >> std::hex >> a >> v)) w.emplace_back(a, v);
            }
            return w;
        }

        // Host steps of a program with the number of instructions pushed before each ("H ..." lines of mmio.txt).
        inline std::vector<std::pair<int, std::string>> host_steps_of(const std::vector<std::string> &lines)
        {
            std::vector<std::pair<int, std::string>> h;
            int pushes = 0;
            for (const std::string &l : lines)
            {
                if (l.compare(0, 2, "H ") == 0) h.emplace_back(pushes, l);
                else if (l.compare(0, 11, "W 40000310 ") == 0) pushes++;
            }
            return h;
        }

        // The instructions a v4.5 register-write stream pushes (the DFC side of has::MmioCompat), in order.
        inline std::vector<LayerInstr> replay_writes(const Writes &w, long *mmio_errors = nullptr)
        {
            MmioCompat m(1 << 20);
            std::vector<LayerInstr> out;
            long errs = 0;
            for (const auto &x : w)
            {
                m.write(x.first, x.second);
                LayerInstr li;
                while (m.pop(li)) { out.push_back(li); m.retire(li); }
                if (m.last_error() != Err::NONE) { errs++; m.write(mmio::STATUS, 0); }
            }
            if (mmio_errors) *mmio_errors = errs;
            return out;
        }

        // Same instruction streams (every field after host_neo::normalize; the tile grids of GEMM layers). Returns the number of differences; msg = first one.
        inline long compare_streams(const std::vector<LayerInstr> &a, const std::vector<LayerInstr> &b, std::string &msg)
        {
            long bad = 0;
            if (a.size() != b.size()) { msg = "instruction counts differ: " + std::to_string(a.size()) + " vs " + std::to_string(b.size()); return 1; }
            for (size_t i = 0; i < a.size(); i++)
            {
                const LayerInstr x = host_neo::normalize(a[i]), y = host_neo::normalize(b[i]);
                for (const host_neo::FieldDef &f : host_neo::fields())
                    if (f.get(x) != f.get(y)) { if (!bad++) msg = "instruction " + std::to_string(i) + " field " + f.name + ": " + std::to_string(f.get(x)) + " vs " + std::to_string(f.get(y)); }
                if (x.opcode == 0x12)
                {
                    TileIter p(x.geom), q(y.geom);
                    bool same = p.count() == q.count();
                    for (long t = 0; same && t < p.count(); t++) { const TileGeom u = p.at(t), v = q.at(t); same = std::memcmp(&u, &v, sizeof u) == 0; }
                    if (!same && !bad++) msg = "instruction " + std::to_string(i) + ": tile grids differ";
                }
            }
            return bad;
        }
    } // namespace host_trace
} // namespace has

#endif // HAS_HOST_TRACE_H
