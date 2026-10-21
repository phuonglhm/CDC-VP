cmake_minimum_required(VERSION 3.16)

project(fu3_mailbox VERSION 0.1.0 LANGUAGES CXX)

include(CTest)

if(NOT TARGET SystemC::systemc)
  if(SYSTEMC_HOME)
    list(PREPEND CMAKE_PREFIX_PATH "${SYSTEMC_HOME}")
  endif()
  find_package(SystemCLanguage CONFIG REQUIRED)
endif()

add_library(fu3_mailbox src/mailbox.cpp)
add_library(fu3::mailbox ALIAS fu3_mailbox)

set_target_properties(fu3_mailbox PROPERTIES EXPORT_NAME mailbox POSITION_INDEPENDENT_CODE ON WINDOWS_EXPORT_ALL_SYMBOLS ON)

target_compile_features(fu3_mailbox PUBLIC cxx_std_17)
target_include_directories(fu3_mailbox PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include> $<INSTALL_INTERFACE:include>)
target_link_libraries(fu3_mailbox PUBLIC SystemC::systemc)

if(NOT MSVC)
  target_compile_options(fu3_mailbox PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(BUILD_TESTING)
  set(FU3_BUS_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  if(NOT TARGET fu3::bus_system)
    add_subdirectory(../bus bus)
  endif()
  foreach(kind behavior integration)
    add_executable(mailbox_${kind}_tests tests/${kind}.cpp)
    target_link_libraries(mailbox_${kind}_tests PRIVATE fu3::mailbox fu3::bus_system)
    add_test(NAME mailbox_${kind} COMMAND mailbox_${kind}_tests)
    set_tests_properties(mailbox_${kind} PROPERTIES TIMEOUT 30)
  endforeach()
endif()

install(TARGETS fu3_mailbox EXPORT fu3-mailbox-targets ARCHIVE DESTINATION lib LIBRARY DESTINATION lib RUNTIME DESTINATION bin)
install(EXPORT fu3-mailbox-targets NAMESPACE fu3:: DESTINATION lib/cmake/fu3-mailbox)
install(FILES cmake/fu3-mailbox-config.cmake DESTINATION lib/cmake/fu3-mailbox)
install(DIRECTORY include/ DESTINATION include)
