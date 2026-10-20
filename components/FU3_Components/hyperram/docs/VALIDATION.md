# Validation and remaining work

Validated on 2026-10-20 using WSL Linux, GCC 15.2, SystemC 2.3.4, C++17, Release. Static and shared configurations each passed all seven CTest cases: direct HyperRAM behavior, HyperRAM through the FU3 bus, bus configuration, bus behavior, bus arbitration, bus contract, and zero-latency fairness. Installed consumers for both static and shared packages also configured, linked, elaborated and ran successfully. Test executables use `sc_main`, simulation watchdogs, and CTest timeouts.

The initial two HyperRAM watchdog failures came from the test waiting on a join whose process had already terminated during reset. Guarding the wait with the remaining process count resolved both failures; the functional expectations remain unchanged. No issue exhausted three unsuccessful repair attempts.

## Implemented and exercised

| Behavior | Evidence / test |
|---|---|
| Device ID and configuration defaults | Datasheet pp.18-24; literal ID0/ID1/CR0/CR1 assertions |
| Memory access only after initialization | Provisional firmware init, READY and NOT_READY tests |
| Whole burst forwarding | One 64-byte request produces exactly one device transaction with 16 words, directly and through the bus |
| WSTRB and byte order | Roundtrip patterns, sparse masks preserve untouched bytes, zero-mask write |
| Profile checks | Reject narrow beats, FIXED, crossing 4 KiB and array end |
| tCSM admission | 256-word burst rejected before launch under 1-us profile |
| Faults and IRQ | Injected write underflow/read failure, retained prefix, partial-write diagnostic, error clear |
| Configuration coherence | CR0 variable-mode change followed by matching controller apply |
| Variable latency | Refresh indication increases transaction duration |
| Apply validation | Invalid LC and virtual delay rejected, previous active setting retained |
| Admission | Concurrent requests reach four read and four write slots |
| Reset | Controller warm retention, cancellation in flight, device reset invalidation |
| Existing bus behavior | Five unchanged bus regressions pass with burst preservation disabled by default |

## Not yet complete

The approved AP_HBCTRL register map is still unavailable. The v1 VP ABI and reset/init/IRQ decisions must be reconciled with that map before claiming hardware-driver compatibility. No CPU firmware binary or SoC boot has run. There is no pin-level HyperBus/native-x16 lane model, vendor Verilog co-simulation, physical calibration, raw AXI AW/W/R/B backpressure/framing, or full streaming-underflow detection. Failures are explicit TLM fault injection. Low-power modes, runtime clock changes, precise electrical timing, and same-cycle error-clear race qualification remain outside this implementation. Native Windows DLL validation has not been performed.

The package includes an installed-consumer test under `tests/installed_consumer`; it configures via `find_package(fu3-hyperram)`, links the exported target, elaborates the module and runs reset. Build artifacts and private spec extracts remain ignored. No commit or push was performed.
