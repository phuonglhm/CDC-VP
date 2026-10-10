# H.264 DMA functional VP

Spec DMA ngắn gọn: [DMA_SUBSYSTEM_SPEC.md](DMA_SUBSYSTEM_SPEC.md).

Mô hình C++17 của CMB, SW, DF/REFM, NAL, ordered arbiter và AXI memory bridge,
tham chiếu [SISLAB HAS v2.1.11](../SISLAB_H264_AVC_HAS_Spec_Detailed_v2.1.11.md), §3, §5–6, §11.4, §12 và Appendix B. Core không cần SystemC kernel.

Wrapper SystemC/TLM tích hợp MMIO, shared DDR, IRQ/reset và completion vào CDC-VP. Hướng dẫn SystemC/TLM và API tích hợp nằm trong README này; kết quả test trong [VALIDATION.md](VALIDATION.md).

## Build và test core C++

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j 2
ctest --test-dir build --output-on-failure
```

Runner chứa cả test ban đầu và regression theo review, tổng **272 assertion**.
Kết quả đã chạy trên Ubuntu WSL/GCC 15.2.0: tất cả pass; ASan/UBSan cũng pass.
Baseline core 267 assertions đã compile sạch trên Windows MSYS2 GCC 16.1.0
với `-Wall -Wextra -Wpedantic -Werror`; build hiện tại xác minh bằng WSL.
Windows Smart App Control chặn khởi chạy runner mở rộng trong phiên này, nên
kết quả runtime cuối được lấy từ WSL, không tuyên bố Windows runtime đã pass.

Sanitizer trên GCC/Linux:

```sh
cmake -S . -B build-sanitized -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer"
cmake --build build-sanitized -j 2
ctest --test-dir build-sanitized --output-on-failure
```

## Contract giao dịch

- `DmaRequest.beats` là số **internal transfers**, mỗi transfer `size` byte.
  Tổng payload là `beats * size`, độc lập AXI data width.
- `DmaRequest.data` chứa write payload; arbiter copy metadata/payload/sink
  theo request. Read completion có addressed bytes trong `DmaResponse.data`.
- Bridge căn external address xuống bus beat, chọn lane và WSTRB, chia burst
  tại 4 KiB và burst limit. Planner không làm thay đổi accepted-address log.
- Mỗi write segment nhận một B response. Chỉ byte có strobe được thay đổi.
  Memory được commit theo segment sau B OKAY trong abstraction của VP; đây
  không phải bảo đảm rollback của AXI hardware khi một burst sau bị lỗi.
- `service_one()` xử lý một transaction ở cấu hình zero-delay. Nếu inject
  delay, call trả owner và giữ grant pending; chỉ NONE mới có nghĩa idle.
  `run()` tiếp tục qua stalls và chỉ throw nếu còn work khi hết bound.
- Các delay trong `AxiBridgeConfig` là số service steps pending cho từng
  channel/beat/consumer. Một service step không phải một fabric clock RTL.
  Read FIFO có tối đa 16 internal words, mỗi entry tối đa 4 byte.
- `error()` của subsystem giữ lỗi DMA đến khi idle và host gọi `clear_error()`.

## Client APIs

**CMB:** `update_enable(true)` nhận một frame start; giữ high không tạo start
lần hai. Hạ enable mới rearm. `fetch_macroblock()` chỉ cho một tile pending;
`pixels()` trả planar Y16×16/U8×8/V8×8 khi tất cả rows đã thành công.
Mode `WorkingSet` dùng `select_working_frame(slot)` trước enable; controller
cung cấp storage slot theo coding order. Không tự suy ra GOP mapping từ HAS.
`RegisterBases.cmb_frames` và source capacity giới hạn allocation này.

**SW:** `set_ref_slot(list, slot, picture_tag)` retag riêng từng list.
`fill_window()` chỉ ready sau completion toàn vùng Y/U/V; overlap đã nhận
thành công được dùng lại khi chuyển MB/hàng. Reference slot chỉ nên được
chọn sau producer DF xác nhận frame valid. Chính sách geometry của VP là
window bắt đầu tại tọa độ MB và clip vào coded frame; `SwPlane` công bố
origin/extent thực. Codec xử lý padding biên/search origin cụ thể bên ngoài.
`invalidate()` làm mất residency và selection; cần chọn slot lại.

**DF:** producer phải truyền `MacroblockPixels`:

```cpp
vp.df().latch_df_enable(false);  // DFCON.ENABLE=0: bypass
vp.df().set_ref_slot(1);
vp.df().on_sofm();
vp.df().schedule_macroblock(mb_x, mb_y, reconstructed, false);
vp.df().dma_fmdone();           // producer đã issue toàn bộ work của frame
vp.df().consume_done();
vp.run();
// Chỉ dùng reference khi vp.df().frame_complete() là true.
```

Ở filter mode, truyền tile đã lọc (`already_filtered=true`) hoặc đăng ký
`set_filter(callback)` và truyền tile chưa lọc. Callback chạy vertical trước
horizontal, hoàn tất cả hai rồi DMA mới export Y/U/V. Gọi schedule không có
pixel input hoặc chưa lọc mà không có producer sẽ bị từ chối.
`dma_fmdone(frame_id)` hỗ trợ done đến trễ sau SOF mới; pending writes và
retained done được giữ theo frame. `frame_complete(old_id)` kiểm frame cũ,
không làm frame mới complete. `latch_dis_idc(0/1)` chỉ là tên API cũ cho
DFCON.ENABLE, không phải AVC disable_deblocking_filter_idc.

**NAL:** accepted words được snapshot riêng tại mỗi flush; flush nhiều lần
append trong cùng activation. Pointer/count chỉ commit theo response thành
công; failure giữ error và không tạo final done. `on_disable()` yêu cầu
không còn pending output, reload pointer nhưng giữ STM_LEN để host drain.
Activation tiếp theo phải qua disable rồi `begin_activation()`. Capacity
được kiểm tra khi accept, trước enqueue. Byte order là cấu hình explicit
`WordByteOrder`; default little-endian giữ convention cũ. `accept_eos()`
phát bytes `00 00 01 0B` theo cả hai convention. Endian của RTL vẫn cần audit.
`commit_payload()` cũ là compatibility no-op; chỉ bridge ghi DDR.

## Memory và giới hạn

Constructor validate coded dims, natural alignment, region capacity và overlap.
REFM cần 3F; default NAL base là `0x01000000`, vượt vùng REFM full HD.
`nal_capacity=0` được suy ra đến region kế tiếp hoặc cuối modeled DDR.

Constructor nhận `MemoryIf&`, physical `memory_base` và `memory_bytes` dùng
memory của platform; ownership giữ ở platform. `memory()` trả shared interface,
`ddr()` chỉ dùng với constructor có private DDR và throw khi memory là external.

Đây là DMA functional model; wrapper bổ sung MMIO/STAT/IRQ/reset ở mức TLM.
Deblocking arithmetic, codec scheduling, golden H.264 decode và FPGA signoff
thuộc producer/RTL harness. Priority, exact search geometry, RTL endian và
pin/cycle timing không được suy đoán đã qualification từ test VP.

## Build và test SystemC/TLM

Trong `components/h264/dma_subsystems`:

```sh
cmake -S . -B build-tlm -DH264_BUILD_TLM=ON \
  -DSYSTEMC_HOME=/opt/systemc-2.3.4 -DCMAKE_BUILD_TYPE=Debug
