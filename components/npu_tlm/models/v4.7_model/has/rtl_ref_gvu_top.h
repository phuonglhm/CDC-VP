// rtl_ref_gvu_top.h -- Top level of the vector unit (GVU), ported from the RTL (neo_core/gvu/gvu_top.sv).
// Integrates gvu_pipe_controller.sv, gvu_vlsu.sv, gvu_scratchpad.sv, stream_vrf.sv, gvu_lut_unit.sv,
// and functional units (gvu_vadder64, gvu_vadder32, gvu_vmul48x40, gvu_vmul32x32, gvu_vcmp32,
// gvu_vroundshift72, gvu_vroundshift64, gvu_vector_clamp).
//
// Dual-use design:
// 1. Pure C++17 cycle model (GvuTop) with eval() and tick() methods, compatible with the standalone
//    rtl_ref test harness without requiring SystemC compilation or libraries.
// 2. SystemC sc_module wrapper (ScGvuTop) available when <systemc.h> is included or when compiled
//    in a SystemC virtual prototype environment (like has_npu_top.h).
#ifndef RTL_REF_GVU_TOP_H
#define RTL_REF_GVU_TOP_H

#include <algorithm>
#include <array>
#include <cstdint>
#include <deque>
#include <vector>
#include <cstring>
#include "has/rtl_ref_gvu_fu.h"
#include "has/rtl_ref_gvu_scratchpad.h"
#include "has/rtl_ref_gvu_vrf.h"
#include "has/rtl_ref_gvu_lut.h"
#include "has/rtl_ref_gvu_vlsu.h"
#include "has/gvu_agu_nd_vlsu.h"
#include "has/rtl_ref_gvu_ctrl.h"

// Depth of the GEMM_FUSED stream pipeline (In::i_gemm_stream): clock edges from the latch of a beat to its result on
// o_gemm_fused_data. The one place that sets it for GvuTop, the clocked connection (GvuTopStream::kDepth) and the
// call-style connection (GvuTopBridge::kDepth); override at build time with -DGVU_GEMM_STREAM_DEPTH=<n>. 9 = the register
// cuts of the MAS Requant / GEMM_FUSED drawings. The hardware RTL measures 11: two more cycles to read the per-channel
// Out_Scale / Out_Shift from the sbank in front of the multiplier. A depth above 9 adds pure delay stages (values do not
// change); the computing stages stay those of the MAS. Range 9 .. GEMM_STREAM_DEPTH_MAX.
#ifndef GVU_GEMM_STREAM_DEPTH
#define GVU_GEMM_STREAM_DEPTH 9
#endif

namespace rtl_ref_gvu
{
    static constexpr int GEMM_STREAM_DEPTH_MAS = 9;
    static constexpr int GEMM_STREAM_DEPTH_MAX = 16;
    static constexpr int GEMM_STREAM_DEPTH = GVU_GEMM_STREAM_DEPTH;
    static_assert(GEMM_STREAM_DEPTH >= GEMM_STREAM_DEPTH_MAS && GEMM_STREAM_DEPTH <= GEMM_STREAM_DEPTH_MAX,
                  "GVU_GEMM_STREAM_DEPTH out of range");

    // ---- GVU Top Parameters matching gvu_top.sv -------------------------------------------------------------
    struct GvuTopParams
    {
        // Depth of the GEMM_FUSED stream pipeline of this unit (see GEMM_STREAM_DEPTH; a test may build one unit with
        // another depth). Values outside 9 .. GEMM_STREAM_DEPTH_MAX are clamped to that range.
        int gemm_stream_depth{GEMM_STREAM_DEPTH};
        int NUM_LANE{GVU_LANES};
        int DMA_AW{32};
        int DMA_DW{128};
        int LUT_INIT_DW{32 * GVU_LANES}; // 1024 bits
        ScratchpadParams sp_params{};
        LutParams lut_params{false, true, true, 32 * GVU_LANES}; // LERP_MUL_PIPE=1, LERP_SHIFT_PIPE=1, INIT_DW=1024
        FuParams fu_params{};
    };

    // Opcode definitions from gvu_pkg.sv
    enum GvuOpcode : int
    {
        GVU_OP_REQUANT     = 0,
        GVU_OP_DEQUANT     = 1,
        GVU_OP_GEMM_FUSED  = 2,
        GVU_OP_SOFTMAX     = 3,
        GVU_OP_FUSED_ATTN  = 4,
        GVU_OP_LAYERNORM   = 5,
        GVU_OP_RESIDUAL    = 6,
        GVU_OP_MAXPOOL     = 7,
        GVU_OP_AVGPOOL     = 8,
        GVU_OP_ELEMWISE    = 9
    };

    // Control FSM state definitions from gvu_pkg.sv
    enum control_fsm_t : int
    {
        IDLE            = 0,
        LUT_ACT_INIT    = 1,
        LUT_EXP_INIT    = 2,
        LUT_RECIP_INIT  = 3,
        LUT_RSQRT_INIT  = 4,
        SOFTMAX_ST1     = 5,
        SOFTMAX_ST2     = 6,
        SOFTMAX_ST3     = 7,
        LAYERNORM_ST1   = 8,
        LAYERNORM_ST2   = 9,
        LAYERNORM_ST3   = 10,
        LAYERNORM_ST4   = 11,
        LAYERNORM_ST5   = 12,
        GEMM_FUSED_ST1  = 13,
        RESIDUAL_ST1    = 14,
        RESIDUAL_ST2    = 15,
        POOLING_MAX     = 16     // latest hardware RTL drop (gvu_pkg.sv numbers its states differently; the names are the same)
    };

    // ---- Cycle-accurate structural C++ core model of gvu_top.sv --------------------------------------------
    class GvuTop
    {
    public:
        // Top-level input beat matching gvu_top.sv ports
        struct In
        {
            bool i_pipeline_en{false};
            GvuOpcode i_gvu_op{GVU_OP_SOFTMAX};

            // Systolic Array interface
            bool i_psum_valid{false};
            uint32_t i_psum_data{0};

            // Softmax / Attn parameters
            uint32_t i_num_tile{1};
            uint32_t i_token_len{0};
            int8_t i_attn_z{0};
            int32_t i_attn_scale{0};
            uint8_t i_attn_shift{0};

            // LayerNorm configuration parameters matching MAS Section 5.2.5 and has::LnParams
            int32_t i_ln_h{0};           // H channel dimension (defaults to i_token_len if 0)
            int32_t i_ln_pre_shift{0};   // Pre_Shift for variance
            int64_t i_ln_m0_var{1};      // M0_var
            int32_t i_ln_ts_var{0};      // TS_var
            int32_t i_ln_z_var{-128};    // Z_var
            int64_t i_ln_m0_7{1};        // M0_7
            int32_t i_ln_ts_7{0};        // TS_7
            int64_t i_ln_e_bias{0};      // E_bias (epsilon bias)
            int32_t i_ln_z_7{-128};      // Z_7
            int64_t i_ln_m0_div{1};      // M0_div
            int32_t i_ln_ts_div{0};      // TS_div
            int32_t i_ln_z_div{0};       // Z_div
            int64_t i_ln_m0_mul{1};      // M0_mul
            int32_t i_ln_ts_mul{0};      // TS_mul
            int32_t i_ln_z_mul{0};       // Z_mul
            int32_t i_ln_z_out{0};       // Z_out
            bool    i_ln_r8_floor_z_out{true};
            int32_t i_ln_r8_z_out{1};
            bool    i_ln_r9_exact{true};
            int32_t i_ln_r9_lsb_bits{32};
            bool    i_ln_out_int16{false};

            // GEMM_FUSED configuration parameters matching MAS Section 5.2.2 & 5.2.1
            int32_t i_gemm_scale{1};
            int8_t  i_gemm_shift{0};
            int8_t  i_gemm_zp{0};
            bool    i_gemm_per_channel{false};
            bool    i_gemm_skip_act{false};
            bool    i_gemm_skip_zp{false};
            bool    i_gemm_out_int16{false};
            // Round-shift mode of the Requant step (the layer's rounding mode on the core path; the default keeps the
            // datasets of target/test/gemm_fused).
            RoundMode i_gemm_round{RND_NEAR_AWAY};
            // Narrowing to int16 after the round-shift, in the tile mode and in the stream mode: NARROW_SATURATE (default, as
            // GvuRequantPipe of has/rtl_ref_gvu_requant.h and has::requant with has::SAT16) or NARROW_WRAP (keep the low 16
            // bits, as the hardware RTL; has::requant with has::WRAP16). Held while beats are in flight, like the rounding mode.
            NarrowMode i_gemm_narrow{NARROW_SATURATE};
            // Load the activation table again from vbank1 buffer 1 before the next GEMM_FUSED run (a new layer has its own
            // table). Hold it with i_pipeline_en until the controller is in LUT_ACT_INIT, then drop it: the controller passes
            // through IDLE between tiles and would load the table again.
            bool    i_gemm_lut_reload{false};
            std::array<int32_t, GVU_LANES> i_psum_vec{};
            // Stream mode of GEMM_FUSED (default off = the tile mode above). For the core path: one beat per PSM write to the
            // PSUM SRAM, at most one beat per cycle, gaps allowed. A beat is i_psum_valid + i_psum_vec, with its word address
            // and lane mask carried through the pipeline, and the Out_Scale / Out_Shift of its output channel (one channel
            // per beat). The zero point is 16 bits (i_gemm_zp16; i_gemm_skip_zp still forces 0), the output is int8 (no
            // i_gemm_out_int16), and the rounding mode, narrowing, zero point and i_gemm_skip_act are held while beats are in
            // flight. In GEMM_FUSED_ST1 the mode counts no tokens and does not read vbank2 or the sbank or write vbank0;
            // i_gemm_stream_stop ends the run (back to IDLE) once the pipeline is empty. A beat with an empty mask goes
            // through and leaves with an empty mask. Depth GvuTopParams::gemm_stream_depth cycles (default 9), the register
            // cuts of the MAS Requant / GEMM_FUSED drawings: multiplier 32x32 4 + v32int64 register 1, round-shift (narrow
            // to int16, i_gemm_narrow) 1 + v32int16 register 1, add zero point (16 bits, wraps) + v32int16 register 1, clamp
            // to int8 + v32int8 register 1, LUT_act 0; a depth above 9 lengthens the multiplier register chain by pure delay.
            bool     i_gemm_stream{false};
            bool     i_gemm_stream_stop{false};
            // HAS_GVU_REGS: the run ends by the flags of the PSM stream, as the RTL controller does (gvu_pipe_controller.sv:250-266,
            // 624): a beat carrying i_oc_last & i_oc_tile_last (with i_gemm_exit_flags set) is marked, and when it leaves the pipeline
            // (the last write) the unit goes back to IDLE and reports pipe_done. i_gemm_stream_stop (a signal of the model only) is
            // not needed then; it stays for the callers that do not set i_gemm_exit_flags (default: off, nothing changes for them).
            bool     i_gemm_exit_flags{false};
            bool     i_oc_last{false};
            bool     i_oc_tile_last{false};
            // Level reset of the activation table flag (RTL i_lut_act_reset, gvu_pipe_controller.sv:698): while high the flag is cleared,
            // the next run from IDLE loads the table. Unlike i_gemm_lut_reload it acts in any state.
            bool     i_lut_act_reset{false};
            uint32_t i_psum_addr{0};
            uint32_t i_psum_mask{0xFFFFFFFFu};
            int32_t  i_psum_scale{1};
            uint8_t  i_psum_shift{0};
            int32_t  i_gemm_zp16{0};

            // Residual Add configuration parameters matching MAS Section 5.2.6 & Figure 5-7
            int32_t i_res_scale_a{1};
            uint8_t i_res_shift_a{0};
            int8_t  i_res_zp_a{0};
            int32_t i_res_scale_b{1};
            uint8_t i_res_shift_b{0};
            int8_t  i_res_zp_b{0};
            int32_t i_res_scale_out{1};
            uint8_t i_res_shift_out{0};
            int8_t  i_res_zp_out{0};
            RoundMode i_res_round{RND_NEAR_UP};
            bool    i_res_out_int16{false};
            unsigned i_res_a_row{0};
            unsigned i_res_b_row{0};
            unsigned i_res_sum_row{0};
            // Lanes written in the last row of a residual run (last tile, last token) by both stages: a tensor whose length
            // is not a multiple of 32 leaves the other lanes of that row untouched. Default all lanes.
            uint32_t i_res_last_lane_mask{0xFFFFFFFFu};

            // Max pool (latest hardware RTL drop, gvu_top.sv i_pooling_*; GVU registers POOLING_0 / POOLING_1). The input tensor sits in
            // vbank0 buffer i_vbank0_buf_sel as rows of 32 bytes: row c * H * num_w_tile + h * num_w_tile + w_tile (no
            // padding in the unit); the result goes to vbank2 port 1 (v32int8), row c * OH * num_w_tile + oh * num_w_tile +
            // w_tile, from row 0. c_move = num_w_tile * H, h_move = num_w_tile * stride. Supported as in the RTL: 2x2 s1/s2,
            // 3x3 s1, 4x4 s1/s2/s4, 5x5 s1.
            int      i_pooling_mode{POOLING_2X2};
            unsigned i_pooling_stride{1};
            unsigned i_pooling_num_w_tile{1};
            unsigned i_pooling_c{1};
            unsigned i_pooling_oh{1};
            unsigned i_pooling_c_move{0};
            unsigned i_pooling_h_move{0};

            // AGU_ND (model proposal, Reg 16..24, has/gvu_agu_nd.h): AGU_ND_CTRL and the source / destination descriptors. Nothing bound
            // (default) = the unit as before. Residual add: vbank0 bound as source (A, RESIDUAL_ST1) and / or destination (result,
            // RESIDUAL_ST2), num_tile 1. Max pool: destination bound to vbank2 places output row k at addr_at(k).
            AguNdCtrl i_agu_nd_ctrl{};
            AguNdDesc i_agu_src{}, i_agu_dst{};

            // Ping-pong buffer selection
            bool i_sbank_buf_sel{false};
            bool i_vbank0_buf_sel{false};
            bool i_vbank1_buf_sel{false};
            bool i_vbank2_buf_sel{false};

            // Scratchpad DMA access
            bool i_scratch_dma_en{false};
            bool i_scratch_dma_wen{false};
            uint32_t i_scratch_dma_addr{0};
            SpBytes i_scratch_dma_wdata{};
            SpBytes i_scratch_dma_wmask{};
        };

        // Top-level output beat matching gvu_top.sv ports
        struct Out
        {
            bool o_psum_ready{false};
            bool o_softmax_lut_init_done{false};
            bool o_softmax_pipe_done{false};
            bool o_layernorm_lut_init_done{false};
            bool o_layernorm_pipe_done{false};
            bool o_gemm_fused_lut_init_done{false};
            bool o_gemm_fused_pipe_done{false};
            bool o_residual_pipe_done{false};
            control_fsm_t o_gvu_pipe_ctrl_state{IDLE};
            SpBytes o_scratch_dma_rdata{};
            std::array<int8_t, GVU_LANES> o_gemm_fused_data{};
            bool o_gemm_fused_valid{false};
            uint32_t o_gemm_fused_addr{0};     // stream mode: word address and lane mask of the beat on o_gemm_fused_data
            uint32_t o_gemm_fused_mask{0};
            bool o_gemm_fused_busy{false};     // stream mode: a beat is in the pipeline
            std::array<int8_t, GVU_LANES> o_residual_data{};
            bool o_residual_valid{false};
            bool o_maxpool_pipe_done{false};
            // Scratchpad requests of this cycle, for cycle traces against the RTL (tools/has/gvu_oracle/tb_oracle_maxpool.sv)
            bool d_vbank0_en{false};
            unsigned d_vbank0_addr{0};
            bool d_vbank2_wen_1{false};
            unsigned d_vbank2_addr_1{0};
            uint32_t d_vbank2_wmask_1{0};
            std::array<uint8_t, GVU_LANES> d_vbank2_wdata_1{};
        };

