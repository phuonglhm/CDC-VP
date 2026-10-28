# Usage and integration

## Connect the component

Include `fu3/dma/ap_dma.h` and instantiate `fu3::dma::ApDma`. Provide a positive
`peripheral_cycle` when using peripheral transfers. Its default is zero, which
permits memory-to-memory transfers only.

```cpp
fu3::dma::EngineConfig config;
fu3::dma::ApDma dma{"dma", sc_core::sc_time(10, sc_core::SC_NS), config};
```

Bind every socket and port:

| Interface | Connection |
|---|---|
| `target_socket` | Host or bus initiator issuing MMIO accesses with local DMA offsets |
| `master_socket` | Shared memory/peripheral bus accepting physical addresses |
| `reset_n` | Active-low reset signal |
| `irq` | Level interrupt signal or interrupt-controller input |
| `rx_request` | 32-bit peripheral read-request bitmap |
| `tx_request` | 32-bit peripheral write-request bitmap |
| `rx_clear` | 32-bit read-service acknowledgement bitmap |
| `tx_clear` | 32-bit write-service acknowledgement bitmap |

For a direct host-to-DMA connection, the platform wiring looks like this. `host`
has an initiator named `cpu`; `memory` has a target named `target_socket`:

```cpp
sc_core::sc_signal<bool> reset_n, irq;
sc_core::sc_signal<sc_dt::sc_uint<32>> rx, tx, rx_clear, tx_clear;

host.cpu.bind(dma.target_socket);
dma.master_socket.bind(memory.target_socket);
dma.reset_n(reset_n);
dma.irq(irq);
dma.rx_request(rx);
dma.tx_request(tx);
dma.rx_clear(rx_clear);
dma.tx_clear(tx_clear);
```

Keep both request bitmaps at zero for memory-only transfers. Peripheral ID 0
selects memory and never needs a request. IDs 1 through 31 use their corresponding
bitmap bits; bit 0 is reserved.

When connecting through a router, allocate an MMIO aperture of at least `0x1100`
bytes and subtract the chosen DMA base before forwarding transactions to
`target_socket`. Source, destination, and descriptor addresses remain physical
32-bit bus addresses. The host and DMA must see the same backing memory.

Bus targets must support concurrent blocking transport calls. They may wait,
annotate delay, or do both. Preserve payload data and extension lifetimes until
the call and its annotated delay complete. The [M2M test](../tests/test_m2m.cpp)
provides a complete host, RAM target, and port-binding example.

Drive reset low and allow the simulation to process it before releasing it.
Perform MMIO operations from a SystemC thread, since an annotated arrival delay
may cause the target to wait.

## Access registers

MMIO accepts aligned 32-bit reads and writes, little-endian register bytes,
streaming width of at least four bytes, and no byte enables. For channel `n`,
add `0x100 * n` to its register offsets. Global offsets already include `0x1000`.

The following helpers belong in a host module with an initiator socket named
`cpu`. Call them from its `SC_THREAD`:

```cpp
std::uint32_t access32(bool write, std::uint32_t offset, std::uint32_t value = 0) {
    unsigned char bytes[4]{};
    for (unsigned i = 0; i < 4; ++i) bytes[i] = (value >> (8 * i)) & 0xff;
    tlm::tlm_generic_payload payload;
    payload.set_command(write ? tlm::TLM_WRITE_COMMAND : tlm::TLM_READ_COMMAND);
    payload.set_address(offset);
    payload.set_data_ptr(bytes);
    payload.set_data_length(4);
    payload.set_streaming_width(4);
    sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
    cpu->b_transport(payload, delay);
    if (delay != sc_core::SC_ZERO_TIME) sc_core::wait(delay);
    if (payload.get_response_status() != tlm::TLM_OK_RESPONSE)
        SC_REPORT_FATAL("DMA_HOST", payload.get_response_string().c_str());
    value = 0;
    for (unsigned i = 0; i < 4; ++i) value |= std::uint32_t(bytes[i]) << (8 * i);
    return value;
}

void write32(std::uint32_t offset, std::uint32_t value) {
    access32(true, offset, value);
}

std::uint32_t read32(std::uint32_t offset) {
    return access32(false, offset);
}
```

Unmapped offsets and misaligned addresses return `TLM_ADDRESS_ERROR_RESPONSE`.
Invalid payload lengths or streaming widths return `TLM_BURST_ERROR_RESPONSE`.
Byte enables return `TLM_BYTE_ENABLE_ERROR_RESPONSE`. Unsupported commands,
accesses during reset, writes to read-only registers, reads from write-only
registers, and invalid starts return `TLM_COMMAND_ERROR_RESPONSE`. Reserved
register bits are masked on writes.

