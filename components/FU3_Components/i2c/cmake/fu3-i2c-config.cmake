include(CMakeFindDependencyMacro)
find_dependency(SystemCLanguage CONFIG)
include("${CMAKE_CURRENT_LIST_DIR}/fu3-i2c-targets.cmake")
