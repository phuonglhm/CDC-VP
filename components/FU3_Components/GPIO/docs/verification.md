# GPIO and IO_PAD verification

Review date: 2026-10-21.

This records the completed specification review and CTest results. External
integration requirements are listed separately and do not count as passed.
The temporary requirement JSON maps and report generator were removed after
the final Debug and UBSan reruns each passed all 30 scenarios on 2026-10-21.
Use [testing.md](testing.md) to run the retained functional tests.

| Test run | Passed |
| --- | --- |
| final-debug-results.xml (GPIO + IO_PAD, Debug clean rebuild) | 30 |
| final-ubsan-results.xml (GPIO + IO_PAD, UBSan clean rebuild) | 30 |
| final-pad-results.xml (standalone IO_PAD clean rebuild) | 4 |

Final build logs and JUnit files are local artifacts under
`build/fu3-gpio-review-20261021/`; the source tree retains the functional tests.

## GPIO

Source: `FTEL_CDC_IP_GPIO_Hardware_Design_Specification-1.pdf`, V1.0, 19 pages.
SHA-256: `82c3ebc68ff9031f0bcde028da42d3d86816750985286909230e4d1608916cb1`.

| ID / scope | Source clause | Requirement | Result / evidence |
| --- | --- | --- | --- |
| G01 / HDS | pp7/9, sections 2/5 | Fixed 32-bit core inputs, outputs and direction; 32 six-bit configuration outputs | PASS: `fu3_gpio_spec_register_outputs`, `fu3_gpio_apb_registers` |
| G02 / HDS | p9, section 6.1 | Sequential state changes on rising HCLK; stopped clock retains state | PASS: `fu3_gpio_spec_clock_reset`, `fu3_gpio_apb_phases`, `fu3_gpio_apb_sampling_wake` |
| G03 / HDS | p10, section 6.2 | Asynchronous active-low reset restores all registers, pipeline, IRQ and pad fields | PASS: `fu3_gpio_reset`, `fu3_gpio_spec_clock_reset`, `fu3_gpio_apb_reset` |
| G04 / HDS | p10, section 6.3 | Two synchronizer registers plus PADIN sample; sync output is the second stage | PASS: `fu3_gpio_sampling`, `fu3_gpio_spec_input_stream`, `fu3_gpio_spec_irq_histories`, `fu3_gpio_apb_sampling_wake`, `fu3_gpio_apb_random` |
| G05 / HDS | p10, sections 6.3-6.4 | PADDIR and PADOUT do not gate each other or input sampling | PASS: `fu3_gpio_spec_register_outputs`, `fu3_gpio_sampling`, `fu3_gpio_apb_registers` |
| G06 / HDS | p10, section 6.4 | PADCFG fields pack four six-bit values; reserved bits zero; per-pin reset 2 | PASS: `fu3_gpio_spec_register_outputs`, `fu3_gpio_spec_address_writes`, `fu3_gpio_apb_registers`, `fu3_gpio_apb_reset` |
| G07 / HDS | pp10-11, section 6.5 | INTEN masks four modes per pin: high, low, rising, falling | PASS: `fu3_gpio_interrupts`, `fu3_gpio_spec_irq_boundaries`, `fu3_gpio_spec_irq_histories`, `fu3_gpio_apb_interrupts` |
| G08 / HDS | p11, section 6.5 | An event latches the complete enabled vector; status freezes while IRQ is pending | PASS: `fu3_gpio_interrupts`, `fu3_gpio_spec_irq_boundaries`, `fu3_gpio_spec_irq_histories`, `fu3_gpio_apb_random` |
| G09 / HDS | pp11-12, sections 6.5/7.1 | Only a selected read access edge clears pending INTSTATUS/IRQ; read returns pre-edge data | PASS: `fu3_gpio_spec_irq_boundaries`, `fu3_gpio_spec_irq_histories`, `fu3_gpio_apb_interrupts` |
| G10 / HDS | p11, section 6.5 | Persistent levels may reassert; an edge does not repeat without a new transition | PASS: `fu3_gpio_interrupts`, `fu3_gpio_apb_interrupts` |
| G11 / HDS | p11, section 6.5 | New edge at pending clear is lost; empty status read does not discard a new event | PASS: `fu3_gpio_interrupts`, `fu3_gpio_spec_irq_boundaries`, `fu3_gpio_spec_irq_histories`, `fu3_gpio_apb_interrupts` |
| G12 / HDS | p11, section 6.5 | Raw combinational POWEREVENT: highest selected pin wins, zero with no selection | PASS: `fu3_gpio_wake`, `fu3_gpio_spec_wake_priority`, `fu3_gpio_apb_sampling_wake`, `fu3_gpio_apb_random` |
| G13 / HDS | pp11-12, section 7.1 | Register decode uses only PADDR[5:2], ignoring upper and alignment bits | PASS: `fu3_gpio_protocol`, `fu3_gpio_spec_address_decode`, `fu3_gpio_spec_address_writes`, `fu3_gpio_apb_registers`, `fu3_gpio_apb_random` |
| G14 / HDS | pp13-14, section 8.2 | Sixteen 32-bit registers have specified offsets, access rules and reset values | PASS: `fu3_gpio_registers`, `fu3_gpio_spec_address_writes`, `fu3_gpio_apb_registers` |
| G15 / HDS | p12, section 7.1 | APB writes require PSEL, PENABLE and PWRITE at rising edge; no setup/unselected write | PASS: `fu3_gpio_apb_phases`, `fu3_gpio_apb_random` |
| G16 / HDS | p12, section 7.1 | PRDATA follows decoded register combinationally regardless of PSEL/PENABLE | PASS: `fu3_gpio_apb_phases`, `fu3_gpio_apb_random`, `fu3_gpio_apb_interrupts` |
| G17 / HDS | pp10/12, sections 6.2/7.1 | PREADY is always 1 and PSLVERR always 0, including during reset | PASS: `fu3_gpio_apb_phases`, `fu3_gpio_apb_reset`, `fu3_gpio_apb_random` |
| G18 / HDS | p9, section 5 | PADDR default width is 12, minimum usable width 6; upper bits do not affect decode | PASS: `fu3_gpio_apb_registers`, `fu3_gpio_apb_random` |
| V01 / VP | VP transport contract | Four-byte little-endian TLM payload validation and ignored RO writes | PASS: `fu3_gpio_protocol`, `fu3_gpio_registers` |
| V02 / VP | VP transport contract | Debug reads are non-destructive; debug writes rejected; DMI disabled | PASS: `fu3_gpio_protocol`, `fu3_gpio_spec_irq_boundaries`, `fu3_gpio_interrupts` |
| V03 / VP | VP timing/reset contract | Annotated delay, same-bank serialization, bank independence and reset cancellation | PASS: `fu3_gpio_protocol`, `fu3_gpio_concurrency`, `fu3_gpio_reset_pulse` |
| V04 / VP | Board wiring policy | Selected receiver feeds GPIO; unbonded high bits available; aggregate wake ORs banks | PASS: `fu3_gpio_integration`, `fu3_gpio_wake_iosel`, `fu3_gpio_wake`, `fu3_gpio_random`, `fu3_gpio_iosel` |
| GE1 / HDS | p10, section 6.3 | PADDIR polarity and physical output-enable wiring | EXTERNAL: The HDS leaves the pad-wrapper interpretation external; only the raw digital value is modeled. |
| GE2 / HDS | p10, section 6.4 | Electrical meaning/polarity of six PADCFG bits | EXTERNAL: Requires selected pad cell and wrapper/PDK mapping; reset and field values are verified separately. |
| GE3 / HDS | pp11-14, sections 7-8 | Approved SoC address aperture, interrupt wiring and reset release timing | EXTERNAL: The GPIO HDS defines local decode and IP behavior, not the complete SoC integration contract. |

