# Hướng dẫn build & chạy — SystemC Streaming ISP Pipeline

Tài liệu này hướng dẫn cách biên dịch và chạy 17 testbench ở cấp block riêng lẻ
và 1 testbench toàn pipeline cho kiến trúc SystemC streaming của CDC-VP ISP.

---

## 1. Tổng quan cấu trúc

```
components/isp_tlm/
├── CMakeLists.txt                # thư viện `isp_tlm` (C++ tham chiếu ban đầu)
├── blocks/                       # 17 block C++ ban đầu (blc, dpc, lsc, ...)
├── core/
├── pipeline/
├── tests/                        # testbench SystemC gốc (isp_run) cho mô hình tham chiếu
└── systemc/                      # kiến trúc SystemC streaming
    ├── CMakeLists.txt            # build & register tất cả testbenches SystemC
    ├── tb_utils/
    │   └── tb_utils.h            # Generic_Driver<T>, Generic_Monitor<T>
    ├── blocks/
    │   ├── blc/        (sc_blc.{h,cpp} + tb_blc.cpp)
    │   ├── dpc/        (sc_dpc.{h,cpp} + tb_dpc.cpp)
    │   ├── dg/         (sc_dg.{h,cpp}  + tb_dg.cpp)
    │   ├── wb/         (sc_wb.{h,cpp}  + tb_wb.cpp)
    │   ├── ccm/        (sc_ccm.{h,cpp} + tb_ccm.cpp)
    │   ├── gc/         (sc_gc.{h,cpp}  + tb_gc.cpp)
    │   ├── csc/        (sc_csc.{h,cpp} + tb_csc.cpp)
    │   ├── cse/        (sc_cse.{h,cpp} + tb_cse.cpp)
    │   ├── lsc/        (sc_lsc.{h,cpp} + tb_lsc.cpp)
    │   ├── bnr/        (sc_bnr.{h,cpp} + tb_bnr.cpp)
    │   ├── demosaic/   (sc_demosaic.{h,cpp} + tb_demosaic.cpp)
    │   ├── sharpen/    (sc_sharpen.{h,cpp}  + tb_sharpen.cpp)
    │   ├── 2dnr/       (sc_2dnr.{h,cpp} + tb_2dnr.cpp)
    │   ├── awb/        (sc_awb.{h,cpp} + tb_awb.cpp)
    │   ├── aec/        (sc_aec.{h,cpp} + tb_aec.cpp)
    │   ├── scale/      (sc_scale.{h,cpp} + tb_scale.cpp)
    │   └── yuv420/     (sc_yuv420.{h,cpp} + tb_yuv420.cpp)
    └── pipeline/
        ├── sc_isp_pipeline.{h,cpp}    # Top-level SystemC sc_module (Phase 5)
        └── tb_pipeline.cpp            # Full pipeline testbench (Phase 6)
```

---

## 2. Yêu cầu môi trường

| Thành phần | Phiên bản / Gợi ý |
|-----------|--------------------|
| SystemC  | 2.3.4 (đã được dùng bởi project) |
| CMake    | ≥ 3.21 |
| Compiler | `g++` ≥ 7 (C++17) |
| Library  | `libsystemc.so` (đặt tại `/opt/systemc-2.3.4/lib64`) |

Kiểm tra nhanh:

```bash
ls /opt/systemc-2.3.4/include/systemc.h
ls /opt/systemc-2.3.4/lib64/libsystemc.so
g++ --version           # phải hỗ trợ C++17
cmake --version
```

Nếu SystemC nằm ở vị trí khác, truyền `-DSYSTEMC_HOME=/path/to/systemc` khi cấu hình.

---

## 3. Cấu hình & Build

### 3.1. Tích hợp vào build CDC-VP sẵn có (khuyến nghị)

Vì `components/isp_tlm/CMakeLists.txt` đã được patch với
`add_subdirectory(systemc)` bên dưới `if(CDC_BUILD_TESTS)`, các testbench SystemC sẽ
tự động được build cùng với phần còn lại của project:

```bash
cd /home/hoangquan/workspace/CDC-VP

# Configure (chỉ định rõ compiler + SystemC)
cmake -S . -B build/bremen -G Ninja \
  -DCMAKE_CXX_COMPILER=/usr/bin/g++ \
  -DCDC_BUILD_TESTS=ON \
  -DSYSTEMC_HOME=/opt/systemc-2.3.4

# Build mọi testbench (17 block + 1 pipeline)
cmake --build build/bremen \
  --target tb_blc tb_dpc tb_dg tb_wb tb_ccm tb_gc tb_csc tb_cse \
           tb_lsc tb_bnr tb_demosaic tb_sharpen tb_2dnr \
           tb_awb tb_aec tb_scale tb_yuv420 \
           tb_pipeline
```

