// SPDX-License-Identifier: Apache-2.0
// Register-level behaviour of the FX1 ISP that does not depend on the DMA
// engines or the pixel pipeline (milestone M1): reset, soft reset,
// interrupt/error/statistics aggregation, LUT ports and derived geometry.
// Commands whose behaviour belongs to later milestones are delivered to
// pluggable block hooks. No SystemC dependency; time is a core-clock cycle.
#pragma once

#include <algorithm>
#include <array>
#include <bitset>
#include <cstdint>
#include <deque>
#include <functional>
#include <limits>
#include <map>
#include <unordered_map>
#include <vector>

#include "control/channel_geometry.h"
#include "control/ee_types.h"
#include "control/stats_types.h"
#include "fx1_isp/fx1_isp_csr.h"
#include "pipeline/gtm_curve.h"
#include "registers/register_file.h"

namespace cdc::components::fx1_isp {

// Resizer configuration of one channel (MAS v1.07 §6.20.4.3): the key and
// the complete validated set published for it (control/channel_geometry.h).
using resizer_key = channel_key;
using resizer_set = channel_set;

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

   // Scope of a soft reset (MAS Table 183, p316-317).
   enum class reset_scope {
      common,  // COMMON_CTRL.soft_rst: pixel pipeline and both DMA engines
      dma,     // DMA_CTRL.soft_reset: DMA engines and the frame in flight only
   };

   // 15-core-clock soft-reset pulse (MAS §6.5.3 p33).
   explicit control_unit(unsigned soft_reset_cycles = 15);

   // i_rst_n asserted: registers to reset values, LUTs to their documented
   // power-up contents (the poison pattern, MAS §6.5.2), soft-reset window cancelled.
   void external_reset();
   // i_rst_n released at `cycle`: FRAME_COUNTER starts counting from zero.
   void release_reset(std::uint64_t cycle) { time_base_epoch_ = cycle; }

   // FRAME_COUNTER (MAS Table 160): free-running 32-bit core-clock count since
   // the release of i_rst_n, computed on demand (no per-clock event). It reads
   // zero while i_rst_n is asserted; soft resets do not touch it (VP decision
   // U5-A1).
   std::uint32_t frame_counter(std::uint64_t cycle) const {
      return time_base_epoch_ == settled || cycle < time_base_epoch_
                 ? 0u
                 : static_cast<std::uint32_t>(cycle - time_base_epoch_);
   }

   // CSR port. A read at `cycle` sees hardware state of that edge (the AWB
   // lease becomes readable one core clock after ACQUIRE, MAS §6.22.4.3); the
   // overload without a cycle returns the settled state, as a debugger would.
   std::uint32_t csr_read(std::uint32_t addr) const { return regs_.sw_read(addr); }
   std::uint32_t csr_read(std::uint32_t addr, std::uint64_t cycle) const {
      read_cycle_ = cycle;
      const std::uint32_t v = regs_.sw_read(addr);
      read_cycle_ = settled;
      return v;
   }
   // A read on the CSR port at `cycle`: as csr_read, plus the register's
   // bus-read side effects (MAS §9.8; never run by debug reads). The events
   // those effects raise (TUNE_ERROR NO_LEASE / INVALID) happen on this edge,
   // so a W1C on the same edge loses to them (MAS Table 8-180).
   std::uint32_t csr_bus_read(std::uint32_t addr, std::uint64_t cycle) {
      advance_to(cycle);
      read_cycle_ = cycle;
      const std::uint32_t v = regs_.bus_read(addr, cycle);
      read_cycle_ = settled;
      return v;
   }

