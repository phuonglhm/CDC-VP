# Encoder top-level

Owner: Huy Nguyen Huynh Quoc.

Chương 3: top-level, pipeline stub và kiểm thử nhiều module.

CMake target: `h264_top`. Public headers: h264/top/encoder_vp.h, h264/top/pipeline_factory.h.

Sau refactor, `h264_top` chỉ chứa top/wiring; `h264_pipeline_stub` là target riêng chứa stub.
`FrameExecutorIf` nằm trong interfaces; `PipelineFactory` chọn assembly lúc elaboration.
Header stub được export bởi h264_pipeline_stub, không nằm trong API của h264_top.
Hướng dẫn thêm module release và full-pipeline testbench:
[full_pipeline_testbench.md](docs/full_pipeline_testbench.md).

Regression hiện nằm trong tests vì cần host + DDR + top. Chạy từ thư mục components/h264:
`ctest --test-dir build/vp --output-on-failure`. Nhóm test liên quan: toàn bộ regression CTest.

Xem `interfaces/docs/interface_contracts.md` trước khi
thay interface. Tất cả header dùng .h. Không sao chép source giữa các task.

