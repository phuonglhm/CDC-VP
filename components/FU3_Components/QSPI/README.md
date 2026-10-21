# FU3 QSPI SystemC/TLM VP

Functional SystemC/TLM model for QSPI, AP_QSPIC, A2H_QSPI and XIP, based on
`FTEL_CDC_IP_QSPI_Specification.pdf`, HDS V1.0 dated 2026-10-20 (63 pages).
Pinned spec identity: **V1.0 / 2026-10-20**, recorded in
[SPEC_VERSION.md](SPEC_VERSION.md) and [spec-baseline.json](spec-baseline.json).
The HDS closed DEC decisions supersede the older register/wrapper PDFs.
See [spec mapping and scope](docs/spec-decisions.md) for the source hash and migration notes.
Supports the 16-register map, indirect transfers, polling, XIP, FIFOs, IRQ and
DMA. Includes a generic NOR flash backend for firmware/image testing.

## Build and test

From the repository root in Linux/WSL with SystemC 2.3.4 and C++17:

```sh
cmake -S components/FU3_Components/QSPI -B build/fu3-qspi \
  -DSYSTEMC_HOME=/opt/systemc-2.3.4 -DCMAKE_BUILD_TYPE=Debug \
  -DFU3_QSPI_BUILD_TESTS=ON
cmake --build build/fu3-qspi -j4
ctest --test-dir build/fu3-qspi --output-on-failure
```

The default suite has 57 tests. `FU3_QSPI_BUS_TESTS=OFF` omits the real FU3 bus
integration case. The repository build enables this component through
`CDC_BUILD_FU3_SOC=ON`; link the CMake target `fu3::qspi`.
See [testing](docs/testing.md) for P0/P1 selection, source clauses and UBSan.
The recorder requires all mapped functional groups, embeds the spec version/hash
in the report, and can verify the source PDF with `--spec-pdf`.

## Components

Headers are under `include/fu3/qspi/`.

| Class | Header | Responsibility |
| --- | --- | --- |
| QspiRegisterModel | qspi_register_model.h | Register storage, reset, masks, access policy, W1C |
| QspiController | qspi_controller.h | FSM, triggers, BUSY/ABORT/TCF/TEF, AP/XIP sockets |
| QspiTxRxFifo | qspi_tx_rx_fifo.h | TX/RX storage, levels and threshold flags |
| QspiFlashModel | qspi_flash_model.h | NOR storage, commands, program/erase, image I/O |
| QspiAhbAdapter | qspi_ahb_adapter.h | XIP aperture and functional forwarding |
| QspiDmaInterface | qspi_dma_interface.h | Request/clear, burst eligibility, rearm |
| QspiInterrupt | qspi_interrupt.h | Enabled status/error flag aggregation |
| QspiTimingModel | qspi_timing_model.h | Divider, serial phase, CSHT/PIR and timeout timing |

Controller, flash and adapter are SystemC modules. The other classes are
components driven by the controller processes; their short implementations
live in headers. `Qspi` and `A2hQspi` are compatible aliases. The older
`components/qspi_tlm` uses a different register map/protocol.

## Integration

```cpp
#include <fu3/qspi/qspi_controller.h>
#include <fu3/qspi/qspi_ahb_adapter.h>
#include <fu3/qspi/qspi_flash_model.h>

fu3::qspi::QspiController qspi{"qspi"};
fu3::qspi::QspiAhbAdapter a2h{"a2h"};
fu3::qspi::QspiFlashModel nor{"nor"};
fabric.target("AP_QSPIC").bind(qspi.ap_qspic);
fabric.target("QSPI_XIP").bind(a2h.socket);
a2h.ahb.bind(qspi.xip);
qspi.flash.bind(nor.socket);
qspi.clk(qspi_clk); qspi.pclk(apb_clk); qspi.hclk(ahb_clk); qspi.dma_clk(dma_clk);
qspi.reset_n(core_reset_n); qspi.preset_n(apb_reset_n);
qspi.hreset_n(ahb_reset_n); qspi.dma_reset_n(dma_reset_n);
nor.reset_n(flash_reset_n);
qspi.irq(qspi_irq);
qspi.dma_rx_req(rx_request); qspi.dma_rx_clr(rx_clear);
qspi.dma_tx_req(tx_request); qspi.dma_tx_clr(tx_clear);
nor.load_image("boot.bin"); // Raw binary; load before transactions start.
```

