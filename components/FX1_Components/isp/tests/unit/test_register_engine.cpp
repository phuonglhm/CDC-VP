// SPDX-License-Identifier: Apache-2.0
// Register engine and discovery of CSR v1.07 (milestone V107-1). Expected
// values come from MAS v1.07 Table 8-178 (access types), Table 8-180 (per-field
// update and priority rules, with its worked examples) and §8.2 (discovery),
// not from the generated table.
//   - W1P: a write of 1 issues a one-clock command, reads zero, is repeatable,
//     follows the byte strobes, and is not stored; every W1P field of the map
//   - mixed RW + W1P word: the hook sees the old and the new RW value and the pulse
//   - soft-reset window: W1P commands and W1S sets dropped, RW kept
//   - W1S: software set wins over a hardware clear on the same edge, Q = 0 or
//     1, in either call order (review V107-1 R1)
//   - registered view: a consumer samples Q of its edge, a set reaches it on
//     the next edge (V107-A6); AEC commit on the SOF edge
//   - W1C: hardware set wins over a software clear and a hardware clear on the
//     same edge; the Table 8-180 examples
//   - W1SC reads zero and pulses
//   - pre-write filter (reject / partial lanes) and bus-read actions (not run
//     by debug reads)
//   - reserved custom-size words, buffer bits above the build count
//   - discovery: COMMON_VER_ID/DATE, TUNE_ID/REV, TUNE_CAPS reports the implemented subset
//     unimplemented (V107-A1)

#include <cstdint>
#include <string>
#include <vector>

#include "control/control_unit.h"
#include "fx1_check.h"
#include "fx1_isp/fx1_isp_csr.h"
#include "registers/register_file.h"

using namespace cdc::components::fx1_isp;

