// SystemC Model for SAURIA NPU Core
// Reduction Engine (RE) & Reconfigurable Compute Engine (RCE)
// Parameterized Dimensions and Datatypes
// Supports Softmax, LayerNorm, MaxPool, and Residual Addition

#ifndef SAURIA_RE_RCE_H
#define SAURIA_RE_RCE_H

#include <systemc.h>
#include <type_traits>
#include <cmath>
#include <algorithm>
#include "sauria_types.h"
#include "debug.h"

#ifndef FX1_NO_PERF
#include "instrumentation/perf_counters.h"
#endif

namespace sauria
{
    // RCE LUT Operation Codes
    enum rce_lut_op_t
    {
        LUT_OP_EXP = 0,
        LUT_OP_RECIP = 1,
        LUT_OP_RSQRT = 2
    };

    // RE Operation Modes
    enum re_mode_t
    {
        RE_MODE_IDLE = 0,
        RE_MODE_SOFTMAX_PASS1 = 1, // Find max(x)
        RE_MODE_SOFTMAX_PASS2 = 2, // Compute exp(x - max), sum, and multiply by recip(sum)
        RE_MODE_LAYERNORM_PASS1 = 3, // Compute mean
        RE_MODE_LAYERNORM_PASS2 = 4, // Compute variance and scale with rsqrt
        RE_MODE_MAXPOOL = 5,         // MaxPool using shared max comparator tree
        RE_MODE_RESIDUAL_ADD = 6,    // Elemwise Add (dequant, skip add, requant)
        RE_MODE_SOFTMAX_TILE_PASS1 = 7, // Multi-Vector / Tile Softmax Pass 1 (Online Softmax)
        RE_MODE_SOFTMAX_TILE_PASS2 = 8  // Multi-Vector / Tile Softmax Pass 2 (Global Tile Normalization)
    };

    // =========================================================================
    // Reconfigurable Compute Engine (RCE)
    // Manages non-linear LUTs (exp, recip, rsqrt) and coordinates control flow
    // =========================================================================
    template <
        uint32_t LUT_EXP_OFFSET = 0x00200000,
        uint32_t LUT_RECIP_OFFSET = 0x00210000,
        uint32_t LUT_RSQRT_OFFSET = 0x00220000>
    class ReconfigurableEngine : public sc_module
    {
    public:
        // Clock & Reset
        sc_in<bool> i_clk{"i_clk"};
        sc_in<bool> i_rstn{"i_rstn"};

#ifndef FX1_NO_PERF
        fx1::PerfCounters *perf{nullptr};
#endif

        // Host Programming Interface
        sc_in<uint32_t> i_host_addr{"i_host_addr"};
        sc_in<bool> i_host_wren{"i_host_wren"};
        sc_in<bool> i_host_rden{"i_host_rden"};
        sc_in<host_data_t> i_host_wdata{"i_host_wdata"};
        sc_in<host_mask_t> i_host_wmask{"i_host_wmask"};
        sc_out<host_data_t> o_host_rdata{"o_host_rdata"};

        // Functional LUT Interface
        sc_in<uint32_t> i_lut_op{"i_lut_op"};
        sc_in<float> i_lut_in{"i_lut_in"};
        sc_in<bool> i_lut_valid{"i_lut_valid"};
        sc_out<float> o_lut_out{"o_lut_out"};
        sc_out<bool> o_lut_valid{"o_lut_valid"};

        SC_CTOR(ReconfigurableEngine)
        {
            SC_METHOD(rce_process);
            sensitive << i_clk.pos();
        }

        // Functional lookups for direct method calls from RE or external models
        float lookup_exp(float x) const
        {
            bool is_empty = true;
            for (int i = 0; i < 256; i++) {
                if (lut_exp[i] != 0) { is_empty = false; break; }
            }
            if (is_empty) {
                return std::exp(x);
            }
            float pos = (x + 8.0f) * 31.875f;
            if (pos <= 0.0f) return static_cast<float>(lut_exp[0]) / 255.0f;
            if (pos >= 255.0f) return static_cast<float>(lut_exp[255]) / 255.0f;
            int32_t idx = static_cast<int32_t>(std::floor(pos));
            float frac = pos - static_cast<float>(idx);
            float v0 = static_cast<float>(lut_exp[idx]) / 255.0f;
            float v1 = static_cast<float>(lut_exp[idx + 1]) / 255.0f;
            return v0 + frac * (v1 - v0);
        }

