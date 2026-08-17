# SPDX-License-Identifier: Apache-2.0

foreach(_required BINARY NM)
    if(NOT DEFINED ${_required})
        message(FATAL_ERROR "static_state_gate.cmake requires -D${_required}=")
    endif()
endforeach()

execute_process(
    COMMAND "${NM}" "${BINARY}"
    RESULT_VARIABLE _nm_result
    OUTPUT_VARIABLE _symbols
    ERROR_VARIABLE _nm_error)
if(NOT _nm_result EQUAL 0)
    message(FATAL_ERROR "nm failed for ${BINARY}: ${_nm_error}")
endif()

# Itanium ABI: _ZZ<function>E<object> is a function-local static.  The kept
# Sauria templates previously emitted dozens of these even with
# SAURIA_DEBUG=0, including counters and null streams shared by both engines.
string(REGEX MATCHALL "[^\n]*_ZZN6sauria[^\n]*" _local_statics "${_symbols}")
if(_local_statics)
    list(JOIN _local_statics "\n  " _formatted)
    message(FATAL_ERROR
        "The compiled Sauria engine still contains function-local static "
        "state shared between instances:\n  ${_formatted}\n"
        "D17 requires SAURIA_DEBUG=0 and SAURIA_TRACE_FILES=0 to compile this "
        "state out, not merely redirect its output.")
endif()

message(STATUS "Sauria binary contains no function-local static state")
