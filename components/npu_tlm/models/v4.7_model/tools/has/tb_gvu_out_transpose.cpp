// tb_gvu_out_transpose.cpp -- unit test of has/gvu_out_transpose.h (corner turn of the GEMM_FUSED epilogue, HAS_OUT_TRANSPOSE).
// Random tiles of nch channels x npos positions in contexts of per_ctx positions: the output must be the transpose, and the edges
// must follow the timing of the header: context k (n beats, p_k positions) has its last beat taken at T_k = T_(k-1) + max(n, p_(k-1)),
// T_0 = n; the run ends at T_last + p_last, so the tail after the last beat is p_last.
// Build (repository root): g++ -std=c++17 -O2 -I. tools/has/tb_gvu_out_transpose.cpp -o tools/has/tb_gvu_out_transpose
#include <cstdio>
#include <random>
#include <vector>
#include "has/gvu_out_transpose.h"

int main()
{
    std::mt19937 rng(1660);
    std::uniform_int_distribution<int> val(-128, 127);
    const unsigned nchs[] = {1, 5, 16, 31, 32}, nposs[] = {1, 5, 32, 33, 197, 1024}, pcs[] = {32, 16, 7};
    int cases = 0, bad = 0;
    for (unsigned nch : nchs)
        for (unsigned npos : nposs)
            for (unsigned pc : pcs)
            {
                std::vector<int8_t> cm(size_t(nch) * npos);
                for (auto &v : cm) v = int8_t(val(rng));
                uint64_t edges = 0, tail = 0;
                const std::vector<int8_t> tm = has::GvuOutTranspose<32>::run(cm, nch, npos, pc, &edges, &tail);
                bool ok = tm.size() == cm.size();
                for (unsigned c = 0; ok && c < nch; c++)
                    for (unsigned p = 0; ok && p < npos; p++) ok = tm[size_t(p) * nch + c] == cm[size_t(c) * npos + p];
                uint64_t T = 0, prev_p = 0;
                for (unsigned p0 = 0; p0 < npos; p0 += pc)
                {
                    const uint64_t p = std::min(pc, npos - p0);
                    T = (p0 == 0) ? nch : T + std::max<uint64_t>(nch, prev_p);
                    prev_p = p;
                }
                const uint64_t want_edges = T + prev_p, want_tail = prev_p;
                const bool tok = edges == want_edges && tail == want_tail && tail == has::GvuOutTranspose<32>::tail_edges(npos, pc);
                cases++;
                if (!ok || !tok)
                {
                    bad++;
                    std::printf("FAIL nch %u npos %u per_ctx %u: data %s, edges %llu (want %llu), tail %llu (want %llu)\n", nch, npos, pc,
                                ok ? "ok" : "WRONG", (unsigned long long)edges, (unsigned long long)want_edges, (unsigned long long)tail,
                                (unsigned long long)want_tail);
                }
            }
    // the ViT case: a 32-channel tile of 197 positions in contexts of 32 -> 7 contexts, tail 5, edges 7 * 32 + 5
    std::vector<int8_t> cm(32 * 197, 1);
    uint64_t e = 0, t = 0;
    has::GvuOutTranspose<32>::run(cm, 32, 197, 32, &e, &t);
    std::printf("ViT tile 32 x 197: edges %llu, tail %llu\n", (unsigned long long)e, (unsigned long long)t);
    std::printf("RESULT %s: %d / %d cases\n", bad ? "FAIL" : "PASS", cases - bad, cases);
    return bad ? 1 : 0;
}
