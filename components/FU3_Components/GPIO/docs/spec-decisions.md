# HDS decisions and VP policies

Reviewed on 2026-10-21 against the supplied GPIO and IO_PAD PDFs.
Implementation paths below are relative
to `FU3_Components`: `GPIO/src/gpio.cpp`, `IO_PAD/src/io_pad.cpp` and
`GPIO/src/subsystem.cpp`; `GPIO/src/state.cpp` now holds the shared digital
state machine for TLM and APB. GPIO and IO_PAD are separate libraries;
`GPIO` supplies their three-bank integration. This is an open
integration/decision log, not an HDS approval or a list of confirmed bugs.

## Source baseline

| Source | Source IP folder | SHA-256 |
| --- | --- | --- |
| `FTEL_CDC_IP_GPIO_Hardware_Design_Specification-1.pdf`, 19 pages | `009_GPIO/` | `82c3ebc68ff9031f0bcde028da42d3d86816750985286909230e4d1608916cb1` |
| `FTEL_CDC_IP_IO_PAD_Hardware_Design_Specification.pdf`, V1.0, 16 pages | Latest supplied IO_PAD PDF | `a473c96b2f01bb5d760c22e3f6b5aaf44c72fe9dacd5972666fcd699f18f20a8` |

PDF page references are 1-based physical pages. Both specifications are V1.0.
IO_PAD sections 6-7 describe selection and electrical controls on p9, test
decoding on p10 and the package map in Table 7-2 on p11.

## Behavior that is already explicit

GPIO pp10-14 explicitly define asynchronous active-low reset, three input
sampling stages, four interrupt modes, read-clear INTSTATUS and frozen status
while an interrupt is pending. An edge coincident with clearing a pending IRQ
is lost; a persistent level may reassert later. These are specified behaviors,
not unresolved VP assumptions.

POWEREVENT selects raw input with the highest-numbered selected pin winning
(p11). It is not a bank-local OR or a latched event. Address decoding uses only
PADDR[5:2], so upper bits and low alignment bits alias (p12). The interconnect
owns range/alignment enforcement. Neither behavior should be changed to match
a more familiar GPIO design without a revised specification.

IO_PAD section 6.3 specifies selection 00/01 and zero data/input routing for
unassigned codes 10/11. Section 6.5 defines test decoding from positions 5/11
and suppression of normal routing. Section 6.4 explicitly says requested
output enable is not connected to physical pad tri-state control in the
described PHY. The missing item is approval of that integration, not evidence
that toggling PADDIR already changes electrical drive.

## Open decisions

`Undefined` identifies a missing contract, `Integration` an external dependency,
`Conflict` inconsistent document text, and `VP contract` a simulation choice.
All rows remain open for owner confirmation. GPIO register logic is relatively
well defined; most open items concern pad/SoC integration.

