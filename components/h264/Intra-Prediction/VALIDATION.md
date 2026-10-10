# Intra-Prediction validation

Đối chiếu [HAS v2.1.11](../SISLAB_H264_AVC_HAS_Spec_Detailed_v2.1.11.md)
§7.1–7.5, §13.2, §15.3–15.4 và §18.3, cùng
[DMA contract](../dma_subsystems/DMA_SUBSYSTEM_SPEC.md).
Đây là validation của block prediction và giao thức VP/TLM.

## Kiểm tra cuối

Ngày 2026-10-10 lúc 11:59 +07: build lại và chạy standalone **10/10 PASS**,
UBSan **10/10 PASS**, không diagnostic. Đọc lại production sources và đối
chiếu HAS/DMA spec; chưa phát hiện lỗi chức năng mới trong phạm vi vectors
và VP contract hiện tại. Source/test hashes vẫn khớp regression evidence.
Lượt này chỉ ghi trong folder Intra; H264 integration 55/55 bên dưới là
evidence lượt 11:47, không được chạy lại trong kiểm tra cuối.

## Kết quả

Lượt regression ngày 2026-10-10, 11:46–11:47 +07: WSL, GNU C++ 15.2.0,
C++17, SystemC 2.3.4. Release cho regression, Debug cho UBSan.

| Suite | Kết quả |
|---|---|
| Standalone Intra | **10/10 PASS** |
| H264 component | **55/55 PASS**, gồm 22 test liên quan intra/prediction |
| Standalone UBSan | **10/10 PASS**, `halt_on_error=1`, không diagnostic |
| Core algorithms / independent regression | 2.316 / 79.382 checks |
| TLM contract / reset matrix | 62 / 590 checks; reset matrix có 44 scenarios |
| Extended TLM / zero latency | 7.420 / 7.388 checks |
| Shared-reset boundary / zero service latency | 710 / 228 checks; 42 / 14 cases |
| Shared-DMA integration 32/64/128 | 3/3 PASS, 18 checks mỗi bus |
| CMB full picture 32/64/128 | 3/3 PASS, 22.626 checks mỗi bus |
| CMB mixed partition 32/64/128 | 3/3 PASS, 22.630 checks mỗi bus |
| CMB TQ wiring 32/64/128 | 3/3 PASS, 13.881 checks mỗi bus |

[Regression evidence](tests/evidence/regression_20261010.txt) chứa commands,
source/test hashes và raw logs của ba suite. Các test Intra không dùng
`WILL_FAIL`. Tổng 55 test H264 có cả codec stub và expected-failure cases;
PASS của những test đó xác nhận contract/từ chối lỗi, không chứng nhận codec.

## Yêu cầu và evidence

