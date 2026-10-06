# Bàn giao Bus System để thay component FX1 trong CDC-VP

Nguồn phát triển là thư mục `Bus System`. Đích dự kiến: `components/FX1_Components/bus`. Không trộn API cũ với header/source của bản này.

## File bàn giao

- `include/`, `src/`, `CMakeLists.txt`, `cmake/`: thư viện độc lập, không cần test-support để build.
- `tests/`, `scripts/`: regression, integration harness và installed consumer.
- `README.md`, `docs/`, `Bus Config.xlsx`, `.gitignore`: tài liệu và nguồn đối chiếu. Giữ workbook tại root component để các link hiện tại đúng.
- Không mang theo `build/`, `build-*/`, binary, cache, thư viện SystemC cài trên máy hoặc file tạm Excel.

Repo cha và platform cần thêm subdirectory/link target và chuyển binding sang named ports. Các file cấp cha do đội tích hợp xử lý. README và CDC_VP_INTEGRATION.md mô tả API mới; không copy wiring cũ `fabric.rom`/`fabric.pp0`.

## Kiểm thử trước bàn giao

```bash
export SYSTEMC_HOME=/path/to/systemc-install
export CDC_VP_SOURCE_DIR=/path/to/CDC-VP
bash scripts/build.sh
bash scripts/check_cdc.sh
```

Ba regression component phải pass; CDC integration và consumer phải pass. Test CMake standalone shared/install bằng hướng dẫn integration nếu thay đổi export/build. Tất cả output ở trong thư mục build của Bus System; không cần sửa source CDC-VP để chạy harness.

## Phần platform cần tiếp tục

1. Chốt base/size của SRAM, IRAM và CSR TBD; bind IP thật tương ứng.
2. Xác nhận driver/CPU dùng APB 1/2/4 byte, byte enable và timing blocking phù hợp.
3. Chạy firmware boot và workload nhiều DMA trên platform; component test không thay thế việc này.
4. Nếu yêu cầu hiệu năng/protocol cao hơn, thiết kế thêm nonblocking/DMI hoặc AXI burst/ID/QoS; hiện chúng ngoài phạm vi.

Workbook/ảnh tham chiếu là tài liệu ngoài source code; đội bàn giao cần có thông tin nguồn/quyền phân phối theo CONTRIBUTING của CDC-VP. Lượt phát triển này không sửa THIRD_PARTY hoặc các file bên ngoài Bus System.
