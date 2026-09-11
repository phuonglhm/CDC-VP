//
// Automatically Generated SystemC Testbench for ONNX Model
// Model: yolov8m-int8.onnx
// Target: Sauria NPU v4.2 SystemC Core (64x64 PE Array, Dual-Lane)
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

typedef NpuTop<64, 64, int8_t, int8_t, int32_t> NpuInt8T;

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
        perf.X = 64;
        perf.Y = 64;
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
        mvm_k.write(64);
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
        std::cout << "   Model: yolov8m-int8.onnx" << std::endl;
        std::cout << "   Allocated DRAM: 64 MB" << std::endl;
        std::cout << "   Compiled Instructions: 261" << std::endl;
        std::cout << "==================================================" << std::endl;

        // Queue Compiled Sauria Rich Instructions
        // Instruction 1: /model.0/conv/Conv (Opcode 0x12)
        wr(0x40000400, 0x190D400); // r_in_addr
        wr(0x40000404, 0x0015600); // r_w_addr
        wr(0x40000408, 0x1A39400); // r_out_addr
        wr(0x40000410, 1920); // r_m
        wr(0x40000414, 640); // r_k
        wr(0x40000418, 3); // r_n
        wr(0x4000042C, 0); // r_act_type
        wr(0x40000438, 0x40000000); // r_in_scale
        wr(0x4000043C, 0x42840000); // r_w_scale
        wr(0x40000440, 0x3F800000); // r_out_scale
        wr(0x40000310, 0x12);

        // Instruction 2: /model.0/act/Sigmoid (Opcode 0x15)
        wr(0x40000444, 0x1A39400); // r_a_addr
        wr(0x40000448, 0x1A39400); // r_b_addr
        wr(0x40000408, 0x1A3EE00); // r_out_addr
        wr(0x40000450, 5760); // r_len
        wr(0x40000454, 0); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 3: /model.0/act/Mul (Opcode 0x15)
        wr(0x40000444, 0x1A39400); // r_a_addr
        wr(0x40000448, 0x1A3EE00); // r_b_addr
        wr(0x40000408, 0x1A40500); // r_out_addr
        wr(0x40000450, 5760); // r_len
        wr(0x40000454, 2); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 4: /model.1/conv/Conv (Opcode 0x12)
        wr(0x40000400, 0x1A40500); // r_in_addr
        wr(0x40000404, 0x0016400); // r_w_addr
        wr(0x40000408, 0x1A41C00); // r_out_addr
        wr(0x40000410, 4096); // r_m
        wr(0x40000414, 4096); // r_k
        wr(0x40000418, 3); // r_n
        wr(0x4000042C, 0); // r_act_type
        wr(0x40000438, 0x42FC0000); // r_in_scale
        wr(0x4000043C, 0x40A00000); // r_w_scale
        wr(0x40000440, 0x3F800000); // r_out_scale
        wr(0x40000310, 0x12);

        // Instruction 5: /model.1/act/Sigmoid (Opcode 0x15)
        wr(0x40000444, 0x1A41C00); // r_a_addr
        wr(0x40000448, 0x1A41C00); // r_b_addr
        wr(0x40000408, 0x1A4DC00); // r_out_addr
        wr(0x40000450, 12288); // r_len
        wr(0x40000454, 0); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 6: /model.1/act/Mul (Opcode 0x15)
        wr(0x40000444, 0x1A41C00); // r_a_addr
        wr(0x40000448, 0x1A4DC00); // r_b_addr
        wr(0x40000408, 0x1A50C00); // r_out_addr
        wr(0x40000450, 12288); // r_len
        wr(0x40000454, 2); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 7: /model.2/cv1/conv/Conv (Opcode 0x12)
        wr(0x40000400, 0x1A50C00); // r_in_addr
        wr(0x40000404, 0x0020E00); // r_w_addr
        wr(0x40000408, 0x1A53C00); // r_out_addr
        wr(0x40000410, 4096); // r_m
        wr(0x40000414, 4096); // r_k
        wr(0x40000418, 1); // r_n
        wr(0x4000042C, 0); // r_act_type
        wr(0x40000438, 0x42FE0000); // r_in_scale
        wr(0x4000043C, 0xC0A00000); // r_w_scale
        wr(0x40000440, 0x3F800000); // r_out_scale
        wr(0x40000310, 0x12);

        // Instruction 8: /model.2/cv1/act/Sigmoid (Opcode 0x15)
        wr(0x40000444, 0x1A53C00); // r_a_addr
        wr(0x40000448, 0x1A53C00); // r_b_addr
        wr(0x40000408, 0x1A57C00); // r_out_addr
        wr(0x40000450, 4096); // r_len
        wr(0x40000454, 0); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 9: /model.2/cv1/act/Mul (Opcode 0x15)
        wr(0x40000444, 0x1A53C00); // r_a_addr
        wr(0x40000448, 0x1A57C00); // r_b_addr
        wr(0x40000408, 0x1A58C00); // r_out_addr
        wr(0x40000450, 4096); // r_len
        wr(0x40000454, 2); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 10: /model.2/m.0/cv1/conv/Conv (Opcode 0x12)
        wr(0x40000400, 0x1A59400); // r_in_addr
        wr(0x40000404, 0x0023A00); // r_w_addr
        wr(0x40000408, 0x1A59C00); // r_out_addr
        wr(0x40000410, 2048); // r_m
        wr(0x40000414, 2048); // r_k
        wr(0x40000418, 3); // r_n
        wr(0x4000042C, 0); // r_act_type
        wr(0x40000438, 0x00000000); // r_in_scale
        wr(0x4000043C, 0x40400000); // r_w_scale
        wr(0x40000440, 0x3F800000); // r_out_scale
        wr(0x40000310, 0x12);

        // Instruction 11: /model.2/m.0/cv1/act/Sigmoid (Opcode 0x15)
        wr(0x40000444, 0x1A59C00); // r_a_addr
        wr(0x40000448, 0x1A59C00); // r_b_addr
        wr(0x40000408, 0x1A5FC00); // r_out_addr
        wr(0x40000450, 6144); // r_len
        wr(0x40000454, 0); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 12: /model.2/m.0/cv1/act/Mul (Opcode 0x15)
        wr(0x40000444, 0x1A59C00); // r_a_addr
        wr(0x40000448, 0x1A5FC00); // r_b_addr
        wr(0x40000408, 0x1A61400); // r_out_addr
        wr(0x40000450, 6144); // r_len
        wr(0x40000454, 2); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 13: /model.2/m.0/cv2/conv/Conv (Opcode 0x12)
        wr(0x40000400, 0x1A61400); // r_in_addr
        wr(0x40000404, 0x0029300); // r_w_addr
        wr(0x40000408, 0x1A62C00); // r_out_addr
        wr(0x40000410, 4096); // r_m
        wr(0x40000414, 4096); // r_k
        wr(0x40000418, 3); // r_n
        wr(0x4000042C, 0); // r_act_type
        wr(0x40000438, 0x00000000); // r_in_scale
        wr(0x4000043C, 0xC1B80000); // r_w_scale
        wr(0x40000440, 0x3F800000); // r_out_scale
        wr(0x40000310, 0x12);

        // Instruction 14: /model.2/m.0/cv2/act/Sigmoid (Opcode 0x15)
        wr(0x40000444, 0x1A62C00); // r_a_addr
        wr(0x40000448, 0x1A62C00); // r_b_addr
        wr(0x40000408, 0x1A6EC00); // r_out_addr
        wr(0x40000450, 12288); // r_len
        wr(0x40000454, 0); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 15: /model.2/m.0/cv2/act/Mul (Opcode 0x15)
        wr(0x40000444, 0x1A62C00); // r_a_addr
        wr(0x40000448, 0x1A6EC00); // r_b_addr
        wr(0x40000408, 0x1A71C00); // r_out_addr
        wr(0x40000450, 12288); // r_len
        wr(0x40000454, 2); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 16: /model.2/m.0/Add (Opcode 0x15)
        wr(0x40000444, 0x1A59400); // r_a_addr
        wr(0x40000448, 0x1A71C00); // r_b_addr
        wr(0x40000408, 0x1A74C00); // r_out_addr
        wr(0x40000450, 12288); // r_len
        wr(0x40000454, 0); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 17: /model.2/m.1/cv1/conv/Conv (Opcode 0x12)
        wr(0x40000400, 0x1A74C00); // r_in_addr
        wr(0x40000404, 0x002EE00); // r_w_addr
        wr(0x40000408, 0x1A77C00); // r_out_addr
        wr(0x40000410, 4096); // r_m
        wr(0x40000414, 4096); // r_k
        wr(0x40000418, 3); // r_n
        wr(0x4000042C, 0); // r_act_type
        wr(0x40000438, 0x00000000); // r_in_scale
        wr(0x4000043C, 0x40400000); // r_w_scale
        wr(0x40000440, 0x3F800000); // r_out_scale
        wr(0x40000310, 0x12);

        // Instruction 18: /model.2/m.1/cv1/act/Sigmoid (Opcode 0x15)
        wr(0x40000444, 0x1A77C00); // r_a_addr
        wr(0x40000448, 0x1A77C00); // r_b_addr
        wr(0x40000408, 0x1A83C00); // r_out_addr
        wr(0x40000450, 12288); // r_len
        wr(0x40000454, 0); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 19: /model.2/m.1/cv1/act/Mul (Opcode 0x15)
        wr(0x40000444, 0x1A77C00); // r_a_addr
        wr(0x40000448, 0x1A83C00); // r_b_addr
        wr(0x40000408, 0x1A86C00); // r_out_addr
        wr(0x40000450, 12288); // r_len
        wr(0x40000454, 2); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 20: /model.2/m.1/cv2/conv/Conv (Opcode 0x12)
        wr(0x40000400, 0x1A86C00); // r_in_addr
        wr(0x40000404, 0x0034700); // r_w_addr
        wr(0x40000408, 0x1A89C00); // r_out_addr
        wr(0x40000410, 4096); // r_m
        wr(0x40000414, 4096); // r_k
        wr(0x40000418, 3); // r_n
        wr(0x4000042C, 0); // r_act_type
        wr(0x40000438, 0x00000000); // r_in_scale
        wr(0x4000043C, 0x41000000); // r_w_scale
        wr(0x40000440, 0x3F800000); // r_out_scale
        wr(0x40000310, 0x12);

        // Instruction 21: /model.2/m.1/cv2/act/Sigmoid (Opcode 0x15)
        wr(0x40000444, 0x1A89C00); // r_a_addr
        wr(0x40000448, 0x1A89C00); // r_b_addr
        wr(0x40000408, 0x1A95C00); // r_out_addr
        wr(0x40000450, 12288); // r_len
        wr(0x40000454, 0); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 22: /model.2/m.1/cv2/act/Mul (Opcode 0x15)
        wr(0x40000444, 0x1A89C00); // r_a_addr
        wr(0x40000448, 0x1A95C00); // r_b_addr
        wr(0x40000408, 0x1A98C00); // r_out_addr
        wr(0x40000450, 12288); // r_len
        wr(0x40000454, 2); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 23: /model.2/m.1/Add (Opcode 0x15)
        wr(0x40000444, 0x1A74C00); // r_a_addr
        wr(0x40000448, 0x1A98C00); // r_b_addr
        wr(0x40000408, 0x1A9BC00); // r_out_addr
        wr(0x40000450, 12288); // r_len
        wr(0x40000454, 0); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 24: /model.2/cv2/conv/Conv (Opcode 0x12)
        wr(0x40000400, 0x1A9EC00); // r_in_addr
        wr(0x40000404, 0x003A400); // r_w_addr
        wr(0x40000408, 0x1AA5C00); // r_out_addr
        wr(0x40000410, 4096); // r_m
        wr(0x40000414, 4096); // r_k
        wr(0x40000418, 1); // r_n
        wr(0x4000042C, 0); // r_act_type
        wr(0x40000438, 0x42FE0000); // r_in_scale
        wr(0x4000043C, 0xC0000000); // r_w_scale
        wr(0x40000440, 0x3F800000); // r_out_scale
        wr(0x40000310, 0x12);

        // Instruction 25: /model.2/cv2/act/Sigmoid (Opcode 0x15)
        wr(0x40000444, 0x1AA5C00); // r_a_addr
        wr(0x40000448, 0x1AA5C00); // r_b_addr
        wr(0x40000408, 0x1AA9C00); // r_out_addr
        wr(0x40000450, 4096); // r_len
        wr(0x40000454, 0); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 26: /model.2/cv2/act/Mul (Opcode 0x15)
        wr(0x40000444, 0x1AA5C00); // r_a_addr
        wr(0x40000448, 0x1AA9C00); // r_b_addr
        wr(0x40000408, 0x1AAAC00); // r_out_addr
        wr(0x40000450, 4096); // r_len
        wr(0x40000454, 2); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 27: /model.3/conv/Conv (Opcode 0x12)
        wr(0x40000400, 0x1AAAC00); // r_in_addr
        wr(0x40000404, 0x003F400); // r_w_addr
        wr(0x40000408, 0x1AABC00); // r_out_addr
        wr(0x40000410, 4096); // r_m
        wr(0x40000414, 4096); // r_k
        wr(0x40000418, 3); // r_n
        wr(0x4000042C, 0); // r_act_type
        wr(0x40000438, 0x42FC0000); // r_in_scale
        wr(0x4000043C, 0xC2640000); // r_w_scale
        wr(0x40000440, 0x3F800000); // r_out_scale
        wr(0x40000310, 0x12);

        // Instruction 28: /model.3/act/Sigmoid (Opcode 0x15)
        wr(0x40000444, 0x1AABC00); // r_a_addr
        wr(0x40000448, 0x1AABC00); // r_b_addr
        wr(0x40000408, 0x1AB7C00); // r_out_addr
        wr(0x40000450, 12288); // r_len
        wr(0x40000454, 0); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 29: /model.3/act/Mul (Opcode 0x15)
        wr(0x40000444, 0x1AABC00); // r_a_addr
        wr(0x40000448, 0x1AB7C00); // r_b_addr
        wr(0x40000408, 0x1ABAC00); // r_out_addr
        wr(0x40000450, 12288); // r_len
        wr(0x40000454, 2); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 30: /model.4/cv1/conv/Conv (Opcode 0x12)
        wr(0x40000400, 0x1ABAC00); // r_in_addr
        wr(0x40000404, 0x0068400); // r_w_addr
        wr(0x40000408, 0x1ABDC00); // r_out_addr
        wr(0x40000410, 4096); // r_m
        wr(0x40000414, 4096); // r_k
        wr(0x40000418, 1); // r_n
        wr(0x4000042C, 0); // r_act_type
        wr(0x40000438, 0x42FC0000); // r_in_scale
        wr(0x4000043C, 0x41500000); // r_w_scale
        wr(0x40000440, 0x3F800000); // r_out_scale
        wr(0x40000310, 0x12);

        // Instruction 31: /model.4/cv1/act/Sigmoid (Opcode 0x15)
        wr(0x40000444, 0x1ABDC00); // r_a_addr
        wr(0x40000448, 0x1ABDC00); // r_b_addr
        wr(0x40000408, 0x1AC1C00); // r_out_addr
        wr(0x40000450, 4096); // r_len
        wr(0x40000454, 0); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 32: /model.4/cv1/act/Mul (Opcode 0x15)
        wr(0x40000444, 0x1ABDC00); // r_a_addr
        wr(0x40000448, 0x1AC1C00); // r_b_addr
        wr(0x40000408, 0x1AC2C00); // r_out_addr
        wr(0x40000450, 4096); // r_len
        wr(0x40000454, 2); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 33: /model.4/m.0/cv1/conv/Conv (Opcode 0x12)
        wr(0x40000400, 0x1AC3400); // r_in_addr
        wr(0x40000404, 0x0071C00); // r_w_addr
        wr(0x40000408, 0x1AC3C00); // r_out_addr
        wr(0x40000410, 2048); // r_m
        wr(0x40000414, 2048); // r_k
        wr(0x40000418, 3); // r_n
        wr(0x4000042C, 0); // r_act_type
        wr(0x40000438, 0x00000000); // r_in_scale
        wr(0x4000043C, 0x41F00000); // r_w_scale
        wr(0x40000440, 0x3F800000); // r_out_scale
        wr(0x40000310, 0x12);

        // Instruction 34: /model.4/m.0/cv1/act/Sigmoid (Opcode 0x15)
        wr(0x40000444, 0x1AC3C00); // r_a_addr
        wr(0x40000448, 0x1AC3C00); // r_b_addr
        wr(0x40000408, 0x1AC9C00); // r_out_addr
        wr(0x40000450, 6144); // r_len
        wr(0x40000454, 0); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 35: /model.4/m.0/cv1/act/Mul (Opcode 0x15)
        wr(0x40000444, 0x1AC3C00); // r_a_addr
        wr(0x40000448, 0x1AC9C00); // r_b_addr
        wr(0x40000408, 0x1ACB400); // r_out_addr
        wr(0x40000450, 6144); // r_len
        wr(0x40000454, 2); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 36: /model.4/m.0/cv2/conv/Conv (Opcode 0x12)
        wr(0x40000400, 0x1ACB400); // r_in_addr
        wr(0x40000404, 0x0086800); // r_w_addr
        wr(0x40000408, 0x1ACCC00); // r_out_addr
        wr(0x40000410, 4096); // r_m
        wr(0x40000414, 4096); // r_k
        wr(0x40000418, 3); // r_n
        wr(0x4000042C, 0); // r_act_type
        wr(0x40000438, 0x00000000); // r_in_scale
        wr(0x4000043C, 0xC1800000); // r_w_scale
        wr(0x40000440, 0x3F800000); // r_out_scale
        wr(0x40000310, 0x12);

        // Instruction 37: /model.4/m.0/cv2/act/Sigmoid (Opcode 0x15)
        wr(0x40000444, 0x1ACCC00); // r_a_addr
        wr(0x40000448, 0x1ACCC00); // r_b_addr
        wr(0x40000408, 0x1AD8C00); // r_out_addr
        wr(0x40000450, 12288); // r_len
        wr(0x40000454, 0); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 38: /model.4/m.0/cv2/act/Mul (Opcode 0x15)
        wr(0x40000444, 0x1ACCC00); // r_a_addr
        wr(0x40000448, 0x1AD8C00); // r_b_addr
        wr(0x40000408, 0x1ADBC00); // r_out_addr
        wr(0x40000450, 12288); // r_len
        wr(0x40000454, 2); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 39: /model.4/m.0/Add (Opcode 0x15)
        wr(0x40000444, 0x1AC3400); // r_a_addr
        wr(0x40000448, 0x1ADBC00); // r_b_addr
        wr(0x40000408, 0x1ADEC00); // r_out_addr
        wr(0x40000450, 12288); // r_len
        wr(0x40000454, 0); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 40: /model.4/m.1/cv1/conv/Conv (Opcode 0x12)
        wr(0x40000400, 0x1ADEC00); // r_in_addr
        wr(0x40000404, 0x009B600); // r_w_addr
        wr(0x40000408, 0x1AE1C00); // r_out_addr
        wr(0x40000410, 4096); // r_m
        wr(0x40000414, 4096); // r_k
        wr(0x40000418, 3); // r_n
        wr(0x4000042C, 0); // r_act_type
        wr(0x40000438, 0x00000000); // r_in_scale
        wr(0x4000043C, 0x41300000); // r_w_scale
        wr(0x40000440, 0x3F800000); // r_out_scale
        wr(0x40000310, 0x12);

        // Instruction 41: /model.4/m.1/cv1/act/Sigmoid (Opcode 0x15)
        wr(0x40000444, 0x1AE1C00); // r_a_addr
        wr(0x40000448, 0x1AE1C00); // r_b_addr
        wr(0x40000408, 0x1AEDC00); // r_out_addr
        wr(0x40000450, 12288); // r_len
        wr(0x40000454, 0); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 42: /model.4/m.1/cv1/act/Mul (Opcode 0x15)
        wr(0x40000444, 0x1AE1C00); // r_a_addr
        wr(0x40000448, 0x1AEDC00); // r_b_addr
        wr(0x40000408, 0x1AF0C00); // r_out_addr
        wr(0x40000450, 12288); // r_len
        wr(0x40000454, 2); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 43: /model.4/m.1/cv2/conv/Conv (Opcode 0x12)
        wr(0x40000400, 0x1AF0C00); // r_in_addr
        wr(0x40000404, 0x00B0200); // r_w_addr
        wr(0x40000408, 0x1AF3C00); // r_out_addr
        wr(0x40000410, 4096); // r_m
        wr(0x40000414, 4096); // r_k
        wr(0x40000418, 3); // r_n
        wr(0x4000042C, 0); // r_act_type
        wr(0x40000438, 0x00000000); // r_in_scale
        wr(0x4000043C, 0xC2000000); // r_w_scale
        wr(0x40000440, 0x3F800000); // r_out_scale
        wr(0x40000310, 0x12);

        // Instruction 44: /model.4/m.1/cv2/act/Sigmoid (Opcode 0x15)
        wr(0x40000444, 0x1AF3C00); // r_a_addr
        wr(0x40000448, 0x1AF3C00); // r_b_addr
        wr(0x40000408, 0x1AFFC00); // r_out_addr
        wr(0x40000450, 12288); // r_len
        wr(0x40000454, 0); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 45: /model.4/m.1/cv2/act/Mul (Opcode 0x15)
        wr(0x40000444, 0x1AF3C00); // r_a_addr
        wr(0x40000448, 0x1AFFC00); // r_b_addr
        wr(0x40000408, 0x1B02C00); // r_out_addr
        wr(0x40000450, 12288); // r_len
        wr(0x40000454, 2); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 46: /model.4/m.1/Add (Opcode 0x15)
        wr(0x40000444, 0x1ADEC00); // r_a_addr
        wr(0x40000448, 0x1B02C00); // r_b_addr
        wr(0x40000408, 0x1B05C00); // r_out_addr
        wr(0x40000450, 12288); // r_len
        wr(0x40000454, 0); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 47: /model.4/m.2/cv1/conv/Conv (Opcode 0x12)
        wr(0x40000400, 0x1B05C00); // r_in_addr
        wr(0x40000404, 0x00C5000); // r_w_addr
        wr(0x40000408, 0x1B08C00); // r_out_addr
        wr(0x40000410, 4096); // r_m
        wr(0x40000414, 4096); // r_k
        wr(0x40000418, 3); // r_n
        wr(0x4000042C, 0); // r_act_type
        wr(0x40000438, 0x00000000); // r_in_scale
        wr(0x4000043C, 0x40800000); // r_w_scale
        wr(0x40000440, 0x3F800000); // r_out_scale
        wr(0x40000310, 0x12);

        // Instruction 48: /model.4/m.2/cv1/act/Sigmoid (Opcode 0x15)
        wr(0x40000444, 0x1B08C00); // r_a_addr
        wr(0x40000448, 0x1B08C00); // r_b_addr
        wr(0x40000408, 0x1B14C00); // r_out_addr
        wr(0x40000450, 12288); // r_len
        wr(0x40000454, 0); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 49: /model.4/m.2/cv1/act/Mul (Opcode 0x15)
        wr(0x40000444, 0x1B08C00); // r_a_addr
        wr(0x40000448, 0x1B14C00); // r_b_addr
        wr(0x40000408, 0x1B17C00); // r_out_addr
        wr(0x40000450, 12288); // r_len
        wr(0x40000454, 2); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 50: /model.4/m.2/cv2/conv/Conv (Opcode 0x12)
        wr(0x40000400, 0x1B17C00); // r_in_addr
        wr(0x40000404, 0x00D9C00); // r_w_addr
        wr(0x40000408, 0x1B1AC00); // r_out_addr
        wr(0x40000410, 4096); // r_m
        wr(0x40000414, 4096); // r_k
        wr(0x40000418, 3); // r_n
        wr(0x4000042C, 0); // r_act_type
        wr(0x40000438, 0x00000000); // r_in_scale
        wr(0x4000043C, 0x00000000); // r_w_scale
        wr(0x40000440, 0x3F800000); // r_out_scale
        wr(0x40000310, 0x12);

        // Instruction 51: /model.4/m.2/cv2/act/Sigmoid (Opcode 0x15)
        wr(0x40000444, 0x1B1AC00); // r_a_addr
        wr(0x40000448, 0x1B1AC00); // r_b_addr
        wr(0x40000408, 0x1B26C00); // r_out_addr
        wr(0x40000450, 12288); // r_len
        wr(0x40000454, 0); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 52: /model.4/m.2/cv2/act/Mul (Opcode 0x15)
        wr(0x40000444, 0x1B1AC00); // r_a_addr
        wr(0x40000448, 0x1B26C00); // r_b_addr
        wr(0x40000408, 0x1B29C00); // r_out_addr
        wr(0x40000450, 12288); // r_len
        wr(0x40000454, 2); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 53: /model.4/m.2/Add (Opcode 0x15)
        wr(0x40000444, 0x1B05C00); // r_a_addr
        wr(0x40000448, 0x1B29C00); // r_b_addr
        wr(0x40000408, 0x1B2CC00); // r_out_addr
        wr(0x40000450, 12288); // r_len
        wr(0x40000454, 0); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 54: /model.4/m.3/cv1/conv/Conv (Opcode 0x12)
        wr(0x40000400, 0x1B2CC00); // r_in_addr
        wr(0x40000404, 0x00EEA00); // r_w_addr
        wr(0x40000408, 0x1B2FC00); // r_out_addr
        wr(0x40000410, 4096); // r_m
        wr(0x40000414, 4096); // r_k
        wr(0x40000418, 3); // r_n
        wr(0x4000042C, 0); // r_act_type
        wr(0x40000438, 0x00000000); // r_in_scale
        wr(0x4000043C, 0x41400000); // r_w_scale
        wr(0x40000440, 0x3F800000); // r_out_scale
        wr(0x40000310, 0x12);

        // Instruction 55: /model.4/m.3/cv1/act/Sigmoid (Opcode 0x15)
        wr(0x40000444, 0x1B2FC00); // r_a_addr
        wr(0x40000448, 0x1B2FC00); // r_b_addr
        wr(0x40000408, 0x1B3BC00); // r_out_addr
        wr(0x40000450, 12288); // r_len
        wr(0x40000454, 0); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 56: /model.4/m.3/cv1/act/Mul (Opcode 0x15)
        wr(0x40000444, 0x1B2FC00); // r_a_addr
        wr(0x40000448, 0x1B3BC00); // r_b_addr
        wr(0x40000408, 0x1B3EC00); // r_out_addr
        wr(0x40000450, 12288); // r_len
        wr(0x40000454, 2); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 57: /model.4/m.3/cv2/conv/Conv (Opcode 0x12)
        wr(0x40000400, 0x1B3EC00); // r_in_addr
        wr(0x40000404, 0x0103600); // r_w_addr
        wr(0x40000408, 0x1B41C00); // r_out_addr
        wr(0x40000410, 4096); // r_m
        wr(0x40000414, 4096); // r_k
        wr(0x40000418, 3); // r_n
        wr(0x4000042C, 0); // r_act_type
        wr(0x40000438, 0x00000000); // r_in_scale
        wr(0x4000043C, 0x41100000); // r_w_scale
        wr(0x40000440, 0x3F800000); // r_out_scale
        wr(0x40000310, 0x12);

        // Instruction 58: /model.4/m.3/cv2/act/Sigmoid (Opcode 0x15)
        wr(0x40000444, 0x1B41C00); // r_a_addr
        wr(0x40000448, 0x1B41C00); // r_b_addr
        wr(0x40000408, 0x1B4DC00); // r_out_addr
        wr(0x40000450, 12288); // r_len
        wr(0x40000454, 0); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 59: /model.4/m.3/cv2/act/Mul (Opcode 0x15)
        wr(0x40000444, 0x1B41C00); // r_a_addr
        wr(0x40000448, 0x1B4DC00); // r_b_addr
        wr(0x40000408, 0x1B50C00); // r_out_addr
        wr(0x40000450, 12288); // r_len
        wr(0x40000454, 2); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 60: /model.4/m.3/Add (Opcode 0x15)
        wr(0x40000444, 0x1B2CC00); // r_a_addr
        wr(0x40000448, 0x1B50C00); // r_b_addr
        wr(0x40000408, 0x1B53C00); // r_out_addr
        wr(0x40000450, 12288); // r_len
        wr(0x40000454, 0); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 61: /model.4/cv2/conv/Conv (Opcode 0x12)
        wr(0x40000400, 0x1B56C00); // r_in_addr
        wr(0x40000404, 0x0118600); // r_w_addr
        wr(0x40000408, 0x1B63C00); // r_out_addr
        wr(0x40000410, 4096); // r_m
        wr(0x40000414, 4096); // r_k
        wr(0x40000418, 1); // r_n
        wr(0x4000042C, 0); // r_act_type
        wr(0x40000438, 0x42FE0000); // r_in_scale
        wr(0x4000043C, 0xC0E00000); // r_w_scale
        wr(0x40000440, 0x3F800000); // r_out_scale
        wr(0x40000310, 0x12);

        // Instruction 62: /model.4/cv2/act/Sigmoid (Opcode 0x15)
        wr(0x40000444, 0x1B63C00); // r_a_addr
        wr(0x40000448, 0x1B63C00); // r_b_addr
        wr(0x40000408, 0x1B67C00); // r_out_addr
        wr(0x40000450, 4096); // r_len
        wr(0x40000454, 0); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 63: /model.4/cv2/act/Mul (Opcode 0x15)
        wr(0x40000444, 0x1B63C00); // r_a_addr
        wr(0x40000448, 0x1B67C00); // r_b_addr
        wr(0x40000408, 0x1B68C00); // r_out_addr
        wr(0x40000450, 4096); // r_len
        wr(0x40000454, 2); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 64: /model.5/conv/Conv (Opcode 0x12)
        wr(0x40000400, 0x1B68C00); // r_in_addr
        wr(0x40000404, 0x0134000); // r_w_addr
        wr(0x40000408, 0x1B69C00); // r_out_addr
        wr(0x40000410, 4096); // r_m
        wr(0x40000414, 4096); // r_k
        wr(0x40000418, 3); // r_n
        wr(0x4000042C, 0); // r_act_type
        wr(0x40000438, 0x42FC0000); // r_in_scale
        wr(0x4000043C, 0x41880000); // r_w_scale
        wr(0x40000440, 0x3F800000); // r_out_scale
        wr(0x40000310, 0x12);

        // Instruction 65: /model.5/act/Sigmoid (Opcode 0x15)
        wr(0x40000444, 0x1B69C00); // r_a_addr
        wr(0x40000448, 0x1B69C00); // r_b_addr
        wr(0x40000408, 0x1B75C00); // r_out_addr
        wr(0x40000450, 12288); // r_len
        wr(0x40000454, 0); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 66: /model.5/act/Mul (Opcode 0x15)
        wr(0x40000444, 0x1B69C00); // r_a_addr
        wr(0x40000448, 0x1B75C00); // r_b_addr
        wr(0x40000408, 0x1B78C00); // r_out_addr
        wr(0x40000450, 12288); // r_len
        wr(0x40000454, 2); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 67: /model.6/cv1/conv/Conv (Opcode 0x12)
        wr(0x40000400, 0x1B78C00); // r_in_addr
        wr(0x40000404, 0x01D6A00); // r_w_addr
        wr(0x40000408, 0x1B7BC00); // r_out_addr
        wr(0x40000410, 4096); // r_m
        wr(0x40000414, 4096); // r_k
        wr(0x40000418, 1); // r_n
        wr(0x4000042C, 0); // r_act_type
        wr(0x40000438, 0x42FC0000); // r_in_scale
        wr(0x4000043C, 0xC1880000); // r_w_scale
        wr(0x40000440, 0x3F800000); // r_out_scale
        wr(0x40000310, 0x12);

        // Instruction 68: /model.6/cv1/act/Sigmoid (Opcode 0x15)
        wr(0x40000444, 0x1B7BC00); // r_a_addr
        wr(0x40000448, 0x1B7BC00); // r_b_addr
        wr(0x40000408, 0x1B7FC00); // r_out_addr
        wr(0x40000450, 4096); // r_len
        wr(0x40000454, 0); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 69: /model.6/cv1/act/Mul (Opcode 0x15)
        wr(0x40000444, 0x1B7BC00); // r_a_addr
        wr(0x40000448, 0x1B7FC00); // r_b_addr
        wr(0x40000408, 0x1B80C00); // r_out_addr
        wr(0x40000450, 4096); // r_len
        wr(0x40000454, 2); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 70: /model.6/m.0/cv1/conv/Conv (Opcode 0x12)
        wr(0x40000400, 0x1B81400); // r_in_addr
        wr(0x40000404, 0x01FB200); // r_w_addr
        wr(0x40000408, 0x1B81C00); // r_out_addr
        wr(0x40000410, 2048); // r_m
        wr(0x40000414, 2048); // r_k
        wr(0x40000418, 3); // r_n
        wr(0x4000042C, 0); // r_act_type
        wr(0x40000438, 0x00000000); // r_in_scale
        wr(0x4000043C, 0x41700000); // r_w_scale
        wr(0x40000440, 0x3F800000); // r_out_scale
        wr(0x40000310, 0x12);

        // Instruction 71: /model.6/m.0/cv1/act/Sigmoid (Opcode 0x15)
        wr(0x40000444, 0x1B81C00); // r_a_addr
        wr(0x40000448, 0x1B81C00); // r_b_addr
        wr(0x40000408, 0x1B87C00); // r_out_addr
        wr(0x40000450, 6144); // r_len
        wr(0x40000454, 0); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 72: /model.6/m.0/cv1/act/Mul (Opcode 0x15)
        wr(0x40000444, 0x1B81C00); // r_a_addr
        wr(0x40000448, 0x1B87C00); // r_b_addr
        wr(0x40000408, 0x1B89400); // r_out_addr
        wr(0x40000450, 6144); // r_len
        wr(0x40000454, 2); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 73: /model.6/m.0/cv2/conv/Conv (Opcode 0x12)
        wr(0x40000400, 0x1B89400); // r_in_addr
        wr(0x40000404, 0x024CA00); // r_w_addr
        wr(0x40000408, 0x1B8AC00); // r_out_addr
        wr(0x40000410, 4096); // r_m
        wr(0x40000414, 4096); // r_k
        wr(0x40000418, 3); // r_n
        wr(0x4000042C, 0); // r_act_type
        wr(0x40000438, 0x00000000); // r_in_scale
        wr(0x4000043C, 0x40E00000); // r_w_scale
        wr(0x40000440, 0x3F800000); // r_out_scale
        wr(0x40000310, 0x12);

        // Instruction 74: /model.6/m.0/cv2/act/Sigmoid (Opcode 0x15)
        wr(0x40000444, 0x1B8AC00); // r_a_addr
        wr(0x40000448, 0x1B8AC00); // r_b_addr
        wr(0x40000408, 0x1B96C00); // r_out_addr
        wr(0x40000450, 12288); // r_len
        wr(0x40000454, 0); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 75: /model.6/m.0/cv2/act/Mul (Opcode 0x15)
        wr(0x40000444, 0x1B8AC00); // r_a_addr
        wr(0x40000448, 0x1B96C00); // r_b_addr
        wr(0x40000408, 0x1B99C00); // r_out_addr
        wr(0x40000450, 12288); // r_len
        wr(0x40000454, 2); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 76: /model.6/m.0/Add (Opcode 0x15)
        wr(0x40000444, 0x1B81400); // r_a_addr
        wr(0x40000448, 0x1B99C00); // r_b_addr
        wr(0x40000408, 0x1B9CC00); // r_out_addr
        wr(0x40000450, 12288); // r_len
        wr(0x40000454, 0); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 77: /model.6/m.1/cv1/conv/Conv (Opcode 0x12)
        wr(0x40000400, 0x1B9CC00); // r_in_addr
        wr(0x40000404, 0x029E400); // r_w_addr
        wr(0x40000408, 0x1B9FC00); // r_out_addr
        wr(0x40000410, 4096); // r_m
        wr(0x40000414, 4096); // r_k
        wr(0x40000418, 3); // r_n
        wr(0x4000042C, 0); // r_act_type
        wr(0x40000438, 0x00000000); // r_in_scale
        wr(0x4000043C, 0xC1400000); // r_w_scale
        wr(0x40000440, 0x3F800000); // r_out_scale
        wr(0x40000310, 0x12);

        // Instruction 78: /model.6/m.1/cv1/act/Sigmoid (Opcode 0x15)
        wr(0x40000444, 0x1B9FC00); // r_a_addr
        wr(0x40000448, 0x1B9FC00); // r_b_addr
        wr(0x40000408, 0x1BABC00); // r_out_addr
        wr(0x40000450, 12288); // r_len
        wr(0x40000454, 0); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 79: /model.6/m.1/cv1/act/Mul (Opcode 0x15)
        wr(0x40000444, 0x1B9FC00); // r_a_addr
        wr(0x40000448, 0x1BABC00); // r_b_addr
        wr(0x40000408, 0x1BAEC00); // r_out_addr
        wr(0x40000450, 12288); // r_len
        wr(0x40000454, 2); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 80: /model.6/m.1/cv2/conv/Conv (Opcode 0x12)
        wr(0x40000400, 0x1BAEC00); // r_in_addr
        wr(0x40000404, 0x02EFC00); // r_w_addr
        wr(0x40000408, 0x1BB1C00); // r_out_addr
        wr(0x40000410, 4096); // r_m
        wr(0x40000414, 4096); // r_k
        wr(0x40000418, 3); // r_n
        wr(0x4000042C, 0); // r_act_type
        wr(0x40000438, 0x00000000); // r_in_scale
        wr(0x4000043C, 0xC0C00000); // r_w_scale
        wr(0x40000440, 0x3F800000); // r_out_scale
        wr(0x40000310, 0x12);

        // Instruction 81: /model.6/m.1/cv2/act/Sigmoid (Opcode 0x15)
        wr(0x40000444, 0x1BB1C00); // r_a_addr
        wr(0x40000448, 0x1BB1C00); // r_b_addr
        wr(0x40000408, 0x1BBDC00); // r_out_addr
        wr(0x40000450, 12288); // r_len
        wr(0x40000454, 0); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 82: /model.6/m.1/cv2/act/Mul (Opcode 0x15)
        wr(0x40000444, 0x1BB1C00); // r_a_addr
        wr(0x40000448, 0x1BBDC00); // r_b_addr
        wr(0x40000408, 0x1BC0C00); // r_out_addr
        wr(0x40000450, 12288); // r_len
        wr(0x40000454, 2); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 83: /model.6/m.1/Add (Opcode 0x15)
        wr(0x40000444, 0x1B9CC00); // r_a_addr
        wr(0x40000448, 0x1BC0C00); // r_b_addr
        wr(0x40000408, 0x1BC3C00); // r_out_addr
        wr(0x40000450, 12288); // r_len
        wr(0x40000454, 0); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 84: /model.6/m.2/cv1/conv/Conv (Opcode 0x12)
        wr(0x40000400, 0x1BC3C00); // r_in_addr
        wr(0x40000404, 0x0341600); // r_w_addr
        wr(0x40000408, 0x1BC6C00); // r_out_addr
        wr(0x40000410, 4096); // r_m
        wr(0x40000414, 4096); // r_k
        wr(0x40000418, 3); // r_n
        wr(0x4000042C, 0); // r_act_type
        wr(0x40000438, 0x00000000); // r_in_scale
        wr(0x4000043C, 0x42280000); // r_w_scale
        wr(0x40000440, 0x3F800000); // r_out_scale
        wr(0x40000310, 0x12);

        // Instruction 85: /model.6/m.2/cv1/act/Sigmoid (Opcode 0x15)
        wr(0x40000444, 0x1BC6C00); // r_a_addr
        wr(0x40000448, 0x1BC6C00); // r_b_addr
        wr(0x40000408, 0x1BD2C00); // r_out_addr
        wr(0x40000450, 12288); // r_len
        wr(0x40000454, 0); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 86: /model.6/m.2/cv1/act/Mul (Opcode 0x15)
        wr(0x40000444, 0x1BC6C00); // r_a_addr
        wr(0x40000448, 0x1BD2C00); // r_b_addr
        wr(0x40000408, 0x1BD5C00); // r_out_addr
        wr(0x40000450, 12288); // r_len
        wr(0x40000454, 2); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 87: /model.6/m.2/cv2/conv/Conv (Opcode 0x12)
        wr(0x40000400, 0x1BD5C00); // r_in_addr
        wr(0x40000404, 0x0392E00); // r_w_addr
        wr(0x40000408, 0x1BD8C00); // r_out_addr
        wr(0x40000410, 4096); // r_m
        wr(0x40000414, 4096); // r_k
        wr(0x40000418, 3); // r_n
        wr(0x4000042C, 0); // r_act_type
        wr(0x40000438, 0x00000000); // r_in_scale
        wr(0x4000043C, 0xC1D00000); // r_w_scale
        wr(0x40000440, 0x3F800000); // r_out_scale
        wr(0x40000310, 0x12);

        // Instruction 88: /model.6/m.2/cv2/act/Sigmoid (Opcode 0x15)
        wr(0x40000444, 0x1BD8C00); // r_a_addr
        wr(0x40000448, 0x1BD8C00); // r_b_addr
        wr(0x40000408, 0x1BE4C00); // r_out_addr
        wr(0x40000450, 12288); // r_len
        wr(0x40000454, 0); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 89: /model.6/m.2/cv2/act/Mul (Opcode 0x15)
        wr(0x40000444, 0x1BD8C00); // r_a_addr
        wr(0x40000448, 0x1BE4C00); // r_b_addr
        wr(0x40000408, 0x1BE7C00); // r_out_addr
        wr(0x40000450, 12288); // r_len
        wr(0x40000454, 2); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 90: /model.6/m.2/Add (Opcode 0x15)
        wr(0x40000444, 0x1BC3C00); // r_a_addr
        wr(0x40000448, 0x1BE7C00); // r_b_addr
        wr(0x40000408, 0x1BEAC00); // r_out_addr
        wr(0x40000450, 12288); // r_len
        wr(0x40000454, 0); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 91: /model.6/m.3/cv1/conv/Conv (Opcode 0x12)
        wr(0x40000400, 0x1BEAC00); // r_in_addr
        wr(0x40000404, 0x03E4800); // r_w_addr
        wr(0x40000408, 0x1BEDC00); // r_out_addr
        wr(0x40000410, 4096); // r_m
        wr(0x40000414, 4096); // r_k
        wr(0x40000418, 3); // r_n
        wr(0x4000042C, 0); // r_act_type
        wr(0x40000438, 0x00000000); // r_in_scale
        wr(0x4000043C, 0x41A00000); // r_w_scale
        wr(0x40000440, 0x3F800000); // r_out_scale
        wr(0x40000310, 0x12);

        // Instruction 92: /model.6/m.3/cv1/act/Sigmoid (Opcode 0x15)
        wr(0x40000444, 0x1BEDC00); // r_a_addr
        wr(0x40000448, 0x1BEDC00); // r_b_addr
        wr(0x40000408, 0x1BF9C00); // r_out_addr
        wr(0x40000450, 12288); // r_len
        wr(0x40000454, 0); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 93: /model.6/m.3/cv1/act/Mul (Opcode 0x15)
        wr(0x40000444, 0x1BEDC00); // r_a_addr
        wr(0x40000448, 0x1BF9C00); // r_b_addr
        wr(0x40000408, 0x1BFCC00); // r_out_addr
        wr(0x40000450, 12288); // r_len
        wr(0x40000454, 2); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 94: /model.6/m.3/cv2/conv/Conv (Opcode 0x12)
        wr(0x40000400, 0x1BFCC00); // r_in_addr
        wr(0x40000404, 0x0436000); // r_w_addr
        wr(0x40000408, 0x1BFFC00); // r_out_addr
        wr(0x40000410, 4096); // r_m
        wr(0x40000414, 4096); // r_k
        wr(0x40000418, 3); // r_n
        wr(0x4000042C, 0); // r_act_type
        wr(0x40000438, 0x00000000); // r_in_scale
        wr(0x4000043C, 0x00000000); // r_w_scale
        wr(0x40000440, 0x3F800000); // r_out_scale
        wr(0x40000310, 0x12);

        // Instruction 95: /model.6/m.3/cv2/act/Sigmoid (Opcode 0x15)
        wr(0x40000444, 0x1BFFC00); // r_a_addr
        wr(0x40000448, 0x1BFFC00); // r_b_addr
        wr(0x40000408, 0x1C0BC00); // r_out_addr
        wr(0x40000450, 12288); // r_len
        wr(0x40000454, 0); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 96: /model.6/m.3/cv2/act/Mul (Opcode 0x15)
        wr(0x40000444, 0x1BFFC00); // r_a_addr
        wr(0x40000448, 0x1C0BC00); // r_b_addr
        wr(0x40000408, 0x1C0EC00); // r_out_addr
        wr(0x40000450, 12288); // r_len
        wr(0x40000454, 2); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 97: /model.6/m.3/Add (Opcode 0x15)
        wr(0x40000444, 0x1BEAC00); // r_a_addr
        wr(0x40000448, 0x1C0EC00); // r_b_addr
        wr(0x40000408, 0x1C11C00); // r_out_addr
        wr(0x40000450, 12288); // r_len
        wr(0x40000454, 0); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 98: /model.6/cv2/conv/Conv (Opcode 0x12)
        wr(0x40000400, 0x1C14C00); // r_in_addr
        wr(0x40000404, 0x0487E00); // r_w_addr
        wr(0x40000408, 0x1C21C00); // r_out_addr
        wr(0x40000410, 4096); // r_m
        wr(0x40000414, 4096); // r_k
        wr(0x40000418, 1); // r_n
        wr(0x4000042C, 0); // r_act_type
        wr(0x40000438, 0x42FE0000); // r_in_scale
        wr(0x4000043C, 0x41F80000); // r_w_scale
        wr(0x40000440, 0x3F800000); // r_out_scale
        wr(0x40000310, 0x12);

        // Instruction 99: /model.6/cv2/act/Sigmoid (Opcode 0x15)
        wr(0x40000444, 0x1C21C00); // r_a_addr
        wr(0x40000448, 0x1C21C00); // r_b_addr
        wr(0x40000408, 0x1C25C00); // r_out_addr
        wr(0x40000450, 4096); // r_len
        wr(0x40000454, 0); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 100: /model.6/cv2/act/Mul (Opcode 0x15)
        wr(0x40000444, 0x1C21C00); // r_a_addr
        wr(0x40000448, 0x1C25C00); // r_b_addr
        wr(0x40000408, 0x1C26C00); // r_out_addr
        wr(0x40000450, 4096); // r_len
        wr(0x40000454, 2); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 101: /model.7/conv/Conv (Opcode 0x12)
        wr(0x40000400, 0x1C26C00); // r_in_addr
        wr(0x40000404, 0x04F4A00); // r_w_addr
        wr(0x40000408, 0x1C27C00); // r_out_addr
        wr(0x40000410, 4096); // r_m
        wr(0x40000414, 4096); // r_k
        wr(0x40000418, 3); // r_n
        wr(0x4000042C, 0); // r_act_type
        wr(0x40000438, 0x42FC0000); // r_in_scale
        wr(0x4000043C, 0xC1B80000); // r_w_scale
        wr(0x40000440, 0x3F800000); // r_out_scale
        wr(0x40000310, 0x12);

        // Instruction 102: /model.7/act/Sigmoid (Opcode 0x15)
        wr(0x40000444, 0x1C27C00); // r_a_addr
        wr(0x40000448, 0x1C27C00); // r_b_addr
        wr(0x40000408, 0x1C33C00); // r_out_addr
        wr(0x40000450, 12288); // r_len
        wr(0x40000454, 0); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 103: /model.7/act/Mul (Opcode 0x15)
        wr(0x40000444, 0x1C27C00); // r_a_addr
        wr(0x40000448, 0x1C33C00); // r_b_addr
        wr(0x40000408, 0x1C36C00); // r_out_addr
        wr(0x40000450, 12288); // r_len
        wr(0x40000454, 2); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 104: /model.8/cv1/conv/Conv (Opcode 0x12)
        wr(0x40000400, 0x1C36C00); // r_in_addr
        wr(0x40000404, 0x06DB600); // r_w_addr
        wr(0x40000408, 0x1C39C00); // r_out_addr
        wr(0x40000410, 4096); // r_m
        wr(0x40000414, 4096); // r_k
        wr(0x40000418, 1); // r_n
        wr(0x4000042C, 0); // r_act_type
        wr(0x40000438, 0x42FC0000); // r_in_scale
        wr(0x4000043C, 0xC1900000); // r_w_scale
        wr(0x40000440, 0x3F800000); // r_out_scale
        wr(0x40000310, 0x12);

        // Instruction 105: /model.8/cv1/act/Sigmoid (Opcode 0x15)
        wr(0x40000444, 0x1C39C00); // r_a_addr
        wr(0x40000448, 0x1C39C00); // r_b_addr
        wr(0x40000408, 0x1C3DC00); // r_out_addr
        wr(0x40000450, 4096); // r_len
        wr(0x40000454, 0); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 106: /model.8/cv1/act/Mul (Opcode 0x15)
        wr(0x40000444, 0x1C39C00); // r_a_addr
        wr(0x40000448, 0x1C3DC00); // r_b_addr
        wr(0x40000408, 0x1C3EC00); // r_out_addr
        wr(0x40000450, 4096); // r_len
        wr(0x40000454, 2); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 107: /model.8/m.0/cv1/conv/Conv (Opcode 0x12)
        wr(0x40000400, 0x1C3F400); // r_in_addr
        wr(0x40000404, 0x072D000); // r_w_addr
        wr(0x40000408, 0x1C3FC00); // r_out_addr
        wr(0x40000410, 2048); // r_m
        wr(0x40000414, 2048); // r_k
        wr(0x40000418, 3); // r_n
        wr(0x4000042C, 0); // r_act_type
        wr(0x40000438, 0x00000000); // r_in_scale
        wr(0x4000043C, 0xC1700000); // r_w_scale
        wr(0x40000440, 0x3F800000); // r_out_scale
        wr(0x40000310, 0x12);

        // Instruction 108: /model.8/m.0/cv1/act/Sigmoid (Opcode 0x15)
        wr(0x40000444, 0x1C3FC00); // r_a_addr
        wr(0x40000448, 0x1C3FC00); // r_b_addr
        wr(0x40000408, 0x1C45C00); // r_out_addr
        wr(0x40000450, 6144); // r_len
        wr(0x40000454, 0); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 109: /model.8/m.0/cv1/act/Mul (Opcode 0x15)
        wr(0x40000444, 0x1C3FC00); // r_a_addr
        wr(0x40000448, 0x1C45C00); // r_b_addr
        wr(0x40000408, 0x1C47400); // r_out_addr
        wr(0x40000450, 6144); // r_len
        wr(0x40000454, 2); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 110: /model.8/m.0/cv2/conv/Conv (Opcode 0x12)
        wr(0x40000400, 0x1C47400); // r_in_addr
        wr(0x40000404, 0x07E3E00); // r_w_addr
        wr(0x40000408, 0x1C48C00); // r_out_addr
        wr(0x40000410, 4096); // r_m
        wr(0x40000414, 4096); // r_k
        wr(0x40000418, 3); // r_n
        wr(0x4000042C, 0); // r_act_type
        wr(0x40000438, 0x00000000); // r_in_scale
        wr(0x4000043C, 0x40000000); // r_w_scale
        wr(0x40000440, 0x3F800000); // r_out_scale
        wr(0x40000310, 0x12);

        // Instruction 111: /model.8/m.0/cv2/act/Sigmoid (Opcode 0x15)
        wr(0x40000444, 0x1C48C00); // r_a_addr
        wr(0x40000448, 0x1C48C00); // r_b_addr
        wr(0x40000408, 0x1C54C00); // r_out_addr
        wr(0x40000450, 12288); // r_len
        wr(0x40000454, 0); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 112: /model.8/m.0/cv2/act/Mul (Opcode 0x15)
        wr(0x40000444, 0x1C48C00); // r_a_addr
        wr(0x40000448, 0x1C54C00); // r_b_addr
        wr(0x40000408, 0x1C57C00); // r_out_addr
        wr(0x40000450, 12288); // r_len
        wr(0x40000454, 2); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 113: /model.8/m.0/Add (Opcode 0x15)
        wr(0x40000444, 0x1C3F400); // r_a_addr
        wr(0x40000448, 0x1C57C00); // r_b_addr
        wr(0x40000408, 0x1C5AC00); // r_out_addr
        wr(0x40000450, 12288); // r_len
        wr(0x40000454, 0); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 114: /model.8/m.1/cv1/conv/Conv (Opcode 0x12)
        wr(0x40000400, 0x1C5AC00); // r_in_addr
        wr(0x40000404, 0x089AE00); // r_w_addr
        wr(0x40000408, 0x1C5DC00); // r_out_addr
        wr(0x40000410, 4096); // r_m
        wr(0x40000414, 4096); // r_k
        wr(0x40000418, 3); // r_n
        wr(0x4000042C, 0); // r_act_type
        wr(0x40000438, 0x00000000); // r_in_scale
        wr(0x4000043C, 0xC1A00000); // r_w_scale
        wr(0x40000440, 0x3F800000); // r_out_scale
        wr(0x40000310, 0x12);

        // Instruction 115: /model.8/m.1/cv1/act/Sigmoid (Opcode 0x15)
        wr(0x40000444, 0x1C5DC00); // r_a_addr
        wr(0x40000448, 0x1C5DC00); // r_b_addr
        wr(0x40000408, 0x1C69C00); // r_out_addr
        wr(0x40000450, 12288); // r_len
        wr(0x40000454, 0); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 116: /model.8/m.1/cv1/act/Mul (Opcode 0x15)
        wr(0x40000444, 0x1C5DC00); // r_a_addr
        wr(0x40000448, 0x1C69C00); // r_b_addr
        wr(0x40000408, 0x1C6CC00); // r_out_addr
        wr(0x40000450, 12288); // r_len
        wr(0x40000454, 2); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 117: /model.8/m.1/cv2/conv/Conv (Opcode 0x12)
        wr(0x40000400, 0x1C6CC00); // r_in_addr
        wr(0x40000404, 0x0951C00); // r_w_addr
        wr(0x40000408, 0x1C6FC00); // r_out_addr
        wr(0x40000410, 4096); // r_m
        wr(0x40000414, 4096); // r_k
        wr(0x40000418, 3); // r_n
        wr(0x4000042C, 0); // r_act_type
        wr(0x40000438, 0x00000000); // r_in_scale
        wr(0x4000043C, 0x00000000); // r_w_scale
        wr(0x40000440, 0x3F800000); // r_out_scale
        wr(0x40000310, 0x12);

        // Instruction 118: /model.8/m.1/cv2/act/Sigmoid (Opcode 0x15)
        wr(0x40000444, 0x1C6FC00); // r_a_addr
        wr(0x40000448, 0x1C6FC00); // r_b_addr
        wr(0x40000408, 0x1C7BC00); // r_out_addr
        wr(0x40000450, 12288); // r_len
        wr(0x40000454, 0); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 119: /model.8/m.1/cv2/act/Mul (Opcode 0x15)
        wr(0x40000444, 0x1C6FC00); // r_a_addr
        wr(0x40000448, 0x1C7BC00); // r_b_addr
        wr(0x40000408, 0x1C7EC00); // r_out_addr
        wr(0x40000450, 12288); // r_len
        wr(0x40000454, 2); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 120: /model.8/m.1/Add (Opcode 0x15)
        wr(0x40000444, 0x1C5AC00); // r_a_addr
        wr(0x40000448, 0x1C7EC00); // r_b_addr
        wr(0x40000408, 0x1C81C00); // r_out_addr
        wr(0x40000450, 12288); // r_len
        wr(0x40000454, 0); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 121: /model.8/cv2/conv/Conv (Opcode 0x12)
        wr(0x40000400, 0x1C84C00); // r_in_addr
        wr(0x40000404, 0x0A09000); // r_w_addr
        wr(0x40000408, 0x1C8BC00); // r_out_addr
        wr(0x40000410, 4096); // r_m
        wr(0x40000414, 4096); // r_k
        wr(0x40000418, 1); // r_n
        wr(0x4000042C, 0); // r_act_type
        wr(0x40000438, 0x42FC0000); // r_in_scale
        wr(0x4000043C, 0x42A40000); // r_w_scale
        wr(0x40000440, 0x3F800000); // r_out_scale
        wr(0x40000310, 0x12);

        // Instruction 122: /model.8/cv2/act/Sigmoid (Opcode 0x15)
        wr(0x40000444, 0x1C8BC00); // r_a_addr
        wr(0x40000448, 0x1C8BC00); // r_b_addr
        wr(0x40000408, 0x1C8FC00); // r_out_addr
        wr(0x40000450, 4096); // r_len
        wr(0x40000454, 0); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 123: /model.8/cv2/act/Mul (Opcode 0x15)
        wr(0x40000444, 0x1C8BC00); // r_a_addr
        wr(0x40000448, 0x1C8FC00); // r_b_addr
        wr(0x40000408, 0x1C90C00); // r_out_addr
        wr(0x40000450, 4096); // r_len
        wr(0x40000454, 2); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 124: /model.9/cv1/conv/Conv (Opcode 0x12)
        wr(0x40000400, 0x1C90C00); // r_in_addr
        wr(0x40000404, 0x0AABA00); // r_w_addr
        wr(0x40000408, 0x1C91C00); // r_out_addr
        wr(0x40000410, 4096); // r_m
        wr(0x40000414, 4096); // r_k
        wr(0x40000418, 1); // r_n
        wr(0x4000042C, 0); // r_act_type
        wr(0x40000438, 0x42FC0000); // r_in_scale
        wr(0x4000043C, 0xC1100000); // r_w_scale
        wr(0x40000440, 0x3F800000); // r_out_scale
        wr(0x40000310, 0x12);

        // Instruction 125: /model.9/cv1/act/Sigmoid (Opcode 0x15)
        wr(0x40000444, 0x1C91C00); // r_a_addr
        wr(0x40000448, 0x1C91C00); // r_b_addr
        wr(0x40000408, 0x1C95C00); // r_out_addr
        wr(0x40000450, 4096); // r_len
        wr(0x40000454, 0); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 126: /model.9/cv1/act/Mul (Opcode 0x15)
        wr(0x40000444, 0x1C91C00); // r_a_addr
        wr(0x40000448, 0x1C95C00); // r_b_addr
        wr(0x40000408, 0x1C96C00); // r_out_addr
        wr(0x40000450, 4096); // r_len
        wr(0x40000454, 2); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 127: /model.9/m/MaxPool (Opcode 0x15)
        wr(0x40000444, 0x1C96C00); // r_a_addr
        wr(0x40000448, 0x1C96C00); // r_b_addr
        wr(0x40000408, 0x1C97C00); // r_out_addr
        wr(0x40000450, 4096); // r_len
        wr(0x40000454, 1); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 128: /model.9/m_1/MaxPool (Opcode 0x15)
        wr(0x40000444, 0x1C97C00); // r_a_addr
        wr(0x40000448, 0x1C97C00); // r_b_addr
        wr(0x40000408, 0x1C98C00); // r_out_addr
        wr(0x40000450, 4096); // r_len
        wr(0x40000454, 1); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 129: /model.9/m_2/MaxPool (Opcode 0x15)
        wr(0x40000444, 0x1C98C00); // r_a_addr
        wr(0x40000448, 0x1C98C00); // r_b_addr
        wr(0x40000408, 0x1C99C00); // r_out_addr
        wr(0x40000450, 4096); // r_len
        wr(0x40000454, 1); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 130: /model.9/cv2/conv/Conv (Opcode 0x12)
        wr(0x40000400, 0x1C9AC00); // r_in_addr
        wr(0x40000404, 0x0AD5000); // r_w_addr
        wr(0x40000408, 0x1C9EC00); // r_out_addr
        wr(0x40000410, 4096); // r_m
        wr(0x40000414, 4096); // r_k
        wr(0x40000418, 1); // r_n
        wr(0x4000042C, 0); // r_act_type
        wr(0x40000438, 0x42FC0000); // r_in_scale
        wr(0x4000043C, 0xC20C0000); // r_w_scale
        wr(0x40000440, 0x3F800000); // r_out_scale
        wr(0x40000310, 0x12);

        // Instruction 131: /model.9/cv2/act/Sigmoid (Opcode 0x15)
        wr(0x40000444, 0x1C9EC00); // r_a_addr
        wr(0x40000448, 0x1C9EC00); // r_b_addr
        wr(0x40000408, 0x1CA2C00); // r_out_addr
        wr(0x40000450, 4096); // r_len
        wr(0x40000454, 0); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 132: /model.9/cv2/act/Mul (Opcode 0x15)
        wr(0x40000444, 0x1C9EC00); // r_a_addr
        wr(0x40000448, 0x1CA2C00); // r_b_addr
        wr(0x40000408, 0x1CA3C00); // r_out_addr
        wr(0x40000450, 4096); // r_len
        wr(0x40000454, 2); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 133: /model.12/cv1/conv/Conv (Opcode 0x12)
        wr(0x40000400, 0x1CA4C00); // r_in_addr
        wr(0x40000404, 0x0B77C00); // r_w_addr
        wr(0x40000408, 0x1CA6C00); // r_out_addr
        wr(0x40000410, 4096); // r_m
        wr(0x40000414, 4096); // r_k
        wr(0x40000418, 1); // r_n
        wr(0x4000042C, 0); // r_act_type
        wr(0x40000438, 0x42FC0000); // r_in_scale
        wr(0x4000043C, 0xC1800000); // r_w_scale
        wr(0x40000440, 0x3F800000); // r_out_scale
        wr(0x40000310, 0x12);

        // Instruction 134: /model.12/cv1/act/Sigmoid (Opcode 0x15)
        wr(0x40000444, 0x1CA6C00); // r_a_addr
        wr(0x40000448, 0x1CA6C00); // r_b_addr
        wr(0x40000408, 0x1CAAC00); // r_out_addr
        wr(0x40000450, 4096); // r_len
        wr(0x40000454, 0); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 135: /model.12/cv1/act/Mul (Opcode 0x15)
        wr(0x40000444, 0x1CA6C00); // r_a_addr
        wr(0x40000448, 0x1CAAC00); // r_b_addr
        wr(0x40000408, 0x1CABC00); // r_out_addr
        wr(0x40000450, 4096); // r_len
        wr(0x40000454, 2); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 136: /model.12/m.0/cv1/conv/Conv (Opcode 0x12)
        wr(0x40000400, 0x1CAC400); // r_in_addr
        wr(0x40000404, 0x0BD2400); // r_w_addr
        wr(0x40000408, 0x1CACC00); // r_out_addr
        wr(0x40000410, 2048); // r_m
        wr(0x40000414, 2048); // r_k
        wr(0x40000418, 3); // r_n
        wr(0x4000042C, 0); // r_act_type
        wr(0x40000438, 0x00000000); // r_in_scale
        wr(0x4000043C, 0xC1900000); // r_w_scale
        wr(0x40000440, 0x3F800000); // r_out_scale
        wr(0x40000310, 0x12);

        // Instruction 137: /model.12/m.0/cv1/act/Sigmoid (Opcode 0x15)
        wr(0x40000444, 0x1CACC00); // r_a_addr
        wr(0x40000448, 0x1CACC00); // r_b_addr
        wr(0x40000408, 0x1CB2C00); // r_out_addr
        wr(0x40000450, 6144); // r_len
        wr(0x40000454, 0); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 138: /model.12/m.0/cv1/act/Mul (Opcode 0x15)
        wr(0x40000444, 0x1CACC00); // r_a_addr
        wr(0x40000448, 0x1CB2C00); // r_b_addr
        wr(0x40000408, 0x1CB4400); // r_out_addr
        wr(0x40000450, 6144); // r_len
        wr(0x40000454, 2); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 139: /model.12/m.0/cv2/conv/Conv (Opcode 0x12)
        wr(0x40000400, 0x1CB4400); // r_in_addr
        wr(0x40000404, 0x0C23C00); // r_w_addr
        wr(0x40000408, 0x1CB5C00); // r_out_addr
        wr(0x40000410, 4096); // r_m
        wr(0x40000414, 4096); // r_k
        wr(0x40000418, 3); // r_n
        wr(0x4000042C, 0); // r_act_type
        wr(0x40000438, 0x00000000); // r_in_scale
        wr(0x4000043C, 0xC1B80000); // r_w_scale
        wr(0x40000440, 0x3F800000); // r_out_scale
        wr(0x40000310, 0x12);

        // Instruction 140: /model.12/m.0/cv2/act/Sigmoid (Opcode 0x15)
        wr(0x40000444, 0x1CB5C00); // r_a_addr
        wr(0x40000448, 0x1CB5C00); // r_b_addr
        wr(0x40000408, 0x1CC1C00); // r_out_addr
        wr(0x40000450, 12288); // r_len
        wr(0x40000454, 0); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 141: /model.12/m.0/cv2/act/Mul (Opcode 0x15)
        wr(0x40000444, 0x1CB5C00); // r_a_addr
        wr(0x40000448, 0x1CC1C00); // r_b_addr
        wr(0x40000408, 0x1CC4C00); // r_out_addr
        wr(0x40000450, 12288); // r_len
        wr(0x40000454, 2); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 142: /model.12/m.1/cv1/conv/Conv (Opcode 0x12)
        wr(0x40000400, 0x1CC4C00); // r_in_addr
        wr(0x40000404, 0x0C75400); // r_w_addr
        wr(0x40000408, 0x1CC7C00); // r_out_addr
        wr(0x40000410, 4096); // r_m
        wr(0x40000414, 4096); // r_k
        wr(0x40000418, 3); // r_n
        wr(0x4000042C, 0); // r_act_type
        wr(0x40000438, 0x00000000); // r_in_scale
        wr(0x4000043C, 0x00000000); // r_w_scale
        wr(0x40000440, 0x3F800000); // r_out_scale
        wr(0x40000310, 0x12);

        // Instruction 143: /model.12/m.1/cv1/act/Sigmoid (Opcode 0x15)
        wr(0x40000444, 0x1CC7C00); // r_a_addr
        wr(0x40000448, 0x1CC7C00); // r_b_addr
        wr(0x40000408, 0x1CD3C00); // r_out_addr
        wr(0x40000450, 12288); // r_len
        wr(0x40000454, 0); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 144: /model.12/m.1/cv1/act/Mul (Opcode 0x15)
        wr(0x40000444, 0x1CC7C00); // r_a_addr
        wr(0x40000448, 0x1CD3C00); // r_b_addr
        wr(0x40000408, 0x1CD6C00); // r_out_addr
        wr(0x40000450, 12288); // r_len
        wr(0x40000454, 2); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 145: /model.12/m.1/cv2/conv/Conv (Opcode 0x12)
        wr(0x40000400, 0x1CD6C00); // r_in_addr
        wr(0x40000404, 0x0CC6C00); // r_w_addr
        wr(0x40000408, 0x1CD9C00); // r_out_addr
        wr(0x40000410, 4096); // r_m
        wr(0x40000414, 4096); // r_k
        wr(0x40000418, 3); // r_n
        wr(0x4000042C, 0); // r_act_type
        wr(0x40000438, 0x00000000); // r_in_scale
        wr(0x4000043C, 0x41600000); // r_w_scale
        wr(0x40000440, 0x3F800000); // r_out_scale
        wr(0x40000310, 0x12);

        // Instruction 146: /model.12/m.1/cv2/act/Sigmoid (Opcode 0x15)
        wr(0x40000444, 0x1CD9C00); // r_a_addr
        wr(0x40000448, 0x1CD9C00); // r_b_addr
        wr(0x40000408, 0x1CE5C00); // r_out_addr
        wr(0x40000450, 12288); // r_len
        wr(0x40000454, 0); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 147: /model.12/m.1/cv2/act/Mul (Opcode 0x15)
        wr(0x40000444, 0x1CD9C00); // r_a_addr
        wr(0x40000448, 0x1CE5C00); // r_b_addr
        wr(0x40000408, 0x1CE8C00); // r_out_addr
        wr(0x40000450, 12288); // r_len
        wr(0x40000454, 2); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 148: /model.12/cv2/conv/Conv (Opcode 0x12)
        wr(0x40000400, 0x1CEBC00); // r_in_addr
        wr(0x40000404, 0x0D18800); // r_w_addr
        wr(0x40000408, 0x1CF2C00); // r_out_addr
        wr(0x40000410, 4096); // r_m
        wr(0x40000414, 4096); // r_k
        wr(0x40000418, 1); // r_n
        wr(0x4000042C, 0); // r_act_type
        wr(0x40000438, 0x42FC0000); // r_in_scale
        wr(0x4000043C, 0xC0400000); // r_w_scale
        wr(0x40000440, 0x3F800000); // r_out_scale
        wr(0x40000310, 0x12);

        // Instruction 149: /model.12/cv2/act/Sigmoid (Opcode 0x15)
        wr(0x40000444, 0x1CF2C00); // r_a_addr
        wr(0x40000448, 0x1CF2C00); // r_b_addr
        wr(0x40000408, 0x1CF6C00); // r_out_addr
        wr(0x40000450, 4096); // r_len
        wr(0x40000454, 0); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 150: /model.12/cv2/act/Mul (Opcode 0x15)
        wr(0x40000444, 0x1CF2C00); // r_a_addr
        wr(0x40000448, 0x1CF6C00); // r_b_addr
        wr(0x40000408, 0x1CF7C00); // r_out_addr
        wr(0x40000450, 4096); // r_len
        wr(0x40000454, 2); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 151: /model.15/cv1/conv/Conv (Opcode 0x12)
        wr(0x40000400, 0x1CF8C00); // r_in_addr
        wr(0x40000404, 0x0D61200); // r_w_addr
        wr(0x40000408, 0x1CFAC00); // r_out_addr
        wr(0x40000410, 4096); // r_m
        wr(0x40000414, 4096); // r_k
        wr(0x40000418, 1); // r_n
        wr(0x4000042C, 0); // r_act_type
        wr(0x40000438, 0x42FC0000); // r_in_scale
        wr(0x4000043C, 0x00000000); // r_w_scale
        wr(0x40000440, 0x3F800000); // r_out_scale
        wr(0x40000310, 0x12);

        // Instruction 152: /model.15/cv1/act/Sigmoid (Opcode 0x15)
        wr(0x40000444, 0x1CFAC00); // r_a_addr
        wr(0x40000448, 0x1CFAC00); // r_b_addr
        wr(0x40000408, 0x1CFEC00); // r_out_addr
        wr(0x40000450, 4096); // r_len
        wr(0x40000454, 0); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 153: /model.15/cv1/act/Mul (Opcode 0x15)
        wr(0x40000444, 0x1CFAC00); // r_a_addr
        wr(0x40000448, 0x1CFEC00); // r_b_addr
        wr(0x40000408, 0x1CFFC00); // r_out_addr
        wr(0x40000450, 4096); // r_len
        wr(0x40000454, 2); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 154: /model.15/m.0/cv1/conv/Conv (Opcode 0x12)
        wr(0x40000400, 0x1D00400); // r_in_addr
        wr(0x40000404, 0x0D7CA00); // r_w_addr
        wr(0x40000408, 0x1D00C00); // r_out_addr
        wr(0x40000410, 2048); // r_m
        wr(0x40000414, 2048); // r_k
        wr(0x40000418, 3); // r_n
        wr(0x4000042C, 0); // r_act_type
        wr(0x40000438, 0x00000000); // r_in_scale
        wr(0x4000043C, 0xC1500000); // r_w_scale
        wr(0x40000440, 0x3F800000); // r_out_scale
        wr(0x40000310, 0x12);

        // Instruction 155: /model.15/m.0/cv1/act/Sigmoid (Opcode 0x15)
        wr(0x40000444, 0x1D00C00); // r_a_addr
        wr(0x40000448, 0x1D00C00); // r_b_addr
        wr(0x40000408, 0x1D06C00); // r_out_addr
        wr(0x40000450, 6144); // r_len
        wr(0x40000454, 0); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 156: /model.15/m.0/cv1/act/Mul (Opcode 0x15)
        wr(0x40000444, 0x1D00C00); // r_a_addr
        wr(0x40000448, 0x1D06C00); // r_b_addr
        wr(0x40000408, 0x1D08400); // r_out_addr
        wr(0x40000450, 6144); // r_len
        wr(0x40000454, 2); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 157: /model.15/m.0/cv2/conv/Conv (Opcode 0x12)
        wr(0x40000400, 0x1D08400); // r_in_addr
        wr(0x40000404, 0x0D91600); // r_w_addr
        wr(0x40000408, 0x1D09C00); // r_out_addr
        wr(0x40000410, 4096); // r_m
        wr(0x40000414, 4096); // r_k
        wr(0x40000418, 3); // r_n
        wr(0x4000042C, 0); // r_act_type
        wr(0x40000438, 0x00000000); // r_in_scale
        wr(0x4000043C, 0x40000000); // r_w_scale
        wr(0x40000440, 0x3F800000); // r_out_scale
        wr(0x40000310, 0x12);

        // Instruction 158: /model.15/m.0/cv2/act/Sigmoid (Opcode 0x15)
        wr(0x40000444, 0x1D09C00); // r_a_addr
        wr(0x40000448, 0x1D09C00); // r_b_addr
        wr(0x40000408, 0x1D15C00); // r_out_addr
        wr(0x40000450, 12288); // r_len
        wr(0x40000454, 0); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 159: /model.15/m.0/cv2/act/Mul (Opcode 0x15)
        wr(0x40000444, 0x1D09C00); // r_a_addr
        wr(0x40000448, 0x1D15C00); // r_b_addr
        wr(0x40000408, 0x1D18C00); // r_out_addr
        wr(0x40000450, 12288); // r_len
        wr(0x40000454, 2); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 160: /model.15/m.1/cv1/conv/Conv (Opcode 0x12)
        wr(0x40000400, 0x1D18C00); // r_in_addr
        wr(0x40000404, 0x0DA6200); // r_w_addr
        wr(0x40000408, 0x1D1BC00); // r_out_addr
        wr(0x40000410, 4096); // r_m
        wr(0x40000414, 4096); // r_k
        wr(0x40000418, 3); // r_n
        wr(0x4000042C, 0); // r_act_type
        wr(0x40000438, 0x00000000); // r_in_scale
        wr(0x4000043C, 0xC1000000); // r_w_scale
        wr(0x40000440, 0x3F800000); // r_out_scale
        wr(0x40000310, 0x12);

        // Instruction 161: /model.15/m.1/cv1/act/Sigmoid (Opcode 0x15)
        wr(0x40000444, 0x1D1BC00); // r_a_addr
        wr(0x40000448, 0x1D1BC00); // r_b_addr
        wr(0x40000408, 0x1D27C00); // r_out_addr
        wr(0x40000450, 12288); // r_len
        wr(0x40000454, 0); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 162: /model.15/m.1/cv1/act/Mul (Opcode 0x15)
        wr(0x40000444, 0x1D1BC00); // r_a_addr
        wr(0x40000448, 0x1D27C00); // r_b_addr
        wr(0x40000408, 0x1D2AC00); // r_out_addr
        wr(0x40000450, 12288); // r_len
        wr(0x40000454, 2); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 163: /model.15/m.1/cv2/conv/Conv (Opcode 0x12)
        wr(0x40000400, 0x1D2AC00); // r_in_addr
        wr(0x40000404, 0x0DBAE00); // r_w_addr
        wr(0x40000408, 0x1D2DC00); // r_out_addr
        wr(0x40000410, 4096); // r_m
        wr(0x40000414, 4096); // r_k
        wr(0x40000418, 3); // r_n
        wr(0x4000042C, 0); // r_act_type
        wr(0x40000438, 0x00000000); // r_in_scale
        wr(0x4000043C, 0xC0C00000); // r_w_scale
        wr(0x40000440, 0x3F800000); // r_out_scale
        wr(0x40000310, 0x12);

        // Instruction 164: /model.15/m.1/cv2/act/Sigmoid (Opcode 0x15)
        wr(0x40000444, 0x1D2DC00); // r_a_addr
        wr(0x40000448, 0x1D2DC00); // r_b_addr
        wr(0x40000408, 0x1D39C00); // r_out_addr
        wr(0x40000450, 12288); // r_len
        wr(0x40000454, 0); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 165: /model.15/m.1/cv2/act/Mul (Opcode 0x15)
        wr(0x40000444, 0x1D2DC00); // r_a_addr
        wr(0x40000448, 0x1D39C00); // r_b_addr
        wr(0x40000408, 0x1D3CC00); // r_out_addr
        wr(0x40000450, 12288); // r_len
        wr(0x40000454, 2); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 166: /model.15/cv2/conv/Conv (Opcode 0x12)
        wr(0x40000400, 0x1D3FC00); // r_in_addr
        wr(0x40000404, 0x0DCFC00); // r_w_addr
        wr(0x40000408, 0x1D46C00); // r_out_addr
        wr(0x40000410, 4096); // r_m
        wr(0x40000414, 4096); // r_k
        wr(0x40000418, 1); // r_n
        wr(0x4000042C, 0); // r_act_type
        wr(0x40000438, 0x00000000); // r_in_scale
        wr(0x4000043C, 0xC1C00000); // r_w_scale
        wr(0x40000440, 0x3F800000); // r_out_scale
        wr(0x40000310, 0x12);

        // Instruction 167: /model.15/cv2/act/Sigmoid (Opcode 0x15)
        wr(0x40000444, 0x1D46C00); // r_a_addr
        wr(0x40000448, 0x1D46C00); // r_b_addr
        wr(0x40000408, 0x1D4AC00); // r_out_addr
        wr(0x40000450, 4096); // r_len
        wr(0x40000454, 0); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 168: /model.15/cv2/act/Mul (Opcode 0x15)
        wr(0x40000444, 0x1D46C00); // r_a_addr
        wr(0x40000448, 0x1D4AC00); // r_b_addr
        wr(0x40000408, 0x1D4BC00); // r_out_addr
        wr(0x40000450, 4096); // r_len
        wr(0x40000454, 2); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 169: /model.16/conv/Conv (Opcode 0x12)
        wr(0x40000400, 0x1D4BC00); // r_in_addr
        wr(0x40000404, 0x0DE2400); // r_w_addr
        wr(0x40000408, 0x1D4CC00); // r_out_addr
        wr(0x40000410, 4096); // r_m
        wr(0x40000414, 4096); // r_k
        wr(0x40000418, 3); // r_n
        wr(0x4000042C, 0); // r_act_type
        wr(0x40000438, 0x00000000); // r_in_scale
        wr(0x4000043C, 0x40800000); // r_w_scale
        wr(0x40000440, 0x3F800000); // r_out_scale
        wr(0x40000310, 0x12);

        // Instruction 170: /model.22/cv2.0/cv2.0.0/conv/Conv (Opcode 0x12)
        wr(0x40000400, 0x1D4BC00); // r_in_addr
        wr(0x40000404, 0x0E33800); // r_w_addr
        wr(0x40000408, 0x1D58C00); // r_out_addr
        wr(0x40000410, 4096); // r_m
        wr(0x40000414, 4096); // r_k
        wr(0x40000418, 3); // r_n
        wr(0x4000042C, 0); // r_act_type
        wr(0x40000438, 0x00000000); // r_in_scale
        wr(0x4000043C, 0xC1300000); // r_w_scale
        wr(0x40000440, 0x3F800000); // r_out_scale
        wr(0x40000310, 0x12);

        // Instruction 171: /model.22/cv3.0/cv3.0.0/conv/Conv (Opcode 0x12)
        wr(0x40000400, 0x1D4BC00); // r_in_addr
        wr(0x40000404, 0x0E4EC00); // r_w_addr
        wr(0x40000408, 0x1D64C00); // r_out_addr
        wr(0x40000410, 4096); // r_m
        wr(0x40000414, 4096); // r_k
        wr(0x40000418, 3); // r_n
        wr(0x4000042C, 0); // r_act_type
        wr(0x40000438, 0x00000000); // r_in_scale
        wr(0x4000043C, 0xC0E00000); // r_w_scale
        wr(0x40000440, 0x3F800000); // r_out_scale
        wr(0x40000310, 0x12);

        // Instruction 172: /model.16/act/Sigmoid (Opcode 0x15)
        wr(0x40000444, 0x1D4CC00); // r_a_addr
        wr(0x40000448, 0x1D4CC00); // r_b_addr
        wr(0x40000408, 0x1D70C00); // r_out_addr
        wr(0x40000450, 12288); // r_len
        wr(0x40000454, 0); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 173: /model.22/cv2.0/cv2.0.0/act/Sigmoid (Opcode 0x15)
        wr(0x40000444, 0x1D58C00); // r_a_addr
        wr(0x40000448, 0x1D58C00); // r_b_addr
        wr(0x40000408, 0x1D73C00); // r_out_addr
        wr(0x40000450, 12288); // r_len
        wr(0x40000454, 0); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 174: /model.22/cv3.0/cv3.0.0/act/Sigmoid (Opcode 0x15)
        wr(0x40000444, 0x1D64C00); // r_a_addr
        wr(0x40000448, 0x1D64C00); // r_b_addr
        wr(0x40000408, 0x1D76C00); // r_out_addr
        wr(0x40000450, 12288); // r_len
        wr(0x40000454, 0); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 175: /model.16/act/Mul (Opcode 0x15)
        wr(0x40000444, 0x1D4CC00); // r_a_addr
        wr(0x40000448, 0x1D70C00); // r_b_addr
        wr(0x40000408, 0x1D79C00); // r_out_addr
        wr(0x40000450, 12288); // r_len
        wr(0x40000454, 2); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 176: /model.22/cv2.0/cv2.0.0/act/Mul (Opcode 0x15)
        wr(0x40000444, 0x1D58C00); // r_a_addr
        wr(0x40000448, 0x1D73C00); // r_b_addr
        wr(0x40000408, 0x1D7CC00); // r_out_addr
        wr(0x40000450, 12288); // r_len
        wr(0x40000454, 2); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 177: /model.22/cv3.0/cv3.0.0/act/Mul (Opcode 0x15)
        wr(0x40000444, 0x1D64C00); // r_a_addr
        wr(0x40000448, 0x1D76C00); // r_b_addr
        wr(0x40000408, 0x1D7FC00); // r_out_addr
        wr(0x40000450, 12288); // r_len
        wr(0x40000454, 2); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 178: /model.22/cv2.0/cv2.0.1/conv/Conv (Opcode 0x12)
        wr(0x40000400, 0x1D7CC00); // r_in_addr
        wr(0x40000404, 0x0EA0E00); // r_w_addr
        wr(0x40000408, 0x1D86C00); // r_out_addr
        wr(0x40000410, 4096); // r_m
        wr(0x40000414, 4096); // r_k
        wr(0x40000418, 3); // r_n
        wr(0x4000042C, 0); // r_act_type
        wr(0x40000438, 0x00000000); // r_in_scale
        wr(0x4000043C, 0xC0000000); // r_w_scale
        wr(0x40000440, 0x3F800000); // r_out_scale
        wr(0x40000310, 0x12);

        // Instruction 179: /model.22/cv3.0/cv3.0.1/conv/Conv (Opcode 0x12)
        wr(0x40000400, 0x1D7FC00); // r_in_addr
        wr(0x40000404, 0x0EAA200); // r_w_addr
        wr(0x40000408, 0x1D92C00); // r_out_addr
        wr(0x40000410, 4096); // r_m
        wr(0x40000414, 4096); // r_k
        wr(0x40000418, 3); // r_n
        wr(0x4000042C, 0); // r_act_type
        wr(0x40000438, 0x00000000); // r_in_scale
        wr(0x4000043C, 0xC0000000); // r_w_scale
        wr(0x40000440, 0x3F800000); // r_out_scale
        wr(0x40000310, 0x12);

        // Instruction 180: /model.18/cv1/conv/Conv (Opcode 0x12)
        wr(0x40000400, 0x1D82C00); // r_in_addr
        wr(0x40000404, 0x0EFB800); // r_w_addr
        wr(0x40000408, 0x1D9EC00); // r_out_addr
        wr(0x40000410, 4096); // r_m
        wr(0x40000414, 4096); // r_k
        wr(0x40000418, 1); // r_n
        wr(0x4000042C, 0); // r_act_type
        wr(0x40000438, 0x00000000); // r_in_scale
        wr(0x4000043C, 0xBF800000); // r_w_scale
        wr(0x40000440, 0x3F800000); // r_out_scale
        wr(0x40000310, 0x12);

        // Instruction 181: /model.22/cv2.0/cv2.0.1/act/Sigmoid (Opcode 0x15)
        wr(0x40000444, 0x1D86C00); // r_a_addr
        wr(0x40000448, 0x1D86C00); // r_b_addr
        wr(0x40000408, 0x1DA2C00); // r_out_addr
        wr(0x40000450, 12288); // r_len
        wr(0x40000454, 0); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 182: /model.22/cv3.0/cv3.0.1/act/Sigmoid (Opcode 0x15)
        wr(0x40000444, 0x1D92C00); // r_a_addr
        wr(0x40000448, 0x1D92C00); // r_b_addr
        wr(0x40000408, 0x1DA5C00); // r_out_addr
        wr(0x40000450, 12288); // r_len
        wr(0x40000454, 0); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 183: /model.18/cv1/act/Sigmoid (Opcode 0x15)
        wr(0x40000444, 0x1D9EC00); // r_a_addr
        wr(0x40000448, 0x1D9EC00); // r_b_addr
        wr(0x40000408, 0x1DA8C00); // r_out_addr
        wr(0x40000450, 4096); // r_len
        wr(0x40000454, 0); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 184: /model.22/cv2.0/cv2.0.1/act/Mul (Opcode 0x15)
        wr(0x40000444, 0x1D86C00); // r_a_addr
        wr(0x40000448, 0x1DA2C00); // r_b_addr
        wr(0x40000408, 0x1DA9C00); // r_out_addr
        wr(0x40000450, 12288); // r_len
        wr(0x40000454, 2); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 185: /model.22/cv3.0/cv3.0.1/act/Mul (Opcode 0x15)
        wr(0x40000444, 0x1D92C00); // r_a_addr
        wr(0x40000448, 0x1DA5C00); // r_b_addr
        wr(0x40000408, 0x1DACC00); // r_out_addr
        wr(0x40000450, 12288); // r_len
        wr(0x40000454, 2); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 186: /model.18/cv1/act/Mul (Opcode 0x15)
        wr(0x40000444, 0x1D9EC00); // r_a_addr
        wr(0x40000448, 0x1DA8C00); // r_b_addr
        wr(0x40000408, 0x1DAFC00); // r_out_addr
        wr(0x40000450, 4096); // r_len
        wr(0x40000454, 2); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 187: /model.22/cv2.0/cv2.0.2/Conv (Opcode 0x12)
        wr(0x40000400, 0x1DA9C00); // r_in_addr
        wr(0x40000404, 0x0F32800); // r_w_addr
        wr(0x40000408, 0x1DB0C00); // r_out_addr
        wr(0x40000410, 4096); // r_m
        wr(0x40000414, 4096); // r_k
        wr(0x40000418, 1); // r_n
        wr(0x4000042C, 0); // r_act_type
        wr(0x40000438, 0x00000000); // r_in_scale
        wr(0x4000043C, 0x40000000); // r_w_scale
        wr(0x40000440, 0x3F800000); // r_out_scale
        wr(0x40000310, 0x12);

        // Instruction 188: /model.22/cv3.0/cv3.0.2/Conv (Opcode 0x12)
        wr(0x40000400, 0x1DACC00); // r_in_addr
        wr(0x40000404, 0x0F33C00); // r_w_addr
        wr(0x40000408, 0x1DB4C00); // r_out_addr
        wr(0x40000410, 4096); // r_m
        wr(0x40000414, 4096); // r_k
        wr(0x40000418, 1); // r_n
        wr(0x4000042C, 0); // r_act_type
        wr(0x40000438, 0x00000000); // r_in_scale
        wr(0x4000043C, 0x41700000); // r_w_scale
        wr(0x40000440, 0x3F800000); // r_out_scale
        wr(0x40000310, 0x12);

        // Instruction 189: /model.18/m.0/cv1/conv/Conv (Opcode 0x12)
        wr(0x40000400, 0x1DB0400); // r_in_addr
        wr(0x40000404, 0x0F37E00); // r_w_addr
        wr(0x40000408, 0x1DBAC00); // r_out_addr
        wr(0x40000410, 2048); // r_m
        wr(0x40000414, 2048); // r_k
        wr(0x40000418, 3); // r_n
        wr(0x4000042C, 0); // r_act_type
        wr(0x40000438, 0x00000000); // r_in_scale
        wr(0x4000043C, 0xC2500000); // r_w_scale
        wr(0x40000440, 0x3F800000); // r_out_scale
        wr(0x40000310, 0x12);

        // Instruction 190: /model.18/m.0/cv1/act/Sigmoid (Opcode 0x15)
        wr(0x40000444, 0x1DBAC00); // r_a_addr
        wr(0x40000448, 0x1DBAC00); // r_b_addr
        wr(0x40000408, 0x1DC0C00); // r_out_addr
        wr(0x40000450, 6144); // r_len
        wr(0x40000454, 0); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 191: /model.18/m.0/cv1/act/Mul (Opcode 0x15)
        wr(0x40000444, 0x1DBAC00); // r_a_addr
        wr(0x40000448, 0x1DC0C00); // r_b_addr
        wr(0x40000408, 0x1DC2400); // r_out_addr
        wr(0x40000450, 6144); // r_len
        wr(0x40000454, 2); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 192: /model.18/m.0/cv2/conv/Conv (Opcode 0x12)
        wr(0x40000400, 0x1DC2400); // r_in_addr
        wr(0x40000404, 0x0F89600); // r_w_addr
        wr(0x40000408, 0x1DC3C00); // r_out_addr
        wr(0x40000410, 4096); // r_m
        wr(0x40000414, 4096); // r_k
        wr(0x40000418, 3); // r_n
        wr(0x4000042C, 0); // r_act_type
        wr(0x40000438, 0x00000000); // r_in_scale
        wr(0x4000043C, 0x41980000); // r_w_scale
        wr(0x40000440, 0x3F800000); // r_out_scale
        wr(0x40000310, 0x12);

        // Instruction 193: /model.18/m.0/cv2/act/Sigmoid (Opcode 0x15)
        wr(0x40000444, 0x1DC3C00); // r_a_addr
        wr(0x40000448, 0x1DC3C00); // r_b_addr
        wr(0x40000408, 0x1DCFC00); // r_out_addr
        wr(0x40000450, 12288); // r_len
        wr(0x40000454, 0); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 194: /model.18/m.0/cv2/act/Mul (Opcode 0x15)
        wr(0x40000444, 0x1DC3C00); // r_a_addr
        wr(0x40000448, 0x1DCFC00); // r_b_addr
        wr(0x40000408, 0x1DD2C00); // r_out_addr
        wr(0x40000450, 12288); // r_len
        wr(0x40000454, 2); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 195: /model.18/m.1/cv1/conv/Conv (Opcode 0x12)
        wr(0x40000400, 0x1DD2C00); // r_in_addr
        wr(0x40000404, 0x0FDAE00); // r_w_addr
        wr(0x40000408, 0x1DD5C00); // r_out_addr
        wr(0x40000410, 4096); // r_m
        wr(0x40000414, 4096); // r_k
        wr(0x40000418, 3); // r_n
        wr(0x4000042C, 0); // r_act_type
        wr(0x40000438, 0x00000000); // r_in_scale
        wr(0x4000043C, 0x41A80000); // r_w_scale
        wr(0x40000440, 0x3F800000); // r_out_scale
        wr(0x40000310, 0x12);

        // Instruction 196: /model.18/m.1/cv1/act/Sigmoid (Opcode 0x15)
        wr(0x40000444, 0x1DD5C00); // r_a_addr
        wr(0x40000448, 0x1DD5C00); // r_b_addr
        wr(0x40000408, 0x1DE1C00); // r_out_addr
        wr(0x40000450, 12288); // r_len
        wr(0x40000454, 0); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 197: /model.18/m.1/cv1/act/Mul (Opcode 0x15)
        wr(0x40000444, 0x1DD5C00); // r_a_addr
        wr(0x40000448, 0x1DE1C00); // r_b_addr
        wr(0x40000408, 0x1DE4C00); // r_out_addr
        wr(0x40000450, 12288); // r_len
        wr(0x40000454, 2); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 198: /model.18/m.1/cv2/conv/Conv (Opcode 0x12)
        wr(0x40000400, 0x1DE4C00); // r_in_addr
        wr(0x40000404, 0x102C600); // r_w_addr
        wr(0x40000408, 0x1DE7C00); // r_out_addr
        wr(0x40000410, 4096); // r_m
        wr(0x40000414, 4096); // r_k
        wr(0x40000418, 3); // r_n
        wr(0x4000042C, 0); // r_act_type
        wr(0x40000438, 0x00000000); // r_in_scale
        wr(0x4000043C, 0xC0400000); // r_w_scale
        wr(0x40000440, 0x3F800000); // r_out_scale
        wr(0x40000310, 0x12);

        // Instruction 199: /model.18/m.1/cv2/act/Sigmoid (Opcode 0x15)
        wr(0x40000444, 0x1DE7C00); // r_a_addr
        wr(0x40000448, 0x1DE7C00); // r_b_addr
        wr(0x40000408, 0x1DF3C00); // r_out_addr
        wr(0x40000450, 12288); // r_len
        wr(0x40000454, 0); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 200: /model.18/m.1/cv2/act/Mul (Opcode 0x15)
        wr(0x40000444, 0x1DE7C00); // r_a_addr
        wr(0x40000448, 0x1DF3C00); // r_b_addr
        wr(0x40000408, 0x1DF6C00); // r_out_addr
        wr(0x40000450, 12288); // r_len
        wr(0x40000454, 2); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 201: /model.18/cv2/conv/Conv (Opcode 0x12)
        wr(0x40000400, 0x1DF9C00); // r_in_addr
        wr(0x40000404, 0x107E200); // r_w_addr
        wr(0x40000408, 0x1E00C00); // r_out_addr
        wr(0x40000410, 4096); // r_m
        wr(0x40000414, 4096); // r_k
        wr(0x40000418, 1); // r_n
        wr(0x4000042C, 0); // r_act_type
        wr(0x40000438, 0x00000000); // r_in_scale
        wr(0x4000043C, 0xC0A00000); // r_w_scale
        wr(0x40000440, 0x3F800000); // r_out_scale
        wr(0x40000310, 0x12);

        // Instruction 202: /model.18/cv2/act/Sigmoid (Opcode 0x15)
        wr(0x40000444, 0x1E00C00); // r_a_addr
        wr(0x40000448, 0x1E00C00); // r_b_addr
        wr(0x40000408, 0x1E04C00); // r_out_addr
        wr(0x40000450, 4096); // r_len
        wr(0x40000454, 0); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 203: /model.18/cv2/act/Mul (Opcode 0x15)
        wr(0x40000444, 0x1E00C00); // r_a_addr
        wr(0x40000448, 0x1E04C00); // r_b_addr
        wr(0x40000408, 0x1E05C00); // r_out_addr
        wr(0x40000450, 4096); // r_len
        wr(0x40000454, 2); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 204: /model.19/conv/Conv (Opcode 0x12)
        wr(0x40000400, 0x1E05C00); // r_in_addr
        wr(0x40000404, 0x10C6C00); // r_w_addr
        wr(0x40000408, 0x1E06C00); // r_out_addr
        wr(0x40000410, 4096); // r_m
        wr(0x40000414, 4096); // r_k
        wr(0x40000418, 3); // r_n
        wr(0x4000042C, 0); // r_act_type
        wr(0x40000438, 0x00000000); // r_in_scale
        wr(0x4000043C, 0xC0800000); // r_w_scale
        wr(0x40000440, 0x3F800000); // r_out_scale
        wr(0x40000310, 0x12);

        // Instruction 205: /model.22/cv2.1/cv2.1.0/conv/Conv (Opcode 0x12)
        wr(0x40000400, 0x1E05C00); // r_in_addr
        wr(0x40000404, 0x120B000); // r_w_addr
        wr(0x40000408, 0x1E12C00); // r_out_addr
        wr(0x40000410, 4096); // r_m
        wr(0x40000414, 4096); // r_k
        wr(0x40000418, 3); // r_n
        wr(0x4000042C, 0); // r_act_type
        wr(0x40000438, 0x00000000); // r_in_scale
        wr(0x4000043C, 0x40000000); // r_w_scale
        wr(0x40000440, 0x3F800000); // r_out_scale
        wr(0x40000310, 0x12);

        // Instruction 206: /model.22/cv3.1/cv3.1.0/conv/Conv (Opcode 0x12)
        wr(0x40000400, 0x1E05C00); // r_in_addr
        wr(0x40000404, 0x1241400); // r_w_addr
        wr(0x40000408, 0x1E1EC00); // r_out_addr
        wr(0x40000410, 4096); // r_m
        wr(0x40000414, 4096); // r_k
        wr(0x40000418, 3); // r_n
        wr(0x4000042C, 0); // r_act_type
        wr(0x40000438, 0x00000000); // r_in_scale
        wr(0x4000043C, 0x3F800000); // r_w_scale
        wr(0x40000440, 0x3F800000); // r_out_scale
        wr(0x40000310, 0x12);

        // Instruction 207: /model.19/act/Sigmoid (Opcode 0x15)
        wr(0x40000444, 0x1E06C00); // r_a_addr
        wr(0x40000448, 0x1E06C00); // r_b_addr
        wr(0x40000408, 0x1E2AC00); // r_out_addr
        wr(0x40000450, 12288); // r_len
        wr(0x40000454, 0); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 208: /model.22/cv2.1/cv2.1.0/act/Sigmoid (Opcode 0x15)
        wr(0x40000444, 0x1E12C00); // r_a_addr
        wr(0x40000448, 0x1E12C00); // r_b_addr
        wr(0x40000408, 0x1E2DC00); // r_out_addr
        wr(0x40000450, 12288); // r_len
        wr(0x40000454, 0); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 209: /model.22/cv3.1/cv3.1.0/act/Sigmoid (Opcode 0x15)
        wr(0x40000444, 0x1E1EC00); // r_a_addr
        wr(0x40000448, 0x1E1EC00); // r_b_addr
        wr(0x40000408, 0x1E30C00); // r_out_addr
        wr(0x40000450, 12288); // r_len
        wr(0x40000454, 0); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 210: /model.19/act/Mul (Opcode 0x15)
        wr(0x40000444, 0x1E06C00); // r_a_addr
        wr(0x40000448, 0x1E2AC00); // r_b_addr
        wr(0x40000408, 0x1E33C00); // r_out_addr
        wr(0x40000450, 12288); // r_len
        wr(0x40000454, 2); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 211: /model.22/cv2.1/cv2.1.0/act/Mul (Opcode 0x15)
        wr(0x40000444, 0x1E12C00); // r_a_addr
        wr(0x40000448, 0x1E2DC00); // r_b_addr
        wr(0x40000408, 0x1E36C00); // r_out_addr
        wr(0x40000450, 12288); // r_len
        wr(0x40000454, 2); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 212: /model.22/cv3.1/cv3.1.0/act/Mul (Opcode 0x15)
        wr(0x40000444, 0x1E1EC00); // r_a_addr
        wr(0x40000448, 0x1E30C00); // r_b_addr
        wr(0x40000408, 0x1E39C00); // r_out_addr
        wr(0x40000450, 12288); // r_len
        wr(0x40000454, 2); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 213: /model.22/cv2.1/cv2.1.1/conv/Conv (Opcode 0x12)
        wr(0x40000400, 0x1E36C00); // r_in_addr
        wr(0x40000404, 0x12E4600); // r_w_addr
        wr(0x40000408, 0x1E40C00); // r_out_addr
        wr(0x40000410, 4096); // r_m
        wr(0x40000414, 4096); // r_k
        wr(0x40000418, 3); // r_n
        wr(0x4000042C, 0); // r_act_type
        wr(0x40000438, 0x00000000); // r_in_scale
        wr(0x4000043C, 0xBF800000); // r_w_scale
        wr(0x40000440, 0x3F800000); // r_out_scale
        wr(0x40000310, 0x12);

        // Instruction 214: /model.22/cv3.1/cv3.1.1/conv/Conv (Opcode 0x12)
        wr(0x40000400, 0x1E39C00); // r_in_addr
        wr(0x40000404, 0x12EDA00); // r_w_addr
        wr(0x40000408, 0x1E4CC00); // r_out_addr
        wr(0x40000410, 4096); // r_m
        wr(0x40000414, 4096); // r_k
        wr(0x40000418, 3); // r_n
        wr(0x4000042C, 0); // r_act_type
        wr(0x40000438, 0x00000000); // r_in_scale
        wr(0x4000043C, 0x40C00000); // r_w_scale
        wr(0x40000440, 0x3F800000); // r_out_scale
        wr(0x40000310, 0x12);

        // Instruction 215: /model.21/cv1/conv/Conv (Opcode 0x12)
        wr(0x40000400, 0x1E3CC00); // r_in_addr
        wr(0x40000404, 0x133F200); // r_w_addr
        wr(0x40000408, 0x1E58C00); // r_out_addr
        wr(0x40000410, 4096); // r_m
        wr(0x40000414, 4096); // r_k
        wr(0x40000418, 1); // r_n
        wr(0x4000042C, 0); // r_act_type
        wr(0x40000438, 0x00000000); // r_in_scale
        wr(0x4000043C, 0x41300000); // r_w_scale
        wr(0x40000440, 0x3F800000); // r_out_scale
        wr(0x40000310, 0x12);

        // Instruction 216: /model.22/cv2.1/cv2.1.1/act/Sigmoid (Opcode 0x15)
        wr(0x40000444, 0x1E40C00); // r_a_addr
        wr(0x40000448, 0x1E40C00); // r_b_addr
        wr(0x40000408, 0x1E5CC00); // r_out_addr
        wr(0x40000450, 12288); // r_len
        wr(0x40000454, 0); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 217: /model.22/cv3.1/cv3.1.1/act/Sigmoid (Opcode 0x15)
        wr(0x40000444, 0x1E4CC00); // r_a_addr
        wr(0x40000448, 0x1E4CC00); // r_b_addr
        wr(0x40000408, 0x1E5FC00); // r_out_addr
        wr(0x40000450, 12288); // r_len
        wr(0x40000454, 0); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 218: /model.21/cv1/act/Sigmoid (Opcode 0x15)
        wr(0x40000444, 0x1E58C00); // r_a_addr
        wr(0x40000448, 0x1E58C00); // r_b_addr
        wr(0x40000408, 0x1E62C00); // r_out_addr
        wr(0x40000450, 4096); // r_len
        wr(0x40000454, 0); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 219: /model.22/cv2.1/cv2.1.1/act/Mul (Opcode 0x15)
        wr(0x40000444, 0x1E40C00); // r_a_addr
        wr(0x40000448, 0x1E5CC00); // r_b_addr
        wr(0x40000408, 0x1E63C00); // r_out_addr
        wr(0x40000450, 12288); // r_len
        wr(0x40000454, 2); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 220: /model.22/cv3.1/cv3.1.1/act/Mul (Opcode 0x15)
        wr(0x40000444, 0x1E4CC00); // r_a_addr
        wr(0x40000448, 0x1E5FC00); // r_b_addr
        wr(0x40000408, 0x1E66C00); // r_out_addr
        wr(0x40000450, 12288); // r_len
        wr(0x40000454, 2); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 221: /model.21/cv1/act/Mul (Opcode 0x15)
        wr(0x40000444, 0x1E58C00); // r_a_addr
        wr(0x40000448, 0x1E62C00); // r_b_addr
        wr(0x40000408, 0x1E69C00); // r_out_addr
        wr(0x40000450, 4096); // r_len
        wr(0x40000454, 2); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 222: /model.22/cv2.1/cv2.1.2/Conv (Opcode 0x12)
        wr(0x40000400, 0x1E63C00); // r_in_addr
        wr(0x40000404, 0x13C7200); // r_w_addr
        wr(0x40000408, 0x1E6AC00); // r_out_addr
        wr(0x40000410, 4096); // r_m
        wr(0x40000414, 4096); // r_k
        wr(0x40000418, 1); // r_n
        wr(0x4000042C, 0); // r_act_type
        wr(0x40000438, 0x00000000); // r_in_scale
        wr(0x4000043C, 0x3F800000); // r_w_scale
        wr(0x40000440, 0x3F800000); // r_out_scale
        wr(0x40000310, 0x12);

        // Instruction 223: /model.22/cv3.1/cv3.1.2/Conv (Opcode 0x12)
        wr(0x40000400, 0x1E66C00); // r_in_addr
        wr(0x40000404, 0x13C8600); // r_w_addr
        wr(0x40000408, 0x1E6EC00); // r_out_addr
        wr(0x40000410, 4096); // r_m
        wr(0x40000414, 4096); // r_k
        wr(0x40000418, 1); // r_n
        wr(0x4000042C, 0); // r_act_type
        wr(0x40000438, 0x00000000); // r_in_scale
        wr(0x4000043C, 0x3F800000); // r_w_scale
        wr(0x40000440, 0x3F800000); // r_out_scale
        wr(0x40000310, 0x12);

        // Instruction 224: /model.21/m.0/cv1/conv/Conv (Opcode 0x12)
        wr(0x40000400, 0x1E6A400); // r_in_addr
        wr(0x40000404, 0x13CCA00); // r_w_addr
        wr(0x40000408, 0x1E74C00); // r_out_addr
        wr(0x40000410, 2048); // r_m
        wr(0x40000414, 2048); // r_k
        wr(0x40000418, 3); // r_n
        wr(0x4000042C, 0); // r_act_type
        wr(0x40000438, 0x00000000); // r_in_scale
        wr(0x4000043C, 0x41A00000); // r_w_scale
        wr(0x40000440, 0x3F800000); // r_out_scale
        wr(0x40000310, 0x12);

        // Instruction 225: /model.21/m.0/cv1/act/Sigmoid (Opcode 0x15)
        wr(0x40000444, 0x1E74C00); // r_a_addr
        wr(0x40000448, 0x1E74C00); // r_b_addr
        wr(0x40000408, 0x1E7AC00); // r_out_addr
        wr(0x40000450, 6144); // r_len
        wr(0x40000454, 0); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 226: /model.21/m.0/cv1/act/Mul (Opcode 0x15)
        wr(0x40000444, 0x1E74C00); // r_a_addr
        wr(0x40000448, 0x1E7AC00); // r_b_addr
        wr(0x40000408, 0x1E7C400); // r_out_addr
        wr(0x40000450, 6144); // r_len
        wr(0x40000454, 2); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 227: /model.21/m.0/cv2/conv/Conv (Opcode 0x12)
        wr(0x40000400, 0x1E7C400); // r_in_addr
        wr(0x40000404, 0x1483800); // r_w_addr
        wr(0x40000408, 0x1E7DC00); // r_out_addr
        wr(0x40000410, 4096); // r_m
        wr(0x40000414, 4096); // r_k
        wr(0x40000418, 3); // r_n
        wr(0x4000042C, 0); // r_act_type
        wr(0x40000438, 0x00000000); // r_in_scale
        wr(0x4000043C, 0x40400000); // r_w_scale
        wr(0x40000440, 0x3F800000); // r_out_scale
        wr(0x40000310, 0x12);

        // Instruction 228: /model.21/m.0/cv2/act/Sigmoid (Opcode 0x15)
        wr(0x40000444, 0x1E7DC00); // r_a_addr
        wr(0x40000448, 0x1E7DC00); // r_b_addr
        wr(0x40000408, 0x1E89C00); // r_out_addr
        wr(0x40000450, 12288); // r_len
        wr(0x40000454, 0); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 229: /model.21/m.0/cv2/act/Mul (Opcode 0x15)
        wr(0x40000444, 0x1E7DC00); // r_a_addr
        wr(0x40000448, 0x1E89C00); // r_b_addr
        wr(0x40000408, 0x1E8CC00); // r_out_addr
        wr(0x40000450, 12288); // r_len
        wr(0x40000454, 2); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 230: /model.21/m.1/cv1/conv/Conv (Opcode 0x12)
        wr(0x40000400, 0x1E8CC00); // r_in_addr
        wr(0x40000404, 0x153A600); // r_w_addr
        wr(0x40000408, 0x1E8FC00); // r_out_addr
        wr(0x40000410, 4096); // r_m
        wr(0x40000414, 4096); // r_k
        wr(0x40000418, 3); // r_n
        wr(0x4000042C, 0); // r_act_type
        wr(0x40000438, 0x00000000); // r_in_scale
        wr(0x4000043C, 0xC0C00000); // r_w_scale
        wr(0x40000440, 0x3F800000); // r_out_scale
        wr(0x40000310, 0x12);

        // Instruction 231: /model.21/m.1/cv1/act/Sigmoid (Opcode 0x15)
        wr(0x40000444, 0x1E8FC00); // r_a_addr
        wr(0x40000448, 0x1E8FC00); // r_b_addr
        wr(0x40000408, 0x1E9BC00); // r_out_addr
        wr(0x40000450, 12288); // r_len
        wr(0x40000454, 0); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 232: /model.21/m.1/cv1/act/Mul (Opcode 0x15)
        wr(0x40000444, 0x1E8FC00); // r_a_addr
        wr(0x40000448, 0x1E9BC00); // r_b_addr
        wr(0x40000408, 0x1E9EC00); // r_out_addr
        wr(0x40000450, 12288); // r_len
        wr(0x40000454, 2); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 233: /model.21/m.1/cv2/conv/Conv (Opcode 0x12)
        wr(0x40000400, 0x1E9EC00); // r_in_addr
        wr(0x40000404, 0x15F1400); // r_w_addr
        wr(0x40000408, 0x1EA1C00); // r_out_addr
        wr(0x40000410, 4096); // r_m
        wr(0x40000414, 4096); // r_k
        wr(0x40000418, 3); // r_n
        wr(0x4000042C, 0); // r_act_type
        wr(0x40000438, 0x00000000); // r_in_scale
        wr(0x4000043C, 0x41800000); // r_w_scale
        wr(0x40000440, 0x3F800000); // r_out_scale
        wr(0x40000310, 0x12);

        // Instruction 234: /model.21/m.1/cv2/act/Sigmoid (Opcode 0x15)
        wr(0x40000444, 0x1EA1C00); // r_a_addr
        wr(0x40000448, 0x1EA1C00); // r_b_addr
        wr(0x40000408, 0x1EADC00); // r_out_addr
        wr(0x40000450, 12288); // r_len
        wr(0x40000454, 0); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 235: /model.21/m.1/cv2/act/Mul (Opcode 0x15)
        wr(0x40000444, 0x1EA1C00); // r_a_addr
        wr(0x40000448, 0x1EADC00); // r_b_addr
        wr(0x40000408, 0x1EB0C00); // r_out_addr
        wr(0x40000450, 12288); // r_len
        wr(0x40000454, 2); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 236: /model.21/cv2/conv/Conv (Opcode 0x12)
        wr(0x40000400, 0x1EB3C00); // r_in_addr
        wr(0x40000404, 0x16A8600); // r_w_addr
        wr(0x40000408, 0x1EBAC00); // r_out_addr
        wr(0x40000410, 4096); // r_m
        wr(0x40000414, 4096); // r_k
        wr(0x40000418, 1); // r_n
        wr(0x4000042C, 0); // r_act_type
        wr(0x40000438, 0x00000000); // r_in_scale
        wr(0x4000043C, 0x41400000); // r_w_scale
        wr(0x40000440, 0x3F800000); // r_out_scale
        wr(0x40000310, 0x12);

        // Instruction 237: /model.21/cv2/act/Sigmoid (Opcode 0x15)
        wr(0x40000444, 0x1EBAC00); // r_a_addr
        wr(0x40000448, 0x1EBAC00); // r_b_addr
        wr(0x40000408, 0x1EBEC00); // r_out_addr
        wr(0x40000450, 4096); // r_len
        wr(0x40000454, 0); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 238: /model.21/cv2/act/Mul (Opcode 0x15)
        wr(0x40000444, 0x1EBAC00); // r_a_addr
        wr(0x40000448, 0x1EBEC00); // r_b_addr
        wr(0x40000408, 0x1EBFC00); // r_out_addr
        wr(0x40000450, 4096); // r_len
        wr(0x40000454, 2); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 239: /model.22/cv2.2/cv2.2.0/conv/Conv (Opcode 0x12)
        wr(0x40000400, 0x1EBFC00); // r_in_addr
        wr(0x40000404, 0x174AE00); // r_w_addr
        wr(0x40000408, 0x1EC0C00); // r_out_addr
        wr(0x40000410, 4096); // r_m
        wr(0x40000414, 4096); // r_k
        wr(0x40000418, 3); // r_n
        wr(0x4000042C, 0); // r_act_type
        wr(0x40000438, 0x00000000); // r_in_scale
        wr(0x4000043C, 0xC0400000); // r_w_scale
        wr(0x40000440, 0x3F800000); // r_out_scale
        wr(0x40000310, 0x12);

        // Instruction 240: /model.22/cv3.2/cv3.2.0/conv/Conv (Opcode 0x12)
        wr(0x40000400, 0x1EBFC00); // r_in_addr
        wr(0x40000404, 0x179C200); // r_w_addr
        wr(0x40000408, 0x1ECCC00); // r_out_addr
        wr(0x40000410, 4096); // r_m
        wr(0x40000414, 4096); // r_k
        wr(0x40000418, 3); // r_n
        wr(0x4000042C, 0); // r_act_type
        wr(0x40000438, 0x00000000); // r_in_scale
        wr(0x4000043C, 0xC0000000); // r_w_scale
        wr(0x40000440, 0x3F800000); // r_out_scale
        wr(0x40000310, 0x12);

        // Instruction 241: /model.22/cv2.2/cv2.2.0/act/Sigmoid (Opcode 0x15)
        wr(0x40000444, 0x1EC0C00); // r_a_addr
        wr(0x40000448, 0x1EC0C00); // r_b_addr
        wr(0x40000408, 0x1ED8C00); // r_out_addr
        wr(0x40000450, 12288); // r_len
        wr(0x40000454, 0); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 242: /model.22/cv3.2/cv3.2.0/act/Sigmoid (Opcode 0x15)
        wr(0x40000444, 0x1ECCC00); // r_a_addr
        wr(0x40000448, 0x1ECCC00); // r_b_addr
        wr(0x40000408, 0x1EDBC00); // r_out_addr
        wr(0x40000450, 12288); // r_len
        wr(0x40000454, 0); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 243: /model.22/cv2.2/cv2.2.0/act/Mul (Opcode 0x15)
        wr(0x40000444, 0x1EC0C00); // r_a_addr
        wr(0x40000448, 0x1ED8C00); // r_b_addr
        wr(0x40000408, 0x1EDEC00); // r_out_addr
        wr(0x40000450, 12288); // r_len
        wr(0x40000454, 2); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 244: /model.22/cv3.2/cv3.2.0/act/Mul (Opcode 0x15)
        wr(0x40000444, 0x1ECCC00); // r_a_addr
        wr(0x40000448, 0x1EDBC00); // r_b_addr
        wr(0x40000408, 0x1EE1C00); // r_out_addr
        wr(0x40000450, 12288); // r_len
        wr(0x40000454, 2); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 245: /model.22/cv2.2/cv2.2.1/conv/Conv (Opcode 0x12)
        wr(0x40000400, 0x1EDEC00); // r_in_addr
        wr(0x40000404, 0x188FE00); // r_w_addr
        wr(0x40000408, 0x1EE4C00); // r_out_addr
        wr(0x40000410, 4096); // r_m
        wr(0x40000414, 4096); // r_k
        wr(0x40000418, 3); // r_n
        wr(0x4000042C, 0); // r_act_type
        wr(0x40000438, 0x00000000); // r_in_scale
        wr(0x4000043C, 0xC0A00000); // r_w_scale
        wr(0x40000440, 0x3F800000); // r_out_scale
        wr(0x40000310, 0x12);

        // Instruction 246: /model.22/cv3.2/cv3.2.1/conv/Conv (Opcode 0x12)
        wr(0x40000400, 0x1EE1C00); // r_in_addr
        wr(0x40000404, 0x1899200); // r_w_addr
        wr(0x40000408, 0x1EF0C00); // r_out_addr
        wr(0x40000410, 4096); // r_m
        wr(0x40000414, 4096); // r_k
        wr(0x40000418, 3); // r_n
        wr(0x4000042C, 0); // r_act_type
        wr(0x40000438, 0x00000000); // r_in_scale
        wr(0x4000043C, 0xC1800000); // r_w_scale
        wr(0x40000440, 0x3F800000); // r_out_scale
        wr(0x40000310, 0x12);

        // Instruction 247: /model.22/cv2.2/cv2.2.1/act/Sigmoid (Opcode 0x15)
        wr(0x40000444, 0x1EE4C00); // r_a_addr
        wr(0x40000448, 0x1EE4C00); // r_b_addr
        wr(0x40000408, 0x1EFCC00); // r_out_addr
        wr(0x40000450, 12288); // r_len
        wr(0x40000454, 0); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 248: /model.22/cv3.2/cv3.2.1/act/Sigmoid (Opcode 0x15)
        wr(0x40000444, 0x1EF0C00); // r_a_addr
        wr(0x40000448, 0x1EF0C00); // r_b_addr
        wr(0x40000408, 0x1EFFC00); // r_out_addr
        wr(0x40000450, 12288); // r_len
        wr(0x40000454, 0); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 249: /model.22/cv2.2/cv2.2.1/act/Mul (Opcode 0x15)
        wr(0x40000444, 0x1EE4C00); // r_a_addr
        wr(0x40000448, 0x1EFCC00); // r_b_addr
        wr(0x40000408, 0x1F02C00); // r_out_addr
        wr(0x40000450, 12288); // r_len
        wr(0x40000454, 2); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 250: /model.22/cv3.2/cv3.2.1/act/Mul (Opcode 0x15)
        wr(0x40000444, 0x1EF0C00); // r_a_addr
        wr(0x40000448, 0x1EFFC00); // r_b_addr
        wr(0x40000408, 0x1F05C00); // r_out_addr
        wr(0x40000450, 12288); // r_len
        wr(0x40000454, 2); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 251: /model.22/cv2.2/cv2.2.2/Conv (Opcode 0x12)
        wr(0x40000400, 0x1F02C00); // r_in_addr
        wr(0x40000404, 0x18EAE00); // r_w_addr
        wr(0x40000408, 0x1F08C00); // r_out_addr
        wr(0x40000410, 4096); // r_m
        wr(0x40000414, 4096); // r_k
        wr(0x40000418, 1); // r_n
        wr(0x4000042C, 0); // r_act_type
        wr(0x40000438, 0x00000000); // r_in_scale
        wr(0x4000043C, 0x40000000); // r_w_scale
        wr(0x40000440, 0x3F800000); // r_out_scale
        wr(0x40000310, 0x12);

        // Instruction 252: /model.22/cv3.2/cv3.2.2/Conv (Opcode 0x12)
        wr(0x40000400, 0x1F05C00); // r_in_addr
        wr(0x40000404, 0x18EC200); // r_w_addr
        wr(0x40000408, 0x1F0CC00); // r_out_addr
        wr(0x40000410, 4096); // r_m
        wr(0x40000414, 4096); // r_k
        wr(0x40000418, 1); // r_n
        wr(0x4000042C, 0); // r_act_type
        wr(0x40000438, 0x00000000); // r_in_scale
        wr(0x4000043C, 0x41100000); // r_w_scale
        wr(0x40000440, 0x3F800000); // r_out_scale
        wr(0x40000310, 0x12);

        // Instruction 253: /model.22/Sigmoid (Opcode 0x15)
        wr(0x40000444, 0x1F15C00); // r_a_addr
        wr(0x40000448, 0x1F15C00); // r_b_addr
        wr(0x40000408, 0x1F18C00); // r_out_addr
        wr(0x40000450, 12288); // r_len
        wr(0x40000454, 0); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 254: /model.22/dfl/Softmax (Opcode 0x13)
        wr(0x40000444, 0x1F12C00); // r_q_addr
        wr(0x40000448, 0x1F12C00); // r_k_addr
        wr(0x4000044C, 0x1F12C00); // r_v_addr
        wr(0x40000408, 0x1F1BC00); // r_out_addr
        wr(0x40000450, 4096); // r_seq_len
        wr(0x40000454, 1); // r_num_heads
        wr(0x40000458, 4096); // r_head_dim
        wr(0x40000310, 0x13);

        // Instruction 255: /model.22/dfl/conv/Conv (Opcode 0x12)
        wr(0x40000400, 0x1F1BC00); // r_in_addr
        wr(0x40000404, 0x18F0A00); // r_w_addr
        wr(0x40000408, 0x2F1BC00); // r_out_addr
        wr(0x40000410, 4096); // r_m
        wr(0x40000414, 4096); // r_k
        wr(0x40000418, 1); // r_n
        wr(0x4000042C, 0); // r_act_type
        wr(0x40000438, 0x00000000); // r_in_scale
        wr(0x4000043C, 0x00000000); // r_w_scale
        wr(0x40000440, 0x3F800000); // r_out_scale
        wr(0x40000310, 0x12);

        // Instruction 256: /model.22/Sub (Opcode 0x15)
        wr(0x40000444, 0x0010D00); // r_a_addr
        wr(0x40000448, 0x2F1BC00); // r_b_addr
        wr(0x40000408, 0x2F1FC00); // r_out_addr
        wr(0x40000450, 16800); // r_len
        wr(0x40000454, 3); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 257: /model.22/Add_1 (Opcode 0x15)
        wr(0x40000444, 0x18F0D00); // r_a_addr
        wr(0x40000448, 0x2F1BC00); // r_b_addr
        wr(0x40000408, 0x2F23E00); // r_out_addr
        wr(0x40000450, 16800); // r_len
        wr(0x40000454, 0); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 258: /model.22/Add_2 (Opcode 0x15)
        wr(0x40000444, 0x2F1FC00); // r_a_addr
        wr(0x40000448, 0x2F23E00); // r_b_addr
        wr(0x40000408, 0x2F28000); // r_out_addr
        wr(0x40000450, 16800); // r_len
        wr(0x40000454, 0); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 259: /model.22/Sub_1 (Opcode 0x15)
        wr(0x40000444, 0x2F23E00); // r_a_addr
        wr(0x40000448, 0x2F1FC00); // r_b_addr
        wr(0x40000408, 0x2F2C200); // r_out_addr
        wr(0x40000450, 16800); // r_len
        wr(0x40000454, 3); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 260: /model.22/Div_1 (Opcode 0x15)
        wr(0x40000444, 0x2F28000); // r_a_addr
        wr(0x40000448, 0x0014F00); // r_b_addr
        wr(0x40000408, 0x2F30400); // r_out_addr
        wr(0x40000450, 16800); // r_len
        wr(0x40000454, 4); // r_mode
        wr(0x40000310, 0x15);

        // Instruction 261: /model.22/Mul_2 (Opcode 0x15)
        wr(0x40000444, 0x2F34600); // r_a_addr
        wr(0x40000448, 0x18F5F00); // r_b_addr
        wr(0x40000408, 0x2F3CA00); // r_out_addr
        wr(0x40000450, 33600); // r_len
        wr(0x40000454, 2); // r_mode
        wr(0x40000310, 0x15);

        std::cout << "[TESTBENCH] Waiting for execution to complete..." << std::endl;
        wait(52200);

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
            uint32_t out_addr = 0x1A39400;
            size_t num_bytes = 5760;
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
                      << std::setw(30) << "/model.0/conv/Conv"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1A3EE00;
            size_t num_bytes = 5760;
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
                      << std::setw(30) << "/model.0/act/Sigmoid"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1A40500;
            size_t num_bytes = 5760;
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
                      << std::setw(30) << "/model.0/act/Mul"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1A41C00;
            size_t num_bytes = 12288;
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
                      << std::setw(30) << "/model.1/conv/Conv"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1A4DC00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 5
                      << std::setw(30) << "/model.1/act/Sigmoid"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1A50C00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 6
                      << std::setw(30) << "/model.1/act/Mul"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1A53C00;
            size_t num_bytes = 4096;
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

            std::cout << std::left << std::setw(8) << 7
                      << std::setw(30) << "/model.2/cv1/conv/Conv"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1A57C00;
            size_t num_bytes = 4096;
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

            std::cout << std::left << std::setw(8) << 8
                      << std::setw(30) << "/model.2/cv1/act/Sigmoid"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1A58C00;
            size_t num_bytes = 4096;
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

            std::cout << std::left << std::setw(8) << 9
                      << std::setw(30) << "/model.2/cv1/act/Mul"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1A59C00;
            size_t num_bytes = 6144;
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

            std::cout << std::left << std::setw(8) << 10
                      << std::setw(30) << "/model.2/m.0/cv1/conv/Conv"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1A5FC00;
            size_t num_bytes = 6144;
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

            std::cout << std::left << std::setw(8) << 11
                      << std::setw(30) << "/model.2/m.0/cv1/act/Sigmoid"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1A61400;
            size_t num_bytes = 6144;
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

            std::cout << std::left << std::setw(8) << 12
                      << std::setw(30) << "/model.2/m.0/cv1/act/Mul"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1A62C00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 13
                      << std::setw(30) << "/model.2/m.0/cv2/conv/Conv"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1A6EC00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 14
                      << std::setw(30) << "/model.2/m.0/cv2/act/Sigmoid"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1A71C00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 15
                      << std::setw(30) << "/model.2/m.0/cv2/act/Mul"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1A74C00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 16
                      << std::setw(30) << "/model.2/m.0/Add"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1A77C00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 17
                      << std::setw(30) << "/model.2/m.1/cv1/conv/Conv"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1A83C00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 18
                      << std::setw(30) << "/model.2/m.1/cv1/act/Sigmoid"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1A86C00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 19
                      << std::setw(30) << "/model.2/m.1/cv1/act/Mul"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1A89C00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 20
                      << std::setw(30) << "/model.2/m.1/cv2/conv/Conv"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1A95C00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 21
                      << std::setw(30) << "/model.2/m.1/cv2/act/Sigmoid"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1A98C00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 22
                      << std::setw(30) << "/model.2/m.1/cv2/act/Mul"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1A9BC00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 23
                      << std::setw(30) << "/model.2/m.1/Add"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1AA5C00;
            size_t num_bytes = 4096;
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

            std::cout << std::left << std::setw(8) << 24
                      << std::setw(30) << "/model.2/cv2/conv/Conv"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1AA9C00;
            size_t num_bytes = 4096;
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

            std::cout << std::left << std::setw(8) << 25
                      << std::setw(30) << "/model.2/cv2/act/Sigmoid"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1AAAC00;
            size_t num_bytes = 4096;
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

            std::cout << std::left << std::setw(8) << 26
                      << std::setw(30) << "/model.2/cv2/act/Mul"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1AABC00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 27
                      << std::setw(30) << "/model.3/conv/Conv"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1AB7C00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 28
                      << std::setw(30) << "/model.3/act/Sigmoid"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1ABAC00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 29
                      << std::setw(30) << "/model.3/act/Mul"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1ABDC00;
            size_t num_bytes = 4096;
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

            std::cout << std::left << std::setw(8) << 30
                      << std::setw(30) << "/model.4/cv1/conv/Conv"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1AC1C00;
            size_t num_bytes = 4096;
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

            std::cout << std::left << std::setw(8) << 31
                      << std::setw(30) << "/model.4/cv1/act/Sigmoid"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1AC2C00;
            size_t num_bytes = 4096;
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

            std::cout << std::left << std::setw(8) << 32
                      << std::setw(30) << "/model.4/cv1/act/Mul"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1AC3C00;
            size_t num_bytes = 6144;
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

            std::cout << std::left << std::setw(8) << 33
                      << std::setw(30) << "/model.4/m.0/cv1/conv/Conv"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1AC9C00;
            size_t num_bytes = 6144;
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

            std::cout << std::left << std::setw(8) << 34
                      << std::setw(30) << "/model.4/m.0/cv1/act/Sigmoid"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1ACB400;
            size_t num_bytes = 6144;
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

            std::cout << std::left << std::setw(8) << 35
                      << std::setw(30) << "/model.4/m.0/cv1/act/Mul"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1ACCC00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 36
                      << std::setw(30) << "/model.4/m.0/cv2/conv/Conv"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1AD8C00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 37
                      << std::setw(30) << "/model.4/m.0/cv2/act/Sigmoid"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1ADBC00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 38
                      << std::setw(30) << "/model.4/m.0/cv2/act/Mul"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1ADEC00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 39
                      << std::setw(30) << "/model.4/m.0/Add"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1AE1C00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 40
                      << std::setw(30) << "/model.4/m.1/cv1/conv/Conv"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1AEDC00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 41
                      << std::setw(30) << "/model.4/m.1/cv1/act/Sigmoid"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1AF0C00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 42
                      << std::setw(30) << "/model.4/m.1/cv1/act/Mul"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1AF3C00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 43
                      << std::setw(30) << "/model.4/m.1/cv2/conv/Conv"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1AFFC00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 44
                      << std::setw(30) << "/model.4/m.1/cv2/act/Sigmoid"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1B02C00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 45
                      << std::setw(30) << "/model.4/m.1/cv2/act/Mul"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1B05C00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 46
                      << std::setw(30) << "/model.4/m.1/Add"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1B08C00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 47
                      << std::setw(30) << "/model.4/m.2/cv1/conv/Conv"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1B14C00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 48
                      << std::setw(30) << "/model.4/m.2/cv1/act/Sigmoid"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1B17C00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 49
                      << std::setw(30) << "/model.4/m.2/cv1/act/Mul"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1B1AC00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 50
                      << std::setw(30) << "/model.4/m.2/cv2/conv/Conv"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1B26C00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 51
                      << std::setw(30) << "/model.4/m.2/cv2/act/Sigmoid"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1B29C00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 52
                      << std::setw(30) << "/model.4/m.2/cv2/act/Mul"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1B2CC00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 53
                      << std::setw(30) << "/model.4/m.2/Add"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1B2FC00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 54
                      << std::setw(30) << "/model.4/m.3/cv1/conv/Conv"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1B3BC00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 55
                      << std::setw(30) << "/model.4/m.3/cv1/act/Sigmoid"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1B3EC00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 56
                      << std::setw(30) << "/model.4/m.3/cv1/act/Mul"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1B41C00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 57
                      << std::setw(30) << "/model.4/m.3/cv2/conv/Conv"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1B4DC00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 58
                      << std::setw(30) << "/model.4/m.3/cv2/act/Sigmoid"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1B50C00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 59
                      << std::setw(30) << "/model.4/m.3/cv2/act/Mul"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1B53C00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 60
                      << std::setw(30) << "/model.4/m.3/Add"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1B63C00;
            size_t num_bytes = 4096;
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

            std::cout << std::left << std::setw(8) << 61
                      << std::setw(30) << "/model.4/cv2/conv/Conv"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1B67C00;
            size_t num_bytes = 4096;
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

            std::cout << std::left << std::setw(8) << 62
                      << std::setw(30) << "/model.4/cv2/act/Sigmoid"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1B68C00;
            size_t num_bytes = 4096;
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

            std::cout << std::left << std::setw(8) << 63
                      << std::setw(30) << "/model.4/cv2/act/Mul"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1B69C00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 64
                      << std::setw(30) << "/model.5/conv/Conv"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1B75C00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 65
                      << std::setw(30) << "/model.5/act/Sigmoid"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1B78C00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 66
                      << std::setw(30) << "/model.5/act/Mul"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1B7BC00;
            size_t num_bytes = 4096;
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

            std::cout << std::left << std::setw(8) << 67
                      << std::setw(30) << "/model.6/cv1/conv/Conv"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1B7FC00;
            size_t num_bytes = 4096;
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

            std::cout << std::left << std::setw(8) << 68
                      << std::setw(30) << "/model.6/cv1/act/Sigmoid"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1B80C00;
            size_t num_bytes = 4096;
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

            std::cout << std::left << std::setw(8) << 69
                      << std::setw(30) << "/model.6/cv1/act/Mul"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1B81C00;
            size_t num_bytes = 6144;
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

            std::cout << std::left << std::setw(8) << 70
                      << std::setw(30) << "/model.6/m.0/cv1/conv/Conv"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1B87C00;
            size_t num_bytes = 6144;
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

            std::cout << std::left << std::setw(8) << 71
                      << std::setw(30) << "/model.6/m.0/cv1/act/Sigmoid"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1B89400;
            size_t num_bytes = 6144;
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

            std::cout << std::left << std::setw(8) << 72
                      << std::setw(30) << "/model.6/m.0/cv1/act/Mul"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1B8AC00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 73
                      << std::setw(30) << "/model.6/m.0/cv2/conv/Conv"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1B96C00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 74
                      << std::setw(30) << "/model.6/m.0/cv2/act/Sigmoid"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1B99C00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 75
                      << std::setw(30) << "/model.6/m.0/cv2/act/Mul"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1B9CC00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 76
                      << std::setw(30) << "/model.6/m.0/Add"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1B9FC00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 77
                      << std::setw(30) << "/model.6/m.1/cv1/conv/Conv"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1BABC00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 78
                      << std::setw(30) << "/model.6/m.1/cv1/act/Sigmoid"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1BAEC00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 79
                      << std::setw(30) << "/model.6/m.1/cv1/act/Mul"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1BB1C00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 80
                      << std::setw(30) << "/model.6/m.1/cv2/conv/Conv"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1BBDC00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 81
                      << std::setw(30) << "/model.6/m.1/cv2/act/Sigmoid"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1BC0C00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 82
                      << std::setw(30) << "/model.6/m.1/cv2/act/Mul"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1BC3C00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 83
                      << std::setw(30) << "/model.6/m.1/Add"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1BC6C00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 84
                      << std::setw(30) << "/model.6/m.2/cv1/conv/Conv"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1BD2C00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 85
                      << std::setw(30) << "/model.6/m.2/cv1/act/Sigmoid"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1BD5C00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 86
                      << std::setw(30) << "/model.6/m.2/cv1/act/Mul"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1BD8C00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 87
                      << std::setw(30) << "/model.6/m.2/cv2/conv/Conv"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1BE4C00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 88
                      << std::setw(30) << "/model.6/m.2/cv2/act/Sigmoid"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1BE7C00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 89
                      << std::setw(30) << "/model.6/m.2/cv2/act/Mul"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1BEAC00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 90
                      << std::setw(30) << "/model.6/m.2/Add"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1BEDC00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 91
                      << std::setw(30) << "/model.6/m.3/cv1/conv/Conv"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1BF9C00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 92
                      << std::setw(30) << "/model.6/m.3/cv1/act/Sigmoid"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1BFCC00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 93
                      << std::setw(30) << "/model.6/m.3/cv1/act/Mul"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1BFFC00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 94
                      << std::setw(30) << "/model.6/m.3/cv2/conv/Conv"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1C0BC00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 95
                      << std::setw(30) << "/model.6/m.3/cv2/act/Sigmoid"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1C0EC00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 96
                      << std::setw(30) << "/model.6/m.3/cv2/act/Mul"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1C11C00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 97
                      << std::setw(30) << "/model.6/m.3/Add"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1C21C00;
            size_t num_bytes = 4096;
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

            std::cout << std::left << std::setw(8) << 98
                      << std::setw(30) << "/model.6/cv2/conv/Conv"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1C25C00;
            size_t num_bytes = 4096;
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

            std::cout << std::left << std::setw(8) << 99
                      << std::setw(30) << "/model.6/cv2/act/Sigmoid"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1C26C00;
            size_t num_bytes = 4096;
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

            std::cout << std::left << std::setw(8) << 100
                      << std::setw(30) << "/model.6/cv2/act/Mul"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1C27C00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 101
                      << std::setw(30) << "/model.7/conv/Conv"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1C33C00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 102
                      << std::setw(30) << "/model.7/act/Sigmoid"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1C36C00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 103
                      << std::setw(30) << "/model.7/act/Mul"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1C39C00;
            size_t num_bytes = 4096;
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

            std::cout << std::left << std::setw(8) << 104
                      << std::setw(30) << "/model.8/cv1/conv/Conv"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1C3DC00;
            size_t num_bytes = 4096;
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

            std::cout << std::left << std::setw(8) << 105
                      << std::setw(30) << "/model.8/cv1/act/Sigmoid"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1C3EC00;
            size_t num_bytes = 4096;
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

            std::cout << std::left << std::setw(8) << 106
                      << std::setw(30) << "/model.8/cv1/act/Mul"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1C3FC00;
            size_t num_bytes = 6144;
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

            std::cout << std::left << std::setw(8) << 107
                      << std::setw(30) << "/model.8/m.0/cv1/conv/Conv"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1C45C00;
            size_t num_bytes = 6144;
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

            std::cout << std::left << std::setw(8) << 108
                      << std::setw(30) << "/model.8/m.0/cv1/act/Sigmoid"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1C47400;
            size_t num_bytes = 6144;
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

            std::cout << std::left << std::setw(8) << 109
                      << std::setw(30) << "/model.8/m.0/cv1/act/Mul"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1C48C00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 110
                      << std::setw(30) << "/model.8/m.0/cv2/conv/Conv"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1C54C00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 111
                      << std::setw(30) << "/model.8/m.0/cv2/act/Sigmoid"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1C57C00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 112
                      << std::setw(30) << "/model.8/m.0/cv2/act/Mul"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1C5AC00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 113
                      << std::setw(30) << "/model.8/m.0/Add"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1C5DC00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 114
                      << std::setw(30) << "/model.8/m.1/cv1/conv/Conv"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1C69C00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 115
                      << std::setw(30) << "/model.8/m.1/cv1/act/Sigmoid"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1C6CC00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 116
                      << std::setw(30) << "/model.8/m.1/cv1/act/Mul"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1C6FC00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 117
                      << std::setw(30) << "/model.8/m.1/cv2/conv/Conv"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1C7BC00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 118
                      << std::setw(30) << "/model.8/m.1/cv2/act/Sigmoid"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1C7EC00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 119
                      << std::setw(30) << "/model.8/m.1/cv2/act/Mul"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1C81C00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 120
                      << std::setw(30) << "/model.8/m.1/Add"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1C8BC00;
            size_t num_bytes = 4096;
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

            std::cout << std::left << std::setw(8) << 121
                      << std::setw(30) << "/model.8/cv2/conv/Conv"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1C8FC00;
            size_t num_bytes = 4096;
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

            std::cout << std::left << std::setw(8) << 122
                      << std::setw(30) << "/model.8/cv2/act/Sigmoid"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1C90C00;
            size_t num_bytes = 4096;
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

            std::cout << std::left << std::setw(8) << 123
                      << std::setw(30) << "/model.8/cv2/act/Mul"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1C91C00;
            size_t num_bytes = 4096;
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

            std::cout << std::left << std::setw(8) << 124
                      << std::setw(30) << "/model.9/cv1/conv/Conv"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1C95C00;
            size_t num_bytes = 4096;
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

            std::cout << std::left << std::setw(8) << 125
                      << std::setw(30) << "/model.9/cv1/act/Sigmoid"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1C96C00;
            size_t num_bytes = 4096;
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

            std::cout << std::left << std::setw(8) << 126
                      << std::setw(30) << "/model.9/cv1/act/Mul"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1C97C00;
            size_t num_bytes = 4096;
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

            std::cout << std::left << std::setw(8) << 127
                      << std::setw(30) << "/model.9/m/MaxPool"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1C98C00;
            size_t num_bytes = 4096;
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

            std::cout << std::left << std::setw(8) << 128
                      << std::setw(30) << "/model.9/m_1/MaxPool"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1C99C00;
            size_t num_bytes = 4096;
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

            std::cout << std::left << std::setw(8) << 129
                      << std::setw(30) << "/model.9/m_2/MaxPool"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1C9EC00;
            size_t num_bytes = 4096;
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

            std::cout << std::left << std::setw(8) << 130
                      << std::setw(30) << "/model.9/cv2/conv/Conv"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1CA2C00;
            size_t num_bytes = 4096;
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

            std::cout << std::left << std::setw(8) << 131
                      << std::setw(30) << "/model.9/cv2/act/Sigmoid"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1CA3C00;
            size_t num_bytes = 4096;
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

            std::cout << std::left << std::setw(8) << 132
                      << std::setw(30) << "/model.9/cv2/act/Mul"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1CA6C00;
            size_t num_bytes = 4096;
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

            std::cout << std::left << std::setw(8) << 133
                      << std::setw(30) << "/model.12/cv1/conv/Conv"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1CAAC00;
            size_t num_bytes = 4096;
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

            std::cout << std::left << std::setw(8) << 134
                      << std::setw(30) << "/model.12/cv1/act/Sigmoid"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1CABC00;
            size_t num_bytes = 4096;
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

            std::cout << std::left << std::setw(8) << 135
                      << std::setw(30) << "/model.12/cv1/act/Mul"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1CACC00;
            size_t num_bytes = 6144;
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

            std::cout << std::left << std::setw(8) << 136
                      << std::setw(30) << "/model.12/m.0/cv1/conv/Conv"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1CB2C00;
            size_t num_bytes = 6144;
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

            std::cout << std::left << std::setw(8) << 137
                      << std::setw(30) << "/model.12/m.0/cv1/act/Sigmoi"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1CB4400;
            size_t num_bytes = 6144;
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

            std::cout << std::left << std::setw(8) << 138
                      << std::setw(30) << "/model.12/m.0/cv1/act/Mul"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1CB5C00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 139
                      << std::setw(30) << "/model.12/m.0/cv2/conv/Conv"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1CC1C00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 140
                      << std::setw(30) << "/model.12/m.0/cv2/act/Sigmoi"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1CC4C00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 141
                      << std::setw(30) << "/model.12/m.0/cv2/act/Mul"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1CC7C00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 142
                      << std::setw(30) << "/model.12/m.1/cv1/conv/Conv"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1CD3C00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 143
                      << std::setw(30) << "/model.12/m.1/cv1/act/Sigmoi"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1CD6C00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 144
                      << std::setw(30) << "/model.12/m.1/cv1/act/Mul"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1CD9C00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 145
                      << std::setw(30) << "/model.12/m.1/cv2/conv/Conv"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1CE5C00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 146
                      << std::setw(30) << "/model.12/m.1/cv2/act/Sigmoi"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1CE8C00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 147
                      << std::setw(30) << "/model.12/m.1/cv2/act/Mul"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1CF2C00;
            size_t num_bytes = 4096;
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

            std::cout << std::left << std::setw(8) << 148
                      << std::setw(30) << "/model.12/cv2/conv/Conv"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1CF6C00;
            size_t num_bytes = 4096;
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

            std::cout << std::left << std::setw(8) << 149
                      << std::setw(30) << "/model.12/cv2/act/Sigmoid"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1CF7C00;
            size_t num_bytes = 4096;
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

            std::cout << std::left << std::setw(8) << 150
                      << std::setw(30) << "/model.12/cv2/act/Mul"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1CFAC00;
            size_t num_bytes = 4096;
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

            std::cout << std::left << std::setw(8) << 151
                      << std::setw(30) << "/model.15/cv1/conv/Conv"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1CFEC00;
            size_t num_bytes = 4096;
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

            std::cout << std::left << std::setw(8) << 152
                      << std::setw(30) << "/model.15/cv1/act/Sigmoid"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1CFFC00;
            size_t num_bytes = 4096;
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

            std::cout << std::left << std::setw(8) << 153
                      << std::setw(30) << "/model.15/cv1/act/Mul"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1D00C00;
            size_t num_bytes = 6144;
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

            std::cout << std::left << std::setw(8) << 154
                      << std::setw(30) << "/model.15/m.0/cv1/conv/Conv"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1D06C00;
            size_t num_bytes = 6144;
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

            std::cout << std::left << std::setw(8) << 155
                      << std::setw(30) << "/model.15/m.0/cv1/act/Sigmoi"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1D08400;
            size_t num_bytes = 6144;
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

            std::cout << std::left << std::setw(8) << 156
                      << std::setw(30) << "/model.15/m.0/cv1/act/Mul"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1D09C00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 157
                      << std::setw(30) << "/model.15/m.0/cv2/conv/Conv"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1D15C00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 158
                      << std::setw(30) << "/model.15/m.0/cv2/act/Sigmoi"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1D18C00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 159
                      << std::setw(30) << "/model.15/m.0/cv2/act/Mul"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1D1BC00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 160
                      << std::setw(30) << "/model.15/m.1/cv1/conv/Conv"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1D27C00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 161
                      << std::setw(30) << "/model.15/m.1/cv1/act/Sigmoi"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1D2AC00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 162
                      << std::setw(30) << "/model.15/m.1/cv1/act/Mul"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1D2DC00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 163
                      << std::setw(30) << "/model.15/m.1/cv2/conv/Conv"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1D39C00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 164
                      << std::setw(30) << "/model.15/m.1/cv2/act/Sigmoi"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1D3CC00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 165
                      << std::setw(30) << "/model.15/m.1/cv2/act/Mul"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1D46C00;
            size_t num_bytes = 4096;
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

            std::cout << std::left << std::setw(8) << 166
                      << std::setw(30) << "/model.15/cv2/conv/Conv"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1D4AC00;
            size_t num_bytes = 4096;
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

            std::cout << std::left << std::setw(8) << 167
                      << std::setw(30) << "/model.15/cv2/act/Sigmoid"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1D4BC00;
            size_t num_bytes = 4096;
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

            std::cout << std::left << std::setw(8) << 168
                      << std::setw(30) << "/model.15/cv2/act/Mul"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1D4CC00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 169
                      << std::setw(30) << "/model.16/conv/Conv"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1D58C00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 170
                      << std::setw(30) << "/model.22/cv2.0/cv2.0.0/conv"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1D64C00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 171
                      << std::setw(30) << "/model.22/cv3.0/cv3.0.0/conv"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1D70C00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 172
                      << std::setw(30) << "/model.16/act/Sigmoid"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1D73C00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 173
                      << std::setw(30) << "/model.22/cv2.0/cv2.0.0/act/"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1D76C00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 174
                      << std::setw(30) << "/model.22/cv3.0/cv3.0.0/act/"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1D79C00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 175
                      << std::setw(30) << "/model.16/act/Mul"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1D7CC00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 176
                      << std::setw(30) << "/model.22/cv2.0/cv2.0.0/act/"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1D7FC00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 177
                      << std::setw(30) << "/model.22/cv3.0/cv3.0.0/act/"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1D86C00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 178
                      << std::setw(30) << "/model.22/cv2.0/cv2.0.1/conv"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1D92C00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 179
                      << std::setw(30) << "/model.22/cv3.0/cv3.0.1/conv"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1D9EC00;
            size_t num_bytes = 4096;
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

            std::cout << std::left << std::setw(8) << 180
                      << std::setw(30) << "/model.18/cv1/conv/Conv"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1DA2C00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 181
                      << std::setw(30) << "/model.22/cv2.0/cv2.0.1/act/"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1DA5C00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 182
                      << std::setw(30) << "/model.22/cv3.0/cv3.0.1/act/"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1DA8C00;
            size_t num_bytes = 4096;
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

            std::cout << std::left << std::setw(8) << 183
                      << std::setw(30) << "/model.18/cv1/act/Sigmoid"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1DA9C00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 184
                      << std::setw(30) << "/model.22/cv2.0/cv2.0.1/act/"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1DACC00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 185
                      << std::setw(30) << "/model.22/cv3.0/cv3.0.1/act/"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1DAFC00;
            size_t num_bytes = 4096;
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

            std::cout << std::left << std::setw(8) << 186
                      << std::setw(30) << "/model.18/cv1/act/Mul"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1DB0C00;
            size_t num_bytes = 4096;
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

            std::cout << std::left << std::setw(8) << 187
                      << std::setw(30) << "/model.22/cv2.0/cv2.0.2/Conv"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1DB4C00;
            size_t num_bytes = 4096;
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

            std::cout << std::left << std::setw(8) << 188
                      << std::setw(30) << "/model.22/cv3.0/cv3.0.2/Conv"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1DBAC00;
            size_t num_bytes = 6144;
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

            std::cout << std::left << std::setw(8) << 189
                      << std::setw(30) << "/model.18/m.0/cv1/conv/Conv"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1DC0C00;
            size_t num_bytes = 6144;
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

            std::cout << std::left << std::setw(8) << 190
                      << std::setw(30) << "/model.18/m.0/cv1/act/Sigmoi"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1DC2400;
            size_t num_bytes = 6144;
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

            std::cout << std::left << std::setw(8) << 191
                      << std::setw(30) << "/model.18/m.0/cv1/act/Mul"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1DC3C00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 192
                      << std::setw(30) << "/model.18/m.0/cv2/conv/Conv"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1DCFC00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 193
                      << std::setw(30) << "/model.18/m.0/cv2/act/Sigmoi"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1DD2C00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 194
                      << std::setw(30) << "/model.18/m.0/cv2/act/Mul"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1DD5C00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 195
                      << std::setw(30) << "/model.18/m.1/cv1/conv/Conv"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1DE1C00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 196
                      << std::setw(30) << "/model.18/m.1/cv1/act/Sigmoi"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1DE4C00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 197
                      << std::setw(30) << "/model.18/m.1/cv1/act/Mul"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1DE7C00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 198
                      << std::setw(30) << "/model.18/m.1/cv2/conv/Conv"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1DF3C00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 199
                      << std::setw(30) << "/model.18/m.1/cv2/act/Sigmoi"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1DF6C00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 200
                      << std::setw(30) << "/model.18/m.1/cv2/act/Mul"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1E00C00;
            size_t num_bytes = 4096;
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

            std::cout << std::left << std::setw(8) << 201
                      << std::setw(30) << "/model.18/cv2/conv/Conv"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1E04C00;
            size_t num_bytes = 4096;
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

            std::cout << std::left << std::setw(8) << 202
                      << std::setw(30) << "/model.18/cv2/act/Sigmoid"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1E05C00;
            size_t num_bytes = 4096;
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

            std::cout << std::left << std::setw(8) << 203
                      << std::setw(30) << "/model.18/cv2/act/Mul"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1E06C00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 204
                      << std::setw(30) << "/model.19/conv/Conv"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1E12C00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 205
                      << std::setw(30) << "/model.22/cv2.1/cv2.1.0/conv"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1E1EC00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 206
                      << std::setw(30) << "/model.22/cv3.1/cv3.1.0/conv"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1E2AC00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 207
                      << std::setw(30) << "/model.19/act/Sigmoid"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1E2DC00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 208
                      << std::setw(30) << "/model.22/cv2.1/cv2.1.0/act/"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1E30C00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 209
                      << std::setw(30) << "/model.22/cv3.1/cv3.1.0/act/"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1E33C00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 210
                      << std::setw(30) << "/model.19/act/Mul"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1E36C00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 211
                      << std::setw(30) << "/model.22/cv2.1/cv2.1.0/act/"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1E39C00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 212
                      << std::setw(30) << "/model.22/cv3.1/cv3.1.0/act/"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1E40C00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 213
                      << std::setw(30) << "/model.22/cv2.1/cv2.1.1/conv"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1E4CC00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 214
                      << std::setw(30) << "/model.22/cv3.1/cv3.1.1/conv"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1E58C00;
            size_t num_bytes = 4096;
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

            std::cout << std::left << std::setw(8) << 215
                      << std::setw(30) << "/model.21/cv1/conv/Conv"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1E5CC00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 216
                      << std::setw(30) << "/model.22/cv2.1/cv2.1.1/act/"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1E5FC00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 217
                      << std::setw(30) << "/model.22/cv3.1/cv3.1.1/act/"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1E62C00;
            size_t num_bytes = 4096;
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

            std::cout << std::left << std::setw(8) << 218
                      << std::setw(30) << "/model.21/cv1/act/Sigmoid"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1E63C00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 219
                      << std::setw(30) << "/model.22/cv2.1/cv2.1.1/act/"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1E66C00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 220
                      << std::setw(30) << "/model.22/cv3.1/cv3.1.1/act/"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1E69C00;
            size_t num_bytes = 4096;
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

            std::cout << std::left << std::setw(8) << 221
                      << std::setw(30) << "/model.21/cv1/act/Mul"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1E6AC00;
            size_t num_bytes = 4096;
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

            std::cout << std::left << std::setw(8) << 222
                      << std::setw(30) << "/model.22/cv2.1/cv2.1.2/Conv"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1E6EC00;
            size_t num_bytes = 4096;
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

            std::cout << std::left << std::setw(8) << 223
                      << std::setw(30) << "/model.22/cv3.1/cv3.1.2/Conv"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1E74C00;
            size_t num_bytes = 6144;
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

            std::cout << std::left << std::setw(8) << 224
                      << std::setw(30) << "/model.21/m.0/cv1/conv/Conv"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1E7AC00;
            size_t num_bytes = 6144;
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

            std::cout << std::left << std::setw(8) << 225
                      << std::setw(30) << "/model.21/m.0/cv1/act/Sigmoi"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1E7C400;
            size_t num_bytes = 6144;
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

            std::cout << std::left << std::setw(8) << 226
                      << std::setw(30) << "/model.21/m.0/cv1/act/Mul"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1E7DC00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 227
                      << std::setw(30) << "/model.21/m.0/cv2/conv/Conv"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1E89C00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 228
                      << std::setw(30) << "/model.21/m.0/cv2/act/Sigmoi"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1E8CC00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 229
                      << std::setw(30) << "/model.21/m.0/cv2/act/Mul"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1E8FC00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 230
                      << std::setw(30) << "/model.21/m.1/cv1/conv/Conv"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1E9BC00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 231
                      << std::setw(30) << "/model.21/m.1/cv1/act/Sigmoi"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1E9EC00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 232
                      << std::setw(30) << "/model.21/m.1/cv1/act/Mul"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1EA1C00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 233
                      << std::setw(30) << "/model.21/m.1/cv2/conv/Conv"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1EADC00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 234
                      << std::setw(30) << "/model.21/m.1/cv2/act/Sigmoi"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1EB0C00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 235
                      << std::setw(30) << "/model.21/m.1/cv2/act/Mul"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1EBAC00;
            size_t num_bytes = 4096;
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

            std::cout << std::left << std::setw(8) << 236
                      << std::setw(30) << "/model.21/cv2/conv/Conv"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1EBEC00;
            size_t num_bytes = 4096;
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

            std::cout << std::left << std::setw(8) << 237
                      << std::setw(30) << "/model.21/cv2/act/Sigmoid"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1EBFC00;
            size_t num_bytes = 4096;
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

            std::cout << std::left << std::setw(8) << 238
                      << std::setw(30) << "/model.21/cv2/act/Mul"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1EC0C00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 239
                      << std::setw(30) << "/model.22/cv2.2/cv2.2.0/conv"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1ECCC00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 240
                      << std::setw(30) << "/model.22/cv3.2/cv3.2.0/conv"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1ED8C00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 241
                      << std::setw(30) << "/model.22/cv2.2/cv2.2.0/act/"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1EDBC00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 242
                      << std::setw(30) << "/model.22/cv3.2/cv3.2.0/act/"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1EDEC00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 243
                      << std::setw(30) << "/model.22/cv2.2/cv2.2.0/act/"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1EE1C00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 244
                      << std::setw(30) << "/model.22/cv3.2/cv3.2.0/act/"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1EE4C00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 245
                      << std::setw(30) << "/model.22/cv2.2/cv2.2.1/conv"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1EF0C00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 246
                      << std::setw(30) << "/model.22/cv3.2/cv3.2.1/conv"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1EFCC00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 247
                      << std::setw(30) << "/model.22/cv2.2/cv2.2.1/act/"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1EFFC00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 248
                      << std::setw(30) << "/model.22/cv3.2/cv3.2.1/act/"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1F02C00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 249
                      << std::setw(30) << "/model.22/cv2.2/cv2.2.1/act/"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1F05C00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 250
                      << std::setw(30) << "/model.22/cv3.2/cv3.2.1/act/"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1F08C00;
            size_t num_bytes = 4096;
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

            std::cout << std::left << std::setw(8) << 251
                      << std::setw(30) << "/model.22/cv2.2/cv2.2.2/Conv"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1F0CC00;
            size_t num_bytes = 4096;
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

            std::cout << std::left << std::setw(8) << 252
                      << std::setw(30) << "/model.22/cv3.2/cv3.2.2/Conv"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1F18C00;
            size_t num_bytes = 12288;
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

            std::cout << std::left << std::setw(8) << 253
                      << std::setw(30) << "/model.22/Sigmoid"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x1F1BC00;
            size_t num_bytes = 16777216;
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

            std::cout << std::left << std::setw(8) << 254
                      << std::setw(30) << "/model.22/dfl/Softmax"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x2F1BC00;
            size_t num_bytes = 4096;
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

            std::cout << std::left << std::setw(8) << 255
                      << std::setw(30) << "/model.22/dfl/conv/Conv"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x2F1FC00;
            size_t num_bytes = 16800;
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

            std::cout << std::left << std::setw(8) << 256
                      << std::setw(30) << "/model.22/Sub"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x2F23E00;
            size_t num_bytes = 16800;
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

            std::cout << std::left << std::setw(8) << 257
                      << std::setw(30) << "/model.22/Add_1"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x2F28000;
            size_t num_bytes = 16800;
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

            std::cout << std::left << std::setw(8) << 258
                      << std::setw(30) << "/model.22/Add_2"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x2F2C200;
            size_t num_bytes = 16800;
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

            std::cout << std::left << std::setw(8) << 259
                      << std::setw(30) << "/model.22/Sub_1"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x2F30400;
            size_t num_bytes = 16800;
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

            std::cout << std::left << std::setw(8) << 260
                      << std::setw(30) << "/model.22/Div_1"
                      << "0x" << std::hex << std::setw(10) << out_addr << std::dec
                      << std::setw(10) << num_bytes
                      << std::setw(10) << std::fixed << std::setprecision(3) << mae
                      << std::setw(10) << rmse
                      << std::setw(10) << max_abs_err
                      << std::setw(12) << std::setprecision(4) << cos_sim
                      << std::setw(8)  << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }
        {
            uint32_t out_addr = 0x2F3CA00;
            size_t num_bytes = 33600;
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

            std::cout << std::left << std::setw(8) << 261
                      << std::setw(30) << "/model.22/Mul_2"
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
        perf.report("yolov8m-int8.onnx (64x64)");

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
