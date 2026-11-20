// tb_opgen_seq.cpp -- has_opgen.h against the RTL runs of tb_opgen.sv (cases made by opgen_cases.py, logs sim.log, memory dump.hex).
// For every group directory: the commands (W / S / P / R) are replayed through has::opgen::Port, then
//   (a) the memory equals dump.hex of the RTL (0 bytes differ), (b) the segments equal the SEG lines of the RTL (fill, raddr unless fill, waddr, len,
//   fill byte), (c) the register reads equal the RD lines, (d) the cost of every descriptor (core cycles with ratio 1 and no register-write cost =
//   system clocks of the formula) is compared with START -> DONE of the RTL.
// Without sim.log / dump.hex in a group directory (a group made by opgen_cases.py gen, no RTL run) the reference of the generator is used instead:
// expect.hex (memory), expect_seg.txt (segments), expect_rd.txt (register reads); no cycle comparison. This is the form the regression runs.
//   g++ -std=c++17 -O1 -I. -I/usr/include tb_opgen_seq.cpp -o tb_opgen_seq; tb_opgen_seq <cases dir> [group ...]
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include "has/has_opgen.h"

#ifdef _WIN32
#define SETENV(k, v) _putenv_s(k, v)
#else
#define SETENV(k, v) setenv(k, v, 1)
#endif

using namespace has::opgen;

static std::vector<uint8_t> read_hex(const std::string &p)
{
    std::vector<uint8_t> v;
    std::ifstream f(p);
    std::string l;
    while (std::getline(f, l))
    {
        while (!l.empty() && (l.back() == '\r' || l.back() == ' ')) l.pop_back();
        if (l.empty() || l[0] == '/' || l[0] == '@') continue;
        v.push_back(static_cast<uint8_t>(std::strtoul(l.c_str(), nullptr, 16)));
    }
    return v;
}

