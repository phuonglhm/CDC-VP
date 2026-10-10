# H.264 intra prediction VP / TLM

Module production cho chương 7 của [HAS v2.1.11](../SISLAB_H264_AVC_HAS_Spec_Detailed_v2.1.11.md), dùng hợp đồng CMB/local RAM của [DMA spec](../dma_subsystems/DMA_SUBSYSTEM_SPEC.md). C++17, SystemC 2.3.4, TLM-2.0. Public namespace `h264::intra`.

`h264_intra_core` chứa thuật toán và reconstructed context; `h264_intra_tlm` chứa timing, frame/reset và socket. Target alias: `cdc::components::h264_intra_core`, `cdc::components::h264_intra_tlm`. Parent H264 phải đăng ký module và export targets trong CMake để build/install tích hợp; các thay đổi parent nằm ngoài folder này.

## Khả năng

- Luma 4×4: đủ mode H.264 0..8; luma 16×16: vertical/horizontal/DC/plane; chroma U/V 8×8: DC/horizontal/vertical/plane.
- Resolve top/left/upper-left/upper-right từ pixel **đã reconstruction** và bitmap valid riêng từng plane. Thiếu upper-right thì E..H lặp D. Block scan index 3/7/11/13/15 không lấy upper-right chưa hợp lệ; biên phải dùng coded width đầy đủ.
- SAD hoặc SATD Hadamard 4×4, cộng optional `mode_penalty[mode]` trên accumulator 64 bit. SATD block lớn là tổng các tile 4×4. Tất cả legal candidate được xét theo mode số tăng dần; chỉ thay winner khi cost nhỏ hơn.
- Predictor và residual signed 16 bit đi cùng block/plane, token, mode/cost và reference snapshot. Replay lấy nguyên snapshot đã chọn.
- Một block outstanding. Giữ winner cho tới khi downstream đọc Replay và cung cấp Reconstruct. Commit mode và neighbor pixels cùng lúc sau feedback latency.
- Mỗi cạnh lên `frame_enable` invalidate context, mô hình hóa reset `ipc_rst_n` của §7.4. Giữ enable cao không tạo thêm frame. `reset_n` thấp hủy frame và công việc cũ.

