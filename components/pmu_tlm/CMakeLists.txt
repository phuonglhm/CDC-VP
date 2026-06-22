cmake_minimum_required(VERSION 3.16)
project(pmu_tlm LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Optional: tests toggle like in your example
option(CDC_BUILD_TESTS "Build tests" OFF)

# SystemC
set(SYSTEMC_HOME "/usr/local/systemc-2.3.4")

add_library(SystemC::systemc UNKNOWN IMPORTED)
set_target_properties(SystemC::systemc PROPERTIES
    IMPORTED_LOCATION "${SYSTEMC_HOME}/lib/libsystemc.so"
    INTERFACE_INCLUDE_DIRECTORIES "${SYSTEMC_HOME}/include"
)

# Main library
add_library(pmu_tlm STATIC
    src/pmu.cpp
)

add_library(cdc::components::pmu_tlm ALIAS pmu_tlm)

target_include_directories(pmu_tlm
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        src
)

target_link_libraries(pmu_tlm
    PUBLIC
        SystemC::systemc
)

# Testbench executable
add_executable(tb
    src/testbench.cpp
)

target_link_libraries(tb
    PRIVATE
        pmu_tlm
)

# Runtime search path for libsystemc.so
set_target_properties(tb PROPERTIES
    BUILD_RPATH "${SYSTEMC_HOME}/lib"
)

# Install rules
install(TARGETS pmu_tlm EXPORT cdc-components-targets
    ARCHIVE DESTINATION lib
    INCLUDES DESTINATION include
)

install(DIRECTORY include/ DESTINATION include)

# Optional tests folder
if(CDC_BUILD_TESTS)
    add_subdirectory(tests)
endif()