## IO_PAD

Source: `FTEL_CDC_IP_IO_PAD_Hardware_Design_Specification.pdf`, V1.0, 16 pages.
SHA-256: `a473c96b2f01bb5d760c22e3f6b5aaf44c72fe9dacd5972666fcd699f18f20a8`.

| ID / scope | Source clause | Requirement | Result / evidence |
| --- | --- | --- | --- |
| I01 / HDS | p9, sections 6.1-6.2 | Combinational IO_PAD has no internal clock or reset; POR input routes in normal mode | PASS: `fu3_io_pad_truth`, `fu3_io_pad_random`, `fu3_io_pad_isolation` |
| I02 / HDS | pp10-11, section 7 | Package metadata covers 80 positions, 45 digital cells, three input-only cells and 24 GPIOs | PASS: `fu3_io_pad_metadata`, `fu3_io_pad_truth` |
| I03 / HDS | p9, section 6.3 | Configurable select 0/1 routes primary/alternate data, receiver and requested enable | PASS: `fu3_io_pad_truth`, `fu3_io_pad_isolation`, `fu3_gpio_integration` |
| I04 / HDS | p9, section 6.3 | Unassigned codes 2/3 zero normal data and routed receivers on configurable pins | PASS: `fu3_io_pad_truth`, `fu3_io_pad_isolation`, `fu3_gpio_wake_iosel` |
| I05 / HDS | p11, Table 7-2 | Fixed-function pins ignore select and retain the primary path | PASS: `fu3_io_pad_truth`, `fu3_io_pad_isolation`, `fu3_gpio_wake_iosel` |
| I06 / HDS | p10, section 6.5 | Test mode is pad 5; test select is pad 5 AND pad 11; normal routing suppressed | PASS: `fu3_io_pad_truth`, `fu3_io_pad_random`, `fu3_io_pad_isolation`, `fu3_gpio_iosel`, `fu3_gpio_wake_iosel` |
| I07 / HDS | p11, Table 7-2 | Reserved/supply/analog positions have no digital function route; input-only cells have no requested drive | PASS: `fu3_io_pad_metadata`, `fu3_io_pad_truth` |
| I08 / HDS | p11, Table 7-2 | GPIO bank/bit/function-select mapping matches every bonded GPIO position | PASS: `fu3_io_pad_metadata`, `fu3_gpio_integration`, `fu3_gpio_wake_iosel` |
| I09 / HDS | p9, section 6.4 | Digital data and requested OE are independent; OE does not gate data or receiver routing | PASS: `fu3_io_pad_truth`, `fu3_io_pad_isolation` |
| IE1 / HDS | pp7/9-11, sections 1/6.4/7 | GF22 physical pad cells, analog/supply and electrical controls | EXTERNAL: Requires approved PDK and electrical models; no invented tri-state, pull, drive, receiver-enable or voltage semantics. |
| IE2 / HDS | pp7-8/11-12, sections 1/3/8 | Software IOSEL registers and startup values for external controls | EXTERNAL: No internal IO_PAD register map is specified; the model exposes external select inputs. |
| IE3 / HDS | pp7-8, sections 1/4 | Physical partition/side-to-control mapping | EXTERNAL: The VP uses logical 1-40 and 41-80 groups; physical indexing requires integration confirmation. |
| IE4 / HDS | p11, Table 7-2 | Individual protocol signal names in grouped JTAG/SPI ranges | EXTERNAL: Generic package-position endpoints preserve routing; exact grouped-signal order requires another source. |

