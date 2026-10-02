// tb_gvu_elemwise.cpp -- unit validation of has/gvu_elemwise.h + has/gvu_scratchpad.h (bit-exact unit checks).
//   E1. ADD compat (unit scales) == sat8(a + b) -- the v4.5 residual add
//   E2. MAX_POOL 5x5 s1 p2 on the SPPF shape [288,20,20] == a bounds-checked direct loop (no padding buffer), both MP_MODEs
//   E3. MAX_POOL DIRECT == SEPARABLE on random shapes / strides
//   E4. Scratchpad overflow is reported (throws), not wrapped
// Cycle counts printed are ESTIMATES (see gvu_elemwise.h header).
#include <systemc.h>
#include <cstdio>
#include <random>
#include "has/gvu_elemwise.h"

using namespace has;
static int g_fail = 0;
#define CHECK(cond, ...) do { if (!(cond)) { if (g_fail < 20) std::printf(__VA_ARGS__); g_fail++; } } while (0)

static int8_t ref_max(const std::vector<int8_t> &in, int c, int H, int Wd, int y, int x, int k, int s, int p)
{
    int best = -128;
    for (int ky = 0; ky < k; ky++)
        for (int kx = 0; kx < k; kx++)
        {
            const int iy = y * s + ky - p, ix = x * s + kx - p;
            const int v = (iy >= 0 && iy < H && ix >= 0 && ix < Wd) ? in[(static_cast<size_t>(c) * H + iy) * Wd + ix] : -128;
            best = v > best ? v : best;
        }
    return static_cast<int8_t>(best);
}

