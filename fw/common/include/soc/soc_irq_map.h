/*
 * soc_irq_map.h - VP_FX1 Full SoC interrupt API (firmware-facing; pins the IRQ ABI).
 *
 * PLIC external-interrupt source IDs (1-based; source 0 is reserved by the
 * RISC-V PLIC architecture and must never be assigned) plus the RISC-V local
 * interrupt cause codes routed through the CLINT.
 *
 * Matches docs/peripheral_memory_map.md "PLIC IRQ Map".
 */
#ifndef CDC_SOC_IRQ_MAP_H
#define CDC_SOC_IRQ_MAP_H

/* ---- RISC-V local interrupt causes (mcause low bits, MSB=interrupt) --- */
#define CDC_CAUSE_MSIP  3u   /* CLINT software interrupt  */
#define CDC_CAUSE_MTIP  7u   /* CLINT timer interrupt     */
#define CDC_CAUSE_MEIP  11u  /* PLIC external interrupt   */

/* ---- mie / mstatus bits ---------------------------------------------- */
#define CDC_MIE_MSIE     (1u << 3)
#define CDC_MIE_MTIE     (1u << 7)
#define CDC_MIE_MEIE     (1u << 11)
#define CDC_MSTATUS_MIE  (1u << 3)

/* ---- PLIC source IDs -------------------------------------------------- */
#define CDC_IRQ_UART0       1u   /* uart2_tlm combined UARTINTR            */
#define CDC_IRQ_I2C0        2u
#define CDC_IRQ_SPI0        3u
#define CDC_IRQ_TIMER0      4u
#define CDC_IRQ_WDT0        5u
#define CDC_IRQ_PWM0        6u   /* reserved: PWM has no IRQ output yet    */
#define CDC_IRQ_DMA0        7u   /* dma nonzero/transfer-done              */
#define CDC_IRQ_DMA0_ABORT  8u
#define CDC_IRQ_TRNG0       9u
#define CDC_IRQ_CMU0        10u  /* reserved / tied low                   */
#define CDC_IRQ_PMU0        11u
#define CDC_IRQ_DMIC0       12u
#define CDC_IRQ_OTP0        13u
#define CDC_IRQ_QSPI0       14u
#define CDC_IRQ_ISP0        15u  /* reserved until ISP model exposes IRQ  */
#define CDC_IRQ_VPU0        16u  /* reserved until VPU model exposes IRQ  */
#define CDC_IRQ_NPU0        17u  /* optional NPU IRQ; default tied low    */
#define CDC_IRQ_UART1       18u
#define CDC_IRQ_I2C1        19u
#define CDC_IRQ_SPI1        20u
#define CDC_IRQ_TIMER1      21u
#define CDC_IRQ_RTC0        22u  /* RTC alarm                             */
#define CDC_IRQ_ADC0        23u
/* 24-31 reserved for GPIO / AES / future instances. */

#define CDC_PLIC_NUM_SOURCES 31u

#endif /* CDC_SOC_IRQ_MAP_H */
