# Register behavior — PDF table 20-3

Nguồn: SISLAB H.264/AVC Hardware Architecture Specification v2.1.11, bảng 20-3,
trang 53–55; activation/completion ở §4.3–4.5 và bảng 20-6 trang 57.
Nhận định trước đây rằng PDF không có SPARA bit mapping là sai: đã bỏ sót phụ lục.

| Register | Bit fields áp dụng |
|---|---|
| SCON | ECE[0], GIE[1] |
| STAT | NORMAL[18], ERROR[17], BUSY[16], FMCNT[15:0] |
| FMSIZE | WIDTH[26:16], HEIGHT[10:0] |
| DFCON | ALPHA signed[10:6], BETA signed[5:1], ENABLE[0] |
| SPARA0 | INIT_QP[26:21], PKFM[20:12], CMB_FRAME_MODE[9], COMPAT_ENABLE[8], GOP_N[7:4], GOP_M[3:0] |
| SPARA1 | CMB_ON_MEM[31:25], LOG2_FG[24], LOG2_FN[23:20], LOG2_POC[19:16], FMENC[15:0] |
| SPARA2 | CROP_BOTTOM[16:6], signed SLICE_QP_DELTA[5:0] |
| REFM/NAL/CMB | Physical base[31:0] |
| STM_LEN | Accepted NAL words[31:0], activation-relative |

Reset: SPARA0=0x12c (COMPAT=1, N=2, M=12); các register khác bằng 0.
PKFM được lưu nhưng không tham gia scheduling. Reserved bits đọc zero sau mask.
ControlRegs::config decode các trường từ register, không lấy QP/FMENC từ constructor.
HostDriver::program_sequence encode cùng mapping. Các trường filter/syntax/crop được
chuyển cho module thật; stub không thực hiện thuật toán lọc/crop/H.264.

STAT/STM_LEN read acknowledge IRQ. GIE mask output; tắt GIE không xóa event đã latch;
start mới hoặc clearing read xóa event. Nếu hoàn tất khi GIE=0 thì không tạo event mới.
Thao tác register invalid trả error là policy VP vì PDF chỉ đề cập zero/error behavior
tùy RTL. Access chỉ 32-bit aligned, byte enable được tôn trọng.

## Working set và sequence

FMENC là tổng số frame, CMB_ON_MEM là số frame trong working set/activation (1..127).
Preload xử lý min(remaining,CMB_ON_MEM); frame-address xử lý một picture tại REG_CMB.
FMCNT giữ qua các activation. STM_LEN reset khi rising enable, giữ sau disable để drain.
Host chỉ cập nhật CMB/NAL/SPARA1 khi disabled và BUSY clear. Thay batch bits của SPARA1
không reset sequence counter. Khởi động sau khi đã đủ FMENC bắt đầu sequence mới.

Chính sách VP cho sequence mới: thay FMSIZE/REFM/filter/crop hoặc active SPARA0 fields,
hay low 25 bits SPARA1 thì restart count. Ghi lại cùng giá trị/đổi PKFM không restart.
Chính sách này cần đối chiếu driver release cho các luồng cập nhật ngoài reactivation chuẩn.

## NAL progress / error

Pipeline gọi nal_words_accepted tại lúc NAL DMA nhận word. STM_LEN tăng trước response;
finalization phải đợi output/reference responses và báo total khớp counter. Error giữ số
word accepted, không giả vờ đây là bitstream hợp lệ. Generation cũ không được cập nhật counter.

GOP_N=1: I mỗi M picture, còn lại P, coding index/display index tiến theo sequence.
GOP_N>1: adapter release cung cấp PictureTask; thiếu lịch thì ERROR, không suy đoán B-order.
Controller kiểm tra duplicate display index, coding index, source slot qua các activation.
Source-slot mapping và reference lifetime thật vẫn thuộc adapter/modules của nhóm.
