cmake_minimum_required(VERSION 3.16)
project(crm_consumer LANGUAGES CXX)

find_package(fu3-crm CONFIG REQUIRED)

add_executable(crm_consumer ../behavior.cpp)
target_link_libraries(crm_consumer PRIVATE fu3::crm)

enable_testing()

add_test(NAME installed_crm_behavior COMMAND crm_consumer)
set_tests_properties(installed_crm_behavior PROPERTIES TIMEOUT 30)