        explicit GvuTop(const GvuTopParams &p = GvuTopParams())
            : P_(p),
              mul_n_(kStreamMul + std::min(std::max(p.gemm_stream_depth, GEMM_STREAM_DEPTH_MAS), GEMM_STREAM_DEPTH_MAX) - GEMM_STREAM_DEPTH_MAS),
              scratchpad_(p.sp_params),
              vlsu_(p.sp_params),
              stream_vrf_(),
              vlut_(p.lut_params),
              vcmp_(p.fu_params),
              sm_v_(static_cast<size_t>(mul_n_), 0),
              sm_d_(static_cast<size_t>(mul_n_))
        {
            exp_e_shift_buf_.fill({});
            vbank0_rd_delay_.fill(false);
            vbank1_rd_delay_.fill(false);
            vbank2_rd_delay_.fill(false);
            sbank_rd_delay_.fill(false);
            st1_last_rd_delay_.fill(false);
            b_part_delay_.fill(0);
        }

        const GvuTopParams &params() const { return P_; }
        // Depth of the GEMM_FUSED stream pipeline of this unit (GvuTopParams::gemm_stream_depth after the clamp).
        int gemm_stream_depth() const { return mul_n_ + kStreamRs + 2; }

        // Component accessors for inspection and testing
        GvuScratchpad &scratchpad() { return scratchpad_; }
        const GvuScratchpad &scratchpad() const { return scratchpad_; }
        StreamVrf &stream_vrf() { return stream_vrf_; }
        const StreamVrf &stream_vrf() const { return stream_vrf_; }
        LutUnit &vlut() { return vlut_; }
        const LutUnit &vlut() const { return vlut_; }
        GvuVlsu &vlsu() { return vlsu_.frozen(); }
        const GvuVlsu &vlsu() const { return vlsu_.frozen(); }
        control_fsm_t state() const { return pipe_ctrl_fsm_; }
        uint32_t start_d_cnt() const { return start_d_cnt_; }
        uint32_t end_d_cnt() const { return end_d_cnt_; }
        uint32_t tile_cnt() const { return tile_cnt_; }

        // Evaluates combinational logic, multiplexers, and submodules
        Out eval(const In &in) const
        {
            Wires w;
            return comb(in, w);
        }

        // Helper functions for LayerNorm wide rounding and requantization
        static inline i128 ln_rshift_wide(i128 p, int s)
        {
            if (s <= 0) return p;
            if (s > 126) return 0;
            const i128 half = static_cast<i128>(1) << (s - 1);
            i128 q = p >> s;
            const i128 rem = p - (q << s);
            if (rem > half || (rem == half && (q & 1))) q += 1;
            return q;
        }

        static inline int32_t ln_requant_clamp(i128 x, int64_t S, int s, int zp, int lo, int hi)
        {
            const i128 r = ln_rshift_wide(x * S, s) + zp;
            if (r < lo) return lo;
            if (r > hi) return hi;
            return static_cast<int32_t>(r);
        }

        // Advances the model by one clock edge and returns active combinational outputs
        Out tick(const In &in, bool i_rstn = true)
        {
            Wires w;
            const Out o = comb(in, w);

            if (!i_rstn)
            {
                pipe_ctrl_fsm_ = IDLE;
                is_lut_exp_init_ = false;
                is_lut_recip_init_ = false;
                is_lut_rsqrt_init_ = false;
                is_lut_act_init_ = false;
                start_d_cnt_ = 0;
                end_d_cnt_ = 0;
                tile_cnt_ = 0;
                softmax_st1_pipe_en_r_ = false;
                ln_s_acc_ = 0;
                ln_v_acc_ = 0;
                ln_final_s_ = 0;
                ln_final_var_ = 0;
                ln_final_w_ = 1;
                exp_e_shift_buf_.fill({});
                vbank0_rd_delay_.fill(false);
                vbank1_rd_delay_.fill(false);
                vbank2_rd_delay_.fill(false);
                sbank_rd_delay_.fill(false);
                st1_last_rd_delay_.fill(false);
                b_part_delay_.fill(0);
                scratchpad_.reset_addr_delays();
                stream_clear();
                vlsu_.reset_pooling();
                return o;
            }

            // Stream mode of GEMM_FUSED: its own pipeline registers (the tile-mode signals below see no beat)
            if (pipe_ctrl_fsm_ == GEMM_FUSED_ST1 && in.i_gemm_stream)
                stream_tick(in);

            // Submodule clock edge updates
            vlut_.tick(w.lut_in, i_rstn);

            scratchpad_.tick(w.sp_in);
            vlsu_.bind(in.i_agu_nd_ctrl, in.i_agu_src, in.i_agu_dst, w.agu_nd_sel);
            vlsu_.tick(w.vlsu_in);
            stream_vrf_.tick(w.vrf_in, i_rstn);
            vcmp_.tick(w.cmp_in);

            // Shift register updates (3-cycle LUT E_shift buffer)
            for (size_t s = exp_e_shift_buf_.size() - 1; s > 0; s--)
                exp_e_shift_buf_[s] = exp_e_shift_buf_[s - 1];
            for (int l = 0; l < GVU_LANES; l++)
                exp_e_shift_buf_[0][l] = w.lut_out.o_e_shift[l];

            // 2-cycle Scratchpad read delays (capture unshifted delayed valid beat for pipeline counter updates)
            const bool vbank0_valid_beat = vbank0_rd_delay_[1];
            const bool vbank2_valid_beat = vbank2_rd_delay_[1];
            vbank0_rd_delay_[1] = vbank0_rd_delay_[0];
            vbank0_rd_delay_[0] = w.ctrl_vbank0_rd;
            vbank1_rd_delay_[1] = vbank1_rd_delay_[0];
            vbank1_rd_delay_[0] = w.ctrl_vbank1_rd;
            vbank2_rd_delay_[1] = vbank2_rd_delay_[0];
            vbank2_rd_delay_[0] = w.ctrl_vbank2_rd;
            sbank_rd_delay_[1] = sbank_rd_delay_[0];
            sbank_rd_delay_[0] = w.ctrl_sbank_rd_0 || w.ctrl_sbank_rd_1;
            st1_last_rd_delay_[1] = st1_last_rd_delay_[0];
            st1_last_rd_delay_[0] = w.softmax_st1_last_rd;
            b_part_delay_[1] = b_part_delay_[0];
            b_part_delay_[0] = (w.ctrl_vbank1_rd && pipe_ctrl_fsm_ == RESIDUAL_ST1)
                ? static_cast<unsigned>((in.i_res_b_row + tile_cnt_ * in.i_token_len + start_d_cnt_) & 3u) : 0u;

            // Pipeline controller counters and FSM updates
            softmax_st1_pipe_en_r_ = w.softmax_st1_pipe_en;

            // Token length counters (start_d_cnt_proc, end_d_cnt_proc)
            if (w.fsm_nxt != pipe_ctrl_fsm_)
            {
                start_d_cnt_ = 0;
                end_d_cnt_ = 0;
                ln_s_acc_ = 0;
                ln_v_acc_ = 0;
                b_part_delay_.fill(0);
                scratchpad_.reset_addr_delays();
            }
            else
            {
                if (pipe_ctrl_fsm_ == SOFTMAX_ST1 && (in.i_psum_valid || w.ctrl_vbank0_rd))
                    start_d_cnt_++;
                else if (pipe_ctrl_fsm_ == SOFTMAX_ST2 && w.ctrl_vbank0_rd)
                    start_d_cnt_++;
                else if (pipe_ctrl_fsm_ == SOFTMAX_ST3 && w.ctrl_vbank1_rd)
                    start_d_cnt_++;
                else if (pipe_ctrl_fsm_ == LAYERNORM_ST1 && w.ctrl_vbank0_rd)
                    start_d_cnt_++;
                else if (pipe_ctrl_fsm_ == LAYERNORM_ST2 && w.ctrl_vbank0_rd)
                    start_d_cnt_++;
                else if (pipe_ctrl_fsm_ == LAYERNORM_ST3 && w.ctrl_vbank0_rd)
                    start_d_cnt_++;
                else if (pipe_ctrl_fsm_ == LAYERNORM_ST4 && (w.ctrl_vbank1_rd || w.ctrl_vbank2_rd))
                    start_d_cnt_++;
                else if (pipe_ctrl_fsm_ == LAYERNORM_ST5 && w.ctrl_vbank0_rd)
                    start_d_cnt_++;
                else if (pipe_ctrl_fsm_ == GEMM_FUSED_ST1 && (in.i_psum_valid || w.ctrl_vbank2_rd))
                    start_d_cnt_++;
                else if (pipe_ctrl_fsm_ == RESIDUAL_ST1 && w.ctrl_vbank0_rd)
                    start_d_cnt_++;
                else if (pipe_ctrl_fsm_ == RESIDUAL_ST2 && (start_d_cnt_ == 0 || w.ctrl_vbank2_rd))
                    start_d_cnt_++;

                if (pipe_ctrl_fsm_ == SOFTMAX_ST2 && w.vrf_out.vint32[2].o_valid)
                    end_d_cnt_++;
                else if (pipe_ctrl_fsm_ == SOFTMAX_ST3 && w.vrf_out.vint16[5].o_valid)
                    end_d_cnt_++;
                else if (pipe_ctrl_fsm_ == LAYERNORM_ST1 && vbank0_rd_delay_[1])
                    end_d_cnt_++;
                else if (pipe_ctrl_fsm_ == LAYERNORM_ST2 && vbank0_rd_delay_[1])
                    end_d_cnt_++;
                else if (pipe_ctrl_fsm_ == LAYERNORM_ST3 && vbank0_rd_delay_[1])
                    end_d_cnt_++;
                else if (pipe_ctrl_fsm_ == LAYERNORM_ST4 && (w.vrf_out.vint8[0].o_valid || vbank2_rd_delay_[1]))
                    end_d_cnt_++;
                else if (pipe_ctrl_fsm_ == LAYERNORM_ST5 && vbank0_rd_delay_[1])
                    end_d_cnt_++;
                else if (pipe_ctrl_fsm_ == GEMM_FUSED_ST1 && (in.i_psum_valid || vbank2_valid_beat))
                    end_d_cnt_++;
                else if (pipe_ctrl_fsm_ == RESIDUAL_ST1 && vbank0_valid_beat)
                    end_d_cnt_++;
                else if (pipe_ctrl_fsm_ == RESIDUAL_ST2 && vbank2_valid_beat)
                    end_d_cnt_++;
            }

            // LayerNorm internal accumulator and register updates on clock edge
            if (pipe_ctrl_fsm_ == LAYERNORM_ST1 && vbank0_rd_delay_[1])
            {
                for (int l = 0; l < GVU_LANES; l++)
                {
                    const int8_t val = (l < static_cast<int>(w.sp_out.o_vbank0_rdata.size()))
                        ? static_cast<int8_t>(w.sp_out.o_vbank0_rdata[l]) : 0;
                    ln_s_acc_ += val;
                }
                if (w.layernorm_token_last)
                    ln_final_s_ = ln_s_acc_;
            }
            else if (pipe_ctrl_fsm_ == LAYERNORM_ST2 && vbank0_rd_delay_[1])
            {
                for (int l = 0; l < GVU_LANES; l++)
                {
                    const int32_t D_val = static_cast<int32_t>(w.adder2_r[l]);
                    ln_v_acc_ += static_cast<int64_t>(D_val) * D_val;
                }
                if (w.layernorm_token_last)
                {
                    const int64_t v_pre = ln_v_acc_ >> in.i_ln_pre_shift;
                    ln_final_var_ = ln_requant_clamp(v_pre, in.i_ln_m0_var, in.i_ln_ts_var, in.i_ln_z_var, -128, 127);
                }
            }
            else if (pipe_ctrl_fsm_ == LAYERNORM_ST3 && (vbank0_rd_delay_[1] || start_d_cnt_ == 1))
            {
                const int32_t v = ln_final_var_;
                const i128 u = ln_rshift_wide(static_cast<i128>(v - in.i_ln_z_var) * in.i_ln_m0_7 + in.i_ln_e_bias, in.i_ln_ts_7);
                const int32_t f = static_cast<int32_t>(clamp_lane(static_cast<uint32_t>(u + in.i_ln_z_7), static_cast<uint32_t>(-128), 127, 32, true));
                ln_final_w_ = (f > in.i_ln_z_7) ? static_cast<uint32_t>(f - in.i_ln_z_7) : static_cast<uint32_t>(in.i_ln_r8_floor_z_out ? in.i_ln_r8_z_out : 1);
            }

            if ((pipe_ctrl_fsm_ == SOFTMAX_ST3 && w.softmax_token_last) ||
                (pipe_ctrl_fsm_ == GEMM_FUSED_ST1 && w.gemm_fused_token_last) ||
                (pipe_ctrl_fsm_ == RESIDUAL_ST2 && w.residual_st2_done))
            {
                const uint32_t max_tiles = (in.i_num_tile > 0 ? in.i_num_tile : 1);
                tile_cnt_ = (tile_cnt_ == max_tiles - 1) ? 0 : (tile_cnt_ + 1);
            }

            if (in.i_pipeline_en)
                pipe_ctrl_fsm_ = w.fsm_nxt;
            is_lut_exp_init_ = w.is_lut_exp_init_nxt;
            is_lut_recip_init_ = w.is_lut_recip_init_nxt;
            is_lut_rsqrt_init_ = w.is_lut_rsqrt_init_nxt;
            is_lut_act_init_ = w.is_lut_act_init_nxt;
            return o;
        }

    private:
        // Stream mode of GEMM_FUSED: stage counts of the MAS cuts (see In::i_gemm_stream)
        static constexpr int kStreamMul = 5;                  // multiplier 32x32 4 + v32int64 register
        static constexpr int kStreamRs = 2;                   // round-shift 1 + v32int16 register
        static_assert(kStreamMul + kStreamRs + 2 == GEMM_STREAM_DEPTH_MAS, "stream stages of the MAS");

        GvuTopParams P_;
        int mul_n_;                                           // multiplier chain in use: kStreamMul + delay stages
        GvuScratchpad scratchpad_;
        GvuVlsuNdPort vlsu_;   // the frozen GvuVlsu, plus AGU_ND when bound
        StreamVrf stream_vrf_;
        LutUnit vlut_;
        Vcmp32 vcmp_;

        // Controller FSM state & flags
        control_fsm_t pipe_ctrl_fsm_{IDLE};
        bool is_lut_exp_init_{false};
        bool is_lut_recip_init_{false};
        bool is_lut_rsqrt_init_{false};
        bool is_lut_act_init_{false};
        uint32_t start_d_cnt_{0};
        uint32_t end_d_cnt_{0};
        uint32_t tile_cnt_{0};
        bool softmax_st1_pipe_en_r_{false};

