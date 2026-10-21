# Integration and timing

GPIO owns its core and three-bank subsystem; IO_PAD owns pad routing.
The two implementation libraries are:
`fu3::gpio` ([GPIO](../README.md)) and `fu3::io_pad`
([IO_PAD](../../IO_PAD/README.md)). Link `fu3::gpio` for the core and combined
subsystem; its public IO_PAD dependency provides the pad headers/library.

## GPIO transport

GPIO offers a TLM `Gpio` and a signal-level `ApbGpio<AddressWidth = 12>`.
They share `detail::State` register/sampling/IRQ behavior. For APB, include
`fu3/gpio/apb.h`, bind HCLK/HRESETn, the PADDR/PWDATA/PWRITE/PSEL/PENABLE
inputs, PRDATA/PREADY/PSLVERR outputs and the GPIO ports described in HDS 7.1–7.2.
PADDR uses `sc_biguint<AddressWidth>`; widths
must be at least six. The regression exercises widths 6, 12 and 128.
PREADY is always true and PSLVERR always false, including reset. Writes and
INTSTATUS read-clear occur on rising HCLK with PSEL/PENABLE asserted; PRDATA
follows the decoded register combinationally without select/enable gating.
The APB module has its own instance of the shared state and no TLM socket.

`fu3::gpio::Subsystem` contains `gpioa`, `gpiob`, `gpioc` and `io_pad`.
Bind three initiator/interconnect targets to `gpioa.socket`, `gpiob.socket`,
`gpioc.socket`, forwarding **local** addresses. Bind `clk`, `reset_n`, the
three `irq` outputs, three `wake_bank` outputs and aggregate `wake`.
`tests/test_integration.cpp` contains a complete runnable integration.

Each valid `b_transport` consumes its incoming annotated delay, serializes
accesses to that bank and waits for the next available rising clock edge.
It returns with zero delay. The caller must be a SystemC thread process that
may suspend. Calls to distinct banks can proceed independently. The three
GPIO state machines continue sampling on every edge even without bus traffic.
A transaction arriving in the same simulation time as an edge may use that
edge or the next depending on scheduling; initiators should launch away from
the edge when exact cycle placement matters. APB setup / PSEL / PENABLE wires
are abstracted by the single accepted TLM access.

Reset is active low and asynchronous. It restores all GPIO registers and the
input pipeline and aborts a queued access with `TLM_GENERIC_ERROR_RESPONSE`.
Calls waiting for the bank mutex are also invalidated, even if a short reset
pulse has ended before they acquire the mutex. A call made while reset is
asserted receives the same response immediately after its incoming delay.
The model does not assign a hardware APB error signal to these reset-time
software transport errors.

The bus interface accepts four-byte, little-endian reads/writes with streaming
width at least four and no byte enables. Other sizes or wrapping bursts get
`TLM_BURST_ERROR_RESPONSE`; byte enables get `TLM_BYTE_ENABLE_ERROR_RESPONSE`;
unsupported commands get `TLM_COMMAND_ERROR_RESPONSE`; a null data pointer
gets `TLM_GENERIC_ERROR_RESPONSE`. These validate the VP transport contract;
the actual GPIO APB IP has no PSLVERR mechanism.

`transport_dbg` provides immediate, non-destructive reads, including INTSTATUS.
Debug writes are rejected. DMI is disabled because register reads have state
and input sampling depends on the clock.

For a raw input changed before edge 1:

| Edge | State visible after the edge |
| --- | --- |
| 1 | First synchronizer stage |
| 2 | Second stage, `input_sync[bank]` |
| 3 | PADIN; edge-mode IRQ can assert |
| 4 | Level-mode IRQ can assert |

## IO_PAD routing

`fu3::io_pad::IoPad` is combinational and has no reset, clock, TLM socket or
register bank. The compatibility header `fu3/gpio/io_pad.h` aliases the former
`fu3::gpio` pad types to this implementation.
Bind its `select[80]`, `pad_in[80]`, `pad_data[80]`, `requested_oe[80]`,
`test_mode` and `test_select` ports. Vectors index **package position minus 1**.
`iosel0` handles logical positions 1–40 and `iosel1` handles 41–80. This split
is a VP organization, pending confirmation against the additional IOSEL spec.
Outputs settle over SystemC delta cycles; consumers should allow combinational
settling before sampling them.

