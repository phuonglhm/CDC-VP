# Run one vp_fu3_soc command line and check both its exit status and its output.
# PASS_REGULAR_EXPRESSION alone ignores the exit status, so a regression to the
# wrong exit code would still pass; this script fails on either mismatch.
#
#   cmake -DCMD=<exe> -DARGS=<a;b;...> -DEXPECT_RC=<n> [-DEXPECT_OUTPUT=<regex>]
#         [-DFORBID_OUTPUT=<regex>] [-DCHECK_FILE=<file> -DCHECK_HEX=<lower-case hex>] -P expect_exit.cmake
#
# CHECK_FILE (a file the run writes, removed first so a stale copy cannot pass)
# must then hold exactly the bytes CHECK_HEX.

if(NOT DEFINED CMD OR NOT DEFINED EXPECT_RC)
    message(FATAL_ERROR "expect_exit.cmake needs -DCMD=... and -DEXPECT_RC=...")
endif()

if(DEFINED CHECK_FILE)
    file(REMOVE "${CHECK_FILE}")
endif()

execute_process(COMMAND ${CMD} ${ARGS}
                RESULT_VARIABLE rc
                OUTPUT_VARIABLE out
                ERROR_VARIABLE out)

if(NOT rc STREQUAL EXPECT_RC)
    message(FATAL_ERROR "'${CMD} ${ARGS}' exited with '${rc}', expected ${EXPECT_RC}\n${out}")
endif()
if(DEFINED EXPECT_OUTPUT AND NOT out MATCHES "${EXPECT_OUTPUT}")
    message(FATAL_ERROR "'${CMD} ${ARGS}' output does not match '${EXPECT_OUTPUT}'\n${out}")
endif()
if(DEFINED FORBID_OUTPUT AND out MATCHES "${FORBID_OUTPUT}")
    message(FATAL_ERROR "'${CMD} ${ARGS}' output matches the forbidden '${FORBID_OUTPUT}'\n${out}")
endif()
if(DEFINED CHECK_FILE)
    if(NOT EXISTS "${CHECK_FILE}")
        message(FATAL_ERROR "'${CMD} ${ARGS}' did not write '${CHECK_FILE}'\n${out}")
    endif()
    file(READ "${CHECK_FILE}" content HEX)
    if(NOT content STREQUAL CHECK_HEX)
        message(FATAL_ERROR "'${CHECK_FILE}' holds '${content}', expected '${CHECK_HEX}'")
    endif()
endif()
message(STATUS "'${CMD} ${ARGS}': exit ${rc} as expected")
