# cdc::components::fx1_access_guard — word-only access filter for register
# models that always transfer 4 bytes.
add_library(fx1_access_guard src/word_access_guard.cpp)
add_library(cdc::components::fx1_access_guard ALIAS fx1_access_guard)
target_compile_features(fx1_access_guard PUBLIC cxx_std_17)
target_include_directories(fx1_access_guard PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>)
target_link_libraries(fx1_access_guard PUBLIC SystemC::systemc)
target_compile_options(fx1_access_guard PRIVATE -Wall -Wextra -Wpedantic)

if(CDC_BUILD_TESTS)
  add_executable(test_fx1_access_guard tests/test_word_access_guard.cpp)
  target_link_libraries(test_fx1_access_guard PRIVATE fx1_access_guard)
  if(TARGET cdc::components::uart2_tlm)
    target_link_libraries(test_fx1_access_guard PRIVATE cdc::components::uart2_tlm)
    target_compile_definitions(test_fx1_access_guard PRIVATE FX1_GUARD_TEST_WITH_UART2)
  endif()
  add_test(NAME fx1_access_guard COMMAND test_fx1_access_guard)
  set_tests_properties(fx1_access_guard PROPERTIES TIMEOUT 30 LABELS "fx1;fx1_unit")
endif()

install(TARGETS fx1_access_guard EXPORT cdc-components-targets
  ARCHIVE DESTINATION lib LIBRARY DESTINATION lib RUNTIME DESTINATION bin)
install(DIRECTORY include/ DESTINATION include)
