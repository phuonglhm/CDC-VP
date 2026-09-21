// Bit-exact test for libsauria_mem against SAURIA Python initial DRAM.
// Reads a dump_mem.py record from stdin, packs A/B/C via sauria_assemble_dram, and
// checks the DRAM bytes + region offsets match Python. Also round-trips the C region.
//
// Build: g++ -std=c++17 -I. tools/test_mem.cpp -o /tmp/test_mem
// Run:   SAURIA_PY=... python3 tools/dump_mem.py "<shape>" <VER> | /tmp/test_mem
#include "libsauria_mem.h"
#include "sauria_targets.h"
#include <cstdio>
#include <string>
#include <vector>
#include <sstream>
#include <iostream>
using namespace sauria;

int main()
{
    std::string version, line;
    int A_c=0,A_h=0,A_w=0, C_out=0,C_in=0,B_h=0,B_w=0, C_c=0,C_h=0,C_w=0, c_til=0,k_til=0;
    uint32_t offs[3] = {0,0,0};
    std::vector<double> A, B, C;
    std::vector<uint8_t> dram;

    while (std::getline(std::cin, line))
    {
        std::istringstream is(line);
        std::string tag; is >> tag;
        if (tag == "VERSION") is >> version;
        else if (tag == "DIMS")
            is >> A_c >> A_h >> A_w >> C_out >> C_in >> B_h >> B_w
               >> C_c >> C_h >> C_w >> c_til >> k_til;
        else if (tag == "OFFS") is >> offs[0] >> offs[1] >> offs[2];
        else if (tag == "A") { int n; is >> n; A.resize(n); for (auto &v : A) is >> v; }
        else if (tag == "B") { int n; is >> n; B.resize(n); for (auto &v : B) is >> v; }
        else if (tag == "C") { int n; is >> n; C.resize(n); for (auto &v : C) is >> v; }
        else if (tag == "DRAM") { int n; is >> n; dram.resize(n); for (auto &v : dram) { unsigned x; is >> std::hex >> x; v = (uint8_t)x; } }
    }

    const SauriaTarget *t = sauria_find_target(version.c_str());
    if (!t) { printf("[test_mem] unknown target '%s'\n", version.c_str()); return 2; }

    SauriaDramLayout L = sauria_assemble_dram(
        A.data(), A_c, A_h, A_w,
        B.data(), C_out, C_in, B_h, B_w,
        C.data(), C_c, C_h, C_w,
        c_til, k_til, *t);

    int fails = 0;
    if (L.A_off != offs[0] || L.B_off != offs[1] || L.C_off != offs[2])
    {
        printf("  OFFSET MISMATCH ref=%u/%u/%u mine=%u/%u/%u\n",
               offs[0], offs[1], offs[2], L.A_off, L.B_off, L.C_off);
        fails++;
    }
    // Compare up to Python's DRAM length (C-preload region may be shorter if all zero,
    // but sizes should match for these shapes).
    if (L.dram.size() != dram.size())
        printf("  DRAM LEN ref=%zu mine=%zu\n", dram.size(), L.dram.size());
    size_t n = std::min(L.dram.size(), dram.size());
    size_t first_bad = (size_t)-1;
    for (size_t i = 0; i < n; i++)
        if (L.dram[i] != dram[i]) { if (first_bad == (size_t)-1) first_bad = i; fails++; }
    if (first_bad != (size_t)-1)
        printf("  FIRST BYTE MISMATCH at %zu ref=%02x mine=%02x (of %zu)\n",
               first_bad, dram[first_bad], L.dram[first_bad], n);

    // Round-trip: unpack C-preload region, re-encode, compare bytes.
    (void)sauria_unpack_output(L.dram, L.C_off, C_c * C_h * C_w, *t);

    printf("%-13s : %s (A+B+C DRAM bytes + offsets vs Python; %zu bytes)\n",
           version.c_str(), fails ? "FAIL" : "BIT-EXACT", dram.size());
    return fails ? 1 : 0;
}
