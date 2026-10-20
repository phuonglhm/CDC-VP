# FU3 MAILBOX functional contract

## Sources and scope

FU3 HAS v2.0 is the specification: the block list (items 45-46) defines MAILBOX_CVA6 (APB4, PERIBUS_1) and MAILBOX_IBEX (APB, PERIBUS_0) as the two endpoints of inter-CPU messaging, the memory map places them at 0x5002_0000 and 0x4404_0000, and section 10.2 routes `irq_mailbox_ibex` to Ibex `irq_fast_i[14]`.
HAS v2.0 has no register map and no mailbox HDS exists yet. Register offsets, fields and FIFO behavior follow the RTL team's IP_Feature_Spec (sheet MAILBOX, V1.0) as a temporary reference, pending mentor approval (obtained directly from the RTL team, not an issued specification). Every point where that sheet is wrong or silent is a VP choice listed below.
The source documents are private and are not included in this package.

## Structure

One `fu3_mailbox::Mailbox` with two target sockets: `cva6_socket` (port 0, CPU0) and `ibex_socket` (port 1, CPU1).
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
- `irq_cva6` / `irq_ibex` are levels: high while that port's IRQ_PENDING is nonzero and `reset_n` is high.
- `irq_ibex` goes to INTMAP `kSrcMailboxIbex` (Ibex `irq_fast_i[14]`). HAS has no consumer for `irq_cva6`.

## Reset

`reset_n` low clears both FIFOs and both ports, deasserts both IRQs, reads return 0 and writes are ignored with an OK response. The model is untimed: no APB wait state and no clock-domain crossing latency.

## VP choices and open questions

| Topic | VP choice | Status |
| --- | --- | --- |
| CTRL.EN at 0x000 | Not modeled: the row is the sheet template (same row in other sheets) and collides with FIFO_WR at 0x000 | Ask for the HDS register map |
| FIFO_WR access "W0" | Write-only | Sheet typo |
| IRQ_PENDING[29] formula uses bit 30 | IRQ_STATUS[29] & IRQ_ENABLE[29] | Sheet typo |
| Port 0/1 to CPU | Port 0 = CVA6 (CPU0 sheet), port 1 = Ibex (CPU1 sheet) | Confirm |
| Which accesses raise PSLVERR | Full/empty FIFO, size, alignment, PSTRB, unmapped, WO read, RO write | Sheet says "illegal or invalid accesses" only |
| FIFO depth | 32 words | Sheet default, configurable |
| CVA6 mailbox interrupt | `irq_cva6` provided, not routed | HAS Table 11-1 has no PLIC source: CVA6 polls |
| Reset domains | One `reset_n` for the whole block | CRM sheet has separate mailbox_cva6 / mailbox_ibex resets and gates |
| Timing | Untimed and one APB wait state and CDC latency not modeled | Functional model |