   // TUNE_CAPS (MAS §8.2): the capabilities this build implements and has
   // verified end to end. The CSR reset value 0x003FFFFF describes a complete
   // v1.07 implementation; the VP reports only what it implements and sets a
   // bit in the milestone that verifies it (VP decision V107-A1).
   // CHANNEL_CROP and CSC_WIDE_RANGE (CH2 inverse) wait for V107-7, FRAME_ID
   // (per-buffer publication) for V107-8, RECOVERY_REPORT for V107-9.
   static constexpr std::uint32_t implemented_caps =
       FX1_ISP_TUNE_CAPS_RAW_INPUT_MASK |                                               // V107-2
       FX1_ISP_TUNE_CAPS_GAMMA_BANK_MASK | FX1_ISP_TUNE_CAPS_BPC_CAL_MASK |              // V107-4/5
       FX1_ISP_TUNE_CAPS_LSC_SECTORS_MASK | FX1_ISP_TUNE_CAPS_RESIDENT_OWNERSHIP_MASK |  // V107-4/5
       FX1_ISP_TUNE_CAPS_NR_MASK | FX1_ISP_TUNE_CAPS_WB_GB_MASK | FX1_ISP_TUNE_CAPS_DEMOSAIC_MASK |
       FX1_ISP_TUNE_CAPS_CNF_BLEND_MASK | FX1_ISP_TUNE_CAPS_CSC_LOOK_MASK |             // V107-5
       FX1_ISP_TUNE_CAPS_PROFILE_MASK | FX1_ISP_TUNE_CAPS_AWB_COLOR_MASK | FX1_ISP_TUNE_CAPS_AF_WINDOWS_MASK |
       FX1_ISP_TUNE_CAPS_HIST_WEIGHT_MASK | FX1_ISP_TUNE_CAPS_AEC_LEASE_MASK | FX1_ISP_TUNE_CAPS_AF_LEASE_MASK |
       FX1_ISP_TUNE_CAPS_BAYER_AE_MASK | FX1_ISP_TUNE_CAPS_LEASE_GEOMETRY_MASK |         // V107-3/6
       FX1_ISP_TUNE_CAPS_CHANNEL_CROP_MASK | FX1_ISP_TUNE_CAPS_CSC_WIDE_RANGE_MASK |
       FX1_ISP_TUNE_CAPS_FRAME_ID_MASK |                                                 // V107-7/8
       FX1_ISP_TUNE_CAPS_RECOVERY_REPORT_MASK;                                           // V107-9
   void csr_write(std::uint32_t addr, std::uint32_t data, std::uint32_t lane_mask,
                  std::uint64_t cycle);

   // Level of the o_irq output (HAS Table 9-2).
   bool irq_level() const;

   // Soft reset (MAS §6.5.3, Table 183; decisions §9). The register effects
   // are applied at the write; the engines stay in reset until
   // `soft_reset_end_cycle()` (the end of a recovery's pulse; MAS v1.07 §9.9.8).
   bool soft_reset_active(std::uint64_t cycle) const { return cycle < soft_reset_end_; }
   std::uint64_t soft_reset_end_cycle() const { return soft_reset_end_; }
   // Time has reached `cycle`: the following hardware events (publication,
   // abort) happen on that edge; lifts the command block once the reset
   // window has elapsed.
   void advance_to(std::uint64_t cycle);

   // Pipeline geometry after the Input Formatter phase crop (HAS Table 6-17)
   // and the output geometry derived by the Resizer (HAS §6.20.10).
   geometry active_geometry() const;
   // Published geometry set of channel `ch` (MAS v1.07 §6.20.4.3, §8.3): what
   // GEOMETRY_STATUS, ACTIVE_CROP_* and RESIZER_OUT_W/H report and the next
   // accepted SOF uses. A scaling key is derived in E0-E35 (35 clocks); a
   // pass-through or invalid key publishes on the next edge; during a
   // derivation the previous set stays published; a key changed during it is
   // derived again. `cycle` is the observation edge; without one, settled.
   resizer_set resizer_published(unsigned ch, std::uint64_t cycle) const;
   resizer_set resizer_published(std::uint64_t cycle) const { return resizer_published(0, cycle); }
   geometry resizer_output_geometry(unsigned ch = 0) const { return resizer_published(ch, settled).out; }
   // GEOMETRY_STATUS of channel `ch` at `cycle`: BUSY while the live key is
   // not yet published (VALID and ERROR read 0), else VALID / ERROR.
   std::uint32_t geometry_status(unsigned ch, std::uint64_t cycle) const;
   // The sets the frame in flight uses, sampled at its accepted SOF.
   const resizer_set &resizer_frame(unsigned ch = 0) const { return rz_frame_.at(ch); }
   std::uint32_t output_enable_frame() const { return outputs_frame_; }  // OUTPUT_ENABLE at the SOF
   static constexpr unsigned resizer_derivation_cycles = 35;  // E0 .. E35
   // The active size after the Input Formatter phase crop.
   static geometry derive_active(std::uint32_t width, std::uint32_t height, std::uint32_t bayer_pattern);