cmake --build build-tlm -j2
ctest --test-dir build-tlm --output-on-failure
```

Từ thư mục `CDC-VP`, dùng các targets H.264 đã đăng ký:

```sh
cmake -S . -B build-h264 \
  -DCDC_BUILD_H264_DMA=ON -DCDC_BUILD_TESTS=ON \
  -DCDC_BUILD_MINI_TLM=OFF -DCDC_BUILD_CPU_EVAL=OFF \
  -DCDC_BUILD_CUSTOM_SOC=OFF -DCDC_BUILD_NOC_SOC=OFF
cmake --build build-h264 --target h264_dma_platform h264_dma_tests -j2
ctest --test-dir build-h264 -R h264_dma_ --output-on-failure
```

`CDC_BUILD_H264_DMA` mặc định OFF. Component export là
`cdc::components::h264_dma_tlm`; core là `cdc::components::h264_dma`.
Root configuration có các component/test khác; chỉ các targets/test H.264
trong lệnh trên được build/chạy trong evidence này.

## Evidence

Kết quả core/SystemC, môi trường chạy và raw logs được gom trong
[VALIDATION.md](VALIDATION.md). Log validation được version trong các folder
`evidence`; build, binary và log tạm được ignore.

## Kết nối platform

`h264_dma_tlm` cung cấp:

- `target_socket`: MMIO target, offset local sau address decode.
- `master_socket`: DMA initiator, phát địa chỉ vật lý đầy đủ.
- `reset_n`: input reset active low.
- `irq`: output level, nối interrupt controller của platform.

Map target MMIO qua `bus_router.add_target()`. Bind CPU/host và DMA master
vào hai upstream ports của cùng router. Map DDR một lần, chia sẻ cho cả hai.
`H264DmaOptions.memory_base/memory_bytes` phải trùng physical RAM window.
Ví dụ đã chạy ở [platform test](tlm/tests/h264_dma_platform.cpp):

| Vùng | Địa chỉ |
|---|---|
| MMIO test aperture | `0x10080000`, 4 KiB |
| Shared DDR | `0x80000000`, 1 MiB |
| CMB | `0x80001000` |
| REFM | `0x80010000` |
| NAL | `0x80030000`, allocator budget 4 KiB |

Đây là memory map của test platform, không phải địa chỉ cố định từ HAS.
CPU-style host driver nạp pixels và đọc REFM/NAL bằng `b_transport` trên bus;
DMA không giữ bản DDR riêng. Test dùng `bus_router` và `memory_tlm` thật của
CDC-VP, có proxy để inject downstream lỗi. Platform này chạy driver SystemC,
chưa phải test boot ELF trên RISC-V hart.

## MMIO contract

| Offset | Thanh ghi | Hành vi |
|---|---|---|
| `0x00` | SCON | ECE bit 0, GIE bit 1; cạnh ECE 0→1 latch config và start |
| `0x04` | STAT | NORMAL 18, ERROR 17, BUSY 16, FMCNT 15:0; read clear IRQ |
| `0x08` | FMSIZE | Coded width 26:16, height 10:0 |
| `0x0C` | DFCON | Enable bit 0; signed alpha/beta offsets giữ cho producer |
| `0x10` | SPARA0 | QP, CMB mode, syntax/GOP fields; reset `0x12C` |
| `0x14` | SPARA1 | Working-set allocation 31:25, syntax widths, FMENC 15:0 |
| `0x18` | SPARA2 | Crop/delta fields |
| `0x1C` | REFM | Physical base, 3 coded-frame slots |
| `0x20` | NAL | Physical base, explicit/inferred allocator capacity |
| `0x24` | CMB | Physical source base |
| `0x28` | STM_LEN | Committed 32-bit word count; read clear IRQ |

Config writes chỉ được nhận khi ECE=0 và BUSY=0. Giữ ECE=1 không tạo start
thứ hai. Disable sau completion giữ STM_LEN; start mới reset count/pointer.
GIE chỉ gate IRQ, không ngăn work/completion. Đọc STAT hoặc STM_LEN xóa pending
IRQ, giữ NORMAL/ERROR/FMCNT/count. `transport_dbg` chỉ đọc, không clear IRQ,
không cho debug write kích hoạt DMA.

**VP policies cho phần HAS chưa định nghĩa đầy đủ:** MMIO nhận aligned
32-bit transfers, little-endian register bytes, byte enables `00/FF`;
invalid size/alignment trả BURST_ERROR, write RO trả COMMAND_ERROR,
reserved-bit/config-phase violation trả GENERIC_ERROR. Undefined offset
mặc định ADDRESS_ERROR; option `ReadZero` cho undefined reads. Không suy ra
các response này là behavior RTL đã qualification.

Kích thước coded phải dương, multiple 16, tối đa 1920×1088; QP 0..51;
FMENC và working-set frame allocation khác 0; crop không được xóa hết coded
picture. Layout validate physical window, alignment theo DMA width,
capacity và overlap trước giao dịch. Config sai hoàn tất bằng ERROR.

## Codec/workload boundary

Cài `set_workload_provider(callback)` trước activation hoặc enqueue một
`H264ActivationWorkload` khi ECE=0/BUSY=0, sau reset. Provider nhận cấu hình
MMIO đã latch. Queued workloads có ưu tiên; mỗi start tiêu thụ đúng một trace,
kể cả khi config của start đó sai. Reset xóa queued traces, giữ callback/filter
đã cài ở simulation harness.

Workload phải cung cấp đầy đủ macroblocks của từng coded picture, mỗi tọa độ
đúng một lần, explicit source/reference storage slots, List0/List1 selections
và picture tags, reconstructed/filtered pixels, và NAL words. Frame count của
trace không vượt FMENC. CMB mode 1 nhận một picture cho mỗi activation; mode 0
chọn source slot trong working set. FMCNT của wrapper là số pictures đã hoàn
tất trong activation; wrapper không suy ra coding order/GOP schedule hoặc
cumulative frame counter từ các syntax fields.

`H264MacroblockJob.reconstructed` chứa Y256/U64/V64 samples. Nếu DF enabled,
producer cung cấp tile đã lọc hoặc tile chưa lọc cùng `set_filter()` callback;
callback chạy vertical rồi horizontal. DF bypass cũng cần reconstructed
samples. Riêng DMA-only trace có thể chọn explicit
`replay_source_for_bypass=true` để copy CMB pixels sang REFM khi DF disabled.
Đây là memory replay phục vụ kiểm tra DMA.

B-picture bị chặn nếu một trong hai reference lists chưa resident. SW caches
được invalidate giữa các pictures để tránh dùng cache cũ sau REFM writes.
REFM slot validity/coding decisions nằm ở codec producer; wrapper không tự
xác định reference picture nào hợp lệ cho thuật toán H.264.

NAL words append theo các frames trong activation; `eos=true` thêm bytes
`00 00 01 0B`. `H264DmaOptions.nal_word_order` mặc định LittleEndian, hỗ trợ
explicit BigEndian; không khẳng định endian RTL từ HAS. Không có producer
hoặc output hoàn chỉnh thì ERROR, không phát NORMAL giả.

## Scheduling, lỗi và reset

Worker SC_THREAD chạy core, chờ latency trả về bởi DMA `b_transport`, rồi
service delay cấu hình. NORMAL/IRQ chỉ xuất hiện sau đủ successful REFM và
NAL responses. Mỗi transfer có trace address/client/timestamps/response và
reset-cancelled flag. IRQ có một writer process, tránh SystemC multiple-driver
violation giữa MMIO thread và worker.

TLM giao dịch những addressed-byte intervals; không gửi padded writes gây
clobber sang `memory_tlm` vốn không áp byte enables. AXI planning/4-KiB split,
lanes/WSTRB/response ordering vẫn được core kiểm tra. Đây là functional TLM
abstraction; không phải AXI pin waveform hoặc cycle-accurate codec timing.
Default register latency 10 ns và service latency 8 ns là model parameters.

Downstream non-OK TLM response hoặc core response failure → ERROR, không
NORMAL. `max_service_steps` giới hạn stalled core progression trong activation;
không thể preempt một target `b_transport` không bao giờ return. Target/harness
vẫn phải có watchdog cho trường hợp đó.

Hạ ECE khi BUSY chọn **controlled-stop VP policy**: hoàn thành transaction đang
blocking, dừng queued work tiếp theo rồi ERROR; không rollback bytes đã accepted.
Reset active low xóa registers/status/IRQ/count và invalidates completion epoch.
Transaction được downstream nhận trước reset có thể đã tác động DDR; không
rollback. Worker đang wait giữ memory/core sống tới khi return, không cho stale
completion assert IRQ hoặc làm mất start mới sau reset. Test cố tình launch
activation mới trước khi worker cũ hết latency để kiểm tra race này.

## Phạm vi đã hoàn thiện

Có thể dùng component cho functional DMA/driver bring-up trong CDC-VP với
shared DDR, MMIO, IRQ/reset và producer/trace boundary rõ ràng. Test bao gồm
full planar payload, cả bốn DMA clients, DF filter/bypass, multi-picture source
slots, reactivation, GIE/ack, invalid/incomplete/duplicate traces, stale/missing
reference lists, read/write/NAL errors, stop, stalled-channel budget và reset race.

Codec scheduling, FME/MC, transform/entropy/NAL generation và exact deblocking
arithmetic phải do codec model cung cấp. Golden decode/video quality, RISC-V
firmware boot/PLIC integration và RTL cycle/pin signoff chưa được kiểm chứng bởi
host platform này. DMA integration completion không thay thế full encoder signoff.

