# Báo cáo tổng hợp xây dựng mô hình FlooNoC SystemC/TLM

## 1. Thông tin chung

- Tên hạng mục: Mô hình FlooNoC mức cycle-approximate bằng SystemC/TLM.
- Thư mục triển khai:
  `/home/duyptt_HW/Desktop/VP_INTER/upgit/CDC-VP/components/floo_noc_model`
- Kiến trúc tham chiếu: chính IP FlooNoC, upstream
  `https://github.com/pulp-platform/FlooNoC.git`, đóng băng tại
  `/home/duyptt_HW/Documents/work/Study_FlooNoC/FlooNoC`
  (đã verify `origin` đúng upstream, `describe` = `v0.8.4-10-g9a6972a`)
- FlooNoC revision cố định: `9a6972a`.
- FlooGen version: `0.8.4`.
- SystemC: `2.3.4`.
- C++ standard: C++17.
- Compiler đã kiểm thử: GCC/G++ `11.5.0`.
- RTL simulator đã kiểm thử: Verilator `5.022`.
- Dependency RTL đã resolve: toàn bộ 13 package theo `Bender.lock`, trong đó
  `common_cells` `1.39.0` @ `9ca8a7655f741e7dd5736669a20a301325194c28`.
- Bender đã cài (pin): `0.32.1` tại `/home/duyptt_HW/.local/bin/bender`.
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

Mọi quyết định mô hình hóa phải truy vết được về một file cụ thể trong cây
FlooNoC đóng băng. Không suy diễn từ tài liệu hay từ mô hình NPU.

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

Hiện tại đã RTL-signed: XY route selector, input FIFO
(`stream_fifo_optimal_wrap`), wormhole arbiter (`floo_wormhole_arbiter` trên
`rr_arb_tree`) và router năm cổng (`floo_router`). Các block còn lại vẫn là
cycle-approximate.

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
- flit header đầy đủ theo `FLOO_TYPEDEF_HDR_T` đóng băng, đúng thứ tự khai báo:
  `rob_req, rob_idx, dst_id, collective_mask, src_id, last, atop, axi_ch,
  collective_op`. Hai trường `collective_mask`/`collective_op` bất hoạt ở v0
  (unicast, `EnMultiCast = 0`) nhưng vẫn mang theo để header là tập con trung
  thực của header thật.
- enum `collect_op` theo `floo_pkg::collect_op_e`.
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

### 6.2b Kiểu AXI và số học sizing flit

File:

```text
include/floo_noc_model/axi_types.hpp
```

Mirror `floo_pkg::axi_cfg_t`, năm kiểu payload AXI channel, và bốn hàm
`axi_chan_mapping`, `get_axi_chan_width`, `get_max_axi_payload_bits`,
`get_axi_rsvd_bits`, dựa trên hằng số field width của `axi_pkg` tại revision
khóa.

Đây là **số học**, không phải timing, nên cross-check đánh giá hai bên trên
cùng một danh sách config thay vì theo cycle. Harness này tồn tại vì hai chi
tiết rất dễ chép sai và sai âm thầm ở mọi chỗ phía sau:

- `get_max_axi_payload_bits` **cộng thêm 1 bit dự phòng** — physical channel
  luôn rộng hơn payload rộng nhất của nó ít nhất 1 bit;
- width các channel dùng `cfg.InIdWidth`, **không bao giờ** dùng `OutIdWidth`.

Cả hai đã được kiểm chứng bằng negative control.

Kết quả: `axi-sizing cross-check PASS: 8 configurations match`.

Một edge case của RTL được model tái tạo nguyên trạng chứ không "làm cho đẹp":
`get_axi_chan_width` dùng `cfg.UserWidth` thô, trong khi
`FLOO_TYPEDEF_AXI_FROM_CFG` khai báo kiểu user là
`logic [floo_iomsb(UserWidth):0]`. Ở `UserWidth == 0` hai cái lệch nhau 1 bit.
Không config nào trong phạm vi hiện tại dùng user width bằng 0.

