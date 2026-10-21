# Hướng dẫn build và kiểm thử

Chạy các lệnh từ thư mục gốc CDC-VP trong Linux/WSL. Cần C++17, CMake và
SystemC 2.3.4. Bộ kiểm thử đầy đủ cần thêm GCC/gcov, Python 3, OpenSSL CLI
và CMake/CTest từ 3.21.

## Build component

```sh
cmake -S components/FU3_Components/AES-256 -B build/fu3-aes \
  -DSYSTEMC_HOME=/opt/systemc-2.3.4 \
  -DCMAKE_BUILD_TYPE=Debug -DFU3_AES_BUILD_TESTS=ON
cmake --build build/fu3-aes -j4
ctest --test-dir build/fu3-aes --output-on-failure
```

Các tùy chọn:

| Tùy chọn | Mặc định | Chức năng |
| --- | --- | --- |
| `SYSTEMC_HOME` | `/opt/systemc-2.3.4` | Đường dẫn cài đặt SystemC |
| `FU3_AES_BUILD_TESTS` | Theo `BUILD_TESTING`/`CDC_BUILD_TESTS` | Build testbench |
| `FU3_AES_BUS_TESTS` | ON | Thêm kiểm thử kết nối FU3 bus |
| `FU3_AES_ORACLE_TESTS` | OFF | Tạo expected độc lập bằng OpenSSL |
| `FU3_AES_COVERAGE` | OFF | Đo coverage hai file triển khai AES bằng GCC |

## Chạy toàn bộ kiểm thử

```sh
bash components/FU3_Components/AES-256/scripts/run_full.sh
```

Script build Debug, UBSan và Release với `-Wall -Wextra -Wpedantic -Werror`,
bật kiểm thử bus/OpenSSL và thu thập source coverage cho bản Debug. Có thể đặt
biến môi trường `SYSTEMC_HOME` và `JOBS` để đổi đường dẫn SystemC/số luồng build.
Toàn bộ file sinh ra nằm trong `build/`.

## Chọn nhóm kiểm thử

```sh
ctest --test-dir build/fu3-aes -L '^spec$' --output-on-failure
ctest --test-dir build/fu3-aes -L '^vp_policy$' --output-on-failure
ctest --test-dir build/fu3-aes -L '^reference$' --output-on-failure
ctest --test-dir build/fu3-aes -L integration --output-on-failure
ctest --test-dir build/fu3-aes -R '^fu3_aes_policy_reset_boundaries$' -V
```

| Nhóm | Nội dung | Nguồn expected |
| --- | --- | --- |
| `spec` | AES-128/256 hai chiều, lựa chọn nửa khóa AES-128, từng phase và round key | HDS viện dẫn FIPS 197; hằng số Appendix C.1/C.3 |
| `vp_policy` | Thanh ghi, TLM/debug, INIT/NEXT, busy, READY/VALID, reset, timing và FU3 bus | Quy ước VP trong [spec-decisions.md](spec-decisions.md) |
| `reference` | Dữ liệu bit-walking/pseudorandom; nhiều block; tái sử dụng/thay khóa và đổi mode | OpenSSL CLI độc lập |

Chỉ expected từ yêu cầu đã rõ mới dùng `SPEC_PASS/FAIL`. Expected từ OpenSSL
dùng `REFERENCE_PASS/FAIL`; các lựa chọn VP dùng `POLICY_PASS/FAIL`.
Các yêu cầu chưa rõ hoặc mâu thuẫn cùng nhóm test liên quan được ghi trong
[spec-decisions.md](spec-decisions.md); test POLICY/REFERENCE đạt không đóng
các vấn đề HDS này.

## Dữ liệu kiểm thử

- `tests/test_hds_core.cpp`: vector mã hóa/giải mã cố định và nửa khóa AES-128.
- `tests/test_fips_trace.cpp`, `tests/fips197_rounds.txt`: đối chiếu từng trạng
  thái, thứ tự phase và round key với ví dụ FIPS 197.
- `tests/test_aes.cpp`, `tests/test_corners.inc`: driver MMIO, giao dịch lỗi,
  byte-enable, lifecycle, reset sát cạnh hoàn tất, busy protection và bus.
- `tests/test_core.cpp`: lifecycle lõi và đối chiếu OpenSSL.
- `scripts/generate_vectors.py`: tạo dữ liệu OpenSSL trong thư mục build;
  seed cố định `0xAE500000 + key_bits`, không dùng model làm reference.
- `scripts/import_fips_trace.py`: tiện ích nhập lại dữ liệu literal từ bản
  PDF FIPS đã pin; chỉ cần `pypdf` khi dùng tiện ích này.

## Xem lỗi

Chạy lại test lỗi với `ctest -R '<tên-test>' -V`. CTest lưu log tại
`build/<tên-build>/Testing/Temporary/LastTest.log`. Source coverage của bản Debug
nằm tại `build/fu3-aes/coverage/` (`gcov.log` và các file `.gcov`).

Test MMIO có watchdog mô phỏng 20 ms. CTest giới hạn 30 giây mỗi test và
60 giây với nhóm reference. Dừng clock có thể giữ một giao dịch đang chờ;
platform tích hợp cần watchdog riêng.

Kiểm thử chức năng TLM không xác nhận AXI ở mức tín hiệu, tương đương RTL
hoặc boot toàn bộ SoC. Các điểm HDS cần xác nhận nằm trong
[spec-decisions.md](spec-decisions.md).
