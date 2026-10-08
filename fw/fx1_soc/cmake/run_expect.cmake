# Run the simulator and require a specific exit status and output pattern.
# Used for negative firmware tests whose correct outcome is a FAIL verdict.
#   cmake -DSIM=<fx1_soc> -DELF=<image> -DTIMEOUT_MS=<ms> -DEXPECT_RC=<n>
#         -DEXPECT_REGEX=<regex> [-DLOG=<uart log>] [-DARGS=<a;b;...>] -P run_expect.cmake
# Also used for positive tests that must print a specific line (RC 0).
foreach(var SIM ELF TIMEOUT_MS EXPECT_RC EXPECT_REGEX)
    if(NOT DEFINED ${var})
        message(FATAL_ERROR "run_expect.cmake: ${var} is required")
    endif()
endforeach()

set(_args --fw "${ELF}" --timeout-ms "${TIMEOUT_MS}")
if(DEFINED LOG)
    list(APPEND _args --uart-log "${LOG}")
endif()
if(DEFINED ARGS)
    list(APPEND _args ${ARGS})
endif()
execute_process(COMMAND "${SIM}" ${_args}
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
message("${_out}${_err}")
if(NOT _rc STREQUAL "${EXPECT_RC}")
    message(FATAL_ERROR "expected exit status ${EXPECT_RC}, got ${_rc}")
endif()
if(NOT "${_out}${_err}" MATCHES "${EXPECT_REGEX}")
    message(FATAL_ERROR "output does not match '${EXPECT_REGEX}'")
endif()
message("run_expect: exit ${_rc} and output match as expected")