        // Stream mode of GEMM_FUSED: pipeline registers (valid + data), see In::i_gemm_stream
        struct StreamProd { V64 p{}; unsigned shift{0}; uint32_t addr{0}, mask{0}; bool last{false}; };
        struct StreamBeat { V32 v{}; uint32_t addr{0}, mask{0}; bool last{false}; };
        // multiplier chain of mul_n_ registers; the ones past kStreamMul are the pure delay stages. On the heap: the unit is
        // built on the stack of SystemC threads, whose stack is small.
        std::vector<uint8_t> sm_v_;
        std::vector<StreamProd> sm_d_;
        std::array<bool, kStreamRs> sr_v_{};
        std::array<StreamBeat, kStreamRs> sr_d_{};
        bool sz_v_{false};                                    // add zero point + v32int16 register
        StreamBeat sz_d_{};
        bool s8_v_{false};                                    // clamp + v32int8 register
        StreamBeat s8_d_{};
        // The beat marked by the flags of the PSM stream leaves the pipeline in this cycle (see In::i_gemm_exit_flags).
        bool stream_exit(const In &in) const { return in.i_gemm_exit_flags && s8_v_ && s8_d_.last; }
        bool stream_busy() const
        {
            for (uint8_t b : sm_v_) if (b) return true;
            for (bool b : sr_v_) if (b) return true;
            return sz_v_ || s8_v_;
        }
        void stream_clear() { std::fill(sm_v_.begin(), sm_v_.end(), uint8_t(0)); sr_v_.fill(false); sz_v_ = false; s8_v_ = false; }
        // Residual add: the write of this cycle belongs to the last row of the run and i_res_last_lane_mask is in use.
        bool res_last_row(const In &in) const
        {
            const uint32_t nt = in.i_num_tile > 0 ? in.i_num_tile : 1;
            return in.i_res_last_lane_mask != 0xFFFFFFFFu && in.i_token_len > 0 && tile_cnt_ == nt - 1 && end_d_cnt_ + 1 == in.i_token_len;
        }
        // One clock edge of the stream pipeline; every stage works on the values present in this cycle.
        void stream_tick(const In &in)
        {
            StreamProd po{};
            if (in.i_psum_valid)
            {
                for (int l = 0; l < GVU_LANES; l++)
                    po.p[l] = vmul32x32_lane(MA_32X32, static_cast<uint32_t>(in.i_psum_vec[l]), static_cast<uint32_t>(in.i_psum_scale));
                po.shift = in.i_psum_shift & 0x3Fu; po.addr = in.i_psum_addr; po.mask = in.i_psum_mask;
                po.last = in.i_gemm_exit_flags && in.i_oc_last && in.i_oc_tile_last;
            }
            StreamBeat rs{};
            if (sm_v_[mul_n_ - 1])
            {
                const StreamProd &q = sm_d_[mul_n_ - 1];
                for (int l = 0; l < GVU_LANES; l++)
                    rs.v[l] = vroundshift64_lane(q.p[l], q.shift, in.i_gemm_round, in.i_gemm_narrow, NW_16).out_data & 0xFFFFu;
                rs.addr = q.addr; rs.mask = q.mask; rs.last = q.last;
            }
            StreamBeat zs{};
            if (sr_v_[kStreamRs - 1])
            {
                const StreamBeat &q = sr_d_[kStreamRs - 1];
                const uint32_t zp16 = static_cast<uint32_t>(in.i_gemm_skip_zp ? 0 : in.i_gemm_zp16) & 0xFFFFu;
                for (int l = 0; l < GVU_LANES; l++) zs.v[l] = adder32_lane(ADD_16, 0, 0, false, q.v[l], zp16) & 0xFFFFu;
                zs.addr = q.addr; zs.mask = q.mask; zs.last = q.last;
            }
            StreamBeat c8{};
            if (sz_v_)
            {
                for (int l = 0; l < GVU_LANES; l++) c8.v[l] = clamp_lane(sz_d_.v[l], static_cast<uint32_t>(-128), 127, 16, true) & 0xFFu;
                c8.addr = sz_d_.addr; c8.mask = sz_d_.mask; c8.last = sz_d_.last;
            }
            s8_v_ = sz_v_; s8_d_ = c8;
            sz_v_ = sr_v_[kStreamRs - 1]; sz_d_ = zs;
            for (int s = kStreamRs - 1; s > 0; s--) { sr_v_[s] = sr_v_[s - 1]; sr_d_[s] = sr_d_[s - 1]; }
            sr_v_[0] = sm_v_[mul_n_ - 1]; sr_d_[0] = rs;
            for (int s = mul_n_ - 1; s > 0; s--) { sm_v_[s] = sm_v_[s - 1]; sm_d_[s] = sm_d_[s - 1]; }
            sm_v_[0] = in.i_psum_valid; sm_d_[0] = po;
        }

        // LayerNorm internal pipeline accumulators and registers
        int64_t ln_s_acc_{0};
        int64_t ln_v_acc_{0};
        int64_t ln_final_s_{0};
        int32_t ln_final_var_{0};
        uint32_t ln_final_w_{1};

        // Shift buffers
        std::array<std::array<uint32_t, GVU_LANES>, 4> exp_e_shift_buf_{};
        std::array<bool, 2> vbank0_rd_delay_{false, false};
        std::array<bool, 2> vbank1_rd_delay_{false, false};
        std::array<bool, 2> vbank2_rd_delay_{false, false};
        std::array<bool, 2> sbank_rd_delay_{false, false};
        std::array<bool, 2> st1_last_rd_delay_{false, false};
        std::array<unsigned, 2> b_part_delay_{0, 0};

        // Temporary evaluation signals connecting the submodules
        struct Wires
        {
            // Controller FSM signals
            control_fsm_t fsm_nxt{IDLE};
            std::array<uint8_t, 5> agu_nd_sel{{0, 0, 0, 0, 0}};   // AGU_ND pick per VLSU port for the clock edge
            bool is_lut_exp_init_nxt{false}, is_lut_recip_init_nxt{false}, is_lut_rsqrt_init_nxt{false}, is_lut_act_init_nxt{false};
            bool softmax_token_last{false};
            bool softmax_st1_last_rd{false};
            bool softmax_st1_pipe_en{false}, softmax_st2_pipe_en{false}, softmax_st3_pipe_en{false};
            bool softmax_st1_cmp_first{false}, softmax_st1_cmp_last{false};
            bool layernorm_token_last{false};
            bool gemm_fused_token_last{false};
            bool residual_st1_done{false};
            bool residual_st2_done{false};

            // VLSU control outputs from pipe_ctrl
            bool ctrl_sbank_rd_0{false}, ctrl_sbank_wr_0{false};
            bool ctrl_sbank_rd_1{false}, ctrl_sbank_wr_1{false};
            bool ctrl_vbank0_rd{false}, ctrl_vbank0_wr{false};
            unsigned ctrl_vbank1_port_sel{0}; bool ctrl_vbank1_rd{false}, ctrl_vbank1_wr{false};
            bool ctrl_vbank2_port_sel{false}; bool ctrl_vbank2_rd{false}, ctrl_vbank2_wr{false};

            // LUT init signals
            bool pipe_ctrl_lut_init_wr{false};
            LutType pipe_ctrl_lut_init_sel{LUT_RECIP};
            SpBytes pipe_ctrl_lut_init_data{};

            // LUT runtime lookup signals
            LutType lut_rsel{LUT_ACT};
            LutIdxMode lut_idx_mode{LUT_PWL};
            bool lut_compens{false};
            unsigned lut_base{7}, lut_W{16}, lut_lerp_shift{8};
            bool lut_req_vld{false}, lut_rsp_rdy{false};
            std::array<uint8_t, GVU_LANES> lut_8b_idx{};
            V32 lut_32b_idx{};
            LutUnit::In lut_in{};
            LutUnit::Out lut_out{};

            // Submodule I/O structures
            GvuVlsu::In vlsu_in{};
            GvuVlsu::Out vlsu_out{};
            GvuScratchpad::In sp_in{};
            GvuScratchpad::Out sp_out{};
            StreamVrf::In vrf_in{};
            StreamVrf::Out vrf_out{};
            Vcmp32::In cmp_in{};
            Vcmp32::Out cmp_out{};

            // Adders & Multipliers & Rounding outputs
            V32 adder1_r{}, adder2_r{};
            V64 mul1_prod{};
            std::array<MulbOut, GVU_LANES> mul0_out{};
            V32 roundshift0_out{}, roundshift1_out{};
            V32 clamp_out{};
        };

