# Test DMA clients, intra-prediction và inter-prediction (chương 6–8)

Owner: **Vinh**. Thư mục này dành cho test module của Vinh ghép với hạ tầng Huy, trước full pipeline.
Hiện chỉ có README và cấu hình CMake, **chưa có test implementation/IP thật và không tạo PASS giả**.

## Bố trí file

~~~text
prediction/
  CMakeLists.txt
  README.md
  integration_tb.cpp       # người phụ trách bổ sung, chứa sc_main và bench
  fixtures/                # tùy chọn: input, golden, các producer/sink giả
~~~

Module production để ở thư mục kỹ thuật riêng trong components/h264; không đặt module thật trong thư mục test này. Nếu cần thêm .cpp hỗ trợ bench, khai báo target_sources trong CMakeLists.txt tại đây.

## Ghép từng phần với DMA của Huy

Test ban đầu không bắt buộc dùng EncoderVp hoặc FrameExecutorIf. Có thể tạo trực tiếp module thật + DmaArbiter + DmaBridge + DdrMemory:

~~~text
Test stimulus → Module/client thật → arbiter.clients
                                      ↓
                                arbiter.memory → bridge.input
                                                   ↓
                                              bridge.memory → ddr.socket
Test memory initiator (nạp/đọc golden) ─────────────────────→ ddr.socket
~~~

Các class có sẵn: h264::DmaArbiter, h264::DmaBridge, h264::DdrMemory, h264::ResetDomain.
Include tương ứng: <h264/dma/dma_arbiter.h>, <h264/dma/dma_bridge.h>, <h264/platform/ddr_memory.h>, <h264/sync/reset_domain.h>.

Ví dụ binding, với client.dma là tên minh họa — thay bằng port thực tế của module:

~~~cpp
client.dma.bind(arbiter.clients);
arbiter.memory.bind(bridge.input);
bridge.memory.bind(ddr.socket);
memory_driver.socket.bind(ddr.socket);
~~~

Tạo mọi sc_module/socket trước sc_start. Stimulus chạy SC_THREAD vì b_transport có thể wait.
Đặt watchdog simulation và assertion; sc_main trả 0 chỉ khi mọi kiểm tra thành công, lỗi trả khác 0.
Giữ payload/data sống tới response; test nhiều latency và error, không chỉ đường thành công.
Top ghép thủ công phải tự nối/reset client và ResetDomain; bridge/arbiter không có reset signal riêng. Không thay completion bằng delay cố định.

## Kịch bản của Vinh

1. **DMA client trước**: tạo YUV/reference đã biết trong DDR; chạy CMB/SW rồi so từng byte, stride, biên hàng và source slot. Cho NAL/DF client ghi mẫu do test driver cung cấp, kiểm tra địa chỉ, byte enable và dữ liệu. Chưa cần entropy/DF thật để kiểm tra giao dịch.
2. **Intra**: cấp macroblock và reconstructed-neighbor mẫu, so predictor/mode với golden của module; kiểm tra frame-start reset và block boundary.
3. **Inter**: cấp reference và search-window qua SW DMA, so predictor/motion data với golden; thử list/reference theo chế độ module thực sự hỗ trợ.
4. **Tranh chấp DMA**: ít nhất hai client đồng thời; tăng DDR latency, kiểm tra không nhầm response giữa client, không mất request. Chọn priority được nhóm thống nhất.
5. **Lỗi/reset**: đặt error_begin/error_end của DdrMemory cho vùng lỗi; reset khi client đang chờ. Phải báo lỗi/dừng đúng hợp đồng, không trả dữ liệu cũ như kết quả hợp lệ.

Giả lập phần downstream của Nguyên bằng sink có khả năng trì hoãn tiêu thụ. Sink phải kiểm tra đủ dữ liệu và thứ tự, không chỉ trả done ngay. Ghi rõ phần nào dùng fixture và phần nào là module thật.

## Khi cần kiểm tra thêm control/top

Dùng adapter **chỉ dành cho test của phần mình** implement FrameExecutorIf để nối module thật vào EncoderVp. Những phần chưa có được thay bằng fixture có output/điều kiện hoàn tất rõ ràng.
Truyền factory của adapter vào tham số cuối EncoderVp; không dùng default factory vì test này không tự link stub.
HostDriver cấu hình register; bind host.registers vào encoder.registers.socket, host.memory và encoder.bridge.memory vào DDR; nối rstn/irq.
Tuân thủ accepted-word, final output/reference response, syntax preflight và reset generation. Xem [hợp đồng bàn giao](../../integration_docs/team_handoff.md).
Test adapter không phải release adapter: không đặt target h264_released_pipeline cho fixture này. Chỉ ghép full pipeline khi các module thực sự có và golden full encoder đã sẵn sàng.

## Đăng ký và chạy

1. Bổ sung integration_tb.cpp tại đây.
2. Module thật phải export CMake target, không chứa sc_main. Không định nghĩa lại h264_top/h264_dma/SystemC::systemc.
3. Nếu target chưa có, đặt H264_PREDICTION_MODULE_DIR tới thư mục CMake của module (chỉ thêm một lần). Nếu repo cha đã tạo target, bỏ biến MODULE_DIR.
4. Đặt H264_PREDICTION_TEST_LIBRARIES bằng danh sách target thật, ngăn cách bằng dấu chấm phẩy.

Chạy từ **components/h264**, sau khi đã configure SystemC theo hướng dẫn chung:

~~~text
cmake -S . -B build/vp -DH264_BUILD_TESTS=ON -DH264_PREDICTION_MODULE_DIR="<absolute-module-directory>" -DH264_PREDICTION_TEST_LIBRARIES="<target_a>;<target_b>"
cmake --build build/vp -j 8
ctest --test-dir build/vp -N -R "^h264_prediction_integration_tb$"
ctest --test-dir build/vp -R "^h264_prediction_integration_tb$" --output-on-failure
~~~

Thay placeholder bằng target/path thật, xóa target_b nếu không dùng. Lần configure đầu cần thêm SYSTEMC_SOURCE_DIR hoặc CMAKE_PREFIX_PATH như hướng dẫn chung.
**Kiểm tra lệnh -N liệt kê đúng một test**: khi chưa có integration_tb.cpp, CMake báo not registered; CTest có thể trả thành công dù không có test nào, không coi đó là PASS.
Target thư viện thiếu sẽ làm configure lỗi thay vì âm thầm dùng stub. Khi bỏ MODULE_DIR đã cache, cấu hình lại với giá trị rỗng.

## Bàn giao kết quả

Ghi cấu hình, revision module, input/golden, danh sách phần thật/phần fixture và lỗi còn mở.
Giữ regression gốc chạy bằng ctest --test-dir build/vp --output-on-failure.
Vector .hex ở thư mục mới có thể bị .gitignore root bỏ qua: thêm ngoại lệ giới hạn đúng folder và kiểm tra git check-ignore trước khi bàn giao. Không commit build output.
