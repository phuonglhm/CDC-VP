# FX1 NPU: SAURIA v4.7 integration for platforms/fx1_soc (a clone of
# components/npu_tlm, which stays unchanged for platforms/VP_FX1_Full_SoC).
# The model drop is read in place: FX1_NPU_MODEL_ROOT is its repository root
# (the directory holding npu_top.h, has/ and core_rtl/), never copied or patched.
set(FX1_NPU_MODEL_ROOT "${CMAKE_SOURCE_DIR}/components/npu_tlm/models/v4.7_model"
    CACHE PATH "SAURIA v4.7 model drop root used by the FX1 NPU (read only)")

set(_fx1_npu_required
    npu_top.h has/has_npu_top.h has/has_mmio_compat.h has/has_host_neo_port.h
    driver/libsauria_cfg.h driver/sauria_run.h sauria_targets.h npu_profile.h
    instrumentation/perf_counters.h package_manifest.txt)
foreach(_f IN LISTS _fx1_npu_required)
    if(NOT EXISTS "${FX1_NPU_MODEL_ROOT}/${_f}")
        message(FATAL_ERROR "FX1_SOC_ENABLE_NPU=ON: '${_f}' not found in FX1_NPU_MODEL_ROOT='${FX1_NPU_MODEL_ROOT}'")
    endif()
endforeach()

# VP_MODEL_HASH: the first 32 bits of the SHA-256 of the drop's manifest, so
# firmware and the SDK manifest can tell which model a binary carries.
file(SHA256 "${FX1_NPU_MODEL_ROOT}/package_manifest.txt" _fx1_npu_manifest_sha)
string(SUBSTRING "${_fx1_npu_manifest_sha}" 0 8 _fx1_npu_hash)
set(FX1_NPU_MODEL_MANIFEST_SHA256 "${_fx1_npu_manifest_sha}" CACHE INTERNAL "")
message(STATUS "FX1 NPU: model ${FX1_NPU_MODEL_ROOT} (manifest sha256 ${_fx1_npu_manifest_sha})")

add_library(fx1_npu STATIC src/fx1_npu.cpp)
add_library(cdc::components::fx1_npu ALIAS fx1_npu)
target_include_directories(fx1_npu PUBLIC include)
# SYSTEM: the drop's headers are not held to this component's warning flags.
target_include_directories(fx1_npu SYSTEM PRIVATE
    "${FX1_NPU_MODEL_ROOT}"
    "${FX1_NPU_MODEL_ROOT}/driver"
    "${FX1_NPU_MODEL_ROOT}/instrumentation")
target_compile_definitions(fx1_npu PRIVATE FX1_NPU_MODEL_HASH=0x${_fx1_npu_hash}u)
target_compile_options(fx1_npu PRIVATE -Wall -Wextra)
target_link_libraries(fx1_npu PUBLIC SystemC::systemc)
target_compile_features(fx1_npu PUBLIC cxx_std_17)

if(CDC_BUILD_TESTS)
    add_subdirectory(tests)
endif()
