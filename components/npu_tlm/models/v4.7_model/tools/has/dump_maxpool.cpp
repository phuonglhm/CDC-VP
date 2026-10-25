// dump_maxpool.cpp -- writes every ELEM_WISE MAX_POOL instruction of a program (mmio.txt + dram_golden.bin) as
// mp<idx>_in.txt ([C][H][W], one int8 per line, not padded), mp<idx>_out.txt ([C][Ho][Wo]) and mp<idx>_par.txt.
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include "has/has_mmio_compat.h"
int main(int argc, char **argv)
{
    if (argc < 3) { std::printf("usage: %s <program dir> <out dir>\n", argv[0]); return 2; }
    const std::string dir = argv[1], od = argv[2];
    std::ifstream bf(dir + "/dram_golden.bin", std::ios::binary);
    std::vector<char> gold((std::istreambuf_iterator<char>(bf)), std::istreambuf_iterator<char>());
    const int8_t *g = reinterpret_cast<const int8_t *>(gold.data());
    std::ifstream f(dir + "/mmio.txt");
    has::MmioCompat mm;
    std::string line, label;
    unsigned long index = 0; int n = 0;
    while (std::getline(f, line))
    {
        if (line.rfind("# [", 0) == 0) { label = line.substr(2); continue; }
        if (line.empty() || line[0] == '#' || line[0] == 'H') continue;
        std::istringstream is(line);
        std::string tag; uint32_t addr = 0, val = 0;
        is >> tag >> std::hex >> addr >> val;
        mm.write(addr, val);
        has::LayerInstr li;
        while (mm.pop(li))
        {
            const unsigned long idx = index;
            if (li.last_of_host) index++;
            mm.retire(li);
            if (li.opcode != 0x15 || li.mode == 0 || li.mode == 5) continue;
            const int Ho = (li.h + 2 * li.pool_p - li.pool_k) / li.pool_s + 1, Wo = (li.w + 2 * li.pool_p - li.pool_k) / li.pool_s + 1;
            const std::string p = od + "/mp" + std::to_string(idx);
            FILE *a = std::fopen((p + "_in.txt").c_str(), "w");
            for (long e = 0; e < static_cast<long>(li.c) * li.h * li.w; e++) std::fprintf(a, "%d\n", g[li.a_addr + e]);
            std::fclose(a);
            FILE *b = std::fopen((p + "_out.txt").c_str(), "w");
            for (long e = 0; e < static_cast<long>(li.c) * Ho * Wo; e++) std::fprintf(b, "%d\n", g[li.out_addr + e]);
            std::fclose(b);
            FILE *c = std::fopen((p + "_par.txt").c_str(), "w");
            std::fprintf(c, "instr %lu C %d H %d W %d k %d s %d p %d in_addr 0x%x out_addr 0x%x label %s\n", idx, li.c, li.h, li.w, li.pool_k, li.pool_s, li.pool_p,
                         static_cast<unsigned>(li.a_addr), static_cast<unsigned>(li.out_addr), label.c_str());
            std::fclose(c);
            std::printf("MAX_POOL %lu C %d H %d W %d k %d s %d p %d in 0x%x out 0x%x  %s\n", idx, li.c, li.h, li.w, li.pool_k, li.pool_s, li.pool_p,
                        static_cast<unsigned>(li.a_addr), static_cast<unsigned>(li.out_addr), label.c_str());
            n++;
        }
    }
    std::printf("max pools %d\n", n);
    return 0;
}
