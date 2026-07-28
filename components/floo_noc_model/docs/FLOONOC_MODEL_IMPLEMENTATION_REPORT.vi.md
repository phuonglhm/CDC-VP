# Báo cáo tổng hợp xây dựng mô hình FlooNoC SystemC/TLM

## 1. Thông tin chung

- Tên hạng mục: Mô hình FlooNoC mức cycle-approximate bằng SystemC/TLM.
- Thư mục triển khai:
  `/home/duyptt_HW/Desktop/VP_INTER/upgit/CDC-VP/components/floo_noc_model`
- Nguồn RTL tham chiếu:
  `/home/duyptt_HW/Documents/work/Study_FlooNoC/FlooNoC`
- FlooNoC revision cố định: `9a6972a`.
- FlooGen version: `0.8.4`.
- SystemC: `2.3.4`.
- C++ standard: C++17.
- Compiler đã kiểm thử: GCC/G++ `11.5.0`.
- RTL simulator đã kiểm thử: Verilator `5.022`.
- Dependency RTL đã resolve: `common_cells` `1.39.0` @
  `9ca8a7655f741e7dd5736669a20a301325194c28`.
- Ngày cập nhật báo cáo: 2026-07-28.

## 2. Mục tiêu

Mục tiêu của hạng mục là xây dựng một mô hình SystemC có khả năng mô phỏng
hoạt động vi kiến trúc của FlooNoC theo từng chu kỳ, phục vụ:

- nghiên cứu routing, buffering, arbitration và back-pressure;
- đo latency, throughput, stall và utilization;
- so sánh hành vi theo chu kỳ với RTL;
- tiến tới tích hợp FlooNoC vào CDC-VP;
- cho phép phần mềm chạy trên Virtual Platform đi qua một fabric có đặc tính
  gần với RTL hơn mô hình functional thuần túy.

Đây là hướng Direction-2, tức mô hình cycle-approximate/micro-architecture.

## 3. Nguyên tắc kiến trúc đã thống nhất

### 3.1 Bám sát FlooNoC

Nguồn sự thật được ưu tiên theo thứ tự:

1. FlooNoC RTL tại revision đã cố định.
2. Cấu hình FlooGen và topology được sinh ra.
3. RTL testbench và assertion của FlooNoC.
4. Tài liệu kiến trúc FlooNoC.
5. Mô hình SystemC là đối tượng cần được kiểm chứng.

Khi SystemC và RTL khác nhau, cần kiểm tra cấu hình RTL rồi sửa SystemC theo
RTL. Không sửa RTL tham chiếu chỉ để test SystemC chạy qua.

### 3.2 Không sao chép mô hình NPU

Mô hình SAURIA/NPU chỉ được dùng làm ví dụ cho quy trình Direction-2, cách chia
phase, tổ chức build, verification và packaging.

Các đặc điểm của NPU không được mang sang FlooNoC:

- không có giao thức `START/DONE` giả lập;
- không có tensor staging;
- không có GEMM FSM;
- không dùng register map của NPU;
- không dùng golden model để ghi đè output;
- không xem FlooNoC như một MMIO peripheral đơn lẻ.

FlooNoC là interconnect hoạt động theo traffic. Hoạt động của nó được kích hoạt
bởi transaction và handshake, không phải một lệnh start.

### 3.3 Vị trí tích hợp trong CDC-VP

FlooNoC cần thay thế, cấu thành hoặc nằm phân cấp bên trong fabric của CDC-VP.
Hướng tích hợp dự kiến là M:N TLM fabric adapter hoặc tập hợp các AXI endpoint
transactor.

Việc gắn FlooNoC như một target socket MMIO thông thường là không đúng bản chất
kiến trúc.

### 3.4 Chính sách về độ chính xác

- Trạng thái và datapath SystemC là output chính thức của mô hình.
- Golden/reference model chỉ dùng để so sánh.
- Một block chỉ được gọi là RTL-signed sau khi có so sánh theo từng cycle.
- Các block chưa cross-check RTL phải được ghi là cycle-approximate hoặc
  estimate.

