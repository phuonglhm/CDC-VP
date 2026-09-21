#include <systemc.h>
#include <cstdint>
#include <iostream>
#include <vector>
#include <iomanip>
#include <cmath>
#include "sauria_types.h"
#include "npu_top.h"

using namespace sauria;

// Float type configuration for simple arithmetic verification
typedef NpuTop<32, 32, float, float, float, 16, 64, 1> NpuFloatT;

SC_MODULE(TbRichIsa)
{
    sc_in<bool> i_clk;

    sc_signal<bool> rstn, soft_reset, start, done, deadlock;
    sc_signal<uint32_t> mvm_k, host_addr, total_contexts;
    sc_signal<bool> host_wren, host_rden;
    sc_signal<host_data_t> host_wdata, host_rdata;
    sc_signal<host_mask_t> host_wmask;
    sc_signal<float> threshold;
    sc_signal<sc_bv<3>> select;

    NpuFloatT *dut;
    std::vector<uint8_t> dram;
    int errors = 0;

    SC_CTOR(TbRichIsa)
    {
        dut = new NpuFloatT("dut");
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

    void wr(uint32_t addr, uint32_t val)
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

    void wr_f(uint32_t addr, float val)
    {
        host_data_t d;
        d.data.fill(0.0f);
        d[0] = val;
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

    void run()
    {
        // Allocate 1MB of virtual DRAM
        dram.resize(1024 * 1024, 0);
        dut->set_dram(&dram);

        // Reset system
        rstn.write(false);
        soft_reset.write(false);
        start.write(false);
        mvm_k.write(32);
        total_contexts.write(1);
        threshold.write(0.0f);
        select.write(0);
        host_wren.write(false);
        host_rden.write(false);
        wait(5);
        rstn.write(true);
        wait(5);

        std::cout << "\n==================================================" << std::endl;
        std::cout << "   STARTING RICH ISA & DMA SUBSYSTEM VERIFICATION" << std::endl;
        std::cout << "==================================================" << std::endl;

        // --------------------------------------------------------
        // TEST 1: GEMM_FUSED Emulation & DMA Prefetch/Writeback
        // --------------------------------------------------------
        std::cout << "\n[TEST 1] GEMM_FUSED" << std::endl;
        
        // Populate Input matrix A (at address 0) with 1.5f
        float *A_ptr = reinterpret_cast<float*>(&dram[0]);
        for (int i = 0; i < 32 * 32; i++) A_ptr[i] = 1.5f;

        // Populate Weight matrix B (at address 0x2000) with 2.0f
        float *B_ptr = reinterpret_cast<float*>(&dram[0x2000]);
        for (int i = 0; i < 32 * 32; i++) B_ptr[i] = 2.0f;

        // Setup MMIO Registers for GEMM_FUSED
        wr(0x40000400, 0);        // r_in_addr
        wr(0x40000404, 0x2000);   // r_w_addr
        wr(0x40000408, 0x4000);   // r_out_addr (C placed here)
        wr(0x4000040C, 0);        // r_bias_addr
        wr(0x40000410, 32);       // r_m
        wr(0x40000414, 32);       // r_k
        wr(0x40000418, 32);       // r_n
        wr(0x4000042C, 0);        // r_act_type (None)
        wr_f(0x40000438, 1.0f);   // r_in_scale
        wr_f(0x4000043C, 1.0f);   // r_w_scale
        wr_f(0x40000440, 1.0f);   // r_out_scale
        wr(0x40000430, 0);        // r_has_skip = 0

        // Trigger dispatch to Lane A (opcode = 0x12)
        wr(0x40000310, 0x12);

        // Wait for execution (takes some cycles for DMA and compute)
        wait(1000);

        // Verify Output matrix C
        float *C_ptr = reinterpret_cast<float*>(&dram[0x4000]);
        int gemm_mismatches = 0;
        // Expected value: 32 * 1.5f * 2.0f = 96.0f
        for (int i = 0; i < 32 * 32; i++)
        {
            if (std::abs(C_ptr[i] - 96.0f) > 1e-4)
            {
                gemm_mismatches++;
                if (gemm_mismatches < 5)
                {
                    std::cout << "  Mismatch at index " << i << ": got " << C_ptr[i] << ", expected 96.0" << std::endl;
                }
            }
        }
        if (gemm_mismatches == 0)
        {
            std::cout << "  [PASS] GEMM_FUSED executed correctly. Output matches 96.0f." << std::endl;
        }
        else
        {
            std::cout << "  [FAIL] GEMM_FUSED mismatch count: " << gemm_mismatches << std::endl;
            errors++;
        }

        // --------------------------------------------------------
        // TEST 2: LAYERNORM Emulation & DMA
        // --------------------------------------------------------
        std::cout << "\n[TEST 2] LAYERNORM" << std::endl;

        // Gamma (scale) at 0x6000 -> 1.5f
        float *gamma_ptr = reinterpret_cast<float*>(&dram[0x6000]);
        for (int i = 0; i < 32; i++) gamma_ptr[i] = 1.5f;

        // Beta (shift) at 0x7000 -> 0.5f
        float *beta_ptr = reinterpret_cast<float*>(&dram[0x7000]);
        for (int i = 0; i < 32; i++) beta_ptr[i] = 0.5f;

        // Input at 0 (reuse A space) -> half 9.0f, half 11.0f (mean=10.0f, var=1.0f)
        for (int i = 0; i < 32; i++)
        {
            A_ptr[i] = (i < 16) ? 9.0f : 11.0f;
        }

        // Setup MMIO Registers for LAYERNORM
        wr(0x40000400, 0);        // r_in_addr
        wr(0x40000408, 0x8000);   // r_out_addr
        wr(0x40000444, 0x6000);   // r_gamma_addr
        wr(0x4000044C, 0x7000);   // r_beta_addr
        wr(0x40000450, 1);        // r_seq_len (1 row)
        wr(0x40000454, 32);       // r_dim (32 elements)

        // Trigger dispatch to Lane A (opcode = 0x14)
        wr(0x40000310, 0x14);

        wait(1000);

        // Verify Output
        float *LN_out_ptr = reinterpret_cast<float*>(&dram[0x8000]);
        int ln_mismatches = 0;
        // Expected value:
        // for i < 16 (input = 9.0): (9-10)/1.0 * 1.5 + 0.5 = -1.0
        // for i >= 16 (input = 11.0): (11-10)/1.0 * 1.5 + 0.5 = 2.0
        for (int i = 0; i < 32; i++)
        {
            float exp_val = (i < 16) ? -1.0f : 2.0f;
            if (std::abs(LN_out_ptr[i] - exp_val) > 1e-3)
            {
                ln_mismatches++;
                if (ln_mismatches < 5)
                {
                    std::cout << "  Mismatch at LN index " << i << ": got " << LN_out_ptr[i] << ", expected " << exp_val << std::endl;
                }
            }
        }
        if (ln_mismatches == 0)
        {
            std::cout << "  [PASS] LAYERNORM executed correctly. Outputs match expected normalized values (-1.0f and 2.0f)." << std::endl;
        }
        else
        {
            std::cout << "  [FAIL] LAYERNORM mismatch count: " << ln_mismatches << std::endl;
            errors++;
        }

        // --------------------------------------------------------
        // TEST 3: ELEM_WISE (ADD mode) Emulation & DMA
        // --------------------------------------------------------
        std::cout << "\n[TEST 3] ELEM_WISE ADD" << std::endl;

        // Input A at 0 -> 3.0f
        for (int i = 0; i < 32; i++) A_ptr[i] = 3.0f;

        // Input B at 0x2000 -> 4.0f
        for (int i = 0; i < 32; i++) B_ptr[i] = 4.0f;

        // Setup MMIO Registers for ELEM_WISE ADD
        wr(0x40000408, 0x9000);   // r_out_addr
        wr(0x40000444, 0);        // r_a_addr
        wr(0x40000448, 0x2000);   // r_b_addr
        wr(0x40000450, 32);       // r_len
        wr(0x40000454, 0);        // r_mode = 0 (ADD)
        wr_f(0x40000458, 2.0f);   // r_scale_a
        wr_f(0x4000045C, 0.5f);   // r_scale_b
        wr_f(0x40000460, 10.0f);  // r_scale_out

        // Trigger dispatch to Lane A (opcode = 0x15)
        wr(0x40000310, 0x15);

        wait(1000);

        // Verify Output
        float *add_out_ptr = reinterpret_cast<float*>(&dram[0x9000]);
        int add_mismatches = 0;
        // Expected value: 10.0f * (2.0f * 3.0f + 0.5f * 4.0f) = 80.0f
        for (int i = 0; i < 32; i++)
        {
            if (std::abs(add_out_ptr[i] - 80.0f) > 1e-4)
            {
                add_mismatches++;
                if (add_mismatches < 5)
                {
                    std::cout << "  Mismatch at ELEM ADD index " << i << ": got " << add_out_ptr[i] << ", expected 80.0" << std::endl;
                }
            }
        }
        if (add_mismatches == 0)
        {
            std::cout << "  [PASS] ELEM_WISE ADD executed correctly. Outputs match 80.0f." << std::endl;
        }
        else
        {
            std::cout << "  [FAIL] ELEM_WISE ADD mismatch count: " << add_mismatches << std::endl;
            errors++;
        }

        std::cout << "\n==================================================" << std::endl;
        if (errors == 0)
        {
            std::cout << "   ALL RICH ISA VERIFICATION TESTS PASSED!" << std::endl;
        }
        else
        {
            std::cout << "   VERIFICATION FAILED WITH " << errors << " ERRORS!" << std::endl;
        }
        std::cout << "==================================================\n" << std::endl;

        sc_stop();
    }
};

int sc_main(int argc, char **argv)
{
    sc_clock clk("clk", 10, SC_NS);
    TbRichIsa tb("TbRichIsa_inst");
    tb.i_clk(clk);

    sc_start();
    return tb.errors;
}
