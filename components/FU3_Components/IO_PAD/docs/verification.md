# IO_PAD specification review and tests

Review date: 2026-10-21. No digital routing mismatch was found in the reviewed
implementation. Two test scenarios and boundary metadata checks were added;
the production model and its public interface were unchanged.

Source: the user-supplied `FTEL_CDC_IP_IO_PAD_Hardware_Design_Specification.pdf`,
V1.0, 16 physical pages. SHA-256:
`a473c96b2f01bb5d760c22e3f6b5aaf44c72fe9dacd5972666fcd699f18f20a8`.
The PDF text was read directly, and the diagram on p8 and pin table on p11
were also inspected as rendered pages. Page references below are physical,
1-based PDF pages.

## Results

All builds used WSL Linux, GCC 15.2.0, SystemC 2.3.4 and
`-Wall -Wextra -Wpedantic -Werror`.

| Build | Scope | Result |
| --- | --- | --- |
| Debug, GCC coverage | Standalone IO_PAD | 6/6 passed |
| Release, `-O3 -DNDEBUG` | Standalone IO_PAD | 6/6 passed |
| Debug, UBSan | GPIO core, APB, GPIO/IO_PAD integration and standalone IO_PAD | 32/32 passed, including all six IO_PAD scenarios |

UBSan used `-fsanitize=undefined -fno-sanitize-recover=undefined`; no sanitizer
diagnostic was reported. The installed SystemC library was not built with
sanitizers. Release checks remain active because tests use explicit failure
counts rather than C/C++ `assert`.

Each standalone run made **2,306,156 explicit checks**, plus the invalid
partition exception check. These counts include repeated assertions across
stimuli; they are not a count of distinct requirements.

| Scenario | Evidence |
| --- | --- |
| `fu3_io_pad_truth` | 103,201 checks: all 80 positions, four select values, 32 source combinations and both test modes; recovery with nonzero sources in both partitions |
| `fu3_io_pad_random` | 579,600 checks: 1,800 simultaneous 80-position vectors across three deterministic seeds |
| `fu3_io_pad_metadata` | 165 checks: independent pin membership and all 24 GPIO bank/bit/select mappings; cardinalities; positions 0, 81, 100 and maximum unsigned have no route |
| `fu3_io_pad_isolation` | 611,184 checks: per-pin select and source changes while checking every position; repeated test-mode entry/exit |
| `fu3_io_pad_delta` | 1,012,006 checks: nonzero sources before simulation starts; two instances; all 16 old/new select pairs; decoder inputs in both transition orders; independent changes to the five source kinds |
| `fu3_io_pad_invalid_partition` | Construction of `IoSel` with logical start position 2 throws `std::invalid_argument` |

The delta scenario checks both instances after each stimulus, including the
instance whose sources were held constant. All checks finish without
advancing simulation time. Eight delta cycles are allowed for settling;
this is a regression bound, not a specified physical propagation delay.
The tests use an independently transcribed HDS pin table for expected routing
and GPIO mapping, rather than calculating expected results through `pin_info`.

## Requirement review

| ID | Source | Result and evidence |
| --- | --- | --- |
| I01 | Sections 3, 5, 6.1-6.2, pp8-9 | PASS: fixed 80-position combinational interface; no clock, reset, bus, interrupt or DMA interface; POR receiver available in normal mode. `truth`, `delta`; source/header inspection |
| I02 | Sections 2, 7.1-7.2, pp7/10-11 | PASS for digital metadata: 45 digital cells, three input-only cells, 15 configurable positions and 24 GPIO connections. `metadata`, `truth` |
| I03 | Section 6.3, p9 | PASS: configurable code 00 selects primary and 01 selects alternate data, receiver and requested OE. `truth`, `random`, `delta` |
| I04 | Section 6.3, p9 | PASS: codes 10/11 clear selected data and receiver paths on configurable positions. Requested OE is also zero in this VP. `truth`, `isolation`, `delta` |
| I05 | Table 7-2, p11 | PASS: fixed-function positions retain the primary path independently of select. `truth`, `isolation`, `delta` |
| I06 | Section 6.5, p10 | PASS: test mode follows position 5; test select is position 5 AND position 11; normal routing is suppressed and recovers across both logical partitions. `truth`, `random`, `delta` |
| I07 | Table 7-2, p11 | PASS for digital abstraction: reserved positions 23/24, analog 42/43 and supply/reference positions have zero digital routes; input-only cells do not request output drive. `truth`, `metadata` |
| I08 | Table 7-2, p11 | PASS: every GPIO A/B/C bank, bit and select mapping agrees with the table. `metadata`; integration scenarios `fu3_gpio_integration` and `fu3_gpio_wake_iosel` also pass |
| I09 | Section 6.4, p9 | PASS for requested signals: data, requested OE and receiver paths are independent. OE does not gate selected data or imply electrical drive. `truth`, `isolation`, `delta` |

The production model uses generic primary/alternate endpoints indexed by
package position. The tests establish the routing of these endpoints and the
GPIO wiring in the subsystem. They do not validate named UART/SPI/JTAG
peripheral implementations or a complete SoC connection map.

