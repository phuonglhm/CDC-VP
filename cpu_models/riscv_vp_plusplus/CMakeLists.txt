# cdc::cpu::riscv_vp_plusplus — cpu_base wrapper around the RISC-V VP++ RV32
# ISS, which provides both scalar and RVV 1.0 execution in one hart.
#
# The TPU_V3 runtime CPU backend (decision record D3). The compiled scope below
# is the audited minimum: vendored softfloat plus nine upstream translation
# units. Everything else upstream ships — Qt, VNC, gdb-mc and its `mpc`
# submodule, nlohmann/json, and every upstream platform — is deliberately
# excluded, and the audit shows none of it is needed. See
# components/TPU_V3/docs/TPU_V3_PHASE2_AUDIT.md §4.

# ── the pinned source: a base revision plus an approved patch series ─────────
#
# Effective source = BASE_REVISION + the patches in `patches/`, applied in
# filename order.
#
# The base is the newest upstream revision that builds against SystemC 2.3.4;
# master requires SystemC 3.0.1 (upstream commit 6f6ec679) and CDC-VP is built
# against 2.3.4 throughout.
#
# Two kinds of patch live in the series, and the distinction is deliberate:
#
#   *upstream backport*  — a commit that exists upstream, carried back onto the
#                          base pin. Identified by its upstream SHA, and it
#                          disappears the day the pin moves past it.
#   *downstream          — a fix with no upstream counterpart, written here.
#    conformance patch*    It does **not** disappear when the pin moves; it has
#                          to be re-checked against the new base, and ideally
#                          offered upstream.
#
# Calling both "backports" would hide exactly the difference that matters when
# someone later bumps the pin, so the series records which each one is.
#
# **This file verifies; it never patches.** `fetch_riscv_vp_plusplus.sh` is the
# only thing allowed to modify the checkout. A configure step that silently
# edited third-party source would make the built binary depend on when CMake
# last ran, which is precisely what the recorded-patch policy exists to stop.
set(RISCV_VP_PLUSPLUS_REVISION "7a36fe859cae242f513ca6ad16ab8238f1e82977"
    CACHE STRING
    "Base ics-jku/riscv-vp-plusplus revision (tag 2025.09); see TPU_V3_PHASE2_AUDIT.md §1")

# The series, as `file|kind|reference|sha256|why` records. The separator is `|`
# and not `;` because `;` is CMake's list separator and would split a record
# mid-field; for the same reason the reason text must stay semicolon-free. Cached so the
# platform's build manifest can read them back: plain variables do not escape
# this directory scope, and the manifest must record the exact source that was
# compiled.
set(RISCV_VP_PLUSPLUS_PATCH_SERIES
    "0001-b710fa7b-dbbcache-fixed-random-cycle-counting.patch|upstream-backport|b710fa7be2643b42cee92f5bbcb8cead4c0ed282|16758c959a534e71c23254599ac7229c0ff85d3b4e1dc67ee3c82cc96f23b8ad|F11: the ISS banked ~2.29 s of simulated time before its first instruction and mcycle was wrong from reset (D8)"
    "0002-d12-rv32-index-eew64-illegal.patch|downstream-conformance|audit F12|de147236d885b6a76584ad1ac83da6f4d6cab975a0464b40acdb9c619090a319|F12: RVV 1.0 18.2 forbids index EEW=64 when XLEN=32, so all 32 RV32 indexed encodings must raise an illegal instruction (D12)"
    "0003-d13-bus-error-is-an-access-fault.patch|downstream-conformance|audit F13|c3535b21b849581af340b755ad1d22c33647c88dc3b9659072b75fd7fefce84b|F13: a failed bus access reported a page fault on a core with satp.MODE=Bare, but it must be an access fault chosen by access origin (D13)"
    "0004-d19-cycle-baseline-survives-reset.patch|downstream-conformance|decision record D19|e1ce2d42f3fd30114b35240098a49e6df6ce091c5389d308d5fe1afe8a351054|D19: `init()` zeroed the cycle baseline while the dbbcache accumulator survived it, so the first commit after a hart reset re-added the whole pre-reset count into simulated time"
    CACHE STRING "Approved patch series applied on top of the base revision")

