# FX1 ISP -- SystemC/TLM model of the FTEL ISP IP.
#
# Standalone:  see README.md (the environment setup there is mandatory).
# As a subproject of CDC-VP it reuses the parent's SystemC::systemc target.
cmake_minimum_required(VERSION 3.21)

project(fx1_isp
    VERSION 0.1.0
    LANGUAGES CXX
    DESCRIPTION "FX1 ISP SystemC/TLM model (standalone IP)")

if(PROJECT_IS_TOP_LEVEL)
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
    if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
        set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
    endif()
endif()

option(FX1_ISP_BUILD_TESTS "Build FX1 ISP unit and integration tests" ${PROJECT_IS_TOP_LEVEL})
option(FX1_ISP_INSTALL "Generate the install rules and the fx1_isp CMake package" ${PROJECT_IS_TOP_LEVEL})

# ---- SystemC ---------------------------------------------------------------
if(NOT TARGET SystemC::systemc)
    set(SYSTEMC_HOME "/opt/systemc-2.3.4" CACHE PATH "SystemC installation prefix")
    find_path(SYSTEMC_INCLUDE_DIR NAMES systemc HINTS "${SYSTEMC_HOME}/include")
    find_library(SYSTEMC_LIBRARY NAMES systemc HINTS "${SYSTEMC_HOME}/lib64" "${SYSTEMC_HOME}/lib")
    if(NOT SYSTEMC_INCLUDE_DIR OR NOT SYSTEMC_LIBRARY)
        message(FATAL_ERROR "SystemC not found. Set SYSTEMC_HOME to an Accellera SystemC prefix.")
    endif()
    add_library(SystemC::systemc UNKNOWN IMPORTED)
    set_target_properties(SystemC::systemc PROPERTIES
        IMPORTED_LOCATION "${SYSTEMC_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${SYSTEMC_INCLUDE_DIR}")
    get_filename_component(_fx1_isp_systemc_libdir "${SYSTEMC_LIBRARY}" DIRECTORY)
    set(CMAKE_BUILD_RPATH "${_fx1_isp_systemc_libdir}")
endif()

set(FX1_ISP_WARNINGS -Wall -Wextra -Wpedantic -Wconversion -Wshadow)

# ---- Core: register file and control, no SystemC ---------------------------
add_library(fx1_isp_core STATIC
    src/registers/csr_table_gen.cpp
    src/registers/register_file.cpp
    src/control/control_unit.cpp
    src/control/control_unit_stats.cpp
    src/dma/dma_rules.cpp
    src/pipeline/isp_pipeline.cpp)
target_include_directories(fx1_isp_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>)
target_compile_features(fx1_isp_core PUBLIC cxx_std_17)
target_compile_options(fx1_isp_core PRIVATE ${FX1_ISP_WARNINGS})
set_target_properties(fx1_isp_core PROPERTIES POSITION_INDEPENDENT_CODE ON)

# ---- TLM model --------------------------------------------------------------
add_library(fx1_isp_tlm STATIC
    src/fx1_isp_tlm.cpp
    src/engine/engines.cpp)
add_library(cdc::components::fx1_isp_tlm ALIAS fx1_isp_tlm)
target_include_directories(fx1_isp_tlm
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE src)
target_link_libraries(fx1_isp_tlm PUBLIC SystemC::systemc fx1_isp_core)
target_compile_options(fx1_isp_tlm PRIVATE ${FX1_ISP_WARNINGS})

