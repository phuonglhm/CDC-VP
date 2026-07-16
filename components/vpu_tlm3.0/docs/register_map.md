# vpu_tlm3.0 register map

All registers are 32-bit little-endian. `tick()` advances one FIFO cycle;
legacy `step()` advances ticks until one frame completes.

Implementation SystemC-native có hardware metric dashboard báo
`VERSION=0x0003000A`. Với implementation
này, `FIFO_CONFIG` phản ánh độ sâu `sc_fifo` được chọn khi elaboration và giá
trị driver ghi phải khớp phần cứng; FIFO không thể resize sau `sc_start()`.

| Offset | Name | Access | Description |
|---:|---|---|---|
| `0x000` | ID | RO | `0x56505533` (`VPU3`) |
| `0x004` | VERSION | RO | `0x00030008` |
| `0x008` | CONTROL | RW/command | bit0 START, bit1 SOFT_RESET, bit2 IRQ_ENABLE |
| `0x00c` | STATUS | RO | bit0 BUSY, bit1 DONE, bit2 ERROR, bit3 IRQ |
| `0x010/14` | SRC_ADDR | RW | 64-bit raw YUV DMA source |
| `0x018/1c` | DST_ADDR | RW | 64-bit Annex-B bitstream destination |
| `0x020` | DST_CAPACITY | RW | Output buffer capacity in bytes |
| `0x024` | WIDTH | RW | Even luma width |
| `0x028` | HEIGHT | RW | Even luma height |
| `0x02c` | STRIDE_Y | RW | Luma stride; 0 means WIDTH, chroma stride is half |
| `0x030` | FRAME_COUNT | RW | Number of input frames |
| `0x034` | QP | RW | 0..51; PCM MVP does not quantize samples |
| `0x038` | INPUT_FORMAT | RW | 0 = planar YUV420p 8-bit |
| `0x03c` | BITSTREAM_BYTES | RO | Bytes written to DST_ADDR |
| `0x040` | FRAMES_DONE | RO | Completed frames |
| `0x044` | ERROR_CODE | RO | See `VpuError` |
| `0x048` | IRQ_STATUS | RO/W1C | bit0 DONE, bit1 ERROR |
| `0x04c/50` | CYCLES | RO | 64-bit modeled cycle count |
| `0x054` | ENCODER_MODE | RW | 0 PCM, 1 intra-DC, 2 hybrid-DC, 3 DC-only TQ, 4 full TQ32, 5 split TQ16, 6 adaptive TQ32/TQ16, 7 directional DC/H/V |
| `0x058` | FIFO_CONFIG | RW | `[7:0]` input, `[15:8]` residual, `[23:16]` coefficient, `[31:24]` output FIFO depth; reset `0x04040404` |
| `0x05c` | STALL_INPUT_FULL | RO | Cycle DMA reader giữ CTU vì input FIFO đầy |
| `0x060` | STALL_PREDICTION_FULL | RO | Cycle prediction giữ kết quả vì residual FIFO đầy |
| `0x064` | STALL_TRANSFORM_FULL | RO | Cycle transform giữ kết quả vì coefficient FIFO đầy |
| `0x068` | STALL_CABAC_FULL | RO | Cycle CABAC giữ packet vì output FIFO đầy |
| `0x06c` | FIFO_MAX_OCCUPANCY | RO | Peak occupancy đóng gói theo cùng thứ tự byte như FIFO_CONFIG |
| `0x070` | DMA_READ_ACTIVE | RO | DMA reader busy cycle, gồm cycle bị backpressure |
| `0x074` | PREDICTION_ACTIVE | RO | Prediction busy cycle, gồm cycle bị backpressure |
| `0x078` | TRANSFORM_ACTIVE | RO | Transform/quant busy cycle, gồm cycle bị backpressure |
| `0x07c` | CABAC_ACTIVE | RO | CABAC/packer busy cycle, gồm cycle bị backpressure |
| `0x080` | DMA_WRITE_ACTIVE | RO | DMA writer active cycle |

## Driver sequence

1. Allocate and fill a contiguous planar YUV420p input buffer.
2. Allocate an output buffer and program its capacity.
3. Program source/destination, dimensions, stride, frames, QP, format, mode and FIFO_CONFIG.
4. Write `IRQ_ENABLE | START` to CONTROL.
5. Run/schedule the model while STATUS.BUSY is set.
6. Wait for IRQ or poll DONE/ERROR.
7. Read BITSTREAM_BYTES and consume that many bytes from DST_ADDR.
8. Clear IRQ by writing the observed bits to IRQ_STATUS.

Performance counters ổn định khi `DONE=1`. Nhiều stage có thể stall trong cùng
một cycle, nên tổng bốn stall counter có thể lớn hơn `CYCLES`.

Dashboard còn đọc các counter nội bộ 64-bit cho TLM burst/byte, SRAM response
delay, frame token và timestamp pipeline. Các counter mở rộng này thuộc
testbench instrumentation, chưa chiếm thêm offset trong register space 0x100;
software SoC vẫn dùng các register chuẩn trong bảng trên.
