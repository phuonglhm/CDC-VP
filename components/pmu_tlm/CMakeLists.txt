add_library(pmu_tlm STATIC
    src/pmu.cpp
)

add_library(cdc::components::pmu_tlm ALIAS pmu_tlm)

target_include_directories(pmu_tlm
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        src
)

target_link_libraries(pmu_tlm
    PUBLIC
        SystemC::systemc
)

# Optional standalone PMU testbench.
# Use pmu_tb instead of tb to avoid target name conflict with other components.
if(CDC_BUILD_TESTS)
    add_executable(pmu_tb
        src/testbench.cpp
    )

    target_link_libraries(pmu_tb
        PRIVATE
            pmu_tlm
    )
endif()

install(TARGETS pmu_tlm EXPORT cdc-components-targets
    ARCHIVE DESTINATION lib
    INCLUDES DESTINATION include
)

install(DIRECTORY include/ DESTINATION include)
