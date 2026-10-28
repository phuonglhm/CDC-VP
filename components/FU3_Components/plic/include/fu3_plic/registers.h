#pragma once
#include <stdint.h>

// FU3 AP_PLIC (Common Spec v2.1, PLIC Configuration / Interrupt Mapping: pulp rv_plic plic_top)
// 64 KiB window on PERIBUS_1, 32 slots: ID 0 reserved, sources ID 1-31, 1 target (CVA6 irq_i[0]),
// priority 1-31 (5 bit). Offsets: VP ABI confirmed by the user (decision Q03-a), not RTL-confirmed.
#define FU3_PLIC_BASE UINT32_C(0x40100000)

#define FU3_PLIC_PRIORITY_OFFSET(id) (4u * (id))   // RW [4:0]: ID 1-31, ID 0 reads 0
#define FU3_PLIC_PENDING_OFFSET      0x1000u       // RO: bit n = ID n, bit 0 = 0
#define FU3_PLIC_ENABLE_OFFSET       0x2000u       // RW: bit n = ID n cho context 0

// Spec: offset threshold/claim là "TBD (compact-map offset)" => VP tạm chọn, chờ RTL chốt
#define FU3_PLIC_THRESHOLD_OFFSET    0x3000u       // RW [4:0]: interrupt only when priority > threshold
#define FU3_PLIC_CLAIM_OFFSET        0x3004u       // Đọc = claim (0 = không có), ghi ID = complete

#define FU3_PLIC_SOURCES      31u
#define FU3_PLIC_MAX_PRIORITY 31u
#define FU3_PLIC_ENABLE_MASK  0xFFFFFFFEu
#define FU3_PLIC_SOURCE_BIT(id) (1u << (id))

// Source ID theo Common Spec v2.0 Interrupt Mapping: ID n <- irq_sources_i[n-1]
#define FU3_PLIC_ID_CRM          1u
#define FU3_PLIC_ID_UART0        2u
#define FU3_PLIC_ID_AES          3u
#define FU3_PLIC_ID_QSPI         4u
#define FU3_PLIC_ID_DMA0         5u
#define FU3_PLIC_ID_TIMER1       6u
#define FU3_PLIC_ID_TIMER2       7u
#define FU3_PLIC_ID_TIMER3       8u
#define FU3_PLIC_ID_SPI0         9u
#define FU3_PLIC_ID_UART1        10u
#define FU3_PLIC_ID_UART2        11u
#define FU3_PLIC_ID_GPIOA        12u
#define FU3_PLIC_ID_GPIOB        13u
#define FU3_PLIC_ID_GPIOC        14u
#define FU3_PLIC_ID_I2C0         15u
#define FU3_PLIC_ID_SPI1         16u
#define FU3_PLIC_ID_ADC          17u
#define FU3_PLIC_ID_WDT1         18u
#define FU3_PLIC_ID_GPIO0_WAKE   19u
#define FU3_PLIC_ID_EDHOC_OSCORE 20u
#define FU3_PLIC_ID_RNG          21u
#define FU3_PLIC_ID_SPI2         22u
#define FU3_PLIC_ID_SPI3         23u
// Common v2.1 IDs 24-31 (provisional allocation, decision Q03-h): UART3-7 in R1,
// PWM0/1 and CAN stay inactive until R2.
#define FU3_PLIC_ID_UART3        24u
#define FU3_PLIC_ID_UART4        25u
#define FU3_PLIC_ID_UART5        26u
#define FU3_PLIC_ID_UART6        27u
#define FU3_PLIC_ID_UART7        28u
#define FU3_PLIC_ID_PWM0         29u
#define FU3_PLIC_ID_PWM1         30u
#define FU3_PLIC_ID_CAN          31u
