# Hướng dẫn ghép module và chạy test cho Vinh, Nguyên

Component: CDC-VP/components/h264. SystemC **2.3.4**, TLM-2.0, C++17; không phải RTL.
**Các lệnh bên dưới chạy từ components/h264**, không phải root CDC-VP.

## 1. Phân công và vai trò các khối

| Phần | Người phụ trách | Vai trò |
|---|---|---|
| control/ | Huy — chương 4 | Thanh ghi, cấu hình, start, BUSY/NORMAL/ERROR, IRQ và điều phối activation |
| axi_dma/ | Huy — chương 5 | Arbiter giữ owner, bridge chia burst/4 KiB, xử lý byte lane và response |
| synchronization/ | Huy — chương 13 | Reset generation, vô hiệu hóa công việc cũ, lưu completion |
| top/ | Huy — chương 3 | EncoderVp ghép control/DMA và pipeline factory |
| interfaces/ | Huy và nhóm thống nhất | FrameConfig, PictureTask, FrameExecutorIf và dữ liệu dùng chung |
| DMA clients, intra, inter | Vinh — chương 6–8 | Di chuyển dữ liệu theo layout/stride, thực hiện prediction |
| TQ, entropy/NAL, reconstruction/DF, RAM nội bộ | Nguyên — chương 9–12 | Thuật toán coding, tái tạo/reference và bộ nhớ nội bộ |

DMA của Vinh quyết định dữ liệu nào cần đọc/ghi, địa chỉ và thời điểm yêu cầu; axi_dma của Huy phân xử và chuyển request ra bộ nhớ. NAL formatter tạo dữ liệu, NAL DMA ghi dữ liệu. Hai bên thống nhất điểm accepted-word và final-response.

## 2. Code chính và hạ tầng kiểm thử

| Thư mục | Tác dụng |
|---|---|
| integration_tests/stubs/ | Checksum/copy pipeline; không tạo video H264 |
| integration_tests/platform/ | HostDriver giả lập phần mềm CPU, DdrMemory giả lập DDR |
| integration_tests/tests/ | Regression và full-pipeline harness |
| integration_tests/vectors/ | Input, golden và case .cfg |
| integration_tests/cdc_fx1/ | Test với bus/RAM/PLIC/exclusive monitor thật của CDC-VP |
| integration_tests/embedded/ | Kiểm tra nhúng CMake và consumer sau install |
| integration_tests/examples/ | Demo end-to-end dùng stub |
| integration_scripts/ | Script build/test |
| integration_docs/ | Traceability, giới hạn và kết quả kiểm thử |
| cmake/ | Install/export thư viện và public header |

Stub và host/memory là fixture. Testbench vẫn dùng tiếp khi có IP thật; thay backend bằng adapter thật và golden độc lập. Không dùng golden checksum để chứng nhận codec.

## 3. Build và chạy ngay

Cần CMake >= 3.21, compiler C++17, Ninja (với lệnh dưới) và SystemC 2.3.4. SystemC phải cùng compiler/ABI với ứng dụng; không dùng thư viện MinGW cho MSVC. Không copy build cache từ máy khác.

### Dùng source SystemC có sẵn

Thay đường dẫn ví dụ bằng đường dẫn máy mình. Trên Linux dùng đường dẫn Linux.

~~~text
cmake -S . -B build/vp -G Ninja -DCMAKE_BUILD_TYPE=Release -DSYSTEMC_SOURCE_DIR="C:/deps/systemc-2.3.4" -DH264_BUILD_TESTS=ON -DH264_BUILD_EXAMPLES=ON
cmake --build build/vp -j 8
ctest --test-dir build/vp --output-on-failure
~~~

Nếu compiler chưa trong PATH, thêm -DCMAKE_CXX_COMPILER=<đường-dẫn-g++> khi configure. SystemC được build dưới build/vp.

### Dùng SystemC đã cài

~~~text
cmake -S . -B build/vp -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="<SystemC-install-prefix>" -DH264_BUILD_TESTS=ON
cmake --build build/vp -j 8
ctest --test-dir build/vp --output-on-failure
~~~

Cũng có thể cung cấp cả SYSTEMC_INCLUDE_DIR và SYSTEMC_LIBRARY. Chọn một cách cung cấp SystemC; khi đổi compiler/dependency, dùng build directory mới để tránh cache cũ.

### Script Windows

~~~powershell
./integration_scripts/build.ps1 -SystemCSourceDir "C:/deps/systemc-2.3.4"
~~~

Script chạy configure/build/CTest; hỗ trợ -CMake, -Compiler, -Jobs. Ninja phải có trong PATH. Linux dùng CMake trực tiếp.