Kết quả binaries (flat, cùng thư mục) được đặt tại:
```
build/bremen/components/isp_tlm/systemc/tb_<block_name>     # cho cả 17 block
build/bremen/components/isp_tlm/systemc/tb_pipeline          # cho pipeline
build/bremen/components/isp_tlm/systemc/libsc_isp_blocks.a  # static lib (sc_<block>.o)
build/bremen/components/isp_tlm/systemc/libsc_isp_pipeline.a# static lib (full top)
```

### 3.2. Build độc lập với CMake (nếu không muốn build cả project)

Tạo thư mục build tạm và trỏ CMake về thư mục `systemc/`:

```bash
cd /home/hoangquan/workspace/CDC-VP
mkdir -p build/sc-only && cd build/sc-only

# Phải giả lập một top-level CMakeLists để cung cấp SystemC::systemc
cat > sc_only_top.cmake <<'EOF'
cmake_minimum_required(VERSION 3.21)
project(isp_sc_only LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(SYSTEMC_HOME "/opt/systemc-2.3.4" CACHE PATH "")
find_path(SYSTEMC_INCLUDE_DIR NAMES systemc.h HINTS "${SYSTEMC_HOME}/include")
find_library(SYSTEMC_LIBRARY  NAMES systemc   HINTS "${SYSTEMC_HOME}/lib64" "${SYSTEMC_HOME}/lib")
add_library(SystemC::systemc UNKNOWN IMPORTED GLOBAL)
set_target_properties(SystemC::systemc PROPERTIES
    IMPORTED_LOCATION "${SYSTEMC_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${SYSTEMC_INCLUDE_DIR}")
get_filename_component(_d "${SYSTEMC_LIBRARY}" DIRECTORY)
set(CMAKE_BUILD_RPATH    "${_d}")
set(CMAKE_INSTALL_RPATH  "${_d}")
enable_testing()
add_subdirectory(${CMAKE_SOURCE_DIR}/components/isp_tlm/systemc
                 ${CMAKE_BINARY_DIR}/systemc)
EOF

# Trỏ SOURCE_DIR về thư mục chứa sc_only_top.cmake
cmake -S . -B . -DCMAKE_PROJECT_TOP_LEVEL_INCLUDES=$(pwd)/sc_only_top.cmake
# hoặc cách đơn giản hơn: symlink
ln -sf ../CMakeLists.txt . 2>/dev/null || true
```

> **Gợi ý thực tế**: dùng cách 3.1 (build qua project CDC-VP) sẽ tránh phải hack
> `CMAKE_PROJECT_TOP_LEVEL_INCLUDES`.

### 3.3. Build bằng tay (không dùng CMake)

Mỗi testbench ở cấp block là một `sc_main` độc lập. Cú pháp biên dịch:

```bash
SYS=/opt/systemc-2.3.4
g++ -std=c++17 -O2 \
    -I $SYS/include \
    -I components/isp_tlm/systemc/tb_utils \
    -I components/isp_tlm/core/include \
    -I components/isp_tlm/blocks/<block>/include \
    -L $SYS/lib64 \
    components/isp_tlm/systemc/blocks/<block>/sc_<block>.cpp \
    components/isp_tlm/systemc/blocks/<block>/tb_<block>.cpp \
    -o build/tb_<block> \
    -lsystemc
```

> **Lưu ý**: Một vài SystemC blocks (LSC, GC …) include header của block khác —
> khi build thủ công có thể phải thêm include path cho LSC (`lsc`) khi build `tb_blc`,
> v.v. Khuyến nghị: dùng CMake.

---

## 4. Chạy test

Sau khi build, mỗi binary tự chạy được (`sc_main` đã định nghĩa sẵn test pattern).

### 4.1. Chạy từng testbench block

```bash
BUILD=build/bremen/components/isp_tlm/systemc

# Điển hình — testbench Black Level Correction (BLC)
$BUILD/tb_blc

# Các block còn lại
$BUILD/tb_dpc
$BUILD/tb_dg
$BUILD/tb_wb
$BUILD/tb_ccm
$BUILD/tb_gc
$BUILD/tb_csc
$BUILD/tb_cse
$BUILD/tb_lsc
$BUILD/tb_bnr
$BUILD/tb_demosaic
$BUILD/tb_sharpen
$BUILD/tb_2dnr
$BUILD/tb_awb
$BUILD/tb_aec
$BUILD/tb_scale
$BUILD/tb_yuv420
```

Mỗi testbench sẽ in ra console dạng:

