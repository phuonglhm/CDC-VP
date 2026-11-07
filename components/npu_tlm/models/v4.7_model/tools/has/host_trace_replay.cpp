// host_trace_replay.cpp -- reads a register trace of neo_lite (has/has_host_trace.h).
//
//   host_trace_replay [--hw] [--emit-mmio <file>] [--compare <program dir>] <trace>
//
//   --hw            replay for the RTL / FPGA: REFUSES (exit 3) a trace that has an X line, naming the first one with its reason code. Without
//                   X lines the 84 writes of each instruction are what the hardware runs (after the fix of the GEMM fault A-H1, COMPILER_GUIDE.md, support matrix).
//   (no --hw)       model mode: decodes every instruction back (words + X / Y lines). Exit 0 when the words are consistent.
//   --compare dir   also replays dir/mmio.txt through has::MmioCompat and compares the two instruction streams field by field and tile by tile,
//                   and the host steps ("H" lines) with the place of each one.
//   --emit-mmio f   writes the trace as a v4.5 program (mmio.txt: register writes and host steps) and checks it: replayed through has::MmioCompat it
//                   gives back the same instructions. tb_has_npu_top runs that program.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include "has/has_host_trace.h"

using namespace has;

static std::vector<std::string> read_lines(const std::string &path, bool &ok)
{
    std::vector<std::string> v;
    std::ifstream f(path);
    ok = bool(f);
    std::string line;
    while (std::getline(f, line)) { if (!line.empty() && line.back() == '\r') line.pop_back(); v.push_back(line); }
    return v;
}