        Out comb(const In &in, Wires &w) const
        {
            // ---- 1. Basic flags and controller decodes -----------------------------------------------------
            w.fsm_nxt = pipe_ctrl_fsm_;
            w.is_lut_exp_init_nxt = is_lut_exp_init_;
            w.is_lut_recip_init_nxt = is_lut_recip_init_;
            w.is_lut_rsqrt_init_nxt = is_lut_rsqrt_init_;
            w.is_lut_act_init_nxt = is_lut_act_init_;

            const bool token_read_done = (in.i_token_len > 0) && (start_d_cnt_ == in.i_token_len);
            const bool softmax_token_read_done = token_read_done;
            w.softmax_token_last = (in.i_token_len > 0) && (end_d_cnt_ == in.i_token_len - 1);
            w.softmax_st1_last_rd = (in.i_token_len > 0) && (start_d_cnt_ == in.i_token_len - 1) && (pipe_ctrl_fsm_ == SOFTMAX_ST1);
            w.layernorm_token_last = (in.i_token_len > 0) && (end_d_cnt_ >= in.i_token_len - 1);
            w.gemm_fused_token_last = (in.i_token_len > 0) && (end_d_cnt_ >= in.i_token_len - 1) && (pipe_ctrl_fsm_ == GEMM_FUSED_ST1) && !in.i_gemm_stream && (in.i_psum_valid || vbank2_rd_delay_[1]);
            w.residual_st1_done = (in.i_token_len > 0) && (end_d_cnt_ >= in.i_token_len - 1) && (pipe_ctrl_fsm_ == RESIDUAL_ST1) && vbank0_rd_delay_[1];
            w.residual_st2_done = (in.i_token_len > 0) && (end_d_cnt_ >= in.i_token_len - 1) && (pipe_ctrl_fsm_ == RESIDUAL_ST2) && vbank2_rd_delay_[1];

            // ---- 2. Controller Output Decode (gvu_pipe_controller.sv) ---------------------------------------
            const bool lut_init_phase = (pipe_ctrl_fsm_ == LUT_RSQRT_INIT || pipe_ctrl_fsm_ == LUT_RECIP_INIT ||
                                         pipe_ctrl_fsm_ == LUT_EXP_INIT || pipe_ctrl_fsm_ == LUT_ACT_INIT);
            w.pipe_ctrl_lut_init_wr = vbank1_rd_delay_[1] && lut_init_phase;
            w.pipe_ctrl_lut_init_sel = pipe_ctrl_fsm_ == LUT_RSQRT_INIT ? LUT_RSQRT
                                     : (pipe_ctrl_fsm_ == LUT_RECIP_INIT ? LUT_RECIP
                                     : (pipe_ctrl_fsm_ == LUT_EXP_INIT ? LUT_EXP : LUT_ACT));

            w.lut_rsel = pipe_ctrl_fsm_ == SOFTMAX_ST2 ? LUT_EXP
                       : (pipe_ctrl_fsm_ == SOFTMAX_ST3 ? LUT_RECIP
                       : (pipe_ctrl_fsm_ == LAYERNORM_ST4 ? LUT_RSQRT : LUT_ACT));
            w.lut_idx_mode = LUT_PWL;
            w.lut_compens = (w.lut_rsel == LUT_ACT);
            w.lut_base = 7;
            w.lut_W = 16;
            w.lut_lerp_shift = 8;

            // VLSU control requests from controller FSM
            if (pipe_ctrl_fsm_ == SOFTMAX_ST1 || (pipe_ctrl_fsm_ == SOFTMAX_ST2 && !softmax_token_read_done))
                w.ctrl_vbank1_port_sel = 2; // port 2 (v32int8)
            else if (pipe_ctrl_fsm_ == LAYERNORM_ST3 || pipe_ctrl_fsm_ == LAYERNORM_ST4)
                w.ctrl_vbank1_port_sel = 1; // port 1 (v32int16)
            else if (pipe_ctrl_fsm_ == LAYERNORM_ST5 || pipe_ctrl_fsm_ == RESIDUAL_ST1)
                w.ctrl_vbank1_port_sel = 2; // port 2 (v32int8)
            else
                w.ctrl_vbank1_port_sel = 0; // port 0 (v32int32)

            w.ctrl_vbank1_rd = (pipe_ctrl_fsm_ == LUT_RSQRT_INIT || pipe_ctrl_fsm_ == LUT_RECIP_INIT || pipe_ctrl_fsm_ == LUT_EXP_INIT ||
                                pipe_ctrl_fsm_ == LUT_ACT_INIT ||
                                (pipe_ctrl_fsm_ == SOFTMAX_ST2 && !softmax_token_read_done) ||
                                (pipe_ctrl_fsm_ == SOFTMAX_ST3 && !softmax_token_read_done) ||
                                (pipe_ctrl_fsm_ == LAYERNORM_ST2 && !token_read_done) ||
                                (pipe_ctrl_fsm_ == LAYERNORM_ST4 && !token_read_done) ||
                                (pipe_ctrl_fsm_ == RESIDUAL_ST1 && !token_read_done));
            w.ctrl_vbank1_wr = false;

            w.ctrl_vbank0_rd = ((pipe_ctrl_fsm_ == SOFTMAX_ST1 && !softmax_token_read_done) ||
                                (pipe_ctrl_fsm_ == SOFTMAX_ST2 && !softmax_token_read_done) ||
                                (pipe_ctrl_fsm_ == LAYERNORM_ST1 && !token_read_done) ||
                                (pipe_ctrl_fsm_ == LAYERNORM_ST2 && !token_read_done) ||
                                (pipe_ctrl_fsm_ == LAYERNORM_ST3 && (start_d_cnt_ == 0)) ||
                                (pipe_ctrl_fsm_ == LAYERNORM_ST5 && !token_read_done) ||
                                (pipe_ctrl_fsm_ == RESIDUAL_ST1 && !token_read_done));
            w.ctrl_vbank0_wr = false;

            w.ctrl_vbank2_port_sel = (pipe_ctrl_fsm_ == SOFTMAX_ST2 || pipe_ctrl_fsm_ == SOFTMAX_ST3 || pipe_ctrl_fsm_ == POOLING_MAX); // true = port 1 (v32int8), false = port 0 (v32int32)
            w.ctrl_vbank2_rd = ((pipe_ctrl_fsm_ == SOFTMAX_ST3 && vlut_.o_pipe_almost_done(LUT_PWL)) ||
                                (pipe_ctrl_fsm_ == LAYERNORM_ST4 && !token_read_done) ||
                                (pipe_ctrl_fsm_ == GEMM_FUSED_ST1 && !in.i_gemm_stream && !in.i_psum_valid && !token_read_done) ||
                                (pipe_ctrl_fsm_ == RESIDUAL_ST2 && start_d_cnt_ >= 1 && start_d_cnt_ <= in.i_token_len));
            w.ctrl_vbank2_wr = false;

            w.ctrl_sbank_rd_0 = ((pipe_ctrl_fsm_ == LAYERNORM_ST5 || (pipe_ctrl_fsm_ == GEMM_FUSED_ST1 && !in.i_gemm_stream)) && !token_read_done);
            w.ctrl_sbank_rd_1 = ((pipe_ctrl_fsm_ == LAYERNORM_ST5 || (pipe_ctrl_fsm_ == GEMM_FUSED_ST1 && !in.i_gemm_stream)) && !token_read_done);

            w.softmax_st1_pipe_en = (pipe_ctrl_fsm_ == SOFTMAX_ST1) && (in.i_psum_valid || vbank0_rd_delay_[1]);
            w.softmax_st2_pipe_en = (pipe_ctrl_fsm_ == SOFTMAX_ST2) && vbank0_rd_delay_[1];
            w.softmax_st3_pipe_en = (pipe_ctrl_fsm_ == SOFTMAX_ST3) && vbank1_rd_delay_[1];
            w.softmax_st1_cmp_first = w.softmax_st1_pipe_en && !softmax_st1_pipe_en_r_;
            w.softmax_st1_cmp_last = st1_last_rd_delay_[1];

            // ---- 3. VLSU and Scratchpad Routing -------------------------------------------------------------
            w.vlsu_in.i_sbank_rd_0 = w.ctrl_sbank_rd_0;
            w.vlsu_in.i_sbank_wr_0 = false;
            w.vlsu_in.i_sbank_start_addr_0 = 0;
            w.vlsu_in.i_sbank_wmask_0 = 0xF;

            w.vlsu_in.i_sbank_rd_1 = w.ctrl_sbank_rd_1;
            w.vlsu_in.i_sbank_wr_1 = false;
            w.vlsu_in.i_sbank_start_addr_1 = 0;
            w.vlsu_in.i_sbank_wmask_1 = true;

            const bool is_res = (pipe_ctrl_fsm_ == RESIDUAL_ST1 || pipe_ctrl_fsm_ == RESIDUAL_ST2 ||
                                 (pipe_ctrl_fsm_ == IDLE && in.i_pipeline_en && (in.i_gvu_op == GVU_OP_RESIDUAL || in.i_gvu_op == GVU_OP_ELEMWISE)));
            const unsigned tile_offset = tile_cnt_ * in.i_token_len;
            w.vlsu_in.i_agu_addr_reset = (w.fsm_nxt != pipe_ctrl_fsm_);
            w.vlsu_in.i_vbank0_rd = w.ctrl_vbank0_rd;
            w.vlsu_in.i_vbank0_wr = false;
            w.vlsu_in.i_vbank0_start_addr = (in.i_gvu_op == GVU_OP_SOFTMAX || in.i_gvu_op == GVU_OP_GEMM_FUSED)
                ? tile_offset
                : (is_res ? (in.i_res_a_row + tile_offset) : 0u);
            w.vlsu_in.i_vbank0_lane_wmask = 0xFFFFFFFFu;

            w.vlsu_in.i_vbank1_port_sel = w.ctrl_vbank1_port_sel;
            w.vlsu_in.i_vbank1_rd = w.ctrl_vbank1_rd;
            w.vlsu_in.i_vbank1_wr = false;
            w.vlsu_in.i_vbank1_agu_mode = (pipe_ctrl_fsm_ == SOFTMAX_ST2 || pipe_ctrl_fsm_ == SOFTMAX_ST3)
                ? AGU_FIXED : AGU_INCR;
            // one start address per bank, address mode AGU_INCR (the default of GvuVlsu::In), as the controller drives it
            w.vlsu_in.i_vbank1_start_addr = (pipe_ctrl_fsm_ == LUT_EXP_INIT)
                ? static_cast<unsigned>(16 * 256 / P_.LUT_INIT_DW)
                : (is_res ? (in.i_res_b_row + tile_offset) : 0u);
            w.vlsu_in.i_vbank1_lane_wmask = 0xFFFFFFFFu;

            w.vlsu_in.i_vbank2_port_sel = w.ctrl_vbank2_port_sel;
            w.vlsu_in.i_vbank2_rd = w.ctrl_vbank2_rd;
            w.vlsu_in.i_vbank2_wr = false;
            w.vlsu_in.i_vbank2_start_addr = (in.i_gvu_op == GVU_OP_GEMM_FUSED)
                ? tile_offset
                : (is_res ? (in.i_res_sum_row + tile_offset) : 0u);
            w.vlsu_in.i_vbank2_lane_wmask = 0xFFFFFFFFu;

            // Pooling (latest hardware RTL drop): the configuration inputs go to the VLSU as they are, the controller enables the feeder
            // in POOLING_MAX and gives vbank0 / vbank2 the pooling AGU. The read data and the comparator's lane valid are
            // filled in below once the Scratchpad and the comparator are evaluated.
            const bool pool = pipe_ctrl_fsm_ == POOLING_MAX;
            w.vlsu_in.i_pooling_enable = pool;
            w.vlsu_in.i_pooling_mode = in.i_pooling_mode;
            w.vlsu_in.i_pooling_stride = in.i_pooling_stride;
            w.vlsu_in.i_pooling_num_w_tile = in.i_pooling_num_w_tile;
            w.vlsu_in.i_pooling_c = in.i_pooling_c;
            w.vlsu_in.i_pooling_oh = in.i_pooling_oh;
            w.vlsu_in.i_pooling_c_move = in.i_pooling_c_move;
            w.vlsu_in.i_pooling_h_move = in.i_pooling_h_move;
            if (pool)
            {
                w.vlsu_in.i_vbank0_agu_mode = AGU_POOLING;
                w.vlsu_in.i_vbank2_agu_mode = AGU_POOLING;
            }

            w.vlsu_out = vlsu_.eval(w.vlsu_in);

            // Connect Scratchpad memory inputs
            w.sp_in.i_sbank_buf_sel = in.i_sbank_buf_sel;
            w.sp_in.i_vbank0_buf_sel = in.i_vbank0_buf_sel;
            w.sp_in.i_vbank1_buf_sel = in.i_vbank1_buf_sel;
            w.sp_in.i_vbank2_buf_sel = in.i_vbank2_buf_sel;

            w.sp_in.i_sbank_en_0 = w.vlsu_out.o_sbank_en_0;
            w.sp_in.i_sbank_wen_0 = w.vlsu_out.o_sbank_wen_0;
            w.sp_in.i_sbank_addr_0 = w.vlsu_out.o_sbank_addr_0;
            w.sp_in.i_sbank_wmask_0 = w.vlsu_out.o_sbank_wmask_0;
            w.sp_in.i_sbank_en_1 = w.vlsu_out.o_sbank_en_1;
            w.sp_in.i_sbank_wen_1 = w.vlsu_out.o_sbank_wen_1;
            w.sp_in.i_sbank_addr_1 = w.vlsu_out.o_sbank_addr_1;
            w.sp_in.i_sbank_wmask_1 = w.vlsu_out.o_sbank_wmask_1;

            w.sp_in.i_vbank0_en = w.vlsu_out.o_vbank0_en;
            w.sp_in.i_vbank0_wen = w.vlsu_out.o_vbank0_wen;
            w.sp_in.i_vbank0_addr = w.vlsu_out.o_vbank0_addr;
            w.sp_in.i_vbank0_wmask = SpBytes(P_.NUM_LANE / 8, 0xFF);

            w.sp_in.i_vbank1_port_sel = w.vlsu_out.o_vbank1_port_sel;
            w.sp_in.i_vbank1_en_0 = w.vlsu_out.o_vbank1_en_0;
            w.sp_in.i_vbank1_wen_0 = w.vlsu_out.o_vbank1_wen_0;
            w.sp_in.i_vbank1_addr_0 = w.vlsu_out.o_vbank1_addr_0;
            w.sp_in.i_vbank1_wmask_0 = w.vlsu_out.o_vbank1_wmask_0;
            w.sp_in.i_vbank1_en_1 = w.vlsu_out.o_vbank1_en_1;
            w.sp_in.i_vbank1_wen_1 = w.vlsu_out.o_vbank1_wen_1;
            w.sp_in.i_vbank1_addr_1 = w.vlsu_out.o_vbank1_addr_1;
            w.sp_in.i_vbank1_wmask_1 = w.vlsu_out.o_vbank1_wmask_1;
            w.sp_in.i_vbank1_en_2 = w.vlsu_out.o_vbank1_en_2;
            w.sp_in.i_vbank1_wen_2 = w.vlsu_out.o_vbank1_wen_2;
            w.sp_in.i_vbank1_addr_2 = w.vlsu_out.o_vbank1_addr_2;
            w.sp_in.i_vbank1_wmask_2 = SpBytes(P_.NUM_LANE / 8, 0xFF);

            w.sp_in.i_vbank2_port_sel = w.vlsu_out.o_vbank2_port_sel;
            w.sp_in.i_vbank2_en_0 = w.vlsu_out.o_vbank2_en_0;
            w.sp_in.i_vbank2_wen_0 = w.vlsu_out.o_vbank2_wen_0;
            w.sp_in.i_vbank2_addr_0 = w.vlsu_out.o_vbank2_addr_0;
            w.sp_in.i_vbank2_wmask_0 = w.vlsu_out.o_vbank2_wmask_0;
            w.sp_in.i_vbank2_en_1 = w.vlsu_out.o_vbank2_en_1;
            w.sp_in.i_vbank2_wen_1 = w.vlsu_out.o_vbank2_wen_1;
            w.sp_in.i_vbank2_addr_1 = w.vlsu_out.o_vbank2_addr_1;
            w.sp_in.i_vbank2_wmask_1 = SpBytes(P_.NUM_LANE / 8, 0xFF);

            w.sp_in.i_vbank0_wdata.assign(P_.NUM_LANE, 0);
            w.sp_in.i_vbank1_wdata_0.assign(4 * P_.NUM_LANE, 0);
            w.sp_in.i_vbank1_wdata_1.assign(2 * P_.NUM_LANE, 0);
            w.sp_in.i_vbank1_wdata_2.assign(P_.NUM_LANE, 0);
            w.sp_in.i_vbank2_wdata_0.assign(4 * P_.NUM_LANE, 0);
            w.sp_in.i_vbank2_wdata_1.assign(P_.NUM_LANE, 0);

            // Scratchpad outputs
            w.sp_out = scratchpad_.out(w.sp_in);
            w.pipe_ctrl_lut_init_data = w.sp_out.o_vbank1_rdata_0;

            // ---- 4. Comparator Execution (SOFTMAX_ST1, POOLING_MAX) ----------------------------------------
            w.cmp_in.in_cmp_mode = (pipe_ctrl_fsm_ == SOFTMAX_ST1 || pool) ? CMP_ACCUMULATE : CMP_ELEMENTWISE;
            w.cmp_in.in_op = CMP_MAX;
            w.cmp_in.in_elem_w = (pipe_ctrl_fsm_ == SOFTMAX_ST1 || pool) ? EW_8 : EW_32;
            w.cmp_in.in_lane_mode = pool;
            w.cmp_in.in_signed = true;
            w.cmp_in.in_src1_bcast = false;
            w.cmp_in.in_first = w.softmax_st1_cmp_first;
            w.cmp_in.in_last = w.softmax_st1_cmp_last;
            w.cmp_in.in_valid = w.softmax_st1_pipe_en;
            w.cmp_in.out_ready = true;
            for (int l = 0; l < GVU_LANES; l++)
            {
                w.cmp_in.in_src0[l] = (l < static_cast<int>(w.sp_out.o_vbank0_rdata.size()))
                    ? static_cast<uint32_t>(w.sp_out.o_vbank0_rdata[l]) : 0u;
                w.cmp_in.in_src1[l] = 0;
            }
            // MAXPOOL (gvu_top.sv vcmp_inp_arb, keyed on the opcode, not the state): the comparator takes the feeder's lane
            // stream (byte in bits 7:0 of each lane), every lane ready at the output.
            if (in.i_gvu_op == GVU_OP_MAXPOOL)
            {
                w.vlsu_in.i_pooling_rdata = w.sp_out.o_vbank0_rdata;
                w.vlsu_out = vlsu_.eval(w.vlsu_in);
                w.cmp_in.in_lane_valid = w.vlsu_out.o_pooling_window_valid;
                w.cmp_in.out_lane_ready = 0xFFFFFFFFu;
                w.cmp_in.in_lane_first = w.vlsu_out.o_feeder_first;
                w.cmp_in.in_lane_last = w.vlsu_out.o_feeder_last;
                for (int l = 0; l < GVU_LANES; l++) w.cmp_in.in_src0[l] = w.vlsu_out.o_pooling_window_data[static_cast<size_t>(l)];
            }
            w.cmp_out = vcmp_.eval(w.cmp_in);
            // MAXPOOL write: lane valid of the comparator -> vbank2 port 1 at the pooling address, its bytes as data
            if (in.i_gvu_op == GVU_OP_MAXPOOL)
            {
                w.vlsu_in.i_pooling_valid = w.cmp_out.out_lane_valid;
                w.vlsu_out = vlsu_.eval(w.vlsu_in);
                w.sp_in.i_vbank0_en = w.vlsu_out.o_vbank0_en;
                w.sp_in.i_vbank0_wen = w.vlsu_out.o_vbank0_wen;
                w.sp_in.i_vbank0_addr = w.vlsu_out.o_vbank0_addr;
                w.sp_in.i_vbank2_port_sel = w.vlsu_out.o_vbank2_port_sel;
                w.sp_in.i_vbank2_en_1 = w.vlsu_out.o_vbank2_en_1;
                w.sp_in.i_vbank2_wen_1 = w.vlsu_out.o_vbank2_wen_1;
                w.sp_in.i_vbank2_addr_1 = (in.i_agu_nd_ctrl.dst_on && in.i_agu_nd_ctrl.dst_port == AguNdCtrl::VBANK2)
                    ? in.i_agu_dst.addr_at(w.vlsu_out.o_vbank2_addr_1, P_.sp_params.VBANK2_AW() + 2) : w.vlsu_out.o_vbank2_addr_1;   // AGU_ND destination
                for (int b = 0; b < P_.NUM_LANE / 8; b++) w.sp_in.i_vbank2_wmask_1[static_cast<size_t>(b)] = static_cast<uint8_t>(w.vlsu_out.o_vbank2_wmask_1 >> (8 * b));
                for (int l = 0; l < GVU_LANES; l++) w.sp_in.i_vbank2_wdata_1[static_cast<size_t>(l)] = static_cast<uint8_t>(w.cmp_out.out_res[l] & 0xFFu);
            }

            if (pipe_ctrl_fsm_ == SOFTMAX_ST1 && w.cmp_out.out_valid)
            {
                w.ctrl_vbank1_wr = true;
                for (int l = 0; l < GVU_LANES; l++)
                    w.sp_in.i_vbank1_wdata_2[l] = static_cast<uint8_t>(w.cmp_out.out_res[l] & 0xFFu);
                w.vlsu_in.i_vbank1_wr = true;
                w.vlsu_out = vlsu_.eval(w.vlsu_in);
                w.sp_in.i_vbank1_en_2 = w.vlsu_out.o_vbank1_en_2;
                w.sp_in.i_vbank1_wen_2 = w.vlsu_out.o_vbank1_wen_2;
            }

            // ---- 5. Stream VRF Initial Evaluation (registered outputs from previous clock edge) ----------
            w.vrf_in.i_vint32_0_cfg_mode = (pipe_ctrl_fsm_ == SOFTMAX_ST1 || pipe_ctrl_fsm_ == SOFTMAX_ST2 || pool) ? 2
                                         : ((pipe_ctrl_fsm_ == SOFTMAX_ST3) ? 1 : 0);
            w.vrf_in.i_vint64_0_cfg_mode = (pipe_ctrl_fsm_ == SOFTMAX_ST3);
            w.vrf_in.i_vint64_2_cfg_mode = false;
            w.vrf_out = stream_vrf_.eval(w.vrf_in);

            // ---- 6. Functional Units Data Paths (SOFTMAX & LAYERNORM & GEMM_FUSED) ------------------------
            if (pipe_ctrl_fsm_ == GEMM_FUSED_ST1 && !in.i_gemm_stream)
            {
                // per-channel scale / shift from sbank only when i_gemm_per_channel (a channel may have shift 0)
                const int32_t scale = in.i_gemm_per_channel ? static_cast<int32_t>(w.sp_out.o_sbank_rdata_0) : in.i_gemm_scale;
                const uint8_t shift = in.i_gemm_per_channel ? static_cast<uint8_t>(w.sp_out.o_sbank_rdata_1) : static_cast<uint8_t>(in.i_gemm_shift);
                const int8_t zp = in.i_gemm_skip_zp ? 0 : in.i_gemm_zp;
                const uint32_t zp16 = sext8to16(static_cast<uint32_t>(static_cast<uint8_t>(zp)));
                const int32_t lo = in.i_gemm_out_int16 ? -32768 : -128;
                const int32_t hi = in.i_gemm_out_int16 ? 32767 : 127;

                for (int l = 0; l < GVU_LANES; l++)
                {
                    int32_t psum_l = in.i_psum_vec[l];
                    if (psum_l == 0 && l == 0 && in.i_psum_data != 0)
                        psum_l = static_cast<int32_t>(in.i_psum_data);
                    else if (!in.i_psum_valid && vbank2_rd_delay_[1] && 4 * l + 3 < static_cast<int>(w.sp_out.o_vbank2_rdata_0.size()))
                    {
                        psum_l = static_cast<int32_t>(
                            static_cast<uint32_t>(w.sp_out.o_vbank2_rdata_0[4 * l]) |
                            (static_cast<uint32_t>(w.sp_out.o_vbank2_rdata_0[4 * l + 1]) << 8) |
                            (static_cast<uint32_t>(w.sp_out.o_vbank2_rdata_0[4 * l + 2]) << 16) |
                            (static_cast<uint32_t>(w.sp_out.o_vbank2_rdata_0[4 * l + 3]) << 24)
                        );
                    }
                    w.mul1_prod[l] = vmul32x32_lane(MA_32X32, static_cast<uint32_t>(psum_l), static_cast<uint32_t>(scale));
                    w.roundshift1_out[l] = vroundshift64_lane(w.mul1_prod[l], shift & 0x3Fu, in.i_gemm_round, in.i_gemm_narrow, NW_16).out_data;
                    w.adder2_r[l] = adder32_lane(ADD_16, 0, 0, false, w.roundshift1_out[l] & 0xFFFFu, zp16) & 0xFFFFu;
                    w.clamp_out[l] = clamp_lane(w.adder2_r[l], static_cast<uint32_t>(lo), hi, 16, true) & 0xFFu;
                    w.lut_8b_idx[l] = static_cast<uint8_t>(w.clamp_out[l] & 0xFFu);
                }
            }
            else if (pipe_ctrl_fsm_ == GEMM_FUSED_ST1)
            {
                // stream mode: LUT_act reads the v32int8 register, the last stage of the pipeline
                for (int l = 0; l < GVU_LANES; l++)
                {
                    w.clamp_out[l] = s8_d_.v[l] & 0xFFu;
                    w.lut_8b_idx[l] = static_cast<uint8_t>(s8_d_.v[l] & 0xFFu);
                }
            }

            // Adder#2: in SOFTMAX_ST2 computes (xi - row_max), in SOFTMAX_ST3 adds zero-point attn_z;
            // in LAYERNORM_ST2 computes D_k = H*x_k - S
            for (int l = 0; l < GVU_LANES; l++)
            {
                if (pipe_ctrl_fsm_ == SOFTMAX_ST2)
                {
                    const uint32_t row_max = (l < static_cast<int>(w.sp_out.o_vbank1_rdata_2.size()))
                        ? static_cast<uint32_t>(w.sp_out.o_vbank1_rdata_2[l]) : 0u;
                    const uint32_t xi = (l < static_cast<int>(w.sp_out.o_vbank0_rdata.size()))
                        ? static_cast<uint32_t>(w.sp_out.o_vbank0_rdata[l]) : 0u;
                    // ADD_8 with sub8_mode = 4'b0001: adder2_a = row_max, adder2_b = xi (gvu_top.sv line 593-594)
                    w.adder2_r[l] = adder32_lane(ADD_8, 0x1, 0, false, row_max, xi) & 0xFFu;
                }
                else if (pipe_ctrl_fsm_ == SOFTMAX_ST3)
                {
                    const uint32_t val = (l * 2 + 1 < static_cast<int>(w.vrf_out.vint16[4].o_data.size()))
                        ? (static_cast<uint32_t>(w.vrf_out.vint16[4].o_data[2 * l]) |
                          (static_cast<uint32_t>(w.vrf_out.vint16[4].o_data[2 * l + 1]) << 8))
                        : 0u;
                    const uint32_t zp = sext8to16(static_cast<uint32_t>(in.i_attn_z));
                    w.adder2_r[l] = adder32_lane(ADD_16, 0, 0, false, val, zp) & 0xFFFFu;
                }
                else if (pipe_ctrl_fsm_ == LAYERNORM_ST2)
                {
                    const uint32_t xi_raw = (l < static_cast<int>(w.sp_out.o_vbank0_rdata.size()))
                        ? static_cast<uint32_t>(w.sp_out.o_vbank0_rdata[l]) : 0u;
                    const int32_t xi = static_cast<int32_t>(static_cast<int8_t>(xi_raw & 0xFFu));
                    const int32_t S = (4 * l + 3 < static_cast<int>(w.sp_out.o_vbank1_rdata_0.size()) &&
                                       (w.sp_out.o_vbank1_rdata_0[4 * l] != 0 || w.sp_out.o_vbank1_rdata_0[4 * l + 1] != 0 ||
                                        w.sp_out.o_vbank1_rdata_0[4 * l + 2] != 0 || w.sp_out.o_vbank1_rdata_0[4 * l + 3] != 0))
                        ? (static_cast<int32_t>(static_cast<uint32_t>(w.sp_out.o_vbank1_rdata_0[4 * l]) |
                                               (static_cast<uint32_t>(w.sp_out.o_vbank1_rdata_0[4 * l + 1]) << 8) |
                                               (static_cast<uint32_t>(w.sp_out.o_vbank1_rdata_0[4 * l + 2]) << 16) |
                                               (static_cast<uint32_t>(w.sp_out.o_vbank1_rdata_0[4 * l + 3]) << 24)))
                        : static_cast<int32_t>(ln_final_s_);
                    const int32_t H = in.i_ln_h > 0 ? in.i_ln_h : (in.i_token_len > 0 ? static_cast<int32_t>(in.i_token_len) : 32);
                    const uint32_t H_xi = static_cast<uint32_t>(H * xi);
                    w.adder2_r[l] = adder32_lane(ADD_32, 0, 0, true, H_xi, static_cast<uint32_t>(S));
                }
            }

            // LUT lookup arbitration (vlut_inp_arb)
            if (pipe_ctrl_fsm_ == SOFTMAX_ST2)
            {
                w.lut_req_vld = w.vrf_out.vint8[0].o_valid;
                for (int l = 0; l < GVU_LANES; l++)
                    w.lut_8b_idx[l] = (l < static_cast<int>(w.vrf_out.vint8[0].o_data.size()))
                        ? w.vrf_out.vint8[0].o_data[l] : 0u;
                w.lut_rsp_rdy = true;
            }
            else if (pipe_ctrl_fsm_ == SOFTMAX_ST3)
            {
                w.lut_req_vld = w.softmax_st3_pipe_en;
                for (int l = 0; l < GVU_LANES; l++)
                {
                    w.lut_32b_idx[l] = (4 * l + 3 < static_cast<int>(w.sp_out.o_vbank1_rdata_0.size()))
                        ? (static_cast<uint32_t>(w.sp_out.o_vbank1_rdata_0[4 * l]) |
                          (static_cast<uint32_t>(w.sp_out.o_vbank1_rdata_0[4 * l + 1]) << 8) |
                          (static_cast<uint32_t>(w.sp_out.o_vbank1_rdata_0[4 * l + 2]) << 16) |
                          (static_cast<uint32_t>(w.sp_out.o_vbank1_rdata_0[4 * l + 3]) << 24))
                        : 0u;
                }
                w.lut_rsp_rdy = true;
            }
            else if (pipe_ctrl_fsm_ == LAYERNORM_ST4)
            {
                w.lut_req_vld = vbank1_rd_delay_[1] || (start_d_cnt_ > 0);
                for (int l = 0; l < GVU_LANES; l++)
                {
                    uint32_t sp_val = (2 * l + 1 < static_cast<int>(w.sp_out.o_vbank1_rdata_1.size()))
                        ? (static_cast<uint32_t>(w.sp_out.o_vbank1_rdata_1[2 * l]) |
                          (static_cast<uint32_t>(w.sp_out.o_vbank1_rdata_1[2 * l + 1]) << 8))
                        : 0u;
                    w.lut_32b_idx[l] = sp_val != 0 ? sp_val : ln_final_w_;
                }
                w.lut_rsp_rdy = true;
            }
            else if (pipe_ctrl_fsm_ == GEMM_FUSED_ST1)
            {
                w.lut_req_vld = in.i_gemm_stream ? s8_v_ : (in.i_psum_valid || vbank2_rd_delay_[1]);
                w.lut_rsp_rdy = true;
            }
            w.lut_in.i_pipe_flush = false;
            w.lut_in.i_lut_wen = w.pipe_ctrl_lut_init_wr;
            w.lut_in.i_lut_wsel = w.pipe_ctrl_lut_init_sel;
            if (!w.pipe_ctrl_lut_init_data.empty())
            {
                std::copy_n(w.pipe_ctrl_lut_init_data.begin(),
                            std::min(w.pipe_ctrl_lut_init_data.size(), w.lut_in.i_lut_wdata.size()),
                            w.lut_in.i_lut_wdata.begin());
            }
            w.lut_in.i_lookup_valid = w.lut_req_vld;
            w.lut_in.i_lut_rsel = w.lut_rsel;
            w.lut_in.i_lut_idx_mode = w.lut_idx_mode;
            w.lut_in.i_lut_compens = w.lut_compens;
            w.lut_in.i_base = w.lut_base;
            w.lut_in.i_W = w.lut_W;
            w.lut_in.i_lerp_shift = w.lut_lerp_shift;
            for (size_t l = 0; l < GVU_LANES; l++)
                w.lut_in.i_8b_lookup_idx[l] = w.lut_8b_idx[l];
            w.lut_in.i_32b_lookup_idx = w.lut_32b_idx;
            w.lut_in.i_lookup_ready = w.lut_rsp_rdy;

            w.lut_out = vlut_.eval(w.lut_in);

            // Adder#1: in SOFTMAX_ST2 accumulates sum of exp(xi - row_max); in LAYERNORM_ST1 accumulates row sum
            for (int l = 0; l < GVU_LANES; l++)
            {
                if (pipe_ctrl_fsm_ == SOFTMAX_ST2)
                {
                    const uint32_t cur_exp = static_cast<uint32_t>(w.lut_out.o_8b_lookup_data[l]);
                    const uint32_t acc = (w.vrf_out.vint32[2].o_valid && 4 * l + 3 < static_cast<int>(w.vrf_out.vint32[2].o_data.size()))
                        ? (static_cast<uint32_t>(w.vrf_out.vint32[2].o_data[4 * l]) |
                          (static_cast<uint32_t>(w.vrf_out.vint32[2].o_data[4 * l + 1]) << 8) |
                          (static_cast<uint32_t>(w.vrf_out.vint32[2].o_data[4 * l + 2]) << 16) |
                          (static_cast<uint32_t>(w.vrf_out.vint32[2].o_data[4 * l + 3]) << 24))
                        : 0u;
                    w.adder1_r[l] = adder32_lane(ADD_32, 0, 0, false, cur_exp, acc);
                }
                else if (pipe_ctrl_fsm_ == LAYERNORM_ST1)
                {
                    w.adder1_r[l] = static_cast<uint32_t>(ln_s_acc_);
                }
                else if (pipe_ctrl_fsm_ == RESIDUAL_ST1 && vbank0_rd_delay_[1])
                {
                    const unsigned part = b_part_delay_[1];
                    const int8_t a_val = (l < static_cast<int>(w.sp_out.o_vbank0_rdata.size()))
                        ? static_cast<int8_t>(w.sp_out.o_vbank0_rdata[l]) : static_cast<int8_t>(0);
                    const int8_t b_val = (GVU_LANES * static_cast<int>(part) + l < static_cast<int>(w.sp_out.o_vbank1_rdata_0.size()))
                        ? static_cast<int8_t>(w.sp_out.o_vbank1_rdata_0[GVU_LANES * static_cast<int>(part) + l]) : static_cast<int8_t>(0);

                    const uint32_t da = adder32_lane(ADD_16, 0, 1, false, static_cast<uint32_t>(static_cast<int32_t>(a_val)) & 0xFFFFu, static_cast<uint32_t>(in.i_res_zp_a) & 0xFFFFu) & 0xFFFFu;
                    const uint32_t db = adder32_lane(ADD_16, 0, 1, false, static_cast<uint32_t>(static_cast<int32_t>(b_val)) & 0xFFFFu, static_cast<uint32_t>(in.i_res_zp_b) & 0xFFFFu) & 0xFFFFu;

                    const uint64_t a40 = static_cast<uint64_t>(static_cast<int64_t>(sext32(da, 16))) & 0xFFFFFFFFFFull;
                    const u128 pa = vmul48x40_lane(MB_40X32, a40, static_cast<uint32_t>(in.i_res_scale_a)).out_prod;
                    const uint64_t pb = vmul32x32_lane(MA_32X16, static_cast<uint32_t>(in.i_res_scale_b), db);

                    const uint32_t ra = vroundshift72_lane(pa, in.i_res_shift_a, in.i_res_round, NARROW_SATURATE, NW_32).out_data;
                    const uint32_t rb = vroundshift64_lane(pb, in.i_res_shift_b, in.i_res_round, NARROW_SATURATE, NW_32).out_data;

                    w.adder1_r[l] = adder32_lane(ADD_32, 0, 0, false, ra, rb);
                }
            }

            // Mul#1 (16x16): in SOFTMAX_ST3 multiplies exp * recip via MA_16X16
            for (int l = 0; l < GVU_LANES; l++)
            {
                if (pipe_ctrl_fsm_ == SOFTMAX_ST3)
                {
                    const uint32_t recip_val = w.lut_out.o_16b_lookup_data[l];
                    const uint32_t exp_val = (l < static_cast<int>(w.sp_out.o_vbank2_rdata_1.size()))
                        ? static_cast<uint32_t>(w.sp_out.o_vbank2_rdata_1[l] & 0xFFu) : 0u;
                    w.mul1_prod[l] = vmul32x32_lane(MA_16X16, recip_val, exp_val) & 0xFFFFFFFFu;
                }
            }

            // RoundShift#0 (72-bit): normalizes product
            for (int l = 0; l < GVU_LANES; l++)
            {
                if (pipe_ctrl_fsm_ == SOFTMAX_ST3)
                {
                    const int32_t p32 = static_cast<int32_t>(w.mul1_prod[l]);
                    const u128 prod72 = static_cast<u128>(static_cast<i128>(p32));
                    const unsigned sh = (w.lut_out.o_e_shift[l] != 0) ? (w.lut_out.o_e_shift[l] & 0x1Fu) : (exp_e_shift_buf_[3][l] & 0x1Fu);
                    w.roundshift0_out[l] = vroundshift72_lane(prod72, sh, RND_NEAR_AWAY, NARROW_WRAP, NW_32).out_data;
                }
            }

            // Mul#0 (32x32): in SOFTMAX_ST3 multiplies by attn_scale via MB_32X32_16X8
            for (int l = 0; l < GVU_LANES; l++)
            {
                if (pipe_ctrl_fsm_ == SOFTMAX_ST3)
                {
                    const uint64_t scale40 = static_cast<uint64_t>(static_cast<int64_t>(in.i_attn_scale)) & 0xFFFFFFFFFFull;
                    w.mul0_out[l] = vmul48x40_lane(MB_32X32_16X8, w.roundshift0_out[l], scale40);
                }
            }

            // RoundShift#1 (64-bit): narrows to 16 bits
            for (int l = 0; l < GVU_LANES; l++)
            {
                if (pipe_ctrl_fsm_ == SOFTMAX_ST3)
                {
                    const unsigned sh = in.i_attn_shift & 0x3Fu;
                    const uint64_t prod64 = static_cast<uint64_t>(w.mul0_out[l].out_prod);
                    w.roundshift1_out[l] = vroundshift64_lane(prod64, sh, RND_NEAR_AWAY, NARROW_WRAP, NW_16).out_data;
                }
            }

            // Clamp: clamps to signed int8 [-128, 127]
            for (int l = 0; l < GVU_LANES; l++)
            {
                if (pipe_ctrl_fsm_ == SOFTMAX_ST3)
                {
                    const uint32_t val16 = (2 * l + 1 < static_cast<int>(w.vrf_out.vint16[5].o_data.size()))
                        ? (static_cast<uint32_t>(w.vrf_out.vint16[5].o_data[2 * l]) |
                          (static_cast<uint32_t>(w.vrf_out.vint16[5].o_data[2 * l + 1]) << 8))
                        : 0u;
                    const int32_t val = static_cast<int32_t>(sext32(val16, 16));
                    w.clamp_out[l] = static_cast<uint32_t>(clamp32_lane(val, -128, 127)) & 0xFFu;
                }
                else if (pipe_ctrl_fsm_ == LAYERNORM_ST2)
                    w.clamp_out[l] = static_cast<uint32_t>(static_cast<int8_t>(ln_final_var_)) & 0xFFu;
                else if (pipe_ctrl_fsm_ == LAYERNORM_ST4)
                {
                    const int32_t D_val = (4 * l + 3 < static_cast<int>(w.sp_out.o_vbank2_rdata_0.size()))
                        ? (static_cast<int32_t>(static_cast<uint32_t>(w.sp_out.o_vbank2_rdata_0[4 * l]) |
                                               (static_cast<uint32_t>(w.sp_out.o_vbank2_rdata_0[4 * l + 1]) << 8) |
                                               (static_cast<uint32_t>(w.sp_out.o_vbank2_rdata_0[4 * l + 2]) << 16) |
                                               (static_cast<uint32_t>(w.sp_out.o_vbank2_rdata_0[4 * l + 3]) << 24)))
                        : static_cast<int32_t>(w.adder2_r[l]);
                    const uint32_t r_lut = w.lut_out.o_16b_lookup_data[l];
                    const uint32_t r = r_lut != 0 ? r_lut : 1u;
                    const unsigned E = w.lut_out.o_e_shift[l];
                    const i128 prod = static_cast<i128>(r) * D_val;
                    const i128 acc = in.i_ln_r9_exact ? (prod * in.i_ln_m0_div)
                                   : (((prod >> in.i_ln_r9_lsb_bits) * in.i_ln_m0_div) << in.i_ln_r9_lsb_bits);
                    const i128 z_raw = ln_rshift_wide(acc, in.i_ln_ts_div + E) + in.i_ln_z_div;
                    w.clamp_out[l] = clamp_lane(static_cast<uint32_t>(z_raw), static_cast<uint32_t>(-128), 127, 32, true) & 0xFFu;
                }
                else if (pipe_ctrl_fsm_ == LAYERNORM_ST5)
                {
                    const int8_t z_val = (l < static_cast<int>(w.sp_out.o_vbank0_rdata.size()))
                        ? static_cast<int8_t>(w.sp_out.o_vbank0_rdata[l]) : static_cast<int8_t>(0);
                    const int8_t gamma = (w.sp_out.o_sbank_rdata_1 != 0)
                        ? static_cast<int8_t>(w.sp_out.o_sbank_rdata_1) : static_cast<int8_t>(1);
                    const int32_t beta = static_cast<int32_t>(w.sp_out.o_sbank_rdata_0);
                    const int32_t lo = in.i_ln_out_int16 ? -32768 : -128;
                    const int32_t hi = in.i_ln_out_int16 ? 32767 : 127;
                    const int32_t t = ln_requant_clamp(static_cast<i128>(z_val - in.i_ln_z_div) * gamma, in.i_ln_m0_mul, in.i_ln_ts_mul, in.i_ln_z_mul, lo, hi);
                    const int32_t y = static_cast<int32_t>(clamp_lane(static_cast<uint32_t>(t - in.i_ln_z_mul + beta + in.i_ln_z_out),
                                                                      static_cast<uint32_t>(lo), static_cast<uint32_t>(hi), 32, true));
                    w.clamp_out[l] = static_cast<uint32_t>(y) & 0xFFu;
                }
                else if (pipe_ctrl_fsm_ == RESIDUAL_ST2 && vbank2_rd_delay_[1])
                {
                    const int32_t sum_val = (4 * l + 3 < static_cast<int>(w.sp_out.o_vbank2_rdata_0.size()))
                        ? static_cast<int32_t>(static_cast<uint32_t>(w.sp_out.o_vbank2_rdata_0[4 * l]) |
                                              (static_cast<uint32_t>(w.sp_out.o_vbank2_rdata_0[4 * l + 1]) << 8) |
                                              (static_cast<uint32_t>(w.sp_out.o_vbank2_rdata_0[4 * l + 2]) << 16) |
                                              (static_cast<uint32_t>(w.sp_out.o_vbank2_rdata_0[4 * l + 3]) << 24))
                        : 0;
                    const uint64_t po = vmul32x32_lane(MA_32X32, static_cast<uint32_t>(sum_val), static_cast<uint32_t>(in.i_res_scale_out));
                    const uint32_t rs = vroundshift64_lane(po, in.i_res_shift_out, in.i_res_round, NARROW_SATURATE, NW_16).out_data & 0xFFFFu;
                    const uint32_t zs = adder32_lane(ADD_16, 0, 0, false, rs, static_cast<uint32_t>(in.i_res_zp_out) & 0xFFFFu) & 0xFFFFu;
                    if (in.i_res_out_int16)
                        w.clamp_out[l] = clamp_lane(zs, 0x8000u, 0x7FFFu, 16, true) & 0xFFFFu;
                    else
                        w.clamp_out[l] = clamp_lane(zs, 0xFF80u, 0x007Fu, 16, true) & 0xFFu;
                }
            }

            // ---- 7. Stream Register File Connections (stream_vrf_ports_arb) --------------------------------
            if (pipe_ctrl_fsm_ == SOFTMAX_ST2)
            {
                w.vrf_in.vint8[0].valid = w.softmax_st2_pipe_en;
                w.vrf_in.vint8[0].ready = w.lut_out.o_lookup_ready;
                w.vrf_in.vint32[0].ready = w.lut_out.o_lookup_ready;
                w.vrf_in.vint8[0].data.resize(GVU_LANES);
                for (int l = 0; l < GVU_LANES; l++)
                    w.vrf_in.vint8[0].data[l] = static_cast<uint8_t>(w.adder2_r[l] & 0xFFu);

                w.vrf_in.vint32[2].valid = w.lut_out.o_lookup_valid;
                w.vrf_in.vint32[2].ready = true;
                w.vrf_in.vint32[2].data.resize(4 * GVU_LANES);
                for (int l = 0; l < GVU_LANES; l++)
                    for (int b = 0; b < 4; b++)
                        w.vrf_in.vint32[2].data[4 * l + b] = static_cast<uint8_t>(w.adder1_r[l] >> (8 * b));
            }
            else if (pipe_ctrl_fsm_ == SOFTMAX_ST3)
            {
                w.vrf_in.vint16[2].valid = w.lut_out.o_lookup_valid;
                w.vrf_in.vint16[2].ready = true;
                w.vrf_in.vint16[2].data.resize(2 * GVU_LANES);
                for (int l = 0; l < GVU_LANES; l++)
                {
                    w.vrf_in.vint16[2].data[2 * l] = static_cast<uint8_t>(w.lut_out.o_16b_lookup_data[l] & 0xFFu);
                    w.vrf_in.vint16[2].data[2 * l + 1] = static_cast<uint8_t>(w.lut_out.o_16b_lookup_data[l] >> 8);
                }

                w.vrf_in.vint32[1].valid = w.lut_out.o_lookup_valid;
                w.vrf_in.vint32[1].ready = true;
                w.vrf_in.vint32[1].data.resize(4 * GVU_LANES);
                for (int l = 0; l < GVU_LANES; l++)
                {
                    const uint32_t prod_s = static_cast<uint32_t>(w.mul1_prod[l]);
                    for (int b = 0; b < 4; b++)
                        w.vrf_in.vint32[1].data[4 * l + b] = static_cast<uint8_t>(prod_s >> (8 * b));
                }

                w.vrf_in.vint32[3].valid = w.lut_out.o_lookup_valid;
                w.vrf_in.vint32[3].ready = true;
                w.vrf_in.vint32[3].data.resize(4 * GVU_LANES);
                for (int l = 0; l < GVU_LANES; l++)
                    for (int b = 0; b < 4; b++)
                        w.vrf_in.vint32[3].data[4 * l + b] = static_cast<uint8_t>(w.roundshift0_out[l] >> (8 * b));

                w.vrf_in.vint64[1].valid = w.lut_out.o_lookup_valid;
                w.vrf_in.vint64[1].ready = true;
                w.vrf_in.vint64[1].data.resize(8 * GVU_LANES);
                for (int l = 0; l < GVU_LANES; l++)
                {
                    const uint64_t p64 = static_cast<uint64_t>(w.mul0_out[l].out_prod);
                    for (int b = 0; b < 8; b++)
                        w.vrf_in.vint64[1].data[8 * l + b] = static_cast<uint8_t>(p64 >> (8 * b));
                }

                w.vrf_in.vint16[4].valid = vbank2_rd_delay_[1];
                w.vrf_in.vint16[4].ready = true;
                w.vrf_in.vint16[4].data.resize(2 * GVU_LANES);
                for (int l = 0; l < GVU_LANES; l++)
                {
                    w.vrf_in.vint16[4].data[2 * l] = static_cast<uint8_t>(w.roundshift1_out[l] & 0xFFu);
                    w.vrf_in.vint16[4].data[2 * l + 1] = static_cast<uint8_t>(w.roundshift1_out[l] >> 8);
                }

                w.vrf_in.vint16[5].valid = w.vrf_out.vint16[4].o_valid;
                w.vrf_in.vint16[5].ready = true;
                w.vrf_in.vint16[5].data.resize(2 * GVU_LANES);
                for (int l = 0; l < GVU_LANES; l++)
                {
                    w.vrf_in.vint16[5].data[2 * l] = static_cast<uint8_t>(w.adder2_r[l] & 0xFFu);
                    w.vrf_in.vint16[5].data[2 * l + 1] = static_cast<uint8_t>(w.adder2_r[l] >> 8);
                }
            }
            else if (pipe_ctrl_fsm_ == LAYERNORM_ST1)
            {
                w.vrf_in.vint32[0].valid = vbank0_rd_delay_[1];
                w.vrf_in.vint32[0].ready = true;
                w.vrf_in.vint32[0].data.resize(4 * GVU_LANES);
                for (int l = 0; l < GVU_LANES; l++)
                    for (int b = 0; b < 4; b++)
                        w.vrf_in.vint32[0].data[4 * l + b] = static_cast<uint8_t>(w.adder1_r[l] >> (8 * b));
            }
            else if (pipe_ctrl_fsm_ == LAYERNORM_ST2)
            {
                w.vrf_in.vint32[2].valid = vbank0_rd_delay_[1];
                w.vrf_in.vint32[2].ready = true;
                w.vrf_in.vint32[2].data.resize(4 * GVU_LANES);
                for (int l = 0; l < GVU_LANES; l++)
                    for (int b = 0; b < 4; b++)
                        w.vrf_in.vint32[2].data[4 * l + b] = static_cast<uint8_t>(w.adder2_r[l] >> (8 * b));

                w.vrf_in.vint64[0].valid = vbank0_rd_delay_[1];
                w.vrf_in.vint64[0].ready = true;
                w.vrf_in.vint64[0].data.resize(8 * GVU_LANES);
                for (int l = 0; l < GVU_LANES; l++)
                {
                    const int64_t d2 = static_cast<int64_t>(static_cast<int32_t>(w.adder2_r[l])) * static_cast<int32_t>(w.adder2_r[l]);
                    for (int b = 0; b < 8; b++)
                        w.vrf_in.vint64[0].data[8 * l + b] = static_cast<uint8_t>(d2 >> (8 * b));
                }
            }
            else if (pipe_ctrl_fsm_ == LAYERNORM_ST3)
            {
                w.vrf_in.vint8[0].valid = vbank0_rd_delay_[1];
                w.vrf_in.vint8[0].ready = true;
                w.vrf_in.vint8[0].data.resize(GVU_LANES);
                for (int l = 0; l < GVU_LANES; l++)
                    w.vrf_in.vint8[0].data[l] = static_cast<uint8_t>(ln_final_var_ & 0xFFu);

                w.vrf_in.vint16[0].valid = vbank0_rd_delay_[1];
                w.vrf_in.vint16[0].ready = true;
                w.vrf_in.vint16[0].data.resize(2 * GVU_LANES);
                for (int l = 0; l < GVU_LANES; l++)
                {
                    w.vrf_in.vint16[0].data[2 * l] = static_cast<uint8_t>(ln_final_w_ & 0xFFu);
                    w.vrf_in.vint16[0].data[2 * l + 1] = static_cast<uint8_t>((ln_final_w_ >> 8) & 0xFFu);
                }
            }
            else if (pipe_ctrl_fsm_ == LAYERNORM_ST4)
            {
                w.vrf_in.vint8[0].valid = vbank2_rd_delay_[1];
                w.vrf_in.vint8[0].ready = true;
                w.vrf_in.vint8[0].data.resize(GVU_LANES);
                for (int l = 0; l < GVU_LANES; l++)
                    w.vrf_in.vint8[0].data[l] = static_cast<uint8_t>(w.clamp_out[l] & 0xFFu);
            }
            else if (pipe_ctrl_fsm_ == LAYERNORM_ST5)
            {
                w.vrf_in.vint8[0].valid = vbank0_rd_delay_[1];
                w.vrf_in.vint8[0].ready = true;
                w.vrf_in.vint8[0].data.resize(GVU_LANES);
                for (int l = 0; l < GVU_LANES; l++)
                    w.vrf_in.vint8[0].data[l] = static_cast<uint8_t>(w.clamp_out[l] & 0xFFu);
            }
            else if (pipe_ctrl_fsm_ == GEMM_FUSED_ST1 && !in.i_gemm_stream)
            {
                const bool valid_beat = in.i_psum_valid || vbank2_rd_delay_[1];
                w.vrf_in.vint64[1].valid = valid_beat;
                w.vrf_in.vint64[1].ready = true;
                w.vrf_in.vint64[1].data.resize(8 * GVU_LANES);
                for (int l = 0; l < GVU_LANES; l++)
                    for (int b = 0; b < 8; b++)
                        w.vrf_in.vint64[1].data[8 * l + b] = static_cast<uint8_t>(w.mul1_prod[l] >> (8 * b));

                w.vrf_in.vint16[4].valid = valid_beat;
                w.vrf_in.vint16[4].ready = true;
                w.vrf_in.vint16[4].data.resize(2 * GVU_LANES);
                for (int l = 0; l < GVU_LANES; l++)
                {
                    w.vrf_in.vint16[4].data[2 * l] = static_cast<uint8_t>(w.roundshift1_out[l] & 0xFFu);
                    w.vrf_in.vint16[4].data[2 * l + 1] = static_cast<uint8_t>(w.roundshift1_out[l] >> 8);
                }

                w.vrf_in.vint16[5].valid = valid_beat;
                w.vrf_in.vint16[5].ready = true;
                w.vrf_in.vint16[5].data.resize(2 * GVU_LANES);
                for (int l = 0; l < GVU_LANES; l++)
                {
                    w.vrf_in.vint16[5].data[2 * l] = static_cast<uint8_t>(w.adder2_r[l] & 0xFFu);
                    w.vrf_in.vint16[5].data[2 * l + 1] = static_cast<uint8_t>(w.adder2_r[l] >> 8);
                }

                w.vrf_in.vint8[0].valid = valid_beat;
                w.vrf_in.vint8[0].ready = true;
                w.vrf_in.vint8[0].data.resize(GVU_LANES);
                for (int l = 0; l < GVU_LANES; l++)
                    w.vrf_in.vint8[0].data[l] = static_cast<uint8_t>(w.clamp_out[l] & 0xFFu);
            }
            else if (pipe_ctrl_fsm_ == RESIDUAL_ST2)
            {
                const bool valid_beat = vbank2_rd_delay_[1];
                w.vrf_in.vint8[0].valid = valid_beat;
                w.vrf_in.vint8[0].ready = true;
                w.vrf_in.vint8[0].data.resize(GVU_LANES);
                for (int l = 0; l < GVU_LANES; l++)
                    w.vrf_in.vint8[0].data[l] = static_cast<uint8_t>(w.clamp_out[l] & 0xFFu);
            }

            w.vrf_out = stream_vrf_.eval(w.vrf_in);

            // ---- 7. Scratchpad Write Data & Enables for ST2 and ST3 -----------------------------------------
            if (pipe_ctrl_fsm_ == SOFTMAX_ST2)
            {
                w.ctrl_vbank2_wr = w.lut_out.o_lookup_valid;
                for (int l = 0; l < GVU_LANES; l++)
                    w.sp_in.i_vbank2_wdata_1[l] = w.lut_out.o_8b_lookup_data[l];

                w.ctrl_vbank1_wr = w.softmax_token_last;
                if (w.softmax_token_last)
                {
                    for (int l = 0; l < GVU_LANES; l++)
                    {
                        const uint32_t val = (4 * l + 3 < static_cast<int>(w.vrf_out.vint32[2].o_data.size()))
                            ? (static_cast<uint32_t>(w.vrf_out.vint32[2].o_data[4 * l]) |
                              (static_cast<uint32_t>(w.vrf_out.vint32[2].o_data[4 * l + 1]) << 8) |
                              (static_cast<uint32_t>(w.vrf_out.vint32[2].o_data[4 * l + 2]) << 16) |
                              (static_cast<uint32_t>(w.vrf_out.vint32[2].o_data[4 * l + 3]) << 24))
                            : 0u;
                        for (int b = 0; b < 4; b++)
                            w.sp_in.i_vbank1_wdata_0[4 * l + b] = static_cast<uint8_t>(val >> (8 * b));
                    }
                }

                w.vlsu_in.i_vbank1_wr = w.ctrl_vbank1_wr;
                w.vlsu_in.i_vbank2_wr = w.ctrl_vbank2_wr;
                w.vlsu_out = vlsu_.eval(w.vlsu_in);
                w.sp_in.i_vbank1_en_0 = w.vlsu_out.o_vbank1_en_0;
                w.sp_in.i_vbank1_wen_0 = w.vlsu_out.o_vbank1_wen_0;
                w.sp_in.i_vbank2_en_1 = w.vlsu_out.o_vbank2_en_1;
                w.sp_in.i_vbank2_wen_1 = w.vlsu_out.o_vbank2_wen_1;
            }
            else if (pipe_ctrl_fsm_ == SOFTMAX_ST3)
            {
                w.ctrl_vbank0_wr = w.vrf_out.vint16[5].o_valid;
                for (int l = 0; l < GVU_LANES; l++)
                    w.sp_in.i_vbank0_wdata[l] = static_cast<uint8_t>(w.clamp_out[l] & 0xFFu);

                w.vlsu_in.i_vbank0_wr = w.ctrl_vbank0_wr;
                w.vlsu_out = vlsu_.eval(w.vlsu_in);
                w.sp_in.i_vbank0_en = w.vlsu_out.o_vbank0_en;
                w.sp_in.i_vbank0_wen = w.vlsu_out.o_vbank0_wen;
            }
            else if (pipe_ctrl_fsm_ == LAYERNORM_ST1)
            {
                if (w.layernorm_token_last)
                {
                    w.ctrl_vbank1_port_sel = 0;
                    w.ctrl_vbank1_wr = true;
                    for (int l = 0; l < GVU_LANES; l++)
                        for (int b = 0; b < 4; b++)
                            w.sp_in.i_vbank1_wdata_0[4 * l + b] = static_cast<uint8_t>(w.adder1_r[l] >> (8 * b));

                    w.vlsu_in.i_vbank1_port_sel = 0;
                    w.vlsu_in.i_vbank1_wr = true;
                    w.vlsu_out = vlsu_.eval(w.vlsu_in);
                    w.sp_in.i_vbank1_en_0 = w.vlsu_out.o_vbank1_en_0;
                    w.sp_in.i_vbank1_wen_0 = w.vlsu_out.o_vbank1_wen_0;
                }
            }
            else if (pipe_ctrl_fsm_ == LAYERNORM_ST2)
            {
                w.ctrl_vbank2_port_sel = false; // port 0 (v32int32)
                w.ctrl_vbank2_wr = vbank0_rd_delay_[1];
                for (int l = 0; l < GVU_LANES; l++)
                    for (int b = 0; b < 4; b++)
                        w.sp_in.i_vbank2_wdata_0[4 * l + b] = static_cast<uint8_t>(w.adder2_r[l] >> (8 * b));

                if (w.layernorm_token_last)
                {
                    w.ctrl_vbank0_wr = true;
                    for (int l = 0; l < GVU_LANES; l++)
                        w.sp_in.i_vbank0_wdata[l] = static_cast<uint8_t>(w.clamp_out[l] & 0xFFu);
                }

                w.vlsu_in.i_vbank2_port_sel = false;
                w.vlsu_in.i_vbank2_wr = w.ctrl_vbank2_wr;
                w.vlsu_in.i_vbank0_wr = w.ctrl_vbank0_wr;
                w.vlsu_out = vlsu_.eval(w.vlsu_in);
                w.sp_in.i_vbank2_en_0 = w.vlsu_out.o_vbank2_en_0;
                w.sp_in.i_vbank2_wen_0 = w.vlsu_out.o_vbank2_wen_0;
                w.sp_in.i_vbank0_en = w.vlsu_out.o_vbank0_en;
                w.sp_in.i_vbank0_wen = w.vlsu_out.o_vbank0_wen;
            }
            else if (pipe_ctrl_fsm_ == LAYERNORM_ST3)
            {
                w.ctrl_vbank1_port_sel = 1; // port 1 (v32int16)
                w.ctrl_vbank1_wr = vbank0_rd_delay_[1];
                for (int l = 0; l < GVU_LANES; l++)
                {
                    w.sp_in.i_vbank1_wdata_1[2 * l] = static_cast<uint8_t>(ln_final_w_ & 0xFFu);
                    w.sp_in.i_vbank1_wdata_1[2 * l + 1] = static_cast<uint8_t>((ln_final_w_ >> 8) & 0xFFu);
                }

                w.vlsu_in.i_vbank1_port_sel = 1;
                w.vlsu_in.i_vbank1_wr = w.ctrl_vbank1_wr;
                w.vlsu_out = vlsu_.eval(w.vlsu_in);
                w.sp_in.i_vbank1_en_1 = w.vlsu_out.o_vbank1_en_1;
                w.sp_in.i_vbank1_wen_1 = w.vlsu_out.o_vbank1_wen_1;
            }
            else if (pipe_ctrl_fsm_ == LAYERNORM_ST4)
            {
                w.ctrl_vbank0_wr = vbank2_rd_delay_[1] || w.lut_out.o_lookup_valid;
                for (int l = 0; l < GVU_LANES; l++)
                    w.sp_in.i_vbank0_wdata[l] = static_cast<uint8_t>(w.clamp_out[l] & 0xFFu);

                w.vlsu_in.i_vbank0_wr = w.ctrl_vbank0_wr;
                w.vlsu_out = vlsu_.eval(w.vlsu_in);
                w.sp_in.i_vbank0_en = w.vlsu_out.o_vbank0_en;
                w.sp_in.i_vbank0_wen = w.vlsu_out.o_vbank0_wen;
            }
            else if (pipe_ctrl_fsm_ == LAYERNORM_ST5)
            {
                w.ctrl_vbank1_port_sel = 2; // port 2 (v32int8)
                w.ctrl_vbank1_wr = vbank0_rd_delay_[1];
                for (int l = 0; l < GVU_LANES; l++)
                    w.sp_in.i_vbank1_wdata_2[l] = static_cast<uint8_t>(w.clamp_out[l] & 0xFFu);

                w.vlsu_in.i_vbank1_port_sel = 2;
                w.vlsu_in.i_vbank1_wr = w.ctrl_vbank1_wr;
                w.vlsu_out = vlsu_.eval(w.vlsu_in);
                w.sp_in.i_vbank1_en_2 = w.vlsu_out.o_vbank1_en_2;
                w.sp_in.i_vbank1_wen_2 = w.vlsu_out.o_vbank1_wen_2;
            }
            else if (pipe_ctrl_fsm_ == GEMM_FUSED_ST1 && !in.i_gemm_stream)
            {
                const bool valid_beat = in.i_psum_valid || vbank2_rd_delay_[1];
                w.ctrl_vbank0_wr = valid_beat;
                for (int l = 0; l < GVU_LANES; l++)
                {
                    const uint8_t byte_out = in.i_gemm_skip_act ? static_cast<uint8_t>(w.clamp_out[l] & 0xFFu)
                                                                : w.lut_out.o_8b_lookup_data[l];
                    w.sp_in.i_vbank0_wdata[l] = byte_out;
                }

                w.vlsu_in.i_vbank0_wr = w.ctrl_vbank0_wr;
                w.vlsu_out = vlsu_.eval(w.vlsu_in);
                w.sp_in.i_vbank0_en = w.vlsu_out.o_vbank0_en;
                w.sp_in.i_vbank0_wen = w.vlsu_out.o_vbank0_wen;
            }
            else if (pipe_ctrl_fsm_ == RESIDUAL_ST1)
            {
                w.ctrl_vbank2_port_sel = false; // port 0 (v32int32)
                w.ctrl_vbank2_wr = vbank0_rd_delay_[1];
                for (int l = 0; l < GVU_LANES; l++)
                    for (int b = 0; b < 4; b++)
                        w.sp_in.i_vbank2_wdata_0[4 * l + b] = static_cast<uint8_t>(w.adder1_r[l] >> (8 * b));

                w.vlsu_in.i_vbank2_port_sel = false;
                w.vlsu_in.i_vbank2_wr = w.ctrl_vbank2_wr;
                // the write of this cycle is token end_d_cnt_ of tile tile_cnt_ (both count after the write)
                if (res_last_row(in)) w.vlsu_in.i_vbank2_lane_wmask = in.i_res_last_lane_mask;
                w.vlsu_out = vlsu_.eval(w.vlsu_in);
                w.sp_in.i_vbank2_en_0 = w.vlsu_out.o_vbank2_en_0;
                w.sp_in.i_vbank2_wen_0 = w.vlsu_out.o_vbank2_wen_0;
                w.sp_in.i_vbank2_wmask_0 = w.vlsu_out.o_vbank2_wmask_0;
            }
            else if (pipe_ctrl_fsm_ == RESIDUAL_ST2)
            {
                w.ctrl_vbank0_wr = vbank2_rd_delay_[1];
                for (int l = 0; l < GVU_LANES; l++)
                    w.sp_in.i_vbank0_wdata[l] = static_cast<uint8_t>(w.clamp_out[l] & 0xFFu);

                w.vlsu_in.i_vbank0_wr = w.ctrl_vbank0_wr;
                if (res_last_row(in))
                {
                    w.vlsu_in.i_vbank0_lane_wmask = in.i_res_last_lane_mask;
                    for (int b = 0; b < P_.NUM_LANE / 8; b++) w.sp_in.i_vbank0_wmask[static_cast<size_t>(b)] = static_cast<uint8_t>(in.i_res_last_lane_mask >> (8 * b));
                }
                w.vlsu_out = vlsu_.eval(w.vlsu_in);
                w.sp_in.i_vbank0_en = w.vlsu_out.o_vbank0_en;
                w.sp_in.i_vbank0_wen = w.vlsu_out.o_vbank0_wen;
            }

            // ---- 8. Controller FSM next-state logic (fsm_nxt_comb) -------------------------------------------
            switch (pipe_ctrl_fsm_)
            {
            case IDLE:
                if (in.i_gvu_op == GVU_OP_SOFTMAX)
                {
                    if (!is_lut_recip_init_) w.fsm_nxt = LUT_RECIP_INIT;
                    else if (!is_lut_exp_init_) w.fsm_nxt = LUT_EXP_INIT;
                    else w.fsm_nxt = SOFTMAX_ST1;
                }
                else if (in.i_gvu_op == GVU_OP_LAYERNORM)
                {
                    if (!is_lut_rsqrt_init_) w.fsm_nxt = LUT_RSQRT_INIT;
                    else w.fsm_nxt = LAYERNORM_ST1;
                }
                else if (in.i_gvu_op == GVU_OP_GEMM_FUSED)
                {
                    if (!is_lut_act_init_ || in.i_gemm_lut_reload) { w.fsm_nxt = LUT_ACT_INIT; w.is_lut_act_init_nxt = false; }
                    else w.fsm_nxt = GEMM_FUSED_ST1;
                }
                else if (in.i_gvu_op == GVU_OP_RESIDUAL || in.i_gvu_op == GVU_OP_ELEMWISE)
                {
                    w.fsm_nxt = RESIDUAL_ST1;
                }
                else if (in.i_gvu_op == GVU_OP_MAXPOOL)
                {
                    w.fsm_nxt = POOLING_MAX;
                }
                break;
            case POOLING_MAX:
                if (w.vlsu_out.o_pooling_last_wr)
                    w.fsm_nxt = IDLE;
                break;
            case LUT_ACT_INIT:
                if (vlut_.mem.o_init_done(LUT_ACT))
                {
                    w.is_lut_act_init_nxt = true;
                    w.fsm_nxt = GEMM_FUSED_ST1;
                }
                break;
            case LUT_RECIP_INIT:
                if (vlut_.mem.o_init_done(LUT_RECIP))
                {
                    w.is_lut_recip_init_nxt = true;
                    w.fsm_nxt = !is_lut_exp_init_ ? LUT_EXP_INIT : SOFTMAX_ST1;
                }
                break;
            case LUT_EXP_INIT:
                if (vlut_.mem.o_init_done(LUT_EXP))
                {
                    w.is_lut_exp_init_nxt = true;
                    w.fsm_nxt = SOFTMAX_ST1;
                }
                break;
            case LUT_RSQRT_INIT:
                if (vlut_.mem.o_init_done(LUT_RSQRT))
                {
                    w.is_lut_rsqrt_init_nxt = true;
                    w.fsm_nxt = LAYERNORM_ST1;
                }
                break;
            case SOFTMAX_ST1:
                if (w.cmp_out.out_valid)
                    w.fsm_nxt = SOFTMAX_ST2;
                break;
            case SOFTMAX_ST2:
                if (w.softmax_token_last)
                    w.fsm_nxt = SOFTMAX_ST3;
                break;
            case SOFTMAX_ST3:
                if (w.softmax_token_last)
                    w.fsm_nxt = IDLE;
                break;
            case LAYERNORM_ST1:
                if (w.layernorm_token_last)
                    w.fsm_nxt = LAYERNORM_ST2;
                break;
            case LAYERNORM_ST2:
                if (w.layernorm_token_last)
                    w.fsm_nxt = LAYERNORM_ST3;
                break;
            case LAYERNORM_ST3:
                if (start_d_cnt_ >= 1 && (vbank0_rd_delay_[1] || end_d_cnt_ >= 1))
                    w.fsm_nxt = LAYERNORM_ST4;
                break;
            case LAYERNORM_ST4:
                if (w.layernorm_token_last)
                    w.fsm_nxt = LAYERNORM_ST5;
                break;
            case LAYERNORM_ST5:
                if (w.layernorm_token_last)
                    w.fsm_nxt = IDLE;
                break;
            case GEMM_FUSED_ST1:
                if (in.i_gemm_stream ? ((in.i_gemm_stream_stop && !stream_busy()) || stream_exit(in)) : w.gemm_fused_token_last)
                    w.fsm_nxt = IDLE;
                break;
            case RESIDUAL_ST1:
                if (w.residual_st1_done)
                    w.fsm_nxt = RESIDUAL_ST2;
                break;
            case RESIDUAL_ST2:
                if (w.residual_st2_done)
                    w.fsm_nxt = IDLE;
                break;
            default:
                w.fsm_nxt = IDLE;
                break;
            }

            // RTL gvu_pipe_controller.sv:698: the reset wins over the init-done update above
            if (in.i_lut_act_reset) w.is_lut_act_init_nxt = false;

            w.vlsu_in.i_agu_addr_reset = (w.fsm_nxt != pipe_ctrl_fsm_);
            // AGU_ND: with vbank0 bound, A is read with the source descriptor in RESIDUAL_ST1 and the result written with the destination
            // descriptor in RESIDUAL_ST2; the pick is the one of the state the address register enters at this edge (bound in tick()).
            {
                const control_fsm_t st = w.vlsu_in.i_agu_addr_reset ? w.fsm_nxt : pipe_ctrl_fsm_;
                const AguNdCtrl &c = in.i_agu_nd_ctrl;
                if (st == RESIDUAL_ST1 && c.src_on && c.src_port == AguNdCtrl::VBANK0) w.agu_nd_sel[2] = 1;
                if (st == RESIDUAL_ST2 && c.dst_on && c.dst_port == AguNdCtrl::VBANK0) w.agu_nd_sel[2] = 2;
                if (w.agu_nd_sel[2]) w.vlsu_in.i_vbank0_agu_mode = AGU_ND;
            }

            // ---- 9. Populate Top-Level Outputs -------------------------------------------------------------
            Out o;
            o.o_psum_ready = true;
            o.o_softmax_lut_init_done = is_lut_recip_init_ && is_lut_exp_init_;
            const uint32_t max_tiles = (in.i_num_tile > 0 ? in.i_num_tile : 1);
            o.o_softmax_pipe_done = ((pipe_ctrl_fsm_ == SOFTMAX_ST3) && (tile_cnt_ == max_tiles - 1) && w.softmax_token_last) ||
                                    ((pipe_ctrl_fsm_ == LAYERNORM_ST5) && w.layernorm_token_last);
            o.o_layernorm_lut_init_done = is_lut_rsqrt_init_;
            o.o_layernorm_pipe_done = (pipe_ctrl_fsm_ == LAYERNORM_ST5) && w.layernorm_token_last;
            o.o_gemm_fused_lut_init_done = is_lut_act_init_;
            if (in.i_gemm_stream)
            {
                o.o_gemm_fused_pipe_done = (pipe_ctrl_fsm_ == GEMM_FUSED_ST1) && ((in.i_gemm_stream_stop && !stream_busy()) || stream_exit(in));
                o.o_gemm_fused_valid = (pipe_ctrl_fsm_ == GEMM_FUSED_ST1) && s8_v_;
                o.o_gemm_fused_addr = s8_d_.addr;
                o.o_gemm_fused_mask = s8_v_ ? s8_d_.mask : 0u;
                o.o_gemm_fused_busy = stream_busy();
            }
            else
            {
                o.o_gemm_fused_pipe_done = (pipe_ctrl_fsm_ == GEMM_FUSED_ST1) && (tile_cnt_ == max_tiles - 1) && w.gemm_fused_token_last;
                o.o_gemm_fused_valid = (pipe_ctrl_fsm_ == GEMM_FUSED_ST1) && (in.i_psum_valid || vbank2_rd_delay_[1]);
            }
            for (int l = 0; l < GVU_LANES; l++)
            {
                const uint8_t byte_out = in.i_gemm_skip_act ? static_cast<uint8_t>(w.clamp_out[l] & 0xFFu)
                                                            : w.lut_out.o_8b_lookup_data[l];
                o.o_gemm_fused_data[l] = static_cast<int8_t>(byte_out);
            }
            o.o_residual_pipe_done = (pipe_ctrl_fsm_ == RESIDUAL_ST2) && (tile_cnt_ == max_tiles - 1) && w.residual_st2_done;
            o.o_residual_valid = (pipe_ctrl_fsm_ == RESIDUAL_ST2) && vbank2_rd_delay_[1];
            for (int l = 0; l < GVU_LANES; l++)
            {
                o.o_residual_data[l] = static_cast<int8_t>(w.clamp_out[l] & 0xFFu);
            }
            o.o_maxpool_pipe_done = pool && w.vlsu_out.o_pooling_last_wr;
            o.d_vbank0_en = w.sp_in.i_vbank0_en;
            o.d_vbank0_addr = w.sp_in.i_vbank0_addr;
            o.d_vbank2_wen_1 = w.sp_in.i_vbank2_wen_1;
            o.d_vbank2_addr_1 = w.sp_in.i_vbank2_addr_1;
            for (size_t b = 0; b < w.sp_in.i_vbank2_wmask_1.size() && b < 4; b++) o.d_vbank2_wmask_1 |= static_cast<uint32_t>(w.sp_in.i_vbank2_wmask_1[b]) << (8 * b);
            for (size_t l = 0; l < static_cast<size_t>(GVU_LANES) && l < w.sp_in.i_vbank2_wdata_1.size(); l++) o.d_vbank2_wdata_1[l] = w.sp_in.i_vbank2_wdata_1[l];
            o.o_gvu_pipe_ctrl_state = pipe_ctrl_fsm_;
            o.o_scratch_dma_rdata = SpBytes(P_.DMA_DW / 8, 0);

            return o;
        }
    };

} // namespace rtl_ref_gvu

