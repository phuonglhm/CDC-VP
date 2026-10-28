#pragma once
/*
 * FU3_VP_ABI_1 - firmware interface of the FU3 virtual platform beyond the
 * IP register maps (approved 2026-10-26, plan/De_Xuat_Chot_Spec_Ban_Giao_FW_SW.md
 * with the review amendments). C header shared by the platform model and
 * firmware. These registers are a VP baseline: FU3_SYSCSR_v1 (RTL) does not
 * define them yet; see doc/FU3_VP_ABI_1.md for the VP/RTL differences.
 *
 * Extension window in the SYSCSR aperture (0x4401_0000), offsets 0x100-0x123.
 * 32-bit registers; byte enables select lanes, a zero-strobe write has no
 * effect, reserved bits read 0 and ignore writes, debug (backdoor) reads have
 * no side effect.
 */
#include <stdint.h>

#define FU3_VP_ABI_VERSION_VALUE UINT32_C(0x00010000) /* 1.0 */

#define FU3_VP_ABI_SYSCSR_BASE     UINT32_C(0x44010000)
#define FU3_VP_ABI_VERSION_OFFSET  0x0100u /* RO */
#define FU3_VP_BOOT_STATUS_OFFSET  0x0104u /* RO */
#define FU3_VP_RESET_CAUSE_OFFSET  0x0108u /* R/W1C, POR-only state */
#define FU3_VP_RESERVED_10C_OFFSET 0x010Cu /* RAZ/WI */
#define FU3_VP_INTMAP_EN0_OFFSET   0x0110u /* RW, sources 0-31 */
#define FU3_VP_INTMAP_EN1_OFFSET   0x0114u /* RW, sources 32-33 (bits 0-1) */
#define FU3_VP_SWIRQ_PENDING_OFFSET 0x0118u /* RO */
#define FU3_VP_SWIRQ_SET_OFFSET    0x011Cu /* W1S, reads 0 */
#define FU3_VP_SWIRQ_CLEAR_OFFSET  0x0120u /* W1C, reads 0 */
#define FU3_VP_ABI_END_OFFSET      0x0124u

/* BOOT_STATUS: straps captured when the SYSCSR reset is released (invalid
 * while it is held), DEBUG_EN live (pad die 6). */
#define FU3_VP_BOOT_BOOT          (1u << 0)
#define FU3_VP_BOOT_MODE_0        (1u << 1)
#define FU3_VP_BOOT_MODE_1        (1u << 2)
#define FU3_VP_BOOT_TEST_MODE     (1u << 3)
#define FU3_VP_BOOT_TEST_MODE_SEL (1u << 4)
#define FU3_VP_BOOT_DEBUG_EN      (1u << 8)
#define FU3_VP_BOOT_STRAPS_VALID  (1u << 31)

/* RESET_CAUSE: cumulative bitmap until written 1 to clear; only POR clears the
 * state (POR sets bit 0). One event per reset, recorded when it is taken. */
#define FU3_VP_RESET_POR      (1u << 0)
#define FU3_VP_RESET_GLOBAL   (1u << 1)
#define FU3_VP_RESET_SOFTWARE (1u << 2)
#define FU3_VP_RESET_WATCHDOG (1u << 3)
#define FU3_VP_RESET_DEBUG    (1u << 4)
#define FU3_VP_RESET_MASK     UINT32_C(0x1f)

/* SWIRQ: CVA6 ipi_i (cause 3) and Ibex irq_software_i (cause 3, through the
 * INTMAP route FU3_VP_INTMAP_SRC_SW_IBEX). Level until cleared; SYSCSR reset clears. */
#define FU3_VP_SWIRQ_CVA6 (1u << 0)
#define FU3_VP_SWIRQ_IBEX (1u << 1)

/* INTMAP route enables (reset 0): bit s of EN0/EN1 enables INTMAP source s.
 * Source indices are this ABI's, not PLIC IDs or Ibex fast IRQ numbers. */
#define FU3_VP_INTMAP_SRC_WDT0      0u  /* -> fast[0] */
#define FU3_VP_INTMAP_SRC_CRM       1u  /* -> fast[1] */
#define FU3_VP_INTMAP_SRC_WAKE      2u  /* -> fast[2] */
#define FU3_VP_INTMAP_SRC_EDHOC     3u  /* -> fast[3] */
#define FU3_VP_INTMAP_SRC_AES       4u  /* -> fast[4] */
#define FU3_VP_INTMAP_SRC_RNG       5u  /* -> fast[5] */
#define FU3_VP_INTMAP_SRC_DMA0      6u  /* -> fast[6] */
#define FU3_VP_INTMAP_SRC_QSPI      7u  /* -> fast[7] */
#define FU3_VP_INTMAP_SRC_UART(n)   (8u + (n))  /* UART0-7 -> fast[8] */
#define FU3_VP_INTMAP_SRC_SPI(n)    (16u + (n)) /* SPI0-3 -> fast[9] */
#define FU3_VP_INTMAP_SRC_CAN       20u /* -> fast[10] */
#define FU3_VP_INTMAP_SRC_GPIO(n)   (21u + (n)) /* GPIOA/B/C -> fast[11] */
#define FU3_VP_INTMAP_SRC_I2C       24u /* -> fast[12] */
#define FU3_VP_INTMAP_SRC_ADC       25u /* -> fast[13] */
#define FU3_VP_INTMAP_SRC_PWM(n)    (26u + (n)) /* PWM0/1 -> fast[14] */
#define FU3_VP_INTMAP_SRC_TIMER2    28u /* -> irq_external_i */
#define FU3_VP_INTMAP_SRC_TIMER3    29u /* -> irq_external_i */
#define FU3_VP_INTMAP_SRC_WDT1      30u /* -> irq_external_i */
#define FU3_VP_INTMAP_SRC_MAILBOX   31u /* -> irq_external_i */
#define FU3_VP_INTMAP_SRC_SW_IBEX   32u /* -> irq_software_i */
#define FU3_VP_INTMAP_SRC_TIMER1    33u /* -> irq_timer_i */
#define FU3_VP_INTMAP_SOURCES       34u
#define FU3_VP_INTMAP_EN1_MASK      UINT32_C(0x3)
