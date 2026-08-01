// Testbench: SET_NSPLIT Barrier Logic & Independent Execution Verification (64x64)
#define SC_ALLOW_DEPRECATED_IEEE_API

#include <systemc.h>
#include <iostream>
#include <vector>
#include <cassert>

#include "sauria_types.h"
#include "config_map.h"
#include "npu_top.h"

using namespace sauria;

typedef NpuTop<64, 64, int8_t, int8_t, int32_t, 65536, 65536, 65536, 16, 128, 1> NpuTestT;

SC_MODULE(TbNsplitBarrier)
{
    sc_in<bool> i_clk;

    sc_signal<bool> rstn, soft_reset, start, done, deadlock;
    sc_signal<uint32_t> mvm_k, host_addr, total_contexts;
    sc_signal<bool> host_wren, host_rden;
    sc_signal<host_data_t> host_wdata, host_rdata;
    sc_signal<host_mask_t> host_wmask;
    sc_signal<float> threshold;
    sc_signal<sc_bv<3>> select;

    NpuTestT *dut;
    std::vector<uint8_t> dram;

    SC_CTOR(TbNsplitBarrier)
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

    void reset_dut()
    {
        rstn.write(false);
        soft_reset.write(false);
        start.write(false);
        mvm_k.write(64);
        total_contexts.write(1);
        threshold.write(0.0f);
        select.write(0);
        host_wren.write(false);
        host_rden.write(false);
        wait(5);
        rstn.write(true);
        wait(5);
    }

    void run()
    {
        dram.resize(2 * 1024 * 1024, 0);
        dut->set_dram(&dram);

        std::cout << "==================================================" << std::endl;
        std::cout << "   SET_NSPLIT BARRIER LOGIC VERIFICATION (64x64)  " << std::endl;
        std::cout << "==================================================" << std::endl;

        reset_dut();

        // 1. Set Initial N_split to 32 via SET_NSPLIT instruction while idle
        std::cout << "\n[TEST 1] Initial N_split set to 32 when idle..." << std::endl;
        write_mmio(0x40000418, 32);   // r_n = 32
        write_mmio(0x40000310, 0x05); // Push SET_NSPLIT to Queue A
        wait(10);
        assert(dut->get_nsplit() == 32);
        std::cout << "  [PASS] N_split set to 32." << std::endl;

        // 2. Normal Independent Execution Test (No Barrier Stalls)
        std::cout << "\n[TEST 2] Verifying normal independent execution (no barrier stalls)..." << std::endl;
        
        // Setup MMIO Registers for GEMM_FUSED on Lane A & Lane B
        write_mmio(0x40000400, 0x0000);   // r_in_addr
        write_mmio(0x40000404, 0x4000);   // r_w_addr
        write_mmio(0x40000408, 0x8000);   // r_out_addr
        write_mmio(0x40000410, 64);       // r_m = 64
        write_mmio(0x40000414, 64);       // r_k = 64
        write_mmio(0x40000418, 64);       // r_n = 64

        // Push GEMM_FUSED to Queue A and Queue B back-to-back
        write_mmio(0x40000310, 0x12);     // Push GEMM_FUSED to Queue A
        write_mmio(0x40000314, 0x12);     // Push GEMM_FUSED to Queue B

        wait(2);
        // Verify both queues are executing (state != 4, i.e., not stalled in WAIT_BARRIER)
        assert(dut->decoder_inst->get_state_a() != 4);
        assert(dut->decoder_inst->get_state_b() != 4);
        std::cout << "  [PASS] Both queues began execution immediately with zero barrier stalls." << std::endl;

        // Wait for both execution units to complete
        int timeout = 0;
        while ((dut->decoder_inst->get_state_a() != 0 || dut->decoder_inst->get_state_b() != 0) && timeout < 1000)
        {
            wait();
            timeout++;
        }
        assert(dut->decoder_inst->get_state_a() == 0);
        assert(dut->decoder_inst->get_state_b() == 0);
        std::cout << "  [PASS] Independent executions completed cleanly." << std::endl;

        // 3. SET_NSPLIT Partition-Change Point Barrier Logic
        std::cout << "\n[TEST 3] Verifying SET_NSPLIT barrier logic during active execution..." << std::endl;

        // Dispatch long GEMM (m=64, k=256, n=64) to Queue A
        write_mmio(0x40000400, 0x0000);   // r_in_addr
        write_mmio(0x40000404, 0x10000);  // r_w_addr
        write_mmio(0x40000408, 0x20000);  // r_out_addr
        write_mmio(0x40000410, 64);       // r_m = 64
        write_mmio(0x40000414, 256);      // r_k = 256
        write_mmio(0x40000418, 64);       // r_n = 64
        write_mmio(0x40000310, 0x12);     // Push GEMM_FUSED to Queue A

        std::cout << "  [DEBUG] state_a after dispatch = " << dut->decoder_inst->get_state_a() << std::endl;
        assert(dut->decoder_inst->get_state_a() != 0);

        // Issue SET_NSPLIT (opcode 0x05, N_split=48) instruction to Queue B while Queue A is running
        write_mmio(0x40000418, 48);       // r_n = 48
        write_mmio(0x40000314, 0x05);     // Push SET_NSPLIT to Queue B

        wait(2);

        // Queue B MUST enter WAIT_BARRIER state (state_b == 4) because Queue A is actively running!
        int state_b_curr = dut->decoder_inst->get_state_b();
        std::cout << "  [CHECK] Queue B state while Queue A is running = " << state_b_curr << std::endl;
        assert(state_b_curr == 4); // 4 == WAIT_BARRIER
        assert(dut->get_nsplit() == 32); // N_split MUST NOT change yet!
        std::cout << "  [PASS] Queue B correctly held in WAIT_BARRIER state; N_split remained 32." << std::endl;

        // Wait until Queue A completes tile execution & pipeline drain
        timeout = 0;
        while (dut->decoder_inst->get_state_a() != 0 && timeout < 2000)
        {
            wait();
            timeout++;
        }
        std::cout << "  [DEBUG] state_a after wait loop = " << dut->decoder_inst->get_state_a() << " (timeout=" << timeout << ")" << std::endl;
        assert(dut->decoder_inst->get_state_a() == 0);

        // Give 5 cycles for barrier condition check in Queue B
        wait(5);

        // After Queue A drains completely, barrier releases and N_split updates to 48
        assert(dut->get_nsplit() == 48);
        assert(dut->decoder_inst->get_state_b() == 0); // Queue B cleared barrier
        std::cout << "  [PASS] Barrier released after SA drain; N_split successfully updated to 48." << std::endl;

        std::cout << "\n==================================================" << std::endl;
        std::cout << "  [PASS] ALL SET_NSPLIT BARRIER TESTS PASSED!     " << std::endl;
        std::cout << "==================================================" << std::endl;

        sc_stop();
    }
};

int sc_main(int argc, char **argv)
{
    sc_clock clk("clk", 10, SC_NS);
    TbNsplitBarrier tb("TbNsplitBarrier_inst");
    tb.i_clk(clk);

    sc_start();
    return 0;
}
