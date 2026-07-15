# M7a FIFO cycle-stepped release

## Kiến trúc

```text
DMA reader -> input FIFO -> prediction -> residual FIFO
           -> transform/quant -> coefficient FIFO -> CABAC
           -> output FIFO -> DMA writer
```

Mỗi FIFO có depth hữu hạn. Mỗi lần `VpuMmioDevice::tick()` chỉ tiến một cycle.
Stage hoàn tất nhưng FIFO sau đầy phải giữ token, không được ghi đè, đồng thời
tăng stall counter. `step()` gọi nhiều tick đến khi xong một frame để giữ API
cũ; `run_to_completion()` gọi tick đến DONE/ERROR.

Throughput mặc định:

| Khối | Cấu hình |
|---|---:|
| DMA read | 16 byte/cycle |
| Prediction | 16 sample/cycle |
| Transform/quant | 8 sample/cycle |
| CABAC/packer | 2 byte/cycle |
| DMA write | 8 byte/cycle |
| Mỗi FIFO | 4 packet |

Directional mode tính bốn prediction/transform candidate; adaptive mode tính
hai candidate. Các hằng số là tham số kiến trúc, chưa phải số đo RTL.

## Bus CIF benchmark

Vector: YUV420p 8-bit, 352×288, 150 frame, directional QP26, FIFO depth 4.

| Counter | Giá trị |
|---|---:|
| Total cycles | 11,475,799 |
| DMA read busy | 10,305,900 |
| Prediction busy | 10,867,350 |
| Transform busy | 11,404,800 |
| CABAC busy | 1,613,368 |
| DMA write active | 408,902 |
| Input-full stall | 8,880,300 |
| Prediction-full stall | 7,065,750 |
| Transform-full stall | 0 |
| CABAC-full stall | 0 |
| Peak input/residual/coefficient/output | 4 / 4 / 1 / 1 |

Transform/quant là bottleneck trong cấu hình này. Tổng stall có thể lớn hơn
total cycle vì nhiều stage được phép stall đồng thời.

Bitstream M7a MMIO vẫn có 3,219,490 byte và SHA-256
`1c1d7977e99308a64e0d8230c1e4e2530d3be51296c9d91aba1bfc542160d7f4`,
bit-exact với M3a CLI. Reconstruction vẫn byte-exact với decoder ngoài.

## Mức chính xác

M7a là cycle-accurate đối với state machine/FIFO/throughput được mô tả trong
source: cùng input và cấu hình luôn cho cùng cycle, occupancy và stall trace.
Nó chưa được hiệu chuẩn với một RTL cụ thể, chưa mô hình bus arbitration,
outstanding transaction hoặc SRAM bank conflict. Vì vậy gọi đúng nhất là
`cycle-stepped architectural model`; chỉ nâng lên RTL-correlated cycle-accurate
sau khi M7b so trace từng CTU với RTL.
