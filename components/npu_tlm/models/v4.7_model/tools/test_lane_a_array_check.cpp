// Lane-A single-lane wiring, array step: checks array_inst with the act/wei input read from real SRAM.
// act[k] = 100+k, wei[k] = 50+k, k = 0..7 (K = 8). Expected (PE step(): mac += a*b, INT32 accumulation):
//   sum_{k=0}^{7} (100+k)*(50+k) = 44340
// psm_inst_a / obp_inst_a are not used.
//
// o_c_arr_a is the SCAN-CHAIN output (scan_out_a[y] = grid[y][0].mac_sc_q, systolic_array/sa_array.h), valid
// only when a scan / cswitch event occurs; it is not the live MAC accumulator. The check therefore reads
// get_pe_mac(y,x), which returns mac_q directly (live accumulator, bypassing the scan chain).
#define SC_ALLOW_DEPRECATED_IEEE_API

#include <systemc.h>
#include <iostream>
#include <vector>
#include <set>

#include "sauria_types.h"
#include "config_map.h"
#include "npu_top.h"

using namespace sauria;

typedef NpuTop<32, 32, int8_t, int8_t, int32_t, 16, 64, 1> NpuTestT;

SC_MODULE(TbLaneAArrayCheck)
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
    static const int32_t EXPECTED_SUM = 44340; // sum_{k=0}^{7} (100+k)*(50+k), tinh tay

    SC_CTOR(TbLaneAArrayCheck)
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
        std::cout << " STEP 3c: array_inst compute check (32x32, 1 lane)" << std::endl;
        std::cout << " Expected: some lane of o_c_arr_a reaches the value " << EXPECTED_SUM << std::endl;
        std::cout << "==================================================" << std::endl;

        std::vector<int8_t> act_pattern(K), wei_pattern(K);
        for (int i = 0; i < K; i++) { act_pattern[i] = static_cast<int8_t>(100 + i); wei_pattern[i] = static_cast<int8_t>(50 + i); }

        reset_dut();

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
        std::set<int32_t> seen_values; // every non-zero value seen on lane 0
        int32_t max_seen = 0;
        bool found_expected = false;
        int found_cycle = -1;

        while (timeout < TIMEOUT_CYCLES)
        {
            wait();
            timeout++;

            auto cvec = dut->array_inst->o_c_arr_a.read();
            int32_t v0 = cvec[0];
            if (v0 != 0) seen_values.insert(v0);
            if (v0 > max_seen) max_seen = v0;

            // Read the mac_q accumulator directly (bypasses the scan chain, see file header).
            int32_t pe_mac = dut->array_inst->get_pe_mac(0, 0);
            if (pe_mac == EXPECTED_SUM && !found_expected)
            {
                found_expected = true;
                found_cycle = timeout;
                std::cout << "  [FOUND] get_pe_mac(0,0) == " << EXPECTED_SUM << " o cycle " << timeout << std::endl;
            }

            if (deadlock.read()) { std::cout << "  [FAIL] deadlock o cycle " << timeout << std::endl; errors++; break; }
            if (done.read()) { saw_done = true; std::cout << "  [INFO] o_done o cycle " << timeout << std::endl; break; }
        }

        check(saw_done, "completes before timeout");
        check(!deadlock.read(), "no deadlock");
        check(found_expected, "get_pe_mac(0,0) reaches the expected value " + std::to_string(EXPECTED_SUM) + " at some point (MAC accumulator alive)");

        std::cout << "\n[DEBUG] (information only, not used for PASS/FAIL) non-zero values seen on o_c_arr_a[0] (scan chain, first 20 values): ";
        int cnt = 0;
        for (auto v : seen_values) { std::cout << v << " "; if (++cnt >= 20) break; }
        std::cout << "\n[DEBUG] max_seen=" << max_seen << " (distinct non-zero values observed=" << seen_values.size() << ")" << std::endl;

        std::cout << "\n==================================================" << std::endl;
        if (errors == 0) std::cout << "  [PASS] STEP 3c." << std::endl;
        else std::cout << "  [FAIL] STEP 3c: " << errors << " errors." << std::endl;
        std::cout << "==================================================" << std::endl;

        sc_stop();
    }
};

int sc_main(int argc, char **argv)
{
    sc_clock clk("clk", 10, SC_NS);
    TbLaneAArrayCheck tb("TbLaneAArrayCheck_inst");
    tb.i_clk(clk);
    sc_start();
    return 0;
}
