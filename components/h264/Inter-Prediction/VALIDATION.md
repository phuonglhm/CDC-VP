# Inter VP validation — 2026-10-10

Đối chiếu [SISLAB HAS v2.1.11](../SISLAB_H264_AVC_HAS_Spec_Detailed_v2.1.11.md)
§8 và §6.2. Phạm vi là functional transaction VP với explicit search/rate/MVP
input. Lần này mở rộng tests và CMake trong Inter-Prediction; SHA256 của 9 file
production header/source giữ nguyên so với baseline. Không sửa module/parent khác.

## Kết quả thực thi

| Configuration | Environment | Kết quả |
|---|---|---|
| Full VP Release | WSL/Linux, GNU 15.2.0, C++17, SystemC 2.3.4 | 18/18 PASS |
| Full VP Debug + UBSan | -fsanitize=undefined; halt_on_error=1:print_stacktrace=1 | 18/18 PASS, không sanitizer diagnostics |
| Core Debug + ASan/UBSan | TLM/DMA OFF; -fsanitize=address,undefined; leak detection ON | 3/3 PASS, không sanitizer diagnostics |
| Full VP Debug + gcov | --coverage; 7 production .cpp files | 18/18 PASS |
| Core Release Windows | MSYS2 UCRT64 GNU 16.1.0; TLM/DMA OFF | 2 PASS, 1 NOT RUN: Application Control |

Final Linux/UBSan/coverage rerun khoảng 13:47–13:48 +07. ASan core và native core
được chạy trên bản core tests cuối; thay đổi sau đó chỉ bổ sung TLM rejection.
Sanitizers instrument module/tests và DMA headers; không rebuild thư viện SystemC.
Windows chặn khởi chạy `h264_inter_core_tests.exe` với thông báo
“An Application Control policy has blocked this file”. CTest elevated cũng bị
chặn; chưa hoàn tất toàn bộ native Windows suite. Hai executable interpolation
và limits chạy PASS. Cùng core test bị chặn đã PASS trên Linux và sanitizers.

Raw LastTest logs, CTest registration, config, coverage summary/script và SHA256:
[full_test_20261010.txt](tests/evidence/full_test_20261010.txt).
[validation_20261010.txt](tests/evidence/validation_20261010.txt) là baseline
13 tests lúc 12:49, không phải evidence của suite mở rộng hiện tại.
Evidence được giữ lại sau khi dọn build.

| Test group | Assertions |
|---|---:|
| Core SAD/cache/search/MC/EEI | 103190 |
| Interpolation oracle, 3 seeds | 144002 |
| Limits / geometry / invalid inputs | 30836 |
| Actual SW DMA integration bus 32 / 64 / 128 | 33686 / 29078 / 27254 |
| Normal / zero-latency TLM | 11337 mỗi group |
| Shared / local / disable cancellation | 601 mỗi group |
| Retag / incoming-delay / stale-malformed-rearm | 594 / 605 / 51 |
| Partitions / partitions-zero | 107359 mỗi group |
| Boundaries / boundary-zero | 19843 / 19793 |

Counts bao gồm assertions của harness/protocol, không phải tỷ lệ HAS coverage.
Không có expected-failure test. Bench yêu cầu driver hoàn thành và helper action
được settle; simulation starvation không được tính PASS.

## Coverage

GCC 15.2.0 gcov JSON, counters được xóa trước final coverage run. Chỉ tính 7
production .cpp; không tính headers, tests, standard library, SystemC hoặc DMA.
Covered line/branch outcome nghĩa là execution count lớn hơn 0.

| Source | Lines | Branch outcomes taken |
|---|---:|---:|
| cache/reference_cache.cpp | 70/70 | 117/154 |
| eei/motion_syntax.cpp | 4/4 | 3/6 |
| fme/interpolation.cpp | 50/50 | 62/74 |
| fme/search.cpp | 74/74 | 120/160 |
| ime/sad_tree.cpp | 17/18 | 16/18 |
| ipc/inter_tlm.cpp | 150/150 | 232/321 |
| mc/motion_compensation.cpp | 47/48 | 77/98 |
| Total | **412/414 (99,52%)** | **627/831 (75,45%)** |

Hai dòng chưa chạy là closing braces tại sad_tree.cpp:24 và
motion_compensation.cpp:40. Gcov branch denominator gồm compiler exception/unwind
paths; chưa đạt 100% branch coverage. Số liệu này không chứng minh mọi yêu cầu HAS.

## Hành vi đã kiểm tra

IME kiểm tra 41 partition positions, max SAD 65280, wide rate, overflow, stable
ties và randomized oracle. Nội suy kiểm tra 16 luma/64 chroma phases, âm/border,
constant/ramp/impulse/checkerboard/step/random và signed diagonal intermediate.