namespace {

constexpr std::uint32_t all = 0xFFFFFFFFu;

void test_w1p_every_field() {
   // Every W1P field of the map (19 in v1.07): a write of 1 pulses, reads 0.
   register_file rf;
   std::size_t fields = 0;
   for (std::size_t i = 0; i < csr::num_registers; ++i) {
      const csr::reg_desc &d = csr::registers[i];
      if (!d.w1p_mask) {
         continue;
      }
      for (std::uint8_t k = 0; k < d.num_fields; ++k) {
         fields += d.fields[k].type == csr::access::w1p;
      }
      std::vector<std::uint32_t> pulses;
      rf.on_write(d.offset, [&](const register_file::write_effect &fx) { pulses.push_back(fx.w1p_pulse); });
      const std::string n = d.name;
      rf.sw_write(d.offset, d.w1p_mask, all, 10);
      FX1_CHECK_EQ_CTX(pulses.size(), 1u, n + " hook");
      FX1_CHECK_EQ_CTX(pulses.empty() ? 0u : pulses[0], d.w1p_mask, n + " pulse");
      FX1_CHECK_EQ_CTX(rf.sw_read(d.offset) & d.w1p_mask, 0u, n + " reads zero");
      FX1_CHECK_EQ_CTX(rf.peek(d.offset) & d.w1p_mask, 0u, n + " not stored");
      rf.sw_write(d.offset, d.w1p_mask, all, 11);  // repeat: a second command
      FX1_CHECK_EQ_CTX(pulses.size() == 2 ? pulses[1] : 0u, d.w1p_mask, n + " repeated pulse");
      rf.sw_write(d.offset, 0, all, 12);  // zero: no command
      FX1_CHECK_EQ_CTX(pulses.size() == 3 ? pulses[2] : 1u, 0u, n + " zero write");
      rf.hw_write(d.offset, all, all);  // hardware cannot store a command bit either
      rf.hw_set(d.offset, all, 13);
      FX1_CHECK_EQ_CTX(rf.peek(d.offset) & d.w1p_mask, 0u, n + " hardware write not stored");
      FX1_CHECK_EQ_CTX(rf.sw_read(d.offset) & d.w1p_mask, 0u, n + " reads zero after hardware write");
   }
   FX1_CHECK_EQ(fields, 19u);
}

void test_w1p_strobes() {
   // TUNE_CMD: INIT..EXACT are W1P bits in byte lane 0; a write whose strobes
   // miss lane 0 still reaches the consumer (nonzero strobes) with no pulse.
   register_file rf;
   std::uint32_t pulse = 0xDEAD, hooks = 0;
   rf.on_write(FX1_ISP_TUNE_CMD_OFFSET, [&](const register_file::write_effect &fx) {
      pulse = fx.w1p_pulse;
      ++hooks;
   });
   rf.sw_write(FX1_ISP_TUNE_CMD_OFFSET, FX1_ISP_TUNE_CMD_SEAL_MASK, 0xFFFFFF00u, 1);
   FX1_CHECK_EQ(hooks, 1u);
   FX1_CHECK_EQ(pulse, 0u);
   rf.sw_write(FX1_ISP_TUNE_CMD_OFFSET, FX1_ISP_TUNE_CMD_SEAL_MASK, 0x000000FFu, 2);
   FX1_CHECK_EQ(pulse, FX1_ISP_TUNE_CMD_SEAL_MASK);
   rf.sw_write(FX1_ISP_TUNE_CMD_OFFSET, all, 0, 3);  // no strobes: nothing at all
   FX1_CHECK_EQ(hooks, 2u);
}

void test_w1p_command_block() {
   // Soft-reset window: RW still updates, W1S/W1C effects and W1P commands are
   // dropped (V107-D4); the write still reaches the consumer.
   register_file rf;
   register_file::write_effect last{};
   rf.on_write(FX1_ISP_GAMMA_CTRL_OFFSET, [&](const register_file::write_effect &fx) { last = fx; });
   rf.set_command_block(true);
   rf.sw_write(FX1_ISP_GAMMA_CTRL_OFFSET, FX1_ISP_GAMMA_CTRL_EN_MASK | FX1_ISP_GAMMA_CTRL_LUT_LOAD_CONFIRM_MASK, all, 1);
   FX1_CHECK_EQ(last.w1p_pulse, 0u);
   FX1_CHECK_EQ(rf.sw_read(FX1_ISP_GAMMA_CTRL_OFFSET), FX1_ISP_GAMMA_CTRL_EN_MASK);
   rf.sw_write(FX1_ISP_IDMA_BUF_VALID_OFFSET, 0x1, all, 1);
   FX1_CHECK_EQ(rf.sw_read(FX1_ISP_IDMA_BUF_VALID_OFFSET), 0u);
   rf.set_command_block(false);
   rf.sw_write(FX1_ISP_GAMMA_CTRL_OFFSET, FX1_ISP_GAMMA_CTRL_LUT_LOAD_CONFIRM_MASK, all, 2);
   FX1_CHECK_EQ(last.w1p_pulse, FX1_ISP_GAMMA_CTRL_LUT_LOAD_CONFIRM_MASK);
}

void test_mixed_rw_w1p() {
   // GAMMA_CTRL: EN (RW) and LUT_LOAD_CONFIRM (W1P) in one word.
   register_file rf;
   register_file::write_effect last{};
   rf.on_write(FX1_ISP_GAMMA_CTRL_OFFSET, [&](const register_file::write_effect &fx) { last = fx; });
   rf.sw_write(FX1_ISP_GAMMA_CTRL_OFFSET, FX1_ISP_GAMMA_CTRL_EN_MASK, all, 1);
   rf.sw_write(FX1_ISP_GAMMA_CTRL_OFFSET, FX1_ISP_GAMMA_CTRL_LUT_LOAD_CONFIRM_MASK, all, 2);  // EN 1 -> 0 + confirm
   FX1_CHECK_EQ(last.w1p_pulse, FX1_ISP_GAMMA_CTRL_LUT_LOAD_CONFIRM_MASK);
   FX1_CHECK_EQ(last.old_value & FX1_ISP_GAMMA_CTRL_EN_MASK, FX1_ISP_GAMMA_CTRL_EN_MASK);  // pre-edge EN
   FX1_CHECK_EQ(last.new_value & FX1_ISP_GAMMA_CTRL_EN_MASK, 0u);
   FX1_CHECK_EQ(rf.sw_read(FX1_ISP_GAMMA_CTRL_OFFSET), 0u);
}

void test_w1s_software_set_wins() {
   // IDMA_BUF_VALID (W1S): (Q & ~H) | (D&M) on the same edge, Q = 0 or 1,
   // whichever call the model makes first within the cycle.
   const std::uint32_t off = FX1_ISP_IDMA_BUF_VALID_OFFSET;
   for (const std::uint32_t q : {0u, 1u}) {
      for (const bool clear_first : {false, true}) {
         register_file rf;
         const std::string c = "Q=" + std::to_string(q) + (clear_first ? " clear,set" : " set,clear");
         if (q) {
            rf.sw_write(off, 0x1, all, 4);
         }
         if (clear_first) {
            rf.hw_clear(off, 0x1, 5);
         }
         rf.sw_write(off, 0x1, all, 5);
         if (!clear_first) {
            rf.hw_clear(off, 0x1, 5);
         }
         FX1_CHECK_EQ_CTX(rf.sw_read(off), 0x1u, c);
         rf.hw_clear(off, 0x1, 6);  // later edge: cleared
         FX1_CHECK_EQ_CTX(rf.sw_read(off), 0x0u, c + " next edge");
      }
   }
   register_file rf;
   rf.sw_write(off, 0x3, 0x0000FF00u, 6);  // strobes miss the lane: nothing set, nothing protected
   rf.hw_clear(off, 0x3, 6);
   FX1_CHECK_EQ(rf.sw_read(off), 0x0u);
   rf.sw_write(off, 0x2, all, 6);
   rf.sw_write(off, 0x1, all, 7);  // a set of bit 0 does not protect bit 1
   rf.hw_clear(off, 0x3, 7);
   FX1_CHECK_EQ(rf.sw_read(off), 0x1u);
   rf.hw_ack(off, 0x1);  // self-feedback acknowledgement clears at once
   FX1_CHECK_EQ(rf.sw_read(off), 0x0u);
}

void test_registered_view() {
   // A consumer clocked on edge c samples Q of that edge: a W1S set from 0 on
   // c reaches it on c+1; a re-set of a bit already 1 is seen on c.
   register_file rf;
   const std::uint32_t off = FX1_ISP_ODMA_BUF_FREE_OFFSET;
   rf.sw_write(off, 0x1, all, 10);
   FX1_CHECK_EQ(rf.registered(off, 10), 0x0u);
   FX1_CHECK_EQ(rf.registered(off, 11), 0x1u);
   rf.sw_write(off, 0x3, all, 12);  // bit 0 re-set (already 1), bit 1 new
   FX1_CHECK_EQ(rf.registered(off, 12), 0x1u);
   FX1_CHECK_EQ(rf.registered(off, 13), 0x3u);
   rf.sw_write(off, 0x4, all, 14);  // two writes on one edge: Q is the value before the first
   rf.sw_write(off, 0x4, all, 14);
   FX1_CHECK_EQ(rf.registered(off, 14), 0x3u);
   rf.hw_clear(off, 0x1, 15);  // consumer takes buffer 0 on edge 15 ...
   rf.sw_write(off, 0x1, all, 15);  // ... software frees it again on the same edge
   FX1_CHECK_EQ(rf.sw_read(off) & 0x1u, 0x1u);  // set wins: a new request
   FX1_CHECK_EQ(rf.registered(off, 15) & 0x1u, 0x1u);  // Q of edge 15 was 1
   FX1_CHECK_EQ(rf.registered(off, 16) & 0x1u, 0x1u);
   rf.reset();
   FX1_CHECK_EQ(rf.registered(off, 0), 0x0u);
}

void test_aec_commit_on_sof_edge() {
   // AEC_CTRL.commit (W1S) is consumed at an accepted SOF from the registered
   // Q: a commit written on the SOF edge is taken by the next SOF, so that
   // frame keeps the previous shadow set. The bit itself stays set on the SOF
   // edge either way (software set wins), so the shadow set is the evidence.
   control_unit cu;
   const std::uint32_t commit = FX1_ISP_AEC_CTRL_EN_MASK | FX1_ISP_AEC_CTRL_COMMIT_MASK;
   auto th_ue = [&] { return cu.aec_active().th_ue; };
   cu.csr_write(FX1_ISP_AEC_THRESH_OFFSET, 0x0EA00010u, all, 10);
   cu.csr_write(FX1_ISP_AEC_CTRL_OFFSET, commit, all, 20);
   cu.accepted_sof(50);  // committed before the edge: transferred
   FX1_CHECK_EQ(th_ue(), 0x010u);
   cu.csr_write(FX1_ISP_AEC_THRESH_OFFSET, 0x0EA00020u, all, 60);
   cu.csr_write(FX1_ISP_AEC_CTRL_OFFSET, commit, all, 100);
   cu.accepted_sof(100);  // committed on the SOF edge: not this frame
   FX1_CHECK_EQ(th_ue(), 0x010u);
   FX1_CHECK_EQ(cu.regs().peek(FX1_ISP_AEC_CTRL_OFFSET) & FX1_ISP_AEC_CTRL_COMMIT_MASK, FX1_ISP_AEC_CTRL_COMMIT_MASK);
   cu.accepted_sof(200);  // the next SOF takes it
   FX1_CHECK_EQ(th_ue(), 0x020u);
   FX1_CHECK_EQ(cu.regs().peek(FX1_ISP_AEC_CTRL_OFFSET) & FX1_ISP_AEC_CTRL_COMMIT_MASK, 0u);
}

void test_w1c_hardware_set_wins() {
   // DMA_ERR (W1C): (Q & ~(D&M | H)) | S, and the Table 8-180 examples.
   register_file rf;
   const std::uint32_t off = FX1_ISP_DMA_ERR_OFFSET;
   rf.hw_set(off, 0x05, 1);  // Q = 0x05
   rf.sw_write(off, 0x01, 0xFFFFFF00u, 2);  // WSTRB = 0xE: lane 0 not written
   FX1_CHECK_EQ(rf.sw_read(off), 0x05u);
   rf.sw_write(off, 0x01, 0x000000FFu, 3);  // WSTRB = 1
   FX1_CHECK_EQ(rf.sw_read(off), 0x04u);
   rf.hw_set(off, 0x01, 4);
   rf.sw_write(off, 0x01, 0x000000FFu, 4);  // simultaneous set: result stays 0x05
   FX1_CHECK_EQ(rf.sw_read(off), 0x05u);
   rf.hw_set(off, 0x02, 8);
   rf.hw_clear(off, 0x02, 8);  // hardware set wins over a hardware clear too
   FX1_CHECK_EQ(rf.sw_read(off) & 0x2u, 0x2u);
   rf.hw_clear(off, 0x02, 9);
   FX1_CHECK_EQ(rf.sw_read(off) & 0x2u, 0x0u);
   // RW example: Q = 0xAABBCCDD, D = 0x11223344, WSTRB = 0x5 -> 0xAA22CC44.
   const std::uint32_t s = FX1_ISP_COMMON_SCRATCH_OFFSET;
   rf.sw_write(s, 0xAABBCCDDu, all, 10);
   rf.sw_write(s, 0x11223344u, 0x00FF00FFu, 11);
   FX1_CHECK_EQ(rf.sw_read(s), 0xAA22CC44u);
}

void test_w1sc_reads_zero() {
   register_file rf;
   std::uint32_t pulse = 0;
   rf.on_write(FX1_ISP_DMA_CTRL_OFFSET, [&](const register_file::write_effect &fx) { pulse = fx.w1sc_pulse; });
   rf.sw_write(FX1_ISP_DMA_CTRL_OFFSET, FX1_ISP_DMA_CTRL_SOFT_RESET_MASK, all, 1);
   FX1_CHECK_EQ(pulse, FX1_ISP_DMA_CTRL_SOFT_RESET_MASK);
   FX1_CHECK_EQ(rf.sw_read(FX1_ISP_DMA_CTRL_OFFSET) & FX1_ISP_DMA_CTRL_SOFT_RESET_MASK, 0u);
}

void test_pre_write_and_bus_read() {
   register_file rf;
   const std::uint32_t s = FX1_ISP_COMMON_SCRATCH_OFFSET;
   unsigned hooks = 0;
   rf.on_write(s, [&](const register_file::write_effect &) { ++hooks; });
   rf.on_pre_write(s, [](std::uint32_t data, std::uint32_t, std::uint64_t) {
      return data == 0xBAD0BAD0u ? 0u : 0x0000FFFFu;  // reject one value; lanes 0-1 only otherwise
   });
   rf.sw_write(s, 0x12345678u, all, 1);
   FX1_CHECK_EQ(rf.sw_read(s), 0x00005678u);
   FX1_CHECK_EQ(hooks, 1u);
   rf.sw_write(s, 0xBAD0BAD0u, all, 2);  // rejected: no storage change, no hook
   FX1_CHECK_EQ(rf.sw_read(s), 0x00005678u);
   FX1_CHECK_EQ(hooks, 1u);
   unsigned actions = 0;
   std::uint64_t at = 0;
   rf.on_bus_read(s, [&](std::uint64_t cycle) {
      ++actions;
      at = cycle;
   });
   (void)rf.sw_read(s);  // debug view: no side effect
   FX1_CHECK_EQ(actions, 0u);
   FX1_CHECK_EQ(rf.bus_read(s, 42), 0x00005678u);
   FX1_CHECK_EQ(actions, 1u);
   FX1_CHECK_EQ(at, 42u);
}

void test_reserved_words_and_bits() {
   register_file rf;
   for (const std::uint32_t off : {FX1_ISP_RESIZER_CUSTOM_W_OFFSET, FX1_ISP_RESIZER_CUSTOM_H_OFFSET,
                                   FX1_ISP_CH1_RESIZER_CUSTOM_W_OFFSET, FX1_ISP_CH1_RESIZER_CUSTOM_H_OFFSET,
                                   FX1_ISP_CH2_RESIZER_CUSTOM_W_OFFSET, FX1_ISP_CH2_RESIZER_CUSTOM_H_OFFSET}) {
      rf.sw_write(off, all, all, 1);
      FX1_CHECK_EQ_CTX(rf.sw_read(off), 0u, "custom-size word reads zero @" + fx1_test::hex(off));
   }
   // Buffer bits at or above the build count (4) read zero and ignore writes.
   rf.sw_write(FX1_ISP_IDMA_BUF_VALID_OFFSET, 0xFF, all, 2);
   FX1_CHECK_EQ(rf.sw_read(FX1_ISP_IDMA_BUF_VALID_OFFSET), 0x0Fu);
   rf.sw_write(FX1_ISP_CH2_ODMA_BUF_FREE_OFFSET, 0xFF, all, 2);
   FX1_CHECK_EQ(rf.sw_read(FX1_ISP_CH2_ODMA_BUF_FREE_OFFSET), 0x0Fu);
   // An unlisted word of the extension page: unmapped.
   FX1_CHECK(rf.find(0x8B04u) == nullptr);
   rf.sw_write(0x8B04u, all, all, 3);
   FX1_CHECK_EQ(rf.sw_read(0x8B04u), 0u);
}

void test_discovery() {
   control_unit cu;
   FX1_CHECK_EQ(cu.csr_read(FX1_ISP_COMMON_VER_ID_OFFSET), 0x00010007u);
   FX1_CHECK_EQ(cu.csr_read(FX1_ISP_COMMON_VER_DATE_OFFSET), 0x18102026u);
   FX1_CHECK_EQ(cu.csr_read(FX1_ISP_TUNE_ID_OFFSET), 0x54554E45u);  // "TUNE"
   FX1_CHECK_EQ(cu.csr_read(FX1_ISP_TUNE_REV_OFFSET), 0x00010007u);
   // The map's reset describes a complete implementation; the VP reports only
   // what it implements (V107-A1): after V107-6 every bit except CHANNEL_CROP
   // (16), CSC_WIDE_RANGE (17), RECOVERY_REPORT (18) and FRAME_ID (19).
   FX1_CHECK_EQ(FX1_ISP_TUNE_CAPS_RESET, 0x003FFFFFu);
   FX1_CHECK_EQ(control_unit::implemented_caps, 0x003FFFFFu);
   FX1_CHECK_EQ(cu.csr_read(FX1_ISP_TUNE_CAPS_OFFSET), 0x003FFFFFu);
   FX1_CHECK_EQ(cu.csr_bus_read(FX1_ISP_TUNE_CAPS_OFFSET, 5), 0x003FFFFFu);
   cu.csr_write(FX1_ISP_TUNE_CAPS_OFFSET, all, all, 6);  // read-only
   FX1_CHECK_EQ(cu.csr_read(FX1_ISP_TUNE_CAPS_OFFSET), 0x003FFFFFu);
}

}  // namespace

int main() {
   test_w1p_every_field();
   test_w1p_strobes();
   test_w1p_command_block();
   test_mixed_rw_w1p();
   test_w1s_software_set_wins();
   test_registered_view();
   test_aec_commit_on_sof_edge();
   test_w1c_hardware_set_wins();
   test_w1sc_reads_zero();
   test_pre_write_and_bus_read();
   test_reserved_words_and_bits();
   test_discovery();
   return fx1_test::summary("fx1_isp_test_register_engine");
}
