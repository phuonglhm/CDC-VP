## Delivery into CDC-VP/components/h264 — 2026-10-09

The component was copied from h264_demo/encoder_integration and built in
components/h264/build/vp with C++17, MinGW GCC 12.1.0 and existing SystemC 2.3.4.
All 34 tests passed (2.01 s), including the test against FX1 bus/RAM/PLIC/monitor.
The CSR/DMA models remain unchanged; package entry, build script, embedding fixture
and README were adapted to this component directory.

Only components/h264 was changed. Root CMake integration was intentionally left
untouched. No commit or push was performed. Build outputs are locally ignored.
The validation sections below describe earlier work in the original demo.
# Kết quả kiểm thử VP v0.1

## PDF và CDC-VP component integration — 2026-10-09

Đối chiếu chương 3/4/5/13 và bảng 20-3 với model hiện tại; giữ nguyên register map,
reset defaults, start/IRQ và DMA completion contract. Các điểm cần IP/release thật
vẫn được liệt kê trong model_limitations.md, không được đánh dấu đã hoàn tất.

CDC-VP tham chiếu commit `528007d`; git status của repo này vẫn sạch sau kiểm thử.
Toolchain: Windows, MinGW GCC 12.1.0, CMake 3.24.2, SystemC 2.3.4, C++17 Release.

| Kiểm tra | Kết quả |
|---|---|
| Build standalone + external FX1 fixture, build/vp | Build OK; **34/34 PASS**, 1.71 s |
| Foreign parent có SystemC target, build/embedded | Build OK; **33/33 PASS**, 1.59 s |
| Install vào build/install và find_package consumer | Configure/build OK; **1/1 PASS** |
| Component-only với CDC_BUILD_TESTS=OFF | Configure OK; H264 integration_tests/example/stub đều OFF |

FX1 fixture dùng source bus, SparseRam, PLIC và ExclusiveMonitor thật từ repo ngoài.
Kiểm tra MMIO có base, byte enables, DDR checksum/reference, IRQ claim/ack/complete,
DMA làm mất LR reservation và hai activation. Reset khi atomic bracket giữ NAL write:
drain timeout không cấp lại buffer; sau release bracket drain thành công, không có IRQ
hay register completion cũ, activation tiếp theo chạy bình thường.

Đây không phải test CPU firmware hoặc full platform fx1_soc. Installed smoke chỉ
kiểm tra public headers/link/constructor, không thay thế functional regression.
Không có IP codec thật; checksum/EOS fixture không phải bitstream H264.
Hướng dẫn tái lập và nối platform: cdc_vp_integration.md.

## Error recovery và syntax preflight — 2026-10-09

Build Release thành công, **33/33 CTest PASS**, 1.61 s.
Thêm drain, drain_reset, drain_failed, drain_throw, width_exact, width_fn_short,
width_poc_short. Recovery fixture có DMA worker độc lập ghi 200 ns và lỗi peer sau 10 ns.
Xác nhận BUSY giữ đến drain, cấm đổi base, không IRQ sớm, false/exception giữ BUSY đến reset,
reset giữa drain không có stale write/completion. FN/POC thiếu một bit bị chặn trước begin/DMA.
Width test dùng policy 4/5 literal của fixture; chưa chứng nhận encoding/công thức của release.
Hợp đồng mới là pure virtual để adapter thật bắt buộc cung cấp drain và requirements.

## Cập nhật chức năng theo phụ lục — 2026-10-09

Build Release/SystemC 2.3.4 thành công. **26/26 CTest PASS**, tổng 1.07 s.
Demo QCIF vẫn PASS: hai activation 100 word.

Test bổ sung so với 15-test baseline:

| Test | Điều kiện được kiểm chứng |
|---|---|
| register_fields | Reset 0x12c; decode QP/FMENC/GOP/working-set và signed DFCON/SPARA2 |
| working_set | 5 frame dùng một source slot qua 5 activation; vùng CMB không bị tính thành 5 frame |
| frame_address | Host-selected one-picture base, counter tiến và STM_LEN restart |
| nal_progress | Word accepted đếm khi response đang chờ; GIE mask giữ pending IRQ |
| host_timeout | Snapshot trước disable, recovery bounded, output được giữ sau quiescence |
| host_timeout_stalled | BUSY quá grace: không đọc/reuse live buffer, diagnostic yêu cầu reset |
| priority | Rank cấu hình chọn client đúng, không preempt active transaction |
| priority_fairness | Request priority thấp được phục vụ dù stream priority cao liên tục |
| schedule | I/P theo M; thiếu B schedule bị từ chối thay vì silently raster |
| release_schedule | Adapter fixture cung cấp 0,2,1,4,3 qua batch 2/2/1; không phải chứng nhận lịch SISLAB |
| duplicate_schedule | Bắt display index trùng giữa các activation |

