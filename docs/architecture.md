# Architecture & Fast Models ↔ cdc-vp mapping

Tài liệu này giải thích kiến trúc của cdc-vp (SystemC/TLM-2.0) và đối chiếu 1-1 với
Arm Fast Models (LISA/FVP) để team quen Fast Models tra cứu nhanh.

Ví dụ tham chiếu phía Fast Models lấy từ project Corstone-320 nội bộ:
- Top LISA: `FVP_FX1_SSE_320.lisa`
- Wrapper LISA: `FastModels_Wrapper/I2C/Wrapper_I2C.lisa`
- IP logic thuần: `TLM_Corstone320/IP/i2c/{i2c.h,i2c.cpp}`

Ví dụ phía cdc-vp lấy từ Step 1: [`platforms/mini_tlm`](../platforms/mini_tlm) +
[`components/`](../components).

---

## 1. Ba tầng kiến trúc (giống nhau ở cả hai)

| Tầng | Fast Models | cdc-vp |
|---|---|---|
| **Top / System** | `component ... { component_type = "System" }` trong top `.lisa` | `struct <platform>_top::impl : sc_module` (vd `mini_tlm_top.cpp`) |
| **Wrapper (adapter)** | `Wrapper_X.lisa`: `PVBusSlave` + `internal port<PVDevice>` | phần "glue": `simple_target_socket` + `register_b_transport` trong `components/<x>_tlm/src/*.cpp` |
| **IP logic thuần** | `class X_Model` (C++ thuần, vd `I2C_Model`) | logic register bên trong `b_transport` (hoặc tách riêng nếu là model bên thứ 3) |

> Arm **tách** wrapper (LISA) khỏi core logic (C++ thuần). cdc-vp **gộp** wrapper + logic
> vào một component cho gọn. Tinh thần "thin wrapper + model bên thứ 3" của Arm chính là
> chiến lược spec áp dụng cho CPU (Bremen RISC-V VP): xem `cpu_models/include/cdc/cpu/cpu_base.h`.

---

## 2. Khái niệm cốt lõi — bảng từ điển

| Khái niệm | Fast Models (PVBus) | cdc-vp (TLM-2.0) |
|---|---|---|
| Bus protocol | PVBus | TLM-2.0 generic payload |
| Đầu phát giao dịch | `master port<PVBus> pvbus_m` | `tlm_utils::simple_initiator_socket` |
| Đầu nhận giao dịch | `slave port<PVBus> pvbus_s` | `tlm_utils::simple_target_socket` |
| Nối dây bus | `A.master => B.slave;` | `A.bind(B);` |
| Bộ giải mã địa chỉ | `PVBusDecoder` | [`bus_router`](../components/bus_router) |
| Bộ nhớ | `RAMDevice()` | [`memory_tlm`](../components/memory_tlm) |
| Giao dịch | `pv::ReadTransaction` / `WriteTransaction` | `tlm::tlm_generic_payload` |
| Handler đọc | `behavior read(tx)` | `b_transport` + `TLM_READ_COMMAND` |
| Handler ghi | `behavior write(tx)` | `b_transport` + `TLM_WRITE_COMMAND` |
| Truy cập debug (không tốn thời gian) | `behavior debugRead/debugWrite` | `transport_dbg` |
| Tín hiệu 1 bit (ngắt/reset) | `port<Signal>` | `sc_in<bool>` / `sc_out<bool>` + `sc_signal<bool>` |
| Khởi tạo | `behavior init()` | constructor |
| Reset | `behavior reset(int level)` | (sẽ là `reset_cpu()` / `sc_in reset`) |
| Hủy | `behavior terminate()` | destructor |

---

## 3. `composition` ↔ khai báo member

```lisa
// Fast Models — top .lisa
composition {
    cpu0      : ARMCortexM55CT();
    uart0     : PL011_Uart();
    timer_clk : SP804_Timer();
    ext_dram  : RAMDevice();
    axi_inner : PVBusDecoder();
}
```

