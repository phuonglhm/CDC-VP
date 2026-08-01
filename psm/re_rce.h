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
        RE_MODE_RESIDUAL_ADD = 6     // Elemwise Add (dequant, skip add, requant)
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
            // Map floating input to 8-bit index [0, 255]
            // Input range is typically in (-8, 0] for softmax subtraction
            int32_t idx = static_cast<int32_t>(std::round((x + 8.0f) * 31.875f));
            idx = (idx < 0) ? 0 : ((idx > 255) ? 255 : idx);
            return static_cast<float>(lut_exp[idx]) / 255.0f; // Normalized output
        }

        float lookup_recip(float x) const
        {
            // Map input [0, 1] or similar scale to index [0, 511]
            int32_t idx = static_cast<int32_t>(std::round(x * 511.0f));
            idx = (idx < 0) ? 0 : ((idx > 511) ? 511 : idx);
            return static_cast<float>(lut_recip[idx]) / 511.0f;
        }

        float lookup_rsqrt(float x) const
        {
            // Map input to index [0, 1023]
            int32_t idx = static_cast<int32_t>(std::round(x * 1023.0f));
            idx = (idx < 0) ? 0 : ((idx > 1023) ? 1023 : idx);
            return static_cast<float>(lut_rsqrt[idx]) / 1023.0f;
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
        typename T_PSUM = float,
        typename T_ACT = float>
    class ReductionEngine : public sc_module
    {
    public:
        // Clock & Reset
        sc_in<bool> i_clk{"i_clk"};
        sc_in<bool> i_rstn{"i_rstn"};

        // Data & Control Inputs
        sc_in<uint32_t> i_mode{"i_mode"};
        sc_in<bool> i_start{"i_start"};
        sc_in<bool> i_valid{"i_valid"};
        sc_in<psum_vector_t<Y_DIM, T_PSUM>> i_vector_data{"i_vector_data"};
        sc_in<act_vector_t<Y_DIM, T_ACT>> i_skip_data{"i_skip_data"}; // Residual skip input

        // Scale & Requant params for Residual Skip
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
        sc_out<bool> o_valid{"o_valid"};
        sc_out<bool> o_done{"o_done"};

        SC_CTOR(ReductionEngine)
        {
            SC_METHOD(re_process);
            sensitive << i_clk.pos();
        }

    private:
        // 24 KB Scratch SRAM (stores intermediate vectors during multi-pass calculations)
        // 24 KB / (Y_DIM * sizeof(float)) vectors. For Y_DIM=32, 24576 / 128 = 192 vectors.
        static constexpr int SCRATCH_VECTORS = (24 * 1024) / (Y_DIM * sizeof(T_PSUM) > 0 ? Y_DIM * sizeof(T_PSUM) : 4);
        psum_vector_t<Y_DIM, T_PSUM> scratch_mem[SCRATCH_VECTORS];

        // Running accumulation registers
        float running_max;
        float running_sum;
        float running_mean;
        float running_var;

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
                }
                running_max = -INFINITY;
                running_sum = 0.0f;
                running_mean = 0.0f;
                running_var = 0.0f;

                o_vector_out.write(psum_vector_t<Y_DIM, T_PSUM>());
                o_valid.write(false);
                o_done.write(false);
                o_lut_op.write(0);
                o_lut_in.write(0.0f);
                o_lut_valid.write(false);
                return;
            }

            if (i_start.read() && i_valid.read())
            {
                uint32_t mode = i_mode.read();
                psum_vector_t<Y_DIM, T_PSUM> in_vec = i_vector_data.read();
                psum_vector_t<Y_DIM, T_PSUM> out_vec;

                // -------------------------------------------------------------
                // 1. Max Comparator Tree (Shared between Softmax & MaxPool)
                // -------------------------------------------------------------
                auto compute_max_tree = [](const psum_vector_t<Y_DIM, T_PSUM> &v) -> float {
                    float local_max = -INFINITY;
                    for (int l = 0; l < Y_DIM; l++)
                    {
                        float val = static_cast<float>(v[l]);
                        if (val > local_max) local_max = val;
                    }
                    return local_max;
                };

                // -------------------------------------------------------------
                // 2. Adder/Accumulator Tree
                // -------------------------------------------------------------
                auto compute_sum_tree = [](const psum_vector_t<Y_DIM, T_PSUM> &v) -> float {
                    float local_sum = 0.0f;
                    for (int l = 0; l < Y_DIM; l++)
                    {
                        local_sum += static_cast<float>(v[l]);
                    }
                    return local_sum;
                };

                switch (mode)
                {
                    case RE_MODE_SOFTMAX_PASS1:
                    {
                        // Pass 1: Find row max
                        running_max = compute_max_tree(in_vec);
                        
                        // Store vector in Scratch SRAM for Pass 2
                        scratch_mem[0] = in_vec; 

                        out_vec[0] = running_max; // return max in first lane for verification
                        o_vector_out.write(out_vec);
                        o_valid.write(true);
                        o_done.write(true);
                        break;
                    }

                    case RE_MODE_SOFTMAX_PASS2:
                    {
                        // Retrieve input from Scratch SRAM
                        psum_vector_t<Y_DIM, T_PSUM> orig_vec = scratch_mem[0];
                        psum_vector_t<Y_DIM, T_PSUM> exp_sub_vec;

                        // Compute exp(x_i - max) using functional lookup (emulates RCE lookup)
                        running_sum = 0.0f;
                        for (int l = 0; l < Y_DIM; l++)
                        {
                            float diff = static_cast<float>(orig_vec[l]) - running_max;
                            // Safe range subtraction prevents overflow
                            float exp_val = std::exp(diff);
                            exp_sub_vec[l] = exp_val;
                            running_sum += exp_val;
                        }

                        // Compute reciprocal of running sum
                        float recip_sum = (running_sum > 0.0f) ? (1.0f / running_sum) : 0.0f;

                        // Output final Softmax: exp(x_i - max) * recip_sum
                        for (int l = 0; l < Y_DIM; l++)
                        {
                            out_vec[l] = exp_sub_vec[l] * recip_sum;
                        }

                        o_vector_out.write(out_vec);
                        o_valid.write(true);
                        o_done.write(true);
                        break;
                    }

                    case RE_MODE_LAYERNORM_PASS1:
                    {
                        // Pass 1: Compute mean
                        running_sum = compute_sum_tree(in_vec);
                        running_mean = running_sum / Y_DIM;

                        // Save in scratch SRAM for Pass 2
                        scratch_mem[0] = in_vec;

                        out_vec[0] = running_mean; // return mean in first lane
                        o_vector_out.write(out_vec);
                        o_valid.write(true);
                        o_done.write(true);
                        break;
                    }

                    case RE_MODE_LAYERNORM_PASS2:
                    {
                        // Retrieve vector
                        psum_vector_t<Y_DIM, T_PSUM> orig_vec = scratch_mem[0];

                        // Compute variance: sum((x_i - mean)^2)
                        float sq_sum = 0.0f;
                        for (int l = 0; l < Y_DIM; l++)
                        {
                            float diff = static_cast<float>(orig_vec[l]) - running_mean;
                            sq_sum += diff * diff;
                        }
                        running_var = sq_sum / Y_DIM;

                        // Reciprocal square root of variance (+ epsilon)
                        float rsqrt_var = 1.0f / std::sqrt(running_var + 1e-5f);

                        // Normalize: (x_i - mean) * rsqrt
                        for (int l = 0; l < Y_DIM; l++)
                        {
                            out_vec[l] = (static_cast<float>(orig_vec[l]) - running_mean) * rsqrt_var;
                        }

                        o_vector_out.write(out_vec);
                        o_valid.write(true);
                        o_done.write(true);
                        break;
                    }

                    case RE_MODE_MAXPOOL:
                    {
                        // MaxPool mode simply uses the shared max tree
                        float max_val = compute_max_tree(in_vec);
                        out_vec.data.fill(static_cast<T_PSUM>(max_val));

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
                                sum_val = (sum_val * scale) / (1ULL << shift);
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
                        break;
                    }
                }
            }
            else
            {
                o_valid.write(false);
                o_done.write(false);
            }
        }
    };
} // namespace sauria

#endif // SAURIA_RE_RCE_H
