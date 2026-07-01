# Architecture & Fast Models <-> cdc-vp Mapping

This document explains the cdc-vp architecture (SystemC/TLM-2.0) and provides a
1:1 mapping to Arm Fast Models (LISA/FVP), so engineers who are familiar with
Fast Models can look things up quickly.

Fast Models reference examples are taken from the internal Corstone-320 project:
- Top LISA: `FVP_FX1_SSE_320.lisa`
- Wrapper LISA: `FastModels_Wrapper/I2C/Wrapper_I2C.lisa`
- Pure IP logic: `TLM_Corstone320/IP/i2c/{i2c.h,i2c.cpp}`

cdc-vp examples are taken from Step 1: [`platforms/mini_tlm`](../platforms/mini_tlm)
+ [`components/`](../components).

---

## 1. Three architecture layers, common to both

| Layer | Fast Models | cdc-vp |
|---|---|---|
| **Top / System** | `component ... { component_type = "System" }` in the top `.lisa` | `struct <platform>_top::impl : sc_module` (for example, `mini_tlm_top.cpp`) |
| **Wrapper (adapter)** | `Wrapper_X.lisa`: `PVBusSlave` + `internal port<PVDevice>` | Glue logic: `simple_target_socket` + `register_b_transport` in `components/<x>_tlm/src/*.cpp` |
| **Pure IP logic** | `class X_Model` (plain C++, for example `I2C_Model`) | Register logic inside `b_transport` (or separated out if it is a third-party model) |

> Arm **separates** the wrapper (LISA) from the core logic (plain C++). cdc-vp
> **combines** the wrapper and logic into one component for simplicity. Arm's
> "thin wrapper + third-party model" approach is exactly the strategy applied by
> the spec for the CPU model (Bremen RISC-V VP): see
> `cpu_models/include/cdc/cpu/cpu_base.h`.

---

## 2. Core concepts - dictionary

| Concept | Fast Models (PVBus) | cdc-vp (TLM-2.0) |
|---|---|---|
| Bus protocol | PVBus | TLM-2.0 generic payload |
| Transaction initiator | `master port<PVBus> pvbus_m` | `tlm_utils::simple_initiator_socket` |
| Transaction receiver | `slave port<PVBus> pvbus_s` | `tlm_utils::simple_target_socket` |
| Bus connection | `A.master => B.slave;` | `A.bind(B);` |
| Address decoder | `PVBusDecoder` | [`bus_router`](../components/bus_router) |
| Memory | `RAMDevice()` | [`memory_tlm`](../components/memory_tlm) |
| Transaction | `pv::ReadTransaction` / `WriteTransaction` | `tlm::tlm_generic_payload` |
| Read handler | `behavior read(tx)` | `b_transport` + `TLM_READ_COMMAND` |
| Write handler | `behavior write(tx)` | `b_transport` + `TLM_WRITE_COMMAND` |
| Debug access (untimed) | `behavior debugRead/debugWrite` | `transport_dbg` |
| 1-bit signal (interrupt/reset) | `port<Signal>` | `sc_in<bool>` / `sc_out<bool>` + `sc_signal<bool>` |
| Initialization | `behavior init()` | constructor |
| Reset | `behavior reset(int level)` | planned as `reset_cpu()` / `sc_in reset` |
| Destruction | `behavior terminate()` | destructor |

---

## 3. `composition` <-> member declaration

```lisa
// Fast Models - top .lisa
composition {
    cpu0      : MCU_CPU();
    uart0     : UART_Model();
    timer_clk : SP804_Timer();
    ext_dram  : RAMDevice();
    axi_inner : PVBusDecoder();
}
```

```cpp
// cdc-vp - mini_tlm_top.cpp, struct impl
cpu_stub                   cpu;    // CPU model
cdc::components::bus_router bus;    // PVBusDecoder()
cdc::components::uart_tlm   uart;   // UART model
cdc::components::timer_tlm  timer;  // SP804_Timer()
cdc::components::memory_tlm ram;    // RAMDevice()
```

---

## 4. `connection` (1) - bus master

```lisa
cpu0.pvbus_m => axi_top.pvbus_s;        // master => slave
```

```cpp
cpu.bus_socket.bind(bus.target_socket); // initiator -> target
```

`=>` <-> `.bind()`. `pvbus_m`/`pvbus_s` (master/slave) <-> `initiator`/`target`
socket.

---

## 5. `connection` (2) - memory map

```lisa
// Fast Models - [lower bound .. upper bound] syntax
axi_inner.pvbus_m_range[0x40100000..0x40100FFF] => uart0.pvbus;     // UART  4KB
ahb_inner.pvbus_m_range[0x40000000..0x40000FFF] => timer_clk.pvbus; // Timer 4KB
axi_inner.pvbus_m_range[0x80000000..0x8FFFFFFF] => ext_dram.pvbus;  // DRAM  256MB
```

```cpp
// cdc-vp - (base, size) syntax
bus.add_target(0x10000000, 0x1000).bind(uart.socket);   // UART
bus.add_target(0x10001000, 0x1000).bind(timer.socket);  // Timer
bus.add_target(0x80000000, 0x1000).bind(ram.socket);    // RAM
```

| | Fast Models | cdc-vp |
|---|---|---|
| Declare one region | `decoder.pvbus_m_range[lo..hi] => ip.pvbus` | `bus.add_target(base, size).bind(ip.socket)` |
| Region representation | lower bound - upper bound | base + size (`hi = base + size - 1`) |
| Translate address to internal offset | Done by `PVBusDecoder` | Done by `bus_router::b_transport` by subtracting base |

Both approaches **translate the address** to an internal offset before entering
the IP, so the IP does not need to know its own base address and can be reused at
any address.

