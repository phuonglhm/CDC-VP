# CDC-VP - Clock/Reset & Bus Protocol

Supplement to the CDC-VP overview document, sections 7 and 8. The content follows
the current implementation in `components/` and clearly separates what is
*already modeled* from what is *not yet modeled / planned*.

## 7. Clock and reset

### Clock

**TLM-2.0 loosely-timed** - there is no real clock tree / clock net and no PLL.
Timing is modeled locally per IP using `sc_time`: `access_latency` (10 ns by
default) is added to the `delay` of each transaction; `tick_period` is used for
counters (Timer 20 ns, RTC 1 s, WDT/UART parameterized); **CLINT**
`mtime`/`mtimecmp` uses **microseconds** as the unit and acts as the tick source
for the RTOS. Baud rate (UART) / decimation (DMIC) values are only stored in
registers and are not yet converted into timing behavior.

**CMU0 (`clkmgr_tlm`, OpenTitan-style):** only the software-visible interface is
modeled (`CLK_ENABLES/HINTS`, `EXTCLK_CTRL`, `REGWEN`, life-cycle gating) - it
**does not gate real clocks** and **does not have IRQ support yet**.

### Reset

The convention is **active-low `reset_n`** (`sc_in<bool>`).

| Group | IP / port |
|---|---|
| Has `reset_n` | adc, dma, dmic, qspi, rtc, spi, timer, trng, wdt |
| WDT special case | `reset_n` (input) + `reset_o` (output) - requests a system reset when the watchdog expires |
| PMU0 (power FSM) | inputs `por_rst_n`, `sw_rst_req`, `ndmreset_req`; outputs `rst_lc_n`, `sys_rst_n`, `wakeup_irq` |
| No `reset_n` | UART (`UartTLM`), I2C, OTP, PWM, CMU, CLINT, PLIC, RAM, bus_router (state initialized in the constructor) |

- When `reset_n` is held low, registers are cleared or held in reset (for
  example, adc/rtc reset their cores when `!reset_n`; the timer thread waits for
  `posedge reset_n`).
- **No global reset tree is modeled**: the platform top ties the `reset_n`
  signals to a common reset signal; the reset/power root is **PMU0**.

## 8. Bus protocol

**Standard:** **TLM-2.0, loosely-timed (LT)**, base protocol. Sockets use
`tlm_utils::simple_initiator_socket` / `simple_target_socket`.

- **Transactions:** **blocking `b_transport`** (timed through the `delay`
  parameter) + **`transport_dbg`** (untimed backdoor access, used for debug / ELF
  loader). There is **no** non-blocking / approximately-timed path
  (nb_transport / AT). **DMI** (`get_direct_mem_ptr`) is **not supported yet** -
  `bus_router` does not forward it, so all accesses fall back to `b_transport`.
- **Generic payload:** command (READ/WRITE), address, data ptr, data length, and
  response status. Registers are **32-bit** wide; many IPs **require 4-byte,
  4-byte-aligned accesses**. Violations return `TLM_ADDRESS_ERROR_RESPONSE`;
  invalid commands return
  `TLM_COMMAND_ERROR_RESPONSE`.
- **Endianness:** host endianness (memcpy `uint32`), which means little-endian on
  x86. Byte-enable / streaming width are generally unused; the model reads and
  writes full 32-bit words.

### Interconnect - `bus_router`

Address decoding over one shared physical address space for all initiators:

- `[base, size)` regions are registered during construction via
  `add_target(base, size)`. On each transaction: decode the region containing
  the address -> **translate to the region-local address (`addr - base`)** ->
  forward to the target -> restore the original address. If no region matches,
  return `TLM_ADDRESS_ERROR_RESPONSE`.
- **Multiple upstream ports** share the same downstream map: CPU instruction bus
  + data bus (or unified bus), **DMA master**, and the active **NPU master**
  (plus future ISP/VPU masters) -
  configured through
  `num_initiators` / `cpu_port(i)`.
- **Master access:** DMA and accelerators issue `b_transport` as initiators to
  read/write **RAM0** through this same bus. There is no shortcut path to RAM.
- **Timing:** `delay` accumulates each target's `access_latency`; the level of
  temporal relaxation (temporal decoupling / quantum) depends on the CPU backend.

> **Two points to note (facts, not optimistic assumptions):**
> 1. CMU **does not gate real clocks**.
> 2. **DMI is not available yet** - every CPU fetch / RAM read-write goes through
>    `b_transport`, which will be **slow** when booting an RTOS or running large
>    firmware. If speed is required, **DMI for RAM0** should be the first
>    optimization item.
