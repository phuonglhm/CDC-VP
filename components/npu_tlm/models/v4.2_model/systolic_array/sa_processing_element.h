// SystemC Model for SAURIA NPU Core
// Processing Element (PE) block with configurable hardware parameters

#ifndef SAURIA_SA_PROCESSING_ELEMENT_H
#define SAURIA_SA_PROCESSING_ELEMENT_H

#include "sauria_types.h"
#include <cmath>
#include <vector>

namespace sauria
{

    struct PeConfig
    {
        int arithmetic_type{1};                 // 0 = INT, 1 = FP
        int mul_type{0};                        // 0 = standard, 1 = approximate
        int add_type{0};                        // 0 = standard, 1 = approximate
        float m_approx{1.0f};                   // Multiplier approximation factor
        float a_approx{1.0f};                   // Adder approximation factor
        int stages_mul{1};                      // Multiplier pipeline stages
        bool intermediate_pipeline_stage{true}; // Pipeline register stage between multiplier and adder
        bool zero_gating_mult{true};            // Gating control switches
        bool zero_gating_add{false};
        bool zd_lookahead{false};
        bool extra_csreg{false};
    };

    template <typename T_ACT = float, typename T_WEI = float, typename T_PSUM = float>
    class ProcessingElement
    {
    public:
        PeConfig config;

        // Registers
        T_ACT a_q{static_cast<T_ACT>(0)};
        T_WEI b_q{static_cast<T_WEI>(0)};
        T_PSUM mac_q{static_cast<T_PSUM>(0)};    // Local active accumulator register
        T_PSUM mac_sc_q{static_cast<T_PSUM>(0)}; // Shadow context / Scan-chain register

        // Gated inputs for power-saving zero-gating simulation
        T_ACT a_zd_q{static_cast<T_ACT>(0)};
        T_WEI b_zd_q{static_cast<T_WEI>(0)};
        T_PSUM mhold_q{static_cast<T_PSUM>(0)};

        // Multiplier pipeline delay line
        std::vector<T_PSUM> mul_pipeline;

        // Constructor
        ProcessingElement(const PeConfig &cfg = PeConfig()) : config(cfg) {}

        // Reset state
        void reset()
        {
            a_q = static_cast<T_ACT>(0);
            b_q = static_cast<T_WEI>(0);
            mac_q = static_cast<T_PSUM>(0);
            mac_sc_q = static_cast<T_PSUM>(0);
            a_zd_q = static_cast<T_ACT>(0);
            b_zd_q = static_cast<T_WEI>(0);
            mhold_q = static_cast<T_PSUM>(0);
            mul_pipeline.clear();

            // Pre-fill pipeline with zeroes
            int total_stages = config.stages_mul + (config.intermediate_pipeline_stage ? 1 : 0);
            if (total_stages > 0)
            {
                mul_pipeline.assign(total_stages, static_cast<T_PSUM>(0));
            }
        }

