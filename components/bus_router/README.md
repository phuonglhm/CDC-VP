# bus_router

Generic TLM-2.0 address-decoding bus router (`cdc::components::bus_router`).

One target socket faces the initiator (CPU). The number of peripheral initiator sockets
is fixed at construction (SystemC requires sockets be created during module construction);
`add_target()` then maps a region onto the next socket and returns it to bind.

## Address decode

Each region is a half-open `[base, base + size)`. On a transaction the router finds the
region containing the address, translates to region-local (`addr - base`), forwards, then
restores the original address. No matching region returns `TLM_ADDRESS_ERROR_RESPONSE`.

Regions are checked in registration order; the first match wins. The caller is
responsible for not registering overlapping regions.

## Usage

```cpp
#include <bus_router.h>

cdc::components::bus_router bus("bus", /*num_targets=*/3);
bus.add_target(0x10000000, 0x1000).bind(uart.socket);
bus.add_target(0x10001000, 0x1000).bind(timer.socket);
bus.add_target(0x80000000, 0x1000).bind(ram.socket);

cpu.bus_socket.bind(bus.target_socket);
```

## Forwarded protocol

- `b_transport` — yes (with address translation).
- `transport_dbg` — yes (with address translation).
- DMI (`get_direct_mem_ptr` / `invalidate_direct_mem_ptr`) — **not yet** (TODO). Targets
  may advertise DMI but the router does not forward it, so accesses fall back to
  `b_transport`.
