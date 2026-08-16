// Standalone Testbench for Reduction Engine (RE) & Reconfigurable Compute Engine (RCE) Module
// Verifies all operational modes: Softmax, LayerNorm, MaxPool, and Residual Skip Addition.
//
// Build: make tb_re
// Run:   ./tb_re

#include <systemc.h>
#include <cstdint>
#include <iostream>
#include <iomanip>
#include <vector>
#include <cmath>
#include <cstring>
#include "sauria_types.h"
#include "psm/re_rce.h"

using namespace sauria;

constexpr int TEST_Y_DIM = 8;

typedef ReconfigurableEngine<0x00200000, 0x00210000, 0x00220000> RceDut;
typedef ReductionEngine<TEST_Y_DIM, float, float> ReDut;

SC_MODULE(TbRe)
{
    sc_in<bool> i_clk;

    // DUT Signals
    sc_signal<bool> rstn{"rstn"};

    // RCE Signals
    sc_signal<uint32_t> host_addr{"host_addr"};
    sc_signal<bool> host_wren{"host_wren"};
    sc_signal<bool> host_rden{"host_rden"};
    sc_signal<host_data_t> host_wdata{"host_wdata"};
    sc_signal<host_mask_t> host_wmask{"host_wmask"};
    sc_signal<host_data_t> host_rdata{"host_rdata"};

    // RCE test interface
    sc_signal<uint32_t> test_lut_op{"test_lut_op"};
    sc_signal<float> test_lut_in{"test_lut_in"};
    sc_signal<bool> test_lut_in_valid{"test_lut_in_valid"};
    sc_signal<float> test_lut_out{"test_lut_out"};
    sc_signal<bool> test_lut_out_valid{"test_lut_out_valid"};

    // RE dummy interface
    sc_signal<uint32_t> dummy_lut_op{"dummy_lut_op"};
    sc_signal<float> dummy_lut_in{"dummy_lut_in"};
    sc_signal<bool> dummy_lut_in_valid{"dummy_lut_in_valid"};
    sc_signal<float> dummy_lut_out{"dummy_lut_out"};
    sc_signal<bool> dummy_lut_out_valid{"dummy_lut_out_valid"};

    // RE Signals
    sc_signal<uint32_t> i_mode{"i_mode"};
    sc_signal<bool> i_start{"i_start"};
    sc_signal<bool> i_valid{"i_valid"};
    sc_signal<psum_vector_t<TEST_Y_DIM, float>> i_vector_data{"i_vector_data"};
    sc_signal<act_vector_t<TEST_Y_DIM, float>> i_skip_data{"i_skip_data"};
    sc_signal<uint32_t> i_requant_scale{"i_requant_scale"};
    sc_signal<uint32_t> i_requant_shift{"i_requant_shift"};

    sc_signal<psum_vector_t<TEST_Y_DIM, float>> o_vector_out{"o_vector_out"};
    sc_signal<bool> o_valid{"o_valid"};
    sc_signal<bool> o_done{"o_done"};

    RceDut *rce_dut{nullptr};
    ReDut *re_dut{nullptr};

    int total_tests = 0;
    int tests_passed = 0;

    SC_CTOR(TbRe)
    {
        // RCE Instantiation & Binding
        rce_dut = new RceDut("rce_dut");
        rce_dut->i_clk(i_clk);
        rce_dut->i_rstn(rstn);
        rce_dut->i_host_addr(host_addr);
        rce_dut->i_host_wren(host_wren);
        rce_dut->i_host_rden(host_rden);
        rce_dut->i_host_wdata(host_wdata);
        rce_dut->i_host_wmask(host_wmask);
        rce_dut->o_host_rdata(host_rdata);

        rce_dut->i_lut_op(test_lut_op);
        rce_dut->i_lut_in(test_lut_in);
        rce_dut->i_lut_valid(test_lut_in_valid);
        rce_dut->o_lut_out(test_lut_out);
        rce_dut->o_lut_valid(test_lut_out_valid);

        // RE Instantiation & Binding
        re_dut = new ReDut("re_dut");
        re_dut->i_clk(i_clk);
        re_dut->i_rstn(rstn);
        re_dut->i_mode(i_mode);
        re_dut->i_start(i_start);
        re_dut->i_valid(i_valid);
        re_dut->i_vector_data(i_vector_data);
        re_dut->i_skip_data(i_skip_data);
        re_dut->i_requant_scale(i_requant_scale);
        re_dut->i_requant_shift(i_requant_shift);

        // RE dummy bindings
        re_dut->o_lut_op(dummy_lut_op);
        re_dut->o_lut_in(dummy_lut_in);
        re_dut->o_lut_valid(dummy_lut_in_valid);
        re_dut->i_lut_out(dummy_lut_out);
        re_dut->i_lut_valid(dummy_lut_out_valid);

        re_dut->o_vector_out(o_vector_out);
        re_dut->o_valid(o_valid);
        re_dut->o_done(o_done);

        SC_THREAD(test_process);
        sensitive << i_clk.pos();
    }

    ~TbRe()
    {
        delete rce_dut;
        delete re_dut;
    }

    // Host Programming Helper Methods
    void host_write_lut(uint32_t addr, uint32_t val)
    {
        host_data_t d;
        d.data.fill(0.0);
        uint32_t region = addr & 0x00FF0000;
        if (region == 0x00200000 || region == 0x00210000) // EXP or RECIP
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

    void host_write_rsqrt(uint32_t addr, uint16_t e0, uint16_t e1, uint16_t e2, uint16_t e3)
    {
        host_data_t d;
        d[0] = e0;
        d[1] = e1;
        d[2] = e2;
        d[3] = e3;
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

    void host_read_lut(uint32_t addr, double &e0, double &e1, double &e2, double &e3)
    {
        host_addr.write(addr);
        host_rden.write(true);
        host_wren.write(false);
        wait();
        wait();
        host_data_t r = host_rdata.read();
        host_rden.write(false);
        wait();
        e0 = r[0];
        e1 = r[1];
        e2 = r[2];
        e3 = r[3];
    }

    void run_reset()
    {
        rstn.write(false);
        host_wren.write(false);
        host_rden.write(false);
        test_lut_op.write(0);
        test_lut_in.write(0.0f);
        test_lut_in_valid.write(false);
        i_mode.write(RE_MODE_IDLE);
        i_start.write(false);
        i_valid.write(false);
        i_requant_scale.write(1);
        i_requant_shift.write(0);
        
        psum_vector_t<TEST_Y_DIM, float> zero_vec;
        i_vector_data.write(zero_vec);
        act_vector_t<TEST_Y_DIM, float> zero_skip;
        i_skip_data.write(zero_skip);

        wait(5);
        rstn.write(true);
        wait(2);
    }

    void test_process()
    {
        std::cout << "\n==================================================" << std::endl;
        std::cout << "         RE STANDALONE TESTBENCH" << std::endl;
        std::cout << "==================================================\n" << std::endl;

        // ----------------------------------------------------
        // Case 1: RCE Host Programming & Readback
        // ----------------------------------------------------
        std::cout << "--- CASE 1: RCE LUT Host Write & Readback ---" << std::endl;
        run_reset();

        // Program EXP LUT
        host_write_lut(0x00200000 + 100, 0x04030201); // Entries 100, 101, 102, 103 -> 1, 2, 3, 4
        // Program RECIP LUT
        host_write_lut(0x00210000 + 200, 0x08070605); // Entries 200, 201, 202, 203 -> 5, 6, 7, 8
        // Program RSQRT LUT
        host_write_rsqrt(0x00220000 + (300 * 2), 10, 11, 12, 13); // Entries 300, 301, 302, 303 -> 10, 11, 12, 13

        // Verify EXP LUT
        double e0, e1, e2, e3;
        host_read_lut(0x00200000 + 100, e0, e1, e2, e3);
        bool exp_ok = (e0 == 1 && e1 == 2 && e2 == 3 && e3 == 4);
        total_tests++;
        if (exp_ok)
        {
            tests_passed++;
            std::cout << "  [PASS] EXP LUT Write/Read verified: " << e0 << ", " << e1 << ", " << e2 << ", " << e3 << std::endl;
        }
        else
        {
            std::cout << "  [FAIL] EXP LUT expected 1,2,3,4 got " << e0 << "," << e1 << "," << e2 << "," << e3 << std::endl;
        }

        // Verify RECIP LUT
        host_read_lut(0x00210000 + 200, e0, e1, e2, e3);
        bool recip_ok = (e0 == 5 && e1 == 6 && e2 == 7 && e3 == 8);
        total_tests++;
        if (recip_ok)
        {
            tests_passed++;
            std::cout << "  [PASS] RECIP LUT Write/Read verified: " << e0 << ", " << e1 << ", " << e2 << ", " << e3 << std::endl;
        }
        else
        {
            std::cout << "  [FAIL] RECIP LUT expected 5,6,7,8 got " << e0 << "," << e1 << "," << e2 << "," << e3 << std::endl;
        }

        // Verify RSQRT LUT
        host_read_lut(0x00220000 + (300 * 2), e0, e1, e2, e3);
        bool rsqrt_ok = (e0 == 10 && e1 == 11 && e2 == 12 && e3 == 13);
        total_tests++;
        if (rsqrt_ok)
        {
            tests_passed++;
            std::cout << "  [PASS] RSQRT LUT Write/Read verified: " << e0 << ", " << e1 << ", " << e2 << ", " << e3 << std::endl;
        }
        else
        {
            std::cout << "  [FAIL] RSQRT LUT expected 10,11,12,13 got " << e0 << "," << e1 << "," << e2 << "," << e3 << std::endl;
        }

        // ----------------------------------------------------
        // Case 2: RCE Pipelined Functional Lookup
        // ----------------------------------------------------
        std::cout << "\n--- CASE 2: RCE Functional Lookup ---" << std::endl;
        // Verify RCE EXP functional mapping
        // index formula for EXP: round((x + 8) * 31.875)
        // Let's choose x = -4.8627 -> idx = round((-4.8627 + 8) * 31.875) = round(3.1373 * 31.875) = round(100.00) = 100.
        // We programmed index 100 with value 1.
        // lookup_exp(x) should return 1.0f / 255.0f = 0.00392157f.
        test_lut_op.write(LUT_OP_EXP);
        test_lut_in.write(-4.8627f);
        test_lut_in_valid.write(true);
        wait();
        test_lut_in_valid.write(false);
        wait(); // wait for update

        float out_exp = test_lut_out.read();
        float exp_exp = 1.0f / 255.0f;
        total_tests++;
        if (std::abs(out_exp - exp_exp) < 1e-5f)
        {
            tests_passed++;
            std::cout << "  [PASS] EXP functional lookup passed: got " << out_exp << std::endl;
        }
        else
        {
            std::cout << "  [FAIL] EXP lookup: expected " << exp_exp << " got " << out_exp << std::endl;
        }

        // ----------------------------------------------------
        // Case 3: RE Softmax Pass 1 (Max Finder)
        // ----------------------------------------------------
        std::cout << "\n--- CASE 3: RE Softmax Pass 1 (Max Finder) ---" << std::endl;
        run_reset();

        psum_vector_t<TEST_Y_DIM, float> input_vec;
        for (int i = 0; i < TEST_Y_DIM; i++)
        {
            input_vec[i] = static_cast<float>(i + 1) * 2.5f; // [2.5, 5.0, 7.5, ..., 20.0]
        }

        i_vector_data.write(input_vec);
        i_mode.write(RE_MODE_SOFTMAX_PASS1);
        i_start.write(true);
        i_valid.write(true);
        wait();
        std::cout << "    [Cycle 1] i_start=1 i_valid=1 o_valid=" << o_valid.read()
                  << " o_done=" << o_done.read() << " out[0]=" << o_vector_out.read()[0] << std::endl;
        i_start.write(false);
        i_valid.write(false);
        wait();
        std::cout << "    [Cycle 2] i_start=0 i_valid=0 o_valid=" << o_valid.read()
                  << " o_done=" << o_done.read() << " out[0]=" << o_vector_out.read()[0] << std::endl;

        float got_max = o_vector_out.read()[0];
        float exp_max = 20.0f;
        total_tests++;
        if (got_max == exp_max && o_valid.read() && o_done.read())
        {
            tests_passed++;
            std::cout << "  [PASS] Softmax Pass 1 Max Finder: got " << got_max << " (exp " << exp_max << ")" << std::endl;
        }
        else
        {
            std::cout << "  [FAIL] Softmax Pass 1 Max Finder: expected " << exp_max << " got " << got_max << std::endl;
        }

        // ----------------------------------------------------
        // Case 4: RE Softmax Pass 2 (Normalization)
        // ----------------------------------------------------
        std::cout << "\n--- CASE 4: RE Softmax Pass 2 (Normalization) ---" << std::endl;
        // The internal Softmax Pass 2 reads the original vector from scratch_mem[0] 
        // and computes: exp(x_i - running_max) / sum(exp(x_j - running_max))
        // Let's invoke Softmax Pass 2
        i_mode.write(RE_MODE_SOFTMAX_PASS2);
        i_start.write(true);
        i_valid.write(true);
        wait();
        std::cout << "    [Cycle 1] i_start=1 i_valid=1 o_valid=" << o_valid.read()
                  << " o_done=" << o_done.read() << " out[0]=" << o_vector_out.read()[0] << std::endl;
        i_start.write(false);
        i_valid.write(false);
        wait();
        std::cout << "    [Cycle 2] i_start=0 i_valid=0 o_valid=" << o_valid.read()
                  << " o_done=" << o_done.read() << " out[0]=" << o_vector_out.read()[0] << std::endl;

        psum_vector_t<TEST_Y_DIM, float> got_softmax = o_vector_out.read();
        
        // Calculate golden software softmax
        double sum_exp = 0.0;
        std::vector<double> golden_softmax(TEST_Y_DIM);
        for (int i = 0; i < TEST_Y_DIM; i++)
        {
            double diff = (i + 1) * 2.5 - 20.0;
            double val = std::exp(diff);
            golden_softmax[i] = val;
            sum_exp += val;
        }
        for (int i = 0; i < TEST_Y_DIM; i++)
        {
            golden_softmax[i] /= sum_exp;
        }

        bool softmax_ok = true;
        for (int i = 0; i < TEST_Y_DIM; i++)
        {
            float got = got_softmax[i];
            float exp = static_cast<float>(golden_softmax[i]);
            if (std::abs(got - exp) > 1e-4f)
            {
                softmax_ok = false;
                std::cout << "  [FAIL] Softmax Pass 2 lane " << i << ": expected " << exp << ", got " << got << std::endl;
            }
        }
        total_tests++;
        if (softmax_ok)
        {
            tests_passed++;
            std::cout << "  [PASS] Softmax Pass 2 Normalization verified." << std::endl;
            std::cout << "    Softmax vector output: " << got_softmax << std::endl;
        }

        // ----------------------------------------------------
        // Case 5: RE LayerNorm Pass 1 (Mean Finder)
        // ----------------------------------------------------
        std::cout << "\n--- CASE 5: RE LayerNorm Pass 1 (Mean Finder) ---" << std::endl;
        run_reset();

        psum_vector_t<TEST_Y_DIM, float> ln_input_vec;
        double ln_sum = 0.0;
        for (int i = 0; i < TEST_Y_DIM; i++)
        {
            ln_input_vec[i] = static_cast<float>(i + 1) * 10.0f; // [10.0, 20.0, ..., 80.0]
            ln_sum += ln_input_vec[i];
        }
        double ln_mean = ln_sum / TEST_Y_DIM; // 45.0

        i_vector_data.write(ln_input_vec);
        i_mode.write(RE_MODE_LAYERNORM_PASS1);
        i_start.write(true);
        i_valid.write(true);
        wait();
        std::cout << "    [Cycle 1] i_start=1 i_valid=1 o_valid=" << o_valid.read()
                  << " o_done=" << o_done.read() << " out[0]=" << o_vector_out.read()[0] << std::endl;
        i_start.write(false);
        i_valid.write(false);
        wait();
        std::cout << "    [Cycle 2] i_start=0 i_valid=0 o_valid=" << o_valid.read()
                  << " o_done=" << o_done.read() << " out[0]=" << o_vector_out.read()[0] << std::endl;

        float got_mean = o_vector_out.read()[0];
        total_tests++;
        if (got_mean == static_cast<float>(ln_mean))
        {
            tests_passed++;
            std::cout << "  [PASS] LayerNorm Pass 1 Mean Finder: got " << got_mean << " (exp " << ln_mean << ")" << std::endl;
        }
        else
        {
            std::cout << "  [FAIL] LayerNorm Pass 1 Mean Finder: expected " << ln_mean << " got " << got_mean << std::endl;
        }

        // ----------------------------------------------------
        // Case 6: RE LayerNorm Pass 2 (Variance & Scaling)
        // ----------------------------------------------------
        std::cout << "\n--- CASE 6: RE LayerNorm Pass 2 (Variance & Scaling) ---" << std::endl;
        i_mode.write(RE_MODE_LAYERNORM_PASS2);
        i_start.write(true);
        i_valid.write(true);
        wait();
        std::cout << "    [Cycle 1] i_start=1 i_valid=1 o_valid=" << o_valid.read()
                  << " o_done=" << o_done.read() << " out[0]=" << o_vector_out.read()[0] << std::endl;
        i_start.write(false);
        i_valid.write(false);
        wait();
        std::cout << "    [Cycle 2] i_start=0 i_valid=0 o_valid=" << o_valid.read()
                  << " o_done=" << o_done.read() << " out[0]=" << o_vector_out.read()[0] << std::endl;

        psum_vector_t<TEST_Y_DIM, float> got_ln = o_vector_out.read();

        // Calculate golden variance
        double sq_sum = 0.0;
        for (int i = 0; i < TEST_Y_DIM; i++)
        {
            double diff = ln_input_vec[i] - ln_mean;
            sq_sum += diff * diff;
        }
        double ln_var = sq_sum / TEST_Y_DIM;
        double ln_rsqrt = 1.0 / std::sqrt(ln_var + 1e-5);

        bool ln_ok = true;
        for (int i = 0; i < TEST_Y_DIM; i++)
        {
            float got = got_ln[i];
            float exp = static_cast<float>((ln_input_vec[i] - ln_mean) * ln_rsqrt);
            if (std::abs(got - exp) > 1e-4f)
            {
                ln_ok = false;
                std::cout << "  [FAIL] LayerNorm Pass 2 lane " << i << ": expected " << exp << ", got " << got << std::endl;
            }
        }
        total_tests++;
        if (ln_ok)
        {
            tests_passed++;
            std::cout << "  [PASS] LayerNorm Pass 2 Normalization verified." << std::endl;
            std::cout << "    LayerNorm vector output: " << got_ln << std::endl;
        }

        // ----------------------------------------------------
        // Case 7: RE MaxPool (Broadcast Max)
        // ----------------------------------------------------
        std::cout << "\n--- CASE 7: RE MaxPool (Broadcast Max) ---" << std::endl;
        run_reset();

        psum_vector_t<TEST_Y_DIM, float> maxpool_in;
        maxpool_in[0] = 12.0f;
        maxpool_in[1] = 99.5f; // maximum
        maxpool_in[2] = -4.0f;
        maxpool_in[3] = 45.0f;
        maxpool_in[4] = 0.0f;
        maxpool_in[5] = 99.0f;
        maxpool_in[6] = 2.0f;
        maxpool_in[7] = -99.9f;

        i_vector_data.write(maxpool_in);
        i_mode.write(RE_MODE_MAXPOOL);
        i_start.write(true);
        i_valid.write(true);
        wait();
        std::cout << "    [Cycle 1] i_start=1 i_valid=1 o_valid=" << o_valid.read()
                  << " o_done=" << o_done.read() << " out[0]=" << o_vector_out.read()[0] << std::endl;
        i_start.write(false);
        i_valid.write(false);
        wait();
        std::cout << "    [Cycle 2] i_start=0 i_valid=0 o_valid=" << o_valid.read()
                  << " o_done=" << o_done.read() << " out[0]=" << o_vector_out.read()[0] << std::endl;

        psum_vector_t<TEST_Y_DIM, float> got_maxpool = o_vector_out.read();
        bool maxpool_ok = true;
        for (int i = 0; i < TEST_Y_DIM; i++)
        {
            if (got_maxpool[i] != 99.5f)
            {
                maxpool_ok = false;
                std::cout << "  [FAIL] MaxPool lane " << i << ": expected 99.5, got " << got_maxpool[i] << std::endl;
            }
        }
        total_tests++;
        if (maxpool_ok)
        {
            tests_passed++;
            std::cout << "  [PASS] MaxPool broadcasted max value (99.5) to all lanes." << std::endl;
        }

        // ----------------------------------------------------
        // Case 8: RE Residual skip addition
        // ----------------------------------------------------
        std::cout << "\n--- CASE 8: RE Residual Skip Addition ---" << std::endl;
        run_reset();

        psum_vector_t<TEST_Y_DIM, float> res_in_vec;
        act_vector_t<TEST_Y_DIM, float> res_skip_vec;
        for (int i = 0; i < TEST_Y_DIM; i++)
        {
            res_in_vec[i] = static_cast<float>((i + 1) * 15);   // [15, 30, ..., 120]
            res_skip_vec[i] = static_cast<float>((i + 1) * 5);  // [5, 10, ..., 40]
        }

        i_vector_data.write(res_in_vec);
        i_skip_data.write(res_skip_vec);
        // scale multiplier = 50, right shift = 6 -> factor = 50/64 = 0.78125
        i_requant_scale.write(50);
        i_requant_shift.write(6);
        i_mode.write(RE_MODE_RESIDUAL_ADD);
        i_start.write(true);
        i_valid.write(true);
        wait();
        std::cout << "    [Cycle 1] i_start=1 i_valid=1 o_valid=" << o_valid.read()
                  << " o_done=" << o_done.read() << " out[0]=" << o_vector_out.read()[0] << std::endl;
        i_start.write(false);
        i_valid.write(false);
        wait();
        std::cout << "    [Cycle 2] i_start=0 i_valid=0 o_valid=" << o_valid.read()
                  << " o_done=" << o_done.read() << " out[0]=" << o_vector_out.read()[0] << std::endl;

        psum_vector_t<TEST_Y_DIM, float> got_res = o_vector_out.read();
        bool res_ok = true;
        for (int i = 0; i < TEST_Y_DIM; i++)
        {
            double sum_val = static_cast<double>(res_in_vec[i]) + static_cast<double>(res_skip_vec[i]);
            double scaled = (sum_val * 50) / 64.0;
            // clamp to INT8 range (-128 to 127) as per clamp_val<float>(sum_val)? 
            // Wait, clamp_val template parameters:
            // "clamp_val<T_ACT>(sum_val)" -> T_ACT = float. 
            // Since float is not integral, it is not clamped.
            float exp = static_cast<float>(scaled);
            if (std::abs(got_res[i] - exp) > 1e-4f)
            {
                res_ok = false;
                std::cout << "  [FAIL] Residual add lane " << i << ": expected " << exp << ", got " << got_res[i] << std::endl;
            }
        }
        total_tests++;
        if (res_ok)
        {
            tests_passed++;
            std::cout << "  [PASS] Residual skip addition matches expected scaled values." << std::endl;
            std::cout << "    Output: " << got_res << std::endl;
        }

        // Print final status summary
        std::cout << "\n==================================================" << std::endl;
        std::cout << "  TEST SUMMARY: " << tests_passed << " / " << total_tests << " Passed" << std::endl;
        std::cout << "  RESULT: " << (tests_passed == total_tests ? "SUCCESS" : "FAILURE") << std::endl;
        std::cout << "==================================================" << std::endl;

        sc_stop();
    }
};

int sc_main(int argc, char *argv[])
{
    sc_clock clk("clk", 2, SC_NS);
    TbRe tb("tb");
    tb.i_clk(clk);

    sc_start();
    return 0;
}
