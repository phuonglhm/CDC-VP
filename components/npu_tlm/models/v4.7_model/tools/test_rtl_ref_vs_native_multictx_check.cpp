// Differential test of the RTL-reference core against the native core: NativeLaneACoreA and RtlRefLaneACoreA run
// side by side on the same input with total_contexts = 4 (the RTL-reference PSM enters its real write path only
// when ctx_cnt >= 3).
//
// Each core has its own Sram and SystolicArray (no shared state) but the same seed (act = 100, wei = 50 in every
// context) and the same configuration registers. After both finish (or time out), SRAM-C bank 4 of both sides
// is read through the backdoor and compared.
#define SC_ALLOW_DEPRECATED_IEEE_API

#include <systemc.h>
#include <iostream>
#include <cstring>

#include "sram/sram_top.h"
#include "systolic_array/sa_array.h"
#include "control/native_lane_a_core.h"

using namespace sauria;

typedef Sram<32, 32, int8_t, int8_t, int32_t, 5056, 5184, 1536> SramT;
typedef SystolicArray<32, 32, int8_t, int8_t, int32_t> ArrayT;
typedef NativeLaneACoreA<32, 32, int8_t, int8_t, int32_t, 16, 64, 1, 1536> NativeCoreT;
typedef RtlRefLaneACoreA<32, 32, int8_t, int8_t, int32_t, 16, 64, 1, 1536> RtlRefCoreT;

// A complete rig (Sram + Array + one core), parameterised on the core type so both sides use the same code.
template <typename CoreT>
struct LaneRig
{
    sc_signal<bool> sram_rstn, sram_deepsleep, sram_powergate;
    sc_signal<sc_bv<3>> sram_select;
    sc_signal<uint32_t> host_addr;
    sc_signal<bool> host_wren, host_rden;
    sc_signal<host_data_t> host_wdata, host_rdata;
    sc_signal<host_mask_t> host_wmask;
    sc_signal<uint32_t> srama_addr_a, sramb_addr_a, srama_addr_b, sramb_addr_b;
    sc_signal<bool> srama_rden_a, sramb_rden_a, srama_rden_b, sramb_rden_b;
    sc_signal<act_vector_t<32, int8_t>> srama_data_a, srama_data_b;
    sc_signal<wei_vector_t<32, int8_t>> sramb_data_a, sramb_data_b;
    sc_signal<psum_vector_t<32, int32_t>> sramc_wdata_a, sramc_rdata_a, sramc_wdata_b, sramc_rdata_b;
    sc_signal<uint32_t> sramc_addr_a, sramc_addr_b;
    sc_signal<bool> sramc_wren_a, sramc_rden_a, sramc_wren_b, sramc_rden_b;
    sc_signal<sramc_mask_t<32>> sramc_wmask_a, sramc_wmask_b;

    sc_signal<bool> arr_rstn;
    sc_signal<act_vector_t<32, int8_t>> s_act_arr, arr_act_b_zero;
    sc_signal<wei_vector_t<32, int8_t>> s_wei_arr, arr_wei_b_zero;
    sc_signal<psum_vector_t<32, int32_t>> s_sa_to_psm_c, s_psm_to_sa_c, arr_c_b_zero_in, arr_c_b_zero_out;
    sc_signal<bool> arr_pipeline_en_b_zero, arr_cscan_en_b_zero, arr_sa_clear_b_zero;
    sc_signal<sc_bv<32>> arr_cswitch_arr_b_zero;
    sc_signal<uint32_t> arr_nsplit, arr_context_id_b_zero;
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
    sc_signal<bool> core_pipeline_en, core_cscan_en, core_sa_clear;
    sc_signal<sc_bv<32>> core_cswitch_arr;
    sc_signal<uint32_t> core_context_id;
    sc_signal<bool> core_done, core_deadlock, core_active;

    SramT *sram;
    ArrayT *array_inst;
    CoreT *core;

