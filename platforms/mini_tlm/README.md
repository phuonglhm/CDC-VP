# mini_tlm

First SystemC/TLM bring-up slice for the CDC virtual platform.

This platform currently keeps the CPU stub, bus router, UART, and timer in one
small executable so the project has a runnable baseline before the reusable
`components/*` libraries are filled in.

```bash
cmake --preset debug
cmake --build --preset debug --target mini_tlm

./build/debug/platforms/mini_tlm/mini_tlm \
    -c platforms/mini_tlm/configs/default.yaml
```

Expected output:

```text
mini_tlm config: platforms/mini_tlm/configs/default.yaml
Hello from mini VP
Timer IRQ fired at 10ms
```
