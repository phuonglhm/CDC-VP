include(CMakeFindDependencyMacro)
find_dependency(fu3-bus CONFIG)
include("${CMAKE_CURRENT_LIST_DIR}/fu3-hyperram-targets.cmake")
