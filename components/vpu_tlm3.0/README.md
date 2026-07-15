# vpu_tlm3.0 — TLM HEVC/H.265 raw-YUV encoder

Đây là mô hình tham chiếu tối thiểu cho một VPU nhận **raw YUV420p 8-bit** và tự
sinh **HEVC/H.265 Annex-B elementary bitstream** (`.h265`). Đường encode không
gọi FFmpeg, x265 hay thư viện codec bên ngoài.

Phiên bản hiện tại có **PCM lossless**, các mốc nén all-intra đến M3a và lớp
timing M7a FIFO cycle-stepped, kèm control plane kiểu SoC qua MMIO/DMA/IRQ. Mỗi CTU 32×32 nằm trong một independent
slice. Bitstream có VPS, SPS, PPS, IDR slice, CABAC, emulation prevention và
conformance-window cropping. Cách này chứng minh trọn đường:

```text
raw YUV420p -> TLM transaction -> HEVC syntax/RBSP -> NAL/EBSP -> Annex B .h265
```

Nó chưa có hiệu quả/chất lượng như x265/HM. M3a đã mã hóa đầy đủ hệ số DC/AC,
split transform-tree, chọn TU 32/16, rồi chọn luma DC/horizontal/vertical theo
distortion từng CTU. Planar, 31 hướng angular còn lại, quadtree CU, rate control
và inter prediction nằm trong lộ trình tiếp theo.

## Chế độ mã hóa

| Mode | Ý nghĩa | Mục đích |
|---|---|---|
| `pcm` | HEVC PCM, lossless, gần kích thước raw | Golden bring-up, byte-exact |
| `intra-dc` | DC prediction, không residual | Kiểm thử I-slice/CABAC, ảnh ra gần như 128 |
| `hybrid-dc` | Chọn DC hoặc PCM theo sai số trung bình và QP | M1 input-dependent, trade-off đơn giản |
| `intra-dc-tq` | DC prediction + integer transform + quant/dequant + CABAC coefficient DC | M2 nén thật, giữ trung bình từng CTU nhưng chưa giữ chi tiết AC |
| `intra-full-tq` | DC prediction + full integer transform + quant/dequant + grouped diagonal scan/CABAC DC+AC | M2.1 giữ cạnh và chi tiết tốt hơn; luma TU 32×32, chroma TU 16×16 |
| `intra-full-tq16` | Split transform-tree cố định; bốn TU luma 16×16 và chroma 8×8 | Regression cú pháp split/CBF và prediction tuần tự |
| `intra-adaptive-tq` | Thử TU32 và TU16, chọn reconstruction có SSE thấp hơn cho từng CTU | M2.2 baseline có bitrate thấp hơn M3a trên bus CIF |
| `intra-directional-tq` | Thử DC TU32, DC TU16, horizontal TU16 và vertical TU16; chọn SSE thấp nhất | M3a, prediction theo hướng và mode khuyến nghị để đánh giá chất lượng |

Mode mặc định vẫn là `pcm` để giữ tương thích regression cũ.

## Build và chạy

Yêu cầu mặc định: GNU Make và compiler C++20.

```bash
make
./build/yuvgen input.yuv 96 66 2
./build/vpu_tlm3.0 \
  --input input.yuv --width 96 --height 66 --frames 2 \
  --output output.h265
```

Chạy M2.2 adaptive TU và xuất reconstruction nội bộ:

```bash
./build/vpu_tlm3.0 \
  -i input/bus_cif.yuv -w 352 -h 288 -n 150 \
  --mode intra-adaptive-tq --qp 26 \
  --recon output/bus_cif_adaptive_recon.yuv \
  -o output/bus_cif_adaptive.h265
```

Chạy M3a directional:

```bash
./build/vpu_tlm3.0 \
  -i input/bus_cif.yuv -w 352 -h 288 -n 150 \
  --mode intra-directional-tq --qp 26 \
  --recon output/bus_cif_directional_recon.yuv \
  -o output/bus_cif_directional.h265
```