int main(int argc, char **argv)
{
    bool hw = false;
    std::string trace_path, emit_path, cmp_dir, neo_path, fw_path;
    uint32_t neo_dram_add = 0;
    std::string neo_force;
    for (int i = 1; i < argc; i++)
    {
        if (!std::strcmp(argv[i], "--hw")) hw = true;
        else if (!std::strcmp(argv[i], "--emit-mmio") && i + 1 < argc) emit_path = argv[++i];
        else if (!std::strcmp(argv[i], "--compare") && i + 1 < argc) cmp_dir = argv[++i];
        else if (!std::strcmp(argv[i], "--emit-neo-mmio") && i + 1 < argc) neo_path = argv[++i];
        else if (!std::strcmp(argv[i], "--emit-wrapper-fw") && i + 1 < argc) fw_path = argv[++i];
        else if (!std::strcmp(argv[i], "--neo-force") && i + 1 < argc) neo_force = argv[++i];
        else if (!std::strcmp(argv[i], "--neo-dram-add") && i + 1 < argc) neo_dram_add = static_cast<uint32_t>(std::strtoul(argv[++i], nullptr, 16));
        else trace_path = argv[i];
    }
    if (trace_path.empty()) { std::fprintf(stderr, "usage: %s [--hw] [--emit-mmio f] [--compare dir] <trace>\n", argv[0]); return 2; }
    std::ifstream tf(trace_path);
    if (!tf) { std::fprintf(stderr, "cannot read %s\n", trace_path.c_str()); return 2; }
    host_trace::Trace t;
    if (!host_trace::read_trace(tf, t)) { std::printf("[host_trace_replay] %s: %s\n", trace_path.c_str(), t.error.c_str()); return 2; }

    long a_only = 0, with_x = 0, dec_err = 0;
    for (const host_trace::Item &it : t.items)
    {
        dec_err += static_cast<long>(it.errors.size());
        bool x = false;
        for (const host_neo::Ext &e : it.ext) x = x || !e.provenance();
        with_x += x ? 1 : 0;
        if (!x && it.issues.find("A-") != std::string::npos) a_only++;
    }
    if (hw)
    {
        const host_trace::Item *owner = nullptr;
        if (const host_neo::Ext *e = host_trace::first_x(t, &owner))
        {
            std::printf("[host_trace_replay] REFUSED: %s: instruction %d (%s) needs more than the hardware has: X %s %lld %s "
                        "(%ld of %zu instructions carry X lines)\n", trace_path.c_str(), owner->index, owner->kind.c_str(), e->field.c_str(), (long long)e->value, e->code.c_str(),
                        with_x, t.items.size());
            return 3;
        }
        std::printf("[host_trace_replay] ACCEPTED for the hardware: %s: %zu instructions, no X line; %ld of them need the hardware fixes of A-* issues "
                    "(COMPILER_GUIDE.md, support matrix: A-H1)\n", trace_path.c_str(), t.items.size(), a_only);
        return 0;
    }

    bool pass = dec_err == 0;
    long mmio_err = 0;
    std::vector<LayerInstr> got;
    for (const host_trace::Item &it : t.items) got.push_back(it.li);
    std::printf("[host_trace_replay] %s: %zu instructions, %ld with X lines, %ld decode inconsistencies\n", trace_path.c_str(), t.items.size(), with_x, dec_err);

    std::vector<std::string> lines;   // the trace as a v4.5 program
    if (!cmp_dir.empty() || !emit_path.empty())
    {
        std::string err;
        if (!host_trace::emit_mmio(t, lines, err)) { std::printf("[host_trace_replay] cannot write the v4.5 program: %s\n", err.c_str()); return 1; }
    }
    // HAS_HOST=neo programs: --emit-neo-mmio = the instructions the hardware runs as given go through the neo window, the others stay v4.5 (a mixed program for
    // tb_has_neo_host); --emit-wrapper-fw = firmware list for the CDC wrapper (VP offsets, system RAM addresses), the other instructions left out with a comment.
    for (int pass_i = 0; pass_i < 2; pass_i++)
    {
        const std::string &path = pass_i == 0 ? neo_path : fw_path;
        if (path.empty()) continue;
        host_trace::NeoEmit ne;
        if (pass_i == 1) { ne.win = 0x10800u; ne.marker = 0x104E4u; ne.bell = 0x10310u; ne.dram_add = neo_dram_add ? neo_dram_add : 0x80000000u; ne.only = true; }
        else ne.dram_add = neo_dram_add;
        ne.force_kind = neo_force;
        std::vector<std::string> nl;
        std::string err;
        if (!host_trace::emit_mmio(t, nl, err, &ne)) { std::printf("[host_trace_replay] cannot write %s: %s\n", path.c_str(), err.c_str()); return 1; }
        std::ofstream os(path);
        for (const std::string &l : nl) os << l << '\n';
        long via_neo = 0;
        for (const host_trace::Item &it : t.items) via_neo += it.hw_runnable ? 1 : 0;
        std::printf("[host_trace_replay] %s: %zu lines; %ld of %zu instructions through the neo window\n", path.c_str(), nl.size(), via_neo, t.items.size());
    }
    if (!cmp_dir.empty())
    {
        bool ok;
        const std::vector<std::string> orig = read_lines(cmp_dir + "/mmio.txt", ok);
        if (!ok) { std::printf("cannot read %s/mmio.txt\n", cmp_dir.c_str()); return 2; }
        std::string msg;
        const long bad = host_trace::compare_streams(host_trace::replay_writes(host_trace::writes_of(orig), &mmio_err), got, msg);
        const auto ho = host_trace::host_steps_of(orig), ht = host_trace::host_steps_of(lines);
        const bool hsame = ho == ht;
        std::printf("[host_trace_replay] trace vs %s/mmio.txt: %ld instruction differences%s%s, %ld MMIO errors; host steps: %zu in the program, %zu in the trace, %s\n",
                    cmp_dir.c_str(), bad, bad ? " (first: " : "", bad ? (msg + ")").c_str() : "", mmio_err, ho.size(), ht.size(),
                    hsame ? "same steps at the same places" : "DIFFERENT");
        pass = pass && bad == 0 && mmio_err == 0 && hsame;
    }
    if (!emit_path.empty())
    {
        std::ofstream os(emit_path);
        for (const std::string &l : lines) os << l << '\n';
        std::string msg;
        const host_trace::Writes w = host_trace::writes_of(lines);
        const long bad = host_trace::compare_streams(host_trace::replay_writes(w, &mmio_err), got, msg);
        std::printf("[host_trace_replay] %s: %zu register writes, %zu host steps; replayed through MmioCompat: %ld differences%s%s, %ld MMIO errors\n", emit_path.c_str(),
                    w.size(), host_trace::host_steps_of(lines).size(), bad, bad ? " (first: " : "", bad ? (msg + ")").c_str() : "", mmio_err);
        pass = pass && bad == 0 && mmio_err == 0;
    }
    std::printf("[host_trace_replay] RESULT: %s\n", pass ? "PASS" : "FAIL");
    return pass ? 0 : 1;
}
