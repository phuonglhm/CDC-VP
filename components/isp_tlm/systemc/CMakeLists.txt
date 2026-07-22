cmake_minimum_required(VERSION 3.10)
project(isp_tlm_systemc)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find SystemC
find_package(SystemC REQUIRED)

if(NOT SystemC_FOUND)
    message(FATAL_ERROR "SystemC not found. Please set SystemC_ROOT or install SystemC.")
endif()

message(STATUS "SystemC include dir: ${SystemC_INCLUDE_DIRS}")
message(STATUS "SystemC libraries: ${SystemC_LIBRARIES}")

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/common
    ${CMAKE_SOURCE_DIR}/src/base
    ${CMAKE_SOURCE_DIR}/src/utils
    ${CMAKE_SOURCE_DIR}/src/blocks
    ${CMAKE_SOURCE_DIR}/src/pipeline
    ${SystemC_INCLUDE_DIRS}
)

# Enable debug symbols by default
set(CMAKE_BUILD_TYPE Debug)

# Compiler flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")

#=============================================================================
# Source file groups
#=============================================================================

# Common utilities
set(COMMON_SOURCES
    src/common/isp_types.h
    src/common/isp_params.h
    src/common/common_defs.h
)

# Base classes
set(BASE_SOURCES
    src/base/isp_block_base.h
)

# Utils
set(UTILS_SOURCES
    src/utils/line_buffer.h
    src/utils/timing_adapter.h
    src/utils/window_buffer.h
)

# Block headers (placeholder - implementation in separate files)
file(GLOB BLOCK_HEADERS "src/blocks/*/*.h")
file(GLOB BLOCK_SRCS "src/blocks/*/*.cpp")

# Pipeline
set(PIPELINE_SOURCES
    src/pipeline/isp_top.h
    src/pipeline/isp_config.h
)

#=============================================================================
# Testbench
#=============================================================================
set(TESTBENCH_SOURCES
    tb/isp_tb.cpp
)

#=============================================================================
# Build options
#=============================================================================

# Option to build with trace
option(ENABLE_TRACE "Enable VCD tracing" ON)

#=============================================================================
# Main executable
#=============================================================================
add_executable(isp_tb
    ${TESTBENCH_SOURCES}
    ${BLOCK_SRCS}
)

target_link_libraries(isp_tb ${SystemC_LIBRARIES})

# Set include directories for target
target_include_directories(isp_tb PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/common
    ${CMAKE_SOURCE_DIR}/src/base
    ${CMAKE_SOURCE_DIR}/src/utils
    ${CMAKE_SOURCE_DIR}/src/blocks
    ${CMAKE_SOURCE_DIR}/src/pipeline
    ${SystemC_INCLUDE_DIRS}
)

#=============================================================================
# Optional: Build example blocks individually for testing
#=============================================================================

# Example: Build DPC block test
# add_executable(test_dpc
#     tb/test_dpc.cpp
#     src/blocks/dpc/sc_dpc.cpp
# )
# target_link_libraries(test_dpc ${SystemC_LIBRARIES})

#=============================================================================
# Installation (optional)
#=============================================================================
install(TARGETS isp_tb DESTINATION bin)

# Print configuration summary
message(STATUS "")
message(STATUS "==================================")
message(STATUS "ISP TLM SystemC Configuration")
message(STATUS "==================================")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "SystemC: ${SystemC_INCLUDE_DIRS}")
message(STATUS "Enable trace: ${ENABLE_TRACE}")
message(STATUS "==================================")