Bind all four clocks and resets, IRQ and DMA signals. Assert the four controller
resets together and release each synchronously to its clock (HDS ASM-002/014). Disable or bind other enabled
fabric targets as described in the FU3 bus README. The fabric supplies local
addresses: AP_QSPIC is mapped at `0x44000000` (64 KiB, implemented offsets
`0x00..0x3c`), XIP at `0x30000000` (128 KiB). These addresses come from the
existing FU3 bus map. FSIZE additionally limits accesses.

Call `b_transport` from an `SC_THREAD`. Bus accesses are little-endian,
full-lane 32-bit words; the low two APB/AHB address bits are ignored. The fabric decomposes CPU/DMA bursts into singles.
Partial/narrow accesses fail; XIP currently accepts reads only. AP debug reads
are non-destructive; debug writes and DMI are unavailable.

## Register use

See [registers](docs/registers.md) for offsets, fields and trigger rules.
A minimal 128-KiB XIP setup is:

```cpp
write32(0x44000004, 16u << 16); // FSIZE = 16.
write32(0x44000000, 0x00000001); // EN; SCLK = core clock / 2 (PRESCALER + 2).
write32(0x44000014, 0x0d002503); // Mapped READ 03, 1-1-1, 24-bit address.
auto first_word = read32(0x30000000);
```

For indirect reads, program DLR to byte count minus one, CCR to FMODE=1,
then AR when ADMODE is nonzero. Drain DR while RXFLEVEL is nonzero. TCF marks
serial completion; BUSY clears once RX is empty. For indirect writes with data,
the first DR write starts transmission. Clear sticky flags through FCR/DMAFCR
before reusing them as completion tests.

## Flash backend