   // Hardware events (used by the DMA/pipeline models and by tests).
   void hw_set(std::uint32_t offset, std::uint32_t mask, std::uint64_t cycle);
   // Hardware clear on the current edge (or `cycle`): a W1S bit software set
   // on that edge and a W1C bit hardware set on it survive (MAS Table 8-180).
   void hw_clear(std::uint32_t offset, std::uint32_t mask) { regs_.hw_clear(offset, mask, current_cycle_); }
   void hw_clear(std::uint32_t offset, std::uint32_t mask, std::uint64_t cycle) {
      regs_.hw_clear(offset, mask, cycle);
   }
   void hw_write(std::uint32_t offset, std::uint32_t mask, std::uint32_t value) {
      regs_.hw_write(offset, mask, value);
   }
   void hw_hold(std::uint32_t offset, std::uint32_t mask, bool level, std::uint64_t cycle);
   // Accepted SOF: clears frame_done (DEC-17; frame_start is a no-op since v1.07),
   // and performs pending commits of the gated configuration sets (DEC-26).
   void accepted_sof(std::uint64_t cycle);

   // Value of a configuration register as the frame starting now samples it.
   // No register is gated by an `updated` bit since CSR/MAS 07/10 (DEC-24/26/27
   // superseded), so this is the live value.
   std::uint32_t committed(std::uint32_t offset) const { return regs_.peek(offset); }

   void set_block_hook(std::uint32_t offset, block_hook hook);
   void set_soft_reset_hook(soft_reset_hook hook) { soft_reset_hook_ = std::move(hook); }
   const std::vector<command_record> &commands() const { return commands_; }
   void clear_commands() { commands_.clear(); }

   register_file &regs() { return regs_; }
   const register_file &regs() const { return regs_; }

