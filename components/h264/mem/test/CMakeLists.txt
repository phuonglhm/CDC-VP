cmake_minimum_required(VERSION 3.16)
project(h264_vp_mem_test LANGUAGES CXX)

set(SYSTEMC_HOME "/usr/local/systemc-2.3.4")
find_package(SystemCLanguage CONFIG REQUIRED HINTS "${SYSTEMC_HOME}/lib/cmake/SystemCLanguage")

add_library(h264_vp_mem STATIC 
    ../src/memory_map.cpp
    ../src/internal_memory.cpp
    ../src/external_memory.cpp
)
target_include_directories(h264_vp_mem PUBLIC ../include)
target_link_libraries(h264_vp_mem PUBLIC SystemC::systemc)

add_executable(test_h264_vp_mem test_mem.cpp)
target_link_libraries(test_h264_vp_mem PRIVATE h264_vp_mem)

enable_testing()
add_test(NAME h264_vp_mem_unit COMMAND test_h264_vp_mem)