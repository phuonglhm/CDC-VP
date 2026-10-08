# cdc::components::fx1_sparse_ram — page-allocated RAM/ROM TLM target.
add_library(fx1_sparse_ram src/sparse_ram.cpp)
add_library(cdc::components::fx1_sparse_ram ALIAS fx1_sparse_ram)
target_compile_features(fx1_sparse_ram PUBLIC cxx_std_17)
target_include_directories(fx1_sparse_ram PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>)
target_link_libraries(fx1_sparse_ram PUBLIC SystemC::systemc)
target_compile_options(fx1_sparse_ram PRIVATE -Wall -Wextra -Wpedantic)

if(CDC_BUILD_TESTS)
  add_executable(test_fx1_sparse_ram tests/test_sparse_ram.cpp)
  target_link_libraries(test_fx1_sparse_ram PRIVATE fx1_sparse_ram)
  add_test(NAME fx1_sparse_ram COMMAND test_fx1_sparse_ram)
  set_tests_properties(fx1_sparse_ram PROPERTIES TIMEOUT 30 LABELS "fx1;fx1_unit")
endif()

install(TARGETS fx1_sparse_ram EXPORT cdc-components-targets
  ARCHIVE DESTINATION lib LIBRARY DESTINATION lib RUNTIME DESTINATION bin)
install(DIRECTORY include/ DESTINATION include)
