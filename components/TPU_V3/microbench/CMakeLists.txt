# SPDX-License-Identifier: Apache-2.0
#
# tpu_v3_microbench: the standalone NEO-CORE benchmark definitions, the
# independent host golden and the machine-readable result row.
#
# Gate G2 of `docs/NEO_CORE_MICROBENCH_DSE_PLAN.md`. Three properties of this
# target are deliberate:
#
#   * it links no SystemC and no core model. The frozen case tables, the golden
#     arithmetic and the row schema are ordinary C++, so their unit tests cost a
#     compile rather than an elaboration -- and a golden that could reach the
#     model is a golden that could be written to agree with it;
#   * it does not contain a second `tpu_core`. The runner next door reuses the
#     Phase 7 composition, which is what D27 and the plan's section 13 require;
#   * the runner lives in its own subdirectory because it needs the CPU backend
#     and both extracted accelerators, and this library needs none of them.

add_library(tpu_v3_microbench STATIC
    src/benchmark_case.cpp
    src/golden.cpp
    src/result_row.cpp
    src/sha256.cpp)
add_library(cdc::components::tpu_v3_microbench ALIAS tpu_v3_microbench)

target_include_directories(tpu_v3_microbench
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>)

target_compile_features(tpu_v3_microbench PUBLIC cxx_std_17)

install(TARGETS tpu_v3_microbench EXPORT cdc-components-targets
    ARCHIVE DESTINATION lib
    INCLUDES DESTINATION include)
install(DIRECTORY include/ DESTINATION include)

if(CDC_BUILD_TPU_V3_TESTS)
    add_subdirectory(tests)
endif()

# The runner is the standalone executable. It exists only when a NEO-CORE can
# actually be composed; a benchmark runner without an engine to benchmark is
# the "target that links and does nothing" plan section 10 warns about.
if(TARGET tpu_v3_neo_core AND CDC_BUILD_TPU_V3_TESTS)
    add_subdirectory(runner)
endif()
