# SPDX-License-Identifier: Apache-2.0
#
# Runs the two-instance program in an empty directory and requires that the
# Sauria source wrote nothing to the filesystem.
#
# The `trace_sysc/` directory is created *before* the run, and the check is that
# it is still empty afterwards.
#
# That is not the obvious design, and the obvious one is broken. Checking
# "no `trace_sysc/` directory appeared" cannot fail: `std::ofstream` does not
# create directories, so on a machine without one the source's writers fail to
# open, write nothing, and report nothing. The first version of this gate did
# exactly that and passed against the *unpatched* source — a gate that could not
# distinguish the thing it existed to distinguish.
#
# Pre-creating the directory gives the unpatched source everything it needs to
# succeed. If the writers are still compiled in, files appear; if the patch did
# its job, the directory is untouched.

foreach(_required BINARY WORKDIR)
    if(NOT DEFINED ${_required})
        message(FATAL_ERROR "run_hygiene_gate.cmake: -D${_required}= is required")
    endif()
endforeach()

file(REMOVE_RECURSE "${WORKDIR}")
file(MAKE_DIRECTORY "${WORKDIR}")
file(MAKE_DIRECTORY "${WORKDIR}/trace_sysc")

execute_process(
    COMMAND "${BINARY}"
    WORKING_DIRECTORY "${WORKDIR}"
    RESULT_VARIABLE _result
    OUTPUT_VARIABLE _output
    ERROR_VARIABLE _output)
message(STATUS "${_output}")

if(NOT _result EQUAL 0)
    message(FATAL_ERROR
        "the two-instance Sauria program failed with status ${_result}")
endif()

file(GLOB_RECURSE _traces "${WORKDIR}/trace_sysc/*")
if(NOT _traces STREQUAL "")
    message(FATAL_ERROR
        "the Sauria source wrote trace files into ${WORKDIR}/trace_sysc\n"
        "  files: ${_traces}\n"
        "Decision record D17 requires the source's static trace writers to be "
        "compiled out, because a function-local static ofstream is shared "
        "between instances and INTERFACE_CONTRACT.md forbids mutable state "
        "shared between accelerator instances. Either the hygiene patch is not "
        "applied, or SAURIA_TRACE_FILES is not 0.")
endif()

file(GLOB _stray "${WORKDIR}/*")
list(REMOVE_ITEM _stray "${WORKDIR}/trace_sysc")
if(NOT _stray STREQUAL "")
    message(FATAL_ERROR
        "the two-instance run left files behind: ${_stray}")
endif()

message(STATUS
    "sauria two-instance hygiene: PASS (trace_sysc/ was present and writable, "
    "and stayed empty)")