For configurable pins, selection 0 routes the primary function, 1 the alternate,
and 2/3 return zero data, requested enable and routed SoC inputs. For fixed
functions the select value is ignored. Reserved/no-function and supply/analog
positions give zero digital routing. Three input-only pad cells never request
output enable. Physical PAD drive remains controlled by external electrical
configuration and is outside this digital VP model.

The two test inputs at package positions 5 and 11 remain available to the decoder.
`test_mode = pad[5]`, `test_select = pad[5] && pad[11]` in package notation.
Test mode zeros normal routing, including the reset and strap function paths;
it does not synthesize GPIO reset. No test scan/flash path is implemented.

| Bank | GPIO bits 0 through 7, package positions |
| --- | --- |
| A | 6, 12, 13, 14, 15, 20, 21, 22 |
| B | 25, 26, 27, 28, 29, 30, 44, 45 |
| C | 46, 47, 54, 55, 56, 59, 60, 61 |

A[0:4] and C[0:1] use selection 1. Other configurable GPIO pins use selection
0. Fixed GPIO pins ignore select. All 32 bits of each GPIO IP remain implemented;
the package bonds eight per group. `unbonded_in[bank]` provides optional input
stimulus for bits 8–31. Its low eight bits are ignored. Output loopback is not
implicit: a board/pad-cell model must resolve and feed package inputs explicitly.

`peripheral_primary_out`, `peripheral_alternate_out` and the matching requested
enable signals provide generic per-package-position function endpoints. The
selected GPIO legs are overridden by PADOUT/PADDIR; remaining legs are supplied
by the platform. `primary_in` and `alternate_in` expose the routed receiver paths.
JTAG and SPI signal orders are not guessed from grouped pin ranges in the HDS.
These arrays are simulation wiring, not software registers.

Each `power_event` is exported as `wake_bank[bank]`. The aggregate `wake` is a
documented **board integration policy**, ORing the three bank outputs. There is
no additional wake enable, status, clear, debounce or power-domain state machine.
`pad_config[bank*32+pin]` exposes GPIO's six-bit configuration values without
assuming a mapping to IO_PAD's foundry-specific electrical controls.

## Modeling boundaries

The supplied HDS documents define GPIO behavior and digital IO_PAD routing.
The following integration contracts require additional SoC or PDK information.

| Topic | HDS reference | Model boundary |
| --- | --- | --- |
| IOSEL0/1 system registers | IO_PAD Sections 3, 6.3 and 8 | No register map is supplied. Function selection is configured through 80 two-bit inputs. |
| Physical partition identity | IO_PAD Section 4 | `iosel0` and `iosel1` are logical groups for positions 1-40 and 41-80; their hardware identity is unverified. |
| Dedicated WAKE controller | GPIO Section 6.5 | Per-bank POWEREVENT behavior is implemented. ORing bank outputs is a VP policy; no sleep/power controller is modeled. |
| Pad electrical behavior | IO_PAD Sections 6.4 and 7 | No analog, supply, contention, pull, drive-strength, receiver-enable, Schmitt, slew or physical tri-state simulation. |
| Direction polarity | GPIO Section 6.3 | PADDIR is passed through as a requested enable; its mapping to physical drive is unspecified. |
| PADCFG electrical mapping | GPIO Section 6.4; IO_PAD Section 6.4 | Six-bit GPIO values are exposed unchanged; mapping to foundry controls is external. |
| Grouped peripheral pin order | IO_PAD Table 7-2 | JTAG/SPI functions use generic endpoints by package position where detailed signal order is absent. Peripheral protocols are not modeled. |
| APB signal-level phases | GPIO Section 7.1 | `Gpio` abstracts the phases through TLM; `ApbGpio` implements the specified digital pins and is covered by six APB scenarios. Source RTL equivalence is not established. |

The tests validate this digital VP contract. Source RTL equivalence, foundry
pad behavior and full-SoC firmware boot have not been verified.