        float lookup_recip(float x) const
        {
            bool is_empty = true;
            for (int i = 0; i < 512; i++) {
                if (lut_recip[i] != 0) { is_empty = false; break; }
            }
            if (is_empty) {
                return (x != 0.0f) ? (1.0f / x) : 0.0f;
            }
            float pos = x * 511.0f;
            if (pos <= 0.0f) return static_cast<float>(lut_recip[0]) / 511.0f;
            if (pos >= 511.0f) return static_cast<float>(lut_recip[511]) / 511.0f;
            int32_t idx = static_cast<int32_t>(std::floor(pos));
            float frac = pos - static_cast<float>(idx);
            float v0 = static_cast<float>(lut_recip[idx]) / 511.0f;
            float v1 = static_cast<float>(lut_recip[idx + 1]) / 511.0f;
            return v0 + frac * (v1 - v0);
        }

        float lookup_rsqrt(float x) const
        {
            bool is_empty = true;
            for (int i = 0; i < 1024; i++) {
                if (lut_rsqrt[i] != 0) { is_empty = false; break; }
            }
            if (is_empty) {
                return (x > 0.0f) ? (1.0f / std::sqrt(x)) : 0.0f;
            }
            float pos = x * 1023.0f;
            if (pos <= 0.0f) return static_cast<float>(lut_rsqrt[0]) / 1023.0f;
            if (pos >= 1023.0f) return static_cast<float>(lut_rsqrt[1023]) / 1023.0f;
            int32_t idx = static_cast<int32_t>(std::floor(pos));
            float frac = pos - static_cast<float>(idx);
            float v0 = static_cast<float>(lut_rsqrt[idx]) / 1023.0f;
            float v1 = static_cast<float>(lut_rsqrt[idx + 1]) / 1023.0f;
            return v0 + frac * (v1 - v0);
        }

    private:
        // Internal LUT RAMs
        uint8_t lut_exp[256];     // 256 bytes
        uint8_t lut_recip[512];   // 512 bytes
        uint16_t lut_rsqrt[1024]; // 1024 entries (2048 bytes total)

