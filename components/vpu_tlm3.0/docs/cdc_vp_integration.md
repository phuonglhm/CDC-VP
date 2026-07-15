# Ghép `vpu_tlm3.0` vào CDC-VP

## Cổng của component

`model::systemc_tlm::VpuTlmMmio` trong
`optional_systemc/vpu_tlm_mmio.hpp` có:

| Cổng | Hướng | Vai trò |
|---|---|---|
| `mmio_socket` | target | Thanh ghi 32-bit little-endian, vùng local `0x000..0x0ff` |
| `dma_socket` | initiator | Đọc raw YUV và ghi Annex-B vào system memory |
| `irq` | output | IRQ mức, clear bằng `IRQ_STATUS` W1C |

Interconnect phải decode một base address riêng cho VPU và chuyển địa chỉ local
vào `mmio_socket`. Nếu interconnect chuyển nguyên địa chỉ vật lý, cần thêm offset
adapter vì target cố ý chỉ nhận `0x000..0x0ff`.

Ví dụ wiring tối thiểu:

```cpp
model::systemc_tlm::VpuTlmMmio vpu{"vpu"};
cpu.mmio_initiator.bind(vpu.mmio_socket);
vpu.dma_socket.bind(system_bus.target_socket);
vpu.irq(vpu_irq_signal);
```

Tên socket của CPU/bus thực tế có thể khác. Không nên sửa lõi codec để khớp bus;
hãy dùng adapter/range decoder của CDC-VP.

## Ghép implementation SystemC-native

Implementation native có clock/reset, `sc_fifo` thật và DMA burst:

```cpp
model::systemc_native::VpuSystemCNative vpu{
    "vpu", 4, sc_core::sc_time(1, sc_core::SC_NS), 64};

cpu.mmio_initiator.bind(vpu.mmio_socket());
vpu.dma_socket().bind(system_bus.target_socket);
vpu.clk(vpu_clock);
vpu.reset_n(vpu_reset_n);
vpu.irq(vpu_irq_signal);
```

Tham số `4` là độ sâu phần cứng của bốn data FIFO; `64` là kích thước DMA
burst tối đa. Driver phải ghi `FIFO_CONFIG=0x04040404`. Nếu platform dùng base
address toàn cục, interconnect vẫn phải dịch về offset local `0x000..0x0ff`.

Model native dùng blocking TLM ở biên bus và pipeline clocked ở bên trong. Đây
là lựa chọn hybrid phù hợp virtual platform; nó không yêu cầu bus CDC-VP phải
dùng non-blocking four-phase transport.

## Trình tự driver

1. Cấp phát input/output buffer trong vùng mà DMA initiator nhìn thấy.
2. Ghi `SRC_ADDR`, `DST_ADDR`, `DST_CAPACITY`, `WIDTH`, `HEIGHT`, `STRIDE_Y`,
   `FRAME_COUNT`, `QP`, `INPUT_FORMAT`, `ENCODER_MODE`, `FIFO_CONFIG`.
3. Ghi `CONTROL.IRQ_ENABLE | CONTROL.START`.
4. Chờ IRQ hoặc poll `STATUS`; không sửa cấu hình khi `BUSY=1`.
5. Nếu `DONE=1`, đọc `BITSTREAM_BYTES`; nếu `ERROR=1`, đọc `ERROR_CODE`.
6. Ghi các bit đã xử lý vào `IRQ_STATUS` để hạ IRQ.
7. Sau DONE, đọc stall/active/max-occupancy counter nếu cần phân tích hiệu năng.

## Việc cần đối chiếu trong source tree CDC-VP

- Base address chưa trùng component khác.
- Kiểu/socket và policy của interconnect (blocking hay non-blocking transport).
- Quy tắc address translation và bus width/alignment.
- Interrupt controller nhận level hay pulse.
- Makefile/CMake/manifest cấp cha và đường dẫn thư viện SystemC.
- Quy ước cycle/time của platform để đổi latency `SC_NS` trong adapter.
- Trace latency từng khối và bus arbitration của RTL để hiệu chuẩn M7a.

Các mục này không thể chốt chỉ từ thư mục component; cần cây source CDC-VP hoặc
ít nhất các file top-level, interconnect, component manifest và build system.
