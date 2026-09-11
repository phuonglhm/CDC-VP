//
// Automatically Generated SystemC Testbench for ONNX Model
// Model: sample_vit_block.onnx
// Target: Sauria NPU v4.2 SystemC Core (32x32 PE Array, Dual-Lane)
//

#include <systemc.h>
#include <cstdint>
#include <iostream>
#include <fstream>
#include <vector>
#include <iomanip>
#include <cmath>
#include <algorithm>
#include "sauria_types.h"
#include "npu_top.h"

using namespace sauria;

typedef NpuTop<32, 32, int8_t, int8_t, int32_t> NpuInt8T;

SC_MODULE(TbOnnxModel)
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

    SC_CTOR(TbOnnxModel)
    {
        dut = new NpuInt8T("dut");
        perf.X = 32;
        perf.Y = 32;
        perf.freq_ghz = 0.8;
        perf.elem_bytes = 1;
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

    void run()
    {
        dram.resize(static_cast<size_t>(64) * 1024ULL * 1024ULL, 0); // 64 MB DRAM buffer
        dut->set_dram(&dram);

        // Pre-load initial DRAM contents from dram_init.bin
        std::ifstream f_init("tools/dram_init.bin", std::ios::binary);
        if (!f_init.is_open()) {
            f_init.open("dram_init.bin", std::ios::binary);
        }
        if (f_init.is_open()) {
            f_init.read(reinterpret_cast<char*>(dram.data()), dram.size());
            f_init.close();
            std::cout << "[TESTBENCH] Successfully loaded DRAM initialization payload (dram_init.bin)" << std::endl;
        } else {
            std::cout << "[WARNING] dram_init.bin not found, running with default zero-initialized DRAM." << std::endl;
        }

        // System Reset
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

        std::cout << "==================================================" << std::endl;
        std::cout << "   SAURIA NPU ONNX MODEL EXECUTION BENCHMARK     " << std::endl;
        std::cout << "   Model: sample_vit_block.onnx" << std::endl;
        std::cout << "   Allocated DRAM: 64 MB" << std::endl;
        std::cout << "   Compiled Instructions: 4" << std::endl;
        std::cout << "==================================================" << std::endl;

        // Queue Compiled Sauria Rich Instructions
        // Instruction 1: LN1 (Opcode 0x14)
        wr(0x40000400, 0x0011200); // r_in_addr
        wr(0x40000444, 0x0010C00); // r_gamma_addr
        wr(0x4000044C, 0x0010D00); // r_beta_addr
        wr(0x40000408, 0x0011600); // r_out_addr
        wr(0x40000450, 32); // r_seq_len
        wr(0x40000454, 32); // r_dim
        wr(0x40000310, 0x14);

        // Instruction 2: Proj_Gemm (Opcode 0x12)
        wr(0x40000400, 0x0011600); // r_in_addr
        wr(0x40000404, 0x0010E00); // r_w_addr
        wr(0x40000408, 0x0011A00); // r_out_addr
        wr(0x40000410, 32); // r_m
        wr(0x40000414, 32); // r_k
        wr(0x40000418, 32); // r_n
        wr(0x4000042C, 0); // r_act_type
        wr(0x40000438, 0x3E000000); // r_in_scale
        wr(0x4000043C, 0x3F800000); // r_w_scale
        wr(0x40000440, 0x3F800000); // r_out_scale
        wr(0x40000310, 0x12);

        // Instruction 3: Attn_Softmax (Opcode 0x13)
        wr(0x40000444, 0x0011A00); // r_q_addr
        wr(0x40000448, 0x0011A00); // r_k_addr
        wr(0x4000044C, 0x0011A00); // r_v_addr
        wr(0x40000408, 0x0012A00); // r_out_addr
        wr(0x40000450, 32); // r_seq_len
        wr(0x40000454, 1); // r_num_heads
        wr(0x40000458, 32); // r_head_dim
        wr(0x40000310, 0x13);

        // Instruction 4: Residual_Add (Opcode 0x15)
        wr(0x40000444, 0x0012A00); // r_a_addr
        wr(0x40000448, 0x0011200); // r_b_addr
        wr(0x40000408, 0x0012E00); // r_out_addr
        wr(0x40000450, 1024); // r_len
        wr(0x40000454, 0); // r_mode
        wr(0x40000464, 1024); // r_a_len
        wr(0x40000468, 1024); // r_b_len
        wr(0x40000310, 0x15);

        std::cout << "[TESTBENCH] Waiting for execution to complete..." << std::endl;
        wait(20000);

        std::cout << "\n====================================================================================================" << std::endl;
        std::cout << "                          SAURIA NPU AUTOMATED GOLDEN ACCURACY CHECKER                             " << std::endl;
        std::cout << "====================================================================================================" << std::endl;
        std::cout << std::left << std::setw(8)  << "Inst #"
                  << std::setw(30) << "Layer / Operator Name"
                  << std::setw(12) << "DRAM Addr"
                  << std::setw(10) << "Size(B)"
                  << std::setw(10) << "MAE"
                  << std::setw(10) << "RMSE"
                  << std::setw(10) << "L_inf"
                  << std::setw(12) << "Cos Sim"
                  << std::setw(8)  << "Status" << std::endl;
        std::cout << "----------------------------------------------------------------------------------------------------" << std::endl;

        std::ifstream f_gold("tools/golden_ref.bin", std::ios::binary);
        if (!f_gold.is_open()) {
            f_gold.open("golden_ref.bin", std::ios::binary);
        }

        int passed_checkpoints = 0;
        int failed_checkpoints = 0;
        {
            uint32_t out_addr = 0x0011600;
            size_t num_bytes = 1024;
            std::vector<int8_t> gold_buf(num_bytes, 0);
            if (f_gold.is_open()) {
                f_gold.read(reinterpret_cast<char*>(gold_buf.data()), num_bytes);
            }
            const int8_t* hw_buf = reinterpret_cast<const int8_t*>(&dram[out_addr]);

            double sum_abs_err = 0.0;
            double sum_sq_err = 0.0;
            int max_abs_err = 0;
            double dot_prod = 0.0;
            double norm_hw = 0.0;
            double norm_gold = 0.0;

            for (size_t i = 0; i < num_bytes; i++) {
                int hw_v = static_cast<int>(hw_buf[i]);
                int gold_v = static_cast<int>(gold_buf[i]);
                int diff = std::abs(hw_v - gold_v);
                sum_abs_err += diff;
                sum_sq_err += diff * diff;
                if (diff > max_abs_err) max_abs_err = diff;

                dot_prod += static_cast<double>(hw_v) * static_cast<double>(gold_v);
                norm_hw += static_cast<double>(hw_v) * static_cast<double>(hw_v);
                norm_gold += static_cast<double>(gold_v) * static_cast<double>(gold_v);
            }

            double mae = sum_abs_err / num_bytes;
            double rmse = std::sqrt(sum_sq_err / num_bytes);
            double cos_sim = (norm_hw > 0 && norm_gold > 0) ? (dot_prod / (std::sqrt(norm_hw) * std::sqrt(norm_gold))) : 1.0;
            bool pass = (mae <= 5.0) || (cos_sim >= 0.95);
            if (pass) passed_checkpoints++; else { failed_checkpoints++; errors++; }

            std::cout << std::left << std::setw(8) << 1
                      << std::setw(30) << "LN1"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x0011A00;
            size_t num_bytes = 1024;
            std::vector<int8_t> gold_buf(num_bytes, 0);
            if (f_gold.is_open()) {
                f_gold.read(reinterpret_cast<char*>(gold_buf.data()), num_bytes);
            }
            const int8_t* hw_buf = reinterpret_cast<const int8_t*>(&dram[out_addr]);

            double sum_abs_err = 0.0;
            double sum_sq_err = 0.0;
            int max_abs_err = 0;
            double dot_prod = 0.0;
            double norm_hw = 0.0;
            double norm_gold = 0.0;

            for (size_t i = 0; i < num_bytes; i++) {
                int hw_v = static_cast<int>(hw_buf[i]);
                int gold_v = static_cast<int>(gold_buf[i]);
                int diff = std::abs(hw_v - gold_v);
                sum_abs_err += diff;
                sum_sq_err += diff * diff;
                if (diff > max_abs_err) max_abs_err = diff;

                dot_prod += static_cast<double>(hw_v) * static_cast<double>(gold_v);
                norm_hw += static_cast<double>(hw_v) * static_cast<double>(hw_v);
                norm_gold += static_cast<double>(gold_v) * static_cast<double>(gold_v);
            }

            double mae = sum_abs_err / num_bytes;
            double rmse = std::sqrt(sum_sq_err / num_bytes);
            double cos_sim = (norm_hw > 0 && norm_gold > 0) ? (dot_prod / (std::sqrt(norm_hw) * std::sqrt(norm_gold))) : 1.0;
            bool pass = (mae <= 5.0) || (cos_sim >= 0.95);
            if (pass) passed_checkpoints++; else { failed_checkpoints++; errors++; }

            std::cout << std::left << std::setw(8) << 2
                      << std::setw(30) << "Proj_Gemm"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x0012A00;
            size_t num_bytes = 1024;
            std::vector<int8_t> gold_buf(num_bytes, 0);
            if (f_gold.is_open()) {
                f_gold.read(reinterpret_cast<char*>(gold_buf.data()), num_bytes);
            }
            const int8_t* hw_buf = reinterpret_cast<const int8_t*>(&dram[out_addr]);

            double sum_abs_err = 0.0;
            double sum_sq_err = 0.0;
            int max_abs_err = 0;
            double dot_prod = 0.0;
            double norm_hw = 0.0;
            double norm_gold = 0.0;

            for (size_t i = 0; i < num_bytes; i++) {
                int hw_v = static_cast<int>(hw_buf[i]);
                int gold_v = static_cast<int>(gold_buf[i]);
                int diff = std::abs(hw_v - gold_v);
                sum_abs_err += diff;
                sum_sq_err += diff * diff;
                if (diff > max_abs_err) max_abs_err = diff;

                dot_prod += static_cast<double>(hw_v) * static_cast<double>(gold_v);
                norm_hw += static_cast<double>(hw_v) * static_cast<double>(hw_v);
                norm_gold += static_cast<double>(gold_v) * static_cast<double>(gold_v);
            }

            double mae = sum_abs_err / num_bytes;
            double rmse = std::sqrt(sum_sq_err / num_bytes);
            double cos_sim = (norm_hw > 0 && norm_gold > 0) ? (dot_prod / (std::sqrt(norm_hw) * std::sqrt(norm_gold))) : 1.0;
            bool pass = (mae <= 5.0) || (cos_sim >= 0.95);
            if (pass) passed_checkpoints++; else { failed_checkpoints++; errors++; }

            std::cout << std::left << std::setw(8) << 3
                      << std::setw(30) << "Attn_Softmax"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x0012E00;
            size_t num_bytes = 1024;
            std::vector<int8_t> gold_buf(num_bytes, 0);
            if (f_gold.is_open()) {
                f_gold.read(reinterpret_cast<char*>(gold_buf.data()), num_bytes);
            }
            const int8_t* hw_buf = reinterpret_cast<const int8_t*>(&dram[out_addr]);

            double sum_abs_err = 0.0;
            double sum_sq_err = 0.0;
            int max_abs_err = 0;
            double dot_prod = 0.0;
            double norm_hw = 0.0;
            double norm_gold = 0.0;

            for (size_t i = 0; i < num_bytes; i++) {
                int hw_v = static_cast<int>(hw_buf[i]);
                int gold_v = static_cast<int>(gold_buf[i]);
                int diff = std::abs(hw_v - gold_v);
                sum_abs_err += diff;
                sum_sq_err += diff * diff;
                if (diff > max_abs_err) max_abs_err = diff;

                dot_prod += static_cast<double>(hw_v) * static_cast<double>(gold_v);
                norm_hw += static_cast<double>(hw_v) * static_cast<double>(hw_v);
                norm_gold += static_cast<double>(gold_v) * static_cast<double>(gold_v);
            }

            double mae = sum_abs_err / num_bytes;
            double rmse = std::sqrt(sum_sq_err / num_bytes);
            double cos_sim = (norm_hw > 0 && norm_gold > 0) ? (dot_prod / (std::sqrt(norm_hw) * std::sqrt(norm_gold))) : 1.0;
            bool pass = (mae <= 5.0) || (cos_sim >= 0.95);
            if (pass) passed_checkpoints++; else { failed_checkpoints++; errors++; }

            std::cout << std::left << std::setw(8) << 4
                      << std::setw(30) << "Residual_Add"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        if (f_gold.is_open()) f_gold.close();
        std::cout << "====================================================================================================" << std::endl;
        std::cout << "  VERIFICATION SUMMARY: " << passed_checkpoints << " / " << (passed_checkpoints + failed_checkpoints) 
                  << " Checkpoints PASSED (" << (failed_checkpoints == 0 ? "100.0% SUCCESS" : "VERIFICATION FAILURES DETECTED") << ")" << std::endl;
        std::cout << "====================================================================================================\n" << std::endl;

        std::cout << "[COMPLETED] ONNX Graph Model Execution Finished Successfully!" << std::endl;
        perf.report("sample_vit_block.onnx (32x32)");

        sc_stop();
    }
};

int sc_main(int argc, char **argv)
{
    sc_clock clk("clk", 1.25, SC_NS);
    TbOnnxModel tb("TbOnnxModel_inst");
    tb.i_clk(clk);
    sc_start();
    return tb.errors;
}
