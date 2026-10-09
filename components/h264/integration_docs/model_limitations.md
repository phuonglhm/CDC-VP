# Phạm vi VP và trạng thái chức năng

## Đã cập nhật theo PDF

- Register decode/reset theo bảng 20-3 (trang 53–55), không còn mapping giả định.
- QP/FMENC/GOP/CMB mode/working set lấy từ register. Filter/crop/syntax fields chuyển tới adapter.
- CMB validation theo working set; frame-address một source picture/activation; counters qua activation.
- STM_LEN cập nhật khi NAL DMA accepts word; giữ progress sau lỗi; IRQ normal chờ final responses.
- Host timeout snapshot, disable, bounded recovery, lưu trạng thái/output khi bus đã quiescent.
- Arbiter giữ owner, có queue priority cấu hình và aging để tránh starvation.

## Chờ IP thật

Prediction/TQ/entropy/deblocking/memory và DMA clients của Vinh/Nguyên chưa release.
Stub vẫn checksum/copy-reference; golden fixture không phải H.264 golden. Các callback
accepted-word, reset, picture schedule và completion phải nối vào client thật.

## Điểm chưa đủ nguồn để khẳng định giống release

- FN/POC validation đã có preflight bắt buộc và test biên. Công thức yêu cầu độ rộng và
  encoding của release được adapter thật cung cấp; checksum fixture không phát syntax nên
  khai báo zero requirements. Chưa claim kiểm chứng FN/POC encoder thật.

- Coding/display order B-picture, GOP tail/boundary và slot lifetime cụ thể cần adapter
  theo Algorithm Specification/driver release. M/N bit mapping đã rõ; thuật toán reorder
  không suy ra duy nhất từ bảng register. Default N>1 bị từ chối rõ ràng.
- Priority order CMB/SW/NAL/DF không được liệt kê trong PDF. Adapter cấu hình rank; default
  cùng rank/FIFO, aging sau 8 lần bị vượt là policy VP, không claim bản sao priority RTL.
- Disable khi busy drain activation đã nhận; boundary abort/reset accounting và các update
  config ngoài trình tự chuẩn vẫn cần đối chiếu release. Host không reuse buffer nếu BUSY còn set.
- Unknown register response và sequence-restart policy khi thay cấu hình dùng quy tắc VP đã ghi
  trong register_behavior.md. Giá trị reset không còn nằm trong nhóm chưa rõ.

## Phạm vi mô hình đã thống nhất — không tính là khác spec

Loosely timed TLM: không kiểm chứng handshake AXI theo từng chu kỳ, timing/CDC/metastability,
FIFO pressure hoặc CPU cache. Không mở rộng sang RTL/cycle-accurate để giải quyết các phần này.
Latency là tham số simulation; kết quả thời gian không phải throughput encoder thật.

## Reset và diagnostic

Với RAM ngoài không hiểu EpochExtension, platform phải giữ reset, dừng producer và
chờ `arbiter.wait_idle(timeout)` trước khi reuse buffer. Timeout giữ nguyên quyền sở
hữu buffer; không được coi BUSY đã clear do reset là DMA đã drain. Regression với
RAM/bus thật của FX1 kiểm tra trường hợp này; xem `cdc_vp_integration.md`.

Epoch invalidates request/completion cũ; DDR giữ segment đã commit, không rollback.
Pending payload sống đến response. Host timeout không ngắt cưỡng bức giao dịch đang live:
disable và chờ grace; nếu vẫn BUSY thì giữ snapshot và buffer, yêu cầu reset trước reuse.
STM_LEN khi error đếm accepted words; diagnostic output không được dùng như chunk thành công.
