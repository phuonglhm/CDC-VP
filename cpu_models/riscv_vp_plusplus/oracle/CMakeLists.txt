# SPDX-License-Identifier: Apache-2.0
#
# The Spike differential oracle.
#
# Nothing here is linked into the platform or into a portable package (decision
# record D3). Spike is located as an *executable* and invoked as a child
# process; `cdc_package_platform` never sees it, and a package built without
# this directory is byte-identical.
#
# The oracle is optional by construction: if the binary is absent the test
# skips (77) instead of failing. A developer who has not run `fetch_spike.sh`
# has an incomplete setup, not a broken tree.

set(SPIKE_DIR "${CMAKE_SOURCE_DIR}/third_party/riscv-isa-sim"
    CACHE PATH "Spike checkout used as the Phase 2 differential oracle")
set(SPIKE_BIN "${SPIKE_DIR}/install/bin/spike")

if(NOT EXISTS "${SPIKE_BIN}")
    message(STATUS
        "Spike oracle not built at ${SPIKE_BIN}; spike_differential will skip. "
        "Run cpu_models/riscv_vp_plusplus/oracle/fetch_spike.sh to build it.")
endif()

add_executable(test_spike_differential test_spike_differential.cpp)
target_link_libraries(test_spike_differential PRIVATE
    cdc::cpu::riscv_vp_plusplus
    SystemC::systemc)
# The comparator names its fields from the same X-macro the firmware fills the
# block with, so neither side can add, drop or reorder a word alone.
target_include_directories(test_spike_differential PRIVATE "${RVV_SMOKE_DIR}")

if(TARGET rvv_smoke_image)
    add_dependencies(test_spike_differential rvv_smoke_image)
endif()

add_test(NAME spike_differential
    COMMAND test_spike_differential "${RVV_SIG_ELF}" "${SPIKE_BIN}")
set_tests_properties(spike_differential PROPERTIES
    LABELS "tpu_v3;cpu;differential"
    SKIP_RETURN_CODE 77
    # The two runs write `vpp.sig` and `spike.sig` into the working directory;
    # giving the test its own directory keeps a parallel ctest run from having
    # two comparisons overwrite each other's evidence.
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    TIMEOUT 600)