## Copy memory

Wait for the channel to become inactive before changing its configuration.
Disabling an active channel pauses new grants but does not make configuration
registers writable. Set a nonzero burst limit in both directions: the reset
values of `CH_READ_CONFIG` and `CH_WRITE_CONFIG` have a zero burst limit.

This example copies 64 bytes from `0x1000` to `0x8000` on channel 0. Both ranges
must be mapped in the shared RAM. It uses the helpers above:

```cpp
const std::uint32_t ch = 0x000; // Channel 0; use 0x100 * n for channel n.
write32(ch + 0x40, 0);          // Disable while configuring an idle channel.
write32(ch + 0xa4, 0x1fff);     // Acknowledge old interrupt sources.
write32(ch + 0x00, 0x1000);     // Source address.
write32(ch + 0x04, 0x8000);     // Destination address.
write32(ch + 0x08, 64);         // Byte count.
write32(ch + 0x0c, 3);          // SET_INT=1, LAST=1.
write32(ch + 0x10, 0x81010010); // INCR, one token, 16-byte read burst limit.
write32(ch + 0x14, 0x81010010); // INCR, one token, 16-byte write burst limit.
write32(ch + 0x18, 0);          // No endian conversion.
write32(ch + 0x20, 0);          // Both endpoints are memory.
write32(ch + 0x24, 0);          // Use integration CACHE/PROT defaults.
write32(ch + 0xa8, 0x1fff);     // Enable all interrupt sources.
write32(ch + 0x40, 1);          // Enable does not start a transfer.
write32(ch + 0x44, 1);          // Start.

for (unsigned retry = 0; retry < 10000 && read32(ch + 0x48); ++retry)
    sc_core::wait(1, sc_core::SC_NS);
if (read32(ch + 0x48)) SC_REPORT_FATAL("DMA_HOST", "DMA did not become inactive");
const auto raw = read32(ch + 0xa0);
if (raw != 1) SC_REPORT_FATAL("DMA_HOST", "DMA failed or completion is missing");
write32(ch + 0xa4, 1);          // Acknowledge one completion event.
```

