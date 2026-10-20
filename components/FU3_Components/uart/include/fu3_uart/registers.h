#pragma once
#include <stdint.h>

/* Byte offsets, not word indices. Use byte MMIO through the FU3 bus. */
#define FU3_UART0_BASE UINT32_C(0x440d0000)
#define FU3_UART1_BASE UINT32_C(0x440e0000)
#define FU3_UART2_BASE UINT32_C(0x50050000)
#define FU3_UART_RBR 0u
#define FU3_UART_THR 0u
#define FU3_UART_DLL 0u
#define FU3_UART_IER 1u
#define FU3_UART_DLM 1u
#define FU3_UART_IIR 2u
#define FU3_UART_FCR 2u
#define FU3_UART_LCR 3u
#define FU3_UART_MCR 4u
#define FU3_UART_LSR 5u
#define FU3_UART_MSR 6u
#define FU3_UART_SCR 7u
#define FU3_UART_DLAB 0x80u
#define FU3_UART_DR 0x01u
#define FU3_UART_OE 0x02u
#define FU3_UART_PE 0x04u
#define FU3_UART_FE 0x08u
#define FU3_UART_BI 0x10u
#define FU3_UART_THRE 0x20u
#define FU3_UART_TEMT 0x40u
#define FU3_UART_FIFO_ERROR 0x80u
#define FU3_UART_IER_RX 0x01u
#define FU3_UART_IER_TX 0x02u
#define FU3_UART_IER_LINE 0x04u
#define FU3_UART_IER_MODEM 0x08u
