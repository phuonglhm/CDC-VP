cmake_minimum_required(VERSION 3.16)
project(fu3_syscsr VERSION 0.1.0 LANGUAGES CXX)
include(CTest)

if(NOT TARGET SystemC::systemc)
  if(SYSTEMC_HOME)
    list(PREPEND CMAKE_PREFIX_PATH "${SYSTEMC_HOME}")
  endif()
  find_package(SystemCLanguage CONFIG REQUIRED)
endif()

add_library(fu3_syscsr src/syscsr.cpp)
add_library(fu3::syscsr ALIAS fu3_syscsr)

set_target_properties(fu3_syscsr PROPERTIES EXPORT_NAME syscsr POSITION_INDEPENDENT_CODE ON WINDOWS_EXPORT_ALL_SYMBOLS ON)

target_compile_features(fu3_syscsr PUBLIC cxx_std_17)
target_include_directories(fu3_syscsr PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include> $<INSTALL_INTERFACE:include>)
target_link_libraries(fu3_syscsr PUBLIC SystemC::systemc)

if(NOT MSVC)
  target_compile_options(fu3_syscsr PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(BUILD_TESTING)
  if(NOT TARGET fu3::bus_system)
    # Own copy of the bus: skip its tests in this scope only, never in the cache.
    set(FU3_BUS_BUILD_TESTS OFF)
    add_subdirectory(../bus bus)
  endif()
  foreach(kind behavior integration)
    add_executable(syscsr_${kind}_tests tests/${kind}.cpp)
    target_link_libraries(syscsr_${kind}_tests PRIVATE fu3::syscsr fu3::bus_system)
    add_test(NAME syscsr_${kind} COMMAND syscsr_${kind}_tests)
    set_tests_properties(syscsr_${kind} PROPERTIES TIMEOUT 30)
  endforeach()
endif()

install(TARGETS fu3_syscsr EXPORT fu3-syscsr-targets ARCHIVE DESTINATION lib LIBRARY DESTINATION lib RUNTIME DESTINATION bin)
install(EXPORT fu3-syscsr-targets NAMESPACE fu3:: DESTINATION lib/cmake/fu3-syscsr)
install(FILES cmake/fu3-syscsr-config.cmake DESTINATION lib/cmake/fu3-syscsr)
install(DIRECTORY include/ DESTINATION include)
