# FX1 DMA — SystemC/TLM

Component DMA trong `FX1_Components`, triển khai theo
[DMA_Config.xlsx](docs/DMA_Config.xlsx). Dùng
[DMA_HDS_Page_by_Page.xlsx](docs/DMA_HDS_Page_by_Page.xlsx) để bổ sung ý nghĩa
bit interrupt và trình tự vận hành. Đây là model functional/timing TLM, dùng
cho virtual platform; chưa phải mô hình pin-level AXI/APB hay cycle-accurate RTL.

## Cấu hình theo workbook

| Thuộc tính | Giá trị |
|---|---|
| Channels | 8, stride `0x100` |
| Channel registers | `0x0000..0x07ff` |
| Reserved region | `0x0800..0x0fff`, trả lỗi |
| Global registers | `0x1000..0x10ff` |
| Register aperture cần map | **`0x1100` byte** |
| FIFO | 32 byte/channel |
| APB3 | Slave, register access 32 bit |
| AXI metadata | Address/data 32 bit, ID width 4, ID cố định `4` |
| Burst | FIXED/INCR, tối đa 16 beat |
| Pending | Tối đa 4 read + 4 write toàn core |
| Transfer size | Byte count 10 bit (`0..1023`) |
| Arbitration | Weighted round-robin riêng cho read/write, token `[21:16]` |
| Transfer direction | M2M, M2P, P2M |
| Capability 0 | `0x0a602258` |

Toàn bộ offset, bit mask và giá trị reset nằm trong
[`include/dma/registers.h`](include/dma/registers.h). Model mới là
`fx1::dma::Dma`, target CMake `cdc::components::fx1_dma`.
`components/dma_tlm` là model có register map/tập lệnh khác và vẫn được build
riêng cho các platform đang sử dụng nó.

## Build từ thư mục FX1_Components

Yêu cầu C++17, CMake, SystemC 2.3.4 được build bằng toolchain tương thích.
Build và chạy regression C++ không cần Python hay file Excel.

```bash
cd /mnt/d/Intern_doc/Project/CDC-VP/components/FX1_Components
make -C dma BUILD_DIR=/tmp/cdc-fx1-dma-build test
```

`SYSTEMC_HOME` mặc định `/opt/systemc-2.3.4`. Có thể truyền prefix khác:

```bash
make -C dma SYSTEMC_HOME=/path/to/systemc BUILD_DIR=/tmp/cdc-fx1-dma-build test
```

Build bằng CMake, kèm regression qua bus FX1:

```bash
cmake -S dma -B /tmp/cdc-fx1-dma-build -G Ninja \
  -DSYSTEMC_HOME=/opt/systemc-2.3.4 \
  -DFX1_DMA_BUILD_TESTS=ON -DFX1_DMA_BUILD_BUS_TESTS=ON
cmake --build /tmp/cdc-fx1-dma-build --parallel 2
ctest --test-dir /tmp/cdc-fx1-dma-build --output-on-failure
```

Thư mục `/tmp` tránh lỗi CMake `Operation not permitted` khi copy file và giữ
permission trên một số mount `/mnt/d`. Nếu mount của bạn hoạt động bình thường,
có thể dùng `dma/build`. Build riêng production library bằng
`-DFX1_DMA_BUILD_TESTS=OFF`, sau đó `cmake --build ... --target fx1_dma`.

Từ repository root, `components/CMakeLists.txt` đã thêm component; target
`test_fx1_dma` dùng chung `SystemC::systemc` của parent.

## Kết nối vào VP

```cpp
#include "dma/dma.h"

fx1::dma::Dma dma{"dma"};
dma.master_socket.bind(fabric.target);       // DMA initiator -> SYSBUS_1
fabric.pp1[dma_port].socket.bind(dma.target_socket); // CPU -> APB registers
dma.reset_n(reset_n_signal);
dma.irq(dma_irq_signal);                     // Một interrupt tổng hợp
dma.rx_request(rx_request_bitmap);
dma.tx_request(tx_request_bitmap);
dma.rx_clear(rx_clear_bitmap);
dma.tx_clear(tx_clear_bitmap);
```

