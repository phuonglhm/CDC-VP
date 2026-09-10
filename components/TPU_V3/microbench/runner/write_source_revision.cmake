# SPDX-License-Identifier: Apache-2.0
#
# Writes the current source revision to a file, for §8.1's reproducibility
# field. Run on every build rather than at configure time: a revision captured
# when the tree was configured names the tree that was configured, not the one
# that produced the binary under test, and the two diverge the moment anyone
# commits without reconfiguring.
#
# The dirty marker is not decoration. A benchmark row produced from a modified
# working tree is not reproducible from the commit it names, and saying so in
# the row is the difference between an honest identifier and a misleading one.

set(_revision "unavailable-no-git")

find_program(_git git)
if(_git)
    execute_process(
        COMMAND "${_git}" -C "${SOURCE_DIR}" rev-parse --short=12 HEAD
        OUTPUT_VARIABLE _head
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
        RESULT_VARIABLE _head_status)
    if(_head_status EQUAL 0 AND NOT _head STREQUAL "")
        set(_revision "${_head}")
        execute_process(
            COMMAND "${_git}" -C "${SOURCE_DIR}" status --porcelain
            OUTPUT_VARIABLE _porcelain
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
            RESULT_VARIABLE _status_status)
        if(NOT _status_status EQUAL 0)
            set(_revision "${_revision}-dirty-unknown")
        elseif(NOT _porcelain STREQUAL "")
            set(_revision "${_revision}-dirty")
        endif()
    endif()
endif()

set(_existing "")
if(EXISTS "${OUTPUT_FILE}")
    file(READ "${OUTPUT_FILE}" _existing)
    string(STRIP "${_existing}" _existing)
endif()
# Only rewrite on change, so an unchanged revision does not retrigger whatever
# depends on the file.
if(NOT _existing STREQUAL "${_revision}")
    file(WRITE "${OUTPUT_FILE}" "${_revision}\n")
endif()
