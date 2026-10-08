# cdc::components::fx1_plic — N-source, N-context PLIC (SiFive layout).
add_library(fx1_plic src/plic.cpp)
add_library(cdc::components::fx1_plic ALIAS fx1_plic)
target_compile_features(fx1_plic PUBLIC cxx_std_17)
target_include_directories(fx1_plic PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>)
target_link_libraries(fx1_plic PUBLIC SystemC::systemc)
target_compile_options(fx1_plic PRIVATE -Wall -Wextra -Wpedantic)

if(CDC_BUILD_TESTS)
  add_executable(test_fx1_plic tests/test_plic.cpp)
  target_link_libraries(test_fx1_plic PRIVATE fx1_plic)
  add_test(NAME fx1_plic COMMAND test_fx1_plic)
  set_tests_properties(fx1_plic PROPERTIES TIMEOUT 30 LABELS "fx1;fx1_unit")
endif()

install(TARGETS fx1_plic EXPORT cdc-components-targets
  ARCHIVE DESTINATION lib LIBRARY DESTINATION lib RUNTIME DESTINATION bin)
install(DIRECTORY include/ DESTINATION include)
