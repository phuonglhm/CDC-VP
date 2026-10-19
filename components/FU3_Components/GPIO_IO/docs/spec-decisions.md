# HDS decisions and VP policies

Reviewed on 2026-10-19 against both supplied documents, `src/gpio.cpp`,
`src/io_pad.cpp`, `src/subsystem.cpp` and the current tests. GPIO here includes
the digital IO_PAD routing in the FU3 `GPIO_IO` component. This is an open
integration/decision log, not an HDS approval or a list of confirmed bugs.

## Source baseline

| Source | Location below `D:/Intern_doc/Intern/VP/004_IP/` | SHA-256 |
| --- | --- | --- |
| `FTEL_CDC_IP_GPIO_Hardware_Design_Specification-1.pdf`, 19 pages | `009_GPIO/` | `82c3ebc68ff9031f0bcde028da42d3d86816750985286909230e4d1608916cb1` |
| `FTEL_CDC_IP_IO_PAD_Hardware_Design_Specification.docx`, V1.0 | `011_IO_PAD/` | `2e3a2078188ca936a19b73317c03b9f6f9e4541c5ed751e103da7605d41eb22e` |

GPIO references use 1-based physical PDF pages. IO_PAD references use section
and table numbers, since DOCX pagination depends on the rendering application.

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

## Regression evidence and closure

CTest names use the prefix `fu3_gpio_`. Tests verify the digital interface and
the stated VP choices, not PDK electrical behavior.

| Decisions / functionality | Existing test suffixes | Evidence boundary |
| --- | --- | --- |
| Registers, pad fields and exposed data/direction; D01-D02 | `registers`, `spec_register_outputs` | Register outputs only; does not prove pad-drive polarity or PADCFG electrical mapping |
| Input sampling and interrupt lifecycle | `sampling`, `interrupts`, `spec_input_stream`, `spec_irq_boundaries` | Explicit digital GPIO requirements |
| Wake; D05 | `wake`, `spec_wake_priority` | Highest-selected raw bank input plus chosen board OR; no sleep controller |
| Package routing and test mode; D03-D04/D06-D08 | `iosel`, `integration`, `spec_pad_truth`, `spec_pad_random` | Independent digital pin-map expectations, no system control CSR/PDK approval |
| Transport and reset; D09-D10/D12 | `protocol`, `reset`, `concurrency`, `reset_pulse`, `spec_clock_reset`, `spec_address_decode` | VP transport/clock/reset behavior; no pin-level APB or full SoC proof |
| Source consistency; D11 | Source review | Editorial/specification closure is not a simulation verdict |

The 2026-10-19 Debug rebuild and rerun passed **19/19 tests**, with no failures
or disabled tests. The `random` scenario also checks 5,400 seeded cycles; pad
random testing uses 1,800 vectors. Evidence is in
`build/hds-review/gpio-results.xml` and `gpio-regression.log` from the repository
root. This review did not rerun UBSan.

See [usage.md](usage.md), [registers.md](registers.md) and
[testing.md](testing.md) for the existing digital contract. Close a decision
only with an approved source revision and corresponding model/test review.
Full hardware/pad sign-off remains open.
