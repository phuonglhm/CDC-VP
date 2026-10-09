# Traceability theo PDF v2.1.11

CTest hiện dùng tiền tố `h264_` cho các tên regression trong bảng dưới.
Kiểm tra tích hợp repo/FX1 được ghi riêng trong `cdc_vp_integration.md`;
địa chỉ SoC và LR/SC guard không thay đổi yêu cầu register/DMA của PDF.

| Yêu cầu | Implementation | Regression / giới hạn |
|---|---|---|
| Ch.3 wrapper/control/DMA partition | EncoderVp, EncoderController | integration_32/64/128 |
| Ch.3 completion phụ thuộc sự kiện | Blocking executor + retained start + reset epoch | completion, reset |
| Ch.4 register/WSTRB | ControlRegs | registers |
| Ch.4 read STAT/STM_LEN clear IRQ | ControlRegs::transport | registers |
| Ch.4 low phase reactivation | ControlRegs rising-edge start | registers, integration_* |
| Ch.4 parameter/error | Controller validation, DMA status | registers, invalid_params, fault |
| Table 20-3 fields/reset | ControlRegs decode + HostDriver encode | register_fields |
| Ch.4 / Table 20-3 CMB working set | activation_frames + persistent frame counter | working_set, frame_address |
| Table 20-3 accepted NAL words | nal_words_accepted progress callback | nal_progress, fault |
| Ch.4.5 host timeout | wait_for_completion + save_diagnostics | host_timeout, host_timeout_stalled |
| Ch.4 BUSY/buffer ownership khi error | abort_and_drain trước finish; false/throw chờ reset | drain, drain_reset, drain_failed, drain_throw |
| Table 20-3 FN/POC đủ độ rộng | Adapter requirements + preflight trước begin | width_exact, width_fn_short, width_poc_short (fixture policy) |
| Ch.4.3 GOP state | I/P M-period; explicit release schedule for B | schedule, release_schedule, duplicate_schedule |
| Ch.5.3 no starvation | Configurable priority + aging queue | priority, priority_fairness |
| Ch.5 width/lanes/4 KiB | DmaBridge | dma_32/64/128 |
| Ch.5 transaction owner | DmaArbiter | concurrent clients trong dma_* |
| Ch.5 final response before done | DDR delayed commit, blocking bridge | completion, fault |
| Ch.5 read FIFO/AXI channel stalls | Chưa mô hình signal-level | Không claim coverage |
| Ch.13 reset release | EncoderVp::release_reset | reset |
| Ch.13 retained completion | StickyCompletion + blocking executor | registers, reset |
| Ch.13 clock constraints/CDC | Ngoài phạm vi LT VP | Không claim RTL qualification |

Tham chiếu trang PDF: chương 3 trang 11–15; chương 4 trang 15–17;
chương 5 trang 17–20; chương 13 trang 41–42.
