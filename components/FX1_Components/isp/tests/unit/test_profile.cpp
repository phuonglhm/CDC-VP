// SPDX-License-Identifier: Apache-2.0
// Profile controller (MAS v1.07 §9.7-9.9, milestone V107-3), on the control
// unit: banks and readback, INIT/SEAL/APPLY/CANCEL/UNSEAL, the APPLY error
// priority and edge ordering, EXACT targets modulo 2^32, admission (no
// profile, drain barrier, no output, credits), references, resets, the IDLE
// write class and legacy-mode MODE errors. Expected values are taken from the
// MAS text (sections cited per test), not from the implementation.

#include <cstdint>
#include <functional>
#include <string>

#include "control/control_unit.h"
#include "control/ee_types.h"
#include "fx1_check.h"
#include "fx1_isp/fx1_isp_csr.h"

using namespace cdc::components::fx1_isp;

namespace {

constexpr std::uint32_t all = 0xFFFFFFFFu;
constexpr std::uint32_t e_locked = FX1_ISP_TUNE_ERROR_LOCKED_MASK;
constexpr std::uint32_t e_invalid = FX1_ISP_TUNE_ERROR_INVALID_MASK;
constexpr std::uint32_t e_not_ready = FX1_ISP_TUNE_ERROR_NOT_READY_MASK;
constexpr std::uint32_t e_pending = FX1_ISP_TUNE_ERROR_PENDING_MASK;
constexpr std::uint32_t e_missed = FX1_ISP_TUNE_ERROR_MISSED_MASK;
constexpr std::uint32_t e_mode = FX1_ISP_TUNE_ERROR_MODE_MASK;
constexpr std::uint32_t c_init = FX1_ISP_TUNE_CMD_INIT_MASK;
constexpr std::uint32_t c_seal = FX1_ISP_TUNE_CMD_SEAL_MASK;
constexpr std::uint32_t c_apply = FX1_ISP_TUNE_CMD_APPLY_MASK;
constexpr std::uint32_t c_cancel = FX1_ISP_TUNE_CMD_CANCEL_MASK;
constexpr std::uint32_t c_unseal = FX1_ISP_TUNE_CMD_UNSEAL_MASK;
constexpr std::uint32_t c_exact = FX1_ISP_TUNE_CMD_EXACT_MASK;

struct rig {
   control_unit cu;
   std::uint64_t t = 10;
   unsigned in_flight = 0;
   void wr(std::uint32_t off, std::uint32_t v) { cu.csr_write(off, v, all, t++); }
   std::uint32_t rd(std::uint32_t off) { return cu.csr_read(off); }
   std::uint32_t err() {
      const std::uint32_t e = rd(FX1_ISP_TUNE_ERROR_OFFSET);
      wr(FX1_ISP_TUNE_ERROR_OFFSET, e);  // W1C
      return e;
   }
   void cmd(std::uint32_t c) { wr(FX1_ISP_TUNE_CMD_OFFSET, c); }
   // Admit one ingress frame if the controller lets it, on edge t.
   bool admit() {
      if (cu.admission_state(t, in_flight) != control_unit::admission::go) {
         return false;
      }
      cu.accepted_sof(t++);
      ++in_flight;
      return true;
   }
   void retire() {
      cu.frame_retired();
      --in_flight;
   }
   void enter_profile_mode() { wr(FX1_ISP_TUNE_MODE_OFFSET, FX1_ISP_TUNE_MODE_ENABLE_MASK); }
   // INIT and SEAL bank b with profile ID `id`.
   void prepare(unsigned b, std::uint32_t id) {
      wr(FX1_ISP_TUNE_BANK_OFFSET, b);
      cmd(c_init);
      wr(FX1_ISP_TUNE_PROFILE_ID_OFFSET, id);
      cmd(c_seal);
   }
   std::uint32_t status() { return rd(FX1_ISP_TUNE_STATUS_OFFSET); }
};

// §9.9.2, Table 9-195: legacy mode rejects every profile command with MODE
// only; extension controls are bank storage read back through TUNE_BANK; the
// datapath keeps their reset value. TUNE_NEXT_FRAME works in both modes.
void test_legacy_mode() {
   rig r;
   FX1_CHECK_EQ(r.rd(FX1_ISP_TUNE_NEXT_FRAME_OFFSET), 1u);
   r.cmd(c_init | c_seal);  // even an invalid combination: MODE only
   FX1_CHECK_EQ(r.err(), e_mode);
   r.wr(FX1_ISP_TUNE_TABLE_CMD_OFFSET, FX1_ISP_TUNE_TABLE_CMD_GAMMA_CONFIRM_MASK);
   FX1_CHECK_EQ(r.err(), e_mode);
   r.wr(FX1_ISP_TUNE_WB_GB_OFFSET, 0x123);  // bank 0
   r.wr(FX1_ISP_TUNE_BANK_OFFSET, 1);
   FX1_CHECK_EQ(r.rd(FX1_ISP_TUNE_WB_GB_OFFSET), FX1_ISP_TUNE_WB_GB_RESET);  // bank 1 untouched
   r.wr(FX1_ISP_TUNE_WB_GB_OFFSET, 0x77);
   r.wr(FX1_ISP_TUNE_BANK_OFFSET, 0);
   FX1_CHECK_EQ(r.rd(FX1_ISP_TUNE_WB_GB_OFFSET), 0x123u);
   FX1_CHECK_EQ(r.cu.profile_bank_value(1, FX1_ISP_TUNE_WB_GB_OFFSET), 0x77u);
   FX1_CHECK_EQ(r.cu.regs().peek(FX1_ISP_TUNE_WB_GB_OFFSET), FX1_ISP_TUNE_WB_GB_RESET);  // datapath: reset
   // Legacy algorithm scalars stay plain registers in legacy mode.
   r.wr(FX1_ISP_WB_GAIN_R_OFFSET, 0x1C0);
   FX1_CHECK_EQ(r.cu.regs().peek(FX1_ISP_WB_GAIN_R_OFFSET), 0x1C0u);
   FX1_CHECK(r.admit());  // legacy admission needs no profile
   FX1_CHECK_EQ(r.rd(FX1_ISP_TUNE_NEXT_FRAME_OFFSET), 2u);
   FX1_CHECK_EQ(r.err(), 0u);
}

// §9.7: ENABLE starts with no active profile and holds ingress; INIT copies
// the legacy scalars and the extension defaults; writes in profile mode go to
// the selected bank and leave the datapath alone until activation.
void test_init_edit_seal_apply() {
   rig r;
   r.wr(FX1_ISP_WB_GAIN_R_OFFSET, 0x1C0);
   r.wr(FX1_ISP_TUNE_WB_GB_OFFSET, 0x155);  // legacy storage in bank 0
   r.enter_profile_mode();
   FX1_CHECK_EQ(r.status() & FX1_ISP_TUNE_STATUS_ACTIVE_VALID_MASK, 0u);
   FX1_CHECK(r.cu.admission_state(r.t, 0) == control_unit::admission::wait_event);  // held: no profile
   r.cmd(c_init);  // bank 0
   FX1_CHECK_EQ(r.err(), 0u);
   FX1_CHECK_EQ(r.rd(FX1_ISP_WB_GAIN_R_OFFSET), 0x1C0u);                    // copied from legacy
   FX1_CHECK_EQ(r.rd(FX1_ISP_TUNE_WB_GB_OFFSET), FX1_ISP_TUNE_WB_GB_RESET);  // extension default
   r.wr(FX1_ISP_WB_GAIN_R_OFFSET, 0x200);
   FX1_CHECK_EQ(r.rd(FX1_ISP_WB_GAIN_R_OFFSET), 0x200u);
   FX1_CHECK_EQ(r.cu.regs().peek(FX1_ISP_WB_GAIN_R_OFFSET), 0x1C0u);  // datapath unchanged
   r.wr(FX1_ISP_TUNE_PROFILE_ID_OFFSET, 0xA5);
   r.cmd(c_apply);  // not sealed
   FX1_CHECK_EQ(r.err(), e_not_ready);
   r.cmd(c_seal);
   FX1_CHECK_EQ(r.err(), 0u);
   FX1_CHECK_EQ(r.status() & FX1_ISP_TUNE_STATUS_BANK0_VALID_MASK, FX1_ISP_TUNE_STATUS_BANK0_VALID_MASK);
   r.wr(FX1_ISP_WB_GAIN_R_OFFSET, 0x300);  // sealed: LOCKED, no effect
   FX1_CHECK_EQ(r.err(), e_locked);
   FX1_CHECK_EQ(r.rd(FX1_ISP_WB_GAIN_R_OFFSET), 0x200u);
   r.wr(FX1_ISP_WB_GAIN_R_OFFSET, 0x200);  // unchanged bits: no-op, no error
   FX1_CHECK_EQ(r.err(), 0u);
   r.wr(FX1_ISP_TUNE_BANK_OFFSET, 1);  // selecting another bank never changes contents
   r.wr(FX1_ISP_TUNE_BANK_OFFSET, 0);
   const std::uint64_t at = r.t;
   r.cmd(c_apply);
   FX1_CHECK_EQ(r.err(), 0u);
   FX1_CHECK_EQ(r.status() & FX1_ISP_TUNE_STATUS_BANK0_PENDING_MASK, FX1_ISP_TUNE_STATUS_BANK0_PENDING_MASK);
   // Eligible from the edge after acceptance; no admission on the APPLY edge
   // without an active profile.
   FX1_CHECK(r.cu.admission_state(at, 0) == control_unit::admission::wait_edge);
   FX1_CHECK(r.admit());
   FX1_CHECK_EQ(r.cu.regs().peek(FX1_ISP_WB_GAIN_R_OFFSET), 0x200u);  // activated at the SOF
   FX1_CHECK_EQ(r.status() & (FX1_ISP_TUNE_STATUS_ACTIVE_VALID_MASK | FX1_ISP_TUNE_STATUS_PENDING_MASK),
                FX1_ISP_TUNE_STATUS_ACTIVE_VALID_MASK);
   FX1_CHECK_EQ(r.rd(FX1_ISP_TUNE_ACTIVE_ID_OFFSET), 0xA5u);
   FX1_CHECK_EQ(r.rd(FX1_ISP_TUNE_ACTIVE_FRAME_OFFSET), 1u);
   FX1_CHECK_EQ(r.status() & FX1_ISP_TUNE_STATUS_BANK0_REFERENCED_MASK, FX1_ISP_TUNE_STATUS_BANK0_REFERENCED_MASK);
   r.cmd(c_unseal);  // active: even with zero references
   FX1_CHECK_EQ(r.err(), e_locked);
   r.retire();
   FX1_CHECK_EQ(r.cu.profile_bank_refs(0), 0u);
   r.cmd(c_unseal);
   FX1_CHECK_EQ(r.err(), e_locked);
   r.cmd(c_apply);  // the already active bank
   FX1_CHECK_EQ(r.err(), e_invalid);
   FX1_CHECK(r.admit());  // the active profile keeps admitting
   FX1_CHECK_EQ(r.rd(FX1_ISP_TUNE_ACTIVE_FRAME_OFFSET), 1u);
}

// §9.9.7: one command per write; EXACT only with APPLY; APPLY error
// priority MODE, PENDING, NOT_READY, INVALID, MISSED; EXACT arithmetic.
void test_command_rules() {
   rig r;
   r.enter_profile_mode();
   r.cmd(c_init | c_seal);
   FX1_CHECK_EQ(r.err(), e_invalid);
   r.cmd(c_exact);
   FX1_CHECK_EQ(r.err(), e_invalid);
   r.cmd(c_seal | c_exact);
   FX1_CHECK_EQ(r.err(), e_invalid);
   r.cmd(c_cancel);  // nothing pending: no effect, no error
   FX1_CHECK_EQ(r.err(), 0u);
   r.prepare(0, 1);
   FX1_CHECK_EQ(r.err(), 0u);
   // No active profile: EXACT must equal NEXT_FRAME.
   r.wr(FX1_ISP_TUNE_APPLY_FRAME_OFFSET, r.cu.next_frame() + 1u);
   r.cmd(c_apply | c_exact);
   FX1_CHECK_EQ(r.err(), e_invalid);
   r.wr(FX1_ISP_TUNE_APPLY_FRAME_OFFSET, r.cu.next_frame());
   r.cmd(c_apply | c_exact);
   FX1_CHECK_EQ(r.err(), 0u);
   r.cmd(c_apply | c_exact);  // second APPLY: PENDING, the first is intact
   FX1_CHECK_EQ(r.err(), e_pending);
   r.t++;
   FX1_CHECK(r.admit());
   r.retire();
   // Now an active profile: bank 1 with exact targets.
   r.prepare(1, 2);
   FX1_CHECK_EQ(r.err(), 0u);
   const std::uint32_t next = r.cu.next_frame();
   r.wr(FX1_ISP_TUNE_APPLY_FRAME_OFFSET, next + 0x80000000u);  // d = 2^31: ambiguous
   r.cmd(c_apply | c_exact);
   FX1_CHECK_EQ(r.err(), e_invalid);
   r.wr(FX1_ISP_TUNE_APPLY_FRAME_OFFSET, next - 1u);  // d > 2^31: past
   r.cmd(c_apply | c_exact);
   FX1_CHECK_EQ(r.err(), e_missed);
   r.wr(FX1_ISP_TUNE_BANK_OFFSET, 0);
   r.cmd(c_apply);  // bank 0 is active: INVALID
   FX1_CHECK_EQ(r.err(), e_invalid);
   r.wr(FX1_ISP_TUNE_BANK_OFFSET, 1);
   r.wr(FX1_ISP_TUNE_MODE_OFFSET, FX1_ISP_TUNE_MODE_ENABLE_MASK);  // same value: no-op
   FX1_CHECK_EQ(r.err(), 0u);
   // Error priority: a pending request reports PENDING before NOT_READY.
   r.wr(FX1_ISP_TUNE_APPLY_FRAME_OFFSET, next + 2u);
   r.cmd(c_apply | c_exact);
   FX1_CHECK_EQ(r.err(), 0u);
   r.cmd(c_unseal);  // pending target: LOCKED
   FX1_CHECK_EQ(r.err(), e_locked);
   r.wr(FX1_ISP_TUNE_BANK_OFFSET, 0);
   r.cmd(c_unseal);  // bank 0 active
   FX1_CHECK_EQ(r.err(), e_locked);
   r.cmd(c_apply);
   FX1_CHECK_EQ(r.err(), e_pending);
   // Exact future target: older sequences keep the old profile, then the
   // target frame activates bank 1 after the drain.
   r.t++;
   FX1_CHECK(r.admit());  // next
   FX1_CHECK_EQ(r.rd(FX1_ISP_TUNE_ACTIVE_ID_OFFSET), 1u);
   FX1_CHECK(r.admit());  // next + 1
   FX1_CHECK_EQ(r.cu.next_frame(), next + 2u);
   FX1_CHECK(r.cu.admission_state(r.t, r.in_flight) == control_unit::admission::wait_event);  // drain
   r.retire();
   FX1_CHECK(!r.admit());
   r.retire();
   FX1_CHECK(r.admit());  // next + 2: bank 1
   FX1_CHECK_EQ(r.rd(FX1_ISP_TUNE_ACTIVE_ID_OFFSET), 2u);
   FX1_CHECK_EQ(r.rd(FX1_ISP_TUNE_ACTIVE_FRAME_OFFSET), next + 2u);
   FX1_CHECK_EQ(r.status() & FX1_ISP_TUNE_STATUS_ACTIVE_BANK_MASK, 1u);
   r.retire();
   // Bank 0 is no longer active or referenced: UNSEAL permits reuse.
   r.cmd(c_unseal);
   FX1_CHECK_EQ(r.err(), 0u);
}

// §9.7, §9.9.7: CANCEL before admission removes the request (no MISSED);
// CANCEL on the admission edge loses; the frame counter wraps modulo 2^32.
void test_cancel_and_wrap() {
   rig r;
   r.enter_profile_mode();
   r.prepare(0, 10);
   r.cmd(c_apply);
   r.t++;
   FX1_CHECK(r.admit());
   r.retire();
   r.prepare(1, 11);
   r.cmd(c_apply);
   r.cmd(c_cancel);
   FX1_CHECK_EQ(r.err(), 0u);
   FX1_CHECK_EQ(r.status() & FX1_ISP_TUNE_STATUS_PENDING_MASK, 0u);
   FX1_CHECK(r.admit());
   FX1_CHECK_EQ(r.rd(FX1_ISP_TUNE_ACTIVE_ID_OFFSET), 10u);
   r.retire();
   // Coincident CANCEL and admission: admission wins.
   r.cmd(c_apply);
   r.t++;
   const std::uint64_t edge = r.t;
   r.cu.csr_write(FX1_ISP_TUNE_CMD_OFFSET, c_cancel, all, edge);
   FX1_CHECK(r.cu.admission_state(edge, 0) == control_unit::admission::go);
   r.cu.accepted_sof(edge);
   r.t = edge + 1;
   FX1_CHECK_EQ(r.rd(FX1_ISP_TUNE_ACTIVE_ID_OFFSET), 11u);
   r.cu.frame_retired();
   // Wrap: an exact target across 2^32.
   r.cu.debug_set_frame_counter(0xFFFFFFFEu);  // NEXT_FRAME = 0xFFFFFFFF
   r.wr(FX1_ISP_TUNE_BANK_OFFSET, 0);
   r.cmd(c_unseal);
   r.cmd(c_seal);
   r.wr(FX1_ISP_TUNE_APPLY_FRAME_OFFSET, 1u);  // two frames ahead across the wrap
   r.cmd(c_apply | c_exact);
   FX1_CHECK_EQ(r.err(), 0u);
   r.t++;
   FX1_CHECK(r.admit());
   FX1_CHECK_EQ(r.cu.next_frame(), 0u);
   r.retire();
   FX1_CHECK(r.admit());
   r.retire();
   FX1_CHECK(r.admit());
   FX1_CHECK_EQ(r.rd(FX1_ISP_TUNE_ACTIVE_ID_OFFSET), 10u);
   FX1_CHECK_EQ(r.rd(FX1_ISP_TUNE_ACTIVE_FRAME_OFFSET), 1u);
}

// §9.7: no output enabled holds admission without an error; four context
// credits bound the admitted, unretired frames in both modes.
void test_admission_holds() {
   rig r;
   for (unsigned k = 0; k < control_unit::context_credits; ++k) {
      FX1_CHECK(r.admit());
   }
   FX1_CHECK(!r.admit());  // fifth: held
   r.retire();
   FX1_CHECK(r.admit());
   while (r.in_flight) {
      r.retire();
   }
   r.enter_profile_mode();
   r.prepare(0, 3);
   r.cmd(c_apply);
   r.t++;
   r.wr(FX1_ISP_OUTPUT_ENABLE_OFFSET, 0);
   r.t++;
   FX1_CHECK(r.cu.admission_state(r.t, 0) == control_unit::admission::wait_event);
   FX1_CHECK_EQ(r.err(), 0u);
   r.wr(FX1_ISP_OUTPUT_ENABLE_OFFSET, 1);
   FX1_CHECK(r.admit());
}

// §9.9.3, §9.9.6: IDLE-class words are rejected with MODE unless the pipeline
// is idle; the input domain cannot change in profile mode; TUNE_MODE changes
// need idle and no leased result bank; unchanged writes are never rejected.
void test_idle_class() {
   rig r;
   r.wr(FX1_ISP_COMMON_FRAME_WIDTH_OFFSET, 640);  // idle: accepted
   FX1_CHECK_EQ(r.rd(FX1_ISP_COMMON_FRAME_WIDTH_OFFSET), 640u);
   r.wr(FX1_ISP_COMMON_CTRL_OFFSET, FX1_ISP_COMMON_CTRL_ISP_EN_MASK);
   r.wr(FX1_ISP_DMA_CTRL_OFFSET, FX1_ISP_DMA_CTRL_IDMA_EN_MASK);  // IDMA_EN and ISP_EN: not idle
   r.wr(FX1_ISP_COMMON_FRAME_WIDTH_OFFSET, 320);
   FX1_CHECK_EQ(r.err(), e_mode);
   FX1_CHECK_EQ(r.rd(FX1_ISP_COMMON_FRAME_WIDTH_OFFSET), 640u);
   r.wr(FX1_ISP_COMMON_FRAME_WIDTH_OFFSET, 640);  // unchanged: no-op, no error
   FX1_CHECK_EQ(r.err(), 0u);
   r.wr(FX1_ISP_COMMON_INPUT_FORMAT_OFFSET, 1);
   FX1_CHECK_EQ(r.err(), e_mode);
   r.wr(FX1_ISP_TUNE_MODE_OFFSET, 1);
   FX1_CHECK_EQ(r.err(), e_mode);
   FX1_CHECK(!r.cu.profile_mode());
   r.wr(FX1_ISP_DMA_CTRL_OFFSET, 0);
   FX1_CHECK(r.admit());  // an admitted frame keeps the pipeline busy
   r.wr(FX1_ISP_COMMON_BAYER_OFFSET, 3);
   FX1_CHECK_EQ(r.err(), e_mode);
   r.retire();
   r.wr(FX1_ISP_COMMON_BAYER_OFFSET, 3);
   FX1_CHECK_EQ(r.err(), 0u);
   r.enter_profile_mode();
   FX1_CHECK(r.cu.profile_mode());
   r.wr(FX1_ISP_COMMON_FRAME_HEIGHT_OFFSET, 480);  // input domain in profile mode, even idle
   FX1_CHECK_EQ(r.err(), e_mode);
   r.wr(FX1_ISP_TUNE_MODE_OFFSET, 0);  // leave: idle, no lease
   FX1_CHECK(!r.cu.profile_mode());
   r.wr(FX1_ISP_COMMON_FRAME_HEIGHT_OFFSET, 480);
   FX1_CHECK_EQ(r.err(), 0u);
}

// §9.7: AEC COMMIT is rejected with MODE in profile mode.
void test_legacy_commands_in_profile_mode() {
   rig r;
   r.enter_profile_mode();
   // LSC_PROFILE_SEL is a bank scalar in profile mode: the legacy select
   // check (and its LSC_ERROR) does not run on the write.
   r.cmd(c_init);
   r.wr(FX1_ISP_LSC_PROFILE_SEL_OFFSET, 2);
   FX1_CHECK_EQ(r.rd(FX1_ISP_LSC_ERROR_OFFSET), 0u);
   FX1_CHECK_EQ(r.rd(FX1_ISP_LSC_PROFILE_SEL_OFFSET), 2u);
   r.wr(FX1_ISP_AEC_CTRL_OFFSET, FX1_ISP_AEC_CTRL_COMMIT_MASK);
   FX1_CHECK_EQ(r.err(), e_mode);
   FX1_CHECK_EQ(r.cu.regs().peek(FX1_ISP_AEC_CTRL_OFFSET) & FX1_ISP_AEC_CTRL_COMMIT_MASK, 0u);
}

// Table 9-196 rules available in V107-3: every failing check sets its bit and
// leaves the bank unsealed.
void test_seal_validation() {
   struct bad {
      const char *name;
      std::uint32_t off, value;
      std::uint32_t err;
   };
   const bad cases[] = {
      {"NR mode 3", FX1_ISP_TUNE_NR_CTRL_OFFSET, 3, e_invalid},
      {"NR blend 257", FX1_ISP_TUNE_NR_BLEND_OFFSET, 257, e_invalid},
      {"CNF blend 257", FX1_ISP_TUNE_CNF_BLEND_OFFSET, 257, e_invalid},
      {"NR min > max", FX1_ISP_TUNE_NR_LIMITS_OFFSET, 0x00010002u, e_invalid},
      {"EE profile 3", FX1_ISP_EE_CTRL_OFFSET, 3u << 2, e_invalid},
      {"LSC profile 3", FX1_ISP_LSC_PROFILE_SEL_OFFSET, 3, e_invalid},
      {"AWB MIN_G 0 with colour", FX1_ISP_TUNE_AWB_CTRL_OFFSET, FX1_ISP_TUNE_AWB_CTRL_COLOR_EN_MASK, e_invalid},
      {"AWB ellipse A 0", FX1_ISP_TUNE_AWB_E0_A_OFFSET, 0, e_invalid},
      {"AWB ellipse C 0", FX1_ISP_TUNE_AWB_E0_C_OFFSET, 0, e_invalid},
      {"AWB ellipse A*C = B^2", FX1_ISP_TUNE_AWB_E0_B_OFFSET, 4, e_invalid},
      {"AWB ROI empty", FX1_ISP_TUNE_AWB_CTRL_OFFSET, FX1_ISP_TUNE_AWB_CTRL_ROI_EN_MASK, e_invalid},
      {"AF window empty", FX1_ISP_TUNE_AF_CTRL_OFFSET, FX1_ISP_TUNE_AF_CTRL_WINDOW_MODE_MASK | (1u << 16), e_invalid},
      {"HIST step 0", FX1_ISP_TUNE_HIST_STEP_OFFSET, 0, e_invalid},
      {"LSC enabled, profile invalid", FX1_ISP_LSC_CTRL_OFFSET, FX1_ISP_LSC_CTRL_EN_MASK, e_not_ready},
   };
   for (const bad &c : cases) {
      rig r;
      r.enter_profile_mode();
      r.cmd(c_init);
      // A context in which only the case's field fails: ellipse 0 legal and
      // enabled, the weighted histogram enabled with legal steps.
      r.wr(FX1_ISP_TUNE_AWB_E0_A_OFFSET, 4);
      r.wr(FX1_ISP_TUNE_AWB_E0_B_OFFSET, 0);
      r.wr(FX1_ISP_TUNE_AWB_E0_C_OFFSET, 4);
      r.wr(FX1_ISP_TUNE_AWB_CTRL_OFFSET, 1u << 8);
      r.wr(FX1_ISP_TUNE_HIST_STEP_OFFSET, 0x0101);
      r.wr(FX1_ISP_TUNE_HIST_CTRL_OFFSET, FX1_ISP_TUNE_HIST_CTRL_ENABLE_MASK);
      r.wr(FX1_ISP_TUNE_AWB_MIN_G_OFFSET, 0);
      r.cmd(c_seal);
      FX1_CHECK_EQ_CTX(r.err(), 0u, std::string(c.name) + ": legal context");
      r.cmd(c_unseal);
      if (c.off == FX1_ISP_TUNE_AWB_CTRL_OFFSET) {
         r.wr(c.off, c.value | (1u << 8));
      } else {
         r.wr(c.off, c.value);
      }
      r.cmd(c_seal);
      FX1_CHECK_EQ_CTX(r.err(), c.err, c.name);
      FX1_CHECK_EQ_CTX(r.cu.profile_bank_sealed(0), false, c.name);
   }
   // A legal ellipse and ROI seal.
   rig r;
   r.enter_profile_mode();
   r.cmd(c_init);
   r.wr(FX1_ISP_TUNE_AWB_E0_A_OFFSET, 4);
   r.wr(FX1_ISP_TUNE_AWB_E0_B_OFFSET, 1);
   r.wr(FX1_ISP_TUNE_AWB_E0_C_OFFSET, 4);
   r.wr(FX1_ISP_TUNE_AWB_ROI_START_OFFSET, 0);
   r.wr(FX1_ISP_TUNE_AWB_ROI_END_OFFSET, (32u << 16) | 32u);
   r.wr(FX1_ISP_TUNE_AWB_CTRL_OFFSET, FX1_ISP_TUNE_AWB_CTRL_ROI_EN_MASK | (1u << 8));
   r.cmd(c_seal);
   FX1_CHECK_EQ(r.err(), 0u);
   FX1_CHECK(r.cu.profile_bank_sealed(0));
   // B^2 >= A*C is rejected (signed 16-bit B).
   r.cmd(c_unseal);
   r.wr(FX1_ISP_TUNE_AWB_E0_B_OFFSET, 0xFFFCu);  // -4: 16 >= 16
   r.cmd(c_seal);
   FX1_CHECK_EQ(r.err(), e_invalid);
   // A and C are unsigned Q2.14: 2.0 (0x8000) and 3.99 (0xFFFF) are legal
   // (err() above cleared TUNE_ERROR; the failed SEAL left the bank open).
   r.wr(FX1_ISP_TUNE_AWB_E0_A_OFFSET, 0x8000u);
   r.wr(FX1_ISP_TUNE_AWB_E0_B_OFFSET, 0);
   r.wr(FX1_ISP_TUNE_AWB_E0_C_OFFSET, 0xFFFFu);
   r.cmd(c_seal);
   FX1_CHECK_EQ(r.err(), 0u);
   FX1_CHECK(r.cu.profile_bank_sealed(0));
}

// §9.7: DMA recovery cancels a pending APPLY without MISSED, keeps the
// active profile and retires references once; common reset clears active
// state and seals; hard reset also the bank scalars. TUNE_ERROR survives DMA
// recovery and is cleared by the common reset.
void test_resets() {
   rig r;
   r.enter_profile_mode();
   r.prepare(0, 7);
   r.cmd(c_apply);
   r.t++;
   FX1_CHECK(r.admit());
   r.prepare(1, 8);
   r.cmd(c_apply);
   r.cmd(c_apply);  // PENDING error, to see it survive the DMA scope
   r.wr(FX1_ISP_DMA_CTRL_OFFSET, FX1_ISP_DMA_CTRL_SOFT_RESET_MASK);
   r.t += 20;
   r.in_flight = 0;
   FX1_CHECK_EQ(r.status() & FX1_ISP_TUNE_STATUS_PENDING_MASK, 0u);
   FX1_CHECK_EQ(r.status() & FX1_ISP_TUNE_STATUS_ACTIVE_VALID_MASK, FX1_ISP_TUNE_STATUS_ACTIVE_VALID_MASK);
   FX1_CHECK_EQ(r.cu.profile_bank_refs(0), 0u);
   FX1_CHECK_EQ(r.err(), e_pending);  // kept by the DMA scope, no MISSED
   FX1_CHECK(r.cu.profile_bank_sealed(1));
   r.wr(FX1_ISP_TUNE_ERROR_OFFSET, e_pending);
   r.cmd(c_apply | c_exact);  // set an error, then common reset
   r.wr(FX1_ISP_COMMON_CTRL_OFFSET, FX1_ISP_COMMON_CTRL_SOFT_RST_MASK);
   r.t += 20;
   FX1_CHECK_EQ(r.rd(FX1_ISP_TUNE_ERROR_OFFSET), 0u);
   FX1_CHECK_EQ(r.status() & (FX1_ISP_TUNE_STATUS_ACTIVE_VALID_MASK | FX1_ISP_TUNE_STATUS_BANK1_VALID_MASK), 0u);
   FX1_CHECK(r.cu.profile_mode());  // configuration survives the soft reset
   FX1_CHECK_EQ(r.cu.profile_bank_value(1, FX1_ISP_TUNE_PROFILE_ID_OFFSET), 8u);
   r.cu.external_reset();
   FX1_CHECK(!r.cu.profile_mode());
   FX1_CHECK_EQ(r.cu.profile_bank_value(1, FX1_ISP_TUNE_PROFILE_ID_OFFSET), 0u);
}

// MAS v1.07 §9.9.5, Table 9-197 and §9.9.1 in profile mode (V107-4): bank
// payloads, TUNE_TABLE_CMD, SEAL payload readiness, LOCKED and resident pins.
void test_profile_tables() {
   rig r;
   // Legacy set-up of the input domain and the shared mesh (fixed in profile mode).
   r.wr(FX1_ISP_COMMON_FRAME_WIDTH_OFFSET, 64);
   r.wr(FX1_ISP_COMMON_FRAME_HEIGHT_OFFSET, 48);
   r.wr(FX1_ISP_LSC_MESH_NODES_OFFSET, 3u | (3u << 8));
   r.enter_profile_mode();
   r.cmd(c_init);  // bank 0
   const std::uint32_t tc = FX1_ISP_TUNE_TABLE_CMD_OFFSET, ts = FX1_ISP_TUNE_TABLE_STATUS_OFFSET;
   // Gamma bank 0 through the banked port; GAMMA_STATUS.LUT_LOADED follows bank 0.
   for (unsigned i = 0; i < 256; ++i) {
      r.wr(FX1_ISP_TUNE_GAMMA_PORT_OFFSET, i);
      r.wr(FX1_ISP_TUNE_GAMMA_DATA_OFFSET, 16u * i);
   }
   r.wr(FX1_ISP_TUNE_GAMMA_PORT_OFFSET, 9);
   FX1_CHECK_EQ(r.rd(FX1_ISP_TUNE_GAMMA_DATA_OFFSET), 144u);
   // NR: 16 knots are not enough; the 17th completes it. Bits combine.
   for (unsigned i = 0; i < 16; ++i) {
      r.wr(FX1_ISP_TUNE_NR_LUT_ADDR_OFFSET, i);
      r.wr(FX1_ISP_TUNE_NR_LUT_DATA_OFFSET, 100 + i);
   }
   r.wr(tc, FX1_ISP_TUNE_TABLE_CMD_GAMMA_CONFIRM_MASK | FX1_ISP_TUNE_TABLE_CMD_NR_CONFIRM_MASK);
   FX1_CHECK_EQ(r.err(), e_not_ready);
   FX1_CHECK_EQ(r.rd(ts), FX1_ISP_TUNE_TABLE_STATUS_GAMMA_MASK);
   FX1_CHECK_EQ(r.rd(FX1_ISP_GAMMA_STATUS_OFFSET) & FX1_ISP_GAMMA_STATUS_LUT_LOADED_MASK,
                FX1_ISP_GAMMA_STATUS_LUT_LOADED_MASK);
   r.wr(FX1_ISP_TUNE_NR_LUT_ADDR_OFFSET, 16);
   r.wr(FX1_ISP_TUNE_NR_LUT_DATA_OFFSET, 200);
   r.wr(tc, FX1_ISP_TUNE_TABLE_CMD_NR_CONFIRM_MASK);
   FX1_CHECK_EQ(r.err(), 0u);
   FX1_CHECK_EQ(r.rd(ts), FX1_ISP_TUNE_TABLE_STATUS_GAMMA_MASK | FX1_ISP_TUNE_TABLE_STATUS_NR_MASK);
   r.wr(FX1_ISP_TUNE_NR_LUT_DATA_OFFSET, 201);  // a payload write clears its confirmation
   FX1_CHECK_EQ(r.rd(ts) & FX1_ISP_TUNE_TABLE_STATUS_NR_MASK, 0u);
   r.wr(tc, FX1_ISP_TUNE_TABLE_CMD_NR_CONFIRM_MASK);
   // BPC factory list: count, raster order, bounds; a COUNT write clears it.
   auto bpc = [&](unsigned i, std::uint32_t x, std::uint32_t y) {
      r.wr(FX1_ISP_TUNE_BPC_ADDR_OFFSET, i);
      r.wr(FX1_ISP_TUNE_BPC_DATA_OFFSET, (y << 16) | x);
   };
   r.wr(FX1_ISP_TUNE_BPC_COUNT_OFFSET, 2);
   bpc(0, 1, 1);
   bpc(1, 0, 2);  // 2W > W + 1: increasing
   r.wr(tc, FX1_ISP_TUNE_TABLE_CMD_BPC_CONFIRM_MASK);
   FX1_CHECK_EQ(r.err(), 0u);
   FX1_CHECK_EQ(r.rd(ts) & FX1_ISP_TUNE_TABLE_STATUS_BPC_MASK, FX1_ISP_TUNE_TABLE_STATUS_BPC_MASK);
   bpc(1, 0, 1);  // W: before W + 1
   r.wr(tc, FX1_ISP_TUNE_TABLE_CMD_BPC_CONFIRM_MASK);
   FX1_CHECK_EQ(r.err(), e_not_ready);
   bpc(1, 1, 1);  // the same site twice: not strictly increasing
   r.wr(tc, FX1_ISP_TUNE_TABLE_CMD_BPC_CONFIRM_MASK);
   FX1_CHECK_EQ(r.err(), e_not_ready);
   bpc(1, 64, 2);  // X = W: out of bounds
   r.wr(tc, FX1_ISP_TUNE_TABLE_CMD_BPC_CONFIRM_MASK);
   FX1_CHECK_EQ(r.err(), e_not_ready);
   bpc(1, 0, 2);
   r.wr(FX1_ISP_TUNE_BPC_COUNT_OFFSET, 3);  // entry 2 never written
   r.wr(tc, FX1_ISP_TUNE_TABLE_CMD_BPC_CONFIRM_MASK);
   FX1_CHECK_EQ(r.err(), e_not_ready);
   r.wr(FX1_ISP_TUNE_BPC_COUNT_OFFSET, 2);
   r.wr(tc, FX1_ISP_TUNE_TABLE_CMD_BPC_CONFIRM_MASK);
   FX1_CHECK_EQ(r.err(), 0u);
   r.wr(FX1_ISP_TUNE_BPC_COUNT_OFFSET, 1);  // a COUNT write clears the confirmation
   FX1_CHECK_EQ(r.rd(ts) & FX1_ISP_TUNE_TABLE_STATUS_BPC_MASK, 0u);
   r.wr(FX1_ISP_TUNE_BPC_COUNT_OFFSET, 2);
   r.wr(tc, FX1_ISP_TUNE_TABLE_CMD_BPC_CONFIRM_MASK);
   FX1_CHECK_EQ(r.rd(ts) & FX1_ISP_TUNE_TABLE_STATUS_BPC_MASK, FX1_ISP_TUNE_TABLE_STATUS_BPC_MASK);
   r.wr(FX1_ISP_TUNE_BPC_COUNT_OFFSET, 2);  // the stored value again: still a payload write (§9.9.5)
   FX1_CHECK_EQ(r.rd(ts) & FX1_ISP_TUNE_TABLE_STATUS_BPC_MASK, 0u);
   r.wr(tc, FX1_ISP_TUNE_TABLE_CMD_BPC_CONFIRM_MASK);
   r.cu.csr_write(FX1_ISP_TUNE_BPC_COUNT_OFFSET, 1, 0u, r.t++);  // no byte lane: no write
   FX1_CHECK_EQ(r.rd(ts) & FX1_ISP_TUNE_TABLE_STATUS_BPC_MASK, FX1_ISP_TUNE_TABLE_STATUS_BPC_MASK);
   FX1_CHECK_EQ(r.rd(FX1_ISP_TUNE_BPC_COUNT_OFFSET), 2u);
   // LSC sectors: nodes - 1 nonzero intervals per axis summing to W - 1 / H - 1.
   auto sector = [&](unsigned axis, unsigned i, std::uint32_t v) {
      r.wr(FX1_ISP_TUNE_LSC_SECTOR_ADDR_OFFSET, (axis << FX1_ISP_TUNE_LSC_SECTOR_ADDR_AXIS_SHIFT) | i);
      r.wr(FX1_ISP_TUNE_LSC_SECTOR_DATA_OFFSET, v);
   };
   sector(0, 0, 31);
   sector(0, 1, 32);
   sector(1, 0, 23);
   sector(1, 1, 23);  // 46 != 47
   r.wr(tc, FX1_ISP_TUNE_TABLE_CMD_LSC_CONFIRM_MASK);
   FX1_CHECK_EQ(r.err(), e_not_ready);
   sector(1, 1, 0);
   sector(1, 0, 47);  // a zero interval
   r.wr(tc, FX1_ISP_TUNE_TABLE_CMD_LSC_CONFIRM_MASK);
   FX1_CHECK_EQ(r.err(), e_not_ready);
   sector(1, 0, 23);
   sector(1, 1, 24);
   r.wr(FX1_ISP_TUNE_LSC_SECTOR_ADDR_OFFSET, (1u << FX1_ISP_TUNE_LSC_SECTOR_ADDR_AXIS_SHIFT) | 1u);
   FX1_CHECK_EQ(r.rd(FX1_ISP_TUNE_LSC_SECTOR_DATA_OFFSET), 24u);
   r.wr(tc, FX1_ISP_TUNE_TABLE_CMD_LSC_CONFIRM_MASK);
   FX1_CHECK_EQ(r.err(), 0u);
   FX1_CHECK_EQ(r.rd(ts), 0xFu);
   // SEAL requires the payloads its enables use (Table 9-196).
   r.wr(FX1_ISP_GAMMA_CTRL_OFFSET, FX1_ISP_GAMMA_CTRL_EN_MASK);
   r.wr(FX1_ISP_TUNE_NR_CTRL_OFFSET, 2);
   r.wr(FX1_ISP_TUNE_BPC_CTRL_OFFSET, FX1_ISP_TUNE_BPC_CTRL_FACTORY_EN_MASK);
   r.wr(FX1_ISP_TUNE_LSC_CTRL_OFFSET, FX1_ISP_TUNE_LSC_CTRL_SECTOR_EN_MASK);
   r.wr(FX1_ISP_GTM_CTRL_OFFSET, FX1_ISP_GTM_CTRL_MANUAL_MASK);  // GTM disabled in the bank: loadable
   r.wr(FX1_ISP_GTM_LUT_ADDR_OFFSET, 0);
   for (unsigned i = 0; i < 64; ++i) {
      r.wr(FX1_ISP_GTM_LUT_DATA_OFFSET, 0x100);
   }
   r.wr(FX1_ISP_GTM_CTRL_OFFSET, FX1_ISP_GTM_CTRL_MANUAL_MASK | FX1_ISP_GTM_CTRL_EN_MASK);
   r.cmd(c_seal);  // 64 of 65 GTM points
   FX1_CHECK_EQ(r.err(), e_not_ready);
   r.wr(FX1_ISP_GTM_CTRL_OFFSET, FX1_ISP_GTM_CTRL_MANUAL_MASK);
   r.wr(FX1_ISP_GTM_LUT_ADDR_OFFSET, 64);
   r.wr(FX1_ISP_GTM_LUT_DATA_OFFSET, 0x100);
   r.wr(FX1_ISP_GTM_CTRL_OFFSET, FX1_ISP_GTM_CTRL_MANUAL_MASK | FX1_ISP_GTM_CTRL_EN_MASK);
   r.cmd(c_seal);
   FX1_CHECK_EQ(r.err(), 0u);
   // Sealed: payload writes and confirmations are LOCKED and have no effect.
   r.wr(FX1_ISP_TUNE_GAMMA_PORT_OFFSET, 9);
   r.wr(FX1_ISP_TUNE_GAMMA_DATA_OFFSET, 1);
   FX1_CHECK_EQ(r.err(), e_locked);
   FX1_CHECK_EQ(r.rd(FX1_ISP_TUNE_GAMMA_DATA_OFFSET), 144u);
   r.wr(tc, FX1_ISP_TUNE_TABLE_CMD_NR_CONFIRM_MASK);
   FX1_CHECK_EQ(r.err(), e_locked);
   r.wr(FX1_ISP_GTM_LUT_ADDR_OFFSET, 3);
   r.wr(FX1_ISP_GTM_LUT_DATA_OFFSET, 0x222);
   FX1_CHECK_EQ(r.err(), 0u);  // GTM is enabled in the bank: the write is discarded (no LOCKED)
   // Bank 1 is independent; legacy confirmation is a legacy command (MODE).
   r.wr(FX1_ISP_TUNE_BANK_OFFSET, 1);
   FX1_CHECK_EQ(r.rd(ts), 0u);
   r.wr(FX1_ISP_GAMMA_CTRL_OFFSET, FX1_ISP_GAMMA_CTRL_LUT_LOAD_CONFIRM_MASK);
   FX1_CHECK_EQ(r.err(), e_mode);
   // Leaving profile mode invalidates the bank payloads and LUT_LOADED.
   r.wr(FX1_ISP_TUNE_MODE_OFFSET, 0);
   FX1_CHECK_EQ(r.rd(FX1_ISP_GAMMA_STATUS_OFFSET) & FX1_ISP_GAMMA_STATUS_LUT_LOADED_MASK, 0u);
   r.wr(FX1_ISP_TUNE_BANK_OFFSET, 0);
   FX1_CHECK_EQ(r.rd(ts), 0u);
}

// §9.9.1: SEAL pins the selected EE resident and LSC resident until UNSEAL;
// a pinned resident's scalars, tables, confirmation and LSC load are LOCKED.
void test_resident_pins() {
   rig r;
   r.wr(FX1_ISP_COMMON_FRAME_WIDTH_OFFSET, 64);
   r.wr(FX1_ISP_COMMON_FRAME_HEIGHT_OFFSET, 48);
   r.wr(FX1_ISP_LSC_MESH_NODES_OFFSET, 2u | (2u << 8));
   // LSC resident 1 loaded in legacy mode.
   r.wr(FX1_ISP_LSC_LOAD_CTRL_OFFSET, FX1_ISP_LSC_LOAD_CTRL_LOAD_BEGIN_MASK | 1u);
   for (unsigned i = 0; i < 16; ++i) {
      r.wr(FX1_ISP_LSC_COEF_DATA_OFFSET, 0x40000);
   }
   r.wr(FX1_ISP_LSC_LOAD_CTRL_OFFSET, FX1_ISP_LSC_LOAD_CTRL_LOAD_VALIDATE_MASK | 1u);
   r.t += 64;
   FX1_CHECK(r.cu.lsc_profile_valid(1));
   r.enter_profile_mode();
   // EE resident 2: loaded and confirmed in profile mode (unowned).
   r.wr(FX1_ISP_EE_CTRL_OFFSET, 2u << FX1_ISP_EE_CTRL_PROFILE_WR_SEL_SHIFT);
   for (unsigned b = 0; b < 4; ++b) {
      r.wr(FX1_ISP_EE_LUT_CTRL_OFFSET, b << FX1_ISP_EE_LUT_CTRL_LUT_SEL_SHIFT);
      r.wr(FX1_ISP_EE_LUT_ADDR_OFFSET, 0);
      for (unsigned i = 0; i < control_unit::ee_bank_entries(b); ++i) {
         r.wr(FX1_ISP_EE_LUT_WDATA_OFFSET, 0x8000);
      }
   }
   r.wr(FX1_ISP_EE_LUT_CTRL_OFFSET, FX1_ISP_EE_LUT_CTRL_PROFILE_LOAD_CONFIRM_MASK);
   FX1_CHECK_EQ(r.err(), 0u);
   FX1_CHECK(r.cu.ee_resident_valid(2));
   r.cmd(c_init);
   r.wr(FX1_ISP_EE_CTRL_OFFSET, FX1_ISP_EE_CTRL_EN_MASK | (2u << FX1_ISP_EE_CTRL_PROFILE_SEL_SHIFT) |
                                    (2u << FX1_ISP_EE_CTRL_PROFILE_WR_SEL_SHIFT));
   r.wr(FX1_ISP_LSC_PROFILE_SEL_OFFSET, 1);
   r.wr(FX1_ISP_LSC_CTRL_OFFSET, FX1_ISP_LSC_CTRL_EN_MASK);
   r.cmd(c_seal);
   FX1_CHECK_EQ(r.err(), 0u);
   r.wr(FX1_ISP_EE_ALPHA_OFFSET, 0x1234);  // pinned resident 2 (PROFILE_WR_SEL is live)
   FX1_CHECK_EQ(r.err(), e_locked);
   FX1_CHECK_EQ(r.cu.ee_profile_scalar(2, ee_alpha), 0x4000u);
   FX1_CHECK(r.cu.ee_resident_valid(2));
   r.wr(FX1_ISP_EE_LUT_CTRL_OFFSET, FX1_ISP_EE_LUT_CTRL_PROFILE_LOAD_CONFIRM_MASK);
   FX1_CHECK_EQ(r.err(), e_locked);
   // PROFILE_WR_SEL is live; EN and PROFILE_SEL belong to the sealed bank, so
   // software keeps them (read-modify-write) to avoid LOCKED.
   r.wr(FX1_ISP_EE_CTRL_OFFSET, FX1_ISP_EE_CTRL_EN_MASK | (2u << FX1_ISP_EE_CTRL_PROFILE_SEL_SHIFT) |
                                    (1u << FX1_ISP_EE_CTRL_PROFILE_WR_SEL_SHIFT));  // resident 1: free
   r.wr(FX1_ISP_EE_ALPHA_OFFSET, 0x1234);
   FX1_CHECK_EQ(r.err(), 0u);
   FX1_CHECK_EQ(r.cu.ee_profile_scalar(1, ee_alpha), 0x1234u);
   r.wr(FX1_ISP_LSC_LOAD_CTRL_OFFSET, FX1_ISP_LSC_LOAD_CTRL_LOAD_BEGIN_MASK | 1u);  // pinned LSC resident
   FX1_CHECK_EQ(r.err(), e_locked);
   FX1_CHECK(r.cu.lsc_profile_valid(1));
   r.cmd(c_unseal);
   r.wr(FX1_ISP_EE_CTRL_OFFSET, 2u << FX1_ISP_EE_CTRL_PROFILE_WR_SEL_SHIFT);
   r.wr(FX1_ISP_EE_ALPHA_OFFSET, 0x1234);
   FX1_CHECK_EQ(r.err(), 0u);
   FX1_CHECK(!r.cu.ee_resident_valid(2));  // a scalar write clears the confirmation
}

// Load and confirm EE resident p in legacy mode (EE disabled, idle) with the
// given radial scalars; LUTs at unity.
void load_ee(rig &r, unsigned p, std::uint32_t feature, std::uint32_t center, std::uint32_t th0, std::uint32_t th1,
             std::uint32_t th2) {
   r.wr(FX1_ISP_EE_CTRL_OFFSET, p << FX1_ISP_EE_CTRL_PROFILE_WR_SEL_SHIFT);
   r.wr(FX1_ISP_EE_FEATURE_EN_OFFSET, feature);
   r.wr(FX1_ISP_EE_RADIAL_CENTER_OFFSET, center);
   r.wr(FX1_ISP_EE_RADIAL_R2_TH0_OFFSET, th0);
   r.wr(FX1_ISP_EE_RADIAL_R2_TH1_OFFSET, th1);
   r.wr(FX1_ISP_EE_RADIAL_R2_TH2_OFFSET, th2);
   for (unsigned b = 0; b < 4; ++b) {
      r.wr(FX1_ISP_EE_LUT_CTRL_OFFSET, b << FX1_ISP_EE_LUT_CTRL_LUT_SEL_SHIFT);
      r.wr(FX1_ISP_EE_LUT_ADDR_OFFSET, 0);
      for (unsigned i = 0; i < control_unit::ee_bank_entries(b); ++i) {
         r.wr(FX1_ISP_EE_LUT_WDATA_OFFSET, 0x8000);
      }
   }
   r.wr(FX1_ISP_EE_LUT_CTRL_OFFSET, FX1_ISP_EE_LUT_CTRL_PROFILE_LOAD_CONFIRM_MASK);
   FX1_CHECK(r.cu.ee_resident_valid(p));
}

void geometry_64x48(rig &r) {
   r.wr(FX1_ISP_COMMON_FRAME_WIDTH_OFFSET, 64);
   r.wr(FX1_ISP_COMMON_FRAME_HEIGHT_OFFSET, 48);
   r.wr(FX1_ISP_COMMON_BAYER_OFFSET, 0);
}

// Table 9-196, EE radial (RADIAL_EN = 1) in the resident the bank's
// PROFILE_SEL selects: CENTER_X2 <= 2(W - 1), CENTER_Y2 <= 2(H - 1),
// R2_TH0 <= R2_TH1 <= R2_TH2 (review V107-2..6 R1). 64 x 48: X2 <= 126, Y2 <= 94.
void test_seal_ee_radial() {
   constexpr std::uint32_t radial = FX1_ISP_EE_FEATURE_EN_RADIAL_EN_MASK;
   auto xy = [](std::uint32_t x2, std::uint32_t y2) { return x2 | (y2 << 16); };
   const std::uint32_t bad_center = xy(0x1FFF, 0x1FFF);
   struct radial_case {
      const char *name;
      unsigned sel;
      std::uint32_t feature, center, th0, th1, th2;
      bool en;
      std::uint32_t err;
   };
   const radial_case cases[] = {
      {"limits exactly", 0, radial, xy(126, 94), 1, 2, 3, true, 0},
      {"equal thresholds", 0, radial, xy(0, 0), 5, 5, 5, true, 0},
      {"X2 above 2(W-1)", 0, radial, xy(127, 94), 1, 2, 3, true, e_invalid},
      {"Y2 above 2(H-1)", 0, radial, xy(126, 95), 1, 2, 3, true, e_invalid},
      {"TH0 > TH1", 0, radial, xy(10, 10), 2, 1, 3, true, e_invalid},
      {"TH1 > TH2", 0, radial, xy(10, 10), 1, 3, 2, true, e_invalid},
      {"RADIAL_EN = 0 ignores the radial scalars", 0, 0, bad_center, 3, 2, 1, true, 0},
      {"resident 2 selected, legal", 2, radial, xy(126, 94), 1, 2, 3, true, 0},
      {"resident 2 selected, illegal", 2, radial, xy(127, 94), 1, 2, 3, true, e_invalid},
      {"EE disabled: still checked", 0, radial, xy(127, 94), 1, 2, 3, false, e_invalid},
   };
   for (const radial_case &c : cases) {
      rig r;
      geometry_64x48(r);
      for (unsigned p = 0; p < 3; ++p) {
         if (p == c.sel) {
            load_ee(r, p, c.feature, c.center, c.th0, c.th1, c.th2);
         } else {  // the residents not selected are illegal: they must not matter
            load_ee(r, p, radial, bad_center, 3, 2, 1);
         }
      }
      r.enter_profile_mode();
      r.cmd(c_init);
      r.wr(FX1_ISP_EE_CTRL_OFFSET, (c.en ? FX1_ISP_EE_CTRL_EN_MASK : 0u) | (c.sel << FX1_ISP_EE_CTRL_PROFILE_SEL_SHIFT));
      FX1_CHECK_EQ_CTX(r.err(), 0u, c.name);
      r.cmd(c_seal);
      FX1_CHECK_EQ_CTX(r.err(), c.err, c.name);
      FX1_CHECK_EQ_CTX(r.cu.profile_bank_sealed(0), c.err == 0, c.name);
   }
}

// Table 9-196 rectangles: the AWB ROI is in complete 2x2 cells (W/2 x H/2),
// AF windows and the weighted-histogram ROI in pixels (review V107-2..6 R2).
void test_seal_rect_domains() {
   auto seal = [](const std::function<void(rig &)> &body) {
      rig r;
      geometry_64x48(r);
      r.enter_profile_mode();
      r.cmd(c_init);
      body(r);
      r.cmd(c_seal);
      return r.err();
   };
   auto awb_roi = [](std::uint32_t x1, std::uint32_t y1) {
      return [=](rig &r) {
         r.wr(FX1_ISP_AWB_CTRL_OFFSET, 1u | (1u << 1) | (1u << 8));  // EN, 1 x 1 grid
         r.wr(FX1_ISP_TUNE_AWB_CTRL_OFFSET, FX1_ISP_TUNE_AWB_CTRL_ROI_EN_MASK);
         r.wr(FX1_ISP_TUNE_AWB_ROI_START_OFFSET, 0);
         r.wr(FX1_ISP_TUNE_AWB_ROI_END_OFFSET, x1 | (y1 << 16));
      };
   };
   FX1_CHECK_EQ(seal(awb_roi(32, 24)), 0u);          // the whole 32 x 24 cell raster
   FX1_CHECK_EQ(seal(awb_roi(33, 24)), e_invalid);  // one cell beyond W/2
   FX1_CHECK_EQ(seal(awb_roi(32, 25)), e_invalid);  // one cell beyond H/2
   FX1_CHECK_EQ(seal([](rig &r) {  // ROI [32, 33) x [0, 1): wholly outside the cells
                   r.wr(FX1_ISP_TUNE_AWB_CTRL_OFFSET, FX1_ISP_TUNE_AWB_CTRL_ROI_EN_MASK);
                   r.wr(FX1_ISP_TUNE_AWB_ROI_START_OFFSET, 32);
                   r.wr(FX1_ISP_TUNE_AWB_ROI_END_OFFSET, 33 | (1u << 16));
                }),
                e_invalid);
   auto af = [](std::uint32_t x1, std::uint32_t y1) {
      return [=](rig &r) {
         r.wr(FX1_ISP_TUNE_AF_CTRL_OFFSET, FX1_ISP_TUNE_AF_CTRL_WINDOW_MODE_MASK | (1u << 16));
         r.wr(FX1_ISP_TUNE_AF_W0_START_OFFSET, 0);
         r.wr(FX1_ISP_TUNE_AF_W0_END_OFFSET, x1 | (y1 << 16));
      };
   };
   FX1_CHECK_EQ(seal(af(64, 48)), 0u);  // pixels: the full frame, not half of it
   FX1_CHECK_EQ(seal(af(65, 48)), e_invalid);
   FX1_CHECK_EQ(seal(af(64, 49)), e_invalid);
   auto hist = [](std::uint32_t x1, std::uint32_t y1) {
      return [=](rig &r) {
         r.wr(FX1_ISP_TUNE_HIST_CTRL_OFFSET, FX1_ISP_TUNE_HIST_CTRL_ENABLE_MASK | FX1_ISP_TUNE_HIST_CTRL_ROI_EN_MASK);
         r.wr(FX1_ISP_TUNE_HIST_STEP_OFFSET, 0x0101);
         r.wr(FX1_ISP_TUNE_HIST_START_OFFSET, 0);
         r.wr(FX1_ISP_TUNE_HIST_END_OFFSET, x1 | (y1 << 16));
      };
   };
   FX1_CHECK_EQ(seal(hist(64, 48)), 0u);
   FX1_CHECK_EQ(seal(hist(65, 48)), e_invalid);
   FX1_CHECK_EQ(seal(hist(64, 49)), e_invalid);
}

// §9.9.5: a TUNE_BPC_COUNT write to a protected (sealed) bank is LOCKED and
// keeps the confirmation, also with the stored value (review V107-2..6 R3).
void test_bpc_count_locked() {
   rig r;
   geometry_64x48(r);
   r.enter_profile_mode();
   r.cmd(c_init);
   r.wr(FX1_ISP_TUNE_BPC_COUNT_OFFSET, 1);
   r.wr(FX1_ISP_TUNE_BPC_ADDR_OFFSET, 0);
   r.wr(FX1_ISP_TUNE_BPC_DATA_OFFSET, (1u << 16) | 1u);
   r.wr(FX1_ISP_TUNE_TABLE_CMD_OFFSET, FX1_ISP_TUNE_TABLE_CMD_BPC_CONFIRM_MASK);
   r.cmd(c_seal);
   FX1_CHECK_EQ(r.err(), 0u);
   for (const std::uint32_t count : {1u, 2u}) {
      r.wr(FX1_ISP_TUNE_BPC_COUNT_OFFSET, count);
      FX1_CHECK_EQ_CTX(r.err(), e_locked, std::to_string(count));
      FX1_CHECK_EQ_CTX(r.rd(FX1_ISP_TUNE_TABLE_STATUS_OFFSET) & FX1_ISP_TUNE_TABLE_STATUS_BPC_MASK,
                       FX1_ISP_TUNE_TABLE_STATUS_BPC_MASK, std::to_string(count));
      FX1_CHECK_EQ_CTX(r.rd(FX1_ISP_TUNE_BPC_COUNT_OFFSET), 1u, std::to_string(count));
   }
}

// §9.9.1: EE_STATUS.active_valid reports a confirmed active selection; an
// invalidation of the active resident clears it until the next local SOF, one
// of another resident does not; a common reset clears it (review R4).
void test_ee_active_valid() {
   constexpr std::uint32_t av = FX1_ISP_EE_STATUS_ACTIVE_VALID_MASK;
   rig r;
   geometry_64x48(r);
   load_ee(r, 0, 0, 0, 0, 0, 0);
   load_ee(r, 1, 0, 0, 0, 0, 0);
   auto sof = [&](unsigned sel) {
      r.wr(FX1_ISP_EE_CTRL_OFFSET, FX1_ISP_EE_CTRL_EN_MASK | (sel << FX1_ISP_EE_CTRL_PROFILE_SEL_SHIFT) |
                                       (1u << FX1_ISP_EE_CTRL_PROFILE_WR_SEL_SHIFT));
      FX1_CHECK(r.admit());
      r.retire();
   };
   sof(0);
   FX1_CHECK_EQ(r.rd(FX1_ISP_EE_STATUS_OFFSET) & av, av);
   r.wr(FX1_ISP_EE_ALPHA_OFFSET, 0x1234);  // resident 1 (PROFILE_WR_SEL): not the active one
   FX1_CHECK(!r.cu.ee_resident_valid(1));
   FX1_CHECK_EQ(r.rd(FX1_ISP_EE_STATUS_OFFSET) & av, av);
   r.wr(FX1_ISP_EE_CTRL_OFFSET, FX1_ISP_EE_CTRL_EN_MASK);  // PROFILE_WR_SEL 0: the active resident
   r.wr(FX1_ISP_EE_ALPHA_OFFSET, 0x1234);
   FX1_CHECK(!r.cu.ee_resident_valid(0));
   FX1_CHECK_EQ(r.rd(FX1_ISP_EE_STATUS_OFFSET) & av, 0u);  // scalar write
   r.wr(FX1_ISP_EE_CTRL_OFFSET, 0);  // EE disabled: confirm resident 0 again
   r.wr(FX1_ISP_EE_LUT_CTRL_OFFSET, FX1_ISP_EE_LUT_CTRL_PROFILE_LOAD_CONFIRM_MASK);
   FX1_CHECK(r.cu.ee_resident_valid(0));
   FX1_CHECK_EQ(r.rd(FX1_ISP_EE_STATUS_OFFSET) & av, 0u);  // set again only at the next local SOF
   sof(0);
   FX1_CHECK_EQ(r.rd(FX1_ISP_EE_STATUS_OFFSET) & av, av);
   r.wr(FX1_ISP_EE_CTRL_OFFSET, FX1_ISP_EE_CTRL_EN_MASK);
   r.wr(FX1_ISP_EE_LUT_CTRL_OFFSET, 0);
   r.wr(FX1_ISP_EE_LUT_ADDR_OFFSET, 0);
   r.wr(FX1_ISP_EE_LUT_WDATA_OFFSET, 0x8000);  // LUT write to the active resident
   FX1_CHECK_EQ(r.rd(FX1_ISP_EE_STATUS_OFFSET) & av, 0u);
   r.wr(FX1_ISP_EE_CTRL_OFFSET, 0);
   load_ee(r, 0, 0, 0, 0, 0, 0);
   sof(0);
   FX1_CHECK_EQ(r.rd(FX1_ISP_EE_STATUS_OFFSET) & av, av);
   r.wr(FX1_ISP_COMMON_CTRL_OFFSET, FX1_ISP_COMMON_CTRL_SOFT_RST_MASK);  // legacy common reset
   r.t += 64;
   FX1_CHECK(r.cu.ee_resident_valid(0));  // legacy keeps the table validity ...
   FX1_CHECK_EQ(r.rd(FX1_ISP_EE_STATUS_OFFSET) & av, 0u);  // ... but not active_valid
}

}  // namespace

