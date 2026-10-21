# UART requirement traceability

References are printed page numbers in the supplied UART HDS v1.3 unless stated otherwise.
Tests exercise observable behavior; passing them does not resolve source ambiguities.

| Requirement | Source | Implementation / verification |
|---|---|---|
| Byte register selection, DLAB DLL/DLM overlay | pp.20,22-28 | CSR adapter; alias isolation, byte MMIO through real bus |
| 5/6/7/8 bits, parity, 1/1.5/2 stops | pp.12-13,23,25-26 | independent pin TX sampling and external RX stimulus, every format |
| Integer divisor formula; 8x/16x receive sampling | pp.9,19,28 | divisor 1/2 and 8x/16x tests exercise deterministic functional timing; baud-rate accuracy/scaling is deferred until RTL correlation |
| Majority voter and false start detection | pp.14-16,19 | short start rejection and midpoint single-sample glitch |
| PE/FE/BI/OE and RX FIFO error | pp.16,23-24 | injected parity/stop/break errors, overflow, read-clear and retained data; boundary suite: consecutive erroneous heads |
| FIFO enable/clear, DLAB-qualified size | pp.18,22,25 | 16 and extended mode, depth 32/64, reset during TX, queue order |
| RX triggers | p.25 | every table entry and boundary, unreachable 56-byte threshold with capacity 32 |
| THRE versus TEMT | p.24 | independent TX observation, premature completion checks |
| Interrupt identification and clearing | pp.29-30 | boundary suite: simultaneous line/RX/TX/modem sources and sole-source THRE acknowledgement |
| Receive timeout | pp.8,29 | no premature event, timeout below threshold, RBR clear; duration is compatibility choice |
| Modem outputs/deltas, loopback | pp.20-24 | active-low signals, RI trailing edge, MSR clear, internal serialized loopback |
| Auto-CTS/RTS through MCR.AFE and RTS | pp.19,24,27 | behavior suite checks AFE CTS gating and AFE+RTS receive backpressure |
| DMA enable and clear; RX threshold OR timeout triggers | pp.22,30 | below-threshold timeout, threshold-triggered integration, combined threshold+timeout condition, clear suppression/reassertion, fixed-byte bus transfers with fake peer |
| Synchronous UART reset; asynchronous MSR delta clear | pp.18-19 | active TX/RX abort; behavior suite checks MSR[3:0] clear on a sub-cycle reset pulse while other CSRs retain synchronous reset behavior |
| UART0/1/2 addresses and routes | FU3 HAS memory/IRQ/DMA tables | independent address constants, both PERIBUS paths, fake IRQ fan-out |
| Partial writes and debug reads | no UART PSTRB/debug definition | explicit TLM diagnostics and non-destructive debug policy tests |
| No lost/duplicated accesses | functional VP requirement | boundary suite: two concurrent initiator threads, admitted completion order, error paths yielding |

Not claimed: RS485 register ABI, software-selectable 8x encoding,
physical CDC/STA, exhaustive reset phases or real CPU boot. See MODEL_CONTRACT.md
for the separation between explicit HDS behavior and compatibility choices.
