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
typedef ReductionEngine<TEST_Y_DIM, int32_t, int8_t> ReIntDut;

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

    // RE Float Signals
    sc_signal<uint32_t> i_mode{"i_mode"};
    sc_signal<bool> i_start{"i_start"};
    sc_signal<bool> i_valid{"i_valid"};
    sc_signal<psum_vector_t<TEST_Y_DIM, float>> i_vector_data{"i_vector_data"};
    sc_signal<act_vector_t<TEST_Y_DIM, float>> i_skip_data{"i_skip_data"};
    sc_signal<uint32_t> i_addr{"i_addr"};
    sc_signal<sramc_mask_t<TEST_Y_DIM>> i_wmask{"i_wmask"};
    sc_signal<sramc_mask_t<TEST_Y_DIM>> i_rows_active{"i_rows_active"};
    sc_signal<uint32_t> i_requant_scale{"i_requant_scale"};
    sc_signal<uint32_t> i_requant_shift{"i_requant_shift"};

    sc_signal<psum_vector_t<TEST_Y_DIM, float>> o_vector_out{"o_vector_out"};
    sc_signal<uint32_t> o_addr{"o_addr"};
    sc_signal<sramc_mask_t<TEST_Y_DIM>> o_wmask{"o_wmask"};
    sc_signal<bool> o_valid{"o_valid"};
    sc_signal<bool> o_done{"o_done"};

    // RE Integer Datapath Signals
    sc_signal<uint32_t> int_mode{"int_mode"};
    sc_signal<bool> int_start{"int_start"};
    sc_signal<bool> int_valid{"int_valid"};
    sc_signal<psum_vector_t<TEST_Y_DIM, int32_t>> int_vector_data{"int_vector_data"};
    sc_signal<act_vector_t<TEST_Y_DIM, int8_t>> int_skip_data{"int_skip_data"};
    sc_signal<uint32_t> int_addr{"int_addr"};
    sc_signal<sramc_mask_t<TEST_Y_DIM>> int_wmask{"int_wmask"};
    sc_signal<sramc_mask_t<TEST_Y_DIM>> int_rows_active{"int_rows_active"};
    sc_signal<uint32_t> int_requant_scale{"int_requant_scale"};
    sc_signal<uint32_t> int_requant_shift{"int_requant_shift"};

    sc_signal<uint32_t> int_dummy_lut_op{"int_dummy_lut_op"};
    sc_signal<float> int_dummy_lut_in{"int_dummy_lut_in"};
    sc_signal<bool> int_dummy_lut_in_valid{"int_dummy_lut_in_valid"};
    sc_signal<float> int_dummy_lut_out{"int_dummy_lut_out"};
    sc_signal<bool> int_dummy_lut_out_valid{"int_dummy_lut_out_valid"};

    sc_signal<psum_vector_t<TEST_Y_DIM, int32_t>> int_vector_out{"int_vector_out"};
    sc_signal<uint32_t> int_addr_out{"int_addr_out"};
    sc_signal<sramc_mask_t<TEST_Y_DIM>> int_wmask_out{"int_wmask_out"};
    sc_signal<bool> int_valid_out{"int_valid_out"};
    sc_signal<bool> int_done_out{"int_done_out"};

    RceDut *rce_dut{nullptr};
    ReDut *re_dut{nullptr};
    ReIntDut *re_int_dut{nullptr};

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

        // RE Float Instantiation & Binding
        re_dut = new ReDut("re_dut");
        re_dut->i_clk(i_clk);
        re_dut->i_rstn(rstn);
        re_dut->i_mode(i_mode);
        re_dut->i_start(i_start);
        re_dut->i_valid(i_valid);
        re_dut->i_vector_data(i_vector_data);
        re_dut->i_skip_data(i_skip_data);
        re_dut->i_addr(i_addr);
        re_dut->i_wmask(i_wmask);
        re_dut->i_rows_active(i_rows_active);
        re_dut->i_requant_scale(i_requant_scale);
        re_dut->i_requant_shift(i_requant_shift);

        // RE Float dummy bindings
        re_dut->o_lut_op(dummy_lut_op);
        re_dut->o_lut_in(dummy_lut_in);
        re_dut->o_lut_valid(dummy_lut_in_valid);
        re_dut->i_lut_out(dummy_lut_out);
        re_dut->i_lut_valid(dummy_lut_out_valid);

        re_dut->o_vector_out(o_vector_out);
        re_dut->o_addr(o_addr);
        re_dut->o_wmask(o_wmask);
        re_dut->o_valid(o_valid);
        re_dut->o_done(o_done);

        // RE Integer Instantiation & Binding
        re_int_dut = new ReIntDut("re_int_dut");
        re_int_dut->i_clk(i_clk);
        re_int_dut->i_rstn(rstn);
        re_int_dut->i_mode(int_mode);
        re_int_dut->i_start(int_start);
        re_int_dut->i_valid(int_valid);
        re_int_dut->i_vector_data(int_vector_data);
        re_int_dut->i_skip_data(int_skip_data);
        re_int_dut->i_addr(int_addr);
        re_int_dut->i_wmask(int_wmask);
        re_int_dut->i_rows_active(int_rows_active);
        re_int_dut->i_requant_scale(int_requant_scale);
        re_int_dut->i_requant_shift(int_requant_shift);

        re_int_dut->o_lut_op(int_dummy_lut_op);
        re_int_dut->o_lut_in(int_dummy_lut_in);
        re_int_dut->o_lut_valid(int_dummy_lut_in_valid);
        re_int_dut->i_lut_out(int_dummy_lut_out);
        re_int_dut->i_lut_valid(int_dummy_lut_out_valid);

        re_int_dut->o_vector_out(int_vector_out);
        re_int_dut->o_addr(int_addr_out);
        re_int_dut->o_wmask(int_wmask_out);
        re_int_dut->o_valid(int_valid_out);
        re_int_dut->o_done(int_done_out);

        // Connect companion RCE to RE instances
        re_dut->set_rce(rce_dut);
        re_int_dut->set_rce(rce_dut);

        SC_THREAD(test_process);
        sensitive << i_clk.pos();
    }

    ~TbRe()
    {
        delete rce_dut;
        delete re_dut;
        delete re_int_dut;
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
        i_addr.write(0);
        sramc_mask_t<TEST_Y_DIM> full_mask;
        full_mask.data.fill(true);
        i_wmask.write(full_mask);
        i_rows_active.write(full_mask);
        i_requant_scale.write(1);
        i_requant_shift.write(0);
        
        psum_vector_t<TEST_Y_DIM, float> zero_vec;
        i_vector_data.write(zero_vec);
        act_vector_t<TEST_Y_DIM, float> zero_skip;
        i_skip_data.write(zero_skip);

        int_mode.write(RE_MODE_IDLE);
        int_start.write(false);
        int_valid.write(false);
        int_addr.write(0);
        int_wmask.write(full_mask);
        int_rows_active.write(full_mask);
        int_requant_scale.write(1);
        int_requant_shift.write(0);
        psum_vector_t<TEST_Y_DIM, int32_t> zero_int_vec;
        int_vector_data.write(zero_int_vec);
        act_vector_t<TEST_Y_DIM, int8_t> zero_int_skip;
        int_skip_data.write(zero_int_skip);

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

        float got_max = re_dut->get_running_max();
        float exp_max = 20.0f;
        bool pass1_suppressed = !o_valid.read();
        total_tests++;
        if (got_max == exp_max && pass1_suppressed)
        {
            tests_passed++;
            std::cout << "  [PASS] Softmax Pass 1 Max Finder: got " << got_max << " (exp " << exp_max << ") and SRAM C write suppressed." << std::endl;
        }
        else
        {
            std::cout << "  [FAIL] Softmax Pass 1 Max Finder: expected " << exp_max << " got " << got_max << ", o_valid=" << o_valid.read() << std::endl;
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

        float got_mean = re_dut->get_running_mean();
        bool ln_pass1_suppressed = !o_valid.read();
        total_tests++;
        if (got_mean == static_cast<float>(ln_mean) && ln_pass1_suppressed)
        {
            tests_passed++;
            std::cout << "  [PASS] LayerNorm Pass 1 Mean Finder: got " << got_mean << " (exp " << ln_mean << ") and SRAM C write suppressed." << std::endl;
        }
        else
        {
            std::cout << "  [FAIL] LayerNorm Pass 1 Mean Finder: expected " << ln_mean << " got " << got_mean << ", o_valid=" << o_valid.read() << std::endl;
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
            // Includes negative inputs to verify symmetric rounding across positive and negative values
            res_in_vec[i] = static_cast<float>((i - 4) * 15);   // [-60, -45, -30, -15, 0, 15, 30, 45]
            res_skip_vec[i] = static_cast<float>((i - 4) * 5);  // [-20, -15, -10, -5,  0,  5, 10, 15]
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
            double scaled = sum_val * 50.0;
            double divisor = static_cast<double>(1ULL << 6);
            float exp = static_cast<float>(std::round(scaled / divisor));
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

        bool lane0_literal_ok = (std::abs(got_res[0] - (-63.0f)) < 1e-4f);
        total_tests++;
        if (lane0_literal_ok)
        {
            tests_passed++;
            std::cout << "  [PASS] Residual add lane 0 literal value verified: got " << got_res[0] << " (expected -63.0f)." << std::endl;
        }
        else
        {
            std::cout << "  [FAIL] Residual add lane 0 literal mismatch: expected -63.0f, got " << got_res[0] << std::endl;
        }

        // ----------------------------------------------------
        // Case 9: Multi-Vector Tile Softmax with Scratch SRAM & Address Alignment
        // ----------------------------------------------------
        std::cout << "\n--- CASE 9: Multi-Vector Tile Softmax & Address Alignment ---" << std::endl;
        run_reset();

        constexpr int NUM_VECTORS = 3;
        psum_vector_t<TEST_Y_DIM, float> tile_in[NUM_VECTORS];
        for (int v = 0; v < NUM_VECTORS; v++)
        {
            for (int l = 0; l < TEST_Y_DIM; l++)
            {
                tile_in[v][l] = static_cast<float>(v * 10 + l); // V0: 0..7, V1: 10..17, V2: 20..27
            }
        }
        float expected_tile_max = 27.0f; // maximum across entire tile is V2 lane 7

        // Pass 1: Stream NUM_VECTORS with distinct addresses
        i_mode.write(RE_MODE_SOFTMAX_PASS1);
        i_start.write(true);
        for (int v = 0; v < NUM_VECTORS; v++)
        {
            i_vector_data.write(tile_in[v]);
            i_addr.write(0x1000 + v * 4);
            i_valid.write(true);
            wait();
            i_start.write(false);
        }
        i_valid.write(false);
        wait();

        bool p1_tile_ok = (re_dut->get_running_max() == expected_tile_max) && !o_valid.read();
        total_tests++;
        if (p1_tile_ok)
        {
            tests_passed++;
            std::cout << "  [PASS] Multi-Vector Pass 1 accumulated global tile max (" << expected_tile_max << ") and suppressed writes." << std::endl;
        }
        else
        {
            std::cout << "  [FAIL] Multi-Vector Pass 1 failed: max=" << re_dut->get_running_max() << " o_valid=" << o_valid.read() << std::endl;
        }

        // Pass 2: Stream NUM_VECTORS and verify outputs and address alignment
        i_mode.write(RE_MODE_SOFTMAX_PASS2);
        i_start.write(true);
        std::vector<uint32_t> captured_addrs;
        std::vector<psum_vector_t<TEST_Y_DIM, float>> captured_norm_vecs;

        for (int v = 0; v < NUM_VECTORS; v++)
        {
            i_addr.write(0x1000 + v * 4);
            i_valid.write(true);
            wait();
            i_start.write(false);
            if (o_valid.read())
            {
                captured_addrs.push_back(o_addr.read());
                captured_norm_vecs.push_back(o_vector_out.read());
            }
        }
        i_valid.write(false);
        wait();
        if (o_valid.read())
        {
            captured_addrs.push_back(o_addr.read());
            captured_norm_vecs.push_back(o_vector_out.read());
        }

        bool p2_tile_ok = (captured_norm_vecs.size() == NUM_VECTORS);
        if (p2_tile_ok)
        {
            for (int v = 0; v < NUM_VECTORS; v++)
            {
                if (captured_addrs[v] != static_cast<uint32_t>(0x1000 + v * 4))
                {
                    p2_tile_ok = false;
                    std::cout << "  [FAIL] Address misalignment on vector " << v << ": exp " 
                              << (0x1000 + v * 4) << " got " << captured_addrs[v] << std::endl;
                }
            }
        }

        total_tests++;
        if (p2_tile_ok)
        {
            tests_passed++;
            std::cout << "  [PASS] Multi-Vector Pass 2 produced " << captured_norm_vecs.size() 
                      << " normalized vectors with aligned addresses." << std::endl;
        }
        else
        {
            std::cout << "  [FAIL] Multi-Vector Pass 2 failed to produce expected normalized vectors." << std::endl;
        }

        // Verify exact numerical normalized softmax values against golden math
        bool p2_values_ok = (captured_norm_vecs.size() == NUM_VECTORS);
        if (p2_values_ok)
        {
            for (int v = 0; v < NUM_VECTORS; v++)
            {
                float v_max = -INFINITY;
                for (int l = 0; l < TEST_Y_DIM; l++)
                {
                    if (tile_in[v][l] > v_max) v_max = tile_in[v][l];
                }
                double exp_sum = 0.0;
                std::vector<double> golden_probs(TEST_Y_DIM);
                for (int l = 0; l < TEST_Y_DIM; l++)
                {
                    double diff = static_cast<double>(tile_in[v][l]) - v_max;
                    double ev = std::exp(diff);
                    golden_probs[l] = ev;
                    exp_sum += ev;
                }
                for (int l = 0; l < TEST_Y_DIM; l++)
                {
                    golden_probs[l] /= exp_sum;
                    float got = captured_norm_vecs[v][l];
                    float exp = static_cast<float>(golden_probs[l]);
                    if (std::abs(got - exp) > 1e-4f)
                    {
                        p2_values_ok = false;
                        std::cout << "  [FAIL] Multi-Vector Softmax V" << v << " lane " << l
                                  << ": expected " << exp << ", got " << got << std::endl;
                    }
                }
            }
        }
        total_tests++;
        if (p2_values_ok)
        {
            tests_passed++;
            std::cout << "  [PASS] Multi-Vector Pass 2 normalized values matched per-vector golden softmax math precisely." << std::endl;
        }
        else
        {
            std::cout << "  [FAIL] Multi-Vector Pass 2 normalized values mismatch." << std::endl;
        }

        // ----------------------------------------------------
        // Case 9B: Softmax Wide Dynamic Range (Underflow Prevention: -120 vs +100 Logits)
        // ----------------------------------------------------
        std::cout << "\n--- CASE 9B: Softmax Underflow Prevention (-120 vs +100 Logits) ---" << std::endl;
        run_reset();

        psum_vector_t<TEST_Y_DIM, float> wide_v0, wide_v1;
        // Vector 0: Positive high logits [+90..+97] (max = +97)
        for (int l = 0; l < TEST_Y_DIM; l++) wide_v0[l] = 90.0f + l;
        // Vector 1: Negative low logits [-120..-113] (max = -113)
        // Global max is 97. If subtracted from Vector 1, diff is <= -210, float32 exp underflows to 0!
        for (int l = 0; l < TEST_Y_DIM; l++) wide_v1[l] = -120.0f + l;

        // Pass 1: Stream Vector 0 then Vector 1
        i_mode.write(RE_MODE_SOFTMAX_PASS1);
        i_start.write(true);
        i_vector_data.write(wide_v0);
        i_valid.write(true);
        wait();
        i_start.write(false);

        i_vector_data.write(wide_v1);
        wait();
        i_valid.write(false);
        wait();

        bool p1_wide_ok = (re_dut->get_running_max() == 97.0f) && !o_valid.read();
        total_tests++;
        if (p1_wide_ok)
        {
            tests_passed++;
            std::cout << "  [PASS] Wide-range Pass 1 recorded global max (+97.0f) and per-vector maxes." << std::endl;
        }
        else
        {
            std::cout << "  [FAIL] Wide-range Pass 1 failed: max=" << re_dut->get_running_max() << std::endl;
        }

        // Pass 2: Stream both vectors and verify Vector 1 does NOT underflow to all zeros
        i_mode.write(RE_MODE_SOFTMAX_PASS2);
        i_start.write(true);
        std::vector<psum_vector_t<TEST_Y_DIM, float>> captured_wide_vecs;

        i_valid.write(true);
        wait();
        i_start.write(false);
        if (o_valid.read()) captured_wide_vecs.push_back(o_vector_out.read());

        wait();
        if (o_valid.read()) captured_wide_vecs.push_back(o_vector_out.read());

        i_valid.write(false);
        wait();
        if (o_valid.read()) captured_wide_vecs.push_back(o_vector_out.read());

        bool p2_wide_ok = (captured_wide_vecs.size() == 2);
        bool no_underflow_zero = true;
        float v1_prob_sum = 0.0f;
        if (p2_wide_ok)
        {
            for (int l = 0; l < TEST_Y_DIM; l++)
            {
                float v1_val = captured_wide_vecs[1][l];
                v1_prob_sum += v1_val;
                if (v1_val <= 0.0f) no_underflow_zero = false;
            }
            if (std::abs(v1_prob_sum - 1.0f) > 1e-4f) no_underflow_zero = false;
        }

        total_tests++;
        if (p2_wide_ok && no_underflow_zero)
        {
            tests_passed++;
            std::cout << "  [PASS] Softmax per-vector max prevented underflow for -120 range logits! Vector 1 sum=" 
                      << v1_prob_sum << " (expected 1.0), all lanes non-zero." << std::endl;
        }
        else
        {
            std::cout << "  [FAIL] Softmax underflow bug detected: Vector 1 outputted zeros or invalid sum ("
                      << v1_prob_sum << ")" << std::endl;
        }

        total_tests++;
        if (!re_dut->has_scratch_overflow())
        {
            tests_passed++;
            std::cout << "  [PASS] Scratch SRAM capacity monitored: has_scratch_overflow() is false." << std::endl;
        }
        else
        {
            std::cout << "  [FAIL] Unexpected scratch SRAM overflow flag." << std::endl;
        }

        // Test stale-slot prevention: stream an extra vector in Pass 2 beyond what Pass 1 stored
        psum_vector_t<TEST_Y_DIM, float> extra_vec;
        for (int l = 0; l < TEST_Y_DIM; l++) extra_vec[l] = static_cast<float>(l);
        i_vector_data.write(extra_vec);
        i_valid.write(true);
        wait();
        i_valid.write(false);
        wait();
        psum_vector_t<TEST_Y_DIM, float> got_extra = o_vector_out.read();
        bool extra_safe = true;
        float extra_sum = 0.0f;
        for (int l = 0; l < TEST_Y_DIM; l++)
        {
            if (std::isnan(got_extra[l]) || std::isinf(got_extra[l])) extra_safe = false;
            if (got_extra[l] < 0.0f || got_extra[l] > 1.0f) extra_safe = false;
            extra_sum += got_extra[l];
        }
        if (std::abs(extra_sum - 1.0f) > 1e-4f) extra_safe = false;
        total_tests++;
        if (extra_safe && re_dut->has_scratch_overflow())
        {
            tests_passed++;
            std::cout << "  [PASS] Stale-slot read guarded: extra Pass 2 vector flagged scratch overflow and fallback output is valid (sum="
                      << extra_sum << ", exp 1.0)." << std::endl;
        }
        else
        {
            std::cout << "  [FAIL] Stale-slot read produced invalid fallback output (sum="
                      << extra_sum << ") or failed to flag overflow." << std::endl;
        }

        // ----------------------------------------------------
        // Case 10: Partial i_rows_active Masking Verification
        // Inactive lanes (4..7) contain large non-zero junk values (9999.0)
        // Verifies active lanes only participate in Max, Mean, Variance, Softmax, MaxPool
        // ----------------------------------------------------
        std::cout << "\n--- CASE 10: Partial i_rows_active Masking Verification ---" << std::endl;
        run_reset();

        sramc_mask_t<TEST_Y_DIM> partial_mask;
        for (int l = 0; l < TEST_Y_DIM; l++)
        {
            partial_mask[l] = (l < 4); // Lanes 0..3 active, lanes 4..7 inactive
        }
        i_rows_active.write(partial_mask);

        // 10A: Max Tree with junk inactive lanes
        psum_vector_t<TEST_Y_DIM, float> junk_vec;
        junk_vec[0] = 10.0f;
        junk_vec[1] = 42.0f; // Active max
        junk_vec[2] = 5.0f;
        junk_vec[3] = 30.0f;
        junk_vec[4] = 9999.0f; // Inactive junk
        junk_vec[5] = 8888.0f;
        junk_vec[6] = 7777.0f;
        junk_vec[7] = 6666.0f;

        i_vector_data.write(junk_vec);
        i_mode.write(RE_MODE_SOFTMAX_PASS1);
        i_start.write(true);
        i_valid.write(true);
        wait();
        i_start.write(false);
        i_valid.write(false);
        wait();

        bool mask_max_ok = (re_dut->get_running_max() == 42.0f);
        total_tests++;
        if (mask_max_ok)
        {
            tests_passed++;
            std::cout << "  [PASS] Active-masked Max Tree correctly ignored inactive 9999.0f (got "
                      << re_dut->get_running_max() << ", exp 42.0f)." << std::endl;
        }
        else
        {
            std::cout << "  [FAIL] Active-masked Max Tree failed: got " << re_dut->get_running_max() << " exp 42.0f" << std::endl;
        }

        // 10B: Softmax Pass 2 with inactive lanes producing 0
        i_mode.write(RE_MODE_SOFTMAX_PASS2);
        i_start.write(true);
        i_valid.write(true);
        wait();
        i_start.write(false);
        i_valid.write(false);
        wait();

        psum_vector_t<TEST_Y_DIM, float> got_masked_sm = o_vector_out.read();
        bool mask_sm_ok = true;
        double sm_active_sum = 0.0;
        for (int l = 0; l < 4; l++) sm_active_sum += got_masked_sm[l];
        if (std::abs(sm_active_sum - 1.0) > 1e-4) mask_sm_ok = false;
        for (int l = 4; l < 8; l++)
        {
            if (got_masked_sm[l] != 0.0f) mask_sm_ok = false;
        }
        total_tests++;
        if (mask_sm_ok)
        {
            tests_passed++;
            std::cout << "  [PASS] Masked Softmax Pass 2: active sum=1.0 and inactive lanes clamped to 0." << std::endl;
        }
        else
        {
            std::cout << "  [FAIL] Masked Softmax Pass 2 failed. Active sum=" << sm_active_sum << std::endl;
        }

        // 10C: LayerNorm with junk inactive lanes
        run_reset();
        i_rows_active.write(partial_mask);
        psum_vector_t<TEST_Y_DIM, float> ln_junk_vec;
        ln_junk_vec[0] = 10.0f;
        ln_junk_vec[1] = 20.0f;
        ln_junk_vec[2] = 30.0f;
        ln_junk_vec[3] = 40.0f; // Active mean = 25.0
        ln_junk_vec[4] = 9999.0f;
        ln_junk_vec[5] = 9999.0f;
        ln_junk_vec[6] = 9999.0f;
        ln_junk_vec[7] = 9999.0f;

        i_vector_data.write(ln_junk_vec);
        i_mode.write(RE_MODE_LAYERNORM_PASS1);
        i_start.write(true);
        i_valid.write(true);
        wait();
        i_start.write(false);
        i_valid.write(false);
        wait();

        bool mask_ln_mean_ok = (re_dut->get_running_mean() == 25.0f);
        total_tests++;
        if (mask_ln_mean_ok)
        {
            tests_passed++;
            std::cout << "  [PASS] Masked LayerNorm Pass 1 Mean ignored junk (got "
                      << re_dut->get_running_mean() << ", exp 25.0f)." << std::endl;
        }
        else
        {
            std::cout << "  [FAIL] Masked LayerNorm Pass 1 failed: got mean=" << re_dut->get_running_mean() << std::endl;
        }

        i_mode.write(RE_MODE_LAYERNORM_PASS2);
        i_start.write(true);
        i_valid.write(true);
        wait();
        i_start.write(false);
        i_valid.write(false);
        wait();

        psum_vector_t<TEST_Y_DIM, float> got_masked_ln = o_vector_out.read();
        bool mask_ln_ok = true;
        for (int l = 4; l < 8; l++)
        {
            if (got_masked_ln[l] != 0.0f) mask_ln_ok = false;
        }
        total_tests++;
        if (mask_ln_ok)
        {
            tests_passed++;
            std::cout << "  [PASS] Masked LayerNorm Pass 2: inactive lanes suppressed to 0." << std::endl;
        }
        else
        {
            std::cout << "  [FAIL] Masked LayerNorm Pass 2 inactive lanes non-zero." << std::endl;
        }

        // 10D: MaxPool with junk inactive lanes
        i_mode.write(RE_MODE_MAXPOOL);
        i_start.write(true);
        i_valid.write(true);
        wait();
        i_start.write(false);
        i_valid.write(false);
        wait();

        psum_vector_t<TEST_Y_DIM, float> got_masked_mp = o_vector_out.read();
        bool mask_mp_ok = true;
        for (int l = 0; l < 4; l++)
        {
            if (got_masked_mp[l] != 40.0f) mask_mp_ok = false; // Max of active lanes (10, 20, 30, 40)
        }
        for (int l = 4; l < 8; l++)
        {
            if (got_masked_mp[l] != 0.0f) mask_mp_ok = false;
        }
        total_tests++;
        if (mask_mp_ok)
        {
            tests_passed++;
            std::cout << "  [PASS] Masked MaxPool correctly broadcast active max (40.0f) to active lanes only." << std::endl;
        }
        else
        {
            std::cout << "  [FAIL] Masked MaxPool failed." << std::endl;
        }

        // ----------------------------------------------------
        // Case 11: Real Hardware Datapath (ReductionEngine<8, int32_t, int8_t>)
        // Verifies integer requantization, rounding, and saturation clamping
        // ----------------------------------------------------
        std::cout << "\n--- CASE 11: Integer Datapath, Requantization & Saturation Clamping ---" << std::endl;
        run_reset();

        // 11A: Residual-Add with Integer Saturation Clamping to [-128, 127]
        std::cout << "  --- CASE 11A: Integer Residual-Add Saturation Clamping [-128, 127] ---" << std::endl;
        psum_vector_t<TEST_Y_DIM, int32_t> int_res_in;
        act_vector_t<TEST_Y_DIM, int8_t> int_res_skip;
        int_res_in[0] = 100;    int_res_skip[0] = 50;   // 150 -> clamp 127
        int_res_in[1] = -100;   int_res_skip[1] = -50;  // -150 -> clamp -128
        int_res_in[2] = 20;     int_res_skip[2] = 15;   // 35 -> 35
        int_res_in[3] = -20;    int_res_skip[3] = -15;  // -35 -> -35
        int_res_in[4] = 127;    int_res_skip[4] = 0;    // 127 -> 127
        int_res_in[5] = -128;   int_res_skip[5] = 0;    // -128 -> -128
        int_res_in[6] = 60;     int_res_skip[6] = 67;   // 127 -> 127
        int_res_in[7] = 60;     int_res_skip[7] = 68;   // 128 -> clamp 127

        int_vector_data.write(int_res_in);
        int_skip_data.write(int_res_skip);
        int_mode.write(RE_MODE_RESIDUAL_ADD);
        int_requant_scale.write(1);
        int_requant_shift.write(0);
        int_start.write(true);
        int_valid.write(true);
        wait();
        int_start.write(false);
        int_valid.write(false);
        wait();

        psum_vector_t<TEST_Y_DIM, int32_t> got_int_res = int_vector_out.read();
        bool int_res_ok = (got_int_res[0] == 127 && got_int_res[1] == -128 &&
                           got_int_res[2] == 35 && got_int_res[3] == -35 &&
                           got_int_res[4] == 127 && got_int_res[5] == -128 &&
                           got_int_res[6] == 127 && got_int_res[7] == 127);
        total_tests++;
        if (int_res_ok)
        {
            tests_passed++;
            std::cout << "  [PASS] Integer Residual-Add saturation clamping [-128, 127] verified." << std::endl;
        }
        else
        {
            std::cout << "  [FAIL] Integer Residual-Add saturation mismatch: " << got_int_res << std::endl;
        }

        // 11B: Integer Residual-Add Symmetric Rounding (Negative Tie: in=-5, skip=0, scale=1, shift=1 -> -3)
        std::cout << "  --- CASE 11B: Integer Residual-Add Negative Tie Rounding (-5 >> 1 -> -3) ---" << std::endl;
        psum_vector_t<TEST_Y_DIM, int32_t> neg_tie_in;
        act_vector_t<TEST_Y_DIM, int8_t> neg_tie_skip;
        for (int l = 0; l < TEST_Y_DIM; l++)
        {
            neg_tie_in[l] = -5;
            neg_tie_skip[l] = 0;
        }
        int_vector_data.write(neg_tie_in);
        int_skip_data.write(neg_tie_skip);
        int_mode.write(RE_MODE_RESIDUAL_ADD);
        int_requant_scale.write(1);
        int_requant_shift.write(1);
        int_start.write(true);
        int_valid.write(true);
        wait();
        int_start.write(false);
        int_valid.write(false);
        wait();

        psum_vector_t<TEST_Y_DIM, int32_t> got_neg_tie = int_vector_out.read();
        bool neg_tie_ok = true;
        for (int l = 0; l < TEST_Y_DIM; l++)
        {
            if (got_neg_tie[l] != -3) neg_tie_ok = false;
        }
        total_tests++;
        if (neg_tie_ok)
        {
            tests_passed++;
            std::cout << "  [PASS] Integer Residual-Add negative tie rounding (-5 >> 1 -> -3) verified: "
                      << got_neg_tie[0] << std::endl;
        }
        else
        {
            std::cout << "  [FAIL] Integer Residual-Add negative tie mismatch: expected -3, got "
                      << got_neg_tie[0] << std::endl;
        }

        // 11C: Softmax with Integer Requantization Scaling & Exact Golden Vector Matching
        std::cout << "  --- CASE 11C: Integer Softmax Requantization & Exact Golden Matching ---" << std::endl;
        psum_vector_t<TEST_Y_DIM, int32_t> int_sm_in;
        for (int l = 0; l < TEST_Y_DIM; l++) int_sm_in[l] = (l + 1) * 2; // [2, 4, ..., 16]
        int_vector_data.write(int_sm_in);
        int_mode.write(RE_MODE_SOFTMAX_PASS1);
        int_start.write(true);
        int_valid.write(true);
        wait();
        int_start.write(false);
        int_valid.write(false);
        wait();

        // Pass 2 with scale=127, shift=0
        int_mode.write(RE_MODE_SOFTMAX_PASS2);
        int_requant_scale.write(127);
        int_requant_shift.write(0);
        int_start.write(true);
        int_valid.write(true);
        wait();
        int_start.write(false);
        int_valid.write(false);
        wait();

        psum_vector_t<TEST_Y_DIM, int32_t> got_int_sm = int_vector_out.read();
        int32_t expected_sm[TEST_Y_DIM] = {0, 0, 0, 0, 0, 2, 15, 110};
        bool int_sm_ok = true;
        for (int l = 0; l < TEST_Y_DIM; l++)
        {
            if (got_int_sm[l] != expected_sm[l]) int_sm_ok = false;
        }
        total_tests++;
        if (int_sm_ok)
        {
            tests_passed++;
            std::cout << "  [PASS] Integer Softmax exact golden matching {0, 0, 0, 0, 0, 2, 15, 110} verified: "
                      << got_int_sm << std::endl;
        }
        else
        {
            std::cout << "  [FAIL] Integer Softmax golden mismatch: got " << got_int_sm << std::endl;
        }

        // ----------------------------------------------------
        // Case 12: Multi-Cycle Level i_start Immunity
        // Verifies holding i_start HIGH does not continually wipe accumulators
        // ----------------------------------------------------
        std::cout << "\n--- CASE 12: Multi-Cycle Level i_start Immunity ---" << std::endl;
        run_reset();

        i_mode.write(RE_MODE_SOFTMAX_PASS1);
        // Hold i_start HIGH for 3 consecutive cycles while streaming 3 different vectors
        psum_vector_t<TEST_Y_DIM, float> v1, v2, v3;
        v1[0] = 10.0f; v2[0] = 20.0f; v3[0] = 30.0f;

        i_start.write(true); // Assert start
        i_valid.write(true);
        i_vector_data.write(v1);
        wait(); // Cycle 1: start_edge fires, sets running_max = 10

        i_start.write(true); // Keep start HIGH (level signal)
        i_vector_data.write(v2);
        wait(); // Cycle 2: start_edge is false, accumulates running_max = 20

        i_start.write(true); // Still HIGH
        i_vector_data.write(v3);
        wait(); // Cycle 3: accumulates running_max = 30

        i_start.write(false);
        i_valid.write(false);
        wait();

        bool start_immunity_ok = (re_dut->get_running_max() == 30.0f);
        total_tests++;
        if (start_immunity_ok)
        {
            tests_passed++;
            std::cout << "  [PASS] Rising-edge detector preserved accumulators across held-high i_start level (max="
                      << re_dut->get_running_max() << " exp 30.0f)." << std::endl;
        }
        else
        {
            std::cout << "  [FAIL] Level i_start wiped accumulators: got max=" << re_dut->get_running_max() << std::endl;
        }

        // ----------------------------------------------------
        // ----------------------------------------------------
        // Case 13: Multi-Vector Tile-Wide Softmax (Modes 7 & 8)
        // ----------------------------------------------------
        // Case 13A: Ascending order inputs (0..37)
        std::cout << "\n--- CASE 13A: Multi-Vector Tile-Wide Softmax (Ascending Inputs: 0..37) ---" << std::endl;
        run_reset();

        const int TILE_VECS = 4;
        psum_vector_t<TEST_Y_DIM, float> tile_inputs[TILE_VECS];
        double golden_sum_a = 0.0;
        double golden_probs_a[TILE_VECS][TEST_Y_DIM];

        for (int v = 0; v < TILE_VECS; v++)
        {
            for (int l = 0; l < TEST_Y_DIM; l++)
            {
                tile_inputs[v][l] = static_cast<float>(v * 10 + l); // 0..37 (max is 37)
                double ev = std::exp(static_cast<double>(v * 10 + l) - 37.0);
                golden_probs_a[v][l] = ev;
                golden_sum_a += ev;
            }
        }
        for (int v = 0; v < TILE_VECS; v++)
        {
            for (int l = 0; l < TEST_Y_DIM; l++)
            {
                golden_probs_a[v][l] /= golden_sum_a;
            }
        }

        // Pass 1: Stream 4 vectors with RE_MODE_SOFTMAX_TILE_PASS1
        i_mode.write(RE_MODE_SOFTMAX_TILE_PASS1);
        i_start.write(true);
        for (int v = 0; v < TILE_VECS; v++)
        {
            i_vector_data.write(tile_inputs[v]);
            i_valid.write(true);
            wait();
            i_start.write(false);
        }
        i_valid.write(false);
        wait();

        bool c13_p1_ok = (re_dut->get_running_max() == 37.0f) && !o_valid.read();
        total_tests++;
        if (c13_p1_ok)
        {
            tests_passed++;
            std::cout << "  [PASS] Tile-Wide Softmax Pass 1 accumulated global tile max (37.0f) and online sum." << std::endl;
        }
        else
        {
            std::cout << "  [FAIL] Tile-Wide Softmax Pass 1 failed: max=" << re_dut->get_running_max() << std::endl;
        }

        // Pass 2: Stream 4 vectors with RE_MODE_SOFTMAX_TILE_PASS2
        i_mode.write(RE_MODE_SOFTMAX_TILE_PASS2);
        i_start.write(true);
        std::vector<psum_vector_t<TEST_Y_DIM, float>> captured_tile_vecs;

        for (int v = 0; v < TILE_VECS; v++)
        {
            i_valid.write(true);
            wait();
            i_start.write(false);
            if (o_valid.read()) captured_tile_vecs.push_back(o_vector_out.read());
        }
        i_valid.write(false);
        wait();
        if (o_valid.read()) captured_tile_vecs.push_back(o_vector_out.read());

        bool c13a_elements_ok = (captured_tile_vecs.size() == TILE_VECS);
        double total_tile_sum_a = 0.0;
        if (c13a_elements_ok)
        {
            for (int v = 0; v < TILE_VECS; v++)
            {
                for (int l = 0; l < TEST_Y_DIM; l++)
                {
                    float got = captured_tile_vecs[v][l];
                    float exp_val = static_cast<float>(golden_probs_a[v][l]);
                    if (std::abs(got - exp_val) > 1e-4f)
                    {
                        c13a_elements_ok = false;
                        std::cout << "  [FAIL] Tile-Wide Softmax 13A V" << v << " L" << l
                                  << " got " << got << " exp " << exp_val << std::endl;
                    }
                    total_tile_sum_a += static_cast<double>(got);
                }
            }
            if (std::abs(total_tile_sum_a - 1.0) > 1e-4) c13a_elements_ok = false;
        }
        total_tests++;
        if (c13a_elements_ok)
        {
            tests_passed++;
            std::cout << "  [PASS] Tile-Wide Softmax 13A: per-element golden match & total sum = "
                      << total_tile_sum_a << " (expected 1.0)." << std::endl;
        }
        else
        {
            std::cout << "  [FAIL] Tile-Wide Softmax 13A per-element match failed: total sum = " << total_tile_sum_a << std::endl;
        }

        // Case 13B: Descending order inputs (37 down to 0)
        // Tests online rescale when initial vector contains the global maximum (37) and subsequent vectors are smaller
        std::cout << "\n--- CASE 13B: Multi-Vector Tile-Wide Softmax (Descending Inputs: 37..0) ---" << std::endl;
        run_reset();

        psum_vector_t<TEST_Y_DIM, float> desc_inputs[TILE_VECS];
        double golden_sum_b = 0.0;
        double golden_probs_b[TILE_VECS][TEST_Y_DIM];

        for (int v = 0; v < TILE_VECS; v++)
        {
            for (int l = 0; l < TEST_Y_DIM; l++)
            {
                float val = static_cast<float>((TILE_VECS - 1 - v) * 10 + (TEST_Y_DIM - 1 - l));
                desc_inputs[v][l] = val;
                double ev = std::exp(static_cast<double>(val) - 37.0);
                golden_probs_b[v][l] = ev;
                golden_sum_b += ev;
            }
        }
        for (int v = 0; v < TILE_VECS; v++)
        {
            for (int l = 0; l < TEST_Y_DIM; l++)
            {
                golden_probs_b[v][l] /= golden_sum_b;
            }
        }

        // Pass 1: Stream 4 vectors with RE_MODE_SOFTMAX_TILE_PASS1
        i_mode.write(RE_MODE_SOFTMAX_TILE_PASS1);
        i_start.write(true);
        for (int v = 0; v < TILE_VECS; v++)
        {
            i_vector_data.write(desc_inputs[v]);
            i_valid.write(true);
            wait();
            i_start.write(false);
        }
        i_valid.write(false);
        wait();

        bool c13b_p1_ok = (re_dut->get_running_max() == 37.0f) && !o_valid.read();
        total_tests++;
        if (c13b_p1_ok)
        {
            tests_passed++;
            std::cout << "  [PASS] Tile-Wide Softmax 13B Pass 1 descending global max (37.0f) verified." << std::endl;
        }
        else
        {
            std::cout << "  [FAIL] Tile-Wide Softmax 13B Pass 1 failed: max=" << re_dut->get_running_max() << std::endl;
        }

        // Pass 2: Stream 4 vectors with RE_MODE_SOFTMAX_TILE_PASS2
        i_mode.write(RE_MODE_SOFTMAX_TILE_PASS2);
        i_start.write(true);
        std::vector<psum_vector_t<TEST_Y_DIM, float>> captured_desc_vecs;

        for (int v = 0; v < TILE_VECS; v++)
        {
            i_valid.write(true);
            wait();
            i_start.write(false);
            if (o_valid.read()) captured_desc_vecs.push_back(o_vector_out.read());
        }
        i_valid.write(false);
        wait();
        if (o_valid.read()) captured_desc_vecs.push_back(o_vector_out.read());

        bool c13b_elements_ok = (captured_desc_vecs.size() == TILE_VECS);
        double total_tile_sum_b = 0.0;
        if (c13b_elements_ok)
        {
            for (int v = 0; v < TILE_VECS; v++)
            {
                for (int l = 0; l < TEST_Y_DIM; l++)
                {
                    float got = captured_desc_vecs[v][l];
                    float exp_val = static_cast<float>(golden_probs_b[v][l]);
                    if (std::abs(got - exp_val) > 1e-4f)
                    {
                        c13b_elements_ok = false;
                        std::cout << "  [FAIL] Tile-Wide Softmax 13B V" << v << " L" << l
                                  << " got " << got << " exp " << exp_val << std::endl;
                    }
                    total_tile_sum_b += static_cast<double>(got);
                }
            }
            if (std::abs(total_tile_sum_b - 1.0) > 1e-4) c13b_elements_ok = false;
        }
        total_tests++;
        if (c13b_elements_ok)
        {
            tests_passed++;
            std::cout << "  [PASS] Tile-Wide Softmax 13B: descending per-element golden match & total sum = "
                      << total_tile_sum_b << " (expected 1.0)." << std::endl;
        }
        else
        {
            std::cout << "  [FAIL] Tile-Wide Softmax 13B descending match failed: total sum = " << total_tile_sum_b << std::endl;
        }

        // ----------------------------------------------------
        // Case 14: RE RCE LUT Interface & Output Driving (o_lut_*)
        // Verifies ReductionEngine actively drives o_lut_* and evaluates through companion RCE
        // ----------------------------------------------------
        std::cout << "\n--- CASE 14: RE RCE LUT Interface & Output Driving (o_lut_*) ---" << std::endl;
        run_reset();

        // LayerNorm Pass 2 driving LUT_OP_RSQRT
        psum_vector_t<TEST_Y_DIM, float> ln_vec;
        for (int l = 0; l < TEST_Y_DIM; l++) ln_vec[l] = static_cast<float>(l * 10);

        i_mode.write(RE_MODE_LAYERNORM_PASS1);
        i_start.write(true);
        i_vector_data.write(ln_vec);
        i_valid.write(true);
        wait();
        i_start.write(false);
        i_valid.write(false);
        wait();

        i_mode.write(RE_MODE_LAYERNORM_PASS2);
        i_start.write(true);
        i_valid.write(true);
        wait();
        i_start.write(false);
        i_valid.write(false);
        wait();
        bool lut_driven = dummy_lut_in_valid.read() && (dummy_lut_op.read() == LUT_OP_RSQRT);

        total_tests++;
        if (lut_driven)
        {
            tests_passed++;
            std::cout << "  [PASS] ReductionEngine actively drove o_lut_valid=1 and o_lut_op=LUT_OP_RSQRT." << std::endl;
        }
        else
        {
            std::cout << "  [FAIL] ReductionEngine did not drive o_lut_* ports: valid=" 
                      << dummy_lut_in_valid.read() << " op=" << dummy_lut_op.read() << std::endl;
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
