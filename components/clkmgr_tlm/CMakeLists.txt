#Author: trangnm20
#Verified: QuanNH107

add_library(clkmgr_tlm STATIC
    src/clkmgr.cpp)
add_library(cdc::components::clkmgr_tlm ALIAS clkmgr_tlm)
target_include_directories(clkmgr_tlm
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE src)
target_link_libraries(clkmgr_tlm PUBLIC SystemC::systemc)
install(TARGETS clkmgr_tlm EXPORT cdc-components-targets
    ARCHIVE DESTINATION lib
    INCLUDES DESTINATION include)
install(DIRECTORY include/ DESTINATION include)
if(CDC_BUILD_TESTS)
   add_subdirectory(tests)
endif()