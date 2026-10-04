// tb_gvu_quant.cpp -- unit validation of has/gvu_quant.h and has/gvu_obp.h (bit-exact unit checks).
//   A. hand-derived rounding cases for every ROUND_MODE (spec HAS_IFACE §3)
//   B. compat mode == the v4.5 formula sat8(((psum + bias) * scale) >> shift) on random data (HAS_IFACE §4)
//   C. Python vector files (HAS_IFACE §8) when given on the command line -- the independent golden check
//   D. HasObp as a SystemC module: values, order, fixed latency, 1 vector/cycle, host LUT/scale/shift/NCH writes
// Build (repository root): bash tools/has/build_tb_has.sh tb_gvu_quant ; run: ./tools/has/tb_gvu_quant [vector files...]
#include <systemc.h>
#include <cstdio>
#include <fstream>
#include <random>
#include <sstream>
#include "has/gvu_quant.h"
#include "has/gvu_obp.h"

using namespace has;

static int g_fail = 0;
#define CHECK(cond, ...)                          \
    do                                            \
    {                                             \
        if (!(cond))                              \
        {                                         \
            if (g_fail < 20) std::printf(__VA_ARGS__); \
            g_fail++;                             \
        }                                         \
    } while (0)

// ---------------------------------------------------------------- A
static void test_rounding()
{
    struct Case { int64_t p; int s; int64_t floor_, up, away, even; };
    // p / 2^s: 1.5, -1.5, 2.5, -2.5, 0.25, -0.75, 7 (exact), -7 (exact)
    const Case cs[] = {{3, 1, 1, 2, 2, 2},     {-3, 1, -2, -1, -2, -2}, {5, 1, 2, 3, 3, 2},  {-5, 1, -3, -2, -3, -2},
                       {1, 2, 0, 0, 0, 0},     {-3, 2, -1, -1, -1, -1}, {7, 0, 7, 7, 7, 7},  {-28, 2, -7, -7, -7, -7},
                       {6, 2, 1, 2, 2, 2},     {-6, 2, -2, -1, -2, -2}, {10, 2, 2, 3, 3, 2}, {-10, 2, -3, -2, -3, -2}};
    for (const auto &c : cs)
    {
        const int64_t want[4] = {c.floor_, c.up, c.away, c.even};
        for (int m = 0; m < 4; m++)
        {
            const int64_t got = static_cast<int64_t>(rshift(c.p, c.s, m));
            CHECK(got == want[m], "[A] rshift(%lld, %d, mode %d) = %lld, want %lld\n", (long long)c.p, c.s, m,
                  (long long)got, (long long)want[m]);
        }
    }
    // narrowing: 40000 -> SAT16 32767 -> clamp 127; WRAP16 40000-65536 = -25536 -> clamp -128
    Knobs k;
    k.round_mode = FLOOR;
    k.req_narrow = SAT16;
    QuantCounters c1;
    CHECK(requant(40000, 1, 0, 0, k, &c1) == 127 && c1.sat16 == 1, "[A] SAT16 narrowing\n");
    k.req_narrow = WRAP16;
    QuantCounters c2;
    CHECK(requant(40000, 1, 0, 0, k, &c2) == -128 && c2.sat16 == 1, "[A] WRAP16 narrowing\n");
    k.req_narrow = SAT16;
    CHECK(requant(-10, 1, 0, 5, k) == -5, "[A] zero point add\n");
    // dequant zero-point order: x = 10, zp = 2, S = 3, s = 1 -> SUB_BEFORE (8*3)>>1 = 12 ; ADD_AFTER (30>>1)+2 = 17
    k.deq_zp_order = SUB_BEFORE;
    CHECK(dequant(10, 2, 3, 1, k) == 12, "[A] dequant SUB_BEFORE\n");
    k.deq_zp_order = ADD_AFTER;
    CHECK(dequant(10, 2, 3, 1, k) == 17, "[A] dequant ADD_AFTER\n");
    CHECK(decode_scale(0xFFFFFFFFu, I32) == -1 && decode_scale(0xFFFFFFFFu, U32) == 4294967295LL, "[A] decode_scale\n");
    std::printf("[A] rounding / narrowing / dequant cases: %s\n", g_fail ? "FAIL" : "PASS");
}

