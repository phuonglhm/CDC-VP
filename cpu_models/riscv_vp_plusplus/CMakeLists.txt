# cdc::cpu::riscv_vp_plusplus — cpu_base wrapper around the RISC-V VP++ RV32
# ISS, which provides both scalar and RVV 1.0 execution in one hart.
#
# The TPU_V3 runtime CPU backend (decision record D3). The compiled scope below
# is the audited minimum: vendored softfloat plus nine upstream translation
# units. Everything else upstream ships — Qt, VNC, gdb-mc and its `mpc`
# submodule, nlohmann/json, and every upstream platform — is deliberately
# excluded, and the audit shows none of it is needed. See
# components/TPU_V3/docs/TPU_V3_PHASE2_AUDIT.md §4.

# ── the pinned source: base revision plus one approved backport ──────────────
#
# Effective source = BASE_REVISION + BACKPORT_REVISION.
#
# The base is the newest upstream revision that builds against SystemC 2.3.4;
# master requires SystemC 3.0.1 (upstream commit 6f6ec679) and CDC-VP is built
# against 2.3.4 throughout. The backport fixes audit finding F11, where the ISS
# charged ~2.29 s of bogus simulated time before its first instruction and left
# `mcycle` wrong from reset. It touches two lines of `dbbcache.h` and uses no
# SystemC API, so it can be carried without the rest of master.
#
# **This file verifies; it never patches.** `fetch_riscv_vp_plusplus.sh` is the
# only thing allowed to modify the checkout. A configure step that silently
# edited third-party source would make the built binary depend on when CMake
# last ran, which is precisely what the recorded-patch policy exists to stop.
set(RISCV_VP_PLUSPLUS_REVISION "7a36fe859cae242f513ca6ad16ab8238f1e82977"
    CACHE STRING
    "Base ics-jku/riscv-vp-plusplus revision (tag 2025.09); see TPU_V3_PHASE2_AUDIT.md §1")
set(RISCV_VP_PLUSPLUS_BACKPORT "b710fa7be2643b42cee92f5bbcb8cead4c0ed282"
    CACHE STRING
    "Approved F11 backport commit applied on top of the base revision")
# Cached so the platform's build manifest can read them: plain variables do not
# escape this directory scope, and the manifest must record the exact patch it
# was built with.
set(RISCV_VP_PLUSPLUS_PATCH_NAME
    "0001-b710fa7b-dbbcache-fixed-random-cycle-counting.patch"
    CACHE STRING "Filename of the approved F11 backport patch")
set(RISCV_VP_PLUSPLUS_PATCH_SHA256
    "16758c959a534e71c23254599ac7229c0ff85d3b4e1dc67ee3c82cc96f23b8ad"
    CACHE STRING "Content hash of the approved F11 backport patch")

set(RISCV_VP_PLUSPLUS_DIR "${CMAKE_SOURCE_DIR}/third_party/riscv-vp-plusplus"
    CACHE PATH "Path to the riscv-vp-plusplus checkout")
set(VPP "${RISCV_VP_PLUSPLUS_DIR}/vp/src")
set(RISCV_VP_PLUSPLUS_PATCH
    "${CMAKE_CURRENT_SOURCE_DIR}/patches/${RISCV_VP_PLUSPLUS_PATCH_NAME}")

set(_vpp_fetch_hint
    "Run:\n    ${CMAKE_CURRENT_SOURCE_DIR}/fetch_riscv_vp_plusplus.sh")

if(NOT EXISTS "${VPP}/core/rv32/iss.cpp")
    message(FATAL_ERROR
        "RISC-V VP++ not found at ${VPP}.\n${_vpp_fetch_hint}")
endif()

# The patch is identified by content. A patch file edited in place would
# otherwise pass every check here while changing what gets compiled.
file(SHA256 "${RISCV_VP_PLUSPLUS_PATCH}" _vpp_patch_sha)
if(NOT _vpp_patch_sha STREQUAL RISCV_VP_PLUSPLUS_PATCH_SHA256)
    message(FATAL_ERROR
        "The F11 backport patch has changed.\n"
        "  expected SHA256 ${RISCV_VP_PLUSPLUS_PATCH_SHA256}\n"
        "  found            ${_vpp_patch_sha}\n"
        "If this is intentional, update RISCV_VP_PLUSPLUS_PATCH_SHA256 and the "
        "build manifest, and re-run the Phase 2 gate.")
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
            "re-run the Phase 2 audit — upstream master requires SystemC 3.0.1.")
    endif()

    # Applied-ness is checked by reverse-applying the patch: it succeeds only
    # when the tree already contains exactly this change. Grepping for the
    # changed lines would also pass on a tree that had been edited by hand to
    # look similar.
    execute_process(
        COMMAND "${RISCV_VP_PLUSPLUS_GIT}" -C "${RISCV_VP_PLUSPLUS_DIR}"
                apply --reverse --check "${RISCV_VP_PLUSPLUS_PATCH}"
        RESULT_VARIABLE _vpp_backport_applied
        OUTPUT_QUIET ERROR_QUIET)
    if(NOT _vpp_backport_applied EQUAL 0)
        message(FATAL_ERROR
            "The approved F11 backport (${RISCV_VP_PLUSPLUS_BACKPORT}) is not "
            "applied to ${RISCV_VP_PLUSPLUS_DIR}.\n"
            "Without it the ISS banks ~2.29 s of simulated time before its "
            "first instruction and `mcycle` is wrong from reset.\n"
            "${_vpp_fetch_hint}")
    endif()

    message(STATUS
        "RISC-V VP++: base ${RISCV_VP_PLUSPLUS_REVISION} "
        "+ F11 backport ${RISCV_VP_PLUSPLUS_BACKPORT}")
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
# what the Phase 4 MXU arithmetic contract (D6) reuses.
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

if(CDC_BUILD_TPU_V3_TESTS)
    add_subdirectory(tests)
endif()
