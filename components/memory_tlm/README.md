# memory_tlm

Flat TLM-2.0 memory model usable as ROM or RAM (`cdc::components::memory_tlm`).

Addresses are region-local (`0 .. size-1`); the `bus_router` translates global addresses
before forwarding.

## Construction

```cpp
cdc::components::memory_tlm ram("ram", 0x1000);                 // RAM, 4 KiB
cdc::components::memory_tlm rom("rom", 0x1000, /*read_only=*/true);
```

## Protocol support

- `b_transport` — READ/WRITE with bounds check and `access_latency` (default 10ns).
  Writing to a ROM returns `TLM_COMMAND_ERROR_RESPONSE`; out-of-bounds returns
  `TLM_ADDRESS_ERROR_RESPONSE`.
- `transport_dbg` — backdoor, untimed read/write (clamped to size); ROM ignores writes.
- DMI (`get_direct_mem_ptr`) — exposes the backing store directly (read-only for ROM,
  read+write for RAM).
- `load(data, len, offset)` — backdoor load of an image (e.g. firmware) at a local offset.

## Known limitations

- No byte-enable / partial-strobe handling (full `data_length` copy).
- No wait-state model beyond a flat `access_latency`.
