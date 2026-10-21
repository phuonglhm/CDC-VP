cmake_minimum_required(VERSION 3.16)
project(dbg_consumer LANGUAGES CXX)

find_package(fu3-dbg CONFIG REQUIRED)

add_executable(dbg_consumer ../behavior.cpp)
target_link_libraries(dbg_consumer PRIVATE fu3::dbg)

enable_testing()

add_test(NAME installed_dbg_behavior COMMAND dbg_consumer)
set_tests_properties(installed_dbg_behavior PROPERTIES TIMEOUT 30)
