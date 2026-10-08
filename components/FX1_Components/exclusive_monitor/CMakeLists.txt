# cdc::components::fx1_exclusive_monitor — LR/SC reservations and AMO
# exclusion shared by the CPU harts and the non-CPU masters (plan C7).
add_library(fx1_exclusive_monitor src/exclusive_monitor.cpp)
add_library(cdc::components::fx1_exclusive_monitor ALIAS fx1_exclusive_monitor)
target_compile_features(fx1_exclusive_monitor PUBLIC cxx_std_17)
target_include_directories(fx1_exclusive_monitor PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>)
# cdc::cpu::base carries cdc/cpu/exclusive_monitor_if.h, the hook contract
# the CPU backend calls.
target_link_libraries(fx1_exclusive_monitor PUBLIC $<BUILD_INTERFACE:cdc::cpu::base> SystemC::systemc)
target_compile_options(fx1_exclusive_monitor PRIVATE -Wall -Wextra -Wpedantic)

if(CDC_BUILD_TESTS)
  add_executable(test_fx1_exclusive_monitor tests/test_exclusive_monitor.cpp)
  target_link_libraries(test_fx1_exclusive_monitor PRIVATE fx1_exclusive_monitor)
  add_test(NAME fx1_exclusive_monitor COMMAND test_fx1_exclusive_monitor)
  set_tests_properties(fx1_exclusive_monitor PROPERTIES TIMEOUT 30 LABELS "fx1;fx1_unit")
endif()

install(TARGETS fx1_exclusive_monitor EXPORT cdc-components-targets
  ARCHIVE DESTINATION lib LIBRARY DESTINATION lib RUNTIME DESTINATION bin)
install(DIRECTORY include/ DESTINATION include)