// ---------------------------------------------------------------- B
static void test_compat_vs_m3(uint64_t n)
{
    std::mt19937_64 rng(20260924);
    const Knobs k = Knobs::compat();
    const int before = g_fail;
    for (uint64_t i = 0; i < n; i++)
    {
        const int64_t acc = static_cast<int64_t>(static_cast<int32_t>(rng())) >> (rng() % 12); // psum + bias
        const uint32_t sc = 0x80000000u | static_cast<uint32_t>(rng());                        // v4.5 scale range [2^31, 2^32)
        const int sh = 30 + static_cast<int>(rng() % 20);
        const int64_t m3 = ((static_cast<i128>(acc) * sc) >> sh) > 127 ? 127
                           : ((static_cast<i128>(acc) * sc) >> sh) < -128 ? -128
                           : static_cast<int64_t>((static_cast<i128>(acc) * sc) >> sh);
        const int got = requant(acc, decode_scale(sc, k.scale_fmt), sh, 0, k);
        CHECK(got == m3, "[B] compat acc=%lld scale=%u shift=%d got %d want %lld\n", (long long)acc, sc, sh, got,
              (long long)m3);
    }
    std::printf("[B] compat == v4.5 formula on %llu random values: %s\n", (unsigned long long)n,
                g_fail == before ? "PASS" : "FAIL");
}

// ---------------------------------------------------------------- C
// File format (HAS_IFACE §8): "# knobs round=R narrow=N deq_zp=D scale_fmt=F", then "# x S s zp out sat16 clamp8",
// then one integer row per case. S is the raw 32-bit value as written by the host (decoded with scale_fmt).
static void test_vector_file(const char *path)
{
    std::ifstream f(path);
    if (!f)
    {
        CHECK(false, "[C] cannot open %s\n", path);
        return;
    }
    Knobs k;
    std::string line;
    uint64_t rows = 0;
    const int before = g_fail;
    while (std::getline(f, line))
    {
        if (line.rfind("# knobs", 0) == 0)
        {
            std::istringstream is(line.substr(7));
            std::string kv;
            while (is >> kv)
            {
                const auto eq = kv.find('=');
                if (eq == std::string::npos) continue;
                const std::string key = kv.substr(0, eq);
                const int v = std::stoi(kv.substr(eq + 1));
                if (key == "round") k.round_mode = v;
                else if (key == "narrow") k.req_narrow = v;
                else if (key == "deq_zp") k.deq_zp_order = v;
                else if (key == "scale_fmt") k.scale_fmt = v;
            }
            continue;
        }
        if (line.empty() || line[0] == '#') continue;
        std::istringstream is(line);
        if (std::string(path).find("dequant") != std::string::npos)
        {
            // dequant file (Python golden): "# x zp S s out"
            long long x, zp, S, s, out;
            if (!(is >> x >> zp >> S >> s >> out))
            {
                CHECK(false, "[C] %s: bad row '%s'\n", path, line.c_str());
                continue;
            }
            const int got = dequant(static_cast<int>(x), static_cast<int>(zp), decode_scale(static_cast<uint32_t>(S), k.scale_fmt),
                                    static_cast<int>(s), k);
            CHECK(got == out, "[C] %s row %llu: x=%lld zp=%lld S=%lld s=%lld got %d want %lld\n", path,
                  (unsigned long long)rows, x, zp, S, s, got, out);
            rows++;
            continue;
        }
        long long x, S, s, zp, out, f16, f8;
        if (!(is >> x >> S >> s >> zp >> out >> f16 >> f8))
        {
            CHECK(false, "[C] %s: bad row '%s'\n", path, line.c_str());
            continue;
        }
        QuantCounters c;
        const int got = requant(x, decode_scale(static_cast<uint32_t>(S), k.scale_fmt), static_cast<int>(s),
                                static_cast<int>(zp), k, &c);
        CHECK(got == out && (c.sat16 != 0) == (f16 != 0) && (c.clamp8 != 0) == (f8 != 0),
              "[C] %s row %llu: x=%lld S=%lld s=%lld zp=%lld got %d/%d/%d want %lld/%lld/%lld\n", path,
              (unsigned long long)rows, x, S, s, zp, got, (int)(c.sat16 != 0), (int)(c.clamp8 != 0), out, f16, f8);
        rows++;
    }
    std::printf("[C] %s: %llu rows (%s): %s\n", path, (unsigned long long)rows, k.str().c_str(),
                g_fail == before && rows ? "PASS" : "FAIL");
}

// ---------------------------------------------------------------- D
static constexpr int W = 32;
typedef HasObp<W> ObpH;