`--frames 0` hoặc bỏ `--frames` sẽ encode đến cuối file. Width và height phải
là số chẵn vì input là YUV420p planar theo thứ tự Y, Cb, Cr.

### Chạy qua MMIO/DMA/FIFO như phần mềm SoC

`vpu_mmio_demo` không truyền frame trực tiếp vào encoder. Nó đặt raw YUV trong
RAM mô phỏng, lập trình thanh ghi địa chỉ nguồn/đích, kích `START`, nhận IRQ rồi
lấy đúng `BITSTREAM_BYTES` byte từ buffer đích:

```bash
mkdir -p input output
ffmpeg -v error -i ~/bus_cif.y4m -f rawvideo -pix_fmt yuv420p \
  -y input/bus_cif.yuv
./build/vpu_mmio_demo \
  -i input/bus_cif.yuv -w 352 -h 288 -n 150 \
  --mode intra-adaptive-tq --qp 26 \
  --fifo-depth 4 \
  -o output/bus_cif_adaptive_mmio.h265
ffmpeg -v error -i output/bus_cif_adaptive_mmio.h265 -f rawvideo \
  -pix_fmt yuv420p -y output/bus_cif_decoded.yuv
```

Với mode `pcm`, decoded phải khớp input. Với mode lossy, decoded phải khớp file
`--recon` sinh bởi functional model.

Register map đầy đủ nằm ở [docs/register_map.md](docs/register_map.md).

Đường MMIO dùng bốn FIFO packet hữu hạn giữa DMA input, prediction, transform,
CABAC và DMA output. `tick()` tiến đúng một cycle kiến trúc; FIFO đầy tạo
backpressure và tăng stall counter. `step()` vẫn được giữ để chạy đến hết một
frame cho phần mềm cũ. `--fifo-depth` đặt cùng độ sâu 1..255 cho bốn FIFO.
Bitstream không phụ thuộc cấu hình timing.

## Kiểm thử

```bash
make verify
make verify-mmio
make verify-m2
make verify-m21
make verify-m22
make verify-m3
make verify-m7a
```

Các target thực hiện unit test, tạo YUV 96×66 hai frame, encode, dùng FFprobe đọc
header và FFmpeg **chỉ ở phía decoder/testbench**. PCM so decoded với input;
mode lossy so decoded với reconstruction nội bộ. Cả hai phải khớp từng byte.

Kiểm thử thủ công:

```bash
ffprobe -v error -show_streams output.h265
ffmpeg -v error -i output.h265 -pix_fmt yuv420p -f rawvideo decoded.yuv
cmp reconstruction.yuv decoded.yuv
```

## SystemC/TLM-2.0

Lõi codec không phụ thuộc SystemC. Adapter chuẩn TLM-2.0 nằm tại
`optional_systemc/vpu_tlm_mmio.hpp`. Nó cung cấp ba cổng:

```text
CPU/interconnect --b_transport(32-bit MMIO)--> VpuTlmMmio
VpuTlmMmio     --b_transport(DMA read/write)--> system memory
VpuTlmMmio     -----------------------------IRQ--> CPU/interrupt controller
```

`optional_systemc/sc_main.cpp` là testbench CPU + RAM dùng chính ba cổng đó,
không phải đường encode file riêng.

Khi máy đã cài SystemC:

```bash
make systemc SYSTEMC_HOME=/path/to/systemc
./build/vpu_tlm3.0_systemc -i input.yuv -w 96 -h 66 -n 2 -o output.h265
```

Ngoài adapter tương thích cũ, implementation SystemC-native mới nằm tại
`optional_systemc/vpu_systemc_native.{hpp,cpp}`. Nó tách controller, input DMA,
prediction, transform, CABAC và output DMA thành các `sc_module`; các stage nối
bằng `sc_fifo` hữu hạn và DMA được chia thành TLM burst.

