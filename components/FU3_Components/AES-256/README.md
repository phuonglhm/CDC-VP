# FU3 AES-128 / AES-256 SystemC/TLM VP

Functional single-block AES peripheral based on the supplied
`FTEL_CDC_IP_AES_Specification.pdf` (22 pages, supplied 2026-10-21). Supports AES-128/256
encryption and decryption, key expansion, INIT/NEXT, READY/VALID polling,
write-only key/input registers and byte strobes. No external crypto library
is needed.

## Specification baseline

This component uses the following supplied AES specification:

| Item | Baseline |
| --- | --- |
| Document | `FTEL_CDC_IP_AES_Specification.pdf` (22 pages) |
| Latest revision in document history (p.3) | **V2.0 (April 03, 2026)** |
| Version on cover | V1.0 (March 20, 2026) |
| Reviewed against this model | October 21, 2026 |
| SHA-256 | `b468e39173e614d0c682c39b43eb6aebc7e269d1830375955cb2b55ad5a8b31f` |

The cover and revision history disagree. V2.0 is the latest revision listed
in the history; the filename and SHA-256 above identify the exact PDF used
for this model. See [spec decisions](docs/spec-decisions.md) for unresolved
specification conflicts.

**This is a firmware-oriented VP profile, not an exact model of the delivered
RTL wrapper.** Section 6.2 (p.10) now specifies synchronous active-low reset,
which the model already uses. Appendix B (p.17) still describes the old reset
defect; CSR address translation and result decode also remain unresolved.
The choices below allow firmware to process and read a complete AES block.
See [registers](docs/registers.md) and [spec decisions](docs/spec-decisions.md).

CTest logs and source coverage are stored under `build/`.
Unresolved HDS items and current model conventions are documented in
[spec-decisions.md](docs/spec-decisions.md).

## Build and run

From the repository root in Linux/WSL, using C++17 and SystemC 2.3.4:

```sh
cmake -S components/FU3_Components/AES-256 -B build/fu3-aes \
  -DSYSTEMC_HOME=/opt/systemc-2.3.4 -DCMAKE_BUILD_TYPE=Debug \
  -DFU3_AES_BUILD_TESTS=ON
cmake --build build/fu3-aes -j4
ctest --test-dir build/fu3-aes --output-on-failure
./build/fu3-aes/test_fu3_aes aes256
```

On PowerShell, enter WSL first with `wsl`, then
change to the repository root and run the commands above.
The standalone build avoids CPU/ISS and other SoC dependencies. Its default
suite has 35 tests (seven SPEC and 28 POLICY), including three real FU3 bus tests. Set
`FU3_AES_BUS_TESTS=OFF` to build 32 tests without the bus library, or
`FU3_AES_BUILD_TESTS=OFF` for the library only.

For all 39 cases, adding four REFERENCE tests with 1,158 independently generated OpenSSL vectors,
Debug/UBSan/Release builds and GCC source coverage, run in Linux/WSL:

```sh
bash components/FU3_Components/AES-256/scripts/run_full.sh
```

This verification script requires Python 3, the OpenSSL CLI, GCC/gcov and
CMake/CTest >=3.21. OpenSSL is used only to generate reference test data;
the AES model library does not depend on it. For one build, enable
`-DFU3_AES_ORACLE_TESTS=ON`. See [testing.md](docs/testing.md) for the output files.

Run categories independently after building:

```sh
ctest --test-dir build/fu3-aes -L '^spec$' --output-on-failure
ctest --test-dir build/fu3-aes -L '^vp_policy$' --output-on-failure
ctest --test-dir build/fu3-aes -L '^reference$' --output-on-failure
```

Only the seven core tests with literal FIPS 197 expectations (explicitly
referenced by HDS p.7) issue `SPEC_PASS/FAIL`. OpenSSL results never issue
SPEC verdicts. Existing MMIO/bus tests depend on unresolved reset/address/result
policies and therefore remain POLICY/REFERENCE evidence, not HDS sign-off.

The SPEC suite also checks all 248 published intermediate-state/round-key
records for Appendix C.1/C.3, using the committed `tests/fips197_rounds.txt`.
An optional read-only trace callback observes the actual core transform;
normal callers use the same transform without an observer. Normal builds
need no network/PDF parser; `scripts/import_fips_trace.py` documents optional
regeneration from the pinned NIST PDF.

The repository CMake build includes AES when `CDC_BUILD_FU3_SOC=ON`.
That switch also configures existing CPU and other FU3 components.
Library target: `fu3::aes` (archive `libfu3_aes.a`).

## Integration

```cpp
#include <fu3/aes/aes_controller.h>

fu3::aes::Aes aes{"aes"};
aes.clk(axi_clock);
aes.reset_n(axi_reset_n);
fabric.target("AES").bind(aes.socket);
```

The existing FU3 bus maps AES at **0x40000000**, with a 64-KiB bus window.
The VP implements only the first 0x400 bytes, including reserved CSR slots;
higher local addresses return a TLM address error. The fabric supplies local
byte offsets. Direct initiators also use local offsets, not SoC addresses.
FU3 initiators must attach `fu3_bus::ScopedAccess` metadata; the integration
test demonstrates CVA6 single accesses and a four-word INCR input burst.
No CPU instructions are executed in that test.

Call `b_transport` from an `SC_THREAD`. The socket accepts one aligned
32-bit word, with optional repeating byte enables. The FU3 fabric decomposes
bursts and expands narrow accesses to word lanes. Payload words are
little-endian; AES key/block words are ordered most significant word first.
For example, write numeric KEY0=`0x00010203` using payload bytes
`03 02 01 00`; AES sees the key bytes `00 01 02 03`.

Reset is synchronous active-low. INIT takes 16 rising edges and NEXT takes
64 by default, configurable via `fu3::aes::Config`; these are VP latencies,
not measured or specified RTL timings. Core progress freezes if the clock
stops. A bus call waiting for a clock also waits until the clock resumes;
the enclosing platform should provide its own watchdog.

## Firmware sequence

Using platform-specific `write32` and `read32` helpers:

```cpp
constexpr unsigned base = 0x40000000;
write32(base + 0x28, 3);              // Encrypt, AES-256.
for (unsigned i = 0; i < 8; ++i)
    write32(base + 0x40 + 4*i, key[i]);
write32(base + 0x20, 1);              // INIT.
// Use bounded polling in real firmware.
while (!(read32(base + 0x24) & 1)) {}
for (unsigned i = 0; i < 4; ++i)
    write32(base + 0x80 + 4*i, input[i]);
write32(base + 0x20, 2);              // NEXT.
while (!(read32(base + 0x24) & 2)) {}
for (unsigned i = 0; i < 4; ++i)
    output[i] = read32(base + 0xc0 + 4*i);
```

For AES-128 use CONFIG=1 and KEY0..KEY3. For decryption clear CONFIG bit 0;
changing direction alone can reuse the expanded key. Every accepted key
write or key-length change requires INIT again. Each NEXT handles one
16-byte block; software owns chaining or padding.

There is no IRQ, DMA handshake, AES-192, CBC/CTR/GCM hardware, IV, key vault,
AXI channel waveform model, or FIPS validation. The functional AES core is
simulation code, not a constant-time security library. Testing scope and
commands are in [testing.md](docs/testing.md).