   // Table contents (MAS v1.07 §6.14.4, §6.16.4.3; V107-4). The frame's
   // gamma curve bank (bank 0 in legacy mode, the active bank in profile
   // mode) and whether it is confirmed; an unconfirmed curve passes through.
   unsigned table_frame_bank() const { return profile_mode() ? tune_active_bank_ : 0u; }
   const std::array<std::uint16_t, 256> &gamma_lut() const { return gamma_banks_[table_frame_bank()].v; }
   bool gamma_frame_valid() const { return gamma_banks_[table_frame_bank()].valid; }
   // GTM manual curve of the frame's bank and its completeness (all 65 points,
   // both byte lanes).
   const pipe::gtm_curve &gtm_manual_curve() const { return gtm_manual_[table_frame_bank()].v; }
   bool gtm_manual_complete() const { return gtm_manual_[table_frame_bank()].complete(); }
   bool ee_resident_valid(unsigned p) const { return ee_.at(p).valid; }
   // Bank payloads of the frame's bank, used in profile mode (V107-5; the SEAL
   // rules guarantee they are confirmed when their enables are set).
   const std::uint32_t *bpc_factory_list(std::uint32_t &count) const;
   const std::uint16_t *lsc_sector_intervals(unsigned axis) const;
   const std::uint16_t *nr_curve() const;
   static constexpr unsigned nr_knots = 17, bpc_entries = 1024, lsc_sectors = 31;
   // EE resident profiles (MAS §6.18.4.1): tables of a profile (live: a write
   // reaches the rows processed after it), the profile programmed through
   // EE_CTRL.profile_wr_sel, and the set sampled for the frame in flight.
   static constexpr unsigned ee_profiles = 3;
   const std::array<std::uint16_t, 64> &ee_table(unsigned profile, unsigned bank) const {
      return ee_.at(profile).lut.at(bank);
   }
   const std::array<std::uint16_t, 64> &ee_table(unsigned bank) const { return ee_table(ee_frame_.profile, bank); }
   std::uint32_t ee_profile_scalar(unsigned profile, unsigned k) const { return ee_.at(profile).scalar.at(k); }
   const ee_frame_set &ee_frame() const { return ee_frame_; }
   // End of a frame EE processed: radial gain range event (MAS §6.18.4.1).
   void ee_end_of_frame();
   // GTM tone-curve banks (HAS §6.16.4, Table 6-49; ALG-GTM-05/07/08): the
   // datapath reads the read bank; the generator fills the other one, which
   // becomes the read bank at the next accepted SOF.
   const pipe::gtm_curve &gtm_read_bank() const { return gtm_banks_[gtm_read_]; }
   bool gtm_curve_valid() const { return gtm_valid_; }
   // End of frame: builds from the KEY, LWHITE and MANUAL captured at the
   // frame's accepted SOF (MAS v1.07 §6.16.4.3; ALG-GTM-09 superseded).
   void gtm_end_of_frame(std::uint32_t yavg);
   // Statistics (MAS v1.07 §6.21-6.23, §9.8). The sets of the frame just
   // accepted (valid after accepted_sof()); `en` is set only when the engine
   // admitted the measurement into a FREE bank with a legal configuration.
   const aec_config &aec_frame_config() const { return aec_sof_; }
   const awb_config &awb_frame_config() const { return awb_sof_; }
   const af_config &af_frame_config() const { return af_sof_; }
   // The legacy AEC set transferred by the latest commit (tests).
   const aec_config &aec_active() const { return aec_active_; }
   std::uint32_t frame_id() const { return frame_id_; }
   std::uint32_t frame_profile_id() const { return tune_active_id_; }  // profile ID of the latest admitted frame
   void debug_set_frame_counter(std::uint32_t value) { frame_counter_ = value; }  // test fixtures only
   // Result banks, for tests and debuggers.
   const result_banks<aec_payload> &aec_banks() const { return aec_banks_; }
   const result_banks<awb_payload> &awb_banks() const { return awb_banks_; }
   const result_banks<af_payload> &af_banks() const { return af_banks_; }
   // End of frame: publish the results of the engines that measured the frame.
   void stats_publish(const aec_payload *aec, const awb_payload *awb, const af_payload *af);
   // Frame abandoned before its end: no publication, busy bits cleared.
   void stats_abort();
   // Per-block STATUS.busy of the pixel blocks (Demosaic, CCM, Gamma, GTM,
   // 2DNR, EE, CNF, Resizer): 1 while a frame is inside the pipeline,
   // including while it is stalled (DEC-38).
   void blocks_busy(bool busy);
   // 2DNR noise-variance shadow (HAS §6.17.7.2): measured on frame N, used by N+1.
   std::uint32_t nr_variance() const { return nr_var_; }
   // An automatic estimate exists in the current epoch (MAS v1.07 §6.17.3.10:
   // MODE 0 falls back to SEED until then).
   bool nr_estimate_valid() const { return nr_have_; }
   void nr_publish(std::uint32_t var);

   // LSC resident profiles (MAS v1.07 §6.8.4): 2..32 nodes per axis (V107-A2),
   // not above the active image size (DEC-31 superseded).
   static constexpr unsigned lsc_profiles = 3;
   static constexpr std::uint32_t lsc_mesh_max = 32;
   const std::vector<std::uint32_t> &lsc_profile(unsigned idx) const { return lsc_.mesh.at(idx); }
   unsigned lsc_active_profile() const { return lsc_.active; }
   bool lsc_profile_valid(unsigned idx) const { return idx < lsc_profiles && lsc_.valid[idx]; }
   bool lsc_geometry_valid() const;