Choose a host polling deadline suitable for the connected memory and configured
timers. Completion occurs after all writes have responded. Check raw error bits
even when interrupt delivery is masked. Read [the interrupt reference](registers.md#interrupt-sources)
to interpret errors.

Write a channel bitmap to global offset `0x1048` to start several configured
channels together. For example, `write32(0x1048, 3)` starts channels 0 and 1.
Every selected channel must be startable; otherwise none of the selected channels
starts. A start requires an enabled, inactive channel, no pending work, a valid
nonzero byte count, and nonzero read/write burst limits.

## Transfer peripheral data

Use a fixed address for a peripheral data register and an incrementing address
for memory. Configure only one endpoint as a peripheral. The table shows settings
for single-byte services; use the same enable, interrupt, and start sequence as
the memory example:

| Register | USART0 RX to memory | Memory to SPI0 TX |
|---|---|---|
| `CH_CMD_READ_ADDR` | Platform RX data-register address | Physical RAM source address |
| `CH_CMD_WRITE_ADDR` | Physical RAM destination address | Platform TX data-register address |
| `CH_CMD_TRANSFER_SIZE` | Total bytes to receive | Total bytes to send |
| `CH_CMD_CONTROL` | `3` | `3` |
| `CH_READ_CONFIG` | `0x01010001`: FIXED, one-byte limit | `0x81010010`: INCR, 16-byte limit |
| `CH_WRITE_CONFIG` | `0x81010010`: INCR, 16-byte limit | `0x01010001`: FIXED, one-byte limit |
| `CH_PERIPHERAL_CONFIG` | `0x00000302`: RX ID2, three recovery cycles | `0x02010000`: TX ID1, two recovery cycles |

For external requests, assert bit `id` in `rx_request` or `tx_request`. After a
successful service the corresponding clear bit pulses for one `peripheral_cycle`.
RX clear follows the final read response; TX clear follows the final write
response. Drop the request before presenting another credit; a request held high
does not cause repeated services.

Software can supply credits by writing `1u << id` to `PERIPHERAL_RX_REQUEST`
(`0x1050`) or `PERIPHERAL_TX_REQUEST` (`0x1054`). These writes replace the stored
bitmap. The serviced bit clears automatically after success. Wait for each
service acknowledgement before posting the next credit. Use one request source
per endpoint to avoid combining software and external credits unintentionally.

Each request permits one service, whose byte count depends on burst limit,
alignment, remaining bytes, and FIFO space. To receive or send one byte per
request, use a one-byte burst limit on the peripheral side. Larger FIXED services
use several `b_transport` calls at the same address, followed by one clear pulse.

Recovery delay starts when a service succeeds and applies to every channel using
that peripheral ID in that direction. A new command, channel change, or register
write does not remove the delay. The next service waits for both the clear pulse
and recovery delay to expire. RX and TX have separate recovery state; other IDs
remain available. Reset clears recovery state.

### Use the peripheral adapter

Include `fu3/dma/peripheral_adapter.h` to use `PeripheralAdapter`. Bind its
`clock` and `reset_n`, and use a clock period matching the core's
`peripheral_cycle`.

| Interface | Usage |
|---|---|
| `request_valid`, `request_ready` | Submit one request on a rising clock edge when both are high |
| `request_kind` | `RequestKind::Single=0`, `Burst=1`, `Flush=2`; value 3 is rejected |
| `drlast` | Metadata latched with the request; it does not terminate a DMA command |
| `dma_request` | Pack into the selected core request-bitmap bit |
| `dma_clear` | Unpack from the matching core clear-bitmap bit |
| `ack_valid`, `ack_ready` | Consume acknowledgement on a rising clock edge when both are high |
| `ack_kind`, `ack_last` | Request metadata held stable with the acknowledgement |

SINGLE and BURST each hold one credit until `dma_clear`; the configured core burst
limit determines the service size. FLUSH acknowledges without issuing a data
request. Acknowledgement backpressure prevents accepting another request. Combine
multiple adapter requests in one bitmap-driving process to avoid multiple writers
on the same SystemC signal. See the [adapter test](../tests/test_adapter.cpp) for
a clocked bridge example.

## Convert byte order

Set `CH_MODE_CONFIG[29:28]` before starting:

| Value | Operation | Input bytes | Output bytes |
|---|---|---|---|
| `0x00000000` | None | `10 21 32 43` | `10 21 32 43` |
| `0x10000000` | Reverse each two-byte group | `10 21 32 43` | `21 10 43 32` |
| `0x20000000` | Reverse each four-byte group | `10 21 32 43` | `43 32 21 10` |

Groups start at the first byte of each command and continue across bursts and
peripheral requests. Two-byte mode requires an even transfer size; four-byte
mode requires a size divisible by four. Mode 3 and partial final groups are
rejected. MMIO byte order and descriptor byte order always remain little-endian.

## Chain commands

Keep `EngineConfig::command_chain=true` to enable chaining. Program the first
command through MMIO. If `CH_CMD_CONTROL.LAST=0`, bits 31:2 point to the next
descriptor. Each descriptor contains four little-endian 32-bit words:

| Descriptor offset | Value |
|---|---|
| `0x00` | Source address |
| `0x04` | Destination address |
| `0x08` | Byte count in bits 9:0 |
| `0x0c` | Control: SET_INT bit 0, LAST bit 1, next address in bits 31:2 |

For two copies, configure the first MMIO command as source `0x1000`, destination
`0x8000`, size `16`, and control `0x4001`. Store the following descriptor at
physical RAM address `0x4000` before starting:

```text
0x4000: 0x00001100   source
0x4004: 0x00008100   destination
0x4008: 0x00000010   byte count
0x400c: 0x00000003   SET_INT=1, LAST=1
```

The next command is fetched after the current command drains. Burst, endpoint,
endian, and AXI attribute configuration persists across the chain. Descriptors
must be four-byte aligned and readable by the DMA master. Set LAST on the final
command. Each SET_INT completion queues an event; clear bit 0 once per event.
A new channel start resets command and event counters, so consume any results
you need before restarting.

## Configure concurrency and timers

`CH_READ_CONFIG` and `CH_WRITE_CONFIG` control their directions independently.
For example, `0xc4010004` selects incrementing addresses, up to four outstanding
memory contexts, one arbitration token, and a four-byte burst limit. Peripheral
endpoints always have a single credit. Token count 0 is treated as one grant.

Set integration options through `EngineConfig` before constructing the module:

```cpp
fu3::dma::EngineConfig config;
config.transaction_timeout = sc_core::sc_time(1, sc_core::SC_US);
config.watchdog_timeout = sc_core::sc_time(10, sc_core::SC_US);
config.scheduler_quantum = sc_core::sc_time(1, sc_core::SC_NS);
config.phase_timeouts[static_cast<unsigned>(fu3::dma::BusPhase::ReadAddress)] =
    sc_core::sc_time(100, sc_core::SC_NS);
config.memory_read_cache = 3;
config.memory_write_cache = 6;
config.memory_read_prot = 5;
config.memory_write_prot = 2;
```

| Option | Behavior |
|---|---|
| `transaction_timeout` | Maximum logical transaction duration, including blocking and annotated delays; zero disables it |
| `watchdog_timeout` | Maximum enabled-channel interval without progress, including waiting for a peripheral request; zero disables it |
| `phase_timeouts[5]` | Per-phase thresholds ordered ReadAddress, ReadData, WriteAddress, WriteData, WriteResponse; zero disables an entry |
| `scheduler_quantum` | Minimum interval between dispatch batches; zero uses one simulation-resolution tick |
| `memory_read_cache`, `memory_write_cache` | Default memory CACHE attributes, range 0..15 |
| `peripheral_read_cache`, `peripheral_write_cache` | Default peripheral CACHE attributes, range 0..15 |
| `memory_read_prot`, `memory_write_prot` | Default memory PROT attributes, range 0..7 |
| `peripheral_read_prot`, `peripheral_write_prot` | Default peripheral PROT attributes, range 0..7 |
| `design_capability` | Reported design capability; bits 3:0 must equal 1; default is 1 |
| `command_chain` | Enables descriptor chaining; default is true |

All times, CACHE defaults, and PROT defaults are zero unless set explicitly.
A response exactly at its timeout threshold succeeds; exceeding the threshold
causes a timeout. Watchdog timing pauses while a channel is disabled and starts
a fresh interval when it is re-enabled.

Ordinary RAM targets can ignore `TransferExtension`. An AXI adapter can inspect
its channel, unit, beat, fixed ID4, CACHE/PROT, and write-strobe metadata. For phase
timers, call `enter_phase()` on the live payload extension before waiting in each
observed phase:

```cpp
auto* metadata = payload.get_extension<fu3::dma::TransferExtension>();
if (metadata) metadata->enter_phase(fu3::dma::BusPhase::WriteData);
// Perform the target's write-data wait here.
if (metadata) metadata->enter_phase(fu3::dma::BusPhase::WriteResponse);
```

Targets without phase reporting use ReadData for reads and WriteResponse for
writes. An adapter can report a response ID using `response_id_valid` and
`response_id`, and report protocol observations through `faults`. Keep the
extension attached and use it only during the payload's lifetime. Descriptor
fetches use memory attribute defaults; payload overrides do not apply to them.

## Handle interrupts, pause, and recover

`irq` is high whenever any channel has enabled raw interrupt bits. Use
`CORE_STATUS` to identify channels, then read their raw and masked status.
`CH_INTERRUPT_ENABLE` controls delivery without discarding raw status.

Writing `CH_INTERRUPT_CLEAR` clears selected error bits. Bit 0 acknowledges one
completion event; it stays asserted while more events remain. Command count
wraps at 12 bits and pending completion events saturate at 15.

To pause, write `CH_ENABLE=0`. Accepted transactions drain; remaining work resumes
when `CH_ENABLE=1`. A disabled channel can remain active. To reconfigure it, first
let it complete or recover through reset.

On a bus error or timeout, the channel stops issuing new work and drains accepted
transactions. Wait for `CH_ACTIVE_STATUS=0` and `CH_OUTSTANDING_STATUS=0`, inspect
and acknowledge errors, then reconfigure and start again. Peripheral errors do
not produce a clear pulse, so reset or recover the endpoint adapter as needed.

Active-low reset clears registers, counters, interrupt status, request state,
and recovery deadlines. Responses accepted before reset cannot complete a new
command. Bytes already written downstream remain committed; reset the external
target separately if its state must also be cleared. A target call that never
returns cannot be cancelled by the DMA; use a target or simulation watchdog to
handle that integration failure.

## Clock gate

`set_clock_enabled(false)` (default enabled) models the clock controller's gate:
the scheduler neither dispatches nor retires, and no new bus transfer starts; a
transfer already on the bus completes. Channel state is kept and the engine
continues on enable. With nonzero `transaction_timeout`/`watchdog_timeout`, the
time spent gated counts toward them.
