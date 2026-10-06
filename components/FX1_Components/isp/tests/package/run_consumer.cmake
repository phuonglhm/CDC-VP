# Installs the build tree into a scratch prefix, then configures, builds and
# runs tests/package/consumer against it (CTest fx1_isp.package.consumer).
# -D BUILD_DIR=... -D SOURCE_DIR=... -D WORK_DIR=... -D CXX=... -D CC=...
file(REMOVE_RECURSE "${WORK_DIR}")
foreach(step
        "${CMAKE_COMMAND};--install;${BUILD_DIR};--prefix;${WORK_DIR}/prefix"
        "${CMAKE_COMMAND};-S;${SOURCE_DIR}/tests/package/consumer;-B;${WORK_DIR}/build;-DCMAKE_PREFIX_PATH=${WORK_DIR}/prefix;-DCMAKE_CXX_COMPILER=${CXX};-DCMAKE_C_COMPILER=${CC}"
        "${CMAKE_COMMAND};--build;${WORK_DIR}/build"
        "${WORK_DIR}/build/consumer")
    execute_process(COMMAND ${step} RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
    if(NOT rc EQUAL 0)
        message(FATAL_ERROR "step failed (${rc}): ${step}\n${out}\n${err}")
    endif()
endforeach()
message(STATUS "${out}")
# The handover kit works from the prefix alone: 4K bit-exact check, and the
# installed Python tools import their reference (when numpy is available).
execute_process(COMMAND sh ${WORK_DIR}/prefix/share/fx1_isp/samples/run_4k_check.sh ${WORK_DIR}/prefix ${WORK_DIR}/4k
                RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
    message(FATAL_ERROR "installed run_4k_check.sh failed:\n${out}\n${err}")
endif()
message(STATUS "${out}")
find_program(PY NAMES python3)
if(PY)
    execute_process(COMMAND ${PY} -c "import numpy" RESULT_VARIABLE has_numpy OUTPUT_QUIET ERROR_QUIET)
    if(has_numpy EQUAL 0)
        execute_process(COMMAND ${PY} ${WORK_DIR}/prefix/share/fx1_isp/tools/run_regression.py --help
                        RESULT_VARIABLE rc OUTPUT_QUIET ERROR_VARIABLE err)
        if(NOT rc EQUAL 0)
            message(FATAL_ERROR "installed run_regression.py does not start:\n${err}")
        endif()
        execute_process(COMMAND ${PY} -c "import sys; sys.path.insert(0, '${WORK_DIR}/prefix/share/fx1_isp/reference'); from fx1_isp_ref import regs; regs.Registers()"
                        RESULT_VARIABLE rc ERROR_VARIABLE err)
        if(NOT rc EQUAL 0)
            message(FATAL_ERROR "installed reference does not load its schema:\n${err}")
        endif()
    endif()
endif()
