// OBP configuration path through the RTL-reference core (K = 1): checks that the enable bits and the host-MMIO
// programming of the LUT / Bias / Scale / Shift RAMs work at the splice point of the core, using the formulas
// and scenarios of tb_obp.cpp, but with i_data / i_valid coming from the core (s_psm_sramc_*).
//
// Known limitation: in a single-context run the PSM does not enter its real SRAM-C write path (needs
// ctx_cnt >= 3), so the i_data received here may be 0 (PSM reset / early pulse). The checks do NOT assume 5000;
// they verify the OBP formula for whatever value is received (data_in and data_out sampled at the valid cycle).
#define SC_ALLOW_DEPRECATED_IEEE_API

#include <systemc.h>
#include <iostream>
#include <cmath>

#include "sram/sram_top.h"
#include "control/native_lane_a_core.h"
#include "systolic_array/rtl_ref_sa_array.h"
#include "psm/obp_top.h"

using namespace sauria;

typedef Sram<32, 32, int8_t, int8_t, int32_t, 5056, 5184, 1536> SramT;
typedef sauria_rtl::SystolicArray<32, 32, int8_t, int8_t, int32_t> ArrayT;
typedef RtlRefLaneACoreA<32, 32, int8_t, int8_t, int32_t, 16, 64, 1, 1536> RtlRefCoreT;
typedef Obp<32, 0x00140000, 0x00150000, int32_t, int8_t> ObpT;