   static constexpr unsigned ee_bank_entries(unsigned bank) { return bank < 2 ? 64u : 32u; }

   // ---- Profile controller (MAS v1.07 §9.7-9.9; V107-3) ----------------------
   bool profile_mode() const;
   // TUNE_NEXT_FRAME: the ingress counter plus one, in both modes.
   std::uint32_t next_frame() const;
   // Admission of the next ingress frame on `cycle` with `in_flight` admitted,
   // unretired frames: go; hold until an event (a register write, a
   // retirement); or hold until the next edge (a request becomes eligible).
   enum class admission { go, wait_event, wait_edge };
   admission admission_state(std::uint64_t cycle, unsigned in_flight) const;
   static constexpr unsigned context_credits = 4;  // ingress contexts (§9.7)
   // An admitted frame has retired from every consumer (success or abort).
   void frame_retired();
   // Engine quiescence for the IDLE condition (§9.9.3): no admitted context
   // unretired and no AXI transaction outstanding. Installed by the TLM model.
   void set_quiescence_probe(std::function<bool()> probe) { quiescent_ = std::move(probe); }

   // Recovery (MAS v1.07 §9.9.8). Causes as COMMON_ABORTi_OUTCOME.CAUSE.
   enum recovery_cause : unsigned { rc_read = 1, rc_write = 2, rc_dma = 3, rc_common = 4, rc_malformed = 5 };
   // A recovery request on edge `cycle`: COMMON / DMA soft-reset commands,
   // a terminal AXI error (DMA scope) or a malformed frame. Accepted: capture
   // the abort set, abort the engines, drain, 15-clock pulse, final
   // retirement. While one is busy a software request is rejected with
   // TUNE_ERROR.PENDING and an error joins it. True when accepted.
   bool recovery_request(reset_scope scope, unsigned cause, std::uint64_t cycle);
   bool recovery_busy(std::uint64_t cycle) const;
   // The drain probe returns `never` while an accepted AXI transaction has no
   // response yet; the engines then report the edge on which the last one
   // completed (MAS v1.07 §9.9.8: the pulse follows the drain).
   static constexpr std::uint64_t never = ~std::uint64_t{0};
   void recovery_drained(std::uint64_t cycle);
   // Input / output ownership words: kept through a recovery's drain and
   // pulse, cleared at its final retirement (MAS v1.07 §9.9.8).
   static bool ownership_word(std::uint32_t offset);
   // The admitted frame `ingress` published DONE on channel `ch` (abort records).
   void output_done(std::uint32_t ingress, unsigned ch);
   // TLM: the edge by which every AXI transaction issued before `cycle` has
   // its response (the drain end); without a probe the drain is immediate.
   void set_drain_probe(std::function<std::uint64_t(std::uint64_t)> probe) { drain_probe_ = std::move(probe); }
   bool pipeline_idle() const;
   // Bank storage of a bank-owned word (bank-owned bits only), for tests.
   std::uint32_t profile_bank_value(unsigned bank, std::uint32_t offset) const;
   unsigned profile_bank_refs(unsigned bank) const { return pbanks_.at(bank).refs; }
   bool profile_bank_sealed(unsigned bank) const { return pbanks_.at(bank).sealed; }

private:
   void install_hooks();
   void lsc_load_ctrl(const register_file::write_effect &fx);
   void lsc_coef_data(const register_file::write_effect &fx);
   void lsc_profile_sel(const register_file::write_effect &fx);
   void lsc_error(std::uint32_t mask);
   std::uint32_t lsc_expected_count() const;
   resizer_key resizer_live_key(unsigned ch) const;
   void resizer_key_written();
   void geometry_sync(std::uint64_t t);  // advance the controllers; raise TUNE_ERROR.INVALID
   struct resizer_ctrl {  // configuration controller of one channel (MAS Figure 6-119)
      resizer_key live;             // key of the live registers
      std::uint64_t live_since = 0; // edge from which the live key is stable
      std::uint64_t not_before = 0; // end of a reset pulse: nothing starts earlier
      resizer_set published;
      bool calc = false;            // a derivation is running
      std::uint64_t e0 = 0;
      std::uint64_t free_at = 0;    // edge from which the dividers can start again
      resizer_key calc_key;         // the key saved at E0
      bool invalid_pending = false; // an invalid set was published since the last sync ...
      std::uint64_t invalid_edge = 0;  // ... on this edge (the latest)
      void advance(std::uint64_t t);  // process the edges before t
   };
   void dispatch(const register_file::write_effect &fx, std::uint64_t cycle);
   void apply_soft_reset(std::uint64_t cycle, reset_scope scope);
   void on_hw_set(std::uint32_t offset, std::uint32_t newly);
   std::uint32_t error_flags() const;
   void ee_sof();
   void ee_scalar_write(unsigned k, const register_file::write_effect &fx);
   void stats_install_hooks();
   void stats_reset(bool external);
   void stats_sof();
   aec_config aec_regs() const { return aec_set([this](std::uint32_t off) { return regs_.peek(off); }); }
   // The AEC set from the given word source (register file or a profile bank).
   aec_config aec_set(const std::function<std::uint32_t(std::uint32_t)> &rd) const;
   awb_config awb_regs() const;
   af_config af_regs() const;
   void awb_admit(const geometry &g);
   void awb_loss();
   void awb_result_ctrl(const register_file::write_effect &fx);
   bool awb_lease_readable() const;
   const result_banks<awb_payload>::bank *awb_read_bank() const;
   // AWB grid over the cell raster of the active image (MAS §6.22.3.7).
   static bool awb_grid_legal(std::uint32_t awb_ctrl, const geometry &g);
   template <class P>
   void lease_command(result_banks<P> &e, const register_file::write_effect &fx);
   template <class P>
   void lease_install_hooks(result_banks<P> &e, std::uint32_t cmd, std::uint32_t index, std::uint32_t lo,
                            std::uint32_t hi, std::uint32_t drops, std::uint32_t status,
                            std::uint64_t (*value)(const typename result_banks<P>::bank &, std::uint32_t, bool &));
   template <class P, class F>
   void leased_word(const result_banks<P> &e, std::uint32_t offset, F word);

