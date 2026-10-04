// Real GEMM through the RTL-reference core (RtlRefLaneACoreA, unmodified): checks that PsmShiftFsm enters the
// real SRAM-C write path, and captures EVERY obp_valid_out pulse to find the one carrying the GEMM result.
// Wiring is taken from tools/test_rtl_ref_obp_functions_check.cpp; only the reset configuration and the
// capture loop differ.
#define SC_ALLOW_DEPRECATED_IEEE_API

#include <systemc.h>
#include <iostream>
#include <vector>

#include "sram/sram_top.h"
#include "control/native_lane_a_core.h"
#include "systolic_array/rtl_ref_sa_array.h"
#include "psm/obp_top.h"

using namespace sauria;

typedef Sram<32, 32, int8_t, int8_t, int32_t, 5056, 5184, 1536> SramT;
typedef sauria_rtl::SystolicArray<32, 32, int8_t, int8_t, int32_t> ArrayT;
typedef RtlRefLaneACoreA<32, 32, int8_t, int8_t, int32_t, 16, 64, 1, 1536> RtlRefCoreT;
typedef Obp<32, 0x00140000, 0x00150000, int32_t, int8_t> ObpT;

SC_MODULE(TbRtlRefMultictxRealGemmCheck)
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

    SC_CTOR(TbRtlRefMultictxRealGemmCheck)
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

    struct ValidPulse { int cycle; uint32_t context_id; int32_t data_in0; int32_t data_out0; };

    // Run total_contexts contexts back to back from one i_start (RtlRefLaneACoreA loops over context_id
    // 0..total_contexts-1 internally), OBP in pure passthrough, and capture EVERY obp_valid_out pulse (not only the
    // first) to see how the received value changes across contexts.
    std::vector<ValidPulse> run_multictx(uint32_t total_contexts, int max_cycles, bool &deadlock_out)
    {
        obp_bias_en.write(false); obp_requant_en.write(false); obp_lut_en.write(false);
        obp_residual_en.write(false); obp_vec_channel_mode.write(false);
        obp_requant_scale.write(0); obp_requant_shift.write(0);
        obp_residual.write(act_vector_t<32, int8_t>());
        obp_host_wren.write(false); obp_host_rden.write(false);

        sram_rstn.write(false); sram_deepsleep.write(false); sram_powergate.write(false); sram_select.write(0);
        host_wren.write(false); host_rden.write(false);
        arr_rstn.write(false);
        arr_threshold.write(0.0f);
        arr_softstall_zero.write(false);
        arr_pop_en_dbg_zero.write(false);

        // All register values below come directly from sauria_compute_core_fields() (driver/libsauria_cfg.h, read-only
        // copy in tools/sauria_ref/, see tools/gen_real_cfg.cpp) for a real shape: 1x1 pointwise conv, 8 input channels
        // (K = 8 through WLIM = 8), 1 output channel, 1 spatial tile, X_used = Y_used = 1, no tiling.
        //
        // Note: `i_wei_klim` drives WeiIdxCnt::Inputs::auxlim, the auxiliary SRAM-word alignment counter; it is NOT the
        // K depth. The K depth is `i_wei_wlim` (WLIM = k_til*B_w*B_h*c_til). For this shape the encoder gives
        // klim = 33, wlim = 8, waligned = 0.
        //
        // preload_en must be true: ARRAY_PREP waits for i_outbuf_done (the PSM's o_done). With preload_en = false the
        // PSM signals done almost immediately, the array leaves ARRAY_PREP before the feeders have filled the FIFO,
        // and the last taps are lost. sauria_model logs show ARRAY_PREP lasting 72 cycles for this shape.
        core_rstn.write(false); core_soft_reset.write(false); core_start.write(false);
        core_mvm_k.write(1); core_total_contexts.write(1); core_nsplit.write(32);
        core_incntlim.write(7); core_act_reps.write(1); core_wei_reps.write(1); core_out_ncontexts.write(1);
        core_act_incntlim.write(7); core_act_incntstep.write(1);
        core_act_outcntlim.write(0); core_act_outcntstep.write(1);
        core_act_dil_pat.write(sc_bv<64>(0x8000000000000000ULL));
        core_act_xlim.write(34); core_act_xstep.write(32); core_act_ylim.write(1); core_act_ystep.write(1);
        core_act_chlim.write(8); core_act_chstep.write(1);
        core_act_til_xlim.write(1); core_act_til_xstep.write(1);
        core_act_til_ylim.write(1); core_act_til_ystep.write(1);
        core_act_base_addr.write(0);
        core_wei_incntlim.write(7); core_wei_incntstep.write(1);
        core_wei_wlim.write(8); core_wei_wstep.write(1); core_wei_klim.write(33); core_wei_kstep.write(32);
        core_wei_til_klim.write(1); core_wei_til_kstep.write(1);
        core_wei_cols_active.write(0x80000000u); core_wei_waligned.write(0); core_wei_base_addr.write(0);
        core_cxlim.write(33); core_cxstep.write(32); core_cklim.write(1); core_ckstep.write(1);
        core_out_til_cylim.write(1); core_out_til_cystep.write(1);
        core_out_til_cklim.write(1); core_out_til_ckstep.write(1);
        core_out_preload_en.write(true); // preload_en = true: all 8 taps accumulate
        core_out_base_addr.write(0);
        { sramc_mask_t<32> ra(false); ra[0] = true; core_rows_active.write(ra); }

        tick(5);
        sram_rstn.write(true);
        arr_rstn.write(true);
        core_rstn.write(true);
        tick(5);

        // SRAM-B address is (aux_idx_ + w_idx_) >> WOFS_W with WOFS_W = clog2(SRAMB_N) = 5: only rows 0/1 are used
        // (aux = 0/32), and w_idx_ (0-7) selects the LANE inside a 32-element row, not another row. A seed that is
        // uniform within a row cannot show which lane contributed.
        //
        // Seed: act = 1 everywhere (so the product equals the weight), wei[lane] = lane on row 0 (0..31) and
        // 32 + lane on row 1 (32..63): every (row, lane) has a unique value that can be decoded from the result.
        std::vector<uint8_t> zero_c(32 * sizeof(int32_t), 0);
        for (int row = 0; row < 64; row++)
            sram->write_bank_data(4, row * 32 * sizeof(int32_t), zero_c.data(), zero_c.size());
        for (int row = 0; row < 64; row++)
        {
            int8_t act_pattern[32], wei_pattern[32];
            for (int i = 0; i < 32; i++)
            {
                act_pattern[i] = 1;
                wei_pattern[i] = (int8_t)((row % 2 == 0) ? i : (32 + i));
            }
            sram->write_bank_data(2, row * 32, reinterpret_cast<const uint8_t*>(act_pattern), sizeof(act_pattern));
            sram->write_bank_data(0, row * 32, reinterpret_cast<const uint8_t*>(wei_pattern), sizeof(wei_pattern));
        }

        core_start.write(true);
        tick();
        core_start.write(false);

        std::vector<ValidPulse> pulses;
        deadlock_out = false;
        for (int c = 0; c < max_cycles; c++)
        {
            tick();
            if (c < 210 || (c % 50 == 0) || core->dbg_psm_start() || obp_valid_out.read())
            {
                std::cout << "  [fifo cyc " << c << "] context_id=" << core_context_id.read()
                          << " pe_mac00=" << array_inst->get_pe_mac(0, 0)
                          << " sa_clear=" << core_sa_clear.read()
                          << " c_arr_raw0=" << s_sa_to_psm_c.read()[0]
                          << " pipeline_en=" << core_pipeline_en.read()
                          << " cscan_en=" << core_cscan_en.read()
                          << " act_empty=" << core->dbg_act_fifo_empty()
                          << " act_full=" << core->dbg_act_fifo_full()
                          << " wei_empty=" << core->dbg_wei_fifo_empty()
                          << " wei_full=" << core->dbg_wei_fifo_full()
                          << " wei_stall_any=" << core->dbg_wei_stall_any()
                          << " wei_full_any=" << core->dbg_wei_full_any()
                          << " wei_valid=" << core->dbg_wei_valid()
                          << " wei_finpush2=" << core->dbg_wei_finalpush_q2()
                          << " wei_ptr0=" << core->dbg_wei_fifo_ptr0()
                          << " wei_elm=" << core->dbg_wei_elm()
                          << " wei_nfree=" << core->dbg_wei_nfree()
                          << " sramb_addr=" << sramb_addr_a.read()
                          << " sramb_rden=" << sramb_rden_a.read()
                          << " wei_arr0=" << (int)s_wei_arr.read()[0]
                          << " act_arr0=" << (int)s_act_arr.read()[0]
                          << " wei_aux=" << core->dbg_wei_aux()
                          << " wei_w=" << core->dbg_wei_w()
                          << " wei_tilk=" << core->dbg_wei_tilk()
                          << " wei_cnten=" << core->dbg_wei_cnten()
                          << " wei_hold=" << core->dbg_ctrl_wei_hold()
                          << " fd_state=" << core->dbg_ctrl_feeders_state()
                          << " psm_ctx_cnt=" << core->dbg_psm_ctx_cnt()
                          << " psm_start=" << core->dbg_psm_start()
                          << " obp_valid=" << obp_valid_out.read()
                          << " deadlock=" << core_deadlock.read()
                          << std::endl;
            }
            if (core_deadlock.read()) { deadlock_out = true; break; }
            if (obp_valid_out.read())
            {
                ValidPulse p;
                p.cycle = c;
                p.context_id = core_context_id.read();
                p.data_in0 = s_psm_sramc_wdata_a.read()[0];
                p.data_out0 = s_sramc_wdata_a.read()[0];
                pulses.push_back(p);
            }
            // Do not stop at core_done: observe every fsm_start / obp_valid pulse in the window, including any the PSM
            // may still emit after the core reports done.
        }
        return pulses;
    }

    void run()
    {
        std::cout << "==================================================" << std::endl;
        std::cout << " RTL-REF MULTICTX REAL GEMM CHECK -- total_contexts=3 qua core THAT," << std::endl;
        std::cout << " checks the ctx_cnt>=3 condition: does PsmShiftFsm enter the real write path" << std::endl;
        std::cout << " after the third context. OBP in pure passthrough (no function enabled)." << std::endl;
        std::cout << "==================================================" << std::endl;

        obp_rstn.write(true);
        tick(2);

        bool dl = false;
        auto pulses = run_multictx(1, 3000, dl);

        check(!dl, "no deadlock with total_contexts=1");
        check(!pulses.empty(), "at least 1 obp_valid_out pulse");

        std::cout << "  obp_valid_out pulses captured: " << pulses.size() << std::endl;
        bool saw_nonzero = false;
        for (auto &p : pulses)
        {
            std::cout << "    [pulse] cycle=" << p.cycle << " context_id=" << p.context_id
                      << " data_in[0]=" << p.data_in0 << " data_out[0]=" << p.data_out0 << std::endl;
            if (p.data_in0 != 0) saw_nonzero = true;
        }
        std::cout << "  [RESULT] data_in[0] != 0 was seen (real GEMM value, not 0/reset): "
                  << (saw_nonzero ? "YES" : "NO") << std::endl;

        std::cout << "\n==================================================" << std::endl;
        if (errors == 0) std::cout << "  [PASS] RTL-REF MULTICTX REAL GEMM CHECK (data collected)." << std::endl;
        else std::cout << "  [FAIL] RTL-REF MULTICTX REAL GEMM CHECK: " << errors << " errors." << std::endl;
        std::cout << "==================================================" << std::endl;

        sc_stop();
    }
};

int sc_main(int argc, char **argv)
{
    sc_clock clk("clk", 10, SC_NS);
    TbRtlRefMultictxRealGemmCheck tb("TbRtlRefMultictxRealGemmCheck_inst");
    tb.i_clk(clk);
    sc_start();
    return 0;
}
