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
#include "pipeline/isp_pipeline.h"
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
          // Derived channel geometry: the reset readback is zero only until the
          // reset key publishes, one edge later (test_channel_geometry).
          off == FX1_ISP_RESIZER_OUT_W_OFFSET || off == FX1_ISP_RESIZER_OUT_H_OFFSET ||
          off == FX1_ISP_CH1_RESIZER_OUT_W_OFFSET || off == FX1_ISP_CH1_RESIZER_OUT_H_OFFSET ||
          off == FX1_ISP_CH2_RESIZER_OUT_W_OFFSET || off == FX1_ISP_CH2_RESIZER_OUT_H_OFFSET ||
          off == FX1_ISP_CH0_GEOMETRY_STATUS_OFFSET || off == FX1_ISP_CH1_GEOMETRY_STATUS_OFFSET ||
          off == FX1_ISP_CH2_GEOMETRY_STATUS_OFFSET || off == FX1_ISP_CH0_ACTIVE_CROP_ORIGIN_OFFSET ||
          off == FX1_ISP_CH1_ACTIVE_CROP_ORIGIN_OFFSET || off == FX1_ISP_CH2_ACTIVE_CROP_ORIGIN_OFFSET ||
          off == FX1_ISP_CH0_ACTIVE_CROP_SIZE_OFFSET || off == FX1_ISP_CH1_ACTIVE_CROP_SIZE_OFFSET ||
          off == FX1_ISP_CH2_ACTIVE_CROP_SIZE_OFFSET ||
          // Untouched AEC zones read min 4095 (MAS §6.21.4.3), not the CSR reset value 0.
          off == FX1_ISP_AEC_ZONE_GREEN_MIN_MAX_OFFSET ||
          off == FX1_ISP_FRAME_COUNTER_OFFSET ||  // core-clock count (test_time_base)
          off == FX1_ISP_TUNE_CAPS_OFFSET;  // implemented subset, V107-A1 (test_register_engine)
}

// Registers whose writes have block side effects (covered by dedicated tests).
bool triggers_soft_reset(std::uint32_t off) {
   return off == FX1_ISP_COMMON_CTRL_OFFSET || off == FX1_ISP_DMA_CTRL_OFFSET ||
          off == FX1_ISP_LSC_LOAD_CTRL_OFFSET || off == FX1_ISP_LSC_PROFILE_SEL_OFFSET ||
          off == FX1_ISP_AWB_RESULT_CTRL_OFFSET;  // one-clock commands, read 0 (test_stats)
}

void test_table_shape() {
   FX1_CHECK_EQ(csr::num_registers, 573);  // CSR revision v1.07
   std::size_t fields = 0;
   for (std::size_t i = 0; i < csr::num_registers; ++i) {
      fields += csr::registers[i].num_fields;
   }
   FX1_CHECK_EQ(fields, 819);
}