# ---- Reference driver (C99, no SystemC): docs/ISP_PROGRAMMING_GUIDE.md ------------
enable_language(C)
add_library(fx1_isp_drv STATIC driver/src/fx1_isp_drv.c)
add_library(cdc::components::fx1_isp_drv ALIAS fx1_isp_drv)
target_include_directories(fx1_isp_drv
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/driver/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>)
set_target_properties(fx1_isp_drv PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
target_compile_options(fx1_isp_drv PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Wstrict-prototypes)

# Tool: run RAW frames through the complete model (uses the test RAM model).
add_executable(fx1_isp_run_raw tools/run_raw.cpp)
target_include_directories(fx1_isp_run_raw PRIVATE tests/common)
target_link_libraries(fx1_isp_run_raw PRIVATE fx1_isp_tlm SystemC::systemc)

# ---- Install and CMake package (find_package(fx1_isp)) ---------------------------
if(FX1_ISP_INSTALL)
    include(GNUInstallDirs)
    include(CMakePackageConfigHelpers)
    install(TARGETS fx1_isp_core fx1_isp_tlm fx1_isp_drv EXPORT fx1_ispTargets
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
    # The installed tool must find libsystemc without LD_LIBRARY_PATH.
    get_target_property(_fx1_isp_sc_lib SystemC::systemc IMPORTED_LOCATION)
    if(NOT _fx1_isp_sc_lib)
        get_target_property(_fx1_isp_sc_lib SystemC::systemc IMPORTED_LOCATION_RELEASE)
    endif()
    if(_fx1_isp_sc_lib)
        get_filename_component(_fx1_isp_sc_dir "${_fx1_isp_sc_lib}" DIRECTORY)
        set_target_properties(fx1_isp_run_raw PROPERTIES INSTALL_RPATH "${_fx1_isp_sc_dir}")
    endif()
    install(TARGETS fx1_isp_run_raw RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    install(FILES include/fx1_isp/fx1_isp_tlm.h include/fx1_isp/fx1_isp_csr.h
                  driver/include/fx1_isp/fx1_isp_drv.h
            DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/fx1_isp)
    install(FILES README.md docs/ISP_PROGRAMMING_GUIDE.md docs/ISP_MODEL_INTERFACE.md docs/ISP_CSR_CONTRACT.md
                  docs/ISP_DECISIONS_AND_DISCREPANCIES.md docs/ISP_REQUIREMENTS_MATRIX.md
                  docs/ISP_M4_QUALIFICATION.md docs/ISP_SOURCE_MANIFEST.md docs/ISP_CSR_TRACEABILITY.md
                  docs/csr/fx1_isp_csr_schema.json
            DESTINATION ${CMAKE_INSTALL_DOCDIR})
    install(DIRECTORY docs/evidence/ DESTINATION ${CMAKE_INSTALL_DOCDIR}/evidence)
    # Handover kit usable from the prefix: real-RAW tools and regression with
    # the Python reference, sample profiles, and the 4K bit-exact check.
    set(_fx1_isp_share ${CMAKE_INSTALL_DATADIR}/fx1_isp)
    install(PROGRAMS tools/run_regression.py tools/raw_fixture.py tools/make_profile.py tools/nv12_preview.py
            DESTINATION ${_fx1_isp_share}/tools)
    install(DIRECTORY reference/fx1_isp_ref DESTINATION ${_fx1_isp_share}/reference FILES_MATCHING PATTERN "*.py")
    install(FILES docs/csr/fx1_isp_csr_schema.json DESTINATION ${_fx1_isp_share}/docs/csr)
    install(FILES samples/README.md DESTINATION ${_fx1_isp_share}/samples)
    install(PROGRAMS samples/run_4k_check.sh DESTINATION ${_fx1_isp_share}/samples)
    install(DIRECTORY samples/profiles DESTINATION ${_fx1_isp_share}/samples)
    install(DIRECTORY tests/data/4k/ DESTINATION ${_fx1_isp_share}/samples/4k)
    install(EXPORT fx1_ispTargets NAMESPACE fx1_isp:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/fx1_isp)
    set(FX1_ISP_SYSTEMC_HOME_DEFAULT "${SYSTEMC_HOME}")
    configure_package_config_file(cmake/fx1_ispConfig.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/fx1_ispConfig.cmake
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/fx1_isp)
    write_basic_package_version_file(${CMAKE_CURRENT_BINARY_DIR}/fx1_ispConfigVersion.cmake
        COMPATIBILITY SameMinorVersion)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/fx1_ispConfig.cmake ${CMAKE_CURRENT_BINARY_DIR}/fx1_ispConfigVersion.cmake
            DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/fx1_isp)
endif()

if(FX1_ISP_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
