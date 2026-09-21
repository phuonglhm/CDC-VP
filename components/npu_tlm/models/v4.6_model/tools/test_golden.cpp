// Validate sauria_golden (C reference conv) against a captured case's gold_dram,
// using ONLY the shipped case files — no Python, no torch. Reads case.env + the
// stimuli, unpacks A/B/Cpre from initial_dram, recomputes the golden in C, and
// compares to gold_dram's output region. INT: bit-exact; FP16: within ULP.
//
// Build: g++ -std=c++17 -I. -Idriver tools/test_golden.cpp -o /tmp/test_golden
// Run:   /tmp/test_golden npu_demo_clean/cases/<name>
#include "sauria_golden.h"
#include "libsauria_mem.h"
#include "sauria_targets.h"
#include <cstdio>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
using namespace sauria;

static std::vector<uint8_t> read_hex_bytes(const std::string &path)
{
    std::vector<uint8_t> v; std::ifstream f(path); std::string line;
    while (std::getline(f, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        v.push_back((uint8_t)std::stoul(line, nullptr, 16));
    }
    return v;
}
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
static uint64_t rd_le(const std::vector<uint8_t> &m, size_t off, int nbytes)
{
    uint64_t v = 0;
    for (int i = 0; i < nbytes; i++) v |= (uint64_t)(off + i < m.size() ? m[off + i] : 0) << (8 * i);
    return v;
}
static int32_t fp16_ulp(uint16_t a, uint16_t b)
{
    auto ord = [](uint16_t h) { return (h & 0x8000) ? -(int32_t)(h & 0x7FFF) : (int32_t)h; };
    int32_t d = ord(a) - ord(b); return d < 0 ? -d : d;
}

int main(int argc, char **argv)
{
    if (argc < 2) { printf("usage: test_golden <case_dir>\n"); return 2; }
    std::string dir = argv[1];
    std::string ver = env_val(dir + "/case.env", "VERSION");
    std::string shape = env_val(dir + "/case.env", "SHAPE");
    const SauriaTarget *t = sauria_find_target(ver.c_str());
    if (!t) { printf("[golden] unknown target '%s'\n", ver.c_str()); return 2; }

    std::istringstream is(shape);
    SauriaConvShape sh{}; int Xused, Yused;
    is >> sh.B_w >> sh.B_h >> sh.d >> sh.s >> sh.C_in >> sh.C_w >> sh.C_h >> sh.C_out >> Xused >> Yused >> sh.preload_en;
    sauria_derive_input_dims(sh);
    int c_til = sh.C_in, k_til = sh.C_out; // single external tile

    auto init = read_hex_bytes(dir + "/stimuli/initial_dram.txt");
    auto gold = read_hex_bytes(dir + "/stimuli/gold_dram.txt");
    const bool fp = (t->op_type == 1);
    const int ib = t->in_bytes, ob = t->out_bytes;

    // Region offsets (widths are byte-multiples → byte-aligned).
    uint32_t A_n = sh.C_in * sh.A_h * sh.A_w;
    uint32_t B_n = sh.C_out * sh.C_in * sh.B_h * sh.B_w;
    uint32_t C_n = sh.C_out * sh.C_h * sh.C_w;
    uint32_t A_off = 0, B_off = A_n * ib, C_off = B_off + B_n * ib;

    // Unpack A [C_in][A_h][A_w] C-order.
    std::vector<double> A(A_n);
    for (uint32_t i = 0; i < A_n; i++) A[i] = sauria_dec_elem(rd_le(init, A_off + (size_t)i * ib, ib), fp, t->ia_w);
    // Unpack weights: DRAM is in SAURIA order [K_ext,C_ext,c_til,B_h,B_w,k_til];
    // reconstruct B[k][c][kh][kw] by walking the same order.
    std::vector<double> B(B_n);
    { int K_ext = sh.C_out / k_til, C_ext = sh.C_in / c_til; size_t e = 0;
      for (int kx = 0; kx < K_ext; kx++) for (int cx = 0; cx < C_ext; cx++)
       for (int ct = 0; ct < c_til; ct++) for (int kh = 0; kh < sh.B_h; kh++)
        for (int kw = 0; kw < sh.B_w; kw++) for (int kt = 0; kt < k_til; kt++) {
          int k = kx * k_til + kt, c = cx * c_til + ct;
          B[(((size_t)k * sh.C_in + c) * sh.B_h + kh) * sh.B_w + kw] =
              sauria_dec_elem(rd_le(init, B_off + e * ib, ib), fp, t->ib_w);
          e++;
        } }
    // Unpack C preloads [C_out][C_h][C_w] C-order.
    std::vector<double> Cpre(C_n);
    for (uint32_t i = 0; i < C_n; i++) Cpre[i] = sauria_dec_elem(rd_le(init, C_off + (size_t)i * ob, ob), fp, t->oc_w);

    // Compute golden in C.
    std::vector<double> my = sauria_reference_conv(A.data(), B.data(), Cpre.data(), sh, *t);

    // Compare to gold_dram output region (natural [C_out][C_h][C_w] order).
    int fails = 0, exact = 0, tol = 0, maxulp = 0;
    for (uint32_t i = 0; i < C_n; i++) {
        uint64_t graw = rd_le(gold, C_off + (size_t)i * ob, ob);
        if (fp) {
            uint16_t gb = (uint16_t)graw, mb = fp16_t::float_to_half((float)my[i]);
            int u = fp16_ulp(mb, gb);
            if (u == 0) exact++; else if (u <= 16) tol++; else fails++;
            if (u > maxulp) maxulp = u;
        } else {
            int64_t gv = (int64_t)sauria_dec_elem(graw, false, t->oc_w);
            if ((int64_t)my[i] == gv) exact++; else fails++;
        }
    }
    if (fp)
        printf("%-22s %-12s : %s  (exact=%d tol=%d max=%d ULP, n=%d)\n", ver.c_str(),
               fp ? "FP16" : "", fails ? "FAIL" : "MATCH", exact, tol, maxulp, C_n);
    else
        printf("%-22s %-12s : %s  (exact=%d/%d)\n", ver.c_str(), "INT",
               fails ? "FAIL" : "BIT-EXACT", exact, C_n);
    return fails ? 1 : 0;
}