Hiện tại đã RTL-signed: XY route selector và input FIFO
(`stream_fifo_optimal_wrap`). Các block còn lại vẫn là cycle-approximate.

## 4. Phạm vi vertical slice v0

### 4.1 Tính năng được chọn

| Hạng mục | Cấu hình hiện tại |
|---|---|
| Loại network | Single-AXI vertical slice |
| Routing | Deterministic XY |
| Traffic | Unicast |
| Flow control | Ready/valid |
| Hướng router | North, East, South, West, Eject |
| Local port | Một Eject port trên mỗi router |
| Input FIFO | Có, depth là template parameter |
| Depth dùng trong router/mesh test | 2 |
| Output FIFO | Chưa có |
| Virtual channel | Chưa model |
| Credit flow control | Chưa model |
| Topology | Rectangular 2-D mesh |

### 4.2 Tính năng tạm hoãn

- Narrow-wide AXI và wide physical channel.
- YX routing.
- Source routing.
- ID/table-based routing.
- Virtual channel.
- Credit flow control.
- Multicast.
- Synchronization và reduction collective.
- AXI ATOP.
- Reorder-buffer modes.
- Nhiều downstream AXI ID.
- CDC link và nhiều clock domain.
- Nhiều local Eject port.
- Explicit link pipeline/cut.
- Topology irregular hoặc tree.

Các tính năng này chỉ nên được thêm sau khi những block thấp hơn đã có RTL
cross-check ổn định.

## 5. Phương pháp xây dựng

Mô hình được xây dựng theo hướng bottom-up:

```text
Kiểu dữ liệu và reference model
              |
              v
      Ready/valid FIFO
              |
              v
       XY route selector
              |
              v
      Wormhole arbiter
              |
              v
       Router năm cổng
              |
              v
         Mesh nhiều router
              |
              v
  AXI chimney và TLM adapter trong tương lai
```

Mỗi block được kiểm thử độc lập trước khi được ghép vào block cấp cao hơn.

## 6. Các thành phần đã triển khai

### 6.1 Kiểu dữ liệu FlooNoC

File:

```text
include/floo_noc_model/floo_types.hpp
```

Đã triển khai:

- enum direction:
  - North = 0;
  - East = 1;
  - South = 2;
  - West = 3;
  - Eject = 4.
- enum AXI channel: AW, W, AR, B và R.
- enum physical channel: req và rsp.
- kiểu coordinate gồm X, Y và local port ID.
- flit header gồm source, destination, `last`, AXI channel, RoB và ATOP fields.
- generic `basic_flit<PayloadBits>`.
- `test_flit` với payload 64 bit.

Các kiểu tùy chỉnh có:

- `operator==`;
- `operator!=`;
- stream output;
- `sc_trace`.

Nhờ đó chúng có thể được sử dụng trong `sc_signal` và waveform tracing.

### 6.2 Reference model

File:

```text
include/floo_noc_model/reference_model.hpp
```

Đã triển khai:

- address map theo các vùng `{base, size, destination}`;
- vùng địa chỉ theo dạng `[base, base + size)`;
- phát hiện vùng size bằng 0;
- phát hiện wrap-around địa chỉ 64 bit;
- phát hiện overlap;
- trả về không có destination đối với địa chỉ unmapped;
- tính next hop theo deterministic XY;
- tạo toàn bộ đường đi XY và Eject cuối cùng;
- kiểm tra source/destination nằm trong mesh;
- chỉ chấp nhận một local Eject port trong v0.

### 6.3 Input FIFO (`stream_fifo_optimal_wrap`)

File:

```text
include/floo_noc_model/stream_fifo.hpp
```

File này thay thế `ready_valid_fifo.hpp` cũ.

**Nguyên nhân gốc của việc thay thế.** Model cũ giả định một FIFO "optimal":
khi đầy vẫn nhận push nếu head được pop cùng chu kỳ. RTL đóng băng **không**
hoạt động như vậy ở cả hai nhánh của `stream_fifo_optimal_wrap`:

- `Depth == 2` → `spill_register_flushable`, với
  `ready_o = !a_full_q || !b_full_q`;
- `Depth > 2` → `stream_fifo`/`fifo_v3`, với `ready_o = ~full` và
  `push = valid_i & ~full`.

Ở cả hai nhánh, `ready_o` chỉ là hàm của thanh ghi. Buffer đầy luôn từ chối
push kể cả khi đang pop. Model cũ vì thế:

- cho throughput input buffer cao hơn RTL một flit tại mỗi lần đầy rồi được
  giải phóng;
- tạo ra đường tổ hợp `ready_i -> ready_o` mà RTL không có.

Lưu ý quan trọng: router đóng băng dùng `InFifoDepth = 2`, nên input buffer
thực tế là **spill register**, không phải circular FIFO.

Đã triển khai (bám đúng phân cấp RTL):

- `spill_register<T>`: hai thanh ghi A/B, `valid_o = a_full | b_full`,
  `data_o = b_full ? b_data : a_data`, A dồn sang B khi bị back-pressure;
- `stream_fifo<T, Depth>`: bộ nhớ vòng, con trỏ đọc/ghi và `status_cnt` dạng
  thanh ghi, `data_o = mem_q[read_pointer_q]` kể cả khi rỗng, reset xóa cả
  bộ nhớ giống `fifo_v3`;
- `stream_fifo_optimal_wrap<T, Depth>`: chọn nhánh theo depth, chặn depth 0/1
  bằng `static_assert` tương ứng `$fatal` của RTL.

Không model `flush_i` (router tie xuống 0) và `usage_o` (router để hở, nhánh
depth 2 trả `'x`). `o_occupancy` chỉ là output debug của model.

Block này đã được cross-check theo từng cycle với RTL gốc: 133 cycle khớp ở cả
depth 2 và depth 4.

### 6.4 XY route selector

File:

```text
include/floo_noc_model/xy_route_select.hpp
```

Đã triển khai:

- X được giải quyết trước Y;
- chọn Eject khi đã tới đúng coordinate;
- flit pass-through;
- khóa route sau khi nhận non-last flit;
- giữ route trong toàn bộ packet;
- release lock khi last flit được accept;
- reset route-lock state.

Đây là block duy nhất đã có RTL cross-check và đạt kết quả khớp từng cycle.

### 6.5 Wormhole arbiter

File:

```text
include/floo_noc_model/wormhole_arbiter.hpp
```

Đã triển khai:

- lựa chọn requester theo round-robin;
- chỉ input được chọn nhận ready;
- khóa requester khi packet chưa kết thúc;
- ngăn flit từ packet khác xen vào;
- release lock tại accepted last flit;
- chuyển round-robin priority sau khi packet hoàn thành;
- expose selected input và lock state.

Block này đã pass SystemC unit test nhưng chưa cross-check với RTL
`floo_wormhole_arbiter.sv`.

### 6.6 Router năm cổng

File:

```text
include/floo_noc_model/floo_router.hpp
```

Router hiện tại gồm:

- năm input port;
- năm output port;
- input FIFO trên từng port;
- XY route selector trên từng input;
- combinational crossbar;
- wormhole arbiter trên từng output;
- occupancy/selected/locked debug outputs.

Đã mô hình hóa hai tối ưu của cấu hình XY:

- chặn input-to-same-output loopback;
- traffic đi vào từ hướng Y không được quay lại hướng X.

Chưa có:

- output FIFO;
- virtual channel;
- credit path;
- reduction/collective;
- multicast handshake history;
- VC arbiter.

### 6.7 Rectangular mesh

File:

```text
include/floo_noc_model/floo_mesh.hpp
```

Đã triển khai:

- mesh `Width × Height`;
- node index dạng `y * Width + x`;
- nối East/West và North/South giữa các router;
- tie-off đúng các boundary port;
- một endpoint injection/ejection trên mỗi node;
- endpoint được nối vào Eject port;
- hỗ trợ back-pressure xuyên nhiều router.