```cpp
// cdc-vp — mini_tlm_top.cpp, struct impl
cpu_stub                   cpu;    // ARMCortexM55CT()
cdc::components::bus_router bus;    // PVBusDecoder()
cdc::components::uart_tlm   uart;   // PL011_Uart()
cdc::components::timer_tlm  timer;  // SP804_Timer()
cdc::components::memory_tlm ram;    // RAMDevice()
```

---

## 4. `connection` (1) — bus master

```lisa
cpu0.pvbus_m => axi_top.pvbus_s;        // master => slave
```
```cpp
cpu.bus_socket.bind(bus.target_socket); // initiator -> target
```

`=>` ⟺ `.bind()`. `pvbus_m`/`pvbus_s` (master/slave) ⟺ `initiator`/`target` socket.

---

## 5. `connection` (2) — memory map

```lisa
// Fast Models — cú pháp [cận dưới .. cận trên]
axi_inner.pvbus_m_range[0x40100000..0x40100FFF] => uart0.pvbus;     // UART  4KB
ahb_inner.pvbus_m_range[0x40000000..0x40000FFF] => timer_clk.pvbus; // Timer 4KB
axi_inner.pvbus_m_range[0x80000000..0x8FFFFFFF] => ext_dram.pvbus;  // DRAM  256MB
```

```cpp
// cdc-vp — cú pháp (base, size)
bus.add_target(0x10000000, 0x1000).bind(uart.socket);   // UART
bus.add_target(0x10001000, 0x1000).bind(timer.socket);  // Timer
bus.add_target(0x80000000, 0x1000).bind(ram.socket);    // RAM
```

| | Fast Models | cdc-vp |
|---|---|---|
| Khai 1 vùng | `decoder.pvbus_m_range[lo..hi] => ip.pvbus` | `bus.add_target(base, size).bind(ip.socket)` |
| Biểu diễn vùng | cận dưới–cận trên | base + size (`hi = base + size - 1`) |
| Dịch địa chỉ về offset nội bộ | PVBusDecoder tự làm | `bus_router::b_transport` tự trừ base |

Cả hai đều **dịch địa chỉ** về offset nội bộ trước khi vào IP → IP không cần biết base của
chính nó → tái dùng được ở bất kỳ địa chỉ nào.

### Bus phân cấp (multi-tầng)

Fast Models lồng nhiều `PVBusDecoder`:
```lisa
axi_top.pvbus_m_range[0x00000000..0xFFFFFFFF]   => axi_inner.pvbus_s;
axi_inner.pvbus_m_range[0x40000000..0x5FFFFFFF] => ahb_inner.pvbus_s;
```
cdc-vp làm tương tự bằng cách bind 1 target của bus cha vào `target_socket` của bus con:
```cpp
bus_top.add_target(0x40000000, 0x20000000).bind(bus_ahb.target_socket);
```
(Sẽ dùng ở Step 3 — custom SoC.)

---

## 6. `connection` (3) — IRQ map

```lisa
uart0.intr         => cpu0.irq[0];
timer_clk.irq_out0 => cpu0.irq[8];
```
```cpp
// cdc-vp: cần 1 sc_signal làm "dây" trung gian
sc_core::sc_signal<bool> timer_irq;
timer.irq_out(timer_irq);   // IP.intr  -> dây
cpu.timer_irq(timer_irq);   // dây -> CPU.irq
```

| Fast Models | cdc-vp |
|---|---|
| `master port<Signal> intr` | `sc_out<bool> irq_out` |
| `cpu0.irq[N]` | `sc_in<bool>` ở CPU |
| `ip.intr => cpu0.irq[N]` (nối thẳng, đánh số) | nối qua `sc_signal<bool>` trung gian |

Khi nhiều nguồn IRQ (bảng `cpu0.irq[0..62]` trong LISA Corstone-320) → cdc-vp sẽ cần
`irq_aggregator` / PLIC / CLINT (chưa làm, deferred).

---

## 7. Wrapper & xử lý register: PVDevice ↔ `b_transport`