Quy tắc prediction đối chiếu H.264 §8.3 và [implementation reference của FFmpeg](https://github.com/FFmpeg/FFmpeg/blob/master/libavcodec/h264pred_template.c). Không đưa source bên thứ ba vào module.

## Wiring

```text
CMB DMA/local RAM adapter ── Evaluate(source) ──> IntraTlm.target_socket
TQ adapter               <── Replay(predictor + signed residual)
ITQ/reconstruction       ── Reconstruct(decoded pixels) ──> neighbor/mode RAM
```

Adapter CMB đọc DDR qua arbiter/bridge hoặc lấy `MacroblockPixels` từ DMA subsystem, rồi đóng gói từng block row-major. Source không tự động trở thành reference. Adapter TQ nhận predictor/residual, chạy FTQ/ITQ, clip reconstruction về 8 bit rồi gửi feedback.

Tạo module/socket trước `sc_start`, gọi `b_transport` từ `SC_THREAD`, giữ payload/data/extension sống tới response. Ví dụ constructor:

```cpp
h264::intra::Options opts;
opts.coded_width = 176;
opts.coded_height = 144;
h264::intra::IntraTlm intra("intra", opts);
initiator.bind(intra.target_socket);
intra.reset_n(reset_n);
intra.frame_enable(frame_enable);
```

Kích frame bằng enable thấp rồi cao sau khi release reset. Options cố định coded dimensions cho instance; giới hạn VP là 16..4096, chia hết cho 16. Tọa độ `Block::x/y` theo sample của plane; U/V dùng width/height bằng nửa Y. Luma4 phải align4, luma16 align16, chroma8 align8. Caller chạy MB raster và luma4 theo H.264 scan:

```text
 0  1  4  5
 2  3  6  7
 8  9 12 13
10 11 14 15
```

Khi ghép với DMA subsystem, coded dimensions phải nằm trong giới hạn chung
của hai module: DMA spec hiện giới hạn tối đa 1920×1088. Giới hạn 4096 của
core Intra là khả năng VP riêng, không mở rộng contract DMA.

## Socket protocol

Các address dưới đây là **simulation operation**, không bổ sung encoder MMIO register. Mọi transaction cần `h264::intra::Extension`, buffer đúng N×N byte, streaming width >= buffer length, không byte-enable. DMI không được cấp.

| Address / Operation | Command | Buffer và extension | Điều kiện response OK |
|---|---|---|---|
| 0 / Evaluate | WRITE | Source pixels; input block/metric/penalty; output token/decision | Winner ready; block chưa done |
| 1 / Replay | READ | Predictor pixels; token của Evaluate; output decision chứa residual | Downstream đã chấp nhận predictor |
| 2 / Reconstruct | WRITE | Clipped decoded pixels; cùng token | Neighbor/mode RAM cập nhật; block_done=true |
| 3 / ImportReconstructed | WRITE | Decoded pixels; input block | Import context từ adapter; chỉ khi không có block pending |

Replay được đọc nhiều lần với cùng token trong khi chờ feedback, dữ liệu giữ nguyên. Reconstruct trước Replay bị từ chối. Evaluate/Import khi đang pending bị từ chối và giữ nguyên block cũ. Token gồm generation + sequence; token cũ không dùng được sau frame/reset. Với Replay/Reconstruct, block lấy từ token đã lưu thay vì metadata caller có thể thay đổi.

`ready()` và `block_done()` là trạng thái authoritative; `predictor_ready`/`block_completed` chỉ là wakeup event. Done sticky tới Evaluate tiếp theo thành công hoặc frame/reset. Không lấy Evaluate response làm completion của MB. Caller tổng hợp block completion thành MB completion.

Timing: incoming delay được consume; reference latency một lần, candidate + compare latency cho từng legal mode; replay và feedback latency riêng. Default lần lượt 10/4/1/4/10 ns; đây là tham số VP, **không chứng nhận cycle RTL**. Frame/reset đánh thức và hủy wait cũ. Socket serialize transaction; request đồng thời trả generic error để caller retry sau response của owner.

Có thể gắn `h264::EpochExtension` của reset domain hiện có; giữ domain sống đến response. Epoch đã stale tại entry bị từ chối và giữ nguyên winner hợp lệ. Nếu epoch của một request hợp lệ bị hủy trong incoming/reference/candidate/compare/replay/feedback/import wait, module đánh thức wait và invalidate toàn bộ context frame obsolete, gồm winner/token và reconstructed context cũ. Notification của domain khi epoch vẫn hợp lệ giữ đúng phần latency còn lại. Shared epoch được kiểm tra trong transaction; caller phải phối hợp `reset_n`/frame_enable để invalidate context khi shared reset xảy ra lúc socket idle. Sau khi hủy frame, pulse frame_enable để bắt đầu lại. Trước tái sử dụng buffer, chờ response cũ và kiểm tra `transport_active()==false`.

## Build và test

Từ `CDC-VP`, dùng SystemC 2.3.4 cùng compiler và C++17 ABI:

```sh
cmake -S components/h264 -B components/h264/build/intra -DCMAKE_PREFIX_PATH=<SystemC-prefix>
cmake --build components/h264/build/intra -j 8
ctest --test-dir components/h264/build/intra -R 'intra|prediction' --output-on-failure
ctest --test-dir components/h264/build/intra --output-on-failure
```

Có thể thay `CMAKE_PREFIX_PATH` bằng `SYSTEMC_SOURCE_DIR`, hoặc cặp `SYSTEMC_INCLUDE_DIR` / `SYSTEMC_LIBRARY`. Test prediction gồm real intra + DmaArbiter/DmaBridge/DdrMemory và bench CMB subsystem Y/U/V full-picture với width32/64/128. Bench CMB kiểm tra luma4/luma16/chroma8, MB completion và DMA faults; decoded feedback có fixture rõ ràng. TQ wiring bench dùng actual TqTop cho Y4 nhưng TQ hiện có placeholder quantization/DC, nên kết quả chưa chứng nhận H.264 transform/bitstream.

Build riêng module dùng cùng tham số dependency:

```sh
cmake -S components/h264/Intra-Prediction -B components/h264/Intra-Prediction/build-tlm -DCMAKE_PREFIX_PATH=<SystemC-prefix>
cmake --build components/h264/Intra-Prediction/build-tlm -j 8
ctest --test-dir components/h264/Intra-Prediction/build-tlm --output-on-failure
```

Core-only không cần SystemC: thêm `-DH264_INTRA_BUILD_TLM=OFF` khi configure module riêng. Parent có thể `add_subdirectory(Intra-Prediction)` sau khi tạo `SystemC::systemc`. Standalone mặc định đăng ký10 tests (core-only2): algorithms, independent predictor/cost regression, TLM contract, hai shared-reset regressions, reset matrix, custom timing/zero latency và reset boundary/custom-zero latency. Các bài mới luôn chạy khi `H264_BUILD_TESTS` bật. Filter `intra|prediction` ở parent chọn22 tests, gồm mixed luma4/luma16 CMB integration32/64/128.

Ví dụ khi SystemC được cài tại `/opt/systemc-2.3.4`, chạy từ folder module
trong WSL/Linux:

```sh
cmake -S . -B build-tlm-linux -DCMAKE_BUILD_TYPE=Release \
  -DSYSTEMC_INCLUDE_DIR=/opt/systemc-2.3.4/include \
  -DSYSTEMC_LIBRARY=/opt/systemc-2.3.4/lib/libsystemc.so
cmake --build build-tlm-linux -j 8
ctest --test-dir build-tlm-linux --output-on-failure
```

Nếu chỉ commit folder Intra, suite standalone 10 tests được đăng ký tại đây.
Standalone TLM còn dùng `../interfaces` và `../synchronization` của repo.
Các test CMB/TQ và kết quả H264 55/55 cần CMake/test sources ở parent và
`../dma_subsystems`; chúng không tự đi theo commit folder Intra. Chi tiết
dependency và phạm vi push được ghi trong [VALIDATION.md](VALIDATION.md#phạm-vi-khi-push-riêng-folder).

## Giới hạn và quyết định VP

HAS chương 7 không khóa chính xác SAD/SATD, mode-rate cost hoặc thứ tự candidate; các lựa chọn trên là contract VP công khai. Chưa có RTL golden để khẳng định mode/cost/bitstream byte-identical với SISLAB.

Module thực hiện block prediction và context lifecycle. Caller chọn partition luma4/luma16, tổ chức MB, slice/constrained-intra availability, và ghép TQ/ITQ/entropy/DF; không có automatic partition RDO hoặc full encoder adapter. Không hỗ trợ luma8 High Profile, bit-depth >8, MBAFF hay chroma khác 4:2:0. `ImportReconstructed` là API bàn giao decoded context, caller chịu trách nhiệm đúng picture/slice và ordering. Mode RAM lưu winning mode; chưa xuất syntax MPM/prev_intra4x4_pred_mode_flag.

Kết quả regression, mapping yêu cầu, reset fixes, coverage và giới hạn qualification:
[VALIDATION.md](VALIDATION.md). Evidence có source/test hashes được lưu trong
`tests/evidence`. Kết quả ngày 2026-10-10: standalone10/10, H26455/55 và
UBSan10/10 PASS.
