#pragma once
//
// 1:1 port of RTL sauria_core/psm/psm_rdata_manager.sv.
//
// This is the READ-BACK counterpart of psm_wdata_manager.h: where the wdata
// manager packs a Y-wide psum vector OUT towards SRAM-C word-slots, this one
// gathers SRAM-C word-slots back IN and reassembles them into a Y-wide vector
// pushed towards the FIFO that feeds old partial sums back into the array
// (the i_c/o_c preload chain of sa_processing_element.sv). It is the datapath
// K-tiling / psum-chaining needs (the SRAM-C preload path).
//
// SRAMC_N for this project = MEMC_W/OC_W = Y_DIM (traced in from
// hw_versions.py:458 -- NOT the un-overridden sauria_logic.sv default of 2).
// Masks are uint64_t rather than the uint32_t used by psm_wdata_manager.h
// because Y_DIM is 64 in the 64x64 geometry, which a 32-bit mask cannot hold.
//
#include <cstdint>

namespace sauria_rtl
{
using namespace sauria; // A1 port: shared vector/mask types stay in ::sauria, class bodies move to ::sauria_rtl.

    // Y_DIM: number of row registers (RTL's Y). SRAMC_N: number of input
    // word-slots on the SRAM-C bus (RTL's SRAMC_N = SRAMC_W/OC_W). VecT: the
    // Y_DIM-wide vector type (psum_vector_t<Y_DIM,T_PSUM>), used for BOTH the
    // input bus (indexed 0..SRAMC_N-1, RTL's sram_elements reshape) and the
    // output bus (indexed 0..Y_DIM-1, RTL's BUFF_W buff_obus packing).
    // ElemT: per-element type (T_PSUM).
    template <int Y_DIM, int SRAMC_N, typename VecT, typename ElemT>
    class PsmRdataManager
    {
    public:
        struct Inputs
        {
            VecT sramc_data;          // i_sramc_data (latency 2 wrt address)
            uint64_t rows_active = 0; // i_rows_active, bit j = row j active
            bool feeder_en = false;   // i_feeder_en
            bool clearbuff = false;   // i_clearbuff
            uint64_t mask = 0;        // i_mask, bit i = word-slot i active
        };

        struct Outputs
        {
            bool fifo_push = false; // o_fifo_push
            VecT fifo_din;          // o_fifo_din
        };

        void reset()
        {
            mask_q1_ = 0;
            mask_q2_ = 0;
            buff_active_q_ = 0;
            for (int j = 0; j < Y_DIM; j++)
                buff_q_[j] = ElemT{};
        }

        Outputs tick(const Inputs &in)
        {
            // ---------- Phase 1: combinational (pre-tick register state) ----------

            // FIFO push logic (RTL lines 204-213). Reads buff_active_q, the
            // PRE-tick register -- and the interconnect block below consumes
            // this same combinational value, so it must be computed first.
            const bool fifo_push =
                in.feeder_en && (buff_active_q_ == ALL_ROWS_);

            // Unified Data Interconnect (RTL lines 134-180). The RTL
            // always_comb mutates buff_active_d and map_flags DURING the
            // nested loop and re-reads them in the loop condition, so the
            // mutation order below is load-bearing, not incidental.
            ElemT buff_d[Y_DIM];
            for (int j = 0; j < Y_DIM; j++)
                buff_d[j] = buff_q_[j];

            uint64_t buff_active_d = buff_active_q_;
            uint64_t map_flags = 0;

            if (in.clearbuff)
            {
                buff_active_d = 0;
            }
            else if (in.feeder_en)
            {
                // A push empties the (previously) selected buffer.
                if (fifo_push)
                    buff_active_d = 0;

                for (int i = 0; i < SRAMC_N; i++)
                {
                    if (!((mask_q2_ >> i) & 1u))
                        continue;

                    for (int j = 0; j < Y_DIM; j++)
                    {
                        if (!((in.rows_active >> j) & 1u))
                            continue;

                        if (!((buff_active_d >> j) & 1u) &&
                            !((map_flags >> i) & 1u))
                        {
                            buff_active_d |= (uint64_t(1) << j);
                            map_flags |= (uint64_t(1) << i);
                            buff_d[j] = (i < Y_DIM) ? in.sramc_data[i] : ElemT{};
                        }
                    }
                }
            }

            // All unselected positions are always forced to 1 (RTL line 178).
            // Applies in every branch above, including clearbuff.
            buff_active_d |= (~in.rows_active) & ALL_ROWS_;

            // ---------- Outputs (RTL lines 219-220) ----------
            // o_fifo_din reads buff_obus, i.e. the PRE-tick buff_q, gated to
            // zero when the feeder is stalled.
            Outputs out;
            out.fifo_push = fifo_push;
            if (in.feeder_en)
            {
                for (int j = 0; j < Y_DIM; j++)
                    out.fifo_din[j] = buff_q_[j];
            }

            // ---------- Phase 2: commit ----------
            // pingpong_reg (RTL lines 182-198) has NO enable term -- it
            // commits buff_d/buff_active_d on every clock. Holding is done by
            // buff_d defaulting to buff_q above, not by gating the register.
            for (int j = 0; j < Y_DIM; j++)
                buff_q_[j] = buff_d[j];
            buff_active_q_ = buff_active_d;

            // mask_reg (RTL lines 118-128): two stages, both gated by
            // i_feeder_en. mask_q2 takes the PRE-tick mask_q1, so snapshot it
            // before the overwrite.
            if (in.feeder_en)
            {
                const uint64_t pre_mask_q1 = mask_q1_;
                mask_q1_ = in.mask;
                mask_q2_ = pre_mask_q1;
            }

            return out;
        }

    private:
        static constexpr uint64_t all_ones(int n)
        {
            return (n >= 64) ? ~uint64_t(0) : ((uint64_t(1) << n) - 1);
        }
        static constexpr uint64_t ALL_ROWS_ = all_ones(Y_DIM);

        uint64_t mask_q1_{0};
        uint64_t mask_q2_{0};
        uint64_t buff_active_q_{0};
        ElemT buff_q_[Y_DIM]{};
    };

} // namespace sauria_rtl