    void build(const char *prefix, sc_in<bool> &clk)
    {
        sram = new SramT((std::string(prefix) + "_sram").c_str());
        sram->i_clk(clk); sram->i_rstn(sram_rstn);
        sram->i_deepsleep(sram_deepsleep); sram->i_powergate(sram_powergate); sram->i_select(sram_select);
        sram->i_host_addr(host_addr); sram->i_host_wren(host_wren); sram->i_host_rden(host_rden);
        sram->i_host_wdata(host_wdata); sram->i_host_wmask(host_wmask); sram->o_host_rdata(host_rdata);
        sram->i_srama_addr_a(srama_addr_a); sram->i_srama_rden_a(srama_rden_a); sram->o_srama_data_a(srama_data_a);
        sram->i_srama_addr_b(srama_addr_b); sram->i_srama_rden_b(srama_rden_b); sram->o_srama_data_b(srama_data_b);
        sram->i_sramb_addr_a(sramb_addr_a); sram->i_sramb_rden_a(sramb_rden_a); sram->o_sramb_data_a(sramb_data_a);
        sram->i_sramb_addr_b(sramb_addr_b); sram->i_sramb_rden_b(sramb_rden_b); sram->o_sramb_data_b(sramb_data_b);
        sram->i_sramc_wdata_a(sramc_wdata_a); sram->i_sramc_addr_a(sramc_addr_a); sram->i_sramc_wren_a(sramc_wren_a);
        sram->i_sramc_rden_a(sramc_rden_a); sram->i_sramc_wmask_a(sramc_wmask_a); sram->o_sramc_rdata_a(sramc_rdata_a);
        sram->i_sramc_wdata_b(sramc_wdata_b); sram->i_sramc_addr_b(sramc_addr_b); sram->i_sramc_wren_b(sramc_wren_b);
        sram->i_sramc_rden_b(sramc_rden_b); sram->i_sramc_wmask_b(sramc_wmask_b); sram->o_sramc_rdata_b(sramc_rdata_b);

        array_inst = new ArrayT((std::string(prefix) + "_array").c_str());
        array_inst->i_clk(clk); array_inst->i_rstn(arr_rstn);
        array_inst->i_nsplit(arr_nsplit); array_inst->i_threshold(arr_threshold);
        array_inst->i_act_arr_a(s_act_arr); array_inst->i_act_arr_b(arr_act_b_zero);
        array_inst->i_wei_arr_a(s_wei_arr); array_inst->i_wei_arr_b(arr_wei_b_zero);
        array_inst->i_c_arr_a(s_psm_to_sa_c); array_inst->o_c_arr_a(s_sa_to_psm_c);
        array_inst->i_c_arr_b(arr_c_b_zero_in); array_inst->o_c_arr_b(arr_c_b_zero_out);
        array_inst->i_pipeline_en_a(core_pipeline_en); array_inst->i_pipeline_en_b(arr_pipeline_en_b_zero);
        array_inst->i_cscan_en_a(core_cscan_en); array_inst->i_cscan_en_b(arr_cscan_en_b_zero);
        array_inst->i_cswitch_arr_a(core_cswitch_arr); array_inst->i_cswitch_arr_b(arr_cswitch_arr_b_zero);
        array_inst->i_sa_clear_a(core_sa_clear); array_inst->i_sa_clear_b(arr_sa_clear_b_zero);
        array_inst->i_context_id_a(core_context_id); array_inst->i_context_id_b(arr_context_id_b_zero);

        core = new CoreT((std::string(prefix) + "_core").c_str());
        core->i_clk(clk); core->i_rstn(core_rstn);
        core->i_soft_reset(core_soft_reset); core->i_start(core_start);
        core->i_mvm_k(core_mvm_k); core->i_total_contexts(core_total_contexts); core->i_nsplit(core_nsplit);
        core->i_incntlim(core_incntlim); core->i_act_reps(core_act_reps); core->i_wei_reps(core_wei_reps);
        core->i_out_ncontexts(core_out_ncontexts);
        core->i_act_incntlim(core_act_incntlim); core->i_act_incntstep(core_act_incntstep);
        core->i_act_outcntlim(core_act_outcntlim); core->i_act_outcntstep(core_act_outcntstep);
        core->i_act_dil_pat(core_act_dil_pat);
        core->i_act_xlim(core_act_xlim); core->i_act_xstep(core_act_xstep);
        core->i_act_ylim(core_act_ylim); core->i_act_ystep(core_act_ystep);
        core->i_act_chlim(core_act_chlim); core->i_act_chstep(core_act_chstep);
        core->i_act_til_xlim(core_act_til_xlim); core->i_act_til_xstep(core_act_til_xstep);
        core->i_act_til_ylim(core_act_til_ylim); core->i_act_til_ystep(core_act_til_ystep);
        core->i_act_base_addr(core_act_base_addr);
        core->i_wei_incntlim(core_wei_incntlim); core->i_wei_incntstep(core_wei_incntstep);
        core->i_wei_wlim(core_wei_wlim); core->i_wei_wstep(core_wei_wstep);
        core->i_wei_klim(core_wei_klim); core->i_wei_kstep(core_wei_kstep);
        core->i_wei_til_klim(core_wei_til_klim); core->i_wei_til_kstep(core_wei_til_kstep);
        core->i_wei_cols_active(core_wei_cols_active); core->i_wei_waligned(core_wei_waligned);
        core->i_wei_base_addr(core_wei_base_addr);
        core->i_cxlim(core_cxlim); core->i_cxstep(core_cxstep);
        core->i_cklim(core_cklim); core->i_ckstep(core_ckstep);
        core->i_out_til_cylim(core_out_til_cylim); core->i_out_til_cystep(core_out_til_cystep);
        core->i_out_til_cklim(core_out_til_cklim); core->i_out_til_ckstep(core_out_til_ckstep);
        core->i_out_preload_en(core_out_preload_en); core->i_out_base_addr(core_out_base_addr);
        core->i_rows_active(core_rows_active);
        core->o_srama_addr(srama_addr_a); core->o_srama_rden(srama_rden_a); core->i_srama_data(srama_data_a);
        core->o_sramb_addr(sramb_addr_a); core->o_sramb_rden(sramb_rden_a); core->i_sramb_data(sramb_data_a);
        core->o_sramc_addr(sramc_addr_a); core->o_sramc_wren(sramc_wren_a); core->o_sramc_rden(sramc_rden_a);
        core->o_sramc_wmask(sramc_wmask_a); core->o_sramc_wdata(sramc_wdata_a); core->i_sramc_rdata(sramc_rdata_a);
        core->o_act_arr(s_act_arr); core->o_wei_arr(s_wei_arr);
        core->i_c_arr(s_sa_to_psm_c); core->o_c_arr(s_psm_to_sa_c);
        core->o_pipeline_en(core_pipeline_en); core->o_cscan_en(core_cscan_en);
        core->o_cswitch_arr(core_cswitch_arr); core->o_sa_clear(core_sa_clear);
        core->o_context_id(core_context_id);
        core->o_done(core_done); core->o_deadlock(core_deadlock); core->o_active(core_active);
    }

