// Author: hoangv11

#include "trng_model.h"

#include <cstdlib>

namespace {

constexpr uint32_t REG_IMR = 0x100;
constexpr uint32_t REG_ISR = 0x104;
constexpr uint32_t REG_ICR = 0x108;
constexpr uint32_t REG_CONFIG = 0x10C;
constexpr uint32_t REG_VALID = 0x110;
constexpr uint32_t REG_EHR_DATA0 = 0x114;
constexpr uint32_t REG_EHR_DATA1 = 0x118;
constexpr uint32_t REG_EHR_DATA2 = 0x11C;
constexpr uint32_t REG_EHR_DATA3 = 0x120;
constexpr uint32_t REG_EHR_DATA4 = 0x124;
constexpr uint32_t REG_EHR_DATA5 = 0x128;
constexpr uint32_t REG_SRC_EN = 0x12C;
constexpr uint32_t REG_SAMPLE_CNT1 = 0x130;
constexpr uint32_t REG_AUTOCORR_STAT = 0x134;
constexpr uint32_t REG_DBG_CONTROL = 0x138;
constexpr uint32_t REG_SW_RESET = 0x140;
constexpr uint32_t REG_BUSY = 0x1B8;
constexpr uint32_t REG_RESET_BITS_COUNTER = 0x1BC;
constexpr uint32_t REG_BIST_CNTR0 = 0x1e0;
constexpr uint32_t REG_BIST_CNTR1 = 0x1e4;
constexpr uint32_t REG_BIST_CNTR2 = 0x1e8;

} // namespace

TRNG_Model::TRNG_Model() {
   reset();
}

void TRNG_Model::reset() {
   reg_imr = 0xf;
   reg_isr = 0;
   reg_icr = 0;
   reg_config = 0;
   reg_valid = 0;
   reg_ehr_data[0] = 0;
   reg_ehr_data[1] = 0;
   reg_ehr_data[2] = 0;
   reg_ehr_data[3] = 0;
   reg_ehr_data[4] = 0;
   reg_ehr_data[5] = 0;
   reg_src_en = 0;
   reg_sample_cnt1 = 0xffff;
   reg_autocorr_stat = 0;
   reg_dbg_control = 0;
   reg_sw_reset = 0;
   reg_busy = 0;
   reg_reset_bits_counter = 0;
   reg_bist_cntr[0] = 0;
   reg_bist_cntr[1] = 0;
   reg_bist_cntr[2] = 0;
}

uint32_t TRNG_Model::readReg(uint32_t offset) {
   switch (offset) {
   case REG_IMR:
      return reg_imr;
   case REG_ISR:
      return reg_isr;
   case REG_ICR: // WO
      return 0;
   case REG_CONFIG:
      return reg_config;
   case REG_VALID:
      return reg_valid;
   case REG_EHR_DATA0:
      return reg_ehr_data[0];
   case REG_EHR_DATA1:
      return reg_ehr_data[1];
   case REG_EHR_DATA2:
      return reg_ehr_data[2];
   case REG_EHR_DATA3:
      return reg_ehr_data[3];
   case REG_EHR_DATA4:
      return reg_ehr_data[4];
   case REG_EHR_DATA5:
      return reg_ehr_data[5];
   case REG_SRC_EN:
      return reg_src_en;
   case REG_SAMPLE_CNT1:
      return reg_sample_cnt1;
   case REG_AUTOCORR_STAT:
      return reg_autocorr_stat;
   case REG_DBG_CONTROL:
      return reg_dbg_control;
   case REG_SW_RESET: // WO
      return 0;
   case REG_BUSY:
      return reg_busy;
   case REG_RESET_BITS_COUNTER: // WO
      return 0;
   case REG_BIST_CNTR0:
      return reg_bist_cntr[0];
   case REG_BIST_CNTR1:
      return reg_bist_cntr[1];
   case REG_BIST_CNTR2:
      return reg_bist_cntr[2];

   default:
      return 0;
   }
}

