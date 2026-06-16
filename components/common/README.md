# common

Header-only shared types and helpers for `cdc::components::*`.

## Contents (`common.h`, namespace `cdc::components::common`)

- `struct address_range { uint64_t base, size; bool contains(addr); }` — half-open
  memory-mapped region `[base, base + size)`. Used by `bus_router` for address decode.
- `void log(who, msg)` — minimal logger to `std::cerr` (kept off `std::cout` so it does
  not mix with peripheral output such as UART).

Header-only `INTERFACE` library; link with `cdc::components::common`.