### Test theo nhóm

~~~text
ctest --test-dir build/vp -R "^h264_dma_" --output-on-failure
ctest --test-dir build/vp -R "^h264_register" --output-on-failure
ctest --test-dir build/vp -R "^h264_(drain|width_)" --output-on-failure
ctest --test-dir build/vp -R "^h264_full_pipeline_stub$" --output-on-failure
ctest --test-dir build/vp -R "^h264_pipeline_replacement_contract$" --output-on-failure
~~~

| File trong integration_tests/tests | Kiểm tra |
|---|---|
| vp_tests.cpp | Register, DMA, reset, completion, timeout, priority, schedule và working set |
| recovery_syntax_tb.cpp | Drain khi lỗi/reset, BUSY khi recovery thất bại, preflight FN/POC |
| full_pipeline_tb.cpp | Đọc case, nạp source, chạy activation và so output với golden |
| contract_pipeline.cpp | Pipeline thay thế thử nghiệm có output length thay đổi; link cùng full_pipeline_tb.cpp, không chạy riêng |

Test bad_golden và real_unavailable có WILL_FAIL: chương trình phải trả lỗi thì CTest mới PASS. Chưa có adapter thật và tắt FX1: 33 test; bật FX1: 34 test. Đây là test integration, chưa chứng nhận full codec.

## 4. Test với component FX1 thật

Sau khi configure SystemC, dùng đường dẫn tuyệt đối tới root CDC-VP:

~~~text
cmake -S . -B build/vp -DH264_BUILD_CDC_TESTS=ON -DH264_CDC_VP_ROOT="<absolute-CDC-VP-root>"
cmake --build build/vp -j 8
ctest --test-dir build/vp -R "^h264_cdc_fx1$" --output-on-failure
~~~

Test biên dịch source FX1 từ repo vào build của H264, kiểm tra CSR, DMA, DDR, PLIC, LR reservation và reset/drain. Không chạy CPU firmware hoặc toàn platform fx1_soc.

CMake cấp repo hiện chưa tự thêm H264. Khi được phép sửa repo cha, thêm add_subdirectory(h264) tại components sau khi SystemC::systemc tồn tại; platform link cdc::components::h264_tlm. Không tạo SystemC runtime thứ hai. H264_BUILD_TESTS theo CDC_BUILD_TESTS khi nhúng.

## 5. Điểm ghép IP thật

~~~text
Host/CPU → ControlRegs → EncoderController
                              ↓ FrameExecutorIf
                        Pipeline adapter
                              ↓
                Prediction / TQ / entropy / DF / RAM
                              ↓ DMA clients
                       DmaArbiter → DmaBridge → DDR
~~~

Đọc [FrameExecutorIf](../interfaces/include/h264/frame_executor_if.h), [types.h](../interfaces/include/h264/types.h) và [pipeline_factory.h](../top/include/h264/top/pipeline_factory.h) trước khi nối.

Từng module của Vinh/Nguyên không cần kế thừa FrameExecutorIf. Một adapter sở hữu/nối module và thực hiện interface này. Không sửa controller để phụ thuộc class nội bộ của từng module.

| Hàm/hook | Hợp đồng |
|---|---|
| syntax_requirements(config) | Preflight không wait/không phát DMA; trả độ rộng FN/POC cần và độ rộng đã giải mã từ register |
| begin_activation(config, generation) | Chuẩn bị cấu hình và trạng thái activation |
| picture_task(config, index) | Coding/display order và source slot; override khi cần lịch B-picture |
| execute_picture / execute | Xử lý picture; trả số word output mới đã committed. execute vẫn là pure virtual cần implement |
| nal_words_accepted(generation, count) | Gọi đúng một lần cho mỗi nhóm NAL word được chấp nhận, trước khi chờ response DDR |
| end_activation | Drain formatter cùng output/reference DMA; trả tổng word activation, gồm padding/EOS nếu release có |
| abort_and_drain | Dừng producer và chờ tất cả request đã gửi; true chỉ khi đã quiescent |
| reset | Không blocking; dừng/invalidate producer và trạng thái cũ |

Không báo NORMAL chỉ vì word cuối vào FIFO; phải chờ output/reference response. Drain thất bại trả false/exception, controller giữ BUSY đến reset. Không thay completion thật bằng delay cố định.

Default picture_task chỉ hỗ trợ N=1. Lịch B-picture, FN/POC và priority release phải lấy từ thiết kế của nhóm; không dùng zero requirements của checksum fixture cho codec thật.

## 6. DMA, reset và tên public

DMA initiator bind vào arbiter.clients tại elaboration; không bypass bridge ghi DDR riêng. Payload/data/extension phải sống tới khi b_transport trả về. Client đặt command/address/data_length/streaming_width và xử lý response error.