void TRNG_Model::writeReg(uint32_t offset, uint32_t data) {
   switch (offset) {
   case REG_IMR:
      reg_imr = data & 0xfu;
      return;
   case REG_ISR: // RO
      return;
   case REG_ICR: // WO
      reg_icr = data & 0xfu;
      reg_isr &= ~reg_icr;
      if (data & 0x1u) {
         reg_valid = 0u;
      }
      return;
   case REG_CONFIG:
      reg_config = data & 0x3u;
      return;
   case REG_VALID: // RO
      return;
   case REG_EHR_DATA0: // RO
      return;
   case REG_EHR_DATA1: // RO
      return;
   case REG_EHR_DATA2: // RO
      return;
   case REG_EHR_DATA3: // RO
      return;
   case REG_EHR_DATA4: // RO
      return;
   case REG_EHR_DATA5: // RO
      return;
   case REG_SRC_EN:
      reg_src_en = data & 0x1u;
      if (reg_src_en & 0x1u) {
         // generate random data
         for (int i = 0; i < 6; ++i) {
            reg_ehr_data[i] = rand();
         }
         reg_valid |= 0x1u;
         reg_isr |= 0x1u;
      }
      return;
   case REG_SAMPLE_CNT1:
      reg_sample_cnt1 = data;
      return;
   case REG_AUTOCORR_STAT:
      reg_autocorr_stat = 0x0u;
      return;
   case REG_DBG_CONTROL: // RO
      return;
   case REG_SW_RESET: // WO
      if (data == 0x1u)
         reset();
      return;
   case REG_BUSY: // RO
      return;
   case REG_RESET_BITS_COUNTER: // WO
      if (!(reg_src_en & 0x1u)) {
         reg_valid &= 0x0u;
         reg_isr &= ~0x1u;
      }
      return;
   case REG_BIST_CNTR0: // RO
      return;
   case REG_BIST_CNTR1: // RO
      return;
   case REG_BIST_CNTR2: // RO
      return;
   default:
      break;
   }
}

bool TRNG_Model::hasInterrupt() const {
   return ((reg_isr & ~reg_imr) & 0xfu) != 0u;
}

uint32_t TRNG_Model::debugReadReg(uint32_t offset) const {
   switch (offset) {
   case REG_IMR:
      return reg_imr;
   case REG_ISR:
      return reg_isr;
   case REG_ICR: // WO
      return 0;
   case REG_CONFIG:
      return reg_config;
   case REG_VALID:
      return reg_valid;
   case REG_EHR_DATA0:
      return reg_ehr_data[0];
   case REG_EHR_DATA1:
      return reg_ehr_data[1];
   case REG_EHR_DATA2:
      return reg_ehr_data[2];
   case REG_EHR_DATA3:
      return reg_ehr_data[3];
   case REG_EHR_DATA4:
      return reg_ehr_data[4];
   case REG_EHR_DATA5:
      return reg_ehr_data[5];
   case REG_SRC_EN:
      return reg_src_en;
   case REG_SAMPLE_CNT1:
      return reg_sample_cnt1;
   case REG_AUTOCORR_STAT:
      return reg_autocorr_stat;
   case REG_DBG_CONTROL:
      return reg_dbg_control;
   case REG_SW_RESET: // WO
      return 0;
   case REG_BUSY:
      return reg_busy;
   case REG_RESET_BITS_COUNTER: // WO
      return 0;
   case REG_BIST_CNTR0:
      return reg_bist_cntr[0];
   case REG_BIST_CNTR1:
      return reg_bist_cntr[1];
   case REG_BIST_CNTR2:
      return reg_bist_cntr[2];

   default:
      return 0;
   }
}

void TRNG_Model::debugWriteReg(uint32_t offset, uint32_t data) {
   switch (offset) {
   case REG_IMR:
      reg_imr = data & 0xfu;
      return;
   case REG_ISR: // RO
      return;
   case REG_ICR: // WO
      reg_icr = data & 0xfu;
      return;
   case REG_CONFIG:
      reg_icr = data & 0x3u;
      return;
   case REG_VALID: // RO
      return;
   case REG_EHR_DATA0: // RO
      return;
   case REG_EHR_DATA1: // RO
      return;
   case REG_EHR_DATA2: // RO
      return;
   case REG_EHR_DATA3: // RO
      return;
   case REG_EHR_DATA4: // RO
      return;
   case REG_EHR_DATA5: // RO
      return;
   case REG_SRC_EN:
      reg_src_en = data & 0x1u;
      return;
   case REG_SAMPLE_CNT1:
      reg_sample_cnt1 = data;
      return;
   case REG_AUTOCORR_STAT:
      reg_autocorr_stat = data;
      return;
   case REG_DBG_CONTROL: // RO
      return;
   case REG_SW_RESET: // WO
      reg_sw_reset = data & 0x1u;
      return;
   case REG_BUSY: // RO
      return;
   case REG_RESET_BITS_COUNTER: // WO
      reg_reset_bits_counter = data & 0x1u;
      return;
   case REG_BIST_CNTR0: // RO
      return;
   case REG_BIST_CNTR1: // RO
      return;
   case REG_BIST_CNTR2: // RO
      return;
   default:
      break;
   }
}
