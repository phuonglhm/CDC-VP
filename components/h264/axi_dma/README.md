# AXI DMA

Owner: Huy Nguyen Huynh Quoc.

Chương 5: ordered arbiter, width/lane adapter và phân đoạn bộ nhớ.

CMake target: `h264_dma`. Public header: h264/dma/dma_bridge.h; h264/dma/dma_arbiter.h.

Regression hiện nằm trong integration_tests vì cần host + DDR + top. Chạy từ thư mục components/h264:
`ctest --test-dir build/vp --output-on-failure`. Nhóm test liên quan: dma_32, dma_64, dma_128, completion, fault.

Xem `interfaces/docs/interface_contracts.md` và `integration_docs/model_limitations.md` trước khi
thay interface. Tất cả header dùng .h. Không sao chép source giữa các task.

