// K = 8 accumulation with the ported systolic array (systolic_array/rtl_ref_sa_array.h, namespace sauria_rtl,
// cs_delay[y][x] and swap semantics matching the ported ContextSwitchController) instead of the native
// array_inst. Everything else is as in tools/test_rtl_ref_array_k8_accum_check.cpp (RtlRefLaneACoreA,
// act[k] = 100+k / wei[k] = 50+k, EXPECTED_SUM = 44340, total_contexts = 1). The native array and the ported
// ContextSwitchController/ContextFsm use different cswitch-staggering schemes; this test checks the ported pair.
//
// This file uses a NON-STANDARD configuration (wlim = 1 + auxlim = K to sweep K, cols_active = 0xFFFFFFFF,
// cxlim = cklim = 0) and keeps preload_en = false; enabling preload here deadlocks. The reference test for
// K > 1 with the real configuration is tools/test_rtl_ref_multictx_real_gemm_check.cpp.
#define SC_ALLOW_DEPRECATED_IEEE_API

#include <systemc.h>
#include <iostream>
#include <set>

#include "sram/sram_top.h"
#include "control/native_lane_a_core.h"
#include "systolic_array/rtl_ref_sa_array.h"

using namespace sauria;

typedef Sram<32, 32, int8_t, int8_t, int32_t, 5056, 5184, 1536> SramT;
typedef sauria_rtl::SystolicArray<32, 32, int8_t, int8_t, int32_t> ArrayT;
typedef RtlRefLaneACoreA<32, 32, int8_t, int8_t, int32_t, 16, 64, 1, 1536> RtlRefCoreT;

