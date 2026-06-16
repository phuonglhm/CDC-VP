# Packaging helpers to make a platform executable self-contained, mirroring how
# Arm Fast Models' simgen bundles its runtime .so files next to isim_system.
#
# These functions derive the SystemC library directory from the SystemC::systemc
# imported target, so they work both in the main build tree and in a standalone
# platform build that obtained the target via find_package(cdc-components).

# Internal: list the libsystemc.so* chain next to the SystemC import.
function(_cdc_systemc_runtime_libs out_var)
    get_target_property(_loc SystemC::systemc IMPORTED_LOCATION)
    get_filename_component(_dir "${_loc}" DIRECTORY)
    file(GLOB _sos "${_dir}/libsystemc.so*")
    set(${out_var} "${_sos}" PARENT_SCOPE)
endfunction()

# cdc_make_portable(<target>)
#   - rpath = $ORIGIN  (look for shared libs next to the binary)
#   - copy the SystemC runtime .so chain beside the binary after each build
function(cdc_make_portable target)
    set_target_properties(${target} PROPERTIES
        BUILD_RPATH "$ORIGIN"
        INSTALL_RPATH "$ORIGIN")

    _cdc_systemc_runtime_libs(_sos)
    foreach(_so ${_sos})
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${_so}" "$<TARGET_FILE_DIR:${target}>"
            VERBATIM)
    endforeach()
endfunction()

# cdc_package_platform(<target>)
#   Adds target "<target>_package" -> assembles out/<target>/ with just the
#   binary, the bundled .so files and configs/.  Build with:
#     cmake --build <dir> --target <target>_package
function(cdc_package_platform target)
    set(_dest "${CMAKE_SOURCE_DIR}/out/${target}")

    add_custom_target(${target}_package
        COMMAND ${CMAKE_COMMAND} -E rm -rf "${_dest}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${_dest}"
        COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:${target}>" "${_dest}"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "$<TARGET_FILE_DIR:${target}>/configs" "${_dest}/configs"
        DEPENDS ${target}
        COMMENT "Packaging ${target} -> out/${target} (self-contained)"
        VERBATIM)

    _cdc_systemc_runtime_libs(_sos)
    foreach(_so ${_sos})
        add_custom_command(TARGET ${target}_package POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy "${_so}" "${_dest}"
            VERBATIM)
    endforeach()
endfunction()
