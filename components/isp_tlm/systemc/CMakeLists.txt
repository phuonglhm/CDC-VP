cmake_minimum_required(VERSION 3.21)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(isp_systemc_model LANGUAGES CXX)
    enable_testing()
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT TARGET SystemC::systemc)
    set(SYSTEMC_HOME "/opt/systemc-2.3.4" CACHE PATH
        "Accellera SystemC installation prefix")
    find_path(ISP_SYSTEMC_INCLUDE_DIR
        NAMES systemc systemc.h
        HINTS "${SYSTEMC_HOME}/include")
    find_library(ISP_SYSTEMC_LIBRARY
        NAMES systemc
        HINTS "${SYSTEMC_HOME}/lib" "${SYSTEMC_HOME}/lib64")
    if(NOT ISP_SYSTEMC_INCLUDE_DIR OR NOT ISP_SYSTEMC_LIBRARY)
        message(FATAL_ERROR
            "SystemC was not found; set SYSTEMC_HOME to the installation prefix")
    endif()
    add_library(SystemC::systemc UNKNOWN IMPORTED GLOBAL)
    set_target_properties(SystemC::systemc PROPERTIES
        IMPORTED_LOCATION "${ISP_SYSTEMC_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${ISP_SYSTEMC_INCLUDE_DIR}")
    get_filename_component(ISP_SYSTEMC_LIBRARY_DIR
        "${ISP_SYSTEMC_LIBRARY}" DIRECTORY)
    set(CMAKE_BUILD_RPATH "${ISP_SYSTEMC_LIBRARY_DIR}")
endif()

find_package(Threads REQUIRED)

add_library(isp_systemc_model STATIC
    src/registers/isp_register_bank.cpp)
add_library(cdc::components::isp_systemc_model ALIAS isp_systemc_model)

target_include_directories(isp_systemc_model
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>")
target_compile_features(isp_systemc_model PUBLIC cxx_std_17)
target_link_libraries(isp_systemc_model
    PUBLIC
        SystemC::systemc
        Threads::Threads)

if(MSVC)
    target_compile_options(isp_systemc_model PRIVATE /W4)
else()
    target_compile_options(isp_systemc_model PRIVATE
        -Wall -Wextra -Wpedantic)
endif()

if(DEFINED CDC_BUILD_TESTS)
    set(_isp_tests_default "${CDC_BUILD_TESTS}")
else()
    set(_isp_tests_default ON)
endif()
option(ISP_SYSTEMC_BUILD_TESTS
    "Build the register-driven ISP SystemC tests"
    "${_isp_tests_default}")
unset(_isp_tests_default)

if(ISP_SYSTEMC_BUILD_TESTS)
    function(isp_add_test target source)
        add_executable("${target}" "${source}")
        target_link_libraries("${target}" PRIVATE isp_systemc_model)
        if(MSVC)
            target_compile_options("${target}" PRIVATE /W4 /WX)
        else()
            target_compile_options("${target}" PRIVATE
                -Wall -Wextra -Wpedantic -Werror
                -Wno-overloaded-virtual)
        endif()
        add_test(NAME "${target}" COMMAND "${target}")
    endfunction()

    isp_add_test(isp_register_bank_test
        tests/isp_register_bank_test.cpp)
    isp_add_test(isp_vip_i420_test
        tests/vip_i420_test.cpp)
    isp_add_test(isp_frame_memory_test
        tests/test_isp_frame_memory.cpp)
    isp_add_test(isp_control_target_test
        tests/test_isp_control_target.cpp)
    isp_add_test(isp_axi_lite_adapter_test
        tests/test_isp_axi_lite_adapter.cpp)
    isp_add_test(isp_csc_test
        tests/csc_test.cpp)
    isp_add_test(isp_top_job_test
        tests/isp_top_memory_job_test.cpp)

    set(_isp_ctest_targets
        isp_register_bank_test
        isp_vip_i420_test
        isp_frame_memory_test
        isp_control_target_test
        isp_axi_lite_adapter_test
        isp_csc_test
        isp_top_job_test)

    add_executable(isp_colorchecker_model_test
        tests/colorchecker_model_test.cpp)
    target_link_libraries(isp_colorchecker_model_test
        PRIVATE isp_systemc_model)
    if(MSVC)
        target_compile_options(isp_colorchecker_model_test PRIVATE /W4 /WX)
    else()
        target_compile_options(isp_colorchecker_model_test PRIVATE
            -Wall -Wextra -Wpedantic -Werror
            -Wno-overloaded-virtual)
    endif()

    set(ISP_COLORCHECKER_RAW "" CACHE FILEPATH
        "Path to a 2592x1536 RAW12 RGGB little-endian fixture")
    option(ISP_SYSTEMC_RUN_COLORCHECKER_TEST
        "Register the long full-frame ColorChecker validation with CTest"
        OFF)
    if(ISP_SYSTEMC_RUN_COLORCHECKER_TEST)
        if(NOT ISP_COLORCHECKER_RAW)
            message(FATAL_ERROR
                "ISP_SYSTEMC_RUN_COLORCHECKER_TEST requires "
                "-DISP_COLORCHECKER_RAW=/path/to/input.raw")
        endif()
        if(NOT EXISTS "${ISP_COLORCHECKER_RAW}")
            message(FATAL_ERROR
                "ISP_COLORCHECKER_RAW does not exist: "
                "${ISP_COLORCHECKER_RAW}")
        endif()
        add_test(NAME isp_colorchecker_model_test
            COMMAND isp_colorchecker_model_test "${ISP_COLORCHECKER_RAW}")
        set_tests_properties(isp_colorchecker_model_test PROPERTIES
            LABELS "long;colorchecker")
        list(APPEND _isp_ctest_targets isp_colorchecker_model_test)
    endif()

    # CMake-only convenience entry point. This builds every registered ISP
    # test and runs CTest from this binary directory. When the long test option
    # is enabled, the selected RAW fixture is included automatically.
    add_custom_target(isp_systemc_check
        COMMAND "${CMAKE_CTEST_COMMAND}" --output-on-failure
        DEPENDS ${_isp_ctest_targets}
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
        COMMENT "Running ISP SystemC tests"
        USES_TERMINAL)
endif()