SC_MODULE(TbRtlRefArrayPortedK8AccumCheck)
{
    sc_in<bool> i_clk;

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

    // --- array_inst (ported, not the native one, no lane B -- see sauria_rtl::SystolicArray) ---
    sc_signal<bool> arr_rstn;
    sc_signal<act_vector_t<32, int8_t>> s_act_arr;
    sc_signal<wei_vector_t<32, int8_t>> s_wei_arr;
    sc_signal<psum_vector_t<32, int32_t>> s_sa_to_psm_c, s_psm_to_sa_c;
    sc_signal<bool> arr_softstall_zero, arr_pop_en_dbg_zero;
    sc_signal<float> arr_threshold;

    // --- RtlRefLaneACoreA ports ---
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
    RtlRefCoreT *core;
    int errors = 0;
    static const int K = 8;
    static const int32_t EXPECTED_SUM = 44340; // sum_{k=0}^{7} (100+k)*(50+k), same as the lane A array test

    SC_CTOR(TbRtlRefArrayPortedK8AccumCheck)
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
        sram->i_srama_addr_a(srama_addr_a); sram->i_srama_rden_a(srama_rden_a); sram->o_srama_data_a(srama_data_a);
        sram->i_srama_addr_b(srama_addr_b); sram->i_srama_rden_b(srama_rden_b); sram->o_srama_data_b(srama_data_b);
        sram->i_sramb_addr_a(sramb_addr_a); sram->i_sramb_rden_a(sramb_rden_a); sram->o_sramb_data_a(sramb_data_a);
        sram->i_sramb_addr_b(sramb_addr_b); sram->i_sramb_rden_b(sramb_rden_b); sram->o_sramb_data_b(sramb_data_b);
        sram->i_sramc_wdata_a(sramc_wdata_a); sram->i_sramc_addr_a(sramc_addr_a); sram->i_sramc_wren_a(sramc_wren_a);
        sram->i_sramc_rden_a(sramc_rden_a); sram->i_sramc_wmask_a(sramc_wmask_a); sram->o_sramc_rdata_a(sramc_rdata_a);
        sram->i_sramc_wdata_b(sramc_wdata_b); sram->i_sramc_addr_b(sramc_addr_b); sram->i_sramc_wren_b(sramc_wren_b);
        sram->i_sramc_rden_b(sramc_rden_b); sram->i_sramc_wmask_b(sramc_wmask_b); sram->o_sramc_rdata_b(sramc_rdata_b);

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

        core = new RtlRefCoreT("core");
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
        core->o_srama_addr(srama_addr_a);
        core->o_srama_rden(srama_rden_a);
        core->i_srama_data(srama_data_a);
        core->o_sramb_addr(sramb_addr_a);
        core->o_sramb_rden(sramb_rden_a);
        core->i_sramb_data(sramb_data_a);
        core->o_sramc_addr(sramc_addr_a);
        core->o_sramc_wren(sramc_wren_a);
        core->o_sramc_rden(sramc_rden_a);
        core->o_sramc_wmask(sramc_wmask_a);
        core->o_sramc_wdata(sramc_wdata_a);
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

    void check(bool cond, const std::string &msg)
    {
        if (cond) { std::cout << "  [PASS] " << msg << std::endl; }
        else { std::cout << "  [FAIL] " << msg << std::endl; errors++; }
    }

    void run()
    {
        std::cout << "==================================================" << std::endl;
        std::cout << " RTL-REF ARRAY (PORTED) K=8 ACCUM CHECK -- array_inst from the reference model, no native array" << std::endl;
        std::cout << " Expected: get_pe_mac(0,0) reaches " << EXPECTED_SUM << " at some point" << std::endl;
        std::cout << "==================================================" << std::endl;

        sram_rstn.write(false); sram_deepsleep.write(false); sram_powergate.write(false); sram_select.write(0);
        host_wren.write(false); host_rden.write(false);
        arr_rstn.write(false);
        arr_threshold.write(0.0f);
        arr_softstall_zero.write(false);
        arr_pop_en_dbg_zero.write(false);

        core_rstn.write(false); core_soft_reset.write(false); core_start.write(false);
        core_mvm_k.write(K); core_total_contexts.write(1); core_nsplit.write(32);
        core_incntlim.write(K); core_act_reps.write(1); core_wei_reps.write(1); core_out_ncontexts.write(1);
        core_act_incntlim.write(0); core_act_incntstep.write(1);
        core_act_outcntlim.write(0); core_act_outcntstep.write(1);
        core_act_dil_pat.write(sc_bv<64>(~0ULL));
        core_act_xlim.write(1); core_act_xstep.write(1); core_act_ylim.write(1); core_act_ystep.write(1);
        core_act_chlim.write(K); core_act_chstep.write(1);
        core_act_til_xlim.write(1); core_act_til_xstep.write(1);
        core_act_til_ylim.write(1); core_act_til_ystep.write(1);
        core_act_base_addr.write(0);
        core_wei_incntlim.write(0); core_wei_incntstep.write(1);
        core_wei_wlim.write(1); core_wei_wstep.write(1); core_wei_klim.write(K); core_wei_kstep.write(1);
        core_wei_til_klim.write(1); core_wei_til_kstep.write(1);
        core_wei_cols_active.write(0xFFFFFFFFu); core_wei_waligned.write(1); core_wei_base_addr.write(0);
        core_cxlim.write(0); core_cxstep.write(1); core_cklim.write(0); core_ckstep.write(1);
        core_out_til_cylim.write(0); core_out_til_cystep.write(1);
        core_out_til_cklim.write(0); core_out_til_ckstep.write(1);
        core_out_preload_en.write(false); // preload_en stays false in this configuration (see file header)
        core_out_base_addr.write(0);
        core_rows_active.write(sramc_mask_t<32>(true));

        tick(5);
        sram_rstn.write(true);
        arr_rstn.write(true);
        core_rstn.write(true);
        tick(5);

        check(!errors, "construction and binding without error");

        std::vector<uint8_t> zero_c(32 * sizeof(int32_t), 0);
        sram->write_bank_data(4, 0, zero_c.data(), zero_c.size());
        int8_t act_pattern[K], wei_pattern[K];
        for (int i = 0; i < K; i++) { act_pattern[i] = static_cast<int8_t>(100 + i); wei_pattern[i] = static_cast<int8_t>(50 + i); }
        sram->write_bank_data(2, 0, reinterpret_cast<const uint8_t*>(act_pattern), sizeof(act_pattern));
        sram->write_bank_data(0, 0, reinterpret_cast<const uint8_t*>(wei_pattern), sizeof(wei_pattern));

        std::cout << "\n[PULSE] i_start = true" << std::endl;
        core_start.write(true);
        tick();
        core_start.write(false);

        bool saw_deadlock = false;
        bool found_expected = false;
        int found_cycle = -1;
        std::set<int32_t> seen_values;
        int32_t max_seen = 0;
        const int TOTAL_CYCLES = 5000;

        bool prev_sa_clear = false;
        int32_t prev_pe_mac = 0;

        for (int c = 0; c < TOTAL_CYCLES; c++)
        {
            tick();
            if (core_deadlock.read()) { saw_deadlock = true; }

            int32_t pe_mac = array_inst->get_pe_mac(0, 0);
            if (pe_mac != 0) seen_values.insert(pe_mac);
            if (pe_mac > max_seen) max_seen = pe_mac;
            if (pe_mac == EXPECTED_SUM && !found_expected)
            {
                found_expected = true;
                found_cycle = c;
                std::cout << "  [FOUND] get_pe_mac(0,0) == " << EXPECTED_SUM << " o cycle " << c << std::endl;
            }

            bool sa_clear_now = core_sa_clear.read();
            if (sa_clear_now && !prev_sa_clear)
            {
                std::cout << "  [SA_CLEAR RISING EDGE] cycle=" << c
                          << " pe_mac_before=" << prev_pe_mac
                          << " context_id=" << core_context_id.read()
                          << " active=" << core_active.read()
                          << std::endl;
            }
            if (prev_pe_mac != 0 && pe_mac == 0 && !found_expected)
            {
                std::cout << "  [PE_MAC RESET TO 0] cycle=" << c
                          << " prev_pe_mac=" << prev_pe_mac
                          << " sa_clear_now=" << sa_clear_now
                          << " sa_clear_prev=" << prev_sa_clear
                          << " context_id=" << core_context_id.read()
                          << std::endl;
            }
            prev_sa_clear = sa_clear_now;
            prev_pe_mac = pe_mac;

            if (c < 35 || (c >= 75 && c <= 95) || (c % 200 == 0))
            {
                std::cout << "  [cyc " << c << "] active=" << core_active.read()
                          << " done=" << core_done.read()
                          << " pipeline_en=" << core_pipeline_en.read()
                          << " cscan_en=" << core_cscan_en.read()
                          << " cswitch_arr=" << core_cswitch_arr.read()
                          << " pe_mac(0,0)=" << pe_mac
                          << " act_arr[0]=" << (int)s_act_arr.read()[0]
                          << " wei_arr[0]=" << (int)s_wei_arr.read()[0]
                          << " context_id=" << core_context_id.read()
                          // checks the FIFO empty-shim hypothesis
                          << " wei_eq1=" << core->dbg_wei_fifo_empty_q1()
                          << " wei_eq2=" << core->dbg_wei_fifo_empty_q2()
                          << " wei_estart=" << core->dbg_wei_fifo_empty_start()
                          << " wei_ptr0=" << core->dbg_wei_fifo_ptr0()
                          << " wei_hold=" << core->dbg_ctrl_wei_hold()
                          << " fd_state=" << core->dbg_ctrl_feeders_state()
                          << " wei_aux=" << core->dbg_wei_aux()
                          << " wei_w=" << core->dbg_wei_w()
                          << " wei_tilk=" << core->dbg_wei_tilk()
                          << " wei_elm=" << core->dbg_wei_elm()
                          << " wei_nfree=" << core->dbg_wei_nfree()
                          << " wei_ract=" << core->dbg_wei_ract()
                          << " wei_pren=" << core->dbg_wei_pren()
                          << " wei_cnten=" << core->dbg_wei_cnten()
                          << " wei_valid=" << core->dbg_wei_valid()
                          << " wei_finpush2=" << core->dbg_wei_finalpush_q2()
                          << std::endl;
            }

            if (found_expected && c > found_cycle + 20) break;
        }

        check(!saw_deadlock, "never deadlocks");
        check(found_expected, "get_pe_mac(0,0) reaches the expected value " + std::to_string(EXPECTED_SUM) +
              " at some point (K=8 accumulation correct through the PORTED array)");

        std::cout << "\n[DEBUG] non-zero values seen at get_pe_mac(0,0) (first 20 at most): ";
        int cnt = 0;
        for (auto v : seen_values) { std::cout << v << " "; if (++cnt >= 20) break; }
        std::cout << "\n[DEBUG] max_seen=" << max_seen << std::endl;

        std::cout << "\n==================================================" << std::endl;
        if (errors == 0) std::cout << "  [PASS] RTL-REF ARRAY (PORTED) K=8 ACCUM CHECK." << std::endl;
        else std::cout << "  [FAIL] RTL-REF ARRAY (PORTED) K=8 ACCUM CHECK: " << errors << " errors." << std::endl;
        std::cout << "==================================================" << std::endl;

        sc_stop();
    }
};

int sc_main(int argc, char **argv)
{
    sc_clock clk("clk", 10, SC_NS);
    TbRtlRefArrayPortedK8AccumCheck tb("TbRtlRefArrayPortedK8AccumCheck_inst");
    tb.i_clk(clk);
    sc_start();
    return 0;
}