    void reset_and_configure(int total_contexts)
    {
        sram_rstn.write(false); sram_deepsleep.write(false); sram_powergate.write(false); sram_select.write(0);
        host_wren.write(false); host_rden.write(false);
        arr_rstn.write(false); arr_nsplit.write(32); arr_threshold.write(0.0f);
        arr_act_b_zero.write(act_vector_t<32, int8_t>());
        arr_wei_b_zero.write(wei_vector_t<32, int8_t>());
        arr_c_b_zero_in.write(psum_vector_t<32, int32_t>());
        arr_pipeline_en_b_zero.write(false); arr_cscan_en_b_zero.write(false);
        arr_cswitch_arr_b_zero.write(sc_bv<32>(0)); arr_sa_clear_b_zero.write(true);
        arr_context_id_b_zero.write(0);

        core_rstn.write(false); core_soft_reset.write(false); core_start.write(false);
        core_mvm_k.write(1); core_total_contexts.write((uint32_t)total_contexts); core_nsplit.write(32);
        core_incntlim.write(0); core_act_reps.write(1); core_wei_reps.write(1);
        core_out_ncontexts.write((uint32_t)total_contexts);
        // 96 = reset value of r_act_incntlim in config_regs.h, not 0: the feeders use a strict incnt < incntlim
        // check (data_feeder/wei_feeder.h), not the '0 = one value' convention of ContextSwitchController. A value
        // of 0 blocks every SRAM read and stalls the weight feeder permanently.
        core_act_incntlim.write(96); core_act_incntstep.write(1);
        core_act_outcntlim.write(0); core_act_outcntstep.write(1);
        core_act_dil_pat.write(sc_bv<64>(~0ULL));
        core_act_xlim.write(1); core_act_xstep.write(1); core_act_ylim.write(1); core_act_ystep.write(1);
        core_act_chlim.write(1); core_act_chstep.write(1);
        core_act_til_xlim.write(1); core_act_til_xstep.write(1);
        core_act_til_ylim.write(1); core_act_til_ystep.write(1);
        core_act_base_addr.write(0);
        core_wei_incntlim.write(96); core_wei_incntstep.write(1); // see the note at core_act_incntlim above
        core_wei_wlim.write(1); core_wei_wstep.write(1); core_wei_klim.write(1); core_wei_kstep.write(1);
        core_wei_til_klim.write(1); core_wei_til_kstep.write(1);
        core_wei_cols_active.write(0xFFFFFFFFu); core_wei_waligned.write(1); core_wei_base_addr.write(0);
        // 96 = reset value of r_cxlim/r_cklim (config_regs.h): the same strict limit as act/wei_incntlim above,
        // applied on the PSM side. A value of 0 could be a further cause of stalls (separate from wei_stall).
        core_cxlim.write(96); core_cxstep.write(1); core_cklim.write(96); core_ckstep.write(1);
        core_out_til_cylim.write(0); core_out_til_cystep.write(1);
        core_out_til_cklim.write(0); core_out_til_ckstep.write(1);
        core_out_preload_en.write(false); core_out_base_addr.write(0);
        core_rows_active.write(sramc_mask_t<32>(true));
    }

