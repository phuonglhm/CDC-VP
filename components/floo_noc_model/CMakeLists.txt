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

option(FLOO_NOC_MODEL_BUILD_TESTS "Build FlooNoC model standalone tests" ON)
if(FLOO_NOC_MODEL_BUILD_TESTS)
    add_subdirectory(tests)
endif()

install(TARGETS floo_noc_model EXPORT cdc-components-targets
    INCLUDES DESTINATION include)
install(DIRECTORY include/ DESTINATION include)
