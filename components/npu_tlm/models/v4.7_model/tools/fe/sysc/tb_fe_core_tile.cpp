// Real frontend tiles through the v4.5 port of
// the SAURIA core (RtlRefLaneACoreA + rtl_ref_sa_array), not through a functional stand-in.
//
// One job dir per tile, written by tools/fe/fe_core_jobs.py + tools/fe/fe_core_bins.py:
//   shape.txt        kw kh d s Cin w h k X_used Y_used A_H A_W
//   A.bin            int8  [Cin, A_H, A_W]   host-padded input window
//   B.bin            int8  [k, Cin, kh, kw]  tile weights
//   bias.bin         int32 [k]               PSUM preload value per channel (HAS 6.9)
//   expect_psum.bin  int32 [k, h, w]         bias + sum(B*A): what the core's accumulators must contain
// The DRAM image and the A/B/C region offsets come from driver/libsauria_mem.h (sauria_assemble_dram), the core
// configuration from driver/libsauria_cfg.h (sauria_compute_core_fields) — same path as
// tools/test_sauria_ref_layer_runner.cpp, whose wiring this file reuses. Nothing shared is modified.
//
// The comparison taps the PSM -> SRAM-C bus (o_sramc_wdata/addr/wmask of the core) so it sees the raw accumulators
// before the OBP: element index = addr * 32 + lane, C-order [k, h, w] (CKSTEP = w*h).
//
// Usage (repository root, FE_WORK set): fe_work/core/tb_fe_core_tile <jobs.tsv> [max_cycles_per_tile]
// Build: bash tools/fe/sysc/build_tb_fe_core_tile.sh
#define SC_ALLOW_DEPRECATED_IEEE_API

#include <systemc.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "control/native_lane_a_core.h"
#include "sram/rtl_ref_sram_top.h"
#include "systolic_array/rtl_ref_sa_array.h"
#include "driver/libsauria_cfg.h"
#include "driver/libsauria_mem.h"
#include "sauria_targets.h"

using namespace sauria;

// The core path uses sauria_model's SRAM (sram/rtl_ref_sram_top.h, single Lane A, with the
// FX1_A3_SRAMA/B_RDEN_PHASE pair) instead of v4.5's two-lane sram/sram_top.h. read_bank_data() (a testbench
// backdoor, not hardware logic) must read the buffer the NPU is writing, i.e. follow the current i_select
// (sram_select = 0 -> npu_idx = 1), not always buffer [0].
// Capacities are passed in BYTES per buffer: 5056*32 / 5184*32 are both ping-pong buffers together and Sram
// computes ROWS per buffer, so each buffer gets 79 / 81 KB as in the HAS (same as tb_fe_core_net.cpp).
static constexpr int kSramaBytes = 79 * 1024;
static constexpr int kSrambBytes = 81 * 1024;
typedef sauria_rtl::Sram<32, 32, int8_t, int8_t, int32_t, kSramaBytes, kSrambBytes, 1536> SramT;
typedef sauria_rtl::SystolicArray<32, 32, int8_t, int8_t, int32_t> ArrayT;
typedef RtlRefLaneACoreA<32, 32, int8_t, int8_t, int32_t, 16, 64, 1, 1536, kSramaBytes, kSrambBytes> RtlRefCoreT;

struct Job
{
    std::string name, dir;
    int kw, kh, d, s, cin, w, h, k, x_used, y_used, a_h, a_w;
};