SC_MODULE(TbRtlRefObpFunctionsCheck)
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
    sc_signal<psum_vector_t<32, int32_t>> s_sramc_wdata_a, sramc_rdata_a, sramc_wdata_b, sramc_rdata_b;
    sc_signal<uint32_t> s_sramc_addr_a, sramc_addr_b;
    sc_signal<bool> s_sramc_wren_a, sramc_rden_a, sramc_wren_b, sramc_rden_b;
    sc_signal<sramc_mask_t<32>> s_sramc_wmask_a, sramc_wmask_b;

    sc_signal<bool> arr_rstn;
    sc_signal<act_vector_t<32, int8_t>> s_act_arr;
    sc_signal<wei_vector_t<32, int8_t>> s_wei_arr;
    sc_signal<psum_vector_t<32, int32_t>> s_sa_to_psm_c, s_psm_to_sa_c;
    sc_signal<bool> arr_softstall_zero, arr_pop_en_dbg_zero;
    sc_signal<float> arr_threshold;

    sc_signal<psum_vector_t<32, int32_t>> s_psm_sramc_wdata_a;
    sc_signal<uint32_t> s_psm_sramc_addr_a;
    sc_signal<bool> s_psm_sramc_wren_a;
    sc_signal<sramc_mask_t<32>> s_psm_sramc_wmask_a;

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

    // Separate reset for the OBP, not shared with sram_rstn: reset_and_capture() toggles sram_rstn on every
    // call to reset Sram/array/core, which would CLEAR the OBP RAMs (host-programmed bias/scale/shift/lut)
    // if the reset were shared. The OBP is reset only once, at the start.
    sc_signal<bool> obp_rstn;
    sc_signal<bool> obp_valid_out;
    sc_signal<act_vector_t<32, int8_t>> obp_residual;
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

    SC_CTOR(TbRtlRefObpFunctionsCheck)
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

        obp_inst = new ObpT("obp_inst");
        obp_inst->i_clk(i_clk);
        obp_inst->i_rstn(obp_rstn);
        obp_inst->i_data(s_psm_sramc_wdata_a);
        obp_inst->i_addr(s_psm_sramc_addr_a);
        obp_inst->i_wmask(s_psm_sramc_wmask_a);
        obp_inst->i_valid(s_psm_sramc_wren_a);
        obp_inst->i_residual(obp_residual);
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

    static int8_t clamp_i8(double v)
    {
        if (v > 127.0) return 127;
        if (v < -128.0) return -128;
        return static_cast<int8_t>(v);
    }

    // --- Host programming helpers for the OBP (same as tb_obp.cpp) ---
    void obp_host_write(uint32_t addr, uint32_t val)
    {
        host_data_t d;
        d.data.fill(0.0);
        uint32_t region = addr & 0x00FF0000;
        if (region == 0x00140000) // LUT_OFFSET
        {
            d[0] = static_cast<double>(val & 0xFF);
            d[1] = static_cast<double>((val >> 8) & 0xFF);
            d[2] = static_cast<double>((val >> 16) & 0xFF);
            d[3] = static_cast<double>((val >> 24) & 0xFF);
        }
        else
        {
            d[0] = static_cast<double>(val);
        }
        host_mask_t m;
        m.data.fill(true);
        obp_host_addr.write(addr);
        obp_host_wdata.write(d);
        obp_host_wmask.write(m);
        obp_host_wren.write(true);
        obp_host_rden.write(false);
        wait();
        obp_host_wren.write(false);
        wait();
    }

    uint32_t obp_host_read(uint32_t addr)
    {
        obp_host_addr.write(addr);
        obp_host_rden.write(true);
        obp_host_wren.write(false);
        wait();
        wait();
        host_data_t r = obp_host_rdata.read();
        obp_host_rden.write(false);
        wait();
        uint32_t region = addr & 0x00FF0000;
        if (region == 0x00140000)
        {
            return (static_cast<uint32_t>(r[0]) & 0xFF) | ((static_cast<uint32_t>(r[1]) & 0xFF) << 8) |
                   ((static_cast<uint32_t>(r[2]) & 0xFF) << 16) | ((static_cast<uint32_t>(r[3]) & 0xFF) << 24);
        }
        return static_cast<uint32_t>(r[0]);
    }

    // Reset the K = 1 baseline core, pulse i_start, run up to max_cycles; returns true if obp_valid_out rose and
    // stores the data_in / data_out snapshot of that cycle in the two references.
    bool reset_and_capture(psum_vector_t<32, int32_t> &data_in_out,
                            psum_vector_t<32, int32_t> &data_out_out,
                            bool &deadlock_out, int max_cycles = 200)
    {
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

        tick(5);
        sram_rstn.write(true);
        arr_rstn.write(true);
        core_rstn.write(true);
        tick(5);

        std::vector<uint8_t> zero_c(32 * sizeof(int32_t), 0);
        sram->write_bank_data(4, 0, zero_c.data(), zero_c.size());
        int8_t act_pattern[32], wei_pattern[32];
        for (int i = 0; i < 32; i++) { act_pattern[i] = 100; wei_pattern[i] = 50; }
        sram->write_bank_data(2, 0, reinterpret_cast<const uint8_t*>(act_pattern), sizeof(act_pattern));
        sram->write_bank_data(0, 0, reinterpret_cast<const uint8_t*>(wei_pattern), sizeof(wei_pattern));

        core_start.write(true);
        tick();
        core_start.write(false);

        bool saw_valid = false;
        deadlock_out = false;
        for (int c = 0; c < max_cycles; c++)
        {
            tick();
            if (core_deadlock.read()) deadlock_out = true;
            if (obp_valid_out.read() && !saw_valid)
            {
                saw_valid = true;
                data_in_out = s_psm_sramc_wdata_a.read();
                data_out_out = s_sramc_wdata_a.read();
            }
        }
        return saw_valid;
    }

    void run()
    {
        std::cout << "==================================================" << std::endl;
        std::cout << " RTL-REF OBP FUNCTIONS CHECK (K=1) -- each function enabled through the real core" << std::endl;
        std::cout << " Note: the received input may be 0 (the PSM real write path needs ctx_cnt>=3)" << std::endl;
        std::cout << " -- the checks do NOT assume an input value, they verify the OBP formula." << std::endl;
        std::cout << "==================================================" << std::endl;

        psum_vector_t<32, int32_t> din, dout;
        bool dl;

        // obp_rstn starts FALSE: release the OBP from reset exactly ONCE here (separate from sram_rstn, see the
        // obp_rstn declaration) before the first obp_host_write, and never reset it again in this test --
        // every reset_and_capture() resets Sram/array/core only, not obp_rstn.
        obp_rstn.write(true);
        tick(2);

        // ---------------- CASE 10: bias_en qua core that ----------------
        std::cout << "\n--- CASE 10: bias_en qua core that ---" << std::endl;
        obp_residual.write(act_vector_t<32, int8_t>());
        obp_bias_en.write(false); obp_requant_en.write(false); obp_lut_en.write(false);
        obp_residual_en.write(false); obp_vec_channel_mode.write(false);
        obp_requant_scale.write(0); obp_requant_shift.write(0);
        obp_host_addr.write(0); obp_host_wren.write(false); obp_host_rden.write(false);
        obp_host_wdata.write(host_data_t()); obp_host_wmask.write(host_mask_t());
        for (int i = 0; i < 32; i++) obp_host_write(0x00150000 + i * 4, (i + 1) * 5);
        bool bias_rd_ok = true;
        for (int i = 0; i < 32; i++)
        {
            uint32_t v = obp_host_read(0x00150000 + i * 4);
            if (v != static_cast<uint32_t>((i + 1) * 5)) bias_rd_ok = false;
        }
        check(bias_rd_ok, "host readback of the bias RAM correct after programming obp_inst directly");
        obp_bias_en.write(true);
        bool got_valid10 = reset_and_capture(din, dout, dl);
        check(!dl, "no deadlock (case 10)");
        check(got_valid10, "obp_valid len it nhat 1 lan (case 10)");
        if (got_valid10)
        {
            bool ok = true;
            for (int i = 0; i < 32; i++)
                if (dout[i] != din[i] + static_cast<int32_t>((i + 1) * 5)) ok = false;
            std::cout << "    data_in[0]=" << din[0] << " data_out[0]=" << dout[0]
                      << " (expected data_in[0]+5)" << std::endl;
            check(ok, "bias formula: data_out[i] == data_in[i] + bias[i] on every lane, with the real input received");
        }

        // ---------------- CASE 11: requant_en (per-channel + fallback) ----------------
        std::cout << "\n--- CASE 11: requant_en (per-channel) qua core that ---" << std::endl;
        obp_bias_en.write(false); obp_requant_en.write(false); obp_lut_en.write(false);
        obp_residual_en.write(false); obp_vec_channel_mode.write(false);
        for (int i = 0; i < 32; i++)
        {
            obp_host_write(0x00180000 + i * 4, 200); // scale
            obp_host_write(0x00190000 + i * 4, 4);   // shift
        }
        obp_requant_en.write(true);
        bool got_valid11 = reset_and_capture(din, dout, dl);
        check(!dl, "no deadlock (case 11)");
        check(got_valid11, "obp_valid len it nhat 1 lan (case 11)");
        if (got_valid11)
        {
            bool ok = true;
            for (int i = 0; i < 32; i++)
            {
                int64_t prod = static_cast<int64_t>(din[i]) * 200;
                int32_t exp = clamp_i8(static_cast<double>(prod >> 4));
                if (dout[i] != exp) ok = false;
            }
            std::cout << "    data_in[0]=" << din[0] << " data_out[0]=" << dout[0]
                      << " (expected clamp((data_in[0]*200)>>4))" << std::endl;
            check(ok, "per-channel requant formula (scale=200,shift=4) + saturation clamp");
        }

        // Fallback: the programmed scale/shift RAM (scale_ram_valid[]) must really be cleared to exercise the
        // fallback path; reset_and_capture() does not touch obp_rstn (see its declaration), so it is toggled
        // here by hand (deliberately, only this once).
        std::cout << "  Fallback to the global scalar port (RAM cleared through obp_rstn)..." << std::endl;
        obp_rstn.write(false);
        tick(2);
        obp_rstn.write(true);
        tick(2);
        obp_requant_scale.write(50); obp_requant_shift.write(3);
        bool got_valid11b = reset_and_capture(din, dout, dl);
        check(!dl, "no deadlock (case 11 fallback)");
        check(got_valid11b, "obp_valid len it nhat 1 lan (case 11 fallback)");
        if (got_valid11b)
        {
            bool ok = true;
            for (int i = 0; i < 32; i++)
            {
                int64_t prod = static_cast<int64_t>(din[i]) * 50;
                int32_t exp = clamp_i8(static_cast<double>(prod >> 3));
                if (dout[i] != exp) ok = false;
            }
            std::cout << "    data_in[0]=" << din[0] << " data_out[0]=" << dout[0]
                      << " (expected clamp((data_in[0]*50)>>3), scalar fallback)" << std::endl;
            check(ok, "requant fallback formula (scalar scale=50,shift=3) + clamp");
        }

        // ---------------- CASE 12: lut_en qua core that ----------------
        std::cout << "\n--- CASE 12: lut_en (ham abs) qua core that ---" << std::endl;
        obp_requant_en.write(false);
        for (int idx = 0; idx < 256; idx += 4)
        {
            uint32_t val = 0;
            for (int i = 0; i < 4; i++)
            {
                int8_t x = static_cast<int8_t>((idx + i) - 128);
                uint8_t y = std::abs((int)x) > 127 ? 127 : std::abs((int)x);
                val |= (static_cast<uint32_t>(y) << (i * 8));
            }
            obp_host_write(0x00140000 + idx, val);
        }
        obp_lut_en.write(true);
        bool got_valid12 = reset_and_capture(din, dout, dl);
        check(!dl, "no deadlock (case 12)");
        check(got_valid12, "obp_valid len it nhat 1 lan (case 12)");
        if (got_valid12)
        {
            int8_t x = clamp_i8(static_cast<double>(din[0]));
            int32_t exp = std::abs((int)x) > 127 ? 127 : std::abs((int)x);
            std::cout << "    data_in[0]=" << din[0] << " (clamp=" << (int)x << ") data_out[0]=" << dout[0]
                      << " (expected abs=" << exp << ")" << std::endl;
            check(dout[0] == exp, "LUT (lane 0, abs function) matches the real input received through the real core");
        }

        // ---------------- CASE 13: residual_en qua core that ----------------
        std::cout << "\n--- CASE 13: residual_en qua core that ---" << std::endl;
        obp_lut_en.write(false);
        act_vector_t<32, int8_t> res_vec;
        for (int i = 0; i < 32; i++) res_vec[i] = static_cast<int8_t>((i % 20) + 1);
        obp_residual.write(res_vec);
        obp_residual_en.write(true);
        bool got_valid13 = reset_and_capture(din, dout, dl);
        check(!dl, "no deadlock (case 13)");
        check(got_valid13, "obp_valid len it nhat 1 lan (case 13)");
        if (got_valid13)
        {
            // Note (obp_top.h stage 4): residual_en ALONE (requant_en = lut_en = false) is NOT clamped at the
            // end -- the branch "else { out_val[l] = static_cast<T_PSUM>(val); }" applies clamp_val<T_ACT> only
            // when (requant_en || lut_en) is true.
            bool ok = true;
            for (int i = 0; i < 32; i++)
            {
                int32_t exp = din[i] + static_cast<int32_t>(res_vec[i]); // no clamp (residual only)
                if (dout[i] != exp) ok = false;
            }
            std::cout << "    data_in[0]=" << din[0] << " residual[0]=" << (int)res_vec[0]
                      << " data_out[0]=" << dout[0] << std::endl;
            check(ok, "residual formula: data_out[i] == data_in[i] + residual[i] (no clamp, "
                      "residual_en alone does not enable the clamp branch of obp_top.h stage 4)");
        }

        // ---------------- CASE 14: all 4 at once through the real core ----------------
        std::cout << "\n--- CASE 14: bias+requant+lut+residual AT ONCE through the real core ---" << std::endl;
        obp_bias_en.write(true); obp_requant_en.write(true); obp_lut_en.write(true); obp_residual_en.write(true);
        for (int i = 0; i < 32; i++) obp_host_write(0x00150000 + i * 4, 10); // bias +10
        for (int i = 0; i < 32; i++)
        {
            obp_host_write(0x00180000 + i * 4, 128); // scale
            obp_host_write(0x00190000 + i * 4, 8);   // shift (chia 2)
        }
        for (int idx = 0; idx < 256; idx += 4) // ReLU
        {
            uint32_t val = 0;
            for (int i = 0; i < 4; i++)
            {
                int8_t x = static_cast<int8_t>((idx + i) - 128);
                uint8_t y = (x > 0) ? (uint8_t)x : 0;
                val |= (static_cast<uint32_t>(y) << (i * 8));
            }
            obp_host_write(0x00140000 + idx, val);
        }
        act_vector_t<32, int8_t> res_vec2(5);
        obp_residual.write(res_vec2);
        bool got_valid14 = reset_and_capture(din, dout, dl);
        check(!dl, "no deadlock (case 14)");
        check(got_valid14, "obp_valid rose at least once (case 14)");
        if (got_valid14)
        {
            // Formula as in obp_top.h's pipeline_process(): bias -> requant(scale/shift+clamp)
            // -> lut(ReLU, indexed by the requantized/clamped value) -> residual + final clamp.
            int32_t after_bias = din[0] + 10;
            int64_t prod = static_cast<int64_t>(after_bias) * 128;
            int8_t after_requant = clamp_i8(static_cast<double>(prod >> 8));
            uint8_t after_lut = (after_requant > 0) ? (uint8_t)after_requant : 0;
            int32_t exp = clamp_i8(static_cast<double>((int32_t)after_lut) + 5.0);
            std::cout << "    data_in[0]=" << din[0] << " -> +bias=" << after_bias
                      << " -> requant=" << (int)after_requant << " -> relu=" << (int)after_lut
                      << " -> +residual=" << exp << " | data_out[0]=" << dout[0] << std::endl;
            check(dout[0] == exp, "full 4-stage fusion matches the formula, with the real input received from the core");
        }

        std::cout << "\n==================================================" << std::endl;
        if (errors == 0) std::cout << "  [PASS] RTL-REF OBP FUNCTIONS CHECK." << std::endl;
        else std::cout << "  [FAIL] RTL-REF OBP FUNCTIONS CHECK: " << errors << " errors." << std::endl;
        std::cout << "==================================================" << std::endl;

        sc_stop();
    }
};

int sc_main(int argc, char **argv)
{
    sc_clock clk("clk", 10, SC_NS);
    TbRtlRefObpFunctionsCheck tb("TbRtlRefObpFunctionsCheck_inst");
    tb.i_clk(clk);
    sc_start();
    return 0;
}