Top-level sở hữu bus, RAM, peripheral và mọi signal. Nếu chưa dùng peripheral,
vẫn bind `rx_request`/`tx_request` vào signal có giá trị 0 và bind hai output
clear. Bit `p` của request/clear ứng với peripheral ID `p`; ID 0 là memory.
Request ngoài được giữ đến clear, sau đó phải hạ xuống trước request tiếp theo.
Clear phát xung trong một `Config::cycle`. Software request được ghi qua
`PERIPHERAL_RX_REQUEST`/`PERIPHERAL_TX_REQUEST` và tự clear khi service kết thúc.

Address map do platform quyết định. **Vùng DMA phải dài ít nhất `0x1100` byte**;
slot APB `0x1000` byte mặc định của bus không chứa được global registers.
[`tests/test_bus.cpp`](tests/test_bus.cpp) minh họa cấu hình và bind đầy đủ.

AXI burst dùng `b_transport`. INCR chuyển một buffer liên tiếp; FIXED gửi từng
beat tại cùng địa chỉ để tương thích bus hiện tại. Extension
[`AxiExtension`](include/dma/axi_extension.h) mang channel, ID, unit size,
số beat, cache/protection, command-fetch và thông tin fragmentation. Target
có thể bỏ qua extension. Target có thể `wait()` hoặc trả annotated delay;
DMA tiêu thụ delay một lần.

## Ví dụ M2M

Với channel 0, offset tương đối từ `DMA_BASE`:

```text
0x0040 = 0x00000000   // Disable khi cấu hình
0x00a4 = 0x00001fff   // Clear interrupt cũ
0x0000 = SRC_ADDR
0x0004 = DST_ADDR
0x0008 = 0x00000100   // 256 byte
0x000c = 0x00000003   // CMD_SET_INT=1, CMD_LAST=1
0x0010 = 0x81010008   // Read increment, 8 byte/burst, 1 token
0x0014 = 0x81010008   // Write increment, 8 byte/burst, 1 token
0x0020 = 0x00000000   // M2M
0x0040 = 0x00000001   // Enable
0x0044 = 0x00000001   // Start
```

Poll `CH_ACTIVE_STATUS`/`CH_OUTSTANDING_STATUS` hoặc chờ `irq`, rồi đọc
`CH_INTERRUPT_STATUS` và clear bằng `CH_INTERRUPT_CLEAR`. Clear bit completion
acknowledge một event; IRQ còn giữ nếu có event completion tiếp theo.
Disable ngăn grant mới; transaction đã dispatch vẫn được drain. Enable lại
tiếp tục command đang dở. Ghi start khi đang active bị bỏ qua.

## Phạm vi và lựa chọn đã chốt

Bạn đã xác nhận đơn vị truyền suy ra từ burst size/căn chỉnh địa chỉ và layout
next-command gồm 4 word little-endian: source, destination, byte count,
control. Model không thêm bit chọn transfer size vào register map.

[`docs/spec_mapping.md`](docs/spec_mapping.md) ghi nguồn, các điểm chưa được
workbook định nghĩa, policy của model và giới hạn mô phỏng. Watchdog/transaction
timeout là tham số integration; mặc định tắt vì workbook chưa chốt threshold.
Không mô phỏng riêng AR/AW/W/R/B handshake, RID/BID/RLAST, adapter DTYPE/DRLAST
hay timeout từng phase. Những bit interrupt này vẫn đọc/ghi/clear được để
software verification.

## Regression

Các test kiểm tra register/reset/mask, reserved error, payload byte-for-byte,
burst boundary, narrow/FIXED transfer, endian swap, command chain/completion
queue, pending/FIFO reservation, arbitration 8 kênh, peripheral handshake,
disable/error/restart, reset khi đang có giao dịch và timeout tùy chọn.
Test bus dùng router/AXI–APB bridge của `FX1_Components/bus`.

`ctest` chạy 9 test C++ của DMA; bật `FX1_DMA_BUILD_BUS_TESTS=ON` sẽ có thêm
test bus, tổng cộng 10 test.
