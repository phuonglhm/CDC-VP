// SPDX-License-Identifier: Apache-2.0
// Register-level behaviour of the FX1 ISP that does not depend on the DMA
// engines or the pixel pipeline (milestone M1): reset, soft reset,
// interrupt/error/statistics aggregation, LUT ports and derived geometry.
// Commands whose behaviour belongs to later milestones are delivered to
// pluggable block hooks. No SystemC dependency; time is a core-clock cycle.
#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <vector>

#include "control/stats_types.h"
#include "pipeline/gtm_curve.h"
#include "registers/register_file.h"

namespace cdc::components::fx1_isp {

struct geometry {
   std::uint32_t width;
   std::uint32_t height;
};

class control_unit {
public:
   using block_hook = std::function<void(const register_file::write_effect &, std::uint64_t cycle)>;
   using soft_reset_hook = std::function<void(std::uint64_t cycle)>;

   // A command written by software whose behaviour is implemented by a later
   // milestone. Recorded when no block hook is installed for that register.
   struct command_record {
      std::uint64_t cycle;
      register_file::write_effect effect;
   };

   explicit control_unit(unsigned soft_reset_cycles = 32);

   // i_rst_n asserted: registers to reset values, LUTs to their documented
   // power-up contents (CSR-16), soft-reset window cancelled.
   void external_reset();

   // CSR port.
   std::uint32_t csr_read(std::uint32_t addr) const { return regs_.sw_read(addr); }
   void csr_write(std::uint32_t addr, std::uint32_t data, std::uint32_t lane_mask,
                  std::uint64_t cycle);

   // Level of the o_irq output (HAS Table 9-2).
   bool irq_level() const;

   // Soft reset (DEC-14). The register effects are applied at the write; the
   // engines stay in reset until `soft_reset_end_cycle()`.
   bool soft_reset_active(std::uint64_t cycle) const { return cycle < soft_reset_end_; }
   std::uint64_t soft_reset_end_cycle() const { return soft_reset_end_; }
   // Lifts the command block once the reset window has elapsed.
   void advance_to(std::uint64_t cycle);

   // Pipeline geometry after the Input Formatter phase crop (HAS Table 6-17)
   // and the output geometry derived by the Resizer (HAS §6.20.10).
   geometry active_geometry() const;
   geometry resizer_output_geometry() const;
   // The same derivation from explicit values (frame snapshot at SOF).
   static geometry derive_active(std::uint32_t width, std::uint32_t height, std::uint32_t bayer_pattern);
   static geometry derive_output(const geometry &active, std::uint32_t resizer_ctrl);

   // Hardware events (used by the DMA/pipeline models and by tests).
   void hw_set(std::uint32_t offset, std::uint32_t mask, std::uint64_t cycle);
   void hw_clear(std::uint32_t offset, std::uint32_t mask) { regs_.hw_clear(offset, mask); }
   void hw_write(std::uint32_t offset, std::uint32_t mask, std::uint32_t value) {
      regs_.hw_write(offset, mask, value);
   }
   void hw_hold(std::uint32_t offset, std::uint32_t mask, bool level, std::uint64_t cycle);
   // Accepted SOF: clears the pending frame_start (DEC-16), frame_done (DEC-17),
   // and performs pending commits of the gated configuration sets (DEC-26).
   void accepted_sof(std::uint64_t cycle);

   // Committed value of a register whose configuration is gated by an
   // `updated` bit (DEC-24/27: CCM coefficients and offsets, CNF thresholds,
   // Resizer control); the live register value for every other register.
   std::uint32_t committed(std::uint32_t offset) const;

   void set_block_hook(std::uint32_t offset, block_hook hook);
   void set_soft_reset_hook(soft_reset_hook hook) { soft_reset_hook_ = std::move(hook); }
   const std::vector<command_record> &commands() const { return commands_; }
   void clear_commands() { commands_.clear(); }

   register_file &regs() { return regs_; }
   const register_file &regs() const { return regs_; }