# The *result*, as `path|sha256` records: exactly which files the series is
# allowed to have changed, and exactly what each must contain afterwards.
#
# Reverse-applying each patch, which is all the earlier version did, proves that
# the recorded changes are present. It does not prove that *nothing else* is:
# an extra local edit elsewhere in the file, or in a file no patch touches,
# passes every reverse-apply check. Hashing the outcome closes that, and it does
# so read-only — the verification never has to modify the checkout to find out
# what is in it.
set(RISCV_VP_PLUSPLUS_PATCHED_FILES
    "vp/src/core/common/dbbcache.h|7674449c57ccd419e788e8bb8d5d7fe99b4b85426214b3c113c29af532f481d9"
    "vp/src/core/common/mem.h|98004612efcde708db7078f3ee522445ab871f7acf549061b741e716897bcb1a"
    "vp/src/core/rv32/iss_ctemplate.cpp|6f55dab250b8301fa61611d870ea5381548f8040b940f84f2b7e2fc04a94bc14"
    CACHE STRING "Files the patch series changes, with their post-patch content hashes")

set(RISCV_VP_PLUSPLUS_DIR "${CMAKE_SOURCE_DIR}/third_party/riscv-vp-plusplus"
    CACHE PATH "Path to the riscv-vp-plusplus checkout")
set(VPP "${RISCV_VP_PLUSPLUS_DIR}/vp/src")

set(_vpp_fetch_hint
    "Run:\n    ${CMAKE_CURRENT_SOURCE_DIR}/fetch_riscv_vp_plusplus.sh")

if(NOT EXISTS "${VPP}/core/rv32/iss.cpp")
    message(FATAL_ERROR
        "RISC-V VP++ not found at ${VPP}.\n${_vpp_fetch_hint}")
endif()

find_program(RISCV_VP_PLUSPLUS_GIT git)
if(RISCV_VP_PLUSPLUS_GIT AND EXISTS "${RISCV_VP_PLUSPLUS_DIR}/.git")
    execute_process(
        COMMAND "${RISCV_VP_PLUSPLUS_GIT}" -C "${RISCV_VP_PLUSPLUS_DIR}" rev-parse HEAD
        OUTPUT_VARIABLE _vpp_head
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET)
    if(NOT _vpp_head STREQUAL RISCV_VP_PLUSPLUS_REVISION)
        message(FATAL_ERROR
            "RISC-V VP++ checkout is at ${_vpp_head} but the audited base is "
            "${RISCV_VP_PLUSPLUS_REVISION}.\n${_vpp_fetch_hint}\n"
            "To change the pin instead, update RISCV_VP_PLUSPLUS_REVISION *and* "
            "re-run the Phase 2 audit — upstream master requires SystemC 3.0.1, "
            "and the downstream conformance patches must be re-checked against "
            "the new base rather than assumed to still be needed.")
    endif()
endif()

set(_vpp_series_summary "")
foreach(_vpp_record IN LISTS RISCV_VP_PLUSPLUS_PATCH_SERIES)
    string(REPLACE "|" ";" _vpp_fields "${_vpp_record}")
    list(GET _vpp_fields 0 _vpp_file)
    list(GET _vpp_fields 1 _vpp_kind)
    list(GET _vpp_fields 2 _vpp_ref)
    list(GET _vpp_fields 3 _vpp_sha)
    list(GET _vpp_fields 4 _vpp_why)
    set(_vpp_path "${CMAKE_CURRENT_SOURCE_DIR}/patches/${_vpp_file}")

    if(NOT EXISTS "${_vpp_path}")
        message(FATAL_ERROR "Approved patch missing: ${_vpp_path}\n${_vpp_fetch_hint}")
    endif()

    # Each patch is identified by content. A patch file edited in place would
    # otherwise pass every check here while changing what gets compiled.
    file(SHA256 "${_vpp_path}" _vpp_actual_sha)
    if(NOT _vpp_actual_sha STREQUAL _vpp_sha)
        message(FATAL_ERROR
            "Patch ${_vpp_file} has changed.\n"
            "  expected SHA256 ${_vpp_sha}\n"
            "  found            ${_vpp_actual_sha}\n"
            "If this is intentional, update RISCV_VP_PLUSPLUS_PATCH_SERIES and the "
            "build manifest, and re-run the Phase 2 gate.")
    endif()

    if(RISCV_VP_PLUSPLUS_GIT AND EXISTS "${RISCV_VP_PLUSPLUS_DIR}/.git")
        # Applied-ness is checked by reverse-applying: that succeeds only when
        # the tree already contains exactly this change. Grepping for the
        # changed lines would also pass on a tree edited by hand to look
        # similar.
        execute_process(
            COMMAND "${RISCV_VP_PLUSPLUS_GIT}" -C "${RISCV_VP_PLUSPLUS_DIR}"
                    apply --reverse --check "${_vpp_path}"
            RESULT_VARIABLE _vpp_applied
            OUTPUT_QUIET ERROR_QUIET)
        if(NOT _vpp_applied EQUAL 0)
            message(FATAL_ERROR
                "Approved patch ${_vpp_file} (${_vpp_kind}, ${_vpp_ref}) is not "
                "applied to ${RISCV_VP_PLUSPLUS_DIR}.\n"
                "  ${_vpp_why}\n"
                "${_vpp_fetch_hint}")
        endif()
    endif()

    string(APPEND _vpp_series_summary "\n    ${_vpp_kind}  ${_vpp_ref}")
