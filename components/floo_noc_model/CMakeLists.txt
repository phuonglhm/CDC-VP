cmake_minimum_required(VERSION 3.21)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(floo-noc-model
        VERSION 0.1.0
        LANGUAGES CXX
        DESCRIPTION "Cycle-level SystemC model of the FlooNoC interconnect")

    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)

    set(SYSTEMC_HOME "/opt/systemc-2.3.4" CACHE PATH
        "SystemC installation prefix")

    find_path(SYSTEMC_INCLUDE_DIR
        NAMES systemc systemc.h
        HINTS "${SYSTEMC_HOME}/include")
    find_library(SYSTEMC_LIBRARY
        NAMES systemc
        HINTS "${SYSTEMC_HOME}/lib64" "${SYSTEMC_HOME}/lib")

    if(NOT SYSTEMC_INCLUDE_DIR OR NOT SYSTEMC_LIBRARY)
        message(FATAL_ERROR
            "SystemC was not found. Set SYSTEMC_HOME to an Accellera SystemC install.")
    endif()

    add_library(SystemC::systemc UNKNOWN IMPORTED GLOBAL)
    set_target_properties(SystemC::systemc PROPERTIES
        IMPORTED_LOCATION "${SYSTEMC_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${SYSTEMC_INCLUDE_DIR}")

    get_filename_component(_systemc_libdir "${SYSTEMC_LIBRARY}" DIRECTORY)
    set(CMAKE_BUILD_RPATH "${_systemc_libdir}")

    include(CTest)
endif()

add_library(floo_noc_model INTERFACE)
add_library(cdc::components::floo_noc_model ALIAS floo_noc_model)

target_include_directories(floo_noc_model
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>)
target_link_libraries(floo_noc_model INTERFACE SystemC::systemc)

# The model itself stays header-only so it can be used standalone. The TLM
# wrapper is a separate compiled target: it pulls in TLM and is only needed by
# a platform that wants the NoC in place of `bus_router`.
add_library(noc_interconnect STATIC src/noc_interconnect.cpp)
add_library(cdc::components::noc_interconnect ALIAS noc_interconnect)
target_link_libraries(noc_interconnect PUBLIC floo_noc_model SystemC::systemc)

install(TARGETS noc_interconnect EXPORT cdc-components-targets
    ARCHIVE DESTINATION lib
    INCLUDES DESTINATION include)

# Warning policy for this component's own translation units and its tests.
#
# A plain variable, not an INTERFACE library. An earlier version used a helper
# target and linked it PRIVATE into `noc_interconnect`; that target then had to
# appear in the export set of an installed static library, and a fresh parent
# configure failed with:
#
#   install(EXPORT "cdc-components-targets" ...) includes target
#   "noc_interconnect" which requires target "floo_noc_model_warnings"
#   that is not in any export set.
#
# A private *link* dependency of a static library is still an export-time
# dependency; private *compile options* are not. Variables do not enter the
# export graph at all, which is what this needs.
#
# The component is warning-clean. If a warning is ever accepted deliberately,
# suppress it at the narrowest scope and say why — do not widen this list.
set(FLOO_NOC_MODEL_WARNINGS "")
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set(FLOO_NOC_MODEL_WARNINGS -Wall -Wextra -Wpedantic)
endif()
target_compile_options(noc_interconnect PRIVATE ${FLOO_NOC_MODEL_WARNINGS})

option(FLOO_NOC_MODEL_BUILD_TESTS "Build FlooNoC model standalone tests" ON)
if(FLOO_NOC_MODEL_BUILD_TESTS)
    add_subdirectory(tests)
endif()

install(TARGETS floo_noc_model EXPORT cdc-components-targets
    INCLUDES DESTINATION include)
install(DIRECTORY include/ DESTINATION include)

# The installed headers contain both FlooNoC-derived SHL-0.51 code and
# CDC-VP-authored Apache-2.0 integration/reference code. Install both licence
# texts, the project NOTICE and the exact upstream revision record alongside
# the development package.
set(FLOO_NOC_MODEL_LICENSE_DEST
    "share/licenses/cdc-components/floo_noc_model")
get_filename_component(FLOO_NOC_MODEL_REPO_ROOT
    "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
install(FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/LICENSES/SHL-0.51.txt"
    "${CMAKE_CURRENT_SOURCE_DIR}/PROVENANCE.md"
    DESTINATION "${FLOO_NOC_MODEL_LICENSE_DEST}")
install(FILES "${FLOO_NOC_MODEL_REPO_ROOT}/LICENSE"
    DESTINATION "${FLOO_NOC_MODEL_LICENSE_DEST}"
    RENAME "Apache-2.0.txt")
install(FILES "${FLOO_NOC_MODEL_REPO_ROOT}/NOTICE"
    DESTINATION "${FLOO_NOC_MODEL_LICENSE_DEST}"
    RENAME "NOTICE.txt")