void test_literal_resets() {
   struct lit {
      std::uint32_t off;
      std::uint32_t value;
   };
   // 0x2014/0x2018 RESIZER_OUT_W/H: CSR v1.07 resets them to 0 (no published
   // set); the VP still publishes the 07/10 reset set 1920x1080 until the
   // Resizer milestone V107-7 (U4-A4 superseded there).
   const lit literal_resets[] = {
      {0x0000, 0x18102026}, {0x0004, 0x00010007}, {0x0008, 0x00000000}, {0x0018, 0x00000780},
      {0x001C, 0x00000438}, {0x0020, 0x00000000}, {0x0028, 0x00000000}, {0x0204, 0x00000100},
      {0x0208, 0x000000C8}, {0x021C, 0x000000C8}, {0x0258, 0x00010000}, {0x0264, 0x00010000},
      {0x0404, 0x00000000}, {0x0604, 0x00000001}, {0x0608, 0x00000100}, {0x060C, 0x004D0192},
      {0x0610, 0x000004B0}, {0x0804, 0x00000100}, {0x0808, 0x00000100}, {0x080C, 0x00000100},
      {0x0A04, 0x00000100}, {0x1004, 0x00000200}, {0x1008, 0x00000000}, {0x1014, 0x00000200},
      {0x1024, 0x00000200}, {0x1400, 0x00000000}, {0x160C, 0x0000002E}, {0x1610, 0x00000400},
      {0x1614, 0x0000000B}, {0x1C04, 0x00004000}, {0x1C08, 0x00002000}, {0x1C0C, 0x00001810},
      {0x1C24, 0x80008000}, {0x1C28, 0x80008000}, {0x2014, 0x00000780}, {0x2018, 0x00000438},
      {0x2208, 0x00001820}, {0x220C, 0x005A0078}, {0x2210, 0x0FFF0000}, {0x2214, 0x0EA00110},
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
// access type of every bit. W1SC and W1P bits read 0 whatever the write.
void test_access_sweep() {
   for (std::size_t i = 0; i < csr::num_registers; ++i) {
      const csr::reg_desc &d = csr::registers[i];
      if (has_read_hook(d.offset) || triggers_soft_reset(d.offset)) {
         continue;
      }
      control_unit cu;
      const std::string ctx = d.name;
      const std::uint32_t rst = d.reset & ~(d.w1sc_mask | d.w1p_mask);
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
   FX1_CHECK_EQ(unmapped, 16384 - 573);
   for (std::size_t i = 0; i < csr::num_registers; ++i) {
      FX1_CHECK_EQ_CTX(cu.regs().peek(csr::registers[i].offset), before[i],
                       std::string("unmapped writes changed ") + csr::registers[i].name);
   }
   // Reserved bits read zero after an all-ones write (BLC_GAIN_SEL: [1:0], [8]).
   wr(cu, FX1_ISP_BLC_GAIN_SEL_OFFSET, 0xFFFFFFFFu);
   FX1_CHECK_EQ(rd(cu, FX1_ISP_BLC_GAIN_SEL_OFFSET), 0x103);
   // Read-only word ignores writes.
   wr(cu, FX1_ISP_COMMON_VER_ID_OFFSET, 0);
   FX1_CHECK_EQ(rd(cu, FX1_ISP_COMMON_VER_ID_OFFSET), 0x00010007);
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
   // DMA group: seven sources, bit 0 is CFG_ERROR since the 07/10 map (SPEC-05 superseded).
   cu.hw_set(FX1_ISP_DMA_IRQ_STAT_OFFSET, 0x7F, 3);
   FX1_CHECK_EQ(rd(cu, FX1_ISP_DMA_IRQ_STAT_OFFSET), 0x7F);
   wr(cu, FX1_ISP_DMA_IRQ_EN_OFFSET, 0x7F, 3);
   FX1_CHECK_EQ(rd(cu, FX1_ISP_DMA_IRQ_EN_OFFSET), 0x7F);
   wr(cu, st, 1, 4);
   FX1_CHECK(cu.irq_level());  // DMA group still pending
   wr(cu, FX1_ISP_DMA_IRQ_STAT_OFFSET, 0x3E, 5);
   FX1_CHECK(cu.irq_level());  // bits 6 and 0 remain
   wr(cu, FX1_ISP_DMA_IRQ_STAT_OFFSET, 0x40, 6);
   FX1_CHECK(cu.irq_level());  // CFG_ERROR remains
   wr(cu, FX1_ISP_DMA_IRQ_STAT_OFFSET, FX1_ISP_DMA_IRQ_STAT_IRQ_CFG_ERROR_BIT, 6);
   FX1_CHECK(!cu.irq_level());
   // Unmasking a latched bit raises the line; masking it drops it.
   cu.hw_set(FX1_ISP_DMA_IRQ_STAT_OFFSET, FX1_ISP_DMA_IRQ_STAT_IRQ_ODMA_DONE_BIT, 7);
   wr(cu, FX1_ISP_DMA_IRQ_EN_OFFSET, 0, 8);
   FX1_CHECK(!cu.irq_level());
   wr(cu, FX1_ISP_DMA_IRQ_EN_OFFSET, FX1_ISP_DMA_IRQ_EN_IRQ_EN_ODMA_DONE_BIT, 9);
   FX1_CHECK(cu.irq_level());
}

// MAS v1.07 Table 9-188 (DEC-15 superseded): ERROR_IRQ when DMA_ERR.ALIGN,
// AXI_RD or AXI_WR, any LSC_ERROR bit, EE_STATUS.ERROR or any TUNE_ERROR bit
// rises; COMMON_STATUS.ERROR is their live OR. Flow control (underrun,
// overflow) and statistics drops (BPC candidate overflow) set neither.
void test_error_aggregation() {
   struct src {
      std::uint32_t off;
      std::uint32_t bit;
   };
   const src sources[] = {
      {FX1_ISP_LSC_ERROR_OFFSET, FX1_ISP_LSC_ERROR_ACTIVE_LOAD_REJECT_MASK},
      {FX1_ISP_LSC_ERROR_OFFSET, FX1_ISP_LSC_ERROR_LOAD_PROTOCOL_MASK},
      {FX1_ISP_EE_STATUS_OFFSET, FX1_ISP_EE_STATUS_ERROR_MASK},
      {FX1_ISP_DMA_ERR_OFFSET, FX1_ISP_DMA_ERR_ERR_ALIGN_BIT},
      {FX1_ISP_DMA_ERR_OFFSET, FX1_ISP_DMA_ERR_ERR_AXI_RD_BIT},
      {FX1_ISP_DMA_ERR_OFFSET, FX1_ISP_DMA_ERR_ERR_AXI_WR_BIT},
      {FX1_ISP_TUNE_ERROR_OFFSET, FX1_ISP_TUNE_ERROR_LOCKED_MASK},
      {FX1_ISP_TUNE_ERROR_OFFSET, FX1_ISP_TUNE_ERROR_MODE_MASK},
      {FX1_ISP_TUNE_ERROR_OFFSET, FX1_ISP_TUNE_ERROR_NO_LEASE_MASK},
      {FX1_ISP_TUNE_ERROR_OFFSET, FX1_ISP_TUNE_ERROR_OVERFLOW_MASK},
   };
   const std::uint32_t err_irq = FX1_ISP_COMMON_IRQ_STATUS_ERROR_IRQ_MASK;
   const std::uint32_t err_st = FX1_ISP_COMMON_STATUS_ERROR_MASK;
   for (const src &s : sources) {
      control_unit cu;
      const std::string ctx = fx1_test::hex(s.off) + "/" + fx1_test::hex(s.bit);
      cu.hw_set(s.off, s.bit, 10);
      FX1_CHECK_EQ_CTX(rd(cu, FX1_ISP_COMMON_IRQ_STATUS_OFFSET) & err_irq, err_irq, ctx);
      FX1_CHECK_EQ_CTX(rd(cu, FX1_ISP_COMMON_STATUS_OFFSET) & err_st, err_st, ctx);
      wr(cu, FX1_ISP_COMMON_IRQ_EN_OFFSET, FX1_ISP_COMMON_IRQ_EN_ERROR_EN_MASK, 11);
      FX1_CHECK_EQ_CTX(cu.irq_level() ? 1 : 0, 1, ctx + " pin with ERROR_EN");
      wr(cu, FX1_ISP_COMMON_IRQ_EN_OFFSET, 0, 12);
      FX1_CHECK_EQ_CTX(cu.irq_level() ? 1 : 0, 0, ctx + " masked: no pin, the status bit stays");
      FX1_CHECK_EQ_CTX(rd(cu, FX1_ISP_COMMON_IRQ_STATUS_OFFSET) & err_irq, err_irq, ctx + " masked event latched");
      wr(cu, s.off, s.bit, 13);
      FX1_CHECK_EQ_CTX(rd(cu, FX1_ISP_COMMON_STATUS_OFFSET) & err_st, 0, ctx + " status follows flags");
      FX1_CHECK_EQ_CTX(rd(cu, FX1_ISP_COMMON_IRQ_STATUS_OFFSET) & err_irq, err_irq, ctx + " irq sticky");
      wr(cu, FX1_ISP_COMMON_IRQ_STATUS_OFFSET, err_irq, 14);
      FX1_CHECK_EQ_CTX(rd(cu, FX1_ISP_COMMON_IRQ_STATUS_OFFSET) & err_irq, 0, ctx + " W1C");
   }
   // Not errors: flow-control conditions and statistics drops.
   const src not_errors[] = {
      {FX1_ISP_DMA_ERR_OFFSET, FX1_ISP_DMA_ERR_ERR_IDMA_UNDERRUN_BIT},
      {FX1_ISP_DMA_ERR_OFFSET, FX1_ISP_DMA_ERR_ERR_ODMA_OVERFLOW_BIT},
      {FX1_ISP_BPC_STATUS_OFFSET, FX1_ISP_BPC_STATUS_CAND_REJECT_OVF_MASK},
      {FX1_ISP_AF_STATUS_OFFSET, FX1_ISP_AF_STATUS_FRAME_DONE_MASK},
   };
   for (const src &s : not_errors) {
      control_unit cu;
      const std::string ctx = fx1_test::hex(s.off) + "/" + fx1_test::hex(s.bit) + " not an error";
      cu.hw_set(s.off, s.bit, 10);
      FX1_CHECK_EQ_CTX(rd(cu, FX1_ISP_COMMON_IRQ_STATUS_OFFSET) & err_irq, 0, ctx);
      FX1_CHECK_EQ_CTX(rd(cu, FX1_ISP_COMMON_STATUS_OFFSET) & err_st, 0, ctx);
   }
   // A TUNE_ERROR raised by a rejected command on the bus (MODE: APPLY in
   // legacy mode) and by a result read without a lease (NO_LEASE).
   control_unit cu;
   wr(cu, FX1_ISP_TUNE_CMD_OFFSET, FX1_ISP_TUNE_CMD_APPLY_MASK, 5);
   FX1_CHECK_EQ(rd(cu, FX1_ISP_TUNE_ERROR_OFFSET) & FX1_ISP_TUNE_ERROR_MODE_MASK, FX1_ISP_TUNE_ERROR_MODE_MASK);
   FX1_CHECK_EQ(rd(cu, FX1_ISP_COMMON_IRQ_STATUS_OFFSET) & err_irq, err_irq);
   wr(cu, FX1_ISP_TUNE_ERROR_OFFSET, 0xFF, 6);
   wr(cu, FX1_ISP_COMMON_IRQ_STATUS_OFFSET, err_irq, 7);
   (void)cu.csr_bus_read(FX1_ISP_AEC_FRAME_ID_OFFSET, 8);
   FX1_CHECK_EQ(rd(cu, FX1_ISP_TUNE_ERROR_OFFSET) & FX1_ISP_TUNE_ERROR_NO_LEASE_MASK, FX1_ISP_TUNE_ERROR_NO_LEASE_MASK);
   FX1_CHECK_EQ(rd(cu, FX1_ISP_COMMON_IRQ_STATUS_OFFSET) & err_irq, err_irq);
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

// Soft reset scopes (MAS Table 183 p316-317, §6.5.3; decisions §9).
void test_soft_reset(std::uint32_t reg, std::uint32_t bit) {
   control_unit cu;
   const bool common = reg == FX1_ISP_COMMON_CTRL_OFFSET;
   const std::string ctx = common ? "COMMON soft_rst" : "DMA soft_reset";
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
   wr(cu, FX1_ISP_AEC_CTRL_OFFSET, FX1_ISP_AEC_CTRL_COMMIT_MASK);  // pending until the next accepted SOF
   cu.hw_set(FX1_ISP_ODMA_BUF_DONE_OFFSET, 0x3, 2);
   cu.hw_set(FX1_ISP_DMA_IRQ_STAT_OFFSET, 0x7F, 2);
   cu.hw_hold(FX1_ISP_DMA_ERR_OFFSET, 0x1F, true, 2);
   cu.hw_set(FX1_ISP_COMMON_STATUS_OFFSET, FX1_ISP_COMMON_STATUS_FRAME_DONE_MASK | FX1_ISP_COMMON_STATUS_BUSY_MASK, 2);
   cu.hw_set(FX1_ISP_AEC_STATUS_OFFSET, FX1_ISP_AEC_STATUS_STAT_DONE_MASK, 2);
   // A frame in flight: every pixel block and the AEC/AF accumulators busy.
   cu.blocks_busy(true);
   cu.hw_set(FX1_ISP_AEC_STATUS_OFFSET, FX1_ISP_AEC_STATUS_BUSY_MASK, 2);
   cu.hw_set(FX1_ISP_AF_STATUS_OFFSET, FX1_ISP_AF_STATUS_BUSY_MASK, 2);
   cu.hw_write(FX1_ISP_AEC_GLOBAL_SUM_LO_OFFSET, 0xFFFFFFFFu, 0x1234u);
   cu.hw_write(FX1_ISP_IDMA_FRAME_COUNT_OFFSET, 0xFFFFFFFFu, 7);
   cu.hw_write(FX1_ISP_ODMA_FRAME_COUNT_OFFSET, 0xFFFFFFFFu, 6);
   wr(cu, FX1_ISP_GAMMA_LUT_ADDR_OFFSET, 9);
   wr(cu, FX1_ISP_GAMMA_LUT_DATA_OFFSET, 0xABC);
   // error_irq from DMA_ERR, stats_ready_irq from AEC stat_done.
   FX1_CHECK_EQ_CTX(rd(cu, FX1_ISP_COMMON_IRQ_STATUS_OFFSET), 0x6, ctx + " setup");

   const std::uint32_t done0 = rd(cu, FX1_ISP_ODMA_BUF_DONE_OFFSET), irq0 = rd(cu, FX1_ISP_DMA_IRQ_STAT_OFFSET);
   const std::uint32_t err0 = rd(cu, FX1_ISP_DMA_ERR_OFFSET);
   wr(cu, reg, bit | (common ? FX1_ISP_COMMON_CTRL_ISP_EN_MASK : 0x3F03u), 100);

   // Preserved by both: configuration, LUTs, frame counters.
   FX1_CHECK_EQ_CTX(rd(cu, FX1_ISP_COMMON_SCRATCH_OFFSET), 0x55AA55AA, ctx);
   FX1_CHECK_EQ_CTX(rd(cu, FX1_ISP_BLC_OFS_G0_DFT_OFFSET), 0x40, ctx);
   FX1_CHECK_EQ_CTX(rd(cu, FX1_ISP_DMA_CTRL_OFFSET), 0x3F03, ctx + " enables kept, soft_reset reads 0");
   FX1_CHECK_EQ_CTX(rd(cu, FX1_ISP_COMMON_IRQ_EN_OFFSET), 0x7, ctx);
   FX1_CHECK_EQ_CTX(rd(cu, FX1_ISP_IDMA_FRAME_COUNT_OFFSET), 7, ctx);
   FX1_CHECK_EQ_CTX(rd(cu, FX1_ISP_ODMA_FRAME_COUNT_OFFSET), 6, ctx);
   // AEC_FRAME_ID is a word of the leased result bank (MAS v1.07 §9.8); its
   // reset behaviour is checked with the banks in test_stats.cpp test_resets.
   FX1_CHECK_EQ_CTX(rd(cu, FX1_ISP_COMMON_VER_ID_OFFSET), 0x00010007, ctx);
   FX1_CHECK_EQ_CTX(cu.gamma_lut()[9], 0xABC, ctx);
   // A pending AEC commit survives both (owner answer Q4).
   FX1_CHECK_EQ_CTX(rd(cu, FX1_ISP_AEC_CTRL_OFFSET) & FX1_ISP_AEC_CTRL_COMMIT_MASK, FX1_ISP_AEC_CTRL_COMMIT_MASK,
                    ctx + " pending AEC commit kept");
   // Cleared by both: DMA ownership (at the final retirement, 15 clocks
   // later: kept through the pulse, review R8) and busy. The common scope
   // also clears DONE, DMA_IRQ_STAT and DMA_ERR; the DMA scope keeps them
   // (MAS v1.07 §9.9.8: the fault stays visible until software clears it).
   FX1_CHECK_EQ_CTX(rd(cu, FX1_ISP_IDMA_BUF_VALID_OFFSET), 0xF, ctx + " VALID kept in the pulse");
   FX1_CHECK_EQ_CTX(rd(cu, FX1_ISP_ODMA_BUF_FREE_OFFSET), 0xF, ctx + " FREE kept in the pulse");
   FX1_CHECK_EQ_CTX(rd(cu, FX1_ISP_ODMA_BUF_DONE_OFFSET), common ? 0u : done0, ctx);
   FX1_CHECK_EQ_CTX(rd(cu, FX1_ISP_DMA_IRQ_STAT_OFFSET), common ? 0u : irq0, ctx);
   FX1_CHECK_EQ_CTX(rd(cu, FX1_ISP_DMA_ERR_OFFSET), common ? 0u : err0, ctx + " holds released");
   FX1_CHECK(done0 != 0 && irq0 != 0 && err0 != 0);  // the setup had something to keep
   FX1_CHECK_EQ_CTX(rd(cu, FX1_ISP_COMMON_STATUS_OFFSET) & FX1_ISP_COMMON_STATUS_BUSY_MASK, 0, ctx);
   // The frame in flight is abandoned by both: its busy flags drop (MAS
   // §6.5.3, §9.1.1; review R1 of 2026-10-09).
   for (const std::uint32_t off : {FX1_ISP_DEMOSAIC_STATUS_OFFSET, FX1_ISP_CCM_STATUS_OFFSET,
                                   FX1_ISP_GAMMA_STATUS_OFFSET, FX1_ISP_GTM_STATUS_OFFSET,
                                   FX1_ISP_NR_2D_STATUS_OFFSET, FX1_ISP_EE_STATUS_OFFSET,
                                   FX1_ISP_CNF_STATUS_OFFSET, FX1_ISP_RESIZER_STATUS_OFFSET}) {
      FX1_CHECK_EQ_CTX(rd(cu, off) & 1u, 0, ctx + " block busy @" + fx1_test::hex(off));
   }
   FX1_CHECK_EQ_CTX(rd(cu, FX1_ISP_AEC_STATUS_OFFSET) & FX1_ISP_AEC_STATUS_BUSY_MASK, 0, ctx + " AEC busy");
   FX1_CHECK_EQ_CTX(rd(cu, FX1_ISP_AF_STATUS_OFFSET) & FX1_ISP_AF_STATUS_BUSY_MASK, 0, ctx + " AF busy");
   if (common) {
      // Pipeline scope: statistics, all interrupt status, commands, status.
      FX1_CHECK_EQ_CTX(rd(cu, FX1_ISP_COMMON_CTRL_OFFSET), FX1_ISP_COMMON_CTRL_ISP_EN_MASK, ctx);
      FX1_CHECK_EQ_CTX(rd(cu, FX1_ISP_COMMON_IRQ_STATUS_OFFSET), 0, ctx);
      FX1_CHECK_EQ_CTX(rd(cu, FX1_ISP_COMMON_STATUS_OFFSET), 0, ctx);
      FX1_CHECK_EQ_CTX(rd(cu, FX1_ISP_AEC_STATUS_OFFSET), 0, ctx);
      FX1_CHECK_EQ_CTX(rd(cu, FX1_ISP_AEC_GLOBAL_SUM_LO_OFFSET), 0, ctx);
      FX1_CHECK_EQ_CTX(rd(cu, FX1_ISP_AF_CTRL_OFFSET), 0, ctx + " command bits");
      FX1_CHECK(!cu.irq_level());
   } else {
      // DMA scope only: statistics and the pipeline-side state are untouched;
      // COMMON_IRQ_STATUS is not a DMA interrupt register (assumption U1-A1).
      // (FRAME_START is a self-clearing no-op since CSR v1.07.)
      FX1_CHECK_EQ_CTX(rd(cu, FX1_ISP_COMMON_CTRL_OFFSET), FX1_ISP_COMMON_CTRL_ISP_EN_MASK, ctx);
      FX1_CHECK_EQ_CTX(rd(cu, FX1_ISP_COMMON_IRQ_STATUS_OFFSET), 0x6, ctx);
      FX1_CHECK_EQ_CTX(rd(cu, FX1_ISP_COMMON_STATUS_OFFSET) & FX1_ISP_COMMON_STATUS_FRAME_DONE_MASK,
                       FX1_ISP_COMMON_STATUS_FRAME_DONE_MASK, ctx);
      FX1_CHECK_EQ_CTX(rd(cu, FX1_ISP_AEC_STATUS_OFFSET), FX1_ISP_AEC_STATUS_STAT_DONE_MASK, ctx);
      // (Published results are kept as well; covered over TLM by the G-CONT
      // DMA-reset bursts, where a real publication precedes the reset.)
      FX1_CHECK_EQ_CTX(rd(cu, FX1_ISP_AF_CTRL_OFFSET), FX1_ISP_AF_CTRL_SEARCH_START_MASK, ctx + " command bits");
   }
   // 15-cycle pulse (MAS §6.5.3): W1S/W1C writes are held off, RW writes are accepted.
   FX1_CHECK(cu.soft_reset_active(100));
   FX1_CHECK(cu.soft_reset_active(114));
   FX1_CHECK(!cu.soft_reset_active(115));
   wr(cu, FX1_ISP_IDMA_BUF_VALID_OFFSET, 0x1, 114);
   wr(cu, FX1_ISP_ODMA_BUF_DONE_OFFSET, 0xF, 114);  // W1C dropped: the DMA scope's kept DONE stays
   wr(cu, FX1_ISP_COMMON_SCRATCH_OFFSET, 0x1, 114);
   FX1_CHECK_EQ_CTX(cu.csr_read(FX1_ISP_ODMA_BUF_DONE_OFFSET, 114), common ? 0u : done0, ctx + " W1C held off in pulse");
   FX1_CHECK_EQ_CTX(cu.csr_read(FX1_ISP_IDMA_BUF_VALID_OFFSET, 114), 0xFu, ctx + " VALID kept to the last pulse edge");
   cu.advance_to(115);  // the final edge (the TLM engines advance the unit there)
   FX1_CHECK_EQ_CTX(cu.csr_read(FX1_ISP_IDMA_BUF_VALID_OFFSET, 115), 0u,
                    ctx + " VALID cleared at final retirement, W1S held off in pulse");
   FX1_CHECK_EQ_CTX(cu.csr_read(FX1_ISP_ODMA_BUF_FREE_OFFSET, 115), 0u, ctx + " FREE cleared at final retirement");
   FX1_CHECK_EQ_CTX(rd(cu, FX1_ISP_COMMON_SCRATCH_OFFSET), 1, ctx + " RW accepted in pulse");
   wr(cu, FX1_ISP_IDMA_BUF_VALID_OFFSET, 0x1, 115);
   FX1_CHECK_EQ_CTX(rd(cu, FX1_ISP_IDMA_BUF_VALID_OFFSET), 1, ctx + " W1S after pulse");
   // A request while the recovery is busy is rejected with PENDING; it does
   // not restart the pulse (MAS v1.07 §9.9.8). Reads return zero (W1SC).
   const std::uint32_t seq = rd(cu, FX1_ISP_COMMON_RECOVERY_SEQUENCE_OFFSET);
   wr(cu, FX1_ISP_TUNE_ERROR_OFFSET, 0xFFFFFFFFu, 199);
   wr(cu, reg, bit | (common ? FX1_ISP_COMMON_CTRL_ISP_EN_MASK : 0x3F03u), 200);
   wr(cu, reg, bit | (common ? FX1_ISP_COMMON_CTRL_ISP_EN_MASK : 0x3F03u), 210);
   FX1_CHECK(cu.soft_reset_active(214));
   FX1_CHECK(!cu.soft_reset_active(215));
   FX1_CHECK_EQ_CTX(rd(cu, FX1_ISP_TUNE_ERROR_OFFSET) & FX1_ISP_TUNE_ERROR_PENDING_MASK,
                    FX1_ISP_TUNE_ERROR_PENDING_MASK, ctx + " second request rejected");
   FX1_CHECK_EQ_CTX(rd(cu, FX1_ISP_COMMON_RECOVERY_SEQUENCE_OFFSET), seq + 1, ctx + " one accepted recovery");
   FX1_CHECK_EQ_CTX(rd(cu, reg) & bit, 0, ctx + " W1SC reads zero");
   // A hard reset clears the pending commit.
   cu.external_reset();
   FX1_CHECK_EQ_CTX(rd(cu, FX1_ISP_AEC_CTRL_OFFSET) & FX1_ISP_AEC_CTRL_COMMIT_MASK, 0, ctx + " i_rst_n");
}

// Recovery (MAS v1.07 §9.9.8): abort records in admission order with the
// outputs that had not published DONE, PHASE drain -> pulse -> idle with DONE,
// SEQUENCE, PENDING for a request while busy, an error joining, ERROR[c] and
// FRAME_DONE at final retirement; records survive a soft reset, i_rst_n
// clears them.
void test_recovery() {
   control_unit cu;
   std::uint64_t drain_until = 0;
   cu.set_drain_probe([&](std::uint64_t) { return drain_until; });
   const auto st = [&](std::uint64_t c) { return cu.csr_read(FX1_ISP_COMMON_RECOVERY_STATUS_OFFSET, c); };
   const std::uint32_t busy = FX1_ISP_COMMON_RECOVERY_STATUS_BUSY_MASK, done = FX1_ISP_COMMON_RECOVERY_STATUS_DONE_MASK;
   const auto phase = [](std::uint32_t v) { return (v & FX1_ISP_COMMON_RECOVERY_STATUS_PHASE_MASK) >> FX1_ISP_COMMON_RECOVERY_STATUS_PHASE_SHIFT; };
   const auto scope = [](std::uint32_t v) { return (v & FX1_ISP_COMMON_RECOVERY_STATUS_SCOPE_MASK) >> FX1_ISP_COMMON_RECOVERY_STATUS_SCOPE_SHIFT; };
   const auto count = [](std::uint32_t v) { return (v & FX1_ISP_COMMON_RECOVERY_STATUS_COUNT_MASK) >> FX1_ISP_COMMON_RECOVERY_STATUS_COUNT_SHIFT; };
   const auto outcome = [](std::uint32_t failed, std::uint32_t cause) {
      return (failed << FX1_ISP_COMMON_ABORT0_OUTCOME_FAILED_OUTPUTS_SHIFT) | (cause << FX1_ISP_COMMON_ABORT0_OUTCOME_CAUSE_SHIFT);
   };
   FX1_CHECK_EQ(st(0), 0u);
   wr(cu, FX1_ISP_COMMON_FRAME_WIDTH_OFFSET, 640, 1);
   wr(cu, FX1_ISP_COMMON_FRAME_HEIGHT_OFFSET, 480, 2);
   wr(cu, FX1_ISP_CH2_CROP_SIZE_OFFSET, 640 | (480u << 16), 3);
   wr(cu, FX1_ISP_CH2_CROP_CTRL_OFFSET, 1, 4);
   wr(cu, FX1_ISP_OUTPUT_ENABLE_OFFSET, 5, 5);  // CH0 + CH2
   cu.accepted_sof(20);                         // ingress 1
   cu.accepted_sof(30);                         // ingress 2
   cu.output_done(1, 0);                        // frame 1: CH0 done ...
   cu.frame_retired();                          // ... then retired: not recorded
   cu.accepted_sof(40);                         // ingress 3
   cu.output_done(3, 2);                        // frame 3: CH2 done, CH0 not
   // DMA command at 100, drain to 105 (an issued response), pulse to 120.
   drain_until = 105;
   wr(cu, FX1_ISP_DMA_CTRL_OFFSET, FX1_ISP_DMA_CTRL_SOFT_RESET_MASK, 100);
   FX1_CHECK_EQ(cu.csr_read(FX1_ISP_COMMON_RECOVERY_SEQUENCE_OFFSET, 101), 1u);
   FX1_CHECK_EQ(st(101) & (busy | done), busy);
   FX1_CHECK_EQ(phase(st(101)), 1u);  // drain
   FX1_CHECK_EQ(scope(st(101)), 1u);  // DMA
   FX1_CHECK_EQ(phase(st(105)), 2u);  // reset pulse
   FX1_CHECK(cu.recovery_busy(119));
   FX1_CHECK(cu.admission_state(110, 0) == control_unit::admission::wait_edge);
   // An AXI error during the recovery joins it; a software request is PENDING.
   FX1_CHECK(!cu.recovery_request(control_unit::reset_scope::dma, control_unit::rc_read, 106));
   wr(cu, FX1_ISP_COMMON_CTRL_OFFSET, FX1_ISP_COMMON_CTRL_SOFT_RST_MASK, 107);
   FX1_CHECK_EQ(rd(cu, FX1_ISP_TUNE_ERROR_OFFSET) & FX1_ISP_TUNE_ERROR_PENDING_MASK, FX1_ISP_TUNE_ERROR_PENDING_MASK);
   FX1_CHECK_EQ(cu.csr_read(FX1_ISP_COMMON_RECOVERY_SEQUENCE_OFFSET, 108), 1u);
   // Final retirement at 120: records, DONE, BUSY clear, ERROR[2], FRAME_DONE.
   FX1_CHECK_EQ(st(120), done | (2u << FX1_ISP_COMMON_RECOVERY_STATUS_COUNT_SHIFT));
   cu.advance_to(121);
   FX1_CHECK(!cu.recovery_busy(121));
   FX1_CHECK_EQ(count(st(121)), 2u);  // the unretired ingress 2 and 3, in admission order
   FX1_CHECK_EQ(cu.csr_read(FX1_ISP_COMMON_ABORT0_FRAME_OFFSET, 121), 2u);
   FX1_CHECK_EQ(cu.csr_read(FX1_ISP_COMMON_ABORT0_OUTCOME_OFFSET, 121), outcome(0x5, control_unit::rc_dma));
   FX1_CHECK_EQ(cu.csr_read(FX1_ISP_COMMON_ABORT1_FRAME_OFFSET, 121), 3u);
   FX1_CHECK_EQ(cu.csr_read(FX1_ISP_COMMON_ABORT1_OUTCOME_OFFSET, 121), outcome(0x1, control_unit::rc_dma));  // CH2 succeeded
   FX1_CHECK_EQ(cu.csr_read(FX1_ISP_COMMON_ABORT2_FRAME_OFFSET, 121), 0u);  // unused records read zero
   FX1_CHECK_EQ(cu.csr_read(FX1_ISP_COMMON_ABORT2_OUTCOME_OFFSET, 121), 0u);
   FX1_CHECK_EQ(cu.csr_read(FX1_ISP_COMMON_ABORT3_FRAME_OFFSET, 121), 0u);
   FX1_CHECK_EQ(cu.csr_read(FX1_ISP_COMMON_ABORT3_OUTCOME_OFFSET, 121), 0u);
   FX1_CHECK_EQ(rd(cu, FX1_ISP_OUTPUT_IRQ_STATUS_OFFSET) & FX1_ISP_OUTPUT_IRQ_STATUS_ERROR_MASK,
                0x5u << FX1_ISP_OUTPUT_IRQ_STATUS_ERROR_SHIFT);
   FX1_CHECK_EQ(rd(cu, FX1_ISP_COMMON_STATUS_OFFSET) & FX1_ISP_COMMON_STATUS_FRAME_DONE_MASK,
                FX1_ISP_COMMON_STATUS_FRAME_DONE_MASK);
   FX1_CHECK_EQ(rd(cu, FX1_ISP_COMMON_IRQ_STATUS_OFFSET) & FX1_ISP_COMMON_IRQ_STATUS_ERROR_IRQ_MASK,
                FX1_ISP_COMMON_IRQ_STATUS_ERROR_IRQ_MASK);  // every accepted recovery
   // DONE is W1C; a new accepted recovery clears it; the records survive a
   // soft reset (outside the pulse domain) and are replaced by its own.
   drain_until = 0;
   cu.accepted_sof(200);  // ingress 4, enabled CH0 + CH2, nothing done
   wr(cu, FX1_ISP_COMMON_CTRL_OFFSET, FX1_ISP_COMMON_CTRL_SOFT_RST_MASK, 300);
   FX1_CHECK_EQ(st(301) & (busy | done), busy);
   FX1_CHECK_EQ(scope(st(301)), 2u);  // common
   FX1_CHECK_EQ(phase(st(301)), 2u);  // nothing to drain
   FX1_CHECK_EQ(count(st(301)), 2u);  // the previous records until the final retirement
   cu.advance_to(316);
   FX1_CHECK_EQ(cu.csr_read(FX1_ISP_COMMON_RECOVERY_SEQUENCE_OFFSET, 316), 2u);
   FX1_CHECK_EQ(count(st(316)), 1u);
   FX1_CHECK_EQ(cu.csr_read(FX1_ISP_COMMON_ABORT0_FRAME_OFFSET, 316), 4u);
   FX1_CHECK_EQ(cu.csr_read(FX1_ISP_COMMON_ABORT0_OUTCOME_OFFSET, 316), outcome(0x5, control_unit::rc_common));
   wr(cu, FX1_ISP_COMMON_RECOVERY_STATUS_OFFSET, done, 320);
   FX1_CHECK_EQ(st(321) & done, 0u);
   cu.external_reset();
   FX1_CHECK_EQ(st(0), 0u);
   FX1_CHECK_EQ(cu.csr_read(FX1_ISP_COMMON_RECOVERY_SEQUENCE_OFFSET, 0), 0u);
   FX1_CHECK_EQ(cu.csr_read(FX1_ISP_COMMON_ABORT0_FRAME_OFFSET, 0), 0u);
}

// The pending AEC commit kept by a soft reset is consumed at the next accepted SOF.
void test_aec_commit_across_soft_reset() {
   control_unit cu;
   wr(cu, FX1_ISP_AEC_ZONE_CFG_OFFSET, 4u | (3u << FX1_ISP_AEC_ZONE_CFG_NUM_ZONE_Y_SHIFT), 1);
   wr(cu, FX1_ISP_AEC_CTRL_OFFSET, FX1_ISP_AEC_CTRL_EN_MASK | FX1_ISP_AEC_CTRL_COMMIT_MASK, 2);
   wr(cu, FX1_ISP_COMMON_CTRL_OFFSET, FX1_ISP_COMMON_CTRL_SOFT_RST_MASK, 3);
   FX1_CHECK(rd(cu, FX1_ISP_AEC_CTRL_OFFSET) & FX1_ISP_AEC_CTRL_COMMIT_MASK);
   cu.accepted_sof(40);
   FX1_CHECK_EQ(rd(cu, FX1_ISP_AEC_CTRL_OFFSET) & FX1_ISP_AEC_CTRL_COMMIT_MASK, 0);  // consumed: self-clears
}

void test_frame_start_and_done() {
   control_unit cu;
   const std::uint32_t fs = FX1_ISP_COMMON_CTRL_FRAME_START_MASK;
   // CSR v1.07: FRAME_START is accepted, self-clears and has no effect (DEC-16 superseded).
   wr(cu, FX1_ISP_COMMON_CTRL_OFFSET, fs | FX1_ISP_COMMON_CTRL_ISP_EN_MASK);
   FX1_CHECK_EQ(rd(cu, FX1_ISP_COMMON_CTRL_OFFSET), FX1_ISP_COMMON_CTRL_ISP_EN_MASK);
   wr(cu, FX1_ISP_COMMON_CTRL_OFFSET, 0);
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

// MAS v1.07 §9.9.8-9.9.9, Table 9-192: one ingress sequence counts accepted
// SOFs from 0 (the first frame is 1), modulo 2^32. Both soft resets keep it;
// only a hard reset restarts it. It is not a software tag, a DMA count or the
// core-clock FRAME_COUNTER.
void test_ingress_sequence() {
   control_unit cu;
   std::uint64_t cyc = 10;
   cu.accepted_sof(cyc++);
   FX1_CHECK_EQ(cu.frame_id(), 1u);
   cu.accepted_sof(cyc++);
   FX1_CHECK_EQ(cu.frame_id(), 2u);
   wr(cu, FX1_ISP_COMMON_CTRL_OFFSET, FX1_ISP_COMMON_CTRL_SOFT_RST_MASK, cyc);
   cyc += 20;
   cu.accepted_sof(cyc++);
   FX1_CHECK_EQ(cu.frame_id(), 3u);  // common recovery keeps the numbering
   wr(cu, FX1_ISP_DMA_CTRL_OFFSET, FX1_ISP_DMA_CTRL_SOFT_RESET_MASK, cyc);
   cyc += 20;
   cu.accepted_sof(cyc++);
   FX1_CHECK_EQ(cu.frame_id(), 4u);  // DMA recovery too
   wr(cu, FX1_ISP_COMMON_INPUT_FORMAT_OFFSET, FX1_ISP_COMMON_INPUT_FORMAT_DEPTH_RAW10, cyc++);
   cu.accepted_sof(cyc++);
   FX1_CHECK_EQ(cu.frame_id(), 5u);  // a mode change does not restart it either
   cu.debug_set_frame_counter(0xFFFFFFFFu);
   cu.accepted_sof(cyc++);
   FX1_CHECK_EQ(cu.frame_id(), 0u);  // wrap32
   cu.accepted_sof(cyc++);
   FX1_CHECK_EQ(cu.frame_id(), 1u);
   cu.external_reset();
   cu.accepted_sof(cyc++);
   FX1_CHECK_EQ(cu.frame_id(), 1u);  // only i_rst_n restarts it
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
// Pending commands do not survive a hard reset or a COMMON soft reset; a
// DMA-only soft reset keeps the pipeline's (MAS Table 183).
void test_commands_cleared_on_reset() {
   control_unit cu;
   wr(cu, FX1_ISP_IDMA_BUF_VALID_OFFSET, 0x1, 1);
   FX1_CHECK_EQ(cu.commands().size(), 1);
   cu.external_reset();
   FX1_CHECK_EQ(cu.commands().size(), 0);

   wr(cu, FX1_ISP_ODMA_BUF_FREE_OFFSET, 0x3, 10);
   FX1_CHECK_EQ(cu.commands().size(), 1);
   wr(cu, FX1_ISP_DMA_CTRL_OFFSET, FX1_ISP_DMA_CTRL_SOFT_RESET_MASK | FX1_ISP_DMA_CTRL_IDMA_EN_MASK, 11);
   FX1_CHECK_EQ_CTX(cu.commands().size(), 1, "after DMA soft reset");
   wr(cu, FX1_ISP_COMMON_CTRL_OFFSET, FX1_ISP_COMMON_CTRL_SOFT_RST_MASK, 100);
   FX1_CHECK_EQ_CTX(cu.commands().size(), 0, "after COMMON soft reset");
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
   FX1_CHECK_EQ(rd(cu, FX1_ISP_EE_LUT_RDATA_OFFSET), cu.ee_table(0)[0]);  // poison after i_rst_n (MAS §6.5.2)
   wr(cu, FX1_ISP_EE_LUT_CTRL_OFFSET, 2u << sel_shift);        // CONTRAST_POS, 32 entries
   wr(cu, FX1_ISP_EE_LUT_ADDR_OFFSET, 33);                     // top bit ignored -> entry 1
   wr(cu, FX1_ISP_EE_LUT_WDATA_OFFSET, 0x1234);
   FX1_CHECK_EQ(rd(cu, FX1_ISP_EE_LUT_ADDR_OFFSET), 34);
   FX1_CHECK_EQ(cu.ee_table(2)[1], 0x1234);
   wr(cu, FX1_ISP_EE_LUT_ADDR_OFFSET, 1);
   FX1_CHECK_EQ(rd(cu, FX1_ISP_EE_LUT_RDATA_OFFSET), 0x1234);
   wr(cu, FX1_ISP_EE_LUT_CTRL_OFFSET, 0);  // LUMA, 64 entries
   wr(cu, FX1_ISP_EE_LUT_ADDR_OFFSET, 33);
   FX1_CHECK_EQ(rd(cu, FX1_ISP_EE_LUT_RDATA_OFFSET), ((33u * 0x9E37u) ^ 0xA5A5u) & 0xFFFFu);  // unwritten: poison
   wr(cu, FX1_ISP_EE_LUT_ADDR_OFFSET, 63);
   wr(cu, FX1_ISP_EE_LUT_WDATA_OFFSET, 0x7);
   FX1_CHECK_EQ(rd(cu, FX1_ISP_EE_LUT_ADDR_OFFSET), 0);  // wraps at 64
   FX1_CHECK_EQ(cu.ee_table(0)[63], 0x7);
   FX1_CHECK_EQ(cu.ee_table(1)[63], (((64u + 63u) * 0x9E37u) ^ 0xA5A5u) & 0xFFFFu);  // untouched bank
   // GTM read-back port: zero at reset, out-of-table index reads zero.
   wr(cu, FX1_ISP_GTM_LUT_ADDR_OFFSET, 70);
   FX1_CHECK_EQ(rd(cu, FX1_ISP_GTM_LUT_RDATA_OFFSET), 0);
}

// EE resident profiles (MAS §6.18.4.1, §9.2; milestone U3).
// MAS v1.07 legacy-mode table validity (V107-4): Gamma (§6.14.4), GTM manual
// bank 0 (§6.16.4.3) and EE residents (§9.9.1).
void test_table_confirmation() {
   control_unit cu;
   std::uint64_t cyc = 1;
   const std::uint32_t confirm = FX1_ISP_GAMMA_CTRL_LUT_LOAD_CONFIRM_MASK;
   auto loaded = [&] { return (rd(cu, FX1_ISP_GAMMA_STATUS_OFFSET) & FX1_ISP_GAMMA_STATUS_LUT_LOADED_MASK) != 0; };
   auto load_gamma = [&](unsigned n) {
      wr(cu, FX1_ISP_GAMMA_LUT_ADDR_OFFSET, 0, cyc++);
      for (unsigned i = 0; i < n; ++i) {
         wr(cu, FX1_ISP_GAMMA_LUT_DATA_OFFSET, 16u * i, cyc++);
      }
   };
   // Unwritten entries read zero; a partial curve cannot be confirmed.
   wr(cu, FX1_ISP_GAMMA_LUT_ADDR_OFFSET, 7, cyc++);
   FX1_CHECK_EQ(rd(cu, FX1_ISP_GAMMA_LUT_RDATA_OFFSET), 0);
   load_gamma(255);
   wr(cu, FX1_ISP_GAMMA_CTRL_OFFSET, confirm, cyc++);
   FX1_CHECK(!loaded());
   load_gamma(256);
   wr(cu, FX1_ISP_GAMMA_CTRL_OFFSET, FX1_ISP_GAMMA_CTRL_EN_MASK, cyc++);
   wr(cu, FX1_ISP_GAMMA_CTRL_OFFSET, confirm | FX1_ISP_GAMMA_CTRL_EN_MASK, cyc++);  // EN = 1 before the edge: refused
   FX1_CHECK(!loaded());
   wr(cu, FX1_ISP_GAMMA_CTRL_OFFSET, 0, cyc++);
   cu.accepted_sof(cyc++);  // a frame in flight: not idle
   wr(cu, FX1_ISP_GAMMA_CTRL_OFFSET, confirm, cyc++);
   FX1_CHECK(!loaded());
   cu.frame_retired();
   wr(cu, FX1_ISP_GAMMA_CTRL_OFFSET, confirm, cyc++);
   FX1_CHECK(loaded());
   wr(cu, FX1_ISP_GAMMA_CTRL_OFFSET, FX1_ISP_GAMMA_CTRL_EN_MASK, cyc++);
   cu.accepted_sof(cyc++);
   cu.frame_retired();
   FX1_CHECK(pipe::snapshot_config(cu).gamma_en);
   // A write invalidates, also on the confirmation edge (the write wins).
   wr(cu, FX1_ISP_GAMMA_CTRL_OFFSET, 0, cyc++);
   wr(cu, FX1_ISP_GAMMA_LUT_ADDR_OFFSET, 3, cyc++);
   wr(cu, FX1_ISP_GAMMA_LUT_DATA_OFFSET, 99, cyc);
   wr(cu, FX1_ISP_GAMMA_CTRL_OFFSET, confirm, cyc++);
   FX1_CHECK(!loaded());
   wr(cu, FX1_ISP_GAMMA_CTRL_OFFSET, confirm, cyc++);
   FX1_CHECK(loaded());
   wr(cu, FX1_ISP_GAMMA_CTRL_OFFSET, FX1_ISP_GAMMA_CTRL_EN_MASK, cyc++);
   FX1_CHECK(pipe::snapshot_config(cu).gamma_en);
   wr(cu, FX1_ISP_GAMMA_LUT_DATA_OFFSET, 5, cyc++);  // a live write while enabled: pass-through
   FX1_CHECK(!pipe::snapshot_config(cu).gamma_en);
   load_gamma(256);
   wr(cu, FX1_ISP_GAMMA_CTRL_OFFSET, confirm, cyc++);  // clears EN on the confirming edge: EN was 1 (V107-A15)
   FX1_CHECK(!loaded());
   FX1_CHECK_EQ(rd(cu, FX1_ISP_GAMMA_CTRL_OFFSET) & FX1_ISP_GAMMA_CTRL_EN_MASK, 0);
   wr(cu, FX1_ISP_GAMMA_CTRL_OFFSET, confirm, cyc++);
   FX1_CHECK(loaded());
   // A legacy common reset keeps the confirmation; a hard reset clears it.
   wr(cu, FX1_ISP_COMMON_CTRL_OFFSET, FX1_ISP_COMMON_CTRL_SOFT_RST_MASK, cyc);
   cyc += 64;
   FX1_CHECK(loaded());
   cu.external_reset();
   FX1_CHECK(!loaded());

   // GTM manual bank 0: both byte lanes of every point; readback of
   // incomplete points is zero; the datapath applies it only when complete.
   wr(cu, FX1_ISP_GTM_CTRL_OFFSET, FX1_ISP_GTM_CTRL_MANUAL_MASK, cyc++);
   wr(cu, FX1_ISP_GTM_LUT_ADDR_OFFSET, 0, cyc++);
   for (unsigned i = 0; i < 65; ++i) {
      wr(cu, FX1_ISP_GTM_LUT_DATA_OFFSET, 0x0100u + i, cyc++, i == 7 ? 0x000000FFu : 0xFFFFFFFFu);
   }
   FX1_CHECK(!cu.gtm_manual_complete());
   wr(cu, FX1_ISP_GTM_LUT_ADDR_OFFSET, 7, cyc++);
   FX1_CHECK_EQ(rd(cu, FX1_ISP_GTM_LUT_RDATA_OFFSET), 0);  // one lane only
   wr(cu, FX1_ISP_GTM_LUT_DATA_OFFSET, 0x0100u + 7u, cyc++, 0x0000FF00u);
   FX1_CHECK(cu.gtm_manual_complete());
   wr(cu, FX1_ISP_GTM_LUT_ADDR_OFFSET, 7, cyc++);
   FX1_CHECK_EQ(rd(cu, FX1_ISP_GTM_LUT_RDATA_OFFSET), 0x0107);
   wr(cu, FX1_ISP_GTM_CTRL_OFFSET, FX1_ISP_GTM_CTRL_MANUAL_MASK | FX1_ISP_GTM_CTRL_EN_MASK, cyc++);
   const pipe::pipeline_config c = pipe::snapshot_config(cu);
   FX1_CHECK(c.gtm_curve_valid);
   FX1_CHECK_EQ((*c.gtm_bank)[7], 0x0107);
   wr(cu, FX1_ISP_GTM_LUT_ADDR_OFFSET, 9, cyc++);
   wr(cu, FX1_ISP_GTM_LUT_DATA_OFFSET, 0x2222, cyc++);  // EN = 1: discarded
   FX1_CHECK_EQ((*pipe::snapshot_config(cu).gtm_bank)[9], 0x0109);

   // EE residents: confirmation with EE disabled and all 192 entries; a
   // scalar or LUT write and an input-domain change clear it.
   wr(cu, FX1_ISP_EE_CTRL_OFFSET, 1u << FX1_ISP_EE_CTRL_PROFILE_WR_SEL_SHIFT, cyc++);  // resident 1
   auto ee_load = [&] {
      for (unsigned b = 0; b < 4; ++b) {
         wr(cu, FX1_ISP_EE_LUT_CTRL_OFFSET, b << FX1_ISP_EE_LUT_CTRL_LUT_SEL_SHIFT, cyc++);
         wr(cu, FX1_ISP_EE_LUT_ADDR_OFFSET, 0, cyc++);
         for (unsigned i = 0; i < control_unit::ee_bank_entries(b); ++i) {
            wr(cu, FX1_ISP_EE_LUT_WDATA_OFFSET, 0x8000, cyc++);
         }
      }
   };
   const std::uint32_t ee_confirm = FX1_ISP_EE_LUT_CTRL_PROFILE_LOAD_CONFIRM_MASK;
   wr(cu, FX1_ISP_EE_LUT_CTRL_OFFSET, ee_confirm, cyc++);  // nothing written: NOT_READY
   FX1_CHECK(!cu.ee_resident_valid(1));
   FX1_CHECK_EQ(rd(cu, FX1_ISP_TUNE_ERROR_OFFSET) & FX1_ISP_TUNE_ERROR_NOT_READY_MASK,
                FX1_ISP_TUNE_ERROR_NOT_READY_MASK);
   ee_load();
   wr(cu, FX1_ISP_EE_CTRL_OFFSET, (1u << FX1_ISP_EE_CTRL_PROFILE_WR_SEL_SHIFT) | FX1_ISP_EE_CTRL_EN_MASK, cyc++);
   wr(cu, FX1_ISP_EE_LUT_CTRL_OFFSET, ee_confirm, cyc++);  // EE enabled: refused
   FX1_CHECK(!cu.ee_resident_valid(1));
   wr(cu, FX1_ISP_EE_CTRL_OFFSET, 1u << FX1_ISP_EE_CTRL_PROFILE_WR_SEL_SHIFT, cyc++);
   wr(cu, FX1_ISP_EE_LUT_CTRL_OFFSET, ee_confirm, cyc++);
   FX1_CHECK(cu.ee_resident_valid(1));
   FX1_CHECK_EQ(rd(cu, FX1_ISP_EE_STATUS_OFFSET) & FX1_ISP_EE_STATUS_LUT_VALID_MASK,
                2u << FX1_ISP_EE_STATUS_LUT_VALID_SHIFT);
   wr(cu, FX1_ISP_EE_CTRL_OFFSET,
      (1u << FX1_ISP_EE_CTRL_PROFILE_WR_SEL_SHIFT) | (1u << FX1_ISP_EE_CTRL_PROFILE_SEL_SHIFT) | FX1_ISP_EE_CTRL_EN_MASK,
      cyc++);
   cu.accepted_sof(cyc++);
   cu.frame_retired();
   FX1_CHECK(cu.ee_frame().en);
   FX1_CHECK_EQ(rd(cu, FX1_ISP_EE_STATUS_OFFSET) & (FX1_ISP_EE_STATUS_ACTIVE_PROFILE_MASK | FX1_ISP_EE_STATUS_ACTIVE_VALID_MASK),
                (1u << FX1_ISP_EE_STATUS_ACTIVE_PROFILE_SHIFT) | FX1_ISP_EE_STATUS_ACTIVE_VALID_MASK);
   wr(cu, FX1_ISP_EE_BETA_OFFSET, 0x2001, cyc++);  // scalar write: resident invalid, frame passes through
   cu.accepted_sof(cyc++);
   cu.frame_retired();
   FX1_CHECK(!cu.ee_frame().en);
   FX1_CHECK_EQ(rd(cu, FX1_ISP_EE_STATUS_OFFSET) & FX1_ISP_EE_STATUS_ACTIVE_VALID_MASK, 0);
   wr(cu, FX1_ISP_EE_CTRL_OFFSET, 1u << FX1_ISP_EE_CTRL_PROFILE_WR_SEL_SHIFT, cyc++);
   wr(cu, FX1_ISP_EE_LUT_CTRL_OFFSET, ee_confirm, cyc++);  // the flags survived the scalar write
   FX1_CHECK(cu.ee_resident_valid(1));
   wr(cu, FX1_ISP_COMMON_FRAME_WIDTH_OFFSET, 1280, cyc++);  // input-domain change
   FX1_CHECK(!cu.ee_resident_valid(1));
   wr(cu, FX1_ISP_EE_LUT_CTRL_OFFSET, ee_confirm, cyc++);  // the flags are gone too
   FX1_CHECK(!cu.ee_resident_valid(1));
}

void test_ee_profiles() {
   control_unit cu;
   std::uint64_t cyc = 1;
   const std::uint32_t ctrl = FX1_ISP_EE_CTRL_OFFSET;
   const auto wr_sel = [](unsigned p) { return p << FX1_ISP_EE_CTRL_PROFILE_WR_SEL_SHIFT; };
   const auto sel = [](unsigned p) { return p << FX1_ISP_EE_CTRL_PROFILE_SEL_SHIFT; };
   // Reset: every profile holds the CSR reset values (ALPHA 0.5, BETA 0.25,
   // clamps 16/24, radial gains 1.0; MAS p189).
   for (unsigned p = 0; p < control_unit::ee_profiles; ++p) {
      FX1_CHECK_EQ(cu.ee_profile_scalar(p, ee_alpha), 0x4000);
      FX1_CHECK_EQ(cu.ee_profile_scalar(p, ee_beta), 0x2000);
      FX1_CHECK_EQ(cu.ee_profile_scalar(p, ee_clamp), 0x1810);
      FX1_CHECK_EQ(cu.ee_profile_scalar(p, ee_radial_gain_23), 0x80008000u);
   }
   // Writes land in the resident named by PROFILE_WR_SEL; a scalar reads back
   // that resident's value (MAS v1.07 §9.9.1).
   wr(cu, ctrl, wr_sel(1), cyc++);
   wr(cu, FX1_ISP_EE_ALPHA_OFFSET, 0x1111, cyc++);
   wr(cu, ctrl, wr_sel(2), cyc++);
   wr(cu, FX1_ISP_EE_ALPHA_OFFSET, 0x2222, cyc++);
   FX1_CHECK_EQ(cu.ee_profile_scalar(0, ee_alpha), 0x4000);
   FX1_CHECK_EQ(cu.ee_profile_scalar(1, ee_alpha), 0x1111);
   FX1_CHECK_EQ(cu.ee_profile_scalar(2, ee_alpha), 0x2222);
   FX1_CHECK_EQ(rd(cu, FX1_ISP_EE_ALPHA_OFFSET), 0x2222);
   // A partial write merges with the selected resident's value (MAS v1.07
   // §9.9.1; the 07/10 merge with the last-written bytes is superseded).
   wr(cu, ctrl, wr_sel(1), cyc++);
   wr(cu, FX1_ISP_EE_ALPHA_OFFSET, 0x0055, cyc++, 0x000000FFu);
   FX1_CHECK_EQ(cu.ee_profile_scalar(1, ee_alpha), 0x1155);
   FX1_CHECK_EQ(rd(cu, FX1_ISP_EE_ALPHA_OFFSET), 0x1155);
   // Code 3: writes discarded, reads resident 0 (MAS v1.07 §9.9.1, audit R2).
   wr(cu, ctrl, wr_sel(3), cyc++);
   wr(cu, FX1_ISP_EE_ALPHA_OFFSET, 0x3333, cyc++);
   wr(cu, FX1_ISP_EE_CLAMP_OFFSET, 0x0101, cyc++);
   FX1_CHECK_EQ(rd(cu, FX1_ISP_EE_ALPHA_OFFSET), 0x4000);
   FX1_CHECK_EQ(rd(cu, FX1_ISP_EE_CLAMP_OFFSET), 0x1810);
   for (unsigned p = 0; p < control_unit::ee_profiles; ++p) {
      FX1_CHECK_EQ(cu.ee_profile_scalar(p, ee_clamp), 0x1810);
   }
   // The table port follows PROFILE_WR_SEL for writes and read-back.
   const std::uint32_t lut_sel = 1u << FX1_ISP_EE_LUT_CTRL_LUT_SEL_SHIFT;  // ACTIVITY
   for (unsigned p = 0; p < 3; ++p) {
      wr(cu, ctrl, wr_sel(p), cyc++);
      wr(cu, FX1_ISP_EE_LUT_CTRL_OFFSET, lut_sel, cyc++);
      wr(cu, FX1_ISP_EE_LUT_ADDR_OFFSET, 5, cyc++);
      wr(cu, FX1_ISP_EE_LUT_WDATA_OFFSET, 0x100 + p, cyc++);
   }
   for (unsigned p = 0; p < 3; ++p) {
      FX1_CHECK_EQ(cu.ee_table(p, 1)[5], 0x100 + p);
      wr(cu, ctrl, wr_sel(p), cyc++);
      wr(cu, FX1_ISP_EE_LUT_ADDR_OFFSET, 5, cyc++);
      FX1_CHECK_EQ(rd(cu, FX1_ISP_EE_LUT_RDATA_OFFSET), 0x100 + p);
   }
   wr(cu, ctrl, wr_sel(3), cyc++);  // code 3: data discarded, address advances, reads resident 0
   wr(cu, FX1_ISP_EE_LUT_ADDR_OFFSET, 5, cyc++);
   FX1_CHECK_EQ(rd(cu, FX1_ISP_EE_LUT_RDATA_OFFSET), 0x100);
   wr(cu, FX1_ISP_EE_LUT_WDATA_OFFSET, 0x999, cyc++);
   FX1_CHECK_EQ(rd(cu, FX1_ISP_EE_LUT_ADDR_OFFSET), 6);
   for (unsigned p = 0; p < 3; ++p) {
      FX1_CHECK_EQ(cu.ee_table(p, 1)[5], 0x100 + p);
   }
   // Resident 2 complete and confirmed with EE disabled (MAS v1.07 §9.9.1),
   // keeping its entry ACTIVITY[5].
   wr(cu, ctrl, wr_sel(2), cyc++);
   for (unsigned b = 0; b < 4; ++b) {
      wr(cu, FX1_ISP_EE_LUT_CTRL_OFFSET, b << FX1_ISP_EE_LUT_CTRL_LUT_SEL_SHIFT, cyc++);
      wr(cu, FX1_ISP_EE_LUT_ADDR_OFFSET, 0, cyc++);
      for (unsigned i = 0; i < control_unit::ee_bank_entries(b); ++i) {
         wr(cu, FX1_ISP_EE_LUT_WDATA_OFFSET, b == 1 && i == 5 ? 0x102u : 0x8000u, cyc++);
      }
   }
   wr(cu, FX1_ISP_EE_LUT_CTRL_OFFSET, FX1_ISP_EE_LUT_CTRL_PROFILE_LOAD_CONFIRM_MASK, cyc++);
   FX1_CHECK(cu.ee_resident_valid(2));
   // PROFILE_SEL, EN and the profile's scalars are sampled at the accepted
   // SOF; code 3 selects profile 0.
   wr(cu, ctrl, FX1_ISP_EE_CTRL_EN_MASK | sel(2), cyc++);
   FX1_CHECK(!cu.ee_frame().en);
   cu.accepted_sof(cyc++);
   FX1_CHECK(cu.ee_frame().en);
   FX1_CHECK_EQ(cu.ee_frame().profile, 2);
   FX1_CHECK_EQ(cu.ee_frame().scalar[ee_alpha], 0x2222);
   FX1_CHECK(&cu.ee_table(1) == &cu.ee_table(2, 1));  // the datapath reads the frame's profile
   wr(cu, ctrl, FX1_ISP_EE_CTRL_EN_MASK | sel(2) | wr_sel(2), cyc++);
   wr(cu, FX1_ISP_EE_ALPHA_OFFSET, 0x7777, cyc++);  // the active profile: next frame
   FX1_CHECK_EQ(cu.ee_frame().scalar[ee_alpha], 0x2222);
   wr(cu, ctrl, sel(3), cyc++);
   FX1_CHECK_EQ(cu.ee_frame().profile, 2);
   cu.accepted_sof(cyc++);
   FX1_CHECK_EQ(cu.ee_frame().profile, 0);
   FX1_CHECK(!cu.ee_frame().en);
   wr(cu, ctrl, FX1_ISP_EE_CTRL_EN_MASK | sel(2), cyc++);
   cu.accepted_sof(cyc++);
   FX1_CHECK_EQ(cu.ee_frame().scalar[ee_alpha], 0x7777);
   // Soft reset keeps every profile (configuration); i_rst_n restores them.
   wr(cu, FX1_ISP_COMMON_CTRL_OFFSET, FX1_ISP_COMMON_CTRL_SOFT_RST_MASK, cyc);
   cyc += 64;
   FX1_CHECK_EQ(cu.ee_profile_scalar(1, ee_alpha), 0x1155);
   FX1_CHECK_EQ(cu.ee_table(2, 1)[5], 0x102);
   cu.external_reset();
   FX1_CHECK_EQ(cu.ee_profile_scalar(2, ee_alpha), 0x4000);
   FX1_CHECK_EQ(cu.ee_table(1, 1)[5], ((256u + 64u + 5u) * 0x9E37u ^ 0xA5A5u) & 0xFFFFu);
}

// EE_STATUS.error: one event per frame EE processes with radial gain enabled
// and a gain of the frame's profile above 1.5 (MAS §6.18.4.1, §9.2); not a
// level of the live registers.
void test_ee_radial_error() {
   control_unit cu;
   std::uint64_t cyc = 1;
   const std::uint32_t err = FX1_ISP_EE_STATUS_ERROR_MASK;
   // v1.07 (§9.9.1): a resident is used only once confirmed, with EE
   // disabled; every scalar write clears it, so each frame below confirms the
   // residents first. Tables are loaded once, after the geometry.
   const auto confirm_all = [&] {
      const std::uint32_t c = cu.csr_read(FX1_ISP_EE_CTRL_OFFSET);
      for (unsigned p = 0; p < control_unit::ee_profiles; ++p) {
         wr(cu, FX1_ISP_EE_CTRL_OFFSET, p << FX1_ISP_EE_CTRL_PROFILE_WR_SEL_SHIFT, cyc++);
         wr(cu, FX1_ISP_EE_LUT_CTRL_OFFSET, FX1_ISP_EE_LUT_CTRL_PROFILE_LOAD_CONFIRM_MASK, cyc++);
      }
      wr(cu, FX1_ISP_EE_CTRL_OFFSET, c, cyc++);
   };
   const auto frame = [&] {
      confirm_all();
      cu.accepted_sof(cyc++);
      cu.ee_end_of_frame();
      cu.frame_retired();  // the frame leaves: the pipeline is idle for IDLE-class writes (V107-3)
   };
   // Tables of every resident; an input-domain change clears their flags, so
   // they are reloaded after each geometry change (MAS v1.07 §9.9.1).
   const auto load_tables = [&] {
      const std::uint32_t c = cu.csr_read(FX1_ISP_EE_CTRL_OFFSET);
      for (unsigned p = 0; p < control_unit::ee_profiles; ++p) {
         wr(cu, FX1_ISP_EE_CTRL_OFFSET, p << FX1_ISP_EE_CTRL_PROFILE_WR_SEL_SHIFT, cyc++);
         for (unsigned b = 0; b < 4; ++b) {
            wr(cu, FX1_ISP_EE_LUT_CTRL_OFFSET, b << FX1_ISP_EE_LUT_CTRL_LUT_SEL_SHIFT, cyc++);
            wr(cu, FX1_ISP_EE_LUT_ADDR_OFFSET, 0, cyc++);
            for (unsigned i = 0; i < control_unit::ee_bank_entries(b); ++i) {
               wr(cu, FX1_ISP_EE_LUT_WDATA_OFFSET, 0x8000, cyc++);
            }
         }
      }
      wr(cu, FX1_ISP_EE_CTRL_OFFSET, c, cyc++);
   };
   wr(cu, FX1_ISP_COMMON_FRAME_WIDTH_OFFSET, 64, cyc++);
   wr(cu, FX1_ISP_COMMON_FRAME_HEIGHT_OFFSET, 48, cyc++);
   load_tables();
   wr(cu, FX1_ISP_EE_FEATURE_EN_OFFSET, FX1_ISP_EE_FEATURE_EN_RADIAL_EN_MASK, cyc++);
   wr(cu, FX1_ISP_EE_RADIAL_GAIN_23_OFFSET, 0x8000C001u, cyc++);  // gain 2 just above 1.5
   FX1_CHECK_EQ(rd(cu, FX1_ISP_EE_STATUS_OFFSET) & err, 0);  // a write raises nothing
   frame();
   FX1_CHECK_EQ(rd(cu, FX1_ISP_EE_STATUS_OFFSET) & err, 0);  // EE disabled: not processed
   wr(cu, FX1_ISP_EE_CTRL_OFFSET, FX1_ISP_EE_CTRL_EN_MASK, cyc++);
   frame();
   FX1_CHECK_EQ(rd(cu, FX1_ISP_EE_STATUS_OFFSET) & err, err);
   FX1_CHECK(rd(cu, FX1_ISP_COMMON_IRQ_STATUS_OFFSET) & FX1_ISP_COMMON_IRQ_STATUS_ERROR_IRQ_MASK);
   wr(cu, FX1_ISP_EE_STATUS_OFFSET, err, cyc++);  // W1C: stays clear until the next frame
   FX1_CHECK_EQ(rd(cu, FX1_ISP_EE_STATUS_OFFSET) & err, 0);
   frame();
   FX1_CHECK_EQ(rd(cu, FX1_ISP_EE_STATUS_OFFSET) & err, err);  // the illegal gain is still programmed
   wr(cu, FX1_ISP_EE_STATUS_OFFSET, err, cyc++);
   wr(cu, FX1_ISP_EE_RADIAL_GAIN_23_OFFSET, 0x8000C000u, cyc++);  // exactly 1.5: legal
   frame();
   FX1_CHECK_EQ(rd(cu, FX1_ISP_EE_STATUS_OFFSET) & err, 0);
   wr(cu, FX1_ISP_EE_RADIAL_GAIN_01_OFFSET, 0xC0018000u, cyc++);  // gain 1 above 1.5, radial off
   wr(cu, FX1_ISP_EE_FEATURE_EN_OFFSET, 0, cyc++);
   frame();
   FX1_CHECK_EQ(rd(cu, FX1_ISP_EE_STATUS_OFFSET) & err, 0);
   wr(cu, FX1_ISP_EE_FEATURE_EN_OFFSET, FX1_ISP_EE_FEATURE_EN_RADIAL_EN_MASK, cyc++);
   wr(cu, FX1_ISP_COMMON_FRAME_HEIGHT_OFFSET, 4, cyc++);  // below 5 x 5: not processed
   frame();
   FX1_CHECK_EQ(rd(cu, FX1_ISP_EE_STATUS_OFFSET) & err, 0);
   // The gain of another profile does not count; the frame's profile does.
   wr(cu, FX1_ISP_COMMON_FRAME_HEIGHT_OFFSET, 48, cyc++);
   load_tables();
   wr(cu, FX1_ISP_EE_CTRL_OFFSET, FX1_ISP_EE_CTRL_EN_MASK | (1u << FX1_ISP_EE_CTRL_PROFILE_SEL_SHIFT), cyc++);
   frame();  // profile 1: reset gains, radial off
   FX1_CHECK_EQ(rd(cu, FX1_ISP_EE_STATUS_OFFSET) & err, 0);
   // Profile 1 with radial on and legal gains; profile 0 keeps an illegal gain 1.
   const std::uint32_t wr1 = 1u << FX1_ISP_EE_CTRL_PROFILE_WR_SEL_SHIFT, sel1 = 1u << FX1_ISP_EE_CTRL_PROFILE_SEL_SHIFT;
   wr(cu, FX1_ISP_EE_CTRL_OFFSET, FX1_ISP_EE_CTRL_EN_MASK | sel1 | wr1, cyc++);
   wr(cu, FX1_ISP_EE_FEATURE_EN_OFFSET, FX1_ISP_EE_FEATURE_EN_RADIAL_EN_MASK, cyc++);
   FX1_CHECK_EQ(cu.ee_profile_scalar(0, ee_radial_gain_01), 0xC0018000u);
   frame();
   FX1_CHECK_EQ(rd(cu, FX1_ISP_EE_STATUS_OFFSET) & err, 0);  // profile 0's gain does not count
   wr(cu, FX1_ISP_EE_RADIAL_GAIN_01_OFFSET, 0x8000FFFFu, cyc++);  // profile 1 gain 0 above 1.5
   frame();
   FX1_CHECK_EQ(rd(cu, FX1_ISP_EE_STATUS_OFFSET) & err, err);
}

// GTM table writes reach the memory only while GTM_CTRL.EN = 0, whatever
// MANUAL says (MAS p146; U3). They go to manual bank 0, which GTM_LUT_RDATA
// returns while MANUAL = 1 (MAS v1.07 §6.16.4.2).
void test_gtm_lut_gate() {
   control_unit cu;
   std::uint64_t cyc = 1;
   const auto load = [&](std::uint32_t ctrl, std::uint32_t v) {
      wr(cu, FX1_ISP_GTM_CTRL_OFFSET, ctrl, cyc++);
      wr(cu, FX1_ISP_GTM_LUT_ADDR_OFFSET, 3, cyc++);
      wr(cu, FX1_ISP_GTM_LUT_DATA_OFFSET, v, cyc++);
      FX1_CHECK_EQ(rd(cu, FX1_ISP_GTM_LUT_ADDR_OFFSET), 4);  // the address advances anyway
      wr(cu, FX1_ISP_GTM_CTRL_OFFSET, FX1_ISP_GTM_CTRL_MANUAL_MASK, cyc++);  // read manual bank 0
      wr(cu, FX1_ISP_GTM_LUT_ADDR_OFFSET, 3, cyc++);
      return rd(cu, FX1_ISP_GTM_LUT_RDATA_OFFSET);
   };
   FX1_CHECK_EQ(load(FX1_ISP_GTM_CTRL_MANUAL_MASK, 0x123), 0x123);      // EN = 0: written
   FX1_CHECK_EQ(load(0, 0x124), 0x124);
   FX1_CHECK_EQ(load(FX1_ISP_GTM_CTRL_EN_MASK | FX1_ISP_GTM_CTRL_MANUAL_MASK, 0x555), 0x124);  // discarded
   FX1_CHECK_EQ(load(FX1_ISP_GTM_CTRL_EN_MASK, 0x556), 0x124);
}

// MAS v1.07 §6.16.4.1, §6.16.4.3 (V107-5, ALG-GTM-09 superseded): a frame's
// curve is built from the KEY, LWHITE and MANUAL captured at its accepted SOF,
// not from the live CSRs at its EOF.
void test_gtm_capture_at_sof() {
   const std::uint32_t yavg = 90, k1 = 0xBB, l1 = 0x180, k2 = 0x40, l2 = 0x300;
   FX1_CHECK(pipe::gtm_build_curve(yavg, k1, l1) != pipe::gtm_build_curve(yavg, k2, l2));
   FX1_CHECK(pipe::gtm_build_curve(yavg, k1, l1) != pipe::gtm_build_curve(yavg, k2, l1));
   FX1_CHECK(pipe::gtm_build_curve(yavg, k1, l1) != pipe::gtm_build_curve(yavg, k1, l2));
   control_unit cu;
   std::uint64_t cyc = 1;
   wr(cu, FX1_ISP_GTM_KEY_OFFSET, k1, cyc++);
   wr(cu, FX1_ISP_GTM_LWHITE_OFFSET, l1, cyc++);
   wr(cu, FX1_ISP_GTM_CTRL_OFFSET, FX1_ISP_GTM_CTRL_EN_MASK, cyc++);
   cu.accepted_sof(cyc++);  // frame 1 captures k1, l1, MANUAL = 0
   wr(cu, FX1_ISP_GTM_KEY_OFFSET, k2, cyc++);
   wr(cu, FX1_ISP_GTM_LWHITE_OFFSET, l2, cyc++);
   wr(cu, FX1_ISP_GTM_CTRL_OFFSET, FX1_ISP_GTM_CTRL_EN_MASK | FX1_ISP_GTM_CTRL_MANUAL_MASK, cyc++);
   cu.gtm_end_of_frame(yavg);
   cu.accepted_sof(cyc++);  // frame 2 reads frame 1's curve and captures MANUAL = 1
   FX1_CHECK(cu.gtm_curve_valid());
   FX1_CHECK(cu.gtm_read_bank() == pipe::gtm_build_curve(yavg, k1, l1));
   wr(cu, FX1_ISP_GTM_CTRL_OFFSET, FX1_ISP_GTM_CTRL_EN_MASK, cyc++);  // MANUAL cleared mid-frame
   cu.gtm_end_of_frame(yavg + 60);  // generator held by the captured MANUAL
   cu.accepted_sof(cyc++);  // frame 3 captures k2, l2, MANUAL = 0
   FX1_CHECK(cu.gtm_read_bank() == pipe::gtm_build_curve(yavg, k1, l1));
   cu.gtm_end_of_frame(yavg);
   cu.accepted_sof(cyc++);
   FX1_CHECK(cu.gtm_read_bank() == pipe::gtm_build_curve(yavg, k2, l2));
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
   auto resizer = [&](bool en, std::uint32_t scale) {  // settled reads: the published set
      wr(cu, FX1_ISP_RESIZER_CTRL_OFFSET, (en ? 1u : 0u) | (scale << FX1_ISP_RESIZER_CTRL_SCALE_SHIFT));
   };
   set_geom(2688, 1520, FX1_ISP_COMMON_BAYER_PATTERN_BGGR);  // DEC-04 input
   FX1_CHECK_EQ(out().first, 2686);
   FX1_CHECK_EQ(out().second, 1518);
   resizer(true, FX1_ISP_RESIZER_CTRL_SCALE_RES_1920X1080);
   FX1_CHECK_EQ(out().first, 1920);
   FX1_CHECK_EQ(out().second, 1080);
   const auto status = [&] { return rd(cu, FX1_ISP_CH0_GEOMETRY_STATUS_OFFSET); };
   const auto error = [](unsigned code) { return code << FX1_ISP_CH0_GEOMETRY_STATUS_ERROR_SHIFT; };
   // No silent FullRes fallback (MAS v1.07 §6.20.4.2): a target larger than
   // the crop and a reserved code are rejected; the last valid readback stays.
   resizer(true, FX1_ISP_RESIZER_CTRL_SCALE_RES_3840X2160);
   FX1_CHECK_EQ(status(), error(geo_exceeds_crop));
   FX1_CHECK_EQ(out().first, 1920);
   resizer(true, 0xF);
   FX1_CHECK_EQ(status(), error(geo_mode));
   FX1_CHECK_EQ(out().second, 1080);
   resizer(false, FX1_ISP_RESIZER_CTRL_SCALE_RES_640X480);  // disabled scaler: crop size
   FX1_CHECK_EQ(status(), FX1_ISP_CH0_GEOMETRY_STATUS_VALID_MASK);
   FX1_CHECK_EQ(out().first, 2686);
   resizer(true, FX1_ISP_RESIZER_CTRL_SCALE_RES_640X480);
   FX1_CHECK_EQ(out().first, 640);
   FX1_CHECK_EQ(out().second, 480);
   // CSR v1.07 5-bit scale: 16..21 are the AI presets, 22..31 reserved; no
   // alias onto the 4-bit codes (21 & 0xF = 1920x1080).
   const std::pair<std::uint32_t, std::uint32_t> ai[6] = {{640, 640}, {640, 384}, {640, 352},
                                                          {512, 512}, {416, 416}, {320, 320}};
   for (std::uint32_t code = 16; code < 32; ++code) {
      resizer(true, code);
      const std::string c = "scale " + std::to_string(code);
      FX1_CHECK_EQ_CTX((rd(cu, FX1_ISP_RESIZER_CTRL_OFFSET) & FX1_ISP_RESIZER_CTRL_SCALE_MASK) >>
                           FX1_ISP_RESIZER_CTRL_SCALE_SHIFT,
                       code, c + " stored");
      if (code <= 21) {
         FX1_CHECK_EQ_CTX(status(), FX1_ISP_CH0_GEOMETRY_STATUS_VALID_MASK, c);
         FX1_CHECK_EQ_CTX(out().first, ai[code - 16].first, c + " width");
         FX1_CHECK_EQ_CTX(out().second, ai[code - 16].second, c + " height");
      } else {
         FX1_CHECK_EQ_CTX(status(), error(geo_mode), c);
         // The last published valid readback: every write of this test shares
         // one edge, so only the reset key's set (1920 x 1080) was published
         // before; a settled read derives the live key from it.
         FX1_CHECK_EQ_CTX(out().first, 1920u, c + " width: the last valid readback");
      }
   }
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

// LSC load sequencer (MAS §6.8.4.1 Table 31; owner Q5).
// Growing the mesh during a load must never validate an undersized bank.
void test_lsc_resize_during_load() {
   control_unit cu;
   std::uint64_t cyc = 1;
   wr(cu, FX1_ISP_LSC_MESH_NODES_OFFSET, 0x0202, cyc++);
   wr(cu, FX1_ISP_LSC_LOAD_CTRL_OFFSET, 1u | FX1_ISP_LSC_LOAD_CTRL_LOAD_BEGIN_MASK, cyc++);
   wr(cu, FX1_ISP_LSC_MESH_NODES_OFFSET, 0x0303, cyc++);
   for (unsigned i = 0; i < 36; ++i) {
      wr(cu, FX1_ISP_LSC_COEF_DATA_OFFSET, 0x40000, cyc++);
   }
   wr(cu, FX1_ISP_LSC_LOAD_CTRL_OFFSET, 1u | FX1_ISP_LSC_LOAD_CTRL_LOAD_VALIDATE_MASK, cyc++);
   FX1_CHECK(!cu.lsc_profile_valid(1));
   FX1_CHECK(rd(cu, FX1_ISP_LSC_ERROR_OFFSET) & FX1_ISP_LSC_ERROR_COEF_COUNT_MASK);
   // A fresh load in the new geometry recovers normally.
   lsc_load(cu, 1, 36, 0x40000, cyc);
   FX1_CHECK(cu.lsc_profile_valid(1));
   FX1_CHECK_EQ(cu.lsc_profile(1).size(), 36);
}

void test_lsc_loader() {
   control_unit cu;
   std::uint64_t cyc = 1;
   const std::uint32_t st = FX1_ISP_LSC_PROFILE_STATUS_OFFSET, err = FX1_ISP_LSC_ERROR_OFFSET;
   FX1_CHECK_EQ(rd(cu, FX1_ISP_LSC_LOAD_STATUS_OFFSET), 0);
   // Invalid geometry at reset: begin refused.
   wr(cu, FX1_ISP_LSC_LOAD_CTRL_OFFSET, 1u | FX1_ISP_LSC_LOAD_CTRL_LOAD_BEGIN_MASK, cyc++);
   FX1_CHECK_EQ(rd(cu, err), FX1_ISP_LSC_ERROR_GEOMETRY_INVALID_MASK);
   FX1_CHECK_EQ(rd(cu, FX1_ISP_LSC_LOAD_CTRL_OFFSET) & 0x700u, 0);  // commands self-clear
   wr(cu, err, 0x3F, cyc++);
   // 2..32 nodes per axis (MAS v1.07 §6.8.4, V107-A2), not above the active size.
   wr(cu, FX1_ISP_LSC_MESH_NODES_OFFSET, 33u | (2u << 8), cyc++);
   FX1_CHECK_EQ(rd(cu, st) & FX1_ISP_LSC_PROFILE_STATUS_GEOMETRY_VALID_MASK, 0);
   wr(cu, FX1_ISP_LSC_MESH_NODES_OFFSET, 32u | (32u << 8), cyc++);
   FX1_CHECK(rd(cu, st) & FX1_ISP_LSC_PROFILE_STATUS_GEOMETRY_VALID_MASK);
   wr(cu, FX1_ISP_COMMON_FRAME_WIDTH_OFFSET, 30, cyc++);  // 32 nodes on a 30-pixel line
   FX1_CHECK_EQ(rd(cu, st) & FX1_ISP_LSC_PROFILE_STATUS_GEOMETRY_VALID_MASK, 0);
   wr(cu, FX1_ISP_COMMON_FRAME_WIDTH_OFFSET, 1920, cyc++);
   wr(cu, FX1_ISP_LSC_MESH_NODES_OFFSET, 3u | (2u << 8), cyc++);  // 3 x 2 nodes -> 24 coefficients
   FX1_CHECK_EQ(rd(cu, st), FX1_ISP_LSC_PROFILE_STATUS_GEOMETRY_VALID_MASK);
   // The active index is never a destination, even while invalid (owner Q5):
   // at boot, profile 0 is active and loading it is refused.
   wr(cu, FX1_ISP_LSC_LOAD_CTRL_OFFSET, FX1_ISP_LSC_LOAD_CTRL_LOAD_BEGIN_MASK, cyc++);
   FX1_CHECK_EQ(rd(cu, err), FX1_ISP_LSC_ERROR_ACTIVE_LOAD_REJECT_MASK);
   FX1_CHECK_EQ(rd(cu, FX1_ISP_LSC_LOAD_STATUS_OFFSET) & FX1_ISP_LSC_LOAD_STATUS_LOAD_BUSY_MASK, 0);
   wr(cu, err, 0x3F, cyc++);
   // Boot sequence: load profile 1, validate, select it; adopted at the next SOF.
   lsc_load(cu, 1, 24, 0x40000, cyc);
   FX1_CHECK_EQ(rd(cu, err), 0);
   FX1_CHECK_EQ(rd(cu, st) & 0x47u, 0x02u);  // valid[1]; the active profile (0) is not valid
   FX1_CHECK_EQ(cu.lsc_profile(1)[23], 0x40000 + 23);
   FX1_CHECK_EQ(rd(cu, FX1_ISP_LSC_LOAD_STATUS_OFFSET),
                24u | FX1_ISP_LSC_LOAD_STATUS_LOAD_FULL_MASK | (1u << FX1_ISP_LSC_LOAD_STATUS_LOAD_PROFILE_SHIFT));
   wr(cu, FX1_ISP_LSC_PROFILE_SEL_OFFSET, 1, cyc++);
   FX1_CHECK_EQ(rd(cu, err), 0);
   FX1_CHECK_EQ(cu.lsc_active_profile(), 0);
   cu.accepted_sof(cyc++);
   FX1_CHECK_EQ(cu.lsc_active_profile(), 1);
   FX1_CHECK_EQ(rd(cu, st) & FX1_ISP_LSC_PROFILE_STATUS_ACTIVE_PROFILE_MASK, 1u << 4);
   FX1_CHECK(rd(cu, st) & FX1_ISP_LSC_PROFILE_STATUS_ACTIVE_VALID_MASK);
   // Now profile 1 is active: loading it is refused.
   wr(cu, FX1_ISP_LSC_LOAD_CTRL_OFFSET, 1u | FX1_ISP_LSC_LOAD_CTRL_LOAD_BEGIN_MASK, cyc++);
   FX1_CHECK_EQ(rd(cu, err), FX1_ISP_LSC_ERROR_ACTIVE_LOAD_REJECT_MASK);
   wr(cu, err, 0x3F, cyc++);
   // A short load fails at validate; the destination is invalid (MAS Table 31).
   lsc_load(cu, 2, 24, 0x10, cyc);
   FX1_CHECK(cu.lsc_profile_valid(2));
   lsc_load(cu, 2, 23, 0x99, cyc);
   FX1_CHECK_EQ(rd(cu, err), FX1_ISP_LSC_ERROR_COEF_COUNT_MASK);
   FX1_CHECK(!cu.lsc_profile_valid(2));
   wr(cu, err, 0x3F, cyc++);
   // An extra write also fails.
   lsc_load(cu, 2, 25, 0x77, cyc);
   FX1_CHECK_EQ(rd(cu, err), FX1_ISP_LSC_ERROR_COEF_COUNT_MASK);
   FX1_CHECK(!cu.lsc_profile_valid(2));
   wr(cu, err, 0x3F, cyc++);
   // A begin invalidates its destination at once; abort leaves it invalid,
   // and abort wins over a simultaneous validate. The active profile is unchanged.
   lsc_load(cu, 2, 24, 0x20, cyc);
   FX1_CHECK(cu.lsc_profile_valid(2));
   wr(cu, FX1_ISP_LSC_LOAD_CTRL_OFFSET, 2u | FX1_ISP_LSC_LOAD_CTRL_LOAD_BEGIN_MASK, cyc++);
   FX1_CHECK(!cu.lsc_profile_valid(2));
   wr(cu, FX1_ISP_LSC_COEF_DATA_OFFSET, 5, cyc++);
   FX1_CHECK_EQ(rd(cu, FX1_ISP_LSC_LOAD_STATUS_OFFSET),
                1u | FX1_ISP_LSC_LOAD_STATUS_LOAD_BUSY_MASK | (2u << FX1_ISP_LSC_LOAD_STATUS_LOAD_PROFILE_SHIFT));
   wr(cu, FX1_ISP_LSC_LOAD_CTRL_OFFSET, 2u | FX1_ISP_LSC_LOAD_CTRL_LOAD_ABORT_MASK |
                                             FX1_ISP_LSC_LOAD_CTRL_LOAD_VALIDATE_MASK, cyc++);  // abort wins
   FX1_CHECK(!cu.lsc_profile_valid(2));
   FX1_CHECK(cu.lsc_profile_valid(1) && cu.lsc_active_profile() == 1);
   FX1_CHECK_EQ(rd(cu, err), 0);
   // Protocol violations: data without a load (VP assumption, owner query U0-Q6),
   // validate without begin (MAS Table 31).
   wr(cu, FX1_ISP_LSC_COEF_DATA_OFFSET, 1, cyc++);
   FX1_CHECK_EQ(rd(cu, err), FX1_ISP_LSC_ERROR_LOAD_PROTOCOL_MASK);
   wr(cu, err, 0x3F, cyc++);
   wr(cu, FX1_ISP_LSC_LOAD_CTRL_OFFSET, FX1_ISP_LSC_LOAD_CTRL_LOAD_VALIDATE_MASK, cyc++);
   FX1_CHECK_EQ(rd(cu, err), FX1_ISP_LSC_ERROR_LOAD_PROTOCOL_MASK);
   wr(cu, err, 0x3F, cyc++);
   // Coefficient above 4.0 flags coef_range but is stored.
   lsc_load(cu, 2, 24, 0x100001, cyc);
   FX1_CHECK_EQ(rd(cu, err), FX1_ISP_LSC_ERROR_COEF_RANGE_MASK);
   FX1_CHECK(cu.lsc_profile_valid(2));
   wr(cu, err, 0x3F, cyc++);
   // Selection: unloaded profile or code 3 refused (value kept).
   wr(cu, FX1_ISP_LSC_PROFILE_SEL_OFFSET, 3, cyc++);
   FX1_CHECK_EQ(rd(cu, err), FX1_ISP_LSC_ERROR_PROFILE_SEL_REJECT_MASK);
   FX1_CHECK_EQ(rd(cu, FX1_ISP_LSC_PROFILE_SEL_OFFSET), 1);
   wr(cu, err, 0x3F, cyc++);
   wr(cu, FX1_ISP_LSC_PROFILE_SEL_OFFSET, 0, cyc++);  // profile 0 was never loaded
   FX1_CHECK_EQ(rd(cu, err), FX1_ISP_LSC_ERROR_PROFILE_SEL_REJECT_MASK);
   FX1_CHECK_EQ(rd(cu, FX1_ISP_LSC_PROFILE_SEL_OFFSET), 1);
   wr(cu, err, 0x3F, cyc++);
   // A COMMON soft reset keeps meshes and validity and drops a load in progress;
   // a DMA-only soft reset does not touch the loader (MAS Table 183).
   wr(cu, FX1_ISP_LSC_LOAD_CTRL_OFFSET, FX1_ISP_LSC_LOAD_CTRL_LOAD_BEGIN_MASK, cyc++);
   FX1_CHECK(rd(cu, FX1_ISP_LSC_LOAD_STATUS_OFFSET) & FX1_ISP_LSC_LOAD_STATUS_LOAD_BUSY_MASK);
   wr(cu, FX1_ISP_DMA_CTRL_OFFSET, FX1_ISP_DMA_CTRL_SOFT_RESET_MASK, cyc);
   cyc += 64;
   FX1_CHECK(rd(cu, FX1_ISP_LSC_LOAD_STATUS_OFFSET) & FX1_ISP_LSC_LOAD_STATUS_LOAD_BUSY_MASK);
   wr(cu, FX1_ISP_COMMON_CTRL_OFFSET, FX1_ISP_COMMON_CTRL_SOFT_RST_MASK, cyc);
   cyc += 64;
   FX1_CHECK_EQ(rd(cu, FX1_ISP_LSC_LOAD_STATUS_OFFSET) & FX1_ISP_LSC_LOAD_STATUS_LOAD_BUSY_MASK, 0);
   FX1_CHECK(cu.lsc_profile_valid(1) && cu.lsc_profile_valid(2));
   FX1_CHECK(!cu.lsc_profile_valid(0));  // invalidated by the begin before the reset
   // Maximum mesh: 32 x 32 nodes = 4096 coefficients. LOAD_FULL and validity.
   wr(cu, FX1_ISP_LSC_MESH_NODES_OFFSET, 32u | (32u << 8), cyc++);
   FX1_CHECK_EQ(rd(cu, st) & 0x7u, 0);  // a geometry change invalidates every profile
   lsc_load(cu, 2, 4u * 32u * 32u, 0x40000, cyc);
   FX1_CHECK(cu.lsc_profile_valid(2));
   FX1_CHECK(rd(cu, FX1_ISP_LSC_LOAD_STATUS_OFFSET) & FX1_ISP_LSC_LOAD_STATUS_LOAD_FULL_MASK);
   FX1_CHECK_EQ(cu.lsc_profile(2).size(), 4096);
   // External reset clears validity.
   cu.external_reset();
   FX1_CHECK(!cu.lsc_profile_valid(2));
}

// The selection is re-checked at the accepted SOF (MAS Table 31, Figure 29;
// review R2 of 2026-10-09): a selector accepted while its profile was valid,
// whose profile a later begin, abort or count mismatch invalidated, leaves the
// previous profile active and reports PROFILE_SEL_REJECT at every SOF while it
// differs from the active one. A later successful validate lets it take effect.
void test_lsc_selection_at_sof() {
   control_unit cu;
   std::uint64_t cyc = 1;
   const std::uint32_t st = FX1_ISP_LSC_PROFILE_STATUS_OFFSET, err = FX1_ISP_LSC_ERROR_OFFSET;
   const std::uint32_t reject = FX1_ISP_LSC_ERROR_PROFILE_SEL_REJECT_MASK;
   const auto active_is = [&](unsigned p, const std::string &where) {
      FX1_CHECK_EQ_CTX(cu.lsc_active_profile(), p, where);
      FX1_CHECK_EQ_CTX(rd(cu, st) & FX1_ISP_LSC_PROFILE_STATUS_ACTIVE_PROFILE_MASK,
                       p << FX1_ISP_LSC_PROFILE_STATUS_ACTIVE_PROFILE_SHIFT, where);
      FX1_CHECK_EQ_CTX(rd(cu, st) & FX1_ISP_LSC_PROFILE_STATUS_ACTIVE_VALID_MASK,
                       FX1_ISP_LSC_PROFILE_STATUS_ACTIVE_VALID_MASK, where + " active valid");
   };
   wr(cu, FX1_ISP_LSC_MESH_NODES_OFFSET, 0x0202, cyc++);
   lsc_load(cu, 1, 16, 0x40000, cyc);
   wr(cu, FX1_ISP_LSC_PROFILE_SEL_OFFSET, 1, cyc++);
   cu.accepted_sof(cyc++);
   active_is(1, "boot");

   // 1. Begin after the selection: the SOF arrives while the destination loads.
   lsc_load(cu, 2, 16, 0x50000, cyc);
   wr(cu, FX1_ISP_LSC_PROFILE_SEL_OFFSET, 2, cyc++);
   FX1_CHECK_EQ(rd(cu, err), 0);  // valid when written
   wr(cu, FX1_ISP_LSC_LOAD_CTRL_OFFSET, 2u | FX1_ISP_LSC_LOAD_CTRL_LOAD_BEGIN_MASK, cyc++);
   cu.accepted_sof(cyc++);
   active_is(1, "SOF during the load");
   FX1_CHECK_EQ_CTX(rd(cu, err), reject, "SOF during the load");
   wr(cu, err, 0x3F, cyc++);

   // 2. Abort: the destination stays invalid; every SOF keeps profile 1.
   wr(cu, FX1_ISP_LSC_LOAD_CTRL_OFFSET, 2u | FX1_ISP_LSC_LOAD_CTRL_LOAD_ABORT_MASK, cyc++);
   cu.accepted_sof(cyc++);
   active_is(1, "SOF after abort");
   FX1_CHECK_EQ_CTX(rd(cu, err), reject, "SOF after abort");
   wr(cu, err, 0x3F, cyc++);
   cu.accepted_sof(cyc++);
   FX1_CHECK_EQ_CTX(rd(cu, err), reject, "reported again at the next SOF");
   wr(cu, err, 0x3F, cyc++);

   // 3. Count mismatch: still invalid.
   lsc_load(cu, 2, 15, 0x60000, cyc);
   FX1_CHECK_EQ(rd(cu, err), FX1_ISP_LSC_ERROR_COEF_COUNT_MASK);
   wr(cu, err, 0x3F, cyc++);
   cu.accepted_sof(cyc++);
   active_is(1, "SOF after a count mismatch");
   FX1_CHECK_EQ_CTX(rd(cu, err), reject, "SOF after a count mismatch");
   wr(cu, err, 0x3F, cyc++);

   // 4. A successful validate: the pending selection takes effect at the SOF,
   // with the new mesh, and nothing more is reported.
   lsc_load(cu, 2, 16, 0x70000, cyc);
   FX1_CHECK_EQ(rd(cu, err), 0);
   active_is(1, "validated, before the SOF");
   cu.accepted_sof(cyc++);
   active_is(2, "SOF after validate");
   FX1_CHECK_EQ(cu.lsc_profile(2)[0], 0x70000);
   cu.accepted_sof(cyc++);
   FX1_CHECK_EQ_CTX(rd(cu, err), 0, "no rejection once the selection is active");
}

// Resizer configuration controller (MAS §6.20.4.3-4, Table 123-124; U4):
// stable key -> E0..E35 -> publication; observations at an edge see what was
// published before it. DEC-24/26 (commit gate) superseded.
void test_resizer_publication() {
   control_unit cu;
   const std::uint32_t en = FX1_ISP_RESIZER_CTRL_EN_MASK, sh = FX1_ISP_RESIZER_CTRL_SCALE_SHIFT;
   const auto ow = [&](std::uint64_t c) { return cu.csr_read(FX1_ISP_RESIZER_OUT_W_OFFSET, c); };
   const auto oh = [&](std::uint64_t c) { return cu.csr_read(FX1_ISP_RESIZER_OUT_H_OFFSET, c); };
   // Reset set: readback zero (CSR v1.07); the reset key (1920 x 1080 FullRes)
   // publishes one edge later.
   FX1_CHECK_EQ(ow(0), 0);
   FX1_CHECK_EQ(oh(0), 0);
   FX1_CHECK_EQ(ow(2), 1920);
   FX1_CHECK_EQ(oh(2), 1080);
   // Pass-through (scaling disabled): the key written on edge w is stable from
   // w + 1 (E0) and published on the next edge, so visible from w + 3.
   wr(cu, FX1_ISP_COMMON_FRAME_WIDTH_OFFSET, 3840, 10);
   wr(cu, FX1_ISP_COMMON_FRAME_HEIGHT_OFFSET, 2160, 11);
   FX1_CHECK_EQ(oh(13), 1080);
   FX1_CHECK_EQ(ow(14), 3840);
   FX1_CHECK_EQ(oh(14), 2160);
   // A stable scaling key: E0 on the edge after the write, published at E35.
   wr(cu, FX1_ISP_RESIZER_CTRL_OFFSET, en | (FX1_ISP_RESIZER_CTRL_SCALE_RES_1920X1080 << sh), 20);
   const std::uint64_t e0 = 21, e35 = e0 + control_unit::resizer_derivation_cycles;
   FX1_CHECK_EQ(ow(e35), 3840);      // during the derivation the previous set stays
   FX1_CHECK_EQ(ow(e35 + 1), 1920);  // published
   FX1_CHECK_EQ(oh(e35 + 1), 1080);
   FX1_CHECK_EQ(cu.resizer_published(e35 + 1).scaling ? 1 : 0, 1);
   // A SOF on the publication edge samples the previous set; one edge later the new.
   {
      control_unit a;
      wr(a, FX1_ISP_COMMON_FRAME_WIDTH_OFFSET, 3840, 10);
      wr(a, FX1_ISP_COMMON_FRAME_HEIGHT_OFFSET, 2160, 11);
      wr(a, FX1_ISP_RESIZER_CTRL_OFFSET, en | (FX1_ISP_RESIZER_CTRL_SCALE_RES_1920X1080 << sh), 20);
      a.accepted_sof(e35);
      FX1_CHECK_EQ(a.resizer_frame().out.width, 3840);
      FX1_CHECK_EQ(a.resizer_frame().scaling ? 1 : 0, 0);
      a.accepted_sof(e35 + 1);
      FX1_CHECK_EQ(a.resizer_frame().out.width, 1920);
      FX1_CHECK_EQ(a.resizer_frame().key.width, 3840);
   }
   // A key changed during a derivation: the stale result is not published; a
   // new derivation starts when the dividers are free (at that E35).
   wr(cu, FX1_ISP_RESIZER_CTRL_OFFSET, en | (FX1_ISP_RESIZER_CTRL_SCALE_RES_1280X720 << sh), 100);  // E0 101
   wr(cu, FX1_ISP_RESIZER_CTRL_OFFSET, en | (FX1_ISP_RESIZER_CTRL_SCALE_RES_640X480 << sh), 110);
   FX1_CHECK_EQ(ow(136), 1920);
   FX1_CHECK_EQ(ow(137), 1920);  // HD was derived but is stale: never published
   FX1_CHECK_EQ(ow(171), 1920);
   FX1_CHECK_EQ(ow(172), 640);   // VGA: E0 136, E35 171
   FX1_CHECK_EQ(oh(172), 480);
   // Changed and changed back during a derivation: the saved key matches again.
   wr(cu, FX1_ISP_RESIZER_CTRL_OFFSET, en | (FX1_ISP_RESIZER_CTRL_SCALE_RES_1280X720 << sh), 200);  // E0 201
   wr(cu, FX1_ISP_RESIZER_CTRL_OFFSET, en | (FX1_ISP_RESIZER_CTRL_SCALE_RES_960X540 << sh), 210);
   wr(cu, FX1_ISP_RESIZER_CTRL_OFFSET, en | (FX1_ISP_RESIZER_CTRL_SCALE_RES_1280X720 << sh), 220);
   FX1_CHECK_EQ(ow(237), 1280);
   // Pass-through has priority (MAS §6.20.4.3, review U4-R2): disabling during
   // a derivation publishes the bypass set on schedule (E0 + 1 of the new key),
   // and the derivation's completion does not restore the stale mode.
   wr(cu, FX1_ISP_RESIZER_CTRL_OFFSET, en | (FX1_ISP_RESIZER_CTRL_SCALE_RES_640X360 << sh), 300);  // E0 301
   FX1_CHECK_EQ(ow(320), 1280);
   wr(cu, FX1_ISP_RESIZER_CTRL_OFFSET, FX1_ISP_RESIZER_CTRL_SCALE_RES_640X360 << sh, 320);   // EN = 0
   FX1_CHECK_EQ(ow(322), 1280);
   FX1_CHECK_EQ(ow(323), 3840);  // stable at 321, published at 322
   FX1_CHECK_EQ(cu.resizer_published(323).scaling ? 1 : 0, 0);
   {
      control_unit b;  // the same, with a SOF after the bypass deadline
      wr(b, FX1_ISP_COMMON_FRAME_WIDTH_OFFSET, 3840, 10);
      wr(b, FX1_ISP_COMMON_FRAME_HEIGHT_OFFSET, 2160, 11);
      wr(b, FX1_ISP_RESIZER_CTRL_OFFSET, en | (FX1_ISP_RESIZER_CTRL_SCALE_RES_1280X720 << sh), 20);
      wr(b, FX1_ISP_RESIZER_CTRL_OFFSET, en | (FX1_ISP_RESIZER_CTRL_SCALE_RES_640X360 << sh), 100);
      wr(b, FX1_ISP_RESIZER_CTRL_OFFSET, FX1_ISP_RESIZER_CTRL_SCALE_RES_640X360 << sh, 120);
      b.accepted_sof(122);
      FX1_CHECK_EQ(b.resizer_frame().scaling ? 1 : 0, 1);  // the bypass is published on 122 itself
      b.accepted_sof(123);
      FX1_CHECK_EQ(b.resizer_frame().scaling ? 1 : 0, 0);
      FX1_CHECK_EQ(b.resizer_frame().out.width, 3840);
   }
   FX1_CHECK_EQ(ow(337), 3840);  // the derivation ended at 336: its stale 640 x 360 is dropped
   FX1_CHECK_EQ(ow(400), 3840);
   // Oversized and reserved codes: invalid, published on the next edge (no
   // division); the last valid readback (3840 x 2160) stays.
   const auto gs = [&](std::uint64_t c) { return cu.csr_read(FX1_ISP_CH0_GEOMETRY_STATUS_OFFSET, c); };
   wr(cu, FX1_ISP_COMMON_FRAME_WIDTH_OFFSET, 2688, 400);
   wr(cu, FX1_ISP_COMMON_FRAME_HEIGHT_OFFSET, 1520, 401);
   wr(cu, FX1_ISP_RESIZER_CTRL_OFFSET, en | (FX1_ISP_RESIZER_CTRL_SCALE_RES_3840X2160 << sh), 402);
   FX1_CHECK_EQ(gs(404), FX1_ISP_CH0_GEOMETRY_STATUS_BUSY_MASK);
   FX1_CHECK_EQ(gs(405), unsigned{geo_exceeds_crop} << FX1_ISP_CH0_GEOMETRY_STATUS_ERROR_SHIFT);
   FX1_CHECK_EQ(ow(405), 3840);
   FX1_CHECK_EQ(cu.resizer_published(405).valid ? 1 : 0, 0);
   wr(cu, FX1_ISP_RESIZER_CTRL_OFFSET, en | (0xFu << sh), 410);
   FX1_CHECK_EQ(gs(413), unsigned{geo_mode} << FX1_ISP_CH0_GEOMETRY_STATUS_ERROR_SHIFT);
   FX1_CHECK_EQ(ow(413), 3840);
   wr(cu, FX1_ISP_RESIZER_CTRL_OFFSET, 0, 414);  // FullRes: 2688 x 1520 on the next edge
   FX1_CHECK_EQ(ow(417), 2688);
   // The Bayer trim is part of the key (active size).
   wr(cu, FX1_ISP_COMMON_BAYER_OFFSET, FX1_ISP_COMMON_BAYER_PATTERN_BGGR, 420);
   FX1_CHECK_EQ(ow(422), 2688);
   FX1_CHECK_EQ(ow(423), 2686);
   FX1_CHECK_EQ(oh(423), 1518);
   // `updated` has no effect.
   wr(cu, FX1_ISP_RESIZER_CTRL_OFFSET, en | FX1_ISP_RESIZER_CTRL_UPDATED_MASK |
                                           (FX1_ISP_RESIZER_CTRL_SCALE_RES_1920X1080 << sh), 500);
   FX1_CHECK_EQ(ow(536), 2686);
   FX1_CHECK_EQ(ow(537), 1920);
   // COMMON soft reset: the reset set, then a derivation from the live
   // configuration after the 15-cycle pulse; a frame started earlier is not scaled.
   wr(cu, FX1_ISP_COMMON_CTRL_OFFSET, FX1_ISP_COMMON_CTRL_SOFT_RST_MASK, 600);
   FX1_CHECK_EQ(ow(601), 0);  // the reset readback (CSR v1.07)
   FX1_CHECK_EQ(oh(601), 0);
   FX1_CHECK_EQ(cu.resizer_published(601).key.valid ? 1 : 0, 0);
   cu.accepted_sof(605);
   FX1_CHECK_EQ(cu.resizer_frame().key.valid ? 1 : 0, 0);
   const std::uint64_t r0 = 600 + 15;  // E0 at the end of the pulse
   FX1_CHECK_EQ(ow(r0 + 35), 0);
   FX1_CHECK_EQ(cu.resizer_published(r0 + 35).key.valid ? 1 : 0, 0);
   FX1_CHECK_EQ(cu.resizer_published(r0 + 36).key.valid ? 1 : 0, 1);
   FX1_CHECK_EQ(cu.resizer_published(r0 + 36).key.width, 2686);
   // The DMA-only soft reset leaves the Resizer alone.
   wr(cu, FX1_ISP_DMA_CTRL_OFFSET, FX1_ISP_DMA_CTRL_SOFT_RESET_MASK, 700);
   FX1_CHECK_EQ(cu.resizer_published(701).key.valid ? 1 : 0, 1);
   FX1_CHECK_EQ(ow(701), 1920);
   // A settled (debug) read shows what will be published.
   {
      // Two writes before one core edge give one key: only the last one is
      // derived (review U4-R3); E0 on the next edge, published at E35.
      control_unit d;
      wr(d, FX1_ISP_COMMON_FRAME_WIDTH_OFFSET, 3840, 10);
      wr(d, FX1_ISP_COMMON_FRAME_HEIGHT_OFFSET, 2160, 11);
      wr(d, FX1_ISP_RESIZER_CTRL_OFFSET, en | (FX1_ISP_RESIZER_CTRL_SCALE_RES_1280X720 << sh), 100);
      wr(d, FX1_ISP_RESIZER_CTRL_OFFSET, en | (FX1_ISP_RESIZER_CTRL_SCALE_RES_640X480 << sh), 100);
      FX1_CHECK_EQ(d.csr_read(FX1_ISP_RESIZER_OUT_W_OFFSET, 136), 3840);
      FX1_CHECK_EQ(d.csr_read(FX1_ISP_RESIZER_OUT_W_OFFSET, 137), 640);
      // A soft reset in the middle of a derivation: the derivation restarts
      // from the live key at the end of the pulse (MAS Table 124). A geometry
      // write inside the pulse is an IDLE-class write outside IDLE: rejected
      // with MODE, the register unchanged (V107-7).
      wr(d, FX1_ISP_RESIZER_CTRL_OFFSET, en | (FX1_ISP_RESIZER_CTRL_SCALE_RES_960X540 << sh), 190);  // E0 191
      wr(d, FX1_ISP_COMMON_CTRL_OFFSET, FX1_ISP_COMMON_CTRL_SOFT_RST_MASK, 200);  // pulse to 215
      wr(d, FX1_ISP_RESIZER_CTRL_OFFSET, en | (FX1_ISP_RESIZER_CTRL_SCALE_RES_1280X720 << sh), 205);
      FX1_CHECK_EQ(d.csr_read(FX1_ISP_RESIZER_CTRL_OFFSET, 206) >> sh & 0x1Fu,
                   FX1_ISP_RESIZER_CTRL_SCALE_RES_960X540);
      FX1_CHECK_EQ(d.csr_read(FX1_ISP_TUNE_ERROR_OFFSET) & FX1_ISP_TUNE_ERROR_MODE_MASK,
                   FX1_ISP_TUNE_ERROR_MODE_MASK);
      d.csr_write(FX1_ISP_TUNE_ERROR_OFFSET, FX1_ISP_TUNE_ERROR_MODE_MASK, 0xFFFFFFFFu, 207);
      FX1_CHECK_EQ(d.resizer_published(250).key.valid ? 1 : 0, 0);
      FX1_CHECK_EQ(d.csr_read(FX1_ISP_RESIZER_OUT_W_OFFSET, 251), 960);  // E0 215, E35 250
      FX1_CHECK_EQ(d.resizer_published(251).key.valid ? 1 : 0, 1);
      // Worst case for software: a key changed just after E0 of a running
      // derivation is published within two derivations of the change.
      wr(d, FX1_ISP_RESIZER_CTRL_OFFSET, en | (FX1_ISP_RESIZER_CTRL_SCALE_RES_1280X720 << sh), 300);  // E0 301
      wr(d, FX1_ISP_RESIZER_CTRL_OFFSET, en | (FX1_ISP_RESIZER_CTRL_SCALE_RES_640X360 << sh), 302);
      FX1_CHECK_EQ(d.csr_read(FX1_ISP_RESIZER_OUT_W_OFFSET, 371), 960);  // 336 stale, then E0 336, E35 371
      FX1_CHECK_EQ(d.csr_read(FX1_ISP_RESIZER_OUT_W_OFFSET, 372), 640);
      FX1_CHECK(372 - 302 <= 2u * (control_unit::resizer_derivation_cycles + 1));
      // A pass-through key is not published before the pulse ends: the reset
      // set stays until then.
      wr(d, FX1_ISP_RESIZER_CTRL_OFFSET, FX1_ISP_RESIZER_CTRL_SCALE_RES_640X360 << sh, 390);  // EN = 0
      wr(d, FX1_ISP_COMMON_CTRL_OFFSET, FX1_ISP_COMMON_CTRL_SOFT_RST_MASK, 400);  // pulse to 415
      FX1_CHECK_EQ(d.resizer_published(416).key.valid ? 1 : 0, 0);
      FX1_CHECK_EQ(d.resizer_published(417).key.valid ? 1 : 0, 1);  // E0 415, published 416
      FX1_CHECK_EQ(d.csr_read(FX1_ISP_RESIZER_OUT_W_OFFSET, 417), 3840);
   }
   wr(cu, FX1_ISP_RESIZER_CTRL_OFFSET, en | (FX1_ISP_RESIZER_CTRL_SCALE_RES_640X480 << sh), 800);
   FX1_CHECK_EQ(rd(cu, FX1_ISP_RESIZER_OUT_W_OFFSET), 640);
   FX1_CHECK_EQ(ow(801), 1920);
}

// Channel geometry (MAS v1.07 §6.20.1.1, §8.3; CSR v1.07): reset readback,
// per-channel crop validation with the ERROR priority codes, ACTIVE_CROP and
// the last valid readback, OUTPUT_LIMIT, the CH2 code set, the sticky
// TUNE_ERROR.INVALID of enabled channels only, the IDLE class and admission.
void test_channel_geometry() {
   struct chregs {
      std::uint32_t ctrl, org, size, status, aorg, asize, outw, outh, lim, rsz;
   };
   const chregs r[3] = {
      {FX1_ISP_CH0_CROP_CTRL_OFFSET, FX1_ISP_CH0_CROP_ORIGIN_OFFSET, FX1_ISP_CH0_CROP_SIZE_OFFSET,
       FX1_ISP_CH0_GEOMETRY_STATUS_OFFSET, FX1_ISP_CH0_ACTIVE_CROP_ORIGIN_OFFSET, FX1_ISP_CH0_ACTIVE_CROP_SIZE_OFFSET,
       FX1_ISP_RESIZER_OUT_W_OFFSET, FX1_ISP_RESIZER_OUT_H_OFFSET, FX1_ISP_CH0_OUTPUT_LIMIT_OFFSET,
       FX1_ISP_RESIZER_CTRL_OFFSET},
      {FX1_ISP_CH1_CROP_CTRL_OFFSET, FX1_ISP_CH1_CROP_ORIGIN_OFFSET, FX1_ISP_CH1_CROP_SIZE_OFFSET,
       FX1_ISP_CH1_GEOMETRY_STATUS_OFFSET, FX1_ISP_CH1_ACTIVE_CROP_ORIGIN_OFFSET, FX1_ISP_CH1_ACTIVE_CROP_SIZE_OFFSET,
       FX1_ISP_CH1_RESIZER_OUT_W_OFFSET, FX1_ISP_CH1_RESIZER_OUT_H_OFFSET, FX1_ISP_CH1_OUTPUT_LIMIT_OFFSET,
       FX1_ISP_CH1_RESIZER_CTRL_OFFSET},
      {FX1_ISP_CH2_CROP_CTRL_OFFSET, FX1_ISP_CH2_CROP_ORIGIN_OFFSET, FX1_ISP_CH2_CROP_SIZE_OFFSET,
       FX1_ISP_CH2_GEOMETRY_STATUS_OFFSET, FX1_ISP_CH2_ACTIVE_CROP_ORIGIN_OFFSET, FX1_ISP_CH2_ACTIVE_CROP_SIZE_OFFSET,
       FX1_ISP_CH2_RESIZER_OUT_W_OFFSET, FX1_ISP_CH2_RESIZER_OUT_H_OFFSET, FX1_ISP_CH2_OUTPUT_LIMIT_OFFSET,
       FX1_ISP_CH2_RESIZER_CTRL_OFFSET},
   };
   constexpr std::uint32_t valid = FX1_ISP_CH0_GEOMETRY_STATUS_VALID_MASK, busy = FX1_ISP_CH0_GEOMETRY_STATUS_BUSY_MASK;
   const auto err = [](unsigned code) { return code << FX1_ISP_CH0_GEOMETRY_STATUS_ERROR_SHIFT; };
   const auto xy = [](std::uint32_t x, std::uint32_t y) { return x | (y << 16); };
   control_unit cu;
   std::uint64_t t = 10;
   // A pass-through or invalid key is visible three edges after its write.
   const auto w = [&](std::uint32_t off, std::uint32_t v) {
      cu.csr_write(off, v, all_lanes, t);
      t += 3;
   };
   const auto rd_at = [&](std::uint32_t off) { return cu.csr_read(off, t); };
   const auto tune_err = [&] { return cu.csr_bus_read(FX1_ISP_TUNE_ERROR_OFFSET, t++); };
   // Reset: readback zero, BUSY until the reset key (1920 x 1080 FullRes)
   // publishes on the next edge; CH2 then exceeds its 640 x 640 limit.
   for (unsigned ch = 0; ch < 3; ++ch) {
      const std::string c = "CH" + std::to_string(ch) + " reset";
      FX1_CHECK_EQ_CTX(cu.csr_read(r[ch].status, 0), busy, c);
      FX1_CHECK_EQ_CTX(cu.csr_read(r[ch].outw, 0), 0u, c);
      FX1_CHECK_EQ_CTX(cu.csr_read(r[ch].asize, 0), 0u, c);
      FX1_CHECK_EQ_CTX(cu.csr_read(r[ch].status, 2), ch == 2 ? err(geo_limit) : valid, c + ", one edge later");
      FX1_CHECK_EQ_CTX(cu.csr_read(r[ch].outw, 2), ch == 2 ? 0u : 1920u, c + ", one edge later");
   }
   FX1_CHECK_EQ(rd(cu, r[0].lim), xy(3840, 2160));
   FX1_CHECK_EQ(rd(cu, r[1].lim), xy(3840, 2160));
   FX1_CHECK_EQ(rd(cu, r[2].lim), xy(640, 640));
   FX1_CHECK_EQ(tune_err(), 0u);  // CH2 is not enabled: its invalid reset set raises nothing
   w(FX1_ISP_COMMON_FRAME_WIDTH_OFFSET, 704);
   w(FX1_ISP_COMMON_FRAME_HEIGHT_OFFSET, 672);
   // CH1 crop: half-open, even, inside the image.
   w(r[1].org, xy(32, 16));
   w(r[1].size, xy(640, 640));
   w(r[1].ctrl, 1);
   FX1_CHECK_EQ(rd_at(r[1].status), valid);
   FX1_CHECK_EQ(rd_at(r[1].aorg), xy(32, 16));
   FX1_CHECK_EQ(rd_at(r[1].asize), xy(640, 640));
   FX1_CHECK_EQ(rd_at(r[1].outw), 640u);
   FX1_CHECK_EQ(rd_at(r[0].asize), xy(704, 672));  // CH0 keeps the full image
   w(r[1].org, xy(33, 16));                          // odd origin
   FX1_CHECK_EQ(rd_at(r[1].status), err(geo_crop_odd));
   FX1_CHECK_EQ(rd_at(r[1].aorg), xy(32, 16));       // the last valid readback stays
   w(r[1].org, xy(65, 16));                          // odd and out of bounds: 2 wins
   FX1_CHECK_EQ(rd_at(r[1].status), err(geo_crop_bounds));
   w(r[1].org, xy(64, 32));                          // 64 + 640 = 704, 32 + 640 = 672: on the edge
   FX1_CHECK_EQ(rd_at(r[1].status), valid);
   w(r[1].size, 0);                                  // enabled zero size
   FX1_CHECK_EQ(rd_at(r[1].status), err(geo_crop_bounds));
   w(r[1].ctrl, 0);                                  // disabled: the full image, size ignored
   FX1_CHECK_EQ(rd_at(r[1].status), valid);
   FX1_CHECK_EQ(rd_at(r[1].asize), xy(704, 672));
   w(r[1].org, 0);
   w(r[1].size, xy(320, 320));
   w(r[1].ctrl, 1);
   w(r[1].rsz, 1 | (8u << FX1_ISP_RESIZER_CTRL_SCALE_SHIFT));  // 640 x 360 from 320 x 320: no upscaling
   FX1_CHECK_EQ(rd_at(r[1].status), err(geo_exceeds_crop));
   w(r[1].org, 1);                                   // odd crop and reserved code: 3 wins over 4
   w(r[1].rsz, 1 | (15u << FX1_ISP_RESIZER_CTRL_SCALE_SHIFT));
   FX1_CHECK_EQ(rd_at(r[1].status), err(geo_crop_odd));
   w(r[1].org, 0);
   FX1_CHECK_EQ(rd_at(r[1].status), err(geo_mode));
   w(r[1].rsz, 15u << FX1_ISP_RESIZER_CTRL_SCALE_SHIFT);  // scaler disabled: the code is ignored
   FX1_CHECK_EQ(rd_at(r[1].status), valid);
   FX1_CHECK_EQ(rd_at(r[1].outw), 320u);
   // CH2: FullRes must fit 640 x 640; codes 0, 8, 14, 16..21 only.
   FX1_CHECK_EQ(rd_at(r[2].status), err(geo_limit));  // 704 x 672 full image
   w(r[2].size, xy(640, 640));
   w(r[2].ctrl, 1);
   FX1_CHECK_EQ(rd_at(r[2].status), valid);
   w(r[2].rsz, 1 | (5u << FX1_ISP_RESIZER_CTRL_SCALE_SHIFT));  // FHD: not a CH2 code
   FX1_CHECK_EQ(rd_at(r[2].status), err(geo_mode));
   for (const std::uint32_t code : {8u, 14u, 16u, 17u, 18u, 19u, 20u, 21u}) {
      cu.csr_write(r[2].rsz, 1 | (code << FX1_ISP_RESIZER_CTRL_SCALE_SHIFT), all_lanes, t);
      t += 40;  // a scaling key: E0 .. E35
      FX1_CHECK_EQ_CTX(rd_at(r[2].status), valid, "CH2 code " + std::to_string(code));
   }
   FX1_CHECK_EQ(rd_at(r[2].outw), 320u);
   // Sticky INVALID for enabled channels: CH2 invalid and disabled raises
   // nothing; enabling it raises INVALID; ERROR is level, INVALID sticky.
   w(r[2].rsz, 1 | (5u << FX1_ISP_RESIZER_CTRL_SCALE_SHIFT));
   FX1_CHECK_EQ(tune_err(), 0u);
   w(FX1_ISP_OUTPUT_ENABLE_OFFSET, 5);
   FX1_CHECK_EQ(tune_err() & FX1_ISP_TUNE_ERROR_INVALID_MASK, FX1_ISP_TUNE_ERROR_INVALID_MASK);
   FX1_CHECK(cu.admission_state(t, 0) == control_unit::admission::wait_event);  // CH2 enabled, invalid
   w(FX1_ISP_TUNE_ERROR_OFFSET, FX1_ISP_TUNE_ERROR_INVALID_MASK);
   w(r[2].rsz, 0);  // CH2 FullRes 640 x 640: valid
   FX1_CHECK_EQ(rd_at(r[2].status), valid);
   FX1_CHECK_EQ(tune_err() & FX1_ISP_TUNE_ERROR_INVALID_MASK, 0u);
   FX1_CHECK(cu.admission_state(t, 0) == control_unit::admission::go);
   w(r[2].size, xy(642, 640));  // enabled and invalid: INVALID on its publication
   FX1_CHECK_EQ(rd_at(r[2].status), err(geo_limit));
   FX1_CHECK_EQ(tune_err() & FX1_ISP_TUNE_ERROR_INVALID_MASK, FX1_ISP_TUNE_ERROR_INVALID_MASK);
   w(r[2].size, xy(640, 640));
   // A scaling key: BUSY for its derivation, admission waits edge by edge.
   cu.csr_write(r[0].rsz, 1 | (FX1_ISP_RESIZER_CTRL_SCALE_RES_640X480 << FX1_ISP_RESIZER_CTRL_SCALE_SHIFT), all_lanes, t);
   FX1_CHECK_EQ(cu.csr_read(r[0].status, t + 2), busy);
   FX1_CHECK(cu.admission_state(t + 2, 0) == control_unit::admission::wait_edge);
   FX1_CHECK_EQ(cu.csr_read(r[0].status, t + 37), valid);  // E0 t + 1, E35 t + 36
   FX1_CHECK(cu.admission_state(t + 37, 0) == control_unit::admission::go);
   t += 40;
   // IDLE class: not idle (input enabled), a geometry write is rejected with
   // MODE and changes nothing.
   w(FX1_ISP_COMMON_CTRL_OFFSET, FX1_ISP_COMMON_CTRL_ISP_EN_MASK);
   w(FX1_ISP_DMA_CTRL_OFFSET, FX1_ISP_DMA_CTRL_IDMA_EN_MASK);
   (void)tune_err();
   w(r[1].org, xy(2, 2));
   FX1_CHECK_EQ(tune_err() & FX1_ISP_TUNE_ERROR_MODE_MASK, FX1_ISP_TUNE_ERROR_MODE_MASK);
   FX1_CHECK_EQ(rd(cu, r[1].org), 0u);
}

// The frame scales only with a set derived for its own raster (U4-A1): a frame
// started before the derivation of a new geometry finished passes unscaled.
void test_resizer_frame_raster() {
   control_unit cu;
   wr(cu, FX1_ISP_COMMON_FRAME_WIDTH_OFFSET, 1280, 1);
   wr(cu, FX1_ISP_COMMON_FRAME_HEIGHT_OFFSET, 720, 2);
   wr(cu, FX1_ISP_RESIZER_CTRL_OFFSET,
      FX1_ISP_RESIZER_CTRL_EN_MASK | (FX1_ISP_RESIZER_CTRL_SCALE_RES_640X360 << FX1_ISP_RESIZER_CTRL_SCALE_SHIFT), 3);
   cu.accepted_sof(100);  // published: 1280 x 720 -> 640 x 360
   pipe::pipeline_config cfg = pipe::snapshot_config(cu);
   const pipe::frame_geometry same = pipe::output_geometry(cfg, 1280, 720);
   FX1_CHECK_EQ(same.width, 640);
   FX1_CHECK_EQ(same.height, 360);
   const pipe::frame_geometry other = pipe::output_geometry(cfg, 800, 600);  // raster of another key
   FX1_CHECK_EQ(other.width, 800);
   FX1_CHECK_EQ(other.height, 600);
   // After a COMMON soft reset, before the derivation: the reset set scales nothing.
   wr(cu, FX1_ISP_COMMON_CTRL_OFFSET, FX1_ISP_COMMON_CTRL_SOFT_RST_MASK, 200);
   cu.accepted_sof(210);
   cfg = pipe::snapshot_config(cu);
   const pipe::frame_geometry early = pipe::output_geometry(cfg, 1280, 720);
   FX1_CHECK_EQ(early.width, 1280);
   FX1_CHECK_EQ(early.height, 720);
}

// CCM and CNF configuration as the frame samples it (no commit gate since 07/10).
void test_commit_gates() {
   control_unit cu;
   // CCM and CNF have no gate (owner Q1, DEC-24/27 superseded): the value the
   // pipeline samples at SOF is the current register value, `updated` or not.
   wr(cu, FX1_ISP_CCM_CRR_OFFSET, 0x100, 11);
   FX1_CHECK_EQ(cu.committed(FX1_ISP_CCM_CRR_OFFSET), 0x100);
   wr(cu, FX1_ISP_CNF_LUMA_TH_OFFSET, 20, 14);
   FX1_CHECK_EQ(cu.committed(FX1_ISP_CNF_LUMA_TH_OFFSET), 20);
   // Soft reset preserves configuration.
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
   // LUTs: undefined after i_rst_n (MAS §6.5.2), the deterministic poison
   // pattern of the model and the reference (CSR-16 superseded).
   FX1_CHECK_EQ(cu.gamma_lut()[0], 0xA5A5u & 0xFFFu);
   FX1_CHECK_EQ(cu.gamma_lut()[1], (0x9E37u ^ 0xA5A5u) & 0xFFFu);
   FX1_CHECK_EQ(cu.ee_table(0)[0], 0xA5A5u);
   FX1_CHECK_EQ(cu.ee_table(1)[0], ((64u * 0x9E37u) ^ 0xA5A5u) & 0xFFFFu);
   FX1_CHECK(!cu.soft_reset_active(51));
   wr(cu, FX1_ISP_IDMA_BUF_VALID_OFFSET, 0x2, 51);  // command block lifted by i_rst_n
   FX1_CHECK_EQ(rd(cu, FX1_ISP_IDMA_BUF_VALID_OFFSET), 0x2);
}

}  // namespace

// FRAME_COUNTER (MAS Table 160): core clocks since the release of i_rst_n,
// modulo 2^32, computed at the read edge; zero while i_rst_n is asserted;
// unaffected by soft resets (VP decision U5-A1).
void test_time_base() {
   control_unit cu;
   FX1_CHECK_EQ(cu.csr_read(FX1_ISP_FRAME_COUNTER_OFFSET, 0), 0u);
   FX1_CHECK_EQ(cu.csr_read(FX1_ISP_FRAME_COUNTER_OFFSET, 12345), 12345u);
   FX1_CHECK_EQ(cu.csr_read(FX1_ISP_FRAME_COUNTER_OFFSET, (1ull << 32) + 7), 7u);  // wraps
   FX1_CHECK_EQ(cu.csr_read(FX1_ISP_FRAME_COUNTER_OFFSET, (1ull << 32) - 1), 0xFFFFFFFFu);
   // A settled read sees the last edge the control unit processed.
   cu.csr_write(FX1_ISP_COMMON_SCRATCH_OFFSET, 1, 0xFFFFFFFFu, 500);
   FX1_CHECK_EQ(cu.csr_read(FX1_ISP_FRAME_COUNTER_OFFSET), 500u);
   // Soft resets do not restart it.
   cu.csr_write(FX1_ISP_DMA_CTRL_OFFSET, FX1_ISP_DMA_CTRL_SOFT_RESET_MASK, 0xFFFFFFFFu, 600);
   FX1_CHECK_EQ(cu.csr_read(FX1_ISP_FRAME_COUNTER_OFFSET, 700), 700u);
   cu.csr_write(FX1_ISP_COMMON_CTRL_OFFSET, FX1_ISP_COMMON_CTRL_SOFT_RST_MASK, 0xFFFFFFFFu, 800);
   FX1_CHECK_EQ(cu.csr_read(FX1_ISP_FRAME_COUNTER_OFFSET, 900), 900u);
   // i_rst_n: zero while asserted, then counting from the release edge.
   cu.external_reset();
   FX1_CHECK_EQ(cu.csr_read(FX1_ISP_FRAME_COUNTER_OFFSET, 1000), 0u);
   FX1_CHECK_EQ(cu.frame_counter(5000), 0u);
   cu.release_reset(2000);
   FX1_CHECK_EQ(cu.frame_counter(1999), 0u);
   FX1_CHECK_EQ(cu.frame_counter(2000), 0u);
   FX1_CHECK_EQ(cu.frame_counter(2001), 1u);
   FX1_CHECK_EQ(cu.csr_read(FX1_ISP_FRAME_COUNTER_OFFSET, 2000 + (1ull << 32) + 3), 3u);
   // Read-only: the stored word does not change the count.
   cu.csr_write(FX1_ISP_FRAME_COUNTER_OFFSET, 0xFFFFFFFFu, 0xFFFFFFFFu, 2100);
   FX1_CHECK_EQ(cu.csr_read(FX1_ISP_FRAME_COUNTER_OFFSET, 2100), 100u);
   FX1_CHECK_EQ(cu.regs().peek(FX1_ISP_FRAME_COUNTER_OFFSET), 0u);
}

int main() {
   test_time_base();
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
   test_aec_commit_across_soft_reset();
   test_frame_start_and_done();
   test_table_confirmation();
   test_ingress_sequence();
   test_commands_recorded();
   test_commands_cleared_on_reset();
   test_gamma_lut_port();
   test_ee_lut_port();
   test_ee_profiles();
   test_ee_radial_error();
   test_gtm_lut_gate();
   test_gtm_capture_at_sof();
   test_resizer_geometry();
   test_resizer_publication();
   test_resizer_frame_raster();
   test_channel_geometry();
   test_recovery();
   test_commit_gates();
   test_lsc_resize_during_load();
   test_lsc_loader();
   test_lsc_selection_at_sof();
   test_external_reset();
   return fx1_test::summary("fx1_isp_test_control");
}
