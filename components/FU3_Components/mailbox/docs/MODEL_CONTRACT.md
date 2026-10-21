# FU3 MAILBOX functional contract

## Sources and scope

Specification priority for FU3 (leader decision, 2026-10-20): FU3 HAS v2.0 and FTEL_FU3_Common_Spec v2.0 first, the IP HDS only where both are silent.
HAS v2.0 and Common Spec v2.0 define MAILBOX_CVA6 (APB4, PERIBUS_1, 0x5002_0000) and MAILBOX_IBEX (APB, PERIBUS_0, 0x4404_0000) as the two endpoints of inter-CPU messaging, and HAS section 10.2 routes `irq_mailbox_ibex` to Ibex `irq_fast_i[14]`. Neither gives a register map.
Registers, FIFO behavior, interrupts, errors and the per-port reset follow the Mailbox HDS V1 (FTEL_CDC_Mailbox_Hardware_Design_Specification_V1).
Port 0 (CPU0) is CVA6 and port 1 (CPU1) is Ibex.
The source documents are private and are not included in this package.

## Structure

One `fu3_mailbox::Mailbox` with two target sockets: `cva6_socket` (port 0, CPU0 = CVA6) and `ibex_socket` (port 1, CPU1 = Ibex), and one reset per port: `presetn0` and `presetn1` (HDS 6.1, 6.2).
Two FIFOs of 32-bit words, depth `FU3_MAILBOX_DEPTH` = 32 (constructor argument): CVA6 -> Ibex and Ibex -> CVA6.
For each port, the send FIFO is the one it writes and the receive FIFO is the one it reads.

## Bus interface

- Local offsets from the bus and each window is 64 KiB.
- Only aligned 32-bit accesses with all byte enables. Other sizes, misaligned addresses, partial PSTRB, unmapped offsets, reads of WO and writes of RO registers return `TLM_GENERIC_ERROR_RESPONSE` (PSLVERR) and change nothing.
- `transport_dbg` reads have no side effect: FIFO_RD returns the head word without removing it (0 when empty) and sets no error flag. Debug writes are ignored.

## Registers (per port)

| Offset | Register | Access | Behavior |
| --- | --- | --- | --- |
| 0x000 | FIFO_WR | WO | Push one word to the send FIFO. When full: PSLVERR, word dropped, FIFO_WR_ERROR set |
| 0x008 | FIFO_RD | RO | Pop the oldest word of the receive FIFO. When empty: PSLVERR, returns 0, FIFO_RD_ERROR set |
| 0x010 | STATUS | RO | [31] EMPTY (receive FIFO), [30] FULL (send FIFO), [29] SEND_THRESHOLD_ACTIVE (send count <= WR_THRESHOLDS), [28] RECEIVE_THRESHOLD_ACTIVE (receive count > RD_THRESHOLDS) |
| 0x018 | WR_THRESHOLDS | RW | 32 bits |
| 0x01C | RD_THRESHOLDS | RW | 32 bits |
| 0x020 | IRQ_STATUS | R/W1C | [31] WR_THRESHOLD, [30] RD_THRESHOLD, [29] FIFO_WR_ERROR, [28] FIFO_RD_ERROR and sticky |
| 0x024 | IRQ_ENABLE | RW | [31:28] |
| 0x028 | IRQ_PENDING | RO | IRQ_STATUS & IRQ_ENABLE |

Reset: FIFOs empty, all RW registers and IRQ_STATUS 0, so STATUS reads 0xA000_0000 (EMPTY and SEND_THRESHOLD_ACTIVE).

## Interrupts

- WR_THRESHOLD / RD_THRESHOLD set on a 0 -> 1 change of the matching STATUS bit, whatever caused it (push, pop by the other port, threshold write). Reset itself is not an edge.
- With a threshold of 0, the first word arriving sets RD_THRESHOLD on the receiver and draining the send FIFO to empty sets WR_THRESHOLD on the sender.
- `irq_cva6` / `irq_ibex` are levels: high while that port's IRQ_PENDING is nonzero and that port is out of reset.
- `irq_ibex` goes to INTMAP `kSrcMailboxIbex` (Ibex `irq_fast_i[14]`). HAS has no consumer for `irq_cva6`.

## Reset

- `presetn0` low resets port 0 (CVA6) and `presetn1` low resets port 1 (Ibex) (HDS 8.6). The port's IRQ_STATUS, IRQ_ENABLE and thresholds return to reset values, its IRQ is low, its reads return 0 and its writes are ignored with an OK response. The other port keeps its registers and keeps working.
- Asserting either reset empties both FIFOs (HDS 3: no data persistence across endpoint reset). Words written by the other port while one port is held stay in the FIFO.
- A port in reset records no events, and releasing a reset creates no threshold event: STATUS starts with SEND_THRESHOLD_ACTIVE = 1 and RECEIVE_THRESHOLD_ACTIVE = 0 for empty FIFOs (HDS 8.6 step 3).
- The model is untimed: no APB wait state and no clock-domain crossing latency.

## VP choices and open questions

| Topic | VP choice | Status |
| --- | --- | --- |
| FIFOs on a one-port reset | Both FIFOs emptied when either reset asserts | HDS: "Data persistence across endpoint reset" unsupported, no exact rule |
| Threshold width | 32-bit RW (HDS register table) | HDS test plan reads back 5-bit values |
| Offsets 0x1000 and above | PSLVERR like other unmapped offsets | HDS ADDR_WIDTH = 12, HAS window 64 KiB |
| CVA6 mailbox interrupt | `irq_cva6` provided, not routed | HAS and Common Spec give MAILBOX_CVA6 no PLIC source: CVA6 polls |
| Timing | Untimed: one APB wait state and CDC latency not modeled | Functional model |
