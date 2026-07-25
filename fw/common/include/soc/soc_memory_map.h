/*
 * soc_memory_map.h - VP_FX1 Full SoC address map (firmware-facing API; pins the SoC address ABI).
 *
 * Single source of truth for peripheral bases and RAM buffer windows, matching
 * docs/peripheral_memory_map.md and platforms/VP_FX1_Full_SoC/configs/default.yaml.
 *
 * Rules for driver authors:
 *   - Never hard-code an address; always reference these macros.
 *   - All normal MMIO registers are 32-bit little-endian words.
 *   - ISP0/VPU0 remain reserved. NPU0 is optional and is reserved in the
 *     public default build.
 *
 * If this file and the VP disagree, the VP wins - regenerate from the memory map.
 */
#ifndef CDC_SOC_MEMORY_MAP_H
#define CDC_SOC_MEMORY_MAP_H

/* ---- System / interrupt controllers ---------------------------------- */
#define CDC_BOOTROM_BASE  0x00000000u  /* 64 KiB boot ROM, ROM-code entry 0x0 */
#define CDC_BOOTROM_SIZE  0x00010000u
#define CDC_CLINT_BASE    0x02000000u  /* MSIP/MTIP (local interrupts)      */
#define CDC_PLIC_BASE     0x0C000000u  /* external interrupt controller     */

/* ---- ROM-code boot flow (docs/romcode_boot_hw_plan.md) ---------------- */
/* Internal code flash: 4 MiB read-only XIP window. The ROM code jumps here
 * when the boot strap (GPIO0 pin 1) is LOW. Preloaded via --int-flash.    */
#define CDC_IFLASH_BASE   0x04000000u
#define CDC_IFLASH_SIZE   0x00400000u

/* ---- Peripheral instance 0 ------------------------------------------- */
#define CDC_UART0_BASE    0x10000000u  /* console UART  (uart2_tlm, PL011)  */
#define CDC_I2C0_BASE     0x10010000u
#define CDC_SPI0_BASE     0x10020000u
#define CDC_TIMER0_BASE   0x10030000u
#define CDC_WDT0_BASE     0x10040000u
#define CDC_PWM0_BASE     0x10050000u  /* no IRQ output in current model    */
#define CDC_DMA0_BASE     0x10060000u  /* MMIO + master into system bus     */
#define CDC_TRNG0_BASE    0x10070000u
#define CDC_CMU0_BASE     0x10080000u  /* clkmgr: no real gating, no IRQ    */
#define CDC_PMU0_BASE     0x10090000u
#define CDC_DMIC0_BASE    0x100A0000u
#define CDC_OTP0_BASE     0x100B0000u
#define CDC_QSPI0_BASE    0x100C0000u  /* NOR flash sits behind QSPI0       */

/* ---- Accelerators ----------------------------------------------------- */
#define CDC_ISP0_BASE     0x100D0000u  /* reserved */
#define CDC_VPU0_BASE     0x100E0000u  /* reserved */
#define CDC_NPU0_BASE     0x100F0000u  /* optional private SAURIA build */
#define CDC_ACCEL_MMIO_SIZE 0x00010000u

/* ---- Peripheral instance 1 ------------------------------------------- */
#define CDC_UART1_BASE    0x10100000u
#define CDC_I2C1_BASE     0x10110000u
#define CDC_SPI1_BASE     0x10120000u
#define CDC_TIMER1_BASE   0x10130000u
#define CDC_RTC0_BASE     0x10140000u
#define CDC_ADC0_BASE     0x10150000u
#define CDC_GPIO0_BASE    0x10160000u  /* gpio_tlm, 32 pins, no IRQ yet     */

/* GPIO register offsets (from CDC_GPIO0_BASE); 32-bit accesses only.      */
#define CDC_GPIO_VALUE    0x00u        /* RO: pin levels                    */
#define CDC_GPIO_OUT      0x04u        /* RW: output latch                  */
#define CDC_GPIO_DIR      0x08u        /* RW: 1=output, reset: all inputs   */
#define CDC_GPIO_BOOT_PIN 1u           /* boot strap: LOW=IFLASH app,
                                          HIGH=UART/SPI download probe      */

/* SPI0 (PL022) vendor register: software chip-select for the NOR flash
 * behind SPI0. bit0: 1 = assert (line low). A NOR READ (CMD 0x03) spans
 * many frames and terminates on CS deassert.                              */
#define CDC_SPI_CSR       0x28u

/* ---- Main memory (RAM0 / DDR-like), 256 MiB -------------------------- */
#define CDC_RAM0_BASE     0x80000000u
#define CDC_RAM0_SIZE     0x10000000u

/* RAM0 internal buffer windows (shared by CPU, DMA, and accelerators).   */
#define CDC_FW_BASE        0x80000000u  /* firmware text/data/heap/stack    */
#define CDC_FW_SIZE        0x01000000u  /* 16 MiB                           */
#define CDC_RAW_IN0_BASE   0x81000000u
#define CDC_RAW_IN0_SIZE   0x01000000u
#define CDC_ISP_OUT0_BASE  0x82000000u
#define CDC_ISP_OUT0_SIZE  0x02000000u
#define CDC_VPU_OUT0_BASE  0x84000000u
#define CDC_VPU_OUT0_SIZE  0x02000000u
#define CDC_NPU_WGT0_BASE  0x86000000u
#define CDC_NPU_WGT0_SIZE  0x02000000u
#define CDC_NPU_WORK0_BASE 0x88000000u
#define CDC_NPU_WORK0_SIZE 0x04000000u

/* ---- CLINT register offsets (from CDC_CLINT_BASE) -------------------- */
#define CDC_CLINT_MSIP        0x0000u
#define CDC_CLINT_MTIMECMP    0x4000u  /* 64-bit, microsecond units         */
#define CDC_CLINT_MTIME       0xBFF8u  /* 64-bit free-running, microseconds */

/* ---- PLIC register offsets (from CDC_PLIC_BASE), hart0 M-mode -------- */
#define CDC_PLIC_PRIORITY(id) (0x000000u + 4u * (id))  /* per-source prio   */
#define CDC_PLIC_ENABLE       0x002000u                /* enable bitfield   */
#define CDC_PLIC_THRESHOLD    0x200000u                /* context threshold */
#define CDC_PLIC_CLAIM        0x200004u                /* claim / complete  */

#endif /* CDC_SOC_MEMORY_MAP_H */
