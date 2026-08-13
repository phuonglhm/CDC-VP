# SPDX-License-Identifier: Apache-2.0
#
# The last step of `riscv_vpp_compiler_vp_package`: refuse to call an incomplete
# bundle a package.
#
# ## Why this is a build step and not a test
#
# Without it, a build on a machine with no RISC-V cross toolchain produced a
# package target that exited zero and a bundle with no `examples/` directory —
# recorded only as one `false` in a manifest field. Phase 4.5 makes both
# demonstrations mandatory package content, so that bundle is not a smaller
# package, it is a broken one, and the moment to say so is while the thing is
# being assembled rather than in a test somebody may not run.
#
# The licence half is the same argument with higher stakes. The executable
# statically links the RISC-V VP++ ISS (MIT) and Berkeley SoftFloat
# (BSD-3-Clause), and both require their notice to accompany a binary
# distribution. A package missing them cannot be given to anyone, and a missing
# licence file is exactly the kind of absence nobody notices by looking.
#
# Every check is on content, not just existence: an empty file created by a
# failed copy passes a `-f` test and fails an obligation.
#
# Required: -DPACKAGE_DIR=
# Optional: -DTOOLDIR=   (named in the diagnostic when the images are missing)

if(NOT DEFINED PACKAGE_DIR)
    message(FATAL_ERROR
        "check_package_contents.cmake: -DPACKAGE_DIR= is required")
endif()

set(_missing "")

# `path|why` records. The separator is `|`, and no reason may contain `;` —
# CMake's list separator would split a record in half and the remainder would be
# reported as a filename.
set(_required
    "riscv_vpp_compiler_vp|the simulator itself"
    "README.md|the package introduction"
    "BUILD_MANIFEST.json|what the bundle was built from"
    "configs/default.yaml|the default configuration"
    "configs/tight_watchdog.yaml|the short-watchdog configuration"
    "docs/MEMORY_MAP.md|the memory map and host-I/O contract"
    "docs/COMPILER_QUICKSTART.md|the quickstart the plan requires"
    "docs/ISA_ABI_CONTRACT.md|the frozen ISA/ABI contract the plan requires"
    "sdk/Makefile|the build rules that produced the examples"
    "sdk/common/crt0.S|the startup code"
    "sdk/common/link.ld|the generated linker script"
    "sdk/common/link.ld.in|the linker script it was generated from"
    "sdk/common/host_io.c|the host-I/O shim"
    "sdk/common/host_io.h|the host-I/O shim interface"
    "sdk/include/compiler_vp/host_io_map.h|the shared memory-map header"
    "sdk/examples/scalar_hello/main.c|the scalar example source"
    "sdk/examples/rvv_vector_add/main.c|the vector example source"
    "licenses/Apache-2.0.txt|the CDC-VP licence"
    "licenses/CDC-VP-NOTICE.txt|the CDC-VP notice"
    "licenses/THIRD_PARTY.md|the third-party inventory"
    "licenses/RISCV-VP-PLUSPLUS.MIT.txt|the MIT licence of the statically linked RISC-V VP++ ISS"
    "licenses/BERKELEY-SOFTFLOAT-3d.BSD-3-Clause.txt|the BSD-3-Clause notice of the statically linked Berkeley SoftFloat"
)

# The two demonstrations. Held apart from the list above so their absence gets
# the diagnostic that names the actual cause.
set(_demonstrations
    "examples/scalar_hello/scalar_hello.elf"
    "examples/scalar_hello/scalar_hello.elf.dis"
    "examples/rvv_vector_add/rvv_vector_add.elf"
    "examples/rvv_vector_add/rvv_vector_add.elf.dis"
)

foreach(_entry ${_required})
    string(REGEX REPLACE "\\|.*" "" _path "${_entry}")
    string(REGEX REPLACE "^[^|]*\\|" "" _why "${_entry}")
    set(_full "${PACKAGE_DIR}/${_path}")
    if(NOT EXISTS "${_full}")
        list(APPEND _missing "${_path} -- ${_why}")
    else()
        file(SIZE "${_full}" _size)
        if(_size EQUAL 0)
            list(APPEND _missing "${_path} is empty -- ${_why}")
        endif()
    endif()
endforeach()

set(_missing_demos "")
foreach(_path ${_demonstrations})
    set(_full "${PACKAGE_DIR}/${_path}")
    if(NOT EXISTS "${_full}")
        list(APPEND _missing_demos "${_path}")
    else()
        file(SIZE "${_full}" _size)
        if(_size EQUAL 0)
            list(APPEND _missing_demos "${_path} (empty)")
        endif()
    endif()
endforeach()

# The licence files must contain the licence, not merely exist. A truncated copy
# is worse than a missing one, because it looks like the obligation was met.
set(_licence_content
    "licenses/RISCV-VP-PLUSPLUS.MIT.txt|Permission is hereby granted"
    "licenses/BERKELEY-SOFTFLOAT-3d.BSD-3-Clause.txt|Redistribution and use"
)
foreach(_entry ${_licence_content})
    string(REGEX REPLACE "\\|.*" "" _path "${_entry}")
    string(REGEX REPLACE "^[^|]*\\|" "" _needle "${_entry}")
    set(_full "${PACKAGE_DIR}/${_path}")
    if(EXISTS "${_full}")
        file(READ "${_full}" _content)
        if(NOT _content MATCHES "${_needle}")
            list(APPEND _missing
                "${_path} does not contain '${_needle}' -- it is not the licence text")
        endif()
    endif()
endforeach()

if(_missing_demos)
    message(SEND_ERROR
        "FAIL: the package is missing the required demonstrations:")
    foreach(_path ${_missing_demos})
        message(SEND_ERROR "        ${_path}")
    endforeach()
    if(DEFINED TOOLDIR AND NOT EXISTS "${TOOLDIR}/riscv-none-elf-gcc")
        message(SEND_ERROR
            "      No RV32GCV cross toolchain at ${TOOLDIR}, so they could not "
            "be built. Set COMPILER_VP_TOOLDIR to a riscv-none-elf install.")
    endif()
endif()

if(_missing)
    message(SEND_ERROR "FAIL: the package is missing required content:")
    foreach(_entry ${_missing})
        message(SEND_ERROR "        ${_entry}")
    endforeach()
endif()

if(_missing OR _missing_demos)
    message(FATAL_ERROR
        "riscv_vpp_compiler_vp: refusing to produce an incomplete package. "
        "Phase 4.5 makes the scalar and vector demonstrations mandatory "
        "content, and the licences of everything statically linked into the "
        "binary are a condition of distributing it at all.")
endif()

message(STATUS "riscv_vpp_compiler_vp: package contents PASS")
