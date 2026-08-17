# SPDX-License-Identifier: Apache-2.0
#
# tpu_v3_sauria_matrix — the Phase 5 matrix engine and its provenance gates.
#
# Off by default, and it must stay that way. The Sauria SystemC implementation
# is internal-only (`licenses/SAURIA.PROVENANCE.md`): a public CDC-VP build must
# not acquire it by default, and Phase 12 must be able to produce a package that
# demonstrably does not contain it.

if(NOT CDC_BUILD_TPU_V3_SAURIA_MATRIX)
    return()
endif()

# ── the pin ──────────────────────────────────────────────────────────────────
#
# Recorded in `docs/TPU_V3_PHASE5_AUDIT.md` §1. The default names the tree that
# matches the NPU team's working copy — deliberately *not* the copy
# `components/npu_tlm` falls back to, which is a different source with different
# golden vectors. Changing npu_tlm's own fallback is not Phase 5's decision to
# make, so Phase 5 names what it needs instead of quietly redirecting a shared
# component.

set(TPU_V3_SAURIA_ROOT
    "${CMAKE_SOURCE_DIR}/components/npu_tlm/models/v4.2_model"
    CACHE PATH
    "Pinned Sauria v4.2 source tree (TPU_V3_PHASE5_AUDIT.md section 1)")

set(TPU_V3_SAURIA_BASE_HASH
    "418a8d8831d72e4dccb083ba99fed7630b7438a698a1a1217629f2e50365e2c5"
    CACHE STRING "Content hash of the pinned Sauria tree, before the patch")

set(TPU_V3_SAURIA_PATCHED_HASH
    "c1931405bfa6c8cebb33deb645b4f4cf827fbe49fb91562104ca6b0b10ad8f98"
    CACHE STRING "Content hash of the patched Sauria tree, which is what is compiled")

set(TPU_V3_SAURIA_GOLDEN_HASH
    "1e9dc76c03032bb214544935f1f12c6df63aee1ba2eab4d4c3c97e19a972cb54"
    CACHE STRING "Content hash of the three demo_gemm_64x64 oracle files")

set(TPU_V3_SAURIA_PROFILE "int8_64x64" CACHE STRING
    "Sauria target profile. Phase 5 is int8_64x64 and refuses anything else")

# ── refusals, before anything is built ───────────────────────────────────────
#
# The build-level half of "a deliberately omitted geometry and an explicit
# 32x32/128x128 request are refused" (plan §16 gate). The compile-time half is
# the `static_assert`s in `sauria_geometry.h`; both exist because they fail at
# different moments and catch different mistakes.

if(NOT TPU_V3_SAURIA_PROFILE STREQUAL "int8_64x64")
    if(TPU_V3_SAURIA_PROFILE MATCHES "128x128")
        message(FATAL_ERROR
            "TPU_V3_SAURIA_PROFILE='${TPU_V3_SAURIA_PROFILE}' is refused. The "
            "128x128 engine is an NPU-team delivery that has not arrived, so "
            "its promotion gate cannot have passed (decision record D14). No "
            "build, manifest or report may call the 64x64 bring-up array "
            "128x128.")
    elseif(TPU_V3_SAURIA_PROFILE MATCHES "32x32")
        message(FATAL_ERROR
            "TPU_V3_SAURIA_PROFILE='${TPU_V3_SAURIA_PROFILE}' is refused. 32x32 "
            "is the source's *template default*, not a TPU_V3 configuration — "
            "it is precisely the value a build lands on by forgetting to choose "
            "one, which is what this gate exists to catch (audit section 3).")
    else()
        message(FATAL_ERROR
            "TPU_V3_SAURIA_PROFILE='${TPU_V3_SAURIA_PROFILE}' is not a Phase 5 "
            "profile. Phase 5 extracts int8_64x64: X=64, Y=64, INT8 operands, "
            "INT32 accumulation, index widths 18/18/17.")
    endif()
endif()

if(DEFINED TPU_V3_SA_GEOMETRY AND NOT TPU_V3_SA_GEOMETRY STREQUAL "64x64")
    message(FATAL_ERROR
        "TPU_V3_SA_GEOMETRY='${TPU_V3_SA_GEOMETRY}' contradicts the Sauria "
        "profile '${TPU_V3_SAURIA_PROFILE}', which is 64x64. The platform's "
        "geometry and the engine's cannot disagree: a report would name one and "
        "the run would use the other.")
endif()

# ── verify the pin, patch a build-tree copy, extract the profile ─────────────
#
# At configure time, so a wrong or unpinned source stops the build rather than
# producing a binary whose provenance nobody can state.

set(TPU_V3_SAURIA_WORK "${CMAKE_CURRENT_BINARY_DIR}/sauria_source")
set(TPU_V3_SAURIA_GENERATED
    "${CMAKE_CURRENT_BINARY_DIR}/generated/tpu_v3/sauria/sauria_profile.h")
