# Interface contracts v0.1

Owner điều phối: Huy Nguyen Huynh Quoc. Vinh Nguyen Thanh và Nguyên
cùng review các interface có producer/consumer thuộc mình. Phân công, ranh giới NAL/DF DMA
và quy trình ghép module: `integration_docs/cdc_vp_integration.md` (tính từ root project).
Các API dưới đây mô tả VP đang chạy; API prediction/TQ/entropy thật chưa được chốt.

## Thời gian và concurrency

Mô hình loosely timed; các lời gọi blocking chỉ được gọi từ `SC_THREAD`.
Các target tiêu thụ input delay và trả về với delay zero; bridge cũng tiêu thụ mọi
output annotation từ target thay thế trước khi báo completion. Target register có
latency zero (không mô hình từng handshake AXI-Lite).

Payload và data buffer do caller sở hữu, sống đến hết `b_transport`. Không giữ con trỏ
payload sau return. Không cấp DMI. Địa chỉ là byte, little-endian, streaming_width
phải >= data_length; không hỗ trợ streaming wrap. Byte enable tuần hoàn theo TLM,
mỗi phần tử 00 hoặc FF; chiều dài byte enable phải >0 nếu có pointer.

## Register port

`EncoderVp::registers.socket`: 32-bit aligned access, đúng 4 byte.
Sai offset/alignment: ADDRESS_ERROR; write RO/live config: COMMAND_ERROR.
Đây là chính sách VP rõ ràng cho các hành vi chưa xác định đầy đủ trong PDF.
Side effect read-clear chỉ xảy ra một lần ở access được chấp nhận có ít nhất một byte enable.
IRQ chỉ được một SC_METHOD drive, tránh multiple-writer dù register access và controller
thực thi trong các process khác nhau.

## DMA / memory

`DmaArbiter::clients`: nhiều initiator bind vào multi-target socket. Owner ID là binding
index, giữ đến hết toàn request kể cả mọi segment và response. Queue chọn priority nhỏ
trước, FIFO khi cùng priority; request bị vượt 8 lần được ưu tiên để chống starvation.
Adapter gọi set_priority(owner, rank) theo release. Mặc định cùng priority; không tự gán
thứ tự CMB/SW/NAL/DF vì PDF không liệt kê priority encoder cụ thể.

`DmaBridge::input`: vùng byte liên tiếp, địa chỉ 32-bit. Chia theo max_beats (mặc định 16),
bus width, 4 KiB. Mỗi segment là các beat căn chỉnh với byte enable để bảo toàn lane.
Đây là abstraction byte-range; chưa mô hình chính xác progression của từng legacy beat.

`DmaBridge::memory`: nối DDR qua TLM. Lỗi target được truyền về caller, không biến thành normal.
`EpochExtension` gắn token reset vào request của pipeline. DDR kiểm tra token sau latency,
trước commit. Arbiter không giải phóng owner giữa chừng khi reset.

## Functional adapter

`ProcessingIf::process(Macroblock)` là blocking interface hiện tại. Một macroblock gồm
256 byte Y + 64 U + 64 V, từng plane theo hàng; x/y là tọa độ macroblock, frame là index
trong activation. `ProcessedBlock` chứa reconstruction và output bytes.

Pipeline stub hiện yêu cầu output đúng 4 byte/MB. Encoder hoàn chỉnh cần adapter frame
executor mới với entropy length biến đổi; không buộc entropy thật vào interface checksum.
`FrameExecutorIf` nằm trong `interfaces/include/h264/frame_executor_if.h`. Top nhận
`PipelineFactory` để tạo pipeline lúc elaboration; factory bind các DMA client vào arbiter.
`reset()` không wait; `begin_activation()` khởi tạo state; `execute()` trả số word mới đã
commit (có thể 0 nếu còn buffering). `end_activation()` chờ toàn bộ output/reference DMA
và trả **tổng** số word của activation. Controller không tự thêm EOS hoặc suy ra length từ MB.
Only stub formatter adds its own EOS. Adapter phải kiểm tra generation sau mỗi wait và
không phát request sau khi finalization đã trả về.

Mapping đã có trong PDF bảng 20-3 (trang 53–55). ControlRegs decode SPARA0/1, DFCON,
SPARA2 từ giá trị phần mềm ghi, đồng thời giữ raw fields trong FrameConfig. SequenceParameters
ở HostDriver chỉ giúp encode register; trong EncoderVp chỉ nal_capacity_bytes là cấu hình
allocation ngoài register. QP/frame count của constructor không override register.
`nal_capacity_bytes` là allocation contract của VP, không phải register mới;
DMA client phải kiểm tra capacity trước khi ghi. Kiểm tra length ở controller sau completion
không thể rollback một write vượt vùng đã xảy ra.

## Progress và lịch picture

Adapter bắt buộc implement syntax_requirements và abort_and_drain; xem
`control/docs/recovery_and_syntax.md` từ root. FN/POC preflight chạy trước
begin_activation. Error sau begin chỉ clear BUSY khi adapter xác nhận đã ngừng producer và
drain mọi transaction. Drain thất bại giữ BUSY cho đến reset.

Pipeline gọi nal_words_accepted(generation,count) đúng một lần tại lúc NAL DMA nhận words.
STM_LEN tăng ngay, kể cả response đang chờ hoặc sau đó lỗi; normal/IRQ vẫn phải chờ drain.
end_activation trả tổng accepted words và phải khớp STM_LEN. Nếu lỗi, giữ counter đã nhận.

FrameConfig có completed_frames và activation_frames. Preload activation dùng tối đa
CMB_ON_MEM frame; frame-address dùng một picture/base do host chọn. validate vùng CMB
theo working set, không theo tổng FMENC. Source slot và coding/display index nằm trong
PictureTask. Controller giữ sequence progress qua CMB/NAL/batch updates, bắt duplicate
display index và gọi execute_picture. N=1 có lịch I/P theo M; N>1 yêu cầu adapter override
picture_task bằng coding order của release. Thiếu lịch => ERROR, không chạy nhầm raster.
