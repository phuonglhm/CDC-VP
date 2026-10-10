# Test infrastructure

All non-production simulation support is collected here:
- *.cpp: shared regression and full-pipeline testbench sources.
- vectors/: input, expected outputs and test cases.
- support/stubs/: checksum/copy pipeline used until real team modules arrive.
- support/platform/: SystemC host driver and test DDR model.
- cdc_fx1/: integration tests using external CDC-VP components.
- embedded/: CMake parent/installed-consumer probes.
- examples/: executable standalone demo.
- ../control/test/: controller recovery and syntax tests; DMA/reset test notes live in ../axi_dma/test/ and ../synchronization/test/.

The testbench remains useful after real IP arrives: its released-pipeline mode
uses the real adapter and independent golden data. Stub/host/test-memory modules
are fixtures, not encoder production modules. Public include names and CMake
target names remain stable despite this source-directory reorganization.
