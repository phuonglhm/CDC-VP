# I2C validation

## Common Spec rebaseline (2026-10-21)

The current model passed the joint static/shared scoped regression and installed
package checks recorded in the bus [validation matrix](../../bus/docs/VALIDATION.md).
The full matrix passes 131/131 executions across the owned components, bus
consumers and FX1 coexistence. Source precedence, selected behavior at conflicts
and remaining platform requirements are in
[COMMON_REBASELINE.md](../../bus/docs/COMMON_REBASELINE.md).

The following older results retain their original source baselines. They do not
override the current model contract or establish full firmware boot execution.

Validation date: 2026-10-20. Environment: WSL Linux, GNU C++ 15.2, SystemC 2.3.4, C++17, Release configuration.

| Check | Result |
|---|---|
| Static library build | Passed |
| Static CTest: `i2c_behavior`, `i2c_integration` | 2/2 passed |
| Shared library build | Passed |
| Shared CTest: `i2c_behavior`, `i2c_integration` | 2/2 passed |
| Installed static consumer: configure, link, elaborate, run | Passed |
| Installed shared consumer: configure, link, elaborate, run | Passed |

Each configuration ran 1,961 assertions in the direct behavior executable and 1,559 in the bus integration executable, with zero failures. Counts include repeated MMIO response and delay checks while polling; they are not counts of distinct specification requirements. See TRACEABILITY.md for the actual scenarios and uncovered areas.

The main tests have simulation watchdogs and CTest wall-time limits. They use `sc_main`, real SystemC scheduling, independent fake serial peers, and the actual FU3 bus for integration. The library has no entry point. Installed-consumer validation uses the exported `fu3::i2c` target and exercises elaboration/reset after linking.

The review caught and fixed an initial arbitration rule that treated another master's STOP as loss while the local master was merely waiting for a free bus. A regression now verifies waiting without driving and proceeding after that STOP. No issue reached three unsuccessful repair attempts.

A packaging invocation initially failed because shell quoting split the semicolon-separated CMake prefix list. Explicit package-directory arguments resolved the invocation; no SystemC source or unrelated IP was changed.

## Reproduction

Use the README commands for each value of `BUILD_SHARED_LIBS`. After installing, configure the consumer with both package directories explicitly if the shell does not preserve CMake prefix lists:

```sh
cmake -S components/FU3_Components/i2c/tests/installed_consumer \
  -B build/i2c-consumer \
  -Dfu3-i2c_DIR=/path/to/i2c-install/lib/cmake/fu3-i2c \
  -DSystemCLanguage_DIR=/path/to/systemc/lib/cmake/SystemCLanguage
cmake --build build/i2c-consumer --parallel
./build/i2c-consumer/i2c_consumer
```

Native Windows DLL/toolchain validation and actual CPU firmware boot were not performed. No exhaustive electrical or multi-master protocol compliance is claimed. The model's deliberate timing and register-adaptation decisions remain visible in MODEL_CONTRACT.md.
