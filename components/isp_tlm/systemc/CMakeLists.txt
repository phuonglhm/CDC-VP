# ==============================================================================
# SystemC ISP Streaming Testbenches (Architecture-Aware)
#
# Builds:
#   - 17 individual block testbenches
#   - 1 full-pipeline testbench (tb_pipeline)
#   - 1 architecture-aware timed pipeline testbench (tb_arch_pipeline)
#
# Hardware architecture support files in hw/ directory:
#   - isp_arch_config.h: Architecture configuration
#   - stream_beat.h: Frame-aware stream types
#   - timed_stream.h: Clocked instrumented streams
#   - metrics.h: Architecture metrics collection
#   - local_memory.h: Line buffer and memory models
#   - frame_dma.h: DMA models
#   - timed_block.h: Hardware shell base classes
#   - trace.h: VCD tracing utilities
# ==============================================================================

# ------------------------------------------------------------------------------
# Hardware Architecture Support (header-only, no compilation needed)
# ------------------------------------------------------------------------------
set(SC_ISP_HW_HEADERS
    hw/isp_arch_config.h
    hw/stream_beat.h
    hw/timed_stream.h
    hw/metrics.h
    hw/local_memory.h
    hw/frame_dma.h
    hw/timed_block.h
    hw/trace.h
    hw/hw.h
)

# ------------------------------------------------------------------------------
# Aggregate a list of every sc_<block>.cpp file so we can compile them as a
# convenience static library `sc_isp_blocks` which each block testbench can
# optionally link against.
# ------------------------------------------------------------------------------
set(SC_ISP_BLOCK_SOURCES
    blocks/2dnr/sc_2dnr.cpp
    blocks/aec/sc_aec.cpp
    blocks/awb/sc_awb.cpp
    blocks/blc/sc_blc.cpp
    blocks/bnr/sc_bnr.cpp
    blocks/ccm/sc_ccm.cpp
    blocks/cse/sc_cse.cpp
    blocks/csc/sc_csc.cpp
    blocks/demosaic/sc_demosaic.cpp
    blocks/dg/sc_dg.cpp
    blocks/dpc/sc_dpc.cpp
    blocks/gc/sc_gc.cpp
    blocks/lsc/sc_lsc.cpp
    blocks/scale/sc_scale.cpp
    blocks/sharpen/sc_sharpen.cpp
    blocks/wb/sc_wb.cpp
    blocks/yuv420/sc_yuv420.cpp
)

set(SC_ISP_TB_HEADERS_GLOB "blocks/*/tb_*.cpp")

# Build the streaming blocks as a static library — useful for advanced users
# who want to embed them in a different top-level harness.
add_library(sc_isp_blocks STATIC ${SC_ISP_BLOCK_SOURCES})
add_library(sc_isp_pipeline STATIC
    pipeline/sc_isp_pipeline.cpp
    ${SC_ISP_BLOCK_SOURCES}
)
add_library(cdc::components::sc_isp_pipeline ALIAS sc_isp_pipeline)

target_include_directories(sc_isp_blocks PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/tb_utils
    ${CMAKE_CURRENT_SOURCE_DIR}/hw
    # Re-export the original block headers so DUT code can `#include` them
    ${CMAKE_SOURCE_DIR}/components/isp_tlm/core/include
    ${CMAKE_SOURCE_DIR}/components/isp_tlm/blocks/2dnr/include
    ${CMAKE_SOURCE_DIR}/components/isp_tlm/blocks/aec/include
    ${CMAKE_SOURCE_DIR}/components/isp_tlm/blocks/awb/include
    ${CMAKE_SOURCE_DIR}/components/isp_tlm/blocks/blc/include
    ${CMAKE_SOURCE_DIR}/components/isp_tlm/blocks/bnr/include
    ${CMAKE_SOURCE_DIR}/components/isp_tlm/blocks/ccm/include
    ${CMAKE_SOURCE_DIR}/components/isp_tlm/blocks/cse/include
    ${CMAKE_SOURCE_DIR}/components/isp_tlm/blocks/rgb_to_yuv/include
    ${CMAKE_SOURCE_DIR}/components/isp_tlm/blocks/demosaic/include
    ${CMAKE_SOURCE_DIR}/components/isp_tlm/blocks/dg/include
    ${CMAKE_SOURCE_DIR}/components/isp_tlm/blocks/dpc/include
    ${CMAKE_SOURCE_DIR}/components/isp_tlm/blocks/gc/include
    ${CMAKE_SOURCE_DIR}/components/isp_tlm/blocks/gc/gc_lut
    ${CMAKE_SOURCE_DIR}/components/isp_tlm/blocks/lsc/include
    ${CMAKE_SOURCE_DIR}/components/isp_tlm/blocks/scale/include
    ${CMAKE_SOURCE_DIR}/components/isp_tlm/blocks/sharpen/include
    ${CMAKE_SOURCE_DIR}/components/isp_tlm/blocks/wb/include
    ${CMAKE_SOURCE_DIR}/components/isp_tlm/blocks/yuv420/include
)
target_link_libraries(sc_isp_blocks PUBLIC SystemC::systemc)

