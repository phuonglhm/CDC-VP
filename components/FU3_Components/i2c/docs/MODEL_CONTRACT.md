# I2C model contract

The implementation targets software-visible behavior described by I2C HDS V1.1 and the FU3 architecture. The following gap resolutions were selected under the user's authorization to make implementation decisions. They are model decisions, not newly discovered specification facts.

## Register and bus contract

Registers use little-endian 32-bit accesses. The published header supplies offsets and bit masks. Decode uses address bits [6:2] across the local 64-KiB window: upper bits alias and low address bits are ignored. Reserved registers return zero and ignore writes. RO writes are ignored. Reserved CTRL and CMD bits read zero; implemented values are masked to their defined widths.

The endpoint has no PSTRB. The TLM adapter accepts four-byte payloads with all bytes enabled, rejects narrower or sparse transfers, and never performs read-modify-write on CSRs. Malformed TLM requests receive adapter errors; valid reserved-register accesses succeed, consistent with the IP's fixed-low PSLVERR. Address aliases are observable directly; the upstream bus may reject an unaligned transaction before it reaches the endpoint.

Annotated delay is consumed once. MMIO is serialized at the next HCLK processing boundary. There is no DMI. Debug reads are non-destructive and untimed; debug writes are rejected. Reset aborts pending MMIO with a generic adapter error.

## Command decisions

- CMD, including IACK, is ignored while CTRL.EN is zero. Initialization must enable the core before using IACK to clear retained IF.
- While a command is active, new action bits are ignored. IACK still works and cannot overwrite the active command. There is one active command and no command FIFO.
- RD and WR together are invalid and ignored. START-only is supported. STOP-only and byte commands without START require local bus ownership; otherwise they are ignored. Slave mode ignores master action bits but accepts IACK.
- TIP covers the entire accepted command, including waiting for a free bus, START, byte, and requested STOP. IF is raised after command completion. TBUF delays a subsequent initial START, not completion of the previous STOP.
- All accepted CMD fields, including ACK selection, clear on completion or abort. IACK reads zero. TX is snapshotted when a master command is accepted.
- A fresh START clears AL. IACK clears IF only. Loss aborts the command, clears TIP, releases both outputs, and sets AL and IF without generating STOP. Disabling aborts without inventing a completion event, retains sticky status, and releases the outputs. Reset clears everything.
- Mode changes abort the current local operation; firmware should switch modes while idle. Timing configuration should be changed only while disabled, as prescribed by the specification.

## Slave behavior

The slave never drives SCL. Address zero is not acknowledged as general call. Address mismatch ignores the transaction until START or STOP. Repeated START begins a fresh address phase and discards partial data. DIR retains the last matched direction. ACTIVE lasts from an address match until STOP, repeated START, disable, or reset; after master NACK it remains associated with the addressed transaction until its termination.

RX is replaced when eight data bits have been sampled, sets RX_VALID and IF, and is acknowledged without FIFO backpressure. Reading RX does not clear status. Slave TX latches at the falling edge ending the address ACK and each subsequent master ACK. A write during a byte affects only a later latch. Without refill, the unchanged TX register is sent again. After master NACK the slave releases SDA and ignores further data until START/STOP. TX_DONE and IF are set when the master ACK/NACK is sampled; MASTER_NACK records NACK.

SLAVE_STATUS sticky flags are W1C, independently of IF/IACK. Address match and STOP do not independently set IF. MMIO is processed before serial events at the same model clock boundary, so a new event wins over a software clear. An RX read on that boundary sees the previously completed byte; the incoming byte is then retained for the next read.

## Timing and reset

The model samples pins at HCLK boundaries through two synchronizer stages and an input filter. Programmable filter qualification uses TSP+1 stable samples. Legacy filtering uses a three-sample majority with interval `max(1, (prescaler+1)/4)` HCLK cycles; this interval is an explicit approximation because V1.1 does not provide its formula.

Programmable timing uses each timing register plus one HCLK period. Master phases preserve programmed minimum holds, setup, and free-bus delays. Legacy nominal low/high durations use 3/2 prescaler units. Synchronization, filtering, and phase scheduling add latency: pin frequency is not an RTL cycle-accuracy claim. External low SCL stalls released-high phases indefinitely, with no invented timeout. Configure data hold below the low duration; the VP clamps an excessive hold to the low duration and retains a minimum data-setup step.

Reset is asynchronous active-low and releases outputs even between HCLK boundaries. Core disable is not reset. No electrical metastability, runtime clock gating/frequency changes, simultaneous master/slave operation, FIFO, DMA, 10-bit address engine, general call, or slave-generated stretching is claimed.