SC_MODULE(TbElem)
{
    sc_clock clk{"clk", 10, SC_NS};
    SC_CTOR(TbElem)
    {
        SC_THREAD(run);
        sensitive << clk.posedge_event();
    }

    void run()
    {
        std::mt19937 rng(424242);
        auto rnd8 = [&]() { return static_cast<int8_t>(static_cast<int>(rng() % 256) - 128); };

        // E1 -------------------------------------------------------------------------------------------------
        {
            HasElemwise ew(Knobs::compat());
            const size_t n = 48 * 160 * 160; // largest residual tensor of YOLOv8m (dark2 C2f), 1 228 800 elements
            std::vector<int8_t> A(n), B(n), O(n);
            for (size_t i = 0; i < n; i++) { A[i] = rnd8(); B[i] = rnd8(); }
            const sc_time t0 = sc_time_stamp();
            ew.add(A.data(), B.data(), O.data(), n, AddParams());
            const uint64_t cyc = static_cast<uint64_t>((sc_time_stamp() - t0) / clk.period());
            size_t bad = 0;
            for (size_t i = 0; i < n; i++)
            {
                const int want = std::max(-128, std::min(127, A[i] + B[i]));
                bad += O[i] != want;
            }
            CHECK(bad == 0, "[E1] %zu of %zu elements differ from sat8(a+b)\n", bad, n);
            std::printf("[E1] ADD compat, %zu elements: %s | cycles %llu (dma %llu, compute %llu) scratch peak %u B, "
                        "rd %llu B, wr %llu B, clamp8 %llu\n", n, bad ? "FAIL" : "PASS", (unsigned long long)cyc,
                        (unsigned long long)ew.stats().cyc_dma, (unsigned long long)ew.stats().cyc_compute, ew.scratch().peak,
                        (unsigned long long)ew.scratch().rd_bytes, (unsigned long long)ew.scratch().wr_bytes,
                        (unsigned long long)ew.stats().qc_req.clamp8);
        }
        // E2 -------------------------------------------------------------------------------------------------
        for (int mode = 0; mode < 2; mode++)
        {
            Knobs k = Knobs();
            k.mp_mode = mode;
            HasElemwise ew(k);
            const int C = 288, H = 20, Wd = 20;
            std::vector<int8_t> in(static_cast<size_t>(C) * H * Wd), out(in.size());
            for (auto &v : in) v = rnd8();
            const sc_time t0 = sc_time_stamp();
            ew.maxpool(in.data(), out.data(), C, H, Wd, 5, 1, 2);
            const uint64_t cyc = static_cast<uint64_t>((sc_time_stamp() - t0) / clk.period());
            size_t bad = 0;
            for (int c = 0; c < C; c++)
                for (int y = 0; y < H; y++)
                    for (int x = 0; x < Wd; x++)
                        bad += out[(static_cast<size_t>(c) * H + y) * Wd + x] != ref_max(in, c, H, Wd, y, x, 5, 1, 2);
            CHECK(bad == 0, "[E2] mode %d: %zu elements differ\n", mode, bad);
            std::printf("[E2] MAX 5x5 s1 p2 [288,20,20] %s: %s | cycles %llu (dma %llu, compute %llu) scratch peak %u B\n",
                        mode ? "SEPARABLE" : "DIRECT", bad ? "FAIL" : "PASS", (unsigned long long)cyc,
                        (unsigned long long)ew.stats().cyc_dma, (unsigned long long)ew.stats().cyc_compute, ew.scratch().peak);
        }
        // E3 -------------------------------------------------------------------------------------------------
        {
            size_t bad = 0, cases = 0;
            for (int t = 0; t < 40; t++)
            {
                const int C = 1 + rng() % 9, H = 3 + rng() % 30, Wd = 3 + rng() % 40, kk = 1 + 2 * (rng() % 3),
                          s = 1 + rng() % 2, p = rng() % (kk / 2 + 1);
                if ((H + 2 * p - kk) < 0 || (Wd + 2 * p - kk) < 0) continue;
                std::vector<int8_t> in(static_cast<size_t>(C) * H * Wd);
                for (auto &v : in) v = rnd8();
                const int Ho = (H + 2 * p - kk) / s + 1, Wo = (Wd + 2 * p - kk) / s + 1;
                std::vector<int8_t> o1(static_cast<size_t>(C) * Ho * Wo), o2(o1.size());
                Knobs k1, k2;
                k1.mp_mode = MP_DIRECT;
                k2.mp_mode = MP_SEPARABLE;
                HasElemwise e1(k1), e2(k2);
                e1.maxpool(in.data(), o1.data(), C, H, Wd, kk, s, p);
                e2.maxpool(in.data(), o2.data(), C, H, Wd, kk, s, p);
                for (int c = 0; c < C; c++)
                    for (int y = 0; y < Ho; y++)
                        for (int x = 0; x < Wo; x++)
                        {
                            const size_t i = (static_cast<size_t>(c) * Ho + y) * Wo + x;
                            const int8_t r = ref_max(in, c, H, Wd, y, x, kk, s, p);
                            bad += (o1[i] != r) + (o2[i] != r);
                        }
                cases++;
            }
            CHECK(bad == 0, "[E3] %zu mismatches\n", bad);
            std::printf("[E3] MAX DIRECT == SEPARABLE == direct loop on %zu random shapes: %s\n", cases, bad ? "FAIL" : "PASS");
        }
        // E5 AVG_POOL -----------------------------------------------------------------------------------------
        {
            // (a) 2x2 s2 average with scale 1, shift 2 (divide by 4, HALF_UP) vs a direct integer reference
            Knobs k;
            HasElemwise ew(k);
            const int C = 16, H = 20, Wd = 20;
            std::vector<int8_t> in(static_cast<size_t>(C) * H * Wd), out(static_cast<size_t>(C) * 10 * 10);
            for (auto &v : in) v = rnd8();
            ew.avgpool(in.data(), out.data(), C, H, Wd, 2, 2, 1u, 2);
            size_t bad = 0;
            for (int c = 0; c < C; c++)
                for (int y = 0; y < 10; y++)
                    for (int x = 0; x < 10; x++)
                    {
                        int sum = 0;
                        for (int dy = 0; dy < 2; dy++)
                            for (int dx = 0; dx < 2; dx++) sum += in[(static_cast<size_t>(c) * H + 2 * y + dy) * Wd + 2 * x + dx];
                        const int want = (sum + 2) >> 2; // HALF_UP division by 4 (floor of (sum + 2) / 4)
                        bad += out[(static_cast<size_t>(c) * 10 + y) * 10 + x] != want;
                    }
            CHECK(bad == 0 && ew.acc16_ovf == 0, "[E5a] %zu mismatches, acc16_ovf %llu\n", bad, (unsigned long long)ew.acc16_ovf);
            std::printf("[E5a] AVG 2x2 s2 (/4, HALF_UP) vs integer reference: %s\n", bad ? "FAIL" : "PASS");
            // (b) H5: global 20x20 average of a constant 127 tensor overflows the int16 accumulator (400 * 127 = 50 800)
            HasElemwise ew2(k);
            std::vector<int8_t> big(static_cast<size_t>(4) * 400, 127), o2(4);
            ew2.avgpool(big.data(), o2.data(), 4, 20, 20, 20, 1, 1u, 0);
            CHECK(ew2.acc16_ovf == 4, "[E5b] expected 4 int16 overflows, got %llu\n", (unsigned long long)ew2.acc16_ovf);
            std::printf("[E5b] H5: global 20x20 AVG of 127 overflows the int16 accumulator in %llu/4 channels (sum 50 800 > 32 767): %s\n",
                        (unsigned long long)ew2.acc16_ovf, ew2.acc16_ovf == 4 ? "DETECTED" : "FAIL");
        }
        // E4 -------------------------------------------------------------------------------------------------
        {
            bool threw = false;
            try
            {
                HasScratchpad sp(1024);
                uint8_t buf[64] = {0};
                sp.write(1000, buf, 64);
            }
            catch (const std::exception &) { threw = true; }
            CHECK(threw, "[E4] scratchpad overflow was not reported\n");
            std::printf("[E4] Scratchpad overflow reported: %s\n", threw ? "PASS" : "FAIL");
        }
        std::printf("RESULT: %s (failures %d)\n", g_fail ? "FAIL" : "PASS", g_fail);
        sc_stop();
    }
};

int sc_main(int, char **)
{
    TbElem tb("tb");
    sc_start();
    return g_fail ? 1 : 0;
}