        void rce_process()
        {
            if (!i_rstn.read())
            {
                std::memset(lut_exp, 0, sizeof(lut_exp));
                std::memset(lut_recip, 0, sizeof(lut_recip));
                std::memset(lut_rsqrt, 0, sizeof(lut_rsqrt));

                o_host_rdata.write(host_data_t());
                o_lut_out.write(0.0f);
                o_lut_valid.write(false);
                return;
            }

            // 1. Host Interface Programming
            uint32_t addr = i_host_addr.read();
            uint32_t region = addr & 0x00FF0000;
            uint32_t offset = addr & 0x0000FFFF;

            if (i_host_wren.read())
            {
                host_data_t wdata = i_host_wdata.read();
                host_mask_t wmask = i_host_wmask.read();

                if (region == LUT_EXP_OFFSET)
                {
                    uint32_t base_entry = offset & ~3;
                    for (int i = 0; i < 4; i++)
                    {
                        if (wmask[i] && (base_entry + i < 256))
                        {
                            lut_exp[base_entry + i] = static_cast<uint8_t>(wdata[i]);
                        }
                    }
                }
                else if (region == LUT_RECIP_OFFSET)
                {
                    uint32_t base_entry = offset & ~3;
                    for (int i = 0; i < 4; i++)
                    {
                        if (wmask[i] && (base_entry + i < 512))
                        {
                            lut_recip[base_entry + i] = static_cast<uint8_t>(wdata[i]);
                        }
                    }
                }
                else if (region == LUT_RSQRT_OFFSET)
                {
                    // 16-bit entries grouped in two 32-bit transfers
                    uint32_t base_entry = (offset >> 1) & ~3; // Align to 16-bit boundaries
                    for (int i = 0; i < 4; i++)
                    {
                        if (wmask[i] && (base_entry + i < 1024))
                        {
                            lut_rsqrt[base_entry + i] = static_cast<uint16_t>(wdata[i]);
                        }
                    }
                }
            }

            if (i_host_rden.read())
            {
                host_data_t rdata;
                if (region == LUT_EXP_OFFSET)
                {
                    uint32_t base_entry = offset & ~3;
                    for (int i = 0; i < 4; i++)
                    {
                        if (base_entry + i < 256) rdata[i] = lut_exp[base_entry + i];
                    }
                }
                else if (region == LUT_RECIP_OFFSET)
                {
                    uint32_t base_entry = offset & ~3;
                    for (int i = 0; i < 4; i++)
                    {
                        if (base_entry + i < 512) rdata[i] = lut_recip[base_entry + i];
                    }
                }
                else if (region == LUT_RSQRT_OFFSET)
                {
                    uint32_t base_entry = (offset >> 1) & ~3;
                    for (int i = 0; i < 4; i++)
                    {
                        if (base_entry + i < 1024) rdata[i] = lut_rsqrt[base_entry + i];
                    }
                }
                o_host_rdata.write(rdata);
            }

            // 2. Functional Pipelined Lookup
            if (i_lut_valid.read())
            {
                uint32_t op = i_lut_op.read();
                float val = i_lut_in.read();
                float out_val = 0.0f;

                if (op == LUT_OP_EXP)
                {
                    out_val = lookup_exp(val);
                }
                else if (op == LUT_OP_RECIP)
                {
                    out_val = lookup_recip(val);
                }
                else if (op == LUT_OP_RSQRT)
                {
                    out_val = lookup_rsqrt(val);
                }

                o_lut_out.write(out_val);
                o_lut_valid.write(true);
            }
            else
            {
                o_lut_out.write(0.0f);
                o_lut_valid.write(false);
            }
        }
    };

    // =========================================================================
    // Reduction Engine (RE)
    // Performs mathematical reductions (max, sum, mean, variance) & skip adds
    // Includes 24 KB Scratch SRAM
    // =========================================================================
    template <
        int Y_DIM = 32,
        typename T_PSUM = int32_t,
        typename T_ACT = int8_t>
    class ReductionEngine : public sc_module
    {
    public:
        // Clock & Reset
        sc_in<bool> i_clk{"i_clk"};
        sc_in<bool> i_rstn{"i_rstn"};

#ifndef FX1_NO_PERF
        fx1::PerfCounters *perf{nullptr};
#endif

        // Data & Control Inputs
        sc_in<uint32_t> i_mode{"i_mode"};
        sc_in<bool> i_start{"i_start"};
        sc_in<bool> i_valid{"i_valid"};
        sc_in<psum_vector_t<Y_DIM, T_PSUM>> i_vector_data{"i_vector_data"};
        sc_in<act_vector_t<Y_DIM, T_ACT>> i_skip_data{"i_skip_data"}; // Residual skip input
        sc_in<uint32_t> i_addr{"i_addr"};                              // Address passthrough
        sc_in<sramc_mask_t<Y_DIM>> i_wmask{"i_wmask"};                // Write mask passthrough
        sc_in<sramc_mask_t<Y_DIM>> i_rows_active{"i_rows_active"};    // Active rows mask

        // Scale & Requant params for Residual Skip & RE Output
        sc_in<uint32_t> i_requant_scale{"i_requant_scale"};
        sc_in<uint32_t> i_requant_shift{"i_requant_shift"};

        // Interface to companion Reconfigurable Compute Engine (RCE)
        sc_out<uint32_t> o_lut_op{"o_lut_op"};
        sc_out<float> o_lut_in{"o_lut_in"};
        sc_out<bool> o_lut_valid{"o_lut_valid"};
        sc_in<float> i_lut_out{"i_lut_out"};
        sc_in<bool> i_lut_valid{"i_lut_valid"};