31 requirements/contracts verified in both complete GPIO/IO_PAD runs;
7 external requirements remain open. The separate IO_PAD run covers only the
four standalone pad scenarios.

These results establish tested digital behavior. They do not establish
PDK electrical behavior, a complete SoC sign-off or RTL equivalence.

## Model line coverage

GCC executable lines; union across template instantiations.

| Model source | Executed / executable lines | Missing lines |
| --- | --- | --- |
| `components/FU3_Components/GPIO/src/state.cpp` | 31 / 31 | None |
| `components/FU3_Components/GPIO/src/gpio.cpp` | 87 / 87 | None |
| `components/FU3_Components/GPIO/src/subsystem.cpp` | 64 / 64 | None |
| `components/FU3_Components/IO_PAD/src/io_pad.cpp` | 93 / 94 | 53 |
| `components/FU3_Components/GPIO/include/fu3/gpio/apb.h` | 29 / 29 | None |
| `components/FU3_Components/GPIO/include/fu3/gpio/detail/state.h` | 11 / 11 | None |

Line coverage measures execution; it does not replace the requirement checks
or establish exhaustive branch/state-space coverage.

## Selected fault checks

Deliberately incorrect copies failed assertions; production sources were unchanged.

| Deliberate violation | Scenario detecting it | Result |
| --- | --- | --- |
| `setup_writes` | `phases` | Detected |
| `selected_prdata_only` | `phases` | Detected |
| `ready_low_in_reset` | `reset` | Detected |
| `reserved_bits_writable` | `address_writes` | Detected |
| `ignore_test_mode` | `pad_truth` | Detected |
| `wrong_gpio_c_bit` | `pad_metadata` | Detected |

These selected checks demonstrate sensitivity to these mistakes; they are not
an exhaustive mutation score.
