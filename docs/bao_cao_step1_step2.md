# Báo cáo Step 1, Step 2 & Step 3 — Virtual Platform open-source (cdc-vp)

---

## 0. Bối cảnh & mục tiêu dự án

Mục tiêu của dự án là xây một **Virtual Platform (VP)** — tức một "máy ảo" mô phỏng phần cứng
bằng phần mềm — để thay thế Arm FVP/Fast Models, hoàn toàn dựa trên **open-source**. VP này cho
phép lập trình viên chạy và kiểm thử firmware/phần mềm **trước khi có chip thật**, và mô hình hoá
các IP tự thiết kế của team.

Nền tảng kỹ thuật là **SystemC/TLM-2.0** — một thư viện C++ chuẩn công nghiệp để mô tả phần cứng
và cách các khối "nói chuyện" với nhau qua bus bằng các "giao dịch" (transaction) đọc/ghi.

Lộ trình chia thành nhiều bước nhỏ. Báo cáo này trình bày **Step 1** (dựng backbone TLM),
**Step 2** (đưa CPU RISC-V thật vào), và **Step 3** (dựng custom SoC có CLINT/PLIC).

---

## 1. STEP 1 — Dựng backbone TLM-2.0

### 1.1 Mục tiêu
Chứng minh "xương sống" TLM-2.0 hoạt động — tức bus, bộ nhớ, ngoại vi giao tiếp đúng — **trước
khi** đưa một CPU model phức tạp vào. Ở bước này CPU chỉ là một "CPU giả" (cpu_stub) sinh ra các
giao dịch theo kịch bản.

### 1.2  đã làm gì
Xây dựng nền tảng và tách thành **các linh kiện (component) tái dùng được**, mỗi cái là một
thư viện độc lập trong thư mục `components/`:

- **bus_router** — bộ định tuyến bus: nhận giao dịch từ CPU, nhìn địa chỉ rồi chuyển đúng tới
  UART / Timer / RAM. Nó tự "dịch" địa chỉ toàn cục về địa chỉ nội bộ của từng thiết bị, nên mỗi
  thiết bị không cần biết mình nằm ở đâu trong bản đồ bộ nhớ.
- **memory_tlm** — mô hình bộ nhớ RAM/ROM, hỗ trợ đọc/ghi, truy cập debug (nạp ảnh) và DMI.
- **uart_tlm** — cổng UART, CPU ghi một byte vào nó thì in ký tự ra màn hình (đây là cách firmware
  "printf").
- **timer_tlm** — bộ đếm thời gian, sau một khoảng thời gian sẽ phát tín hiệu ngắt (IRQ).
- **common** — thư viện tiện ích dùng chung (kiểu địa chỉ, log).

ráp các component này lại thành một platform tên **mini_tlm**, nối dây qua một cpu_stub. Sau đó
 làm thêm một platform thứ hai, **mini_soc**, trong đó  **port thử một IP thật từ Fast Models
của team sang** — đó là model I2C — để kiểm chứng quy trình đưa IP vào VP.

### 1.3 Kết quả chạy được
- `mini_tlm`: in ra "Hello from mini VP" qua UART, và Timer phát IRQ sau 10ms, CPU bắt được.
- `mini_soc`: chạy được tới mức firmware **kích ngắt I2C qua trình tự ghi register**, handler chạy.
- Bus đọc/ghi register và truy cập RAM hoạt động chính xác.

### 1.4 Chất lượng & đóng gói
- **Kiểm thử 2 tầng**: unit test C++ cho từng component (chạy bằng ctest), và system test bằng
  Python/pytest chạy cả platform như thật — tất cả đều pass.
- **Đóng gói**:  làm cơ chế xuất ra một thư mục "copy là chạy" (binary + thư viện đi kèm), giống
  hệt cách Fast Models của Arm sinh ra `isim_system`. Ngoài ra còn cơ chế cài component thành thư
  viện dùng chung để build platform độc lập — tương đương workflow `.sgproj`/simgen của Fast Models.

