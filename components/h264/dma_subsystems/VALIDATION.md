# DMA validation — 2026-10-10

Môi trường: Ubuntu WSL, GCC 15.2.0, CMake 4.2.3, Accellera SystemC 2.3.4
ở `/opt/systemc-2.3.4`.

| Kiểm tra | Kết quả |
|---|---|
| Core: 78 original + 194 review/external-memory assertions | 272 pass, 0 fail |
| Shared-bus integration, DMA 32-bit | 284 pass, 0 fail |
| Shared-bus integration, DMA 64-bit | 284 pass, 0 fail |
| Shared-bus integration, DMA 128-bit | 284 pass, 0 fail |
| Bounded stalled-channel activation | 12 pass, 0 fail |
| Tổng mỗi lần chạy CTest đầy đủ | **1.136 assertions, 5/5 suites pass** |
| CMake standalone và CDC-VP root build | Cả hai build/test pass |
| Standalone, `-Wall -Wextra -Wpedantic -Werror` | Compile/link pass |
| Core ASan + UBSan, bao gồm borrowed-memory lifetime | 272 pass; không diagnostic |

[Standalone log](tlm/tests/evidence/standalone_tlm_20261010.log),
[root build log](tlm/tests/evidence/root_tlm_20261010.log),
[core sanitizer log](tlm/tests/evidence/core_sanitized_20261010.log).
Các log này được giữ qua `.gitignore`. SystemC wrapper không được tuyên bố
đã chạy ASan; sanitizer evidence ở trên dành cho core.

## Historical core evidence

Các log dưới đây giữ nguyên nội dung của các lượt review trước khi bổ sung
external-memory/SystemC integration; không thay thế kết quả 272 assertions ở trên:

- [Core fixes](tb/evidence/dma_tests_fixed_20261010.log).
- [Core sanitizer snapshot](tb/evidence/dma_tests_sanitized_20261010.log).
- [Spec review](tb/evidence/spec_review_tests_20261010.log).

Hướng dẫn build, contract và giới hạn mô hình nằm trong [README](README.md).
Đợt dọn folder chỉ sắp xếp tài liệu/log và ignore generated files; không sửa
implementation/tests hay chạy lại regression. Không suy ra full encoder hoặc
RTL signoff từ kết quả functional DMA VP.
