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
if(CDC_BUILD_TESTS)
    enable_testing()
endif()

add_subdirectory(components)
if(CDC_BUILD_CPU_EVAL OR CDC_BUILD_CUSTOM_SOC)
    add_subdirectory(cpu_models)
endif()
add_subdirectory(platforms)
