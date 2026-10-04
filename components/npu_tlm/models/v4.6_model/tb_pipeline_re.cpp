// Testbench: End-to-End Hardware Pipeline Execution Coverage with Reduction Engine
//
// Proves full hardware pipeline execution through:
// SystolicArray -> PSM -> OBP -> ReductionEngine -> SRAM-C
//
// Pass 1: Bypass Mode (F_RE_MODE_A = 0, RE_MODE_IDLE)
//   - Array computes GEMM, PSM accumulates, OBP scales, data streams directly to SRAM-C.
//   - Read back unreduced tensor Y_obp from SRAM-C.
//
// Pass 2: Live Reduction Mode (F_RE_MODE_A = 5, RE_MODE_MAXPOOL)
//   - Identical activations and weights executed through hardware pipeline.
//   - re_ctrl_mux_logic feeds OBP stream to re_inst_a.
//   - re_inst_a calculates vector max across active rows and broadcasts result.
//   - sramc_wdata_mux_logic selects s_re_vector_out_a to write to SRAM-C.
//   - Read back reduced tensor Y_re from SRAM-C.
//
// Verifies:
//   1. Y_re[y][x] == max_{k}(Y_obp[k][x]) for all active lanes.
//   2. Y_re[y][x] != Y_obp[y][x] for rows below maximum, proving data was modified in-flight by hardware RE.

#define SC_ALLOW_DEPRECATED_IEEE_API

#include <systemc.h>
#include <cstdint>
#include <iostream>
#include <vector>
#include <cassert>
#include <cmath>
#include <iomanip>

#include "sauria_types.h"
#include "npu_profile.h"
#include "npu_top.h"

using namespace sauria;

// Array Geometry: 16 cols x 8 rows, int8 activations/weights, int32 psums
typedef NpuTop<16, 8, int8_t, int8_t, int32_t> NpuT;

