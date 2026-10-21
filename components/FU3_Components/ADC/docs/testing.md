# Build, verification and results

Run from the repository root in Linux/WSL. Requirements: C++17, SystemC 2.3.4,
CMake/CTest 3.21+ for JUnit output, GCC/gcov and Python 3 standard library.
The PDF and extra Python packages are not required for build/regression.

```sh
bash components/FU3_Components/ADC/scripts/run_full.sh
```

This builds Debug with coverage, Release and Debug with UBSan
(`-fsanitize=undefined -fno-sanitize-recover=all`), all with `-Werror`.
Assertions use exceptions/exit status and remain enabled in Release.

| Option | Default | Meaning |
| --- | --- | --- |
| SYSTEMC_HOME | /opt/systemc-2.3.4 | SystemC installation prefix; also accepted as a script environment variable |
| FU3_ADC_BUILD_TESTS | BUILD_TESTING or CDC_BUILD_TESTS | Build/register tests |
| FU3_ADC_BUS_TESTS | ON when tests enabled | Test with existing FU3 bus library |
| FU3_ADC_COVERAGE | OFF | Instrument the ADC library with GCC coverage |
| JOBS | 4 | Full-regression script build parallelism |

Standalone build commands are in the [README](../README.md).
ADC is not connected to the root build; bus tests use the existing bus through
add_subdirectory without changing other component files.

## Regression coverage

The suite contains **29 cases: 21 direct scenarios and 8 FU3 bus scenarios**.
[hds-requirements.json](../tests/hds-requirements.json) is the single source
of requirement/page/basis/test mappings. Test names use prefix `fu3_adc_`;
bus cases use `fu3_adc_bus_`.

| Scenarios | What they verify |
| --- | --- |
| registers | All 23 CSRs, reset, RO behavior, reserved masks, RW walking bits |
| channels, snapshot | All 16 selections, mode forwarding and current/next-sequence configuration snapshots |
| counts | Every five-bit count encoding, effective clamp and unused sample slots |
| timing_min, timing_programmed, timing_max | Values 0/min-1/min/min+1/255; measured setup/high/low intervals |
| averaging, randomized | 12-bit boundaries, maximum sum 65520, fractional averages; 160 sequences with seed 0xADC2026 |
| eoc_wait, eoc_policy, macro_phase | Missing/delayed/early/held EOC; normal conversion with an extra internal sampling-edge delay |
| lifecycle, status | No restart at DONE, sticky completion, RO STATUS, all active phases, disable and reset clearing |
| reset, reset_phases, disable_phases, clock_pause | Reset/disable in all seven enabled states; asynchronous reset with stopped clock |
| protocol, debug, reset_transport | Malformed TLM, debug/DMI, incoming delay, concurrency and reset-overlapping transactions |
| bus | Real FU3 decode, four-word burst splitting, byte order, unmapped/subword errors |

Bus scenarios are registers, channels, counts, averaging, lifecycle, status,
snapshot and bus. Every case also monitors CS high while SAMPCLK is high and
channel changes only while CS is low. These are controller-pin checks, not a
model of the internal macro ISAMPCLK or metastability.

The normal stimulus changes D after 12 ADC cycles and raises EOC after 12.5
cycles from SAMPCLK falling; it lowers EOC 2.5 cycles after the next rise.
`macro_phase` adds one cycle to represent internal sampling of SAMPCLK by CLK,
and checks that waiting for EOC still captures the correct sample sequence.
The controller never generates its own D/EOC or expected results.

`SPEC_CHECKS` counts assertions based on explicit HDS content; `POLICY_CHECKS`
counts VP ABI, rounding/readback and transport choices. The STATUS test
separates required DONE[0]/clear behavior from the chosen READY/BUSY bit mapping.
Each CTest case has a 30-second timeout and a 100 ms simulation watchdog;
polling/phase waits are bounded.

## Recorded verification - 2026-10-21

| Profile | Result |
| --- | --- |
| Debug + coverage + Werror | 29/29 PASS |
| Release + Werror | 29/29 PASS |
| Debug + UBSan + Werror | 29/29 PASS; no UBSan diagnostics |

No confirmed digital functional defect was found within the model scope.
No controller behavior was changed in this review. New checks cover STATUS,
pin conditions and delayed macro sampling. The header now identifies the
READY/BUSY bit assignment explicitly as a VP choice.

## Generated evidence

- `build/fu3-adc{,-release,-ubsan}/results.xml`: JUnit results.
- `configure.log`, `build.log`, `tests.log`: corresponding profile logs.
- `build/fu3-adc/coverage-summary.txt`, `adc.cpp.gcov`: ADC implementation coverage.
- `build/fu3-adc/verification-report.json`: pinned source metadata, requirement
  mapping and profile results; failed/skipped/missing mapped tests fail reporting.
- `Testing/Temporary/LastTest.log`: detailed CTest output.

Source hash was manually rechecked against the supplied PDF on 2026-10-21.
The reporting script copies the pinned metadata; it does not validate a PDF.
`hds_signoff` remains false for the boundaries and unspecified details recorded
in [spec-decisions.md](spec-decisions.md). No RTL, analog, CDC or CPU boot
equivalence is claimed.