Lưu ý quan trọng:

Inter-router link hiện là kết nối combinational bằng `SC_METHOD`. Chưa có link
pipeline/cut riêng. Storage giữa các hop hiện đến từ input FIFO của router kế
tiếp. Vì vậy chưa được phép khẳng định link đã có configurable one-cycle
latency.

## 7. Cấu trúc source hiện tại

```text
floo_noc_model/
  CMakeLists.txt
  Makefile
  README.md
  LICENSES/
    SHL-0.51.txt
  docs/
    P0_SCOPE.md
    RTL_MAPPING.md
    STATUS.md
    AI_HANDOFF_CONTEXT.md
    FLOONOC_MODEL_IMPLEMENTATION_REPORT.vi.md
  include/floo_noc_model/
    floo_types.hpp
    reference_model.hpp
    stream_fifo.hpp
    xy_route_select.hpp
    wormhole_arbiter.hpp
    floo_router.hpp
    floo_mesh.hpp
  tests/
    CMakeLists.txt
    test_reference_model.cpp
    test_stream_fifo.cpp
    test_xy_route_select.cpp
    test_wormhole_arbiter.cpp
    test_floo_router.cpp
    test_floo_mesh.cpp
    route_trace_sc.cpp
    fifo_trace_sc.cpp
    data/
      route_select_stimulus.csv
      route_select_expected.csv
      gen_stream_fifo_stimulus.py
      stream_fifo_stimulus.csv
      stream_fifo_expected_d2.csv
      stream_fifo_expected_d4.csv
  rtl_crosscheck/
    compare_traces.py
    fetch_rtl_deps.sh
    run_route_select_crosscheck.sh
    run_stream_fifo_crosscheck.sh
    route_select/
      floo_pkg.sv
      tb_route_select_trace.sv
      shim/common_cells/registers.svh
    stream_fifo/
      tb_stream_fifo_trace.sv
```

`stream_fifo_expected_d*.csv` được sinh từ RTL gốc bởi
`run_stream_fifo_crosscheck.sh`, dùng để regression SystemC bắt lỗi mà không
cần Verilator. Tuyệt đối không sinh lại các file này từ model.

CMake export target hiện tại:

```text
cdc::components::floo_noc_model
```

Mô hình hiện là header-only interface library.

## 8. Hệ thống kiểm thử đã xây dựng

### 8.1 SystemC regression

| Test | Nội dung kiểm thử |
|---|---|
| `test_reference_model` | Address boundary, overlap, out-of-mesh và XY path |
| `test_stream_fifo` | Nhánh spill (depth 2) và FIFO (depth 4): reset, fill, từ chối push khi đầy, pointer wrap, drain |
| `test_xy_route_select` | XY order, Eject, route lock và release |
| `test_wormhole_arbiter` | Round-robin, packet lock và chống interleave |
| `test_floo_router` | Contention, output stall, FIFO và packet continuity |
| `test_floo_mesh` | Mesh 2×2, multi-hop delivery và stable stall |
| `test_route_trace_sc` | CSV-driven route/lock trace |
| `test_fifo_trace_sc_d2` | 133 cycle FIFO trace so với golden lấy từ RTL, depth 2 |
| `test_fifo_trace_sc_d4` | 133 cycle FIFO trace so với golden lấy từ RTL, depth 4 |

Kết quả chạy ngày 2026-07-28:

```text
100% tests passed, 0 tests failed out of 9
```

### 8.2 Route-selector SystemC ↔ RTL cross-check

RTL tham chiếu:

```text
FlooNoC/hw/floo_route_select.sv
```

Flow kiểm thử:

1. Đọc chung một file stimulus CSV.
2. Chạy stimulus qua SystemC route selector.
3. Chạy stimulus qua RTL gốc bằng Verilator.
4. Ghi hai trace CSV.
5. So sánh chính xác từng dòng.

Các trường được so sánh:

```text
cycle,route,locked
```