| HAS requirement | Test / observable | Phạm vi |
|---|---|---|
| §7.1 sample generators và legal modes | `core_tests.cpp`, `review/core_review.cpp`, `prediction_oracle.h`: 9 luma4 modes, 4 luma16/chroma modes; 512 reference sets × 16 availability masks | Oracle độc lập bằng coefficient tables, plane gradients và chroma DC quadrants; random/boundary inputs, seed 20261010 |
| §7.1 reference availability | Đủ 16 scan positions; guard 3/7/11/13/15 dù đã import future pixels; full coded width và toàn right edge | Core và independent decoded/valid map trong CMB bench |
| §7.2 reconstructed ownership | Source 255 / feedback 17 / next predictor 17; storage/valid riêng Y/U/V | CMB source không tự trở thành neighbor; decoded feedback fixture có khai báo rõ |
| §7.2 RAM latency và metadata | `tlm_tests.cpp`, `tlm_regression.cpp`: token/block/reference snapshot; caller đổi metadata không chuyển block | Kiểm tra timing và alignment của abstraction VP, không chứng nhận pipeline RAM RTL |
| §7.3 cost accumulation / best candidate | SAD/SATD matrix oracle, mọi candidate/global minimum trong picture vectors; signed residual; 64-bit penalty | Numeric candidate order và SAD/SATD là lựa chọn VP công bố ở README |
| §7.3 strict ties | Y4/Y16/U8/V8 × SAD/SATD × 3 scenarios: cost 0, nonzero distortion, penalties cho modes 1/2 | 24 vectors chạy với cả custom và zero latency; giữ first legal minimum qua repeated Replay và commit |
| §7.4 reset / repeated activation | `reset_tests.cpp`, `review/epoch_review.cpp`: local/shared/frame reset, benign notifications, stale-entry isolation, old-token rejection, recovery | 44-case matrix qua incoming/reference/candidate/compare/replay/feedback/import; enable giữ cao không restart frame |
| §7.4 winning replay / right edge | Ép modes 3/7 thắng tại x44, width48; so toàn predictor/residual bytes, mode/cost/reference snapshot và committed mode | Evaluate/Replay dùng cùng upper-right/right-edge rules |
| §7.4 stall / completion | Repeated Replay sau stalls; premature feedback rejection; observer trong feedback wait; completion event sau memory/mode commit | TLM blocking acceptance; caller tổng hợp block done thành MB done |
| §7.5 variable latency | Đo 1/2/3/4/6/9 legal candidates; custom 9/6/3/8/11ns, incoming7ns, zero service latency | 6-candidate Evaluate = 70ns; benign notifications giữ deadline; HAS không yêu cầu fixed MB-cycle count |
| Shared-reset boundary | `reset_boundary_tests.cpp`: 7 sites × offsets −1/0/+1ps × 2 timer orders; thêm zero-service cases | 42 + 14 cases; status/state/events theo reset trước/sau acceptance, không obsolete commit; local/frame exact-deadline sweep chưa có |
| §7.1 caller scheduling / CMB handoff | `../integration_tests/prediction/cmb_intra_tb.cpp`: bus32/64/128, Y/U/V, 4 pictures48×32, 24MB/252blocks | Homogeneous và checkerboard luma4/luma16; independent reference map; tất cả pixels commit trước picture completion |
| DMA ownership | CMB readiness sau đủ 32 row responses; planar strides/lanes, 4KiB boundaries và error path | Actual CmbDma/H264Arb/AxiMasterBridge/DdrModel; failed responses không expose tile |
| §15.3 HAS-V-INTRA-01 | Reset, upper-right/right-edge, tie và predictor/replay có vectors | Chưa đóng observable bitstream và RTL/codec comparison |

