# cdc::components::fx1_clint — N-hart CLINT (msip / mtimecmp / mtime).
add_library(fx1_clint src/clint.cpp)
add_library(cdc::components::fx1_clint ALIAS fx1_clint)
target_compile_features(fx1_clint PUBLIC cxx_std_17)
target_include_directories(fx1_clint PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>)
target_link_libraries(fx1_clint PUBLIC SystemC::systemc)
target_compile_options(fx1_clint PRIVATE -Wall -Wextra -Wpedantic)

if(CDC_BUILD_TESTS)
  add_executable(test_fx1_clint tests/test_clint.cpp)
  target_link_libraries(test_fx1_clint PRIVATE fx1_clint)
  add_test(NAME fx1_clint COMMAND test_fx1_clint)
  set_tests_properties(fx1_clint PROPERTIES TIMEOUT 30 LABELS "fx1;fx1_unit")
endif()

install(TARGETS fx1_clint EXPORT cdc-components-targets
  ARCHIVE DESTINATION lib LIBRARY DESTINATION lib RUNTIME DESTINATION bin)
install(DIRECTORY include/ DESTINATION include)