// ---- SystemC Module Wrapper (available when SystemC is included) -------------------------------------------
#if defined(SYSTEMC_INCLUDED) || defined(SC_MODULE) || defined(HAVE_SYSTEMC)

namespace rtl_ref_gvu
{
    class ScGvuTop : public sc_core::sc_module
    {
    public:
        // Clock & Reset
        sc_core::sc_in<bool> i_clk{"i_clk"};
        sc_core::sc_in<bool> i_rstn{"i_rstn"};

        // Pipeline control
        sc_core::sc_in<bool> i_pipeline_en{"i_pipeline_en"};
        sc_core::sc_in<uint32_t> i_gvu_op{"i_gvu_op"};

        // Systolic Array interface
        sc_core::sc_in<bool> i_psum_valid{"i_psum_valid"};
        sc_core::sc_out<bool> o_psum_ready{"o_psum_ready"};
        sc_core::sc_in<uint32_t> i_psum_data{"i_psum_data"};

        // Softmax & Attention parameters
        typedef sc_core::sc_port<sc_core::sc_signal_in_if<uint32_t>, 1, sc_core::SC_ZERO_OR_MORE_BOUND> OptInUint;
        OptInUint i_num_tile{"i_num_tile"};
        sc_core::sc_in<uint32_t> i_token_len{"i_token_len"};
        sc_core::sc_in<int32_t> i_attn_z{"i_attn_z"};
        sc_core::sc_in<int32_t> i_attn_scale{"i_attn_scale"};
        sc_core::sc_in<uint32_t> i_attn_shift{"i_attn_shift"};