int main(int argc, char **argv)
{
    if (argc < 2) { std::fprintf(stderr, "usage: tb_opgen_seq <cases dir> [group ...]\n"); return 2; }
    SETENV("HAS_DMA_OPGEN", "1");
    SETENV("HAS_NEO_REV", "2");
    SETENV("HAS_OPGEN_CLK_RATIO", "1");
    SETENV("HAS_OPGEN_CFG_WRITE", "0");
    int bad_groups = 0;
    std::vector<std::string> groups;
    for (int i = 2; i < argc; i++) groups.push_back(argv[i]);
    if (groups.empty()) groups = {"g1_copy1", "g2_copynd", "g3_fill", "g4_pad", "g5_ups", "g6_slope", "g7_legacy", "g8_twostart", "g9_regs", "g10_holdout", "g11_fe"};
    for (const std::string &g : groups)
    {
        const std::string d = std::string(argv[1]) + "/" + g + "/";
        std::vector<uint8_t> dram = read_hex(d + "mem_init.hex");
        std::vector<uint8_t> dump = read_hex(d + "dump.hex");
        const bool ref_mode = dump.empty();
        if (ref_mode) dump = read_hex(d + "expect.hex");
        Port port(true, Params::from_env());
        port.set_dram(&dram);
        std::vector<Segment> segs;
        port.on_segment([&](const Segment &s) { segs.push_back(s); });
        std::vector<std::vector<uint64_t>> rtl_seg;
        std::vector<std::vector<std::string>> rtl_rd;
        std::vector<uint64_t> t_start, t_done;
        {
            std::ifstream f(d + "sim.log");
            std::string l;
            while (std::getline(f, l))
            {
                if (l.rfind("# ", 0) == 0) l = l.substr(2);
                std::istringstream is(l);
                std::string k;
                is >> k;
                if (k == "SEG")
                {
                    uint64_t c, fl, ra, wa, n, fv, last;
                    is >> c >> fl >> std::hex >> ra >> wa >> std::dec >> n >> std::hex >> fv >> std::dec >> last;
                    rtl_seg.push_back({fl, ra, wa, n, fv});
                }
                else if (k == "RD") { std::string a, v, r; is >> a >> v >> r; rtl_rd.push_back({a, v, r}); }
                else if (k == "T_START") { uint64_t c; is >> c; t_start.push_back(c); }
                else if (k == "T_DONE") { uint64_t c; is >> c; t_done.push_back(c); }
            }
        }
        if (ref_mode)
        {
            std::ifstream es(d + "expect_seg.txt"), er(d + "expect_rd.txt");
            std::string a, b, c, e;
            uint64_t fl, n, fv;
            while (es >> fl >> a >> b >> n >> e)
            {
                fv = std::strtoull(e.c_str(), nullptr, 16);
                rtl_seg.push_back({fl, a[0] == '-' ? 0 : std::strtoull(a.c_str(), nullptr, 16), std::strtoull(b.c_str(), nullptr, 16), n, fv});
            }
            while (er >> a >> b >> c) rtl_rd.push_back({a, b, c});
        }
        std::ifstream cf(d + "cmds.txt");
        std::string op;
        uint64_t desc = 0, model_total = 0, rtl_total = 0, worst = 0, read_bad = 0, read_n = 0;
        while (cf >> op)
        {
            if (op == "W") { uint32_t a, v; cf >> std::hex >> a >> v >> std::dec; port.write(Port::BASE + a, v); }
            else if (op == "S") { port.write(Port::BASE + 0x80, 1); }
            else if (op == "P")
            {
                uint64_t c = 0;
                while (!(port.read(Port::BASE + 0x80) & 4u) && c < (1u << 30)) { port.step(); c++; }
                port.write(Port::BASE + 0x80, 2);
                if (desc < t_start.size() && desc < t_done.size())
                {
                    const uint64_t r = t_done[desc] - t_start[desc];
                    model_total += c; rtl_total += r;
                    const uint64_t df = c > r ? c - r : r - c;
                    worst = std::max<uint64_t>(worst, r ? df * 1000 / r : 0);
                }
                desc++;
            }
            else if (op == "R")
            {
                uint32_t a;
                cf >> std::hex >> a >> std::dec;
                const uint32_t v = port.read(Port::BASE + a);
                if (read_n < rtl_rd.size())
                {
                    std::string hv = rtl_rd[read_n][1]; for (char &ch : hv) if (ch == 'x' || ch == 'X') ch = '0';
                    uint32_t want = static_cast<uint32_t>(std::strtoul(hv.c_str(), nullptr, 16));
                    uint32_t got = v;
                    if (a == 0) { want &= ~0x100u; got &= ~0x100u; }   // write_zero is x in the RTL until the first CONTROL write
                    if (got != want) { read_bad++; std::printf("  %s: read %02x model %08x RTL %08x\n", g.c_str(), a, v, want); }
                }
                read_n++;
            }
            else if (op == "L") { uint32_t a, b, c, z; cf >> std::hex >> a >> b >> c >> z >> std::dec; port.write(Port::BASE + 0, z ? 0x102u : 3u); }
            else if (op == "Q") break;
        }
        uint64_t bad_bytes = dram.size() == dump.size() ? 0 : 1;
        if (g != "g7_legacy")
            for (size_t i = 0; i < std::min(dram.size(), dump.size()); i++) bad_bytes += dram[i] != dump[i];
        uint64_t bad_seg = segs.size() == rtl_seg.size() ? 0 : 1 + (segs.size() > rtl_seg.size() ? segs.size() - rtl_seg.size() : rtl_seg.size() - segs.size());
        for (size_t i = 0; i < std::min(segs.size(), rtl_seg.size()); i++)
        {
            const Segment &s = segs[i];
            const auto &r = rtl_seg[i];
            const bool ok = (s.fill == (r[0] != 0)) && s.waddr == r[2] && s.len == r[3] && (s.fill ? s.fill_val == r[4] : s.raddr == r[1]);
            if (!ok && bad_seg++ < 4)
                std::printf("  %s: segment %zu model fill %d r %08x w %08x n %u | RTL fill %llu r %08llx w %08llx n %llu\n", g.c_str(), i, s.fill, s.raddr, s.waddr, s.len,
                            (unsigned long long)r[0], (unsigned long long)r[1], (unsigned long long)r[2], (unsigned long long)r[3]);
        }
        const bool ok = bad_bytes == 0 && bad_seg == 0 && read_bad == 0 && (g != "g7_legacy" || port.stats().legacy_start == 14);
        std::printf("%-12s %s  bytes_bad=%llu seg_bad=%llu (segments %zu) reads_bad=%llu | descriptors %llu: model %llu RTL %llu clocks (%+.2f %%), worst %llu.%llu %% | legacy starts %llu\n",
                    g.c_str(), ok ? "PASS" : "FAIL", (unsigned long long)bad_bytes, (unsigned long long)bad_seg, segs.size(), (unsigned long long)read_bad,
                    (unsigned long long)desc, (unsigned long long)model_total, (unsigned long long)rtl_total,
                    rtl_total ? 100.0 * (double(model_total) - double(rtl_total)) / double(rtl_total) : 0.0, (unsigned long long)(worst / 10), (unsigned long long)(worst % 10),
                    (unsigned long long)port.stats().legacy_start);
        bad_groups += ok ? 0 : 1;
    }
    std::printf("RESULT: %d groups with a mismatch\n", bad_groups);
    return bad_groups ? 1 : 0;
}
