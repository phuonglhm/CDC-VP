#pragma once
#include <stdint.h>

// FU3 DBG: cầu JTAG2AXI4 (HAS v2.0 3.1) + Debug Module RISC-V 0.13.2 (debug CPU, HAS 2 / Table 11-2)
// Mã lệnh JTAG và độ rộng DR: HAS chưa có => giả định VP choice, chờ chốt
// IDCODE: HAS chưa có, tạm theo IP_Feature_Spec (sheet DBG)

// JTAG instruction (IR 5 bit)
#define FU3_DBG_IR_CMD_IDCODE 0x01u   // Mặc định sau Test-Logic-Reset
#define FU3_DBG_IR_CMD_ADDR   0x02u
#define FU3_DBG_IR_CMD_DATA   0x03u
#define FU3_DBG_IR_CMD_STATUS 0x04u
#define FU3_DBG_IR_CMD_BYPASS 0x1Fu   // IR khác cũng là BYPASS
#define FU3_DBG_IDCODE        UINT32_C(0x00000DB3)

// DR của CMD_DATA 34 bit: [33] read, [32] write, [31:0] data. Capture trả dữ liệu của lần đọc trước
#define FU3_DBG_DATA_WRITE (UINT64_C(1) << 32)
#define FU3_DBG_DATA_READ  (UINT64_C(1) << 33)

// DR của CMD_STATUS 32 bit: lỗi sticky của truy cập AXI, ghi 1 để xoá
#define FU3_DBG_STATUS_BUSY      (1u << 0)   // Luôn 0: truy cập xong trong Update-DR
#define FU3_DBG_STATUS_SLVERR    (1u << 1)   // AXI SLVERR (lỗi slave, chặn quyền truy cập)
#define FU3_DBG_STATUS_DECERR    (1u << 2)   // AXI DECERR (địa chỉ không map)
#define FU3_DBG_STATUS_ALIGN     (1u << 3)   // Địa chỉ không căn 4 byte: không truy cập
#define FU3_DBG_STATUS_ERRORS    (FU3_DBG_STATUS_SLVERR | FU3_DBG_STATUS_DECERR | FU3_DBG_STATUS_ALIGN)

// Debug Module trên bus: cửa sổ DM của Ibex (Common v2.0 MemoryMap), thanh ghi DMI n ở offset 4 * n
#define FU3_DBG_DM_BASE      UINT32_C(0x1A110000)
#define FU3_DBG_DM_OFFSET(n) (4u * (n))

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
#define FU3_DBG_SBCS         0x38u   // Không có SBA: đọc 0, truy cập hệ thống qua cầu JTAG2AXI4
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
#define FU3_DBG_DMSTATUS_ALLUNAVAIL      (1u << 13)
#define FU3_DBG_DMSTATUS_ANYUNAVAIL      (1u << 12)
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