        // Status outputs
        sc_core::sc_out<bool> o_softmax_lut_init_done{"o_softmax_lut_init_done"};
        sc_core::sc_out<bool> o_softmax_pipe_done{"o_softmax_pipe_done"};
        // These two outputs may stay unbound (a softmax-only bench): the bind policy is a template argument of the port,
        // which also builds with SystemC 2.3.x.
        typedef sc_core::sc_port<sc_core::sc_signal_inout_if<bool>, 1, sc_core::SC_ZERO_OR_MORE_BOUND> OptOutBool;
        OptOutBool o_layernorm_lut_init_done{"o_layernorm_lut_init_done"};
        OptOutBool o_layernorm_pipe_done{"o_layernorm_pipe_done"};
        OptOutBool o_gemm_fused_lut_init_done{"o_gemm_fused_lut_init_done"};
        OptOutBool o_gemm_fused_pipe_done{"o_gemm_fused_pipe_done"};
        OptOutBool o_residual_pipe_done{"o_residual_pipe_done"};
        sc_core::sc_out<uint32_t> o_gvu_pipe_ctrl_state{"o_gvu_pipe_ctrl_state"};

        // Buffer selects
        sc_core::sc_in<bool> i_sbank_buf_sel{"i_sbank_buf_sel"};
        sc_core::sc_in<bool> i_vbank0_buf_sel{"i_vbank0_buf_sel"};
        sc_core::sc_in<bool> i_vbank1_buf_sel{"i_vbank1_buf_sel"};
        sc_core::sc_in<bool> i_vbank2_buf_sel{"i_vbank2_buf_sel"};

