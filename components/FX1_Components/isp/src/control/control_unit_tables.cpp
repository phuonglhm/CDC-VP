// SPDX-License-Identifier: Apache-2.0
// Table payloads and resident ownership of CSR/MAS v1.07 (milestone V107-4):
//   - Gamma: two 256-entry shared-RGB curve banks with per-entry write flags;
//     legacy LUT_LOAD_CONFIRM (EN = 0, pipeline idle) and profile GAMMA_CONFIRM
//     (§6.14.4); an unconfirmed curve passes pixels through.
//   - GTM: two manual 65-point banks with both-byte-lane tracking, separate
//     from the automatic ping-pong curves (§6.16.4.3).
//   - EE: three residents with 192 LUT write flags, confirmation, code-3
//     readback of resident 0 and SEAL pins (§9.9.1).
//   - Bank payloads: NR curve (17 knots), BPC factory list (<= 1024 points),
//     LSC sector intervals; TUNE_TABLE_CMD confirmation (§9.9.5, Table 9-197).
// The datapath use of the NR curve, the BPC list and the LSC sectors lands
// with the v1.07 kernels in V107-5.

#include <algorithm>

#include "control/control_unit.h"
#include "fx1_isp/fx1_isp_csr.h"

namespace cdc::components::fx1_isp {

namespace {

constexpr std::uint32_t err_locked = FX1_ISP_TUNE_ERROR_LOCKED_MASK;
constexpr std::uint32_t err_not_ready = FX1_ISP_TUNE_ERROR_NOT_READY_MASK;
constexpr std::uint32_t err_mode = FX1_ISP_TUNE_ERROR_MODE_MASK;

std::uint32_t fld(std::uint32_t v, std::uint32_t mask) {
   return mask ? (v & mask) >> __builtin_ctz(mask) : 0u;
}

// EE resident LUT entry index 0..191: LUMA 0..63, ACT 64..127, CPOS 128..159, CNEG 160..191.
unsigned ee_flag(unsigned bank, unsigned idx) {
   static constexpr unsigned base[4] = {0, 64, 128, 160};
   return base[bank] + idx;
}

}  // namespace

// ---- ownership ------------------------------------------------------------------

bool control_unit::ee_resident_owned(unsigned p, std::uint64_t cycle) const {
   if (!profile_mode()) {
      return false;  // legacy mode: confirmation needs EE disabled and idle instead
   }
   for (unsigned b = 0; b < 2; ++b) {
      if (pbanks_[b].ee_pin == static_cast<int>(p) && bank_protected(b, cycle)) {
         return true;  // sealed (and so active/pending/referenced) owner (§9.9.1)
      }
   }
   return false;
}

bool control_unit::lsc_resident_owned(unsigned p, std::uint64_t cycle) const {
   if (!profile_mode()) {
      return false;
   }
   for (unsigned b = 0; b < 2; ++b) {
      if (pbanks_[b].lsc_pin == static_cast<int>(p) && bank_protected(b, cycle)) {
         return true;
      }
   }
   return false;
}

// A bank payload port write: LOCKED (and no effect) for a protected bank in
// profile mode; in legacy mode the extension payloads are storage (§9.9.2).
bool control_unit::payload_writable(unsigned b) {
   if (profile_mode() && bank_protected(b, current_cycle_)) {
      tune_error(err_locked);
      return false;
   }
   return true;
}

void control_unit::bank_payload_invalidate(unsigned b) {
   // INIT, leaving profile mode, profile-mode common reset, hard reset: write
   // flags and confirmations go; payload storage is not reset (§9.7, §9.9.5).
   gamma_banks_[b].written.reset();
   gamma_banks_[b].valid = false;
   gtm_manual_[b].lanes.fill(0);
   tbank_[b].nr_written.reset();
   tbank_[b].bpc_written.reset();
   tbank_[b].sector_written[0].reset();
   tbank_[b].sector_written[1].reset();
   pbanks_[b].payload_valid = 0;
}

void control_unit::ee_invalidate_all() {
   for (ee_profile &p : ee_) {
      p.written.reset();
      p.valid = false;
   }
   ee_active_valid_ = false;
}

// A resident loses its confirmation; EE_STATUS.active_valid clears with it
// when it is the active selection, until the next local SOF (MAS v1.07
// §9.9.1). The frame in flight keeps the set it captured at its SOF.
void control_unit::ee_invalidate(unsigned p) {
   ee_[p].valid = false;
   if (p == ee_active_profile_) {
      ee_active_valid_ = false;
   }
}

// ---- hooks --------------------------------------------------------------------

void control_unit::tables_install_hooks() {
   // Gamma legacy port (§6.14.4.1): bank 0, address auto-increment.
   regs_.on_write(FX1_ISP_GAMMA_LUT_DATA_OFFSET, [this](const register_file::write_effect &fx) {
      const std::uint32_t addr = regs_.peek(FX1_ISP_GAMMA_LUT_ADDR_OFFSET) & FX1_ISP_GAMMA_LUT_ADDR_ADDR_MASK;
      if (payload_writable(0)) {
         gamma_write(0, addr, fx.new_value & FX1_ISP_GAMMA_LUT_DATA_DATA_MASK);
      }
      regs_.hw_write(FX1_ISP_GAMMA_LUT_ADDR_OFFSET, FX1_ISP_GAMMA_LUT_ADDR_ADDR_MASK, addr + 1u);
   });
   regs_.on_read(FX1_ISP_GAMMA_LUT_RDATA_OFFSET, [this](std::uint32_t) -> std::uint32_t {
      const std::uint32_t addr = regs_.peek(FX1_ISP_GAMMA_LUT_ADDR_OFFSET) & FX1_ISP_GAMMA_LUT_ADDR_ADDR_MASK;
      return gamma_banks_[0].written[addr] ? gamma_banks_[0].v[addr] : 0u;  // unwritten entries read zero
   });
   // Banked gamma port (§6.14.4.2): TUNE_BANK, explicit index, both modes.
   regs_.on_write(FX1_ISP_TUNE_GAMMA_DATA_OFFSET, [this](const register_file::write_effect &fx) {
      const unsigned b = tune_bank();
      if (payload_writable(b)) {
         gamma_write(b, regs_.peek(FX1_ISP_TUNE_GAMMA_PORT_OFFSET) & FX1_ISP_TUNE_GAMMA_PORT_INDEX_MASK,
                     fx.new_value & FX1_ISP_TUNE_GAMMA_DATA_VALUE_MASK);
      }
   });
   regs_.on_read(FX1_ISP_TUNE_GAMMA_DATA_OFFSET, [this](std::uint32_t) -> std::uint32_t {
      const table_bank &g = gamma_banks_[tune_bank()];
      const std::uint32_t i = regs_.peek(FX1_ISP_TUNE_GAMMA_PORT_OFFSET) & FX1_ISP_TUNE_GAMMA_PORT_INDEX_MASK;
      return g.written[i] ? g.v[i] : 0u;
   });
   // Legacy confirmation (§6.14.4): EN = 0, pipeline idle, all 256 entries,
   // no entry written on the same edge. Rejected with MODE in profile mode.
   regs_.on_write(FX1_ISP_GAMMA_CTRL_OFFSET, [this](const register_file::write_effect &fx) {
      if (!(fx.w1p_pulse & FX1_ISP_GAMMA_CTRL_LUT_LOAD_CONFIRM_MASK)) {
         return;
      }
      if (profile_mode()) {
         tune_error(err_mode);
         return;
      }
      // EN = 0 before the edge: a write that clears EN and confirms is refused (V107-A15).
      table_bank &g = gamma_banks_[0];
      if (!(fx.old_value & FX1_ISP_GAMMA_CTRL_EN_MASK) && pipeline_idle() &&
          g.written.all() && g.write_cycle != current_cycle_) {
         g.valid = true;
      }
   });
   regs_.on_read(FX1_ISP_GAMMA_STATUS_OFFSET, [this](std::uint32_t stored) -> std::uint32_t {
      // LUT_LOADED is the validity of curve bank 0 in both modes (§6.14.4.2).
      return (stored & ~FX1_ISP_GAMMA_STATUS_LUT_LOADED_MASK) |
             (gamma_banks_[0].valid ? FX1_ISP_GAMMA_STATUS_LUT_LOADED_MASK : 0u);
   });

   // GTM manual curves (§6.16.4.2-6.16.4.3): profile mode the TUNE_BANK bank,
   // legacy mode bank 0; written only while GTM is disabled in that profile;
   // both byte lanes of a point are tracked; address increments on every write.
   regs_.on_write(FX1_ISP_GTM_LUT_DATA_OFFSET, [this](const register_file::write_effect &fx) {
      const std::uint32_t addr = regs_.peek(FX1_ISP_GTM_LUT_ADDR_OFFSET) & FX1_ISP_GTM_LUT_ADDR_ADDR_MASK;
      const bool prof = profile_mode();
      const unsigned b = prof ? tune_bank() : 0u;
      const std::uint32_t ctrl = prof ? profile_bank_value(b, FX1_ISP_GTM_CTRL_OFFSET)
                                      : regs_.peek(FX1_ISP_GTM_CTRL_OFFSET);
      if (addr < 65 && !(ctrl & FX1_ISP_GTM_CTRL_EN_MASK) && payload_writable(b)) {
         gtm_manual &m = gtm_manual_[b];
         const std::uint32_t lanes = fx.lane_mask & 0xFFFFu;
         m.v[addr] = static_cast<std::uint16_t>((m.v[addr] & ~lanes) | (fx.data & lanes));
         m.lanes[addr] |= static_cast<std::uint8_t>(((lanes & 0x00FFu) ? 1u : 0u) | ((lanes & 0xFF00u) ? 2u : 0u));
      }
      regs_.hw_write(FX1_ISP_GTM_LUT_ADDR_OFFSET, FX1_ISP_GTM_LUT_ADDR_ADDR_MASK, addr + 1u);
   });
   regs_.on_read(FX1_ISP_GTM_LUT_RDATA_OFFSET, [this](std::uint32_t) -> std::uint32_t {
      const std::uint32_t addr = regs_.peek(FX1_ISP_GTM_LUT_ADDR_OFFSET) & FX1_ISP_GTM_LUT_ADDR_ADDR_MASK;
      if (addr >= 65) {
         return 0u;
      }
      const bool prof = profile_mode();
      if (!prof && !(regs_.peek(FX1_ISP_GTM_CTRL_OFFSET) & FX1_ISP_GTM_CTRL_MANUAL_MASK)) {
         return gtm_banks_[gtm_read_][addr];  // the most recently published automatic curve
      }
      const gtm_manual &m = gtm_manual_[prof ? tune_bank() : 0u];
      return m.lanes[addr] == 3u ? m.v[addr] : 0u;  // incomplete entries read zero
   });

   // EE residents (§9.9.1): scalars and LUT reads use the PROFILE_WR_SEL
   // resident (code 3 reads resident 0 and discards writes; audit R2).
   for (unsigned k = 0; k < ee_num_scalars; ++k) {
      regs_.on_write(FX1_ISP_EE_ALPHA_OFFSET + 4u * k,
                     [this, k](const register_file::write_effect &fx) { ee_scalar_write(k, fx); });
      regs_.on_read(FX1_ISP_EE_ALPHA_OFFSET + 4u * k,
                    [this, k](std::uint32_t) -> std::uint32_t { return ee_[ee_wr_resident() % 3u].scalar[k]; });
   }
   regs_.on_write(FX1_ISP_EE_LUT_WDATA_OFFSET, [this](const register_file::write_effect &fx) {
      unsigned bank = 0;
      const std::uint32_t idx = ee_lut_index(bank);
      const unsigned p = ee_wr_resident();
      if (p < ee_profiles) {
         if (ee_resident_owned(p, current_cycle_)) {
            tune_error(err_locked);
         } else {
            ee_[p].lut[bank][idx] = static_cast<std::uint16_t>(fx.new_value & FX1_ISP_EE_LUT_WDATA_LUT_WDATA_MASK);
            ee_[p].written.set(ee_flag(bank, idx));
            ee_invalidate(p);
            ee_[p].write_cycle = current_cycle_;
         }
      }
      // The address advances on every write, also one discarded (U3-A2).
      const std::uint32_t addr = regs_.peek(FX1_ISP_EE_LUT_ADDR_OFFSET);
      regs_.hw_write(FX1_ISP_EE_LUT_ADDR_OFFSET, FX1_ISP_EE_LUT_ADDR_LUT_ADDR_MASK, addr + 1u);
   });
   regs_.on_read(FX1_ISP_EE_LUT_RDATA_OFFSET, [this](std::uint32_t) -> std::uint32_t {
      unsigned bank = 0;
      const std::uint32_t idx = ee_lut_index(bank);
      return ee_[ee_wr_resident() % 3u].lut[bank][idx];  // code 3: resident 0 (§9.9.1, audit R2)
   });
   regs_.on_write(FX1_ISP_EE_LUT_CTRL_OFFSET, [this](const register_file::write_effect &fx) {
      if (fx.w1p_pulse & FX1_ISP_EE_LUT_CTRL_PROFILE_LOAD_CONFIRM_MASK) {
         ee_confirm();
      }
   });
   regs_.on_read(FX1_ISP_EE_STATUS_OFFSET, [this](std::uint32_t stored) -> std::uint32_t {
      std::uint32_t v = stored & ~(FX1_ISP_EE_STATUS_LUT_VALID_MASK | FX1_ISP_EE_STATUS_ACTIVE_PROFILE_MASK |
                                   FX1_ISP_EE_STATUS_ACTIVE_VALID_MASK);
      for (unsigned p = 0; p < ee_profiles; ++p) {
         v |= ee_[p].valid ? (1u << (FX1_ISP_EE_STATUS_LUT_VALID_SHIFT + p)) : 0u;
      }
      v |= ee_active_profile_ << FX1_ISP_EE_STATUS_ACTIVE_PROFILE_SHIFT;
      v |= ee_active_valid_ ? FX1_ISP_EE_STATUS_ACTIVE_VALID_MASK : 0u;
      return v;
   });

   // Bank payload ports (TUNE_BANK, explicit index, both modes; Table 9-195).
   regs_.on_write(FX1_ISP_TUNE_NR_LUT_DATA_OFFSET, [this](const register_file::write_effect &fx) {
      const unsigned b = tune_bank();
      const std::uint32_t i = regs_.peek(FX1_ISP_TUNE_NR_LUT_ADDR_OFFSET) & FX1_ISP_TUNE_NR_LUT_ADDR_VALUE_MASK;
      if (i < nr_knots && payload_writable(b)) {
         tbank_[b].nr_knot[i] = static_cast<std::uint16_t>(fx.new_value & FX1_ISP_TUNE_NR_LUT_DATA_VALUE_MASK);
         tbank_[b].nr_written.set(i);
         pbanks_[b].payload_valid &= ~FX1_ISP_TUNE_TABLE_STATUS_NR_MASK;
      }
   });
   regs_.on_read(FX1_ISP_TUNE_NR_LUT_DATA_OFFSET, [this](std::uint32_t) -> std::uint32_t {
      const table_payload &t = tbank_[tune_bank()];
      const std::uint32_t i = regs_.peek(FX1_ISP_TUNE_NR_LUT_ADDR_OFFSET) & FX1_ISP_TUNE_NR_LUT_ADDR_VALUE_MASK;
      return i < nr_knots && t.nr_written[i] ? t.nr_knot[i] : 0u;
   });
   regs_.on_write(FX1_ISP_TUNE_BPC_DATA_OFFSET, [this](const register_file::write_effect &fx) {
      const unsigned b = tune_bank();
      const std::uint32_t i = regs_.peek(FX1_ISP_TUNE_BPC_ADDR_OFFSET) & FX1_ISP_TUNE_BPC_ADDR_VALUE_MASK;
      if (payload_writable(b)) {
         tbank_[b].bpc[i] = fx.new_value;
         tbank_[b].bpc_written.set(i);
         pbanks_[b].payload_valid &= ~FX1_ISP_TUNE_TABLE_STATUS_BPC_MASK;
      }
   });
   regs_.on_read(FX1_ISP_TUNE_BPC_DATA_OFFSET, [this](std::uint32_t) -> std::uint32_t {
      const table_payload &t = tbank_[tune_bank()];
      const std::uint32_t i = regs_.peek(FX1_ISP_TUNE_BPC_ADDR_OFFSET) & FX1_ISP_TUNE_BPC_ADDR_VALUE_MASK;
      return t.bpc_written[i] ? t.bpc[i] : 0u;
   });
   regs_.on_write(FX1_ISP_TUNE_LSC_SECTOR_DATA_OFFSET, [this](const register_file::write_effect &fx) {
      const unsigned b = tune_bank();
      const std::uint32_t a = regs_.peek(FX1_ISP_TUNE_LSC_SECTOR_ADDR_OFFSET);
      const unsigned axis = fld(a, FX1_ISP_TUNE_LSC_SECTOR_ADDR_AXIS_MASK);
      const unsigned i = fld(a, FX1_ISP_TUNE_LSC_SECTOR_ADDR_INDEX_MASK);
      if (i < lsc_sectors && payload_writable(b)) {
         tbank_[b].sector[axis][i] = static_cast<std::uint16_t>(fx.new_value & FX1_ISP_TUNE_LSC_SECTOR_DATA_VALUE_MASK);
         tbank_[b].sector_written[axis].set(i);
         pbanks_[b].payload_valid &= ~FX1_ISP_TUNE_TABLE_STATUS_LSC_MASK;
      }
   });
   regs_.on_read(FX1_ISP_TUNE_LSC_SECTOR_DATA_OFFSET, [this](std::uint32_t) -> std::uint32_t {
      const table_payload &t = tbank_[tune_bank()];
      const std::uint32_t a = regs_.peek(FX1_ISP_TUNE_LSC_SECTOR_ADDR_OFFSET);
      const unsigned axis = fld(a, FX1_ISP_TUNE_LSC_SECTOR_ADDR_AXIS_MASK);
      const unsigned i = fld(a, FX1_ISP_TUNE_LSC_SECTOR_ADDR_INDEX_MASK);
      return i < lsc_sectors && t.sector_written[axis][i] ? t.sector[axis][i] : 0u;
   });
   regs_.on_read(FX1_ISP_TUNE_TABLE_STATUS_OFFSET, [this](std::uint32_t) -> std::uint32_t {
      const unsigned b = tune_bank();
      return (pbanks_[b].payload_valid & ~FX1_ISP_TUNE_TABLE_STATUS_GAMMA_MASK) |
             (gamma_banks_[b].valid ? FX1_ISP_TUNE_TABLE_STATUS_GAMMA_MASK : 0u);
   });
}

// ---- frame payloads ------------------------------------------------------------------

const std::uint32_t *control_unit::bpc_factory_list(std::uint32_t &count) const {
   const unsigned b = table_frame_bank();
   count = std::min<std::uint32_t>(profile_bank_value(b, FX1_ISP_TUNE_BPC_COUNT_OFFSET), bpc_entries);
   return tbank_[b].bpc.data();
}

const std::uint16_t *control_unit::lsc_sector_intervals(unsigned axis) const {
   return tbank_[table_frame_bank()].sector[axis & 1u].data();
}

const std::uint16_t *control_unit::nr_curve() const { return tbank_[table_frame_bank()].nr_knot.data(); }

// ---- writes, confirmations --------------------------------------------------------

void control_unit::gamma_write(unsigned b, std::uint32_t index, std::uint32_t value) {
   table_bank &g = gamma_banks_[b];
   g.v[index] = static_cast<std::uint16_t>(value);
   g.written.set(index);
   g.valid = false;  // every write clears the confirmation, also a coincident one
   g.write_cycle = current_cycle_;
}

unsigned control_unit::ee_wr_resident() const {
   return fld(regs_.peek(FX1_ISP_EE_CTRL_OFFSET), FX1_ISP_EE_CTRL_PROFILE_WR_SEL_MASK);
}

std::uint32_t control_unit::ee_lut_index(unsigned &bank) const {
   bank = fld(regs_.peek(FX1_ISP_EE_LUT_CTRL_OFFSET), FX1_ISP_EE_LUT_CTRL_LUT_SEL_MASK);
   return regs_.peek(FX1_ISP_EE_LUT_ADDR_OFFSET) & (ee_bank_entries(bank) - 1u);
}

void control_unit::ee_scalar_write(unsigned k, const register_file::write_effect &fx) {
   const unsigned p = ee_wr_resident();
   if (p >= ee_profiles) {
      return;  // code 3: discarded
   }
   if (ee_resident_owned(p, current_cycle_)) {
      tune_error(err_locked);
      return;
   }
   // A masked write merges with the selected resident's value (§9.9.1).
   ee_[p].scalar[k] = (ee_[p].scalar[k] & ~fx.lane_mask) | (fx.data & fx.lane_mask);
   ee_invalidate(p);
   ee_[p].write_cycle = current_cycle_;
}

void control_unit::ee_confirm() {
   const unsigned p = ee_wr_resident();
   if (p >= ee_profiles) {
      return;  // reserved selector: nothing to confirm
   }
   if (profile_mode()) {
      if (ee_resident_owned(p, current_cycle_)) {
         tune_error(err_locked);
         return;
      }
   } else if ((regs_.peek(FX1_ISP_EE_CTRL_OFFSET) & FX1_ISP_EE_CTRL_EN_MASK) || !pipeline_idle()) {
      return;  // legacy mode: EE disabled and idle (§9.9.1)
   }
   ee_profile &r = ee_[p];
   if (r.written.all() && r.write_cycle != current_cycle_) {
      r.valid = true;
   } else {
      ee_invalidate(p);  // a same-edge write wins; missing flags: NOT_READY
      tune_error(err_not_ready);
   }
}

void control_unit::table_command(std::uint32_t cmd) {
   const unsigned b = tune_bank();
   if (bank_protected(b, current_cycle_)) {
      tune_error(err_locked);  // sealed, pending or referenced bank: no effect (§9.9.5)
      return;
   }
   const geometry act = active_geometry();
   std::uint32_t &ok = pbanks_[b].payload_valid;
   auto confirm = [&](std::uint32_t bit, bool pass) {
      if (pass) {
         ok |= bit;
      } else {
         ok &= ~bit;
         tune_error(err_not_ready);
      }
   };
   if (cmd & FX1_ISP_TUNE_TABLE_CMD_GAMMA_CONFIRM_MASK) {
      table_bank &g = gamma_banks_[b];
      g.valid = g.written.all() && g.write_cycle != current_cycle_;
      if (!g.valid) {
         tune_error(err_not_ready);
      }
   }
   if (cmd & FX1_ISP_TUNE_TABLE_CMD_NR_CONFIRM_MASK) {
      confirm(FX1_ISP_TUNE_TABLE_STATUS_NR_MASK, tbank_[b].nr_written.all());
   }
   if (cmd & FX1_ISP_TUNE_TABLE_CMD_BPC_CONFIRM_MASK) {
      // Count <= 1024; entries 0..COUNT-1 written; strictly increasing raster
      // order Y*W + X (so no duplicate); X < W and Y < H (Table 9-197).
      const std::uint32_t count = profile_bank_value(b, FX1_ISP_TUNE_BPC_COUNT_OFFSET);
      bool pass = count <= bpc_entries;
      std::uint64_t prev = 0;
      for (std::uint32_t i = 0; pass && i < count; ++i) {
         const std::uint32_t x = tbank_[b].bpc[i] & 0xFFFFu, y = tbank_[b].bpc[i] >> 16;
         const std::uint64_t pos = std::uint64_t{y} * act.width + x;
         pass = tbank_[b].bpc_written[i] && x < act.width && y < act.height && (i == 0 || pos > prev);
         prev = pos;
      }
      confirm(FX1_ISP_TUNE_TABLE_STATUS_BPC_MASK, pass);
   }
   if (cmd & FX1_ISP_TUNE_TABLE_CMD_LSC_CONFIRM_MASK) {
      // Every interval of each axis written and nonzero, summing to the shared
      // mesh span: nodes - 1 intervals covering W - 1 / H - 1 (§6.8.3.9).
      const std::uint32_t nodes = regs_.peek(FX1_ISP_LSC_MESH_NODES_OFFSET);
      const unsigned n[2] = {fld(nodes, FX1_ISP_LSC_MESH_NODES_MESH_NX_MASK),
                             fld(nodes, FX1_ISP_LSC_MESH_NODES_MESH_NY_MASK)};
      const std::uint32_t span[2] = {act.width - 1u, act.height - 1u};
      bool pass = act.width > 1 && act.height > 1;
      for (unsigned axis = 0; pass && axis < 2; ++axis) {
         if (n[axis] < 2 || n[axis] - 1u > lsc_sectors) {
            pass = false;
            break;
         }
         std::uint32_t sum = 0;
         for (unsigned i = 0; pass && i + 1u < n[axis]; ++i) {
            pass = tbank_[b].sector_written[axis][i] && tbank_[b].sector[axis][i] != 0;
            sum += tbank_[b].sector[axis][i];
         }
         pass = pass && sum == span[axis];
      }
      confirm(FX1_ISP_TUNE_TABLE_STATUS_LSC_MASK, pass);
   }
}

}  // namespace cdc::components::fx1_isp
