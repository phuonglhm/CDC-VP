# M2/M2.1/M2.2/M3a verification snapshot

Vector: `bus_cif.yuv`, YUV420p 8-bit, 352×288, 150 frame, raw size
22,809,600 byte. Decoder oracle: FFmpeg HEVC decoder. Mọi reconstruction dưới
đây đã `cmp` byte-exact với raw decoder output.

| Mode | QP | Bitstream | PSNR trung bình | Ghi chú |
|---|---:|---:|---:|---|
| PCM | 26 | 22,991,608 B | vô hạn | Lossless golden path |
| hybrid-DC | 0 | 22,991,606 B | vô hạn | QP0 giữ PCM trên vector này |
| hybrid-DC | 26 | 21,414,645 B | 30.389 dB | Một phần CTU chuyển sang DC no-residual |
| hybrid-DC | 51 | 6,753,974 B | 15.562 dB | Nén mạnh, mất nhiều chi tiết |
| intra-DC-TQ | 0 | 323,977 B | 17.672 dB | Chỉ hệ số DC mỗi plane/CTU |
| intra-DC-TQ | 26 | 231,915 B | 17.671 dB | Quantized DC CABAC |
| intra-DC-TQ | 51 | 176,546 B | 17.649 dB | DC level thô hơn |
| intra-full-TQ | 0 | 14,372,518 B | 58.561 dB | Full DC/AC, gần lossless nhưng vẫn quantized |
| intra-full-TQ | 26 | 3,248,885 B | 38.621 dB | Full AC; SSIM tổng 0.967747 |
| intra-full-TQ | 51 | 279,873 B | 22.869 dB | QP cao loại bỏ phần lớn AC |
| intra-full-TQ16 | 26 | 3,265,194 B | 38.586 dB | Split cố định, dùng để regression transform-tree |
| intra-adaptive-TQ | 26 | 3,206,526 B | 38.737 dB | Chọn TU32/TU16 theo SSE; SSIM tổng 0.968614 |
| intra-directional-TQ | 26 | 3,219,490 B | 38.773 dB | Chọn DC/H/V và TU theo SSE; SSIM tổng 0.968719 |

PSNR thấp của `intra-dc-tq` là giới hạn dự kiến của một hệ số trên block 32×32,
không phải lỗi bitstream. M2.1 full-AC tăng PSNR QP26 thêm khoảng 20.95 dB và
vẫn giữ reconstruction equivalence. Bitstream CLI/MMIO QP26 có cùng SHA-256
`7ff5c5d1b0f1e64ce2fd467ce4e03836e1621b12581d19a2e24e303cec97e3b5`;
reconstruction/decoded có cùng SHA-256
`07aa6a3db71a010d4cc417e545782faa2011bfcdca24d285ce2af3a77f503c01`.

M2.2 adaptive tăng PSNR thêm khoảng 0.116 dB, tăng SSIM từ 0.967747 lên
0.968614 và giảm 42,359 byte so với M2.1 TU32 cố định. Bitstream adaptive
CLI/MMIO có cùng SHA-256
`179f402a0456717fc9e9d706655a270ac5a9f16341c8570fb777a7fedd900a14`;
reconstruction/decoded có cùng SHA-256
`70de317f9d9870d0913e56f9178f70461eece9b6fbf2ee1b0556eb0b41d069ec`.

M3a tăng PSNR thêm khoảng 0.036 dB và SSIM thêm 0.000105 so với M2.2, nhưng
tăng 12,964 byte vì quyết định hiện tối ưu distortion, chưa có rate term.
Bitstream directional CLI/MMIO có cùng SHA-256
`1c1d7977e99308a64e0d8230c1e4e2530d3be51296c9d91aba1bfc542160d7f4`;
reconstruction/decoded có cùng SHA-256
`648ceb8152fab846aff353dd3bb688abe4506d4d82e010263be09b86b4abdb7a`.

M2.3 sẽ thêm TU8/TU4 và rate term vào quyết định RDO; M3b sẽ thêm planar và
31 hướng angular còn lại.
