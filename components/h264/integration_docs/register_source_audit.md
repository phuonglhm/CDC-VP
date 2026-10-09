# Nguồn register map và bản sửa chức năng — 2026-10-09

Đã tìm các tên axi_pkg, h264_ctrl, SPARA0/SPARA1, h264_axi_wrapper trên web và trong
Downloads; không xác định được source release phù hợp từ kết quả tìm được.
Nguồn quyết định thực tế nằm trong chính PDF workspace: §20.2, bảng 20-3, trang 53–55,
ghi rõ field-level definition từ release 2.1.11. Không cần mượn register map của encoder khác.

Nguồn công khai tham khảo:
[VNU-UET: System-on-Chip Testbed for Validating the Hardware Design of H.264/AVC Encoder](https://eprints.uet.vnu.edu.vn/eprints/id/eprint/417/).
Đây là tài liệu testbed lịch sử; không dùng nó để suy đoán mapping AXI hiện tại.

Đính chính: nhận định cũ trong trao đổi/docs rằng PDF thiếu mapping SPARA0/1 và reset values
là do chưa đọc bảng 20-3. Code/docs hiện đã sửa theo phụ lục, không phải thiết kế field mới.

Các thay đổi thực hiện: decode register, reset SPARA0, working-set activation, live accepted-word
counter, timeout recovery/diagnostics, state/lịch picture qua adapter, configurable priority queue,
và IRQ mask giữ pending event. Xem register_behavior.md và model_limitations.md để phân biệt
phần theo bảng PDF với policy VP chưa được release xác nhận.