The decoder reads raw pad inputs independently of the routed receivers. The
model also keeps `primary_in` observations of test positions 5/11 available
in test mode. These generic observation ports are a VP convention; the HDS
explicitly requires the two inputs to remain available to the decoder.

## Coverage and fault sensitivity

GCC coverage for `src/io_pad.cpp`, including invalid-partition handling:

| Metric | Result |
| --- | --- |
| Executable lines reached | 94/94, 100% |
| Branch sites reached | 284/284, 100% |
| Branch outcomes taken at least once | 70.77% |
| Calls executed | 84.92% |

Branch/call instrumentation includes C++ and SystemC support paths and
exception handling. Full line coverage does not establish all branch
outcomes, all input histories, electrical correctness or RTL equivalence.

Eight implementation copies were deliberately corrupted under the local
build directory. Every copy compiled, then exited with code 1 due to test
assertions. The original production source was not changed.

| Deliberate violation | Detecting scenario |
| --- | --- |
| Decode test select using OR | `pad_truth` |
| Ignore test-mode routing suppression | `pad_truth` |
| Gate selected data with requested OE | `pad_truth` |
| Shift GPIO C bits 5-7 by one | `pad_metadata` |
| Route reserved select codes to alternate | `pad_truth` |
| Apply select to fixed-function positions | `pad_truth` |
| Remove alternate-OE sensitivity | `pad_delta` |
| Shift local partition port indexing by one | `pad_truth` |

This is a selected fault-sensitivity check, not an exhaustive mutation score.

## External requirements and document gaps

These items are not counted as passed:

| Source | Remaining requirement |
| --- | --- |
| Sections 1, 6.4 and 7, pp7/9-11 | Approved GF22/PDK cells, receiver enable, drive strength, tri-state, pulls, Schmitt and slew controls; analog/supply behavior, electrical timing and contention. The VP has no corresponding electrical control ports or resolved pad model. |
| Sections 1, 3, 6.2 and 8, pp7-9/11-12 | External system register map, startup select/control values and reset ownership. IO_PAD itself has no CSR bank. |
| Sections 1 and 4, pp7-8 | Approved physical side/control indexing. Logical ranges 1-40 and 41-80 are VP partitions; their names do not establish the physical right/top versus left/bottom assignment. |
| Table 7-2, p11 | Individual signal ordering/directions within grouped JTAG/SPI functions and approved complete SoC wiring. Generic endpoints do not resolve missing signal names. |

Section 2 says "does not support" under a Supported Features heading,
contradicting the concrete functional descriptions. Sections 9-11 contain
empty/template guidance. The review follows the explicit sections 6-7 and
records these document issues. The cover date and revision-history date also
differ; the PDF hash identifies the reviewed artifact unambiguously.

## Reproduction and artifacts

Run from the repository root in Linux/WSL:

```sh
cmake -S components/FU3_Components/IO_PAD -B build/io-pad-audit-20261021/debug \
  -DSYSTEMC_HOME=/opt/systemc-2.3.4 -DCMAKE_BUILD_TYPE=Debug \
  -DFU3_IO_PAD_BUILD_TESTS=ON -DFU3_GPIO_COVERAGE=ON \
  -DCMAKE_CXX_FLAGS="-Wall -Wextra -Wpedantic -Werror"
cmake --build build/io-pad-audit-20261021/debug -j4
ctest --test-dir build/io-pad-audit-20261021/debug --output-on-failure \
  --output-junit ../debug-results.xml

cmake -S components/FU3_Components/GPIO -B build/io-pad-audit-20261021/ubsan \
  -DSYSTEMC_HOME=/opt/systemc-2.3.4 -DCMAKE_BUILD_TYPE=Debug \
  -DFU3_GPIO_BUILD_TESTS=ON -DFU3_IO_PAD_BUILD_TESTS=ON \
  -DCMAKE_CXX_FLAGS="-Wall -Wextra -Wpedantic -Werror -fsanitize=undefined -fno-sanitize-recover=undefined" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=undefined"
cmake --build build/io-pad-audit-20261021/ubsan -j4
ctest --test-dir build/io-pad-audit-20261021/ubsan --output-on-failure \
  --output-junit ../ubsan-results.xml
```

For Release, use the standalone command with a separate `release` build
directory, `-DCMAKE_BUILD_TYPE=Release` and coverage disabled.

Local evidence is under `build/io-pad-audit-20261021/` (ignored build output):
`debug.log`, `release.log`, `ubsan.log`; matching `*-results.xml` JUnit files;
`coverage/summary.txt` and `coverage/io_pad.cpp.gcov`;
`mutation-results.json` and individual mutation build/test logs.
Temporary build trees, mutation sources/binaries, helper scripts, extracted
PDF text and rendered reference pages were removed after verification.
Source SHA-256 values are recorded in `source-manifest.sha256` for the tested
IO_PAD files and GPIO integration sources. The commands above recreate the
functional test builds.
