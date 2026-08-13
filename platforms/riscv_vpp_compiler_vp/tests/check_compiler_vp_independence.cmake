# SPDX-License-Identifier: Apache-2.0
#
# The Phase 4.5 boundary gate.
#
# Phase 4.5's non-goals are a list of things that must not be in this handoff:
# no FlooNoC or other mesh, no Sauria, no ImageTransform, no NEO DMA, no core
# SRAM, no NEO fabric, no second hart, no Linux, no CLINT/PLIC/MMU/cache, no
# GUI/Qt/VNC platform, and Spike present only as a standalone oracle that is
# never linked. The packaging gate turns that into a requirement: "a
# source/link/manifest guard proves FlooNoC, Sauria, NEO DMA, Spike, GUI and
# full upstream VP++ platform code are absent from the binary and package."
#
# All of it is easy to state and easy to violate later with one convenient
# include. This is what makes it hold. It looks in four places, because each one
# misses what the others catch:
#
#   sources        a dependency arriving through a transitive header is
#                  invisible here, but a copied-in register map or a hand-rolled
#                  mesh is not;
#   symbols        what a transitive dependency actually looks like once the
#                  compiler is done with it;
#   link interface a target nobody needs is the one a later change quietly
#                  starts using, whether or not today's code touches it;
#   VP++ sources   the compiled scope of the upstream ISS. Excluding its Qt/VNC
#                  platforms is a choice made in one CMake list, and nothing
#                  else here would notice that list growing.
#
# The package half of the requirement is checked by `run_packaging_regression.sh`,
# which has the assembled bundle and the manifest to look at.
#
# Required: -DSOURCE_DIR= -DFIRMWARE_DIR= -DBINARY=
# Optional: -DNM= -DLINK_LIBRARIES= -DVPP_SOURCES=

foreach(_required SOURCE_DIR FIRMWARE_DIR BINARY)
    if(NOT DEFINED ${_required})
        message(FATAL_ERROR
            "check_compiler_vp_independence.cmake: -D${_required}= is required")
    endif()
endforeach()

set(_failures "")

# ── 1. the sources ────────────────────────────────────────────────────────────
#
# Comments are stripped first, because the comments have to be able to *name*
# what the code must not use — "no Sauria, no NEO DMA" is the clearest way to
# say it, and a guard that forbade saying so would push the reasoning out of the
# code and into a document nobody reads next to it.

file(GLOB_RECURSE _sources
    "${SOURCE_DIR}/src/*.h"
    "${SOURCE_DIR}/src/*.cpp"
    "${SOURCE_DIR}/include/*.h"
    "${FIRMWARE_DIR}/common/*.h"
    "${FIRMWARE_DIR}/common/*.c"
    "${FIRMWARE_DIR}/examples/*.c")

if(_sources STREQUAL "")
    message(FATAL_ERROR
        "check_compiler_vp_independence.cmake: no sources under ${SOURCE_DIR} "
        "or ${FIRMWARE_DIR}; the guard would pass vacuously")
endif()

# Identifiers that must not appear in code.
#
# Each is a token specific enough not to fire on ordinary code. `floo` is *not*
# in this list on purpose: it is a prefix of `floor`, and a guard that trips on
# a maths call is a guard someone deletes.
#
# The separator is `|`, and the reason text must contain no `;`. CMake's list
# separator is `;`, so a semicolon splits one record into two and the second
# half is then compiled as a regular expression -- which fails with
# "Unmatched parentheses" pointing at a sentence, several files away from the
# comma that caused it.
set(_forbidden_tokens
    "floo_noc|FlooNoC. A VP does not need a mesh to execute an ELF, and the mesh is a Phase 9 full-SoC concern"
    "floonoc|FlooNoC"
    "noc_interconnect|the NoC interconnect component"
    "sauria|the Sauria matrix engine"
    "neo_dma|the NEO DMA"
    "image_transform|the ImageTransform engine"
    "npu_tlm|the NPU model tree"
    "dma_tlm|the shared PL330-style DMA component"
    "riscv_isa_sim|Spike, which is a standalone oracle and is never linked (decision record D3)"
    "dmi_add|DMI, which would let fetches and loads bypass the bus and every counter"
    "set_dmi_ptr|DMI, which would let fetches and loads bypass the bus and every counter"
    "DirectCoreRunner|upstream's runner, which calls sc_stop() when its core terminates"
    "QApplication|Qt"
    "qt_gui|Qt"
)