`QspiFlashModel` is a generic SDR NOR profile, not a complete named vendor chip.
Storage starts at `0xff`; capacity is a power of two from 4 KiB to 16 MiB,
default 128 KiB. `FlashConfig` sets size, operation durations and the synthetic
ID (default `7f 51 11`). Command conventions use the
[W25Q64JV Revision C datasheet, sections 7-8](https://media.digikey.com/pdf/Data%20Sheets/Winbond%20PDFs/W25Q64JV_Rev_C.pdf);
the HDS leaves device-specific commands and initialization to software.

| Opcode (hex) | Operation / requirements |
| --- | --- |
| 06 / 04 | Write enable / disable |
| 05 / 35 | Read repeated status: WIP/WEL / QE |
| 31 / 9f | Write QE (one byte, only bit 1, WEL required) / read repeating 3-byte ID |
| 03 / 0b | 1-1-1 read: no dummy / eight dummy clocks |
| 3b / 6b | 1-1-2 / 1-1-4 read, eight dummy clocks; quad requires QE |
| bb / eb | 1-2-2 / 1-4-4 read, one alternate byte; zero / four dummy clocks; quad requires QE |
| 02 / 32 | Single / quad page program; WEL required, quad also requires QE |
| 20 / d8 | Aligned containing 4-KiB sector / 64-KiB block erase; WEL required |
| c7 / 60 | Chip erase; WEL required |
| 66 then 99 | Consecutive enable-reset / reset commands |

Address phases are three bytes. A program frame accepts 1..256 bytes and wraps
within its 256-byte page; commit uses AND, so program only clears bits. Program,
erase and QE writes consume WEL and set WIP until an atomic commit. Controller
TCF can precede flash completion: poll WIP before reading/programming again.
Only status reads and reset are accepted while WIP is set. Invalid commands or
phase combinations return TLM errors. Defaults are VP timings: program 50 us,
sector erase 200 us, block erase 500 us, chip erase 2 ms, QE write 5 us.

EB alternate byte high nibble `a` enters continuous read for SIOO; another
nibble exits it. An explicit instruction replaces the retained command.
Flash reset clears WEL/QE/continuous state and discards unfinished operations,
while committed bytes survive. Cancel before frame close discards staged data;
it cannot undo a frame already accepted. Load/save require idle flash:

```cpp
nor.load(bytes, offset);
nor.load_image("boot.bin", offset);
auto snapshot = nor.read_image(0, 16); // Last committed bytes, also while busy.
nor.save_image("flash.bin");
```

A custom flash target can consume `FlashTransaction`: at most four bytes per
callback, frame ID, total length, offset, begin/end/cancel, opcode, numeric
address/alternate and serial phase metadata. Targets must annotate latency,
return a response and never call `wait` or retain payload pointers. The
controller waits for annotated latency before exposing data/completion.
SFDP, protection/OTP, suspend/resume, QPI, four-byte flash addresses, DDR flash
commands, ELF loading and automatic persistence are not included.

## HDS behavior and migration

`Config::tx_fifo_words` and `rx_fifo_words` are 32-bit word depths, each one of
4, 8, 16 or 32 (default 32 = 128 bytes). SR FIFO levels and all thresholds count
words. A final partial RX word is valid with unused high bytes zero. The former
byte-capacity fields and selectable byte-threshold profile were removed.

`SCLK = core / (PRESCALER + 2)`: reset PRESCALER=0 is legal SDR, and
PRESCALER=2 is the fastest legal DDR setting. SDR divisors must be even; DDR
divisors must be multiples of four. Illegal triggers set TEF, including an XIP
miss which also returns an error. Instruction phases always use SDR.

EN=0 prevents new frames. A frame in progress finishes; polling stops after its
current status frame, and RX data is retained. A disabled indirect-write DR
access pre-fills the TX FIFO. Setting EN alone does not trigger a frame.
ABORT remains set until the modeled byte boundary and STOP interval have
passed, flushes FIFOs, ends a pending XIP with error, and sets TCF as BUSY and
ABORT clear. Writing zero cannot cancel ABORT. Both DMA requests remain
inhibited until the next accepted CCR write; AR and DLR do not re-arm them.

DMA requests run on `dma_clk` with `dma_reset_n`. Request conditions cross
from the core; a sampled clear holds req low for at least one DMA clock and
until the core has re-evaluated eligibility. Whole bursts only: for
`W = ceil(bytes/4)`, DMA moves `floor(W/burst_words)` bursts and the CPU moves
the remaining words through DR (HDS 9.3). DMAEN may change while BUSY.

APB has **no timeout**: a valid access waits for the core to resume or leave
reset. AHB misses time out after `Config::bus_timeout_cycles` HCLK edges
(default 5000, legal 16..1048576). The FU3 integration must override this to
1048576 with a 5-MHz core and 100-MHz HCLK, and keep mapped PRESCALER <=108
(HDS 11.3). Timed-out data is discarded and never fills the cache.

XIP implements a fully associative word cache (default 8 lines). A NONSEQ
miss fetches one word; a SEQ miss fetches `prefetch_length` words (default 8),
clamped at flash end. Each miss invalidates all lines; a read of a word still
being fetched waits for that word. A different miss waits for the active
fetch, with its own timeout. Cache hits use HCLK without needing a core edge.
EN clear, FSIZE/ABR change, accepted CCR write, abort and reset invalidate it.
DLR is ignored, AR writes are ignored and DR reads return zero in mapped mode.

Attach `AhbTransaction` to select HTRANS (default without the extension:
selected NONSEQ). IDLE/BUSY and unselected transfers return zero-wait OKAY.
An upstream AXI bridge must mark SEQ explicitly; consecutive byte addresses
alone do not establish HTRANS. The existing FU3 fabric word decomposition
uses the NONSEQ default, so it remains functionally correct without prefetch.

```cpp
fu3::qspi::AhbTransaction controls;
controls.transfer = fu3::qspi::AhbTransaction::Seq;
payload.set_extension(&controls);
xip_socket->b_transport(payload, delay);
payload.clear_extension<fu3::qspi::AhbTransaction>(); // Stack-owned extension.
```

`mem_cache_count` is 1..16; `prefetch_length` is 1..mem_cache_count.
`apb_async_fifo_depth` (default 4) and `ahb_async_fifo_depth` (default 8) accept
2/4/8/16. These parameter values are validated; exact async FIFO occupancy and
crossing latency are abstracted by the serialized TLM admission paths.
`Config::validate()` rejects illegal combinations before controller storage
allocation. Bus/cache widths stay fixed at 32 bits per HDS Table 5-1.

## Functional scope

This is a firmware-facing SystemC/TLM model. It models register behavior,
frame descriptors, word buffering, DMA/IRQ, cache/prefetch, core-clock serial
phase durations, minimum CSHT/PIR and HCLK timeout behavior. Backend callbacks
remain bounded to four bytes; large DLR/FSIZE values do not allocate a frame.
Custom flash/PSRAM targets can consume the existing `FlashTransaction` API.
The supplied NOR support profile remains SDR-only and at most 16 MiB;
controller DDR and 4-GiB boundaries are tested using a generic sparse target.

Serial data is committed to the backend in chunks, with cancellation of an
open frame on abort/reset. Sub-chunk effects on a real memory device are not
reproduced. There are no pad waveforms, analog timing, RTL CDC/RDC or bus pin
protocol checkers. AHB error timing has a two-HCLK response abstraction, without
HRESP/HREADYOUT pins. Partial domain reset is outside the HDS integration
contract; some defensive cancellation tests remain for VP robustness. Passing
these tests does not establish RTL or pad-level equivalence.