Tại cùng timestamp, boundary test ghi nhận reset thực sự xảy ra trước hay sau
response rồi đối chiếu status, flags và event counts. Reset đã xảy ra trước
entry chỉ từ chối request stale; reset hủy request live invalidate frame cũ.
Zero service latency không có điểm yield bên trong thao tác; incoming-delay
case của suite đó vẫn dùng 20ns. Shared reset khi idle cần caller phối hợp
local reset/frame enable theo [socket contract](README.md#socket-protocol).

## Reset fixes được bảo vệ bởi regression

- **F1: incoming-delay cancellation giữ token/context cũ.** Validate epoch,
  token, block và payload trước incoming wait; request live hợp lệ sở hữu
  frame trước các wait. Cancellation invalidate frame cũ ngay cả khi chưa
  tạo winner; stale/invalid requests không xóa owner hiện tại. Generation mới
  không bị catch của transaction cũ xóa.
- **F2: shared reset không đánh thức latency wait.** Wait theo cả local
  generation event và `ResetDomain::changed`; kiểm tra lại epoch sau wakeup.
  Notification không đổi epoch tiếp tục chờ đến deadline ban đầu.

Hai reproducers reset tại 12ns đều trả cancellation ở 12ns; obsolete Replay
và feedback commit bị reject. Các fix nằm trong `src/intra_tlm.cpp`.

## Coverage và evidence trước mở rộng regression

[Fix evidence snapshot](tests/evidence/fix_20261010.txt) ghi nhận core
ASan+UBSan 79.382 checks và production gcov trước khi mở rộng suite lên 10 test.
Hai production files vẫn khớp hashes của snapshot đó.

| Production file | Executable lines | Branches executed | Branch outcomes taken ≥1 |
|---|---|---|---|
| `src/intra_core.cpp` | 100% / 244 | 100% / 426 | 86.15% |
| `src/intra_tlm.cpp` | 100% / 114 | 98.17% / 219 | 71.69% |

Branch metrics gồm compiler/exception paths và defensive guards. 100% line
coverage không phải 100% input/branch/spec coverage. Snapshot cũ có một số
test hashes khác vì regression đã được mở rộng; dùng manifest regression
mới để đối chiếu tests hiện tại.

Core-only 2/2, isolated review 3/3 và installed consumer 1/1 đã PASS trong
lượt trước. Coverage/ASan và các suite này không được chạy lại trong lượt
11:46–11:47; production code không đổi trong lần bổ sung tests đó.

## Chạy lại

Configure/build từ source theo [README](README.md#build-và-test). Với các
build đã configure, chạy từ folder H264:

```sh
cmake --build Intra-Prediction/build-tlm-linux -j 8
ctest --test-dir Intra-Prediction/build-tlm-linux --output-on-failure
cmake --build build/intra-linux -j 4
ctest --test-dir build/intra-linux -R 'intra|prediction' --output-on-failure
ctest --test-dir build/intra-linux --output-on-failure
```

Để tạo UBSan build mới, chạy từ folder Intra-Prediction:

```sh
cmake -S . -B build-ubsan -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_FLAGS=-fsanitize=undefined \
  -DCMAKE_PREFIX_PATH=<SystemC-prefix>
cmake --build build-ubsan -j 4
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
  ctest --test-dir build-ubsan --output-on-failure
```

Raw CTest logs nằm trong `Testing/Temporary/LastTest.log` của từng build.
Build trees/caches và working reports không thuộc source delivery; retained
evidence trong `tests/evidence` là text và được version cùng module.

## Phạm vi khi push riêng folder

- 10 standalone tests và source module thuộc folder này. TLM dùng
  `../interfaces` và `../synchronization`, hiện đã được version trong repo;
  core-only không cần SystemC.
- Parent `../CMakeLists.txt` phải thêm module; `../cmake/install.cmake` phải
  export hai libraries. CMB/TQ suites nằm ở `../integration_tests/prediction`
  và cần DMA headers ở `../dma_subsystems`. Những thay đổi ở ngoài folder
  phải được owner tích hợp riêng để tái lập H264 55/55 và installed targets.
- Tại thời điểm review, HAS markdown và `../dma_subsystems` vẫn untracked.
  Commit chỉ chứa Intra không đưa hai nguồn spec/dependency này lên Git;
  owner cần version chúng để links và integrated tests dùng được ở checkout mới.
- Khi tích hợp DMA, dùng coded dimensions chung tối đa 1920×1088 theo DMA
  spec. Core Intra cho phép tới 4096 mỗi chiều ở mức VP riêng.

Build/cache/archive đã được ignore. Phạm vi delivery này là module VP và
evidence đã nêu, chưa phải full HAS/encoder signoff.

## Giới hạn qualification

- TQ wiring dùng actual TqTop cho 192 Y4 blocks mỗi bus tại QP26. FTQ hiện
  bypass special DC Hadamard và thiếu quantization scaling; ITQ chưa inverse
  scaling theo QP. Chroma/luma16 feedback còn là fixture. PASS kiểm tra wiring
  và ownership, không chứng nhận transform H.264.
- Chưa có released codec adapter hoặc SISLAB RTL golden độc lập cho selected
  modes, reconstructed pixels và bitstream. §15.4/§18.3 còn cần QCIF byte
  equality trên đúng source/config, independent decode, PSNR và repeatability.
  Số bytes của release vectors trong HAS không phải expected length cho mọi
  QCIF video.
- Caller/pipeline còn sở hữu partition selection, production MB scheduling,
  slice/constrained-intra availability và MPM syntax. Hỗ trợ module là
  8-bit 4:2:0 như README công bố.
- Chưa sweep local/frame reset tại đúng deadline, multiple random seeds hoặc
  QCIF/HD picture stress. Full-picture integration hiện dùng 48×32.
- SystemC tests không thay thế cocotb trên `h264_axi_wrapper`, pin/cycle RTL
  qualification hay FPGA timing/DRC/long-run hardware evidence của HAS.
