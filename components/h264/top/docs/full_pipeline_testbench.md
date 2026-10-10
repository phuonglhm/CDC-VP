# Full-pipeline testbench và thay stub bằng module release

## Chạy được ngay

Các lệnh dưới chạy từ thư mục components/h264.

```powershell
cmake --build build/vp -j 8
ctest --test-dir build/vp --output-on-failure
./build/vp/bin/full_pipeline_tb.exe stub tests/vectors/full_pipeline/stub.cfg build/full_pipeline_stub
```

Test này chạy host → registers/controller → pipeline → DMA → DDR → IRQ → output,
hai activation, hai frame/activation, bus 128-bit và DDR latency 50 ns. Stub output chỉ là
checksum, không phải bitstream H.264. pipeline_contract_tb dùng adapter thử nghiệm khác:
variable word count, buffering đến final drain, không EOS; chứng minh controller không
phụ thuộc công thức length của stub. Adapter thử nghiệm không được chọn trong executable thật.

## Khi Vinh và Nguyên release module

1. Các task của hai người export CMake library và public header .h; unit test pass.
2. Huy tạo assembly adapter implement FrameExecutorIf và factory make_released_pipeline.
   Assembly giữ ownership các module prediction/DMA/coding/memory; nối từng DMA initiator
   vào DmaArbiter::clients được factory cung cấp. Không dùng lại FramePipelineStub cho real.
3. Adapter CMake export target `h264_released_pipeline`, link target của nhóm + h264_top.
   Adapter mẫu đã được bỏ khỏi gói này. Khi có IP thật, cung cấp thư mục adapter riêng qua H264_RELEASED_PIPELINE_DIR; hợp đồng được mô tả bên dưới.
4. Cung cấp input planar YUV và golden độc lập khớp đúng cấu hình; tạo case.cfg.
5. Cấu hình:

```powershell
cmake -S . -B build/vp -DH264_RELEASED_PIPELINE_DIR="C:/path/to/team_adapter" -DH264_FULL_PIPELINE_CASE="C:/path/to/case.cfg"
cmake --build build/vp -j 8
ctest --test-dir build/vp -R full_pipeline_real --output-on-failure
./build/vp/bin/full_pipeline_tb.exe real C:/path/to/case.cfg build/real_output
```

Không có adapter thì backend real trả lỗi; không fallback sang stub. Nếu chỉ có adapter mà
chưa có case, executable real build được nhưng CTest chưa đăng ký real qualification.
Link được không có nghĩa module thật đã qualified. Các vector có sẵn chỉ dùng cho fixture.

## Contract adapter

- `reset()`: được gọi trong SC_METHOD, không được wait; xóa/invalid state và giữ epoch.
- `begin_activation(config, generation)`: cấu hình module theo snapshot; reset counter/output base.
- `execute(config, generation, frame)`: chạy frame, trả số word mới đã commit; có thể 0
  nếu entropy giữ dữ liệu chờ flush. Huy cập nhật frame counter sau mỗi return.
- `picture_task`/`execute_picture`: bàn giao coding/display index, source slot và I/P/B.
  Adapter có B-picture phải cung cấp lịch release; fallback chỉ hỗ trợ N=1.
- NAL DMA callback phải gọi `nal_words_accepted(generation,count)` khi nhận packed words,
  không đợi finalization mới cập nhật STM_LEN. Finalization vẫn phải đợi phản hồi bộ nhớ.
- `end_activation(config, generation, committed_words)`: flush formatter, đợi **NAL và
  reference response cuối**, trả tổng word của activation gồm padding/EOS theo release.
- Không ghi vượt nal_capacity_bytes; lỗi ném std::runtime_error. Reset epoch cũ không
  commit hoặc cập nhật trạng thái mới. Kiểm tra epoch sau các wait.
- Config mang cả raw và decoded DFCON/SPARA0–2 theo bảng 20-3 trong PDF. Host encode
  QP/frame count vào register; constructor VP không còn quyết định các giá trị này.
  Case có raw spara0/spara1 override sẽ thay cấu hình; golden phải tương ứng.

## Case format

Text `key=value`, mỗi dòng một key, comment bắt đầu `#`, không thêm khoảng trắng quanh `=`.
Vector path tương đối với folder của case. `.hex` là danh sách byte hex cách nhau bằng
whitespace; mọi extension khác đọc raw binary. Thiếu file/golden hoặc key lạ gây lỗi.

```text
width=176
height=144
frames=1
qp=26
bus_width=32
latency_ns=40
timeout_ns=10000000
nal_capacity=65536
repeat=1
source=input.yuv
expected=golden.bin
```

source phải đủ frames * coded_width * coded_height * 3/2 byte. expected chứa chính xác
STM_LEN*4 byte, kể cả final word padding/EOS theo contract. Không tự sửa golden bằng output
của DUT. Repeat dùng cùng vector mặc định; `source.1`, `expected.1`... override activation
index bắt đầu từ 0 (ví dụ GOP state tiếp tục). Các case P/B cần đúng coding-order input.

Optional: dfcon, spara0, spara1, spara2 (integer decimal/0xhex), expected_ref và reference_offset
(byte offset trong REFM). expected_ref so sánh một vùng cụ thể, không đoán slot hữu ích.

## Kiểm tra và giới hạn

Harness kiểm tra normal/error/BUSY/frame count, timeout, IRQ acknowledge, STM_LEN, exact bytes,
hai guard 64-byte quanh NAL, vùng write hợp lệ, pending response lúc IRQ và DMA xuất hiện
trong 100 ns sau drain. Output thực được lưu activation_N.bin trước khi so golden nếu truyền
output-directory. Report lỗi byte đầu khác. Optional reference golden kiểm tra reconstruction.

DDR fixture 64 MiB: CMB=0x1000, REFM=0x01000000, NAL=0x03000000; NAL tối đa 8 MiB;
source/reference không được overlap. Đây là test layout, không quy định địa chỉ production.
Đổi bus_width/latency_ns trong case để chạy matrix 32/64/128 và các độ trễ release yêu cầu.
Harness chưa tự chạy decoder/PSNR, chưa kiểm chứng internal backpressure từng block;
unit test của Vinh/Nguyên và decoder validation vẫn là các gate riêng. Chưa có module thật,
do đó hiện chỉ qualified harness/adapter contract, chưa qualified full H.264 pipeline.