# Headers that must not be included.
#
# Separate from the token list because the distinction matters. `--version`
# legitimately mentions TPU_V3 in prose, and the RAM base legitimately *is* the
# TPU_V3 global-RAM base; including `tpu_v3/address_map.h` to say so would make
# a compiler handoff depend on the SoC's component tree, which is the thing
# Phase 4.5 says it must build without.
set(_forbidden_includes
    "tpu_v3/|the TPU_V3 component tree. This platform must build with CDC_BUILD_TPU_V3_SOC=OFF"
    "floo|any FlooNoC header"
    "sauria|any Sauria header"
    "npu_tlm|the NPU model tree"
    "dma_tlm|the shared DMA component"
    "QtWidgets|Qt"
    "QtCore|Qt"
)

# A real newline, built rather than written: `[^\n]` in a CMake regex means
# "not backslash, not n", so the obvious spelling strips almost nothing, and
# `file(STRINGS)` splits at bytes it does not consider text — including the
# middle of a UTF-8 em dash, which leaves a comment fragment without its `//`
# and trips the guard on its own documentation.
string(ASCII 10 _newline)

foreach(_file ${_sources})
    file(READ "${_file}" _text)
    string(REGEX REPLACE "/\\*([^*]|\\*[^/])*\\*/" "" _code "${_text}")
    string(REGEX REPLACE "//[^${_newline}]*" "" _code "${_code}")
    string(TOLOWER "${_code}" _lower)

    # Includes are read *before* string literals are stripped, because an
    # include directive is a string literal.
    string(REGEX MATCHALL "#include[^${_newline}]*" _includes "${_lower}")

    # String literals go the same way as comments, and for the same reason.
    #
    # `--version` prints "accelerators: none. No Sauria, ImageTransform, NEO
    # DMA, core SRAM or NEO fabric is present." — which is one of the more
    # useful lines in the package, since it is the machine telling a user what
    # it is not. A guard that forbade saying so would force the banner to become
    # vague, and a vague banner is how somebody ends up assuming the accelerator
    # is in there somewhere.
    #
    # Nothing is lost. A dependency cannot live inside a string: it arrives as
    # an include, which was just extracted above, or as a symbol, which the `nm`
    # scan below catches. The pattern deliberately does not handle an escaped
    # quote inside a literal; no source here has one, and the alternative is a
    # backslash-escaping expression that is harder to read than the rule it
    # implements.
    string(REGEX REPLACE "\"[^\"]*\"" "" _prose_free "${_lower}")

    get_filename_component(_name "${_file}" NAME)
    foreach(_entry ${_forbidden_tokens})
        string(REGEX REPLACE "\\|.*" "" _token "${_entry}")
        string(REGEX REPLACE "^[^|]*\\|" "" _why "${_entry}")
        string(TOLOWER "${_token}" _needle)
        if(_prose_free MATCHES "${_needle}")
            list(APPEND _failures "${_name} refers to '${_token}' in code: ${_why}")
        endif()
    endforeach()

    foreach(_entry ${_forbidden_includes})
        string(REGEX REPLACE "\\|.*" "" _pattern "${_entry}")
        string(REGEX REPLACE "^[^|]*\\|" "" _why "${_entry}")
        string(TOLOWER "${_pattern}" _needle)
        foreach(_include ${_includes})
            if(_include MATCHES "${_needle}")
                list(APPEND _failures "${_name} has '${_include}': ${_why}")
            endif()
        endforeach()
    endforeach()