set(TPU_V3_SAURIA_PATCHES
    "${CMAKE_CURRENT_SOURCE_DIR}/patches/0001-instrumentation-only-compile-out-sa-trace-files.patch"
    "${CMAKE_CURRENT_SOURCE_DIR}/patches/0002-compile-out-all-static-debug-state.patch")

execute_process(
    COMMAND ${CMAKE_COMMAND}
            -DSAURIA_ROOT=${TPU_V3_SAURIA_ROOT}
            -DWORK_DIR=${TPU_V3_SAURIA_WORK}
            "-DPATCHES=${TPU_V3_SAURIA_PATCHES}"
            -DBASE_HASH=${TPU_V3_SAURIA_BASE_HASH}
            -DPATCHED_HASH=${TPU_V3_SAURIA_PATCHED_HASH}
            -DGOLDEN_HASH=${TPU_V3_SAURIA_GOLDEN_HASH}
            -DPROFILE=${TPU_V3_SAURIA_PROFILE}
            -DGENERATED_HEADER=${TPU_V3_SAURIA_GENERATED}
            -DTEMPLATE_HEADER=${CMAKE_CURRENT_SOURCE_DIR}/cmake/sauria_profile.h.in
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/prepare_sauria_source.cmake"
    RESULT_VARIABLE _sauria_prepare_result
    OUTPUT_VARIABLE _sauria_prepare_output
    ERROR_VARIABLE _sauria_prepare_output)
message(STATUS "${_sauria_prepare_output}")
if(NOT _sauria_prepare_result EQUAL 0)
    message(FATAL_ERROR
        "Preparing the pinned Sauria source failed. TPU_V3_SAURIA_ROOT="
        "${TPU_V3_SAURIA_ROOT}")
endif()

# Re-run the preparation when the patch or the pinned targets file changes.
# `execute_process` above runs only at configure time, so without this a changed
# patch would be picked up on the next unrelated reconfigure and not before.
file(GLOB_RECURSE _tpu_v3_sauria_pinned_headers
    "${TPU_V3_SAURIA_ROOT}/*.h")
list(FILTER _tpu_v3_sauria_pinned_headers EXCLUDE REGEX "/npu_demo_clean/")
set(_tpu_v3_sauria_golden_files
    "${TPU_V3_SAURIA_ROOT}/npu_demo_clean/cases/demo_gemm_64x64/sauria_tmp/sauria_A_Mat_mvm_flat.txt"
    "${TPU_V3_SAURIA_ROOT}/npu_demo_clean/cases/demo_gemm_64x64/sauria_tmp/sauria_B_Mat_mvm_flat.txt"
    "${TPU_V3_SAURIA_ROOT}/npu_demo_clean/cases/demo_gemm_64x64/sauria_tmp/sauria_C_compute_mvm_flat.txt")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    ${TPU_V3_SAURIA_PATCHES}
    ${_tpu_v3_sauria_pinned_headers}
    ${_tpu_v3_sauria_golden_files})

# ── the gate target ──────────────────────────────────────────────────────────
#
# The public target carries the frozen interface and AXI4-Lite control model,
# plus the extracted profile, compile-time geometry proof and mandatory hygiene
# definitions. Template consumers inherit the verified build-tree source and
# native-port contract rather than restating any of them.

add_library(tpu_v3_sauria_matrix STATIC
    src/sauria_matrix_if.cpp
    src/gemm_config.cpp
    src/config_loader.cpp
    src/sa_control.cpp)
add_library(cdc::components::tpu_v3_sauria_matrix ALIAS tpu_v3_sauria_matrix)

target_include_directories(tpu_v3_sauria_matrix PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/generated>)

# The patched source tree, and the definitions D17 makes mandatory. They are on
# the INTERFACE so anything compiling Sauria headers gets them: a consumer that
# set them differently would be caught by `sauria_geometry.h`'s `#error`s, but
# it is better that no consumer has to know.
target_include_directories(tpu_v3_sauria_matrix SYSTEM PUBLIC
    $<BUILD_INTERFACE:${TPU_V3_SAURIA_WORK}>)
target_compile_definitions(tpu_v3_sauria_matrix PUBLIC
    SAURIA_DEBUG=0
    SAURIA_TRACE_FILES=0
    SAURIA_ACT_IDX_W=18
    SAURIA_WEI_IDX_W=18
    SAURIA_OUT_IDX_W=17)
# SystemC, because `config_loader.cpp` compiles the Sauria headers and
# `sauria_types.h` includes `systemc.h`. PUBLIC rather than PRIVATE: any
# consumer that includes `matrix_composition.h` or `tile_staging_store.h` needs
# it for the same reason.
target_link_libraries(tpu_v3_sauria_matrix PUBLIC
    cdc::components::tpu_v3_common
    cdc::components::tpu_v3_native_port
    SystemC::systemc)

target_compile_features(tpu_v3_sauria_matrix PUBLIC cxx_std_17)

if(CDC_BUILD_TPU_V3_TESTS)
    add_subdirectory(tests)
endif()