```bash
sudo apt install libsystemc-dev
make systemc-native
make verify-systemc-native
```

Chạy trực tiếp model native:

```bash
./build/vpu_tlm3.0_systemc_native \
  -i input/bus_cif.yuv -w 352 -h 288 -n 150 \
  --mode intra-directional-tq --qp 26 \
  --fifo-depth 4 --dma-burst 64 \
  -o output/bus_cif_systemc_native.h265
```

FIFO depth của implementation native là tham số construction-time vì
`sc_fifo` không resize sau elaboration. Testbench tạo top module với giá trị
`--fifo-depth`, sau đó driver ghi đúng giá trị đó vào `FIFO_CONFIG`.

Regression đã được compile/chạy bằng SystemC 3.0.2. Xem
[release SystemC-native](docs/release_systemc_native.md) và
[hướng dẫn ghép CDC-VP](docs/cdc_vp_integration.md).

## Giới hạn hiện tại

- Chỉ YUV420p 8-bit, kích thước chẵn.
- Tất cả frame là IDR/all-intra.
- CTU 32×32, một independent slice trên mỗi CTU.
- Không VUI timing; elementary stream thường được tool phát ở timing mặc định.
- Main profile được báo hiệu; Level 6.2 được dùng để tránh ràng buộc kích thước
  ở giai đoạn mô hình chức năng.
- Mode PCM vẫn không nén và bitstream lớn hơn raw một ít.
- `intra-dc-tq`, `intra-full-tq` và `intra-full-tq16` được giữ làm regression.
  `intra-adaptive-tq` chọn giữa luma TU 32×32 và bốn TU 16×16. Mode
  `intra-directional-tq` thêm DC/horizontal/vertical cho luma TU16; chroma vẫn
  DC. Mỗi CTU vẫn là independent slice nên reference không đi qua biên CTU;
  chưa có đủ planar/33 angular mode và chưa phù hợp sản phẩm.
- MMIO M7a là cycle-stepped và deterministic theo throughput/FIFO đã cấu hình,
  nhưng latency chưa được hiệu chuẩn với RTL hoặc silicon. Không dùng số cycle
  để cam kết hiệu năng phần cứng trước khi có trace RTL làm oracle.
- SystemC-native dùng `sc_module`, `sc_fifo`, clock/reset, TLM MMIO target và
  burst DMA initiator thật. Token hiện ở granularity frame; lõi codec bit-true
  vẫn tập trung trong encoder functional để giữ bitstream regression.

Xem [kiến trúc và lộ trình](docs/architecture.md) và
[kế hoạch all-intra](docs/all_intra_plan.md) để phát triển thành encoder có nén
thật.

## Đưa vào CDC-VP

Component này được đặt tên cố định là `vpu_tlm3.0`. Chép nguyên thư mục vào cây
dự án rồi build tại chỗ:

```bash
mkdir -p ~/CDC-VP/components
cp -a vpu_tlm3.0 ~/CDC-VP/components/
cd ~/CDC-VP/components/vpu_tlm3.0
make verify
```

Nếu CDC-VP có Makefile/manifest cấp cha để liệt kê component, cần thêm
`components/vpu_tlm3.0` theo đúng cơ chế đăng ký của repo đó. Không thể tự thêm
dòng này khi chưa có source tree CDC-VP để đối chiếu.

## Nguồn chuẩn

- [ITU-T H.265 (V11, 01/2026)](https://www.itu.int/rec/T-REC-H.265-202601-I)
- [Accellera TLM-2.0 Language Reference Manual](https://www.accellera.org/images/downloads/standards/systemc/TLM_2_0_LRM.pdf)
- [HM HEVC reference software](https://vcgit.hhi.fraunhofer.de/jvet/HM/-/tree/HM-16.25)

## License

MIT — xem `LICENSE`.