SC_MODULE(TbObp)
{
    sc_clock clk{"clk", 10, SC_NS};
    sc_signal<bool> rstn{"rstn"};
    sc_signal<psum_vector_t<W, int32_t>> d_in{"d_in"}, d_out{"d_out"};
    sc_signal<uint32_t> a_in{"a_in"}, a_out{"a_out"}, bscale{"bscale"}, bshift{"bshift"}, h_addr{"h_addr"};
    sc_signal<sramc_mask_t<W>> m_in{"m_in"}, m_out{"m_out"};
    sc_signal<bool> v_in{"v_in"}, wren{"wren"}, v_out{"v_out"}, bias_en{"bias_en"}, req_en{"req_en"}, lut_en{"lut_en"},
        res_en{"res_en"}, vmode{"vmode"}, h_wren{"h_wren"}, h_rden{"h_rden"};
    sc_signal<act_vector_t<W, int8_t>> resid{"resid"};
    sc_signal<host_data_t> h_wdata{"h_wdata"}, h_rdata{"h_rdata"};
    sc_signal<host_mask_t> h_wmask{"h_wmask"};
    ObpH *obp;
    Knobs k;
    uint64_t n_vec;
    std::vector<std::pair<uint64_t, psum_vector_t<W, int32_t>>> sent; // (cycle sent, expected output)
    std::vector<uint32_t> sent_addr;
    uint64_t cyc{0}, got_n{0}, lat_min{~0ull}, lat_max{0};

    SC_HAS_PROCESS(TbObp);
    TbObp(sc_module_name nm, const Knobs &kn, uint64_t n) : sc_module(nm), k(kn), n_vec(n)
    {
        obp = new ObpH("obp", k);
        obp->i_clk(clk); obp->i_rstn(rstn); obp->i_data(d_in); obp->i_addr(a_in); obp->i_wmask(m_in); obp->i_valid(v_in);
        obp->i_residual(resid); obp->o_sramc_wdata(d_out); obp->o_sramc_addr(a_out); obp->o_sramc_wren(wren);
        obp->o_sramc_wmask(m_out); obp->o_valid(v_out); obp->i_bias_en(bias_en); obp->i_requant_en(req_en);
        obp->i_lut_en(lut_en); obp->i_residual_en(res_en); obp->i_vec_channel_mode(vmode); obp->i_requant_scale(bscale);
        obp->i_requant_shift(bshift); obp->i_host_addr(h_addr); obp->i_host_wren(h_wren); obp->i_host_rden(h_rden);
        obp->i_host_wdata(h_wdata); obp->i_host_wmask(h_wmask); obp->o_host_rdata(h_rdata);
        SC_THREAD(drive);
        sensitive << clk.posedge_event();
        SC_METHOD(monitor);
        sensitive << clk.posedge_event();
        dont_initialize();
    }
    ~TbObp() { delete obp; }

    void hw(uint32_t a, double v0, double v1 = 0, double v2 = 0, double v3 = 0)
    {
        host_data_t d;
        d[0] = v0; d[1] = v1; d[2] = v2; d[3] = v3;
        host_mask_t m;
        m.data.fill(true);
        h_addr.write(a); h_wdata.write(d); h_wmask.write(m); h_wren.write(true);
        wait();
        h_wren.write(false);
    }

    void drive()
    {
        std::mt19937_64 rng(7);
        rstn.write(false); req_en.write(true); bias_en.write(false); res_en.write(false); vmode.write(true);
        wait(); wait();
        rstn.write(true);
        // LUT: every lane written with the same table (like tb_fe_core_net), NCH = 24, per-channel scale/shift
        int8_t lut[256];
        for (int i = 0; i < 256; i++) lut[i] = static_cast<int8_t>((i * 37 + 11) & 0xFF);
        for (int lane = 0; lane < W; lane++)
            for (int e = 0; e < 256; e += 4) hw(0x00140000 + lane * 256 + e, lut[e], lut[e + 1], lut[e + 2], lut[e + 3]);
        const uint32_t NCH = 24;
        std::vector<uint32_t> sc(NCH);
        std::vector<int> sh(NCH);
        for (uint32_t c = 0; c < NCH; c++)
        {
            sc[c] = k.scale_fmt == U32 ? (0x80000000u | static_cast<uint32_t>(rng())) : (0x40000000u | (rng() & 0x3FFFFFFFu));
            sh[c] = 36 + static_cast<int>(rng() % 10);
            hw(0x00180000 + 4 * c, sc[c]);
            hw(0x00190000 + 4 * c, sh[c]);
        }
        hw(0x001A0004, NCH);
        // reload a DIFFERENT table (as the next layer does) and then the real one again: must not count as lane mismatch
        for (int lane = 0; lane < W; lane++)
            for (int e = 0; e < 256; e += 4) hw(0x00140000 + lane * 256 + e, -lut[e], -lut[e + 1], -lut[e + 2], -lut[e + 3]);
        for (int lane = 0; lane < W; lane++)
            for (int e = 0; e < 256; e += 4) hw(0x00140000 + lane * 256 + e, lut[e], lut[e + 1], lut[e + 2], lut[e + 3]);
        wait();
        // stream n_vec vectors back to back (1 per cycle), random addresses, random masks, LUT on for odd vectors
        for (uint64_t v = 0; v < n_vec; v++)
        {
            psum_vector_t<W, int32_t> in, exp(0);
            sramc_mask_t<W> m;
            const uint32_t addr = static_cast<uint32_t>(rng() % 100000);
            const bool lu = v & 1;
            for (int i = 0; i < W; i++)
            {
                in[i] = static_cast<int32_t>(static_cast<int32_t>(rng()) >> (rng() % 16));
                m[i] = (rng() % 8) != 0;
            }
            const uint32_t ch = addr % NCH;
            for (int i = 0; i < W; i++)
            {
                if (!m[i]) continue;
                int8_t q = requant(in[i], decode_scale(sc[ch], k.scale_fmt), sh[ch], 0, k);
                exp[i] = lu ? lut[q + 128] : q;
            }
            d_in.write(in); a_in.write(addr); m_in.write(m); v_in.write(true); lut_en.write(lu);
            sent.push_back({cyc, exp});
            sent_addr.push_back(addr);
            wait();
        }
        v_in.write(false);
        for (int i = 0; i < 20; i++) wait();
        sc_stop();
    }

    void monitor()
    {
        cyc++;
        if (!wren.read()) return;
        CHECK(got_n < sent.size(), "[D] extra output vector\n");
        if (got_n >= sent.size()) return;
        const auto &e = sent[got_n];
        const uint64_t lat = cyc - e.first;
        lat_min = std::min(lat_min, lat);
        lat_max = std::max(lat_max, lat);
        CHECK(a_out.read() == sent_addr[got_n], "[D] vector %llu address %u want %u\n", (unsigned long long)got_n,
              a_out.read(), sent_addr[got_n]);
        const auto &o = d_out.read();
        const auto &m = m_out.read();
        for (int i = 0; i < W; i++)
            if (m[i])
                CHECK(o[i] == e.second[i], "[D] vector %llu lane %d got %d want %d\n", (unsigned long long)got_n, i,
                      o[i], e.second[i]);
        got_n++;
    }
};

