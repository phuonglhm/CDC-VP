# Đối chiếu workbook và quyết định FX1

Nguồn sơ đồ là [Bus Config.xlsx](../Bus%20Config.xlsx). Workbook chứa cả ảnh FX1 và sơ đồ SYSBUS_1/SYSBUS_0/PERIBUS.

## Đối chiếu cập nhật ngày 2026-10-06

So với bản workbook đã đối chiếu trước đó, ba ảnh đầu giữ nguyên từng byte; ảnh FX1 thứ tư thay đổi:

| Thay đổi trong ảnh FX1 | Ảnh hưởng tới Bus System |
|---|---|
| Nhãn `x264 DMA` đổi thành `H264/H265 DMA` | Đã khớp tên `H264_H265_DMA` và `H264_H265_CSR` trong cấu hình; không cần đổi API. |
| Bỏ nhánh trực tiếp MIPI → NPU; nhánh tới NPU xuất phát từ đầu ra ISP | Đây là mô tả luồng dữ liệu giữa IP. Chưa có chỉ định giao thức cho các mũi tên này nên không tự chuyển thành route AXI/APB. Platform chịu trách nhiệm ghép các giao diện dữ liệu của IP. |

Bảng `Bus Parameter` không có thay đổi giá trị ô so với bản lưu đối chiếu. Lần cập nhật này không bổ sung địa chỉ cho các target TBD và không yêu cầu đổi bốn đường routing hiện có. Các quyết định đã chốt bên dưới vẫn là cơ sở triển khai.

Ảnh tổng quan chưa thể hiện rõ kết nối SRAM và các cổng AXI của ISP. Vì vậy vẫn giữ hai master `ISP_IDMA`/`ISP_ODMA`, MIPI chỉ có CSR, và SRAM disabled chờ map theo xác nhận trước đó; không suy ra thêm master từ mũi tên hai chiều trên ảnh.

## Quyết định triển khai đã chốt

- Hai sơ đồ cùng tồn tại: sơ đồ FX1 xác định IP/master, sơ đồ SYSBUS xác định các đường nối nội bộ.
- Số output/tên target lấy từ descriptor; không cố định 5+3. `SysBus0Axi` cho phép AES/QSPI/IP mới nằm sau SYSBUS_0.
- CPU1, CPU2, SYS_DMA, ISP_IDMA, ISP_ODMA, NPU_DMA, H264_H265_DMA, ETH_DMA là các named initiator. MIPI chỉ có CSR target.
- BootROM và MEMCTL_DDR nối AXI/SYSBUS_1; CLINT/PLIC/UART qua PERIBUS_0. System CSR thuộc PERIBUS_0, multimedia CSR thuộc PERIBUS_1.
- Map năm target mặc định đến từ yêu cầu FX1 đã thống nhất, không được suy ra từ bảng tín hiệu AXI. SRAM/IRAM/CSR chưa có map giữ disabled.
- Data width chốt 32 bit; address width mặc định 32. Hai width `RREADY`/`BREADY` có ký tự lỗi trong workbook được hiểu là 1 theo xác nhận của chủ thiết kế. Không chỉnh binary workbook trong lượt sửa code này.
- FIFO được chọn trong hai phương án FIFO/round-robin đã yêu cầu; không tuyên bố round-robin.

## Ánh xạ hành vi

| AXI/diagram | TLM |
|---|---|
| AWADDR/ARADDR | `payload.address` và command WRITE/READ |
| WDATA/RDATA | `data_ptr`, `data_length` |
| WSTRB | byte enable, phải được target thực hiện |
| BRESP/RRESP | `response_status`; không phải mã AXI 2 bit |
| MUX/arbitration | FIFO riêng từng output router |
| Decode/DEMUX | Exact leaf windows, holes trả address error |
| BID/RID/USER/PROT/QOS/ATOP | Chưa mô phỏng các thuộc tính tương ứng |
| VALID/READY, LEN/SIZE/BURST/LAST | Chưa có handshake hoặc beat-level AXI protocol |

APB functional có giới hạn alignment và 1/2/4 byte. Không tự split AXI burst. `transport_dbg` là backdoor không timing. Router timing 2 ns và APB cycle 10 ns mặc định là giả định test, không phải timing chính thức từ Excel.

Regression kiểm tra cả cấu hình topology cũ (ROM/ISRAM/DSRAM/AES/QSPI và peripheral 6/2) lẫn map FX1, thêm IP/master, FIFO và lỗi truy cập. Các địa chỉ AES/QSPI/peripheral trong legacy fixture chỉ là map test.