        // Scratchpad DMA access
        sc_core::sc_in<bool> i_scratch_dma_en{"i_scratch_dma_en"};
        sc_core::sc_in<bool> i_scratch_dma_wen{"i_scratch_dma_wen"};
        sc_core::sc_in<uint32_t> i_scratch_dma_addr{"i_scratch_dma_addr"};

        SC_HAS_PROCESS(ScGvuTop);

        explicit ScGvuTop(sc_core::sc_module_name name, const GvuTopParams &p = GvuTopParams())
            : sc_core::sc_module(name), core_(p)
        {
            SC_METHOD(clock_proc);
            sensitive << i_clk.pos();
            async_reset_signal_is(i_rstn, false);

            SC_METHOD(comb_proc);
            sensitive << i_pipeline_en << i_gvu_op << i_token_len << i_attn_z
                      << i_attn_scale << i_attn_shift << i_sbank_buf_sel << i_vbank0_buf_sel
                      << i_vbank1_buf_sel << i_vbank2_buf_sel << i_psum_valid << state_changed_;
        }

        GvuTop &core() { return core_; }
        const GvuTop &core() const { return core_; }

        void config_gemm_fused(int32_t scale, int8_t shift, int8_t zp,
                               bool per_channel = false, bool skip_act = false,
                               bool skip_zp = false, bool out_int16 = false, RoundMode round = RND_NEAR_AWAY,
                               NarrowMode narrow = NARROW_SATURATE)
        {
            gemm_scale_ = scale; gemm_shift_ = shift; gemm_zp_ = zp; gemm_round_ = round; gemm_narrow_ = narrow;
            gemm_per_channel_ = per_channel; gemm_skip_act_ = skip_act;
            gemm_skip_zp_ = skip_zp; gemm_out_int16_ = out_int16;
        }

