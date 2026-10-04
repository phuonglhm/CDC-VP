// Core-adapter validation: runs the lane-A end-to-end scenario (K = 1, act = 100 / wei = 50, expected SRAM-C =
// 5000) but observes the PSUM only through sauria::NativeCoreAdapter's public interface (not through
// dut->psm_inst_a / dut->ctrl_inst_a). A PASS shows the adapter interface is sufficient for this check,
// independent of the core behind it.
#define SC_ALLOW_DEPRECATED_IEEE_API

#include <systemc.h>
#include <iostream>
#include <vector>

#include "sauria_types.h"
#include "config_map.h"
#include "npu_top.h"
#include "control/core_adapter.h"

using namespace sauria;

typedef NpuTop<32, 32, int8_t, int8_t, int32_t, 16, 64, 1> NpuTestT;

SC_MODULE(TbPhase2AdapterNativeCheck)
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
    NativeCoreAdapter<NpuTestT> *adapter;
    std::vector<uint8_t> dram;
    int errors = 0;
    static const int K = 1;
    static const int32_t EXPECTED = 100 * 50;

    SC_CTOR(TbPhase2AdapterNativeCheck)
    {
        dut = new NpuTestT("dut");
        adapter = new NativeCoreAdapter<NpuTestT>(dut);
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
        std::cout << " PHASE 2 VALIDATION: NativeCoreAdapter reproduces Step 3f (K=1, 32x32)" << std::endl;
        std::cout << "==================================================" << std::endl;

        std::vector<int8_t> act_pattern(K), wei_pattern(K);
        for (int i = 0; i < K; i++) { act_pattern[i] = static_cast<int8_t>(100 + i); wei_pattern[i] = static_cast<int8_t>(50 + i); }

        reset_dut();

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
        int psum_events = 0;
        bool found_expected = false;
        bool prev_wren = false;

        while (timeout < TIMEOUT_CYCLES)
        {
            wait();
            timeout++;

            // Observe ONLY through the adapter's interface -- dut->psm_inst_a is not used directly.
            bool wren = adapter->psum_wren();
            if (wren && !prev_wren)
            {
                auto wdata = adapter->psum_wdata();
                psum_events++;
                std::cout << "  [ADAPTER PSUM] cycle=" << timeout << " addr=" << adapter->psum_addr()
                          << " wdata[0]=" << wdata[0] << " state=" << adapter->state_name() << std::endl;
                if (wdata[0] == EXPECTED && !found_expected) found_expected = true;
            }
            prev_wren = wren;

            if (deadlock.read()) { std::cout << "  [FAIL] deadlock o cycle " << timeout << std::endl; errors++; break; }
            if (done.read()) { saw_done = true; std::cout << "  [INFO] o_done o cycle " << timeout << std::endl; break; }
        }

        wait(10);
        int32_t c_readback[4] = {0, 0, 0, 0};
        dut->sram_inst->read_bank_data(4, 0, reinterpret_cast<uint8_t*>(c_readback), sizeof(c_readback));
        std::cout << "\n[SRAM-C READBACK] bank=4 offset=0 lanes[0..3] = "
                  << c_readback[0] << "," << c_readback[1] << "," << c_readback[2] << "," << c_readback[3]
                  << std::endl;

        check(saw_done, "completes before timeout");
        check(!deadlock.read(), "no deadlock");
        check(psum_events > 0, "the adapter observed at least one psum drain");
        check(found_expected, "adapter->psum_wdata()[0] holds the expected value " + std::to_string(EXPECTED));
        check(c_readback[0] == EXPECTED, "real SRAM-C (bank 4) still correct through the adapter, same as the lane A test");

        std::cout << "\n==================================================" << std::endl;
        if (errors == 0) std::cout << "  [PASS] PHASE 2 VALIDATION." << std::endl;
        else std::cout << "  [FAIL] PHASE 2 VALIDATION: " << errors << " errors." << std::endl;
        std::cout << "==================================================" << std::endl;

        sc_stop();
    }
};

int sc_main(int argc, char **argv)
{
    sc_clock clk("clk", 10, SC_NS);
    TbPhase2AdapterNativeCheck tb("TbPhase2AdapterNativeCheck_inst");
    tb.i_clk(clk);
    sc_start();
    return 0;
}