   // Profile controller (control_unit_profile.cpp).
   struct profile_bank {
      std::vector<std::uint32_t> value;  // one word per profile_fields entry (bank-owned bits)
      bool sealed = false;
      unsigned refs = 0;                 // admitted, unretired frames using the bank
      std::uint32_t payload_valid = 0;   // TUNE_TABLE_STATUS NR/BPC/LSC bits (gamma: gamma_banks_)
      int ee_pin = -1;                   // EE resident pinned by SEAL until UNSEAL/exit (§9.9.1)
      int lsc_pin = -1;                  // LSC resident pinned by SEAL
   };
   struct pending_apply {
      bool valid = false;
      unsigned bank = 0;
      std::uint32_t id = 0;
      bool exact = false;
      std::uint32_t target = 0;
      std::uint64_t cycle = 0;                                       // acceptance edge
      std::uint64_t cancel = std::numeric_limits<std::uint64_t>::max();  // CANCEL edge
   };
   void profile_install_hooks();
   std::uint32_t profile_route_write(std::uint32_t addr, std::uint32_t data, std::uint32_t lanes);
   void profile_mode_change(bool enable);
   void profile_command(const register_file::write_effect &fx, std::uint64_t cycle);
   void profile_admit(std::uint64_t cycle);
   void profile_reset(reset_scope scope, bool hard);
   std::uint32_t seal_errors(unsigned bank) const;
   bool pending_at(std::uint64_t cycle) const;
   bool bank_active(unsigned bank) const;
   bool bank_protected(unsigned bank, std::uint64_t cycle) const;
   unsigned tune_bank() const;
   void tune_error(std::uint32_t mask);
   bool any_result_leased() const;