Source Huy nằm trong axi_dma nhưng public include vẫn h264/dma/... và target vẫn h264_dma. Vinh dùng target riêng, ví dụ h264_dma_clients; không định nghĩa lại h264_dma. Header control/axi_dma đặt phẳng được CMake ánh xạ sang public include trong build; không đưa header sinh ra lên Git.

Reset phải theo ResetDomain generation; completion cũ không được cập nhật trạng thái mới. Không giải phóng payload còn live. RAM ngoài có thể không hiểu EpochExtension: platform giữ reset, dừng producer và chờ encoder.arbiter.wait_idle(timeout) trước khi reuse buffer. Timeout không hủy transaction. Xem [FX1/reset](cdc_vp_integration.md).

## 7. Đưa adapter thật vào harness

Adapter mẫu đã bị xóa. Nhóm tạo adapter thật ở vị trí thống nhất, với CMakeLists.txt xuất target:

~~~cmake
add_library(h264_released_pipeline released_pipeline.cpp)
target_link_libraries(h264_released_pipeline PUBLIC h264_top)
# Link PRIVATE các target thật của Vinh/Nguyên sau khi chúng đã được khai báo.
~~~

Thêm thư mục module bằng add_subdirectory đúng một lần, hoặc dùng target có sẵn từ repo cha. Không thêm lại H264/SystemC trong adapter.

Adapter định nghĩa đúng namespace/signature:

~~~cpp
namespace h264 {
std::unique_ptr<FrameExecutorIf> make_released_pipeline(
    sc_core::sc_module_name name, ResetDomain& reset, DmaArbiter& arbiter);
}
~~~

Factory tạo adapter/module tại elaboration, bind DMA client rồi trả unique_ptr. Không tạo sc_module khi simulation đang chạy. Ứng dụng tự ghép truyền factory vào tham số cuối EncoderVp; harness chọn factory khi chạy backend real.

~~~text
cmake -S . -B build/vp -DH264_BUILD_TESTS=ON -DH264_RELEASED_PIPELINE_DIR="<absolute-adapter-directory>" -DH264_FULL_PIPELINE_CASE="<absolute-case.cfg>"
cmake --build build/vp -j 8
ctest --test-dir build/vp -R "^h264_full_pipeline_real$" --output-on-failure
~~~

Chỉ chạy khi adapter và case đã tồn tại. Real không fallback sang stub. Các tùy chọn này ghép adapter vào harness; platform production vẫn phải tự link adapter. Nếu dùng constructor EncoderVp mặc định thì phải link h264_pipeline_stub; với IP thật truyền factory rõ ràng.

## 8. Input và golden thật

Tham khảo [stub.cfg](../integration_tests/vectors/full_pipeline/stub.cfg) về cấu trúc, không dùng output của nó làm golden codec.

- source: planar YUV420 đúng kích thước, working set và thứ tự frame.
- expected: output golden độc lập đúng cấu hình/revision, so byte chính xác kể cả padding.
- expected_ref/reference_offset: tùy chọn kiểm tra reference cụ thể.
- spara0/1/2, dfcon: raw register để kiểm tra GOP/crop/filter/syntax.
- timeout_ns/nal_capacity: đủ cho output thật, không giả định checksum.
- source.0/source.1, expected.0/expected.1: thay dữ liệu theo activation khi cần.

Path tính từ folder chứa case.cfg. File .hex là byte hex cách nhau bằng whitespace, không phải word 32-bit; đuôi khác đọc binary. Root repo ignore *.hex; ngoại lệ hiện chỉ cho integration_tests/vectors/full_pipeline/*.hex. Nếu thêm folder vectors mới, thêm ngoại lệ và kiểm tra Git nhận đủ file.

## 9. Checklist bàn giao

- CMake target riêng, public header, test module; không đặt sc_main trong library.
- Ghi rõ format pixel/coefficient, signedness, block/frame order, byte order, địa chỉ/stride.
- Thống nhất ready/done, cuối picture/activation, ownership buffer và reset pending request.
- Chỉ định nơi gọi nal_words_accepted, tránh formatter và DMA đếm hai lần.
- Cung cấp cấu hình hỗ trợ, GOP schedule, reference slot lifetime và golden độc lập.
- Chạy test module, regression integration và real golden khi có adapter.
- Không đưa build cache, binary, dependency/toolchain vào Git; nhận đủ input/golden.

Tên folder khác nhau chưa đủ: target CMake và public header cũng phải thống nhất. 34 test fixture PASS không có nghĩa thuật toán H264 đã hoàn tất.
