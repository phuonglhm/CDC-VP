// Lane-A single-lane wiring, control-glue step: ctrl_inst_a runs one clean IDLE->...->DONE cycle when i_start
// is pulsed directly (instruction_decoder bypassed); computed data is not checked. nsplit = Y_DIM (all rows
// to lane A, lane B empty).
#define SC_ALLOW_DEPRECATED_IEEE_API

#include <systemc.h>
#include <iostream>
#include <vector>

#include "sauria_types.h"
#include "config_map.h"
#include "npu_top.h"

using namespace sauria;

// Target geometry (HAS): 1 lane, 32x32, INT8 in / INT32 accumulate.
typedef NpuTop<32, 32, int8_t, int8_t, int32_t, 16, 64, 1> NpuTestT;

SC_MODULE(TbLaneACtrlGlue)
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

    SC_CTOR(TbLaneACtrlGlue)
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

    // Set feeder address-generation limits via config_regs (local_addr = i_host_addr &
    // ~SAURIA_MEM_ADDR_MASK; no 0x40000000 prefix because this is the CFG region, not the rich-instruction region of
    // instruction_decoder). Minimal values: they only need to be non-zero and self-consistent so the feeders leave
    // the stall state.
    void configure_minimal_feeder_limits()
    {
        write_mmio(ACT_XLIM, 1);
        write_mmio(ACT_XSTEP, 1);
        write_mmio(ACT_YLIM, 1);
        write_mmio(ACT_YSTEP, 1);
        write_mmio(ACT_CHLIM, 8);
        write_mmio(ACT_CHSTEP, 1);
        write_mmio(WEI_WLIM, 1);
        write_mmio(WEI_WSTEP, 1);
        write_mmio(WEI_KLIM, 8);
        write_mmio(WEI_KSTEP, 1);
    }

    void reset_dut()
    {
        rstn.write(false);
        soft_reset.write(false);
        start.write(false);
        mvm_k.write(8);          // smallest K; data not checked here
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
        std::cout << " STEP 3a: ctrl_inst_a control-glue-only check (32x32, 1 lane)" << std::endl;
        std::cout << "==================================================" << std::endl;

        reset_dut();

        std::cout << "\n[CONFIG] set minimal feeder address-gen limits through MMIO..." << std::endl;
        configure_minimal_feeder_limits();
        wait(5);

        check(dut->ctrl_inst_a->state_name(dut->ctrl_inst_a->get_state()) == "IDLE",
              "ctrl_inst_a starts in IDLE after reset");
        check(done.read() == false, "o_done = false before start");

        std::cout << "\n[PULSE] i_start = true (top level, bypassing instruction_decoder)" << std::endl;
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
            if (timeout % 200 == 0)
            {
                std::cout << "  [WATCH] cycle=" << timeout
                          << " state=" << dut->ctrl_inst_a->state_name(dut->ctrl_inst_a->get_state())
                          << " o_done=" << done.read()
                          << " o_deadlock=" << deadlock.read()
                          << " o_active=" << dut->ctrl_inst_a->o_active.read()
                          << std::endl;
            }
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

        check(saw_done, "ctrl_inst_a completes (o_done=true) before timeout " + std::to_string(TIMEOUT_CYCLES) + " cycles");
        check(!deadlock.read(), "no deadlock");

        std::cout << "\n==================================================" << std::endl;
        if (errors == 0)
            std::cout << "  [PASS] control glue runs one full round cleanly." << std::endl;
        else
            std::cout << "  [FAIL] STEP 3a: " << errors << " errors." << std::endl;
        std::cout << "==================================================" << std::endl;

        sc_stop();
    }
};

int sc_main(int argc, char **argv)
{
    sc_clock clk("clk", 10, SC_NS);
    TbLaneACtrlGlue tb("TbLaneACtrlGlue_inst");
    tb.i_clk(clk);

    sc_start();
    return 0;
}
