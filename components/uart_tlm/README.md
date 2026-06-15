# uart_tlm

Memory-mapped TLM-2.0 UART model. TX-only.

## Register map

| Offset | Name   | Access | Description                         |
|-------:|--------|--------|-------------------------------------|
| `0x0`  | TXDATA | W      | Write a byte; low 8 bits are emitted to the output stream |

Any other offset, or a read, returns `TLM_ADDRESS_ERROR_RESPONSE`.

## Usage

```cpp
#include <uart_tlm.h>

cdc::components::uart_tlm uart("uart");           // emits to std::cout
// or redirect output / tune latency:
cdc::components::uart_tlm uart("uart", my_stream, sc_core::sc_time(5, sc_core::SC_NS));

bus.uart_socket.bind(uart.socket);
```

## Known limitations

- No RX path, no status/control registers, no FIFO.
- Single-byte writes only; multi-byte payloads emit only the first byte.
