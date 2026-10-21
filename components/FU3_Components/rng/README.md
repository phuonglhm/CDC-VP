# FU3 RNG behavioral VP

Common Spec v2.0 takes precedence over older endpoint restrictions. See the
[updated model contract](docs/MODEL_CONTRACT.md) for byte strobes and remaining VP decisions.

Models the five-register RNG HDS V1.0 interface for software data-flow tests. No additional hardware features or CSRs are introduced. This is not a physical entropy model and does not certify random quality. The existing `components/trng_tlm` has a different ABI and is not used.

Connect `Rng::socket` to `bus.target("RNG")`, bind `reset_n` and `irq`, and provide a `Config` with the platform clock period and an explicit `sample_bit(index)` callback. The callback is a simulation input, must not wait or throw, and may return deterministic test bits. There is deliberately no default random-number generator or firmware seed register.

The bus window is `0x44030000..0x4403FFFF` on PERIBUS_0; direct calls use local offsets. Platform IRQ destinations are PLIC ID21/source index20 and Ibex fast IRQ10. Tests use fake sinks, not a PLIC or CPU implementation. Clear VALID with NEXT/RESTART before completing a level interrupt at the PLIC; reading DATA does not acknowledge RNG.

## Build

```sh
cmake -S components/FU3_Components/rng -B build/fu3-rng \
  -DSYSTEMC_HOME=/path/to/systemc -DCMAKE_BUILD_TYPE=Release
cmake --build build/fu3-rng --parallel
ctest --test-dir build/fu3-rng --output-on-failure
cmake --install build/fu3-rng --prefix /path/to/install
```

Set `BUILD_SHARED_LIBS=ON` for shared libraries, `BUILD_TESTING=OFF` for the standalone IP library. Consumers use `find_package(fu3-rng CONFIG REQUIRED)` and `fu3::rng`, with their own `sc_main`. SystemC and consumer compiler/ABI must match.

Use aligned 32-bit MMIO; write byte enables implement APB PSTRB. No generic CSR read-modify-write is performed. Adapter shape errors are distinguished from valid APB transfers to invalid addresses/RO registers. See [MODEL_CONTRACT.md](docs/MODEL_CONTRACT.md) for the explicitly provisional collection schedule and [VALIDATION.md](docs/VALIDATION.md) for evidence and remaining work.
