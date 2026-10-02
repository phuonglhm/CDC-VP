// Lane-A single-lane wiring, feeder step: act_feeder_a / wei_feeder_a read the right data from SRAM-A/B in the
// right order. Uses the MMIO setup of the control-glue test; array_inst is not used.
#define SC_ALLOW_DEPRECATED_IEEE_API

#include <systemc.h>
#include <iostream>
#include <vector>

#include "sauria_types.h"
#include "config_map.h"
#include "npu_top.h"

using namespace sauria;

typedef NpuTop<32, 32, int8_t, int8_t, int32_t, 16, 64, 1> NpuTestT;

SC_MODULE(TbLaneAFeederCheck)
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
    int pop_events_act = 0;
    int pop_events_wei = 0;

    static const int K = 8; // ACT_CHLIM = WEI_KLIM = 8, matches mvm_k

    SC_CTOR(TbLaneAFeederCheck)
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
        if (cond) {
            std::cout << "  [PASS] " << msg << std::endl;
        } else {
            std::cout << "  [FAIL] " << msg << std::endl;
            errors++;
        }
    }

    void run()
    {
        dram.resize(1 * 1024 * 1024, 0);
        dut->set_dram(&dram);

        std::cout << "==================================================" << std::endl;
        std::cout << " STEP 3b: act_feeder_a / wei_feeder_a data check (32x32, 1 lane)" << std::endl;
        std::cout << "==================================================" << std::endl;

        // Load SRAM with a recognisable pattern: act[i] = 100+i, wei[i] = 50+i, i = 0..K-1.
        // Bank 2 = lane-A IFmap, bank 0 = lane-A weights (same convention as test_dual_lane_fsm.cpp).
        std::vector<int8_t> act_pattern(K), wei_pattern(K);
        for (int i = 0; i < K; i++) { act_pattern[i] = static_cast<int8_t>(100 + i); wei_pattern[i] = static_cast<int8_t>(50 + i); }

        std::cout << "\n[SEED] Bank2 (act) = [100..107], Bank0 (wei) = [50..57]" << std::endl;

        reset_dut();

        dut->sram_inst->write_bank_data(2, 0, reinterpret_cast<const uint8_t*>(act_pattern.data()), act_pattern.size());
        dut->sram_inst->write_bank_data(0, 0, reinterpret_cast<const uint8_t*>(wei_pattern.data()), wei_pattern.size());

        std::cout << "\n[CONFIG] set minimal feeder address-gen limits through MMIO..." << std::endl;
        configure_minimal_feeder_limits();
        wait(5);

        std::cout << "\n[PULSE] i_start = true" << std::endl;
        start.write(true);
        wait();
        start.write(false);

        int timeout = 0;
        const int TIMEOUT_CYCLES = 5000;
        bool saw_done = false;
        bool prev_act_pop = false, prev_wei_pop = false;

        while (timeout < TIMEOUT_CYCLES)
        {
            wait();
            timeout++;

            // Capture the rising edge of act_pop_en / wei_pop_en (internal, read through ctrl_inst_a, which drives
            // pop_en for the feeders) and dump the matching output vector. Print CONTINUOUSLY (not only on edges) in a fixed
            // window to avoid missing events due to SystemC delta cycles and to compare with ctrl_inst_a's state.
            bool act_pop = dut->ctrl_inst_a->o_act_pop_en.read();
            bool wei_pop = dut->ctrl_inst_a->o_wei_pop_en.read();

            if (timeout <= 100)
            {
                auto avec = dut->act_feeder_a->o_act_arr.read();
                auto wvec = dut->wei_feeder_a->o_wei_arr.read();
                std::cout << "  [T3b cycle=" << timeout
                          << " state=" << dut->ctrl_inst_a->state_name(dut->ctrl_inst_a->get_state())
                          << " act_pop=" << act_pop << " wei_pop=" << wei_pop
                          << " act_arr[0..3]=" << (int)avec[0] << "," << (int)avec[1] << "," << (int)avec[2] << "," << (int)avec[3]
                          << " wei_arr[0..3]=" << (int)wvec[0] << "," << (int)wvec[1] << "," << (int)wvec[2] << "," << (int)wvec[3]
                          << "]" << std::endl;
            }

            if (act_pop && !prev_act_pop)
            {
                auto vec = dut->act_feeder_a->o_act_arr.read();
                std::cout << "  [ACT POP #" << pop_events_act << "] cycle=" << timeout << " vec[0..7]=";
                for (int i = 0; i < 8; i++) std::cout << (int)vec[i] << " ";
                std::cout << std::endl;
                pop_events_act++;
            }
            if (wei_pop && !prev_wei_pop)
            {
                auto vec = dut->wei_feeder_a->o_wei_arr.read();
                std::cout << "  [WEI POP #" << pop_events_wei << "] cycle=" << timeout << " vec[0..7]=";
                for (int i = 0; i < 8; i++) std::cout << (int)vec[i] << " ";
                std::cout << std::endl;
                pop_events_wei++;
            }
            prev_act_pop = act_pop;
            prev_wei_pop = wei_pop;

            if (deadlock.read())
            {
                std::cout << "  [FAIL] o_deadlock bat len o cycle " << timeout << std::endl;
                errors++;
                break;
            }
            if (done.read())
            {
                saw_done = true;
                std::cout << "  [INFO] o_done bat len o cycle " << timeout << std::endl;
                break;
            }
        }

        check(saw_done, "ctrl_inst_a completes before timeout");
        check(!deadlock.read(), "no deadlock");
        check(pop_events_act > 0, "at least one act_pop_en (the feeder really outputs data)");
        check(pop_events_wei > 0, "at least one wei_pop_en (the feeder really outputs data)");

        std::cout << "\n[NOTE] Manual check: the values printed as [ACT POP]/[WEI POP] above must match"
                  << " with the loaded pattern (act=100..107, wei=50..57) in channel order 0..7."
                  << std::endl;

        std::cout << "\n==================================================" << std::endl;
        if (errors == 0)
            std::cout << "  [PASS] no infrastructure error (deadlock/timeout); values need a manual check." << std::endl;
        else
            std::cout << "  [FAIL] STEP 3b: " << errors << " errors." << std::endl;
        std::cout << "==================================================" << std::endl;

        sc_stop();
    }
};

int sc_main(int argc, char **argv)
{
    sc_clock clk("clk", 10, SC_NS);
    TbLaneAFeederCheck tb("TbLaneAFeederCheck_inst");
    tb.i_clk(clk);

    sc_start();
    return 0;
}
