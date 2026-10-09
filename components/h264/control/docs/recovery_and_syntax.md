# Error drain và FN/POC preflight

## Hợp đồng bắt buộc với adapter

FrameExecutorIf bổ sung hai phương thức pure virtual; adapter cũ phải cập nhật trước khi build.

`syntax_requirements(config)` không wait, không phát DMA hay chạy producer. Trả số bit cần
cho frame_num/POC của lịch thực, và số bit cấu hình sau khi adapter giải mã register.
Controller kiểm tra configured >= required và không quá 32 bit trước begin_activation.
Không dùng một công thức frame_count cho cả FN/POC, không tự đoán encoding minus-4.
Nếu release không thể xác định yêu cầu, adapter phải throw runtime_error thay vì trả zero.

Processing checksum và contract fixture trả {0,0,0,0} vì không phát H.264 syntax. Đây là
miễn kiểm tra có chủ đích chỉ cho fixture; không chứng minh cấu hình FN/POC của H.264 thật.
Real adapter phải tính requirements từ quy tắc frame_num, POC, reset/wrap và coding order.

`abort_and_drain(generation)` ngừng producer/request mới rồi chờ TẤT CẢ client đã gửi việc
được trả response (gồm transaction đang queued ở arbiter). Payload phải còn sống đến return.
Chỉ trả true khi không còn pending writer và producer không thể phát thêm request.
Không cần thay đổi arbiter: producer dừng ở adapter, arbiter tiếp tục xử lý hàng đợi đã nhận.

## Trạng thái controller

- Preflight lỗi trước begin: không có công việc đã phát, báo ERROR bình thường.
- Lỗi từ begin/execute/finalize: giữ BUSY, gọi abort_and_drain. Chỉ sau true mới finish ERROR/IRQ.
- Drain trả false hoặc ném exception: giữ BUSY, không báo completion IRQ, chờ reset.
  Host timeout giữ buffer, không được reuse chỉ vì đã hết thời gian grace.
- Reset trong drain: adapter thức dậy theo reset.changed, không sửa state generation mới;
  controller bỏ completion cũ. Request DMA cũ dùng EpochExtension để không commit sau reset.
- STM_LEN giữ accepted-word count; lỗi không biến partial output thành bitstream thành công.

Stub hiện chỉ dùng blocking transport trong một thread nên khi unwind không còn request nền;
abort_and_drain của stub trả true. Không sao chép implementation này vào adapter concurrent.

## Test

recovery_syntax_tb có worker DMA ghi trễ 200 ns; thread điều khiển báo lỗi sau 10 ns.
Kiểm tra BUSY/IRQ/quyền ghi base trong lúc drain, data completion, reset giữa drain,
drain false/exception giữ BUSY, width vừa đủ và thiếu một bit FN/POC trước khi có DMA.
Policy width 4/5 literal trong test chỉ kiểm tra cơ chế, không phải assertion về release mapping.
