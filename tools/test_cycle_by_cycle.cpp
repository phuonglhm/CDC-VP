#include <systemc.h>
#include <cstdint>
#include <iostream>
#include <vector>
#include <iomanip>
#include <cmath>
#include "sauria_types.h"
#include "npu_top.h"

using namespace sauria;

typedef NpuTop<32, 32, float, float, float, 1024, 1024, 2048, 16, 64, 1> NpuFloatT;

SC_MODULE(TbCycleByCycle)
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

    SC_CTOR(TbCycleByCycle)
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

    std::string state_name(int state)
    {
        switch (state)
        {
            case 0: return "IDLE";
            case 1: return "DMA_READ_WAIT";
            case 2: return "COMPUTE_WAIT";
            case 3: return "DMA_WRITE_WAIT";
            case 4: return "WAIT_BARRIER";
            default: return "UNKNOWN";
        }
    }

    void wait_cycles(int num_cycles)
    {
        for (int i = 0; i < num_cycles; i++)
        {
            double c = sc_time_stamp().to_double() / 10.0;
            bool is_active = (dut->decoder_inst->state_a != 0) || 
                             dut->dma_inst->is_any_read_active() || 
                             dut->dma_inst->is_write_active();
            
            if (is_active)
            {
                std::cout << "[CYCLE " << std::setw(4) << c << "] "
                          << "State: " << std::setw(14) << state_name(dut->decoder_inst->state_a) << " | "
                          << "DMA Active: CH0=" << (dut->dma_inst->is_read_active(0) ? "Y" : "N") << " "
                          << "CH1=" << (dut->dma_inst->is_read_active(1) ? "Y" : "N") << " "
                          << "CH2=" << (dut->dma_inst->is_read_active(2) ? "Y" : "N") << " "
                          << "CH3=" << (dut->dma_inst->is_read_active(3) ? "Y" : "N") << " | "
                          << "Write=" << (dut->dma_inst->is_write_active() ? "Y" : "N")
                          << std::endl;
            }
            wait();
        }
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

    void check_close(const std::string& name, float got, float exp, float tol = 1e-3)
    {
        if (std::abs(got - exp) > tol)
        {
            std::cout << "  [FAIL] " << name << ": got " << got << ", expected " << exp << " (diff=" << std::abs(got - exp) << ")" << std::endl;
            errors++;
        }
        else
        {
            std::cout << "  [PASS] " << name << ": got " << got << " (matches expected " << exp << ")" << std::endl;
        }
    }

    void run()
    {
        dram.resize(2 * 1024 * 1024, 0); // 2MB
        dut->set_dram(&dram);

        // Reset
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

        std::cout << "\n======================================================================" << std::endl;
        std::cout << "   RUNNING CYCLE-BY-CYCLE VERIFICATION ON SAURIA NPU" << std::endl;
        std::cout << "======================================================================" << std::endl;

        // --------------------------------------------------------------------
        // TEST 1: GEMM_FUSED VARIANTS (CBS, NO ACT, GELU, SKIP)
        // --------------------------------------------------------------------
        std::cout << "\n--- TEST 1: GEMM_FUSED VARIANTS ---" << std::endl;

        // Input A (DRAM 0) = 0.5f
        float *A_ptr = reinterpret_cast<float*>(&dram[0]);
        for (int i = 0; i < 32 * 32; i++) A_ptr[i] = 0.5f;

        // Weight B (DRAM 0x2000) = 1.0f
        float *B_ptr = reinterpret_cast<float*>(&dram[0x2000]);
        for (int i = 0; i < 32 * 32; i++) B_ptr[i] = 1.0f;

        // Bias (DRAM 0x4000)
        float *bias_ptr = reinterpret_cast<float*>(&dram[0x4000]);
        bias_ptr[0] = 2.0f;     // for Variant 1 (CBS)
        bias_ptr[1] = -20.0f;   // for Variant 2 (No Act)

        // Skip connection (DRAM 0x5000) = 10.0f
        float *skip_ptr = reinterpret_cast<float*>(&dram[0x5000]);
        for (int i = 0; i < 32 * 32; i++) skip_ptr[i] = 10.0f;

        // 1.1 CBS: Conv+BN+SiLU
        std::cout << "\n[1.1] Launching Conv+BN+SiLU (CBS)" << std::endl;
        wr(0x40000400, 0);        // r_in_addr
        wr(0x40000404, 0x2000);   // r_w_addr
        wr(0x40000408, 0x8000);   // r_out_addr (Variant 1 C placed here)
        wr(0x4000040C, 0x4000);   // r_bias_addr
        wr(0x40000410, 32);       // r_m
        wr(0x40000414, 32);       // r_k
        wr(0x40000418, 32);       // r_n
        wr(0x4000042C, 2);        // r_act_type = SiLU
        wr(0x40000430, 0);        // r_has_skip = 0
        wr(0x40000310, 0x12);     // Trigger
        wait_cycles(500);

        // Expected output: (32 * 0.5 * 1.0) + 2.0 = 18.0f. SiLU(18) = 18 * sigmoid(18) ~ 18.0f
        float *C_cbs = reinterpret_cast<float*>(&dram[0x8000]);
        check_close("CBS Out[0]", C_cbs[0], 18.0f);

        // 1.2 Conv+Bias (No activation)
        std::cout << "\n[1.2] Launching Conv+Bias (No activation)" << std::endl;
        wr(0x40000400, 0);
        wr(0x40000404, 0x2000);
        wr(0x40000408, 0x9000);   // Variant 2 C
        wr(0x4000040C, 0x4000 + sizeof(float)); // bias_addr (points to -20.0f)
        wr(0x4000042C, 0);        // r_act_type = None
        wr(0x40000310, 0x12);     // Trigger
        wait_cycles(500);

        // Expected output: 16.0 - 20.0 = -4.0f
        float *C_bias = reinterpret_cast<float*>(&dram[0x9000]);
        check_close("Conv+Bias Out[0]", C_bias[0], -4.0f);

        // 1.3 Linear+GELU
        std::cout << "\n[1.3] Launching Linear+GELU" << std::endl;
        // Re-populate A = 1.0f, B = 0.1f
        for (int i = 0; i < 32 * 32; i++) A_ptr[i] = 1.0f;
        for (int i = 0; i < 32 * 32; i++) B_ptr[i] = 0.1f;
        wr(0x40000400, 0);
        wr(0x40000404, 0x2000);
        wr(0x40000408, 0xA000);   // Variant 3 C
        wr(0x4000040C, 0);        // No Bias
        wr(0x4000042C, 3);        // r_act_type = GELU
        wr(0x40000310, 0x12);     // Trigger
        wait_cycles(500);

        // Expected sum: 32 * 1.0 * 0.1 = 3.2f. GELU(3.2f) ~ 3.19813f
        float *C_gelu = reinterpret_cast<float*>(&dram[0xA000]);
        check_close("Linear+GELU Out[0]", C_gelu[0], 3.19813f, 1e-3);

        // 1.4 Conv + Skip Connection (CBS + Add)
        std::cout << "\n[1.4] Launching Conv + Skip Connection" << std::endl;
        // Re-populate A = 0.5f, B = 1.0f
        for (int i = 0; i < 32 * 32; i++) A_ptr[i] = 0.5f;
        for (int i = 0; i < 32 * 32; i++) B_ptr[i] = 1.0f;
        wr(0x40000400, 0);
        wr(0x40000404, 0x2000);
        wr(0x40000408, 0xB000);   // Variant 4 C
        wr(0x4000040C, 0);        // No Bias
        wr(0x4000042C, 0);        // r_act_type = None
        wr(0x40000430, 1);        // r_has_skip = 1
        wr(0x40000434, 0x5000);   // r_skip_addr
        wr(0x40000310, 0x12);     // Trigger
        wait_cycles(500);

        // Expected: 16.0 + 10.0 = 26.0f
        float *C_skip = reinterpret_cast<float*>(&dram[0xB000]);
        check_close("Conv+Skip Out[0]", C_skip[0], 26.0f);

        // Reset Skip connection reg
        wr(0x40000430, 0);

        // --------------------------------------------------------------------
        // TEST 2: FUSED_ATTN & Q-TILING FUNCTIONAL EQUIVALENCE
        // --------------------------------------------------------------------
        std::cout << "\n--- TEST 2: FUSED_ATTN & Q-TILING EQUIVALENCE ---" << std::endl;

        // Q (at DRAM 0) = 0.1f, size 32 * 32 = 1024 floats
        float *Q_ptr = reinterpret_cast<float*>(&dram[0]);
        for (int i = 0; i < 32 * 32; i++) Q_ptr[i] = 0.1f;

        // K (at DRAM 0x2000) = 0.2f
        float *K_ptr = reinterpret_cast<float*>(&dram[0x2000]);
        for (int i = 0; i < 32 * 32; i++) K_ptr[i] = 0.2f;

        // V (at DRAM 0x4000) = 0.5f
        float *V_ptr = reinterpret_cast<float*>(&dram[0x4000]);
        for (int i = 0; i < 32 * 32; i++) V_ptr[i] = 0.5f;

        // 2.1 Non-tiled Full Attention (SeqLen = 32, HeadDim = 32, 1 Head)
        std::cout << "\n[2.1] Launching Non-Tiled FUSED_ATTN (32x32)" << std::endl;
        wr(0x40000444, 0);        // r_q_addr
        wr(0x40000448, 0x2000);   // r_k_addr
        wr(0x4000044C, 0x4000);   // r_v_addr
        wr(0x40000408, 0xC000);   // r_out_addr (Full Attn Out)
        wr(0x40000450, 32);       // r_seq_len = 32
        wr(0x40000454, 1);        // r_num_heads = 1
        wr(0x40000458, 32);       // r_head_dim = 32
        wr_f(0x4000045C, 0.125f); // r_attn_scale = 1/sqrt(64) = 0.125
        wr(0x40000310, 0x13);     // Trigger FUSED_ATTN
        wait_cycles(800);

        float *Attn_full = reinterpret_cast<float*>(&dram[0xC000]);
        std::cout << "  Non-tiled Output[0] = " << Attn_full[0] << std::endl;

        // 2.2 Tiled Attention (Split Q into two 16-row halves)
        std::cout << "\n[2.2] Launching Tiled FUSED_ATTN (Two 16x32 Passes)" << std::endl;
        // Pass 1: Q first half (offset=0, len=16) -> output to 0xD000
        wr(0x40000444, 0);        // Q start
        wr(0x40000408, 0xD000);   // Out start
        wr(0x40000450, 16);       // SeqLen = 16
        wr(0x40000310, 0x13);
        wait_cycles(500);

        // Pass 2: Q second half (offset=16 rows -> 16 * 32 * 4 = 2048 bytes) -> output to 0xD000 + 2048
        wr(0x40000444, 2048);     // Q offset
        wr(0x40000408, 0xD000 + 2048); // Out offset
        wr(0x40000450, 16);       // SeqLen = 16
        wr(0x40000310, 0x13);
        wait_cycles(500);

        // Compare non-tiled vs tiled outputs
        float *Attn_tiled = reinterpret_cast<float*>(&dram[0xD000]);
        int tile_mismatches = 0;
        for (int i = 0; i < 32 * 32; i++)
        {
            if (std::abs(Attn_full[i] - Attn_tiled[i]) > 1e-5)
            {
                tile_mismatches++;
            }
        }
        if (tile_mismatches == 0)
        {
            std::cout << "  [PASS] Q-tiling yields identical results to full calculation!" << std::endl;
        }
        else
        {
            std::cout << "  [FAIL] Q-tiling mismatch count: " << tile_mismatches << std::endl;
            errors++;
        }

        // --------------------------------------------------------------------
        // TEST 3: MULTI-HEAD FUSED_ATTN (12 HEADS LOOP)
        // --------------------------------------------------------------------
        std::cout << "\n--- TEST 3: MULTI-HEAD ATTN (12 HEADS) ---" << std::endl;
        // Q, K, V size = 16 (seq) * 12 (heads) * 32 (dim) = 6144 floats = 24576 bytes
        // Fill them up so we get non-zero values
        for (int i = 0; i < 6144; i++)
        {
            reinterpret_cast<float*>(&dram[0])[i] = 0.05f;
            reinterpret_cast<float*>(&dram[0x8000])[i] = 0.1f;
            reinterpret_cast<float*>(&dram[0x10000])[i] = 0.3f;
        }
        wr(0x40000444, 0);
        wr(0x40000448, 0x8000);   // Place K at 32KB
        wr(0x4000044C, 0x10000);  // Place V at 64KB
        wr(0x40000408, 0x18000);  // Place Out at 96KB
        wr(0x40000450, 16);       // SeqLen = 16
        wr(0x40000454, 12);       // NumHeads = 12
        wr(0x40000458, 32);       // HeadDim = 32
        wr(0x40000310, 0x13);     // Trigger
        wait_cycles(1500);

        float *Attn_mhead = reinterpret_cast<float*>(&dram[0x18000]);
        std::cout << "  Multi-head Output[0] = " << Attn_mhead[0] << std::endl;
        if (std::abs(Attn_mhead[0]) > 1e-5)
        {
            std::cout << "  [PASS] Multi-head attention loop executed successfully." << std::endl;
        }
        else
        {
            std::cout << "  [FAIL] Multi-head output is zero." << std::endl;
            errors++;
        }

        // --------------------------------------------------------------------
        // TEST 4: LAYERNORM INSTRUCTION
        // --------------------------------------------------------------------
        std::cout << "\n--- TEST 4: LAYERNORM PIPELINE ---" << std::endl;
        
        // Populate inputs in DRAM
        float *gamma_ptr = reinterpret_cast<float*>(&dram[0x6000]);
        for (int i = 0; i < 32; i++) gamma_ptr[i] = 1.5f;

        float *beta_ptr = reinterpret_cast<float*>(&dram[0x7000]);
        for (int i = 0; i < 32; i++) beta_ptr[i] = 0.5f;

        // Input at 0 (half 9.0f, half 11.0f)
        float *in_ln_ptr = reinterpret_cast<float*>(&dram[0]);
        for (int i = 0; i < 32; i++)
        {
            in_ln_ptr[i] = (i < 16) ? 9.0f : 11.0f;
        }

        wr(0x40000400, 0);        // Input A
        wr(0x40000408, 0x8000);   // Out
        wr(0x40000444, 0x6000);   // Gamma
        wr(0x4000044C, 0x7000);   // Beta
        wr(0x40000450, 1);        // SeqLen = 1
        wr(0x40000454, 32);       // Dim = 32
        wr(0x40000310, 0x14);     // Trigger
        wait_cycles(500);

        float *LN_out = reinterpret_cast<float*>(&dram[0x8000]);
        check_close("LayerNorm Out[0]", LN_out[0], -1.0f);
        check_close("LayerNorm Out[16]", LN_out[16], 2.0f);

        // --------------------------------------------------------------------
        // TEST 5: ELEM_WISE (ADD & MAXPOOL)
        // --------------------------------------------------------------------
        std::cout << "\n--- TEST 5: ELEM_WISE PIPELINES ---" << std::endl;

        // 5.1 ELEM_WISE ADD (Residual)
        std::cout << "\n[5.1] Launching ELEM_WISE ADD" << std::endl;
        
        // Input A at 0 -> 3.0f
        for (int i = 0; i < 32; i++) reinterpret_cast<float*>(&dram[0])[i] = 3.0f;

        // Input B at 0x2000 -> 4.0f
        for (int i = 0; i < 32; i++) reinterpret_cast<float*>(&dram[0x2000])[i] = 4.0f;

        wr(0x40000408, 0x9000);   // Out
        wr(0x40000444, 0);        // A
        wr(0x40000448, 0x2000);   // B
        wr(0x40000450, 32);       // Len
        wr(0x40000454, 0);        // Mode = ADD
        wr_f(0x40000458, 2.0f);   // Scale A
        wr_f(0x4000045C, 0.5f);   // Scale B
        wr_f(0x40000460, 10.0f);  // Scale Out
        wr(0x40000310, 0x15);     // Trigger
        wait_cycles(500);

        float *Add_out = reinterpret_cast<float*>(&dram[0x9000]);
        check_close("ElemWise ADD[0]", Add_out[0], 80.0f);

        // 5.2 ELEM_WISE MAX_POOL
        std::cout << "\n[5.2] Launching ELEM_WISE MAX_POOL" << std::endl;
        for (int i = 0; i < 32; i++) reinterpret_cast<float*>(&dram[0])[i] = static_cast<float>(i);
        wr(0x40000408, 0xA000);   // Out
        wr(0x40000444, 0);        // A
        wr(0x40000450, 32);       // Len = 32
        wr(0x40000454, 1);        // Mode = MAX_POOL
        wr(0x40000424, 2);        // Stride = 2
        wr(0x40000310, 0x15);     // Trigger
        wait_cycles(500);

        float *Pool_out = reinterpret_cast<float*>(&dram[0xA000]);
        check_close("MaxPool[0]", Pool_out[0], 1.0f);
        check_close("MaxPool[1]", Pool_out[1], 3.0f);
        check_close("MaxPool[15]", Pool_out[15], 31.0f);

        std::cout << "\n======================================================================" << std::endl;
        if (errors == 0)
        {
            std::cout << "   ALL CYCLE-BY-CYCLE VERIFICATION CHECKS PASSED SUCCESSFULLY!" << std::endl;
        }
        else
        {
            std::cout << "   CYCLE-BY-CYCLE VERIFICATION FAILED WITH " << errors << " ERRORS!" << std::endl;
        }
        std::cout << "======================================================================" << std::endl;

        sc_stop();
    }
};

int sc_main(int argc, char **argv)
{
    sc_clock clk("clk", 10, SC_NS);
    TbCycleByCycle tb("TbCycleByCycle_inst");
    tb.i_clk(clk);

    sc_start();
    return tb.errors;
}
