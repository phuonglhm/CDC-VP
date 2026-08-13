# SPDX-License-Identifier: Apache-2.0
#
# Extracts the Berkeley SoftFloat BSD-3-Clause notice from a source file that
# was actually compiled into the binary, and writes it into the package.
#
# ## Why it is extracted rather than written down
#
# SoftFloat Release 3d ships no `LICENSE` or `COPYING` file. Its notice lives in
# the header comment of each of its 230 source files and nowhere else, so a
# package that must carry the notice has three options: transcribe it here,
# check in a copy, or take it from the code.
#
# The first two are the same mistake at different distances. A transcription is
# a second copy of a legal text that no build step compares against the first,
# so it can be edited, truncated by a stray merge, or left behind when the
# vendored release is bumped — and the failure is silent, because a licence file
# that exists is a licence file nobody re-reads. Taking it from a file the
# compiler actually consumed means the shipped notice cannot describe a
# different version of the code than the one in the binary.
#
# ## Why it fails loudly
#
# The alternative to failing is shipping an empty or truncated notice, which is
# strictly worse than shipping none: it looks like the obligation was met. If
# upstream reorganises its headers, this stops the package and says so.
#
# Required: -DSOFTFLOAT_SOURCE= -DOUTPUT=

foreach(_required SOFTFLOAT_SOURCE OUTPUT)
    if(NOT DEFINED ${_required})
        message(FATAL_ERROR
            "write_softfloat_notice.cmake: -D${_required}= is required")
    endif()
endforeach()

if(NOT EXISTS "${SOFTFLOAT_SOURCE}")
    message(FATAL_ERROR
        "write_softfloat_notice.cmake: ${SOFTFLOAT_SOURCE} does not exist. The "
        "package must carry the Berkeley SoftFloat notice, because SoftFloat is "
        "statically linked into the binary and BSD-3-Clause requires the notice "
        "to accompany a binary distribution.")
endif()

file(READ "${SOFTFLOAT_SOURCE}" _text)

# The leading `/*= ... =*/` banner. Matched non-greedily on the first block so a
# later comment in the file cannot extend the capture.
string(REGEX MATCH "/\\*=+[^\\*]*(\\*[^/=][^\\*]*)*=*\\*/" _notice "${_text}")

if(_notice STREQUAL "")
    # Fall back to a plain leading block comment before giving up, so a cosmetic
    # change to the banner style is not treated as a missing licence.
    string(REGEX MATCH "/\\*([^*]|\\*[^/])*\\*/" _notice "${_text}")
endif()

# Whatever was captured has to actually be the licence. Without these two
# checks, a comment that happened to match the shape above would be shipped as
# the notice.
if(NOT _notice MATCHES "SoftFloat")
    message(FATAL_ERROR
        "write_softfloat_notice.cmake: the leading comment of "
        "${SOFTFLOAT_SOURCE} does not mention SoftFloat. The vendored release "
        "has been reorganised; find where the BSD-3-Clause notice now lives and "
        "update this script.")
endif()
if(NOT _notice MATCHES "Redistribution and use")
    message(FATAL_ERROR
        "write_softfloat_notice.cmake: the leading comment of "
        "${SOFTFLOAT_SOURCE} is not a redistribution notice.")
endif()

get_filename_component(_source_name "${SOFTFLOAT_SOURCE}" NAME)

file(WRITE "${OUTPUT}"
"Berkeley SoftFloat, Release 3d -- BSD-3-Clause
================================================================================

Berkeley SoftFloat provides the scalar and vector floating-point arithmetic in
this package. It is vendored inside the RISC-V VP++ source tree and is compiled
directly into the riscv_vpp_compiler_vp executable.

Upstream ships no standalone licence file: the notice below is the header of
${_source_name}, one of the source files compiled into this binary, reproduced
verbatim. Taking it from the code rather than from a transcription is what makes
it certain that this notice covers the version that was built.

Upstream: http://www.jhauser.us/arithmetic/SoftFloat.html

================================================================================

${_notice}
")

message(STATUS
    "riscv_vpp_compiler_vp: wrote the Berkeley SoftFloat notice from "
    "${_source_name}")