Fast Models (`Wrapper_I2C.lisa`) tách 3 phần:
```lisa
composition { busslave : PVBusSlave(size = 0x10000); }   // (a) cổng bus
resources   { I2C_Model* core_logic; }                   // (b) logic thuần
internal slave port<PVDevice> device {
    behavior read (pv::ReadTransaction tx)  { ... core_logic->readReg(off);  }  // (c)
    behavior write(pv::WriteTransaction tx) { ... core_logic->writeReg(off); }
    behavior debugRead/debugWrite(...)      { ... debugReadReg/debugWriteReg; }
}
```

cdc-vp gộp (a)+(c) vào component, (b) nằm cùng file:
```cpp
socket.register_b_transport(this, &X::b_transport);      // (a)
socket.register_transport_dbg(this, &X::transport_dbg);  // debug path

void b_transport(tlm_generic_payload& trans, sc_time& delay) {  // (c)
    auto cmd  = trans.get_command();   // ↔ chọn read vs write behavior
    auto addr = trans.get_address();   // ↔ tx.getAddress()
    auto ptr  = trans.get_data_ptr();  // ↔ tx.getData32() / setReturnData32()
    auto len  = trans.get_data_length(); // ↔ tx.getAccessByteWidth()
    ...                                 // (b) logic register
    trans.set_response_status(tlm::TLM_OK_RESPONSE);  // ↔ tx.writeComplete()
}
```

| Fast Models | cdc-vp |
|---|---|
| `tx.getAddress()` | `trans.get_address()` |
| `tx.getAccessByteWidth()` | `trans.get_data_length()` |
| `tx.getData32()` / `tx.setReturnData32(v)` | đọc/ghi `trans.get_data_ptr()` (memcpy) |
| `tx.writeComplete()` / `setReturnData*` | `trans.set_response_status(TLM_OK_RESPONSE)` |
| trả lỗi (return data 0) | `TLM_ADDRESS_ERROR_RESPONSE` / `TLM_COMMAND_ERROR_RESPONSE` |
| `behavior debugRead/debugWrite` | `transport_dbg` |
| `core_logic->readReg(offset)` | logic decode offset trong `b_transport` |

---

## 8. Sơ đồ ánh xạ gọn

```
Fast Models                          cdc-vp (SystemC/TLM)
─────────────────────────────────────────────────────────────
component System (top .lisa)    →   struct impl : sc_module
  composition { ip : Type(); }  →     <Type> ip;            (member)
  connection {                  →     constructor body:
    m => s;                      →       a.bind(b);
    decoder.range[lo..hi]=>ip;   →       bus.add_target(base,size).bind(ip.socket);
    ip.intr => cpu.irq[N];       →       ip.irq_out(sig); cpu.irq_in(sig);
  }
PVBusDecoder                    →   bus_router
PVBusSlave + PVDevice           →   simple_target_socket + b_transport
RAMDevice                       →   memory_tlm
Wrapper_X.lisa + X_Model(C++)   →   components/<x>_tlm  (wrapper + logic)
pv::ReadTransaction/Write       →   tlm_generic_payload
debugRead/debugWrite            →   transport_dbg
behavior init/reset/terminate   →   constructor / reset_cpu / destructor
```

---

## 9. Những thứ cdc-vp chưa có (Fast Models có)

| Fast Models | Trạng thái cdc-vp |
|---|---|
| `MasterClock` / `ClockDivider` / `clk_in` net | Chưa cần — TLM dùng quantum/thời gian mô phỏng, ít cần clock net |
| `POR_Generator.reset_out => ip.reset_in` | Chưa làm — sẽ thêm `sc_signal` reset broadcast khi cần |
| PLIC/CLINT/IDAU, nhiều line IRQ | Deferred — sẽ có `irq_aggregator`/CLINT/PLIC ở Step sau |
| Parse cấu hình ngoài (map từ file) | Deferred — hiện hardcode trong C++; YAML mới chỉ là tài liệu |
| DMI forwarding qua decoder | `memory_tlm` advertise DMI nhưng `bus_router` chưa forward (TODO) |

Tham chiếu roadmap đầy đủ: [`vp_opensource_stack_implementation_spec_v3.md`](vp_opensource_stack_implementation_spec_v3.md).