// Table 9-196: an enabled gamma needs the bank's confirmed curve; a complete
// but unconfirmed curve, or one cleared by a later write, does not seal.
void test_seal_gamma_confirmation() {
   rig r;
   r.enter_profile_mode();
   r.cmd(c_init);
   for (unsigned i = 0; i < 256; ++i) {
      r.wr(FX1_ISP_TUNE_GAMMA_PORT_OFFSET, i);
      r.wr(FX1_ISP_TUNE_GAMMA_DATA_OFFSET, 16u * i);
   }
   r.wr(FX1_ISP_GAMMA_CTRL_OFFSET, FX1_ISP_GAMMA_CTRL_EN_MASK);
   r.cmd(c_seal);  // all 256 entries written, not confirmed
   FX1_CHECK_EQ(r.err(), e_not_ready);
   r.wr(FX1_ISP_TUNE_TABLE_CMD_OFFSET, FX1_ISP_TUNE_TABLE_CMD_GAMMA_CONFIRM_MASK);
   FX1_CHECK_EQ(r.err(), 0u);
   r.wr(FX1_ISP_TUNE_GAMMA_PORT_OFFSET, 7);
   r.wr(FX1_ISP_TUNE_GAMMA_DATA_OFFSET, 5);  // clears the confirmation
   r.cmd(c_seal);
   FX1_CHECK_EQ(r.err(), e_not_ready);
   r.wr(FX1_ISP_TUNE_TABLE_CMD_OFFSET, FX1_ISP_TUNE_TABLE_CMD_GAMMA_CONFIRM_MASK);
   r.cmd(c_seal);
   FX1_CHECK_EQ(r.err(), 0u);
}

