add_library(pwm_tlm STATIC pwm.cpp)
add_library(cdc::components::pwm_tlm ALIAS pwm_tlm)

target_include_directories(pwm_tlm
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>)

target_link_libraries(pwm_tlm PUBLIC SystemC::systemc)