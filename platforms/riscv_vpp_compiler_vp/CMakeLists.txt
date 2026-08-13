# SPDX-License-Identifier: Apache-2.0
#
# `riscv_vpp_compiler_vp` — the RISC-V VP++ Compiler Enablement VP (plan §16,
# Phase 4.5).
#
# The link list is the architectural boundary, not an incidental detail. This
# executable links the VP++ hart, SystemC, and nothing else: no NoC, no Sauria,
# no NEO DMA, no core SRAM, no `tpu_v3_common`. Phase 4.5 says the handoff must
# build without `CDC_BUILD_TPU_V3_SOC`, and a dependency nobody needs is the one
# a later change quietly starts using — which is how a compiler package acquires
# a licence obligation and an accuracy claim it was never meant to carry.
#
# The CPU model is *reused*, never forked: `cdc::cpu::riscv_vp_plusplus` is the
# same target Phase 7 instantiates once per NEO-CORE.

# The firmware sources, and the directory they are *built* in.
#
# They are not the same directory, and that is the point. Building the examples
# in `fw/` — which is what the Phase 2 tree does, because its images are
# committed artifacts — breaks down here: a Release and a Debug build of this
# platform would write the same absolute paths, clobber each other's ELFs and
# each other's generated `link.ld`, leave the source tree dirty with ignored
# artifacts, and make a fresh packaging build silently reuse a stale image whose
# timestamp happened to look current. It also makes the build fail outright on a
# read-only checkout. So the tree is copied into the build directory and built
# there, once per build type.
set(COMPILER_VP_FW_SOURCE "${CMAKE_SOURCE_DIR}/fw/riscv_vpp_compiler_vp")
set(COMPILER_VP_FW_BUILD "${CMAKE_CURRENT_BINARY_DIR}/firmware")
set(COMPILER_VP_MAP_INCLUDE "${CMAKE_CURRENT_SOURCE_DIR}/include")

# ── what the binary is allowed to say about itself ───────────────────────────

execute_process(
    COMMAND git -C "${CMAKE_SOURCE_DIR}" rev-parse HEAD
    OUTPUT_VARIABLE COMPILER_VP_GIT_REVISION
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET)
if(NOT COMPILER_VP_GIT_REVISION)
    set(COMPILER_VP_GIT_REVISION "unknown")
endif()

set(COMPILER_VP_ISA_STRING "rv32gcv_zvl512b")
set(COMPILER_VP_ABI_STRING "ilp32d")
set(COMPILER_VP_BUILD_TYPE
    "$<IF:$<BOOL:${CMAKE_BUILD_TYPE}>,${CMAKE_BUILD_TYPE},unspecified>")
# `configure_file` does not expand generator expressions, so the build type is
# resolved here rather than left to `$<IF:...>`.
if(CMAKE_BUILD_TYPE)
    set(COMPILER_VP_BUILD_TYPE "${CMAKE_BUILD_TYPE}")
else()
    set(COMPILER_VP_BUILD_TYPE "unspecified")
endif()
set(COMPILER_VP_CXX_COMPILER
    "${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")

# One `"kind  reference  sha256",` line per approved VP++ patch. Emitted into a
# generated header rather than passed as a `-D`: the records contain `;`, which
# is CMake's list separator, so a definition would split each one mid-field and
# the binary would report a mangled provenance.
set(COMPILER_VP_PATCH_LINES "")
foreach(_record IN LISTS RISCV_VP_PLUSPLUS_PATCH_SERIES)
    string(REPLACE "|" ";" _fields "${_record}")
    list(LENGTH _fields _field_count)
    if(NOT _field_count EQUAL 5)
        message(FATAL_ERROR
            "riscv_vpp_compiler_vp: malformed RISCV_VP_PLUSPLUS_PATCH_SERIES "
            "record '${_record}'")
    endif()
    list(GET _fields 1 _kind)
    list(GET _fields 2 _reference)
    list(GET _fields 3 _sha)
    string(APPEND COMPILER_VP_PATCH_LINES
        "    \"${_kind}  ${_reference}  ${_sha}\",\n")
endforeach()

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/build_info.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/build_info.h"
    @ONLY)

# ── the executable ───────────────────────────────────────────────────────────