        // Output Interface
        sc_out<psum_vector_t<Y_DIM, T_PSUM>> o_vector_out{"o_vector_out"};
        sc_out<uint32_t> o_addr{"o_addr"};
        sc_out<sramc_mask_t<Y_DIM>> o_wmask{"o_wmask"};
        sc_out<bool> o_valid{"o_valid"};
        sc_out<bool> o_done{"o_done"};

        float get_running_max() const { return running_max; }
        float get_running_mean() const { return static_cast<float>(running_mean); }
        float get_running_var() const { return static_cast<float>(running_var); }
        float get_running_sum() const { return static_cast<float>(running_sum); }
        double get_running_tile_sum() const { return running_tile_sum; }
        bool has_scratch_overflow() const { return scratch_overflow; }
        float get_vector_max(uint32_t idx) const { return (idx < SCRATCH_VECTORS) ? vector_max[idx] : -INFINITY; }

        // Attach companion Reconfigurable Compute Engine (RCE)
        template <typename TRce>
        void set_rce(TRce *rce_ptr)
        {
            m_rce_exp = [rce_ptr](float x) { return rce_ptr ? rce_ptr->lookup_exp(x) : std::exp(x); };
            m_rce_recip = [rce_ptr](float x) { return rce_ptr ? rce_ptr->lookup_recip(x) : ((x != 0.0f) ? (1.0f / x) : 0.0f); };
            m_rce_rsqrt = [rce_ptr](float x) { return rce_ptr ? rce_ptr->lookup_rsqrt(x) : ((x > 0.0f) ? (1.0f / std::sqrt(x)) : 0.0f); };
        }

        SC_CTOR(ReductionEngine)
        {
            SC_METHOD(re_process);
            sensitive << i_clk.pos();
        }

    private:
        // Companion RCE non-linear lookup delegates
        std::function<float(float)> m_rce_exp{[](float x) { return std::exp(x); }};
        std::function<float(float)> m_rce_recip{[](float x) { return (x != 0.0f) ? (1.0f / x) : 0.0f; }};
        std::function<float(float)> m_rce_rsqrt{[](float x) { return (x > 0.0f) ? (1.0f / std::sqrt(x)) : 0.0f; }};

        // 24 KB Scratch SRAM (stores intermediate vectors during multi-pass calculations)
        // 24 KB / (Y_DIM * sizeof(float)) vectors. For Y_DIM=32, 24576 / 128 = 192 vectors.
        static constexpr int SCRATCH_VECTORS = (24 * 1024) / (Y_DIM * sizeof(T_PSUM) > 0 ? Y_DIM * sizeof(T_PSUM) : 4);
        psum_vector_t<Y_DIM, T_PSUM> scratch_mem[SCRATCH_VECTORS];
        float vector_max[SCRATCH_VECTORS];
        uint32_t scratch_wr_idx{0};
        uint32_t scratch_rd_idx{0};
        bool scratch_overflow{false};

        // Running accumulation registers
        float running_max;
        double running_sum{0.0};
        double running_tile_sum{0.0};
        double running_sq_sum{0.0};
        double running_mean{0.0};
        double running_var{0.0};
        uint32_t total_active_elements{0};
        bool prev_start{false};

        // Saturate / Clamp helpers
        template <typename T>
        static T clamp_val(double val)
        {
            if (std::is_integral<T>::value)
            {
                if (std::is_signed<T>::value)
                {
                    if (val > 127.0) return 127;
                    if (val < -128.0) return -128;
                }
                else
                {
                    if (val > 255.0) return 255;
                    if (val < 0.0) return 0;
                }
                return static_cast<T>(val);
            }
            else
            {
                return static_cast<T>(val);
            }
        }

