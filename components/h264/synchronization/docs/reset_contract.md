# Reset / synchronization

Owner: Huy. Public API: `h264/sync/reset_domain.h` (header-only).
ResetDomain có generation tăng khi assert, active giữ đến release. EpochExtension truyền
generation theo TLM tới DDR. StickyCompletion lưu trạng thái, event chỉ dùng đánh thức.

Không nối clock mới cho từng module trong LT VP. EncoderVp release reset sau 2 * period
(mặc định 8 ns/chu kỳ). Tín hiệu rstn active-low; IRQ được cập nhật theo delta-cycle SystemC.
Test reset và chính sách pending transaction được ghi trong integration_docs/model_limitations.md.