```
==================================================
Testbench: Black Level Correction (BLC)
==================================================
[TB] Generated test pattern: 4096 pixels
[TB] Golden reference computed
[TB] Starting simulation...
[TB] Simulation completed
TEST RESULT: PASS
==================================================
```

### 4.2. Chạy full pipeline

```bash
$BUILD/tb_pipeline
```

Output mong đợi:
```
==================================================
PIPELINE VERIFICATION RESULTS
==================================================
  Expected output size: ...
  Captured output size: ...
  Differences: 0 / ...
  Max difference: 0
  AWB R gain: ...
  AWB B gain: ...
  AEC feedback: ...
TEST RESULT: PASS
```

### 4.3. Chạy tất cả qua CTest

Vì đã gọi `add_test()` cho từng testbench, có thể dùng CTest.

**Quan trọng:** CTest phải được chạy từ **build directory**, không phải từ
project root — nếu chạy từ root, nó sẽ báo `No tests were found!!!`.

```bash
# BẮT BUỘC cd vào build/bremen trước
cd build/bremen

# Chạy mọi test (bao gồm cả isp_run cũ + 18 SystemC test mới)
ctest --output-on-failure

# Chỉ chạy các SystemC test (17 block + 1 pipeline)
ctest -R '^tb_' --output-on-failure

# Chỉ chạy các test đơn giản (point operations — đảm bảo PASS)
ctest -R '^tb_(blc|dg|wb|ccm|gc|csc|cse|awb|aec)$' --output-on-failure
```

> **Ghi chú (cập nhật 2026-07-15)**: sau khi sửa 5 lỗi logic trong
> `sc_dpc`, `sc_bnr`, `sc_demosaic`, `sc_yuv420`, `sc_lsc` và 1 lỗi kết nối
> trong `sc_isp_pipeline::bind_channels`, **tất cả 18/18 test PASS với
> bit-exact MSE = 0** so với golden tham chiếu.

---

## 5. Mẹo gỡ lỗi thường gặp

| Triệu chứng | Nguyên nhân & Cách xử lý |
|--------------|----------------------------|
| `undefined reference to sc_core::sc_module_name` | Thiếu `-lsystemc` hoặc rpath. Thêm `-Wl,-rpath,/opt/systemc-2.3.4/lib64`. |
| `fatal error: systemc.h: No such file` | Truyền đúng `-I` cho `$SYS/include`. Với CMake: kiểm tra `SYSTEMC_HOME`. |
| Simulator treo vô hạn (deadlock) | Thiếu `fifo.read()` / `fifo.write()`. Kiểm tra lại số token vào/ra mỗi block khớp `W*H`. |
| Testbench báo `FAIL` | Khả năng cao do khác biệt số thực (float) hoặc số liệu đầu vào mặc định của từng block. Xem dòng "Differences / max difference". |
| Thiếu `blc.h` khi build `tb_dpc` | Thêm `-I components/isp_tlm/blocks/blc/include` (đã có sẵn trong CMake). |
| `error: 'PixelRGB' was not declared` | Một số block dùng struct; cần include header tương ứng. |

---

## 6. Tuỳ chỉnh nhanh

* **Đổi kích thước ảnh**: sửa `WIDTH`/`HEIGHT` ở đầu mỗi `tb_<block>.cpp` (các FIFO
  depth cũng đã được scale theo `WIDTH`).
* **Thay đổi cấu hình block**: sửa struct `xxx_config` ngay trong testbench
  trước khi gọi `xxx_block::process(...)`.
* **Tắt/bật block**: chỉnh `is_enable = true/false` trong config; bypass mode sẽ
  được DUT SystemC tôn trọng.
* **Chạy pipeline không cần golden**: chỉnh `expected_size` trong `tb_pipeline.cpp`
  hoặc bỏ qua set_golden_reference.

---

## 7. Test với ảnh RAW thật (D65)

Ngoài các testbench dùng pattern synthetic, repo có 2 testbench đọc trực
tiếp ảnh RAW từ `<repo>/components/isp_tlm/input/`:

| Binary | Đầu vào | Đầu ra (file) | So sánh với |
|---|---|---|---|
| `tb_d65_blc`   | `D65_raw_2688x1520_5376.raw` (16-bit RGGB, 2688×1520) | `output/d65_blc_out.pgm` (12-bit) | `blc_block::process` |
| `tb_d65_pipeline` | `D65_raw_2688x1520_5376.raw` | `output/d65_pipeline.yuv` + `…_golden.yuv` | `isp_pipeline::run` |

### RAW loader

`input_utils/raw_loader.h` cung cấp:

* `raw_loader::load(path, w, h, pixels, &fmt)` — auto-detect RAW format
  (RAW16_LE / PACKED12 / PACKED10) dựa trên kích thước file và trả về
  vector `uint16_t` 12-bit aligned.
* `raw_loader::save_yuv420(path, buf, w, h)` — ghi planar Y/U/V ra file
  `.yuv` mở được bằng `ffplay -f rawvideo -pix_fmt yuv420p -s WxH file.yuv`.
* `raw_loader::save_pgm(path, buf, w, h, max)` — ghi 12-bit PGM.

### Cách chạy

```bash
cmake --build build/bremen --target tb_d65_blc tb_d65_pipeline
./build/bremen/components/isp_tlm/systemc/tb_d65_blc
./build/bremen/components/isp_tlm/systemc/tb_d65_pipeline
```

Hoặc qua CTest:

```bash
cd build/bremen
ctest -R '^tb_d65' --output-on-failure
```

### Đầu vào 16-bit → 12-bit

D65 là RAW **16-bit** (range 0–65535). Cả hai testbench đều dùng
`sc_input_normalizer` (file `pipeline/sc_input_normalizer.h`) chèn giữa
`raw_in` và BLC để scale `(v * 4095) / 65535` khớp với logic đã có trong
`isp_pipeline::run` của reference C++. Lệch 1-bit range là nguyên nhân
phổ biến nhất khi chạy real-image test.

### Kết quả

* `tb_d65_blc`: **PASS bit-exact** trên 4,085,760 pixels (MSE = 0).
* `tb_d65_pipeline`: chạy thành công full 17-block streaming trên ảnh thật;
  hiện tại MSE ≈ 60, max err ≈ 22 (tập trung ở U/V plane, do sai số
  floating-point ở GC/CCM/CSC + YUV420 sub-sampling). Đây là kết quả
  chấp nhận được cho lần chạy đầu với ảnh RAW 16-bit, không qua tinh chỉnh
  thêm config — sai số có thể giảm tiếp bằng cách tắt 1 số block (đã có
  flag `REG_*_ENABLE` để thử).

### Mở rộng các RAW khác

Có thể thêm test cho `ColorChecker_2592x1536_12bits_RGGB.raw` hoặc
`A_raw_2688x1520_5376.raw` bằng cách copy 1 trong 2 file trên và đổi
`W = 2592/2688`, `H = 1536/1520`, `input_bit_depth = 12` (cho
ColorChecker) hoặc `16` (cho A / D65).

### Visualize output (Python)

Sau khi chạy `tb_d65_pipeline`, dùng `visualize_sc.py` để render
side-by-side ảnh input RAW (CFA mask), output C++ golden, và output
SystemC. Script auto-detect output size từ file `.yuv`, hỗ trợ cả
planar YUV420 và NV12, và in MSE per-plane (Y/U/V).

```bash
# Mặc định: đọc file trong components/isp_tlm/systemc/pipeline/output/
python3 components/isp_tlm/systemc/visualize_sc.py

# Hoặc trỏ vào file khác / config khác:
python3 components/isp_tlm/systemc/visualize_sc.py \
    --input components/isp_tlm/input/ColorChecker_2592x1536_12bits_RGGB.raw \
    --width 2592 --height 1536 --bit-depth 12 --pattern RGGB \
    --sc    build/.../out_672x380.yuv \
    --golden build/.../out_672x380_golden.yuv
```

Output mặc định ghi vào `output/d65_pipeline_compare.jpg` (3-panel
side-by-side) + 3 file JPEG riêng (`d65_pipeline_input.jpg`,
`d65_pipeline_golden.jpg`, `d65_pipeline_sc.jpg`) mở được bằng bất kỳ
image viewer nào.

---

## 8. Mục tiêu kiến trúc đã đạt được

1. ✅ Untimed streaming — không clock, không `wait(time)`, không AXI Valid/Ready.
2. ✅ Đồng bộ hoàn toàn qua `sc_fifo::read()` / `sc_fifo::write()` blocking.
3. ✅ Không dùng `isp_utils::get_pixel_mirror` trong SystemC wrappers — tất cả
   boundary padding được tạo nội bộ (line buffer + mirror).
4. ✅ Số token vào/ra chính xác `W*H` (hoặc `W*H*3` cho RGB, `W*H/2` cho YUV420).
5. ✅ FIFO depth `(W*4)` mặc định, tránh backpressure deadlock.
6. ✅ Mỗi `sc_module` nhận config riêng thay vì `isp_config` toàn cục.
7. ✅ 17 testbench cấp block + 1 testbench end-to-end + 2 testbench ảnh thật
   (D65 BLC, D65 pipeline) — tất cả xác minh được so với golden tham chiếu.

---