add_executable(riscv_vpp_compiler_vp
    src/main.cpp
    src/cli.cpp
    src/elf_image.cpp
    src/platform_top.cpp)

target_include_directories(riscv_vpp_compiler_vp PRIVATE
    src
    "${COMPILER_VP_MAP_INCLUDE}"
    "${CMAKE_CURRENT_BINARY_DIR}/generated")

target_link_libraries(riscv_vpp_compiler_vp PRIVATE
    cdc::cpu::riscv_vp_plusplus
    SystemC::systemc)

target_compile_features(riscv_vpp_compiler_vp PRIVATE cxx_std_17)

add_custom_command(TARGET riscv_vpp_compiler_vp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/configs"
            "$<TARGET_FILE_DIR:riscv_vpp_compiler_vp>/configs"
    VERBATIM)

cdc_make_portable(riscv_vpp_compiler_vp)
cdc_package_platform(riscv_vpp_compiler_vp)

# ── the demonstration images ─────────────────────────────────────────────────
#
# `verify` runs as part of the build rather than as a test, because its checks
# are about the *artifact*: an image that silently fell back to a scalar -march,
# or a scalar example that quietly contains vector instructions, must not exist
# long enough to be packaged.

set(COMPILER_VP_TOOLDIR "/opt/toolchains/xpack-riscv-none-elf-gcc-15.2.0-1/bin"
    CACHE PATH "Directory holding the riscv-none-elf cross toolchain")

set(COMPILER_VP_SCALAR_ELF
    "${COMPILER_VP_FW_BUILD}/examples/scalar_hello/scalar_hello.elf")
set(COMPILER_VP_VECTOR_ELF
    "${COMPILER_VP_FW_BUILD}/examples/rvv_vector_add/rvv_vector_add.elf")

# The executable does **not** depend on the firmware. Someone without a RISC-V
# cross toolchain can still build and hack on the simulator, which is a
# reasonable thing to want. The *package* is a different matter: Phase 4.5 makes
# both demonstrations mandatory package content, so a bundle without them is not
# a smaller package, it is a broken one. That is enforced below by
# `check_package_contents.cmake` rather than by this `if`.
set(COMPILER_VP_FIRMWARE_INCLUDED FALSE)
if(EXISTS "${COMPILER_VP_TOOLDIR}/riscv-none-elf-gcc")
    set(COMPILER_VP_FIRMWARE_INCLUDED TRUE)
    add_custom_command(
        OUTPUT "${COMPILER_VP_SCALAR_ELF}" "${COMPILER_VP_VECTOR_ELF}"
               "${COMPILER_VP_SCALAR_ELF}.dis" "${COMPILER_VP_VECTOR_ELF}.dis"
               "${COMPILER_VP_FW_BUILD}/common/link.ld"
        # Copied fresh each time rather than incrementally: `make` here is
        # driven by CMake's dependency list above, and a stale object left in
        # the copy from a previous configuration would be invisible to both.
        COMMAND ${CMAKE_COMMAND} -E rm -rf "${COMPILER_VP_FW_BUILD}"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${COMPILER_VP_FW_SOURCE}" "${COMPILER_VP_FW_BUILD}"
        COMMAND ${CMAKE_COMMAND} -E env PATH=/usr/bin:/bin:$ENV{PATH}
                make -C "${COMPILER_VP_FW_BUILD}"
                     TOOLDIR=${COMPILER_VP_TOOLDIR}
                     MAP_INCLUDE=${COMPILER_VP_MAP_INCLUDE}
                     all verify
        DEPENDS "${COMPILER_VP_FW_SOURCE}/Makefile"
                "${COMPILER_VP_FW_SOURCE}/common/crt0.S"
                "${COMPILER_VP_FW_SOURCE}/common/host_io.c"
                "${COMPILER_VP_FW_SOURCE}/common/host_io.h"
                "${COMPILER_VP_FW_SOURCE}/common/link.ld.in"
                "${COMPILER_VP_FW_SOURCE}/examples/scalar_hello/main.c"
                "${COMPILER_VP_FW_SOURCE}/examples/rvv_vector_add/main.c"
                "${COMPILER_VP_MAP_INCLUDE}/compiler_vp/host_io_map.h"
        COMMENT "Building the riscv_vpp_compiler_vp demonstration images"
        VERBATIM)
    # `ALL`, so an ordinary build produces them and the CLI gate has something
    # to run, without making the executable itself need the toolchain.
    add_custom_target(riscv_vpp_compiler_vp_firmware ALL
        DEPENDS "${COMPILER_VP_SCALAR_ELF}" "${COMPILER_VP_VECTOR_ELF}")