endforeach()

# ── the result, not just the ingredients ─────────────────────────────────────
#
# Every file the series touches must contain exactly what the series produces,
# and no other file may be modified at all.
set(_vpp_expected_files "")
foreach(_vpp_record IN LISTS RISCV_VP_PLUSPLUS_PATCHED_FILES)
    string(REPLACE "|" ";" _vpp_fields "${_vpp_record}")
    list(GET _vpp_fields 0 _vpp_rel)
    list(GET _vpp_fields 1 _vpp_want)
    list(APPEND _vpp_expected_files "${_vpp_rel}")

    set(_vpp_abs "${RISCV_VP_PLUSPLUS_DIR}/${_vpp_rel}")
    if(NOT EXISTS "${_vpp_abs}")
        message(FATAL_ERROR "Patched file missing: ${_vpp_abs}\n${_vpp_fetch_hint}")
    endif()
    file(SHA256 "${_vpp_abs}" _vpp_got)
    if(NOT _vpp_got STREQUAL _vpp_want)
        message(FATAL_ERROR
            "RISC-V VP++ source does not match the approved patch series.\n"
            "  file             ${_vpp_rel}\n"
            "  expected SHA256  ${_vpp_want}\n"
            "  found            ${_vpp_got}\n"
            "The checkout has been edited, or the series changed without its "
            "recorded result being updated. Reset and re-run:\n${_vpp_fetch_hint}")
    endif()
endforeach()

if(RISCV_VP_PLUSPLUS_GIT AND EXISTS "${RISCV_VP_PLUSPLUS_DIR}/.git")
    # ...and nothing outside that set may be modified. `--untracked-files=no`
    # deliberately ignores stray new files: they cannot change what is compiled,
    # because the source list is explicit.
    execute_process(
        COMMAND "${RISCV_VP_PLUSPLUS_GIT}" -C "${RISCV_VP_PLUSPLUS_DIR}"
                status --porcelain --untracked-files=no
        OUTPUT_VARIABLE _vpp_status
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET)
    string(REPLACE "\n" ";" _vpp_status_lines "${_vpp_status}")
    foreach(_vpp_line IN LISTS _vpp_status_lines)
        string(STRIP "${_vpp_line}" _vpp_line)
        if(_vpp_line STREQUAL "")
            continue()
        endif()
        string(REGEX REPLACE "^[A-Z?! ]+[ \t]+" "" _vpp_changed "${_vpp_line}")
        if(NOT _vpp_changed IN_LIST _vpp_expected_files)
            message(FATAL_ERROR
                "RISC-V VP++ checkout has a modification the approved series "
                "does not account for:\n  ${_vpp_changed}\n"
                "Only these files may differ from the base revision:\n"
                "  ${_vpp_expected_files}\n${_vpp_fetch_hint}")
        endif()
    endforeach()

    message(STATUS
        "RISC-V VP++: base ${RISCV_VP_PLUSPLUS_REVISION}${_vpp_series_summary}")
endif()

# Boost is header-only for the pieces used here (boost/format and
# boost/multiprecision from the vector unit); the audit confirmed the linked
# binary has no libboost_* runtime dependency.
find_package(Boost 1.70 REQUIRED)

