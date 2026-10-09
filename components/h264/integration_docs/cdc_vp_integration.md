# Tích hợp CDC-VP sau khi đối chiếu PDF

Đối chiếu ngày 2026-10-09 với PDF v2.1.11, chương 3/4/5/13 và bảng 20-3
(trang 53–55). CDC-VP tham chiếu: commit `528007d`, platform `fx1_soc`.
Component hiện nằm trong components/h264. Các lệnh build bên dưới chạy từ thư mục này; lịch sử kiểm thử ban đầu nằm trong verification_report.md.

## Ưu tiên spec

- Giữ nguyên offset, bit field và reset value theo bảng 20-3; SPARA0 reset `0x12c`.
- CSR chỉ nhận offset local và truy cập 32-bit aligned, áp dụng byte enable.
- ECE rising bắt đầu activation; cấu hình lại theo disable → drain → reprogram → enable.
- NORMAL/IRQ chỉ sau output/reference response; đọc STAT/STM_LEN clear IRQ.
- STM_LEN đếm NAL words accepted theo activation, không đổi thành byte count.
- DMA giữ owner, chia burst/lane và biên 4 KiB; reset vẫn active-low, release sau
  hai chu kỳ mô hình. Địa chỉ SoC không được đưa vào register map của core.
- Phần lịch B-picture, yêu cầu độ rộng FN/POC và priority của bản release vẫn cần
  adapter/IP đúng từ nhóm. Không suy đoán thuật toán để làm test pass. Xem
  `model_limitations.md`; mức LT không được tính là sai khác spec.

## Đóng gói

Component nằm tại `components/h264/`; repo cha có thể thêm bằng `add_subdirectory(h264)`. Repo cha tạo
`SystemC::systemc` 2.3.4 trước; h264 dùng lại target này. C++17 là yêu cầu public.

```cmake
# Trong components/CMakeLists.txt của repo đích, khi quyết định tích hợp:
option(CDC_BUILD_H264 "Build H264 control/DMA integration" OFF)
if(CDC_BUILD_H264)
  add_subdirectory(h264)
endif()
# Platform link cdc::components::h264_tlm.
```

Các tùy chọn của component:

| Tùy chọn | Ý nghĩa |
|---|---|
| H264_BUILD_TESTS | Mặc định theo CDC_BUILD_TESTS khi nhúng; standalone ON |
| H264_BUILD_EXAMPLES | Standalone ON; nhúng OFF |
| H264_BUILD_STUBS | Standalone ON; nhúng OFF; integration_tests/example tự cần fixture này |
| H264_BUILD_CDC_TESTS | Mặc định OFF; test component FX1 từ source ngoài |
| H264_CDC_VP_ROOT | Đường dẫn source CDC-VP, chỉ đọc |
| H264_USE_CDC_EXPORT | Tự ON khi CMAKE_PROJECT_NAME là cdc-vp |

Core không link stub. Khi dùng stub phải link thêm `h264_pipeline_stub`; khi có
IP thật truyền factory của adapter và link target của nhóm. Constructor mặc định
của EncoderVp dùng factory stub, vì vậy ứng dụng chỉ link core phải truyền factory.
HostDriver/DdrMemory là hỗ trợ test, không phải bộ nhớ hay CPU của SoC.

Các thư viện core và header có install/export. Ngoài CDC có thể dùng
`find_package(h264-vp CONFIG REQUIRED)` rồi link `cdc::components::h264_tlm`.
Trong CDC dùng export set `cdc-components-targets` do repo cha quản lý.

## Kết nối FX1

Mẫu kết nối đã biên dịch/chạy nằm trong `integration_tests/cdc_fx1/cdc_fx1_tb.cpp`:

1. Cấu hình target `H264_H265_CSR`: enabled, base từ
   `FX1_H264_H265_CSR_BASE`, size `FX1_APB_SLOT_SIZE`. Bus tự dịch về offset local.
