cmake_minimum_required(VERSION 3.16)

project(fu3_plic VERSION 0.1.0 LANGUAGES CXX)

include(CTest)

if(NOT TARGET SystemC::systemc)
  if(SYSTEMC_HOME)
    list(PREPEND CMAKE_PREFIX_PATH "${SYSTEMC_HOME}")
  endif()
  find_package(SystemCLanguage CONFIG REQUIRED)
endif()

add_library(fu3_plic src/plic.cpp src/intmap.cpp)
add_library(fu3::plic ALIAS fu3_plic)

set_target_properties(fu3_plic PROPERTIES EXPORT_NAME plic POSITION_INDEPENDENT_CODE ON WINDOWS_EXPORT_ALL_SYMBOLS ON)

target_compile_features(fu3_plic PUBLIC cxx_std_17)
target_include_directories(fu3_plic PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include> $<INSTALL_INTERFACE:include>)
target_link_libraries(fu3_plic PUBLIC SystemC::systemc)

if(NOT MSVC)
  target_compile_options(fu3_plic PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(BUILD_TESTING)
  if(NOT TARGET fu3::bus_system)
    # Own copy of the bus: skip its tests in this scope only, never in the cache.
    set(FU3_BUS_BUILD_TESTS OFF)
    add_subdirectory(../bus bus)
  endif()

  foreach(kind behavior integration)
    add_executable(plic_${kind}_tests tests/${kind}.cpp)
    target_link_libraries(plic_${kind}_tests PRIVATE fu3::plic fu3::bus_system)
    add_test(NAME plic_${kind} COMMAND plic_${kind}_tests)
    set_tests_properties(plic_${kind} PROPERTIES TIMEOUT 30)
  endforeach()
  
    add_executable(intmap_tests tests/intmap.cpp)
  target_link_libraries(intmap_tests PRIVATE fu3::plic)
  add_test(NAME intmap COMMAND intmap_tests)
  set_tests_properties(intmap PROPERTIES TIMEOUT 30)
endif()

install(TARGETS fu3_plic EXPORT fu3-plic-targets ARCHIVE DESTINATION lib LIBRARY DESTINATION lib RUNTIME DESTINATION bin)
install(EXPORT fu3-plic-targets NAMESPACE fu3:: DESTINATION lib/cmake/fu3-plic)
install(FILES cmake/fu3-plic-config.cmake DESTINATION lib/cmake/fu3-plic)
install(DIRECTORY include/ DESTINATION include)