        // Cycle-by-cycle behavioral execution step
        void step(T_ACT i_a, T_WEI i_b, T_PSUM i_c,
                  bool cswitch, bool cscan_en, bool pipeline_en,
                  float threshold)
        {

            if (!pipeline_en)
                return; // Stall execution

            // 1. Arithmetic representation (INT / FP)
            T_ACT act_in = i_a;
            T_WEI wei_in = i_b;
            if (config.arithmetic_type == 0)
            {
                act_in = static_cast<T_ACT>(static_cast<int>(i_a));
                wei_in = static_cast<T_WEI>(static_cast<int>(i_b));
            }

            // 2. Inputs propagation
            a_q = act_in;
            b_q = wei_in;

            // 3. Zero-Gating / Negligence detection
            bool is_a_zero = (std::abs(static_cast<double>(act_in)) <= threshold);
            bool is_b_zero = (std::abs(static_cast<double>(wei_in)) <= threshold);
            bool zero_det = is_a_zero || is_b_zero;

            // 4. Multiplier gating simulation
            T_ACT mult_in_a = act_in;
            T_WEI mult_in_b = wei_in;
            if (config.zero_gating_mult && zero_det)
            {
                // Freeze multiplier inputs to avoid dynamic toggling
                mult_in_a = a_zd_q;
                mult_in_b = b_zd_q;
            }
            else
            {
                a_zd_q = act_in;
                b_zd_q = wei_in;
            }

            T_PSUM raw_mult_out;
            if (config.arithmetic_type == 0)
            {
                // INT mode: exact integer product at the INPUT type's full width.
                // T_ACT/T_WEI are the integer element types (int8_t, int16_t, ...),
                // so casting through them preserves sign+width. For int8 this is
                // identical to the previous `static_cast<int8_t>` (no-op), so all
                // INT8 configs are unchanged; int16 no longer gets truncated to 8b.
                int64_t mult_a_s = static_cast<int64_t>(static_cast<T_ACT>(mult_in_a));
                int64_t mult_b_s = static_cast<int64_t>(static_cast<T_WEI>(mult_in_b));
                raw_mult_out = static_cast<T_PSUM>(mult_a_s * mult_b_s);
            }
            else
            {
                // FP mode: multiply in native (wide) float precision. T_PSUM
                // accumulates the full-precision sum; rounding down to the
                // storage type (e.g. fp16_t) happens only when the result
                // leaves the array (DRAM write-out), matching the SAURIA
                // "ideal" reference model (float accumulate, round once).
                double mult_a_f = static_cast<double>(mult_in_a);
                double mult_b_f = static_cast<double>(mult_in_b);
                raw_mult_out = static_cast<T_PSUM>(mult_a_f * mult_b_f);
            }
            if (config.mul_type == 1)
            {
                // Apply approximate multiplication scaling factor
                raw_mult_out = static_cast<T_PSUM>(raw_mult_out * config.m_approx);
            }

            if (zero_det)
            {
                raw_mult_out = static_cast<T_PSUM>(0);
            }

            // Pipeline latency delay line
            T_PSUM mult_out = raw_mult_out;
            int total_stages = config.stages_mul + (config.intermediate_pipeline_stage ? 1 : 0);
            if (total_stages > 0)
            {
                mul_pipeline.push_back(raw_mult_out);
                mult_out = mul_pipeline.front();
                mul_pipeline.erase(mul_pipeline.begin());
            }

            // 5. Local accumulation and context swapping / shifting

            T_PSUM adder_input = mult_out;

            if (config.add_type == 1)
            {
                adder_input = static_cast<T_PSUM>(adder_input * config.a_approx);
            }

            // IMPORTANT:
            // zero_gating_add must be aligned with multiplier pipeline.
            // Since zero_det belongs to current input but mult_out is delayed,
            // do not use current zero_det to gate a delayed product here.
            // For now, keep accumulation enabled unless adder_input is actually zero.
            bool do_accumulate = true;

            if (config.zero_gating_add)
            {
                do_accumulate = (adder_input != static_cast<T_PSUM>(0));
            }

            // Accumulate before cswitch.
            // If multiplier pipeline still contains the last product,
            // it must be added to the active accumulator before swapping to scan context.
            T_PSUM mac_q_next = mac_q;

            if (do_accumulate)
            {
                mac_q_next = static_cast<T_PSUM>(mac_q_next + adder_input);
            }

            if (cswitch)
            {
                // Swap after applying the final pending product.
                T_PSUM tmp = mac_q_next;
                mac_q = mac_sc_q;
                mac_sc_q = tmp;
            }
            else
            {
                mac_q = mac_q_next;

                if (cscan_en)
                {
                    // Shift in value from the right neighbor along the scan chain.
                    mac_sc_q = i_c;
                }
            }
        }
    };

} // namespace sauria

#endif // SAURIA_SA_PROCESSING_ELEMENT_H
	
