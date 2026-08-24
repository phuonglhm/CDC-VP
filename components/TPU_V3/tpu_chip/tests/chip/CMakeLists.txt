# SPDX-License-Identifier: Apache-2.0
#
# The Phase 8 composition gate. Separate from the fabric test next door because
# it needs the CPU backend and both extracted accelerators — the same three
# things `tpu_v3_tpu_chip` itself is gated on.

add_executable(test_tpu_v3_tpu_chip test_tpu_chip.cpp)
target_link_libraries(test_tpu_v3_tpu_chip PRIVATE
    cdc::components::tpu_v3_tpu_chip)
# Both chip-fabric timing modes, as separate cases. `annotated` never yields, so
# a whole class of concurrency defect is unreachable in it; `arbitrated` blocks,
# and it is what caught a core's hart and DMA both entering the core's single
# external socket. Phase 9's real NoC hop will block the same way.
foreach(_chip_timing annotated arbitrated)
    add_test(NAME tpu_v3_tpu_chip_${_chip_timing}
        COMMAND test_tpu_v3_tpu_chip ${_chip_timing})
    set_tests_properties(tpu_v3_tpu_chip_${_chip_timing} PROPERTIES
        LABELS "tpu_v3;integration;chip"
        TIMEOUT 600)
endforeach()

# ── Phase 9 composition ──────────────────────────────────────────────────────
#
# The chip, its NoC endpoint and a **real** `noc_interconnect`. It lives here
# rather than under `noc_endpoint/tests` on purpose: this needs the CPU backend
# and both extracted accelerators, which is what `tpu_v3_tpu_chip` is already
# gated on, and it keeps the endpoint's own gate mesh-free and CPU-free.
add_executable(test_tpu_v3_chip_on_mesh test_chip_on_mesh.cpp)
target_link_libraries(test_tpu_v3_chip_on_mesh PRIVATE
    cdc::components::tpu_v3_tpu_chip
    cdc::components::tpu_v3_noc_endpoint
    cdc::components::noc_interconnect)
# Both NoC timing backends. `fast` is what a full-system run uses; `detailed`
# is the one that actually moves flits, and the only one in which "zero flits"
# is a measurement rather than a vacuous truth.
foreach(_noc_timing fast detailed)
    add_test(NAME tpu_v3_chip_on_mesh_${_noc_timing}
        COMMAND test_tpu_v3_chip_on_mesh ${_noc_timing})
    set_tests_properties(tpu_v3_chip_on_mesh_${_noc_timing} PROPERTIES
        LABELS "tpu_v3;integration;noc"
        TIMEOUT 900)
endforeach()