### 6.2c Chimney: đóng gói flit và giải mã đích — **CHƯA cross-check RTL**

File:

```text
include/floo_noc_model/axi_chimney_pack.hpp
```

Mirror các khối `always_comb` đóng gói flit của `hw/floo_axi_chimney.sv`, luật
`gen_route` trên `hw/floo_id_translation.sv`, và state `aw_w_sel_q`.

Các luật đọc thẳng từ RTL, đều là chỗ model tự viết rất dễ sai:

| Luật | Ý nghĩa |
|---|---|
| AW có `hdr.last = 0`, W có `hdr.last = w.last` | AW và W burst là **một** packet wormhole → giữ chung một route |
| W mang reorder tag **của AW**, không phải của chính nó | W thuộc về transaction của AW |
| AR, B, R đều có `hdr.last = 1` | packet một flit; RTL ghi rõ R burst cố ý không wormhole |
| `hdr.atop = (aw.atop != ATOP_NONE)` | là **cờ**, không phải mã ATOP |
| B và R khôi phục AXI id gốc của manager từ metadata | id phía downstream là id cấp lại nội bộ chimney |
| Đích của W là id đã latch lúc AW được nhận | W không bao giờ tự giải mã địa chỉ |

Giải mã đích có **hai mode** dưới XY routing và cây đóng băng dùng cả hai, nên
model làm cả hai: `UseIdTable = 1` tra system address map (như
`floogen/examples/axi_mesh_xy.yml`); `UseIdTable = 0` trích toạ độ từ trường bit
của địa chỉ (như `hw/test/floo_test_pkg.sv`).

**Trạng thái kiểm chứng: ĐÃ RTL-signed cả hai chiều.**

- Request path: 16 flit khớp chính xác.
- Response path: 8 flit khớp chính xác, với 3 giao dịch outstanding mỗi batch.

Đoạn dưới đây giữ lại vì vẫn đúng về *phạm vi*: phép so là **nội dung flit và
thứ tự**, không phải timing chimney. Phần đóng gói nằm inline `always_comb` bên trong
chimney nên không tách ra được; cross-check thật phải instantiate cả chimney
kèm meta buffer và reorder buffer. Cho tới lúc đó, coi header này là **chưa
kiểm chứng**.

Điều đã xác lập được là harness **khả thi**:
`rtl_crosscheck/axi_chimney/tb_floo_axi_chimney_elab.sv` instantiate chimney
gốc với parameter set của test package upstream và **lint 0 error** trên file
list bender. Phần còn lại là stimulus và tracing, không phải type plumbing.

Không tái dùng được `hw/tb/tb_floo_axi_chimney.sv` của upstream dưới Verilator
vì nó phụ thuộc package class-based `axi_test`. Vẫn dùng được dưới VCS, vốn có
sẵn trên máy này.

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
include/floo_noc_model/rr_arb_tree.hpp
include/floo_noc_model/wormhole_arbiter.hpp
```

**Nguyên nhân gốc của việc viết lại.** Model cũ tự cài round-robin: quét
`valid_i` trực tiếp bắt đầu từ thanh ghi `rr_next_q`, và sau mỗi packet thì
`rr_next_q = selected + 1`. RTL đóng băng không làm như vậy. Nó dùng
`rr_arb_tree` với `ExtPrio = 0`, `AxiVldRdy = 1`, `LockIn = 1`, `FairArb = 1`,
và chỉ được grant bởi `ready_i & last_out`. Bốn điểm sai cụ thể:

| Khía cạnh | Model cũ | RTL đóng băng |
|---|---|---|
| Tăng round-robin | `selected + 1` | `FairArb`: index đang request kế tiếp lớn hơn `rr_q`, tính bằng hai `lzc` trên mask |
| Tập request được arbitrate | `valid_i` trực tiếp | snapshot `valid_q`, được giữ bởi `LockIn` của tree |
| `ready_o` | chỉ khi input được chọn đang valid | đặt lên index được chọn khi có bất kỳ input nào valid |
| `data_o` khi không valid | trả về 0 | luôn lấy `data_i[valid_selected_idx]` |

Đã triển khai (bám đúng phân cấp RTL):

- `rr_arb_tree<NumIn>`: tái tạo cấu trúc cây arbitration, `lzc` với `MODE = 0`,
  và `cf_math_pkg::idx_width`. Là helper không trạng thái; các thanh ghi
  `rr_q`, `lock_q`, `req_q` nằm ở module bao ngoài để dùng được `sc_signal`.
- `wormhole_arbiter<FlitT, NumRoutes>`: snapshot `valid_q` chỉ refresh khi rỗng
  hoặc chu kỳ trước đã nhận flit `last`, `valid_selected_idx`, decode handshake,
  và `last_q`.

Không model data mux và grant decode của tree vì instantiation đóng băng nối
`data_i` bằng `'0` và để hở `data_o`/`gnt_o`. `flush_i` bị tie 0 nên cũng không
model.

