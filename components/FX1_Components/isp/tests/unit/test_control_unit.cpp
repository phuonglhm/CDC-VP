// SPDX-License-Identifier: Apache-2.0
// Unit tests of the FX1 ISP register file and control unit (M1, no SystemC).
//
// Reset values in `literal_resets` are copied by hand from the CSR
// spreadsheet, independently of tools/gen_csr.py, so a generator defect cannot
// hide behind the table it produced. The generic sweeps use the generated
// table only to know which bits carry which access type, and check behaviour.

#include <cstdint>
#include <string>
#include <vector>

#include "control/control_unit.h"
#include "fx1_check.h"
#include "fx1_isp/fx1_isp_csr.h"

using namespace cdc::components::fx1_isp;

namespace {

constexpr std::uint32_t all_lanes = 0xFFFFFFFFu;

std::uint32_t rd(const control_unit &cu, std::uint32_t off) { return cu.csr_read(off); }
void wr(control_unit &cu, std::uint32_t off, std::uint32_t v, std::uint64_t cycle = 1,
        std::uint32_t lanes = all_lanes) {
   cu.csr_write(off, v, lanes, cycle);
}

bool has_read_hook(std::uint32_t off) {
   return off == FX1_ISP_COMMON_STATUS_OFFSET || off == FX1_ISP_GAMMA_LUT_RDATA_OFFSET ||
          off == FX1_ISP_EE_LUT_RDATA_OFFSET || off == FX1_ISP_GTM_LUT_RDATA_OFFSET ||
          off == FX1_ISP_RESIZER_OUT_W_OFFSET || off == FX1_ISP_RESIZER_OUT_H_OFFSET;
}

// Registers whose writes have block side effects (covered by dedicated tests).
bool triggers_soft_reset(std::uint32_t off) {
   return off == FX1_ISP_COMMON_CTRL_OFFSET || off == FX1_ISP_DMA_CTRL_OFFSET ||
          off == FX1_ISP_LSC_LOAD_CTRL_OFFSET || off == FX1_ISP_LSC_PROFILE_SEL_OFFSET;
}

void test_table_shape() {
   FX1_CHECK_EQ(csr::num_registers, 226);
   std::size_t fields = 0;
   for (std::size_t i = 0; i < csr::num_registers; ++i) {
      fields += csr::registers[i].num_fields;
   }
   FX1_CHECK_EQ(fields, 298);
}

void test_literal_resets() {
   struct lit {
      std::uint32_t off;
      std::uint32_t value;
   };
   const lit literal_resets[] = {
      {0x0000, 0x18072026}, {0x0004, 0x00010002}, {0x0008, 0x00000000}, {0x0018, 0x00000780},
      {0x001C, 0x00000438}, {0x0020, 0x00000000}, {0x0028, 0x00000000}, {0x0204, 0x00000100},
      {0x0208, 0x000000C8}, {0x021C, 0x000000C8}, {0x0258, 0x00010000}, {0x0264, 0x00010000},
      {0x0404, 0x00000000}, {0x0604, 0x00000001}, {0x0608, 0x00000100}, {0x060C, 0x004D0192},
      {0x0610, 0x000004B0}, {0x0804, 0x00000100}, {0x0808, 0x00000100}, {0x080C, 0x00000100},
      {0x0A04, 0x00000100}, {0x1004, 0x00000200}, {0x1008, 0x00000000}, {0x1014, 0x00000200},
      {0x1024, 0x00000200}, {0x1400, 0x00000000}, {0x160C, 0x0000002E}, {0x1610, 0x00000400},
      {0x1614, 0x0000000B}, {0x1C04, 0x00004000}, {0x1C08, 0x00002000}, {0x1C0C, 0x00001810},
      {0x1C24, 0x80008000}, {0x1C28, 0x80008000}, {0x2014, 0x00000780}, {0x2018, 0x00000438},
      {0x2208, 0x00001820}, {0x220C, 0x005A0078}, {0x2210, 0x0FFF0000}, {0x2214, 0x0F000100},
      {0x2458, 0x00000FFF}, {0x2604, 0x0000FFFF}, {0x2804, 0x00000F00}, {0x2808, 0x00000F00},
      {0x3000, 0x00003F00}, {0x3020, 0x00001E00}, {0x3080, 0x00000F00}, {0x3084, 0x00000F00},
      {0x30DC, 0x00000000},
   };
   control_unit cu;
   for (const lit &l : literal_resets) {
      FX1_CHECK_EQ_CTX(rd(cu, l.off), l.value, "reset @" + fx1_test::hex(l.off));
   }
}

// Every register: reset readback, all-ones write, all-zeros write, against the
// access type of every bit.
void test_access_sweep() {
   for (std::size_t i = 0; i < csr::num_registers; ++i) {
      const csr::reg_desc &d = csr::registers[i];
      if (has_read_hook(d.offset) || triggers_soft_reset(d.offset)) {
         continue;
      }
      control_unit cu;
      const std::string ctx = d.name;
      const std::uint32_t rst = d.reset & ~d.w1sc_mask;
      FX1_CHECK_EQ_CTX(rd(cu, d.offset), rst, ctx + " reset");
      wr(cu, d.offset, 0xFFFFFFFFu);
      const std::uint32_t after_ones = (rst & ~d.rw_mask) | d.rw_mask | d.w1s_mask;
      FX1_CHECK_EQ_CTX(rd(cu, d.offset), after_ones, ctx + " after 0xFFFFFFFF");
      wr(cu, d.offset, 0);
      const std::uint32_t after_zeros = (rst & ~d.rw_mask) | d.w1s_mask;
      FX1_CHECK_EQ_CTX(rd(cu, d.offset), after_zeros, ctx + " after 0x0");
   }
}

void test_unmapped_and_reserved() {
   control_unit cu;
   std::vector<std::uint32_t> before;
   for (std::size_t i = 0; i < csr::num_registers; ++i) {
      before.push_back(cu.regs().peek(csr::registers[i].offset));
   }
   std::size_t unmapped = 0;
   for (std::uint32_t off = 0; off < 0x10000u; off += 4) {
      if (cu.regs().find(off)) {
         continue;
      }
      ++unmapped;
      wr(cu, off, 0xFFFFFFFFu);
      FX1_CHECK_EQ_CTX(rd(cu, off), 0, "unmapped @" + fx1_test::hex(off));
   }
   FX1_CHECK_EQ(unmapped, 16384 - 226);
   for (std::size_t i = 0; i < csr::num_registers; ++i) {
      FX1_CHECK_EQ_CTX(cu.regs().peek(csr::registers[i].offset), before[i],
                       std::string("unmapped writes changed ") + csr::registers[i].name);
   }
   // Reserved bits read zero after an all-ones write (BLC_GAIN_SEL: [1:0], [8]).
   wr(cu, FX1_ISP_BLC_GAIN_SEL_OFFSET, 0xFFFFFFFFu);
   FX1_CHECK_EQ(rd(cu, FX1_ISP_BLC_GAIN_SEL_OFFSET), 0x103);
   // Read-only word ignores writes.
   wr(cu, FX1_ISP_COMMON_VER_ID_OFFSET, 0);
   FX1_CHECK_EQ(rd(cu, FX1_ISP_COMMON_VER_ID_OFFSET), 0x00010002);
}

void test_strobes_and_alias() {
   control_unit cu;
   const std::uint32_t s = FX1_ISP_COMMON_SCRATCH_OFFSET;
   wr(cu, s, 0xAABBCCDDu, 1, 0x0000FF00u);
   FX1_CHECK_EQ(rd(cu, s), 0x0000CC00);
   wr(cu, s, 0x11223344u, 1, 0xFF000000u);
   FX1_CHECK_EQ(rd(cu, s), 0x1100CC00);
   wr(cu, s, 0xFFFFFFFFu, 1, 0);  // no strobes: accepted, changes nothing
   FX1_CHECK_EQ(rd(cu, s), 0x1100CC00);
   // Address bits [1:0] are ignored (HAS Table 7-10).
   wr(cu, s + 3, 0x12345678u);
   FX1_CHECK_EQ(rd(cu, s + 1), 0x12345678);
   // A strobe on a lane holding only W1C bits of a status register.
   cu.hw_set(FX1_ISP_DMA_ERR_OFFSET, 0x1F, 1);
   wr(cu, FX1_ISP_DMA_ERR_OFFSET, 0xFFFFFFFFu, 2, 0xFFFFFF00u);
   FX1_CHECK_EQ(rd(cu, FX1_ISP_DMA_ERR_OFFSET), 0x1F);
}

void test_w1c_semantics() {
   control_unit cu;
   const std::uint32_t e = FX1_ISP_DMA_ERR_OFFSET;
   cu.hw_set(e, 0x15, 10);
   FX1_CHECK_EQ(rd(cu, e), 0x15);
   wr(cu, e, 0x04, 11);  // per-bit clear
   FX1_CHECK_EQ(rd(cu, e), 0x11);
   // Hardware set wins over a software clear in the same cycle.
   cu.hw_set(e, 0x02, 20);
   wr(cu, e, 0x02, 20);
   FX1_CHECK_EQ(rd(cu, e), 0x13);
   wr(cu, e, 0x02, 21);
   FX1_CHECK_EQ(rd(cu, e), 0x11);
   // Software clear first, hardware set later in the same cycle: set survives.
   wr(cu, e, 0x01, 30);
   cu.hw_set(e, 0x01, 30);
   FX1_CHECK_EQ(rd(cu, e), 0x11);
   // Reserved bit 5 is tied to zero.
   cu.hw_set(e, 0x20, 31);
   FX1_CHECK_EQ(rd(cu, e) & 0x20, 0);
}

void test_level_hold() {
   control_unit cu;
   const std::uint32_t e = FX1_ISP_DMA_ERR_OFFSET;
   const std::uint32_t underrun = FX1_ISP_DMA_ERR_ERR_IDMA_UNDERRUN_BIT;
   cu.hw_hold(e, underrun, true, 5);
   wr(cu, e, underrun, 6);
   FX1_CHECK_EQ(rd(cu, e) & underrun, underrun);  // persists while the condition holds
   cu.hw_hold(e, underrun, false, 7);
   FX1_CHECK_EQ(rd(cu, e) & underrun, underrun);  // still sticky
   wr(cu, e, underrun, 8);
   FX1_CHECK_EQ(rd(cu, e) & underrun, 0);
}

void test_irq_aggregation() {
   control_unit cu;
   const std::uint32_t st = FX1_ISP_COMMON_IRQ_STATUS_OFFSET;
   const std::uint32_t en = FX1_ISP_COMMON_IRQ_EN_OFFSET;
   FX1_CHECK(!cu.irq_level());
   cu.hw_set(st, FX1_ISP_COMMON_IRQ_STATUS_FRAME_DONE_IRQ_MASK, 1);
   FX1_CHECK_EQ(rd(cu, st), 1);  // latched while masked
   FX1_CHECK(!cu.irq_level());
   wr(cu, en, FX1_ISP_COMMON_IRQ_EN_FRAME_DONE_EN_MASK, 2);
   FX1_CHECK(cu.irq_level());
   // DMA group, bit 0 tied to zero in both registers.
   cu.hw_set(FX1_ISP_DMA_IRQ_STAT_OFFSET, 0x7F, 3);
   FX1_CHECK_EQ(rd(cu, FX1_ISP_DMA_IRQ_STAT_OFFSET), 0x7E);
   wr(cu, FX1_ISP_DMA_IRQ_EN_OFFSET, 0x7F, 3);
   FX1_CHECK_EQ(rd(cu, FX1_ISP_DMA_IRQ_EN_OFFSET), 0x7E);
   wr(cu, st, 1, 4);
   FX1_CHECK(cu.irq_level());  // DMA group still pending
   wr(cu, FX1_ISP_DMA_IRQ_STAT_OFFSET, 0x3E, 5);
   FX1_CHECK(cu.irq_level());  // bit 6 remains
   wr(cu, FX1_ISP_DMA_IRQ_STAT_OFFSET, 0x40, 6);
   FX1_CHECK(!cu.irq_level());
   // Unmasking a latched bit raises the line; masking it drops it.
   cu.hw_set(FX1_ISP_DMA_IRQ_STAT_OFFSET, FX1_ISP_DMA_IRQ_STAT_IRQ_ODMA_DONE_BIT, 7);
   wr(cu, FX1_ISP_DMA_IRQ_EN_OFFSET, 0, 8);
   FX1_CHECK(!cu.irq_level());
   wr(cu, FX1_ISP_DMA_IRQ_EN_OFFSET, FX1_ISP_DMA_IRQ_EN_IRQ_EN_ODMA_DONE_BIT, 9);
   FX1_CHECK(cu.irq_level());
}

void test_error_aggregation() {
   struct src {
      std::uint32_t off;
      std::uint32_t bit;
   };
   const src sources[] = {
      {FX1_ISP_LSC_ERROR_OFFSET, FX1_ISP_LSC_ERROR_ACTIVE_LOAD_REJECT_MASK},
      {FX1_ISP_LSC_ERROR_OFFSET, FX1_ISP_LSC_ERROR_LOAD_PROTOCOL_MASK},
      {FX1_ISP_BPC_STATUS_OFFSET, FX1_ISP_BPC_STATUS_CAND_REJECT_OVF_MASK},
      {FX1_ISP_EE_STATUS_OFFSET, FX1_ISP_EE_STATUS_ERROR_MASK},
      {FX1_ISP_DMA_ERR_OFFSET, FX1_ISP_DMA_ERR_ERR_ALIGN_OR_GEOMETRY_BIT},
      {FX1_ISP_DMA_ERR_OFFSET, FX1_ISP_DMA_ERR_ERR_IDMA_UNDERRUN_BIT},
      {FX1_ISP_DMA_ERR_OFFSET, FX1_ISP_DMA_ERR_ERR_ODMA_OVERFLOW_BIT},
      {FX1_ISP_DMA_ERR_OFFSET, FX1_ISP_DMA_ERR_ERR_IDMA_AXI_BIT},
      {FX1_ISP_DMA_ERR_OFFSET, FX1_ISP_DMA_ERR_ERR_ODMA_AXI_BIT},
   };
   const std::uint32_t err_irq = FX1_ISP_COMMON_IRQ_STATUS_ERROR_IRQ_MASK;
   const std::uint32_t err_st = FX1_ISP_COMMON_STATUS_ERROR_MASK;
   for (const src &s : sources) {
      control_unit cu;
      const std::string ctx = fx1_test::hex(s.off) + "/" + fx1_test::hex(s.bit);
      cu.hw_set(s.off, s.bit, 10);
      FX1_CHECK_EQ_CTX(rd(cu, FX1_ISP_COMMON_IRQ_STATUS_OFFSET) & err_irq, err_irq, ctx);
      FX1_CHECK_EQ_CTX(rd(cu, FX1_ISP_COMMON_STATUS_OFFSET) & err_st, err_st, ctx);
      wr(cu, s.off, s.bit, 11);
      FX1_CHECK_EQ_CTX(rd(cu, FX1_ISP_COMMON_STATUS_OFFSET) & err_st, 0, ctx + " status follows flags");
      FX1_CHECK_EQ_CTX(rd(cu, FX1_ISP_COMMON_IRQ_STATUS_OFFSET) & err_irq, err_irq, ctx + " irq sticky");
   }
   // Underrun is level held: error_irq on the rising edge only (CSR-06).
   control_unit cu;
   const std::uint32_t u = FX1_ISP_DMA_ERR_ERR_IDMA_UNDERRUN_BIT;
   cu.hw_hold(FX1_ISP_DMA_ERR_OFFSET, u, true, 1);
   wr(cu, FX1_ISP_COMMON_IRQ_STATUS_OFFSET, err_irq, 2);
   cu.hw_hold(FX1_ISP_DMA_ERR_OFFSET, u, true, 3);  // condition persists
   FX1_CHECK_EQ(rd(cu, FX1_ISP_COMMON_IRQ_STATUS_OFFSET) & err_irq, 0);
   FX1_CHECK_EQ(rd(cu, FX1_ISP_COMMON_STATUS_OFFSET) & err_st, err_st);
   // Non-error status (a W1C statistics done flag) does not raise error_irq.
   control_unit cu2;
   cu2.hw_set(FX1_ISP_AF_STATUS_OFFSET, FX1_ISP_AF_STATUS_FRAME_DONE_MASK, 1);
   FX1_CHECK_EQ(rd(cu2, FX1_ISP_COMMON_IRQ_STATUS_OFFSET) & err_irq, 0);
}

void test_stats_ready() {
   const std::uint32_t sources[][2] = {
      {FX1_ISP_AEC_STATUS_OFFSET, FX1_ISP_AEC_STATUS_STAT_DONE_MASK},
      {FX1_ISP_AWB_STATUS_OFFSET, FX1_ISP_AWB_STATUS_STAT_DONE_MASK},
      {FX1_ISP_AF_STATUS_OFFSET, FX1_ISP_AF_STATUS_FRAME_DONE_MASK},
   };
   for (const auto &s : sources) {
      control_unit cu;
      cu.hw_set(s[0], s[1], 1);
      FX1_CHECK_EQ(rd(cu, FX1_ISP_COMMON_IRQ_STATUS_OFFSET), FX1_ISP_COMMON_IRQ_STATUS_STATS_READY_IRQ_MASK);
   }
}

void test_soft_reset(std::uint32_t reg, std::uint32_t bit) {
   control_unit cu;
   const std::string ctx = reg == FX1_ISP_COMMON_CTRL_OFFSET ? "COMMON soft_rst" : "DMA soft_reset";
   // Configuration.
   wr(cu, FX1_ISP_COMMON_SCRATCH_OFFSET, 0x55AA55AAu);
   wr(cu, FX1_ISP_BLC_OFS_G0_DFT_OFFSET, 0x40);
   wr(cu, FX1_ISP_DMA_CTRL_OFFSET, 0x3F03);  // IDMA_EN | ODMA_EN
   wr(cu, FX1_ISP_COMMON_IRQ_EN_OFFSET, 0x7);
   // Ownership, commands, status, results.
   wr(cu, FX1_ISP_IDMA_BUF_VALID_OFFSET, 0xF);
   wr(cu, FX1_ISP_ODMA_BUF_FREE_OFFSET, 0xF);
   wr(cu, FX1_ISP_COMMON_CTRL_OFFSET, FX1_ISP_COMMON_CTRL_FRAME_START_MASK | FX1_ISP_COMMON_CTRL_ISP_EN_MASK);
   wr(cu, FX1_ISP_AF_CTRL_OFFSET, FX1_ISP_AF_CTRL_SEARCH_START_MASK);
   cu.hw_set(FX1_ISP_ODMA_BUF_DONE_OFFSET, 0x3, 2);
   cu.hw_set(FX1_ISP_DMA_IRQ_STAT_OFFSET, 0x7E, 2);
   cu.hw_hold(FX1_ISP_DMA_ERR_OFFSET, 0x1F, true, 2);
   cu.hw_set(FX1_ISP_COMMON_STATUS_OFFSET, FX1_ISP_COMMON_STATUS_FRAME_DONE_MASK, 2);
   cu.hw_set(FX1_ISP_AEC_STATUS_OFFSET, FX1_ISP_AEC_STATUS_STAT_DONE_MASK, 2);
   cu.hw_write(FX1_ISP_AEC_GLOBAL_SUM_LO_OFFSET, 0xFFFFFFFFu, 0x1234u);
   cu.hw_write(FX1_ISP_IDMA_FRAME_COUNT_OFFSET, 0xFFFFFFFFu, 7);
   cu.hw_write(FX1_ISP_ODMA_FRAME_COUNT_OFFSET, 0xFFFFFFFFu, 6);
   cu.hw_write(FX1_ISP_AEC_FRAME_ID_OFFSET, 0xFFFFFFFFu, 5);
   wr(cu, FX1_ISP_GAMMA_LUT_ADDR_OFFSET, 9);
   wr(cu, FX1_ISP_GAMMA_LUT_DATA_OFFSET, 0xABC);
   // error_irq from DMA_ERR, stats_ready_irq from AEC stat_done.
   FX1_CHECK_EQ_CTX(rd(cu, FX1_ISP_COMMON_IRQ_STATUS_OFFSET), 0x6, ctx + " setup");

   wr(cu, reg, bit | (reg == FX1_ISP_DMA_CTRL_OFFSET ? 0x3F03u : FX1_ISP_COMMON_CTRL_ISP_EN_MASK), 100);

   // Preserved (DEC-14).
   FX1_CHECK_EQ_CTX(rd(cu, FX1_ISP_COMMON_SCRATCH_OFFSET), 0x55AA55AA, ctx);
   FX1_CHECK_EQ_CTX(rd(cu, FX1_ISP_BLC_OFS_G0_DFT_OFFSET), 0x40, ctx);
   FX1_CHECK_EQ_CTX(rd(cu, FX1_ISP_DMA_CTRL_OFFSET), 0x3F03, ctx + " enables kept, soft_reset reads 0");
   FX1_CHECK_EQ_CTX(rd(cu, FX1_ISP_COMMON_CTRL_OFFSET), FX1_ISP_COMMON_CTRL_ISP_EN_MASK, ctx);
   FX1_CHECK_EQ_CTX(rd(cu, FX1_ISP_COMMON_IRQ_EN_OFFSET), 0x7, ctx);
   FX1_CHECK_EQ_CTX(rd(cu, FX1_ISP_IDMA_FRAME_COUNT_OFFSET), 7, ctx);
   FX1_CHECK_EQ_CTX(rd(cu, FX1_ISP_ODMA_FRAME_COUNT_OFFSET), 6, ctx);
   FX1_CHECK_EQ_CTX(rd(cu, FX1_ISP_AEC_FRAME_ID_OFFSET), 5, ctx);
   // LSC validity across soft reset: test_lsc_loader (derived from the load FSM).
   FX1_CHECK_EQ_CTX(rd(cu, FX1_ISP_COMMON_VER_ID_OFFSET), 0x00010002, ctx);
   FX1_CHECK_EQ_CTX(cu.gamma_lut()[9], 0xABC, ctx);
   // Cleared.
   FX1_CHECK_EQ_CTX(rd(cu, FX1_ISP_IDMA_BUF_VALID_OFFSET), 0, ctx);
   FX1_CHECK_EQ_CTX(rd(cu, FX1_ISP_ODMA_BUF_FREE_OFFSET), 0, ctx);
   FX1_CHECK_EQ_CTX(rd(cu, FX1_ISP_ODMA_BUF_DONE_OFFSET), 0, ctx);
   FX1_CHECK_EQ_CTX(rd(cu, FX1_ISP_DMA_IRQ_STAT_OFFSET), 0, ctx);
   FX1_CHECK_EQ_CTX(rd(cu, FX1_ISP_DMA_ERR_OFFSET), 0, ctx + " holds released");
   FX1_CHECK_EQ_CTX(rd(cu, FX1_ISP_COMMON_IRQ_STATUS_OFFSET), 0, ctx);
   FX1_CHECK_EQ_CTX(rd(cu, FX1_ISP_COMMON_STATUS_OFFSET), 0, ctx);
   FX1_CHECK_EQ_CTX(rd(cu, FX1_ISP_AEC_STATUS_OFFSET), 0, ctx);
   FX1_CHECK_EQ_CTX(rd(cu, FX1_ISP_AEC_GLOBAL_SUM_LO_OFFSET), 0, ctx);
   FX1_CHECK_EQ_CTX(rd(cu, FX1_ISP_AF_CTRL_OFFSET), 0, ctx + " command bits");
   FX1_CHECK(!cu.irq_level());
   // 32-cycle window: W1S/W1C writes are held off, RW writes are accepted.
   FX1_CHECK(cu.soft_reset_active(100));
   FX1_CHECK(cu.soft_reset_active(131));
   FX1_CHECK(!cu.soft_reset_active(132));
   wr(cu, FX1_ISP_IDMA_BUF_VALID_OFFSET, 0x1, 131);
   wr(cu, FX1_ISP_COMMON_SCRATCH_OFFSET, 0x1, 131);
   FX1_CHECK_EQ_CTX(rd(cu, FX1_ISP_IDMA_BUF_VALID_OFFSET), 0, ctx + " W1S held off in window");
   FX1_CHECK_EQ_CTX(rd(cu, FX1_ISP_COMMON_SCRATCH_OFFSET), 1, ctx + " RW accepted in window");
   wr(cu, FX1_ISP_IDMA_BUF_VALID_OFFSET, 0x1, 132);
   FX1_CHECK_EQ_CTX(rd(cu, FX1_ISP_IDMA_BUF_VALID_OFFSET), 1, ctx + " W1S after window");
}

void test_frame_start_and_done() {
   control_unit cu;
   const std::uint32_t fs = FX1_ISP_COMMON_CTRL_FRAME_START_MASK;
   wr(cu, FX1_ISP_COMMON_CTRL_OFFSET, fs);
   FX1_CHECK_EQ(rd(cu, FX1_ISP_COMMON_CTRL_OFFSET), fs);  // DEC-16: pending
   wr(cu, FX1_ISP_COMMON_CTRL_OFFSET, 0);                 // writing 0 does not clear W1S
   FX1_CHECK_EQ(rd(cu, FX1_ISP_COMMON_CTRL_OFFSET), fs);
   cu.hw_set(FX1_ISP_COMMON_STATUS_OFFSET, FX1_ISP_COMMON_STATUS_FRAME_DONE_MASK, 5);
   FX1_CHECK_EQ(rd(cu, FX1_ISP_COMMON_STATUS_OFFSET), FX1_ISP_COMMON_STATUS_FRAME_DONE_MASK);
   cu.accepted_sof(6);
   FX1_CHECK_EQ(rd(cu, FX1_ISP_COMMON_CTRL_OFFSET), 0);
   FX1_CHECK_EQ(rd(cu, FX1_ISP_COMMON_STATUS_OFFSET), 0);  // DEC-17
   // CSR-07: a W1S field with no hardware stays set.
   wr(cu, FX1_ISP_AF_CTRL_OFFSET, FX1_ISP_AF_CTRL_SEARCH_ABORT_MASK);
   cu.accepted_sof(7);
   FX1_CHECK_EQ(rd(cu, FX1_ISP_AF_CTRL_OFFSET), FX1_ISP_AF_CTRL_SEARCH_ABORT_MASK);
}

void test_commands_recorded() {
   control_unit cu;
   wr(cu, FX1_ISP_IDMA_BUF_VALID_OFFSET, 0x5, 3);
   FX1_CHECK_EQ(rd(cu, FX1_ISP_IDMA_BUF_VALID_OFFSET), 0x5);  // until the IDMA consumes it (M2)
   FX1_CHECK_EQ(cu.commands().size(), 1);
   FX1_CHECK_EQ(cu.commands().back().effect.w1s_set, 0x5);
   FX1_CHECK_EQ(cu.commands().back().cycle, 3);
   // An installed block hook receives the command instead.
   int hooked = 0;
   cu.set_block_hook(FX1_ISP_ODMA_BUF_FREE_OFFSET,
                     [&](const register_file::write_effect &fx, std::uint64_t) { hooked += fx.w1s_set != 0; });
   wr(cu, FX1_ISP_ODMA_BUF_FREE_OFFSET, 0x2, 4);
   FX1_CHECK_EQ(hooked, 1);
   FX1_CHECK_EQ(cu.commands().size(), 1);
}

// Review finding: pending commands must not survive a reset.
void test_commands_cleared_on_reset() {
   control_unit cu;
   wr(cu, FX1_ISP_IDMA_BUF_VALID_OFFSET, 0x1, 1);
   FX1_CHECK_EQ(cu.commands().size(), 1);
   cu.external_reset();
   FX1_CHECK_EQ(cu.commands().size(), 0);

   const std::uint32_t soft_resets[][2] = {
      {FX1_ISP_COMMON_CTRL_OFFSET, FX1_ISP_COMMON_CTRL_SOFT_RST_MASK},
      {FX1_ISP_DMA_CTRL_OFFSET, FX1_ISP_DMA_CTRL_SOFT_RESET_MASK | FX1_ISP_DMA_CTRL_IDMA_EN_MASK},
   };
   std::uint64_t cycle = 10;
   for (const auto &sr : soft_resets) {
      wr(cu, FX1_ISP_ODMA_BUF_FREE_OFFSET, 0x3, cycle);
      FX1_CHECK_EQ(cu.commands().size(), 1);
      wr(cu, sr[0], sr[1], cycle + 1);
      FX1_CHECK_EQ_CTX(cu.commands().size(), 0, "after soft reset via " + fx1_test::hex(sr[0]));
      cycle += 100;
   }
}

void test_gamma_lut_port() {
   control_unit cu;
   wr(cu, FX1_ISP_GAMMA_LUT_ADDR_OFFSET, 0);
   for (std::uint32_t i = 0; i < 256; ++i) {
      wr(cu, FX1_ISP_GAMMA_LUT_DATA_OFFSET, (i * 16u + 3u) & 0xFFFu);
   }
   FX1_CHECK_EQ(rd(cu, FX1_ISP_GAMMA_LUT_ADDR_OFFSET), 0);  // wrapped after 256 writes
   FX1_CHECK_EQ(rd(cu, FX1_ISP_GAMMA_LUT_DATA_OFFSET), (255u * 16u + 3u) & 0xFFFu);  // shadow
   bool all = true;
   for (std::uint32_t i = 0; i < 256; ++i) {
      wr(cu, FX1_ISP_GAMMA_LUT_ADDR_OFFSET, i);
      all = all && rd(cu, FX1_ISP_GAMMA_LUT_RDATA_OFFSET) == ((i * 16u + 3u) & 0xFFFu);
   }
   FX1_CHECK(all);
   // Partial strobe merges into the data register, then writes the entry.
   wr(cu, FX1_ISP_GAMMA_LUT_ADDR_OFFSET, 5);
   wr(cu, FX1_ISP_GAMMA_LUT_DATA_OFFSET, 0x0FF0);
   wr(cu, FX1_ISP_GAMMA_LUT_ADDR_OFFSET, 5);
   wr(cu, FX1_ISP_GAMMA_LUT_DATA_OFFSET, 0xAB, 1, 0x000000FFu);
   FX1_CHECK_EQ(cu.gamma_lut()[5], 0xFAB);
   FX1_CHECK_EQ(rd(cu, FX1_ISP_GAMMA_LUT_ADDR_OFFSET), 6);
   wr(cu, FX1_ISP_GAMMA_LUT_DATA_OFFSET, 0x123, 1, 0);  // no strobes: no write, no increment
   FX1_CHECK_EQ(rd(cu, FX1_ISP_GAMMA_LUT_ADDR_OFFSET), 6);
   FX1_CHECK_EQ(cu.gamma_lut()[6], (6u * 16u + 3u) & 0xFFFu);
}

void test_ee_lut_port() {
   control_unit cu;
   const std::uint32_t sel_shift = FX1_ISP_EE_LUT_CTRL_LUT_SEL_SHIFT;
   FX1_CHECK_EQ(rd(cu, FX1_ISP_EE_LUT_RDATA_OFFSET), 0x8000);  // CSR-16
   wr(cu, FX1_ISP_EE_LUT_CTRL_OFFSET, 2u << sel_shift);        // CONTRAST_POS, 32 entries
   wr(cu, FX1_ISP_EE_LUT_ADDR_OFFSET, 33);                     // top bit ignored -> entry 1
   wr(cu, FX1_ISP_EE_LUT_WDATA_OFFSET, 0x1234);
   FX1_CHECK_EQ(rd(cu, FX1_ISP_EE_LUT_ADDR_OFFSET), 34);
   FX1_CHECK_EQ(cu.ee_table(2)[1], 0x1234);
   wr(cu, FX1_ISP_EE_LUT_ADDR_OFFSET, 1);
   FX1_CHECK_EQ(rd(cu, FX1_ISP_EE_LUT_RDATA_OFFSET), 0x1234);
   wr(cu, FX1_ISP_EE_LUT_CTRL_OFFSET, 0);  // LUMA, 64 entries
   wr(cu, FX1_ISP_EE_LUT_ADDR_OFFSET, 33);
   FX1_CHECK_EQ(rd(cu, FX1_ISP_EE_LUT_RDATA_OFFSET), 0x8000);
   wr(cu, FX1_ISP_EE_LUT_ADDR_OFFSET, 63);
   wr(cu, FX1_ISP_EE_LUT_WDATA_OFFSET, 0x7);
   FX1_CHECK_EQ(rd(cu, FX1_ISP_EE_LUT_ADDR_OFFSET), 0);  // wraps at 64
   FX1_CHECK_EQ(cu.ee_table(0)[63], 0x7);
   FX1_CHECK_EQ(cu.ee_table(1)[63], 0x8000);
   // GTM read-back port: zero at reset, out-of-table index reads zero.
   wr(cu, FX1_ISP_GTM_LUT_ADDR_OFFSET, 70);
   FX1_CHECK_EQ(rd(cu, FX1_ISP_GTM_LUT_RDATA_OFFSET), 0);
}

void test_resizer_geometry() {
   control_unit cu;
   auto set_geom = [&](std::uint32_t w, std::uint32_t h, std::uint32_t bayer) {
      wr(cu, FX1_ISP_COMMON_FRAME_WIDTH_OFFSET, w);
      wr(cu, FX1_ISP_COMMON_FRAME_HEIGHT_OFFSET, h);
      wr(cu, FX1_ISP_COMMON_BAYER_OFFSET, bayer);
   };
   auto out = [&]() {
      return std::pair<std::uint32_t, std::uint32_t>(rd(cu, FX1_ISP_RESIZER_OUT_W_OFFSET),
                                                     rd(cu, FX1_ISP_RESIZER_OUT_H_OFFSET));
   };
   auto resizer = [&](bool en, std::uint32_t scale) {
      wr(cu, FX1_ISP_RESIZER_CTRL_OFFSET, (en ? 1u : 0u) | (scale << FX1_ISP_RESIZER_CTRL_SCALE_SHIFT) |
                                              FX1_ISP_RESIZER_CTRL_UPDATED_MASK);  // DEC-24 commit
   };
   set_geom(2688, 1520, FX1_ISP_COMMON_BAYER_PATTERN_BGGR);  // DEC-04 input
   FX1_CHECK_EQ(out().first, 2686);
   FX1_CHECK_EQ(out().second, 1518);
   resizer(true, FX1_ISP_RESIZER_CTRL_SCALE_RES_1920X1080);
   FX1_CHECK_EQ(out().first, 1920);
   FX1_CHECK_EQ(out().second, 1080);
   resizer(true, FX1_ISP_RESIZER_CTRL_SCALE_RES_3840X2160);  // larger than input: FullRes
   FX1_CHECK_EQ(out().first, 2686);
   resizer(true, 0xF);  // reserved: FullRes
   FX1_CHECK_EQ(out().second, 1518);
   resizer(false, FX1_ISP_RESIZER_CTRL_SCALE_RES_640X480);  // disabled: pass-through
   FX1_CHECK_EQ(out().first, 2686);
   resizer(true, FX1_ISP_RESIZER_CTRL_SCALE_RES_640X480);
   FX1_CHECK_EQ(out().first, 640);
   FX1_CHECK_EQ(out().second, 480);
   resizer(false, 0);
   set_geom(2688, 1520, FX1_ISP_COMMON_BAYER_PATTERN_GRBG);  // H shift only
   FX1_CHECK_EQ(out().first, 2686);
   FX1_CHECK_EQ(out().second, 1520);
   set_geom(2688, 1520, FX1_ISP_COMMON_BAYER_PATTERN_GBRG);  // V shift only
   FX1_CHECK_EQ(out().first, 2688);
   FX1_CHECK_EQ(out().second, 1518);
   set_geom(2689, 1521, FX1_ISP_COMMON_BAYER_PATTERN_RGGB);  // odd input rounds down
   FX1_CHECK_EQ(out().first, 2688);
   FX1_CHECK_EQ(out().second, 1520);
}

void lsc_load(control_unit &cu, unsigned dest, std::uint32_t count, std::uint32_t value, std::uint64_t &cyc) {
   wr(cu, FX1_ISP_LSC_LOAD_CTRL_OFFSET, dest | FX1_ISP_LSC_LOAD_CTRL_LOAD_BEGIN_MASK, cyc++);
   for (std::uint32_t i = 0; i < count; ++i) {
      wr(cu, FX1_ISP_LSC_COEF_DATA_OFFSET, value + i, cyc++);
   }
   wr(cu, FX1_ISP_LSC_LOAD_CTRL_OFFSET, dest | FX1_ISP_LSC_LOAD_CTRL_LOAD_VALIDATE_MASK, cyc++);
}

// LSC load sequencer (HAS §6.8.8, ALG-LSC-04..11).
// Growing the mesh during a load must never validate an undersized bank.
void test_lsc_resize_during_load() {
   control_unit cu;
   std::uint64_t cyc = 1;
   wr(cu, FX1_ISP_LSC_MESH_NODES_OFFSET, 0x0202, cyc++);
   wr(cu, FX1_ISP_LSC_LOAD_CTRL_OFFSET, FX1_ISP_LSC_LOAD_CTRL_LOAD_BEGIN_MASK, cyc++);
   wr(cu, FX1_ISP_LSC_MESH_NODES_OFFSET, 0x0303, cyc++);
   for (unsigned i = 0; i < 36; ++i) {
      wr(cu, FX1_ISP_LSC_COEF_DATA_OFFSET, 0x40000, cyc++);
   }
   wr(cu, FX1_ISP_LSC_LOAD_CTRL_OFFSET, FX1_ISP_LSC_LOAD_CTRL_LOAD_VALIDATE_MASK, cyc++);
   FX1_CHECK(!cu.lsc_profile_valid(0));
   FX1_CHECK(rd(cu, FX1_ISP_LSC_ERROR_OFFSET) & FX1_ISP_LSC_ERROR_COEF_COUNT_MASK);
   // A fresh load in the new geometry recovers normally.
   lsc_load(cu, 0, 36, 0x40000, cyc);
   FX1_CHECK(cu.lsc_profile_valid(0));
   FX1_CHECK_EQ(cu.lsc_profile(0).size(), 36);
}

void test_lsc_loader() {
   control_unit cu;
   std::uint64_t cyc = 1;
   const std::uint32_t st = FX1_ISP_LSC_PROFILE_STATUS_OFFSET, err = FX1_ISP_LSC_ERROR_OFFSET;
   FX1_CHECK_EQ(rd(cu, FX1_ISP_LSC_LOAD_STATUS_OFFSET), 0);
   // Invalid geometry at reset: begin refused.
   wr(cu, FX1_ISP_LSC_LOAD_CTRL_OFFSET, FX1_ISP_LSC_LOAD_CTRL_LOAD_BEGIN_MASK, cyc++);
   FX1_CHECK_EQ(rd(cu, err), FX1_ISP_LSC_ERROR_GEOMETRY_INVALID_MASK);
   FX1_CHECK_EQ(rd(cu, FX1_ISP_LSC_LOAD_CTRL_OFFSET) & 0x700u, 0);  // commands self-clear
   wr(cu, err, 0x3F, cyc++);
   // 33 nodes exceed PARA_LSC_MESH_MAX = 32 (ALG-LSC-11).
   wr(cu, FX1_ISP_LSC_MESH_NODES_OFFSET, 33u | (2u << 8), cyc++);
   FX1_CHECK_EQ(rd(cu, st) & FX1_ISP_LSC_PROFILE_STATUS_GEOMETRY_VALID_MASK, 0);
   wr(cu, FX1_ISP_LSC_MESH_NODES_OFFSET, 3u | (2u << 8), cyc++);  // 3 x 2 nodes -> 24 coefficients
   FX1_CHECK_EQ(rd(cu, st), FX1_ISP_LSC_PROFILE_STATUS_GEOMETRY_VALID_MASK);
   // Profile 0 can be loaded first although it is the active index (ALG-LSC-05).
   lsc_load(cu, 0, 24, 0x40000, cyc);
   FX1_CHECK_EQ(rd(cu, err), 0);
   FX1_CHECK_EQ(rd(cu, st) & 0x47u, 0x41u);  // valid[0], active_valid
   FX1_CHECK_EQ(cu.lsc_profile(0).size(), 24);
   FX1_CHECK_EQ(cu.lsc_profile(0)[23], 0x40000 + 23);
   FX1_CHECK_EQ(rd(cu, FX1_ISP_LSC_LOAD_STATUS_OFFSET), 24u | FX1_ISP_LSC_LOAD_STATUS_LOAD_FULL_MASK);
   // Now profile 0 is active and valid: loading it is refused.
   wr(cu, FX1_ISP_LSC_LOAD_CTRL_OFFSET, FX1_ISP_LSC_LOAD_CTRL_LOAD_BEGIN_MASK, cyc++);
   FX1_CHECK_EQ(rd(cu, err), FX1_ISP_LSC_ERROR_ACTIVE_LOAD_REJECT_MASK);
   wr(cu, err, 0x3F, cyc++);
   // A short load fails at validate and keeps the previous contents.
   lsc_load(cu, 1, 24, 0x10, cyc);
   lsc_load(cu, 1, 23, 0x99, cyc);
   FX1_CHECK_EQ(rd(cu, err), FX1_ISP_LSC_ERROR_COEF_COUNT_MASK);
   FX1_CHECK_EQ(cu.lsc_profile(1)[0], 0x10);
   FX1_CHECK(cu.lsc_profile_valid(1));
   wr(cu, err, 0x3F, cyc++);
   // An extra write also fails; abort keeps the old data.
   lsc_load(cu, 1, 25, 0x77, cyc);
   FX1_CHECK_EQ(rd(cu, err), FX1_ISP_LSC_ERROR_COEF_COUNT_MASK);
   wr(cu, err, 0x3F, cyc++);
   wr(cu, FX1_ISP_LSC_LOAD_CTRL_OFFSET, 2u | FX1_ISP_LSC_LOAD_CTRL_LOAD_BEGIN_MASK, cyc++);
   wr(cu, FX1_ISP_LSC_COEF_DATA_OFFSET, 5, cyc++);
   FX1_CHECK_EQ(rd(cu, FX1_ISP_LSC_LOAD_STATUS_OFFSET),
                1u | FX1_ISP_LSC_LOAD_STATUS_LOAD_BUSY_MASK | (2u << FX1_ISP_LSC_LOAD_STATUS_LOAD_PROFILE_SHIFT));
   wr(cu, FX1_ISP_LSC_LOAD_CTRL_OFFSET, 2u | FX1_ISP_LSC_LOAD_CTRL_LOAD_ABORT_MASK |
                                             FX1_ISP_LSC_LOAD_CTRL_LOAD_VALIDATE_MASK, cyc++);  // abort wins
   FX1_CHECK(!cu.lsc_profile_valid(2));
   FX1_CHECK_EQ(rd(cu, err), 0);
   // Protocol violations: data without a load, validate without begin.
   wr(cu, FX1_ISP_LSC_COEF_DATA_OFFSET, 1, cyc++);
   FX1_CHECK_EQ(rd(cu, err), FX1_ISP_LSC_ERROR_LOAD_PROTOCOL_MASK);
   wr(cu, err, 0x3F, cyc++);
   wr(cu, FX1_ISP_LSC_LOAD_CTRL_OFFSET, FX1_ISP_LSC_LOAD_CTRL_LOAD_VALIDATE_MASK, cyc++);
   FX1_CHECK_EQ(rd(cu, err), FX1_ISP_LSC_ERROR_LOAD_PROTOCOL_MASK);
   wr(cu, err, 0x3F, cyc++);
   // Coefficient above 4.0 flags coef_range but is stored (ALG-LSC-03).
   lsc_load(cu, 2, 24, 0x100001, cyc);
   FX1_CHECK_EQ(rd(cu, err), FX1_ISP_LSC_ERROR_COEF_RANGE_MASK);
   FX1_CHECK(cu.lsc_profile_valid(2));
   wr(cu, err, 0x3F, cyc++);
   // Selection: unloaded or code 3 refused (value kept), a valid one adopted at SOF.
   wr(cu, FX1_ISP_LSC_PROFILE_SEL_OFFSET, 3, cyc++);
   FX1_CHECK_EQ(rd(cu, err), FX1_ISP_LSC_ERROR_PROFILE_SEL_REJECT_MASK);
   FX1_CHECK_EQ(rd(cu, FX1_ISP_LSC_PROFILE_SEL_OFFSET), 0);
   wr(cu, err, 0x3F, cyc++);
   wr(cu, FX1_ISP_LSC_PROFILE_SEL_OFFSET, 1, cyc++);
   FX1_CHECK_EQ(rd(cu, err), 0);
   FX1_CHECK_EQ(cu.lsc_active_profile(), 0);
   cu.accepted_sof(cyc++);
   FX1_CHECK_EQ(cu.lsc_active_profile(), 1);
   FX1_CHECK_EQ(rd(cu, st) & FX1_ISP_LSC_PROFILE_STATUS_ACTIVE_PROFILE_MASK, 1u << 4);
   // Soft reset keeps meshes and validity (DEC-14) and drops a load in progress.
   wr(cu, FX1_ISP_LSC_LOAD_CTRL_OFFSET, FX1_ISP_LSC_LOAD_CTRL_LOAD_BEGIN_MASK, cyc++);
   wr(cu, FX1_ISP_COMMON_CTRL_OFFSET, FX1_ISP_COMMON_CTRL_SOFT_RST_MASK, cyc);
   cyc += 64;
   FX1_CHECK_EQ(rd(cu, FX1_ISP_LSC_LOAD_STATUS_OFFSET) & FX1_ISP_LSC_LOAD_STATUS_LOAD_BUSY_MASK, 0);
   FX1_CHECK(cu.lsc_profile_valid(0) && cu.lsc_profile_valid(1) && cu.lsc_profile_valid(2));
   // A geometry change invalidates every profile (p62).
   wr(cu, FX1_ISP_LSC_MESH_NODES_OFFSET, 4u | (2u << 8), cyc++);
   FX1_CHECK_EQ(rd(cu, st) & 0x7u, 0);
   // External reset clears validity.
   lsc_load(cu, 2, 32, 1, cyc);
   FX1_CHECK(cu.lsc_profile_valid(2));
   cu.external_reset();
   FX1_CHECK(!cu.lsc_profile_valid(2));
}

// DEC-24/26/27: gated configuration sets.
void test_commit_gates() {
   const std::uint32_t upd = FX1_ISP_RESIZER_CTRL_UPDATED_MASK;
   const std::uint32_t fhd = FX1_ISP_RESIZER_CTRL_SCALE_RES_1920X1080 << FX1_ISP_RESIZER_CTRL_SCALE_SHIFT;
   const std::uint32_t hd = FX1_ISP_RESIZER_CTRL_SCALE_RES_1280X720 << FX1_ISP_RESIZER_CTRL_SCALE_SHIFT;
   control_unit cu;
   wr(cu, FX1_ISP_COMMON_FRAME_WIDTH_OFFSET, 3840);
   wr(cu, FX1_ISP_COMMON_FRAME_HEIGHT_OFFSET, 2160);
   // Without `updated` a new scale is not committed; the enable is live.
   wr(cu, FX1_ISP_RESIZER_CTRL_OFFSET, 1u | fhd);
   FX1_CHECK_EQ(rd(cu, FX1_ISP_RESIZER_OUT_W_OFFSET), 3840);
   // With `updated` and an idle pipeline: committed at once.
   wr(cu, FX1_ISP_RESIZER_CTRL_OFFSET, 1u | fhd | upd);
   FX1_CHECK_EQ(rd(cu, FX1_ISP_RESIZER_OUT_W_OFFSET), 1920);
   // A frame in flight defers the commit to the next accepted SOF.
   cu.hw_set(FX1_ISP_COMMON_STATUS_OFFSET, FX1_ISP_COMMON_STATUS_BUSY_MASK, 5);
   wr(cu, FX1_ISP_RESIZER_CTRL_OFFSET, 1u | hd | upd, 6);
   FX1_CHECK_EQ(rd(cu, FX1_ISP_RESIZER_OUT_W_OFFSET), 1920);
   cu.accepted_sof(7);
   FX1_CHECK_EQ(rd(cu, FX1_ISP_RESIZER_OUT_W_OFFSET), 1280);
   // Clearing `updated` freezes the committed scale; disabling still bypasses.
   cu.hw_clear(FX1_ISP_COMMON_STATUS_OFFSET, FX1_ISP_COMMON_STATUS_BUSY_MASK);
   wr(cu, FX1_ISP_RESIZER_CTRL_OFFSET, 1u | fhd, 8);
   cu.accepted_sof(9);
   FX1_CHECK_EQ(rd(cu, FX1_ISP_RESIZER_OUT_W_OFFSET), 1280);
   wr(cu, FX1_ISP_RESIZER_CTRL_OFFSET, fhd, 10);
   FX1_CHECK_EQ(rd(cu, FX1_ISP_RESIZER_OUT_W_OFFSET), 3840);
   // CCM: coefficients follow the same rule.
   wr(cu, FX1_ISP_CCM_CRR_OFFSET, 0x100, 11);
   FX1_CHECK_EQ(cu.committed(FX1_ISP_CCM_CRR_OFFSET), 0x200);
   wr(cu, FX1_ISP_CCM_CTRL_OFFSET, FX1_ISP_CCM_CTRL_EN_MASK | FX1_ISP_CCM_CTRL_UPDATED_MASK, 12);
   FX1_CHECK_EQ(cu.committed(FX1_ISP_CCM_CRR_OFFSET), 0x100);
   wr(cu, FX1_ISP_CCM_OFS_B_OFFSET, 0x7, 13);  // updated still set: follows the writes
   FX1_CHECK_EQ(cu.committed(FX1_ISP_CCM_OFS_B_OFFSET), 0x7);
   // CNF thresholds (DEC-27).
   wr(cu, FX1_ISP_CNF_LUMA_TH_OFFSET, 20, 14);
   FX1_CHECK_EQ(cu.committed(FX1_ISP_CNF_LUMA_TH_OFFSET), 0);
   wr(cu, FX1_ISP_CNF_CTRL_OFFSET, FX1_ISP_CNF_CTRL_UPDATED_MASK, 15);
   FX1_CHECK_EQ(cu.committed(FX1_ISP_CNF_LUMA_TH_OFFSET), 20);
   // Soft reset preserves the committed sets (configuration, DEC-14).
   wr(cu, FX1_ISP_COMMON_CTRL_OFFSET, FX1_ISP_COMMON_CTRL_SOFT_RST_MASK, 16);
   FX1_CHECK_EQ(cu.committed(FX1_ISP_CCM_CRR_OFFSET), 0x100);
   // External reset returns them to the register reset values.
   cu.external_reset();
   FX1_CHECK_EQ(cu.committed(FX1_ISP_CCM_CRR_OFFSET), 0x200);
   FX1_CHECK_EQ(cu.committed(FX1_ISP_CNF_LUMA_TH_OFFSET), 0);
}

void test_external_reset() {
   control_unit cu;
   wr(cu, FX1_ISP_COMMON_SCRATCH_OFFSET, 0x1234);
   wr(cu, FX1_ISP_GAMMA_LUT_DATA_OFFSET, 0x777);
   wr(cu, FX1_ISP_EE_LUT_WDATA_OFFSET, 0x1);
   cu.hw_write(FX1_ISP_IDMA_FRAME_COUNT_OFFSET, 0xFFFFFFFFu, 9);
   cu.hw_hold(FX1_ISP_DMA_ERR_OFFSET, 0x2, true, 1);
   wr(cu, FX1_ISP_DMA_CTRL_OFFSET, FX1_ISP_DMA_CTRL_SOFT_RESET_MASK, 50);
   cu.external_reset();
   FX1_CHECK_EQ(rd(cu, FX1_ISP_COMMON_SCRATCH_OFFSET), 0);
   FX1_CHECK_EQ(rd(cu, FX1_ISP_IDMA_FRAME_COUNT_OFFSET), 0);
   FX1_CHECK_EQ(rd(cu, FX1_ISP_DMA_ERR_OFFSET), 0);
   FX1_CHECK_EQ(cu.regs().held(FX1_ISP_DMA_ERR_OFFSET), 0);
   FX1_CHECK_EQ(cu.gamma_lut()[0], 0);
   FX1_CHECK_EQ(cu.ee_table(0)[0], 0x8000);
   FX1_CHECK(!cu.soft_reset_active(51));
   wr(cu, FX1_ISP_IDMA_BUF_VALID_OFFSET, 0x2, 51);  // command block lifted by i_rst_n
   FX1_CHECK_EQ(rd(cu, FX1_ISP_IDMA_BUF_VALID_OFFSET), 0x2);
}

}  // namespace

int main() {
   test_table_shape();
   test_literal_resets();
   test_access_sweep();
   test_unmapped_and_reserved();
   test_strobes_and_alias();
   test_w1c_semantics();
   test_level_hold();
   test_irq_aggregation();
   test_error_aggregation();
   test_stats_ready();
   test_soft_reset(FX1_ISP_COMMON_CTRL_OFFSET, FX1_ISP_COMMON_CTRL_SOFT_RST_MASK);
   test_soft_reset(FX1_ISP_DMA_CTRL_OFFSET, FX1_ISP_DMA_CTRL_SOFT_RESET_MASK);
   test_frame_start_and_done();
   test_commands_recorded();
   test_commands_cleared_on_reset();
   test_gamma_lut_port();
   test_ee_lut_port();
   test_resizer_geometry();
   test_commit_gates();
   test_lsc_resize_during_load();
   test_lsc_loader();
   test_external_reset();
   return fx1_test::summary("fx1_isp_test_control");
}
