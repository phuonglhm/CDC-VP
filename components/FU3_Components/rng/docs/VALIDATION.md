# RNG validation

## Common Spec rebaseline (2026-10-21)

The current model passed the joint static/shared scoped regression and installed
package checks recorded in the bus [validation matrix](../../bus/docs/VALIDATION.md).
The full matrix passes 131/131 executions across the owned components, bus
consumers and FX1 coexistence. Source precedence, selected behavior at conflicts
and remaining platform requirements are in
[COMMON_REBASELINE.md](../../bus/docs/COMMON_REBASELINE.md).

The following older results retain their original source baselines. They do not
override the current model contract or establish full firmware boot execution.

Run date: 2026-10-20. Environment: WSL Linux, GCC 15.2, SystemC 2.3.4, C++17, Release.

Static and shared builds each passed both CTest cases (`rng_behavior`, `rng_integration`): 2/2 per configuration. Installed consumers for both packages configured, linked, elaborated and ran successfully. The direct suite and real-bus suite cover CSR semantics, strobes, normal collection, command rearm, synchronous reset, non-consuming DATA reads, invalid accesses and IRQ behavior. Each has a simulation watchdog and a 30-second CTest timeout. No failing functional test required repair in this run.

See TRACEABILITY.md for requirements versus VP-policy tests. Passing a policy test does not establish an undocumented RTL behavior. Collection timing and generator equations remain unqualified; the model requires an explicit simulation bit source. No additional hardware CSR or unsupported HDS feature was introduced.

No CPU firmware or actual PLIC claim/complete flow has been executed. Native Windows DLL validation has not been performed. Build artifacts are ignored. No commit, staging or push was performed.
