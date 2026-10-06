// SPDX-License-Identifier: Apache-2.0
#include "control/control_unit.h"

#include <algorithm>

#include "fx1_isp/fx1_isp_csr.h"

namespace cdc::components::fx1_isp {

namespace {

// Registers whose writes are commands for block models outside the control
// unit (the DMA engines). Without an installed block hook they
// are only recorded.
constexpr std::uint32_t command_registers[] = {
   FX1_ISP_COMMON_CTRL_OFFSET,    FX1_ISP_DMA_CTRL_OFFSET,       FX1_ISP_IDMA_BUF_VALID_OFFSET,
   FX1_ISP_ODMA_BUF_FREE_OFFSET,  FX1_ISP_ODMA_BUF_DONE_OFFSET,
};

// Error sources of COMMON_STATUS.error / COMMON_IRQ_STATUS.error_irq (DEC-15).
struct error_source {
   std::uint32_t offset;
   std::uint32_t mask;
};
constexpr error_source error_sources[] = {
   {FX1_ISP_LSC_ERROR_OFFSET, 0x3Fu},
   {FX1_ISP_BPC_STATUS_OFFSET, FX1_ISP_BPC_STATUS_CAND_REJECT_OVF_MASK},
   {FX1_ISP_EE_STATUS_OFFSET, FX1_ISP_EE_STATUS_ERROR_MASK},
   {FX1_ISP_DMA_ERR_OFFSET, FX1_ISP_DMA_ERR_ERR_MASK},
};

// Statistics publication events feeding STATS_READY_IRQ (HAS Table 9-2).
constexpr error_source stats_sources[] = {
   {FX1_ISP_AEC_STATUS_OFFSET, FX1_ISP_AEC_STATUS_STAT_DONE_MASK},
   {FX1_ISP_AWB_STATUS_OFFSET, FX1_ISP_AWB_STATUS_STAT_DONE_MASK},
   {FX1_ISP_AF_STATUS_OFFSET, FX1_ISP_AF_STATUS_FRAME_DONE_MASK},
};

// Hardware state a soft reset preserves (DEC-14); everything else that is not
// RW configuration returns to its reset value.
constexpr std::uint32_t soft_reset_preserved[] = {
   FX1_ISP_IDMA_FRAME_COUNT_OFFSET, FX1_ISP_ODMA_FRAME_COUNT_OFFSET, FX1_ISP_AEC_FRAME_ID_OFFSET,
   FX1_ISP_AWB_FRAME_ID_OFFSET,     FX1_ISP_AF_FRAME_ID_OFFSET,      FX1_ISP_LSC_PROFILE_STATUS_OFFSET,
};

// Resizer target sizes by RESIZER_CTRL.scale (HAS Table 6-67); 0 and 0xF
// select FullRes.
constexpr geometry resizer_modes[16] = {
   {0, 0},       {3840, 2160}, {2560, 1440}, {2880, 1620}, {2304, 1296}, {1920, 1080},
   {1280, 720},  {960, 540},   {640, 360},   {2592, 1944}, {2048, 1536}, {1600, 1200},
   {1280, 960},  {800, 600},   {640, 480},   {0, 0},
};

// Configuration sets gated by an `updated` bit (DEC-24, DEC-27). For the
// Resizer only the scale field is gated; the enable is sampled every SOF.
struct gated_group {
   std::uint32_t ctrl;
   std::uint32_t updated_mask;
   std::uint32_t first;  // gated registers [first, last], word stride
   std::uint32_t last;
   std::uint32_t field_mask;  // bits of each gated register that are committed
};
constexpr gated_group gated_groups[] = {
   {FX1_ISP_CCM_CTRL_OFFSET, FX1_ISP_CCM_CTRL_UPDATED_MASK, FX1_ISP_CCM_CRR_OFFSET, FX1_ISP_CCM_OFS_B_OFFSET,
    0xFFFFFFFFu},
   {FX1_ISP_CNF_CTRL_OFFSET, FX1_ISP_CNF_CTRL_UPDATED_MASK, FX1_ISP_CNF_CHROMA_TH_OFFSET, FX1_ISP_CNF_LUMA_TH_OFFSET,
    0xFFFFFFFFu},
   {FX1_ISP_RESIZER_CTRL_OFFSET, FX1_ISP_RESIZER_CTRL_UPDATED_MASK, FX1_ISP_RESIZER_CTRL_OFFSET,
    FX1_ISP_RESIZER_CTRL_OFFSET, FX1_ISP_RESIZER_CTRL_SCALE_MASK},
};

std::uint32_t field(std::uint32_t reg, std::uint32_t mask, std::uint32_t shift) {
   return (reg & mask) >> shift;
}

}  // namespace

control_unit::control_unit(unsigned soft_reset_cycles) : soft_reset_cycles_(soft_reset_cycles) {
   install_hooks();
   external_reset();
}

void control_unit::external_reset() {
   regs_.reset();
   commands_.clear();  // the commands' state was reset with the registers
   soft_reset_end_ = 0;
   // CSR-16: Gamma powers up as zeros, EE tables as 0x8000 (unity); the GTM
   // table is unstated and taken as zero.
   gamma_lut_.fill(0);
   for (auto &t : ee_tables_) {
      t.fill(0x8000u);
   }
   gtm_banks_ = {};
   gtm_read_ = 0;
   gtm_valid_ = false;
   gtm_pending_ = false;
   nr_var_ = 0;
   stats_reset(true);
   lsc_ = lsc_state{};  // i_rst_n: validity and load state cleared, meshes undefined
   committed_.clear();
   for (const gated_group &g : gated_groups) {
      for (std::uint32_t off = g.first; off <= g.last; off += 4) {
         committed_[off] = regs_.peek(off);
      }
   }
}

void control_unit::commit_gated(bool at_sof) {
   // DEC-26: with `updated` set, commit immediately while the pipeline is idle,
   // otherwise at the next accepted SOF.
   const bool idle = !(regs_.peek(FX1_ISP_COMMON_STATUS_OFFSET) & FX1_ISP_COMMON_STATUS_BUSY_MASK);
   if (!at_sof && !idle) {
      return;
   }
   for (const gated_group &g : gated_groups) {
      if (regs_.peek(g.ctrl) & g.updated_mask) {
         for (std::uint32_t off = g.first; off <= g.last; off += 4) {
            committed_[off] = regs_.peek(off);
         }
      }
   }
}

std::uint32_t control_unit::committed(std::uint32_t offset) const {
   const auto it = committed_.find(offset);
   if (it == committed_.end()) {
      return regs_.peek(offset);
   }
   for (const gated_group &g : gated_groups) {
      if (offset >= g.first && offset <= g.last) {
         return (regs_.peek(offset) & ~g.field_mask) | (it->second & g.field_mask);
      }
   }
   return it->second;
}

void control_unit::install_hooks() {
   for (const std::uint32_t off : command_registers) {
      regs_.on_write(off, [this](const register_file::write_effect &fx) { dispatch(fx, current_cycle_); });
   }

   // Gamma LUT port (CSR rows 193-198, CSR-21).
   regs_.on_write(FX1_ISP_GAMMA_LUT_DATA_OFFSET, [this](const register_file::write_effect &fx) {
      const std::uint32_t addr = regs_.peek(FX1_ISP_GAMMA_LUT_ADDR_OFFSET) & FX1_ISP_GAMMA_LUT_ADDR_ADDR_MASK;
      gamma_lut_[addr] = static_cast<std::uint16_t>(fx.new_value & FX1_ISP_GAMMA_LUT_DATA_DATA_MASK);
      regs_.hw_write(FX1_ISP_GAMMA_LUT_ADDR_OFFSET, FX1_ISP_GAMMA_LUT_ADDR_ADDR_MASK, addr + 1u);
   });
   regs_.on_read(FX1_ISP_GAMMA_LUT_RDATA_OFFSET, [this](std::uint32_t) -> std::uint32_t {
      return gamma_lut_[regs_.peek(FX1_ISP_GAMMA_LUT_ADDR_OFFSET) & FX1_ISP_GAMMA_LUT_ADDR_ADDR_MASK];
   });

   // EE gain-table port (CSR rows 262-272, CSR-21).
   auto ee_index = [this](unsigned &bank) {
      bank = field(regs_.peek(FX1_ISP_EE_LUT_CTRL_OFFSET), FX1_ISP_EE_LUT_CTRL_LUT_SEL_MASK,
                   FX1_ISP_EE_LUT_CTRL_LUT_SEL_SHIFT);
      return regs_.peek(FX1_ISP_EE_LUT_ADDR_OFFSET) & (ee_bank_entries(bank) - 1u);
   };
   regs_.on_write(FX1_ISP_EE_LUT_WDATA_OFFSET, [this, ee_index](const register_file::write_effect &fx) {
      unsigned bank = 0;
      const std::uint32_t idx = ee_index(bank);
      ee_tables_[bank][idx] = static_cast<std::uint16_t>(fx.new_value & FX1_ISP_EE_LUT_WDATA_LUT_WDATA_MASK);
      const std::uint32_t addr = regs_.peek(FX1_ISP_EE_LUT_ADDR_OFFSET);
      regs_.hw_write(FX1_ISP_EE_LUT_ADDR_OFFSET, FX1_ISP_EE_LUT_ADDR_LUT_ADDR_MASK, addr + 1u);
   });
   regs_.on_read(FX1_ISP_EE_LUT_RDATA_OFFSET, [this, ee_index](std::uint32_t) -> std::uint32_t {
      unsigned bank = 0;
      const std::uint32_t idx = ee_index(bank);
      return ee_tables_[bank][idx];
   });

   // GTM table port (CSR rows 206-217, ALG-GTM-08): a DATA write reaches the
   // read bank only while EN = 0 or MANUAL = 1 (live values); the address
   // increments on every write, 7-bit wrap; entries 65..127 do not exist.
   regs_.on_write(FX1_ISP_GTM_LUT_DATA_OFFSET, [this](const register_file::write_effect &fx) {
      const std::uint32_t ctrl = regs_.peek(FX1_ISP_GTM_CTRL_OFFSET);
      const std::uint32_t addr = regs_.peek(FX1_ISP_GTM_LUT_ADDR_OFFSET) & FX1_ISP_GTM_LUT_ADDR_ADDR_MASK;
      const bool writable = !(ctrl & FX1_ISP_GTM_CTRL_EN_MASK) || (ctrl & FX1_ISP_GTM_CTRL_MANUAL_MASK);
      if (writable && addr < 65) {
         gtm_banks_[gtm_read_][addr] = static_cast<std::uint16_t>(fx.new_value & FX1_ISP_GTM_LUT_DATA_DATA_MASK);
         gtm_valid_ = true;  // ALG-GTM-05
      }
      regs_.hw_write(FX1_ISP_GTM_LUT_ADDR_OFFSET, FX1_ISP_GTM_LUT_ADDR_ADDR_MASK, addr + 1u);
   });
   regs_.on_read(FX1_ISP_GTM_LUT_RDATA_OFFSET, [this](std::uint32_t) -> std::uint32_t {
      const std::uint32_t addr = regs_.peek(FX1_ISP_GTM_LUT_ADDR_OFFSET) & FX1_ISP_GTM_LUT_ADDR_ADDR_MASK;
      return addr < 65 ? gtm_banks_[gtm_read_][addr] : 0u;
   });

   // EE radial gain range error: a level on the live registers (ALG-EE-06).
   for (const std::uint32_t off : {FX1_ISP_EE_FEATURE_EN_OFFSET, FX1_ISP_EE_RADIAL_GAIN_01_OFFSET,
                                   FX1_ISP_EE_RADIAL_GAIN_23_OFFSET}) {
      regs_.on_write(off, [this](const register_file::write_effect &) { ee_radial_check(); });
   }

   // LSC coefficient load sequencer and status (HAS §6.8.8, CSR rows 92-117).
   regs_.on_write(FX1_ISP_LSC_LOAD_CTRL_OFFSET, [this](const register_file::write_effect &fx) { lsc_load_ctrl(fx); });
   regs_.on_write(FX1_ISP_LSC_COEF_DATA_OFFSET, [this](const register_file::write_effect &fx) { lsc_coef_data(fx); });
   regs_.on_write(FX1_ISP_LSC_PROFILE_SEL_OFFSET,
                  [this](const register_file::write_effect &fx) { lsc_profile_sel(fx); });
   regs_.on_write(FX1_ISP_LSC_MESH_NODES_OFFSET, [this](const register_file::write_effect &fx) {
      if (fx.new_value != fx.old_value) {
         lsc_.valid.fill(false);  // a geometry change invalidates every profile (p62)
      }
   });
   regs_.on_read(FX1_ISP_LSC_LOAD_STATUS_OFFSET, [this](std::uint32_t) -> std::uint32_t {
      const std::uint32_t expected = lsc_expected_count();
      const bool full = expected > 0 && lsc_.count == expected;
      return (lsc_.count & 0xFFFFu) | (full ? FX1_ISP_LSC_LOAD_STATUS_LOAD_FULL_MASK : 0u) |
             (lsc_.busy ? FX1_ISP_LSC_LOAD_STATUS_LOAD_BUSY_MASK : 0u) |
             (lsc_.dest << FX1_ISP_LSC_LOAD_STATUS_LOAD_PROFILE_SHIFT);
   });
   regs_.on_read(FX1_ISP_LSC_PROFILE_STATUS_OFFSET, [this](std::uint32_t) -> std::uint32_t {
      std::uint32_t v = 0;
      for (unsigned i = 0; i < lsc_profiles; ++i) {
         v |= lsc_.valid[i] ? 1u << i : 0u;
      }
      v |= lsc_.active << FX1_ISP_LSC_PROFILE_STATUS_ACTIVE_PROFILE_SHIFT;
      v |= lsc_profile_valid(lsc_.active) ? FX1_ISP_LSC_PROFILE_STATUS_ACTIVE_VALID_MASK : 0u;
      v |= lsc_geometry_valid() ? FX1_ISP_LSC_PROFILE_STATUS_GEOMETRY_VALID_MASK : 0u;
      return v;
   });

   // Derived geometry (CSR-09, CSR-10).
   regs_.on_read(FX1_ISP_RESIZER_OUT_W_OFFSET, [this](std::uint32_t) { return resizer_output_geometry().width; });
   regs_.on_read(FX1_ISP_RESIZER_OUT_H_OFFSET, [this](std::uint32_t) { return resizer_output_geometry().height; });

   // COMMON_STATUS.error is the live OR of the error flags (DEC-15).
   regs_.on_read(FX1_ISP_COMMON_STATUS_OFFSET, [this](std::uint32_t stored) -> std::uint32_t {
      stored &= ~FX1_ISP_COMMON_STATUS_ERROR_MASK;
      return stored | (error_flags() ? FX1_ISP_COMMON_STATUS_ERROR_MASK : 0u);
   });

   regs_.on_hw_set([this](std::uint32_t offset, std::uint32_t newly) { on_hw_set(offset, newly); });
   stats_install_hooks();

   // Gated configuration: a write to the set or to its control register may
   // commit it (DEC-26).
   for (const gated_group &g : gated_groups) {
      const auto commit = [this](const register_file::write_effect &) { commit_gated(false); };
      regs_.on_write(g.ctrl, commit);
      for (std::uint32_t off = g.first; off <= g.last; off += 4) {
         if (off != g.ctrl) {
            regs_.on_write(off, commit);
         }
      }
   }
}

void control_unit::csr_write(std::uint32_t addr, std::uint32_t data, std::uint32_t lane_mask,
                             std::uint64_t cycle) {
   advance_to(cycle);
   current_cycle_ = cycle;
   regs_.sw_write(addr, data, lane_mask, cycle);
}

void control_unit::advance_to(std::uint64_t cycle) {
   if (cycle >= soft_reset_end_) {
      regs_.set_command_block(false);
   }
}

void control_unit::dispatch(const register_file::write_effect &fx, std::uint64_t cycle) {
   const bool soft_reset =
       (fx.offset == FX1_ISP_COMMON_CTRL_OFFSET && (fx.w1sc_pulse & FX1_ISP_COMMON_CTRL_SOFT_RST_MASK)) ||
       (fx.offset == FX1_ISP_DMA_CTRL_OFFSET && (fx.w1sc_pulse & FX1_ISP_DMA_CTRL_SOFT_RESET_MASK));
   if (soft_reset) {
      apply_soft_reset(cycle);
   }
   const auto hook = block_hooks_.find(fx.offset);
   if (hook != block_hooks_.end() && hook->second) {
      hook->second(fx, cycle);
   } else if (!soft_reset) {
      // A soft-reset write is consumed here; it is not a pending command.
      commands_.push_back({cycle, fx});
   }
}

void control_unit::apply_soft_reset(std::uint64_t cycle) {
   // DEC-14: RW configuration, LUT/mesh contents, LSC profile validity and the
   // frame counters survive; every other hardware-owned bit (status, results,
   // ownership, commands, interrupt and error state) returns to reset.
   for (std::size_t i = 0; i < csr::num_registers; ++i) {
      const csr::reg_desc &d = csr::registers[i];
      if (std::find(std::begin(soft_reset_preserved), std::end(soft_reset_preserved), d.offset) !=
          std::end(soft_reset_preserved)) {
         continue;
      }
      const std::uint32_t hw_owned = d.ro_mask | d.w1c_mask | d.w1s_mask;
      if (hw_owned) {
         regs_.hw_hold(d.offset, hw_owned, false, cycle);
         regs_.hw_write(d.offset, hw_owned, d.reset);
      }
   }
   commands_.clear();  // pending commands are part of the state being reset
   nr_var_ = 0;  // ALG-2DNR-09: a measured statistic, cleared like the published results
   stats_reset(false);
   lsc_.busy = false;  // a load in progress is discarded; meshes and validity survive (DEC-14)
   lsc_.count = 0;
   lsc_.staging.clear();
   soft_reset_end_ = cycle + soft_reset_cycles_;
   ee_radial_check();  // a level condition re-asserts at once
   regs_.set_command_block(true);
   if (soft_reset_hook_) {
      soft_reset_hook_(cycle);
   }
}

void control_unit::hw_set(std::uint32_t offset, std::uint32_t mask, std::uint64_t cycle) {
   current_cycle_ = cycle;  // derived COMMON_IRQ_STATUS sets carry the same cycle
   regs_.hw_set(offset, mask, cycle);
}

void control_unit::hw_hold(std::uint32_t offset, std::uint32_t mask, bool level, std::uint64_t cycle) {
   current_cycle_ = cycle;
   regs_.hw_hold(offset, mask, level, cycle);
}

void control_unit::accepted_sof(std::uint64_t cycle) {
   current_cycle_ = cycle;
   regs_.hw_clear(FX1_ISP_COMMON_CTRL_OFFSET, FX1_ISP_COMMON_CTRL_FRAME_START_MASK);
   regs_.hw_clear(FX1_ISP_COMMON_STATUS_OFFSET, FX1_ISP_COMMON_STATUS_FRAME_DONE_MASK);
   commit_gated(true);
   if (gtm_pending_) {  // bank swap on the SOF beat (HAS p112)
      gtm_read_ ^= 1u;
      gtm_pending_ = false;
      gtm_valid_ = true;
   }
   stats_sof();
   // A validated LSC_PROFILE_SEL value becomes the active profile (ALG-LSC-06).
   lsc_.active = regs_.peek(FX1_ISP_LSC_PROFILE_SEL_OFFSET) & FX1_ISP_LSC_PROFILE_SEL_PROFILE_SEL_MASK;
}

void control_unit::on_hw_set(std::uint32_t offset, std::uint32_t newly) {
   for (const auto &src : error_sources) {
      if (src.offset == offset && (newly & src.mask)) {
         regs_.hw_set(FX1_ISP_COMMON_IRQ_STATUS_OFFSET, FX1_ISP_COMMON_IRQ_STATUS_ERROR_IRQ_MASK, current_cycle_);
      }
   }
   for (const auto &src : stats_sources) {
      if (src.offset == offset && (newly & src.mask)) {
         regs_.hw_set(FX1_ISP_COMMON_IRQ_STATUS_OFFSET, FX1_ISP_COMMON_IRQ_STATUS_STATS_READY_IRQ_MASK,
                      current_cycle_);
      }
   }
}

std::uint32_t control_unit::error_flags() const {
   std::uint32_t any = 0;
   for (const auto &src : error_sources) {
      any |= regs_.peek(src.offset) & src.mask;
   }
   return any;
}

bool control_unit::irq_level() const {
   const std::uint32_t common = regs_.peek(FX1_ISP_COMMON_IRQ_STATUS_OFFSET) & regs_.peek(FX1_ISP_COMMON_IRQ_EN_OFFSET);
   const std::uint32_t dma = regs_.peek(FX1_ISP_DMA_IRQ_STAT_OFFSET) & regs_.peek(FX1_ISP_DMA_IRQ_EN_OFFSET);
   return (common | dma) != 0;
}

geometry control_unit::derive_active(std::uint32_t width, std::uint32_t height, std::uint32_t bayer_pattern) {
   const std::uint32_t sh = bayer_pattern & 1u;         // bit 0: horizontal shift
   const std::uint32_t sv = (bayer_pattern >> 1) & 1u;  // bit 1: vertical shift
   return {width > sh ? (width - sh) / 2u * 2u : 0u, height > sv ? (height - sv) / 2u * 2u : 0u};
}

geometry control_unit::derive_output(const geometry &active, std::uint32_t resizer_ctrl) {
   const std::uint32_t scale =
       field(resizer_ctrl, FX1_ISP_RESIZER_CTRL_SCALE_MASK, FX1_ISP_RESIZER_CTRL_SCALE_SHIFT);
   geometry out = active;
   if (resizer_ctrl & FX1_ISP_RESIZER_CTRL_EN_MASK) {
      const geometry target = resizer_modes[scale];
      if (target.width != 0 && target.width <= active.width && target.height <= active.height) {
         out = target;
      }
   }
   out.width &= FX1_ISP_RESIZER_OUT_W_OUT_WIDTH_MASK;
   out.height &= FX1_ISP_RESIZER_OUT_H_OUT_HEIGHT_MASK;
   return out;
}

geometry control_unit::active_geometry() const {
   return derive_active(regs_.peek(FX1_ISP_COMMON_FRAME_WIDTH_OFFSET) & FX1_ISP_COMMON_FRAME_WIDTH_H_ACTIVE_MASK,
                        regs_.peek(FX1_ISP_COMMON_FRAME_HEIGHT_OFFSET) & FX1_ISP_COMMON_FRAME_HEIGHT_V_ACTIVE_MASK,
                        regs_.peek(FX1_ISP_COMMON_BAYER_OFFSET) & FX1_ISP_COMMON_BAYER_PATTERN_MASK);
}

geometry control_unit::resizer_output_geometry() const {
   return derive_output(active_geometry(), committed(FX1_ISP_RESIZER_CTRL_OFFSET));  // DEC-26
}

void control_unit::set_block_hook(std::uint32_t offset, block_hook hook) {
   block_hooks_[offset] = std::move(hook);
}

// ---- LSC coefficient load (HAS §6.8.8, Table 6-23; ALG-LSC-04..11) ----------------

bool control_unit::lsc_geometry_valid() const {
   const std::uint32_t nodes = regs_.peek(FX1_ISP_LSC_MESH_NODES_OFFSET);
   const std::uint32_t nx = nodes & FX1_ISP_LSC_MESH_NODES_MESH_NX_MASK;
   const std::uint32_t ny = (nodes & FX1_ISP_LSC_MESH_NODES_MESH_NY_MASK) >> FX1_ISP_LSC_MESH_NODES_MESH_NY_SHIFT;
   return nx >= 2 && ny >= 2 && nx <= lsc_mesh_max && ny <= lsc_mesh_max;
}

std::uint32_t control_unit::lsc_expected_count() const {
   const std::uint32_t nodes = regs_.peek(FX1_ISP_LSC_MESH_NODES_OFFSET);
   const std::uint32_t nx = nodes & FX1_ISP_LSC_MESH_NODES_MESH_NX_MASK;
   const std::uint32_t ny = (nodes & FX1_ISP_LSC_MESH_NODES_MESH_NY_MASK) >> FX1_ISP_LSC_MESH_NODES_MESH_NY_SHIFT;
   return 4u * nx * ny;
}

void control_unit::lsc_error(std::uint32_t mask) { hw_set(FX1_ISP_LSC_ERROR_OFFSET, mask, current_cycle_); }

void control_unit::lsc_load_ctrl(const register_file::write_effect &fx) {
   const std::uint32_t cmd = fx.w1s_set;
   const std::uint32_t cmds = FX1_ISP_LSC_LOAD_CTRL_LOAD_BEGIN_MASK | FX1_ISP_LSC_LOAD_CTRL_LOAD_VALIDATE_MASK |
                              FX1_ISP_LSC_LOAD_CTRL_LOAD_ABORT_MASK;
   if (cmd & FX1_ISP_LSC_LOAD_CTRL_LOAD_ABORT_MASK) {           // abort > validate > begin
      lsc_.busy = false;                                         // destination keeps its contents
      lsc_.staging.clear();
   } else if (cmd & FX1_ISP_LSC_LOAD_CTRL_LOAD_VALIDATE_MASK) {
      if (!lsc_.busy) {
         lsc_error(FX1_ISP_LSC_ERROR_LOAD_PROTOCOL_MASK);
      } else {
         // The mesh may have been resized after LOAD_BEGIN. The count alone
         // then describes words received, not words stored in the staging bank.
         const auto expected = lsc_expected_count();
         if (lsc_geometry_valid() && lsc_.count == expected && lsc_.staging.size() == expected) {
            lsc_.mesh[lsc_.dest] = lsc_.staging;
            lsc_.valid[lsc_.dest] = true;
         } else {
            lsc_error(FX1_ISP_LSC_ERROR_COEF_COUNT_MASK);        // old contents and validity kept
         }
         lsc_.busy = false;
         lsc_.staging.clear();
      }
   } else if (cmd & FX1_ISP_LSC_LOAD_CTRL_LOAD_BEGIN_MASK) {
      const unsigned dest = fx.new_value & FX1_ISP_LSC_LOAD_CTRL_LOAD_PROFILE_MASK;
      if (lsc_.busy || dest >= lsc_profiles) {
         lsc_error(FX1_ISP_LSC_ERROR_LOAD_PROTOCOL_MASK);
      } else if (dest == lsc_.active && lsc_.valid[dest]) {
         lsc_error(FX1_ISP_LSC_ERROR_ACTIVE_LOAD_REJECT_MASK);  // ALG-LSC-05
      } else if (!lsc_geometry_valid()) {
         lsc_error(FX1_ISP_LSC_ERROR_GEOMETRY_INVALID_MASK);
      } else {
         lsc_.busy = true;
         lsc_.count = 0;
         lsc_.dest = dest;
         lsc_.staging.assign(lsc_expected_count(), 0);
      }
   }
   regs_.hw_clear(FX1_ISP_LSC_LOAD_CTRL_OFFSET, cmds);  // self-clearing commands
}

void control_unit::lsc_coef_data(const register_file::write_effect &fx) {
   if (!lsc_.busy) {
      lsc_error(FX1_ISP_LSC_ERROR_LOAD_PROTOCOL_MASK);
      return;
   }
   const std::uint32_t data = fx.data & fx.lane_mask;
   if (data > 0x100000u) {
      lsc_error(FX1_ISP_LSC_ERROR_COEF_RANGE_MASK);              // above 4.0 (ALG-LSC-03)
   }
   if (lsc_.count < lsc_.staging.size()) {
      lsc_.staging[lsc_.count] = data & FX1_ISP_LSC_COEF_DATA_COEF_DATA_MASK;
   }
   lsc_.count = std::min<std::uint32_t>(lsc_.count + 1u, 0x1FFFFu);
}

void control_unit::lsc_profile_sel(const register_file::write_effect &fx) {
   const unsigned sel = fx.new_value & FX1_ISP_LSC_PROFILE_SEL_PROFILE_SEL_MASK;
   if (sel >= lsc_profiles || !lsc_.valid[sel] || (lsc_.busy && sel == lsc_.dest)) {
      lsc_error(FX1_ISP_LSC_ERROR_PROFILE_SEL_REJECT_MASK);
      regs_.hw_write(FX1_ISP_LSC_PROFILE_SEL_OFFSET, FX1_ISP_LSC_PROFILE_SEL_PROFILE_SEL_MASK, fx.old_value);
   }
}

// ---- GTM, 2DNR, EE state -------------------------------------------------------

void control_unit::gtm_end_of_frame(std::uint32_t yavg) {
   const std::uint32_t ctrl = regs_.peek(FX1_ISP_GTM_CTRL_OFFSET);
   if (ctrl & FX1_ISP_GTM_CTRL_MANUAL_MASK) {
      return;  // curve generator held; the software curve persists (ALG-GTM-07)
   }
   const std::uint32_t key = regs_.peek(FX1_ISP_GTM_KEY_OFFSET) & FX1_ISP_GTM_KEY_KEY_MASK;
   const std::uint32_t lwhite = regs_.peek(FX1_ISP_GTM_LWHITE_OFFSET) & FX1_ISP_GTM_LWHITE_LWHITE_MASK;
   gtm_banks_[gtm_read_ ^ 1u] = pipe::gtm_build_curve(yavg, key, lwhite);
   gtm_pending_ = true;
}

void control_unit::nr_publish(std::uint32_t var) {
   nr_var_ = var & FX1_ISP_NR_2D_EST_VAR_EST_VAR_MASK;
   regs_.hw_write(FX1_ISP_NR_2D_EST_VAR_OFFSET, FX1_ISP_NR_2D_EST_VAR_EST_VAR_MASK, nr_var_);
}

void control_unit::ee_radial_check() {
   const bool radial = regs_.peek(FX1_ISP_EE_FEATURE_EN_OFFSET) & FX1_ISP_EE_FEATURE_EN_RADIAL_EN_MASK;
   const std::uint32_t g01 = regs_.peek(FX1_ISP_EE_RADIAL_GAIN_01_OFFSET);
   const std::uint32_t g23 = regs_.peek(FX1_ISP_EE_RADIAL_GAIN_23_OFFSET);
   const bool over = (g01 & 0xFFFFu) > 0xC000u || (g01 >> 16) > 0xC000u || (g23 & 0xFFFFu) > 0xC000u ||
                     (g23 >> 16) > 0xC000u;
   regs_.hw_hold(FX1_ISP_EE_STATUS_OFFSET, FX1_ISP_EE_STATUS_ERROR_MASK, radial && over, current_cycle_);
}

}  // namespace cdc::components::fx1_isp
