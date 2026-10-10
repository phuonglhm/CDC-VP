# Control and registers

Owner: Huy Nguyen Huynh Quoc.

Chương 4: register TLM target và controller activation/frame.

CMake target: `h264_control`. Public header: h264/control/control_regs.h; h264/control/encoder_controller.h.

Regression hiện nằm trong tests vì cần host + DDR + top. Chạy từ thư mục components/h264:
`ctest --test-dir build/vp --output-on-failure`. Nhóm test liên quan: registers, invalid_params, integration_*.

Xem `interfaces/docs/interface_contracts.md` trước khi
thay interface. Tất cả header dùng .h. Không sao chép source giữa các task.

