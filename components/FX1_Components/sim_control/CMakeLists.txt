# cdc::components::fx1_sim_control — VP-only PASS/FAIL finisher.
add_library(fx1_sim_control src/sim_control.cpp)
add_library(cdc::components::fx1_sim_control ALIAS fx1_sim_control)
target_compile_features(fx1_sim_control PUBLIC cxx_std_17)
target_include_directories(fx1_sim_control PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>)
target_link_libraries(fx1_sim_control PUBLIC SystemC::systemc PRIVATE fx1_map)
target_compile_options(fx1_sim_control PRIVATE -Wall -Wextra -Wpedantic)

if(CDC_BUILD_TESTS)
  add_executable(test_fx1_sim_control tests/test_sim_control.cpp)
  target_link_libraries(test_fx1_sim_control PRIVATE fx1_sim_control fx1_map)
  add_test(NAME fx1_sim_control COMMAND test_fx1_sim_control)
  set_tests_properties(fx1_sim_control PROPERTIES TIMEOUT 30 LABELS "fx1;fx1_unit")
endif()

install(TARGETS fx1_sim_control EXPORT cdc-components-targets
  ARCHIVE DESTINATION lib LIBRARY DESTINATION lib RUNTIME DESTINATION bin)
install(DIRECTORY include/ DESTINATION include)