### Hierarchical bus, multi-level

Fast Models can nest multiple `PVBusDecoder` instances:

```lisa
axi_top.pvbus_m_range[0x00000000..0xFFFFFFFF]   => axi_inner.pvbus_s;
axi_inner.pvbus_m_range[0x40000000..0x5FFFFFFF] => ahb_inner.pvbus_s;
```

cdc-vp does the same by binding one target of the parent bus to the
`target_socket` of the child bus:

```cpp
bus_top.add_target(0x40000000, 0x20000000).bind(bus_ahb.target_socket);
```

(Used later in Step 3 - custom SoC.)

---

## 6. `connection` (3) - IRQ map

```lisa
uart0.intr         => cpu0.irq[0];
timer_clk.irq_out0 => cpu0.irq[8];
```

```cpp
// cdc-vp: an sc_signal is needed as the intermediate "wire"
sc_core::sc_signal<bool> timer_irq;
timer.irq_out(timer_irq);   // IP.intr -> wire
cpu.timer_irq(timer_irq);   // wire -> CPU.irq
```

| Fast Models | cdc-vp |
|---|---|
| `master port<Signal> intr` | `sc_out<bool> irq_out` |
| `cpu0.irq[N]` | `sc_in<bool>` in the CPU |
| `ip.intr => cpu0.irq[N]` (direct connection with numbering) | connected through an intermediate `sc_signal<bool>` |

When there are many IRQ sources (the `cpu0.irq[0..62]` table in Corstone-320
LISA), cdc-vp will need an `irq_aggregator` / PLIC / CLINT. This is not
implemented yet and is deferred.

---

## 7. Wrapper & register handling: PVDevice <-> `b_transport`

Fast Models (`Wrapper_I2C.lisa`) separates three parts:

```lisa
composition { busslave : PVBusSlave(size = 0x10000); }   // (a) bus port
resources   { I2C_Model* core_logic; }                   // (b) pure logic
internal slave port<PVDevice> device {
    behavior read (pv::ReadTransaction tx)  { ... core_logic->readReg(off);  }  // (c)
    behavior write(pv::WriteTransaction tx) { ... core_logic->writeReg(off); }
    behavior debugRead/debugWrite(...)      { ... debugReadReg/debugWriteReg; }
}
```

cdc-vp combines (a) and (c) into the component, while (b) lives in the same file:

```cpp
socket.register_b_transport(this, &X::b_transport);      // (a)
socket.register_transport_dbg(this, &X::transport_dbg);  // debug path

void b_transport(tlm_generic_payload& trans, sc_time& delay) {  // (c)
    auto cmd  = trans.get_command();   // <-> select read vs write behavior
    auto addr = trans.get_address();   // <-> tx.getAddress()
    auto ptr  = trans.get_data_ptr();  // <-> tx.getData32() / setReturnData32()
    auto len  = trans.get_data_length(); // <-> tx.getAccessByteWidth()
    ...                                 // (b) register logic
    trans.set_response_status(tlm::TLM_OK_RESPONSE);  // <-> tx.writeComplete()
}
```

| Fast Models | cdc-vp |
|---|---|
| `tx.getAddress()` | `trans.get_address()` |
| `tx.getAccessByteWidth()` | `trans.get_data_length()` |
| `tx.getData32()` / `tx.setReturnData32(v)` | read/write `trans.get_data_ptr()` using memcpy |
| `tx.writeComplete()` / `setReturnData*` | `trans.set_response_status(TLM_OK_RESPONSE)` |
| Error return (return data 0) | `TLM_ADDRESS_ERROR_RESPONSE` / `TLM_COMMAND_ERROR_RESPONSE` |
| `behavior debugRead/debugWrite` | `transport_dbg` |
| `core_logic->readReg(offset)` | offset decode logic inside `b_transport` |

---

## 8. Compact mapping diagram

```text
Fast Models                          cdc-vp (SystemC/TLM)
-------------------------------------------------------------
component System (top .lisa)    ->   struct impl : sc_module
  composition { ip : Type(); }  ->     <Type> ip;            (member)
  connection {                  ->     constructor body:
    m => s;                     ->       a.bind(b);
    decoder.range[lo..hi]=>ip;  ->       bus.add_target(base,size).bind(ip.socket);
    ip.intr => cpu.irq[N];      ->       ip.irq_out(sig); cpu.irq_in(sig);
  }
PVBusDecoder                    ->   bus_router
PVBusSlave + PVDevice           ->   simple_target_socket + b_transport
RAMDevice                       ->   memory_tlm
Wrapper_X.lisa + X_Model(C++)   ->   components/<x>_tlm  (wrapper + logic)
pv::ReadTransaction/Write       ->   tlm_generic_payload
debugRead/debugWrite            ->   transport_dbg
behavior init/reset/terminate   ->   constructor / reset_cpu / destructor
```

---

## 9. Features cdc-vp does not have yet, but Fast Models does

| Fast Models | cdc-vp status |
|---|---|
| `MasterClock` / `ClockDivider` / `clk_in` net | Not needed yet - TLM uses quantum / simulation time and usually needs clock nets less often |
| `POR_Generator.reset_out => ip.reset_in` | Not implemented yet - reset broadcast through `sc_signal` will be added when needed |
| PLIC/CLINT/IDAU, many IRQ lines | Deferred - `irq_aggregator`/CLINT/PLIC will be added in a later step |
| External configuration parsing (map from file) | Deferred - currently hardcoded in C++; YAML is documentation only for now |
| DMI forwarding through decoder | `memory_tlm` advertises DMI, but `bus_router` does not forward it yet (TODO) |

Full roadmap reference: [`vp_opensource_stack_implementation_spec_v3.md`](vp_opensource_stack_implementation_spec_v3.md).
