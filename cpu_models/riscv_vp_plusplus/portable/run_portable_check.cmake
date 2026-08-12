# SPDX-License-Identifier: Apache-2.0
#
# Proves the portable-executable half of the Phase 2 gate (plan §11.2):
#
#   * the backend is statically linked into a portable test executable;
#   * that executable has no runtime dependency on a VP++ or Spike binary, Qt,
#     VNC, the source tree or the build tree.
#
# The method is the point. Asserting `RUNPATH == $ORIGIN` only checks an
# intention; copying the executable and its libraries into an otherwise empty
# directory and running it there checks the fact. Anything the binary silently
# reached for is simply absent.
#
# Required: -DRUNNER=, -DIMAGE=, -DWORKDIR=, -DREADELF=

foreach(_required RUNNER IMAGE WORKDIR)
    if(NOT DEFINED ${_required})
        message(FATAL_ERROR "run_portable_check.cmake: -D${_required}= is required")
    endif()
endforeach()

if(NOT EXISTS "${RUNNER}")
    message(FATAL_ERROR "portable runner not built: ${RUNNER}")
endif()
if(NOT EXISTS "${IMAGE}")
    # The cross toolchain is a host prerequisite, not a defect in this tree.
    message(STATUS "SKIP: ${IMAGE} was not built")
    execute_process(COMMAND ${CMAKE_COMMAND} -E true)
    message(FATAL_ERROR "__CDC_SKIP__")
endif()

get_filename_component(_runner_dir "${RUNNER}" DIRECTORY)
get_filename_component(_runner_name "${RUNNER}" NAME)
get_filename_component(_image_name "${IMAGE}" NAME)

# ── 1. a directory containing nothing but the deliverable ────────────────────
file(REMOVE_RECURSE "${WORKDIR}")
file(MAKE_DIRECTORY "${WORKDIR}")
file(COPY "${RUNNER}" DESTINATION "${WORKDIR}")
file(COPY "${IMAGE}" DESTINATION "${WORKDIR}")

# The SystemC runtime that `cdc_make_portable` placed beside the executable.
# Copied by glob rather than by name so a SystemC version bump does not silently
# leave the bundle short of a library and fall back to the host's copy.
file(GLOB _bundled "${_runner_dir}/libsystemc*.so*")
if(_bundled STREQUAL "")
    message(FATAL_ERROR
        "no SystemC runtime beside ${RUNNER}. cdc_make_portable() should have "
        "copied it; without it this check would pass only because the host "
        "happens to have SystemC installed.")
endif()
foreach(_so ${_bundled})
    file(COPY "${_so}" DESTINATION "${WORKDIR}")
endforeach()

# ── 2. no absolute host path baked into the binary ───────────────────────────
if(READELF AND EXISTS "${READELF}")
    execute_process(COMMAND "${READELF}" -d "${WORKDIR}/${_runner_name}"
                    OUTPUT_VARIABLE _dyn ERROR_QUIET)
    if(NOT _dyn MATCHES "\\$ORIGIN")
        message(FATAL_ERROR
            "the portable runner has no \\$ORIGIN RUNPATH:\n${_dyn}")
    endif()
    # An absolute SystemC directory in RUNPATH means the bundle still resolves
    # against the build machine, which is exactly the failure this gate exists
    # to catch.
    if(_dyn MATCHES "R(UN)?PATH.*/(opt|usr)/[^\n]*systemc")
        message(FATAL_ERROR
            "the portable runner keeps an absolute SystemC path in its "
            "RUNPATH:\n${_dyn}")
    endif()
endif()

# ── 3. run it there, with every escape route closed ──────────────────────────
#
# LD_LIBRARY_PATH is cleared so the host's SystemC cannot stand in for a missing
# bundled one, and the working directory is the copy, so a relative reference
# back into the build tree fails rather than quietly succeeding.
execute_process(
    COMMAND ${CMAKE_COMMAND} -E env --unset=LD_LIBRARY_PATH
            "./${_runner_name}" "${_image_name}"
    WORKING_DIRECTORY "${WORKDIR}"
    RESULT_VARIABLE _result
    OUTPUT_VARIABLE _stdout
    ERROR_VARIABLE _stderr)

message(STATUS "portable run in ${WORKDIR}:\n${_stdout}${_stderr}")

if(NOT _result EQUAL 0)
    message(FATAL_ERROR
        "the portable runner failed outside the build tree (exit ${_result}).\n"
        "${_stdout}${_stderr}")
endif()
if(NOT _stdout MATCHES "rvv_runner: PASS")
    message(FATAL_ERROR "the portable runner did not report PASS")
endif()

# ── 4. and it really did resolve against the bundle ──────────────────────────
#
# Without this the run above could have passed by loading the host's SystemC
# through the default search path, which is the outcome the gate is meant to
# exclude.
find_program(_ldd ldd)
if(_ldd)
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E env --unset=LD_LIBRARY_PATH
                "${_ldd}" "./${_runner_name}"
        WORKING_DIRECTORY "${WORKDIR}"
        OUTPUT_VARIABLE _ldd_out ERROR_QUIET)
    if(_ldd_out MATCHES "not found")
        message(FATAL_ERROR "unresolved libraries in the bundle:\n${_ldd_out}")
    endif()
    string(REGEX MATCH "libsystemc[^\n]*" _sysc_line "${_ldd_out}")
    message(STATUS "systemc resolved as: ${_sysc_line}")
    if(_sysc_line AND NOT _sysc_line MATCHES "${WORKDIR}")
        message(FATAL_ERROR
            "the portable runner loaded SystemC from outside the bundle:\n"
            "  ${_sysc_line}\n"
            "It only appeared to work because this host has SystemC installed.")
    endif()
endif()

message(STATUS "portable executable gate: PASS")