   // Table contents (live LUTs read by the pipeline in M3).
   const std::array<std::uint16_t, 256> &gamma_lut() const { return gamma_lut_; }
   const std::array<std::uint16_t, 64> &ee_table(unsigned bank) const { return ee_tables_.at(bank); }
   // GTM tone-curve banks (HAS §6.16.4, Table 6-49; ALG-GTM-05/07/08): the
   // datapath reads the read bank; the generator fills the other one, which
   // becomes the read bank at the next accepted SOF.
   const pipe::gtm_curve &gtm_read_bank() const { return gtm_banks_[gtm_read_]; }
   bool gtm_curve_valid() const { return gtm_valid_; }
   // End of frame: KEY, LWHITE and MANUAL are read now (ALG-GTM-09).
   void gtm_end_of_frame(std::uint32_t yavg);
   // Statistics (HAS §6.21-6.23, §9.5; DEC-29; ALG-STAT-*). Configuration seen
   // by the frame that was just accepted (valid after accepted_sof()).
   const aec_config &aec_active() const { return aec_active_; }
   const awb_config &awb_frame_config() const { return awb_sof_; }
   bool af_frame_enabled() const { return af_en_sof_; }
   std::uint32_t frame_id() const { return frame_id_; }
   // The single in-place zone/histogram memories (DEC-30): restarted at the
   // SOF of an enabled frame, updated by the pipeline row by row.
   aec_memory &aec_zone_memory() { return aec_mem_; }
   void debug_set_frame_counter(std::uint32_t value) { frame_counter_ = value; }  // test fixtures only
   awb_memory &awb_zone_memory() { return awb_mem_; }
   // End of frame: publish the results of the blocks that measured the frame.
   void stats_publish(const aec_result *aec, const awb_result *awb, const af_result *af, bool af_full);
   // Frame abandoned before its end: no publication, busy bits cleared.
   void stats_abort();
   // Per-block STATUS.busy of the pixel blocks (Demosaic, CCM, Gamma, GTM,
   // 2DNR, EE, CNF, Resizer): 1 while a frame is inside the pipeline,
   // including while it is stalled (DEC-38).
   void blocks_busy(bool busy);
   // 2DNR noise-variance shadow (HAS §6.17.7.2): measured on frame N, used by N+1.
   std::uint32_t nr_variance() const { return nr_var_; }
   void nr_publish(std::uint32_t var);

   // LSC resident profiles (HAS §6.8.8, ALG-LSC-04..11). PARA_LSC_MESH_MAX = 32
   // (HAS Table 5-1) bounds the node counts.
   static constexpr unsigned lsc_profiles = 3;
   static constexpr std::uint32_t lsc_mesh_max = 32;
   const std::vector<std::uint32_t> &lsc_profile(unsigned idx) const { return lsc_.mesh.at(idx); }
   unsigned lsc_active_profile() const { return lsc_.active; }
   bool lsc_profile_valid(unsigned idx) const { return idx < lsc_profiles && lsc_.valid[idx]; }
   bool lsc_geometry_valid() const;

   static constexpr unsigned ee_bank_entries(unsigned bank) { return bank < 2 ? 64u : 32u; }

private:
   void install_hooks();
   void lsc_load_ctrl(const register_file::write_effect &fx);
   void lsc_coef_data(const register_file::write_effect &fx);
   void lsc_profile_sel(const register_file::write_effect &fx);
   void lsc_error(std::uint32_t mask);
   std::uint32_t lsc_expected_count() const;
   void commit_gated(bool at_sof);
   void dispatch(const register_file::write_effect &fx, std::uint64_t cycle);
   void apply_soft_reset(std::uint64_t cycle);
   void on_hw_set(std::uint32_t offset, std::uint32_t newly);
   std::uint32_t error_flags() const;
   void ee_radial_check();
   void stats_install_hooks();
   void stats_reset(bool external);
   void stats_sof();

   register_file regs_;
   unsigned soft_reset_cycles_;
   std::uint64_t soft_reset_end_ = 0;
   std::uint64_t current_cycle_ = 0;
   std::map<std::uint32_t, block_hook> block_hooks_;
   soft_reset_hook soft_reset_hook_;
   std::vector<command_record> commands_;

   std::map<std::uint32_t, std::uint32_t> committed_;  // gated registers only

   struct lsc_state {
      std::array<std::vector<std::uint32_t>, 3> mesh;  // committed profiles
      std::array<bool, 3> valid{};
      std::vector<std::uint32_t> staging;              // load in progress (ALG-LSC-04)
      bool busy = false;
      std::uint32_t count = 0;                         // >= 17 bits (ALG-LSC-07)
      unsigned dest = 0;
      unsigned active = 0;                             // adopted at SOF (ALG-LSC-06)
   } lsc_;

   std::array<std::uint16_t, 256> gamma_lut_{};
   std::array<std::array<std::uint16_t, 64>, 4> ee_tables_{};
   std::array<pipe::gtm_curve, 2> gtm_banks_{};
   unsigned gtm_read_ = 0;
   bool gtm_valid_ = false;    // a curve exists (hard reset: pass-through, p112)
   bool gtm_pending_ = false;  // a rebuilt bank waits for the next SOF
   std::uint32_t nr_var_ = 0;

   // Statistics state.
   aec_config aec_active_{};
   awb_config awb_sof_{};
   bool af_en_sof_ = false;
   std::uint32_t frame_counter_ = 0;  // frames started since reset (#316)
   std::uint32_t frame_id_ = 0;       // bound to the frame in flight (ALG-STAT-01)
   std::uint32_t aec_ctx_ = 0, awb_ctx_ = 0, af_ctx_ = 0;
   aec_result aec_pub_{};
   awb_result awb_pub_{};
   af_result af_pub_{};
   aec_memory aec_mem_{};
   awb_memory awb_mem_{};
   bool aec_mem_live_ = false;  // restarted by an enabled SOF since the last reset
   bool awb_mem_live_ = false;
};

}  // namespace cdc::components::fx1_isp
