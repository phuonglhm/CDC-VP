# Build, regression and evidence

Run all commands from the CDC-VP repository root in Linux/WSL. Requirements
are C++17, SystemC 2.3.4 and CMake/CTest 3.21 or later for JUnit output.
The full regression also uses GCC/gcov and Python 3's standard library.
No PDF, analog hardmacro model, additional Python packages or Internet access
is required to build or test the component.

```sh
bash components/FU3_Components/ADC/scripts/run_full.sh
```

The script runs Debug with coverage, Release and UBSan
(`-fsanitize=undefined -fno-sanitize-recover=all`), all with `-Werror`.
Set `SYSTEMC_HOME` and `JOBS` to override the installation prefix and build
parallelism. The library also enables `-Wall -Wextra -Wpedantic`.
Checks use exceptions and exit codes and remain active in Release/NDEBUG.

| Option | Default | Purpose |
| --- | --- | --- |
| FU3_ADC_BUILD_TESTS | BUILD_TESTING or CDC_BUILD_TESTS | Build and register regression tests |
| FU3_ADC_BUS_TESTS | ON when tests are enabled | Test through the existing FU3 bus |
| FU3_ADC_COVERAGE | OFF | Instrument adc.cpp with GCC/gcov |
| SYSTEMC_HOME | /opt/systemc-2.3.4 | SystemC installation prefix |

Build the ADC component independently without editing other components' CMake
files or sources:

```sh
cmake -S components/FU3_Components/ADC -B build/fu3-adc \
  -DSYSTEMC_HOME=/opt/systemc-2.3.4 -DCMAKE_BUILD_TYPE=Debug
cmake --build build/fu3-adc -j4
ctest --test-dir build/fu3-adc --output-on-failure
```

ADC is not connected to the repository root build. The bus tests reuse the
existing FU3 bus library through `add_subdirectory` in the standalone build;
they do not modify bus or CPU files.

## Test organization

The default suite contains 26 CTest cases: 19 direct scenarios and 7 scenarios
through the bus. Expected raw samples and channel values come from
literal data and independent calculations in the testbench. The model receives
D/EOC and does not generate its own sample oracle. The `randomized` scenario
uses seed `0xADC2026` for 160 sequences with n=1..16, channel=0..15,
data=0..4095 and varying timing.

The digital hardmacro stimulus drives D 12 cycles and EOC 12.5 cycles after
SAMPCLK falls. EOC deasserts 2.5 cycles after the next rising edge.
The `eoc_wait/eoc_policy` tests deliberately delay EOC or generate early pulses
and held-high levels.

`SPEC_CHECKS` counts assertions derived from explicit requirements.
`POLICY_CHECKS` includes VP choices, transport checks and consistency checks.
A scenario reporting `PASS` does not establish that every HDS requirement
is sufficiently defined. The report retains `hds_signoff: false` and records
the basis of each requirement.

```sh
ctest --test-dir build/fu3-adc -L integration --output-on-failure
ctest --test-dir build/fu3-adc -R '^fu3_adc_reset_transport$' -V
ctest --test-dir build/fu3-adc -R '^fu3_adc_timing_' -V
```

Each case has a 30-second CTest timeout and a 100ms simulation watchdog.
Polling loops are also bounded. The tests do not use wall-clock sleeps or
add a simulated hardware timeout.

## Artifacts

- `build/fu3-adc{,-release,-ubsan}/results.xml`: JUnit output for each profile.
- `configure.log`, `build.log`, `tests.log`: logs in each build directory.
- `build/fu3-adc/coverage-summary.txt`, `adc.cpp.gcov`: implementation coverage.
- `build/fu3-adc/verification-report.json`: pinned source document, requirements
  and profile results; reporting fails on failed/skipped or missing mapped tests.
- `Testing/Temporary/LastTest.log`: detailed CTest output.

See [verification-results.md](verification-results.md) for recorded results.
RTL comparison and CPU boot have not been performed for this delivery.
Analog behavior, APB pins and divider/CDC implementation are outside the
regression scope. Open HDS issues are listed in [spec-decisions.md](spec-decisions.md).