`o_selected` và `o_locked` là output debug do model tự định nghĩa, không nằm
trong contract đã sign-off.

Block này đã được cross-check theo từng cycle với RTL gốc: 152 cycle khớp ở cả
5, 4 và 2 route, so cả output lẫn toàn bộ thanh ghi nội bộ.

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

**Sai lệch đã phát hiện và sửa qua cross-check.** RTL tie về `'0` **cả**
handshake **lẫn data** của cặp (input, output) bị cấm:

```systemverilog
assign masked_ready_transposed[in][v][out] = '0;
assign masked_valid[out][v][in]            = '0;
assign masked_data[out][v][in]             = '0;   // model đã bỏ sót dòng này
```

Model cũ chỉ tie handshake, vẫn đưa flit đã route lên mọi nhánh crossbar. Điều
này **quan sát được**, vì `floo_wormhole_arbiter` luôn lái `data_o` từ index
được chọn kể cả khi index đó không valid: output nào có arbiter chọn trúng
nhánh bị cấm sẽ ra `'0` ở RTL nhưng ra dữ liệu cũ ở model.

Hai luật crossbar của model đã được xác nhận map đúng parameter RTL:
`NoLoopback` (default `1'b1`) và `XYRouteOpt` (default `1'b1`).

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
    rr_arb_tree.hpp
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
    arbiter_trace_sc.cpp
    data/
      route_select_stimulus.csv
      route_select_expected.csv
      gen_stream_fifo_stimulus.py
      stream_fifo_stimulus.csv
      stream_fifo_expected_d2.csv
      stream_fifo_expected_d4.csv
      gen_wormhole_arbiter_stimulus.py
      wormhole_arbiter_stimulus.csv
      wormhole_arbiter_expected_n2.csv
      wormhole_arbiter_expected_n4.csv
      wormhole_arbiter_expected_n5.csv
  rtl_crosscheck/
    compare_traces.py
    fetch_rtl_deps.sh
    run_route_select_crosscheck.sh
    run_stream_fifo_crosscheck.sh
    run_wormhole_arbiter_crosscheck.sh
    route_select/
      floo_pkg.sv
      tb_route_select_trace.sv
      shim/common_cells/registers.svh
    stream_fifo/
      tb_stream_fifo_trace.sv
    wormhole_arbiter/
      floo_pkg_empty.sv
      tb_wormhole_arbiter_trace.sv
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
| `test_wormhole_arbiter` | Round-robin, packet lock và chống interleave (2 route) |
| `test_floo_router` | Contention, output stall, FIFO và packet continuity |
| `test_floo_mesh` | Mesh 2×2, multi-hop delivery và stable stall |
| `test_route_trace_sc` | CSV-driven route/lock trace |
| `test_fifo_trace_sc_d2` | 133 cycle FIFO trace so với golden lấy từ RTL, depth 2 |
| `test_fifo_trace_sc_d4` | 133 cycle FIFO trace so với golden lấy từ RTL, depth 4 |
| `test_arbiter_trace_sc_n2` | 152 cycle arbiter trace so với golden lấy từ RTL, 2 route |
| `test_arbiter_trace_sc_n4` | 152 cycle arbiter trace so với golden lấy từ RTL, 4 route |
| `test_arbiter_trace_sc_n5` | 152 cycle arbiter trace so với golden lấy từ RTL, 5 route |
| `test_noc_counters` | Đếm accept/stall/high-water suy tay, identity từng port, bảo toàn flit |
| `test_axi_types` | Width AXI tính tay, mapping channel, reserved bits, độc lập `OutIdWidth` |
| `test_axi_sizing_trace` | Bảng sizing 8 config so với golden lấy từ RTL |
| `test_axi_chimney_pack` | Đóng gói flit từng channel, hai mode giải mã đích, FSM AW/W |
| `test_router_trace_sc` | 214 cycle router trace so với golden lấy từ RTL |

