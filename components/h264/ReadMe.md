# H264 control and integration VP

[Hướng dẫn ghép module và chạy test cho Vinh/Nguyên](integration_docs/team_handoff.md)

Owner: Huy Nguyen Huynh Quoc. SystemC 2.3.4 / TLM-2.0 / C++17.

This component contains the chapter 3 top-level integration, chapter 4 control/register
model, chapter 5 DMA arbiter/bridge, and chapter 13 reset/synchronization model.

Folders:
- control, axi_dma, synchronization, top: production VP models.
- interfaces: shared contracts for the team release adapters.
- integration_tests/platform: simulated host and DDR used by tests.
- integration_tests/stubs: explicit checksum/copy fixture, NOT an H264 encoder.
- integration_tests: regression sources/vectors, replacement contract, and FX1 integration tests.
- integration_tests/examples: standalone demo; integration_tests/module_notes: per-module testing notes.
- integration_scripts: component build helper; cmake/integration_docs support the shipped VP component.
- cmake: package install/export; integration_docs: specification traceability and limitations.

No prediction, DMA-client, transform, entropy, deblocking or internal-memory
implementation owned by Vinh/Nguyên is included. Stub code is test support only.
No PDF, third-party dependency sources, or compiled binaries are part of this delivery.

## Standalone build from the CDC-VP root

Use an installed SystemC 2.3.4 built with the same compiler and C++17 ABI:

    cmake -S components/h264 -B components/h264/build/vp -DCMAKE_PREFIX_PATH=<SystemC-prefix>
    cmake --build components/h264/build/vp -j 8
    ctest --test-dir components/h264/build/vp --output-on-failure

Alternatively supply SYSTEMC_SOURCE_DIR, or both SYSTEMC_INCLUDE_DIR and
SYSTEMC_LIBRARY. All build output stays under components/h264/build.

For the test using this repo's actual FX1 bus/RAM/PLIC/exclusive monitor, add:

    -DH264_BUILD_CDC_TESTS=ON -DH264_CDC_VP_ROOT=<absolute-CDC-VP-root>

These tests use a TLM host and codec stub; they do not run CPU firmware or a real codec.

## Parent build integration

The parent must already define SystemC::systemc before add_subdirectory(h264).
Public target: cdc::components::h264_tlm. H264_BUILD_TESTS follows CDC_BUILD_TESTS.
Examples and standalone stubs default OFF when embedded; tests explicitly build
the fixtures they need. Using EncoderVp's default factory requires linking
h264_pipeline_stub; real integrations pass their released pipeline factory.

The repository-level CMake files were deliberately NOT changed in this delivery.
Consequently the ordinary root build does not automatically include this component.
The component supports parent inclusion and the CDC export set once the repo owner
enables it.

See integration_docs/cdc_vp_integration.md for FX1 wiring and reset/drain requirements.
Historical validation reports retain h264_demo context; in this copy the
component root is components/h264, not encoder_integration.
## Files to include in a Git contribution

Include integration_scripts/ and integration_docs/ with the component sources and
integration_tests/. The build script helps reproduce the build; documentation records
interfaces, specification assumptions and validation limits.
Do not include build/, compiled binaries, local CMake caches or downloaded dependencies.
Module-local docs/ folders stay under their respective modules (control, axi_dma,
synchronization, interfaces, top); they do not share the component-root namespace.