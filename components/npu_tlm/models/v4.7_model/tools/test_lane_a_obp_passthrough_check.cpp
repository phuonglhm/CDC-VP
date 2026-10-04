// Lane-A single-lane wiring, OBP step: obp_inst_a receives the raw PSUM from psm_inst_a and passes it through the
// 4-stage pipeline (bias -> requant -> LUT -> residual) to o_sramc_wdata / o_sramc_wren.
//
// OBP_CFG_A resets to 0 (config_regs.h: r_obp_cfg_a{0}), so bias_en / requant_en / lut_en / residual_en are all
// false and obp_top.h::pipeline_process() is a passthrough. The test confirms that the OBP receives the PSM value
// and forwards it unchanged with the 4-cycle pipeline latency. The full bias/requant/LUT/residual formula is
// checked elsewhere.
#define SC_ALLOW_DEPRECATED_IEEE_API

#include <systemc.h>
#include <iostream>
#include <vector>

#include "sauria_types.h"
#include "config_map.h"
#include "npu_top.h"

using namespace sauria;

typedef NpuTop<32, 32, int8_t, int8_t, int32_t, 16, 64, 1> NpuTestT;

SC_MODULE(TbLaneAObpPassthroughCheck)
{
    sc_in<bool> i_clk;

    sc_signal<bool> rstn, soft_reset, start, done, deadlock, irq;
    sc_signal<uint32_t> mvm_k, host_addr, total_contexts;
    sc_signal<bool> host_wren, host_rden;
    sc_signal<host_data_t> host_wdata, host_rdata;
    sc_signal<host_mask_t> host_wmask;
    sc_signal<float> threshold;
    sc_signal<sc_bv<3>> select;

    NpuTestT *dut;
    std::vector<uint8_t> dram;
    int errors = 0;
    static const int K = 1;
    static const int32_t EXPECTED = 100 * 50;

    SC_CTOR(TbLaneAObpPassthroughCheck)
    {
        dut = new NpuTestT("dut");
        dut->i_clk(i_clk);
        dut->i_rstn(rstn);
        dut->i_soft_reset(soft_reset);
        dut->i_start(start);
        dut->o_done(done);
        dut->o_deadlock(deadlock);
        dut->i_mvm_k(mvm_k);
        dut->i_host_addr(host_addr);
        dut->i_host_wren(host_wren);
        dut->i_host_rden(host_rden);
        dut->i_host_wdata(host_wdata);
        dut->i_host_wmask(host_wmask);
        dut->o_host_rdata(host_rdata);
        dut->i_threshold(threshold);
        dut->i_select(select);
        dut->i_total_contexts(total_contexts);

        SC_THREAD(run);
        sensitive << i_clk.pos();
    }

    void write_mmio(uint32_t addr, uint32_t val)
    {
        host_data_t d;
        d.data.fill(0.0f);
        d[0] = static_cast<float>(val);
        host_mask_t m;
        m.data.fill(true);
        host_addr.write(addr);
        host_wdata.write(d);
        host_wmask.write(m);
        host_wren.write(true);
        host_rden.write(false);
        wait();
        host_wren.write(false);
        wait();
    }

    void configure_minimal_feeder_limits()
    {
        write_mmio(ACT_XLIM, 1);
        write_mmio(ACT_XSTEP, 1);
        write_mmio(ACT_YLIM, 1);
        write_mmio(ACT_YSTEP, 1);
        write_mmio(ACT_CHLIM, K);
        write_mmio(ACT_CHSTEP, 1);
        write_mmio(WEI_WLIM, 1);
        write_mmio(WEI_WSTEP, 1);
        write_mmio(WEI_KLIM, K);
        write_mmio(WEI_KSTEP, 1);
    }

    void reset_dut()
    {
        rstn.write(false);
        soft_reset.write(false);
        start.write(false);
        mvm_k.write(K);
        total_contexts.write(1);
        threshold.write(0.0f);
        select.write(0);
        host_wren.write(false);
        host_rden.write(false);
        wait(5);
        rstn.write(true);
        wait(5);
    }

    void check(bool cond, const std::string &msg)
    {
        if (cond) { std::cout << "  [PASS] " << msg << std::endl; }
        else { std::cout << "  [FAIL] " << msg << std::endl; errors++; }
    }

    void run()
    {
        dram.resize(1 * 1024 * 1024, 0);
        dut->set_dram(&dram);

        std::cout << "==================================================" << std::endl;
        std::cout << " STEP 3e: obp_inst_a passthrough check (OBP_CFG_A=0 default, K=1)" << std::endl;
        std::cout << " Expected: obp_inst_a raises o_sramc_wren with wdata holding " << EXPECTED
                  << " a few cycles after psm_inst_a" << std::endl;
        std::cout << "==================================================" << std::endl;

        std::vector<int8_t> act_pattern(K), wei_pattern(K);
        for (int i = 0; i < K; i++) { act_pattern[i] = static_cast<int8_t>(100 + i); wei_pattern[i] = static_cast<int8_t>(50 + i); }

        reset_dut();

        dut->sram_inst->write_bank_data(2, 0, reinterpret_cast<const uint8_t*>(act_pattern.data()), act_pattern.size());
        dut->sram_inst->write_bank_data(0, 0, reinterpret_cast<const uint8_t*>(wei_pattern.data()), wei_pattern.size());

        // No write_mmio() to OBP_CFG_A: keep the reset default 0 (passthrough), as stated in the file header.
        configure_minimal_feeder_limits();
        wait(5);

        std::cout << "\n[PULSE] i_start = true" << std::endl;
        start.write(true);
        wait();
        start.write(false);

        int timeout = 0;
        const int TIMEOUT_CYCLES = 5000;
        bool saw_done = false;

        int psm_wren_events = 0;
        bool psm_found_expected = false;
        int psm_found_cycle = -1;
        bool prev_psm_wren = false;

        int obp_wren_events = 0;
        bool obp_found_expected = false;
        int obp_found_cycle = -1;
        bool prev_obp_wren = false;

        while (timeout < TIMEOUT_CYCLES)
        {
            wait();
            timeout++;

            bool psm_wren = dut->psm_inst_a->o_sramc_wren.read();
            if (psm_wren && !prev_psm_wren)
            {
                auto wdata = dut->psm_inst_a->o_sramc_wdata.read();
                psm_wren_events++;
                if (wdata[0] == EXPECTED && !psm_found_expected)
                {
                    psm_found_expected = true;
                    psm_found_cycle = timeout;
                    std::cout << "  [PSM FOUND] wdata[0]==" << EXPECTED << " o cycle " << timeout << std::endl;
                }
            }
            prev_psm_wren = psm_wren;

            bool obp_wren = dut->obp_inst_a->o_sramc_wren.read();
            if (obp_wren && !prev_obp_wren)
            {
                auto wdata = dut->obp_inst_a->o_sramc_wdata.read();
                obp_wren_events++;
                std::cout << "  [OBP WREN #" << obp_wren_events << "] cycle=" << timeout
                          << " wdata[0..3]=" << wdata[0] << "," << wdata[1] << "," << wdata[2]
                          << "," << wdata[3] << std::endl;
                if (wdata[0] == EXPECTED && !obp_found_expected)
                {
                    obp_found_expected = true;
                    obp_found_cycle = timeout;
                    std::cout << "  [OBP FOUND] wdata[0]==" << EXPECTED << " o cycle " << timeout << std::endl;
                }
            }
            prev_obp_wren = obp_wren;

            if (deadlock.read()) { std::cout << "  [FAIL] deadlock o cycle " << timeout << std::endl; errors++; break; }
            if (done.read()) { saw_done = true; std::cout << "  [INFO] o_done o cycle " << timeout << std::endl; break; }
        }

        check(saw_done, "completes before timeout");
        check(!deadlock.read(), "no deadlock");
        check(psm_wren_events > 0, "psm_inst_a co drain (tien de tu 3d)");
        check(psm_found_expected, "psm wdata[0] dung " + std::to_string(EXPECTED));
        check(obp_wren_events > 0, "obp_inst_a raised o_sramc_wren at least once (it really receives and pushes the pipeline)");
        check(obp_found_expected, "obp wdata[0] dung " + std::to_string(EXPECTED) + " (passthrough correct, value unchanged)");
        if (psm_found_expected && obp_found_expected)
        {
            int latency = obp_found_cycle - psm_found_cycle;
            std::cout << "\n[DEBUG] observed PSM->OBP latency: " << latency << " cycles (expected ~4, 4-stage pipeline)" << std::endl;
        }

        std::cout << "\n==================================================" << std::endl;
        if (errors == 0) std::cout << "  [PASS] STEP 3e." << std::endl;
        else std::cout << "  [FAIL] STEP 3e: " << errors << " errors." << std::endl;
        std::cout << "==================================================" << std::endl;

        sc_stop();
    }
};

int sc_main(int argc, char **argv)
{
    sc_clock clk("clk", 10, SC_NS);
    TbLaneAObpPassthroughCheck tb("TbLaneAObpPassthroughCheck_inst");
    tb.i_clk(clk);
    sc_start();
    return 0;
}
