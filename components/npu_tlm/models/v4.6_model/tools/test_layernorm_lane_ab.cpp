#include <systemc.h>
#include <iostream>
#include <vector>
#include <cmath>
#include <iomanip>
#include "npu_top.h"

using namespace sauria;

SC_MODULE(TbLayerNormLane)
{
    sc_in<bool> i_clk;
    sc_signal<bool> rstn{"rstn"};
    sc_signal<bool> soft_reset{"soft_reset"};
    sc_signal<bool> start{"start"};
    sc_signal<bool> done{"done"};
    sc_signal<bool> deadlock{"deadlock"};
    sc_signal<uint32_t> mvm_k{"mvm_k"};
    sc_signal<uint32_t> total_contexts{"total_contexts"};
    sc_signal<float> threshold{"threshold"};
    sc_signal<sc_bv<3>> select{"select"};

    sc_signal<uint32_t> host_addr{"host_addr"};
    sc_signal<bool> host_wren{"host_wren"};
    sc_signal<bool> host_rden{"host_rden"};
    sc_signal<host_data_t> host_wdata{"host_wdata"};
    sc_signal<host_mask_t> host_wmask{"host_wmask"};
    sc_signal<host_data_t> host_rdata{"host_rdata"};

    using NpuInt8T = NpuTop<64, 64, int8_t, int8_t, int32_t, 16, 128, 1>;
    NpuInt8T *dut;
    std::vector<uint8_t> dram;

    SC_CTOR(TbLayerNormLane)
    {
        dut = new NpuInt8T("dut");
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
        dram.resize(16 * 1024 * 1024, 0); // 16 MB DRAM buffer
        dut->set_dram(&dram);

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

        // Fill DRAM input with user's test pattern (8 rows x 32 cols)
        uint32_t in_addr = 0x10000;
        uint32_t gamma_addr = 0x20000;
        uint32_t beta_addr = 0x21000;
        uint32_t out_a_addr = 0x30000;
        uint32_t out_b_addr = 0x40000;

        int8_t input_matrix[8][32] = {
            {-30, -27, -24, -21, -18, -15, -12,  -9,  -6,  -3,   0,   3,   6,   9,  12,  15,  18,  21,  24,  27,  30, -28, -25, -22, -19, -16, -13, -10,  -7,  -4,  -1,   2},
            {-23, -20, -17, -14, -11,  -8,  -5,  -2,   1,   4,   7,  10,  13,  16,  19,  22,  25,  28, -30, -27, -24, -21, -18, -15, -12,  -9,  -6,  -3,   0,   3,   6,   9},
            {-16, -13, -10,  -7,  -4,  -1,   2,   5,   8,  11,  14,  17,  20,  23,  26,  29, -29, -26, -23, -20, -17, -14, -11,  -8,  -5,  -2,   1,   4,   7,  10,  13,  16},
            { -9,  -6,  -3,   0,   3,   6,   9,  12,  15,  18,  21,  24,  27,  30, -28, -25, -22, -19, -16, -13, -10,  -7,  -4,  -1,   2,   5,   8,  11,  14,  17,  20,  23},
            { -2,   1,   4,   7,  10,  13,  16,  19,  22,  25,  28, -30, -27, -24, -21, -18, -15, -12,  -9,  -6,  -3,   0,   3,   6,   9,  12,  15,  18,  21,  24,  27,  30},
            {  5,   8,  11,  14,  17,  20,  23,  26,  29, -29, -26, -23, -20, -17, -14, -11,  -8,  -5,  -2,   1,   4,   7,  10,  13,  16,  19,  22,  25,  28, -30, -27, -24},
            { 12,  15,  18,  21,  24,  27,  30, -28, -25, -22, -19, -16, -13, -10,  -7,  -4,  -1,   2,   5,   8,  11,  14,  17,  20,  23,  26,  29, -29, -26, -23, -20, -17},
            { 19,  22,  25,  28, -30, -27, -24, -21, -18, -15, -12,  -9,  -6,  -3,   0,   3,   6,   9,  12,  15,  18,  21,  24,  27,  30, -28, -25, -22, -19, -16, -13, -10}
        };

        for (int r = 0; r < 8; r++) {
            for (int c = 0; c < 32; c++) {
                dram[in_addr + r * 32 + c] = static_cast<uint8_t>(input_matrix[r][c]);
            }
        }
        for (int c = 0; c < 32; c++) {
            dram[gamma_addr + c] = 1; // gamma = 1
            dram[beta_addr + c] = 0;  // beta = 0
        }

        // Test 1: Submit LAYERNORM to Lane A
        std::cout << "\n[TEST] Executing LAYERNORM on Lane A..." << std::endl;
        wr(0x40000400, in_addr);
        wr(0x40000408, out_a_addr);
        wr(0x40000444, gamma_addr);
        wr(0x4000044C, beta_addr);
        wr(0x40000450, 8);   // seq_len = 8
        wr(0x40000454, 32);  // dim = 32
        wr(0x40000310, 0x14); // PUSH QUEUE A
        wait(200);

        // Test 2: Submit LAYERNORM to Lane B
        std::cout << "\n[TEST] Executing LAYERNORM on Lane B..." << std::endl;
        wr(0x40000400, in_addr);
        wr(0x40000408, out_b_addr);
        wr(0x40000444, gamma_addr);
        wr(0x4000044C, beta_addr);
        wr(0x40000450, 8);   // seq_len = 8
        wr(0x40000454, 32);  // dim = 32
        wr(0x40000314, 0x14); // PUSH QUEUE B
        wait(200);

        // Verify Output Lane A vs Lane B
        int mismatches = 0;
        std::cout << "\n--- LANE A vs LANE B OUTPUT COMPARISON ---" << std::endl;
        for (int r = 0; r < 8; r++) {
            std::cout << "r" << r << " Lane A: ";
            for (int c = 0; c < 32; c++) {
                int8_t val_a = static_cast<int8_t>(dram[out_a_addr + r * 32 + c]);
                std::cout << std::setw(4) << static_cast<int>(val_a);
            }
            std::cout << "\nr" << r << " Lane B: ";
            for (int c = 0; c < 32; c++) {
                int8_t val_b = static_cast<int8_t>(dram[out_b_addr + r * 32 + c]);
                std::cout << std::setw(4) << static_cast<int>(val_b);
                int8_t val_a = static_cast<int8_t>(dram[out_a_addr + r * 32 + c]);
                if (val_a != val_b) mismatches++;
            }
            std::cout << "\n";
        }

        std::cout << "\n[RESULT] Lane A vs Lane B Mismatch Count: " << mismatches << "/256" << std::endl;
        if (mismatches == 0) {
            std::cout << "[PASS] LAYERNORM ON LANE A AND LANE B MATCH 100%!" << std::endl;
        } else {
            std::cout << "[FAIL] MISMATCH DETECTED!" << std::endl;
        }

        sc_stop();
    }
};

int sc_main(int argc, char **argv)
{
    sc_clock clk("clk", 1.25, SC_NS);
    TbLayerNormLane tb("tb");
    tb.i_clk(clk);
    sc_start();
    return 0;
}
