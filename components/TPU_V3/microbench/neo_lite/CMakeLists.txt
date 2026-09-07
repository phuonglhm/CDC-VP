# SPDX-License-Identifier: Apache-2.0
#
# Gate G5 / WP0. Nothing here implements C1 or C2; it keeps the feasibility
# audit's claims tied to the sources they were read from.

add_test(NAME tpu_v3_wp0_c1_feasibility
    COMMAND ${CMAKE_COMMAND}
            "-DSAURIA_ROOT=${TPU_V3_SAURIA_ROOT}"
            "-DVPP_V_HEADER=${CMAKE_SOURCE_DIR}/third_party/riscv-vp-plusplus/vp/src/core/common/v.h"
            "-DCROSS_GCC=${BENCH_TOOLDIR}/riscv-none-elf-gcc"
            # The hashes WP0 audited against. Passed in rather than embedded in
            # the script so the note, this file and the check share one value.
            "-DSAURIA_TARGETS_SHA256=3ab0b720d1827dd21984c7db1c6928f9d99ace69cef4b1e696259fe764cde836"
            "-DVPP_V_HEADER_SHA256=2833c3cf2caaaa42bcf7b3b70a91b08555ce5965c4219f392220641a0a37e9ee"
            "-DCAPTURE_INFO_SHA256=ecdf4946b5ce765ddab1060d4d4c9bb6e9ea7be6b51fe862f2015973fc5e7eb9"
            # A name of our own: `-P` script mode resets CMAKE_CURRENT_BINARY_DIR
            # to the working directory, so passing that one left a probe file in
            # whatever directory the check happened to run from.
            "-DPROBE_DIR=${CMAKE_CURRENT_BINARY_DIR}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/check_wp0_feasibility.cmake")
set_tests_properties(tpu_v3_wp0_c1_feasibility PROPERTIES
    LABELS "tpu_v3;integration;microbench;d28;g5;wp0"
    TIMEOUT 300)