Coverage:

- reset;
- North;
- East;
- South;
- West;
- Eject;
- valid khi ready bằng 0;
- acquire route lock;
- giữ route khi đang back-pressure;
- release route lock tại last flit.

Kết quả đã được người dùng chạy và xác nhận:

```text
route_trace_sc PASS
route-select cross-check PASS: 12 cycles match
```

### 8.2b Input-FIFO SystemC ↔ RTL cross-check

RTL tham chiếu (không sửa đổi, lấy từ `common_cells` 1.39.0 @ `9ca8a76`):

```text
src/fifo_v3.sv
src/stream_fifo.sv
src/spill_register_flushable.sv
src/stream_fifo_optimal_wrap.sv
```

Testbench instantiate wrap đúng theo cách `hw/floo_router.sv` dùng: `flush_i`
tie 0, `testmode_i` 0, `usage_o` để hở. `Depth` là top-level parameter nên một
testbench phủ cả hai nhánh.

Các trường được so sánh:

```text
cycle,pre_ready,pre_valid,pre_data,post_ready,post_valid,post_data
```

Lấy mẫu cả trước và sau cạnh clock là bắt buộc: nếu chỉ lấy mẫu sau cạnh, một
`ready_o` phụ thuộc sai vào `ready_i` hiện tại sẽ bị che và cross-check vẫn
PASS. Điều này đã được kiểm chứng bằng thực nghiệm.

Coverage của stimulus: reset, idle, fill vượt full khi bị stall, push bị từ
chối khi đầy, push đồng thời pop khi đầy, drain, streaming full-rate, pointer
wrap, reset giữa dòng, xung valid/ready một chu kỳ, và 80 cycle pseudo-random
tất định. Sinh lại bằng `tests/data/gen_stream_fifo_stimulus.py`.

Kết quả:

```text
common_cells 1.39.0 @ 9ca8a76 verified
stream-fifo depth 2 cross-check PASS: 133 cycles match
stream-fifo depth 4 cross-check PASS: 133 cycles match
```

Harness đã được kiểm chứng bằng hai negative control, cả hai đều phải FAIL:

| Lỗi cố ý tiêm vào model | Kết quả |
|---|---|
| `ready_o` phụ thuộc `ready_i` | FAIL tại cột pre-edge, cycle 9 |
| Quy tắc accept-at-full của model cũ | FAIL do `data_o` lệch từ cycle 9 |

Không so sánh `usage_o`: router đóng băng để hở tín hiệu này và nhánh depth 2
trả `'x`.

### 8.3 Bảo vệ revision RTL

Cross-check runner kiểm tra SHA-256 của:

```text
hw/floo_route_select.sv
```

Giá trị yêu cầu:

```text
234fefcb0cee853ee93299b16caee0811e7f142237f205ee399d3f9c53073f1e
```

Nếu RTL root trỏ tới revision khác, runner dừng thay vì báo PASS sai nguồn.

### 8.4 Shim dùng trong leaf RTL cross-check

Do dependency tree của Bender chưa được checkout, route-selector harness cung
cấp shim tối thiểu gồm:

- enum values từ `floo_pkg`;
- hàm `floo_iomsb`;
- macro register `FF` và `FFL`.

Routing và route-lock logic vẫn được compile trực tiếp từ RTL FlooNoC gốc.
Shim không thay thế hành vi route selector.

### 8.5 Warning RTL đã quan sát

Verilator có thể báo `WIDTHEXPAND` tại biểu thức:

```systemverilog
Eject + channel_i.hdr.dst_id.port_id
```

Warning không ảnh hưởng test hiện tại vì v0 chỉ hỗ trợ `port_id == 0`.
Multi-local-port chưa được sign-off.

Dòng sau cũng không phải lỗi:

```text
make: Nothing to be done for 'default'.
```

Nó chỉ cho biết Verilator binary đã được build và source không thay đổi.

## 9. Môi trường và lệnh build bắt buộc

Trước mọi build phải chạy:

```bash
export CC=/usr/bin/gcc
export CXX=/usr/bin/g++
export PATH=/usr/bin:/bin:$PATH

# sanity
$CC -dumpfullversion
$CXX --version | head
```

Mục đích là tránh compiler wrapper không tương thích từ môi trường Synopsys.

### 9.1 Chạy toàn bộ SystemC regression

```bash
cd /home/duyptt_HW/Desktop/VP_INTER/upgit/CDC-VP/components/floo_noc_model

export CC=/usr/bin/gcc
export CXX=/usr/bin/g++
export PATH=/usr/bin:/bin:$PATH

# sanity
$CC -dumpfullversion
$CXX --version | head

cmake -E remove_directory /tmp/floo_noc_model_build
make BUILD_DIR=/tmp/floo_noc_model_build test
```

### 9.2 Chạy route-selector RTL cross-check

```bash
cd /home/duyptt_HW/Desktop/VP_INTER/upgit/CDC-VP/components/floo_noc_model

export CC=/usr/bin/gcc
export CXX=/usr/bin/g++
export PATH=/usr/bin:/bin:$PATH

# sanity
$CC -dumpfullversion
$CXX --version | head

./rtl_crosscheck/run_route_select_crosscheck.sh
```

### 9.2b Chạy input-FIFO RTL cross-check

```bash
cd /home/duyptt_HW/Desktop/VP_INTER/upgit/CDC-VP/components/floo_noc_model
./rtl_crosscheck/run_stream_fifo_crosscheck.sh
```

Script tự export môi trường compiler bắt buộc và tự gọi
`rtl_crosscheck/fetch_rtl_deps.sh` để lấy đúng revision dependency. Lần chạy
đầu tiên cần mạng; các lần sau chạy offline.

Chỉ resolve dependency mà không chạy cross-check:

```bash
./rtl_crosscheck/fetch_rtl_deps.sh
```

### 9.3 Lỗi CMake cache đã gặp

Lỗi:

```text
CMake Error: The source ... does not match the source ... used to generate cache
```

Nguyên nhân:

- build directory dưới `/tmp` từng được tạo từ một staging source path khác;
- CMake cache lưu absolute source path;
- dùng lại cùng build directory từ component thật sẽ bị từ chối.

Cách xử lý:

```bash
cmake -E remove_directory /tmp/floo_noc_model_build
```

Hoặc dùng một build directory mới:

```bash
make BUILD_DIR=/tmp/floo_noc_model_build_check test
```

Route cross-check runner đã dùng `cmake --fresh` để tránh lỗi cache tương tự.

## 10. Dependency và tool status

Đang có:

- Verilator `/usr/bin/verilator`;
- VCS `/opt/synopsys/vcs/X-2025.06/bin/vcs`;
- `Bender.yml`;
- `Bender.lock`.

Chưa có trong `PATH`:

```text
bender
```

Trạng thái dependency trong `Bender.lock`:

- `common_cells` 1.39.0 @ `9ca8a76` — **đã checkout và verify hash**;
- `axi` 0.39.9 — chưa cần, chưa fetch;
- `axi_riscv_atomics` 0.8.3 — chưa cần, chưa fetch;
- `common_verification` 0.2.5 — chưa cần, chưa fetch;
- `idma` 0.6.5 — chưa cần, chưa fetch.

`rtl_crosscheck/fetch_rtl_deps.sh` thay thế Bender ở phạm vi hẹp: đọc revision
từ `Bender.lock`, dừng nếu lock đã đổi so với giá trị đóng băng, checkout đúng
commit, và verify SHA-256 từng file được compile. Script này **không** giải
transitive dependency và không sinh file list theo thứ tự — cross-check ở mức
router vẫn cần Bender thật hoặc một file list đầy đủ tương đương.

FIFO, wormhole arbiter và router cross-check phải dùng đúng dependency version
được khóa. Không được viết một behavioral SV FIFO/arbiter khác rồi gọi đó là
RTL cross-check.

## 11. Kết quả đạt được