else()
    message(WARNING
        "RV32GCV cross toolchain not found at ${COMPILER_VP_TOOLDIR}. "
        "riscv_vpp_compiler_vp will build, but the two required demonstrations "
        "cannot, so `riscv_vpp_compiler_vp_package` will refuse to produce a "
        "package and the packaging gate will fail. Set COMPILER_VP_TOOLDIR.")
endif()

# ── package contents beyond the base bundle ──────────────────────────────────
#
# `cdc_package_platform` copies the binary, the SystemC runtime and configs. A
# distributable handoff also needs the two demonstrations, the SDK that built
# them, the map documentation, the licences of everything statically linked, and
# a record of what it was built from.

set(COMPILER_VP_PACKAGE_DIR "${CDC_PACKAGE_ROOT}/riscv_vpp_compiler_vp")

# Everything statically linked into the binary, with its licence.
#
# Shipping only CDC-VP's own Apache-2.0 was a redistribution defect, not a
# documentation one: the executable statically links the RISC-V VP++ ISS (MIT)
# and Berkeley SoftFloat (BSD-3-Clause), and both licences require the notice to
# accompany a binary distribution.
#
# SoftFloat has no standalone licence file upstream — the notice lives only in
# the header of each of its 230 source files — so it is extracted from one of
# the files that was actually compiled, rather than transcribed here where it
# could drift from the code it covers.
add_custom_command(TARGET riscv_vpp_compiler_vp_package POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
            "${COMPILER_VP_PACKAGE_DIR}/licenses"
    COMMAND ${CMAKE_COMMAND} -E copy
            "${CMAKE_SOURCE_DIR}/LICENSE"
            "${COMPILER_VP_PACKAGE_DIR}/licenses/Apache-2.0.txt"
    COMMAND ${CMAKE_COMMAND} -E copy
            "${CMAKE_SOURCE_DIR}/NOTICE"
            "${COMPILER_VP_PACKAGE_DIR}/licenses/CDC-VP-NOTICE.txt"
    COMMAND ${CMAKE_COMMAND} -E copy
            "${CMAKE_SOURCE_DIR}/THIRD_PARTY.md"
            "${COMPILER_VP_PACKAGE_DIR}/licenses/THIRD_PARTY.md"
    COMMAND ${CMAKE_COMMAND} -E copy
            "${RISCV_VP_PLUSPLUS_DIR}/LICENSE"
            "${COMPILER_VP_PACKAGE_DIR}/licenses/RISCV-VP-PLUSPLUS.MIT.txt"
    COMMAND ${CMAKE_COMMAND}
            -DSOFTFLOAT_SOURCE=${RISCV_VP_PLUSPLUS_DIR}/vp/src/vendor/softfloat/f32_add.c
            -DOUTPUT=${COMPILER_VP_PACKAGE_DIR}/licenses/BERKELEY-SOFTFLOAT-3d.BSD-3-Clause.txt
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/write_softfloat_notice.cmake"
    COMMAND ${CMAKE_COMMAND} -E copy
            "${CMAKE_CURRENT_SOURCE_DIR}/PACKAGE_README.md"
            "${COMPILER_VP_PACKAGE_DIR}/README.md"
    COMMAND ${CMAKE_COMMAND} -E make_directory
            "${COMPILER_VP_PACKAGE_DIR}/docs"
    COMMAND ${CMAKE_COMMAND} -E copy
            "${CMAKE_CURRENT_SOURCE_DIR}/docs/MEMORY_MAP.md"
            "${COMPILER_VP_PACKAGE_DIR}/docs/MEMORY_MAP.md"
    COMMAND ${CMAKE_COMMAND} -E copy
            "${CMAKE_CURRENT_SOURCE_DIR}/docs/COMPILER_QUICKSTART.md"
            "${COMPILER_VP_PACKAGE_DIR}/docs/COMPILER_QUICKSTART.md"
    COMMAND ${CMAKE_COMMAND} -E copy
            "${CMAKE_CURRENT_SOURCE_DIR}/docs/ISA_ABI_CONTRACT.md"
            "${COMPILER_VP_PACKAGE_DIR}/docs/ISA_ABI_CONTRACT.md"
    # The SDK: the exact startup, linker script, shim, header and Makefile that
    # produced the bundled examples. Copied rather than regenerated so what a
    # compiler team receives is byte-identical to what was tested.
    COMMAND ${CMAKE_COMMAND} -E copy
            "${COMPILER_VP_FW_SOURCE}/Makefile"
            "${COMPILER_VP_PACKAGE_DIR}/sdk/Makefile"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${COMPILER_VP_FW_SOURCE}/common"
            "${COMPILER_VP_PACKAGE_DIR}/sdk/common"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${COMPILER_VP_MAP_INCLUDE}"
            "${COMPILER_VP_PACKAGE_DIR}/sdk/include"
    COMMAND ${CMAKE_COMMAND} -E copy
            "${COMPILER_VP_FW_SOURCE}/examples/scalar_hello/main.c"
            "${COMPILER_VP_PACKAGE_DIR}/sdk/examples/scalar_hello/main.c"
    COMMAND ${CMAKE_COMMAND} -E copy
            "${COMPILER_VP_FW_SOURCE}/examples/rvv_vector_add/main.c"
            "${COMPILER_VP_PACKAGE_DIR}/sdk/examples/rvv_vector_add/main.c"
    COMMAND ${CMAKE_COMMAND}
            -DMANIFEST_PATH=${COMPILER_VP_PACKAGE_DIR}/BUILD_MANIFEST.json
            -DSOURCE_DIR=${CMAKE_SOURCE_DIR}
            -DBUILD_REVISION=${COMPILER_VP_GIT_REVISION}
            -DPLATFORM=riscv_vpp_compiler_vp
            -DBUILD_TYPE=${COMPILER_VP_BUILD_TYPE}
            -DCXX_COMPILER=${CMAKE_CXX_COMPILER}
            -DCXX_COMPILER_ID=${CMAKE_CXX_COMPILER_ID}
            -DCXX_COMPILER_VERSION=${CMAKE_CXX_COMPILER_VERSION}
            -DCMAKE_VERSION_USED=${CMAKE_VERSION}
            -DSYSTEMC_HOME=${SYSTEMC_HOME}
            -DISA_STRING=${COMPILER_VP_ISA_STRING}
            -DABI_STRING=${COMPILER_VP_ABI_STRING}
            -DRAM_BASE=0x80000000
            -DRAM_SIZE_DEFAULT=0x04000000
            -DHOSTIO_BASE=0x000F0000
            -DHOSTIO_SIZE=0x00001000
            -DVPP_BASE_REVISION=${RISCV_VP_PLUSPLUS_REVISION}
            "-DVPP_PATCH_SERIES=${RISCV_VP_PLUSPLUS_PATCH_SERIES}"
            -DFIRMWARE_INCLUDED=${COMPILER_VP_FIRMWARE_INCLUDED}
            -DCROSS_TOOLCHAIN=${COMPILER_VP_TOOLDIR}
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/write_build_manifest.cmake"
    VERBATIM)