### 1.5 Đối chiếu với Fast Models (để dễ hình dung)
cdc-vp và Fast Models cùng một kiến trúc, chỉ khác cú pháp:

| Fast Models | cdc-vp |
|---|---|
| File `.lisa` (composition + connection) | file C++ top của platform |
| `.sgproj` (manifest build) | `CMakeLists.txt` |
| `PVBusDecoder` | bus_router |
| `RAMDevice` | memory_tlm |
| Wrapper + IP model | component (wrapper + logic) |

Bảng ánh xạ chi tiết nằm ở `docs/architecture.md`.

---

## 2. STEP 2 — Đưa CPU RISC-V thật vào

### 2.1 Mục tiêu
Thay "CPU stub" bằng **CPU RISC-V thật**, chạy được firmware thật và theo yêu cầu của spec, phải
**chọn được một CPU chính + một CPU dự phòng dựa trên số liệu đo thật**.

### 2.2 Lớp trừu tượng cpu_base
 định nghĩa một interface chung tên **cpu_base**: mọi CPU model đều phải tuân theo interface này
(nạp firmware, lộ ra bus, đọc thanh ghi PC...). Nhờ đó platform **không phụ thuộc vào một CPU cụ
thể** — đổi CPU chỉ bằng một cờ CMake, không sửa code platform.

### 2.3 CPU thật thứ nhất — mariusmm RISC-V-TLM
 tích hợp model **mariusmm RISC-V-TLM** (RV32, single-core) làm CPU đầu tiên, theo đúng chiến
lược "submodule + thin wrapper" của spec: lấy nguyên model gốc làm thư viện, viết một lớp wrapper
mỏng để khớp với cpu_base.

 viết một bộ **ELF loader** để nạp firmware vào bộ nhớ, một platform mới **riscv_cpu_eval**, và
một firmware bare-metal "hello". Kết quả: **một CPU RISC-V thật fetch và thực thi firmware từ bộ
nhớ của ta, in "Hello from RISC-V" ra UART** — cpu_stub chính thức được thay bằng CPU thật.

### 2.4 Gate 2 — CPU thật nhận ngắt
 làm tiếp để chứng minh CPU thật **xử lý được ngắt**: timer của ta phát tín hiệu → một "cầu nối"
trong platform tiêm ngắt vào CPU → CPU lưu trạng thái, nhảy vào **trap handler**, in "TIMER IRQ",
rồi dùng lệnh `mret` quay về. Đây là luồng ngắt hoàn chỉnh trên lõi thật.

Trong lúc làm, phát hiện một **bug của model mariusmm**: lệnh CSRRS bị bỏ qua khi rd = x0, làm
firmware không bật được ngắt.  đã tìm ra nguyên nhân và xử lý phía firmware mà không phải sửa
mã upstream.

### 2.5 CPU thật thứ hai — Bremen riscv-vp
Để có CPU dự phòng và để chứng minh lớp cpu_base thật sự tổng quát,  tích hợp thêm **Bremen
riscv-vp** làm backend thứ hai. Bremen mạnh hơn (hỗ trợ MMU, có khả năng boot Linux) nhưng nặng
hơn (cần thư viện softfloat, core-common, boost).

Kết quả quan trọng: **cùng một file hello.elf chạy được trên CẢ HAI CPU** (mariusmm và Bremen),
chỉ bằng đổi một cờ CMake. Điều này chứng minh lớp trừu tượng cpu_base hoạt động đúng.

### 2.6 Benchmark quantum-keeper
 viết một firmware tính toán và một chế độ đo, chạy một khoảng thời gian mô phỏng cố định rồi đo
**thời gian thực (host wall-clock)** và **số lệnh thực thi**, suy ra tốc độ mô phỏng (MIPS).  đo
ảnh hưởng của **quantum-keeper** — một kỹ thuật "gộp thời gian" để chạy nhanh hơn:

| CPU | Quantum | Tốc độ mô phỏng (MIPS) |
|---|---|---:|
| mariusmm | (mô hình cố định) | 4.1 |
| Bremen | sync mỗi cycle | 4.8 |
| Bremen | 1ms | 7.9 |
| Bremen | 10ms | 8.2 |