### 11.1 Về mô hình

- Đã có nền tảng type và reference model.
- Đã có input FIFO bám đúng phân cấp `stream_fifo_optimal_wrap` của RTL.
- Đã có deterministic XY routing.
- Đã có packet route locking.
- Đã có wormhole output arbitration.
- Đã có router năm cổng.
- Đã có rectangular mesh với abstract endpoint.
- Đã có standalone CMake/Make build.

### 11.2 Về verification

- Chín SystemC test đều PASS.
- Route-selector khớp RTL 12/12 cycle.
- Input FIFO khớp RTL 133/133 cycle ở cả depth 2 và depth 4.
- Đã phát hiện và sửa một sai lệch thật giữa model và RTL (accept-at-full).
- Có common CSV stimulus/trace format.
- Có automatic trace comparison.
- Có RTL source hash guard và dependency revision guard.
- Có negative control chứng minh harness thực sự bắt được lỗi.

### 11.3 Về tài liệu

Đã có:

- scope và quyết định P0;
- RTL-to-SystemC mapping;
- implementation status;
- Direction-2 playbook;
- AI handoff context;
- báo cáo tổng hợp tiếng Việt này.

## 12. Những gì chưa thể khẳng định

Chưa thể khẳng định:

- toàn bộ router cycle-equivalent với RTL;
- FIFO cycle-equivalent ở depth khác 2 và 4, hoặc trên `usage_o`/`flush_i`;
- wormhole arbiter có priority/state hoàn toàn giống RTL;
- mesh có latency theo đúng generated topology;
- current mesh là full single-AXI network;
- req và rsp đã được tách thành hai physical network;
- AXI ordering đã đúng;
- CDC-VP traffic đã đi qua FlooNoC;
- performance counters đã phản ánh RTL.

Việc FIFO đã RTL-signed **không** làm router trở thành cycle-equivalent: phần
routing, crossbar và arbitration bao quanh buffer vẫn chưa được kiểm chứng.

Vì vậy kết luận chính xác hiện tại là:

> Mô hình đã hoàn thành nền tảng router/mesh SystemC và kiểm thử contract nội
> bộ. XY route selector và input FIFO đã được xác nhận khớp RTL theo cycle.
> Các block còn lại vẫn ở mức cycle-approximate.

## 13. Các hạn chế kỹ thuật hiện tại

### 13.1 AXI chưa hoàn chỉnh

Chưa có:

- AXI AW/W/AR/B/R structure đầy đủ;
- address-to-destination mapping trong chimney;
- packetization;
- AW/W coupling;
- B/R unpacking;
- metadata buffer;
- downstream ID mapping;
- outstanding transaction handling;
- response ordering;
- reorder buffer.

### 13.2 Network chưa hoàn chỉnh

- Mesh hiện chỉ chở một generic `FlitT` stream.
- `req` và `rsp` mới là type identifier.
- Chưa có hai network instance độc lập.
- Chưa có output FIFO.
- Chưa có explicit link latency.
- Chưa có virtual channel và credit.

### 13.3 Instrumentation chưa có

Các counter dự kiến nhưng chưa triển khai:

- transaction/flit latency;
- flit và payload bytes per cycle;
- injection/ejection stalls;
- arbitration wait;
- FIFO high-water mark;
- link utilization;
- hop count;
- outstanding responses;
- RoB occupancy.

### 13.4 CDC-VP chưa tích hợp

Chưa có:

- M:N TLM fabric adapter;
- TLM-to-AXI transactor;
- platform bus binding;
- quiescence detector;
- safe gated clock;
- firmware end-to-end test;
- SDK packaging.

## 14. Hướng công việc tiếp theo

### Bước 1: Cross-check FIFO — ĐÃ XONG (2026-07-28)

- Đã resolve `common_cells` 1.39.0 theo đúng revision khóa, có hash guard.
- Đã compile `stream_fifo_optimal_wrap` gốc, không dùng shim.
- Đã phát hiện model sai và sửa model theo RTL.
- 133 cycle khớp ở depth 2 và depth 4.
- Đã kiểm chứng harness bằng negative control.

