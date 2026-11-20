// tb_vert_scan_sa.cpp -- vertical result extraction in the model's systolic array (systolic_array/rtl_ref_sa_array.h).
// Two 32 x 32 arrays are driven side by side with the same inputs: H = horizontal scan (transpose_mode 0, the path every build uses),
// V = vertical extraction (transpose_mode argv[1] = 1 as delivered, 2 with the shadow register cleared at the last row). NCTX contexts of 4k MACs each
// (a[y] = y + 1, b[x] = 3x + 1 so a result matrix is NOT symmetric), cswitch of all columns once per period, then 32 scan edges.
// Sampling as the PSM does: at an edge where the scan enable is visible high the thread reads o_c_arr as written by the previous edge.
//   H sample s, lane y  = PE (y, s)            (right-to-left chain)
//   V sample r, lane c  = PE (r, c)            (row r of the completed context)
// So V[r][c] == H[c][r] is the transpose property.
// Expected (the measurements of the RTL):
//   mode 1: contexts 1, 2 transposed exactly; context k >= 3: V_k = H_k^T + V_(k-2)  (the finished context stays in the shadow register)
//   mode 2: every context exact
// Also prints a digest of every o_c_arr sample of the H array; built against the ORIGINAL header (no set_transpose_mode: compile without
// -DVERT_SCAN_HAS_MODE) and against the current header with mode 0 the digest must be identical (mode 0 = byte-identical behaviour).
// Build: g++ -std=c++17 -O2 -I <tree dir> [-DVERT_SCAN_HAS_MODE] tools/has/tb_vert_scan_sa.cpp -lsystemc -o tb_vert_scan_sa
#define SC_ALLOW_DEPRECATED_IEEE_API
#include <systemc.h>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include "control/rtl_ref_defaults.h"
#include "systolic_array/rtl_ref_sa_array.h"

using namespace sauria;
typedef sauria_rtl::SystolicArray<32, 32, int8_t, int8_t, int32_t> ArrayT;

static constexpr int NCTX = 6, PERIOD = 360, T_CSW = 220, T_SCAN0 = 300, NSCAN = 32, KBASE = 64;   // KBASE: every PE sees both operands (the wavefront skew is up to 62 cycles)

