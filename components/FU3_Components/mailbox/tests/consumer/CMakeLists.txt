cmake_minimum_required(VERSION 3.16)
project(mailbox_consumer LANGUAGES CXX)

find_package(fu3-mailbox CONFIG REQUIRED)

add_executable(mailbox_consumer ../behavior.cpp)
target_link_libraries(mailbox_consumer PRIVATE fu3::mailbox)

enable_testing()

add_test(NAME installed_mailbox_behavior COMMAND mailbox_consumer)
set_tests_properties(installed_mailbox_behavior PROPERTIES TIMEOUT 30)
