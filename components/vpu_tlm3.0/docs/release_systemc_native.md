# SystemC-native TLM pipeline release

## Mục tiêu

Phiên bản này bổ sung một implementation SystemC-native độc lập với lớp
`FifoCycleModel` C++ trước đó. Lõi HEVC bit-true vẫn được tái sử dụng để giữ
bitstream đã kiểm chứng, còn control/data movement và timing được ánh xạ sang
các primitive SystemC/TLM-2.0 thật.

## Kiến trúc

```text
CPU --b_transport--> VpuController --sc_fifo<JobConfig>--> InputDmaStage
                                                            |
                                                     sc_fifo<FramePacket>
                                                            v
                                                     PredictionStage
                                                            |
                                                     sc_fifo<FramePacket>
                                                            v
                                                      TransformStage
                                                            |
                                                     sc_fifo<FramePacket>
                                                            v
                                                        CabacStage
                                                            |
                                                   sc_fifo<BitstreamPacket>
                                                            v
                                                      OutputDmaStage
                                                            |
                                                    TLM DMA bridge/bursts
                                                            v
                                                       system memory
```

`VpuController` phát IRQ mức khi nhận `CompletionPacket`. CPU hạ IRQ bằng
register W1C `IRQ_STATUS`. Input/output DMA dùng một `DmaTransportIf` được
`TlmDmaBridge` hiện thực bằng `tlm_generic_payload` và `b_transport`.

## SystemC primitives thật

- Mỗi stage là một `sc_module` có `SC_THREAD` riêng.
- Bốn data FIFO là `sc_fifo` hữu hạn, có producer/consumer riêng.
- Stage dùng `nb_write()` và chờ cạnh clock khi FIFO đầy; stall counter tăng
  theo từng cycle chờ.
- Clock và active counter tách khỏi simulation delay của target memory.
- MMIO target trả response status và annotated delay; target không gọi
  `wait()` bên trong `b_transport`.
- DMA bridge chia transfer thành burst, gọi initiator socket và chờ delay do
  memory/interconnect trả về.

## Granularity và giới hạn

Token pipeline hiện ở granularity một frame. Prediction/transform stage tạo
timing và backpressure trên dữ liệu frame; lõi `HevcPcmEncoder` bit-true vẫn thực
hiện phép toán codec trong CABAC/packer stage. Việc tách toán học prediction,
transform và CABAC thành các kernel CTU độc lập là bước RTL-partition tiếp theo.
Cấu trúc hiện tại là SystemC-native architectural model, không được tuyên bố là
RTL cycle-accurate nếu chưa có trace RTL để hiệu chuẩn.

FIFO depth là tham số phần cứng khi khởi tạo `VpuSystemCNative`, không resize
sau `sc_start()`. Register `FIFO_CONFIG` phải khớp tham số đó. Testbench chọn
độ sâu bằng `--fifo-depth` trước khi tạo top module.

Soft reset khi VPU đang BUSY bị từ chối với `StartWhileBusy`; model không bỏ
âm thầm một DMA transaction đang block.

## Build

Ubuntu/Debian:

```bash
sudo apt update
sudo apt install -y libsystemc-dev
make systemc-native
make verify-systemc-native
```

SystemC cài ở prefix riêng:

```bash
make verify-systemc-native \
  SYSTEMC_HOME=/path/to/systemc \
  SYSTEMC_LIBDIR=/path/to/systemc/lib-linux64
```

Nếu distro đặt library ở đường dẫn khác, truyền trực tiếp:

```bash
make verify-systemc-native \
  SYSTEMC_CPPFLAGS='-DSC_DISABLE_API_VERSION_CHECK -I/path/include' \
  SYSTEMC_LDFLAGS='-L/path/lib -Wl,-rpath,/path/lib'
```

## Tiêu chí regression

`make verify-systemc-native` chạy directional QP26 tám frame với FIFO depth 1
và 4, sau đó kiểm tra:

1. depth 1 phải tạo ít nhất một FIFO stall;
2. bitstream depth 1 bằng bitstream CLI;
3. bitstream depth 4 bằng bitstream CLI;
4. FFprobe nhận HEVC 96x66 yuv420p;
5. FFmpeg decoded YUV bằng reconstruction nội bộ từng byte.

SystemC 3.0.2 đã được dùng để compile và chạy regression của release này.
