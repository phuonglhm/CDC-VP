# Đánh giá Bus System trước khi bàn giao CDC-VP

Phạm vi: bản phát triển trong `Bus System`, dự kiến thay thế component FX1 bus. Không đánh giá bus cũ đang nằm trong repo, không tính việc sửa CMake cấp cha là lỗi của component. Không stage, commit, push hoặc copy source sang CDC-VP trong lượt này.

## Các mục đã xử lý

- Chốt data width 32 bit và từ chối cấu hình data width khác.
- Bổ sung `SysBus0Axi`, sinh số output và exact routing windows từ descriptor. Test cả topology cũ và FX1, nhánh rỗng và nhánh chỉ có một target.
- Phục hồi regression byte enable, boundary, payload lỗi, APB, debug, timing, DMI và local address.
- Kiểm tra named lookup, thêm/reorder master/IP, bật TBD, số target khác nhau ở hai PERIBUS.
- FIFO tự giải phóng khi IP ném exception; test pending requests và request sau lỗi trên cả bốn path.
- Kiểm tra thứ tự FIFO cho tám master phát nhiều giao dịch liên tục và tiến triển của một output độc lập.
- CMake nhúng/standalone, static/shared, install/export và consumer đã cài đều được kiểm tra.
- Đồng bộ README, integration guide, workbook mapping và handoff theo API mới.
- Đối chiếu lại workbook ngày 2026-10-06: nhãn H264/H265 đã khớp cấu hình; nhánh dữ liệu ISP → NPU thuộc wiring giữa IP, không thay đổi routing memory-mapped. Chi tiết tại [WORKBOOK_MAPPING.md](WORKBOOK_MAPPING.md).

## Bằng chứng kiểm thử

Môi trường xác nhận: WSL/Linux, GNU C++ 15.2, SystemC 2.3.4 shared. Chưa chạy Windows native/MSVC hoặc MinGW trong lượt này.

| Kiểm tra | Kết quả |
|---|---|
| Behavior | 3.888 assertions, 0 failures |
| Config | 26 assertions, 0 failures |
| Arbitration | 956 assertions, 0 failures |
| Ba suite trên với libbus_system static | 3/3 PASS |
| Ba suite trên với libbus_system shared và libsystemc.so.2.3.4 | 3/3 PASS |
| CDC memory_tlm + ELF loader integration | 28 assertions, 0 failures |
| Consumer qua standalone installed shared package | PASS |
| Consumer qua CDC installed export set | PASS |

Các con số là số assertion, không phải line/branch coverage. Consumer tạo bus từ package đã cài và thực hiện giao dịch tới target trên SYSBUS_0, không chỉ include header. Integration đọc source CDC-VP; output build/install chỉ nằm trong Bus System.

## Điểm mạnh

Bus độc lập với IP; API binding theo tên; map và output count cấu hình được; giữ bốn đường của topology; FIFO có kiểm tra thứ tự và exception recovery; hỗ trợ functional/debug transport. Bằng chứng build gồm thư viện static và shared, cùng consumer dùng package đã cài. Tài liệu mô tả hợp đồng IP và giới hạn thay vì suy diễn test component thành boot toàn hệ thống.

## Giới hạn và rủi ro còn lại

1. Cần chốt map SRAM/IRAM/CSR TBD và chạy workload CPU/DMA/boot với IP thật. Đây chưa được xác nhận bởi regression component.
2. APB chỉ 1/2/4 byte; driver phát truy cập 8 byte tới CLINT/CSR sẽ bị từ chối. Chưa có split với quy tắc register side effects.
3. Byte enable được chuyển xuống IP; memory_tlm của CDC hiện không masking strobes. Đội IP phải xác nhận hợp đồng này.
4. FIFO bảo đảm thứ tự request đã xếp hàng khi downstream hoàn tất; không QoS theo master/byte, không timeout target treo. Các leaf dùng chung upstream link vẫn tuần tự hóa.
5. Decode đang duyệt tuyến tính theo số region. Chưa benchmark tải lớn hoặc tối ưu hiệu năng mô phỏng.
6. Cấu trúc SYSBUS/PERIBUS vẫn là topology định sẵn với port động; chưa phải graph interconnect tùy ý. Chưa nonblocking/DMI/reset/clock domain hoặc AXI beat-level protocol.
7. Test exception recovery không thay thế test process reset/kill hoặc reset phần cứng; các chức năng reset không thuộc public contract hiện tại.

## Mức phù hợp ước lượng

Khoảng **92/100** cho mục tiêu component behavioral TLM 32-bit để bàn giao CDC-VP. Đây là đánh giá kỹ thuật theo phạm vi, không phải chứng nhận protocol hay phép đo coverage.

| Tiêu chí | Điểm ước lượng |
|---|---:|
| Tính độc lập, API và cấu trúc component | 19/20 |
| Chức năng routing/topology/validation theo yêu cầu | 28/30 |
| Regression và độ tin cậy | 23/25 |
| Build, package và bằng chứng tích hợp | 13/15 |
| Tài liệu bàn giao | 9/10 |

Phần chưa đạt chủ yếu là kiểm chứng platform thật, benchmark và tính di động toolchain. Không xem các chức năng AXI cycle-accurate ngoài phạm vi là điều kiện bắt buộc trước khi bàn giao component behavioral.
