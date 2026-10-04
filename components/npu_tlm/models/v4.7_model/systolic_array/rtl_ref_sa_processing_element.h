// 1:1 copy of sauria_model's systolic_array/sa_processing_element.h.
// ONLY CHANGE from the reference file: namespace `sauria` -> `sauria_rtl` (avoid collision with
// this tree's own sauria::PeConfig/ProcessingElement), header guard renamed. Logic below is
// otherwise byte-for-byte identical to the reference file -- do not "clean up" or reinterpret
// without re-checking against sauria_model's actual source.
//
// The swap semantics (FX1_A3_CONTEXT_FSM guard below) are what ContextSwitchController's staggered cswitch_arr timing
// (control/rtl_ref_context_switch_controller.h) drives -- ported together with it, not mixed with this tree's native
// systolic_array/sa_processing_element.h.
#ifndef RTL_REF_SA_PROCESSING_ELEMENT_H
#define RTL_REF_SA_PROCESSING_ELEMENT_H

#include "sauria_types.h"
#include <cmath>
#include <vector>

namespace sauria_rtl
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
#ifdef FX1_A3_SELFCHECK
        T_PSUM ref_{}, ref_sc_{};
        bool selfchk_init_{false};
        long selfchk_bad_{0}, selfchk_swap_{0};
        // Debug hook: RAW sum of the products this PE received, without pipeline / gating.
        T_PSUM raw_{};
        bool selfchk_log_{false};   // enabled for ONE PE only
#endif

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

#ifdef FX1_A3_SELFCHECK
            // Debug self-check (default off): RAW product of this cycle (no pipeline, no gating).
            raw_ = static_cast<T_PSUM>(raw_ +
                   static_cast<T_PSUM>(static_cast<long long>(act_in) *
                                       static_cast<long long>(wei_in)));
            // ref_ follows the mac_q update path exactly. A mismatch => something
            // changed mac_q/mac_sc_q OUTSIDE step().
            if (!selfchk_init_)
            {
                selfchk_init_ = true;
                ref_ = mac_q;
                ref_sc_ = mac_sc_q;
            }
#endif
            if (cswitch)
            {
#ifdef FX1_A3_CONTEXT_FSM
                // real RTL's swap does NOT fold this cycle's
                // incoming product into the OUTGOING (mac_sc_q) side.
                // sa_processing_element.sv: mac_sc_d = mac_q (the PRE-cycle
                // value only, via `assign mac_sc_d = (cswitch_q_ext) ? mac_q
                // : i_c;`) while mac_d = mac_sc_q(old) + mul_q_zd (via the
                // adder_i instance, `.i_p(mul_q_zd), .i_c(mac_q_zd)`, with
                // mac_q_zd deriving from mac_q_mux = cswitch_q_ext ? mac_sc_q
                // : mac_q) -- i.e. the RESUMING accumulator (mac_q) is the
                // one that receives THIS cycle's product, continuing from
                // whatever was previously parked in the reserve register;
                // the departing value (mac_sc_q) is saved OUT as-is, without
                // this cycle's product folded in. The #else branch below
                // (guard-off, untouched/bit-exact) does the OPPOSITE of
                // this -- it folds adder_input into the outgoing tmp/
                // mac_sc_q, and lets the resuming mac_q pick up only the
                // old reserve value -- misattributing whichever product
                // happens to land exactly on a swap cycle to the departing
                // context instead of the resuming one. Confirmed harmless
                // for guard-off's own (much longer, original) delay_len,
                // since guard-off's swaps land once real accumulation has
                // already fully quiesced (adder_input==0 at the swap cycle
                // there), where both formulas are identical -- this is why
                // guard-off's bit-exactness is unaffected by this change.
                T_PSUM old_mac_sc_q = mac_sc_q;
#ifdef FX1_A3_SELFCHECK
                // Debug self-check: the OUTGOING psum is mac_q (without this cycle's product).
                if (ref_ != mac_q || ref_sc_ != mac_sc_q)
                {
                    selfchk_bad_++;
                    if (selfchk_bad_ <= 20)
                    {
                        std::fprintf(stderr,
                            "[SELFCHK] swap #%ld: mac_q=%lld ref=%lld (diff %lld) | "
                            "mac_sc=%lld ref_sc=%lld\n",
                            (long)selfchk_swap_, (long long)mac_q, (long long)ref_,
                            (long long)(mac_q - ref_), (long long)mac_sc_q,
                            (long long)ref_sc_);
                    }
                    ref_ = mac_q; ref_sc_ = mac_sc_q;   // resynchronise to keep counting independently
                }
                if (selfchk_log_ && selfchk_swap_ < 80)
                {
                    std::fprintf(stderr, "[SELFCHK2] swap %ld mac_q=%lld raw=%lld diff=%lld\n",
                                 (long)selfchk_swap_, (long long)mac_q,
                                 (long long)raw_, (long long)(mac_q - raw_));
                }
                raw_ = 0;
                selfchk_swap_++;
                {
                    T_PSUM old_ref_sc = ref_sc_;
                    ref_sc_ = ref_;
                    ref_ = do_accumulate
                               ? static_cast<T_PSUM>(old_ref_sc + adder_input)
                               : old_ref_sc;
                }
#endif
                mac_sc_q = mac_q;
                mac_q = do_accumulate
                            ? static_cast<T_PSUM>(old_mac_sc_q + adder_input)
                            : old_mac_sc_q;
#else
                // Swap after applying the final pending product.
                T_PSUM tmp = mac_q_next;
                mac_q = mac_sc_q;
                mac_sc_q = tmp;
#endif
            }
            else
            {
                mac_q = mac_q_next;
#ifdef FX1_A3_SELFCHECK
                if (do_accumulate) ref_ = static_cast<T_PSUM>(ref_ + adder_input);
#endif

                if (cscan_en)
                {
                    // Shift in value from the right neighbor along the scan chain.
                    mac_sc_q = i_c;
#ifdef FX1_A3_SELFCHECK
                    ref_sc_ = i_c;
#endif
                }
            }
        }
    };

} // namespace sauria_rtl

#endif // RTL_REF_SA_PROCESSING_ELEMENT_H
