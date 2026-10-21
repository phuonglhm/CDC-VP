# FU3 bus repository integration

The reusable component is a C++17 SystemC/TLM library. It supplies bus behavior;
the platform supplies masters, targets and boot orchestration. Read the
[model contract](MODEL_CONTRACT.md) before connecting firmware-facing adapters.

## Files and build dependencies

Keep `CMakeLists.txt`, `cmake/`, `include/`, `src/`, `tests/`, `README.md`, `docs/`
and `.gitignore` together. Generated `build*/` directories and optional local
`scripts/` are ignored. Confidential specifications and extracted review artifacts
must remain outside the public package.

Provide a compatible SystemC installation exporting `SystemC::systemc` through
`SystemCLanguage`. The library propagates that dependency and C++17 requirements.
Use the [README commands](../README.md#build-and-test) for reproducible builds;
no ignored script is needed after cloning.

Installed usage:

```cmake
find_package(fu3-bus CONFIG REQUIRED)
add_executable(my_vp main.cpp)
target_link_libraries(my_vp PRIVATE fu3::bus_system)
```

Set `CMAKE_PREFIX_PATH` to the bus installation and SystemC prefixes. Alternatively,
use `add_subdirectory` on the bus source and link the same `fu3::bus_system` target.
Set `FU3_BUS_BUILD_TESTS=OFF` when the parent project does not need its regressions.
`BUILD_SHARED_LIBS` selects static or shared output.

Executables must define `int sc_main(int, char**)`. The reusable library has no
entry point. Do not replace `sc_main` with a regular `main` or manually omit the
imported SystemC dependency. Match compiler, ABI and SystemC build settings.
Native Windows DLL operation is not covered by the recorded Linux validation.

## Platform wiring sequence

1. Start with `BusConfig::fu3()` and disable absent targets before construction.
   A disabled window returns an unmapped-address error; it is not a fake IP.
2. Construct `BusSystem`, bind each enabled target and connect required masters.
   Test-only peers may stand in for unfinished IPs when their behavior is labelled.
3. Supply `ScopedAccess` for every default FU3 functional request. Identify fetches,
   beat sizes and ordering IDs correctly. Issue blocking transactions in threads.
4. Check target width/strobe and side-effect contracts. Do not introduce generic
   CSR read-modify-write. Timer full-word restrictions are documented in the
   [Timer contract](../../timer/docs/MODEL_CONTRACT.md).
5. Load images through the explicit debug backdoor or a permitted functional
   loader. Debug access bypasses policy and cannot demonstrate CPU access rights.
6. Stop and drain IBEX requests before `finish_staging()`, then release CVA6 through
   the platform's reset controller. For system reset, stop masters and call
   `quiesce()` from a coordinator SC_THREAD outside transport callbacks. Reset
   IPs, call bus `reset()`, release IP resets, call `resume()`, restart masters.
   Stop debug loaders separately; a non-returning target cannot be drained.
7. Connect clocks, IP resets and interrupt routing in the platform. Bus reset does
   not reset attached devices; bus tests do not execute a software boot sequence.

## Review and validation

Run static/shared regressions and installed consumers using the README commands.
Enable `FU3_BUS_BUILD_IP_TESTS=ON` to run the production DMA/SPI/UART,
Timer/PLIC and WDT/reset scenarios. Native AP_DMA binds through the optional
`fu3_bus::DmaAdapter`; direct binding does not supply required metadata.
See [integration validation](INTEGRATION_VALIDATION.md) for fixture wiring.
Run the coexistence check when FX1 is present. Review the
[validation record](VALIDATION.md) for the distinction between recorded results
and unverified platform behavior.

Before publication, review the exact staged file list for private documents,
generated outputs and local helper scripts. Restoring these docs does not stage,
commit or push files. Existing staged user work remains unchanged.

When changing the bus contract, update the relevant tests, this documentation and
adapter expectations together. Keep explicit user decisions separate from HAS
requirements and configurable VP assumptions.
