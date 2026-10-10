# H.264 DMA Subsystem — Functional VP Spec

Tóm tắt contract của implementation C++17/SystemC/TLM, tham chiếu
[SISLAB HAS v2.1.11](../SISLAB_H264_AVC_HAS_Spec_Detailed_v2.1.11.md),
§3, §5–6, §11.4, §12 và Appendix B.

## 1. Chức năng và interface

| Khối | Chức năng |
|---|---|
| CMB DMA | Đọc macroblock Y16×16/U8×8/V8×8 từ source frame |
| SW DMA | Đọc search window, giữ cache và readiness riêng cho List0/List1 |
| DF DMA | Ghi reconstructed/filtered Y/U/V vào selected REFM slot |
| NAL DMA | Ghi NAL words nối tiếp; cập nhật length sau successful response |
| Arbiter/bridge | Giữ request/payload/owner tới completion; xử lý lanes, burst và lỗi |

Core dùng `MemoryIf`; wrapper `h264_dma_tlm` có MMIO `target_socket`, DMA
`master_socket`, input `reset_n` và output `irq`. CPU và DMA dùng chung DDR.

## 2. Memory contract

- Physical address 32-bit; DMA data width cấu hình 32/64/128-bit.
- Coded dimensions dương, chia hết 16, tối đa 1920×1088; planar YUV 4:2:0.
  Với `F = 3*Wc*Hc/2`: Y offset 0, U offset `Wc*Hc`, V offset `5*Wc*Hc/4`.
- CMB allocation chứa 1..127 frames; REFM chứa 3 slots tại `base + slot*F`.
  NAL có capacity riêng. Bases phải nonzero, aligned theo bus width;
  các vùng nằm trong shared-memory window và không overlap.
- `DmaRequest.beats` là internal count; số byte = `beats * size`.
  Bridge chia external burst tại 4 KiB, chỉ ghi addressed bytes.
- CMB/SW chỉ ready khi đủ successful reads; DF/NAL chỉ complete sau đủ
  successful write responses. B-picture cần cả hai reference lists resident.

## 3. MMIO

Offsets local, aligned 32-bit access; register bytes little-endian.

| Offset | Register | Ý nghĩa |
|---|---|---|
| `0x00` | SCON | ECE bit 0: enable/start; GIE bit 1: gate IRQ |
| `0x04` | STAT | NORMAL 18, ERROR 17, BUSY 16, FMCNT 15:0; read clears IRQ |
| `0x08` | FMSIZE | Width 26:16, coded height 10:0 |
| `0x0C` | DFCON | Filter enable bit 0, signed alpha/beta offsets |
| `0x10` | SPARA0 | QP 0..51, CMB mode, syntax/GOP fields; reset `0x12C` |
| `0x14` | SPARA1 | CMB frame allocation 31:25, syntax fields, FMENC 15:0 nonzero |
| `0x18` | SPARA2 | Bottom crop 16:6, signed QP delta 5:0 |
| `0x1C` | REFM | Physical reference base |
| `0x20` | NAL | Physical output base |
| `0x24` | CMB | Physical source base |
| `0x28` | STM_LEN | Committed 32-bit word count; read clears IRQ |

Config writes chỉ khi ECE=0/BUSY=0; STAT/STM_LEN read-only. Đọc hai thanh ghi
này giữ status/count, chỉ acknowledge IRQ; debug read không acknowledge.
Invalid access trả TLM error theo VP policy; undefined offset mặc định
ADDRESS_ERROR, có option ReadZero. Các response này không khẳng định behavior RTL.

## 4. Activation và completion

1. Disable, lập trình registers và cung cấp workload/provider.
2. Cạnh ECE 0→1 latch config, clear status/count/IRQ cũ và đặt BUSY.
   Giữ ECE=1 không start lại.
3. Workload cung cấp đủ macroblocks, mỗi tọa độ một lần, source/reference
   slots, reference selections, reconstructed pixels và NAL words.
   Frame count không vượt FMENC; CMB mode 1 xử lý một picture/activation.
4. NORMAL chỉ sau đủ REFM/NAL responses thành công; FMCNT đếm pictures
   hoàn tất trong activation. Completion/error assert IRQ nếu GIE=1.
5. Host đọc STAT và drain `4*STM_LEN` bytes; disable rồi re-enable để chạy tiếp.
   Disable sau completion giữ STM_LEN.

Transfer/config/workload failure hoặc hết service budget → ERROR, không NORMAL.
Hạ ECE khi BUSY: chờ memory call đã accepted trả về, dừng work tiếp theo rồi ERROR.
Reset active low xóa registers/status/count/IRQ, giữ SPARA0 reset `0x12C`;
completion cũ bị hủy. Không rollback DDR accesses đã accepted.

## 5. Giới hạn mô hình

Đây là functional DMA VP. Codec/GOP scheduling, NAL generation và deblocking
arithmetic do producer cung cấp. DF nhận tile đã lọc hoặc callback vertical
rồi horizontal; copy CMB sang REFM chỉ khi chọn explicit bypass replay.
NAL default little-endian, hỗ trợ BigEndian; EOS là bytes `00 00 01 0B`.
TLM latency được chờ trước completion; không mô phỏng AXI pin/cycle timing.
Service budget không preempt target `b_transport` không return.

Hướng dẫn build/test và API chi tiết: [README.md](README.md).
