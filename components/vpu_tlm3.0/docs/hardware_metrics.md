# Hardware metric dashboard

## Mục đích

Executable `vpu_tlm3.0_systemc_native` in báo cáo metric phần cứng sau mỗi job
encode thành công. Báo cáo dùng bốn nhãn nguồn để không trộn số đo mô phỏng với
ước lượng triển khai:

| Nhãn | Ý nghĩa |
|---|---|
| `[M]` | Counter hoặc timestamp được ghi khi SystemC/TLM thực thi |
| `[D]` | Giá trị suy ra từ counter và clock cấu hình |
| `[S]` | Tham số kiến trúc/testbench do người dùng cấu hình |
| `[A]` | Cần công cụ hoặc mô hình bên ngoài, ví dụ synthesis/power |

Area, power và energy không được tự ước lượng từ số cycle. Chúng được in `N/A`
cho tới khi có netlist/synthesis report hoặc mô hình đã hiệu chỉnh với phần
cứng đích.

## Các metric hiện có

- Total cycle, latency theo ms, frame/s, Mpixel/s và cycle/frame.
- Active cycle, frame count, cycle/frame, first-done latency và utilization cho
  input DMA, prediction, transform/TQ, CABAC/packer, output DMA.
- Pipeline fill và drain.
- Peak occupancy, FIFO depth và producer-stall của bốn `sc_fifo`.
- TLM DMA payload byte, số burst, SRAM response delay và bandwidth hiệu dụng.
- Kích thước raw/bitstream, compression ratio và bits/pixel.
- Trạng thái giới hạn của area, power, PE array và RTL correlation.

Tổng stall của nhiều stage có thể lớn hơn total cycle vì các stage có thể stall
đồng thời. Utilization từng stage bằng `active_cycle / total_cycle`; average
pipeline utilization bằng tổng active cycle chia cho `5 * total_cycle`.

## Chạy bus CIF

```bash
make systemc-native

./build/vpu_tlm3.0_systemc_native \
  -i input/bus_cif.yuv -w 352 -h 288 -n 150 \
  --mode intra-directional-tq --qp 26 \
  --fifo-depth 4 --dma-burst 64 \
  --clock-mhz 800 \
  --sram-read-latency 1 --sram-write-latency 1 \
  --sram-bytes-per-cycle 16 \
  -o output/bus_cif_systemc_metrics.h265
```

Dashboard được bật mặc định. Dùng `--no-hw-report` nếu chỉ cần dòng summary;
`--hw-report` bật lại báo cáo. Có thể lưu cả báo cáo terminal:

```bash
./build/vpu_tlm3.0_systemc_native [các tùy chọn] \
  -o output/video.h265 | tee output/hardware_metrics.txt
```

## Ý nghĩa SRAM trong testbench

`TlmMemory` là target RAM phẳng mô phỏng SRAM nối với DMA initiator. Mỗi TLM
burst chịu:

```text
access_cycles = configured_read_or_write_latency
                + ceil(payload_bytes / sram_bytes_per_cycle)
```

Độ trễ này dùng chính clock period của VPU, nên thay `--clock-mhz` không làm thay
đổi số cycle kiến trúc; nó chỉ thay đổi thời gian và throughput suy ra. Model
hiện chưa mô phỏng SRAM bank conflict, arbitration, outstanding request hoặc
cache.

## Mức chính xác

Các counter là chính xác đối với state machine, FIFO, throughput và TLM latency
đang mô tả trong source. Đây là `cycle-stepped architectural model`, chưa phải
con số cycle của một RTL cụ thể. Muốn gắn nhãn RTL-correlated cần so trace theo
CTU/frame với RTL và hiệu chỉnh các hằng throughput/latency.