// §9.7 (V107-D23, V107-D28): the automatic GTM curve and 2DNR estimate are
// histories of the profile epoch. The first frame of a new epoch has neither
// (GTM pass-through, 2DNR SEED), although the old epoch published both.
void test_history_per_epoch() {
   rig r;
   r.wr(FX1_ISP_GTM_CTRL_OFFSET, FX1_ISP_GTM_CTRL_EN_MASK);
   r.wr(FX1_ISP_NR_2D_CTRL_OFFSET, FX1_ISP_NR_2D_CTRL_EN_MASK);
   r.enter_profile_mode();
   r.prepare(0, 1);
   r.cmd(c_apply);
   ++r.t;
   FX1_CHECK(r.admit());  // epoch 1, frame 1
   r.cu.gtm_end_of_frame(100);
   r.cu.nr_publish(321);
   r.retire();
   FX1_CHECK(r.admit());  // frame 2 uses frame 1's curve and estimate
   FX1_CHECK(r.cu.gtm_curve_valid());
   FX1_CHECK(r.cu.nr_estimate_valid());
   r.prepare(1, 2);
   r.cmd(c_apply);
   r.cu.gtm_end_of_frame(110);  // frame 2 still publishes into epoch 1
   r.cu.nr_publish(400);
   r.retire();
   ++r.t;
   FX1_CHECK(r.admit());  // epoch 2 after the drain
   FX1_CHECK_EQ(r.rd(FX1_ISP_TUNE_ACTIVE_ID_OFFSET), 2u);
   FX1_CHECK(!r.cu.gtm_curve_valid());
   FX1_CHECK(!r.cu.nr_estimate_valid());
   r.cu.gtm_end_of_frame(120);
   r.cu.nr_publish(55);
   r.retire();
   FX1_CHECK(r.admit());  // the new epoch's own history
   FX1_CHECK(r.cu.gtm_curve_valid());
   FX1_CHECK(r.cu.nr_estimate_valid());
}

int main() {
   test_legacy_mode();
   test_init_edit_seal_apply();
   test_command_rules();
   test_cancel_and_wrap();
   test_admission_holds();
   test_idle_class();
   test_legacy_commands_in_profile_mode();
   test_seal_validation();
   test_resets();
   test_profile_tables();
   test_resident_pins();
   test_seal_ee_radial();
   test_seal_rect_domains();
   test_bpc_count_locked();
   test_ee_active_valid();
   test_history_per_epoch();
   test_seal_gamma_confirmation();
   return fx1_test::summary("fx1_isp_test_profile");
}
