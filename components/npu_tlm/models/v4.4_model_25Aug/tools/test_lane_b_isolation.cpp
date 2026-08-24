// Verification test: Lane B in isolation (N_split=0, all 64 rows to B)
// verifies that Lane B reproduces identical golden results as Lane A alone on a 64x64 array.

#include <systemc.h>
#include <cstdint>
#include <iostream>
#include <vector>
#include <iomanip>
#include <cmath>
#include "sauria_types.h"
#include "npu_top.h"

using namespace sauria;

typedef NpuTop<64, 64, int8_t, int8_t, int32_t, 16, 128, 1> NpuTestT;

SC_MODULE(TbLaneBIsolation)
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
    int errors = 0;

    SC_CTOR(TbLaneBIsolation)
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

        std::cout << "\n==================================================" << std::endl;
        std::cout << "   LANE B ISOLATION VERIFICATION TEST (64x64, N_split=0)  " << std::endl;
        std::cout << "==================================================" << std::endl;

        // Initialize test data in DRAM
        // Matrix A: 64x64 at 0x0000
        int8_t *A_ptr = reinterpret_cast<int8_t*>(&dram[0]);
        for (int i = 0; i < 64 * 64; i++) A_ptr[i] = static_cast<int8_t>((i % 7) + 1);

        // Matrix B: 64x64 at 0x4000
        int8_t *B_ptr = reinterpret_cast<int8_t*>(&dram[0x4000]);
        for (int i = 0; i < 64 * 64; i++) B_ptr[i] = static_cast<int8_t>((i % 5) + 1);

        // --------------------------------------------------------
        // PHASE 1: Run Lane A in Isolation (N_split = 64, all 64 rows to A)
        // --------------------------------------------------------
        std::cout << "\n[PHASE 1] Executing Lane A in Isolation (N_split = 64)..." << std::endl;
        reset_dut();

        // Configure N_split = 64 via config register
        write_mmio(0x40000014, 64);

        // Setup MMIO Registers for GEMM_FUSED on Lane A
        write_mmio(0x40000400, 0);        // r_in_addr
        write_mmio(0x40000404, 0x4000);   // r_w_addr
        write_mmio(0x40000408, 0x8000);   // r_out_addr
        write_mmio(0x4000040C, 0);        // r_bias_addr
        write_mmio(0x40000410, 64);       // r_m = 64
        write_mmio(0x40000414, 64);       // r_k = 64
        write_mmio(0x40000418, 64);       // r_n = 64
        write_mmio(0x4000042C, 0);        // r_act_type
        write_mmio(0x40000430, 0);        // r_has_skip

        // Dispatch to Lane A (opcode = 0x12)
        write_mmio(0x40000310, 0x12);

        int timeout = 0;
        while ((dut->decoder_inst->get_state_a() != 0 || dut->decoder_inst->get_queue_a_size() > 0) && timeout < 1000)
        {
            wait();
            timeout++;
        }
        assert(dut->decoder_inst->get_state_a() == 0);

        std::vector<int8_t> gold_lane_a(64 * 64);
        int8_t *C_a = reinterpret_cast<int8_t*>(&dram[0x8000]);
        for (int i = 0; i < 64 * 64; i++) gold_lane_a[i] = C_a[i];

        // --------------------------------------------------------
        // PHASE 2: Run Lane B in Isolation (N_split = 0, all 64 rows to B)
        // --------------------------------------------------------
        std::cout << "\n[PHASE 2] Executing Lane B in Isolation (N_split = 0)..." << std::endl;
        reset_dut();

        // Clear output buffer in DRAM
        for (int i = 0; i < 64 * 64; i++) dram[0x8000 + i] = 0;

        // Configure N_split = 0 via config register
        write_mmio(0x40000014, 0);

        // Setup MMIO Registers for GEMM_FUSED on Lane B
        write_mmio(0x40000400, 0);        // r_in_addr
        write_mmio(0x40000404, 0x4000);   // r_w_addr
        write_mmio(0x40000408, 0x8000);   // r_out_addr
        write_mmio(0x4000040C, 0);        // r_bias_addr
        write_mmio(0x40000410, 64);       // r_m = 64
        write_mmio(0x40000414, 64);       // r_k = 64
        write_mmio(0x40000418, 64);       // r_n = 64
        write_mmio(0x4000042C, 0);        // r_act_type
        write_mmio(0x40000430, 0);        // r_has_skip

        // Dispatch to Lane B (opcode = 0x12)
        write_mmio(0x40000314, 0x12);

        timeout = 0;
        while ((dut->decoder_inst->get_state_b() != 0 || dut->decoder_inst->get_queue_b_size() > 0) && timeout < 1000)
        {
            wait();
            timeout++;
        }
        assert(dut->decoder_inst->get_state_b() == 0);

        int8_t *C_b = reinterpret_cast<int8_t*>(&dram[0x8000]);

        // --------------------------------------------------------
        // PHASE 3: Compare Lane A vs Lane B Golden Results
        // --------------------------------------------------------
        std::cout << "\n[PHASE 3] Comparing Lane A vs Lane B Golden Output..." << std::endl;
        int mismatches = 0;
        for (int i = 0; i < 64 * 64; i++)
        {
            if (gold_lane_a[i] != C_b[i])
            {
                mismatches++;
                if (mismatches <= 5)
                {
                    std::cout << "  Mismatch at [" << (i / 64) << "][" << (i % 64) 
                              << "]: Lane A = " << static_cast<int>(gold_lane_a[i])
                              << ", Lane B = " << static_cast<int>(C_b[i]) << std::endl;
                }
            }
        }

        std::cout << "\n==================================================" << std::endl;
        if (mismatches == 0)
        {
            std::cout << "  [PASS] Lane B in isolation (64x64, N_split=0) reproduces exact golden results as Lane A!" << std::endl;
        }
        else
        {
            std::cout << "  [FAIL] Lane B output mismatched Lane A at " << mismatches << " positions." << std::endl;
            errors++;
        }
        std::cout << "==================================================\n" << std::endl;

        sc_stop();
    }
};

int sc_main(int argc, char **argv)
{
    sc_clock clk("clk", 10, SC_NS);
    TbLaneBIsolation tb("TbLaneBIsolation_inst");
    tb.i_clk(clk);

    sc_start();
    return tb.errors;
}
