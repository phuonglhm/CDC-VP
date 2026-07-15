# Kiến trúc và lộ trình

## 1. Ranh giới chức năng

MVP hiện tại chịu trách nhiệm tự sinh toàn bộ cấu trúc bitstream HEVC cần thiết:

| Khối | Trách nhiệm |
|---|---|
| Raw YUV DMA | Đọc frame YUV420p planar, đóng gói `FrameTransaction` |
| Pad/crop | Pad cạnh đến bội số CTU 32 và báo crop trong SPS |
| Parameter set writer | Sinh VPS, SPS và PPS bằng Exp-Golomb/fixed fields |
| PCM slice writer | Chia frame thành CTU, sinh slice header, `split_cu_flag`, `pcm_flag`, raw samples |
| CABAC | Mã hóa regular/termination/bypass bin và flush arithmetic state |
| NAL packer | RBSP→EBSP, chèn emulation-prevention byte và Annex-B start code |
| Sink | Ghi elementary bitstream `.h265` |

FFmpeg/FFprobe không nằm trong encoder. Chúng chỉ là decoder độc lập ở testbench.

## 2. Giao dịch TLM và control plane SoC

Project giữ hai frontend dùng chung lõi codec:

1. `tlm_pipeline` là đường functional đơn giản cho CLI/file regression.
2. `VpuMmioDevice` là đường SoC: CPU lập trình register, VPU làm DMA qua
   `MemoryInterface`, cập nhật trạng thái và phát IRQ.

Adapter `VpuTlmMmio` ánh xạ model thứ hai sang TLM-2.0: target socket nhận MMIO
32-bit, initiator socket phát transaction DMA byte-addressed. Worker SystemC
gọi `tick()` mỗi cycle. M7a dùng FIFO hữu hạn giữa DMA input, prediction,
transform/quant, CABAC và DMA output; full FIFO gây backpressure. Latency từ
memory target vẫn được cộng vào simulation time. IRQ là signal mức và được hạ
bằng W1C.

## 3. Vì sao bắt đầu bằng PCM

Một encoder H.265 có nén thật phải giải quyết song song tìm mode, prediction,
transform, quantization, coefficient coding, CABAC context, reference picture,
rate control và in-loop filter. Nếu làm tất cả ngay từ đầu, lỗi syntax và lỗi
thuật toán khó tách rời.

PCM bỏ qua prediction/transform nhưng vẫn đi qua cú pháp chuẩn HEVC. Vì vậy nó
là mốc bring-up hữu ích cho VPU:

- kiểm tra raw DMA và thứ tự Y/Cb/Cr;
- kiểm tra bit writer, VPS/SPS/PPS/NAL;
- kiểm tra slice/CTU và CABAC termination;
- có golden test byte-exact bằng decoder độc lập;
- tạo giao diện ổn định để thay dần PCM CU bằng coded CU.

## 4. Lộ trình thành encoder có nén thật

| Giai đoạn | Bổ sung | Tiêu chí hoàn thành |
|---|---|---|
| M0 — hoàn tất | PCM all-intra, Annex B | Decode được và raw round-trip byte-exact |
| M0.5 — hoàn tất | MMIO register, DMA source/destination, START/DONE/ERROR/IRQ | Regression trạng thái, lỗi và IRQ; decode byte-exact qua đường MMIO |
| M0.6 — source sẵn sàng | TLM-2.0 MMIO target + DMA initiator + IRQ | Build/chạy trong môi trường có SystemC và ghép được interconnect |
| M1 — DC hoàn tất | Intra DC no-residual và hybrid DC/PCM cho CU 32×32 | Decode sạch; reconstruction khớp model nội bộ; bitrate thay đổi theo QP |
| M2 — DC hoàn tất | Integer transform, quant/dequant và CABAC cho hệ số DC | Reconstruction khớp decoder; stream nhỏ hơn raw; QP tác động level/bitrate |
| M2.1 — hoàn tất | Full AC transform, grouped diagonal scan, significance map và level CABAC | PSNR tăng rõ rệt; reconstruction khớp decoder và CLI/MMIO bitstream bằng nhau |
| M2.2 — hoàn tất | Split transform-tree 32→16, prediction/reconstruction tuần tự và chọn TU theo SSE từng CTU | QP26 bus CIF đạt 38.737 dB; reconstruction khớp decoder; CLI/MMIO bit-exact |
| M2.3 — tiếp theo | Thêm TU 8×8/4×4 và rate-distortion cost có ước lượng số bit | Chọn kích thước theo cả bitrate lẫn distortion |
| M3a — hoàn tất | Luma DC/horizontal/vertical prediction và chọn mode/TU theo SSE | QP26 bus CIF đạt 38.773 dB, SSIM 0.968719; reconstruction và CLI/MMIO bit-exact |
| M3b — tiếp theo | Planar, 31 angular mode còn lại, SATD/RD cost và quadtree CU | Bitrate tốt hơn M2.2 ở cùng PSNR |
| M4 | Deblocking và SAO | Reconstruction khớp decoder chuẩn |
| M5 | P-slice, reference frame, integer motion search | Encode chuỗi video với I/P GOP |
| M6 | Rate control CBR/VBR, VBV, timing/VUI | Đạt target bitrate và buffer constraints |
| M7a — hoàn tất | Bounded FIFO, backpressure, one-cycle `tick()` và performance counter | Deterministic theo cấu hình; bitstream không đổi; có regression FIFO full/empty |
| M7a-SC — hoàn tất | SystemC-native controller/stages, `sc_fifo`, clock/reset, TLM MMIO target và burst DMA initiator | SystemC depth 1/4 bit-exact với CLI; depth 1 có backpressure; decoder/reconstruction exact |
| M7b — tiếp theo | Hiệu chuẩn latency/bandwidth với RTL và bus CDC-VP, thêm arbitration/outstanding burst | Cycle/stall trace khớp RTL theo từng CTU |
| M8 | RTL partition và co-simulation | TLM/RTL bitstream equivalence theo test vector |

## 5. Phân hoạch VPU đề xuất

Khi đi từ functional model sang kiến trúc SoC, giữ các khối sau thành ranh giới
riêng: input DMA, reference/reconstruction SRAM, intra predictor, motion
estimation, transform/quantization, CABAC, in-loop filter và output DMA. CPU chỉ
cấu hình register, descriptor và interrupt; không gọi API x265 trong datapath.

## 6. Kiểm thử bắt buộc cho mỗi mốc

- Header trace không có trường out-of-range.
- Decoder độc lập không báo lỗi.
- So sánh reconstruction với model nội bộ; riêng PCM phải byte-exact với input.
- Test kích thước bằng CTU và không bằng CTU.
- Test nhiều frame, file bị thiếu byte, width/height lẻ và tham số sai.
- Regression cố định cho CABAC bitstream trước khi thêm context mới.
