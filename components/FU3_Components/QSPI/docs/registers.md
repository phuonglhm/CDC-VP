# Register interface (HDS V1.0)

Source: HDS Section 8, physical PDF pp46-51. AP_QSPIC local offsets are
0x00..0x3f; the low two address bits are ignored. Every access is a little-endian,
full 32-bit word. Unmapped reads return zero with error. Registers reset to
zero during reset; after release SR reads 0x00000004 because TXFTF reflects
an empty indirect-write FIFO, independent of EN.

| Offset | Register | Writable mask | Behavior |
| --- | --- | --- | --- |
| 0x00 | CR | 0xffff1fff | EN, persistent ABORT request, polling, word thresholds, IRQ enables, prescaler |
| 0x04 | DCR | 0x001f0701 | FSIZE, CSHT, CKMODE |
| 0x08 | SR | RO | TEF, TCF, TXFTF, RXFTF, SMF, STOF, BUSY; FIFO levels in words |
| 0x0c | FCR | 0x0000003f | W1C SR flags 0..5; reads zero |
| 0x10 | DLR | 0xffffffff | Byte count minus one; all ones is undefined length to flash end |
| 0x14 | CCR | 0x9f7fffff | Phase modes/sizes, DDR, SIOO, FMODE; may trigger |
| 0x18 | AR | 0xffffffff | Full range-check address; selected low bytes transmitted; may trigger |
| 0x1c | ABR | 0xffffffff | Selected low alternate bytes; change flushes cache |
| 0x20 | DR | 0xffffffff | TX word enqueue/RX word dequeue; polling last status; mapped reads zero |
| 0x24 | PSMKR | 0xffffffff | Compared polling bits; bytes beyond DLR+1 ignored |
| 0x28 | PSMAR | 0xffffffff | Expected polling status |
| 0x2c | PIR | 0x0000ffff | Poll gap in core cycles, at least CSHT minimum |
| 0x30 | PTR | 0xffffffff | Polling timeout in core cycles; no XIP inactivity meaning |
| 0x34 | DMACR | 0x0000000f | TX/RX DMA enable and overrun/underrun IRQ enable; always writable |
| 0x38 | DMASR | RO | TXOVRF bit 2, RXUDRF bit 3 |
| 0x3c | DMAFCR | 0x0000000c | W1C DMA error flags; reads zero |

SR bits 0..6 are TEF, TCF, TXFTF, RXFTF, SMF, STOF, BUSY. TXFLEVEL is
bits 13:8 and RXFLEVEL bits 21:16, both in 32-bit words. A zero-padded final
partial RX word counts as one valid word. CR IRQ enables at 21:16 correspond
to SR flags at 5:0. IRQ ORs these enabled flags with enabled DMASR errors.

CR bit 2 is PTCEN; bit 22 APMS; bit 23 PMM. TXFIFOTH is bits 12:8 and
RXFIFOTH bits 7:3. Threshold encodings mean field+1 words, including DMA.
PRESCALER bits 31:24 give divisor **field+2**, even for SDR and divisible by
four for DDR. Reset PRESCALER=0 is legal SDR; minimum DDR PRESCALER=2.

CCR FMODE bits 27:26 select write=0, read=1, polling=2, mapped=3.
IMODE/ADMODE/ABMODE/DMODE encode absent=0, single=1, dual=2, quad=3.
ADSIZE/ABSIZE select one through four low bytes; they are serialized MSB first.
Data bytes use DR low byte first. SIOO suppresses the instruction after the
first frame following a CCR write; instruction rate is always SDR.

| Mode | Trigger when EN=1 |
| --- | --- |
| Indirect write, no address/data | CCR write |
| Indirect write, address present, no data | AR write |
| Indirect write, data present | DR write |
| Indirect read or polling, no address | CCR write |
| Indirect read or polling, address present | AR write |
| Memory-mapped | AHB NONSEQ/SEQ cache miss |

All configuration is BUSY-locked except CR EN/ABORT/IRQ enables, DMACR,
FCR/DMAFCR and normal DR access. AR is ignored in mapped mode. EN clear lets
the current frame finish; disabled write-mode DR accesses retain TX data.
ABORT=1 remains pending until byte boundary and STOP completion; writing zero
has no effect. Completion flushes FIFOs and sets TCF while BUSY and ABORT
clear. Both DMA requests stay low until an accepted CCR write.

Range checking uses full AR only if ADMODE is present. Finite data frames
require AR+DLR+1 <= flash size; command-only and undefined-length frames require
AR < flash size. No indirect range check applies without an address phase.
AHB checks its original byte offset against FSIZE, then ignores its low bits.
