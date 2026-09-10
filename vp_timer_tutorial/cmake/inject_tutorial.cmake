# Loaded with CMAKE_PROJECT_INCLUDE while configuring the containing CDC-VP
# repository.  Keeping the tutorial isolated avoids changes to the existing
# component/platform CMake files.
# CMAKE_PROJECT_INCLUDE can be evaluated for nested third-party projects too,
# therefore only act for the CDC-VP top-level project.
if(PROJECT_NAME STREQUAL "cdc-vp" AND CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    enable_testing()
    get_filename_component(_tutorial_root "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
    add_subdirectory("${_tutorial_root}"
                     "${CMAKE_BINARY_DIR}/vp_timer_tutorial")
endif()
