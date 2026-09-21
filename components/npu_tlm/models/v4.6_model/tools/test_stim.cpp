// Validate sauria_stim (GoldenStimuli emitter) against a captured case's shipped
// GoldenStimuli.txt — no Python. Computes controller_args in C (libsauria_cfg) from
// the case shape, emits the register-command stream, and compares token-for-token.
//
// Build: g++ -std=c++17 -I. -Idriver tools/test_stim.cpp -o /tmp/test_stim
// Run:   /tmp/test_stim npu_demo_clean/cases/<name>
#include "libsauria_cfg.h"
#include "sauria_stim.h"
#include "sauria_targets.h"
#include <cstdio>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
using namespace sauria;

static std::string env_val(const std::string &path, const std::string &key)
{
    std::ifstream f(path); std::string line;
    while (std::getline(f, line)) {
        if (line.rfind(key + "=", 0) == 0) {
            std::string v = line.substr(key.size() + 1);
            if (!v.empty() && v.front() == '"') v = v.substr(1, v.rfind('"') - 1);
            return v;
        }
    }
    return "";
}
// Parse a GoldenStimuli file into a flat list of uint64 tokens (hex).
static std::vector<uint64_t> toks_of_file(const std::string &path)
{
    std::vector<uint64_t> v; std::ifstream f(path); std::string line;
    while (std::getline(f, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        std::istringstream is(line); std::string t;
        while (is >> t) v.push_back(std::stoull(t, nullptr, 16));
    }
    return v;
}
static std::vector<uint64_t> toks_of_text(const std::string &s)
{
    std::vector<uint64_t> v; std::istringstream is(s); std::string t;
    while (is >> t) v.push_back(std::stoull(t, nullptr, 16));
    return v;
}

int main(int argc, char **argv)
{
    if (argc < 2) { printf("usage: test_stim <case_dir>\n"); return 2; }
    std::string dir = argv[1];
    std::string ver = env_val(dir + "/case.env", "VERSION");
    std::string shape = env_val(dir + "/case.env", "SHAPE");
    const SauriaTarget *t = sauria_find_target(ver.c_str());
    if (!t) { printf("[stim] unknown target '%s'\n", ver.c_str()); return 2; }

    std::istringstream is(shape);
    SauriaLayerDesc d{}; int Cin, Cw, Ch, Cout, Xused, Yused, pre;
    is >> d.B_w >> d.B_h >> d.d >> d.s >> Cin >> Cw >> Ch >> Cout >> Xused >> Yused >> pre;
    d.c_til = Cin; d.k_til = Cout; d.h_til = Ch; d.w_til = Cw;
    d.X_used = Xused; d.Y_used = Yused; d.preload_en = pre;
    d.C_w = Cw; d.C_h = Ch; d.C_c = Cout; d.A_c = Cin;

    // DRAM bases = region offsets (dram_offset=0; widths are byte-multiples).
    int Bw_eff = 1 + (d.B_w - 1) * d.d, Bh_eff = 1 + (d.B_h - 1) * d.d;
    int A_w = (1 + d.s * (Cw - 1)) + Bw_eff - 1, A_h = (1 + d.s * (Ch - 1)) + Bh_eff - 1;
    uint32_t ib = t->in_bytes;
    uint32_t A_off = 0;
    uint32_t B_off = (uint32_t)Cin * A_h * A_w * ib;
    uint32_t C_off = B_off + (uint32_t)Cout * Cin * d.B_h * d.B_w * ib;
    uint32_t bases[3] = {A_off, B_off, C_off};

    std::vector<uint32_t> args = sauria_encode_controller_args(d, *t, bases);
    std::string my = sauria_stim_to_text(sauria_emit_stim(args));

    auto ref = toks_of_file(dir + "/stimuli/GoldenStimuli.txt");
    auto mine = toks_of_text(my);

    int fails = 0;
    size_t n = std::min(ref.size(), mine.size());
    if (ref.size() != mine.size()) { printf("  TOKEN COUNT ref=%zu mine=%zu\n", ref.size(), mine.size()); fails++; }
    size_t first_bad = (size_t)-1;
    for (size_t i = 0; i < n; i++)
        if (ref[i] != mine[i]) { if (first_bad == (size_t)-1) first_bad = i; fails++; }
    if (first_bad != (size_t)-1)
        printf("  FIRST TOKEN MISMATCH at %zu (row %zu col %zu) ref=%llX mine=%llX\n",
               first_bad, first_bad / 7, first_bad % 7,
               (unsigned long long)ref[first_bad], (unsigned long long)mine[first_bad]);

    printf("%-22s : %s  (GoldenStimuli vs shipped; %zu tokens)\n", ver.c_str(),
           fails ? "FAIL" : "BIT-EXACT", ref.size());
    return fails ? 1 : 0;
}