Phần FIFO còn thiếu: depth khác 2/4, `usage_o`, `flush_i`, `testmode_i`, và
packing của flit struct thật.

### Bước 2: Cross-check wormhole arbiter (bước kế tiếp)

- Compile `hw/floo_wormhole_arbiter.sv`.
- Dùng đúng `rr_arb_tree` từ checkout `common_cells` đã có sẵn.
- Bổ sung hash của các file dependency mới vào `fetch_rtl_deps.sh`.
- Kiểm tra contention, back-pressure, lock, release và round-robin fairness.
- Lấy mẫu trace cả pre-edge và post-edge, kèm negative control.
- Nếu RTL khác model thì sửa model, không giữ giả định fairness cũ.

### Bước 3: Cross-check router

- Resolve toàn bộ Bender dependency.
- Freeze chính xác RTL parameters tương ứng v0.
- So sánh ready, valid, data, arbitration và FIFO state theo cycle.

### Bước 4: Thêm performance counters

- Chỉ đếm transaction/flit tại handshake `valid && ready`.
- Tách measured counters khỏi analytic estimates.

### Bước 5: Mô hình hóa single-AXI chimney

Thứ tự:

1. AXI channel types.
2. Address-to-destination mapping.
3. Request flit packing.
4. AW/W coupling.
5. Response metadata.
6. Outstanding transaction tracking.
7. Ordering và RoB.

### Bước 6: Hoàn thiện req/rsp mesh

- Tạo network req và rsp riêng.
- Thay abstract endpoint bằng AXI chimney/transactor.
- Kiểm tra end-to-end AXI traffic.

### Bước 7: Tích hợp CDC-VP

- Phân tích topology bus CDC-VP.
- Thiết kế M:N socket ownership.
- Xác định temporal-decoupling policy.
- Thiết kế quiescence detector và gated clock.
- Chạy firmware validation trước khi package SDK.

## 15. Điều kiện hoàn thành vertical slice

Vertical slice chỉ được xem là hoàn thành khi:

- FIFO, route selector, arbiter và router đều có RTL trace comparison;
- mesh req/rsp truyền flit đúng một lần tới đúng endpoint;
- AXI AW/W/AR/B/R chạy end-to-end;
- ordering đúng với cấu hình đã chọn;
- latency/counter đã được cross-check;
- CDC-VP integration đúng vai trò fabric;
- clock chỉ dừng khi toàn network quiescent;
- license và provenance được đóng gói đầy đủ.

## 16. Kết luận

Hạng mục đã hoàn thành nền tảng quan trọng cho mô hình FlooNoC Direction-2:

- xây dựng các block từ type, FIFO, routing, arbitration đến router và mesh;
- xây dựng standalone build và chín SystemC tests, toàn bộ PASS;
- xây dựng flow SystemC ↔ RTL bằng common CSV trace;
- xác nhận XY route selector khớp RTL 12/12 cycle;
- resolve dependency `common_cells` theo revision khóa và verify hash;
- xác nhận input FIFO khớp RTL 133/133 cycle ở depth 2 và depth 4;
- phát hiện và sửa một sai lệch thật của model so với RTL;
- đóng băng source revision và bảo vệ bằng RTL hash;
- tài liệu hóa scope, giới hạn và roadmap.

Bài học quan trọng nhất của giai đoạn này: một giả định "hợp lý" về vi kiến
trúc (FIFO optimal cho phép push khi đầy nếu đang pop) đã sai so với RTL và chỉ
lộ ra khi so sánh theo từng cycle với source gốc. Các block chưa cross-check
phải được xem là chưa đúng, không phải "gần đúng".

Mô hình hiện phù hợp để tiếp tục verification ở cấp arbiter và router. Chưa nên
chuyển sang tích hợp TLM/CDC-VP hoặc công bố latency toàn mạng là cycle-accurate
trước khi các bước RTL cross-check này hoàn thành.