target_include_directories(sc_isp_pipeline PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/tb_utils
    ${CMAKE_SOURCE_DIR}/components/isp_tlm/core/include
    ${CMAKE_SOURCE_DIR}/components/isp_tlm/pipeline/include
)
target_link_libraries(sc_isp_pipeline PUBLIC
    sc_isp_blocks
    SystemC::systemc
)

# ------------------------------------------------------------------------------
# Helper macro: register a single-block testbench.
#
# Usage:
#   add_sc_block_testbench(<name> <block_dir> <tb_filename>)
# Example:
#   add_sc_block_testbench(tb_blc        blc        tb_blc.cpp)
#   add_sc_block_testbench(tb_demosaic   demosaic   tb_demosaic.cpp)
# ------------------------------------------------------------------------------
function(add_sc_block_testbench NAME BLOCK_DIR SRC)
    add_executable(${NAME} blocks/${BLOCK_DIR}/${SRC})
    target_include_directories(${NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/tb_utils
        ${CMAKE_SOURCE_DIR}/components/isp_tlm/core/include
        ${CMAKE_SOURCE_DIR}/components/isp_tlm/blocks/${BLOCK_DIR}/include
        # When a SystemC block references a sibling block's original header,
        # this pulls them all into the include path.
        ${CMAKE_SOURCE_DIR}/components/isp_tlm/blocks
        ${CMAKE_SOURCE_DIR}/components/isp_tlm/blocks/gc/gc_lut
    )
    target_link_libraries(${NAME} PRIVATE
        sc_isp_blocks
        SystemC::systemc
        cdc::components::isp_tlm    # golden reference (original C++ block)
    )

    if(CDC_BUILD_TESTS)
        add_test(NAME ${NAME} COMMAND ${NAME})
    endif()
endfunction()

# ------------------------------------------------------------------------------
# Macro to scan each block directory and register its testbench automatically.
# ------------------------------------------------------------------------------
macro(add_sc_block_testbench_auto)
    foreach(block_dir ${ARGN})
        file(GLOB tb_src "blocks/${block_dir}/tb_*.cpp")
        foreach(src IN LISTS tb_src)
            get_filename_component(name "${src}" NAME_WE)
            add_executable(${name} ${src})
            target_include_directories(${name} PRIVATE
                ${CMAKE_CURRENT_SOURCE_DIR}/tb_utils
                ${CMAKE_SOURCE_DIR}/components/isp_tlm/core/include
                ${CMAKE_SOURCE_DIR}/components/isp_tlm/blocks/${block_dir}/include
                ${CMAKE_SOURCE_DIR}/components/isp_tlm/blocks
                ${CMAKE_SOURCE_DIR}/components/isp_tlm/blocks/gc/gc_lut
            )
            target_link_libraries(${name} PRIVATE
                sc_isp_blocks
                SystemC::systemc
                cdc::components::isp_tlm    # golden reference
            )
            if(CDC_BUILD_TESTS)
                add_test(NAME ${name} COMMAND ${name})
            endif()
            message(STATUS "  + Registered SystemC testbench: ${name}")
        endforeach()
    endforeach()
endmacro()

# ------------------------------------------------------------------------------
# Manually register the 17 block-level testbenches (preferred over glob for
# deterministic ordering and clear build target names).
# ------------------------------------------------------------------------------
add_sc_block_testbench(tb_blc        blc        tb_blc.cpp)
add_sc_block_testbench(tb_dpc        dpc        tb_dpc.cpp)
add_sc_block_testbench(tb_dg         dg         tb_dg.cpp)
add_sc_block_testbench(tb_wb         wb         tb_wb.cpp)
add_sc_block_testbench(tb_ccm        ccm        tb_ccm.cpp)
add_sc_block_testbench(tb_gc         gc         tb_gc.cpp)
add_sc_block_testbench(tb_csc        csc        tb_csc.cpp)
add_sc_block_testbench(tb_cse        cse        tb_cse.cpp)
add_sc_block_testbench(tb_lsc        lsc        tb_lsc.cpp)
add_sc_block_testbench(tb_bnr        bnr        tb_bnr.cpp)
add_sc_block_testbench(tb_demosaic   demosaic   tb_demosaic.cpp)
add_sc_block_testbench(tb_sharpen    sharpen    tb_sharpen.cpp)
add_sc_block_testbench(tb_2dnr       2dnr       tb_2dnr.cpp)
add_sc_block_testbench(tb_awb        awb        tb_awb.cpp)
add_sc_block_testbench(tb_aec        aec        tb_aec.cpp)
add_sc_block_testbench(tb_scale      scale      tb_scale.cpp)
add_sc_block_testbench(tb_yuv420     yuv420     tb_yuv420.cpp)

# ------------------------------------------------------------------------------
# Full pipeline testbench (depends on the original C++ reference pipeline
# for golden comparison).
# ------------------------------------------------------------------------------
add_executable(tb_pipeline pipeline/tb_pipeline.cpp)
target_include_directories(tb_pipeline PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/tb_utils
    ${CMAKE_SOURCE_DIR}/components/isp_tlm/core/include
    ${CMAKE_SOURCE_DIR}/components/isp_tlm/pipeline/include
    ${SYSTEMC_INCLUDE_DIR}
)
target_link_libraries(tb_pipeline PRIVATE
    sc_isp_pipeline
    cdc::components::isp_tlm    # original C++ reference pipeline
    SystemC::systemc
)

if(CDC_BUILD_TESTS)
    add_test(NAME tb_pipeline COMMAND ${CMAKE_CURRENT_BINARY_DIR}/tb_pipeline
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
endif()

# ------------------------------------------------------------------------------
# Real-image (D65 RAW 2688x1520) testbenches.
#
# These testbenches load a real RAW capture from disk, stream it through
# the SystemC DUT and:
#   1. Save the DUT output to `output/`
#   2. Compare bit-exact against the original C++ reference
# ------------------------------------------------------------------------------
add_executable(tb_d65_pipeline pipeline/d65/tb_d65_pipeline.cpp)
target_include_directories(tb_d65_pipeline PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/tb_utils
    ${CMAKE_CURRENT_SOURCE_DIR}/input_utils
    ${CMAKE_CURRENT_SOURCE_DIR}/pipeline    # so `#include "sc_isp_pipeline.h"` works
    ${CMAKE_SOURCE_DIR}/components/isp_tlm/core/include
    ${CMAKE_SOURCE_DIR}/components/isp_tlm/pipeline/include
)
target_link_libraries(tb_d65_pipeline PRIVATE
    sc_isp_pipeline
    cdc::components::isp_tlm    # original C++ reference pipeline
    SystemC::systemc
)
if(CDC_BUILD_TESTS)
    add_test(NAME tb_d65_pipeline COMMAND ${CMAKE_CURRENT_BINARY_DIR}/tb_d65_pipeline
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
endif()

add_executable(tb_d65_blc blocks/blc/d65/tb_d65_blc.cpp)
target_include_directories(tb_d65_blc PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/tb_utils
    ${CMAKE_CURRENT_SOURCE_DIR}/input_utils
    ${CMAKE_CURRENT_SOURCE_DIR}/blocks/blc    # so `#include "sc_blc.h"` works
    ${CMAKE_SOURCE_DIR}/components/isp_tlm/core/include
    ${CMAKE_SOURCE_DIR}/components/isp_tlm/blocks/blc/include
)
target_link_libraries(tb_d65_blc PRIVATE
    sc_isp_blocks
    cdc::components::isp_tlm    # original blc_block
    SystemC::systemc
)
if(CDC_BUILD_TESTS)
    add_test(NAME tb_d65_blc COMMAND ${CMAKE_CURRENT_BINARY_DIR}/tb_d65_blc
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
endif()

# ------------------------------------------------------------------------------
# Metrics wrapper demo testbench (single-block demo)
#
# Exercises sc_metrics_wrapper<uint16_t> between a Generic_Driver and a
# sc_blc block. Dumps per-token latency CSV + summary at sc_main return.
# ------------------------------------------------------------------------------
add_executable(tb_metrics_demo tb_utils/metrics_demo_tb.cpp)
target_include_directories(tb_metrics_demo PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/tb_utils
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/components/isp_tlm/core/include
    ${CMAKE_SOURCE_DIR}/components/isp_tlm/blocks/blc/include
)
target_link_libraries(tb_metrics_demo PRIVATE
    sc_isp_blocks
    SystemC::systemc
)
if(CDC_BUILD_TESTS)
    add_test(NAME tb_metrics_demo COMMAND ${CMAKE_CURRENT_BINARY_DIR}/tb_metrics_demo
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
endif()

# ------------------------------------------------------------------------------
# Architecture-Aware Timed Pipeline Testbench
#
# This testbench demonstrates the full architecture model with:
#   - Clock and reset signals
#   - Timed block processing
#   - Architecture metrics collection
#   - Bottleneck analysis
# ------------------------------------------------------------------------------
add_executable(tb_arch_pipeline pipeline/tb_arch_pipeline.cpp)
target_include_directories(tb_arch_pipeline PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/tb_utils
    ${CMAKE_CURRENT_SOURCE_DIR}/hw
    ${CMAKE_CURRENT_SOURCE_DIR}/pipeline
    ${CMAKE_SOURCE_DIR}/components/isp_tlm/core/include
    ${CMAKE_SOURCE_DIR}/components/isp_tlm/pipeline/include
    ${SYSTEMC_INCLUDE_DIR}
)
target_link_libraries(tb_arch_pipeline PRIVATE
    sc_isp_pipeline
    cdc::components::isp_tlm    # original C++ reference pipeline
    SystemC::systemc
)
if(CDC_BUILD_TESTS)
    add_test(NAME tb_arch_pipeline COMMAND ${CMAKE_CURRENT_BINARY_DIR}/tb_arch_pipeline
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
endif()

# ------------------------------------------------------------------------------
# DMA Integration Testbench
# ------------------------------------------------------------------------------
add_executable(tb_dma_integration pipeline/tb_dma_integration.cpp)
target_include_directories(tb_dma_integration PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/tb_utils
    ${CMAKE_CURRENT_SOURCE_DIR}/hw
    ${CMAKE_CURRENT_SOURCE_DIR}/pipeline
    ${CMAKE_SOURCE_DIR}/components/isp_tlm/core/include
    ${CMAKE_SOURCE_DIR}/components/isp_tlm/pipeline/include
    ${SYSTEMC_INCLUDE_DIR}
)
target_link_libraries(tb_dma_integration PRIVATE
    sc_isp_pipeline
    cdc::components::isp_tlm
    SystemC::systemc
)
if(CDC_BUILD_TESTS)
    add_test(NAME tb_dma_integration COMMAND ${CMAKE_CURRENT_BINARY_DIR}/tb_dma_integration
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
endif()

# ------------------------------------------------------------------------------
# Power Metrics Testbench
# ------------------------------------------------------------------------------
add_executable(tb_power_metrics pipeline/tb_power_metrics.cpp)
target_include_directories(tb_power_metrics PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/tb_utils
    ${CMAKE_CURRENT_SOURCE_DIR}/hw
    ${CMAKE_CURRENT_SOURCE_DIR}/pipeline
    ${CMAKE_SOURCE_DIR}/components/isp_tlm/core/include
    ${CMAKE_SOURCE_DIR}/components/isp_tlm/pipeline/include
    ${SYSTEMC_INCLUDE_DIR}
)
target_link_libraries(tb_power_metrics PRIVATE
    sc_isp_pipeline
    cdc::components::isp_tlm
    SystemC::systemc
)
if(CDC_BUILD_TESTS)
    add_test(NAME tb_power_metrics COMMAND ${CMAKE_CURRENT_BINARY_DIR}/tb_power_metrics
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
endif()

# ------------------------------------------------------------------------------
# Architecture Sweep Testbench
# ------------------------------------------------------------------------------
add_executable(tb_arch_sweep pipeline/tb_arch_sweep.cpp)
target_include_directories(tb_arch_sweep PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/tb_utils
    ${CMAKE_CURRENT_SOURCE_DIR}/hw
    ${CMAKE_CURRENT_SOURCE_DIR}/pipeline
    ${CMAKE_SOURCE_DIR}/components/isp_tlm/core/include
    ${CMAKE_SOURCE_DIR}/components/isp_tlm/pipeline/include
    ${SYSTEMC_INCLUDE_DIR}
)
target_link_libraries(tb_arch_sweep PRIVATE
    sc_isp_pipeline
    cdc::components::isp_tlm
    SystemC::systemc
)
if(CDC_BUILD_TESTS)
    add_test(NAME tb_arch_sweep COMMAND ${CMAKE_CURRENT_BINARY_DIR}/tb_arch_sweep
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
endif()

# ------------------------------------------------------------------------------
# Metrics summary Python aggregator
# ------------------------------------------------------------------------------
find_package(Python3 COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND)
    add_custom_target(metrics_summary
        COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/tb_utils/metrics_summary.py
            ${CMAKE_CURRENT_SOURCE_DIR}/output/metrics
        DEPENDS tb_pipeline
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Aggregating metrics CSVs into summary"
    )
endif()