        void set_psum_vec(const std::array<int32_t, GVU_LANES> &vec) { psum_vec_ = vec; }
        bool out_gemm_fused_valid() const { return last_out_.o_gemm_fused_valid; }
        const std::array<int8_t, GVU_LANES> &out_gemm_fused_data() const { return last_out_.o_gemm_fused_data; }

        void config_residual(int32_t scale_a, uint8_t shift_a, int8_t zp_a,
                             int32_t scale_b, uint8_t shift_b, int8_t zp_b,
                             int32_t scale_out, uint8_t shift_out, int8_t zp_out,
                             RoundMode round = RND_NEAR_UP, bool out_int16 = false,
                             unsigned a_row = 0, unsigned b_row = 0, unsigned sum_row = 0)
        {
            res_scale_a_ = scale_a; res_shift_a_ = shift_a; res_zp_a_ = zp_a;
            res_scale_b_ = scale_b; res_shift_b_ = shift_b; res_zp_b_ = zp_b;
            res_scale_out_ = scale_out; res_shift_out_ = shift_out; res_zp_out_ = zp_out;
            res_round_ = round; res_out_int16_ = out_int16;
            res_a_row_ = a_row; res_b_row_ = b_row; res_sum_row_ = sum_row;
        }

        bool out_residual_valid() const { return last_out_.o_residual_valid; }
        const std::array<int8_t, GVU_LANES> &out_residual_data() const { return last_out_.o_residual_data; }

    private:
        GvuTop core_;
        GvuTop::Out last_out_{};
        int32_t gemm_scale_{1}; int8_t gemm_shift_{0}; int8_t gemm_zp_{0};
        bool gemm_per_channel_{false}; bool gemm_skip_act_{false};
        bool gemm_skip_zp_{false}; bool gemm_out_int16_{false}; RoundMode gemm_round_{RND_NEAR_AWAY};
        NarrowMode gemm_narrow_{NARROW_SATURATE};
        std::array<int32_t, GVU_LANES> psum_vec_{};
        int32_t res_scale_a_{1}; uint8_t res_shift_a_{0}; int8_t res_zp_a_{0};
        int32_t res_scale_b_{1}; uint8_t res_shift_b_{0}; int8_t res_zp_b_{0};
        int32_t res_scale_out_{1}; uint8_t res_shift_out_{0}; int8_t res_zp_out_{0};
        RoundMode res_round_{RND_NEAR_UP}; bool res_out_int16_{false};
        unsigned res_a_row_{0}, res_b_row_{0}, res_sum_row_{0};
        // The outputs have one driver, comb_proc; clock_proc advances the state and wakes it (a signal written by two
        // processes is an error in SystemC).
        sc_core::sc_event state_changed_;

        GvuTop::In sample_in() const
        {
            GvuTop::In in;
            in.i_pipeline_en = i_pipeline_en.read();
            in.i_gvu_op = static_cast<GvuOpcode>(i_gvu_op.read());
            in.i_psum_valid = i_psum_valid.read();
            in.i_psum_data = i_psum_data.read();
            in.i_num_tile = (i_num_tile.size() > 0) ? i_num_tile->read() : 1u;
            in.i_token_len = i_token_len.read();
            in.i_attn_z = static_cast<int8_t>(i_attn_z.read());
            in.i_attn_scale = i_attn_scale.read();
            in.i_attn_shift = static_cast<uint8_t>(i_attn_shift.read());
            in.i_sbank_buf_sel = i_sbank_buf_sel.read();
            in.i_vbank0_buf_sel = i_vbank0_buf_sel.read();
            in.i_vbank1_buf_sel = i_vbank1_buf_sel.read();
            in.i_vbank2_buf_sel = i_vbank2_buf_sel.read();
            in.i_scratch_dma_en = i_scratch_dma_en.read();
            in.i_scratch_dma_wen = i_scratch_dma_wen.read();
            in.i_scratch_dma_addr = i_scratch_dma_addr.read();
            in.i_gemm_scale = gemm_scale_;
            in.i_gemm_shift = gemm_shift_;
            in.i_gemm_zp = gemm_zp_;
            in.i_gemm_per_channel = gemm_per_channel_;
            in.i_gemm_round = gemm_round_;
            in.i_gemm_narrow = gemm_narrow_;
            in.i_gemm_skip_act = gemm_skip_act_;
            in.i_gemm_skip_zp = gemm_skip_zp_;
            in.i_gemm_out_int16 = gemm_out_int16_;
            in.i_psum_vec = psum_vec_;
            in.i_res_scale_a = res_scale_a_;
            in.i_res_shift_a = res_shift_a_;
            in.i_res_zp_a = res_zp_a_;
            in.i_res_scale_b = res_scale_b_;
            in.i_res_shift_b = res_shift_b_;
            in.i_res_zp_b = res_zp_b_;
            in.i_res_scale_out = res_scale_out_;
            in.i_res_shift_out = res_shift_out_;
            in.i_res_zp_out = res_zp_out_;
            in.i_res_round = res_round_;
            in.i_res_out_int16 = res_out_int16_;
            in.i_res_a_row = res_a_row_;
            in.i_res_b_row = res_b_row_;
            in.i_res_sum_row = res_sum_row_;
            return in;
        }

        void comb_proc()
        {
            const GvuTop::Out o = core_.eval(sample_in());
            last_out_ = o;
            o_psum_ready.write(o.o_psum_ready);
            o_softmax_lut_init_done.write(o.o_softmax_lut_init_done);
            o_softmax_pipe_done.write(o.o_softmax_pipe_done);
            if (o_layernorm_lut_init_done.get_interface() != nullptr)
                o_layernorm_lut_init_done->write(o.o_layernorm_lut_init_done);
            if (o_layernorm_pipe_done.get_interface() != nullptr)
                o_layernorm_pipe_done->write(o.o_layernorm_pipe_done);
            if (o_gemm_fused_lut_init_done.get_interface() != nullptr)
                o_gemm_fused_lut_init_done->write(o.o_gemm_fused_lut_init_done);
            if (o_gemm_fused_pipe_done.get_interface() != nullptr)
                o_gemm_fused_pipe_done->write(o.o_gemm_fused_pipe_done);
            if (o_residual_pipe_done.get_interface() != nullptr)
                o_residual_pipe_done->write(o.o_residual_pipe_done);
            o_gvu_pipe_ctrl_state.write(static_cast<uint32_t>(o.o_gvu_pipe_ctrl_state));
        }

        void clock_proc()
        {
            if (!i_rstn.read())
            {
                GvuTop::In empty_in;
                core_.tick(empty_in, false);
                state_changed_.notify(sc_core::SC_ZERO_TIME);
                return;
            }
            core_.tick(sample_in(), true);
            state_changed_.notify(sc_core::SC_ZERO_TIME);
        }
    };
} // namespace rtl_ref_gvu

#endif // SYSTEMC_INCLUDED

#endif // RTL_REF_GVU_TOP_H
