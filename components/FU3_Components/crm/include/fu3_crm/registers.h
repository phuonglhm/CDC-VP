#pragma once
#include <stdint.h>

// FU3 CRM - offset/bit cho firmware theo FU3 CRM Specification V1.1 + CSR_CRM_fu3_v2
#define FU3_CRM_BASE UINT32_C(0x50010000)   // PERIBUS_1, cửa sổ 64 KiB, decode 0x0000-0x0FFF

#define FU3_CRM_VERSION_OFFSET       0x0000u   // RO: [31:16] major, [15:0] minor
#define FU3_CRM_PLL_OFFSET           0x0010u   // [14] lock RO, [13:8] n_div, [7:2] m_div, [1:0] od
#define FU3_CRM_PLL_CTRL_OFFSET      0x0018u   // [14] oe, [13:4] count, [3] lock_type, [2] lken, [1] bypass, [0] powerdown
#define FU3_CRM_CLK_0_OFFSET         0x001Cu   // [0] sel: 0 OSC, 1 PLL
#define FU3_CRM_CLK_1_OFFSET         0x0020u   // [0] sel: 0 OSC, 1 nhánh của CLK_0
#define FU3_CRM_CLK_OFFSET           0x0024u   // [9:0] div: 0 bypass, n > 0 -> Fin / (2n)
#define FU3_CRM_CLK_GATE_0_OFFSET    0x0028u   // 1 = clock bật (FU3_CRM_GATE0_*)
#define FU3_CRM_CLK_GATE_1_OFFSET    0x002Cu   // 1 = clock bật (FU3_CRM_PERI_*)
#define FU3_CRM_IBEX_SW_OFFSET       0x0030u   // [0] rst_n CPU1 (Ibex)
#define FU3_CRM_FCLK_SW_OFFSET       0x0034u   // [0] rst_n CPU0 (CVA6)
#define FU3_CRM_ACLK_SW_OFFSET       0x0038u   // [0] rst_n bus AXI
#define FU3_CRM_PCLK_0_SW_OFFSET     0x003Cu   // [0] rst_n PERIBUS_0
#define FU3_CRM_PCLK_1_SW_OFFSET     0x0040u   // [0] rst_n PERIBUS_1
#define FU3_CRM_SW_RST_N_0_OFFSET    0x0044u   // 0 = giữ reset (FU3_CRM_RST0_*)
#define FU3_CRM_SW_RST_N_1_OFFSET    0x0048u   // 0 = giữ reset (FU3_CRM_PERI_*)
#define FU3_CRM_SYSTEM_CLK_SW_OFFSET 0x0054u   // [0] rst_n miền hệ thống (bus, memory, ngoại vi, 2 CPU)
#define FU3_CRM_SW_RESET_REQ_OFFSET  0x0058u   // RO: Mức live của yêu cầu reset (FU3_CRM_REQ_*)
#define FU3_CRM_CRM_OFFSET           0x005Cu   // [0] int: Mức IRQ của CRM

#define FU3_CRM_PLL_LOCK      (1u << 14)
#define FU3_CRM_PLL_POWERDOWN (1u << 0)
#define FU3_CRM_PLL_BYPASS    (1u << 1)
#define FU3_CRM_PLL_LKEN      (1u << 2)
#define FU3_CRM_PLL_LOCK_TYPE (1u << 3)
#define FU3_CRM_PLL_OE        (1u << 14)

// CLK_GATE_0: IP trên SYSBUS
#define FU3_CRM_GATE0_ROM          (1u << 0)
#define FU3_CRM_GATE0_ISRAM        (1u << 1)   // IBEX_ISRAM và CVA6_ISRAM
#define FU3_CRM_GATE0_DSRAM        (1u << 2)
#define FU3_CRM_GATE0_AES_256      (1u << 3)
#define FU3_CRM_GATE0_QSPI         (1u << 4)
#define FU3_CRM_GATE0_HYPERBUS     (1u << 5)
#define FU3_CRM_GATE0_EDHOC_OSCORE (1u << 6)
#define FU3_CRM_GATE0_DMA          (1u << 7)
#define FU3_CRM_GATE0_DBG          (1u << 8)

// SW_RST_N_0: IP trên SYSBUS
#define FU3_CRM_RST0_ROM          (1u << 0)
#define FU3_CRM_RST0_ISRAM        (1u << 1)
#define FU3_CRM_RST0_DSRAM        (1u << 2)
#define FU3_CRM_RST0_AES_256      (1u << 3)
#define FU3_CRM_RST0_QSPI         (1u << 4)
#define FU3_CRM_RST0_DMA          (1u << 5)
#define FU3_CRM_RST0_EDHOC_OSCORE (1u << 6)
#define FU3_CRM_RST0_HYPERBUS     (1u << 7)

// CLK_GATE_1 và SW_RST_N_1 dùng chung vị trí bit cho IP trên PERIBUS
// Bit 0: CLK_GATE_1 = MRV (CPU nội bộ CRM), SW_RST_N_1 = pll_ctrl
#define FU3_CRM_PERI_MRV          (1u << 0)
#define FU3_CRM_PERI_PLL_CTRL     (1u << 0)
#define FU3_CRM_PERI_WDT0         (1u << 1)
#define FU3_CRM_PERI_WDT1         (1u << 2)
#define FU3_CRM_PERI_ADC          (1u << 3)
#define FU3_CRM_PERI_RNG          (1u << 4)
#define FU3_CRM_PERI_SYSCSR       (1u << 5)
#define FU3_CRM_PERI_PLIC         (1u << 6)
#define FU3_CRM_PERI_GPIOC        (1u << 7)
#define FU3_CRM_PERI_GPIOB        (1u << 8)
#define FU3_CRM_PERI_GPIOA        (1u << 9)
#define FU3_CRM_PERI_I2C          (1u << 10)
#define FU3_CRM_PERI_UART0        (1u << 11)
#define FU3_CRM_PERI_UART1        (1u << 12)
#define FU3_CRM_PERI_UART2        (1u << 13)
#define FU3_CRM_PERI_SPI0         (1u << 14)
#define FU3_CRM_PERI_SPI1         (1u << 15)
#define FU3_CRM_PERI_SPI2         (1u << 16)
#define FU3_CRM_PERI_SPI3         (1u << 17)
#define FU3_CRM_PERI_TIMER0       (1u << 18)
#define FU3_CRM_PERI_TIMER1       (1u << 19)
#define FU3_CRM_PERI_TIMER2       (1u << 20)
#define FU3_CRM_PERI_TIMER3       (1u << 21)
#define FU3_CRM_PERI_MAILBOX_IBEX (1u << 22)
#define FU3_CRM_PERI_MAILBOX_CVA6 (1u << 23)
#define FU3_CRM_PERI_AP_HBCTRL    (1u << 24)

// SW_RESET_REQ (RO, mức live)
#define FU3_CRM_REQ_GLOBAL      (1u << 0)   // SYSCSR sw_global_rst_req: khởi động lại cả CRM
#define FU3_CRM_REQ_SYSTEM      (1u << 1)   // SYSCSR sw_reset_req
#define FU3_CRM_REQ_WATCHDOG    (1u << 2)   // WDT0/1 critical
#define FU3_CRM_REQ_SYSRESETREQ (1u << 3)   // DBG ndmreset

#define FU3_CRM_OSC_HZ 20000000u   // OSC 20 MHz
