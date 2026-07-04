/*
 * soc_regs_timer.h - Timer (PrimeCell-style) register map for VP_FX1.
 *
 * Register OFFSETS from a timer base (CDC_TIMER0_BASE / CDC_TIMER1_BASE).
 * Verified against the TLM model constants (components/timer_tlm) by
 * tools/check_regs_drift.cpp - do not edit offsets without updating the model.
 *
 * All accesses are 32-bit little-endian words.
 */
#ifndef CDC_SOC_REGS_TIMER_H
#define CDC_SOC_REGS_TIMER_H

/* ---- Register offsets ------------------------------------------------- */
#define CDC_TIMER_CTRL       0x00u  /* control (R/W)                        */
#define CDC_TIMER_VALUE      0x04u  /* current counter value (R/W)          */
#define CDC_TIMER_RELOAD     0x08u  /* reload value (R/W)                   */
#define CDC_TIMER_INTSTATUS  0x0Cu  /* interrupt status (W1C)               */

/* ---- CTRL bit-fields -------------------------------------------------- */
#define CDC_TIMER_CTRL_ENABLE   0x1u  /* counter enable                     */
#define CDC_TIMER_CTRL_EX_EN    0x2u  /* external-input enable              */
#define CDC_TIMER_CTRL_EX_CLK   0x4u  /* external clock select             */
#define CDC_TIMER_CTRL_INTR_EN  0x8u  /* interrupt enable                   */

#endif /* CDC_SOC_REGS_TIMER_H */
