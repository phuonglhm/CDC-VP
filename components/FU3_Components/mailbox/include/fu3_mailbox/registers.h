#pragma once
#include <stdint.h>

// FU3 MAILBOX - offset/bit cho firmware
// Địa chỉ: HAS v2.0 memory map. 
// Offset/bit: tham khảo IP_Feature_Spec (MAILBOX V1.0)
// chờ HDS
#define FU3_MAILBOX_IBEX_BASE UINT32_C(0x44040000)   // MAILBOX_IBEX, PERIBUS_0
#define FU3_MAILBOX_CVA6_BASE UINT32_C(0x50020000)   // MAILBOX_CVA6, PERIBUS_1

#define FU3_MAILBOX_FIFO_WR_OFFSET       0x0000u   // WO: đẩy 1 word vào FIFO gửi
#define FU3_MAILBOX_FIFO_RD_OFFSET       0x0008u   // RO: lấy word cũ nhất của FIFO nhận
#define FU3_MAILBOX_STATUS_OFFSET        0x0010u   // RO
#define FU3_MAILBOX_WR_THRESHOLDS_OFFSET 0x0018u   // RW: send active khi số word FIFO gửi <= giá trị này
#define FU3_MAILBOX_RD_THRESHOLDS_OFFSET 0x001Cu   // RW: receive active khi số word FIFO nhận > giá trị này
#define FU3_MAILBOX_IRQ_STATUS_OFFSET    0x0020u   // R/W1C, sticky
#define FU3_MAILBOX_IRQ_ENABLE_OFFSET    0x0024u   // RW
#define FU3_MAILBOX_IRQ_PENDING_OFFSET   0x0028u   // RO: IRQ_STATUS & IRQ_ENABLE

// STATUS
#define FU3_MAILBOX_STATUS_EMPTY          (1u << 31)   // FIFO nhận rỗng
#define FU3_MAILBOX_STATUS_FULL           (1u << 30)   // FIFO gửi đầy
#define FU3_MAILBOX_STATUS_SEND_THRESHOLD (1u << 29)
#define FU3_MAILBOX_STATUS_RECV_THRESHOLD (1u << 28)

// IRQ_STATUS / IRQ_ENABLE / IRQ_PENDING
#define FU3_MAILBOX_IRQ_WR_THRESHOLD  (1u << 31)   // Sườn lên của SEND_THRESHOLD
#define FU3_MAILBOX_IRQ_RD_THRESHOLD  (1u << 30)   // Sườn lên của RECV_THRESHOLD
#define FU3_MAILBOX_IRQ_FIFO_WR_ERROR (1u << 29)   // Ghi khi FIFO gửi đầy
#define FU3_MAILBOX_IRQ_FIFO_RD_ERROR (1u << 28)   // Đọc khi FIFO nhận rỗng
#define FU3_MAILBOX_IRQ_MASK          0xF0000000u

#define FU3_MAILBOX_DEPTH 32u   // Số word mỗi FIFO (mặc định)