   // Tables (control_unit_tables.cpp).
   void tables_install_hooks();
   bool ee_resident_owned(unsigned p, std::uint64_t cycle) const;
   bool lsc_resident_owned(unsigned p, std::uint64_t cycle) const;
   bool payload_writable(unsigned bank);
   void bank_payload_invalidate(unsigned bank);
   void ee_invalidate_all();
   void ee_invalidate(unsigned p);
   void gamma_write(unsigned bank, std::uint32_t index, std::uint32_t value);
   unsigned ee_wr_resident() const;
   std::uint32_t ee_lut_index(unsigned &bank) const;
   void ee_confirm();
   void table_command(std::uint32_t cmd);

   register_file regs_;
   unsigned soft_reset_cycles_;
   std::uint64_t soft_reset_end_ = 0;
   std::uint64_t current_cycle_ = 0;
   static constexpr std::uint64_t settled = std::numeric_limits<std::uint64_t>::max();
   mutable std::uint64_t read_cycle_ = settled;  // cycle of the CSR read in progress
   std::uint64_t time_base_epoch_ = settled;     // release of i_rst_n; settled while asserted
   std::map<std::uint32_t, block_hook> block_hooks_;
   soft_reset_hook soft_reset_hook_;
   std::vector<command_record> commands_;

   std::array<resizer_ctrl, num_channels> rz_;  // advanced at accesses and SOFs; observations advance a copy
   std::array<resizer_set, num_channels> rz_frame_;
   std::uint32_t outputs_frame_ = 1;

   struct lsc_state {
      std::array<std::vector<std::uint32_t>, 3> mesh;  // committed profiles
      std::array<bool, 3> valid{};
      std::vector<std::uint32_t> staging;              // load in progress (ALG-LSC-04)
      bool busy = false;
      std::uint32_t count = 0;                         // >= 17 bits (ALG-LSC-07)
      unsigned dest = 0;
      unsigned active = 0;                             // adopted at SOF (ALG-LSC-06)
   } lsc_;

   struct ee_profile {
      std::array<std::uint32_t, ee_num_scalars> scalar{};
      std::array<std::array<std::uint16_t, 64>, 4> lut{};
      std::bitset<192> written;  // LUT entries written since the last invalidation (§9.9.1)
      bool valid = false;        // confirmed resident
      std::uint64_t write_cycle = std::numeric_limits<std::uint64_t>::max();
   };
   struct table_bank {  // a gamma curve bank (§6.14.4.2)
      std::array<std::uint16_t, 256> v{};
      std::bitset<256> written;
      bool valid = false;
      std::uint64_t write_cycle = std::numeric_limits<std::uint64_t>::max();
   };
   struct gtm_manual {  // a manual GTM curve bank (§6.16.4.3)
      pipe::gtm_curve v{};
      std::array<std::uint8_t, 65> lanes{};  // bit 0 low byte, bit 1 high byte written
      bool complete() const {
         return std::all_of(lanes.begin(), lanes.end(), [](std::uint8_t l) { return l == 3u; });
      }
   };
   struct table_payload {  // bank payloads used by the v1.07 kernels (V107-5)
      std::array<std::uint16_t, nr_knots> nr_knot{};
      std::bitset<nr_knots> nr_written;
      std::array<std::uint32_t, bpc_entries> bpc{};
      std::bitset<bpc_entries> bpc_written;
      std::array<std::array<std::uint16_t, lsc_sectors>, 2> sector{};
      std::array<std::bitset<lsc_sectors>, 2> sector_written;
   };
   std::array<ee_profile, ee_profiles> ee_{};
   std::array<table_bank, 2> gamma_banks_{};
   std::array<gtm_manual, 2> gtm_manual_{};
   std::array<table_payload, 2> tbank_{};
   unsigned ee_active_profile_ = 0;  // EE_STATUS.active_profile: resident of the latest local SOF
   bool ee_active_valid_ = false;
   ee_frame_set ee_frame_{};
   std::array<pipe::gtm_curve, 2> gtm_banks_{};
   unsigned gtm_read_ = 0;
   bool gtm_valid_ = false;    // a curve exists (hard reset: pass-through, p112)
   bool gtm_pending_ = false;  // a rebuilt bank waits for the next SOF
   struct gtm_capture {        // curve-build controls of the frame, captured at its SOF
      bool manual = false;
      std::uint32_t key = 0, lwhite = 0;
   } gtm_sof_{};
   std::uint32_t nr_var_ = 0;
   bool nr_have_ = false;

