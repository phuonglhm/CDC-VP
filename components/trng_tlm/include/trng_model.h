#ifndef TRNG_MODEL_H
#define TRNG_MODEL_H

#include <stdint.h>
#include <stdbool.h>

class TRNG_Model {
private:
   uint32_t reg_imr;                // 0x100
   uint32_t reg_isr;                // 0x104
   uint32_t reg_icr;                // 0x108
   uint32_t reg_config;             // 0x10C
   uint32_t reg_valid;              // 0x110
   uint32_t reg_ehr_data[6];        // 0x114-0x128
   uint32_t reg_src_en;             // 0x12C
   uint32_t reg_sample_cnt1;        // 0x130
   uint32_t reg_autocorr_stat;      // 0x134
   uint32_t reg_dbg_control;        // 0x138
   uint32_t reg_sw_reset;           // 0x140
   uint32_t reg_busy;               // 0x1B8
   uint32_t reg_reset_bits_counter; // 0x1BC
   uint32_t reg_bist_cntr[3];       // 0x1E0-0x1E8

public:
   TRNG_Model();
   void reset();
   uint32_t readReg(uint32_t offset);
   void writeReg(uint32_t offset, uint32_t data);
   bool hasInterrupt() const;

   uint32_t debugReadReg(uint32_t offset) const;
   void debugWriteReg(uint32_t offset, uint32_t data);
};

#endif