SC_MODULE(Tb)
{
    sc_in<bool> i_clk;
    sc_signal<bool> rstn, pipeline_en, cscan_en, sa_clear, softstall, pop_dbg, vscan;
    sc_signal<float> thr;
    sc_signal<act_vector_t<32, int8_t>> act;
    sc_signal<wei_vector_t<32, int8_t>> wei;
    sc_signal<psum_vector_t<32, int32_t>> c_in, h_out, v_out;
    sc_signal<sc_bv<32>> cswitch;
    sc_signal<uint32_t> ctx_id;
    ArrayT *H, *V;
    unsigned mode;

    SC_CTOR(Tb) : mode(0)
    {
        H = new ArrayT("H_std"); V = new ArrayT("V_std");
        for (ArrayT *a : {H, V})
        {
            a->i_clk(i_clk); a->i_rstn(rstn); a->i_threshold(thr); a->i_act_arr(act); a->i_wei_arr(wei); a->i_c_arr(c_in);
            a->i_pipeline_en(pipeline_en); a->i_cscan_en(cscan_en); a->i_cswitch_arr(cswitch); a->i_sa_clear(sa_clear);
            a->i_softstall(softstall); a->i_pop_en_dbg(pop_dbg); a->i_context_id(ctx_id);
        }
        H->o_c_arr(h_out); V->o_c_arr(v_out);
        SC_THREAD(run);
        sensitive << i_clk.pos();
    }
    static int32_t lane(const psum_vector_t<32, int32_t> &v, int i) { return v[i]; }
    void run()
    {
#ifdef VERT_SCAN_HAS_MODE
        V->set_transpose_mode(mode, &vscan);
#endif
        thr.write(0.5f); rstn.write(false); pipeline_en.write(true); cscan_en.write(false); sa_clear.write(false);
        softstall.write(false); pop_dbg.write(false); vscan.write(false); ctx_id.write(0); cswitch.write(sc_bv<32>(0));
        psum_vector_t<32, int32_t> zero; c_in.write(zero);
        wait(4); rstn.write(true); wait(2);
        std::vector<std::vector<std::vector<int32_t>>> Hm(NCTX + 1, std::vector<std::vector<int32_t>>(NSCAN, std::vector<int32_t>(32))), Vm = Hm;
        uint64_t digest = 1469598103934665603ull;
        const int total = NCTX * PERIOD + 8;
        // schedule value for the cycle the SA process sees at edge n: written by this thread at edge n - 1
        for (int n = 0; n < total; n++)
        {
            // sample first (output written by the previous edge), then drive the controls the SA sees at edge n + 1
            const int p = n / PERIOD, t = n % PERIOD;   // p-th context period, t within it
            const bool scan_visible_now = (t >= T_SCAN0 && t < T_SCAN0 + NSCAN);
            psum_vector_t<32, int32_t> ho = h_out.read(), vo = v_out.read();
            for (int i = 0; i < 32; i++) { digest ^= (uint32_t)lane(ho, i); digest *= 1099511628211ull; }
            if (scan_visible_now && p < NCTX)
            {
                const int s = t - T_SCAN0;
                for (int i = 0; i < 32; i++) { Hm[p + 1][s][i] = lane(ho, i); Vm[p + 1][s][i] = lane(vo, i); }
            }
            const int tn = (n + 1) % PERIOD, pn = (n + 1) / PERIOD;
            act_vector_t<32, int8_t> a; wei_vector_t<32, int8_t> b;
            const int kmacs = KBASE + 4 * (pn + 1);        // context pn + 1 has KBASE + 4(pn + 1) MAC cycles
            const bool feed = pn < NCTX && tn < kmacs;
            for (int y = 0; y < 32; y++) a[y] = feed ? (int8_t)(y + 1) : (int8_t)0;
            for (int x = 0; x < 32; x++) b[x] = feed ? (int8_t)(3 * x + 1) : (int8_t)0;
            act.write(a); wei.write(b);
            cswitch.write(sc_bv<32>(pn < NCTX && tn == T_CSW ? 0xFFFFFFFFu : 0u));
            cscan_en.write(pn < NCTX && tn >= T_SCAN0 && tn < T_SCAN0 + NSCAN);
            // vertical flag: from two edges before the first scan edge (row 0 presented before it) until the last scan edge
            vscan.write(pn < NCTX && tn >= T_SCAN0 - 2 && tn < T_SCAN0 + NSCAN);
            ctx_id.write((uint32_t)pn);
            wait();
        }
        // evaluation
        int bad_total = 0;
        std::printf("vert-scan sa: mode %u digest %016llx\n", mode, (unsigned long long)digest);
#ifdef VERT_SCAN_HAS_MODE
        for (int k = 1; k <= NCTX; k++)
        {
            int nz = 0, mis_t = 0, mis_formula = 0, hsum_nonzero = 0;
            for (int r = 0; r < NSCAN; r++) for (int c = 0; c < 32; c++)
            {
                const int32_t v = Vm[k][r][c], h = Hm[k][c][r];
                if (h != 0) hsum_nonzero++;
                if (v != h) mis_t++;
                const int32_t prev = k >= 3 ? Vm[k - 2][r][c] : 0;
                if (v != h + prev) mis_formula++;
                if (v != 0) nz++;
            }
            // expectation: mode 1 -> k <= 2 exact, k >= 3 formula V = H^T + V_(k-2); mode 2 -> exact
            const bool exact = (mis_t == 0);
            const bool want_exact = (mode >= 2) || (k <= 2);
            bool ok = want_exact ? exact : (mis_formula == 0 && mis_t > 0);
            std::printf("vert-scan sa: ctx %d mode %u: H nonzero %d, V != H^T at %d of 1024, V != H^T + V(k-2) at %d  %s\n", k, mode, hsum_nonzero, mis_t,
                        mis_formula, ok ? "ok" : "MISMATCH vs prediction");
            if (!ok) bad_total++;
        }
        std::printf("vert-scan sa: mode %u: RESULT %s\n", mode, bad_total ? "FAIL" : "PASS");
#endif
        sc_stop();
    }
};

int sc_main(int argc, char **argv)
{
    sc_clock clk("clk", 10, SC_NS);
    Tb tb("tb");
    tb.mode = argc > 1 ? (unsigned)std::atoi(argv[1]) : 0;
    tb.i_clk(clk);
    sc_start();
    return 0;
}
