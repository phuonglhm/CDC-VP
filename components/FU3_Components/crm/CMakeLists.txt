cmake_minimum_required(VERSION 3.16)

project(fu3_crm VERSION 0.1.0 LANGUAGES CXX)

include(CTest)

if(NOT TARGET SystemC::systemc)
  if(SYSTEMC_HOME)
    list(PREPEND CMAKE_PREFIX_PATH "${SYSTEMC_HOME}")
  endif()
  find_package(SystemCLanguage CONFIG REQUIRED)
endif()

add_library(fu3_crm src/crm.cpp)
add_library(fu3::crm ALIAS fu3_crm)

set_target_properties(fu3_crm PROPERTIES EXPORT_NAME crm POSITION_INDEPENDENT_CODE ON WINDOWS_EXPORT_ALL_SYMBOLS ON)

target_compile_features(fu3_crm PUBLIC cxx_std_17)
target_include_directories(fu3_crm PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include> $<INSTALL_INTERFACE:include>)
target_link_libraries(fu3_crm PUBLIC SystemC::systemc)

if(NOT MSVC)
  target_compile_options(fu3_crm PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(BUILD_TESTING)
  set(FU3_BUS_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  if(NOT TARGET fu3::bus_system)
    add_subdirectory(../bus bus)
  endif()
  foreach(kind behavior integration)
    add_executable(crm_${kind}_tests tests/${kind}.cpp)
    target_link_libraries(crm_${kind}_tests PRIVATE fu3::crm fu3::bus_system)
    add_test(NAME crm_${kind} COMMAND crm_${kind}_tests)
    set_tests_properties(crm_${kind} PROPERTIES TIMEOUT 30)
  endforeach()
endif()

install(TARGETS fu3_crm EXPORT fu3-crm-targets ARCHIVE DESTINATION lib LIBRARY DESTINATION lib RUNTIME DESTINATION bin)
install(EXPORT fu3-crm-targets NAMESPACE fu3:: DESTINATION lib/cmake/fu3-crm)
install(FILES cmake/fu3-crm-config.cmake DESTINATION lib/cmake/fu3-crm)
install(DIRECTORY include/ DESTINATION include)