# Vendored Berkeley SoftFloat (Release 3d, BSD-3-Clause in the file headers).
#
# Built as our own target rather than through upstream's `add_subdirectory`,
# because that would create a target literally named `softfloat` and the Bremen
# `riscv_vp` backend already has one. They are **not** interchangeable: Bremen
# ships 218 sources with no BF16, VP++ ships 230 including `f32_to_bf16.c`,
# `bf16_to_f32.c` and `s_roundPackToBF16.c`. Reusing the wrong copy would
# silently change floating-point behaviour, and the BF16 primitives are exactly
# what the Phase 5 matrix-engine arithmetic contract (D6) reuses.
#
# A glob is right here, unusually: the directory is a vendored dependency at a
# pinned immutable revision, so the file set cannot change without the pin
# changing, and enumerating 230 filenames would only create a second thing to
# keep in sync.
file(GLOB RISCV_VP_PLUSPLUS_SOFTFLOAT_SRC "${VPP}/vendor/softfloat/*.c")
list(LENGTH RISCV_VP_PLUSPLUS_SOFTFLOAT_SRC _softfloat_count)
if(_softfloat_count LESS 200)
    message(FATAL_ERROR
        "Only ${_softfloat_count} softfloat sources found under "
        "${VPP}/vendor/softfloat; expected the full vendored set. Is the "
        "checkout complete?")
endif()

add_library(softfloat_vpp STATIC ${RISCV_VP_PLUSPLUS_SOFTFLOAT_SRC})
# Copied from upstream vp/src/vendor/softfloat/CMakeLists.txt. These select the
# arithmetic specialisation, so they are not decorative.
target_compile_definitions(softfloat_vpp PRIVATE
    SOFTFLOAT_ROUND_ODD
    INLINE_LEVEL=5
    SOFTFLOAT_FAST_DIV32TO16
    SOFTFLOAT_FAST_DIV64TO32
    SOFTFLOAT_FAST_INT64)
target_include_directories(softfloat_vpp
    PRIVATE
        ${VPP}/vendor/softfloat/include
        ${VPP}/vendor/softfloat/include/softfloat
    PUBLIC
        ${VPP}/vendor/softfloat/include)
target_compile_options(softfloat_vpp PRIVATE -w)

add_library(riscv_vp_plusplus STATIC
    src/riscv_vp_plusplus_wrapper.cpp
    ${VPP}/core/rv32/iss.cpp
    ${VPP}/core/rv32/syscall.cpp
    ${VPP}/core/common/timer.cpp
    ${VPP}/core/common/real_clint.cpp
    ${VPP}/core/common/instr.cpp
    ${VPP}/core/common/debug_memory.cpp
    ${VPP}/core/common/rawmode.cpp
    ${VPP}/core/common/iss_stats.cpp)
add_library(cdc::cpu::riscv_vp_plusplus ALIAS riscv_vp_plusplus)

target_include_directories(riscv_vp_plusplus
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/cpu_models/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE
        ${VPP}                      # for "core/..." and "util/..." includes
        ${VPP}/core/rv32
        ${VPP}/core/common
        ${VPP}/util
        ${Boost_INCLUDE_DIRS})

target_compile_definitions(riscv_vp_plusplus PRIVATE
    SC_INCLUDE_DYNAMIC_PROCESSES
    # Upstream uses SC_HAS_PROCESS, obsolete in IEEE 1666-2023 but retained for
    # compatibility with older SystemC. Matches upstream's own vp/CMakeLists.
    SC_ALLOW_DEPRECATED_IEEE_API)

# Third-party sources; their warnings are not ours to fix and would drown our
# own. The wrapper itself is compiled in the same target, so keep this narrow.
set_source_files_properties(
    ${VPP}/core/rv32/iss.cpp
    ${VPP}/core/rv32/syscall.cpp
    ${VPP}/core/common/timer.cpp
    ${VPP}/core/common/real_clint.cpp
    ${VPP}/core/common/instr.cpp
    ${VPP}/core/common/debug_memory.cpp
    ${VPP}/core/common/rawmode.cpp
    ${VPP}/core/common/iss_stats.cpp
    PROPERTIES COMPILE_OPTIONS "-w")

target_link_libraries(riscv_vp_plusplus
    PUBLIC  SystemC::systemc
    PRIVATE softfloat_vpp pthread)

# `AND CDC_BUILD_TPU_V3_SOC` keeps the previous meaning now that this directory
# is also reached from the Phase 4.5 compiler-enablement build. These are the
# TPU_V3 Phase 2 gates — the Spike oracle, the differential corpus, the D12/D13
# conformance images — and a compiler-handoff build has no business requiring
# them, or the cross toolchain and Spike checkout they need.
if(CDC_BUILD_TPU_V3_TESTS AND CDC_BUILD_TPU_V3_SOC)
    add_subdirectory(tests)
endif()
