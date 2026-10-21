# Timer validation - 2026-10-17

## Common Spec rebaseline (2026-10-21)

The current model passed the joint static/shared scoped regression and installed
package checks recorded in the bus [validation matrix](../../bus/docs/VALIDATION.md).
The full matrix passes 131/131 executions across the owned components, bus
consumers and FX1 coexistence. Source precedence, selected behavior at conflicts
and remaining platform requirements are in
[COMMON_REBASELINE.md](../../bus/docs/COMMON_REBASELINE.md).

The following older results retain their original source baselines. They do not
override the current model contract or establish full firmware boot execution.

Environment: Ubuntu under WSL, GNU C++ 15.2.0, C++17, SystemC 2.3.4 shared
installation. The local validation command completed with exit status 0.

| Check | Result |
|---|---|
| Static library: behavior and FU3 bus integration | 2/2 CTest tests passed |
| Installed static package consumer | 1/1 passed |
| Shared library: behavior and FU3 bus integration | 2/2 passed |
| Installed shared package consumer | 1/1 passed |

Total: 6/6 CTest executions passed. In each static/shared regression build:

- Behavior: 107 checks, zero failures.
- Integration: 70 checks, zero failures.
- Each installed consumer repeats the 107-check behavior suite through the
  exported package, rather than linking directly to the source target.

`ldd` on the shared installed consumer resolves `libfu3_timer.so` from the install
tree and `libsystemc.so.2.3` from the configured SystemC installation. No missing
dependency was reported. All test entry points are `sc_main`.

The integration bench uses the production FU3 bus with four Timer windows enabled,
four real Timer models with different periods, one fake MMIO master and fake IRQ
receivers. It verifies independent registers, direct and PLIC/INTMAP Timer routes,
periodic progress of all four timers, and peer progress while Timer0 is gated.
The behavior suite tests HDS write priority, unusual prescaler sequences, raw IRQ
conditions, disabled-counter interrupts, asynchronous reset, cancellation of a
pending access, byte-enable rejection without register mutation, address aliases,
reserved registers, debug restrictions and consumed incoming delay.

## Reproduction and evidence

Use the CMake/CTest commands in [README](../README.md#build-and-test), once with
BUILD_SHARED_LIBS=OFF and once with ON in separate build directories. The installed
consumer is a separate find_package project. Tests require the sibling FU3 bus;
the production Timer library can be built with BUILD_TESTING=OFF without it.

Local files below are ignored by Git and are not part of the source deliverable:

- `build/validation.log`: complete static/shared configure, build, CTest, install,
  consumer and shared-linkage output.
- `build/static/Testing/Temporary/LastTest.log`
- `build/shared/Testing/Temporary/LastTest.log`
- `build/static/consumer/Testing/Temporary/LastTest.log`
- `build/shared/consumer/Testing/Temporary/LastTest.log`

## Limits

These results establish tested functional behavior and bus integration, not full
SoC boot execution, real PLIC event capture/service, native Windows DLL validation
or cycle-accurate APB equivalence. The mapping from two raw IP interrupt outputs
to one SoC Timer source is an explicit integration choice. Invalid access errors
are VP adapter diagnostics, not Timer PSLVERR behavior. See
[MODEL_CONTRACT](MODEL_CONTRACT.md) and [TRACEABILITY](TRACEABILITY.md).

The implementation processes functional ticks individually; very long simulated
runs may need a future event-skipping optimization that preserves prescaler phase
and interrupt observation. No performance benchmark is claimed here.
