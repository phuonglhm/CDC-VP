// Unit test of sauria_rtl::IfmapFeeder + sauria_rtl::WeightFeeder using this tree's Sram (backdoor memory) to
// seed data, with every real port bound. Checks that the RTL-accurate feeders bind and compile and deliver
// SRAM data to o_act_arr / o_wei_arr for basic K = 1. Not a full bit-exact test (the differential test
// against NativeLaneACoreA covers that).
#define SC_ALLOW_DEPRECATED_IEEE_API

#include <systemc.h>
#include <iostream>
#include <vector>

#include "sram/sram_top.h"
#include "control/rtl_ref_defaults.h"
#include "data_feeder/rtl_ref_ifmap_feeder.h"
#include "data_feeder/rtl_ref_wei_feeder.h"

using namespace sauria;
using namespace sauria_rtl;

typedef Sram<32, 32, int8_t, int8_t, int32_t, 5056, 5184, 1536> SramT;
typedef IfmapFeeder<32, int8_t, 5056, 16> IfmapFeederT;
typedef WeightFeeder<32, int8_t, 5184, 16, 0> WeightFeederT;

SC_MODULE(TbRtlRefFeederUnitCheck)
{
    sc_in<bool> i_clk;

    // --- SRAM (real, backdoor-seeded, not part of the port) ---
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
    // B-side / SRAM-C ports of Sram are unused here but must be bound (SystemC requires all ports bound).
    sc_signal<uint32_t> srama_addr_b, sramb_addr_b;
    sc_signal<bool> srama_rden_b, sramb_rden_b;
    sc_signal<act_vector_t<32, int8_t>> srama_data_b;
    sc_signal<wei_vector_t<32, int8_t>> sramb_data_b;
    sc_signal<psum_vector_t<32, int32_t>> sramc_wdata_a, sramc_rdata_a, sramc_wdata_b, sramc_rdata_b;
    sc_signal<uint32_t> sramc_addr_a, sramc_addr_b;
    sc_signal<bool> sramc_wren_a, sramc_rden_a, sramc_wren_b, sramc_rden_b;
    sc_signal<sramc_mask_t<32>> sramc_wmask_a, sramc_wmask_b;

    // --- IfmapFeeder ports ---
    sc_signal<bool> ia_rstn, ia_feeder_en, ia_feeder_clear, ia_start, ia_valid, ia_finalpush;
    sc_signal<bool> ia_cnt_en, ia_cnt_clear, ia_clearfifo, ia_pop_en, ia_pipeline_en, ia_finalctx;
    sc_signal<uint32_t> ia_incntlim, ia_incntstep, ia_outcntlim, ia_outcntstep;
    sc_signal<sc_bv<64>> ia_dil_pat;
    sc_signal<sramc_mask_t<32>> ia_rows_active;
    sc_signal<act_vector_t<32, uint32_t>> ia_loc_woffs;
    sc_signal<uint32_t> ia_xlim, ia_xstep, ia_ylim, ia_ystep, ia_chlim, ia_chstep;
    sc_signal<uint32_t> ia_til_xlim, ia_til_xstep, ia_til_ylim, ia_til_ystep;
    sc_signal<uint32_t> ia_context_id, ia_ncontexts, ia_reps, ia_mvm_k, ia_base_addr;
    sc_signal<act_vector_t<32, int8_t>> ia_act_arr;
    sc_signal<bool> ia_act_done, ia_act_til_done, ia_fifo_empty, ia_fifo_full, ia_stall;

    // --- WeightFeeder ports ---
    sc_signal<bool> wa_rstn, wa_feeder_en, wa_feeder_clear, wa_start, wa_valid, wa_finalpush;
    sc_signal<bool> wa_cnt_en, wa_cnt_clear, wa_clearfifo, wa_pop_en, wa_pipeline_en, wa_cswitch;
    sc_signal<uint32_t> wa_incntlim, wa_incntstep, wa_base_addr;
    sc_signal<uint32_t> wa_wlim, wa_wstep, wa_klim, wa_kstep, wa_til_klim, wa_til_kstep;
    sc_signal<uint64_t> wa_cols_active;
    sc_signal<uint32_t> wa_waligned, wa_context_id, wa_ncontexts, wa_out_tile_id, wa_mvm_k;
    sc_signal<wei_vector_t<32, int8_t>> wa_wei_arr;
    sc_signal<bool> wa_done, wa_til_done, wa_fifo_empty, wa_fifo_full, wa_stall;

    SramT *sram;
    IfmapFeederT *act_feeder;
    WeightFeederT *wei_feeder;
    int errors = 0;

    SC_CTOR(TbRtlRefFeederUnitCheck)
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

        act_feeder = new IfmapFeederT("act_feeder");
        act_feeder->i_clk(i_clk);
        act_feeder->i_rstn(ia_rstn);
        act_feeder->i_feeder_en(ia_feeder_en);
        act_feeder->i_feeder_clear(ia_feeder_clear);
        act_feeder->i_start(ia_start);
        act_feeder->i_valid(ia_valid);
        act_feeder->i_finalpush(ia_finalpush);
        act_feeder->i_cnt_en(ia_cnt_en);
        act_feeder->i_cnt_clear(ia_cnt_clear);
        act_feeder->i_clearfifo(ia_clearfifo);
        act_feeder->i_pop_en(ia_pop_en);
        act_feeder->i_pipeline_en(ia_pipeline_en);
        act_feeder->i_finalctx(ia_finalctx);
        act_feeder->i_act_incntlim(ia_incntlim);
        act_feeder->i_act_incntstep(ia_incntstep);
        act_feeder->i_act_outcntlim(ia_outcntlim);
        act_feeder->i_act_outcntstep(ia_outcntstep);
        act_feeder->i_act_dil_pat(ia_dil_pat);
        act_feeder->i_rows_active(ia_rows_active);
        act_feeder->i_loc_woffs(ia_loc_woffs);
        act_feeder->i_act_xlim(ia_xlim);
        act_feeder->i_act_xstep(ia_xstep);
        act_feeder->i_act_ylim(ia_ylim);
        act_feeder->i_act_ystep(ia_ystep);
        act_feeder->i_act_chlim(ia_chlim);
        act_feeder->i_act_chstep(ia_chstep);
        act_feeder->i_act_til_xlim(ia_til_xlim);
        act_feeder->i_act_til_xstep(ia_til_xstep);
        act_feeder->i_act_til_ylim(ia_til_ylim);
        act_feeder->i_act_til_ystep(ia_til_ystep);
        act_feeder->i_context_id(ia_context_id);
        act_feeder->i_ncontexts(ia_ncontexts);
        act_feeder->i_act_reps(ia_reps);
        act_feeder->i_mvm_k(ia_mvm_k);
        act_feeder->o_srama_addr(srama_addr_a);
        act_feeder->o_srama_rden(srama_rden_a);
        act_feeder->i_srama_data(srama_data_a);
        act_feeder->i_act_base_addr(ia_base_addr);
        act_feeder->o_act_arr(ia_act_arr);
        act_feeder->o_act_done(ia_act_done);
        act_feeder->o_act_til_done(ia_act_til_done);
        act_feeder->o_fifo_empty(ia_fifo_empty);
        act_feeder->o_fifo_full(ia_fifo_full);
        act_feeder->o_stall(ia_stall);

        wei_feeder = new WeightFeederT("wei_feeder");
        wei_feeder->i_clk(i_clk);
        wei_feeder->i_rstn(wa_rstn);
        wei_feeder->i_feeder_en(wa_feeder_en);
        wei_feeder->i_feeder_clear(wa_feeder_clear);
        wei_feeder->i_start(wa_start);
        wei_feeder->i_valid(wa_valid);
        wei_feeder->i_finalpush(wa_finalpush);
        wei_feeder->i_cnt_en(wa_cnt_en);
        wei_feeder->i_cnt_clear(wa_cnt_clear);
        wei_feeder->i_clearfifo(wa_clearfifo);
        wei_feeder->i_pop_en(wa_pop_en);
        wei_feeder->i_pipeline_en(wa_pipeline_en);
        wei_feeder->i_cswitch(wa_cswitch);
        wei_feeder->i_wei_incntlim(wa_incntlim);
        wei_feeder->i_wei_incntstep(wa_incntstep);
        wei_feeder->i_wei_base_addr(wa_base_addr);
        wei_feeder->i_wei_wlim(wa_wlim);
        wei_feeder->i_wei_wstep(wa_wstep);
        wei_feeder->i_wei_klim(wa_klim);
        wei_feeder->i_wei_kstep(wa_kstep);
        wei_feeder->i_wei_til_klim(wa_til_klim);
        wei_feeder->i_wei_til_kstep(wa_til_kstep);
        wei_feeder->i_wei_cols_active(wa_cols_active);
        wei_feeder->i_wei_waligned(wa_waligned);
        wei_feeder->i_context_id(wa_context_id);
        wei_feeder->i_ncontexts(wa_ncontexts);
        wei_feeder->i_out_tile_id(wa_out_tile_id);
        wei_feeder->i_mvm_k(wa_mvm_k);
        wei_feeder->o_sramb_addr(sramb_addr_a);
        wei_feeder->o_sramb_rden(sramb_rden_a);
        wei_feeder->i_sramb_data(sramb_data_a);
        wei_feeder->o_wei_arr(wa_wei_arr);
        wei_feeder->o_wei_done(wa_done);
        wei_feeder->o_wei_til_done(wa_til_done);
        wei_feeder->o_fifo_empty(wa_fifo_empty);
        wei_feeder->o_fifo_full(wa_fifo_full);
        wei_feeder->o_stall(wa_stall);

        SC_THREAD(run);
        sensitive << i_clk.pos();
    }

    void tick(int n = 1)
    {
        for (int i = 0; i < n; i++)
        {
            wait();
            act_feeder->seam_step();
            wei_feeder->seam_step();
        }
    }

    void check(bool cond, const std::string &msg)
    {
        if (cond) { std::cout << "  [PASS] " << msg << std::endl; }
        else { std::cout << "  [FAIL] " << msg << std::endl; errors++; }
    }

    void run()
    {
        std::cout << "==================================================" << std::endl;
        std::cout << " RTL-REF FEEDER UNIT CHECK (standalone, real Sram seeded through the backdoor)" << std::endl;
        std::cout << "==================================================" << std::endl;

        // Reset SRAM + feeders
        sram_rstn.write(false);
        sram_deepsleep.write(false);
        sram_powergate.write(false);
        sram_select.write(0);
        host_wren.write(false);
        host_rden.write(false);
        ia_rstn.write(false);
        wa_rstn.write(false);
        ia_feeder_en.write(false); ia_feeder_clear.write(true); ia_start.write(false);
        ia_valid.write(false); ia_finalpush.write(false); ia_cnt_en.write(false);
        ia_cnt_clear.write(true); ia_clearfifo.write(true); ia_pop_en.write(false);
        ia_pipeline_en.write(false); ia_finalctx.write(false);
        ia_incntlim.write(0); ia_incntstep.write(1); ia_outcntlim.write(0); ia_outcntstep.write(1);
        ia_dil_pat.write(sc_bv<64>(~0ULL)); // RTL default for finalpush is ALL-ONES
        ia_rows_active.write(sramc_mask_t<32>(true)); // enable all 32 rows -- the default is false (no row active)
        ia_loc_woffs.write(act_vector_t<32, uint32_t>());
        ia_xlim.write(1); ia_xstep.write(1); ia_ylim.write(1); ia_ystep.write(1);
        ia_chlim.write(1); ia_chstep.write(1);
        ia_til_xlim.write(1); ia_til_xstep.write(1); ia_til_ylim.write(1); ia_til_ystep.write(1);
        ia_context_id.write(0); ia_ncontexts.write(1); ia_reps.write(1); ia_mvm_k.write(1);
        ia_base_addr.write(0);

        wa_feeder_en.write(false); wa_feeder_clear.write(true); wa_start.write(false);
        wa_valid.write(false); wa_finalpush.write(false); wa_cnt_en.write(false);
        wa_cnt_clear.write(true); wa_clearfifo.write(true); wa_pop_en.write(false);
        wa_pipeline_en.write(false); wa_cswitch.write(false);
        wa_incntlim.write(0); wa_incntstep.write(1); wa_base_addr.write(0);
        wa_wlim.write(1); wa_wstep.write(1); wa_klim.write(1); wa_kstep.write(1);
        wa_til_klim.write(1); wa_til_kstep.write(1);
        wa_cols_active.write(~0ULL);
        wa_waligned.write(1); wa_context_id.write(0); wa_ncontexts.write(1);
        wa_out_tile_id.write(0); wa_mvm_k.write(1);

        tick(5);
        sram_rstn.write(true);
        ia_rstn.write(true);
        wa_rstn.write(true);
        tick(5);

        check(!errors, "construction without error");

        // Seed SRAM-A (act, bank 2) va SRAM-B (wei, bank 0) qua backdoor -- giong pattern K=1 da
        // dung xuyen suot Phase 1 (test_lane_a_*_check.cpp): act=100, wei=50 tai offset 0.
        int8_t act_pattern[32];
        int8_t wei_pattern[32];
        for (int i = 0; i < 32; i++) { act_pattern[i] = 100; wei_pattern[i] = 50; }
        sram->write_bank_data(2, 0, reinterpret_cast<const uint8_t*>(act_pattern), sizeof(act_pattern));
        sram->write_bank_data(0, 0, reinterpret_cast<const uint8_t*>(wei_pattern), sizeof(wei_pattern));

        std::cout << "\n[START] feeder_en + start + cnt_en + pop cycle" << std::endl;
        ia_feeder_en.write(true); ia_feeder_clear.write(false);
        wa_feeder_en.write(true); wa_feeder_clear.write(false);
        ia_incntlim.write(0); wa_incntlim.write(0); // K=1: incntlim=0 (1 value)
        tick();
        ia_start.write(true); wa_start.write(true);
        ia_cnt_en.write(true); wa_cnt_en.write(true);
        ia_cnt_clear.write(false); wa_cnt_clear.write(false);
        ia_clearfifo.write(false); wa_clearfifo.write(false);
        ia_valid.write(true); wa_valid.write(true);
        ia_pipeline_en.write(true); wa_pipeline_en.write(true);
        tick();
        ia_start.write(false); wa_start.write(false);

        bool saw_act_data = false, saw_wei_data = false;
        int8_t last_act0 = 0, last_wei0 = 0;

        const int TOTAL_CYCLES = 60;
        for (int c = 0; c < TOTAL_CYCLES; c++)
        {
            ia_pop_en.write(true);
            wa_pop_en.write(true);
            tick();

            act_vector_t<32, int8_t> a = ia_act_arr.read();
            wei_vector_t<32, int8_t> w = wa_wei_arr.read();
            if (a[0] != 0) { saw_act_data = true; last_act0 = a[0]; }
            if (w[0] != 0) { saw_wei_data = true; last_wei0 = w[0]; }

            if (c < 15)
            {
                std::cout << "  [cyc " << c << "] act_arr[0]=" << (int)a[0]
                          << " wei_arr[0]=" << (int)w[0]
                          << " act_fifo_empty=" << ia_fifo_empty.read()
                          << " wei_fifo_empty=" << wa_fifo_empty.read()
                          << " act_stall=" << ia_stall.read()
                          << " wei_stall=" << wa_stall.read()
                          << std::endl;
            }
        }

        check(saw_act_data, "at least 1 cycle with o_act_arr[0] != 0 (data from SRAM-A reaches the feeder output)");
        check(saw_wei_data, "at least 1 cycle with o_wei_arr[0] != 0 (data from SRAM-B reaches the feeder output)");
        if (saw_act_data) check(last_act0 == 100, "last observed act_arr[0] = 100 (the seeded pattern)");
        if (saw_wei_data) check(last_wei0 == 50, "last observed wei_arr[0] = 50 (the seeded pattern)");

        std::cout << "\n==================================================" << std::endl;
        if (errors == 0) std::cout << "  [PASS] RTL-REF FEEDER UNIT CHECK." << std::endl;
        else std::cout << "  [FAIL] RTL-REF FEEDER UNIT CHECK: " << errors << " errors." << std::endl;
        std::cout << "==================================================" << std::endl;

        sc_stop();
    }
};

int sc_main(int argc, char **argv)
{
    sc_clock clk("clk", 10, SC_NS);
    TbRtlRefFeederUnitCheck tb("TbRtlRefFeederUnitCheck_inst");
    tb.i_clk(clk);
    sc_start();
    return 0;
}