int sc_main(int argc, char **argv)
{
    test_rounding();
    test_compat_vs_m3(2000000);
    for (int i = 1; i < argc; i++) test_vector_file(argv[i]);

    const uint64_t N = 20000;
    const Knobs kd = Knobs::from_env();
    TbObp tb("tb", kd, N);
    sc_start();
    const int before = g_fail;
    CHECK(tb.got_n == N, "[D] received %llu of %llu vectors\n", (unsigned long long)tb.got_n, (unsigned long long)N);
    CHECK(tb.obp->cfg_errors == 0 && tb.obp->lut_lane_mismatch == 0, "[D] cfg_errors=%llu lut_lane_mismatch=%llu\n",
          (unsigned long long)tb.obp->cfg_errors, (unsigned long long)tb.obp->lut_lane_mismatch);
    CHECK(tb.lat_min == tb.lat_max, "[D] latency not fixed: %llu..%llu\n", (unsigned long long)tb.lat_min,
          (unsigned long long)tb.lat_max);
    std::printf("[D] HasObp (%s): %llu vectors, latency %llu cycles, 1 vector/cycle, sat16=%llu clamp8=%llu, "
                "scratch_param_reads=%llu: %s\n",
                kd.str().c_str(), (unsigned long long)tb.got_n, (unsigned long long)tb.lat_min,
                (unsigned long long)tb.obp->qc.sat16, (unsigned long long)tb.obp->qc.clamp8,
                (unsigned long long)tb.obp->scratch_param_reads, g_fail == before ? "PASS" : "FAIL");
    std::printf("RESULT: %s (failures %d)\n", g_fail ? "FAIL" : "PASS", g_fail);
    return g_fail ? 1 : 0;
}