| ID | Source and unresolved issue | Current VP behavior | Required confirmation |
| --- | --- | --- | --- |
| GPIO-D01 | Undefined/integration: GPIO p10 section 6.3 explicitly leaves PADDIR polarity to the pad wrapper; IO_PAD section 6.4 disconnects requested OE from physical tri-state | Expose PADDIR as requested enable, without physical drive resolution | PADDIR polarity, actual OE/tri-state wiring and approval of the disconnected request path |
| GPIO-D02 | Undefined/integration: GPIO p10 section 6.4 does not establish six PADCFG bits' electrical meaning; IO_PAD section 6.4 depends on foundry controls | Preserve six-bit fields, reset 0x02 per pin, and expose them unchanged | Bit-to-cell mapping, polarity, PDK release, safe reset controls, drive/pull/receiver/Schmitt/slew behavior |
| GPIO-D03 | Integration gap: IO_PAD sections 1/6.2/6.4/8 place control in external system logic and provide no control CSR map or startup sequence | Eighty two-bit select inputs and external requested-enable/data ports; no invented IOSEL/WAKE CSR bank | System-register addresses, fields, defaults, reset ownership and safe startup sequencing |
| GPIO-D04 | Integration gap: IO_PAD sections 1/4 describe right/top and left/bottom partitions without a complete software-to-physical control mapping | Logical iosel0 covers positions 1..40 and iosel1 covers 41..80 | Physical side/position/control indexing; logical VP names are not verified hardware partition identities |
| GPIO-D05 | Integration gap: GPIO p11 specifies one bank's raw power_event, not a SoC wake controller | Export each bank and OR the three outputs into subsystem wake | Wake aggregation, power domains, synchronization, debounce and sleep/clear semantics if provided by another block |
| GPIO-D06 | Integration gap: GPIO has 32 pins per bank; IO_PAD Table 7-2 bonds eight pins in each of three active GPIO groups | Implement all 32 bits, route the listed 24 package GPIOs and expose unbonded input stimulus for bits8..31 | Full SoC bonding/tie-off map and handling of unbonded pins |
| GPIO-D07 | Undefined: IO_PAD Table 7-2 groups JTAG/SPI data/select positions without assigning every individual signal | Generic primary/alternate endpoints indexed by package position; no guessed protocol signal order | Per-position JTAG/SPI signal names and directions |
| GPIO-D08 | Integration gap: IO_PAD sections 6.4/7 and Table 7-2 require approved PDK/package data for supply, analog and physical drive behavior | Digital routing only; analog/supply positions do not receive simulated digital pad drive; no contention/tri-state/voltage model | PDK cell models, package supply assignment, external loopback/resolution and analog wiring |
| GPIO-D09 | VP contract: GPIO pp11-12 specify APB pins and constant PREADY=1/PSLVERR=0, not TLM payload/error/debug semantics | Four-byte little-endian accesses without byte enables; alias [5:2]; malformed payloads return TLM errors; RO writes ignored; debug reads do not clear IRQ; no DMI | Accepted software transport abstraction; TLM errors do not represent a GPIO PSLVERR waveform |
| GPIO-D10 | VP contract/integration: GPIO p10 leaves reset release to SoC integration; p12 defines clocked access but not queued TLM callers | Asynchronous state reset; abort pending/mutex-queued calls using a reset generation; input pipeline refills after release; same-edge arrival can depend on SystemC scheduling | Reset release constraints and integration timing; queued-call cancellation is a VP convention |
| GPIO-D11 | Conflict/editorial: IO_PAD section 2 is titled Supported Features but begins with 'does not support'; GPIO p15 and IO_PAD sections 9-11 contain empty/template operation and verification material | Follow concrete GPIO functions/registers and IO_PAD sections 6-7 rather than the contradictory introductory sentence | Correct feature heading text, complete programming/verification guidance and approved document revisions |
| GPIO-D12 | Integration gap: GPIO pp13-14 define local offsets; IO_PAD has no bus register bank; these documents do not approve the complete FU3 address/wake routing | Use existing platform bank bases A=0x50030000, B=0x50040000, C=0x440F0000 with 64 KiB windows | Approved SoC memory/interrupt/wake map; local decode tests do not validate a complete platform |

Prioritize GPIO-D01 through GPIO-D04 for real pad integration. No change to
interrupt priority, status read-clear or local address aliasing is justified by
these open pad questions.

## Current regression results

The final clean rebuild on 2026-10-21 passed **30/30 in Debug and 30/30
in UBSan**. Standalone IO_PAD passed **4/4**. The suite contains eight direct
GPIO tests, twelve GPIO/IO_PAD integration tests, six signal-level APB tests
and four standalone pad tests. UBSan uses GCC 15.2.0, SystemC 2.3.4,
`-fsanitize=undefined -fno-sanitize-recover=undefined` and
`-Wall -Wextra -Wpedantic -Werror`. The installed SystemC library itself is
not sanitizer-instrumented.

`Gpio` and `ApbGpio` share their register, sampling and interrupt engine in
`detail::State`. APB tests cover widths 6/12/128, setup/access qualification,
combinational PRDATA, constant response pins, reset, all four interrupt modes,
raw wake and 1,536 seeded cycles. GPIO history tests check 3,072 six-cycle
traces; alias-write tests check 960 accesses; integration random testing checks
5,400 seeded cycles. IO_PAD tests use an independent package map, all select
codes, test-mode transitions, per-pin isolation and 1,800 random vectors.

The `wake_iosel` integration scenario checks all 24 bonded GPIO positions,
four selects and two test-mode states, including fixed-pin select ignoring.
It checks bank/aggregate wake, PADIN, bank isolation and simultaneous sources.
Disconnecting one active bank preserves the other bank's wake; disconnecting
the final source clears aggregate wake.

The final layout has two components and two implementation libraries:
`GPIO` (`fu3::gpio`) and `IO_PAD` (`fu3::io_pad`). GPIO adds and links IO_PAD
automatically. Standalone and parent-consumer build checks cover either
component inclusion order and the legacy aliases. IO_PAD has no GPIO, clock,
reset or TLM dependency.

See [verification.md](verification.md) for the completed requirement review,
[testing.md](testing.md) to rerun the functional suite, and
[usage.md](usage.md) / [registers.md](registers.md) for the integration contract.
Temporary JSON maps, report/coverage/fault-check helper scripts and historical
review artifacts were removed after verification. Final local build logs and
JUnit results remain under `build/fu3-gpio-review-20261021/`, outside source.

The tests establish the documented digital VP behavior. Close an external
integration decision only with an approved source revision and corresponding
model/test review. PDK electrical behavior, full-SoC boot and RTL equivalence
remain outside this verification.
