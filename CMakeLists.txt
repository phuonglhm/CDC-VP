cmake_minimum_required(VERSION 3.21)

project(cdc-vp
    VERSION 0.1.0
    LANGUAGES CXX C
    DESCRIPTION "Open-source Virtual Platform stack with SystemC/TLM CPU models")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(CDC_BUILD_MINI_TLM "Build mini TLM platform (Step 1)" ON)
option(CDC_BUILD_CPU_EVAL "Build RISC-V CPU evaluation platform (Step 2)" ON)
option(CDC_BUILD_CUSTOM_SOC "Build RISC-V custom SoC platform (Step 3)" ON)
option(CDC_BUILD_NOC_SOC
    "Build the FlooNoC SoC platform (cycle-accurate interconnect, slow)" ON)
option(CDC_BUILD_TESTS "Build component/platform tests" ON)
option(CDC_ENABLE_SAURIA_NPU_V4
    "Enable the optional external SAURIA v4 SystemC model" OFF)

# TPU_V3: 2D-mesh NoC SoC with RV32GCV NEO-COREs, one Sauria matrix engine,
# one independent DMA and one ImageTransform engine per core (decision record
# D14). Default OFF so the existing default build is untouched; see
# components/TPU_V3/docs/TPU_V3_IMPLEMENTATION_PLAN.md.
option(CDC_BUILD_TPU_V3_SOC
    "Build the TPU_V3 NoC SoC platform" OFF)
option(CDC_BUILD_TPU_V3_TESTS
    "Build TPU_V3 component tests" ON)

# Phase 5's Sauria matrix engine. Off by default and it must stay that way: the
# Sauria SystemC implementation is internal-only (licenses/SAURIA.PROVENANCE.md),
# so a default build must not acquire it and Phase 12 must be able to produce a
# package that demonstrably does not contain it.
option(CDC_BUILD_TPU_V3_SAURIA_MATRIX
    "Build the TPU_V3 Sauria matrix engine (requires the pinned v4.2 source)" OFF)

# Phase 6's source-traced ImageTransform engine. Like the matrix extraction it
# is opt-in: configuring the component verifies an internal NPU-team v4.2 pin,
# while an ordinary CDC-VP build must remain independent of that delivery.
option(CDC_BUILD_TPU_V3_IMAGE_TRANSFORM
    "Build the TPU_V3 Im2Col ImageTransform engine (requires pinned v4.2 evidence)" OFF)

# The RISC-V VP++ Compiler Enablement VP (plan §16, Phase 4.5): one RV32GCV
# hart, TLM RAM and a simulator-only host-I/O target, packaged for a compiler
# team.
#
# Independent of CDC_BUILD_TPU_V3_SOC on purpose. The handoff contains no NoC,
# no Sauria and no NEO composition, so requiring the SoC option to build it
# would make the package depend on parts it must not contain — and would make a
# compiler team's build break for reasons that have nothing to do with them.
option(CDC_BUILD_RISCV_VPP_COMPILER_VP
    "Build the RISC-V VP++ Compiler Enablement VP handoff platform" OFF)

# The matrix-engine geometry this binary is built for. It replaces the retired
# TPU_V3_MXU_BACKEND: after D14 the selectable property is not which of two
# MXU implementations to compile, it is which Sauria delivery the build
# contains — the verified 64x64 bring-up array, or the 128x128 target the NPU
# team has not shipped.
set(TPU_V3_SA_GEOMETRY "64x64" CACHE STRING
    "TPU_V3 Sauria matrix-engine geometry: 64x64 (128x128 awaits its promotion gate)")
set_property(CACHE TPU_V3_SA_GEOMETRY PROPERTY STRINGS 64x64 128x128)