endforeach()

# ── 2. the built binary ───────────────────────────────────────────────────────

if(NOT EXISTS "${BINARY}")
    message(FATAL_ERROR
        "check_compiler_vp_independence.cmake: ${BINARY} was not built")
endif()

if(NM AND EXISTS "${NM}")
    execute_process(COMMAND "${NM}" --defined-only -C "${BINARY}"
                    OUTPUT_VARIABLE _defined ERROR_QUIET)
    execute_process(COMMAND "${NM}" -u -C "${BINARY}"
                    OUTPUT_VARIABLE _undefined ERROR_QUIET)
    string(TOLOWER "${_defined}${_undefined}" _symbols)

    # Deliberately specific spellings. A bare `noc` or `floo` would fire on
    # ordinary library symbols, and the first false positive is what turns a
    # guard into a nuisance somebody weakens.
    foreach(_needle
            floo_noc floonoc noc_interconnect noc_router
            sauria neo_dma image_transform
            dma_tlm pl330 npu_tlm
            riscv_isa_sim htif_t fesvr
            qapplication qwidget vncserver rfbscreen
            directcorerunner gdbserver)
        if(_symbols MATCHES "${_needle}")
            list(APPEND _failures
                "the binary references a symbol containing '${_needle}'")
        endif()
    endforeach()
else()
    message(STATUS
        "check_compiler_vp_independence: no usable nm, symbol check skipped "
        "(the source scan still ran)")
endif()

# ── 3. the CMake link interface ───────────────────────────────────────────────

if(DEFINED LINK_LIBRARIES)
    string(TOLOWER "${LINK_LIBRARIES}" _links)
    foreach(_needle tpu_v3 sauria noc_interconnect dma_tlm npu_tlm floo)
        if(_links MATCHES "${_needle}")
            list(APPEND _failures
                "riscv_vpp_compiler_vp links a target matching '${_needle}': "
                "${LINK_LIBRARIES}")
        endif()
    endforeach()
endif()

# ── 4. the compiled scope of the upstream ISS ─────────────────────────────────
#
# The audited minimum is vendored softfloat plus nine upstream translation
# units, all under `core/`. Everything else upstream ships — every platform,
# Qt, VNC, gdb-mc — is excluded by a list in
# `cpu_models/riscv_vp_plusplus/CMakeLists.txt`, and nothing above would notice
# that list growing a platform file, because the resulting symbols would look
# like ordinary VP++ symbols.

if(DEFINED VPP_SOURCES)
    string(TOLOWER "${VPP_SOURCES}" _vpp_lower)
    string(REPLACE ";" "\n" _vpp_lines "${_vpp_lower}")
    string(REPLACE "\n" ";" _vpp_list "${_vpp_lines}")
    foreach(_source IN LISTS _vpp_list)
        if(_source STREQUAL "")
            continue()
        endif()
        foreach(_needle "/platform/" "/gdb-mc/" "/vendor/mpc" "qt" "vnc")
            if(_source MATCHES "${_needle}")
                list(APPEND _failures
                    "the VP++ library compiles '${_source}', which matches "
                    "'${_needle}': the audited scope is vendored softfloat plus "
                    "the core/ translation units only")
            endif()
        endforeach()
    endforeach()
endif()

# ── verdict ───────────────────────────────────────────────────────────────────

if(_failures)
    foreach(_failure ${_failures})
        message(SEND_ERROR "FAIL: ${_failure}")
    endforeach()
    message(FATAL_ERROR
        "riscv_vpp_compiler_vp is not the platform Phase 4.5 specifies. It is "
        "one RV32GCV hart, a TLM address decoder, program/data RAM and a "
        "simulator-only host-I/O target -- and nothing else.")
endif()

message(STATUS "riscv_vpp_compiler_vp independence: PASS")
