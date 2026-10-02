// Lane-A single-lane wiring, end-to-end step: SRAM-A/B (sram_inst) -> act_feeder_a/wei_feeder_a -> array_inst
// -> psm_inst_a -> obp_inst_a -> SRAM-C (bank 4, read back with sram_inst->read_bank_data()). Confirms the
// full data path end to end.
//
// Scope: K = 1, OBP_CFG_A = 0 (passthrough). Expected: SRAM-C bank 4 offset 0 holds 100*50 = 5000 (int32)
// after o_done. A bit-exact comparison with emulate_gemm_fused() (quantize/activate) needs real OBP
// requant/bias parameters and is not part of this test.
#define SC_ALLOW_DEPRECATED_IEEE_API

#include <systemc.h>
#include <iostream>
#include <vector>

#include "sauria_types.h"
#include "config_map.h"
#include "npu_top.h"

using namespace sauria;

typedef NpuTop<32, 32, int8_t, int8_t, int32_t, 16, 64, 1> NpuTestT;

SC_MODULE(TbLaneAE2eCheck)
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

    SC_CTOR(TbLaneAE2eCheck)
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
        std::cout << " STEP 3f: end-to-end real SRAM-C readback (K=1, 32x32, 1 lane)" << std::endl;
        std::cout << " Expected: SRAM-C bank 4 offset 0 holds " << EXPECTED << " (int32) after o_done" << std::endl;
        std::cout << "==================================================" << std::endl;

        std::vector<int8_t> act_pattern(K), wei_pattern(K);
        for (int i = 0; i < K; i++) { act_pattern[i] = static_cast<int8_t>(100 + i); wei_pattern[i] = static_cast<int8_t>(50 + i); }

        reset_dut();

        // Clear SRAM-C bank 4 first so that the value read back is the one the pipeline wrote, not stale data.
        std::vector<uint8_t> zero_c(32 * sizeof(int32_t), 0);
        dut->sram_inst->write_bank_data(4, 0, zero_c.data(), zero_c.size());

        dut->sram_inst->write_bank_data(2, 0, reinterpret_cast<const uint8_t*>(act_pattern.data()), act_pattern.size());
        dut->sram_inst->write_bank_data(0, 0, reinterpret_cast<const uint8_t*>(wei_pattern.data()), wei_pattern.size());

        configure_minimal_feeder_limits();
        wait(5);

        std::cout << "\n[PULSE] i_start = true" << std::endl;
        start.write(true);
        wait();
        start.write(false);

        int timeout = 0;
        const int TIMEOUT_CYCLES = 5000;
        bool saw_done = false;

        while (timeout < TIMEOUT_CYCLES)
        {
            wait();
            timeout++;
            if (deadlock.read()) { std::cout << "  [FAIL] deadlock o cycle " << timeout << std::endl; errors++; break; }
            if (done.read()) { saw_done = true; std::cout << "  [INFO] o_done o cycle " << timeout << std::endl; break; }
        }

        // Wait a few cycles for the OBP to drain its 4-stage pipeline after ctrl reports done, then read.
        wait(10);

        int32_t c_readback[4] = {0, 0, 0, 0};
        dut->sram_inst->read_bank_data(4, 0, reinterpret_cast<uint8_t*>(c_readback), sizeof(c_readback));

        std::cout << "\n[SRAM-C READBACK] bank=4 offset=0 lanes[0..3] = "
                  << c_readback[0] << "," << c_readback[1] << "," << c_readback[2] << "," << c_readback[3]
                  << std::endl;

        check(saw_done, "completes before timeout");
        check(!deadlock.read(), "no deadlock");
        check(c_readback[0] == EXPECTED, "real SRAM-C (bank 4) holds the expected value " + std::to_string(EXPECTED));

        std::cout << "\n==================================================" << std::endl;
        if (errors == 0) std::cout << "  [PASS] STEP 3f." << std::endl;
        else std::cout << "  [FAIL] STEP 3f: " << errors << " errors." << std::endl;
        std::cout << "==================================================" << std::endl;

        sc_stop();
    }
};

int sc_main(int argc, char **argv)
{
    sc_clock clk("clk", 10, SC_NS);
    TbLaneAE2eCheck tb("TbLaneAE2eCheck_inst");
    tb.i_clk(clk);
    sc_start();
    return 0;
}
