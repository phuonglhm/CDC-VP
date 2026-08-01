cmake_minimum_required(VERSION 3.21)

# Configure this directory as its own source tree. It deliberately has no
# add_subdirectory() path back into CDC-VP: success proves that the installed
# package exports the compiled noc_interconnect target and all transitive usage
# requirements.
project(floo_noc_installed_consumer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(cdc-components CONFIG REQUIRED)

add_executable(floo_noc_installed_consumer main.cpp)
target_link_libraries(floo_noc_installed_consumer
    PRIVATE cdc::components::noc_interconnect)