SC_MODULE(TbPipelineRe)
{
    sc_in<bool> i_clk;

    sc_signal<bool> rstn, soft_reset, start, done, deadlock;
    sc_signal<uint32_t> mvm_k, host_addr, total_contexts;
    sc_signal<bool> host_wren, host_rden;
    sc_signal<host_data_t> host_wdata, host_rdata;
    sc_signal<host_mask_t> host_wmask;
    sc_signal<float> threshold;
    sc_signal<sc_bv<3>> select;

    NpuT *dut;
    int errors = 0;
    int total_checks = 0;

    static const int X_DIM = 16;
    static const int Y_DIM = 8;
    static const int K_DIM = 8;

    const int subwords_a = Y_DIM / 4; // 8 / 4 = 2
    const int mask_a = subwords_a - 1;
    const int shift_a = 1;

    const int subwords_b = X_DIM / 4; // 16 / 4 = 4
    const int mask_b = subwords_b - 1;
    const int shift_b = 2;

    const int subwords_c = Y_DIM / 4; // 8 / 4 = 2
    const int mask_c = subwords_c - 1;
    const int shift_c = 1;

    uint32_t get_srama_addr(uint32_t phys_addr, uint32_t sub_word)
    {
        return SRAMA_OFFSET | ((phys_addr << shift_a) | (sub_word & mask_a));
    }

    uint32_t get_sramb_addr(uint32_t phys_addr, uint32_t sub_word)
    {
        return SRAMB_OFFSET | ((phys_addr << shift_b) | (sub_word & mask_b));
    }

    uint32_t get_sramc_addr(uint32_t phys_addr, uint32_t sub_word)
    {
        return SRAMC_OFFSET | ((phys_addr << shift_c) | (sub_word & mask_c));
    }

    SC_CTOR(TbPipelineRe)
    {
        dut = new NpuT("dut");
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

    void wr_reg(uint32_t addr, uint32_t val)
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
        wait();
    }

    uint32_t rd_reg(uint32_t addr)
    {
        host_addr.write(addr);
        host_rden.write(true);
        host_wren.write(false);
        wait();
        wait();
        host_data_t r = host_rdata.read();
        host_rden.write(false);
        wait();
        return static_cast<uint32_t>(static_cast<int64_t>(r[0]));
    }

    void wr_sram(uint32_t addr, const host_data_t &data)
    {
        host_mask_t m;
        m.data.fill(true);
        host_addr.write(addr);
        host_wdata.write(data);
        host_wmask.write(m);
        host_wren.write(true);
        host_rden.write(false);
        wait();
        host_wren.write(false);
        wait();
    }

    host_data_t rd_sram(uint32_t addr)
    {
        host_addr.write(addr);
        host_rden.write(true);
        host_wren.write(false);
        wait();
        wait();
        host_data_t r = host_rdata.read();
        host_rden.write(false);
        wait();
        return r;
    }

    void check(const char *tag, bool condition, const std::string &detail = "")
    {
        total_checks++;
        if (!condition)
            errors++;
        std::cout << "  [" << (condition ? "PASS" : "FAIL") << "] " << tag;
        if (!detail.empty())
            std::cout << " (" << detail << ")";
        std::cout << "\n";
    }

    void reset_dut()
    {
        rstn.write(false);
        soft_reset.write(false);
        start.write(false);
        host_wren.write(false);
        host_rden.write(false);
        select.write(sc_bv<3>("000"));
        threshold.write(0.0f);
        mvm_k.write(0);
        total_contexts.write(1);
        wait(5);
        rstn.write(true);
        wait(5);
    }

    void program_hw_config(uint32_t re_mode)
    {
        // Select SAURIA profile
        wr_reg(CFG_PROFILE_ADDR, PROFILE_V1_SAURIA);

        // nsplit = Y_DIM (8) -> Lane A only, done_merge_process asserts o_done on Lane A completion
        wr_reg(CFG_CON_OFFSET + 0x14, Y_DIM);

        // Main Controller parameters (K=8, 1 tile, 1 context)
        wr_reg(CFG_CON_OFFSET + 0x00, K_DIM);    // CON.INCNTLIM = 8
        wr_reg(CFG_CON_OFFSET + 0x04, 1);        // CON.ACT_REPS = 1
        wr_reg(CFG_CON_OFFSET + 0x08, 1);        // CON.WEI_REPS = 1

        // Activation Feeder SAURIA Address Generator parameters (1x1 MVM)
        wr_reg(CFG_ACT_OFFSET + 0x00, 0xFF);     // ROWS_ACTIVE = 0xFF (all 8 rows active)
        wr_reg(CFG_ACT_OFFSET + 0x04, K_DIM);    // ACT.INCNTLIM = 8
        wr_reg(CFG_ACT_OFFSET + 0x08, Y_DIM);    // ACT.INCNTSTEP = 8
        wr_reg(CFG_ACT_OFFSET + 0x0C, Y_DIM);    // ACT.OUTCNTLIM = 8
        wr_reg(CFG_ACT_OFFSET + 0x10, Y_DIM);    // ACT.OUTCNTSTEP = 8
        wr_reg(CFG_ACT_OFFSET + 0x14, Y_DIM);    // ACT.XLIM = 8
        wr_reg(CFG_ACT_OFFSET + 0x18, Y_DIM);    // ACT.XSTEP = 8
        wr_reg(CFG_ACT_OFFSET + 0x1C, Y_DIM);    // ACT.YLIM = 8
        wr_reg(CFG_ACT_OFFSET + 0x20, Y_DIM);    // ACT.YSTEP = 8
        wr_reg(CFG_ACT_OFFSET + 0x24, Y_DIM * K_DIM); // ACT.CHLIM = 64
        wr_reg(CFG_ACT_OFFSET + 0x28, 0x1);      // ACT.DIL_PAT_LOW32 = 1
        wr_reg(CFG_ACT_OFFSET + 0x2C, Y_DIM);    // ACT.CHSTEP = 8
        wr_reg(CFG_ACT_OFFSET + 0x30, Y_DIM);    // ACT.TIL_XLIM = 8
        wr_reg(CFG_ACT_OFFSET + 0x34, Y_DIM);    // ACT.TIL_XSTEP = 8
        wr_reg(CFG_ACT_OFFSET + 0x38, Y_DIM);    // ACT.TIL_YLIM = 8
        wr_reg(CFG_ACT_OFFSET + 0x3C, Y_DIM);    // ACT.TIL_YSTEP = 8

        // Weight Feeder SAURIA Address Generator parameters
        wr_reg(CFG_WEI_OFFSET + 0x04, X_DIM * K_DIM); // WEI.INCNTLIM = 128
        wr_reg(CFG_WEI_OFFSET + 0x08, X_DIM);         // WEI.INCNTSTEP = 16
        wr_reg(CFG_WEI_OFFSET + 0x10, X_DIM * K_DIM); // WEI.WLIM = 128
        wr_reg(CFG_WEI_OFFSET + 0x14, X_DIM);         // WEI.WSTEP = 16
        wr_reg(CFG_WEI_OFFSET + 0x18, 1);             // WEI.KLIM = 1 (waligned)
        wr_reg(CFG_WEI_OFFSET + 0x1C, X_DIM);         // WEI.KSTEP = 16
        wr_reg(CFG_WEI_OFFSET + 0x20, X_DIM);         // WEI.TIL_KLIM = 16
        wr_reg(CFG_WEI_OFFSET + 0x24, X_DIM);         // WEI.TIL_KSTEP = 16
        wr_reg(CFG_WEI_OFFSET + 0x28, 0xFFFF);        // WEI.COLS_ACTIVE = 0xFFFF (all 16 cols active)
        wr_reg(CFG_WEI_OFFSET + 0x2C, 1);             // WEI.WALIGNED = 1

        // PSM / Output limits (1 context, 16 output columns)
        wr_reg(CFG_OUT_OFFSET + 0x00, 1);             // OUT.NCONTEXTS = 1
        wr_reg(CFG_OUT_OFFSET + 0x04, X_DIM);         // OUT.CXLIM = 16
        wr_reg(CFG_OUT_OFFSET + 0x08, Y_DIM);         // OUT.CXSTEP = 8
        wr_reg(CFG_OUT_OFFSET + 0x0C, Y_DIM * X_DIM); // OUT.CKLIM = 128
        wr_reg(CFG_OUT_OFFSET + 0x10, Y_DIM);         // OUT.CKSTEP = 8
        wr_reg(CFG_OUT_OFFSET + 0x14, Y_DIM);         // OUT.TIL_CYLIM = 8
        wr_reg(CFG_OUT_OFFSET + 0x18, Y_DIM);         // OUT.TIL_CYSTEP = 8
        wr_reg(CFG_OUT_OFFSET + 0x1C, Y_DIM * X_DIM); // OUT.TIL_CKLIM = 128
        wr_reg(CFG_OUT_OFFSET + 0x20, Y_DIM * X_DIM); // OUT.TIL_CKSTEP = 128

        // Reduction Engine Mode configuration
        wr_reg(CFG_OUT_OFFSET + 0x2C, re_mode);       // F_RE_MODE_A
    }

    bool execute_hw_core()
    {
        mvm_k.write(K_DIM);
        total_contexts.write(1);

        select.write(sc_bv<3>("111")); // Connect memories to accelerator core
        wait(4);

        start.write(true);
        wait(2);
        start.write(false);

        int timeout = 50000;
        int cycle = 0;
        bool completed = false;
        while (timeout-- > 0)
        {
            wait();
            cycle++;
            if (done.read())
            {
                completed = true;
                std::cout << "  Core execution completed at cycle " << cycle << ".\n";
                break;
            }
        }

        select.write(sc_bv<3>("000")); // Reconnect memories to host MMIO
        wait(4);

        return completed;
    }

    void run()
    {
        std::cout << "\n==================================================================\n";
        std::cout << "   END-TO-END HARDWARE PIPELINE EXECUTION COVERAGE (RE TEST)      \n";
        std::cout << "   Architecture: 16x8 Systolic Array -> PSM -> OBP -> RE -> SRAMC \n";
        std::cout << "==================================================================\n\n";

        reset_dut();

        // -------------------------------------------------------------
        // Step 1: Preload SRAM A (Activations) and SRAM B (Weights)
        // -------------------------------------------------------------
        std::cout << "[Step 1] Preloading Activations (SRAM A) & Weights (SRAM B) via MMIO...\n";
        select.write(sc_bv<3>("000"));
        wait(2);

        // Input activations A: 8 rows with strictly increasing distinct values
        // Row 0: 2, Row 1: 5, Row 2: 8, Row 3: 12, Row 4: 17, Row 5: 23, Row 6: 30, Row 7: 38
        const int8_t act_values[Y_DIM] = {2, 5, 8, 12, 17, 23, 30, 38};
        uint32_t incntlim = K_DIM + X_DIM;

        for (uint32_t k = 0; k < incntlim; k++)
        {
            for (int sw = 0; sw < subwords_a; sw++)
            {
                host_data_t pkt;
                pkt.data.fill(0.0f);
                for (int i = 0; i < 4; i++)
                {
                    int row = sw * 4 + i;
                    if (row < Y_DIM)
                    {
                        pkt[i] = (k < K_DIM) ? static_cast<float>(act_values[row]) : 0.0f;
                    }
                }
                wr_sram(get_srama_addr(k, sw), pkt);
            }
        }

        // Input weights B: all active columns set to 1 for k < K_DIM, and 0 for flush cycles
        for (uint32_t k = 0; k < incntlim; k++)
        {
            for (int sw = 0; sw < subwords_b; sw++)
            {
                host_data_t pkt;
                pkt.data.fill(0.0f);
                for (int i = 0; i < 4; i++)
                {
                    int col = sw * 4 + i;
                    if (col < X_DIM)
                    {
                        pkt[i] = (k < K_DIM) ? 1.0f : 0.0f;
                    }
                }
                wr_sram(get_sramb_addr(k, sw), pkt);
            }
        }
        std::cout << "  Preload complete: A loaded across 8 rows, B loaded across 16 cols.\n";

        // -------------------------------------------------------------
        // Step 2: Pass 1 - Bypass Execution (F_RE_MODE_A = 0, RE_MODE_IDLE)
        // -------------------------------------------------------------
        std::cout << "\n[Step 2] PASS 1: Executing Hardware Pipeline in Bypass Mode (RE_MODE_IDLE=0)...\n";
        program_hw_config(0); // F_RE_MODE_A = 0
        check("F_RE_MODE_A set to 0 (IDLE/Bypass)", rd_reg(CFG_OUT_OFFSET + 0x2C) == 0);
        check("NpuTop internal s_re_mode_a idle", dut->get_re_mode_a() == 0);

        bool pass1_done = execute_hw_core();
        check("Pass 1 execution completed (o_done asserted)", pass1_done);

        // Read back unreduced output Y_obp from SRAM C
        int32_t Y_obp[Y_DIM][X_DIM] = {};
        for (int x = 0; x < X_DIM; x++)
        {
            for (int sw = 0; sw < subwords_c; sw++)
            {
                host_data_t chunk = rd_sram(get_sramc_addr(x, sw));
                for (int i = 0; i < 4; i++)
                {
                    int row = sw * 4 + i;
                    if (row < Y_DIM)
                    {
                        Y_obp[row][x] = static_cast<int32_t>(chunk[i]);
                    }
                }
            }
        }

        std::cout << "  Sample Pass 1 Unreduced Outputs (Column 0):\n";
        for (int row = 0; row < Y_DIM; row++)
        {
            std::cout << "    Lane " << row << " (Row " << row << "): " << Y_obp[row][0] << "\n";
        }

        // Verify that Pass 1 output is non-zero and varies across rows
        bool obp_has_variety = false;
        for (int row = 1; row < Y_DIM; row++)
        {
            if (Y_obp[row][0] != Y_obp[0][0])
                obp_has_variety = true;
        }
        check("Pass 1 produced varying values across rows (unreduced)", obp_has_variety);

        // -------------------------------------------------------------
        // Step 3: Clear SRAM C & Pulse Soft Reset
        // -------------------------------------------------------------
        std::cout << "\n[Step 3] Clearing SRAM C and resetting FSMs via soft reset...\n";
        host_data_t zero_pkt;
        zero_pkt.data.fill(0.0f);
        for (int x = 0; x < X_DIM; x++)
        {
            for (int sw = 0; sw < subwords_c; sw++)
            {
                wr_sram(get_sramc_addr(x, sw), zero_pkt);
            }
        }

        soft_reset.write(true);
        wait(4);
        soft_reset.write(false);
        wait(4);

        // -------------------------------------------------------------
        // Step 4: Pass 2 - Live Reduction Execution (F_RE_MODE_A = 5, RE_MODE_MAXPOOL)
        // -------------------------------------------------------------
        std::cout << "\n[Step 4] PASS 2: Executing Hardware Pipeline with Live RE (RE_MODE_MAXPOOL=5)...\n";
        program_hw_config(5); // F_RE_MODE_A = 5 (RE_MODE_MAXPOOL)
        check("F_RE_MODE_A set to 5 (RE_MODE_MAXPOOL)", rd_reg(CFG_OUT_OFFSET + 0x2C) == 5);
        check("NpuTop internal s_re_mode_a active", dut->get_re_mode_a() == 5);

        bool pass2_done = execute_hw_core();
        check("Pass 2 execution completed (o_done asserted)", pass2_done);

        // Read back reduced output Y_re from SRAM C
        int32_t Y_re[Y_DIM][X_DIM] = {};
        for (int x = 0; x < X_DIM; x++)
        {
            for (int sw = 0; sw < subwords_c; sw++)
            {
                host_data_t chunk = rd_sram(get_sramc_addr(x, sw));
                for (int i = 0; i < 4; i++)
                {
                    int row = sw * 4 + i;
                    if (row < Y_DIM)
                    {
                        Y_re[row][x] = static_cast<int32_t>(chunk[i]);
                    }
                }
            }
        }

        std::cout << "  Sample Pass 2 Reduced Outputs (Column 0):\n";
        for (int row = 0; row < Y_DIM; row++)
        {
            std::cout << "    Lane " << row << " (Row " << row << "): " << Y_re[row][0] << "\n";
        }

        // -------------------------------------------------------------
        // Step 5: Verification of Live Reduction Engine Execution
        // -------------------------------------------------------------
        std::cout << "\n[Step 5] Mathematical & Physical Assertions on Hardware Reduction:\n";

        bool all_lanes_match_max = true;
        bool modified_in_flight = false;
        int checked_columns = 0;

        for (int x = 0; x < X_DIM; x++)
        {
            // Determine expected max from Pass 1 unreduced output
            int32_t max_obp = Y_obp[0][x];
            for (int row = 1; row < Y_DIM; row++)
            {
                if (Y_obp[row][x] > max_obp)
                    max_obp = Y_obp[row][x];
            }

            // Only check columns that had active computation
            if (max_obp > 0)
            {
                checked_columns++;
                for (int row = 0; row < Y_DIM; row++)
                {
                    if (Y_re[row][x] != max_obp)
                    {
                        all_lanes_match_max = false;
                        std::cout << "  [MISMATCH] col=" << x << " row=" << row
                                  << " got=" << Y_re[row][x] << " exp=" << max_obp << "\n";
                    }
                    if (Y_obp[row][x] < max_obp && Y_re[row][x] != Y_obp[row][x])
                    {
                        modified_in_flight = true;
                    }
                }
            }
        }

        check("Active computation occurred across systolic array", checked_columns > 0,
              "Checked " + std::to_string(checked_columns) + " active columns");
        check("All active lanes match vector maximum: Y_re[l] == max_k(Y_obp[k])", all_lanes_match_max);
        check("Hardware RE actively modified stream in-flight: Y_re != Y_obp", modified_in_flight);

        // -------------------------------------------------------------
        // Step 6: Teardown & Restore Bypass Mode
        // -------------------------------------------------------------
        wr_reg(CFG_OUT_OFFSET + 0x2C, 0);
        check("F_RE_MODE_A restored to 0 (IDLE)", rd_reg(CFG_OUT_OFFSET + 0x2C) == 0);
        check("NpuTop internal s_re_mode_a restored to idle", dut->get_re_mode_a() == 0);

        std::cout << "\n==================================================================\n";
        std::cout << "RESULT: " << (errors == 0 ? "ALL PASS" : "FAILURES")
                  << " (" << (total_checks - errors) << "/" << total_checks << " checks pass, errors=" << errors << ")\n";
        std::cout << "==================================================================\n";

        sc_stop();
    }
};

int sc_main(int argc, char *argv[])
{
    sc_clock clk("clk", 10, SC_NS);
    TbPipelineRe tb("tb_pipeline_re");
    tb.i_clk(clk);
    sc_start();
    return tb.errors;
}
