# Clock, reset and synchronization

Owner: Huy Nguyen Huynh Quoc.

Chương 13: tiện ích reset generation và sticky completion (header-only).

CMake target: `h264_sync`. Public header: h264/sync/reset_domain.h.

Regression hiện nằm trong integration_tests vì cần host + DDR + top. Chạy từ thư mục components/h264:
`ctest --test-dir build/vp --output-on-failure`. Nhóm test liên quan: reset, registers.

Xem `interfaces/docs/interface_contracts.md` trước khi
thay interface. Tất cả header dùng .h. Không sao chép source giữa các task.