   // Statistics state.
   aec_config aec_active_{};  // legacy shadow set transferred by the latest commit
   aec_config aec_sof_{};     // the set of the frame just accepted
   awb_config awb_sof_{};
   af_config af_sof_{};
   std::uint32_t frame_counter_ = 0;  // frames started since reset (#316)
   std::uint32_t frame_id_ = 0;       // bound to the frame in flight (ALG-STAT-01)
   result_banks<aec_payload> aec_banks_{};
   result_banks<af_payload> af_banks_{};
   result_banks<awb_payload> awb_banks_{};
   // AWB loss reporting (MAS §6.22.4.1): OVERRUN and DROP_COUNT, cleared by
   // CLEAR_REPORT; losses on the CLEAR_REPORT edge are kept.
   bool awb_overrun_ = false;
   std::uint32_t awb_drops_ = 0;
   std::uint64_t awb_loss_cycle_ = settled;
   std::uint32_t awb_loss_count_ = 0;
   // AWB zone read port, synchronous (MAS §6.22.4.3): on the edge that writes
   // AWB_ZONE_ADDR the data still belongs to the previous address.
   std::uint32_t awb_zone_prev_ = 0;
   std::uint64_t awb_zone_cycle_ = settled;

   std::array<profile_bank, 2> pbanks_{};
   pending_apply pending_{};
   bool tune_active_valid_ = false;
   unsigned tune_active_bank_ = 0;
   std::uint32_t tune_active_id_ = 0;     // profile ID of the latest admitted frame
   std::uint32_t tune_active_frame_ = 0;  // sequence at which the active profile became active
   std::uint32_t epoch_id_ = 0;           // profile ID captured by the activating APPLY
   std::deque<int> frame_tokens_;         // bank per admitted, unretired frame; -1 legacy
   // Admitted, unretired frames for the recovery records (admission order).
   struct live_frame {
      std::uint32_t ingress = 0, outputs = 0, done = 0;
   };
   std::deque<live_frame> live_frames_;
   struct abort_record {
      std::uint32_t frame = 0, failed = 0, cause = 0;
   };
   struct recovery_state {
      bool busy = false;
      reset_scope scope = reset_scope::dma;
      std::uint64_t request = 0, pulse = 0, final_edge = 0;  // accepted, pulse start, final retirement
      bool pulse_applied = false;
      std::array<abort_record, 4> records{};
      unsigned count = 0;
   } rec_;
   std::uint32_t rec_seq_ = 0;                 // COMMON_RECOVERY_SEQUENCE
   std::array<abort_record, 4> rec_records_{}; // published records
   unsigned rec_count_ = 0;
   std::function<std::uint64_t(std::uint64_t)> drain_probe_;
   void recovery_sync(std::uint64_t t);  // apply the pulse start and the final retirement due by t
   void recovery_final(std::uint64_t cycle);
   void recovery_install_hooks();
   std::unordered_map<std::uint32_t, std::size_t> pfield_index_;
   std::function<bool()> quiescent_;
};

}  // namespace cdc::components::fx1_isp