Kết luận: **quantum-keeper có tác dụng rõ trên Bremen** (nhanh ~1.7 lần khi tăng quantum), và
Bremen nhanh gấp ~2 lần mariusmm. (Mức tăng còn khiêm tốn vì hiện  chưa bật DMI bật DMI sẽ nhanh
hơn nhiều — đây là việc để dành.)

### 2.7 Quyết định chọn CPU
Dựa trên số liệu,  đề xuất:
- **CPU chính: Bremen riscv-vp** — vì nhanh hơn, có MMU, có khả năng boot Linux, có quantum-keeper
  điều chỉnh được lúc chạy (đúng hướng roadmap).
- **CPU dự phòng: mariusmm RISC-V-TLM** — nhỏ gọn, dễ đọc/sửa, hợp để bring-up nhanh và làm tham
  chiếu.

Hai tài liệu kết quả: `docs/cpu_benchmark_results.md` (số liệu) và
`docs/cpu_integration_strategy.md` (quyết định).

---

## 3. STEP 3 — Custom SoC với CLINT/PLIC

### 3.1 Mục tiêu
Dựng một SoC RISC-V hoàn chỉnh hơn thay vì chỉ có platform đánh giá CPU. Ở bước này platform phải
có subsystem ngắt do team tự control, gồm ngắt timer/software local qua **CLINT** và ngắt external
từ peripheral qua **PLIC**. Platform vẫn phải backend-agnostic: chạy được trên Bremen là CPU chính,
và mariusmm là CPU dự phòng.

### 3.2 Mở rộng interface CPU
 bổ sung vào `cpu_base` hàm **set_irq(cause, level)**. Đây là interface chung để interrupt
controller không cần biết CPU cụ thể là Bremen hay mariusmm.

- `cause = 3`: machine software interrupt (MSIP).
- `cause = 7`: machine timer interrupt (MTIP).
- `cause = 11`: machine external interrupt (MEIP).

Wrapper Bremen map level interrupt này vào ISS thật:
`trigger_software_interrupt`, `trigger_timer_interrupt`, và
`trigger_external_interrupt / clear_external_interrupt`.

Wrapper mariusmm không có level IRQ đầy đủ, nên được xử lý theo mô hình edge: khi `level=true` thì
inject cause qua `irq_line_socket`; khi `level=false` thì bỏ qua. Đây là approximation đã được
document rõ trong interrupt policy.

### 3.3 Component CLINT
 xây `components/clint_tlm` theo map RISC-V chuẩn:

- `msip` tại offset `0x0000` để phát software interrupt.
- `mtimecmp` tại offset `0x4000` để đặt thời điểm timer interrupt.
- `mtime` tại offset `0xBFF8`, lấy từ simulation time.

CLINT gọi `cpu_base::set_irq(3, level)` cho MSIP và `set_irq(7, level)` cho MTIP. Component này có
unit test riêng để kiểm tra timer assert và software interrupt.

### 3.4 Component PLIC
 xây `components/plic_tlm` dạng basic PLIC, single hart / M-mode:

- priority cho từng source.
- pending bitfield.
- enable bitfield.
- threshold.
- claim/complete.

PLIC nhận các line external interrupt từ peripheral, chọn source hợp lệ có priority cao hơn
threshold, rồi gọi `cpu_base::set_irq(11, level)` để assert/deassert MEIP vào CPU.

Trong scope hiện tại, PLIC dùng level-sensitive gateway cơ bản: sau khi firmware claim một source,
PLIC không raise lại source đó cho đến khi firmware complete. Component này cũng có unit test riêng.

### 3.5 Platform riscv_custom_soc
 tạo platform mới **`platforms/riscv_custom_soc`**, gồm:

| Khối | Base | Vai trò |
|---|---:|---|
| RAM | `0x8000_0000` | firmware text/data/stack |
| UART | `0x1000_0000` | console |
| CLINT | `0x0200_0000` | MSIP + MTIP |
| PLIC | `0x0C00_0000` | MEIP từ peripheral |
| I2C | `0x1001_0000` | source external interrupt id 1 |