Kết quả chạy ngày 2026-07-28:

```text
100% tests passed, 0 tests failed out of 21
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

### 8.2c Wormhole-arbiter SystemC ↔ RTL cross-check

RTL tham chiếu (không sửa đổi):

```text
FlooNoC/hw/floo_wormhole_arbiter.sv
common_cells/src/cf_math_pkg.sv
common_cells/src/lzc.sv
common_cells/src/rr_arb_tree.sv
```

File local duy nhất trong compile là `floo_pkg_empty.sv` — một package rỗng cố
ý. `floo_wormhole_arbiter.sv` có `import floo_pkg::*;` nhưng không dùng symbol
nào; compile với package rỗng là cách **chứng minh** điều đó thay vì khẳng
định suông. Không dùng `hw/floo_pkg.sv` thật vì nó phụ thuộc `axi_pkg`.

Cấu hình phủ: `NumRoutes` = 5 (cấu hình router, cây không phải lũy thừa 2),
4 (cây nhị phân đầy đủ), 2 (cây một tầng, đúng cấu hình unit test đang dùng).

Các trường được so sánh:

```text
cycle,pre_ready,pre_valid,pre_data,pre_selected,
post_ready,post_valid,post_data,post_selected,
valid_q,last_q,rr_q,lock_q,req_q
```

Trạng thái nội bộ của RTL được lấy qua hierarchical reference, nên phép so sánh
ghim cả state chứ không chỉ output.

Kết quả:

```text
wormhole-arbiter routes 5 cross-check PASS: 152 cycles match
wormhole-arbiter routes 4 cross-check PASS: 152 cycles match
wormhole-arbiter routes 2 cross-check PASS: 152 cycles match
```

Harness đã được kiểm chứng bằng sáu negative control:

| Lỗi cố ý tiêm vào model | Kết quả |
|---|---|
| Round-robin tăng theo `selected + 1` | FAIL tại `rr_q`, cycle 4 |
| `ready_o` bị gate bởi valid của chính input được chọn | FAIL tại `ready_o`, cycle 40 |
| Snapshot không bao giờ refresh theo `last_q` | FAIL từ cycle 5 |
| Gỡ đồng thời cả hai cơ chế hold | FAIL tại selection, cycle 13 |
| Cho tree ăn `valid_i` trực tiếp thay vì snapshot | PASS — xem giải thích dưới |
| Tắt `LockIn` của tree | PASS — xem giải thích dưới |

**Hai cơ chế hold trùng lặp trong RTL.** Hai control cuối PASS vì snapshot
`valid_q` của wrapper và `LockIn` của tree cài đặt cùng một cơ chế giữ packet.
Khi `lock_q` = 0, trạng thái reachable bảo đảm `valid_d == valid_i`; khi
`lock_q` = 1, tree bỏ qua hoàn toàn input request của nó. Mỗi cơ chế đứng một
mình đều tái tạo đúng hành vi RTL; gỡ cả hai thì hỏng — đúng như control thứ
tư cho thấy. Đây là tính chất của RTL, không phải điểm yếu của stimulus, và
không nên "sửa" bằng cách thêm stimulus.

### 8.2d Router SystemC ↔ RTL cross-check

RTL tham chiếu: `FlooNoC/hw/floo_router.sv` không sửa đổi, ở đúng parameter set
đã đóng băng trong `docs/P0_SCOPE.md`.

Harness này **không dùng shim nào cả**. Toàn bộ compile lấy từ file list do
bender sinh, nên `floo_pkg`, `floo_route_select`, `floo_wormhole_arbiter`,
`floo_vc_arbiter` và mọi dependency `common_cells` đều là source thật. Kiểu
flit/header dựng từ macro `floo_noc/typedef.svh` gốc.

Ở parameter set này, RTL rút gọn về đúng cấu trúc của model: nhánh reduction,
đường parallel-reduction trong `floo_output_arbiter`, output FIFO và
`floo_vc_arbiter` đều thoái hoá hết.

Các trường được so sánh:

```text
cycle,pre_ready,pre_valid,pre_d0..pre_d4,
post_ready,post_valid,post_d0..post_d4,
mask0..mask4
```

`maskN` là `route_mask` dạng one-hot — thứ mà router thật sự dùng, thay vì
index. Cross-check route-selector trước đây chỉ so `route_sel_id_o`, nên cột
này đóng nốt khoảng trống đó bằng thực nghiệm.

Kết quả:

```text
floo-router cross-check PASS: 214 cycles match
```

Assertion `StableValidIn`/`StableValidOut` của router **vẫn bật** trong
Verilator (vì `INC_ASSERT` gate theo `SYNTHESIS`, còn file list chỉ define
`TARGET_SYNTHESIS`). Runner grep log và fail nếu chúng nổ — stimulus vi phạm
contract sẽ bị báo là lỗi stimulus chứ không bị bỏ qua. Stimulus hiện tại không
làm assertion nào nổ.

Ba negative control, đều FAIL đúng:

| Lỗi cố ý tiêm vào model | Kết quả |
|---|---|
| Bỏ tie-off data của crossbar | FAIL từ cycle 15 |
| Bỏ luật `XYRouteOpt` (Y→X) | FAIL từ cycle 15 |
| Bỏ luật `NoLoopback` | FAIL từ cycle 15 |

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

### 9.2e Chạy router RTL cross-check

```bash
cd /home/duyptt_HW/Desktop/VP_INTER/upgit/CDC-VP/components/floo_noc_model
./rtl_crosscheck/run_router_crosscheck.sh
```

Runner này tự gọi `gen_rtl_filelist.sh`, nên cần có bender.

### 9.2d Sinh file list RTL đầy đủ bằng Bender

```bash
cd /home/duyptt_HW/Desktop/VP_INTER/upgit/CDC-VP/components/floo_noc_model
./rtl_crosscheck/gen_rtl_filelist.sh
```

Script chỉ chạy khi: bender đúng version pin, FlooNoC đúng revision đóng băng
và sạch, `Bender.lock` khớp hash trước **và** sau khi chạy. Nó chỉ gọi
`bender checkout`, không bao giờ gọi `bender update`.

Output mặc định tại `/tmp/floo_noc_rtl_filelist`: `floo_verilator.f`,
`floo_vcs.sh`, `floo_flist_plus.f`, `resolved_deps.txt`.

Nếu chưa có bender, script in sẵn lệnh cài từ artifact đã pin. **Không** dùng
installer `https://pulp-platform.github.io/bender/init`: từ v0.32.0 nó là
wrapper cargo-dist, cài vào `$CARGO_HOME/bin` và sửa file shell profile.

