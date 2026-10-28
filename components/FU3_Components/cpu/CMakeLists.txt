# FU3 CPU configuration (CVA6 + Ibex) on top of the cva6_vp wrapper.
# Header-only and built only inside the repository build (CDC_BUILD_FU3_SOC=ON), because cdc_cpu_cva6 comes from cpu_models/ and needs the top-level paths.
if(NOT TARGET cdc_cpu_cva6)
  message(FATAL_ERROR "fu3::cpu needs cdc_cpu_cva6: configure the repository root with -DCDC_BUILD_FU3_SOC=ON")
endif()

add_library(fu3_cpu INTERFACE)
add_library(fu3::cpu ALIAS fu3_cpu)

target_include_directories(fu3_cpu INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(fu3_cpu INTERFACE cdc_cpu_cva6)

# Wrapper reset/interrupt behaviour with the FU3 option profiles; the firmware
# tests of both CPUs run on the platform (platforms/fu3_soc).
if(CDC_BUILD_TESTS)
  add_executable(test_fu3_cpu_reset tests/test_cpu_reset.cpp)
  target_link_libraries(test_fu3_cpu_reset PRIVATE fu3::cpu)
  foreach(core cva6 ibex)
    foreach(scenario fetch load store wfi irq)
      add_test(NAME fu3_cpu_reset_${core}_${scenario} COMMAND test_fu3_cpu_reset ${core} ${scenario})
      set_tests_properties(fu3_cpu_reset_${core}_${scenario} PROPERTIES TIMEOUT 60 LABELS "fu3;fu3_cpu")
    endforeach()
  endforeach()

  # Debug Mode control as a Debug Module sees it (T06).
  add_executable(test_fu3_cpu_debug tests/test_cpu_debug.cpp)
  target_link_libraries(test_fu3_cpu_debug PRIVATE fu3::cpu)
  foreach(core cva6 ibex)
    add_test(NAME fu3_cpu_debug_${core} COMMAND test_fu3_cpu_debug ${core})
    set_tests_properties(fu3_cpu_debug_${core} PROPERTIES TIMEOUT 60 LABELS "fu3;fu3_cpu")
  endforeach()

  # Core clock from the CRM: period changes and stop (T07).
  add_executable(test_fu3_cpu_clock tests/test_cpu_clock.cpp)
  target_link_libraries(test_fu3_cpu_clock PRIVATE fu3::cpu)
  add_test(NAME fu3_cpu_clock COMMAND test_fu3_cpu_clock)
  set_tests_properties(fu3_cpu_clock PROPERTIES TIMEOUT 60 LABELS "fu3;fu3_cpu")
endif()
