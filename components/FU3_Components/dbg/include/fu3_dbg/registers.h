#pragma once
#include <stdint.h>

// FU3 DBG - địa chỉ DMI và field theo RISC-V External Debug Support 0.13.2
// JTAG IR theo Debug Spec 6.1.2. IDCODE: HAS chưa có, tạm theo IP_Feature_Spec (sheet DBG)

// JTAG instruction (IR 5 bit)
#define FU3_DBG_IR_BYPASS0  0x00u
#define FU3_DBG_IR_IDCODE   0x01u
#define FU3_DBG_IR_DTMCS    0x10u
#define FU3_DBG_IR_DMI      0x11u
#define FU3_DBG_IR_BYPASS   0x1Fu
#define FU3_DBG_IDCODE      UINT32_C(0x00000DB3)

// dtmcs
#define FU3_DBG_DTMCS_VERSION      1u          // [3:0] Debug Spec 0.13
#define FU3_DBG_DTMCS_ABITS        7u          // [9:4]
#define FU3_DBG_DTMCS_DMIRESET     (1u << 16)
#define FU3_DBG_DTMCS_DMIHARDRESET (1u << 17)

// dmi DR 41 bit: [40:34] address, [33:2] data, [1:0] op
#define FU3_DBG_DMI_OP_NOP   0u
#define FU3_DBG_DMI_OP_READ  1u
#define FU3_DBG_DMI_OP_WRITE 2u

// Địa chỉ thanh ghi DM
#define FU3_DBG_DATA0        0x04u
#define FU3_DBG_DATA1        0x05u
#define FU3_DBG_DMCONTROL    0x10u
#define FU3_DBG_DMSTATUS     0x11u
#define FU3_DBG_HARTINFO     0x12u
#define FU3_DBG_ABSTRACTCS   0x16u
#define FU3_DBG_COMMAND      0x17u
#define FU3_DBG_ABSTRACTAUTO 0x18u
#define FU3_DBG_NEXTDM       0x1Du
#define FU3_DBG_SBCS         0x38u
#define FU3_DBG_SBADDRESS0   0x39u
#define FU3_DBG_SBDATA0      0x3Cu
#define FU3_DBG_HALTSUM0     0x40u

// dmcontrol
#define FU3_DBG_DMCONTROL_HALTREQ      (1u << 31)
#define FU3_DBG_DMCONTROL_RESUMEREQ    (1u << 30)
#define FU3_DBG_DMCONTROL_ACKHAVERESET (1u << 28)
#define FU3_DBG_DMCONTROL_HARTSEL(h)   ((((h) & 0x3FFu) << 16) | ((((h) >> 10) & 0x3FFu) << 6))
#define FU3_DBG_DMCONTROL_NDMRESET     (1u << 1)
#define FU3_DBG_DMCONTROL_DMACTIVE     (1u << 0)

// dmstatus
#define FU3_DBG_DMSTATUS_ALLHAVERESET    (1u << 19)
#define FU3_DBG_DMSTATUS_ANYHAVERESET    (1u << 18)
#define FU3_DBG_DMSTATUS_ALLRESUMEACK    (1u << 17)
#define FU3_DBG_DMSTATUS_ANYRESUMEACK    (1u << 16)
#define FU3_DBG_DMSTATUS_ALLNONEXISTENT  (1u << 15)
#define FU3_DBG_DMSTATUS_ANYNONEXISTENT  (1u << 14)
#define FU3_DBG_DMSTATUS_ALLRUNNING      (1u << 11)
#define FU3_DBG_DMSTATUS_ANYRUNNING      (1u << 10)
#define FU3_DBG_DMSTATUS_ALLHALTED       (1u << 9)
#define FU3_DBG_DMSTATUS_ANYHALTED       (1u << 8)
#define FU3_DBG_DMSTATUS_AUTHENTICATED   (1u << 7)
#define FU3_DBG_DMSTATUS_VERSION         2u          // Debug Spec 0.13

// abstractcs: [28:24] progbufsize, [12] busy, [10:8] cmderr, [3:0] datacount
#define FU3_DBG_ABSTRACTCS_CMDERR_SHIFT 8
#define FU3_DBG_ABSTRACTCS_CMDERR_MASK  (7u << 8)
#define FU3_DBG_CMDERR_NONE         0u
#define FU3_DBG_CMDERR_BUSY         1u
#define FU3_DBG_CMDERR_NOTSUPPORTED 2u
#define FU3_DBG_CMDERR_EXCEPTION    3u
#define FU3_DBG_CMDERR_HALTRESUME   4u

// command (cmdtype 0 = Access Register)
#define FU3_DBG_CMD_AARSIZE_32       (2u << 20)
#define FU3_DBG_CMD_AARPOSTINCREMENT (1u << 19)
#define FU3_DBG_CMD_POSTEXEC         (1u << 18)
#define FU3_DBG_CMD_TRANSFER         (1u << 17)
#define FU3_DBG_CMD_WRITE            (1u << 16)
#define FU3_DBG_REGNO_CSR(n)         ((n) & 0x0FFFu)
#define FU3_DBG_REGNO_GPR(n)         (0x1000u + (n))

// sbcs
#define FU3_DBG_SBCS_SBVERSION       (1u << 29)
#define FU3_DBG_SBCS_SBBUSYERROR     (1u << 22)
#define FU3_DBG_SBCS_SBBUSY          (1u << 21)
#define FU3_DBG_SBCS_SBREADONADDR    (1u << 20)
#define FU3_DBG_SBCS_SBACCESS(n)     (((n) & 7u) << 17)
#define FU3_DBG_SBCS_SBAUTOINCREMENT (1u << 16)
#define FU3_DBG_SBCS_SBREADONDATA    (1u << 15)
#define FU3_DBG_SBCS_SBERROR_SHIFT   12
#define FU3_DBG_SBCS_SBERROR_MASK    (7u << 12)
#define FU3_DBG_SBCS_SBASIZE_32      (32u << 5)
#define FU3_DBG_SBCS_SBACCESS32      (1u << 2)
#define FU3_DBG_SBERROR_BADADDR      2u
#define FU3_DBG_SBERROR_ALIGNMENT    3u
#define FU3_DBG_SBERROR_SIZE         4u
#define FU3_DBG_SBERROR_OTHER        7u