### 9.2c Chạy wormhole-arbiter RTL cross-check

```bash
cd /home/duyptt_HW/Desktop/VP_INTER/upgit/CDC-VP/components/floo_noc_model
./rtl_crosscheck/run_wormhole_arbiter_crosscheck.sh
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

Toàn bộ dependency tree đã được resolve. Có hai đường, và chúng kiểm chứng
lẫn nhau:

**Đường leaf** — `rtl_crosscheck/fetch_rtl_deps.sh`: pin từng repo theo
revision và SHA-256 từng file, không cần Bender. Đây là đường mà cross-check
FIFO và arbiter đang dùng: nhanh, offline sau lần đầu, có hash guard.

**Đường đầy đủ** — `rtl_crosscheck/gen_rtl_filelist.sh`: chạy Bender 0.32.1
trên `Bender.lock` đóng băng, resolve toàn bộ 13 package và sinh file list có
thứ tự. Script assert rằng revision `common_cells` mà Bender resolve **trùng**
với pin độc lập của đường leaf — và thực tế đã trùng, kể cả hash từng file.

Bảng dependency đã resolve:

| Package | Version | Revision |
|---|---|---|
| apb | 0.2.4 | `77ddf07` |
| axi | 0.39.9 | `a256a3b` |
| axi_riscv_atomics | 0.8.3 | `97a1dd2` |
| axi_stream | 0.1.1 | `54891ff` |
| common_cells | 1.39.0 | `9ca8a76` |
| common_verification | 0.2.5 | `fb1885f` |
| fpnew | — | `e5aa6a0` |
| fpu_div_sqrt_mvp | 1.0.4 | `86e1f55` |
| idma | 0.6.5 | `28a36e5` |
| obi | 0.1.7 | `0155fc3` |
| register_interface | 0.4.7 | `d6e1d4c` |
| tech_cells_generic | 0.2.13 | `7968dd6` |
| floo_noc_pd | path `./pd` | — |

Hai điểm cần nhớ:

- `Bender.lock` **có** được git track trong repo FlooNoC. Dòng `Bender.lock`
  trong `.gitignore` của repo đó vô hiệu vì file đã được commit, nên thay đổi
  lock vẫn hiện trong `git status`.
- Chỉ được chạy `bender checkout` trên cây đóng băng. `bender update` sẽ
  re-resolve và ghi đè lock.

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

- Hai mươi mốt SystemC test đều PASS.
- Route-selector khớp RTL 12/12 cycle.
- Input FIFO khớp RTL 133/133 cycle ở cả depth 2 và depth 4.
- Wormhole arbiter khớp RTL 152/152 cycle ở 5, 4 và 2 route, gồm cả state.
- Router năm cổng khớp RTL 214/214 cycle, gồm cả one-hot route mask.
- Đã phát hiện và sửa ba nhóm sai lệch thật giữa model và RTL: FIFO
  accept-at-full; bốn điểm sai của arbiter (round-robin, tập request,
  `ready_o`, `data_o`); và thiếu tie-off data trên crossbar của router.
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

- router cycle-equivalent ở cấu hình khác v0 (VC, credit, output FIFO,
  collective, multicast, reduction đều chưa model và chưa so);
- FIFO cycle-equivalent ở depth khác 2 và 4, hoặc trên `usage_o`/`flush_i`;
- arbiter cycle-equivalent ở `NumRoutes` khác 2, 4 và 5;
- mesh có latency theo đúng generated topology;
- current mesh là full single-AXI network;
- req và rsp đã được tách thành hai physical network;
- AXI ordering đã đúng;
- CDC-VP traffic đã đi qua FlooNoC;
- performance counters đã phản ánh RTL.

Việc router đã RTL-signed **không** làm mesh trở thành cycle-equivalent: link
giữa các router hiện vẫn là kết nối tổ hợp, và topology sinh bởi FlooGen chưa
có sẵn để đối chiếu.

Vì vậy kết luận chính xác hiện tại là:

> Mô hình đã hoàn thành nền tảng router/mesh SystemC. XY route selector, input
> FIFO, wormhole arbiter và router năm cổng đã được xác nhận khớp RTL theo
> cycle ở cấu hình v0. Mesh, link và latency toàn mạng vẫn ở mức
> cycle-approximate.

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

### 13.1b Kiểu dữ liệu chưa khớp header thật

Audit ngày 2026-07-28 đối chiếu `flit_header` của model với
`FLOO_TYPEDEF_HDR_T` trong `hw/include/floo_noc/typedef.svh`:

```systemverilog
rob_req, rob_idx, dst_id, collective_mask, src_id, last, atop, axi_ch, collective_op
```

Model **thiếu** `collective_mask` và `collective_op`. Trong v0 (Unicast,
`EnMultiCast = 0`) điều này không gây sai hành vi — nên không test nào fail —
nhưng đây không phải chỉ là khác width mà là thiếu hẳn hai trường, và
`hw/floo_route_select.sv` ở revision này **có** đọc `hdr.collective_op`. Phải
bổ sung trước khi động tới multicast, collective hoặc reduction.

Những gì đã verify là khớp chính xác: `direction` ↔ `route_direction_e`,
`axi_channel` ↔ `axi_ch_e` (kể cả width 3 bit), và shim `floo_pkg` của
route-select harness ↔ `hw/floo_pkg.sv`.

Giới hạn coverage trước đây chưa nêu: cross-check route-selector chạy với id
type 2-bit `x`/`y`, nên chỉ sign-off được toạ độ 0..3.

### 13.2 Network chưa hoàn chỉnh

- Mesh hiện chỉ chở một generic `FlitT` stream.
- `req` và `rsp` mới là type identifier.
- Chưa có hai network instance độc lập.
- Chưa có output FIFO.
- Chưa có explicit link latency.
- Chưa có virtual channel và credit.

### 13.3 Instrumentation

**Đã có** (`include/floo_noc_model/noc_counters.hpp`), chỉ quan sát tín hiệu đã
RTL-signed ở biên router:

- accepted flits và packets, từng input và từng output;
- stall cycles và busy cycles từng port;
- occupancy high-water và tổng occupancy của input buffer.

Hai nguyên tắc:

- **Phạm vi:** counter chỉ được quan sát tín hiệu đã qua cross-check. Không có
  counter nào cho link hay latency end-to-end vì mesh vẫn cycle-approximate.
- **Thụ động:** block chỉ khai báo `sc_in`, không lái gì. Đây là điều được
  **kiểm chứng** chứ không phải tuyên bố suông: `router_trace_sc.cpp` gắn
  counters cạnh router, nên cross-check 214 cycle chạy *có* counters và vẫn
  khớp RTL chính xác.

Ba tầng báo cáo tách bạch: **measured** (đếm tại `valid && ready`), **derived**
(số học trên measured, ví dụ utilisation), **analytic** (không cung cấp).

Lưu ý quan trọng: **bảo toàn flit chỉ đúng trong cửa sổ không có reset và đã
drain hết.** Reset hủy flit đang nằm trong buffer. Stimulus router 214 cycle có
hai lần reset nên hiển thị 179 flit vào / 165 flit ra — đúng, không phải lỗi.

**Chưa có**, và cố ý, vì mỗi cái cần một đường chưa RTL-signed hoặc chưa model:

- transaction/flit latency và hop count: cần tag từng flit và mesh đã signed;
- payload bytes per cycle: cần kiểu AXI payload thật;
- link utilization: link là khái niệm của mesh, mà mesh chưa signed;
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

### Bước 2: Cross-check wormhole arbiter — ĐÃ XONG (2026-07-28)

- Đã compile `floo_wormhole_arbiter.sv` gốc trên `rr_arb_tree`/`lzc`/
  `cf_math_pkg` đúng revision khóa, có hash guard cho từng file.
- Đã phát hiện model sai ở bốn điểm và viết lại theo cấu trúc RTL.
- 152 cycle khớp ở 5, 4 và 2 route, so cả output lẫn state nội bộ.
- Đã kiểm chứng harness bằng sáu negative control.

Phần arbiter còn thiếu: `NumRoutes` ngoài 2/4/5; data mux và grant decode của
tree không được model vì instantiation đóng băng để hở chúng.

### Bước 3: Resolve compile flow RTL đầy đủ — ĐÃ XONG (2026-07-28)

- Đã cài Bender 0.32.1 từ artifact pin, không chạy installer cargo-dist và
  không sửa shell profile nào.
- `bender checkout` resolve đủ 13 package từ `Bender.lock` đóng băng. Không
  chạy `bender update`.
- `Bender.lock` giống hệt trước và sau; cây FlooNoC vẫn sạch.
- Revision và hash `common_cells` mà Bender resolve **trùng** với pin độc lập
  trong `fetch_rtl_deps.sh` — tức các cross-check trước đó đã compile đúng
  nguồn mà Bender resolve.
- Đã sinh `floo_verilator.f`, `floo_vcs.sh`, `floo_flist_plus.f`,
  `resolved_deps.txt`.
- `floo_router.sv` elaborate **0 error** từ file list sinh ra, Verilator 5.022.
- Đã test guard: cây bẩn bị chặn, `Bender.lock` bị sửa bị chặn, và backstop
  hash lock có kích hoạt khi test riêng.

### Bước 3b: Cross-check router — ĐÃ XONG (2026-07-28)

- Parameter set v0 được **suy ra từ RTL**, không đoán, và đã ghi vào
  `docs/P0_SCOPE.md` kèm giải thích từng parameter rút gọn RTL về cái gì.
- Harness compile RTL thật từ file list bender, không shim, type dựng từ macro
  `typedef.svh` gốc.
- Tìm và sửa một sai lệch thật: thiếu tie-off data trên crossbar.
- 214 cycle khớp, gồm cả one-hot route mask.
- Assertion của router vẫn bật và không nổ; runner fail nếu chúng nổ.
- Đã kiểm chứng harness bằng ba negative control.

Phần router còn thiếu: chỉ sign-off `NumRoutes=5`, `NumVirtChannels=1`,
`InFifoDepth=2`, `OutFifoDepth=0`. Virtual channel, credit, output FIFO,
collective, multicast, reduction đều chưa model và chưa so.

### Bước 4: Thêm performance counters — ĐÃ XONG (2026-07-28)

- Chỉ đếm tại `valid && ready`, chỉ trên tín hiệu đã RTL-signed ở biên router.
- Block thụ động hoàn toàn; đã **kiểm chứng** bằng cách gắn vào router trace
  runner và chạy lại cross-check: vẫn khớp 214/214.
- Test pin số tuyệt đối suy tay từ spill register: drain liên tục thì nhận 1
  flit/cycle và không back-pressure; bị stall thì nhận đúng 2 rồi từ chối 3.
- Tách bạch measured / derived / analytic; không phát hành analytic nào.

### Bước 5: Mô hình hóa single-AXI chimney (bước kế tiếp)

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
- xây dựng standalone build và mười hai SystemC tests, toàn bộ PASS;
- xây dựng flow SystemC ↔ RTL bằng common CSV trace;
- xác nhận XY route selector khớp RTL 12/12 cycle;
- resolve dependency `common_cells` theo revision khóa và verify hash;
- resolve toàn bộ dependency tree bằng Bender và sinh file list tái lập được;
- xác nhận `floo_router.sv` elaborate sạch từ file list đó;
- xác nhận input FIFO khớp RTL 133/133 cycle ở depth 2 và depth 4;
- xác nhận wormhole arbiter khớp RTL 152/152 cycle ở 5, 4 và 2 route;
- xác nhận router năm cổng khớp RTL 214/214 cycle ở cấu hình v0;
- thêm measured counters thụ động, đã chứng minh không làm nhiễu datapath;
- phát hiện và sửa hai nhóm sai lệch thật của model so với RTL;
- đóng băng source revision và bảo vệ bằng RTL hash;
- tài liệu hóa scope, giới hạn và roadmap.

Bài học quan trọng nhất của giai đoạn này: một giả định "hợp lý" về vi kiến
trúc (FIFO optimal cho phép push khi đầy nếu đang pop) đã sai so với RTL và chỉ
lộ ra khi so sánh theo từng cycle với source gốc. Các block chưa cross-check
phải được xem là chưa đúng, không phải "gần đúng".

Mô hình hiện phù hợp để tiến sang AXI chimney. Chưa nên
chuyển sang tích hợp TLM/CDC-VP hoặc công bố latency toàn mạng là cycle-accurate
trước khi các bước RTL cross-check này hoàn thành.