        void re_process()
        {
            if (!i_rstn.read())
            {
                for (int i = 0; i < SCRATCH_VECTORS; i++)
                {
                    scratch_mem[i] = psum_vector_t<Y_DIM, T_PSUM>();
                    vector_max[i] = -INFINITY;
                }
                running_max = -INFINITY;
                running_sum = 0.0;
                running_tile_sum = 0.0;
                running_sq_sum = 0.0;
                running_mean = 0.0;
                running_var = 0.0;
                total_active_elements = 0;
                scratch_wr_idx = 0;
                scratch_rd_idx = 0;
                scratch_overflow = false;
                prev_start = false;

                o_vector_out.write(psum_vector_t<Y_DIM, T_PSUM>());
                o_addr.write(0);
                o_wmask.write(sramc_mask_t<Y_DIM>());
                o_valid.write(false);
                o_done.write(false);
                o_lut_op.write(0);
                o_lut_in.write(0.0f);
                o_lut_valid.write(false);
                return;
            }

            // Arm/reset RE accumulation states on rising edge of start pulse
            bool start_edge = i_start.read() && !prev_start;
            prev_start = i_start.read();

            if (start_edge)
            {
                uint32_t mode = i_mode.read();
                uint32_t base_mode = mode & 0xFF;
                if (base_mode == RE_MODE_SOFTMAX_PASS2 || base_mode == RE_MODE_LAYERNORM_PASS2 ||
                    base_mode == RE_MODE_SOFTMAX_TILE_PASS2)
                {
                    scratch_rd_idx = 0;
                }
                else
                {
                    running_max = -INFINITY;
                    running_sum = 0.0;
                    running_tile_sum = 0.0;
                    running_sq_sum = 0.0;
                    running_mean = 0.0;
                    running_var = 0.0;
                    total_active_elements = 0;
                    scratch_wr_idx = 0;
                    scratch_rd_idx = 0;
                    scratch_overflow = false;
                    for (int i = 0; i < SCRATCH_VECTORS; i++)
                    {
                        vector_max[i] = -INFINITY;
                    }
                }
            }

            if (i_valid.read())
            {
                uint32_t mode = i_mode.read();
                uint32_t base_mode = mode & 0xFF;
                o_addr.write(i_addr.read());
                o_wmask.write(i_wmask.read());
                o_lut_valid.write(false);

#ifndef FX1_NO_PERF
                if (perf)
                {
                    if (base_mode == RE_MODE_SOFTMAX_PASS1 || base_mode == RE_MODE_SOFTMAX_PASS2 ||
                        base_mode == RE_MODE_SOFTMAX_TILE_PASS1 || base_mode == RE_MODE_SOFTMAX_TILE_PASS2 ||
                        base_mode == RE_MODE_LAYERNORM_PASS1 || base_mode == RE_MODE_LAYERNORM_PASS2)
                    {
                        perf->reduction_engine_cycles++;
                    }
                    else if (base_mode == RE_MODE_MAXPOOL)
                    {
                        perf->pooling_engine_cycles++;
                    }
                    else if (base_mode == RE_MODE_RESIDUAL_ADD)
                    {
                        perf->activation_engine_cycles++;
                    }
                }
#endif
                psum_vector_t<Y_DIM, T_PSUM> in_vec = i_vector_data.read();
                psum_vector_t<Y_DIM, T_PSUM> out_vec;
                sramc_mask_t<Y_DIM> active_mask = i_rows_active.read();

                // -------------------------------------------------------------
                // 1. Max Comparator Tree (Masked with active rows)
                // -------------------------------------------------------------
                auto compute_max_tree = [&](const psum_vector_t<Y_DIM, T_PSUM> &v) -> float {
                    float local_max = -INFINITY;
                    for (int l = 0; l < Y_DIM; l++)
                    {
                        if (active_mask[l])
                        {
                            float val = static_cast<float>(v[l]);
                            if (val > local_max) local_max = val;
                        }
                    }
                    return local_max;
                };

                // -------------------------------------------------------------
                // 2. Adder/Accumulator Tree (Masked with active rows)
                // -------------------------------------------------------------
                auto compute_sum_tree = [&](const psum_vector_t<Y_DIM, T_PSUM> &v) -> float {
                    float local_sum = 0.0f;
                    for (int l = 0; l < Y_DIM; l++)
                    {
                        if (active_mask[l])
                        {
                            local_sum += static_cast<float>(v[l]);
                        }
                    }
                    return local_sum;
                };
                (void)compute_sum_tree;

                switch (base_mode)
                {
                    case RE_MODE_SOFTMAX_PASS1:
                    case RE_MODE_SOFTMAX_TILE_PASS1:
                    {
                        // Pass 1: Find vector max over active lanes
                        float local_max = compute_max_tree(in_vec);
                        
                        // Store vector in Scratch SRAM for Pass 2 with bounds check
                        if (scratch_wr_idx < SCRATCH_VECTORS)
                        {
                            vector_max[scratch_wr_idx] = local_max;
                            scratch_mem[scratch_wr_idx] = in_vec;
                        }
                        else
                        {
                            scratch_overflow = true;
                        }

                        // If tile-wide softmax (mode 7 or bit 8 set), accumulate online exponent sum
                        if (base_mode == RE_MODE_SOFTMAX_TILE_PASS1 || (mode & 0x100))
                        {
                            if (scratch_wr_idx == 0)
                            {
                                running_max = local_max;
                                running_tile_sum = 0.0;
                                for (int l = 0; l < Y_DIM; l++)
                                {
                                    if (active_mask[l])
                                    {
                                        float diff = static_cast<float>(in_vec[l]) - running_max;
                                        o_lut_op.write(LUT_OP_EXP);
                                        o_lut_in.write(diff);
                                        o_lut_valid.write(true);
                                        running_tile_sum += static_cast<double>(m_rce_exp(diff));
                                    }
                                }
                            }
                            else
                            {
                                float new_max = std::max(running_max, local_max);
                                float max_diff = running_max - new_max;
                                o_lut_op.write(LUT_OP_EXP);
                                o_lut_in.write(max_diff);
                                o_lut_valid.write(true);
                                double scale_prev = static_cast<double>(m_rce_exp(max_diff));
                                running_tile_sum *= scale_prev;
                                for (int l = 0; l < Y_DIM; l++)
                                {
                                    if (active_mask[l])
                                    {
                                        float diff = static_cast<float>(in_vec[l]) - new_max;
                                        o_lut_op.write(LUT_OP_EXP);
                                        o_lut_in.write(diff);
                                        o_lut_valid.write(true);
                                        running_tile_sum += static_cast<double>(m_rce_exp(diff));
                                    }
                                }
                                running_max = new_max;
                            }
                        }
                        else
                        {
                            if (local_max > running_max) running_max = local_max;
                        }

                        scratch_wr_idx++;

                        // Pass 1 does NOT write back to SRAM C
                        o_valid.write(false);
                        o_done.write(false);
                        break;
                    }

                    case RE_MODE_SOFTMAX_PASS2:
                    case RE_MODE_SOFTMAX_TILE_PASS2:
                    {
                        // Retrieve input from Scratch SRAM (safe bounds check against written vectors)
                        uint32_t max_valid_vectors = std::min(scratch_wr_idx, static_cast<uint32_t>(SCRATCH_VECTORS));
                        bool valid_rd = (scratch_rd_idx < max_valid_vectors);
                        if (scratch_rd_idx >= SCRATCH_VECTORS || (scratch_wr_idx > 0 && scratch_rd_idx >= scratch_wr_idx))
                        {
                            scratch_overflow = true;
                        }
                        psum_vector_t<Y_DIM, T_PSUM> orig_vec = valid_rd ? scratch_mem[scratch_rd_idx] : in_vec;
                        float vec_max = valid_rd ? vector_max[scratch_rd_idx] : compute_max_tree(orig_vec);
                        scratch_rd_idx++;

                        bool is_tile = (base_mode == RE_MODE_SOFTMAX_TILE_PASS2) || (mode & 0x100);
                        float norm_max = is_tile ? running_max : vec_max;
                        float recip_sum = 0.0f;

                        if (is_tile)
                        {
                            o_lut_op.write(LUT_OP_RECIP);
                            o_lut_in.write(static_cast<float>(running_tile_sum));
                            o_lut_valid.write(true);
                            recip_sum = (running_tile_sum > 0.0) ? m_rce_recip(static_cast<float>(running_tile_sum)) : 0.0f;
                        }
                        else
                        {
                            running_sum = 0.0;
                            for (int l = 0; l < Y_DIM; l++)
                            {
                                if (active_mask[l])
                                {
                                    float diff = static_cast<float>(orig_vec[l]) - norm_max;
                                    o_lut_op.write(LUT_OP_EXP);
                                    o_lut_in.write(diff);
                                    o_lut_valid.write(true);
                                    float exp_val = m_rce_exp(diff);
                                    running_sum += static_cast<double>(exp_val);
                                }
                            }
                            o_lut_op.write(LUT_OP_RECIP);
                            o_lut_in.write(static_cast<float>(running_sum));
                            o_lut_valid.write(true);
                            recip_sum = (running_sum > 0.0) ? m_rce_recip(static_cast<float>(running_sum)) : 0.0f;
                        }

                        uint64_t scale = i_requant_scale.read();
                        uint32_t shift = i_requant_shift.read();

                        // Output final Softmax: exp(x_i - norm_max) * recip_sum
                        for (int l = 0; l < Y_DIM; l++)
                        {
                            if (active_mask[l])
                            {
                                float diff = static_cast<float>(orig_vec[l]) - norm_max;
                                o_lut_op.write(LUT_OP_EXP);
                                o_lut_in.write(diff);
                                o_lut_valid.write(true);
                                float prob = m_rce_exp(diff) * recip_sum;
                                if (std::is_integral<T_PSUM>::value)
                                {
                                    if (scale > 0)
                                    {
                                        if (shift >= 64)
                                        {
                                            out_vec[l] = 0;
                                        }
                                        else
                                        {
                                            double scaled = static_cast<double>(prob) * static_cast<double>(scale);
                                            double divisor = (shift > 0) ? static_cast<double>(1ULL << shift) : 1.0;
                                            double rounded = std::round(scaled / divisor);
                                            out_vec[l] = static_cast<T_PSUM>(clamp_val<T_ACT>(rounded));
                                        }
                                    }
                                    else
                                    {
                                        out_vec[l] = static_cast<T_PSUM>(clamp_val<T_ACT>(std::round(static_cast<double>(prob) * 127.0)));
                                    }
                                }
                                else
                                {
                                    out_vec[l] = static_cast<T_PSUM>(prob);
                                }
                            }
                            else
                            {
                                out_vec[l] = 0;
                            }
                        }

                        o_vector_out.write(out_vec);
                        o_valid.write(true);
                        o_done.write(true);
                        break;
                    }

                    case RE_MODE_LAYERNORM_PASS1:
                    {
                        // Pass 1: Accumulate sum and sum-of-squares over active lanes (double precision)
                        for (int l = 0; l < Y_DIM; l++)
                        {
                            if (active_mask[l])
                            {
                                double val = static_cast<double>(in_vec[l]);
                                running_sum += val;
                                running_sq_sum += (val * val);
                                total_active_elements++;
                            }
                        }

                        // Save in scratch SRAM for Pass 2 with bounds check
                        if (scratch_wr_idx < SCRATCH_VECTORS)
                        {
                            scratch_mem[scratch_wr_idx] = in_vec;
                        }
                        else
                        {
                            scratch_overflow = true;
                        }
                        scratch_wr_idx++;

                        if (total_active_elements > 0)
                        {
                            running_mean = running_sum / static_cast<double>(total_active_elements);
                            double mean_sq = running_mean * running_mean;
                            double var_calc = (running_sq_sum / static_cast<double>(total_active_elements)) - mean_sq;
                            running_var = (var_calc > 0.0) ? var_calc : 0.0;
                        }

                        // Pass 1 does NOT write back to SRAM C
                        o_valid.write(false);
                        o_done.write(false);
                        break;
                    }

                    case RE_MODE_LAYERNORM_PASS2:
                    {
                        // Retrieve vector (safe bounds check against written vectors)
                        uint32_t max_valid_vectors = std::min(scratch_wr_idx, static_cast<uint32_t>(SCRATCH_VECTORS));
                        bool valid_rd = (scratch_rd_idx < max_valid_vectors);
                        if (scratch_rd_idx >= SCRATCH_VECTORS || (scratch_wr_idx > 0 && scratch_rd_idx >= scratch_wr_idx))
                        {
                            scratch_overflow = true;
                        }
                        psum_vector_t<Y_DIM, T_PSUM> orig_vec = valid_rd ? scratch_mem[scratch_rd_idx] : in_vec;
                        scratch_rd_idx++;

                        // Reciprocal square root of global variance (+ epsilon) computed via companion RCE / LUT
                        o_lut_op.write(LUT_OP_RSQRT);
                        o_lut_in.write(static_cast<float>(running_var + 1e-5));
                        o_lut_valid.write(true);
                        double rsqrt_var = static_cast<double>(m_rce_rsqrt(static_cast<float>(running_var + 1e-5)));
                        uint64_t scale = i_requant_scale.read();
                        uint32_t shift = i_requant_shift.read();

                        // Normalize in double: (x_i - mean) * rsqrt over active lanes
                        for (int l = 0; l < Y_DIM; l++)
                        {
                            if (active_mask[l])
                            {
                                double norm = (static_cast<double>(orig_vec[l]) - running_mean) * rsqrt_var;
                                if (std::is_integral<T_PSUM>::value)
                                {
                                    if (scale > 0)
                                    {
                                        if (shift >= 64)
                                        {
                                            out_vec[l] = 0;
                                        }
                                        else
                                        {
                                            double scaled = norm * static_cast<double>(scale);
                                            double divisor = (shift > 0) ? static_cast<double>(1ULL << shift) : 1.0;
                                            double rounded = std::round(scaled / divisor);
                                            out_vec[l] = static_cast<T_PSUM>(clamp_val<T_ACT>(rounded));
                                        }
                                    }
                                    else
                                    {
                                        out_vec[l] = static_cast<T_PSUM>(clamp_val<T_ACT>(std::round(norm)));
                                    }
                                }
                                else
                                {
                                    out_vec[l] = static_cast<T_PSUM>(norm);
                                }
                            }
                            else
                            {
                                out_vec[l] = 0;
                            }
                        }

                        o_vector_out.write(out_vec);
                        o_valid.write(true);
                        o_done.write(true);
                        break;
                    }

                    case RE_MODE_MAXPOOL:
                    {
                        // MaxPool mode simply uses the active-masked max tree
                        float max_val = compute_max_tree(in_vec);
                        for (int l = 0; l < Y_DIM; l++)
                        {
                            out_vec[l] = active_mask[l] ? static_cast<T_PSUM>(max_val) : 0;
                        }

                        o_vector_out.write(out_vec);
                        o_valid.write(true);
                        o_done.write(true);
                        break;
                    }

                    case RE_MODE_RESIDUAL_ADD:
                    {
                        // Element-wise addition of input and skip connections
                        act_vector_t<Y_DIM, T_ACT> skip = i_skip_data.read();
                        uint64_t scale = i_requant_scale.read();
                        uint32_t shift = i_requant_shift.read();

                        for (int l = 0; l < Y_DIM; l++)
                        {
                            // Dequantize, add, and requantize back to float or INT8
                            double sum_val = static_cast<double>(in_vec[l]) + static_cast<double>(skip[l]);
                            if (scale > 0)
                            {
                                if (shift >= 64)
                                {
                                    sum_val = 0.0;
                                }
                                else
                                {
                                    double scaled = sum_val * static_cast<double>(scale);
                                    double divisor = (shift > 0) ? static_cast<double>(1ULL << shift) : 1.0;
                                    sum_val = std::round(scaled / divisor);
                                }
                            }
                            out_vec[l] = static_cast<T_PSUM>(clamp_val<T_ACT>(sum_val));
                        }

                        o_vector_out.write(out_vec);
                        o_valid.write(true);
                        o_done.write(true);
                        break;
                    }

                    default:
                    {
                        o_vector_out.write(psum_vector_t<Y_DIM, T_PSUM>());
                        o_valid.write(false);
                        o_done.write(false);
                        o_lut_op.write(0);
                        o_lut_in.write(0.0f);
                        o_lut_valid.write(false);
                        break;
                    }
                }
            }
            else
            {
                o_valid.write(false);
                o_done.write(false);
                o_lut_op.write(0);
                o_lut_in.write(0.0f);
                o_lut_valid.write(false);
            }
        }
    };
} // namespace sauria

#endif // SAURIA_RE_RCE_H
