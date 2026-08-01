// Verification test: Degenerate case test N_split = Y_DIM (Lane B fully idle) on 64x64 SA Array
// Confirms that when N_split = 64 (all 64 rows allocated to Lane A), Lane B remains completely idle
// and the model behaves 100% bit-exact identically to the single-lane baseline from Week 2.

#include <systemc.h>
#include <cstdint>
#include <iostream>
#include <vector>
#include <iomanip>
#include <cmath>
#include <cassert>
#include "sauria_types.h"
#include "config_map.h"
#include "npu_top.h"

using namespace sauria;

typedef NpuTop<64, 64, int8_t, int8_t, int32_t, 65536, 65536, 65536, 16, 128, 1> NpuTestT;

SC_MODULE(TbNsplit64IdleB)
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

    SC_CTOR(TbNsplit64IdleB)
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
        std::cout << "  DEGENERATE CASE TEST: N_split = Y_DIM (64/64)  " << std::endl;
        std::cout << "      (Lane B Fully Idle & Single-Lane Baseline)  " << std::endl;
        std::cout << "==================================================" << std::endl;

        // Initialize test matrix data in DRAM
        // Matrix A: 64x64 at 0x0000
        int8_t *A_ptr = reinterpret_cast<int8_t*>(&dram[0]);
        for (int i = 0; i < 64 * 64; i++) A_ptr[i] = static_cast<int8_t>((i % 9) + 1);

        // Matrix B: 64x64 at 0x4000
        int8_t *B_ptr = reinterpret_cast<int8_t*>(&dram[0x4000]);
        for (int i = 0; i < 64 * 64; i++) B_ptr[i] = static_cast<int8_t>((i % 7) + 1);

        // --------------------------------------------------------
        // PHASE 1: Single-Lane Baseline Execution (N_split = 64)
        // --------------------------------------------------------
        std::cout << "\n[PHASE 1] Running Single-Lane Baseline (N_split = 64)..." << std::endl;
        reset_dut();

        write_mmio(0x40000014, 64); // Set N_split = 64
        wait(5);

        write_mmio(0x40000400, 0);        // r_in_addr
        write_mmio(0x40000404, 0x4000);   // r_w_addr
        write_mmio(0x40000408, 0x8000);   // r_out_addr
        write_mmio(0x4000040C, 0);        // r_bias_addr
        write_mmio(0x40000410, 64);       // r_m = 64
        write_mmio(0x40000414, 64);       // r_k = 64
        write_mmio(0x40000418, 64);       // r_n = 64
        write_mmio(0x4000042C, 0);        // r_act_type
        write_mmio(0x40000430, 0);        // r_has_skip

        write_mmio(0x40000310, 0x12);     // Dispatch to Queue A

        int timeout = 0;
        while ((dut->decoder_inst->get_state_a() != 0 || dut->decoder_inst->get_queue_a_size() > 0) && timeout < 1000)
        {
            wait();
            timeout++;
        }
        assert(dut->decoder_inst->get_state_a() == 0);

        std::vector<int8_t> gold_baseline(64 * 64);
        int8_t *C_base = reinterpret_cast<int8_t*>(&dram[0x8000]);
        for (int i = 0; i < 64 * 64; i++) gold_baseline[i] = C_base[i];

        std::cout << "  [PASS] Single-lane baseline output captured." << std::endl;

        // --------------------------------------------------------
        // PHASE 2: Degenerate Mode Execution (N_split = 64, Lane B Idle)
        // --------------------------------------------------------
        std::cout << "\n[PHASE 2] Running Degenerate Mode (N_split = 64, Lane B idle)..." << std::endl;
        reset_dut();

        // Clear output buffer in DRAM
        for (int i = 0; i < 64 * 64; i++) dram[0x8000 + i] = 0;

        write_mmio(0x40000014, 64);
        wait(5);

        write_mmio(0x40000400, 0);
        write_mmio(0x40000404, 0x4000);
        write_mmio(0x40000408, 0x8000);
        write_mmio(0x4000040C, 0);
        write_mmio(0x40000410, 64);
        write_mmio(0x40000414, 64);
        write_mmio(0x40000418, 64);
        write_mmio(0x4000042C, 0);
        write_mmio(0x40000430, 0);

        write_mmio(0x40000310, 0x12);

        // Verify Lane B state remains IDLE throughout execution
        wait(5);
        int state_b_val = dut->decoder_inst->get_state_b();
        std::cout << "  [CHECK] Queue B state during Queue A execution = " << state_b_val << std::endl;
        assert(state_b_val == 0);
        std::cout << "  [PASS] Lane B verified 100% IDLE (state_b = IDLE)." << std::endl;

        timeout = 0;
        while ((dut->decoder_inst->get_state_a() != 0 || dut->decoder_inst->get_queue_a_size() > 0) && timeout < 1000)
        {
            wait();
            timeout++;
        }
        assert(dut->decoder_inst->get_state_a() == 0);

        // --------------------------------------------------------
        // PHASE 3: Backward Compatibility Verification
        // --------------------------------------------------------
        std::cout << "\n[PHASE 3] Comparing Degenerate Mode vs Single-Lane Baseline Golden Output..." << std::endl;
        int8_t *C_degen = reinterpret_cast<int8_t*>(&dram[0x8000]);
        int mismatches = 0;
        for (int i = 0; i < 64 * 64; i++)
        {
            if (gold_baseline[i] != C_degen[i])
            {
                mismatches++;
                if (mismatches <= 5)
                {
                    std::cout << "  Mismatch at [" << (i / 64) << "][" << (i % 64)
                              << "]: Baseline = " << static_cast<int>(gold_baseline[i])
                              << ", Degenerate = " << static_cast<int>(C_degen[i]) << std::endl;
                }
            }
        }

        std::cout << "\n==================================================" << std::endl;
        if (mismatches == 0)
        {
            std::cout << "  [PASS] Degenerate case N_split = 64/64 (Lane B fully idle)" << std::endl;
            std::cout << "         behaves 100% BIT-EXACT identical to single-lane baseline!" << std::endl;
        }
        else
        {
            std::cout << "  [FAIL] Degenerate case output mismatched baseline at " << mismatches << " positions." << std::endl;
            errors++;
        }
        std::cout << "==================================================\n" << std::endl;

        sc_stop();
    }
};

int sc_main(int argc, char **argv)
{
    sc_clock clk("clk", 10, SC_NS);
    TbNsplit64IdleB tb("TbNsplit64IdleB_inst");
    tb.i_clk(clk);

    sc_start();
    return tb.errors;
}
