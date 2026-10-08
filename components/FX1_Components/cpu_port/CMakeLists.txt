# cdc::components::fx1_cpu_port — VP++ hart -> FX1 bus adapter (payload
# completion and guest-fault mapping).
add_library(fx1_cpu_port src/cpu_port_adapter.cpp)
add_library(cdc::components::fx1_cpu_port ALIAS fx1_cpu_port)
target_compile_features(fx1_cpu_port PUBLIC cxx_std_17)
target_include_directories(fx1_cpu_port PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>)
target_link_libraries(fx1_cpu_port PUBLIC SystemC::systemc)
target_compile_options(fx1_cpu_port PRIVATE -Wall -Wextra -Wpedantic)

if(CDC_BUILD_TESTS)
  add_executable(test_fx1_cpu_port tests/test_cpu_port_adapter.cpp)
  target_link_libraries(test_fx1_cpu_port PRIVATE fx1_cpu_port)
  add_test(NAME fx1_cpu_port COMMAND test_fx1_cpu_port)
  set_tests_properties(fx1_cpu_port PROPERTIES TIMEOUT 30 LABELS "fx1;fx1_unit")
endif()

install(TARGETS fx1_cpu_port EXPORT cdc-components-targets
  ARCHIVE DESTINATION lib LIBRARY DESTINATION lib RUNTIME DESTINATION bin)
install(DIRECTORY include/ DESTINATION include)
