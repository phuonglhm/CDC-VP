cmake_minimum_required(VERSION 3.21)

# This directory is injected into the CDC-VP build by
# cmake/inject_tutorial.cmake.  Building in that context lets the example use
# the exact component and CPU targets from the reference repository without
# modifying that repository.

add_library(edu_timer_tlm STATIC
    components/edu_timer_tlm/src/edu_timer_tlm.cpp)
add_library(tutorial::edu_timer_tlm ALIAS edu_timer_tlm)

target_compile_features(edu_timer_tlm PUBLIC cxx_std_17)
target_include_directories(edu_timer_tlm
    PUBLIC components/edu_timer_tlm/include)
target_link_libraries(edu_timer_tlm PUBLIC SystemC::systemc)

add_executable(test_edu_timer_tlm
    components/edu_timer_tlm/tests/test_edu_timer_tlm.cpp)
target_link_libraries(test_edu_timer_tlm PRIVATE tutorial::edu_timer_tlm)

add_test(NAME tutorial_edu_timer_unit COMMAND test_edu_timer_tlm)
set_tests_properties(tutorial_edu_timer_unit PROPERTIES LABELS "tutorial;timer")

add_executable(mini_soc_edu_timer
    platforms/mini_soc_real_cpu/src/main.cpp
    platforms/mini_soc_real_cpu/src/mini_soc_top.cpp)

target_compile_features(mini_soc_edu_timer PRIVATE cxx_std_17)
target_include_directories(mini_soc_edu_timer PRIVATE
    platforms/mini_soc_real_cpu/src)
target_link_libraries(mini_soc_edu_timer PRIVATE
    SystemC::systemc
    tutorial::edu_timer_tlm
    cdc::components::common
    cdc::components::bus_router
    cdc::components::memory_tlm
    cdc::components::uart2_tlm
    cdc::cpu::riscv_vp)