2. Bind target đó vào `encoder.registers.socket`.
3. Bind `encoder.bridge.memory` → `fx1::WriteGuard` →
   `fabric.initiator("H264_H265_DMA")`. Guard phải dùng **exclusive monitor chung
   với CPU**, master ID riêng chưa ai dùng. Test dùng `0x102` (SYS_DMA/ISP hiện
   dùng `0x100`/`0x101`). FX1 hiện dùng bus 32-bit nên chọn bridge width 32.
4. Bind rstn active-low; bind IRQ vào PLIC source `FX1_IRQ_H264_H265`.
   Platform hiện dành source 4–7 cho `sim.test_irq`: phải bỏ/đổi kết nối test
   source 5 trước khi nối encoder. Không dùng hai writer trên cùng sc_signal.
5. CMB/REFM/NAL là địa chỉ vật lý DDR. Test dùng DDR_BASE + các offset không
   chồng nhau. Platform phải dành vùng không đè firmware/stack/heap.

Map hiện tại: CSR `0x11020000`, IRQ `5`, DDR `0x80000000`. Đây là placeholder VP
trong fx1_memory_map.h, chờ HAS, không phải hằng số của PDF H264. HostDriver mới
nhận csr_base ở constructor; các hàm read/write vẫn nhận offset trong PDF.

## Reset khi dùng memory ngoài

EpochExtension bảo vệ trạng thái controller và chặn request cũ chưa gửi. RAM ngoài
không bắt buộc hiểu extension và giao dịch đã nhận có thể còn ghi sau reset.
Không dùng STAT.BUSY=0 sau reset làm bằng chứng external DMA đã quiescent.

Ở SC_THREAD quản lý reset của platform:

```cpp
rstn.write(false);
wait(sc_core::SC_ZERO_TIME); // để reset method chạy; giữ reset trong lúc drain
wait(sc_core::SC_ZERO_TIME);
if (!encoder.arbiter.wait_idle(sc_core::sc_time(10, sc_core::SC_US))) {
    // Giữ reset và quyền sở hữu buffer; báo recovery timeout, không reuse.
} else {
    // Producers đã được reset/dừng; mọi giao dịch trong arbiter đã trả về.
    // Có thể quản lý lại buffer rồi nhả reset.
    rstn.write(true);
}
```

`wait_idle()` chỉ chờ các request của arbiter; adapter thật phải thực hiện hợp đồng
reset dừng producer, không phát request mới thuộc generation cũ. Hàm không hủy
payload, không rollback RAM và không kéo dài reset synchronizer của core.

## Kiểm thử tái lập

Sau khi configure standalone với SystemC như README gốc:

```text
cmake -S . -B build/vp -DH264_BUILD_CDC_TESTS=ON -DH264_CDC_VP_ROOT=<CDC-VP>
cmake --build build/vp -j 8
ctest --test-dir build/vp --output-on-failure
```

Test mới biên dịch trực tiếp source bus, SparseRam, PLIC, ExclusiveMonitor từ repo
ngoài vào build của h264_demo. Không copy/sửa source CDC-VP; không dùng mock thay
cho các component đó. Test kiểm tra CSR/WSTRB, output/reference DDR, LEN, hai lần
activation, IRQ claim/ack/complete, LR reservation invalidation, và reset lúc NAL
write bị atomic bracket giữ lại (timeout → drain → không có stale completion).

Fixture `integration_tests/embedded` là project cha khác, nhận H264_SOURCE_DIR,
SYSTEMC_INCLUDE_DIR, SYSTEMC_LIBRARY qua CMake; dùng để kiểm tra PROJECT_SOURCE_DIR
khác và SystemC target đã tồn tại. Tắt CDC_BUILD_TESTS để kiểm tra chỉ build core.

Đây là kiểm chứng **component integration với stub**. Chưa chạy CPU RISC-V firmware,
chưa build toàn bộ platform fx1_soc, chưa xác nhận bitstream H264 bằng IP thật.
Không tự thay stub bằng `components/vpu_tlm`: interface/thuật toán đó phải được
đối chiếu riêng với bản release của nhóm.
