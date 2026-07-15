# Kế hoạch đưa PCM MVP thành HEVC all-intra có nén

## Ranh giới

Bitstream hiện có PCM, intra-DC no-residual, hybrid DC/PCM, DC-only TQ,
full-AC TQ, adaptive TU32/TU16 và luma DC/horizontal/vertical. `x265`, FFmpeg encoder và HM không được link vào datapath. HM/spec
chỉ được dùng để đối chiếu thuật toán; decoder ngoài chỉ làm oracle kiểm thử.

## Thứ tự triển khai

| Mốc | Datapath/syntax cần thêm | Điều kiện qua mốc |
|---|---|---|
| A — hoàn tất DC | Reconstruction picture, CTU/CU/TU cố định và padding | Reconstruction nội bộ khớp decoder ở mọi kích thước regression |
| B — hoàn tất DC | Intra DC syntax luma/chroma, no-residual và hybrid PCM fallback | Decoder chuẩn nhận I-slice; QP điều khiển hybrid selection |
| C — hoàn tất DC | Integer forward/inverse DC, quant/dequant theo QP | Quantized DC và reconstruction khớp decoder |
| D — hoàn tất DC | Last position `(0,0)`, greater-one/two, sign và remaining-level CABAC | Mọi DC magnitude/sign regression giải mã sạch |
| D.1 — hoàn tất | Full transform matrix, grouped scan, significance map và level CABAC cho hệ số AC | Reconstruction chi tiết khớp decoder trên QP 0..51 |
| D.2a — hoàn tất | Transform-tree split 32→16 và chọn TU32/TU16 theo SSE | Chất lượng/bitrate bus CIF tốt hơn TU32 cố định; CLI/MMIO bit-exact |
| D.2b — tiếp theo | Bổ sung TU8/TU4 và RD-cost có rate estimate | Giảm artifact và chọn kích thước theo cả rate/distortion |
| E — hoàn tất cho TU16 | Reconstruct ngay sau mỗi TU và dùng ảnh reconstruct làm reference | Raw reconstruct nội bộ khớp byte với decoder ngoài |
| F.1 — hoàn tất | DC/horizontal/vertical, chọn prediction/TU theo SSE | Cải thiện PSNR/SSIM so với adaptive DC và giữ reconstruction equivalence |
| F.2 — tiếp theo | Planar, 31 angular mode còn lại, SATD/RD cost và quadtree đơn giản | Bitrate giảm so với adaptive DC tại cùng chất lượng |
| G | Deblocking, sau đó SAO | Reconstruction sau filter khớp decoder; reference picture đúng |

## Các invariant bắt buộc

- CABAC context phải được khởi tạo theo slice type và QP; không dùng bộ context
  của PCM termination cho coefficient syntax.
- Prediction luôn đọc sample đã reconstruct, không đọc sample input gốc.
- Quantization, inverse quantization, transform shift và clipping dùng số nguyên
  xác định; tránh floating point trong normative datapath.
- SPS/PPS phải báo đúng transform hierarchy, PCM enable, deblocking và SAO theo
  đúng tool thực sự dùng.
- Mỗi feature có thể tắt để bisect lỗi bitstream và giữ PCM làm fallback/debug.

## Regression matrix

Chạy ít nhất các kích thước `32×32`, `96×66`, `352×288`; frame phẳng, gradient,
checkerboard và `bus_cif`. Với mỗi QP `0, 22, 27, 32, 37`:

1. FFprobe nhận codec/kích thước/pixel format đúng.
2. FFmpeg decode không báo lỗi.
3. Reconstruction dump từ encoder khớp byte với raw decoder output.
4. Bitstream DC/planar nhỏ hơn PCM trên vector có tương quan không gian.
5. Khi QP tăng, bitrate có xu hướng giảm và distortion có xu hướng tăng.
6. `ldd` và symbol scan xác nhận không link `x265`/`libavcodec`.

PCM chỉ có tiêu chí input bằng decoded. Từ mốc có quantization trở đi, không
dùng `cmp input decoded`; phải dùng `cmp reconstructed decoded` và đo PSNR/SSIM
riêng giữa input với decoded.

## Definition of done cho encoder all-intra

- Tự sinh I-slice nén thật cho YUV420p 8-bit mà không gọi codec ngoài.
- DC, planar và angular mode hoạt động; transform/quant/CABAC coefficient hoàn
  chỉnh; reconstruction/reference đúng.
- Không crash hoặc sinh stream lỗi trên regression matrix.
- Có báo cáo bitrate, PSNR, cycle counter và test lỗi MMIO/DMA.
- Kết quả reproducible bằng một target `make verify-intra`.

Inter prediction, GOP, motion estimation và rate control không thuộc definition
of done all-intra; chúng là giai đoạn encoder video đầy đủ sau đó.
