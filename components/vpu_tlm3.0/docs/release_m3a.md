# M3a directional-intra release

## Phạm vi đã hoàn tất

- Encoder tự sinh HEVC/H.265 Annex-B, không gọi hoặc link x265/FFmpeg encoder.
- Luma intra prediction có DC, horizontal và vertical; chroma dùng DC.
- Chọn giữa TU32 DC, TU16 DC, TU16 horizontal và TU16 vertical theo SSE.
- Integer transform, quant/dequant, coefficient CABAC, internal reconstruction.
- Hai frontend CLI và MMIO/DMA/IRQ dùng chung lõi encoder.
- Tại mốc M3a, MMIO dùng `VERSION=0x00030007`; bản M7a hiện tại đã nâng thành
  `0x00030008`. `ENCODER_MODE=7` vẫn là directional mode.

## Kết quả xác minh

Vector `bus_cif.yuv`, YUV420p 8-bit, 352×288, 150 frame, QP26:

| Thuộc tính | Kết quả |
|---|---:|
| Bitstream | 3,219,490 byte |
| PSNR trung bình | 38.772783 dB |
| SSIM tổng | 0.968719 |
| CLI so với MMIO | bit-exact |
| Internal reconstruction so với FFmpeg decode | byte-exact |

SHA-256 bitstream:
`1c1d7977e99308a64e0d8230c1e4e2530d3be51296c9d91aba1bfc542160d7f4`.

SHA-256 reconstruction:
`648ceb8152fab846aff353dd3bb688abe4506d4d82e010263be09b86b4abdb7a`.

`make verify-m3` đã qua toàn bộ regression cũ và mới. QP
`0, 22, 27, 32, 37, 51` trên vector 32×32 đều decode sạch và khớp internal
reconstruction. AddressSanitizer và UndefinedBehaviorSanitizer đã qua; phần
LeakSanitizer được tắt vì môi trường test không cho đọc `/proc`.

Dependency động của binary release chỉ gồm runtime C/C++ chuẩn (`libstdc++`,
`libgcc_s`, `libc`, `libm`). FFmpeg/FFprobe chỉ xuất hiện trong Makefile như
decoder oracle của testbench.

## Chạy bus CIF

```bash
make clean
make -j2
mkdir -p input output

./build/vpu_tlm3.0 \
  -i input/bus_cif.yuv -w 352 -h 288 -n 150 \
  --mode intra-directional-tq --qp 26 \
  --recon output/bus_cif_directional_recon.yuv \
  -o output/bus_cif_directional_cli.h265

./build/vpu_mmio_demo \
  -i input/bus_cif.yuv -w 352 -h 288 -n 150 \
  --mode intra-directional-tq --qp 26 \
  -o output/bus_cif_directional_mmio.h265
```

## Ranh giới còn lại

M3a là mốc functional all-intra, chưa phải encoder production. Chưa có planar,
31 angular mode còn lại, rate term trong RDO, deblocking/SAO, P/B frame,
motion estimation, GOP, VBV/rate control hoặc mô hình cycle-accurate. Các mục
này tiếp tục ở M3b–M7 trong `architecture.md`.
