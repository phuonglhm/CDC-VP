# UART requirement traceability

References are PDF pages in the supplied 34-page UART HDS unless stated otherwise.
Tests exercise observable behavior; passing them does not resolve source ambiguities.

| Requirement | Source | Implementation / verification |
|---|---|---|
| Byte register selection, DLAB DLL/DLM overlay | pp.20,22-28 | CSR adapter; alias isolation, byte MMIO through real bus |
| 5/6/7/8 bits, parity, 1/1.5/2 stops | pp.12-13,23,25-26 | independent pin TX sampling and external RX stimulus, every format |
| Integer divisor, default 16x, optional 8x | pp.8-9,19,27 | divisor 1/2 tests; all behavior tests at 8x and 16x; boot-rate integration |
| Majority voter and false start detection | pp.14-16,19 | short start rejection and midpoint single-sample glitch |
| PE/FE/BI/OE and RX FIFO error | pp.16,23-24 | injected parity/stop/break errors, overflow, read-clear and retained data; boundary suite: consecutive erroneous heads |
| FIFO enable/clear, DLAB-qualified size | pp.18,22,25 | 16 and extended mode, depth 32/64, reset during TX, queue order |
| RX triggers | p.25 | every table entry and boundary, unreachable 56-byte threshold with capacity 32 |
| THRE versus TEMT | p.24 | independent TX observation, premature completion checks |
| Interrupt identification and clearing | pp.28-29 | boundary suite: simultaneous line/RX/TX/modem sources and sole-source THRE acknowledgement |
| Receive timeout | pp.8,29 | no premature event, timeout below threshold, RBR clear; duration is compatibility choice |
| Modem outputs/deltas, loopback | pp.20-24 | active-low signals, RI trailing edge, MSR clear, internal serialized loopback |
| Auto-CTS/RTS | pp.19-20 | platform-enabled flow control tests; firmware enable ABI remains undefined |
| DMA enable and clear | pp.21,29 | request suppression/reassertion; real bus DMA fixed-byte transfers with fake peer |
| Synchronous reset | p.18 | active TX/RX abort; boundary suite: queued-request cancellation, pulses missing the clock edge, defaults |
| UART0/1/2 addresses and routes | FU3 HAS memory/IRQ/DMA tables | independent address constants, both PERIBUS paths, fake IRQ fan-out |
| Partial writes and debug reads | no UART PSTRB/debug definition | explicit TLM diagnostics and non-destructive debug policy tests |
| No lost/duplicated accesses | functional VP requirement | boundary suite: two concurrent initiator threads, admitted completion order, error paths yielding |

Not claimed: RS485 register ABI, software-selectable auto-flow/8x encoding,
physical CDC/STA, exhaustive reset phases or real CPU boot. See MODEL_CONTRACT.md
for the separation between explicit HDS behavior and compatibility choices.