Limits chạy 40 cases: 5 kích thước (16×16, 176×144, 1280×720, 1920×1088,
4096×4096) × 4 cực trị MV signed ±65536 × 2 MB positions. Sparse corner fixtures
kiểm tra clamping, inward fractional refinement và toàn bộ 384 MC samples;
không encode full frame. Kiểm tra tối đa 4096 integer/64 fractional candidates,
invalid dimensions/plane/search/MC, atomic refill và retag giữa core evaluation.

TLM partition regression chạy 246 cases mỗi timing profile: 3 seeds × P/B × 41
positions. Source/oracle độc lập tạo fractional winner, kiểm tra list/tag/MV/MVD,
residual, từng mẫu Y/U/V, independent transform-block order, phase/last, repeated
Peek, bad Accept, consumer stall và final done. Tổng hai profiles: 492 cases.

Deadline regression chạy 144 cases mỗi profile: shared/local reset, disable hoặc
retag × 12 sites × {-1,0,+1} ps. Sites gồm incoming delays, IME/FME service/compare,
Commit, Peek/sample, Accept/final Accept và repeated held Peek. Normal: 46 accepted,
98 cancelled; zero service: 96 accepted, 48 cancelled. Tại timestamp bằng deadline,
chấp nhận thứ tự tuần tự hợp lệ của SystemC; không khẳng định priority RTL. Kiểm
tra completion/cancel timestamp, owner invalidation và không stale output/done.

Existing TLM tests còn kiểm tra row-start 8+8 columns, unrelated-list refill,
delayed U, B startup, P/B/P, current-MB snapshot trước incoming delay, benign
notification giữ deadline, stale-at-entry giữ owner, reset/enable-low rearm,
malformed transport, Peek/Accept trước Commit, duplicate Commit và local-only
waits khi không có EpochExtension.

Actual SwDma/H264Arb/AxiMasterBridge/DdrModel integration dùng 48×32, ba pictures
P/B/P và 18 MBs mỗi bus width 32/64/128. Kiểm tra overlap reuse, successful read
residency, RRESP fault/retry, stale outstanding responses sau retag, 4KiB splitting
và predictor từ reconstructed DDR reference.

## Tái lập

Chạy từ Inter-Prediction, xem [README](README.md) cho full/core build. Environment
đã dùng SystemC include/library tại /opt/systemc-2.3.4, cùng compiler/C++17 ABI.

```sh
cmake -S . -B build/full -DCMAKE_BUILD_TYPE=Release \
  -DSYSTEMC_INCLUDE_DIR=/opt/systemc-2.3.4/include \
  -DSYSTEMC_LIBRARY=/opt/systemc-2.3.4/lib/libsystemc.so
cmake --build build/full -j 4
ctest --test-dir build/full --output-on-failure

cmake -S . -B build/ubsan -DCMAKE_BUILD_TYPE=Debug \
  -DSYSTEMC_INCLUDE_DIR=/opt/systemc-2.3.4/include \
  -DSYSTEMC_LIBRARY=/opt/systemc-2.3.4/lib/libsystemc.so \
  -DCMAKE_CXX_FLAGS=-fsanitize=undefined \
  -DCMAKE_EXE_LINKER_FLAGS=-fsanitize=undefined
cmake --build build/ubsan -j 4
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
  ctest --test-dir build/ubsan --output-on-failure

cmake -S . -B build/asan-core -DCMAKE_BUILD_TYPE=Debug \
  -DH264_INTER_BUILD_TLM=OFF -DH264_INTER_BUILD_DMA_TESTS=OFF \
  -DCMAKE_CXX_FLAGS=-fsanitize=address,undefined \
  -DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address,undefined
cmake --build build/asan-core -j 4
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
  UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
  ctest --test-dir build/asan-core --output-on-failure
```

Coverage: cấu hình full Debug với CXX/link flags `--coverage`, build rồi chạy
CTest tuần tự trên fresh build/counters. Từ `build/coverage`:

```sh
find CMakeFiles/h264_inter_core.dir CMakeFiles/h264_inter_tlm.dir \
  -name '*.gcno' -exec gcov -b -j {} +
python3 ../../tests/evidence/coverage_report.py .
```

## Giới hạn nghiệm thu

Xem [spec mapping](docs/SPEC_MAPPING.md) và [tests còn mở](docs/TEST_PLAN.md).
Suite này kiểm tra đầy đủ các tests hiện có của VP, chưa phải full encoder signoff.
Chưa xác nhận SISLAB search/rate/MVP/comparator policy bằng Algorithm Spec/RTL;
chưa có skip/direct, bi/weighted prediction, autonomous partition selection,
actual TQ/entropy/DF feedback, QCIF byte/PSNR/601-frame qualification hay RTL cycle
equivalence. Không suy ra fixed latency/throughput từ simulation time.

Parent CMake/export chưa gắn module. Khi push riêng folder này, cần shared
contracts và DMA dependencies trong repo đích; tắt DMA tests nếu chưa có sibling
DMA. Spec được đọc ở sibling path, không chép vào module.