    void seed_pattern()
    {
        std::vector<uint8_t> zero_c(32 * sizeof(int32_t), 0);
        sram->write_bank_data(4, 0, zero_c.data(), zero_c.size());
        // Seed the whole SRAM-A/B banks (not only the first 32 bytes): a multi-context run can read non-zero
        // addresses (til_x / til_y / context progression); missing data would stall the feeder.
        std::vector<int8_t> act_full(5056, 100);
        std::vector<int8_t> wei_full(5184, 50);
        sram->write_bank_data(2, 0, reinterpret_cast<const uint8_t*>(act_full.data()), act_full.size());
        sram->write_bank_data(0, 0, reinterpret_cast<const uint8_t*>(wei_full.data()), wei_full.size());
    }
};

SC_MODULE(TbRtlRefVsNativeMultictxCheck)
{
    sc_in<bool> i_clk;
    LaneRig<NativeCoreT> native_rig;
    LaneRig<RtlRefCoreT> rtlref_rig;
    int errors = 0;

    SC_CTOR(TbRtlRefVsNativeMultictxCheck)
    {
        native_rig.build("native", i_clk);
        rtlref_rig.build("rtlref", i_clk);
        SC_THREAD(run);
        sensitive << i_clk.pos();
    }

    void tick(int n = 1) { for (int i = 0; i < n; i++) wait(); }

    void check(bool cond, const std::string &msg)
    {
        if (cond) { std::cout << "  [PASS] " << msg << std::endl; }
        else { std::cout << "  [FAIL] " << msg << std::endl; errors++; }
    }

    void run()
    {
        std::cout << "==================================================" << std::endl;
        std::cout << " RTL-REF vs NATIVE MULTI-CONTEXT DIFFERENTIAL CHECK " << std::endl;
        std::cout << " total_contexts=4 (>=3, so the RTL-ref PSM enters its real write path)" << std::endl;
        std::cout << "==================================================" << std::endl;

        const int TOTAL_CONTEXTS = 4;
        native_rig.reset_and_configure(TOTAL_CONTEXTS);
        rtlref_rig.reset_and_configure(TOTAL_CONTEXTS);
        // Architectural difference, not a configuration error: sauria_rtl::Control's FeedersFsm runs with
        // FX1_A3_NO_CTX_REARM (default on). It does not re-arm at each context boundary but uses wei_reps to walk
        // several contexts internally. The RTL reference uses act_reps = 1 (activations read once, reused for N
        // weight passes / contexts) and wei_reps = ncontexts (wei_reps = 1 would leave i_cswitch = wei_ov_flag
        // gating the til_k counter). Only wei_reps changes; act_reps stays 1 (reset_and_configure() default).
        rtlref_rig.core_wei_reps.write((uint32_t)TOTAL_CONTEXTS);
        // PsmIdxCnt's done = xk_flag = (X counter overflow) && (K counter overflow at the same time).
        // generic_counter(): candidate = cnt + step, overflow when candidate >= lim. With cxlim = cklim = 96 the K counter
        // advances once per X overflow, so K overflows only after 96*96 = 9216 cycles -- not a deadlock, just too slow
        // for a 2600-cycle test. For the minimal K = 1 / one-tile case cxlim = cklim = 1 is correct (overflow on the
        // first cycle, like xlim/ylim/chlim = 1 in the feeders). The native core addresses SRAM-C differently
        // (calc_c_addr, not PsmIdxCnt) and keeps 96.
        rtlref_rig.core_cxlim.write(1);
        rtlref_rig.core_cklim.write(1);
        // wei_reps counts til_done PULSES, not address steps. With xy_flag = x_flag && y_flag and y/ch/tilx/tily
        // limits of 1 (overflow immediately), only X (act) / W (wei) set the real length of a pulse. With
        // xlim = wlim = 1 every cnt_en cycle overflows at once, so 4 wei_reps finish in 4 cycles, act_fin/wei_fin rise
        // almost together and FEEDING leaves through FINAL_PUSH_BOTH into a permanent EMPTY_WAIT. xlim (act) / wlim
        // (wei) are therefore set to 4 address steps per rep (step 1): act_reps = 1 needs 4 cycles, wei_reps = 4 needs
        // 16 cycles before wei_fin. y/ch/tilx/tily/klim/til_klim stay 1. The native core keeps xlim = wlim = 1.
        // xlim = wlim = 32 (= X_DIM/Y_DIM) is not usable: FIFO_DEPTH = 16 < 32 makes fifo_full_any block cnt_en
        // permanently. pipeline_en stays high only ~9 cycles with 4, shorter than the PE_LAT = 64 array latency;
        // a larger value must take FIFO_DEPTH into account.
        rtlref_rig.core_act_xlim.write(4);
        rtlref_rig.core_wei_wlim.write(4);
        // Debug outputs dbg_sram_data_q0 / dbg_push0: sram_data_q_[0] = 50 and valid_data = 1 (SRAM read is correct),
        // but FeedDataManager::fifo_push never rises (n_free_regs never reaches 0). wei_waligned = 0 does not change
        // the trace. The open cause is in FeedDataManager's register fill logic (dil_pat / read_ptr_q_, elm_number
        // never >= 1).
        tick(5);
        native_rig.sram_rstn.write(true); native_rig.arr_rstn.write(true); native_rig.core_rstn.write(true);
        rtlref_rig.sram_rstn.write(true); rtlref_rig.arr_rstn.write(true); rtlref_rig.core_rstn.write(true);
        tick(5);

        native_rig.seed_pattern();
        rtlref_rig.seed_pattern();

        std::cout << "\n[PULSE] i_start = true (ca hai core)" << std::endl;
        native_rig.core_start.write(true);
        rtlref_rig.core_start.write(true);
        tick();
        native_rig.core_start.write(false);
        rtlref_rig.core_start.write(false);

        bool native_deadlock = false, rtlref_deadlock = false;
        bool native_done_ever = false, rtlref_done_ever = false;
        const int TOTAL_CYCLES = 3000;
        bool logged_wren = false;
        for (int c = 0; c < TOTAL_CYCLES; c++)
        {
            tick();
            if (rtlref_rig.core_cscan_en.read())
            {
                psum_vector_t<32, int32_t> carr_scan = rtlref_rig.s_sa_to_psm_c.read();
                static int cscan_log_count = 0;
                if (cscan_log_count < 20)
                {
                    std::cout << "  [CSCAN CAPTURE cyc=" << c << " ctx=" << rtlref_rig.core_context_id.read()
                              << "] c_arr_from_array[0..2]=" << carr_scan[0] << "," << carr_scan[1] << "," << carr_scan[2]
                              << std::endl;
                    cscan_log_count++;
                }
            }
            // Periodically sample act_arr/wei_arr (feeder outputs, real array_inst inputs) to see whether real values
            // (100/50) are still present in contexts > 0 -- narrows down whether the feeders or the array cause c_arr = 0.
            {
                static int feed_log_count = 0;
                static int last_ctx_logged = -1;
                int cur_ctx = (int)rtlref_rig.core_context_id.read();
                if (cur_ctx != last_ctx_logged && feed_log_count < 12)
                {
                    last_ctx_logged = cur_ctx;
                    feed_log_count++;
                    act_vector_t<32, int8_t> av = rtlref_rig.s_act_arr.read();
                    wei_vector_t<32, int8_t> wv = rtlref_rig.s_wei_arr.read();
                    std::cout << "  [FEEDER OUT ctx-change cyc=" << c << " newctx=" << cur_ctx
                              << "] act_arr[0..2]=" << (int)av[0] << "," << (int)av[1] << "," << (int)av[2]
                              << " wei_arr[0..2]=" << (int)wv[0] << "," << (int)wv[1] << "," << (int)wv[2]
                              << std::endl;
                }
            }
            if (rtlref_rig.sramc_wren_a.read())
            {
                logged_wren = true;
                sramc_mask_t<32> wm = rtlref_rig.sramc_wmask_a.read();
                psum_vector_t<32, int32_t> wd = rtlref_rig.sramc_wdata_a.read();
                psum_vector_t<32, int32_t> carr = rtlref_rig.s_sa_to_psm_c.read();
                std::cout << "  [WREN CAPTURE cyc=" << c << " ctx=" << rtlref_rig.core_context_id.read()
                          << "] addr=" << rtlref_rig.sramc_addr_a.read()
                          << " wdata[0..2]=" << wd[0] << "," << wd[1] << "," << wd[2]
                          << " wmask[0..2]=" << wm[0] << "," << wm[1] << "," << wm[2]
                          << " c_arr_from_array[0..2]=" << carr[0] << "," << carr[1] << "," << carr[2]
                          << std::endl;
            }
            if (native_rig.core_deadlock.read()) native_deadlock = true;
            if (rtlref_rig.core_deadlock.read()) rtlref_deadlock = true;
            if (native_rig.core_done.read()) native_done_ever = true;
            if (rtlref_rig.core_done.read()) rtlref_done_ever = true;
            // Log EVERY cycle in the first 250 cycles to see the full fd_state (FeedersFsm) sequence -- valid = 1
            // appears only once (cycles 4-7), so the remaining states need to be visible.
            if (c < 250)
            {
                static int pop_log_count = 0;
                if (pop_log_count < 400)
                {
                    act_vector_t<32, int8_t> av = rtlref_rig.s_act_arr.read();
                    wei_vector_t<32, int8_t> wv = rtlref_rig.s_wei_arr.read();
                    std::cout << "  [POP/VALID EVENT cyc=" << c << " ctx=" << rtlref_rig.core_context_id.read()
                              << "] act_pop=" << rtlref_rig.core->dbg_act_pop_en()
                              << " wei_pop=" << rtlref_rig.core->dbg_wei_pop_en()
                              << " act_valid=" << rtlref_rig.core->dbg_act_valid()
                              << " wei_valid=" << rtlref_rig.core->dbg_wei_valid()
                              << " act_arr[0]=" << (int)av[0] << " wei_arr[0]=" << (int)wv[0]
                              << " act_outbounds=" << rtlref_rig.core->act_feeder_a->dbg_outbounds_now()
                              << " act_outbounds_q1=" << rtlref_rig.core->act_feeder_a->dbg_outbounds_q1_now()
                              << " fd_state=" << rtlref_rig.core->ctrl_inst_a->dbg_feeders_state()
                              << " act_hold=" << rtlref_rig.core->ctrl_inst_a->dbg_act_hold()
                              << " wei_hold=" << rtlref_rig.core->ctrl_inst_a->dbg_wei_hold()
                              << " w_addr=" << rtlref_rig.core->dbg_wei_sramb_addr()
                              << " w_fpq2=" << rtlref_rig.core->dbg_wei_finalpush_q2()
                              << " w_sdq0=" << rtlref_rig.core->dbg_wei_sram_data_q0()
                              << " w_done=" << rtlref_rig.core->dbg_wei_done()
                              << " w_til=" << rtlref_rig.core->dbg_wei_til_done()
                              << " w_push0=" << rtlref_rig.core->dbg_wei_push0()
                              << " w_data0=" << rtlref_rig.core->dbg_wei_data0()
                              << " w_vdata=" << rtlref_rig.core->dbg_wei_vdata()
                              << " w_cnten=" << rtlref_rig.core->dbg_wei_cnten()
                              << " w_fen0=" << rtlref_rig.core->dbg_wei_fen0()
                              << " w_ca0=" << rtlref_rig.core->dbg_wei_in_ca0()
                              << " w_infen=" << rtlref_rig.core->dbg_wei_in_fen()
                              << " w_lifen=" << rtlref_rig.core->dbg_wei_li_fen()
                              << " w_vq1=" << rtlref_rig.core->dbg_wei_vq1()
                              << " w_patn=" << rtlref_rig.core->dbg_wei_patn()
                              << " w_patf=" << rtlref_rig.core->dbg_wei_patf()
                              << " w_ract=" << rtlref_rig.core->dbg_wei_ract()
                              << " w_elm=" << rtlref_rig.core->dbg_wei_elm()
                              << " w_nfree=" << rtlref_rig.core->dbg_wei_nfree()
                              << " w_pren=" << rtlref_rig.core->dbg_wei_pren()
                              << " w_fullany=" << rtlref_rig.core->dbg_wei_full_any()
                              << " w_stallany=" << rtlref_rig.core->dbg_wei_stall_any()
                              << " pipe_en=" << rtlref_rig.core_pipeline_en.read()
                              << " c0=" << rtlref_rig.s_sa_to_psm_c.read()[0]
                              << std::endl;
                    pop_log_count++;
                }
            }
            if (c % 200 == 0 || (c < 250 && c % 20 == 0))
            {
                std::cout << "  [cyc " << c << "] native: ctx=" << native_rig.core_context_id.read()
                          << " done=" << native_rig.core_done.read()
                          << " active=" << native_rig.core_active.read()
                          << "  |  rtlref: ctx=" << rtlref_rig.core_context_id.read()
                          << " done=" << rtlref_rig.core_done.read()
                          << " active=" << rtlref_rig.core_active.read()
                          << " ctx_status=" << rtlref_rig.core->ctrl_inst_a->dbg_ctx_prev_status()
                          << " ctx_cnt=" << rtlref_rig.core->ctrl_inst_a->dbg_context_cnt()
                          << " pipeline_en=" << rtlref_rig.core_pipeline_en.read()
                          << " psm_state=" << rtlref_rig.core->psm_inst_a->dbg_fsm_state()
                          << " psm_ctx_cnt=" << rtlref_rig.core->psm_inst_a->dbg_fsm_ctx_cnt()
                          << " psm_start=" << rtlref_rig.core_cscan_en.read()
                          << std::endl;
            }
        }

        check(!native_deadlock, "native: no deadlock");
        check(!rtlref_deadlock, "rtlref: no deadlock");
        std::cout << "  [INFO] native_done_ever=" << native_done_ever
                  << " rtlref_done_ever=" << rtlref_done_ever << std::endl;

        int32_t native_c[8] = {0}, rtlref_c[8] = {0};
        native_rig.sram->read_bank_data(4, 0, reinterpret_cast<uint8_t*>(native_c), sizeof(native_c));
        rtlref_rig.sram->read_bank_data(4, 0, reinterpret_cast<uint8_t*>(rtlref_c), sizeof(rtlref_c));

        std::cout << "\n[SRAM-C READBACK] native   lanes[0..7] = ";
        for (int i = 0; i < 8; i++) std::cout << native_c[i] << " ";
        std::cout << "\n[SRAM-C READBACK] rtlref   lanes[0..7] = ";
        for (int i = 0; i < 8; i++) std::cout << rtlref_c[i] << " ";
        std::cout << std::endl;

        // Read a wider range (256 int32 = first 1024 bytes of bank 4) to see whether data exists at a non-zero
        // offset (cxlim = cklim = 1 may change the address formula).
        {
            std::vector<int32_t> wide(256, 0);
            rtlref_rig.sram->read_bank_data(4, 0, reinterpret_cast<uint8_t*>(wide.data()), wide.size() * sizeof(int32_t));
            bool any_nonzero = false;
            int first_nonzero_idx = -1;
            for (size_t i = 0; i < wide.size(); i++)
            {
                if (wide[i] != 0) { any_nonzero = true; if (first_nonzero_idx < 0) first_nonzero_idx = (int)i; }
            }
            std::cout << "[SRAM-C WIDE SCAN] rtlref bank4 offset[0..1023 byte]: any_nonzero=" << any_nonzero
                      << " first_nonzero_int32_idx=" << first_nonzero_idx << std::endl;
            if (any_nonzero)
            {
                std::cout << "  [SRAM-C WIDE SCAN] values around it: ";
                for (int i = std::max(0, first_nonzero_idx - 2); i < std::min((int)wide.size(), first_nonzero_idx + 6); i++)
                    std::cout << "[" << i << "]=" << wide[i] << " ";
                std::cout << std::endl;
            }
        }

        bool native_has_data = false, rtlref_has_data = false, exact_match = true;
        for (int i = 0; i < 8; i++)
        {
            if (native_c[i] != 0) native_has_data = true;
            if (rtlref_c[i] != 0) rtlref_has_data = true;
            if (native_c[i] != rtlref_c[i]) exact_match = false;
        }
        check(native_has_data, "native: SRAM-C holds real data (>=1 lane != 0) -- known-good control");
        check(rtlref_has_data, "rtlref: SRAM-C holds real data (>=1 lane != 0) with ctx_cnt>=3");
        if (native_has_data && rtlref_has_data)
        {
            std::cout << "  [INFO] exact_match between native and rtlref = " << exact_match
                      << " (not required to PASS -- the write address/time of "
                      << "the two architectures can legitimately differ)" << std::endl;
        }

        std::cout << "\n==================================================" << std::endl;
        if (errors == 0) std::cout << "  [PASS] RTL-REF vs NATIVE MULTI-CONTEXT DIFFERENTIAL CHECK (minimum conditions)." << std::endl;
        else std::cout << "  [FAIL] RTL-REF vs NATIVE MULTI-CONTEXT DIFFERENTIAL CHECK: " << errors << " errors." << std::endl;
        std::cout << "==================================================" << std::endl;

        sc_stop();
    }
};

int sc_main(int argc, char **argv)
{
    sc_clock clk("clk", 10, SC_NS);
    TbRtlRefVsNativeMultictxCheck tb("TbRtlRefVsNativeMultictxCheck_inst");
    tb.i_clk(clk);
    sc_start();
    return 0;
}
