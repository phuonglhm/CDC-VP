# Run a host command and require an exit status and an output pattern (both
# outputs, merged). Used where the correct outcome is a FAIL verdict, so that
# a crash or a missing input cannot pass as "the check failed".
#   cmake "-DCMD=<prog;arg;...>" -DRC=<n> "-DREGEX=<regex>" -P expect_rc.cmake
foreach(var CMD RC REGEX)
    if(NOT DEFINED ${var})
        message(FATAL_ERROR "expect_rc.cmake: ${var} is required")
    endif()
endforeach()
# ctest hands the list over with its separators escaped ("\;").
string(REPLACE "\\;" ";" _cmd "${CMD}")
execute_process(COMMAND ${_cmd} RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
message("${_out}${_err}")
if(NOT _rc STREQUAL "${RC}")
    message(FATAL_ERROR "expected exit status ${RC}, got ${_rc}")
endif()
if(NOT "${_out}${_err}" MATCHES "${REGEX}")
    message(FATAL_ERROR "output does not match '${REGEX}'")
endif()
message("expect_rc: exit ${_rc} and output match as expected")
