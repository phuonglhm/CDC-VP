// Testbench: Dual Instruction Queues (Queue_A / Queue_B) Independent Dispatch Verification (64x64)
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

SC_MODULE(TbDualInstructionQueues)
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

    SC_CTOR(TbDualInstructionQueues)
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
        std::cout << "   DUAL INSTRUCTION QUEUE DISPATCH VERIFICATION (64x64)   " << std::endl;
        std::cout << "==================================================" << std::endl;

        reset_dut();

        // Configure N_split = 32 for dual-lane split operation
        write_mmio(0x40000014, 32);
        wait(2);
        assert(dut->get_nsplit() == 32);

        std::cout << "\n[TEST 1] Dispatching long GEMM to Queue A and multiple instructions to Queue B..." << std::endl;

        // --- Step 1: Configure & Push Long GEMM to Queue A (0x40000310) ---
        write_mmio(0x40000400, 0x0000);   // r_in_addr
        write_mmio(0x40000404, 0x10000);  // r_w_addr
        write_mmio(0x40000408, 0x20000);  // r_out_addr
        write_mmio(0x40000410, 64);       // r_m = 64
        write_mmio(0x40000414, 128);      // r_k = 128 (large compute workload)
        write_mmio(0x40000418, 64);       // r_n = 64
        write_mmio(0x40000310, 0x12);     // Push GEMM_FUSED to Queue A

        wait(1);

        // --- Step 2: Push 3 Shorter Instructions to Queue B (0x40000314) ---
        
        // 2a. Short Layernorm to Queue B
        write_mmio(0x40000448, 0x30000);  // r_gamma_addr
        write_mmio(0x4000044C, 0x31000);  // r_beta_addr
        write_mmio(0x40000400, 0x32000);  // r_in_addr
        write_mmio(0x40000408, 0x33000);  // r_out_addr
        write_mmio(0x40000450, 16);       // r_seq_len = 16
        write_mmio(0x40000454, 64);       // r_dim = 64
        write_mmio(0x40000314, 0x14);     // Push LAYERNORM to Queue B

        // 2b. Short Element-wise Add to Queue B
        write_mmio(0x40000440, 0x40000);  // r_a_addr
        write_mmio(0x40000444, 0x41000);  // r_b_addr
        write_mmio(0x40000408, 0x42000);  // r_out_addr
        write_mmio(0x40000448, 512);      // r_len = 512
        write_mmio(0x40000454, 0);        // r_mode = 0 (ADD)
        write_mmio(0x40000314, 0x15);     // Push ELEM_WISE to Queue B

        // 2c. Small GEMM to Queue B
        write_mmio(0x40000400, 0x50000);  // r_in_addr
        write_mmio(0x40000404, 0x51000);  // r_w_addr
        write_mmio(0x40000408, 0x52000);  // r_out_addr
        write_mmio(0x40000410, 32);       // r_m = 32
        write_mmio(0x40000414, 32);       // r_k = 32
        write_mmio(0x40000418, 32);       // r_n = 32
        write_mmio(0x40000314, 0x12);     // Push GEMM_FUSED to Queue B

        // Wait for execution of all queued instructions across Queue A and Queue B
        int cycle_count = 0;
        while ((dut->decoder_inst->get_queue_a_size() > 0 ||
                dut->decoder_inst->get_queue_b_size() > 0 ||
                dut->decoder_inst->get_state_a() != 0 ||
                dut->decoder_inst->get_state_b() != 0) && cycle_count < 2000)
        {
            wait();
            cycle_count++;
        }

        std::cout << "\n[RESULT] Both queues processed all instructions in " << cycle_count << " cycles." << std::endl;
        assert(dut->decoder_inst->get_queue_a_size() == 0);
        assert(dut->decoder_inst->get_queue_b_size() == 0);
        std::cout << "  [PASS] Queue A size = 0, Queue B size = 0." << std::endl;

        std::cout << "\n==================================================" << std::endl;
        std::cout << " [PASS] DUAL INSTRUCTION QUEUE DISPATCH TEST PASSED! " << std::endl;
        std::cout << "==================================================" << std::endl;

        sc_stop();
    }
};

int sc_main(int argc, char **argv)
{
    sc_clock clk("clk", 10, SC_NS);
    TbDualInstructionQueues tb("TbDualInstructionQueues_inst");
    tb.i_clk(clk);

    sc_start();
    return 0;
}