# `set_property(... STRINGS ...)` only populates a GUI drop-down; it validates
# nothing on the command line. Without this check `-DTPU_V3_SA_GEOMETRY=typo`
# configures happily and the value is copied verbatim into the package
# manifest, which then names a geometry the binary does not contain.
#
# `128x128` is refused rather than accepted-and-unused. D14 makes it an NPU-team
# delivery behind a promotion gate that cannot have passed, because the source
# has not arrived; offering it as a configure-time choice that silently
# produces the 64x64 array is precisely the mislabelling D14 forbids — "no
# build, manifest or report may identify the 64x64 bring-up engine as 128x128."
if(CDC_BUILD_TPU_V3_SOC)
    if(TPU_V3_SA_GEOMETRY STREQUAL "128x128")
        message(FATAL_ERROR
            "TPU_V3_SA_GEOMETRY=128x128 is not available. The 128x128 Sauria "
            "engine is an NPU-team delivery that has not arrived, so its "
            "promotion gate cannot have passed (decision record D14). Use "
            "-DTPU_V3_SA_GEOMETRY=64x64.")
    elseif(NOT TPU_V3_SA_GEOMETRY STREQUAL "64x64")
        message(FATAL_ERROR
            "TPU_V3_SA_GEOMETRY='${TPU_V3_SA_GEOMETRY}' is not a known "
            "geometry. Accepted values are: 64x64. ('128x128' is reserved for "
            "the D14 promotion gate and is currently refused.)")
    endif()
endif()

set(CDC_CPU_BACKEND "riscv_vp" CACHE STRING "CPU backend: riscv_vp (Bremen riscv-vp, MIT)")
set_property(CACHE CDC_CPU_BACKEND PROPERTY STRINGS riscv_vp)

set(SYSTEMC_HOME "/opt/systemc-2.3.4" CACHE PATH "SystemC installation prefix")
set(SAURIA_NPU_ROOT "$ENV{SAURIA_NPU_ROOT}" CACHE PATH
    "Private path containing the optional SAURIA v4 npu_top.h")

find_path(SYSTEMC_INCLUDE_DIR
    NAMES systemc systemc.h
    HINTS "${SYSTEMC_HOME}/include")
find_library(SYSTEMC_LIBRARY
    NAMES systemc
    HINTS "${SYSTEMC_HOME}/lib64" "${SYSTEMC_HOME}/lib")

if(NOT SYSTEMC_INCLUDE_DIR OR NOT SYSTEMC_LIBRARY)
    message(FATAL_ERROR "SystemC was not found. Set SYSTEMC_HOME to an Accellera SystemC install prefix.")
endif()

add_library(SystemC::systemc UNKNOWN IMPORTED GLOBAL)
set_target_properties(SystemC::systemc PROPERTIES
    IMPORTED_LOCATION "${SYSTEMC_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${SYSTEMC_INCLUDE_DIR}")

# Make every target (platforms + test executables) find libsystemc.so at runtime.
get_filename_component(_systemc_libdir "${SYSTEMC_LIBRARY}" DIRECTORY)
set(CMAKE_BUILD_RPATH "${_systemc_libdir}")
set(CMAKE_INSTALL_RPATH "${_systemc_libdir}")

# Packaging helpers (cdc_make_portable / cdc_package_platform).
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/modules")
include(CdcPortable)

# enable_testing() must run in the top dir before tests are registered in subdirs.
# TPU_V3 has its own switch, so it is included here: otherwise
# -DCDC_BUILD_TESTS=OFF -DCDC_BUILD_TPU_V3_TESTS=ON would register tests that
# CTest never sees.
if(CDC_BUILD_TESTS
   OR (CDC_BUILD_TPU_V3_TESTS
       AND (CDC_BUILD_TPU_V3_SOC OR CDC_BUILD_TPU_V3_SAURIA_MATRIX
            OR CDC_BUILD_TPU_V3_IMAGE_TRANSFORM)))
    enable_testing()
endif()

add_subdirectory(components)
if(CDC_BUILD_CPU_EVAL OR CDC_BUILD_CUSTOM_SOC OR CDC_BUILD_TPU_V3_SOC
   OR CDC_BUILD_RISCV_VPP_COMPILER_VP)
    add_subdirectory(cpu_models)
endif()
add_subdirectory(platforms)
