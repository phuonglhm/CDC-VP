// Connects sauria::Obp to the output of the RTL-accurate core (RtlRefLaneACoreA + sauria_rtl::SystolicArray).
// No port is needed: psum_vector_t / sramc_mask_t / act_vector_t are the same types in both cores.
//
// Scope: K = 1 only. OBP in pure PASSTHROUGH: all four enables (bias / requant / lut / residual) false,
// vec_channel_mode false, scale / shift 0. Per obp_top.h::pipeline_process() this mode leaves the value
// unchanged (about 4 cycles of pipeline latency), so SRAM-C must hold 5000 (100*50) as when the PSM writes
// directly. The OBP host-MMIO interface (LUT / Bias / Scale / Shift RAM programming) is tied off here.
#define SC_ALLOW_DEPRECATED_IEEE_API

#include <systemc.h>
#include <iostream>

#include "sram/sram_top.h"
#include "control/native_lane_a_core.h"
#include "systolic_array/rtl_ref_sa_array.h"
#include "psm/obp_top.h"

using namespace sauria;

typedef Sram<32, 32, int8_t, int8_t, int32_t, 5056, 5184, 1536> SramT;
typedef sauria_rtl::SystolicArray<32, 32, int8_t, int8_t, int32_t> ArrayT;
typedef RtlRefLaneACoreA<32, 32, int8_t, int8_t, int32_t, 16, 64, 1, 1536> RtlRefCoreT;
typedef Obp<32, 0x00140000, 0x00150000, int32_t, int8_t> ObpT;

