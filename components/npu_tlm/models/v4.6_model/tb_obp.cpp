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
typedef Obp<TEST_Y_DIM, 0x00140000, 0x00150000, 0x001C0000, int32_t, int8_t> ObpDut;

SC_MODULE(TbObp)
{
    sc_in<bool> i_clk;

    // DUT Signals
    sc_signal<bool> rstn{"rstn"};
    sc_signal<psum_vector_t<TEST_Y_DIM, int32_t>> i_data{"i_data"};
    sc_signal<uint32_t> i_addr{"i_addr"};
    sc_signal<sramc_mask_t<TEST_Y_DIM>> i_wmask{"i_wmask"};
    sc_signal<bool> i_valid{"i_valid"};
    sc_signal<uint32_t> i_channel_idx{"i_channel_idx"};
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
    sc_signal<int32_t> output_zp{"output_zp"};

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
        dut->i_channel_idx(i_channel_idx);
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
        dut->i_output_zp(output_zp);

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
            return static_cast<uint32_t>(static_cast<int64_t>(r[0]));
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
        output_zp.write(0);
        i_valid.write(false);
        i_addr.write(0);
        psum_vector_t<TEST_Y_DIM, int32_t> zero_data(0);
        i_data.write(zero_data);

        sramc_mask_t<TEST_Y_DIM> zero_mask(false);
        i_wmask.write(zero_mask);
        i_channel_idx.write(0);

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

        // Test Case 2b: Negative Bias Programming & Readback (e.g. -65002, -51331)
        std::cout << "\n--- CASE 2b: Negative Bias Programming & Readback ---" << std::endl;
        int32_t neg_bias_0 = -65002;
        int32_t neg_bias_1 = -51331;
        host_write(0x00150000 + 0 * 4, static_cast<uint32_t>(neg_bias_0));
        host_write(0x00150000 + 1 * 4, static_cast<uint32_t>(neg_bias_1));

        uint32_t rd_neg0 = host_read(0x00150000 + 0 * 4);
        uint32_t rd_neg1 = host_read(0x00150000 + 1 * 4);
        bool neg_bias_ok = (rd_neg0 == static_cast<uint32_t>(neg_bias_0)) &&
                           (rd_neg1 == static_cast<uint32_t>(neg_bias_1));
        if (rd_neg0 == 0x80000000 || rd_neg1 == 0x80000000)
        {
            std::cout << "  [FAIL] Bias readback returned 0x80000000 (saturation bug present)!" << std::endl;
            neg_bias_ok = false;
        }
        if (!neg_bias_ok)
        {
            std::cout << "  [FAIL] Negative bias readback mismatch: lane 0 got " << rd_neg0
                      << " (exp " << static_cast<uint32_t>(neg_bias_0) << "), lane 1 got " << rd_neg1
                      << " (exp " << static_cast<uint32_t>(neg_bias_1) << ")" << std::endl;
        }
        else
        {
            std::cout << "  [PASS] Negative bias values successfully written and read back without 0x80000000 saturation." << std::endl;
        }
        total_tests++;
        if (neg_bias_ok) tests_passed++;

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

        // Verify scale & shift readback
        bool scale_shift_rd_ok = true;
        for (int i = 0; i < TEST_Y_DIM; i++)
        {
            uint32_t sc = host_read(0x00180000 + i * 4);
            uint32_t sh = host_read(0x00190000 + i * 4);
            if (sc != static_cast<uint32_t>(100 + i * 10) || sh != 8)
            {
                scale_shift_rd_ok = false;
                std::cout << "  [FAIL] Scale/Shift readback at lane " << i << ": got scale=" << sc << ", shift=" << sh << std::endl;
            }
        }
        total_tests++;
        if (scale_shift_rd_ok)
        {
            tests_passed++;
            std::cout << "  [PASS] Scale and shift memories successfully programmed and read back." << std::endl;
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
        i_channel_idx.write(0);
        i_valid.write(true);
        wait(); // Feed Vector 0 (Ch 0)

        i_data.write(vec_in);
        i_channel_idx.write(1);
        wait(); // Feed Vector 1 (Ch 1)

        i_data.write(vec_in);
        i_channel_idx.write(2);
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

        // ----------------------------------------------------
        // Test Case 8: PSM Discontinuous Valid Burst Regression Test
        // Valid pattern: 1, 0, 0, 1, 1, 1... (x0, 2 bubbles, x1, x2, x3, x4)
        // Verifies no off-by-one or spurious resets across pipeline gaps
        // ----------------------------------------------------
        std::cout << "\n--- CASE 8: PSM Discontinuous Valid Burst (1, 0, 0, 1, 1...) ---" << std::endl;
        run_reset();

        vec_channel_mode.write(true);
        bias_en.write(true);

        // Program distinct biases for channels 0 to 4
        host_write(0x00150000 + 0 * 4, 100);
        host_write(0x00150000 + 1 * 4, 200);
        host_write(0x00150000 + 2 * 4, 300);
        host_write(0x00150000 + 3 * 4, 400);
        host_write(0x00150000 + 4 * 4, 500);

        // Helper to advance 1 cycle while sampling outputs
        std::vector<int32_t> captured_outputs;
        auto step = [&]() {
            wait();
            if (o_valid.read() && o_sramc_wren.read())
            {
                captured_outputs.push_back(o_sramc_wdata.read()[0]);
            }
        };

        // Phase 1: Write x0 (Channel 0)
        i_data.write(vec_in);
        i_channel_idx.write(0);
        i_valid.write(true);
        step();

        // Phase 2 & 3: PSM drain bubble (2 cycles wren=0 / valid=0)
        i_valid.write(false);
        step();
        step();

        // Phase 4: Write x1, x2, x3, x4 continuously
        i_data.write(vec_in);
        i_channel_idx.write(1);
        i_valid.write(true);
        step();

        i_data.write(vec_in);
        i_channel_idx.write(2);
        step();

        i_data.write(vec_in);
        i_channel_idx.write(3);
        step();

        i_data.write(vec_in);
        i_channel_idx.write(4);
        step();

        i_valid.write(false);

        // Wait out remaining pipeline drain
        for (int cycle = 0; cycle < 6; cycle++)
        {
            step();
        }

        std::vector<int32_t> expected_outputs = {
            10 + 100, // Ch 0: 110
            10 + 200, // Ch 1: 210
            10 + 300, // Ch 2: 310
            10 + 400, // Ch 3: 410
            10 + 500  // Ch 4: 510
        };

        bool psm_burst_ok = (captured_outputs.size() == expected_outputs.size());
        if (psm_burst_ok)
        {
            for (size_t i = 0; i < expected_outputs.size(); i++)
            {
                if (captured_outputs[i] != expected_outputs[i])
                {
                    psm_burst_ok = false;
                    break;
                }
            }
        }

        total_tests++;
        if (psm_burst_ok)
        {
            tests_passed++;
            std::cout << "  [PASS] PSM Discontinuous Valid Burst verified successfully!" << std::endl;
            for (size_t i = 0; i < captured_outputs.size(); i++)
            {
                std::cout << "    Burst Element " << i << " (Ch " << i << "): got " 
                          << captured_outputs[i] << " (exp " << expected_outputs[i] << ")" << std::endl;
            }
        }
        else
        {
            std::cout << "  [FAIL] PSM Discontinuous Valid Burst mismatch!" << std::endl;
            std::cout << "    Expected " << expected_outputs.size() << " outputs, got " << captured_outputs.size() << std::endl;
            for (size_t i = 0; i < captured_outputs.size(); i++)
            {
                std::cout << "    Output[" << i << "] = " << captured_outputs[i] 
                          << " (expected " << (i < expected_outputs.size() ? expected_outputs[i] : -1) << ")" << std::endl;
            }
        }

        // ----------------------------------------------------
        // Case 9: Channels >= 32 with Full Pipeline (Vec-Mode, Requant Rounding, and LUT)
        // Verifies no out-of-bounds in lut_ram[MAX_CHANNELS][256] when channel_idx >= Y_DIM (Y_DIM=16, Ch=32, 45, 63)
        // Verifies multiple lanes (not just lane 0) across all stages
        // ----------------------------------------------------
        std::cout << "\n--- CASE 9: Channels >= 32 with Requant Rounding & LUT (Vec-Mode) ---" << std::endl;
        run_reset();

        vec_channel_mode.write(true);
        bias_en.write(true);
        requant_en.write(true);
        lut_en.write(true);

        // 1. Program Channel 32: Bias=+10, Scale=50, Shift=6, LUT: y = -x (negation)
        host_write(0x00150000 + 32 * 4, 10);
        host_write(0x00140000 + 0x40000 + 32 * 4, 50);
        host_write(0x00140000 + 0x50000 + 32 * 4, 6);
        for (int idx = 0; idx < 256; idx += 4)
        {
            uint32_t val = 0;
            for (int i = 0; i < 4; i++)
            {
                int8_t x = static_cast<int8_t>((idx + i) - 128);
                int8_t y = static_cast<int8_t>(-x);
                val |= (static_cast<uint32_t>(static_cast<uint8_t>(y)) << (i * 8));
            }
            host_write(0x00140000 + (32 * 256) + idx, val);
        }

        // 2. Program Channel 45: Bias=-20, Scale=128, Shift=7 (1.0x), LUT: y = abs(x)
        host_write(0x00150000 + 45 * 4, static_cast<uint32_t>(-20));
        host_write(0x00140000 + 0x40000 + 45 * 4, 128);
        host_write(0x00140000 + 0x50000 + 45 * 4, 7);
        for (int idx = 0; idx < 256; idx += 4)
        {
            uint32_t val = 0;
            for (int i = 0; i < 4; i++)
            {
                int8_t x = static_cast<int8_t>((idx + i) - 128);
                uint8_t y = static_cast<uint8_t>(std::abs(x));
                val |= (static_cast<uint32_t>(y) << (i * 8));
            }
            host_write(0x00140000 + (45 * 256) + idx, val);
        }

        // 3. Program Channel 63: Bias=0, Scale=1, Shift=0, LUT: identity
        host_write(0x00150000 + 63 * 4, 0);
        host_write(0x00140000 + 0x40000 + 63 * 4, 1);
        host_write(0x00140000 + 0x50000 + 63 * 4, 0);
        for (int idx = 0; idx < 256; idx += 4)
        {
            uint32_t val = 0;
            for (int i = 0; i < 4; i++)
            {
                int8_t x = static_cast<int8_t>((idx + i) - 128);
                val |= (static_cast<uint32_t>(static_cast<uint8_t>(x)) << (i * 8));
            }
            host_write(0x00140000 + (63 * 256) + idx, val);
        }

        // Stream 3 vectors corresponding to channels 32, 45, 63
        std::vector<psum_vector_t<TEST_Y_DIM, int32_t>> captured_c9_vectors;
        auto step_c9 = [&]() {
            wait();
            if (o_valid.read() && o_sramc_wren.read())
            {
                captured_c9_vectors.push_back(o_sramc_wdata.read());
            }
        };

        psum_vector_t<TEST_Y_DIM, int32_t> in_v32, in_v45, in_v63;
        for (int l = 0; l < TEST_Y_DIM; l++)
        {
            in_v32[l] = (l * 4);         // [0, 4, 8, ..., 60]
            in_v45[l] = (l * 6) - 10;    // [-10, -4, 2, ..., 80]
            in_v63[l] = (l * 3) - 25;    // [-25, -22, ..., 20]
        }

        // Feed Ch 32
        i_data.write(in_v32);
        i_channel_idx.write(32);
        i_valid.write(true);
        step_c9();

        // Feed Ch 45
        i_data.write(in_v45);
        i_channel_idx.write(45);
        step_c9();

        // Feed Ch 63
        i_data.write(in_v63);
        i_channel_idx.write(63);
        step_c9();

        i_valid.write(false);
        for (int drain = 0; drain < 6; drain++) step_c9();

        bool c9_ok = (captured_c9_vectors.size() == 3);
        if (c9_ok)
        {
            // Verify Ch 32: biased = in + 10, requant: (biased * 50 + 32) >> 6, LUT: -requant
            for (int l = 0; l < TEST_Y_DIM; l++)
            {
                int32_t biased = in_v32[l] + 10;
                int64_t prod = static_cast<int64_t>(biased) * 50 + 32;
                int32_t req = static_cast<int32_t>(prod >> 6);
                int8_t clamped = (req > 127) ? 127 : ((req < -128) ? -128 : static_cast<int8_t>(req));
                int8_t expected_lut = static_cast<int8_t>(-clamped);
                int32_t got = captured_c9_vectors[0][l];
                if (got != expected_lut)
                {
                    c9_ok = false;
                    std::cout << "  [FAIL] Ch 32 Lane " << l << ": exp " << static_cast<int>(expected_lut)
                              << " got " << got << std::endl;
                }
            }

            // Verify Ch 45: biased = in - 20, requant: (biased * 128 + 64) >> 7, LUT: abs(requant)
            for (int l = 0; l < TEST_Y_DIM; l++)
            {
                int32_t biased = in_v45[l] - 20;
                int64_t prod = static_cast<int64_t>(biased) * 128 + 64;
                int32_t req = static_cast<int32_t>(prod >> 7);
                int8_t clamped = (req > 127) ? 127 : ((req < -128) ? -128 : static_cast<int8_t>(req));
                int8_t expected_lut = static_cast<int8_t>(std::abs(clamped));
                int32_t got = captured_c9_vectors[1][l];
                if (got != expected_lut)
                {
                    c9_ok = false;
                    std::cout << "  [FAIL] Ch 45 Lane " << l << ": exp " << static_cast<int>(expected_lut)
                              << " got " << got << std::endl;
                }
            }
        }

        total_tests++;
        if (c9_ok)
        {
            tests_passed++;
            std::cout << "  [PASS] Channels >= 32 with per-channel Requant Rounding & LUT verified across all "
                      << TEST_Y_DIM << " lanes without out-of-bounds error." << std::endl;
        }
        else
        {
            std::cout << "  [FAIL] Channels >= 32 verification failed." << std::endl;
        }

        // ----------------------------------------------------
        // Case 10: Symmetric Round-Half-Away-From-Zero (TFLite Alignment & Shift Guard)
        // ----------------------------------------------------
        std::cout << "\n--- CASE 10: Symmetric Negative Tie Rounding (TFLite Alignment) ---" << std::endl;
        run_reset();

        bias_en.write(false);
        requant_en.write(true);
        lut_en.write(false);
        residual_en.write(false);
        vec_channel_mode.write(false);
        requant_scale.write(1);
        requant_shift.write(1); // shift = 1: divide by 2

        psum_vector_t<TEST_Y_DIM, int32_t> in_v_ties;
        // Lane 0: -5 -> -2.5 -> -3 (TFLite symmetric round-half-away-from-zero)
        in_v_ties[0] = -5;
        // Lane 1: +5 -> +2.5 -> +3
        in_v_ties[1] = 5;
        // Lane 2: -4 -> -2.0 -> -2
        in_v_ties[2] = -4;
        // Lane 3: +4 -> +2.0 -> +2
        in_v_ties[3] = 4;
        // Lane 4: -3 -> -1.5 -> -2
        in_v_ties[4] = -3;
        // Lane 5: +3 -> +1.5 -> +2
        in_v_ties[5] = 3;
        // Lane 6: -1 -> -0.5 -> -1
        in_v_ties[6] = -1;
        // Lane 7: +1 -> +0.5 -> +1
        in_v_ties[7] = 1;

        std::vector<psum_vector_t<TEST_Y_DIM, int32_t>> captured_c10_vectors;
        auto step_c10 = [&]() {
            wait();
            if (o_valid.read())
            {
                captured_c10_vectors.push_back(o_sramc_wdata.read());
            }
        };

        i_data.write(in_v_ties);
        i_valid.write(true);
        step_c10();
        i_valid.write(false);
        for (int drain = 0; drain < 6; drain++) step_c10();

        bool c10_ok = (captured_c10_vectors.size() == 1);
        if (c10_ok)
        {
            int32_t exp_ties[8] = {-3, +3, -2, +2, -2, +2, -1, +1};
            for (int l = 0; l < 8; l++)
            {
                int32_t got = captured_c10_vectors[0][l];
                if (got != exp_ties[l])
                {
                    c10_ok = false;
                    std::cout << "  [FAIL] Tie Rounding Lane " << l << ": expected " << exp_ties[l]
                              << ", got " << got << std::endl;
                }
            }
        }

        total_tests++;
        if (c10_ok)
        {
            tests_passed++;
            std::cout << "  [PASS] Symmetric Round-Half-Away-From-Zero verified: -5>>1 -> -3, +5>>1 -> +3, -3>>1 -> -2, +3>>1 -> +2." << std::endl;
        }
        else
        {
            std::cout << "  [FAIL] Symmetric Round-Half-Away-From-Zero mismatch." << std::endl;
        }

        // Test shift >= 64 safety guard (no undefined behavior)
        requant_shift.write(64);
        captured_c10_vectors.clear();
        i_data.write(in_v_ties);
        i_valid.write(true);
        step_c10();
        i_valid.write(false);
        for (int drain = 0; drain < 6; drain++) step_c10();

        bool c10_shift_ok = (captured_c10_vectors.size() == 1);
        if (c10_shift_ok)
        {
            for (int l = 0; l < TEST_Y_DIM; l++)
            {
                if (captured_c10_vectors[0][l] != 0) c10_shift_ok = false;
            }
        }
        total_tests++;
        if (c10_shift_ok)
        {
            tests_passed++;
            std::cout << "  [PASS] Shift >= 64 unbounded register safety guard: outputs zero without undefined behavior." << std::endl;
        }
        else
        {
            std::cout << "  [FAIL] Shift >= 64 guard failed." << std::endl;
        }

        // ---------------------------------------------------------------------
        // CASE 11: Asymmetric Output Zero-Point (Z_out != 0) Verification
        // ---------------------------------------------------------------------
        std::cout << "\n--- CASE 11: Asymmetric Output Zero-Point (Z_out != 0) ---" << std::endl;
        run_reset();
        requant_en.write(true);
        requant_scale.write(1);
        requant_shift.write(0);
        output_zp.write(15); // Default scalar output zero point +15

        psum_vector_t<TEST_Y_DIM, int32_t> in_v_zp;
        int32_t raw_zp_in[8] = {10, -20, 30, -40, 50, -60, 120, -128};
        int32_t exp_zp_out[8] = {25, -5, 45, -25, 65, -45, 127, -113}; // clamped at 127
        for (int l = 0; l < TEST_Y_DIM; l++) in_v_zp[l] = raw_zp_in[l % 8];

        std::vector<psum_vector_t<TEST_Y_DIM, int32_t>> captured_zp_vectors;
        auto step_zp = [&]() {
            wait();
            if (o_valid.read()) captured_zp_vectors.push_back(o_sramc_wdata.read());
        };

        i_data.write(in_v_zp);
        i_valid.write(true);
        step_zp();
        i_valid.write(false);
        for (int drain = 0; drain < 6; drain++) step_zp();

        bool zp_scalar_ok = (captured_zp_vectors.size() == 1);
        if (zp_scalar_ok)
        {
            for (int l = 0; l < 8; l++)
            {
                if (captured_zp_vectors[0][l] != exp_zp_out[l])
                {
                    zp_scalar_ok = false;
                    std::cout << "  [FAIL] Scalar Zero-Point Lane " << l << ": expected " << exp_zp_out[l]
                              << ", got " << captured_zp_vectors[0][l] << std::endl;
                }
            }
        }
        total_tests++;
        if (zp_scalar_ok)
        {
            tests_passed++;
            std::cout << "  [PASS] Scalar Output Zero-Point (+15) with saturation clamp verified." << std::endl;
        }
        else
        {
            std::cout << "  [FAIL] Scalar Output Zero-Point mismatch." << std::endl;
        }

        // Test Host MMIO Per-Channel Zero-Point RAM (0x001C0000)
        host_write(0x001C0000, static_cast<uint32_t>(-30)); // Channel 0 zp = -30
        uint32_t rd_zp = host_read(0x001C0000);
        bool zp_mmio_ok = (static_cast<int32_t>(rd_zp) == -30);

        vec_channel_mode.write(true);
        i_channel_idx.write(0);
        captured_zp_vectors.clear();
        i_data.write(in_v_zp);
        i_valid.write(true);
        step_zp();
        i_valid.write(false);
        for (int drain = 0; drain < 6; drain++) step_zp();

        if (captured_zp_vectors.size() == 1)
        {
            // Input 10 - 30 = -20
            if (captured_zp_vectors[0][0] != -20) zp_mmio_ok = false;
        }
        else
        {
            zp_mmio_ok = false;
        }
        total_tests++;
        if (zp_mmio_ok)
        {
            tests_passed++;
            std::cout << "  [PASS] Per-Channel Zero-Point MMIO Programming (-30) and execution verified: got -20." << std::endl;
        }
        else
        {
            std::cout << "  [FAIL] Per-Channel Zero-Point MMIO Programming failed." << std::endl;
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
