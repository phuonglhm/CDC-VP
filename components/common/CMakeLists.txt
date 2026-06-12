# Header-only shared types/helpers for cdc components.
add_library(common INTERFACE)
add_library(cdc::components::common ALIAS common)

target_include_directories(common
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>)

install(TARGETS common EXPORT cdc-components-targets
    INCLUDES DESTINATION include)
install(DIRECTORY include/ DESTINATION include)