CPU vẫn được chọn bằng CMake flag `CDC_CPU_BACKEND`. Nếu backend là Bremen thì platform bind một
unified bus; nếu là mariusmm thì bind split instruction/data bus. Điều này giữ nguyên mục tiêu:
**platform không phụ thuộc vào CPU concrete class**.

### 3.6 Firmware kiểm chứng TIMER + EXT IRQ
 thêm firmware **`fw/soc_irq_riscv`**. Firmware thực hiện:

1. Set `mtvec` tới trap handler.
2. Cấu hình PLIC source 1: priority, enable, threshold.
3. Enable `mie.MTIE`, `mie.MEIE`, và `mstatus.MIE`.
4. Arm CLINT timer bằng cách ghi `mtimecmp = mtime + 100us`.
5. Trigger I2C để kéo `irq_out` lên, đi qua PLIC thành MEIP.
6. Trap handler xử lý:
   - MEIP: claim PLIC, clear I2C interrupt source, in `"EXT IRQ"`, complete.
   - MTIP: in `"TIMER"`, disable MTIE để không lặp.
7. Khi nhận đủ hai interrupt thì in `"done"`.

Output pass trên Bremen:

```text
Hello from custom SoC
EXT IRQ
TIMER
done
```

### 3.7 Interrupt modeling policy
 viết `docs/interrupt_modeling_policy.md` để chốt ownership:

- CLINT sở hữu MSIP/MTIP.
- PLIC sở hữu MEIP từ peripheral.
- Không instantiate duplicate CLINT/PLIC trên cùng interrupt path.
- Bremen là level-accurate; mariusmm là edge approximation.
- Scope hiện tại là single hart / M-mode; full PLIC multi-context, delegation S/U-mode và MSIP demo
  để deferred.

---

## 4. Tổng kết & trạng thái

**Step 1 — hoàn thành:**
- Backbone TLM-2.0 với 4 khối TLM + 1 thư viện chung, tái dùng được.
- 2 platform (mini_tlm, mini_soc) chạy được port thử thành công 1 IP (I2C) từ Fast Models.
- Test 2 tầng đầy đủ cơ chế đóng gói "copy là chạy".

**Step 2 — hoàn thành:**
- Lớp trừu tượng cpu_base tích hợp **2 CPU RISC-V thật** (mariusmm + Bremen), cùng firmware chạy
  trên cả hai.
- CPU thật chạy hello + nhận/xử lý ngắt (Gate 2).
- Benchmark quantum-keeper có số liệu quyết định CPU chính/dự phòng đã ký.

**Step 3 — hoàn thành:**
- `cpu_base::set_irq(cause, level)` đã là interrupt input chung cho CPU wrapper.
- `components/clint_tlm` và `components/plic_tlm` đã có unit test.
- `riscv_custom_soc` chạy được trên Bremen primary và mariusmm backup.
- Firmware `fw/soc_irq_riscv` kiểm chứng đủ hai luồng: CLINT timer interrupt và PLIC external
  interrupt từ I2C.
- `docs/interrupt_modeling_policy.md` đã chốt policy CLINT/PLIC.

**Kiểm thử tổng:** unit test 7/7, system test (Python) 5/5, Bremen custom SoC TIMER+EXT IRQ pass,
không hồi quy.

**Bước tiếp theo (Step 4):** bring-up RTOS trước (Zephyr/FreeRTOS hello + thread + IRQ), sau đó
minimal Linux trên Bremen theo timebox. Các việc deferred quan trọng: DMI forwarding qua
`bus_router`, YAML config parsing, MSIP software-interrupt demo, và PLIC đầy đủ multi-context.

---

*Tài liệu liên quan: `docs/architecture.md` (kiến trúc & ánh xạ Fast Models),
`docs/cpu_benchmark_results.md`, `docs/cpu_integration_strategy.md`,
`docs/interrupt_modeling_policy.md`.*
