// Standalone Testbench for Output Boundary Pipeline (OBP) Module
// Verifies all 4 stages: Bias Addition, Requantization (per-channel), LUT Activation, and Residual Skip Addition.
//
// Build: make tb_obp
// Run:   ./tb_obp

#include <systemc.h>
#include <cstdint>
#include <iostream>
#include <iomanip>
#include <vector>
#include <cmath>
#include "sauria_types.h"
#include "psm/obp_top.h"

using namespace sauria;

// Configuration for OBP Testbench: 16 channels, using int32_t accumulation and int8_t activation
constexpr int TEST_Y_DIM = 16;
typedef Obp<TEST_Y_DIM, 0x00140000, 0x00150000, int32_t, int8_t> ObpDut;

SC_MODULE(TbObp)
{
    sc_in<bool> i_clk;

    // DUT Signals
    sc_signal<bool> rstn{"rstn"};
    sc_signal<psum_vector_t<TEST_Y_DIM, int32_t>> i_data{"i_data"};
    sc_signal<uint32_t> i_addr{"i_addr"};
    sc_signal<sramc_mask_t<TEST_Y_DIM>> i_wmask{"i_wmask"};
    sc_signal<bool> i_valid{"i_valid"};
    sc_signal<act_vector_t<TEST_Y_DIM, int8_t>> i_residual{"i_residual"};

    sc_signal<psum_vector_t<TEST_Y_DIM, int32_t>> o_sramc_wdata{"o_sramc_wdata"};
    sc_signal<uint32_t> o_sramc_addr{"o_sramc_addr"};
    sc_signal<bool> o_sramc_wren{"o_sramc_wren"};
    sc_signal<sramc_mask_t<TEST_Y_DIM>> o_sramc_wmask{"o_sramc_wmask"};
    sc_signal<bool> o_valid{"o_valid"};

    sc_signal<bool> bias_en{"bias_en"};
    sc_signal<bool> requant_en{"requant_en"};
    sc_signal<bool> lut_en{"lut_en"};
    sc_signal<bool> residual_en{"residual_en"};
    sc_signal<bool> vec_channel_mode{"vec_channel_mode"};
    sc_signal<uint32_t> requant_scale{"requant_scale"};
    sc_signal<uint32_t> requant_shift{"requant_shift"};

    sc_signal<uint32_t> host_addr{"host_addr"};
    sc_signal<bool> host_wren{"host_wren"};
    sc_signal<bool> host_rden{"host_rden"};
    sc_signal<host_data_t> host_wdata{"host_wdata"};
    sc_signal<host_mask_t> host_wmask{"host_wmask"};
    sc_signal<host_data_t> host_rdata{"host_rdata"};

    ObpDut *dut{nullptr};
    int total_tests = 0;
    int tests_passed = 0;

    SC_CTOR(TbObp)
    {
        dut = new ObpDut("dut");
        dut->i_clk(i_clk);
        dut->i_rstn(rstn);
        dut->i_data(i_data);
        dut->i_addr(i_addr);
        dut->i_wmask(i_wmask);
        dut->i_valid(i_valid);
        dut->i_residual(i_residual);

        dut->o_sramc_wdata(o_sramc_wdata);
        dut->o_sramc_addr(o_sramc_addr);
        dut->o_sramc_wren(o_sramc_wren);
        dut->o_sramc_wmask(o_sramc_wmask);
        dut->o_valid(o_valid);

        dut->i_bias_en(bias_en);
        dut->i_requant_en(requant_en);
        dut->i_lut_en(lut_en);
        dut->i_residual_en(residual_en);
        dut->i_vec_channel_mode(vec_channel_mode);
        dut->i_requant_scale(requant_scale);
        dut->i_requant_shift(requant_shift);

        dut->i_host_addr(host_addr);
        dut->i_host_wren(host_wren);
        dut->i_host_rden(host_rden);
        dut->i_host_wdata(host_wdata);
        dut->i_host_wmask(host_wmask);
        dut->o_host_rdata(host_rdata);

        SC_THREAD(test_process);
        sensitive << i_clk.pos();
    }

    ~TbObp()
    {
        delete dut;
    }

    // Host Programming Helper Methods
    void host_write(uint32_t addr, uint32_t val)
    {
        host_data_t d;
        d.data.fill(0.0f);
        uint32_t region = addr & 0x00FF0000;
        if (region == 0x00140000) // LUT_OFFSET
        {
            d[0] = static_cast<double>(val & 0xFF);
            d[1] = static_cast<double>((val >> 8) & 0xFF);
            d[2] = static_cast<double>((val >> 16) & 0xFF);
            d[3] = static_cast<double>((val >> 24) & 0xFF);
        }
        else
        {
            d[0] = static_cast<double>(val);
        }
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

    uint32_t host_read(uint32_t addr)
    {
        host_addr.write(addr);
        host_rden.write(true);
        host_wren.write(false);
        wait();
        wait();
        host_data_t r = host_rdata.read();
        host_rden.write(false);
        wait();
        
        uint32_t region = addr & 0x00FF0000;
        if (region == 0x00140000) // LUT_OFFSET
        {
            uint32_t val = (static_cast<uint32_t>(r[0]) & 0xFF) |
                           ((static_cast<uint32_t>(r[1]) & 0xFF) << 8) |
                           ((static_cast<uint32_t>(r[2]) & 0xFF) << 16) |
                           ((static_cast<uint32_t>(r[3]) & 0xFF) << 24);
            return val;
        }
        else
        {
            return static_cast<uint32_t>(r[0]);
        }
    }

    void run_reset()
    {
        rstn.write(false);
        bias_en.write(false);
        requant_en.write(false);
        lut_en.write(false);
        residual_en.write(false);
        vec_channel_mode.write(false);
        requant_scale.write(1);
        requant_shift.write(0);
        i_valid.write(false);
        i_addr.write(0);
        psum_vector_t<TEST_Y_DIM, int32_t> zero_data(0);
        i_data.write(zero_data);

        sramc_mask_t<TEST_Y_DIM> zero_mask(false);
        i_wmask.write(zero_mask);

        act_vector_t<TEST_Y_DIM, int8_t> zero_res(0);
        i_residual.write(zero_res);

        host_wren.write(false);
        host_rden.write(false);

        wait(5);
        rstn.write(true);
        wait(2);
    }

    void wait_and_print(int cycles)
    {
        for (int c = 1; c <= cycles; c++)
        {
            wait();
            std::cout << "    [Cycle " << c << "] o_valid=" << o_valid.read()
                      << " o_sramc_wren=" << o_sramc_wren.read()
                      << " data[0]=" << o_sramc_wdata.read()[0]
                      << " data[1]=" << o_sramc_wdata.read()[1] << std::endl;
        }
    }

    void test_process()
    {
        std::cout << "\n==================================================" << std::endl;
        std::cout << "         OBP STANDALONE TESTBENCH" << std::endl;
        std::cout << "==================================================\n" << std::endl;

        // ----------------------------------------------------
        // Test Case 1: Reset and Bypass Verification
        // ----------------------------------------------------
        std::cout << "--- CASE 1: Reset & Bypass Check ---" << std::endl;
        run_reset();
        
        // Feed sample input data
        psum_vector_t<TEST_Y_DIM, int32_t> test_in;
        sramc_mask_t<TEST_Y_DIM> test_mask;
        for (int i = 0; i < TEST_Y_DIM; i++)
        {
            test_in[i] = (i + 1) * 10;
            test_mask[i] = true;
        }
        
        i_data.write(test_in);
        i_addr.write(0x1000);
        i_wmask.write(test_mask);
        i_valid.write(true);
        wait();
        i_valid.write(false); // 1 cycle pulse

        // Pipeline delay: wait 3 cycles (Stage 1 -> Stage 2 -> Stage 3 -> Stage 4 writeback)
        wait_and_print(4);

        bool bypass_ok = true;
        for (int i = 0; i < TEST_Y_DIM; i++)
        {
            int32_t got = o_sramc_wdata.read()[i];
            int32_t exp = (i + 1) * 10;
            if (got != exp)
            {
                std::cout << "  [FAIL] Channel " << i << " expected " << exp << ", got " << got << std::endl;
                bypass_ok = false;
            }
        }
        total_tests++;
        if (bypass_ok)
        {
            tests_passed++;
            std::cout << "  [PASS] Bypass values passed through correctly." << std::endl;
        }

        // ----------------------------------------------------
        // Test Case 2: Stage 1 Bias Addition
        // ----------------------------------------------------
        std::cout << "\n--- CASE 2: Bias Addition (INT32 + INT32) ---" << std::endl;
        run_reset();

        // Program bias values per channel via host programming interface
        // address = 0x00150000 + channel * 4
        for (int i = 0; i < TEST_Y_DIM; i++)
        {
            host_write(0x00150000 + i * 4, (i + 1) * 5);
        }

        // Verify programmed bias values via host readback
        bool bias_rd_ok = true;
        for (int i = 0; i < TEST_Y_DIM; i++)
        {
            uint32_t val = host_read(0x00150000 + i * 4);
            if (val != static_cast<uint32_t>((i + 1) * 5))
            {
                bias_rd_ok = false;
                std::cout << "  [FAIL] Bias readback mismatch at lane " << i << ": got " << val << ", expected " << (i + 1) * 5 << std::endl;
            }
        }
        total_tests++;
        if (bias_rd_ok)
        {
            tests_passed++;
            std::cout << "  [PASS] Bias memory successfully programmed and read back." << std::endl;
        }

        // Test with bias_en = true
        bias_en.write(true);
        i_data.write(test_in);
        i_addr.write(0x2000);
        i_wmask.write(test_mask);
        i_valid.write(true);
        wait();
        i_valid.write(false);
        wait_and_print(4);

        bool bias_add_ok = true;
        for (int i = 0; i < TEST_Y_DIM; i++)
        {
            int32_t got = o_sramc_wdata.read()[i];
            int32_t exp = ((i + 1) * 10) + ((i + 1) * 5); // data + bias
            if (got != exp)
            {
                bias_add_ok = false;
                std::cout << "  [FAIL] Bias addition at lane " << i << ": expected " << exp << ", got " << got << std::endl;
            }
        }
        total_tests++;
        if (bias_add_ok)
        {
            tests_passed++;
            std::cout << "  [PASS] Bias addition performed correctly." << std::endl;
        }

        // ----------------------------------------------------
        // Test Case 3: Stage 2 Requantization (Per-channel scale & shift)
        // ----------------------------------------------------
        std::cout << "\n--- CASE 3: Requantization (Per-Channel scale & shift) ---" << std::endl;
        run_reset();

        // Program per-channel scale and shift rams:
        // SCALE_OFFSET = 0x00140000 + 0x00040000 = 0x00180000
        // SHIFT_OFFSET = 0x00140000 + 0x00050000 = 0x00190000
        for (int i = 0; i < TEST_Y_DIM; i++)
        {
            host_write(0x00180000 + i * 4, 100 + i * 10); // scale multiplier
            host_write(0x00190000 + i * 4, 8);            // right shift of 8 bits
        }

        // Enable requantization
        requant_en.write(true);

        // Input data
        psum_vector_t<TEST_Y_DIM, int32_t> requant_in;
        for (int i = 0; i < TEST_Y_DIM; i++)
        {
            requant_in[i] = 256; // 2^8
        }
        i_data.write(requant_in);
        i_addr.write(0x3000);
        i_wmask.write(test_mask);
        i_valid.write(true);
        wait();
        i_valid.write(false);
        wait_and_print(4);

        bool requant_ok = true;
        for (int i = 0; i < TEST_Y_DIM; i++)
        {
            int32_t got = o_sramc_wdata.read()[i];
            // exp = (256 * (100 + i*10)) >> 8 = 100 + i*10
            int32_t exp = 100 + i * 10;
            // Clamped to int8_t range (-128 to 127)
            if (exp > 127) exp = 127;
            if (exp < -128) exp = -128;

            if (got != exp)
            {
                requant_ok = false;
                std::cout << "  [FAIL] Requantization at lane " << i << ": expected " << exp << ", got " << got << std::endl;
            }
        }
        total_tests++;
        if (requant_ok)
        {
            tests_passed++;
            std::cout << "  [PASS] Per-channel scale and shift requantization verified with saturation clamping." << std::endl;
        }

        // Test fallback to default scalar ports: reset to clear scale/shift ram valid flags
        std::cout << "  Testing fallback to scalar ports..." << std::endl;
        run_reset();
        requant_en.write(true);
        requant_scale.write(50); // global scale multiplier
        requant_shift.write(6);  // global right shift
        i_data.write(requant_in);
        i_valid.write(true);
        wait();
        i_valid.write(false);
        wait_and_print(4);

        bool fallback_ok = true;
        for (int i = 0; i < TEST_Y_DIM; i++)
        {
            int32_t got = o_sramc_wdata.read()[i];
            // exp = (256 * 50) >> 6 = 200 -> clamped to 127
            int32_t exp = 127;
            if (got != exp)
            {
                fallback_ok = false;
                std::cout << "  [FAIL] Fallback at lane " << i << ": expected " << exp << ", got " << got << std::endl;
            }
        }
        total_tests++;
        if (fallback_ok)
        {
            tests_passed++;
            std::cout << "  [PASS] Fallback logic to global scalar ports works correctly." << std::endl;
        }

        // ----------------------------------------------------
        // Test Case 4: Stage 3 LUT Activation
        // ----------------------------------------------------
        std::cout << "\n--- CASE 4: LUT Activation (Exact matching) ---" << std::endl;
        run_reset();

        // Program activation lookup table: map input values from -128 to 127
        // For channel 0, let's program a custom absolute value function: y = abs(x)
        // address offset = channel * 256 + index
        // We write 4 entries at a time via host interface
        for (int idx = 0; idx < 256; idx += 4)
        {
            // Calculate absolute value mapping for 4 entries
            uint32_t val = 0;
            for (int i = 0; i < 4; i++)
            {
                int8_t x = static_cast<int8_t>((idx + i) - 128);
                uint8_t y = std::abs(x);
                val |= (static_cast<uint32_t>(y) << (i * 8));
            }
            host_write(0x00140000 + idx, val);
        }

        lut_en.write(true);
        psum_vector_t<TEST_Y_DIM, int32_t> lut_in;
        for (int i = 0; i < TEST_Y_DIM; i++)
        {
            lut_in[i] = -25; // x = -25 -> abs(x) = 25 for channel 0
        }
        i_data.write(lut_in);
        i_valid.write(true);
        wait();
        i_valid.write(false);
        wait_and_print(4);

        // Verify output for channel 0 (which has our programmed LUT)
        int32_t ch0_got = o_sramc_wdata.read()[0];
        int32_t ch0_exp = 25;
        total_tests++;
        if (ch0_got == ch0_exp)
        {
            tests_passed++;
            std::cout << "  [PASS] Channel 0 LUT Activation mapped " << -25 << " -> " << ch0_got << " (exact absolute value)." << std::endl;
        }
        else
        {
            std::cout << "  [FAIL] Channel 0 LUT Activation expected " << ch0_exp << ", got " << ch0_got << std::endl;
        }

        // ----------------------------------------------------
        // Test Case 5: Stage 4 Residual Skip Addition
        // ----------------------------------------------------
        std::cout << "\n--- CASE 5: Stage 4 Residual Skip Addition ---" << std::endl;
        run_reset();

        residual_en.write(true);
        
        // Input activation values
        psum_vector_t<TEST_Y_DIM, int32_t> res_data_in;
        act_vector_t<TEST_Y_DIM, int8_t> residual_in;
        for (int i = 0; i < TEST_Y_DIM; i++)
        {
            res_data_in[i] = (i + 1) * 10;
            residual_in[i] = (i + 1) * 3;
        }

        i_data.write(res_data_in);
        i_residual.write(residual_in);
        i_valid.write(true);
        wait();
        i_valid.write(false);
        wait_and_print(4);

        bool residual_ok = true;
        for (int i = 0; i < TEST_Y_DIM; i++)
        {
            int32_t got = o_sramc_wdata.read()[i];
            int32_t exp = ((i + 1) * 10) + ((i + 1) * 3);
            if (got != exp)
            {
                residual_ok = false;
                std::cout << "  [FAIL] Residual skip at lane " << i << ": expected " << exp << ", got " << got << std::endl;
            }
        }
        total_tests++;
        if (residual_ok)
        {
            tests_passed++;
            std::cout << "  [PASS] Residual skip addition matches expected values." << std::endl;
        }

        // ----------------------------------------------------
        // Test Case 6: Complete Pipeline Fusion
        // ----------------------------------------------------
        std::cout << "\n--- CASE 6: Complete Pipeline Fusion ---" << std::endl;
        run_reset();

        bias_en.write(true);
        requant_en.write(true);
        lut_en.write(true);
        residual_en.write(true);

        // 1. Program Bias: +10 per channel
        for (int i = 0; i < TEST_Y_DIM; i++)
        {
            host_write(0x00150000 + i * 4, 10);
        }

        // 2. Program Requant: scale=128, shift=8 (divide by 2)
        for (int i = 0; i < TEST_Y_DIM; i++)
        {
            host_write(0x00180000 + i * 4, 128);
            host_write(0x00190000 + i * 4, 8);
        }

        // 3. Program LUT: ReLU activation mapping (y = max(0, x))
        for (int idx = 0; idx < 256; idx += 4)
        {
            uint32_t val = 0;
            for (int i = 0; i < 4; i++)
            {
                int8_t x = static_cast<int8_t>((idx + i) - 128);
                uint8_t y = (x > 0) ? x : 0;
                val |= (static_cast<uint32_t>(y) << (i * 8));
            }
            // Program channel 0
            host_write(0x00140000 + idx, val);
        }

        // 4. Residual Skip input: +5
        act_vector_t<TEST_Y_DIM, int8_t> skip_in(5);
        i_residual.write(skip_in);

        // Feed negative and positive inputs to check ReLU logic
        psum_vector_t<TEST_Y_DIM, int32_t> fusion_in(-40); // (-40 + 10) * 128 >> 7 = -30 -> ReLU = 0 -> output = 0 + 5 = 5
        i_data.write(fusion_in);
        i_valid.write(true);
        wait();
        std::cout << "    [Cycle 1] Negative input fed. o_valid=" << o_valid.read() << " wren=" << o_sramc_wren.read() << std::endl;

        fusion_in = psum_vector_t<TEST_Y_DIM, int32_t>(30);  // (30 + 10) * 128 >> 7 = 20 -> ReLU = 20 -> output = 20 + 5 = 25
        i_data.write(fusion_in);
        wait();
        std::cout << "    [Cycle 2] Positive input fed. o_valid=" << o_valid.read() << " wren=" << o_sramc_wren.read() << std::endl;

        i_valid.write(false);
        wait();
        std::cout << "    [Cycle 3] Inputs disabled. o_valid=" << o_valid.read() << " wren=" << o_sramc_wren.read() << std::endl;
        wait();
        std::cout << "    [Cycle 4] Negative input output ready. o_valid=" << o_valid.read() << " wren=" << o_sramc_wren.read() << " data[0]=" << o_sramc_wdata.read()[0] << std::endl;

        // Check output 1 (ready at cycle 5)
        wait();
        std::cout << "    [Cycle 5] Negative input output ready. o_valid=" << o_valid.read() << " wren=" << o_sramc_wren.read() << " data[0]=" << o_sramc_wdata.read()[0] << std::endl;
        int32_t got1 = o_sramc_wdata.read()[0];
        int32_t exp1 = 5;
        
        wait(); // next output (Cycle 6)
        std::cout << "    [Cycle 6] Positive input output ready. o_valid=" << o_valid.read() << " wren=" << o_sramc_wren.read() << " data[0]=" << o_sramc_wdata.read()[0] << std::endl;
        int32_t got2 = o_sramc_wdata.read()[0];
        int32_t exp2 = 25;

        bool fusion_ok = (got1 == exp1) && (got2 == exp2);
        total_tests++;
        if (fusion_ok)
        {
            tests_passed++;
            std::cout << "  [PASS] Full pipelined OBP fusion verified successfully." << std::endl;
            std::cout << "    Negative input flow output (ReLU clamped + Residual) got " << got1 << " (exp " << exp1 << ")" << std::endl;
            std::cout << "    Positive input flow output (ReLU pass + Residual) got " << got2 << " (exp " << exp2 << ")" << std::endl;
        }
        else
        {
            std::cout << "  [FAIL] OBP Fusion: expected (" << exp1 << ", " << exp2 << "), got (" << got1 << ", " << got2 << ")" << std::endl;
        }

        // ----------------------------------------------------
        // Test Case 7: Per-Vector Per-Channel Mode (1 Vector = 1 Channel)
        // ----------------------------------------------------
        std::cout << "\n--- CASE 7: Per-Vector Per-Channel Mode (1 Vector = 1 Channel) ---" << std::endl;
        run_reset();

        vec_channel_mode.write(true);
        bias_en.write(true);

        // Program bias: Channel 0 -> 100, Channel 1 -> 200, Channel 2 -> 300
        host_write(0x00150000 + 0 * 4, 100);
        host_write(0x00150000 + 1 * 4, 200);
        host_write(0x00150000 + 2 * 4, 300);

        // Feed Vector 0 (Channel 0), Vector 1 (Channel 1), Vector 2 (Channel 2)
        psum_vector_t<TEST_Y_DIM, int32_t> vec_in(10);
        i_data.write(vec_in);
        i_valid.write(true);
        wait(); // Feed Vector 0 (Ch 0)

        i_data.write(vec_in);
        wait(); // Feed Vector 1 (Ch 1)

        i_data.write(vec_in);
        wait(); // Feed Vector 2 (Ch 2)

        i_valid.write(false);
        wait(2); // Wait for pipeline delay (4 cycles total: Cycle 1 fed -> Cycle 5 output ready)

        int32_t v0_got = o_sramc_wdata.read()[0];
        int32_t v0_exp = 10 + 100; // 110 for all elements of Vector 0

        wait();
        int32_t v1_got = o_sramc_wdata.read()[0];
        int32_t v1_exp = 10 + 200; // 210 for all elements of Vector 1

        wait();
        int32_t v2_got = o_sramc_wdata.read()[0];
        int32_t v2_exp = 10 + 300; // 310 for all elements of Vector 2

        bool vec_mode_ok = (v0_got == v0_exp) && (v1_got == v1_exp) && (v2_got == v2_exp);
        total_tests++;
        if (vec_mode_ok)
        {
            tests_passed++;
            std::cout << "  [PASS] Per-Vector Per-Channel Mode verified successfully!" << std::endl;
            std::cout << "    Vector 0 (Channel 0): got " << v0_got << " (exp " << v0_exp << ")" << std::endl;
            std::cout << "    Vector 1 (Channel 1): got " << v1_got << " (exp " << v1_exp << ")" << std::endl;
            std::cout << "    Vector 2 (Channel 2): got " << v2_got << " (exp " << v2_exp << ")" << std::endl;
        }
        else
        {
            std::cout << "  [FAIL] Per-Vector Per-Channel Mode mismatch: got (" << v0_got << ", " << v1_got << ", " << v2_got
                      << "), expected (" << v0_exp << ", " << v1_exp << ", " << v2_exp << ")" << std::endl;
        }

        // Print final status summary
        std::cout << "\n==================================================" << std::endl;
        std::cout << "  TEST SUMMARY: " << tests_passed << " / " << total_tests << " Passed" << std::endl;
        std::cout << "  RESULT: " << (tests_passed == total_tests ? "SUCCESS" : "FAILURE") << std::endl;
        std::cout << "==================================================\n" << std::endl;

        sc_stop();
    }
};

int sc_main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    sc_clock clk("clk", 10, SC_NS);
    TbObp tb("tb");
    tb.i_clk(clk);
    sc_start();
    return 0;
}