if(COMPILER_VP_FIRMWARE_INCLUDED)
    add_custom_command(TARGET riscv_vpp_compiler_vp_package POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
                "${COMPILER_VP_SCALAR_ELF}"
                "${COMPILER_VP_PACKAGE_DIR}/examples/scalar_hello/scalar_hello.elf"
        COMMAND ${CMAKE_COMMAND} -E copy
                "${COMPILER_VP_SCALAR_ELF}.dis"
                "${COMPILER_VP_PACKAGE_DIR}/examples/scalar_hello/scalar_hello.elf.dis"
        COMMAND ${CMAKE_COMMAND} -E copy
                "${COMPILER_VP_VECTOR_ELF}"
                "${COMPILER_VP_PACKAGE_DIR}/examples/rvv_vector_add/rvv_vector_add.elf"
        COMMAND ${CMAKE_COMMAND} -E copy
                "${COMPILER_VP_VECTOR_ELF}.dis"
                "${COMPILER_VP_PACKAGE_DIR}/examples/rvv_vector_add/rvv_vector_add.elf.dis"
        # `link.ld` is generated from the shared header, so it exists only in
        # the build tree. The SDK ships both it and the `link.ld.in` it came
        # from: a team that would rather not preprocess anything can use the
        # generated one directly.
        COMMAND ${CMAKE_COMMAND} -E copy
                "${COMPILER_VP_FW_BUILD}/common/link.ld"
                "${COMPILER_VP_PACKAGE_DIR}/sdk/common/link.ld"
        VERBATIM)
    add_dependencies(riscv_vpp_compiler_vp_package riscv_vpp_compiler_vp_firmware)
