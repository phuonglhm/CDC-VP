# Inter-Prediction VP

Module C++17/SystemC 2.3.4/TLM cho HAS §8: IME, FME, MC, EEI metadata,
IPC và current-MB/reference snapshots. CMake parent chưa được thay đổi.

Nguồn chính: [SISLAB HAS v2.1.11](../SISLAB_H264_AVC_HAS_Spec_Detailed_v2.1.11.md),
§8.1–8.6 và §6.2. Nội suy đối chiếu [ITU-T H.264 §8.4.2.2](https://www.itu.int/rec/dologin_pub.asp?id=T-REC-H.264-200503-S%21%21PDF-E&lang=e&type=items).
Xem [spec mapping](docs/SPEC_MAPPING.md), [test plan](docs/TEST_PLAN.md),
[validation](VALIDATION.md) để biết phạm vi đã kiểm chứng.

## Chức năng

- IME tính 16 SAD primitive 4×4 một lần/candidate, tổng hợp 41 vị trí thuộc
  7 loại partition. Cost uint64_t; overflow bị reject.
- FME lấy integer winner, đánh giá explicit fractional offsets; AVC six-tap
  luma/quarter averaging, giữ signed intermediate trước clip.
- MC dùng committed mode/reference incarnation; Y/U/V theo transform-block
  order cùng plane/block/phase/last metadata.
- EEI đóng gói reference/partition/MV/MVD từ MVP do caller cấp.
- Dual-list sparse cache: independent tags/residency và AVC edge extension.
- TLM giữ metadata trong refill/stall, hủy stale epoch/reset; done sau final Accept.

Caller cung cấp candidate order, rate cost và MVP. Tie policy của VP giữ
candidate đầu tiên có minimum total cost; integer winner là seed đầu của FME.
Policy này tái lập được, chưa xác nhận comparator priority của SISLAB RTL.

## Build và test

Chạy từ folder Inter-Prediction. Core không cần SystemC hay DMA:

```sh
cmake -S . -B build/core -DH264_INTER_BUILD_TLM=OFF -DH264_INTER_BUILD_DMA_TESTS=OFF
cmake --build build/core -j 4
ctest --test-dir build/core --output-on-failure
```

Full VP với SystemC 2.3.4, cùng compiler/C++17 ABI với thư viện SystemC:

```sh
cmake -S . -B build/vp -DCMAKE_PREFIX_PATH=<SystemC-prefix>
cmake --build build/vp -j 4
ctest --test-dir build/vp --output-on-failure
```

Cũng có thể dùng SYSTEMC_SOURCE_DIR hoặc cả SYSTEMC_INCLUDE_DIR/SYSTEMC_LIBRARY.
SW DMA tests mặc định ON, cần ../dma_subsystems. Tắt H264_INTER_BUILD_DMA_TESTS
nếu chỉ lấy module và shared contracts: có 15 tests khi TLM ON; full suite 18;
core-only 3. Build output được ignore, evidence trong tests/evidence.

Public targets: cdc::components::h264_inter_core và
cdc::components::h264_inter_tlm. TLM cần ../interfaces và ../synchronization.
Headers DMA chỉ là dependency của integration test. Parent có thể dùng
add_subdirectory(Inter-Prediction); install/export chưa gắn vào package H.264.

## Core API

[inter_core.h](include/h264/inter/inter_core.h) không phụ thuộc SystemC.
Reference tag gồm picture identity và REFM slot 0..2; mỗi list giữ một tag.
retag luôn đổi incarnation kể cả tag giống nhau. fill validate toàn rectangle
trước publish residency; resident pixels bất biến đến retag, overlap cùng data
được chấp nhận. Cache nhận reconstructed reference, không tự lấy current/source.

Request nhận Y16×16 current MB, absolute MB coordinates và một partition.
P chỉ dùng L0. B cần integer candidates trên cả L0/L1; VP preflight support cả
hai trước search. Mode là uni-prediction winner trên một list. Fractional
candidate offsets tương đối với IME winner, trong [-3,3] quarter samples.

Hỗ trợ progressive 8-bit planar YUV 4:2:0, coded dimensions 16..4096 mỗi chiều,
MB-aligned. Khi ghép DMA hiện có, dùng giao của hai contract: tối đa 1920×1088
([DMA spec](../dma_subsystems/DMA_SUBSYSTEM_SPEC.md)). MV giới hạn ±65536 quarter
samples để giữ arithmetic trong miền đã validate.

## SystemC/TLM protocol

[inter_tlm.h](include/h264/inter/inter_tlm.h) định nghĩa operation simulation,
không phải MMIO register của HAS.

| Operation | Command / payload | Kết quả |
|---|---|---|
| Evaluate = 0 | WRITE, 256 bytes Y + request extension | Decision/token/EEI; chưa commit/done |
| Commit = 1 | WRITE, 0 bytes + token | Khóa winner cho MC |
| Peek = 2 | READ, 1 byte + token | Held sample/metadata, không tăng cursor |
| Accept = 3 | WRITE, held byte + token/accept_sequence | Handshake mẫu; final Accept mới set done |

Lặp Peek trả cùng sample. Caller chờ trước Accept để mô hình hóa consumer stall;
output() giữ data/metadata. Chroma của partition nhỏ chỉ là phần thuộc enclosing
4×4 transform block; TQ adapter cần assemble các phần trước transform.

retag/refill là successful-response adapter calls cho SW DMA. Retag trước search;
chỉ refill từ successful completed reads. Tag match thiếu support vẫn chờ.
Retag trong search hoặc selected committed reference hủy/stale mode tương ứng.
Model không tự phát AXI; tests dùng actual SW DMA/bridge/DDR.

b_transport chỉ gọi từ SC_THREAD; một transport và một pending mode tại một thời
điểm. Request/source copy trước incoming-delay wait. Options timing là VP knobs,
không phải latency cố định/cycle RTL. progress() cho stage/candidate/sample state.

Reset active-low, enable falling/frame rising invalidate cả lists và mode.
Reset khi enable high cần enable-low rồi rising để rearm. Optional
h264::EpochExtension dùng shared reset domain, phải sống đến response.
Shared cancellation invalidate frame của request live; stale-at-entry giữ owner.
Caller cần phối hợp enable/reset để rearm sau shared reset, kể cả khi domain
đổi lúc module idle.

## Giới hạn

Chưa có autonomous partition/mode selection, SISLAB rate/MVP derivation,
skip/direct policy, bi/weighted prediction hoặc CAVLC encoding. Chưa ghép
TQ/reconstruction/deblocking/full encoder, chưa golden QCIF/601-frame hay cycle
RTL equivalence. Cần Algorithm Spec/RTL và integration evidence cho các mục này;
unit/contract PASS không thay thế full codec signoff.
