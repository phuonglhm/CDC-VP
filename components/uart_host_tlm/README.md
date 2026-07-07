# uart_host_tlm — host-side byte source/sink for UartTLM

`cdc::components::uart_host_bridge` gives the VP process an input path into a
`UartTLM` RX FIFO (and forwards firmware TX bytes back out). It exists for the
ROM-code boot flow (`docs/romcode_boot_hw_plan.md`, Phase 2): the boot
sequence's "send request over USART0, wait for the PC host tool's response"
branch is dead without a way to feed bytes into UART0 from outside the VP.

Not a bus peripheral: no TLM socket, no registers. It binds to the UART from
the *pin* side:

```
             ┌────────────┐  rx_out ──► UartTLM::rx (sc_buffer)
 TCP client ─┤ uart_host_ │
 or file     │   bridge   │  tx_in  ◄── uart tx signal
             └────────────┘
```

## Backends (configure before `sc_start()`)

| Call | Behavior |
|---|---|
| `listen_on(port, wait_for_client=false)` | TCP server on `127.0.0.1:port` (port 0 = ephemeral, see `listen_port()`). Client bytes → RX; firmware TX → client. One client at a time; reconnects allowed. `wait_for_client=true` blocks the sim at t=0 until a client connects. |
| `replay_file(path, start_delay=0)` | CI backend: file bytes → RX starting at `start_delay`, one byte per `byte_interval`. |

Both may be active: the file replays first, then the socket poll loop runs.
Unconfigured, the bridge idles (its thread exits at t=0); ports must still be
bound.

## Timing caveats

- No line-rate modeling (19200 8N1 on the real part is cosmetic here).
  `byte_interval` (default 10 µs) only paces injection so each byte gets its
  own delta; `poll_interval` (default 100 µs) is the socket poll cadence.
- Sim time usually runs much faster than wall clock: for interactive host
  tools use a long `--sim-ms` or `wait_for_client=true`, otherwise the sim can
  end before the tool connects.
- A stalled/slow client never stalls the sim: TX forwarding is non-blocking
  best-effort (bytes to a full socket buffer are dropped).

## Platform hookup (VP_FX1_Full_SoC)

CLI: `--uart0-socket <port>`, `--uart0-wait`, `--uart0-rx-file <f>`.

## Test

`tests/test_uart_host_bridge.cpp` (needs `CDC_BUILD_TESTS=ON`): file replay
into a real `UartTLM`, then TCP loopback both directions.
