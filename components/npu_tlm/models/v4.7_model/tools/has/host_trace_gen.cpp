// host_trace_gen.cpp -- writes the register trace of a program for neo_lite (has/has_host_trace.h).
//
//   HAS_HOST_TRACE=<trace file> host_trace_gen [--dram-out <file>] <program dir>      program dir = mmio.txt (+ dram_init.bin, dram_golden.bin)
//   HAS_HOST_TRACE_CFG_BASE / HAS_HOST_TRACE_DRAM_BASE (hex, default 80000000 / 40000000) place the configuration window and the DRAM in the SoC map.
// The knob is HAS_HOST_TRACE: not set = nothing is written (usage printed, exit 2). The trace is a function of the instruction stream the program pushes
// (has::MmioCompat), so no simulation is needed to produce it.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include "has/has_host_trace.h"

using namespace has;

int main(int argc, char **argv)
{
    std::string dir, dram_out;
    for (int i = 1; i < argc; i++)
    {
        if (!std::strcmp(argv[i], "--dram-out") && i + 1 < argc) dram_out = argv[++i];
        else dir = argv[i];
    }
    const char *out_path = std::getenv("HAS_HOST_TRACE");
    if (!out_path || !*out_path || dir.empty())
    {
        std::fprintf(stderr, "usage: HAS_HOST_TRACE=<trace file> %s [--dram-out <file>] <program dir with mmio.txt>\n", argv[0]);
        return 2;
    }
    host_trace::Config cfg;
    if (const char *e = std::getenv("HAS_HOST_TRACE_CFG_BASE")) cfg.cfg_base = static_cast<uint32_t>(std::strtoul(e, nullptr, 16));
    if (const char *e = std::getenv("HAS_HOST_TRACE_DRAM_BASE")) cfg.dram_base = static_cast<uint32_t>(std::strtoul(e, nullptr, 16));
    std::ifstream f(dir + "/mmio.txt");
    if (!f) { std::fprintf(stderr, "cannot read %s/mmio.txt\n", dir.c_str()); return 2; }
    std::ofstream os(out_path);
    if (!os) { std::fprintf(stderr, "cannot write %s\n", out_path); return 2; }
    host_trace::write_header(os, cfg);
    os << "D in " << dir << "/dram_init.bin\nD golden " << dir << "/dram_golden.bin\n";
    if (!dram_out.empty()) os << "D out " << dram_out << '\n';

    MmioCompat m(1 << 20);
    std::map<uint32_t, uint32_t> pending;   // v4.5 float register words written since the last push
    static const uint32_t kFloat[] = {mmio::IN_SCALE, mmio::W_SCALE, mmio::OUT_SCALE, mmio::R458, mmio::R45C, mmio::SCALE_OUT_E};
    std::vector<std::string> pending_host;   // "H ..." host steps seen since the last instruction
    int n = 0;
    long as_given = 0, x_instr = 0, errors = 0;
    std::string line;
    while (std::getline(f, line))
    {
        if (line.compare(0, 2, "H ") == 0) { pending_host.push_back(line); continue; }
        std::istringstream is(line);
        std::string tag; uint32_t a, v;
        if (!(is >> tag) || tag != "W" || !(is >> std::hex >> a >> v)) continue;
        m.write(a, v);
        for (uint32_t k : kFloat) if (a == k) pending[a] = v;
        LayerInstr li;
        while (m.pop(li))
        {
            m.retire(li);
            const host_neo::Plan p = host_neo::plan(li);
            if (!p.error.empty()) { std::fprintf(stderr, "instruction %d: %s\n", n, p.error.c_str()); errors++; n++; continue; }
            host_trace::write_item(os, cfg, n, li, p, li.from_float_scale ? pending : std::map<uint32_t, uint32_t>(), pending_host);
            pending_host.clear();
            as_given += p.as_given() ? 1 : 0;
            for (const host_neo::Ext &e : p.ext) if (!e.provenance()) { x_instr++; break; }
            n++;
        }
        if (a == mmio::PUSH_A) pending.clear();
    }
    for (const std::string &h : pending_host) os << h << '\n';   // host steps after the last instruction
    std::printf("[host_trace_gen] %s: %d instructions -> %s | runnable as given on neo_lite RTL revision 1 %ld, with X lines %ld, plan errors %ld\n",
                dir.c_str(), n, out_path, as_given, x_instr, errors);
    return errors ? 1 : 0;
}
