cmake_minimum_required(VERSION 3.16)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  project(bus_system_tlm LANGUAGES CXX)
endif()

if(NOT TARGET SystemC::systemc)
  if(SYSTEMC_HOME)
    list(PREPEND CMAKE_PREFIX_PATH "${SYSTEMC_HOME}")
  elseif(DEFINED ENV{SYSTEMC_HOME})
    list(PREPEND CMAKE_PREFIX_PATH "$ENV{SYSTEMC_HOME}")
  endif()
  find_package(SystemCLanguage CONFIG REQUIRED)
endif()

add_library(bus_system src/config.cpp src/router.cpp src/apb_bridge.cpp src/bus_system.cpp)
add_library(cdc::components::bus_system ALIAS bus_system)
target_compile_features(bus_system PUBLIC cxx_std_17)
target_include_directories(bus_system PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>)
target_link_libraries(bus_system PUBLIC SystemC::systemc)
# BusConfig::fx1() takes its addresses from the shared FX1 memory map. Only the
# implementation includes it, so the installed bus headers stay self-contained.
target_include_directories(bus_system PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../fx1_map/include)
if(MSVC)
  target_compile_options(bus_system PRIVATE /W4)
else()
  target_compile_options(bus_system PRIVATE -Wall -Wextra -Wpedantic)
endif()
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  include(CTest)
  set(_bus_tests_default ${BUILD_TESTING})
elseif(DEFINED CDC_BUILD_TESTS)
  set(_bus_tests_default ${CDC_BUILD_TESTS})
else()
  set(_bus_tests_default OFF)
endif()
option(BUS_SYSTEM_BUILD_TESTS "Build standalone bus regression" ${_bus_tests_default})
if(BUS_SYSTEM_BUILD_TESTS)
  enable_testing()
  add_executable(bus_behavior_tests tests/bus_behavior_tests.cpp)
  target_link_libraries(bus_behavior_tests PRIVATE bus_system)
  add_test(NAME bus_behavior COMMAND bus_behavior_tests)
  set_tests_properties(bus_behavior PROPERTIES TIMEOUT 30)
  add_executable(bus_config_tests tests/bus_config_tests.cpp)
  target_link_libraries(bus_config_tests PRIVATE bus_system)
  add_test(NAME bus_config COMMAND bus_config_tests)
  set_tests_properties(bus_config PROPERTIES TIMEOUT 10)

  add_executable(bus_arbitration_tests tests/bus_arbitration_tests.cpp)
  target_link_libraries(bus_arbitration_tests PRIVATE bus_system)
  add_test(NAME bus_arbitration COMMAND bus_arbitration_tests)
  set_tests_properties(bus_arbitration PROPERTIES TIMEOUT 30)
endif()

# CDC-VP owns this export set and its package config at components/ level.
option(BUS_SYSTEM_USE_CDC_EXPORT "Join the parent-owned CDC components export set" OFF)
if(CMAKE_PROJECT_NAME STREQUAL "cdc-vp" OR BUS_SYSTEM_USE_CDC_EXPORT)
  install(TARGETS bus_system EXPORT cdc-components-targets ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib RUNTIME DESTINATION bin)
else()
  install(TARGETS bus_system EXPORT bus-system-targets ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib RUNTIME DESTINATION bin)
  install(EXPORT bus-system-targets NAMESPACE cdc::components::
    DESTINATION lib/cmake/bus-system)
  install(FILES cmake/bus-system-config.cmake DESTINATION lib/cmake/bus-system)
endif()
install(DIRECTORY include/ DESTINATION include)
