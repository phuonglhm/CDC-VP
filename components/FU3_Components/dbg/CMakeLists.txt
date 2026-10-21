cmake_minimum_required(VERSION 3.16)

project(fu3_dbg VERSION 0.1.0 LANGUAGES CXX)

include(CTest)

if(NOT TARGET SystemC::systemc)
  if(SYSTEMC_HOME)
    list(PREPEND CMAKE_PREFIX_PATH "${SYSTEMC_HOME}")
  endif()
  find_package(SystemCLanguage CONFIG REQUIRED)
endif()

add_library(fu3_dbg src/dm.cpp src/jtag2axi.cpp)
add_library(fu3::dbg ALIAS fu3_dbg)

set_target_properties(fu3_dbg PROPERTIES EXPORT_NAME dbg POSITION_INDEPENDENT_CODE ON WINDOWS_EXPORT_ALL_SYMBOLS ON)

target_compile_features(fu3_dbg PUBLIC cxx_std_17)
target_include_directories(fu3_dbg PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include> $<INSTALL_INTERFACE:include>)
target_link_libraries(fu3_dbg PUBLIC SystemC::systemc)
# fu3_bus/access.h (header-only) để gắn Access cho truy cập AXI của cầu JTAG2AXI4 trên cổng DBG
target_include_directories(fu3_dbg PRIVATE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../bus/include>)

if(NOT MSVC)
  target_compile_options(fu3_dbg PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(BUILD_TESTING)
  set(FU3_BUS_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  if(NOT TARGET fu3::bus_system)
    add_subdirectory(../bus bus)
  endif()
  foreach(kind behavior integration)
    add_executable(dbg_${kind}_tests tests/${kind}.cpp)
    target_link_libraries(dbg_${kind}_tests PRIVATE fu3::dbg fu3::bus_system)
    add_test(NAME dbg_${kind} COMMAND dbg_${kind}_tests)
    set_tests_properties(dbg_${kind} PROPERTIES TIMEOUT 30)
  endforeach()
endif()

install(TARGETS fu3_dbg EXPORT fu3-dbg-targets ARCHIVE DESTINATION lib LIBRARY DESTINATION lib RUNTIME DESTINATION bin)
install(EXPORT fu3-dbg-targets NAMESPACE fu3:: DESTINATION lib/cmake/fu3-dbg)
install(FILES cmake/fu3-dbg-config.cmake DESTINATION lib/cmake/fu3-dbg)
install(DIRECTORY include/ DESTINATION include)
