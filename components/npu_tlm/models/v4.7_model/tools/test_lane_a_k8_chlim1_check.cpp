// Lane-A pipeline with K = 8, read back from SRAM-C after psm_inst_a has finished shift/drain (instead of
// observing the intermediate o_c_arr_a signal, which shows the individual products 100*(50+k)).
//
// Expected (PE array accumulating K = 8 with a simple MAC model):
//   sum_{k=0}^{7} (100+k)*(50+k) = 44340
#define SC_ALLOW_DEPRECATED_IEEE_API

#include <systemc.h>
#include <iostream>
#include <vector>

#include "sauria_types.h"
#include "config_map.h"
#include "npu_top.h"

using namespace sauria;

typedef NpuTop<32, 32, int8_t, int8_t, int32_t, 16, 64, 1> NpuTestT;

SC_MODULE(TbLaneAK8E2eCheck)
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
    static const int K = 8;
    static const int32_t EXPECTED_SUM = 44340; // sum_{k=0}^{7} (100+k)*(50+k)

    SC_CTOR(TbLaneAK8E2eCheck)
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
        // ACT_CHLIM / WEI_KLIM may be address ranges (like ACT_XLIM/YLIM = 1) rather than the accumulation depth:
        // i_mvm_k (= K, set in reset_dut()) controls the accumulation depth (main_controller.h::ctrl_process() and
        // ifmap_feeder.h::get_effective_k() read i_mvm_k first). This variant uses CHLIM/KLIM = 1 instead of K.
        write_mmio(ACT_CHLIM, 1);
        write_mmio(ACT_CHSTEP, 1);
        write_mmio(WEI_WLIM, 1);
        write_mmio(WEI_WSTEP, 1);
        write_mmio(WEI_KLIM, 1);
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
        std::cout << " STEP K8-E2E: array_inst K=8, read back from the REAL SRAM-C (not inferred from intermediate signals)" << std::endl;
        std::cout << " Expected: SRAM-C bank 4 offset 0 holds " << EXPECTED_SUM << " (int32) after o_done" << std::endl;
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

        while (timeout < TIMEOUT_CYCLES)
        {
            wait();
            timeout++;
            if (deadlock.read()) { std::cout << "  [FAIL] deadlock o cycle " << timeout << std::endl; errors++; break; }
            if (done.read()) { saw_done = true; std::cout << "  [INFO] o_done o cycle " << timeout << std::endl; break; }
        }

        wait(10); // let the OBP drain its 4-stage pipeline

        int32_t c_readback[8] = {0,0,0,0,0,0,0,0};
        dut->sram_inst->read_bank_data(4, 0, reinterpret_cast<uint8_t*>(c_readback), sizeof(c_readback));
        std::cout << "\n[SRAM-C READBACK] bank=4 offset=0 lanes[0..7] = ";
        for (int i = 0; i < 8; i++) std::cout << c_readback[i] << " ";
        std::cout << std::endl;

        check(saw_done, "completes before timeout");
        check(!deadlock.read(), "no deadlock");
        bool found_expected = false;
        for (int i = 0; i < 8; i++) if (c_readback[i] == EXPECTED_SUM) found_expected = true;
        check(found_expected, "at least 1 SRAM-C lane reaches the full accumulated value " + std::to_string(EXPECTED_SUM));

        std::cout << "\n==================================================" << std::endl;
        if (errors == 0) std::cout << "  [PASS] STEP K8-E2E." << std::endl;
        else std::cout << "  [FAIL] STEP K8-E2E: " << errors << " error(s) (may reflect a semantics finding rather than a model bug)." << std::endl;
        std::cout << "==================================================" << std::endl;

        sc_stop();
    }
};

int sc_main(int argc, char **argv)
{
    sc_clock clk("clk", 10, SC_NS);
    TbLaneAK8E2eCheck tb("TbLaneAK8E2eCheck_inst");
    tb.i_clk(clk);
    sc_start();
    return 0;
}
