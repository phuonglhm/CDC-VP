// Unified PROFILE smoke test.
//
// Proves the linchpin: ONE binary, runtime-selectable profile, where the SAME config
// address decodes to DIFFERENT fields per profile (the v1<->v4 collision), using only
// NpuTop's host AXI interface (write PROFILE -> write reg -> read it back).
//
// Build (from v1_model/):   make tb_unified_smoke
// Run:                      ./tb_unified_smoke
// Expect: "RESULT: ALL PASS (errors=0)".

#include <systemc.h>
#include <cstdint>
#include <iostream>
#include "sauria_types.h"
#include "npu_profile.h"
#include "npu_top.h" // resolves to unified/npu_top.h via -Iunified

using namespace sauria;

// Same geometry as the eval config (16x8 array, int8/int8/int32).
typedef NpuTop<16, 8, int8_t, int8_t, int32_t, 4096, 2048, 2048, 16, 24, 1> NpuT;

SC_MODULE(TbSmoke)
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

    SC_CTOR(TbSmoke)
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
        wait();
    }

    uint32_t rd(uint32_t addr)
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

    void check(const char *tag, uint32_t got, uint32_t exp)
    {
        total_checks++;
        bool ok = (got == exp);
        if (!ok)
            errors++;
        std::cout << "  [" << (ok ? "PASS" : "FAIL") << "] " << tag
                  << "  got=" << got << " exp=" << exp << "\n";
    }

    void run()
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
        wait(4);
        rstn.write(true);
        wait(2);

        std::cout << "\n=== UNIFIED PROFILE SMOKE TEST ===\n";

        // ---------------- V4 (LINEAR) profile ----------------
        wr(CFG_PROFILE_ADDR, PROFILE_V4_LINEAR);
        std::cout << "PROFILE readback = " << rd(CFG_PROFILE_ADDR)
                  << " (" << profile_name((NpuProfile)rd(CFG_PROFILE_ADDR)) << ")\n";
        wr(CFG_OUT_OFFSET + 0x00, 111); // V4: cxlim
        wr(CFG_OUT_OFFSET + 0x10, 222); // V4: til_cylim
        wr(CFG_CON_OFFSET + 0x0C, 7);   // V4: ncontexts (lives in CON for v4)
        std::cout << "[V4 map]\n";
        check("OUT+0x00 -> cxlim", rd(CFG_OUT_OFFSET + 0x00), 111);
        check("OUT+0x10 -> til_cylim", rd(CFG_OUT_OFFSET + 0x10), 222);
        check("CON+0x0C -> ncontexts", rd(CFG_CON_OFFSET + 0x0C), 7);

        // ---------------- V1 (SAURIA) profile ----------------
        wr(CFG_PROFILE_ADDR, PROFILE_V1_SAURIA);
        std::cout << "PROFILE readback = " << rd(CFG_PROFILE_ADDR)
                  << " (" << profile_name((NpuProfile)rd(CFG_PROFILE_ADDR)) << ")\n";
        wr(CFG_OUT_OFFSET + 0x04, 333); // V1: cxlim
        wr(CFG_OUT_OFFSET + 0x10, 444); // V1: ckstep (SAME addr as V4 til_cylim)
        wr(NCONTEXTS, 9);               // V1: ncontexts lives at OUT+0x00
        std::cout << "[V1 map]\n";
        check("OUT+0x04 -> cxlim", rd(CFG_OUT_OFFSET + 0x04), 333);
        check("OUT+0x10 -> ckstep", rd(CFG_OUT_OFFSET + 0x10), 444);
        check("OUT+0x00 -> ncontexts", rd(NCONTEXTS), 9);

        std::cout << "\nCollision check at OUT+0x10: V4 read=222 (til_cylim), "
                     "V1 read=444 (ckstep) -> same address, different field per profile.\n";

        // ---------------- OBP A & B Register Smoke Tests (All 8 RAMs) ----------------
        std::cout << "\n[OBP A & B Register Decode & Collision-Free RAM Tests]\n";
        // 1. OBP-A Bias (0x00150000)
        wr(0x00150000, static_cast<uint32_t>(-65002));
        check("OBP-A Bias[0] (-65002)", rd(0x00150000), static_cast<uint32_t>(-65002));
        wr(0x00150004, static_cast<uint32_t>(-51331));
        check("OBP-A Bias[1] (-51331)", rd(0x00150004), static_cast<uint32_t>(-51331));

        // 2. OBP-A Scale (0x00180000) & Shift (0x00190000)
        wr(0x00180000, 12345);
        check("OBP-A Scale[0] (12345)", rd(0x00180000), 12345);
        wr(0x00190000, 7);
        check("OBP-A Shift[0] (7)", rd(0x00190000), 7);

        // 3. OBP-A Zero-Point RAM (0x001C0000)
        wr(0x001C0000, static_cast<uint32_t>(-25));
        check("OBP-A Zero-Point[0] (-25)", rd(0x001C0000), static_cast<uint32_t>(-25));

        // 4. OBP-B Bias (0x00170000)
        wr(0x00170000, static_cast<uint32_t>(-43210));
        check("OBP-B Bias[0] (-43210)", rd(0x00170000), static_cast<uint32_t>(-43210));

        // 5. OBP-B Scale (0x001A0000) & Shift (0x001B0000)
        wr(0x001A0000, 54321);
        check("OBP-B Scale[0] (54321)", rd(0x001A0000), 54321);
        wr(0x001B0000, 9);
        check("OBP-B Shift[0] (9)", rd(0x001B0000), 9);

        // 6. OBP-B Zero-Point RAM (0x001D0000)
        wr(0x001D0000, static_cast<uint32_t>(-35));
        check("OBP-B Zero-Point[0] (-35)", rd(0x001D0000), static_cast<uint32_t>(-35));

        // 7. Re-verify OBP-A Zero-Point is NOT overwritten by OBP-B Scale (Collision Check)
        check("Collision Check: OBP-A Zero-Point still -25", rd(0x001C0000), static_cast<uint32_t>(-25));
        check("Collision Check: OBP-B Scale still 54321", rd(0x001A0000), 54321);

        // ---------------- RE Mode Register & Mux Integration Smoke Tests ----------------
        std::cout << "\n[RE Mode Register Decode, Mux Gating & SRAM C Smoke Tests]\n";
        // Explicitly select V4 profile for RE mode configuration
        wr(CFG_PROFILE_ADDR, PROFILE_V4_LINEAR);

        // 1. Write F_RE_MODE_A (0x000F082C / OUT+0x2C) to RE_MODE_SOFTMAX_TILE_PASS1 (7)
        wr(CFG_OUT_OFFSET + 0x2C, 7);
        check("OUT+0x2C -> F_RE_MODE_A (TILE_PASS1=7)", rd(CFG_OUT_OFFSET + 0x2C), 7);
        check("NpuTop internal s_re_mode_a bound", dut->get_re_mode_a(), 7);
        // Verify wren_a is idle after RE mode write
        check("wren idle after mode write", dut->get_final_sramc_wren_a() ? 1 : 0, 0);

        // 2. Write F_RE_MODE_B (0x000F083C / OUT+0x3C) to RE_MODE_SOFTMAX_TILE_PASS2 (8)
        wr(CFG_OUT_OFFSET + 0x3C, 8);
        check("OUT+0x3C -> F_RE_MODE_B (TILE_PASS2=8)", rd(CFG_OUT_OFFSET + 0x3C), 8);
        check("NpuTop internal s_re_mode_b bound", dut->get_re_mode_b(), 8);

        // 3. Verify Host MMIO path to SRAM C (0x000C0000)
        uint32_t test_val = 0x5A5A1234;
        wr(SRAMC_OFFSET + 0x00, test_val);
        check("Host MMIO SRAM C Write & Readback", rd(SRAMC_OFFSET + 0x00), test_val);

        // 4. Test Top-Level Asymmetric Zero-Point unpack from F_OBP_CFG_A bits [23:16]
        int8_t test_zp_val = -15;
        uint32_t cfg_with_zp = (static_cast<uint32_t>(static_cast<uint8_t>(test_zp_val)) << 16) | 0x2; // requant_en + zp
        wr(CFG_OUT_OFFSET + 0x20, cfg_with_zp);
        wait();
        check("OBP-A Zero-Point unpacked from F_OBP_CFG_A (-15)", dut->get_obp_output_zp_a(), test_zp_val);

        // 5. Test Top-Level Active Residual Skip Connection (set_residual_skip_a)
        // Note: OBP and RE share one skip signal (s_residual_skip_a/b).
        // re_inst_a->i_skip_data(s_residual_skip_a) port binding is verified by
        // querying re_inst_a->i_skip_data.read() directly through RE's own input port.
        act_vector_t<8, int8_t> skip_test_vec;
        for (int l = 0; l < 8; l++) skip_test_vec[l] = static_cast<int8_t>(42 + l);
        dut->set_residual_skip_a(skip_test_vec);
        wait();
        check("Top-Level active residual skip injected (lane 0)", dut->get_residual_skip_a()[0], 42);
        check("Top-Level active residual skip injected (lane 7)", dut->get_residual_skip_a()[7], 49);
        check("RE-A i_skip_data port directly sampled skip input (lane 0)", dut->get_re_skip_data_in_a()[0], 42);
        check("RE-A i_skip_data port directly sampled skip input (lane 7)", dut->get_re_skip_data_in_a()[7], 49);

        // 6. Restore F_RE_MODE_A and F_RE_MODE_B to 0 (IDLE/Bypass)
        wr(CFG_OUT_OFFSET + 0x2C, 0);
        check("OUT+0x2C -> F_RE_MODE_A restored to 0 (IDLE)", rd(CFG_OUT_OFFSET + 0x2C), 0);
        check("NpuTop internal s_re_mode_a idle", dut->get_re_mode_a(), 0);

        wr(CFG_OUT_OFFSET + 0x3C, 0);
        check("OUT+0x3C -> F_RE_MODE_B restored to 0 (IDLE)", rd(CFG_OUT_OFFSET + 0x3C), 0);
        check("NpuTop internal s_re_mode_b idle", dut->get_re_mode_b(), 0);

        std::cout << "\nRESULT: " << (errors == 0 ? "ALL PASS" : "FAILURES")
                  << " (" << (total_checks - errors) << "/" << total_checks << " checks pass, errors=" << errors << ")\n";
        sc_stop();
    }
};

int sc_main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    sc_clock clk("clk", 10, SC_NS);
    TbSmoke tb("tb");
    tb.i_clk(clk);
    sc_start();
    return 0;
}
