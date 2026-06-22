cmake_minimum_required(VERSION 3.16)

project(otp_tlm_complete LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Cho phép truyền từ command line:
# -DSYSTEMC_HOME=/opt/systemc-2.3.4
set(SYSTEMC_HOME "/opt/systemc-2.3.4" CACHE PATH "SystemC installation path")

# Tìm SystemC bằng pkg-config trước
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(SYSTEMC QUIET systemc)
endif()

if(SYSTEMC_FOUND)
    message(STATUS "Found SystemC using pkg-config")
    message(STATUS "SystemC include dirs: ${SYSTEMC_INCLUDE_DIRS}")
    message(STATUS "SystemC libraries: ${SYSTEMC_LIBRARIES}")

    add_executable(otp_tlm
        otp.cpp
        tb.cpp
    )

    target_include_directories(otp_tlm PRIVATE
        ${SYSTEMC_INCLUDE_DIRS}
        ${CMAKE_CURRENT_SOURCE_DIR}
    )

    target_link_libraries(otp_tlm PRIVATE
        ${SYSTEMC_LIBRARIES}
    )

    target_compile_options(otp_tlm PRIVATE
        ${SYSTEMC_CFLAGS_OTHER}
    )

else()
    message(STATUS "pkg-config cannot find SystemC, using SYSTEMC_HOME=${SYSTEMC_HOME}")

    if(EXISTS "${SYSTEMC_HOME}/lib-linux64/libsystemc.so")
        set(SYSTEMC_LIB_DIR "${SYSTEMC_HOME}/lib-linux64")
    elseif(EXISTS "${SYSTEMC_HOME}/lib64/libsystemc.so")
        set(SYSTEMC_LIB_DIR "${SYSTEMC_HOME}/lib64")
    elseif(EXISTS "${SYSTEMC_HOME}/lib/libsystemc.so")
        set(SYSTEMC_LIB_DIR "${SYSTEMC_HOME}/lib")
    else()
        message(FATAL_ERROR "Cannot find libsystemc.so under ${SYSTEMC_HOME}")
    endif()

    add_executable(otp_tlm
        otp.cpp
        tb.cpp
    )

    target_include_directories(otp_tlm PRIVATE
        ${SYSTEMC_HOME}/include
        ${CMAKE_CURRENT_SOURCE_DIR}
    )

    target_link_directories(otp_tlm PRIVATE
        ${SYSTEMC_LIB_DIR}
    )

    target_link_libraries(otp_tlm PRIVATE
        systemc
        m
    )

    target_compile_definitions(otp_tlm PRIVATE
        SC_INCLUDE_DYNAMIC_PROCESSES
    )

    message(STATUS "SystemC lib dir: ${SYSTEMC_LIB_DIR}")
endif()

target_compile_options(otp_tlm PRIVATE
    -Wall
    -Wextra
    -O0
    -g
)
