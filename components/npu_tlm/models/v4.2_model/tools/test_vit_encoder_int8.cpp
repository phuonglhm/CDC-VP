#include <systemc.h>
#include <cstdint>
#include <iostream>
#include <vector>
#include <iomanip>
#include <cmath>
#include <algorithm>
#include "sauria_types.h"
#include "npu_top.h"

using namespace sauria;

// Parameterized configuration for the target 64x64 INT8/INT32 NPU
typedef NpuTop<64, 64, int8_t, int8_t, int32_t, 1024, 1024, 2048, 16, 128, 1> NpuInt8T;

SC_MODULE(TbVitEncoderInt8)
{
    sc_in<bool> i_clk;

    sc_signal<bool> rstn, soft_reset, start, done, deadlock;
    sc_signal<uint32_t> mvm_k, host_addr, total_contexts;
    sc_signal<bool> host_wren, host_rden;
    sc_signal<host_data_t> host_wdata, host_rdata;
    sc_signal<host_mask_t> host_wmask;
    sc_signal<float> threshold;
    sc_signal<sc_bv<3>> select;

    NpuInt8T *dut;
    fx1::PerfCounters perf;
    std::vector<uint8_t> dram;
    int errors = 0;

    SC_CTOR(TbVitEncoderInt8)
    {
        dut = new NpuInt8T("dut");
        perf.X = 64;
        perf.Y = 64;
        dut->attach_perf(&perf);
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
        d.data.fill(0.0);
        d[0] = static_cast<double>(val);
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
        d.data.fill(0.0);
        d[0] = static_cast<double>(val);
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

    // Helper to calculate GELU (tanh approximation) matching executor math
    double gelu(double x)
    {
        double cdf = 0.5 * (1.0 + std::tanh(std::sqrt(2.0 / M_PI) * (x + 0.044715 * x * x * x)));
        return x * cdf;
    }

    void run()
    {
        // Allocate 2MB of virtual DRAM
        dram.resize(2 * 1024 * 1024, 0);
        dut->set_dram(&dram);

        // Reset system
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

        std::cout << "\n==================================================" << std::endl;
        std::cout << "   STARTING VIT ENCODER BLOCK INT8/INT32 VERIFICATION" << std::endl;
        std::cout << "   Geometry: 64x64, Frequency: 800MHz (1.25ns)" << std::endl;
        std::cout << "==================================================" << std::endl;

        // --------------------------------------------------------
        // Step 1: Populate DRAM with Stimuli Data (INT8 range [-127, 127])
        // --------------------------------------------------------

        // Deterministic Pseudo-Random Generation formula
        auto gen_val = [](int i, float scale = 64.0f) {
            float val = std::sin(static_cast<float>(i)) * scale;
            if (val > 127.0f) val = 127.0f;
            if (val < -127.0f) val = -127.0f;
            return static_cast<int8_t>(std::round(val));
        };

        // 1. Input X (shape 64x64, offset 0x00000)
        int8_t *X_ptr = reinterpret_cast<int8_t*>(&dram[0x00000]);
        for (int i = 0; i < 64 * 64; i++) X_ptr[i] = gen_val(i, 80.0f);

        // 2. LN1 Gamma1 and Beta1 (size 64, offsets 0x01000 and 0x01040)
        int8_t *gamma1_ptr = reinterpret_cast<int8_t*>(&dram[0x01000]);
        int8_t *beta1_ptr = reinterpret_cast<int8_t*>(&dram[0x01040]);
        for (int i = 0; i < 64; i++) {
            gamma1_ptr[i] = gen_val(i + 10, 10.0f);
            beta1_ptr[i] = gen_val(i + 20, 5.0f);
        }

        // 3. QKV Weights (shape 64x192, offset 0x02100)
        int8_t *W_qkv_ptr = reinterpret_cast<int8_t*>(&dram[0x02100]);
        for (int i = 0; i < 64 * 192; i++) W_qkv_ptr[i] = gen_val(i + 30, 20.0f);

        // 4. Proj Weights (shape 64x64, offset 0x09100)
        int8_t *W_proj_ptr = reinterpret_cast<int8_t*>(&dram[0x09100]);
        for (int i = 0; i < 64 * 64; i++) W_proj_ptr[i] = gen_val(i + 40, 20.0f);

        // 5. LN2 Gamma2 and Beta2 (size 64, offsets 0x0C100 and 0x0C140)
        int8_t *gamma2_ptr = reinterpret_cast<int8_t*>(&dram[0x0C100]);
        int8_t *beta2_ptr = reinterpret_cast<int8_t*>(&dram[0x0C140]);
        for (int i = 0; i < 64; i++) {
            gamma2_ptr[i] = gen_val(i + 50, 10.0f);
            beta2_ptr[i] = gen_val(i + 60, 5.0f);
        }

        // 6. FFN1 Weights (shape 64x64, offset 0x0D180)
        int8_t *W_ffn1_ptr = reinterpret_cast<int8_t*>(&dram[0x0D180]);
        for (int i = 0; i < 64 * 64; i++) W_ffn1_ptr[i] = gen_val(i + 70, 20.0f);

        // 7. FFN2 Weights (shape 64x64, offset 0x0F180)
        int8_t *W_ffn2_ptr = reinterpret_cast<int8_t*>(&dram[0x0F180]);
        for (int i = 0; i < 64 * 64; i++) W_ffn2_ptr[i] = gen_val(i + 80, 20.0f);


        // --------------------------------------------------------
        // Step 2: Program and Dispatch the 9 Chained Instructions
        // --------------------------------------------------------

        std::cout << "[TESTBENCH] Pushing the 9 chained instructions..." << std::endl;

        // --- Instruction 1: LN1 ---
        wr(0x40000400, 0x00000); // r_in_addr (X)
        wr(0x40000408, 0x01080); // r_out_addr (X_ln1)
        wr(0x40000444, 0x01000); // r_gamma_addr
        wr(0x4000044C, 0x01040); // r_beta_addr
        wr(0x40000450, 64);      // r_seq_len
        wr(0x40000454, 64);      // r_dim
        wr(0x40000310, 0x14);    // Trigger opcode 0x14 (LAYERNORM)

        // --- Instruction 2: QKV Projection ---
        wr(0x40000400, 0x01080); // r_in_addr (X_ln1)
        wr(0x40000404, 0x02100); // r_w_addr (W_qkv)
        wr(0x40000408, 0x05100); // r_out_addr (QKV)
        wr(0x4000040C, 0);       // r_bias_addr
        wr(0x40000410, 64);      // r_m
        wr(0x40000414, 64);      // r_k
        wr(0x40000418, 192);     // r_n (192 columns: Q, K, V)
        wr(0x4000042C, 0);       // r_act_type (None)
        wr(0x40000430, 0);       // r_has_skip = 0
        wr_f(0x40000438, 0.005f); // r_in_scale
        wr_f(0x4000043C, 0.005f); // r_w_scale
        wr_f(0x40000440, 1.0f);  // r_out_scale
        wr(0x40000310, 0x12);    // Trigger opcode 0x12 (GEMM_FUSED)

        // --- Instruction 3: FUSED_ATTN ---
        wr(0x40000444, 0x05100); // r_q_addr
        wr(0x40000448, 0x06100); // r_k_addr (QKV + 4096 bytes)
        wr(0x4000044C, 0x07100); // r_v_addr (QKV + 8192 bytes)
        wr(0x40000408, 0x08100); // r_out_addr (Attn_out)
        wr(0x40000450, 64);      // r_seq_len
        wr(0x40000454, 1);       // r_num_heads
        wr(0x40000458, 64);      // r_head_dim
        wr_f(0x4000045C, 0.125f); // r_attn_scale = 1/sqrt(64)
        wr(0x40000310, 0x13);    // Trigger opcode 0x13 (FUSED_ATTN)

        // --- Instruction 4: Proj MatMul ---
        wr(0x40000400, 0x08100); // r_in_addr (Attn_out)
        wr(0x40000404, 0x09100); // r_w_addr (W_proj)
        wr(0x40000408, 0x0A100); // r_out_addr (Proj_out)
        wr(0x4000040C, 0);       // r_bias_addr
        wr(0x40000410, 64);      // r_m
        wr(0x40000414, 64);      // r_k
        wr(0x40000418, 64);      // r_n
        wr(0x4000042C, 0);       // r_act_type (None)
        wr(0x40000430, 0);       // r_has_skip = 0
        wr_f(0x40000438, 0.015f); // r_in_scale
        wr_f(0x4000043C, 0.015f); // r_w_scale
        wr_f(0x40000440, 1.0f);  // r_out_scale
        wr(0x40000310, 0x12);    // Trigger opcode 0x12 (GEMM_FUSED)

        // --- Instruction 5: ADD (Residual 1) ---
        wr(0x40000444, 0x0A100); // r_a_addr (Proj_out)
        wr(0x40000448, 0x00000); // r_b_addr (X)
        wr(0x40000408, 0x0B100); // r_out_addr (X2)
        wr(0x40000450, 4096);    // r_len (64x64 elements)
        wr(0x40000454, 0);       // r_mode = 0 (ADD)
        wr_f(0x40000458, 1.0f);  // r_scale_a
        wr_f(0x4000045C, 1.0f);  // r_scale_b
        wr_f(0x40000460, 1.0f);  // r_scale_out
        wr(0x40000310, 0x15);    // Trigger opcode 0x15 (ELEM_WISE)

        // --- Instruction 6: LN2 ---
        wr(0x40000400, 0x0B100); // r_in_addr (X2)
        wr(0x40000408, 0x0C180); // r_out_addr (X2_ln)
        wr(0x40000444, 0x0C100); // r_gamma_addr
        wr(0x4000044C, 0x0C140); // r_beta_addr
        wr(0x40000450, 64);      // r_seq_len
        wr(0x40000454, 64);      // r_dim
        wr(0x40000310, 0x14);    // Trigger opcode 0x14 (LAYERNORM)

        // --- Instruction 7: FFN1 (GELU) ---
        wr(0x40000400, 0x0C180); // r_in_addr (X2_ln)
        wr(0x40000404, 0x0D180); // r_w_addr (W_ffn1)
        wr(0x40000408, 0x0E180); // r_out_addr (FFN1_out)
        wr(0x4000040C, 0);       // r_bias_addr
        wr(0x40000410, 64);      // r_m
        wr(0x40000414, 64);      // r_k
        wr(0x40000418, 64);      // r_n
        wr(0x4000042C, 3);       // r_act_type = 3 (GELU)
        wr(0x40000430, 0);       // r_has_skip = 0
        wr_f(0x40000438, 0.015f); // r_in_scale
        wr_f(0x4000043C, 0.015f); // r_w_scale
        wr_f(0x40000440, 1.0f);  // r_out_scale
        wr(0x40000310, 0x12);    // Trigger opcode 0x12 (GEMM_FUSED)

        // --- Instruction 8: FFN2 (Linear) ---
        wr(0x40000400, 0x0E180); // r_in_addr (FFN1_out)
        wr(0x40000404, 0x0F180); // r_w_addr (W_ffn2)
        wr(0x40000408, 0x10180); // r_out_addr (FFN2_out)
        wr(0x4000040C, 0);       // r_bias_addr
        wr(0x40000410, 64);      // r_m
        wr(0x40000414, 64);      // r_k
        wr(0x40000418, 64);      // r_n
        wr(0x4000042C, 0);       // r_act_type = 0 (None)
        wr(0x40000430, 0);       // r_has_skip = 0
        wr_f(0x40000438, 0.015f); // r_in_scale
        wr_f(0x4000043C, 0.015f); // r_w_scale
        wr_f(0x40000440, 1.0f);  // r_out_scale
        wr(0x40000310, 0x12);    // Trigger opcode 0x12 (GEMM_FUSED)

        // --- Instruction 9: ADD (Residual 2) ---
        wr(0x40000444, 0x10180); // r_a_addr (FFN2_out)
        wr(0x40000448, 0x0B100); // r_b_addr (X2)
        wr(0x40000408, 0x11180); // r_out_addr (Y final)
        wr(0x40000450, 4096);    // r_len (64x64 elements)
        wr(0x40000454, 0);       // r_mode = 0 (ADD)
        wr_f(0x40000458, 1.0f);  // r_scale_a
        wr_f(0x4000045C, 1.0f);  // r_scale_b
        wr_f(0x40000460, 1.0f);  // r_scale_out
        wr(0x40000310, 0x15);    // Trigger opcode 0x15 (ELEM_WISE)

        // --------------------------------------------------------
        // Step 3: Wait for Execution to Finish
        // --------------------------------------------------------
        std::cout << "[TESTBENCH] Waiting for execution cycles..." << std::endl;
        wait(20000); // 20000 simulation clock cycles should be plenty for 64x64 dimensions

        // --------------------------------------------------------
        // Step 4: Golden Reference Computation in C++
        // --------------------------------------------------------
        std::cout << "[GOLDEN] Running reference C++ model..." << std::endl;

        std::vector<int8_t> gold_X(64 * 64);
        std::vector<int8_t> gold_X_ln1(64 * 64);
        std::vector<int8_t> gold_QKV(64 * 192);
        std::vector<int8_t> gold_Attn_out(64 * 64);
        std::vector<int8_t> gold_Proj_out(64 * 64);
        std::vector<int8_t> gold_X2(64 * 64);
        std::vector<int8_t> gold_X2_ln(64 * 64);
        std::vector<int8_t> gold_FFN1_out(64 * 64);
        std::vector<int8_t> gold_FFN2_out(64 * 64);
        std::vector<int8_t> gold_Y(64 * 64);

        // Copy input X
        for (int i = 0; i < 64 * 64; i++) gold_X[i] = X_ptr[i];

        // 1. Golden LN1
        double eps = 1e-5;
        for (int i = 0; i < 64; i++) {
            double sum = 0.0;
            for (int d = 0; d < 64; d++) sum += static_cast<double>(gold_X[i * 64 + d]);
            double mean = sum / 64.0;

            double sum_sq = 0.0;
            for (int d = 0; d < 64; d++) {
                double diff = static_cast<double>(gold_X[i * 64 + d]) - mean;
                sum_sq += diff * diff;
            }
            double var = sum_sq / 64.0;
            double inv_std = 1.0 / std::sqrt(var + eps);

            for (int d = 0; d < 64; d++) {
                double norm = (static_cast<double>(gold_X[i * 64 + d]) - mean) * inv_std;
                double scaled = norm * static_cast<double>(gamma1_ptr[d]) + static_cast<double>(beta1_ptr[d]);
                gold_X_ln1[i * 64 + d] = static_cast<int8_t>(scaled);
            }
        }

        // 2. Golden QKV Projection
        for (int r = 0; r < 64; r++) {
            for (int c = 0; c < 192; c++) {
                double sum = 0.0;
                for (int i = 0; i < 64; i++) {
                    sum += static_cast<double>(gold_X_ln1[r * 64 + i]) * static_cast<double>(W_qkv_ptr[i * 192 + c]);
                }
                double scaled = sum * 0.005 * 0.005 * 1.0;
                gold_QKV[r * 192 + c] = static_cast<int8_t>(scaled);
            }
        }

        // 3. Golden Attention (FUSED_ATTN equivalent)
        float scale = 0.125f;
        std::vector<double> qk(64 * 64, 0.0);
        // Q * K^T
        for (int i = 0; i < 64; i++) {
            for (int j = 0; j < 64; j++) {
                double sum = 0.0;
                for (int d = 0; d < 64; d++) {
                    double q_val = gold_QKV[i * 192 + d];
                    double k_val = gold_QKV[j * 192 + 64 + d];
                    sum += q_val * k_val;
                }
                qk[i * 64 + j] = sum * scale;
            }
        }
        // Softmax
        for (int i = 0; i < 64; i++) {
            double max_val = qk[i * 64];
            for (int j = 1; j < 64; j++) max_val = std::max(max_val, qk[i * 64 + j]);

            double sum_exp = 0.0;
            for (int j = 0; j < 64; j++) {
                qk[i * 64 + j] = std::exp(qk[i * 64 + j] - max_val);
                sum_exp += qk[i * 64 + j];
            }
            for (int j = 0; j < 64; j++) qk[i * 64 + j] /= sum_exp;
        }
        // Attn * V
        for (int i = 0; i < 64; i++) {
            for (int d = 0; d < 64; d++) {
                double sum = 0.0;
                for (int j = 0; j < 64; j++) {
                    double attn_val = qk[i * 64 + j];
                    double v_val = gold_QKV[j * 192 + 128 + d];
                    sum += attn_val * v_val;
                }
                gold_Attn_out[i * 64 + d] = static_cast<int8_t>(sum);
            }
        }

        // 4. Golden Output Projection
        for (int r = 0; r < 64; r++) {
            for (int c = 0; c < 64; c++) {
                double sum = 0.0;
                for (int i = 0; i < 64; i++) {
                    sum += static_cast<double>(gold_Attn_out[r * 64 + i]) * static_cast<double>(W_proj_ptr[i * 64 + c]);
                }
                double scaled = sum * 0.015 * 0.015 * 1.0;
                gold_Proj_out[r * 64 + c] = static_cast<int8_t>(scaled);
            }
        }

        // 5. Golden Residual 1 (Add)
        for (int i = 0; i < 64 * 64; i++) {
            double res = 1.0 * (1.0 * static_cast<double>(gold_Proj_out[i]) + 1.0 * static_cast<double>(gold_X[i]));
            gold_X2[i] = static_cast<int8_t>(res);
        }

        // 6. Golden LN2
        for (int i = 0; i < 64; i++) {
            double sum = 0.0;
            for (int d = 0; d < 64; d++) sum += static_cast<double>(gold_X2[i * 64 + d]);
            double mean = sum / 64.0;

            double sum_sq = 0.0;
            for (int d = 0; d < 64; d++) {
                double diff = static_cast<double>(gold_X2[i * 64 + d]) - mean;
                sum_sq += diff * diff;
            }
            double var = sum_sq / 64.0;
            double inv_std = 1.0 / std::sqrt(var + eps);

            for (int d = 0; d < 64; d++) {
                double norm = (static_cast<double>(gold_X2[i * 64 + d]) - mean) * inv_std;
                double scaled = norm * static_cast<double>(gamma2_ptr[d]) + static_cast<double>(beta2_ptr[d]);
                gold_X2_ln[i * 64 + d] = static_cast<int8_t>(scaled);
            }
        }

        // 7. Golden FFN1 (GELU)
        for (int r = 0; r < 64; r++) {
            for (int c = 0; c < 64; c++) {
                double sum = 0.0;
                for (int i = 0; i < 64; i++) {
                    sum += static_cast<double>(gold_X2_ln[r * 64 + i]) * static_cast<double>(W_ffn1_ptr[i * 64 + c]);
                }
                double scaled = sum * 0.015 * 0.015 * 1.0;
                gold_FFN1_out[r * 64 + c] = static_cast<int8_t>(gelu(scaled));
            }
        }

        // 8. Golden FFN2 (Linear)
        for (int r = 0; r < 64; r++) {
            for (int c = 0; c < 64; c++) {
                double sum = 0.0;
                for (int i = 0; i < 64; i++) {
                    sum += static_cast<double>(gold_FFN1_out[r * 64 + i]) * static_cast<double>(W_ffn2_ptr[i * 64 + c]);
                }
                double scaled = sum * 0.015 * 0.015 * 1.0;
                gold_FFN2_out[r * 64 + c] = static_cast<int8_t>(scaled);
            }
        }

        // 9. Golden Residual 2 (Add)
        for (int i = 0; i < 64 * 64; i++) {
            double res = 1.0 * (1.0 * static_cast<double>(gold_FFN2_out[i]) + 1.0 * static_cast<double>(gold_X2[i]));
            gold_Y[i] = static_cast<int8_t>(res);
        }


        // --------------------------------------------------------
        // Step 5: Verification & Diagnostics
        // --------------------------------------------------------
        int8_t *sim_Y = reinterpret_cast<int8_t*>(&dram[0x11180]);
        int mismatches = 0;
        int max_diff = 0;

        std::cout << "[VERIFICATION] Comparing Simulator final output with Golden C++..." << std::endl;
        
        for (int i = 0; i < 64 * 64; i++) {
            int diff = std::abs(static_cast<int>(sim_Y[i]) - static_cast<int>(gold_Y[i]));
            max_diff = std::max(max_diff, diff);
            
            if (diff > 0) {
                mismatches++;
                if (mismatches < 10) {
                    std::cout << "  Mismatch at index " << i 
                              << " (row " << (i / 64) << ", col " << (i % 64) << "): got " 
                              << static_cast<int>(sim_Y[i]) << ", expected " << static_cast<int>(gold_Y[i]) 
                              << " (diff=" << diff << ")" << std::endl;
                }
            }
        }

        std::cout << "\n==================================================" << std::endl;
        std::cout << "               VERIFICATION RESULTS               " << std::endl;
        std::cout << "==================================================" << std::endl;
        std::cout << "  Total Mismatches : " << mismatches << std::endl;
        std::cout << "  Maximum Abs Diff : " << max_diff << std::endl;

        if (mismatches == 0) {
            std::cout << "  [PASS] ViT Encoder Block Chained Execution SUCCEEDED!" << std::endl;
        } else {
            std::cout << "  [FAIL] ViT Encoder Block Verification FAILED!" << std::endl;
            errors++;
        }
        std::cout << "==================================================\n" << std::endl;

        perf.report("ViT Encoder Block (64x64)");

        sc_stop();
    }
};

int sc_main(int argc, char **argv)
{
    // Clock setup for 800 MHz (1.25 ns period)
    sc_clock clk("clk", 1.25, SC_NS);
    TbVitEncoderInt8 tb("TbVitEncoderInt8_inst");
    tb.i_clk(clk);

    sc_start();
    return tb.errors;
}