endif()

# The last word on what a package must contain.
#
# Added after every copy above, because POST_BUILD commands run in the order
# they are attached. Without it, a build with no cross toolchain produced an
# exit-zero package that was missing both mandatory demonstrations and said so
# only in one manifest field -- a bundle that looks successful and is not
# shippable.
add_custom_command(TARGET riscv_vpp_compiler_vp_package POST_BUILD
    COMMAND ${CMAKE_COMMAND}
            -DPACKAGE_DIR=${COMPILER_VP_PACKAGE_DIR}
            -DTOOLDIR=${COMPILER_VP_TOOLDIR}
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_package_contents.cmake"
    VERBATIM)

# ── tests ────────────────────────────────────────────────────────────────────

if(CDC_BUILD_TESTS)
    find_program(COMPILER_VP_PYTHON3 NAMES python3 REQUIRED)

    # Options, refusals, watchdogs and the two required demonstrations.
    add_test(
        NAME riscv_vpp_compiler_vp_cli
        COMMAND ${COMPILER_VP_PYTHON3}
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_compiler_vp_cli.py"
                $<TARGET_FILE:riscv_vpp_compiler_vp>
                "${CMAKE_CURRENT_SOURCE_DIR}/configs"
                "${COMPILER_VP_FW_BUILD}"
                "${CMAKE_CURRENT_BINARY_DIR}/cli_scratch"
                "${COMPILER_VP_TOOLDIR}"
                "${COMPILER_VP_MAP_INCLUDE}")
    set_tests_properties(riscv_vpp_compiler_vp_cli PROPERTIES
        LABELS "compiler_vp;platform"
        SKIP_RETURN_CODE 77
        TIMEOUT 900)

    # The architectural boundary, checked against the sources, the emitted
    # symbols and the link interface rather than against a comment.
    add_test(
        NAME riscv_vpp_compiler_vp_independence
        COMMAND ${CMAKE_COMMAND}
                -DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
                -DFIRMWARE_DIR=${COMPILER_VP_FW_SOURCE}
                -DBINARY=$<TARGET_FILE:riscv_vpp_compiler_vp>
                -DNM=${CMAKE_NM}
                -DLINK_LIBRARIES=$<TARGET_PROPERTY:riscv_vpp_compiler_vp,LINK_LIBRARIES>
                "-DVPP_SOURCES=$<TARGET_PROPERTY:riscv_vp_plusplus,SOURCES>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_compiler_vp_independence.cmake")
    set_tests_properties(riscv_vpp_compiler_vp_independence PROPERTIES
        LABELS "compiler_vp;boundary"
        TIMEOUT 300)

    # The distribution gate: a fresh Release build, packaged, moved, and run
    # from a directory with no access to the source or build tree.
    add_test(
        NAME riscv_vpp_compiler_vp_packaging
        COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/tests/run_packaging_regression.sh")
    # No SKIP_RETURN_CODE. Every other packaging regression in this repository
    # skips when a host prerequisite is missing, and for a component test that is
    # right. This one gates a *release package*: if it cannot build the bundle,
    # the answer a CI run needs is "there is no shippable package", not a green
    # tick and a skip nobody reads.
    set_tests_properties(riscv_vpp_compiler_vp_packaging PROPERTIES
        ENVIRONMENT "PACKAGING_SYSTEMC_HOME=${SYSTEMC_HOME}"
        LABELS "compiler_vp;packaging;distribution"
        RUN_SERIAL TRUE
        TIMEOUT 2400)
endif()