SC_MODULE(TbRtlRefObpSpliceCheck)
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
    // s_sramc_* = REAL input of the Sram (after the OBP). The core writes s_psm_sramc_* (before the OBP); the OBP
    // writes s_sramc_* (after the OBP, into the Sram).
    sc_signal<psum_vector_t<32, int32_t>> s_sramc_wdata_a, sramc_rdata_a, sramc_wdata_b, sramc_rdata_b;
    sc_signal<uint32_t> s_sramc_addr_a, sramc_addr_b;
    sc_signal<bool> s_sramc_wren_a, sramc_rden_a, sramc_wren_b, sramc_rden_b;
    sc_signal<sramc_mask_t<32>> s_sramc_wmask_a, sramc_wmask_b;

    // --- array_inst (ported) ---
    sc_signal<bool> arr_rstn;
    sc_signal<act_vector_t<32, int8_t>> s_act_arr;
    sc_signal<wei_vector_t<32, int8_t>> s_wei_arr;
    sc_signal<psum_vector_t<32, int32_t>> s_sa_to_psm_c, s_psm_to_sa_c;
    sc_signal<bool> arr_softstall_zero, arr_pop_en_dbg_zero;
    sc_signal<float> arr_threshold;

    // --- Splice: signals BEFORE the OBP (the core's raw output, the OBP's input), separate from
    // s_sramc_wdata_a/addr_a/wren_a/wmask_a (above, after the OBP, the Sram's input) so that one signal never has
    // two drivers (core and obp_inst).
    sc_signal<psum_vector_t<32, int32_t>> s_psm_sramc_wdata_a;
    sc_signal<uint32_t> s_psm_sramc_addr_a;
    sc_signal<bool> s_psm_sramc_wren_a;
    sc_signal<sramc_mask_t<32>> s_psm_sramc_wmask_a;

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

    // --- OBP ports (Milestone 1: passthrough thuan, tat ca enable=false) ---
    sc_signal<bool> obp_valid_out;
    sc_signal<act_vector_t<32, int8_t>> obp_residual_zero;
    sc_signal<bool> obp_bias_en, obp_requant_en, obp_lut_en, obp_residual_en, obp_vec_channel_mode;
    sc_signal<uint32_t> obp_requant_scale, obp_requant_shift;
    sc_signal<uint32_t> obp_host_addr;
    sc_signal<bool> obp_host_wren, obp_host_rden;
    sc_signal<host_data_t> obp_host_wdata, obp_host_rdata;
    sc_signal<host_mask_t> obp_host_wmask;

    SramT *sram;
    ArrayT *array_inst;
    RtlRefCoreT *core;
    ObpT *obp_inst;
    int errors = 0;
    static const int32_t EXPECTED_SUM = 5000; // 100*50, K=1 -- same as test_rtl_ref_lane_a_smoke_check.cpp

    SC_CTOR(TbRtlRefObpSpliceCheck)
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
        // SRAM-C receives from the OBP (s_sramc_*), not directly from the core
        sram->i_sramc_wdata_a(s_sramc_wdata_a); sram->i_sramc_addr_a(s_sramc_addr_a); sram->i_sramc_wren_a(s_sramc_wren_a);
        sram->i_sramc_rden_a(sramc_rden_a); sram->i_sramc_wmask_a(s_sramc_wmask_a); sram->o_sramc_rdata_a(sramc_rdata_a);
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
        // The core writes s_psm_sramc_* (BEFORE the OBP) instead of the Sram directly -- this is the SPLICE POINT
        core->o_sramc_addr(s_psm_sramc_addr_a);
        core->o_sramc_wren(s_psm_sramc_wren_a);
        core->o_sramc_rden(sramc_rden_a);
        core->o_sramc_wmask(s_psm_sramc_wmask_a);
        core->o_sramc_wdata(s_psm_sramc_wdata_a);
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

        // --- OBP: input from the core (s_psm_sramc_*, BEFORE the OBP), output to the real Sram (s_sramc_*, AFTER the OBP) ---
        obp_inst = new ObpT("obp_inst");
        obp_inst->i_clk(i_clk);
        obp_inst->i_rstn(sram_rstn); // shares the reset with Sram/array (simplification)
        obp_inst->i_data(s_psm_sramc_wdata_a);
        obp_inst->i_addr(s_psm_sramc_addr_a);
        obp_inst->i_wmask(s_psm_sramc_wmask_a);
        obp_inst->i_valid(s_psm_sramc_wren_a);
        obp_inst->i_residual(obp_residual_zero);
        obp_inst->o_sramc_wdata(s_sramc_wdata_a);
        obp_inst->o_sramc_addr(s_sramc_addr_a);
        obp_inst->o_sramc_wren(s_sramc_wren_a);
        obp_inst->o_sramc_wmask(s_sramc_wmask_a);
        obp_inst->o_valid(obp_valid_out);
        obp_inst->i_bias_en(obp_bias_en);
        obp_inst->i_requant_en(obp_requant_en);
        obp_inst->i_lut_en(obp_lut_en);
        obp_inst->i_residual_en(obp_residual_en);
        obp_inst->i_vec_channel_mode(obp_vec_channel_mode);
        obp_inst->i_requant_scale(obp_requant_scale);
        obp_inst->i_requant_shift(obp_requant_shift);
        obp_inst->i_host_addr(obp_host_addr);
        obp_inst->i_host_wren(obp_host_wren);
        obp_inst->i_host_rden(obp_host_rden);
        obp_inst->i_host_wdata(obp_host_wdata);
        obp_inst->i_host_wmask(obp_host_wmask);
        obp_inst->o_host_rdata(obp_host_rdata);

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
        std::cout << " RTL-REF OBP SPLICE CHECK (K=1, OBP passthrough thuan)" << std::endl;
        std::cout << " Expected: SRAM-C bank4 offset0 = " << EXPECTED_SUM << " after OBP passthrough" << std::endl;
        std::cout << "==================================================" << std::endl;

        sram_rstn.write(false); sram_deepsleep.write(false); sram_powergate.write(false); sram_select.write(0);
        host_wren.write(false); host_rden.write(false);
        arr_rstn.write(false);
        arr_threshold.write(0.0f);
        arr_softstall_zero.write(false);
        arr_pop_en_dbg_zero.write(false);

        core_rstn.write(false); core_soft_reset.write(false); core_start.write(false);
        core_mvm_k.write(1); core_total_contexts.write(1); core_nsplit.write(32);
        core_incntlim.write(0); core_act_reps.write(1); core_wei_reps.write(1); core_out_ncontexts.write(1);
        core_act_incntlim.write(0); core_act_incntstep.write(1);
        core_act_outcntlim.write(0); core_act_outcntstep.write(1);
        core_act_dil_pat.write(sc_bv<64>(~0ULL));
        core_act_xlim.write(1); core_act_xstep.write(1); core_act_ylim.write(1); core_act_ystep.write(1);
        core_act_chlim.write(1); core_act_chstep.write(1);
        core_act_til_xlim.write(1); core_act_til_xstep.write(1);
        core_act_til_ylim.write(1); core_act_til_ystep.write(1);
        core_act_base_addr.write(0);
        core_wei_incntlim.write(0); core_wei_incntstep.write(1);
        core_wei_wlim.write(1); core_wei_wstep.write(1); core_wei_klim.write(1); core_wei_kstep.write(1);
        core_wei_til_klim.write(1); core_wei_til_kstep.write(1);
        core_wei_cols_active.write(0xFFFFFFFFu); core_wei_waligned.write(1); core_wei_base_addr.write(0);
        core_cxlim.write(0); core_cxstep.write(1); core_cklim.write(0); core_ckstep.write(1);
        core_out_til_cylim.write(0); core_out_til_cystep.write(1);
        core_out_til_cklim.write(0); core_out_til_ckstep.write(1);
        core_out_preload_en.write(false);
        core_out_base_addr.write(0);
        core_rows_active.write(sramc_mask_t<32>(true));

        // OBP: passthrough thuan -- tat ca enable = false, scale/shift = 0, residual = 0
        obp_residual_zero.write(act_vector_t<32, int8_t>());
        obp_bias_en.write(false);
        obp_requant_en.write(false);
        obp_lut_en.write(false);
        obp_residual_en.write(false);
        obp_vec_channel_mode.write(false);
        obp_requant_scale.write(0);
        obp_requant_shift.write(0);
        obp_host_addr.write(0);
        obp_host_wren.write(false);
        obp_host_rden.write(false);
        obp_host_wdata.write(host_data_t());
        obp_host_wmask.write(host_mask_t());

        tick(5);
        sram_rstn.write(true);
        arr_rstn.write(true);
        core_rstn.write(true);
        tick(5);

        check(!errors, "construction and binding without error");

        std::vector<uint8_t> zero_c(32 * sizeof(int32_t), 0);
        sram->write_bank_data(4, 0, zero_c.data(), zero_c.size());
        int8_t act_pattern[32], wei_pattern[32];
        for (int i = 0; i < 32; i++) { act_pattern[i] = 100; wei_pattern[i] = 50; }
        sram->write_bank_data(2, 0, reinterpret_cast<const uint8_t*>(act_pattern), sizeof(act_pattern));
        sram->write_bank_data(0, 0, reinterpret_cast<const uint8_t*>(wei_pattern), sizeof(wei_pattern));

        std::cout << "\n[PULSE] i_start = true" << std::endl;
        core_start.write(true);
        tick();
        core_start.write(false);

        bool saw_deadlock = false;
        bool saw_obp_valid = false;
        const int TOTAL_CYCLES = 500;
        for (int c = 0; c < TOTAL_CYCLES; c++)
        {
            tick();
            if (core_deadlock.read()) { saw_deadlock = true; }
            if (obp_valid_out.read()) { saw_obp_valid = true; }

            if (c < 20 || (c % 50 == 0))
            {
                std::cout << "  [cyc " << c << "] active=" << core_active.read()
                          << " done=" << core_done.read()
                          << " sramc_wren(pre-obp)=" << s_psm_sramc_wren_a.read()
                          << " sramc_wren(post-obp)=" << s_sramc_wren_a.read()
                          << " obp_valid_out=" << obp_valid_out.read()
                          << std::endl;
            }
        }

        check(!saw_deadlock, "never deadlocks");
        check(saw_obp_valid, "obp_inst->o_valid rose at least once (data really flows through the OBP)");

        int32_t sramc_val = 0;
        sram->read_bank_data(4, 0, reinterpret_cast<uint8_t*>(&sramc_val), sizeof(sramc_val));
        std::cout << "\n[RESULT] SRAM-C bank4 offset0 = " << sramc_val << " (expected " << EXPECTED_SUM << ")" << std::endl;
        check(sramc_val == EXPECTED_SUM, "SRAM-C equals " + std::to_string(EXPECTED_SUM) +
              " (OBP passthrough leaves the value unchanged)");

        std::cout << "\n==================================================" << std::endl;
        if (errors == 0) std::cout << "  [PASS] RTL-REF OBP SPLICE CHECK." << std::endl;
        else std::cout << "  [FAIL] RTL-REF OBP SPLICE CHECK: " << errors << " errors." << std::endl;
        std::cout << "==================================================" << std::endl;

        sc_stop();
    }
};

int sc_main(int argc, char **argv)
{
    sc_clock clk("clk", 10, SC_NS);
    TbRtlRefObpSpliceCheck tb("TbRtlRefObpSpliceCheck_inst");
    tb.i_clk(clk);
    sc_start();
    return 0;
}