static std::vector<char> read_file(const std::string &path)
{
    std::ifstream f(path, std::ios::binary);
    if (!f)
    {
        std::cout << "cannot open " << path << std::endl;
        std::exit(2);
    }
    return std::vector<char>((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

SC_MODULE(TbFeCoreTile)
{
    sc_in<bool> i_clk;

    sc_signal<bool> sram_rstn, sram_deepsleep, sram_powergate;
    sc_signal<sc_bv<3>> sram_select;
    sc_signal<uint32_t> host_addr;
    sc_signal<bool> host_wren, host_rden;
    sc_signal<host_data_t> host_wdata, host_rdata;
    sc_signal<host_mask_t> host_wmask;
    sc_signal<uint32_t> srama_addr_a, sramb_addr_a;
    sc_signal<bool> srama_rden_a, sramb_rden_a;
    sc_signal<act_vector_t<32, int8_t>> srama_data_a;
    sc_signal<wei_vector_t<32, int8_t>> sramb_data_a;
    sc_signal<psum_vector_t<32, int32_t>> s_sramc_wdata_a, sramc_rdata_a;
    sc_signal<uint32_t> s_sramc_addr_a;
    sc_signal<bool> s_sramc_wren_a, sramc_rden_a;
    sc_signal<sramc_mask_t<32>> s_sramc_wmask_a;

    sc_signal<bool> arr_rstn;
    sc_signal<act_vector_t<32, int8_t>> s_act_arr;
    sc_signal<wei_vector_t<32, int8_t>> s_wei_arr;
    sc_signal<psum_vector_t<32, int32_t>> s_sa_to_psm_c, s_psm_to_sa_c;
    sc_signal<bool> arr_softstall_zero, arr_pop_en_dbg_zero;
    sc_signal<float> arr_threshold;

    sc_signal<bool> core_rstn, core_soft_reset, core_start;
    sc_signal<uint32_t> core_mvm_k, core_total_contexts, core_nsplit;
    sc_signal<uint32_t> core_incntlim, core_act_reps, core_wei_reps, core_out_ncontexts;
    sc_signal<uint32_t> core_act_incntlim, core_act_incntstep, core_act_outcntlim, core_act_outcntstep;
    sc_signal<sc_bv<64>> core_act_dil_pat;
    sc_signal<uint32_t> core_act_xlim, core_act_xstep, core_act_ylim, core_act_ystep;
    sc_signal<uint32_t> core_act_chlim, core_act_chstep;
    sc_signal<uint32_t> core_act_til_xlim, core_act_til_xstep, core_act_til_ylim, core_act_til_ystep;
    sc_signal<uint32_t> core_act_base_addr;
    sc_signal<uint32_t> core_wei_incntlim, core_wei_incntstep, core_wei_wlim, core_wei_wstep;
    sc_signal<uint32_t> core_wei_klim, core_wei_kstep, core_wei_til_klim, core_wei_til_kstep;
    sc_signal<uint32_t> core_wei_cols_active, core_wei_waligned, core_wei_base_addr;
    sc_signal<uint32_t> core_cxlim, core_cxstep, core_cklim, core_ckstep;
    sc_signal<uint32_t> core_out_til_cylim, core_out_til_cystep, core_out_til_cklim, core_out_til_ckstep;
    sc_signal<bool> core_out_preload_en;
    sc_signal<uint32_t> core_out_base_addr;
    sc_signal<sramc_mask_t<32>> core_rows_active;
    sc_signal<uint32_t> core_inactive_cols;
    sc_signal<bool> core_pipeline_en, core_cscan_en, core_sa_clear;
    sc_signal<sc_bv<32>> core_cswitch_arr;
    sc_signal<uint32_t> core_context_id;
    sc_signal<bool> core_done, core_deadlock, core_active;

    SramT *sram;
    ArrayT *array_inst;
    RtlRefCoreT *core;

    std::vector<Job> jobs;
    int max_cycles = 2000000;
    int failures = 0;

    SC_CTOR(TbFeCoreTile)
    {
        sram = new SramT("sram");
        sram->i_clk(i_clk);
        sram->i_rstn(sram_rstn);
        sram->i_deepsleep(sram_deepsleep);
        sram->i_powergate(sram_powergate);
        sram->i_select(sram_select);
        sram->i_host_addr(host_addr);
        sram->i_host_wren(host_wren);
        sram->i_host_rden(host_rden);
        sram->i_host_wdata(host_wdata);
        sram->i_host_wmask(host_wmask);
        sram->o_host_rdata(host_rdata);
        sram->i_srama_addr(srama_addr_a); sram->i_srama_rden(srama_rden_a); sram->o_srama_data(srama_data_a);
        sram->i_sramb_addr(sramb_addr_a); sram->i_sramb_rden(sramb_rden_a); sram->o_sramb_data(sramb_data_a);
        sram->i_sramc_wdata(s_sramc_wdata_a); sram->i_sramc_addr(s_sramc_addr_a); sram->i_sramc_wren(s_sramc_wren_a);
        sram->i_sramc_rden(sramc_rden_a); sram->i_sramc_wmask(s_sramc_wmask_a); sram->o_sramc_rdata(sramc_rdata_a);

        array_inst = new ArrayT("array_inst");
        array_inst->i_clk(i_clk);
        array_inst->i_rstn(arr_rstn);
        array_inst->i_threshold(arr_threshold);
        array_inst->i_act_arr(s_act_arr);
        array_inst->i_wei_arr(s_wei_arr);
        array_inst->i_c_arr(s_psm_to_sa_c);
        array_inst->o_c_arr(s_sa_to_psm_c);
        array_inst->i_pipeline_en(core_pipeline_en);
        array_inst->i_cscan_en(core_cscan_en);
        array_inst->i_cswitch_arr(core_cswitch_arr);
        array_inst->i_sa_clear(core_sa_clear);
        array_inst->i_softstall(arr_softstall_zero);
        array_inst->i_pop_en_dbg(arr_pop_en_dbg_zero);
        array_inst->i_context_id(core_context_id);

        // Instance name contains "NpuTop_std" so the ported feeder's FX1_A3_FEED_TAPE trace (which filters on that
        // name, rtl_ref_ifmap_feeder.h:790) also fires here and can be diffed against sauria_model's tape.
        core = new RtlRefCoreT("NpuTop_std_core");
        core->i_clk(i_clk);
        core->i_rstn(core_rstn);
        core->i_soft_reset(core_soft_reset);
        core->i_start(core_start);
        core->i_mvm_k(core_mvm_k);
        core->i_total_contexts(core_total_contexts);
        core->i_nsplit(core_nsplit);
        core->i_incntlim(core_incntlim);
        core->i_act_reps(core_act_reps);
        core->i_wei_reps(core_wei_reps);
        core->i_out_ncontexts(core_out_ncontexts);
        core->i_act_incntlim(core_act_incntlim);
        core->i_act_incntstep(core_act_incntstep);
        core->i_act_outcntlim(core_act_outcntlim);
        core->i_act_outcntstep(core_act_outcntstep);
        core->i_act_dil_pat(core_act_dil_pat);
        core->i_act_xlim(core_act_xlim);
        core->i_act_xstep(core_act_xstep);
        core->i_act_ylim(core_act_ylim);
        core->i_act_ystep(core_act_ystep);
        core->i_act_chlim(core_act_chlim);
        core->i_act_chstep(core_act_chstep);
        core->i_act_til_xlim(core_act_til_xlim);
        core->i_act_til_xstep(core_act_til_xstep);
        core->i_act_til_ylim(core_act_til_ylim);
        core->i_act_til_ystep(core_act_til_ystep);
        core->i_act_base_addr(core_act_base_addr);
        core->i_wei_incntlim(core_wei_incntlim);
        core->i_wei_incntstep(core_wei_incntstep);
        core->i_wei_wlim(core_wei_wlim);
        core->i_wei_wstep(core_wei_wstep);
        core->i_wei_klim(core_wei_klim);
        core->i_wei_kstep(core_wei_kstep);
        core->i_wei_til_klim(core_wei_til_klim);
        core->i_wei_til_kstep(core_wei_til_kstep);
        core->i_wei_cols_active(core_wei_cols_active);
        core->i_wei_waligned(core_wei_waligned);
        core->i_wei_base_addr(core_wei_base_addr);
        core->i_cxlim(core_cxlim);
        core->i_cxstep(core_cxstep);
        core->i_cklim(core_cklim);
        core->i_ckstep(core_ckstep);
        core->i_out_til_cylim(core_out_til_cylim);
        core->i_out_til_cystep(core_out_til_cystep);
        core->i_out_til_cklim(core_out_til_cklim);
        core->i_out_til_ckstep(core_out_til_ckstep);
        core->i_out_preload_en(core_out_preload_en);
        core->i_out_base_addr(core_out_base_addr);
        core->i_rows_active(core_rows_active);
#ifdef FX1_A3_PSM_INACTIVE_COLS
        core->i_inactive_cols(core_inactive_cols);
#endif
        core->o_srama_addr(srama_addr_a);
        core->o_srama_rden(srama_rden_a);
        core->i_srama_data(srama_data_a);
        core->o_sramb_addr(sramb_addr_a);
        core->o_sramb_rden(sramb_rden_a);
        core->i_sramb_data(sramb_data_a);
        core->o_sramc_addr(s_sramc_addr_a);
        core->o_sramc_wren(s_sramc_wren_a);
        core->o_sramc_rden(sramc_rden_a);
        core->o_sramc_wmask(s_sramc_wmask_a);
        core->o_sramc_wdata(s_sramc_wdata_a);
        core->i_sramc_rdata(sramc_rdata_a);
        core->o_act_arr(s_act_arr);
        core->o_wei_arr(s_wei_arr);
        core->i_c_arr(s_sa_to_psm_c);
        core->o_c_arr(s_psm_to_sa_c);
        core->o_pipeline_en(core_pipeline_en);
        core->o_cscan_en(core_cscan_en);
        core->o_cswitch_arr(core_cswitch_arr);
        core->o_sa_clear(core_sa_clear);
        core->o_context_id(core_context_id);
        core->o_done(core_done);
        core->o_deadlock(core_deadlock);
        core->o_active(core_active);

        SC_THREAD(run);
        sensitive << i_clk.pos();
    }

    void tick(int n = 1) { for (int i = 0; i < n; i++) wait(); }

    // Same mapping as tools/test_sauria_ref_layer_runner.cpp::apply_layer_cfg (ports outside F_CFG_* documented there).
    void apply_layer_cfg(const SauriaLayerDesc &desc, const SauriaTarget &target)
    {
        uint64_t f[F_CFG_COUNT];
        sauria_compute_core_fields(desc, target, f);
        core_incntlim.write((uint32_t)f[F_CFG_INCNTLIM] + (getenv("FE_CON_INCNTLIM_PLUS1") ? 1 : 0));
        core_act_reps.write((uint32_t)f[F_CFG_ACT_REPS]);
        core_wei_reps.write((uint32_t)f[F_CFG_WEI_REPS]);
        // NOT the same as tools/test_sauria_ref_layer_runner.cpp, which fills these by hand with values that only hold
        // for its 1x1/k_til=1 case. Values below follow the reference decode of sauria_model (the "[APPLY CFG]" lines of
        // its run.log for the same shape): ACT.INCNTLIM = K (= CON.INCNTLIM + 1), INCNTSTEP = SRAM word width,
        // ACT.OUTCNTLIM = XLIM, WEI.INCNTLIM = WLIM.
        core_act_incntlim.write((uint32_t)f[F_CFG_INCNTLIM] + 1);
        core_act_incntstep.write((uint32_t)target.Y);
        core_act_outcntlim.write((uint32_t)f[F_CFG_XLIM]);
        core_act_outcntstep.write((uint32_t)target.Y);
        core_act_dil_pat.write(sc_bv<64>((uint64_t)f[F_CFG_DIL_PAT]));
        core_act_xlim.write((uint32_t)f[F_CFG_XLIM]);
        core_act_xstep.write((uint32_t)f[F_CFG_XSTEP]);
        core_act_ylim.write((uint32_t)f[F_CFG_YLIM]);
        core_act_ystep.write((uint32_t)f[F_CFG_YSTEP]);
        core_act_chlim.write((uint32_t)f[F_CFG_CHLIM]);
        core_act_chstep.write((uint32_t)f[F_CFG_CHSTEP]);
        core_act_til_xlim.write((uint32_t)f[F_CFG_TIL_XLIM]);
        core_act_til_xstep.write((uint32_t)f[F_CFG_TIL_XSTEP]);
        core_act_til_ylim.write((uint32_t)f[F_CFG_TIL_YLIM]);
        core_act_til_ystep.write((uint32_t)f[F_CFG_TIL_YSTEP]);
        core_act_base_addr.write(0);
        core_wei_incntlim.write((uint32_t)f[F_CFG_WLIM]);
        core_wei_incntstep.write((uint32_t)target.X);
        core_wei_wlim.write((uint32_t)f[F_CFG_WLIM]);
        core_wei_wstep.write((uint32_t)f[F_CFG_WSTEP]);
        core_wei_klim.write((uint32_t)f[F_CFG_KLIM]);
        core_wei_kstep.write((uint32_t)f[F_CFG_KSTEP]);
        core_wei_til_klim.write((uint32_t)f[F_CFG_TIL_KLIM]);
        core_wei_til_kstep.write((uint32_t)f[F_CFG_TIL_KSTEP]);
        core_wei_cols_active.write((uint32_t)f[F_CFG_COLS_ACTIVE]);
        core_wei_waligned.write((uint32_t)f[F_CFG_WALIGNED]);
        core_wei_base_addr.write(0);
        core_out_ncontexts.write((uint32_t)f[F_CFG_NCONTEXTS]);
        core_total_contexts.write((uint32_t)f[F_CFG_NCONTEXTS]);
        core_cxlim.write((uint32_t)f[F_CFG_CXLIM]);
        core_cxstep.write((uint32_t)f[F_CFG_CXSTEP]);
        core_cklim.write((uint32_t)f[F_CFG_CKLIM]);
        core_ckstep.write((uint32_t)f[F_CFG_CKSTEP]);
        core_out_til_cylim.write((uint32_t)f[F_CFG_TIL_CYLIM]);
        core_out_til_cystep.write((uint32_t)f[F_CFG_TIL_CYSTEP]);
        core_out_til_cklim.write((uint32_t)f[F_CFG_TIL_CKLIM]);
        core_out_til_ckstep.write((uint32_t)f[F_CFG_TIL_CKSTEP]);
        core_out_base_addr.write(0);
        {
            sramc_mask_t<32> ra(false);
            uint64_t bits = f[F_CFG_ROWS_ACTIVE];
            for (int i = 0; i < 32; i++) ra[i] = ((bits >> i) & 1ULL) != 0;
            core_rows_active.write(ra);
        }
        core_mvm_k.write(1);
        core_nsplit.write(32);
        core_out_preload_en.write(true);
#ifdef FX1_A3_PSM_INACTIVE_COLS
        // Inactive PSM columns = X - X_used, as sauria_model's config_regs computes o_out_inactive_cols (host
        // poked, not derived from other registers). Without it the PSM never shifts all preload columns when
        // X_used < X, and every such tile fails.
        core_inactive_cols.write((uint32_t)target.X - (uint32_t)desc.X_used);
#endif
    }

    void run_job(const Job &j)
    {
        const SauriaTarget *t = sauria_find_target("int8_32x32");
        std::vector<char> a8 = read_file(j.dir + "/A.bin");
        std::vector<char> b8 = read_file(j.dir + "/B.bin");
        std::vector<char> bias_raw = read_file(j.dir + "/bias.bin");
        std::vector<char> exp_raw = read_file(j.dir + "/expect_psum.bin");
        const int32_t *bias = reinterpret_cast<const int32_t *>(bias_raw.data());
        const int32_t *expect = reinterpret_cast<const int32_t *>(exp_raw.data());
        size_t n_out = (size_t)j.k * j.h * j.w;

        // DRAM image through the real packer: A [Cin, A_H, A_W], B [k, Cin, kh, kw], C preload [k, h, w] = bias
        // FE_ZERO_PRELOAD=1 isolates the multiply-accumulate path from the preload path: preload 0, expected = sum(B*A)
        const bool zero_pre = getenv("FE_ZERO_PRELOAD") != nullptr;
        std::vector<double> A(a8.size()), B(b8.size()), C(n_out);
        for (size_t i = 0; i < a8.size(); i++) A[i] = (double)(int8_t)a8[i];
        for (size_t i = 0; i < b8.size(); i++) B[i] = (double)(int8_t)b8[i];
        for (int c = 0; c < j.k; c++)
            for (int p = 0; p < j.h * j.w; p++) C[(size_t)c * j.h * j.w + p] = zero_pre ? 0.0 : (double)bias[c];
        SauriaDramLayout L = sauria_assemble_dram(A.data(), j.cin, j.a_h, j.a_w,
                                                 B.data(), j.k, j.cin, j.kh, j.kw,
                                                 C.data(), j.k, j.h, j.w, j.cin, j.k, *t);

        if (getenv("FE_DUMP_DRAM"))
        {
            std::ofstream df(j.dir + "/dram_cxx.bin", std::ios::binary);
            df.write((const char *)L.dram.data(), (std::streamsize)L.dram.size());
            std::printf("  [dram] %s A_off=%u B_off=%u C_off=%u size=%zu\n", j.name.c_str(), L.A_off, L.B_off, L.C_off,
                        L.dram.size());
        }
        // reset, then load the DRAM regions into SRAM banks: act -> bank 2, wei -> bank 0, preload -> bank 4
        sram_rstn.write(false); arr_rstn.write(false); core_rstn.write(false); core_soft_reset.write(false);
        core_start.write(false); sram_deepsleep.write(false); sram_powergate.write(false); sram_select.write(0);
        host_wren.write(false); host_rden.write(false); arr_threshold.write(0.0f);
        arr_softstall_zero.write(false); arr_pop_en_dbg_zero.write(false);
        tick(5);
        sram_rstn.write(true); arr_rstn.write(true); core_rstn.write(true);
        tick(5);
        sram->write_bank_data(2, 0, (const uint8_t *)L.dram.data() + L.A_off, (size_t)(L.B_off - L.A_off));
        sram->write_bank_data(0, 0, (const uint8_t *)L.dram.data() + L.B_off, (size_t)(L.C_off - L.B_off));
        sram->write_bank_data(4, 0, (const uint8_t *)L.dram.data() + L.C_off, L.dram.size() - (size_t)L.C_off);

        SauriaLayerDesc desc{};
        desc.B_w = j.kw; desc.B_h = j.kh; desc.d = j.d; desc.s = j.s;
        desc.c_til = j.cin; desc.k_til = j.k; desc.h_til = j.h; desc.w_til = j.w;
        desc.X_used = j.x_used; desc.Y_used = j.y_used; desc.preload_en = 1;
        apply_layer_cfg(desc, *t);

        // capture the PSM -> SRAM-C writes (raw accumulators, before the OBP)
        std::vector<int32_t> got(n_out, 0);
        std::vector<char> seen(n_out, 0);
        core_start.write(true);
        tick();
        core_start.write(false);
        // The core keeps cycling contexts past o_out_ncontexts and never raises o_done in this wiring (observed:
        // "CONTEXT START global_context=13 / 12"), so stop once every output element has been written plus a short
        // drain, and keep max_cycles as the backstop.
        int cyc = 0, drain = -1, n_writes_dumped = 0, n_feed_dumped = 0, feed_rows = 0;
        FILE *feed_csv = nullptr;
        if (getenv("FE_FEED_CSV"))
        {
            feed_csv = std::fopen((j.dir + "/port_feed.csv").c_str(), "w");
            std::fprintf(feed_csv, "count,context");
            for (int q = 0; q < 32; q++) std::fprintf(feed_csv, ",act%d", q);
            for (int q = 0; q < 32; q++) std::fprintf(feed_csv, ",wei%d", q);
            std::fprintf(feed_csv, "\n");
        }
        size_t n_seen = 0;
        bool deadlock = false;
        while (cyc < max_cycles)
        {
            tick();
            cyc++;
            if (s_sramc_wren_a.read())
            {
                uint32_t addr = s_sramc_addr_a.read();
                psum_vector_t<32, int32_t> v = s_sramc_wdata_a.read();
                sramc_mask_t<32> m = s_sramc_wmask_a.read();
                for (int lane = 0; lane < 32; lane++)
                {
                    if (!m[lane]) continue;
                    size_t e = (size_t)addr * 32 + lane;
                    if (getenv("FE_TRACE_WR") && n_writes_dumped < 40)
                    {
                        std::printf("  [wr] cyc=%d addr=%u lane=%d e=%zu val=%d\n", cyc, addr, lane, e, (int)v[lane]);
                        n_writes_dumped++;
                    }
                    if (e < n_out)
                    {
                        if (!seen[e]) n_seen++;
                        got[e] = v[lane];
                        seen[e] = 1;
                    }
                }
            }
            // Same gate as sauria_model npu_top.h's sa_input_stream.csv (pipeline_en && act_pop_en && wei_pop_en), so the
            // two CSVs can be compared row by row.
            if (feed_csv && core_pipeline_en.read() && core->dbg_act_pop_en() && core->dbg_wei_pop_en())
            {
                act_vector_t<32, int8_t> av = s_act_arr.read();
                wei_vector_t<32, int8_t> wv = s_wei_arr.read();
                std::fprintf(feed_csv, "%d,%u", feed_rows++, core_context_id.read());
                for (int q = 0; q < 32; q++) std::fprintf(feed_csv, ",%d", (int)av[q]);
                for (int q = 0; q < 32; q++) std::fprintf(feed_csv, ",%d", (int)wv[q]);
                std::fprintf(feed_csv, "\n");
            }
            if (getenv("FE_TRACE_FEED") && n_feed_dumped < atoi(getenv("FE_TRACE_FEED")))
            {
                act_vector_t<32, int8_t> av = s_act_arr.read();
                wei_vector_t<32, int8_t> wv = s_wei_arr.read();
                bool any = false;
                for (int q = 0; q < 32; q++) any = any || av[q] != 0 || wv[q] != 0;
                if (!any) goto no_feed_dump;
                n_feed_dumped++;
                std::printf("  [feed] cyc=%d pipe=%d cscan=%d clear=%d ctx=%u act=%d,%d,%d,%d wei=%d,%d,%d,%d\n", cyc,
                            (int)core_pipeline_en.read(), (int)core_cscan_en.read(), (int)core_sa_clear.read(),
                            core_context_id.read(), (int)av[0], (int)av[1], (int)av[2], (int)av[3],
                            (int)wv[0], (int)wv[1], (int)wv[2], (int)wv[3]);
            }
        no_feed_dump:;
            if (core_deadlock.read()) { deadlock = true; break; }
            if (drain < 0 && (n_seen == n_out || core_done.read())) drain = 64;
            if (drain > 0 && --drain == 0) break;
        }
        bool timeout = (cyc >= max_cycles);
        if (feed_csv) std::fclose(feed_csv);
        // The bus capture above only tells us WHICH elements were written (the data vector is pipelined per lane, so
        // its content at the write cycle does not belong to that lane). SRAM-C itself is the authority: read it back.
        {
            std::vector<uint8_t> raw(n_out * 4, 0);
            sram->read_bank_data(4, 0, raw.data(), raw.size());
            std::memcpy(got.data(), raw.data(), raw.size());
        }
        size_t bad = 0, missing = 0;
        for (size_t e = 0; e < n_out; e++)
        {
            int32_t want = zero_pre ? (int32_t)(expect[e] - bias[e / ((size_t)j.h * j.w)]) : expect[e];
            if (!seen[e]) missing++;
            else if (got[e] != want)
            {
                if (getenv("FE_DUMP") && bad < 8)
                    std::printf("  [dump] e=%zu (k=%zu y=%zu x=%zu) got=%d want=%d\n", e,
                                e / ((size_t)j.h * j.w), (e / j.w) % j.h, e % j.w, got[e], want);
                bad++;
            }
        }
        if (getenv("FE_DUMP_FULL"))
        {
            // FE_DUMP_FULL: write all got[] / want[] for an offline permutation / garbage analysis (does not
            // change the pass/fail logic above). One line per element: e got want.
            std::ofstream df(j.dir + "/dump_full.txt");
            for (size_t e = 0; e < n_out; e++)
            {
                int32_t want = zero_pre ? (int32_t)(expect[e] - bias[e / ((size_t)j.h * j.w)]) : expect[e];
                df << e << " " << got[e] << " " << want << "\n";
            }
        }
        bool pass = (bad == 0 && missing == 0 && !deadlock && !timeout);
        if (!pass) failures++;
        std::printf("%s\t%s\tmismatch=%zu\tmissing=%zu\telems=%zu\tcycles=%d\tdeadlock=%d\ttimeout=%d\tXu=%d\tYu=%d\n",
                    j.name.c_str(), pass ? "PASS" : "FAIL", bad, missing, n_out, cyc, (int)deadlock, (int)timeout,
                    j.x_used, j.y_used);
        std::fflush(stdout);
    }

    void run()
    {
        for (const Job &j : jobs) run_job(j);
        std::printf("[tb_fe_core_tile] RESULT: %s (jobs %zu, failures %d)\n",
                    failures == 0 ? "PASS" : "FAIL", jobs.size(), failures);
        sc_stop();
    }
};

int sc_main(int argc, char *argv[])
{
    if (argc < 2)
    {
        std::cout << "usage: tb_fe_core_tile <jobs.tsv> [max_cycles_per_tile]" << std::endl;
        return 2;
    }
    sc_clock clk("clk", 10, SC_NS);
    TbFeCoreTile tb("tb");
    tb.i_clk(clk);
    if (argc > 2) tb.max_cycles = std::atoi(argv[2]);

    std::ifstream f(argv[1]);
    std::string line;
    while (std::getline(f, line))
    {
        if (line.empty()) continue;
        std::string name, gargs, tile, dir;
        std::istringstream ls(line);
        std::getline(ls, name, '\t'); std::getline(ls, gargs, '\t');
        std::getline(ls, tile, '\t'); std::getline(ls, dir, '\t');
        std::ifstream sf(dir + "/shape.txt");
        Job j{};
        j.name = name; j.dir = dir;
        if (!(sf >> j.kw >> j.kh >> j.d >> j.s >> j.cin >> j.w >> j.h >> j.k >> j.x_used >> j.y_used >> j.a_h >> j.a_w))
        {
            std::cout << "cannot read " << dir << "/shape.txt (run tools/fe/fe_core_bins.py)" << std::endl;
            return 2;
        }
        tb.jobs.push_back(j);
    }
    if (tb.jobs.empty())
    {
        std::cout << "no jobs in " << argv[1] << std::endl;
        return 2;
    }
    sc_start();
    return tb.failures == 0 ? 0 : 1;
}
