// SPDX-License-Identifier: Apache-2.0
// Statistics publication and readout of the control unit: AEC (HAS §6.21),
// AWB (§6.22), AF (§6.23), §9.5 frame association. Decisions: DEC-14 (soft
// reset), DEC-29/DEC-30 (single in-place zone/histogram memory read with the
// partial sums of the frame in progress; FRAME_ID bound to the frame),
// ALG-AEC-01..06, ALG-AWB-01..04, ALG-AF-01..02, ALG-STAT-01..10.

#include "control/control_unit.h"
#include "fx1_isp/fx1_isp_csr.h"

namespace cdc::components::fx1_isp {

namespace {

std::uint32_t fld(std::uint32_t reg, std::uint32_t mask, std::uint32_t shift) { return (reg & mask) >> shift; }

}  // namespace

void control_unit::stats_reset(bool external) {
   // Soft reset clears published results and the zone/histogram memories
   // (they read the CSR reset value 0 again until the next enabled SOF) but
   // keeps configuration and the frame counter (DEC-14, ALG-STAT-07); i_rst_n
   // clears everything. The memories keep their size: only their contents
   // are made invisible.
   if (external) {
      aec_active_ = aec_config{};
      awb_sof_ = awb_config{};
      af_en_sof_ = false;
      frame_counter_ = 0;
      frame_id_ = 0;
   }
   aec_pub_ = aec_result{};
   awb_pub_ = awb_result{};
   af_pub_ = af_result{};
   aec_mem_live_ = false;
   awb_mem_live_ = false;
}

void control_unit::stats_sof() {
   frame_id_ = ++frame_counter_;  // counts frames started, bound to this frame (ALG-STAT-01)
   const register_file &r = regs_;
   // AEC: the shadow set is transferred at the SOF after a commit (#296).
   if (r.peek(FX1_ISP_AEC_CTRL_OFFSET) & FX1_ISP_AEC_CTRL_COMMIT_MASK) {
      const std::uint32_t zc = r.peek(FX1_ISP_AEC_ZONE_CFG_OFFSET), zs = r.peek(FX1_ISP_AEC_ZONE_SIZE_OFFSET);
      const std::uint32_t cl = r.peek(FX1_ISP_AEC_SAMPLE_CLIP_OFFSET), th = r.peek(FX1_ISP_AEC_THRESH_OFFSET);
      aec_active_.en = r.peek(FX1_ISP_AEC_CTRL_OFFSET) & FX1_ISP_AEC_CTRL_EN_MASK;
      aec_active_.nx = fld(zc, FX1_ISP_AEC_ZONE_CFG_NUM_ZONE_X_MASK, FX1_ISP_AEC_ZONE_CFG_NUM_ZONE_X_SHIFT);
      aec_active_.ny = fld(zc, FX1_ISP_AEC_ZONE_CFG_NUM_ZONE_Y_MASK, FX1_ISP_AEC_ZONE_CFG_NUM_ZONE_Y_SHIFT);
      aec_active_.zw = fld(zs, FX1_ISP_AEC_ZONE_SIZE_ZONE_WIDTH_MASK, FX1_ISP_AEC_ZONE_SIZE_ZONE_WIDTH_SHIFT);
      aec_active_.zh = fld(zs, FX1_ISP_AEC_ZONE_SIZE_ZONE_HEIGHT_MASK, FX1_ISP_AEC_ZONE_SIZE_ZONE_HEIGHT_SHIFT);
      aec_active_.min_clip = fld(cl, FX1_ISP_AEC_SAMPLE_CLIP_SAMPLE_MIN_CLIP_MASK, FX1_ISP_AEC_SAMPLE_CLIP_SAMPLE_MIN_CLIP_SHIFT);
      aec_active_.max_clip = fld(cl, FX1_ISP_AEC_SAMPLE_CLIP_SAMPLE_MAX_CLIP_MASK, FX1_ISP_AEC_SAMPLE_CLIP_SAMPLE_MAX_CLIP_SHIFT);
      aec_active_.th_ue = fld(th, FX1_ISP_AEC_THRESH_TH_UE_MASK, FX1_ISP_AEC_THRESH_TH_UE_SHIFT);
      aec_active_.th_oe = fld(th, FX1_ISP_AEC_THRESH_TH_OE_MASK, FX1_ISP_AEC_THRESH_TH_OE_SHIFT);
      regs_.hw_clear(FX1_ISP_AEC_CTRL_OFFSET, FX1_ISP_AEC_CTRL_COMMIT_MASK);  // self-clearing
   }
   // AWB: configuration sampled at SOF (HAS §9.4.3, ALG-STAT-05).
   const std::uint32_t ac = r.peek(FX1_ISP_AWB_CTRL_OFFSET);
   awb_sof_.en = ac & FX1_ISP_AWB_CTRL_EN_MASK;
   awb_sof_.nx = fld(ac, FX1_ISP_AWB_CTRL_NUM_ZONE_X_MASK, FX1_ISP_AWB_CTRL_NUM_ZONE_X_SHIFT);
   awb_sof_.ny = fld(ac, FX1_ISP_AWB_CTRL_NUM_ZONE_Y_MASK, FX1_ISP_AWB_CTRL_NUM_ZONE_Y_SHIFT);
   awb_sof_.under = r.peek(FX1_ISP_AWB_UNDEREXPOSED_LIMIT_OFFSET) & FX1_ISP_AWB_UNDEREXPOSED_LIMIT_UNDEREXPOSED_LIMIT_MASK;
   awb_sof_.sat = r.peek(FX1_ISP_AWB_SATURATION_LIMIT_OFFSET) & FX1_ISP_AWB_SATURATION_LIMIT_SATURATION_LIMIT_MASK;
   af_en_sof_ = r.peek(FX1_ISP_AF_CTRL_OFFSET) & FX1_ISP_AF_CTRL_EN_MASK;
   // Context tags latched at SOF (HAS §9.5.1, ALG-STAT-04).
   aec_ctx_ = r.peek(FX1_ISP_AEC_CONTEXT_ID_OFFSET);
   awb_ctx_ = r.peek(FX1_ISP_AWB_CONTEXT_ID_OFFSET);
   af_ctx_ = r.peek(FX1_ISP_AF_CONTEXT_ID_OFFSET);
   // Enabled blocks start measuring: busy, and the in-place zone/histogram
   // memory restarts; the pipeline updates it row by row (DEC-30).
   if (aec_active_.en) {
      aec_mem_.restart(aec_active_);
      aec_mem_live_ = true;
      regs_.hw_set(FX1_ISP_AEC_STATUS_OFFSET, FX1_ISP_AEC_STATUS_BUSY_MASK, current_cycle_);
   }
   if (awb_sof_.en) {
      awb_mem_.restart(awb_sof_);
      awb_mem_live_ = true;
   }
   if (af_en_sof_) {
      regs_.hw_set(FX1_ISP_AF_STATUS_OFFSET, FX1_ISP_AF_STATUS_BUSY_MASK, current_cycle_);
   }
}

void control_unit::blocks_busy(bool busy) {
   // Frame granularity: every pixel block holds pixels from the accepted SOF
   // until the pipeline has emitted the frame's last row; a bypassed block
   // still carries the stream (DEC-38).
   static constexpr std::uint32_t regs[] = {
      FX1_ISP_DEMOSAIC_STATUS_OFFSET, FX1_ISP_CCM_STATUS_OFFSET, FX1_ISP_GAMMA_STATUS_OFFSET,
      FX1_ISP_GTM_STATUS_OFFSET,      FX1_ISP_NR_2D_STATUS_OFFSET, FX1_ISP_EE_STATUS_OFFSET,
      FX1_ISP_CNF_STATUS_OFFSET,      FX1_ISP_RESIZER_STATUS_OFFSET,
   };
   static_assert(FX1_ISP_DEMOSAIC_STATUS_BUSY_MASK == 1u && FX1_ISP_CCM_STATUS_BUSY_MASK == 1u &&
                     FX1_ISP_GAMMA_STATUS_BUSY_MASK == 1u && FX1_ISP_GTM_STATUS_BUSY_MASK == 1u &&
                     FX1_ISP_NR_2D_STATUS_BUSY_MASK == 1u && FX1_ISP_EE_STATUS_BUSY_MASK == 1u &&
                     FX1_ISP_CNF_STATUS_BUSY_MASK == 1u && FX1_ISP_RESIZER_STATUS_BUSY_MASK == 1u,
                 "busy is bit 0 of every block status register");
   for (const std::uint32_t r : regs) {
      if (busy) {
         regs_.hw_set(r, 1u, current_cycle_);
      } else {
         regs_.hw_clear(r, 1u);
      }
   }
}

void control_unit::stats_abort() {
   regs_.hw_clear(FX1_ISP_AEC_STATUS_OFFSET, FX1_ISP_AEC_STATUS_BUSY_MASK);
   regs_.hw_clear(FX1_ISP_AF_STATUS_OFFSET, FX1_ISP_AF_STATUS_BUSY_MASK);
}

void control_unit::stats_publish(const aec_result *aec, const awb_result *awb, const af_result *af, bool af_full) {
   // Tap order AEC -> AWB -> AF; each publication sets its done bit and
   // raises stats_ready_irq, also when the done bit was still set from an
   // unread earlier frame (HAS Table 9-2, §6.21.6.5, ALG-STAT-11).
   const auto published = [this](std::uint32_t status, std::uint32_t done) {
      hw_set(status, done, current_cycle_);
      regs_.hw_set(FX1_ISP_COMMON_IRQ_STATUS_OFFSET, FX1_ISP_COMMON_IRQ_STATUS_STATS_READY_IRQ_MASK, current_cycle_);
   };
   if (aec) {
      aec_pub_ = *aec;  // the zone/histogram memory is already complete in place
      regs_.hw_write(FX1_ISP_AEC_FRAME_ID_OFFSET, 0xFFFFFFFFu, frame_id_);
      regs_.hw_write(FX1_ISP_AEC_RESULT_CONTEXT_ID_OFFSET, 0xFFFFFFFFu, aec_ctx_);
      regs_.hw_clear(FX1_ISP_AEC_STATUS_OFFSET, FX1_ISP_AEC_STATUS_BUSY_MASK);
      published(FX1_ISP_AEC_STATUS_OFFSET, FX1_ISP_AEC_STATUS_STAT_DONE_MASK);
   }
   if (awb) {
      awb_pub_ = *awb;
      const auto split = [&](std::uint32_t h_off, std::uint32_t l_off, std::uint64_t v) {
         regs_.hw_write(h_off, 0x7u, static_cast<std::uint32_t>((v >> 32) & 0x7u));
         regs_.hw_write(l_off, 0xFFFFFFFFu, static_cast<std::uint32_t>(v));
      };
      split(FX1_ISP_AWB_GLOBAL_SUM_R_H_OFFSET, FX1_ISP_AWB_GLOBAL_SUM_R_L_OFFSET, awb->r);
      split(FX1_ISP_AWB_GLOBAL_SUM_G_H_OFFSET, FX1_ISP_AWB_GLOBAL_SUM_G_L_OFFSET, awb->g);
      split(FX1_ISP_AWB_GLOBAL_SUM_B_H_OFFSET, FX1_ISP_AWB_GLOBAL_SUM_B_L_OFFSET, awb->b);
      regs_.hw_write(FX1_ISP_AWB_GLOBAL_COUNT_OFFSET, FX1_ISP_AWB_GLOBAL_COUNT_GLOBAL_COUNT_MASK, awb->n);
      regs_.hw_write(FX1_ISP_AWB_FRAME_ID_OFFSET, 0xFFFFFFFFu, frame_id_);
      regs_.hw_write(FX1_ISP_AWB_RESULT_CONTEXT_ID_OFFSET, 0xFFFFFFFFu, awb_ctx_);
      published(FX1_ISP_AWB_STATUS_OFFSET, FX1_ISP_AWB_STATUS_STAT_DONE_MASK);
   }
   // AF publishes iff EN is set at the end of the frame (ALG-AF-01);
   // score_valid marks a frame measured from SOF to EOF (ALG-AF-02).
   regs_.hw_clear(FX1_ISP_AF_STATUS_OFFSET, FX1_ISP_AF_STATUS_BUSY_MASK);
   if (af && (regs_.peek(FX1_ISP_AF_CTRL_OFFSET) & FX1_ISP_AF_CTRL_EN_MASK)) {
      af_pub_ = *af;
      regs_.hw_write(FX1_ISP_AF_FRAME_ID_OFFSET, 0xFFFFFFFFu, frame_id_);
      regs_.hw_write(FX1_ISP_AF_RESULT_CONTEXT_ID_OFFSET, 0xFFFFFFFFu, af_ctx_);
      regs_.hw_write(FX1_ISP_AF_STATUS_OFFSET, FX1_ISP_AF_STATUS_SCORE_VALID_MASK,
                     af_full ? FX1_ISP_AF_STATUS_SCORE_VALID_MASK : 0u);
      published(FX1_ISP_AF_STATUS_OFFSET, FX1_ISP_AF_STATUS_FRAME_DONE_MASK);
   }
}

void control_unit::stats_install_hooks() {
   // ---- AEC readout multiplexers (#319-#342) ----
   auto aec_chan = [this] {
      return fld(regs_.peek(FX1_ISP_AEC_CHANNEL_SEL_OFFSET), FX1_ISP_AEC_CHANNEL_SEL_CHANNEL_SEL_MASK,
                 FX1_ISP_AEC_CHANNEL_SEL_CHANNEL_SEL_SHIFT);
   };
   regs_.on_read(FX1_ISP_AEC_GLOBAL_SUM_LO_OFFSET, [this, aec_chan](std::uint32_t) {
      return static_cast<std::uint32_t>(aec_pub_.gsum[aec_chan()]);
   });
   regs_.on_read(FX1_ISP_AEC_GLOBAL_SUM_HI_OFFSET, [this, aec_chan](std::uint32_t) {
      return static_cast<std::uint32_t>((aec_pub_.gsum[aec_chan()] >> 32) & 1u);
   });
   regs_.on_read(FX1_ISP_AEC_GLOBAL_COUNT_OFFSET, [this, aec_chan](std::uint32_t) { return aec_pub_.gcnt[aec_chan()] & FX1_ISP_AEC_GLOBAL_COUNT_GLOBAL_COUNT_MASK; });
   // Zones read the memory as it is now (DEC-30): during an enabled frame a
   // zone already written holds its partial sums, one not yet written the
   // empty value (0, min 4095, max 0), as do addresses beyond the grid and an
   // illegal grid. Before the first enabled SOF after a reset every field
   // reads the CSR reset value 0 (ALG-AEC-03/04).
   auto aec_zone = [this]() -> long {
      const std::uint32_t z = regs_.peek(FX1_ISP_AEC_ZONE_ADDR_OFFSET) & FX1_ISP_AEC_ZONE_ADDR_ZONE_ADDR_MASK;
      if (!aec_mem_live_ || !aec_mem_.zones || z >= aec_mem_.nx * aec_mem_.ny) {
         return -1;
      }
      return static_cast<long>(z);
   };
   regs_.on_read(FX1_ISP_AEC_ZONE_SUM_OFFSET, [this, aec_zone, aec_chan](std::uint32_t) {
      const long z = aec_zone();
      return z < 0 ? 0u : aec_mem_.zsum[static_cast<std::size_t>(z)][aec_chan()] & FX1_ISP_AEC_ZONE_SUM_ZONE_SUM_MASK;
   });
   regs_.on_read(FX1_ISP_AEC_ZONE_COUNT_OFFSET, [this, aec_zone, aec_chan](std::uint32_t) {
      const long z = aec_zone();
      return z < 0 ? 0u : aec_mem_.zcnt[static_cast<std::size_t>(z)][aec_chan()] & FX1_ISP_AEC_ZONE_COUNT_ZONE_COUNT_MASK;
   });
   regs_.on_read(FX1_ISP_AEC_ZONE_GREEN_OE_UE_OFFSET, [this, aec_zone](std::uint32_t) {
      const long z = aec_zone();
      if (z < 0) {
         return 0u;
      }
      const std::size_t i = static_cast<std::size_t>(z);
      return ((aec_mem_.oe[i] & 0xFFFFu) << 16) | (aec_mem_.ue[i] & 0xFFFFu);
   });
   regs_.on_read(FX1_ISP_AEC_ZONE_GREEN_MIN_MAX_OFFSET, [this, aec_zone](std::uint32_t) {
      if (!aec_mem_live_) {
         return 0u;  // CSR reset value
      }
      const long z = aec_zone();
      if (z < 0) {
         return 4095u;  // empty: min 4095, max 0
      }
      return (aec_mem_.gmax[static_cast<std::size_t>(z)] << 12) | aec_mem_.gmin[static_cast<std::size_t>(z)];
   });
   regs_.on_read(FX1_ISP_AEC_HIST_DATA_OFFSET, [this](std::uint32_t) {
      const std::uint32_t b = regs_.peek(FX1_ISP_AEC_HIST_ADDR_OFFSET) & FX1_ISP_AEC_HIST_ADDR_HIST_ADDR_MASK;
      return aec_mem_live_ ? aec_mem_.hist[b] & FX1_ISP_AEC_HIST_DATA_HIST_COUNT_MASK : 0u;
   });

   // ---- AWB zone readout (#371-#386): the memory as it is now; 0 when not
   // written in the current frame, beyond the grid or before the first SOF ----
   auto awb_zone = [this]() -> long {
      const std::uint32_t z = regs_.peek(FX1_ISP_AWB_ZONE_ADDR_OFFSET) & FX1_ISP_AWB_ZONE_ADDR_ZONE_ADDR_MASK;
      if (!awb_mem_live_ || !awb_mem_.zones || z >= awb_mem_.nx * awb_mem_.ny) {
         return -1;
      }
      return static_cast<long>(z);
   };
   const struct {
      std::uint32_t h, l;
      unsigned c;
   } awb_zone_regs[] = {{FX1_ISP_AWB_ZONE_SUM_R_H_OFFSET, FX1_ISP_AWB_ZONE_SUM_R_L_OFFSET, 0},
                        {FX1_ISP_AWB_ZONE_SUM_G_H_OFFSET, FX1_ISP_AWB_ZONE_SUM_G_L_OFFSET, 1},
                        {FX1_ISP_AWB_ZONE_SUM_B_H_OFFSET, FX1_ISP_AWB_ZONE_SUM_B_L_OFFSET, 2}};
   for (const auto &zr : awb_zone_regs) {
      const unsigned c = zr.c;
      regs_.on_read(zr.h, [this, awb_zone, c](std::uint32_t) {
         const long z = awb_zone();
         return z < 0 ? 0u : static_cast<std::uint32_t>((awb_mem_.zsum[static_cast<std::size_t>(z)][c] >> 32) & 0x7u);
      });
      regs_.on_read(zr.l, [this, awb_zone, c](std::uint32_t) {
         const long z = awb_zone();
         return z < 0 ? 0u : static_cast<std::uint32_t>(awb_mem_.zsum[static_cast<std::size_t>(z)][c]);
      });
   }
   regs_.on_read(FX1_ISP_AWB_ZONE_COUNT_OFFSET, [this, awb_zone](std::uint32_t) {
      const long z = awb_zone();
      return z < 0 ? 0u : awb_mem_.zcnt[static_cast<std::size_t>(z)] & FX1_ISP_AWB_ZONE_COUNT_ZONE_COUNT_MASK;
   });

   // ---- AF focus scores (#413-#416): latched registers ----
   regs_.on_read(FX1_ISP_AF_STAT_DATA_OFFSET, [this](std::uint32_t) {
      return af_pub_.fv[regs_.peek(FX1_ISP_AF_STAT_ADDR_OFFSET) & FX1_ISP_AF_STAT_ADDR_ZONE_ADDR_MASK];
   });
}

}  // namespace cdc::components::fx1_isp