Test fault cũng kiểm tra STM_LEN giữ accepted final word khi write response lỗi.
Hai test schedule ban đầu bắt stale IRQ level khi rearm ngay; host/test helper đã settle
delta-cycle trước khi chờ completion mới. Toàn suite đã chạy lại PASS sau sửa.

Nguồn register fields là PDF bảng 20-3 trang 53–55, không phải mapping tự đặt.
Các phần chưa có nguồn release đầy đủ được ghi riêng trong model_limitations.md.

## Tổ chức module — 2026-10-09

Đã gom source, interface, platform, stub, adapter, integration_tests/vector và demo vào
`encoder_integration/`. Tách CMake theo module; public include/target giữ nguyên.
Build lại thành công trên SystemC 2.3.4; **15/15 CTest PASS**, tổng 1.54 s.
Thay đổi chỉ tổ chức source/build và tài liệu, không thay thuật toán/mô hình VP.

## Cập nhật v0.2 — pipeline factory và full-pipeline harness

Build SystemC 2.3.4/C++17 Release thành công; **15/15 CTest PASS** (0.76 s).
11 test cũ giữ kết quả PASS. Bốn test bổ sung:

| Test | Kết quả | Ý nghĩa |
|---|---|---|
| full_pipeline_stub | PASS | 2 frame/activation, 2 activation, exact output + reference golden, guard, IRQ |
| full_pipeline_bad_golden | PASS (expected failure) | Harness phải trả nonzero khi golden sai |
| full_pipeline_real_unavailable | PASS (expected failure) | Không có release adapter thì real bị từ chối |
| pipeline_replacement_contract | PASS | Adapter khác buffer đến finalize, trả 5 word, không theo checksum/EOS của stub |

Chạy riêng full_pipeline_tb stub tạo output ở build/full_pipeline_stub:
activation 0 = 3 word, completed 6842 ns; activation 1 = 3 word, completed 13902 ns.
Pipeline contract fixture không phải module thật của nhóm. Chưa chạy real H.264 qualification
do chưa có release/golden. Phần dưới lưu kết quả baseline v0.1 trước refactor.

Ngày: 2026-10-08 (Asia/Saigon).

## Môi trường đã chạy

- Windows, MinGW-w64 GCC 12.1.0, CMake 3.24.2, Ninja.
- Accellera SystemC 2.3.4 / TLM 2.0.6, static library, C++17, Release.
- Source dependency trên máy kiểm thử:
  `C:/Users/NguyenHuy/Downloads/SystemC-I2C/third_party/systemc-2.3.4`.
  Đây là đường dẫn môi trường kiểm thử, không hardcode vào source hoặc CMake project.
- Build directory: `build/vp`.

## Kết quả thực tế

Build thành công. CTest: **11/11 PASS**, tổng thời gian báo bởi CTest 0.39 s.

| Test | Kết quả | Nội dung chính |
|---|---|---|
| integration_32 | PASS | 3 activation; latency 0/20/50 ns; golden checksum; reference planar |
| integration_64 | PASS | Cùng golden với bus 64-bit |
| integration_128 | PASS | Cùng golden với bus 128-bit |
| dma_32 | PASS | Lane, byte/halfword/word, 4 KiB, enables tuần hoàn, lỗi địa chỉ, tranh chấp |
| dma_64 | PASS | Cùng nhóm test với bus 64-bit |
| dma_128 | PASS | Cùng nhóm test với bus 128-bit |
| registers | PASS | Byte enable, RO/undefined/alignment, read-clear, rearm và sticky completion |
| reset | PASS | Reset lúc final write đang chờ; không stale commit/IRQ; restart thành công |
| fault | PASS | Lỗi final write không thành normal; reactivation sau lỗi |
| completion | PASS | Busy giữ, IRQ chưa phát trong final-response latency |
| invalid_params | PASS | QP=52 của fixture bị từ chối |

Demo `encoder_demo`: PASS, QCIF 176x144, hai activation tái sử dụng NAL buffer.

```text
Activation 0: 100 words, completed at 265472 ns
Activation 1: 100 words, completed at 530872 ns
PASS: control/DMA integration demo (stub output, NOT H.264 video)
```

Thời gian trên là thời gian mô phỏng theo latency giả định, **không phải benchmark
encoder RTL hoặc tốc độ mã hóa H.264**. Build dependency có warning trong source
SystemC/MinGW; không chỉnh sửa source dependency để che warning.

Các giới hạn không được kiểm chứng (FIFO, AXI từng kênh, thuật toán video, CDC/timing)
được liệt kê trong `model_limitations.md`. Kết quả này không phải qualification đầy đủ
của encoder trong PDF.
